#!/usr/bin/env python3

"""
strace_to_dxt.py - extract IO events from strace output

output format:

  First line: "# strace io log v<version_number>"
  Remaining lines are tab-delimited.
    <pid> open <fd> <file_name>
    <pid> open <fd> <file_name> 1   # for files opened with O_APPEND
    <pid> read|write <offset> <length> <ts> <fd>
    <pid> close <fd>
    <pid> dup|dup2 <fd1> <fd2>
    <pid> pipe|pipe2 <fd1> <fd2>


  pid: process id
  fd: file descriptor (an integer)
  time: timestamp in seconds (floating point)

Ed Karrels, edk@illinois.edu, April 2021

  

TODO

 - handle clone calls (duplicate open files)
   (from montage.strace)
   64289 1608267494.907119 clone(child_stack=NULL, flags=CLONE_CHILD_CLEARTID|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x2b290b5b8050) = 64290

Compare the output of this (foo1) with the output of strace_to_dxt.py (foo2)
  sed 's/\topenat\t/\topen\t/' < foo1.out | sed 's/^\(.*\)\t1$/\1/' | grep -v $'\twrite\t.*\t2$' > foo1b.out
  grep -v $'^[0-9]+\t\(close\|dup\)\t' < foo2.out > foo2b.out

todo: pipe:[...]
  $ grep pipe montage.strace | head
  64289 1608267494.907033 pipe([6<pipe:[28091846]>, 7<pipe:[28091846]>]) = 0
  64289 1608267494.907078 pipe([8<pipe:[28091847]>, 9<pipe:[28091847]>]) = 0
  64289 1608267494.907343 close(6<pipe:[28091846]>) = 0


Need to handle pipe and pipe2 also, to deal with all fd's.
  for example, in maskrcnn.strace, process 219958 opens 90 with:
  219958 1608231120.324873 pipe2([90, 91], O_CLOEXEC) = 0
  and then reads from it

$ ./strace_to_dxt.py ../BigData/Montage/montage.log | grep fn= | cutfast -d ' ' -f 4 | cutfast -d = -f 2 | sort | uniq -c
   1767 close
    112 dup2
   5984 lseek
   7830 open
      2 openat
 101717 read
  41484 write

$ ./strace_to_dxt.py ../DL/MaskRCNN/MaskRCNN-io.strace | grep fn= | cutfast -d ' ' -f 4 | cutfast -d = -f 2 | sort | uniq -c
  98730 close
     75 dup
   1218 dup2
  75153 lseek
  87009 open
  25751 openat
      8 pread64
 212874 read
  12811 write

All open flags used
$ ./strace_to_dxt.py ../DL/MaskRCNN/MaskRCNN-io.strace | grep fn=open | sed 's/^.*, \(O_[A-Z]\+\(|O_[A-Z]\+\)*\).*$/\1/g' | tr '|' $'\n' | sort | uniq -c
      1 O_APPEND
 106806 O_CLOEXEC
   9967 O_CREAT
  25503 O_DIRECTORY
   9855 O_EXCL
    131 O_NOFOLLOW
  25560 O_NONBLOCK
 101368 O_RDONLY
   1436 O_RDWR
    111 O_TRUNC
   9956 O_WRONLY


strace_to_dxt.py bugs
  in maskrcnn.strace, these reads gets lost:
    219964 1608231094.532765 read(3,  <unfinished ...>
    219964 1608231094.532823 <... read resumed>"\"\"\"\nRead and write ZIP files.\n\nX"..., 79735) = 79734
    219964 1608231095.459229 read(3,  <unfinished ...>
    219964 1608231095.459283 <... read resumed>"Metadata-Version: 1.2\nName: read"..., 6488) = 6487
  probably because of the presence of doublequotes.

  These writes to stdout should be ignored:
    219964 1608231151.944772 write(1, "loading annotations into memory."..., 34 <unfinished ...>
    219964 1608231151.944865 <... write resumed>) = 34
    219964 1608231151.944958 write(1, "\n", 1 <unfinished ...>
    219964 1608231151.945063 <... write resumed>) = 1
  but they are output without file descriptors:
    219964	write	0	34	1608231151.944865
    219964	write	0	1	1608231151.945063
  again, probably confused by the quotes.

todo:
ERROR line 537556: closeFile error (219965,48) not open
  219965 1608231115.254851 <... close resumed>) = 0
find all references to a file descriptor (48 in this instance)


"""

