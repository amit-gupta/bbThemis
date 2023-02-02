default: all

EXECS = test_lustre_scan_files bulk_lustre_read

all: $(EXECS)

HAVE_LUSTRE := $(shell command -v lfs 2> /dev/null)
ifdef HAVE_LUSTRE
LUSTRE_FLAG=-DHAVE_LUSTRE
LUSTRE_LIB=-llustreapi
else
LUSTRE_FLAG=-DNO_LUSTRE
endif


OPT = -g
# OPT = -O3
CFLAGS = -Wall -D_GNU_SOURCE $(OPT) $(LUSTRE_FLAG)
CC = gcc $(CFLAGS)
CXX = g++ -std=c++14 $(CFLAGS)
MPICC = mpicc $(CFLAGS)
MPICXX = mpicxx -std=c++14 $(CFLAGS)


bulk_lustre_read: bulk_lustre_read.cc lustre_scan_files.o \
  lustre_wrapper.o canonical_path.o
	$(MPICXX) $^ $(LUSTRE_LIB) -o $@

test_lustre_scan_files: test_lustre_scan_files.cc lustre_scan_files.o \
  lustre_wrapper.o canonical_path.o
	$(CXX) $^ $(LUSTRE_LIB) -o $@

lustre_wrapper.o: lustre_wrapper.c lustre_wrapper.h
	$(CC) -c $<

lustre_scan_files.o: lustre_scan_files.cc lustre_scan_files.hh
	$(CXX) -c $<

canonical_path.o: canonical_path.cc canonical_path.hh
	$(CXX) -c $<

clean:
	rm -f $(EXECS) *.exe *.o *.stackdump *~

