/*
  darshan_dxt_conflicts - reads the output of darshan-dxt-parser (which
  contains per-call data on each read or write) and outputs any conflicts
  found.

  A conflict is when a pair of events A and B are found such that:
   - A and B access the same file (A.file_hash == B.file_hash)
   - A and B came from different processes (A.rank != B.rank)
   - A and B access overlapping byte ranges
     (A.offset < (B.offset+B.length) && ((A.offset+A.length) > B.offset)
   - At least one of the accesses is a write. (A.mode==WRITE || B.mode==WRITE)


  Sample input:

# DXT, file_id: 8515199880342690440, file_name: /mnt/c/Ed/UI/Research/Darshan/conflict_app.out.RAW.POSIX.NONE
# DXT, rank: 0, hostname: XPS13
# DXT, write_count: 10, read_count: 0
# DXT, mnt_pt: /mnt/c, fs_type: 9p
# Module    Rank  Wt/Rd  Segment          Offset       Length    Start(s)      End(s)
 X_POSIX       0  write        0               0         1048576      4.8324      4.8436
 X_POSIX       0  write        1         1048576         1048576      4.8436      4.8534
 X_POSIX       0  write        2         2097152         1048576      4.8534      4.8637
 X_POSIX       0  write        3         3145728         1048576      4.8638      4.8706
 X_POSIX       0  write        4         4194304         1048576      4.8706      4.8769
 X_POSIX       0  write        5         5242880         1048576      4.8769      4.8832
 X_POSIX       0  write        6         6291456         1048576      4.8832      4.8892
 X_POSIX       0  write        7         7340032         1048576      4.8892      4.8944
 X_POSIX       0  write        8         8388608         1048576      4.8944      4.9000
 X_POSIX       0  write        9         9437184         1048576      4.9000      4.9059

# DXT, file_id: 8515199880342690440, file_name: /mnt/c/Ed/UI/Research/Darshan/conflict_app.out.RAW.POSIX.NONE
# DXT, rank: 1, hostname: XPS13
# DXT, write_count: 0, read_count: 10
# DXT, mnt_pt: /mnt/c, fs_type: 9p
# Module    Rank  Wt/Rd  Segment          Offset       Length    Start(s)      End(s)
 X_POSIX       1   read        0               0         1048576      6.8327      6.8392
 X_POSIX       1   read        1         1048576         1048576      6.8392      6.8434
 X_POSIX       1   read        2         2097152         1048576      6.8434      6.8473
 X_POSIX       1   read        3         3145728         1048576      6.8473      6.8513
 X_POSIX       1   read        4         4194304         1048576      6.8513      6.8553
 X_POSIX       1   read        5         5242880         1048576      6.8553      6.8601
 X_POSIX       1   read        6         6291456         1048576      6.8601      6.8639
 X_POSIX       1   read        7         7340032         1048576      6.8639      6.8673
 X_POSIX       1   read        8         8388608         1048576      6.8673      6.8709
 X_POSIX       1   read        9         9437184         1048576      6.8709      6.8747

*/

#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <string>
#include <cinttypes>
#include <cstdint>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <regex>

using namespace std;


class Access {
public:
  const int rank;
  const enum Mode {READ, WRITE} mode;
  const int64_t offset, length;
  const double start_time, end_time;

  Access(int rank_, enum Mode mode_, int64_t offset_, int64_t length_,
         double start_time_, double end_time_)
    : rank(rank_), mode(mode_), offset(offset_), length(length_),
      start_time(start_time_), end_time(end_time_) {}
  
  bool overlaps(const Access &other) const;

  /* If all accesses are done in terms of blocks of data, set this to
     the block size so overlaps can be computed correctly.

     For example, let block_size be 100. Then every read or write to disk
     occurs in blocks of 100 bytes. If P0 wants to overwrite bytes 0..3, it
     will need to read bytes 0..99 from disk, overwrite the first four bytes,
     then write bytes 0..99 to disk. If P1 writes bytes 96..99 with no
     synchronization, it may complete its operation after P0 read the block
     and before P0 wrote the block. Then when P0 writes its block, it will
     overwrite P1's changes.

     AFAICT, this will only be an issue in write-after-write (WAW) situations.
     In RAW or WAR situations, if the byte range doesn't actually overlap, the
     read will get the same result whether preceding write completed or not.
  */     
  static int block_size;
  static void setBlockSize(int b) {
    block_size = b;
  }

  bool overlapsBlocks(const Access &other) const;

  // round down an offset to the beginning of a block
  static int64_t blockStart(int64_t offset);

  // round up an offset to the end of a block
  static int64_t blockEnd(int64_t offset);
};

int Access::block_size = 1;


class File {
public:
  const string id;  // a hash of the filename generated by Darshan
  const string name;

  // ordered by offset
  // named "a" just so I don't have to type accesses a lot
  vector<Access> a;

  File(const string &id_, const string &name_) : id(id_), name(name_) {}
};


typedef unordered_map<uint64_t, unique_ptr<File>> FileTableType;

int readDarshanDxtInput(istream &in, FileTableType &file_table);


int main(int argc, char **argv) {
  FileTableType file_table;

  // Access a(0, Access::READ, 0, 100, 1.0, 1.25);
  // cout << a.offset << ".." << (a.offset + a.length - 1) << endl;

  readDarshanDxtInput(cin, file_table);

  return 0;
}


bool Access::overlaps(const Access &other) const {
  return (offset < (other.offset + other.length))
    && ((offset+length) > other.offset);
}


bool Access::overlapsBlocks(const Access &other) const {
  int64_t this_start, this_end, other_start, other_end;

  this_start = blockStart(offset);
  this_end = blockEnd(offset + length - 1);

  other_start = blockStart(other.offset);
  other_end = blockEnd(other.offset + other.length - 1);
  
  return (this_start < other_end) && (this_end > other_start);
}


// round down an offset to the beginning of a block
int64_t Access::blockStart(int64_t offset) {
  return offset - (offset % block_size);
}

// round up an offset to the end of a block
int64_t Access::blockEnd(int64_t offset) {
  return blockStart(offset) + block_size - 1;
}


bool startsWith(const string &s, const char *search_str) {
  size_t search_len = strlen(search_str);
  return s.length() >= search_len
    && 0 == s.compare(0, search_len, search_str);
}


int readDarshanDxtInput(istream &in, FileTableType &file_table) {
  string line;

  regex section_header_re("^# DXT, file_id: ([0-9]+), file_name: (.*)$");
  regex rank_line_re("^# DXT, rank: ([0-9]+),");
  smatch re_matches;
  
  while (true) {

    // skip until the beginning of a section is found
    bool section_found = false;
    while (true) {
      if (!getline(in, line)) break;
      if (regex_search(line, re_matches, section_header_re)) {
        section_found = true;
        break;
      }
    }
    if (!section_found) break;
    
    assert(re_matches.size() == 3);
    string file_id_str = re_matches[1];
    string file_name = re_matches[2];

    // find the line with the rank id
    bool rank_found = false;
    while (true) {
      if (!getline(in, line)) break;
      if (regex_search(line, re_matches, rank_line_re)) {
        rank_found = true;
        break;
      }
    }
    if (!rank_found) break;

    int rank = stoi(re_matches[1]);

    cout << "section rank=" << rank << " id=" << file_id_str << " "
         << file_name << endl;

    // read until a blank line at the end of the section or EOF
    while (true) {
      if (!getline(in, line)) break;
      if (!line.compare(0, 
  }

  return 0;
}