import re, time, sys, collections, errno, signal, ast, traceback

# pid timestamp [<... ] function-name [ resumed>]
line_prefix_re = re.compile(r'^(\d+) ([0-9.]+) ')
# line_prefix_re = re.compile(r'^(\d+) ([0-9.]+) (<\.\.\. )?([a-zA-Z_][a-zA-Z_0-9]*)( resumed>)?')
# ident_re = re.compile(r'[a-zA-Z0-9_]+')

exited_re = re.compile(r'^\+\+\+ exited with (-?[0-9]+) \+\+\+')
resumed_re = re.compile(r'^<\.\.\. ([a-zA-Z_][a-zA-Z_0-9]*) resumed>')
fn_call_re = re.compile(r'^([a-zA-Z_][a-zA-Z_0-9]*)')
signal_re = re.compile(r'^--- (SIG[A-Z_0-9]+)')

UNFINISHED_SUFFIX = ' <unfinished ...>'

STD_NAMES = {0: '<STDIN>', 1: '<STDOUT>', 2: '<STDERR>'}

# silence warnings
WARN_CLOSE_BAD_FD = False
WARN_DUP_BAD_FD = False
WARN_RW_BAD_FD = False


Token = collections.namedtuple('Token', ['type', 'value', 'offset'])

# don't complain about SIGPIPEs
signal.signal(signal.SIGPIPE, signal.SIG_DFL)


def isList(x):
  return isinstance(x, list)


class OpenFile:
  def __init__(self, filename):
    self.filename = filename
    self.offset = 0

  def setPos(self, offset):
    self.offset = offset

  def setPosRelative(self, offset_relative):
    self.offset += offset_relative

  def pos(self):
    return self.offset
    

