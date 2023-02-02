default: all

EXECS = test_lustre_scan_files

all: $(EXECS)

USE_LUSTRE=-DNO_LUSTRE

OPT = -g
# OPT = -O3
CC = gcc -Wall $(OPT) $(USE_LUSTRE)
CXX = g++ -std=c++14 -Wall $(OPT) $(USE_LUSTRE)
MPICC = mpicc $(OPT) $(USE_LUSTRE)
MPICXX = mpicxx -std=c++14 -Wall $(OPT) $(USE_LUSTRE)

test_lustre_scan_files: test_lustre_scan_files.cc lustre_scan_files.o \
  lustre_wrapper.o canonical_path.o
	$(CXX) $^ -o $@

lustre_wrapper.o: lustre_wrapper.c lustre_wrapper.h
	$(CC) -c $<

lustre_scan_files.o: lustre_scan_files.cc lustre_scan_files.hh
	$(CXX) -c $<

canonical_path.o: canonical_path.cc canonical_path.hh
	$(CXX) -c $<

clean:
	rm -f $(EXECS) *.exe *.o *.stackdump *~

