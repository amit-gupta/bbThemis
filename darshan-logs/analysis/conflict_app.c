/*
  This is an MPI application designed to have I/O conflicts, with the purpose
  of generating interesting Darshan profiling data.

  Using two ranks, it does one operation (a read or write) to a file
  with rank 0, then optionally performs a syncronization action,
  then reads or writes the file with rank 1.

  Before running this, call it with the -init command line flag
  to initialize the output files. This is necessary to support the cases
  where the first operation is to read the data file.

    ./conflict_app -init

  There are multiple parameters for each test, and a different file
  will be created for each set of parameter values.
  1. Access pattern: {RAR,RAW,WAR,WAW}
     Read after read, read after write, etc
  2. IO library: {POSIX,MPIIO}
     read()/write() or MPI_File_read()/MPI_File_write()
  3. Synchronization: {NONE,MSG,CLOSE,FSYNC}
     This defines the syncronization (if any) between the two IO operations.
     NONE: rank 1 just sleeps for a little while before accessing
     MSG: rank 1 waits for a message from rank 0
     CLOSE: rank 0 closes the file, sends a message to rank 1, rank 1
       receives the message, and then opens the file
     FSYNC: rank 0 syncs the file (via POSIX fsync() or MPI_File_sync(),
       depending on the IO routines used) before sending a message to rank 1.

  The data file names will include the parameters:
    conflict_app.out.{RAR,RAW,WAR,WAW}.{POSIX,MPIIO}.{NONE,MSG,CLOSE,FSYNC}

  Note: Darshan captures calls to open(), close(), fsync(), fdatasync(),
  and MPI_File_sync(). In theory these could be used to ensure the
  accesses are synchronized. However, parallel IO systems are not necessarily
  reliable, so it would be wise to test each of those synchronization methods
  on your intended system before relying on them.
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <mpi.h>

int np, rank;

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#define PATTERN_RAR 0
#define PATTERN_RAW 1
#define PATTERN_WAR 2
#define PATTERN_WAW 3
#define PATTERN_COUNT 4
const char *PATTERN_NAMES[] = {"RAR", "RAW", "WAR", "WAW"};

#define IOLIB_POSIX 0
#define IOLIB_MPIIO 1
#define IOLIB_COUNT 2
const char *IOLIB_NAMES[] = {"POSIX", "MPIIO"};

#define SYNC_NONE  0
#define SYNC_MSG   1
#define SYNC_CLOSE 2
#define SYNC_FSYNC 3
#define SYNC_COUNT 4
const char *SYNC_NAMES[] = {"NONE", "MSG", "CLOSE", "FSYNC"};


typedef struct {
  int do_init; /* just initialize the data files */
  int64_t size;
  int64_t block_size;
  double wait_seconds;
} Options;


int parseArgs(int argc, char **argv, Options *opt);
int parseSize(const char *str, uint64_t *result);
int initializeFile(const char *filename, int pattern_idx, int iolib_idx,
                   int sync_idx, Options *opt);
void doIO(const char *filename, int pattern_idx, int iolib_idx,
          int sync_idx, Options *opt);

        
int main(int argc, char **argv) {
  Options opt;
  char filename[100];
  
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &np);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  printf("[%d of %d] started\n", rank, np);
  
  if (parseArgs(argc, argv, &opt)) {
    MPI_Finalize();
    return 1;
  }

  int p, i, s, err = 0;
  for (p=0; p < PATTERN_COUNT; p++) {
    for (i=0; i < IOLIB_COUNT; i++) {
      for (s=0; s < SYNC_COUNT; s++) {

        snprintf(filename, sizeof filename - 1,
                 "conflict_app.out.%s.%s.%s",
                 PATTERN_NAMES[p],
                 IOLIB_NAMES[i],
                 SYNC_NAMES[s]);
        
        if (opt.do_init) {
          if (rank == 0) {
            err = err || initializeFile(filename, p, i, s, &opt);
          }
        } else {
          if (rank < 2) {
            doIO(filename, p, i, s, &opt);
          }
        }
      }
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);
  printf("[%d] finalizing\n", rank);
  MPI_Finalize();
  
  return err;
}


void printHelp() {
  if (rank == 0) {
    fprintf(stderr, "\n"
            "  conflict_app [options]\n"
            "  options:\n"
            "    -init : (run this first) initialize the data files\n"
            "    -size <size> : total # of bytes in each operation (accepts \n"
            "       suffixes k,m,g,t). Default = 10m\n"
            "    -block <size> : amount of data in each IO call. Default = 1m\n"
            "    -sleep <sec> : the number of seconds rank 1 will sleep when\n"
            "       there is no other synchronization\n"
            "\n");
  }
  MPI_Finalize();
  exit(1);
}