class StraceParser:

  def __init__(self):
    # these (pid, timestamp, line_no, line) are all referring to the current
    # line being parsed, and will change between calls
    # self.pid = -1
    # self.timestamp = 0
    # self.line_no = -1
    # self.line = ''

    # map pid -> (fn_name, args_str)
    # for lines ending with " <unfinished ...>", save the function name
    # and string of arguments up to " <unfinished ...>".
    # e.g.: ('access', '("/etc/ld.so.preload", R_OK ')
    self.unfinished_calls = {}

    # (pid,fd) -> OpenFile object, or 'pipe' if it's a pipe.
    # If dup() is called, the two file descriptors will point to
    # the same OpenFile and share an offset
    self.open_files = {}

    self.function_handlers = {
      'close' : self.fnClose,
      'dup' : self.fnDup,
      'dup2' : self.fnDup2,
      'dup3' : None,
      'lseek' : self.fnLseek,
      'lseek64' : self.fnLseek,
      'open' : self.fnOpen,
      'openat' : self.fnOpenat,
      'pipe' : self.fnPipe,
      'pipe2' : self.fnPipe2,
      'pread' : self.fnPread,
      'pread64' : self.fnPread,
      'preadv' : None,
      'preadv2' : None,
      'pwrite' : None,
      'pwritev' : None,
      'pwritev2' : None,
      'read' : self.fnRead,
      'readv' : None,
      'write' : self.fnWrite,
      'writev' : None,
    }
    

  def readFile(self, inf):
    timer = time.time()

    line_no = 0

    while True:
      line = inf.readline()
      if not line: break
      line_no += 1

      line_prefix_match = line_prefix_re.search(line)
      if not line_prefix_match:
        sys.stdout.write(f'{line_no}: NO MATCH  {line[:-1]}\n')
        continue

      # (pid, timestamp, is_resuming1, fn_name, is_resuming2) = line_prefix_match.groups()
      (pid, timestamp) = line_prefix_match.groups()
      pid = int(pid)
      # strip pid, timestamp, and newline
      line2 = line[line_prefix_match.end():-1]

      fn_call_match = fn_call_re.search(line2)
      if fn_call_match:
        fn_name = fn_call_match.group()

        # check if this is an unfinished call in the form:
        # 219859 1608231013.035480 read(5,  <unfinished ...>
        if line2.endswith(UNFINISHED_SUFFIX):
          if pid in self.unfinished_calls:
            sys.stderr.write(f'{line_no}: ERROR nested unfinished calls\n')
          else:
            start_args = line2[fn_call_match.end():-len(UNFINISHED_SUFFIX)]
            self.unfinished_calls[pid] = (fn_name, start_args)
            # print(f'{line_no}: unfinished call {pid} {fn_name}{start_args}...')
          continue

        # regular function call
        args_str = line2[fn_call_match.end():]
        self.functionCall(line_no, line, pid, timestamp, fn_name, args_str)
        # print(f'{line_no}: pid={pid} time={timestamp} fn={fn_name}')

        continue

      # ignore lines where a process exits like "... +++ exited with 0 +++"
      exited_match = exited_re.search(line2)
      if exited_match:
        # print(f'{line_no}: pid {pid} exited with code {exited_match.group(1)}')
        continue

      # handle resume lines like '... <... close resumed>) = 0'
      resume_match = resumed_re.search(line2)
      if resume_match:
        line_remainder = line2[resume_match.end():]
        saved = self.unfinished_calls.get(pid)
        # print('resuming ' + repr(saved))
        if not saved:
          sys.stderr.write(f'{line_no}: ERROR resume of call not left unfinished {line2}\n')
        else:
          del self.unfinished_calls[pid]
          fn_name = saved[0]
          args_str = saved[1] + line_remainder
          self.functionCall(line_no, line, pid, timestamp, fn_name, args_str)
        continue

      # ignore signal lines like '--- SIGCHLD {si_signo=SIGCHLD...'
      signal_match = signal_re.search(line2)
      if signal_match:
        # signal_name = signal_match.group(1)
        # print(f'{line_no}: {signal_match.group(1)}')
        continue

      # unknown line. complain about it
      sys.stderr.write(f'{line_no}: UNRECOGNIZED LINE {line.strip()}\n')

    timer = time.time() - timer

    sys.stderr.write(f'{line_no} lines in {timer:.3f}s\n')


  def functionCall(self, line_no, line, pid, timestamp, fn_name, args_str):
    fn = self.function_handlers.get(fn_name)
    if not fn:
      if False:
        (args,result) = self.parseArgsAndResult(args_str)
        self.printFnCall('unknown function ' + fn_name, args, result)
        print(line)
      return

    # print(f'{line_no}: pid={pid} time={timestamp} fn={fn_name} args={args_str}')

    if args_str.endswith(' = ?'):
      """
      When a process exits, some function calls can end like this. Ignore them.
      64278 1608267494.801919 exit_group(0)   = ?
      220057 1608231115.247595 read(46,  <unfinished ...>
      220057 1608232352.784886 <... read resumed> <unfinished ...>) = ?
      220057 1608232352.785682 +++ exited with 0 +++
      """
      return
    
    (args,result) = self.parseArgsAndResult(args_str)

    # print("args = " + repr(args))
    # print("result = " + repr(result))
    # sys.stdout.flush()

    try:
      fn(pid, timestamp, args, result)
    except Exception as e:
      sys.stderr.write(f'ERROR line {line_no}: {e}\n  {line}')
      # self.printFnCall(fn_name, args, result, sys.stderr)
      # traceback.print_tb(sys.exc_info()[2])


  def splitOnCommas(self, list_in):
    """
    Given a list of token where some of them are commas, group the tokens between
    commas into lists if there are more than one, and remove the commas.
      [x, ',', y, ',', z] -> [x, y, z]
      [1, 2, ',', 3, ',', a, b, c] -> [[1,2], 3, [a,b,c]]
    Returns the replacement list.
    """
    def nextComma(mylist, pos):
      while pos < len(mylist):
        if type(mylist[pos]) == Token and mylist[pos].type == 'COMMA':
          return pos
        pos += 1
      return len(mylist)

    list_out = []
    pos = 0
    while pos < len(list_in):
      next_comma = nextComma(list_in, pos+1)
      elem_len = next_comma - pos
      assert elem_len >= 0
      if elem_len == 0:
        list_out.append([])
      elif elem_len == 1:
        list_out.append(list_in[pos])
      else:
        list_out.append(list_in[pos:pos+elem_len])

      if next_comma == len(list_in) - 1:
        list_out.append([])
        break
      pos = next_comma + 1
    return list_out


  def parseArgsAndResult(self, args_str):
    """
    Parse a string like:
      ("/lib64/libm.so.6", O_RDONLY|O_CLOEXEC) = 3
    and return:
      (list of lists of arg tokens, list of result tokens)
    """

    args = None
    result = []
    rparen_seen = False
    list_stack = [[]]
    top = list_stack[-1]

    # for token in tokenize(args_str):
    #   print('  ' + repr(token))

    
    for token in tokenize(args_str):
      if rparen_seen:
        if token.type != 'EQUALS':
          result.append(token)
      elif token.type == 'LPAREN':
        assert len(list_stack) == 1
      elif token.type == 'RPAREN':
        rparen_seen = True
        assert len(list_stack) == 1
        args = self.splitOnCommas(list_stack[0])
        list_stack = []
      elif token.type == 'LBRACKET':
        top = []
        list_stack.append(top)
      elif token.type == 'RBRACKET':
        if len(list_stack) == 0:
          raise Exception('Unbalanced square brackets: "' + args_str + '", ' + repr(current_arg))
        del(list_stack[-1])
        list_stack[-1].append(self.splitOnCommas(top))
        top = list_stack[-1]
      else:
        top.append(token)

      if False:
        print('token = ' + repr(token))
        print('current_arg = ' + repr(current_arg))
        print('args = ' + repr(args))
        for i, sf in enumerate(list_stack):
          print(f'list_stack[{i}] = {sf!r}')
        print()

    if len(result) == 1:
      result = result[0]
      
    return (args, result)
    

  def openFile(self, pid, fd, filename):
    assert filename
    self.open_files[(pid,fd)] = OpenFile(filename)

    
  def createStdStream(self, pid, fd):
    # print(f'CREATING ({pid}, {fd})')
    assert fd in STD_NAMES
    name = STD_NAMES[fd]
    self.openFile(pid, fd, name)
    

  def closeFile(self, pid, fd):
    key = (pid,fd)
    if key not in self.open_files:
      if WARN_CLOSE_BAD_FD:
        raise Exception(f'closeFile error ({pid},{fd}) not open')
      # sys.stderr.write(f'WARNING close(pid={pid}, fd={fd}) not open\n')
    else:
      del(self.open_files[(pid,fd)])

      
  def openPipe(self, pid, fd1, fd2):
    self.open_files[(pid,fd1)] = 'pipe'
    self.open_files[(pid,fd2)] = 'pipe'

    
  def dup(self, pid, fd_orig, fd_new):
    if fd_orig <= 2:
      self.createStdStream(pid, fd_orig)
    if (pid,fd_orig) not in self.open_files:
      # raise Exception(f'dup of unrecognized fd ({pid},{fd_orig})')
      if WARN_DUP_BAD_FD:
        sys.stderr.write(f'WARNING dup of unrecognized fd ({pid},{fd_orig})\n')
    else:
      self.open_files[(pid,fd_new)] = self.open_files[(pid,fd_orig)]
      print(f'{pid}\tdup\t{fd_orig}\t{fd_new}')
    

  def printFnCall(self, fn_name, args, result, outf = sys.stderr):
    outf.write(f'fn {fn_name}\n')
    for i, arg in enumerate(args):
        print(f'  arg[{i}] = {repr(arg)}')
    outf.write(f'  result = {result!r}\n')

    
  def fnClose(self, pid, timestamp, args, result):
    """
    Samples:
    219859 1608231011.659031 close(3)       = 0
    219862 1608231013.035436 close(4 <unfinished ...>
    219859 1608231013.035443 <... close resumed>) = 0
    220080 1608231120.401160 close(5)       = -1 EBADF (Bad file descriptor)
    """
    if len(args) != 1:
      raise RuntimeError(f'close() args')

    return_code = tokenToInt(firstOrOnly(result))

    if return_code != 0:
      # close failed; ignore
      return
    
    fd = tokenToInt(firstOrOnly(args[0]))
    if fd <= 2:
      # ignore closing of std streams
      return

    self.closeFile(pid, fd)
    print(f'{pid}\tclose\t{fd}')
    # print(f'{pid}\t{timestamp}\tclose\t{fd}')


  def fnDup(self, pid, timestamp, args, result):
    """
    219859 1608231011.903185 dup(2)         = 3
    219958 1608231092.841097 dup(0 <unfinished ...>
    219958 1608231092.841291 <... dup resumed>) = 3
    """
    if len(args) != 1:
      raise Exception('dup() args/result')

    fd2 = tokenToInt(firstOrOnly(result))
    if fd2 != -1:
      fd1 = tokenToInt(firstOrOnly(args[0]))
      self.dup(pid, fd1, fd2)
      # print(f'{pid}\t{timestamp}\tdup\t{fd1}\t{fd2}')
      print(f'{pid}\tdup\t{fd1}\t{fd2}')


  def fnDup2(self, pid, timestamp, args, result):
    """
    221108 1608231148.212901 dup2(4, 1)     = 1
    221107 1608231148.199127 dup2(79, 2 <unfinished ...>
    221107 1608231148.199198 <... dup2 resumed>) = 2
    64299 1608267498.525116 dup2(6<pipe:[28074534]>, 0</dev/pts/0> <unfinished ...>
    64299 1608267498.525148 <... dup2 resumed>) = 0<pipe:[28074534]>
    """
    if len(args) != 2:
      raise Exception('dup() args/result')

    fd1 = tokenToInt(firstOrOnly(args[0]))
    fd2 = tokenToInt(firstOrOnly(args[1]))
    result_fd = tokenToInt(firstOrOnly(result))

    if result_fd != -1:
      assert fd2 == result_fd
      self.dup(pid, fd1, fd2)
      # print(f'{pid}\t{timestamp}\tdup2\t{fd1}\t{fd2}')
      print(f'{pid}\tdup2\t{fd1}\t{fd2}')

      
  def fnLseek(self, pid, timestamp, args, result):
    if (len(args) != 3
        or type(args[2]) != Token
        or args[2].type != 'IDENT'):
      raise Exception('lseek args')

    fd = tokenToInt(firstOrOnly(args[0]))
    offset = tokenToInt(args[1])
    whence = args[2].value
    return_code = tokenToInt(firstOrOnly(result))

    # ignore invalid calls, such as seeking on stdout
    if return_code < 0:
      return

    key = (pid, fd)
    f = self.open_files.get(key)
    if not f:
      sys.stderr.write(f'WARNING seek on unknown fd {key!r} at {timestamp}\n')
      return

    if f == 'pipe':
      # can't seek on a pipe, so this should have returned -1
      sys.stderr.write(f'WARNING seek on unknown a pipe {key!r} at {timestamp}\n')
      return
    
    if whence == 'SEEK_CUR':
      f.setPosRelative(offset)
      if return_code != f.offset:
        sys.stderr.write(f'WARNING after lseek(pid={pid},ts={timestamp},SEEK_CUR) computed offset={f.offset} but return code={return_code}\n')
    elif whence == 'SEEK_SET':
      f.setPos(offset)
      if return_code != f.offset:
        sys.stderr.write(f'WARNING after lseek(pid={pid},ts={timestamp},SEEK_SET) computed offset={f.offset} but return code={return_code}\n')
    elif whence == 'SEEK_END':
      # sys.stderr.write(f'WARNING lseek(SEEK_END) on {key!r}\n')
      f.setPos(return_code)


  def fnOpen(self, pid, timestamp, args, result):
    # result may be [INT], [INT, ERRNO], or [INT, FILENAME]
    """
    samples
    64278 1608267486.473999 open("/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3</etc/ld.so.cache>
    64278 1608267486.478986 open("rawdir/2mass-atlas-990214n-j1100244.fits", O_RDONLY) = 5</scratch1/06058/iwang/benchmarks/montage/m101/rawdir/2mass-atlas-990214n-j1100244.fits>
    219859 1608231011.551421 open("/home1/00946/zzhang/anaconda3/lib/x86_64/libpython3.7m.so.1.0", O_RDONLY|O_CLOEXEC) = -1 ENOENT (No such file or directory)
    219859 1608231011.554048 open("/lib64/libpthread.so.0", O_RDONLY|O_CLOEXEC) = 3
    """
    if len(args) < 2 or len(args) > 3:
      raise RuntimeError(f'open() args error')

    pathname = tokenToString(args[0])
    
    if type(result) == Token:
      r = result
    else:
      assert type(result) == list
      r = result[0]
      if type(result[1]) == Token and result[1].type == 'FILENAME':
        pathname = result[1].value
    fd = tokenToInt(r)

    # print(repr(result))
    if fd >= 0:
      pathname = pathname.replace('\t', '\\t').replace('\n', '\\n')
      self.openFile(pid, fd, pathname)
      # print(f'{pid}\t{timestamp}\topen\t{fd}\t{pathname}')
      print(f'{pid}\topen\t{fd}\t{pathname}')


  def fnOpenat(self, pid, timestamp, args, result):
    if len(args) < 3 or len(args) > 4:
      raise RuntimeError(f'openat() args error')

    # args[0] may be an integer or a symbol e.g. AT_FDCWD
    # dirfd = args[0].value
    
    pathname = tokenToString(args[1])

    if type(result) == list:
      fd = tokenToInt(firstOrOnly(result))
      if len(result) > 1 and type(result[1]) == Token and result[1].type == 'FILENAME':
        pathname = result[1].value
    else:
      fd = tokenToInt(result)

    if fd >= 0:
      self.openFile(pid, fd, pathname)
      # print(f'{pid}\t{timestamp}\topen\t{fd}\t{pathname}')
      print(f'{pid}\topen\t{fd}\t{pathname}')


  def fnPipe2(self, pid, timestamp, args, result):
    # self.printFnCall('pipe2', args, result)
    if (len(args) != 2
        or type(result) != Token
        or result.type != 'INT'):
      raise RuntimeError('bad args to pipe2')

    if result.value != '0':
      # failed call to pipe; ignore it
      return

    (fd1, fd2) = self.parsePipeArg0(args[0])
    self.openPipe(pid, fd1, fd2)
    # print(f'{pid}\t{timestamp}\tpipe2\t{fd1}\t{fd2}')
    print(f'{pid}\tpipe2\t{fd1}\t{fd2}')


  def fnPipe(self, pid, timestamp, args, result):
    """
    Samples
    64289 1608267494.907078 pipe([8<pipe:[28091847]>, 9<pipe:[28091847]>]) = 0
    219958 1608231107.176570 pipe([33, 34]) = 0
    """
    if (len(args) != 1
        or type(result) != Token
        or result.type != 'INT'):
      raise Exception('bad args to pipe')

    if result.value != '0':
      # failed call to pipe; ignore it
      return

    (fd1, fd2) = self.parsePipeArg0(args[0])
    
    self.openPipe(pid, fd1, fd2)
    # print(f'{pid}\t{timestamp}\tpipe\t{fd1}\t{fd2}')
    print(f'{pid}\tpipe\t{fd1}\t{fd2}')


  def parsePipeArg0(self, arg):
    """
    arg with be in one of these two forms:
      [33, 34]
      [6<pipe:[28091846]>, 7<pipe:[28091846]>]
    The first is a list of two Token objects.
    The second is two lists of two Tokens each.
    Raises an exception on error.
    Returns tuple (fd1, fd1) on success.
    """
    if (type(arg) != list
        or len(arg) != 2):
      raise Exception('bad arg[0] to pipe')

    return (tokenToInt(firstOrOnly(arg[0])), tokenToInt(firstOrOnly(arg[1])))

    
  def fnReadOrWrite(self, pid, timestamp, args, result, rw):
    assert rw == 'read' or rw == 'write'
    
    if len(args) != 3:
      raise Exception(f'{rw}() args error')

    fd = tokenToInt(firstOrOnly(args[0]))

    # ignore stdin, stdout, stderr
    if fd <= 2:
      return
    
    # length = tokenToInt(args[2])
    length = tokenToInt(firstOrOnly(result))

    open_file = self.open_files.get((pid, fd))
    if not open_file:
      # raise Exception(f'{rw}() of invalid fd')
      if WARN_RW_BAD_FD:
        sys.stderr.write(f'ERROR {rw} of invalid fd (pid={pid}, fd={fd}, ts={timestamp})\n')
    elif open_file == 'pipe':
      # ignore; this is a pipe
      pass
    else:
      offset = open_file.pos()
      open_file.setPosRelative(length)
      print(f'{pid}\t{rw}\t{offset}\t{length}\t{timestamp}\t{fd}')

    
  def fnRead(self, pid, timestamp, args, result):
    self.fnReadOrWrite(pid, timestamp, args, result, 'read')

    
  def fnWrite(self, pid, timestamp, args, result):
    self.fnReadOrWrite(pid, timestamp, args, result, 'write')


  def fnPread(self, pid, timestamp, args, result):
    # ssize_t pread(int fd, void *buf, size_t count, off_t offset);
    if len(args) != 4:
      raise Exception(f'pread() args error')
    fd = tokenToInt(firstOrOnly(args[0]))
    # count = tokenToInt(args[2])
    offset = tokenToInt(args[3])
    result_count = tokenToInt(result)

    open_file = self.open_files.get((pid, fd))
    if not open_file:
      sys.error.write(f'ERROR pread of invalid fd (pid={pid}, fd={fd}, ts={timestamp})\n')
    elif open_file == 'pipe':
      # ignore; this is a pipe
      pass
    else:
      open_file.setPos(offset + result_count)
      print(f'{pid}\tread\t{offset}\t{result_count}\t{timestamp}\t{fd}')
    



