/* Read many files efficiently from a Lustre file system by aligning
   Lustre stripes with IO nodes. 

   Lustre introduces locking when one OST is being accessed by multiple
   nodes, so we can improve access speeds by mapping OSTs to nodes such
   that only one node ever access data on a given OST. 

   This scans a list of input files, queries the file system for which
   OSTs hold the content for each file, and creates a list of content
   for each OST.

   It then distributes those lists of content to IO ranks to read the data.

   For comparison, the files are also read in simpler ways:
    - one process reads everything
    - reading assignments are distributed to all ranks

   TODO:
    - start sending tasks to IO ranks while scanning. The initial version
      of the code finishes scanning before starting to read.

   Ed Karrels, ed.karrels@gmail.com, January 2023
*/


#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits.h>
#include <map>
#include <mpi.h>
#include <string>
#include <vector>
#include "lustre_scan_files.hh"


using std::vector;
using std::string;
using std::map;


int np, rank, node_count, node_idx, node_size, rank_on_node;
double t0;
MPI_Comm comm;

const int N_TESTS = 3;

enum Tags {TAG_CONTENT1 = 100, TAG_CONTENT2, TAG_CONTENT3, TAG_CONTENT4};

struct Options {
  vector<string> file_list;

  bool parseArgs(int argc, char **argv);
};


double getTime() {
  return MPI_Wtime() - t0;
}


/* Computes how processes have been mapped to nodes.
   node_count: total number of nodes
   node_idx: index (0..node_count-1) of this node
   node_size: number of ranks on this node
   rank_on_node: index (0..node_size-1) of this process on this node.
*/
void nodeMapping(int *node_count, int *node_idx, int *node_size,
                 int *rank_on_node) {

  MPI_Comm in_comm = MPI_COMM_WORLD, node_comm, leader_comm;
  int rank;

  MPI_Comm_rank(in_comm, &rank);

  /* create independent communicators for each node */
  MPI_Comm_split_type(in_comm, MPI_COMM_TYPE_SHARED, rank, MPI_INFO_NULL,
                      &node_comm);

  MPI_Comm_rank(node_comm, rank_on_node);
  MPI_Comm_size(node_comm, node_size);

  /* create communicator of just the leaders */
  int color = (*rank_on_node)==0 ? 0 : MPI_UNDEFINED;
  MPI_Comm_split(in_comm, color, rank, &leader_comm);
  
  int sz[2];
  if (color == 0) {  /* is a leader */
    MPI_Comm_size(leader_comm, node_count);
    MPI_Comm_rank(leader_comm, node_idx);
    sz[0] = *node_count;
    sz[1] = *node_idx;
  }

  /* independent bcast on each node */
  MPI_Bcast(sz, 2, MPI_INT, 0, node_comm);
  if (color != 0) {
    *node_count = sz[0];
    *node_idx = sz[1];
  }

  MPI_Comm_free(&node_comm);
  if (leader_comm != MPI_COMM_NULL) MPI_Comm_free(&leader_comm);
}


/* Root gathers all filenames into one large character array, each one terminated
   with a null character. Then it makes the following broadcasts:
    - total number of files
    - total size of all-filenames array
    - content of all-filenames
    - array containing all file sizes
*/
void broadcastFileSet(FileSet &all_files, int root) {
  vector<char> all_filenames;
  vector<uint64_t> all_file_sizes;
  int file_count;
  int all_filenames_size;

  if (rank == root) {
    all_filenames.reserve(10000);
    all_file_sizes.reserve(all_files.size());
    for (auto &it : all_files) {
      const char *s = it.first.c_str();
      all_filenames.insert(all_filenames.end(), s, s + it.first.length() + 1);
      all_file_sizes.push_back(it.second);
    }

    if (all_filenames.size() > INT_MAX) {
      fprintf(stderr, "[%d] ERROR: %ld bytes of filenames (can only handle 2GB)\n", rank, (long)all_filenames.size());
    }
    
    file_count = (int) all_files.size();
    all_filenames_size = (int) all_filenames.size();
    MPI_Bcast(&file_count, 1, MPI_INT, root, comm);
    MPI_Bcast(&all_filenames_size, 1, MPI_INT, root, comm);
    MPI_Bcast(all_filenames.data(), all_filenames_size, MPI_BYTE, root, comm);
    MPI_Bcast(all_file_sizes.data(), file_count, MPI_UINT64_T, root, comm);
    
  } else {
    MPI_Bcast(&file_count, 1, MPI_INT, root, comm);
    all_file_sizes.resize(file_count);
    MPI_Bcast(&all_filenames_size, 1, MPI_INT, root, comm);
    all_filenames.resize(all_filenames_size);
    MPI_Bcast(all_filenames.data(), all_filenames_size, MPI_BYTE, root, comm);
    MPI_Bcast(all_file_sizes.data(), file_count, MPI_UINT64_T, root, comm);

    all_files.clear();
    const char *fn = all_filenames.data();
    for (int i=0; i < file_count; i++) {
      all_files[fn] = all_file_sizes[i];
      fn += strlen(fn) + 1;
    }
  }

  /*
  for (int r=1; r < np; r++) {
    MPI_Barrier(comm);
    if (r == rank) {
      printf("[%d] %d files:\n", rank, (int)all_files.size());
      for (auto &it : all_files) {
        printf("  %10ld %s\n", (long)it.second, it.first.c_str());
      }
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 500*1000*1000;
      nanosleep(&ts, NULL);
    }
  }
  MPI_Barrier(comm);
  */
}