int parseArgs(int argc, char **argv, Options *opt) {
  opt->do_init = 0;
  opt->size = 10 * 1024 * 1024;
  opt->block_size = 1024 * 1024;
  opt->wait_seconds = 2.0;

  int i;
  uint64_t tmp;

  for (i=1; i < argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-init")) {
      opt->do_init = 1;
    }

    else if (!strcmp(arg, "-size")) {
      i++;
      if (i >= argc) printHelp();
      if (parseSize(argv[i], &tmp)) {
        if (rank==0) {
          fprintf(stderr, "Invalid size: \"%s\"\n", argv[i]);
        }
        return 1;
      }
      opt->size = tmp;
    }

    else if (!strcmp(arg, "-block")) {
      i++;
      if (i >= argc) printHelp();
      if (parseSize(argv[i], &tmp)) {
        if (rank==0) {
          fprintf(stderr, "Invalid block size: \"%s\"\n", argv[i]);
        }
        return 1;
      }
      opt->block_size = tmp;
    }

    else if (!strcmp(arg, "-sleep")) {
      i++;
      if (i >= argc) printHelp();
      if (1 != sscanf(argv[i], "%lf", &opt->wait_seconds)) {
        if (rank==0) {
          fprintf(stderr, "Invalid number of seconds: \"%s\"\n", argv[i]);
        }
        return 1;
      }
      if (opt->wait_seconds < 0) opt->wait_seconds = 0;
    }

    else {
      printHelp();
    }
  }
  return 0;
}


/* Parse an unsigned number with a case-insensitive magnitude suffix:
     k : multiply by 2^10
     m : multiply by 2^20
     g : multiply by 2^30
     t : multiply by 2^40
     p : multiply by 2^50
     x : multiply by 2^60

   For example, "32m" would parse as 33554432.
   Floating point numbers are allowed in the input, but the result is always
   a 64-bit integer:  ".5g" yields uint64_t(.5 * 2^30)
   Return nonzero on error.
*/
static const uint64_t SIZE_FACTORS[6] = {
  (uint64_t)1 << 10,
  (uint64_t)1 << 20,
  (uint64_t)1 << 30,
  (uint64_t)1 << 40,
  (uint64_t)1 << 50,
  (uint64_t)1 << 60
};
static const char SIZE_SUFFIXES[6] = {'k', 'm', 'g', 't', 'p', 'x'};
static const int SIZES_COUNT = 6;
int parseSize(const char *str, uint64_t *result) {
  uint64_t multiplier;
  const char *last;
  char suffix;
  int consumed, i;
  double mantissa;
  
  /* missing argument check */
  if (!str || !str[0] || (!isdigit((int)str[0]) && str[0] != '.')) return 1;

  if (1 != sscanf(str, "%lf%n", &mantissa, &consumed))
    return 1;
  last = str + consumed;

  /* these casts are to avoid issues with signed chars */
  suffix = (char)tolower((int)*last);
  
  if (suffix == 0) {

    multiplier = 1;

  } else {
    
    for (i=0; i < SIZES_COUNT; i++) {
      if (suffix == SIZE_SUFFIXES[i]) {
        break;
      }
    }
    
    /* unrecognized suffix */
    if (i == SIZES_COUNT)
      return 1;

    multiplier = SIZE_FACTORS[i];
  }

  *result = (uint64_t)(multiplier * mantissa);
  return 0;
}


int writeStuff(int byte_value, int64_t size, int64_t block_size,
               int fd, const char *filename) {
  int err = 0;
  char *buf = (char*) malloc(block_size);
  assert(buf);
  memset(buf, byte_value, block_size);

  int64_t bytes_written = 0;
  while (bytes_written < size) {
    size_t write_len = MIN(size - bytes_written, block_size);
    int64_t b = write(fd, buf, write_len);
    bytes_written += b;
    if (b != write_len) {
      fprintf(stderr, "[%d] short write (%" PRIi64 " of %" PRIi64
              " bytes) to %s\n", rank, bytes_written, size, filename);
      err = 1;
      break;
    }
  }

  free(buf);
  return err;
}


int readStuff(int64_t size, int64_t block_size,
              int fd, const char *filename) {
  int err = 0;
  char *buf = (char*) malloc(block_size);
  assert(buf);

  int64_t bytes_read = 0;
  while (bytes_read < size) {
    size_t read_len = MIN(size - bytes_read, block_size);
    int64_t r = read(fd, buf, read_len);
    bytes_read += r;
    if (r != read_len) {
      fprintf(stderr, "[%d] short read (%" PRIi64 " of %" PRIi64
              " bytes) to %s\n", rank, bytes_read, size,
              filename);
      err = 1;
      break;
    }
  }

  free(buf);
  return err;
}


int initializeFile(const char *filename, int pattern_idx, int iolib_idx,
                    int sync_idx, Options *opt) {
  printf("[%d] initializing %s\n", rank, filename);

  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0664);
  if (fd < 0) {
    fprintf(stderr, "[%d] failed to create %s\n", rank, filename);
    return 1;
  }

  int err = writeStuff(0, opt->size, opt->block_size, fd, filename);
  close(fd);
  return err;
}


void doIO(const char *filename, int pattern_idx, int iolib_idx,
          int sync_idx, Options *opt) {
  printf("[%d] running\n", rank);
}