def main(args):
  if len(args) == 1:
    inf = open(args[0], 'r', encoding='ascii')
  elif len(args) == 0:
    inf = sys.stdin
  else:
    print('''
  strace_to_dxt.py [<input_file>]
''')
    return 1

  print('# strace io log v1.0')
  parser = StraceParser()
  
  parser.readFile(inf)


def tokenize(s):
  token_spec = [
    # ('NUMBER', r'[-+]?([0-9]*[.])?[0-9]+([eE][-+]?\d+)?'),
    ('FLOAT', r'([+-]?\d+[eE][-+]?\d+)|([+-]?\d+[.]\d*([eE][-+]?\d+)?)|([+-][.]\d+([eE][-+]?\d+)?)'),
    #  try [+-]?(\d+([.]\d*)?([eE][+-]?\d+)?|[.]\d+([eE][+-]?\d+)?)
    #  from https://stackoverflow.com/questions/12643009/regular-expression-for-floating-point-numbers
    ('INT', r'[-+]?((0[xX][0-9a-fA-F]+)|([0-9]+))'),
    ('STRING', r'"([^"\\]|(\\.))*"(\.\.\.)?'),
    ('FILENAME', r'<[^>]*>'),
    ('PIPE', r'\|'),
#     ('OPEN_FLAGS', '(O_APPEND|O_CLOEXEC|O_CREAT|O_DIRECTORY|O_EXCL|O_NOFOLLOW|O_NONBLOCK|O_RDONLY|O_RDWR|O_TRUNC|O_WRONLY)([|](O_APPEND|O_CLOEXEC|O_CREAT|O_DIRECTORY|O_EXCL|O_NOFOLLOW|O_NONBLOCK|O_RDONLY|O_RDWR|O_TRUNC|O_WRONLY))*'),
    ('ERRNO', r'(EACCES|EBADF|ENOENT|ESPIPE) \([^)]*\)'),
    ('IDENT', r'[a-zA-Z_][a-zA-Z_0-9]*'),
    ('COMMA', r','),
    ('EQUALS', r'='),
    ('LPAREN', r'\('),
    ('RPAREN', r'\)'),
    ('LBRACKET', r'\['),
    ('RBRACKET', r'\]'),
    ('COMMENT', r'/[*].*?[*]/'),
    # ('LIST', r'\[[^]]*\]'),
    ('SKIP', r'[ \t]+'),
    ('ERR', r'.'),
  ]
  tok_regex = '|'.join('(?P<%s>%s)' % pair for pair in token_spec)
  for match in re.finditer(tok_regex, s):
    kind = match.lastgroup
    value = match.group()
    offset = match.start()
    if kind == 'SKIP':
      continue
    elif kind == 'FILENAME':
      value = value[1:-1]
    elif kind == 'LIST':
      value = [x.strip() for x in value[1:-1].split(',')]
    # elif kind == 'ERR':
    #   raise RuntimeError(f'{value!r} unexpected in "{s[match.start():]}"')
    yield Token(kind, value, offset)

    
def firstOrOnly(t):
  # if t is a token, return it
  # if t is a list, return the first element
  # otherwise complain
  if type(t) == Token:
    return t
  elif type(t) == list:
    if len(t) < 1:
      raise Exception('Expected nonempty list')
    return t[0]
  else:
    raise Exception('Expected Token or list')


def tokenToInt(token):
  if type(token) != Token or token.type != 'INT':
    raise Exception('Expected int token, got ' + repr(token))
  return int(token.value)


def tokenToString(token):
  if (type(token) != Token
      or token.type != 'STRING'
      or len(token) < 2
      or token.value[0] != '"'
      or (not token.value.endswith('"') and not token.value.endswith('"...'))):
    raise Exception('Expected string, got ' + repr(token))
  v = token.value if token.value[-1] == '"' else token.value[:-3]
  return ast.literal_eval(v)


def tokenListToString(token_list):
  return ''.join([x.value for x in token_list])


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