void gatherRootContent(vector<StridedContent> &my_content,
                       const OSTContentMap &ost_content,
                       const map<int,int> &map_ost_to_node) {

  for (auto &ost_iter : ost_content) {
    int ost_id = ost_iter.first;
    auto lookup_it = map_ost_to_node.find(ost_id);
    assert(lookup_it != map_ost_to_node.end());
    if (lookup_it->second == 0) {
      const vector<StridedContent> &list = ost_iter.second;
      my_content.insert(my_content.end(), list.begin(), list.end());
    }
  }
  
}


/* Send the content for all the OSTs which map to dest_rank to dest_rank.
   Like broadcastFileSet this first gathers the data into contiguous memory areas,
   the sends it in a few messages.
    - total number of StridedContent objects
    - total size of all-filenames array
    - content of all-filenames
    - array containing (file_count) copies of {offset,length,stride,file_size}
 */
void sendContent(int dest_rank,
                 const OSTContentMap &ost_content,
                 const map<int,int> &map_ost_to_node) {
  vector<char> all_filenames;
  vector<uint64_t> all_values;
  int count = 0;

  all_filenames.reserve(10000);
  all_values.reserve(100);
  
  for (auto &it : ost_content) {
    int ost_idx = it.first;
    auto map_it = map_ost_to_node.find(ost_idx);
    assert(map_it != map_ost_to_node.end());
    if (map_it->second != dest_rank) continue;
    const vector<StridedContent> &v = it.second;
    count += v.size();
    for (const StridedContent &sc : v) {
      const char *s = sc.file_name.c_str();
      all_filenames.insert(all_filenames.end(), s, s + sc.file_name.length() + 1);
      all_values.push_back(sc.offset);
      all_values.push_back(sc.length);
      all_values.push_back(sc.stride);
      all_values.push_back(sc.file_size);
    }
  }

  MPI_Send(&count, 1, MPI_INT, dest_rank, TAG_CONTENT1, comm);
  uint64_t all_filenames_len = all_filenames.size();
  if (all_filenames.size() > INT_MAX) {
    fprintf(stderr, "[%d] ERROR: %ld bytes of filenames (can only handle 2GB)\n", rank, (long)all_filenames.size());
  }
  MPI_Send(&all_filenames_len, 1, MPI_UINT64_T, dest_rank, TAG_CONTENT2, comm);
  MPI_Send(all_filenames.data(), (int) all_filenames.size(), MPI_BYTE, dest_rank, TAG_CONTENT3, comm);
  MPI_Send(all_values.data(), count * 4, MPI_UINT64_T, dest_rank, TAG_CONTENT4, comm);
}


void receiveContent(vector<StridedContent> &my_content, int source_rank) {
  // receive the packed data
  int count;
  MPI_Recv(&count, 1, MPI_INT, source_rank, TAG_CONTENT1, comm, MPI_STATUS_IGNORE);
  uint64_t all_filenames_len;
  MPI_Recv(&all_filenames_len, 1, MPI_UINT64_T, source_rank, TAG_CONTENT2, comm, MPI_STATUS_IGNORE);
  vector<char> all_filenames(all_filenames_len);
  MPI_Recv(all_filenames.data(), all_filenames_len, MPI_BYTE, source_rank, TAG_CONTENT3, comm, MPI_STATUS_IGNORE);
  vector<uint64_t> all_values(count * 4);
  MPI_Recv(all_values.data(), count * 4, MPI_UINT64_T, source_rank, TAG_CONTENT4, comm, MPI_STATUS_IGNORE);

  // unapck it into my_content
  my_content.clear();
  my_content.reserve(count);
  const char *fn = all_filenames.data();
  for (int i=0; i < count; i++) {
    my_content.emplace_back(fn, all_values[i*4], all_values[i*4+1], all_values[i*4+2], all_values[i*4+3]);
  }
}


