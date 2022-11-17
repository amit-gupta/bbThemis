import re
import sys


def get_func(split):
    return split[2].split('(')[0]


process_offsets = dict()
process_queue = dict()

o_append = False

with open(sys.argv[1], 'r') as f, open(sys.argv[2], 'w') as o:
    o.write("# strace io log\n")
    for line in f:
        
        l_sp = line.split()
        l_c_sp = line.split(',')
        process_id = int(l_sp[0])
        if process_id not in process_offsets:
            process_offsets[process_id] = dict()
        if process_id not in process_queue:
            process_queue[process_id] = list()

        curr = process_offsets[process_id]
        curr[0] = 0
        curr[1] = 0
        curr[2] = 0

        curr_status = process_queue[process_id]

        func = get_func(l_sp)

        if func == "open" or (func == "<..." and len(re.findall(r'\b(open)\b', line)) == 1):
            if("O_APPEND" in line):
                o_append = True
            fd = l_sp[5]
            if(l_sp[5] == "="):
                fd = l_sp[6]
            nums = re.findall(r'^([+-]?[1-9]\d*|0)$', fd)
            fd_calc = int(nums[0]) if len(nums) > 0 else -1
            if fd_calc != -1:
                curr[fd_calc] = 0
            if(func == "open"):
                f_name = re.findall(r'"(.*?)"',l_sp[2])[0]
                if("unfinished" in line):
                    curr_status.append(f_name)
                else:
                    if fd_calc != -1:
                        if(o_append):
                            o.write(f'{process_id}\topen\t{fd_calc}\t{f_name}\t1\n')
                            o_append = False
                        else:
                            o.write(f'{process_id}\topen\t{fd_calc}\t{f_name}\n')
            if(func == "<..."):
                if fd_calc != -1:
                    f_name = curr_status.pop(-1)
                    if(o_append):
                        o.write(f'{process_id}\topen\t{fd_calc}\t{f_name}\t1\n')
                        o_append = False
                    else:
                        o.write(f'{process_id}\topen\t{fd_calc}\t{f_name}\n')
                

        if func == "openat" or (func == "<..." and len(re.findall(r'\b(openat)\b', line)) == 1):
            if("O_APPEND" in line):
                o_append = True
            fd = l_sp[6]
            if(l_sp[6] == "="):
                fd = l_sp[7]
            nums = re.findall(r'([+-]?[1-9]\d*|0)\b', fd)
            fd_calc = int(nums[0]) if len(nums) > 0 else -1
            if fd_calc != -1:
                curr[fd_calc] = 0
            if(func == "openat"):
                f_name = re.findall(r'"(.*?)"',l_sp[3])[0]
                if("unfinished" in line):
                    curr_status.append(f_name)
                else:
                    if fd_calc != -1:
                        if(o_append):
                            o.write(f'{process_id}\topen\t{fd_calc}\t{f_name}\t1\n')
                            o_append = False
                        else:
                            o.write(f'{process_id}\topen\t{fd_calc}\t{f_name}\n')
            if(func == "<..."):
                if fd_calc != -1:
                    f_name = curr_status.pop(-1)
                    if(o_append):
                        o.write(f'{process_id}\topenat\t{fd_calc}\t{f_name}\t1\n')
                        o_append = False
                    else:
                        o.write(f'{process_id}\topenat\t{fd_calc}\t{f_name}\n')


        if (func == "read" or func == "write") and ("...>" not in line):
            bytes_read = l_c_sp[-1].split("=")[-1]
            bytes_list = re.findall(r'([+-]?[1-9]\d*|0)\b', bytes_read)

            read_fd = int(re.findall(r'([+-]?[1-9]\d*|0)',l_c_sp[0].split("(")[1])[0])
            #print(read_fd)

            if(len(bytes_list) != 1):
                #not sure about this case
                continue
            len_read = int(bytes_list[0])
            if(len_read != -1):
                if read_fd not in curr and read_fd > 2:
    #                 print("fudge")
                    continue
                else:
                    o.write(f'{process_id}\t{func}\t{curr[read_fd]}\t{len_read}\t{l_sp[1]}\t{read_fd}\n')
                    curr[read_fd] += len_read
                    
        elif (func == "read" or func == "write") and ("...>" in line):
            read_fd = int(re.findall(r'([+-]?[1-9]\d*|0)',l_c_sp[0].split("(")[1])[0])
            if read_fd not in curr and read_fd > 2:
                curr_status.append(-1)
            else:
                curr_status.append(read_fd)

        if ("pread" in func or "pwrite" in func) and ("...>" not in line):
            bytes_read = l_c_sp[-1].split("=")[-1]
            bytes_list = re.findall(r'([+-]?[1-9]\d*|0)\b', bytes_read)

            read_fd = int(re.findall(r'([+-]?[1-9]\d*|0)',l_c_sp[0].split("(")[1])[0])
            #print(read_fd)

            if(len(bytes_list) != 1):
                #not sure about this case
                continue
            len_read = int(bytes_list[0])
            if(len_read != -1):
                if read_fd not in curr and read_fd > 2:
                    print("PROBLEM")
                    #print("fudge")
                else:
                    o.write(f'{process_id}\t{func}\t{curr[read_fd]}\t{len_read}\t{l_sp[1]}\t{read_fd}\n')
        elif ("pread" in func or "pwrite" in func) and ("...>" in line): 
            read_fd = int(re.findall(r'([+-]?[1-9]\d*|0)',l_c_sp[0].split("(")[1])[0])
            if read_fd not in curr and read_fd > 2:
                curr_status.append(-1)
            else:
                curr_status.append(read_fd)

            
        check_rw_resume = re.findall(r'\b(read|write)\b|\b(pread|pwrite)\d*\b', line)
        if ("<..." in line) and (len(check_rw_resume) == 1) and "resumed" in line:
            func_name = check_rw_resume[0][0]
            fd = curr_status.pop(-1)
            if(fd != -1):
                bytes_read = l_c_sp[-1].split("=")[-1]
                bytes_list = re.findall(r'([+-]?[1-9]\d*|0)\b', bytes_read)
                o.write(f'{process_id}\t{func_name}\t{curr[fd]}\t{bytes_list[0]}\t{l_sp[1]}\t{fd}\n')
                if(func_name == "read" or func_name == "write"):
                    curr[fd] += int(bytes_list[0])


        if (func == "lseek") and ("...>" not in line):
            bytes_read = l_c_sp[-1].split("=")[-1]
            bytes_list = re.findall(r'([+-]?[1-9]\d*|0)\b', bytes_read)

            lseek_fd = int(re.findall(r'([+-]?[1-9]\d*|0)',l_c_sp[0].split("(")[1])[0])
            curr[lseek_fd] = int(bytes_list[0])
        elif (func == "lseek") and ("...>" in line):
            lseek_fd = int(re.findall(r'([+-]?[1-9]\d*|0)',l_c_sp[0].split("(")[1])[0])
            curr_status.append(lseek_fd)

        if ("<..." in line) and (len(re.findall(r'\b(lseek)\b', line)) == 1):
            lseek_fd = curr_status.pop(-1)
            bytes_read = l_c_sp[-1].split("=")[-1]
            bytes_list = re.findall(r'([+-]?[1-9]\d*|0)\b', bytes_read)
            offset = int(bytes_list[0])
            if offset != -1:
                curr[lseek_fd] = offset
        
        if (func == "close") and ("...>" not in line):
            ret_val = l_c_sp[-1].split("=")[-1]
            ret_val_int = int(re.findall(r'([+-]?[1-9]\d*|0)\b', ret_val)[0])

            close_fd = int(re.findall(r'([+-]?[1-9]\d*|0)',l_c_sp[0].split("(")[1])[0])
            if(ret_val_int == 0):
                curr.pop(close_fd, 'None')      
        elif (func == "close") and ("...>" in line):
            close_fd = int(re.findall(r'([+-]?[1-9]\d*|0)',l_c_sp[0].split("(")[1])[0])
            curr_status.append(close_fd)
        
        if ("<..." in line) and (len(re.findall(r'\b(close)\b', line)) == 1):
            close_fd = curr_status.pop(-1)
            ret_val = l_c_sp[-1].split("=")[-1]
            ret_val_int = int(re.findall(r'([+-]?[1-9]\d*|0)\b', ret_val)[0])
            
            if(ret_val_int == 0):
                curr.pop(close_fd, 'None')  