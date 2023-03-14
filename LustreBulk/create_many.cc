/*
  Create many dummy files on a Lustre system.

  count=10000; size=1m; sc=1; sl=1m; ./create_many  $SCRATCH/read_test/$size.$sc.$sl. $count $size $sc $sl
  count=10; size=1g; sc=4; sl=1m; ./create_many  $SCRATCH/read_test/$size.$sc.$sl. $count $size $sc $sl
  count=1; size=100g; sc=16; sl=16m; ./create_many  $SCRATCH/read_test/$size.$sc.$sl. $count $size $sc $sl
*/

const char *help_str = "\n"
  "  create_many <name_prefix> <count> <size> [stripe_count [stripe_length]]\n"
  "\n"
  "  Creates many dummy files with the given size and lustre striping parameters.\n"
  "  To create 100 1GB files in $SCRATCH/Testdir with 4x1MB striping:\n"
  "    create_many $SCRATCH/Testdir/file. 100 1g 4 1m\n"
  "\n";

// const int BUF_SIZE = 64*1024*1024;
const int BUF_SIZE = 1000000;


#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <vector>
#include <string>
#include "lustre_wrapper.h"

using std::string;
using std::vector;

#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)>(b) ? (a) : (b))

void printHelp() {
  fputs(help_str, stderr);
  exit(1);
}


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
int parseSize(const char *str, uint64_t *result) {
  uint64_t multiplier = 1;
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
    
    /* unrecognized suffix--ignore it */
    if (i == SIZES_COUNT)
      multiplier = 1;
    else
      multiplier = SIZE_FACTORS[i];
  }

  *result = (uint64_t)(multiplier * mantissa);
  return 0;
}


double getSeconds() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + 1e-9 * t.tv_nsec;
}


struct Options {
  string name_prefix;
  int count;
  long size;
  int stripe_count;
  long stripe_length;

  bool parseArgs(int argc, char **argv) {
    if (argc < 4) printHelp();

    uint64_t tmp;
    stripe_count = 1;
    stripe_length = 1<<20;

    char *end;
    int argno = 1;
    char *arg = argv[argno];
    
    name_prefix = arg;
    
    arg = argv[++argno];
    count = strtol(arg, &end, 10);
    if (end == arg || count <= 0) {
      printf("Invalid count: %s\n", arg);
      return false;
    }

    arg = argv[++argno];
    if (parseSize(arg, &tmp)) {
      printf("Invalid size: %s\n", arg);
      return false;
    }
    size = tmp;

    arg = argv[++argno];
    if (arg) {
      stripe_count = strtol(arg, &end, 10);
      if (end == arg || stripe_count <= 0) {
        printf("Invalid stripe count: %s\n", arg);
        return false;
      }

      arg = argv[++argno];
      if (arg) {
        if (parseSize(arg, &tmp)) {
          printf("Invalid stripe length: %s\n", arg);
          return false;
        }
        stripe_length = tmp;
      }
    }
    
    return true;
  }
    
};


// returns number of digits in positive value n
int nDigits(int n) {
  if (n > 999999999) return 10;
  int compare = 9, len = 1;
  while (n > compare) {
    compare = compare * 10 + 9;
    len++;
  }
  return len;
}


// return nonzero on error
int createFile(const char *name, long size, const char *buf, int stripe_count,
               long stripe_length) {

  int fd = lustre_create_striped_open(name, 0644, stripe_count, stripe_length,
                                      -1);
  if (fd <= 0) return -1;

  long offset = 0;
  while (true) {
    long remaining = size - offset;
    if (remaining <= 0) break;
    int write_len = MIN(BUF_SIZE, remaining);
    ssize_t written = write(fd, buf, write_len);
    if (written != write_len) {
      printf("Error: out of space? short write to %s, %d of %d at offset %ld\n",
             name, (int)written, write_len, offset);
      close(fd);
      return -1;
    }
    // printf("  wrote %d bytes at %ld\n", write_len, offset);
    offset += write_len;
  }

  return 0;
}


int main(int argc, char **argv) {
  Options opt;

  if (!opt.parseArgs(argc, argv)) return 1;

  /*
  printf("Create %d files of %ld bytes with prefix %s, stripe_count=%d "
         "stripe_length=%ld\n",
         opt.count, opt.size, opt.name_prefix.c_str(), opt.stripe_count,
         opt.stripe_length);
  */

  char *buf = (char*) malloc(BUF_SIZE);
  assert(buf);
  memset(buf, 255, BUF_SIZE);

  char *name = (char*) malloc(opt.name_prefix.length() + 100);
  assert(name);

  char format[100];
  int n_digits = nDigits(opt.count-1);
  // prefix.index
  // %s%06d
  sprintf(format, "%%s%%0%dd", n_digits);

  int files_created = 0;
  long bytes_written = 0;
  
  double timer = getSeconds();
  
  for (int i=0; i < opt.count; i++) {
    sprintf(name, format, opt.name_prefix.c_str(), i);
    // printf("Create %s\n", name);
    if (createFile(name, opt.size, buf, opt.stripe_count, opt.stripe_length)) {
      printf("Failed to create %s: %s (errno %d)\n", name, strerror(errno), errno);
      break;
    }
    files_created++;
    bytes_written += opt.size;
  }

  timer = getSeconds() - timer;
  double mb = (double)bytes_written / (1<<20);
  printf("Created %d files, wrote %ld bytes in %.6f sec, or %.3f MB/sec\n",
         files_created, bytes_written, timer, mb / timer);

  free(name);
  free(buf);

  return 0;
}