void printContentList(const vector<StridedContent> &my_content) {
  printf("[%d] %d entries:\n", rank, (int) my_content.size());
  for (const StridedContent &sc : my_content) {
    printf("  %s {%lu,%lu,%lu,%lu}\n", sc.file_name.c_str(), sc.offset, sc.length, sc.stride, sc.file_size);
  }
}


bool Options::parseArgs(int argc, char **argv) {
  // no options yet

  file_list.insert(file_list.end(), argv + 1, argv + argc);

  return true;
}


int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);

  // initial setup
  comm = MPI_COMM_WORLD;
  MPI_Comm_size(comm, &np);
  MPI_Comm_rank(comm, &rank);
  nodeMapping(&node_count, &node_idx, &node_size, &rank_on_node);
  MPI_Barrier(comm);
  t0 = MPI_Wtime();

  Options opt;
  
  if (!opt.parseArgs(argc, argv)) {
    MPI_Finalize();
    return 0;
  }

  // this describes the data this rank will read
  vector<StridedContent> my_content;
  uint64_t total_bytes = 0;
  // double total_mb = 0;
  FileSet all_files;

  // scan all the input files on rank 0, then distribute work to other ranks
  if (rank == 0) {
    printf("[%d] %.6f Scanning files...\n", rank, getTime());
    double scan_timer = MPI_Wtime();

    OSTContentMapper mapper;
    
    int file_count = scanLustreFiles(opt.file_list, &mapper, all_files);
    if (file_count == 0) {
      printf("No files found!\n");
    }
    scan_timer = MPI_Wtime() - scan_timer;
    printf("[%d] %.6f %d files scanned in %.6fs\n", rank, getTime(), (int) all_files.size(),
           scan_timer);

    for (auto &it : all_files)
      total_bytes += it.second;
    // total_mb = (double)total_bytes / (1<<20);

    broadcastFileSet(all_files, 0);
    printf("%ld total bytes\n", (long)total_bytes);

    // map the OSTs seen in the file list to nodes in this job
    map<int,int> map_ost_to_node;
    int map_node_idx = 0;
    for (auto &ost : mapper.ost_content) {
      map_ost_to_node[ost.first] = map_node_idx;
      printf("OST %d -> node %d\n", ost.first, map_node_idx);
      if (++map_node_idx == node_count)
        map_node_idx = 0;
    }

    // Send all the tasks for a node to the leader rank on that node.
    // That node will then distribute the work across other ranks on the node.

    gatherRootContent(my_content, mapper.ost_content, map_ost_to_node);


    for (int map_node_idx = 1; map_node_idx < node_count; map_node_idx++) {
      int dest_rank = node_size * map_node_idx;
      sendContent(dest_rank, mapper.ost_content, map_ost_to_node);
    }

    
  } else {
    broadcastFileSet(all_files, 0);

    // leader ranks on nodes 1..(n-1)
    if (rank_on_node == 0) {
      receiveContent(my_content, 0);
    }
      
      // TODO distribute work to other ranks on this node
  }

  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 500*1000*1000;
  for (int r=0; r < np; r++) {
    MPI_Barrier(comm);
    if (rank == r) {
      printContentList(my_content);
      nanosleep(&ts, NULL);
    }
  }
  MPI_Barrier(comm);

#if 0
  for (int i=0; i < N_TESTS; i++) {
    double timer_single, timer_all, timer_aligned;

    // single reader
    if (rank == 0) {
      timer_single = MPI_Wtime();
      singleReader(opt.file_list);
      timer_single = MPI_Wtime() - timer_single;
    }

    // all ranks read, selecting their files round-robin from all_files
    MPI_Barrier(comm);
    timer_all = MPI_Wtime();
    allRanksRead(all_files);
    MPI_Barrier(comm);
    timer_all = MPI_Wtime() - timer_all;

    // aligned read
    timer_aligned = MPI_Wtime();
    alignedRead(my_content);
    MPI_Barrier(comm);
    timer_aligned = MPI_Wtime() - timer_aligned;

    if (rank == 0) {
      printf("Test %d\n", i);
      printf("  single reader: %.6fs, %.3f mb/s\n", timer_single, total_mb / timer_single);
      printf("  all ranks read: %.6fs, %.3f mb/s\n", timer_all, total_mb / timer_all);
      printf("  aligned readers: %.6fs, %.3f mb/s\n", timer_aligned, total_mb / timer_aligned);
    }
  }
#endif
  

  MPI_Finalize();
  return 0;
}
