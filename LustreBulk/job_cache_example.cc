/*
  Simplified example of using a background thread on each node in MPI job
  to serve as a distributed cache.

  Run this with one filename as a command line argument.
  That file should be a list of files, one per line, which
  will be read by every process. Each process will randomly
  reorder the list so accesses don't get serialized.

  For example:
    IBRUN_TASKS_PER_NODE=16 ibrun -n 32 ./job_cache_example ~edk/tmp/python_files.txt
  python_files.txt is a list of 27930 python library files in
  /opt/apps/intel19/python3/3.7.0/lib.

  One background thread will be started on each node, and those will
  serve as cache servers. Files are distributed across servers according
  to a hash of the filename. server_idx = hash(filename) % server_count

  MPI must be initialized with MPI_THREAD_MULTIPLE, since background threads will
  be making MPI calls.

  Important: when making MPI calls in a background thread, if another thread
  calls MPI_Finalize the background thread will crash. Be sure to shut down
  the background thread before calling MPI_Finalize.

  Limitations:
   - This is just a demo designed for small files. It may fail on large files
     and will definitely fail for files larger than 2GB.
   - The cache is not purged. Once a file is read, it will remain in the cache.
*/

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cassert>
#include <algorithm>
#include <random>
#include <atomic>
#include "job_cache_example.hh"


// Before calling MPI_Finalize, use this to check that the server thread has ended.
std::atomic<int> server_thread_running(0);


int main(int argc, char **argv) {
  int thread_support;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &thread_support);
  int np, rank;

  MPI_Comm_size(MPI_COMM_WORLD, &np);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  // printf("[%d] pid=%ld\n", rank, (long)getpid());

  if (thread_support != MPI_THREAD_MULTIPLE) {
    if (rank==0) {
      fprintf(stderr, "Error: job_cache_example requires MPI thread support %d, but got %d\n",
              MPI_THREAD_MULTIPLE, thread_support);
    }
    MPI_Finalize();
    return 1;
  }

  if (argc != 2) {
    if (rank==0) {
      fprintf(stderr, "\n"
              "  job_cache_example <file_list>\n"
              "\n"
              "  file_list: name of a file containing filenames, one per line\n"
              "  All ranks will use the distributed cache to read these files,\n"
              "  in a random order.\n"
              "\n");
    }
    MPI_Finalize();
    return 1;
  }

  const char *list_filename = argv[1];

  CacheClient *cache_client = startCacheServers();

  vector<char> content;
  if (cache_client->getFile(content, list_filename)) {
    printf("[%d] failed to read %s\n", rank, list_filename);
    MPI_Finalize();
    return 1;
  }

  int success_count = 0, error_count = 0;
  long bytes_read = 0;
  readFiles(rank, cache_client, content, success_count, error_count, bytes_read);
  printf("[%d] %d files read, %d failed, %ld total bytes read\n",
         rank, success_count, error_count, bytes_read);
  
  stopCacheServers(cache_client);

  if (server_thread_running.load()) {
    fprintf(stderr, "[%d] Error: server thread still running\n", rank);
  }

  MPI_Finalize();
  return 0;
}


/* Computes how processes have been mapped to nodes.
   node_count: total number of nodes
   node_idx: index (0..node_count-1) of this node
   node_size: number of ranks on this node
   rank_on_node: index (0..ranks_per_node-1) of this process on this node.
*/
void nodeMapping(int *node_count, int *node_idx, int *node_size, int *rank_on_node) {

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


/* Split input on a given character. Skip empty lines. */
void splitString(vector<string> &output_list, const vector<char> &input, char split_char) {
  output_list.clear();
  size_t pos = 0, end = input.size();
  while (pos < end) {
    size_t next = pos;
    while (next < end && input[next] != split_char) next++;
    if (next > pos) {
      output_list.emplace_back(&input[pos], next-pos);
    }
    pos = next + 1;
  }
}


/* Reorder a vector randomly. */
template <class T>
void shuffleVector(vector<T> &v) {
  std::default_random_engine engine;
  std::random_device dev;
  engine.seed(dev());

  for (size_t i=1; i < v.size(); i++) {
    // swap v[i] with a randomly chosen element [0..i]
    std::uniform_int_distribution<size_t> rand(0, i);
    size_t j = rand(engine);
    if (i != j) {
      T tmp = v[i];
      v[i] = v[j];
      v[j] = tmp;
    }
  }
}
  


/* Try to read a bunch of files.
   Read them in a random order, otherwise the accesses will get serialized.
   file_list_all - a list of input files, separated by newlines
   success_count - number of files successfully read
   error_count - number of files that couldn't be read
   bytes_read - total number of bytes read
*/
void readFiles(int rank, CacheClient *cache_client, const vector<char> &file_list_all,
               int &success_count, int &error_count, long &bytes_read) {

  // split newline-delimited list of files into a list of strings
  vector<string> file_list;
  splitString(file_list, file_list_all, '\n');

  // shuffle the list
  shuffleVector(file_list);

  vector<char> content;
  
  for (size_t i=0; i < file_list.size(); i++) {
    // printf("[%d] readfile %d %s\n", rank, (int)i, file_list[i].c_str());
    if (cache_client->getFile(content, file_list[i])) {
      error_count++;
      printf("[%d] failed to read %s\n", rank, file_list[i].c_str());
    } else {
      success_count++;
      bytes_read += content.size();
    }
  }
}


/* Collective call across all ranks in comm.
   Starts one CacheServer running on each node.
   Returns a CacheClient object. Caller is responsible for
   deleting the object. */
CacheClient* startCacheServers() {
  int node_count, node_idx, node_size, rank_on_node;
  
  nodeMapping(&node_count, &node_idx, &node_size, &rank_on_node);

  vector<int> server_ranks;
  for (int i=0; i < node_count; i++)
    server_ranks.push_back(i * node_size);

  // Create a communicators just for cache communications
  MPI_Comm cache_comm;
  MPI_Comm_dup(MPI_COMM_WORLD, &cache_comm);

  std::thread *server_thread = nullptr;

  // start server thread
  if (rank_on_node == 0) {
    server_thread = new std::thread(CacheServer::runCacheServer, cache_comm, node_idx);

    // Don't detach, because we need to insure the server thread has completed and will
    // not try to make any more MPI calls before its parent thread calls MPI_Finalize.
    // server_thread.detach();
  }

  CacheClient *client = new CacheClient(cache_comm, server_ranks, server_thread);

  return client;
}


/* Colletive call stopping cache servers and deallocating client objects. */
void stopCacheServers(CacheClient *&client) {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Barrier(MPI_COMM_WORLD);

  // if rank_on_node==0, tell the local server thread to shut down
  client->shutdown();

  MPI_Comm comm = client->getCacheComm();
  MPI_Comm_free(&comm);
  
  delete client;
  client = nullptr;
}


void CacheServer::runCacheServer(MPI_Comm comm, int node_idx) {
  server_thread_running.store(1);
  
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  // printf("[%d,node-%d] running CacheServer...\n", rank, node_idx);
  
  CacheServer server(comm, node_idx);
  server.loop();

  // printf("[server-%d] CacheServer exiting.\n", node_idx);
  
  server_thread_running.store(0);
}


void CacheServer::loop() {
  vector<char> msg_buf;
  string file_name;
  
  while (1) {

    // use MPI_Probe to get the source and size of the next request before receiving the message
    MPI_Status status;
    int src, msg_size;
    MPI_Probe(MPI_ANY_SOURCE, TAG_REQUEST, comm, &status);
    src = status.MPI_SOURCE;
    MPI_Get_count(&status, MPI_PACKED, &msg_size);

    // fprintf(stderr, "[server-%d] %d-byte request from %d\n", node_idx, msg_size, src);

    msg_buf.resize(msg_size);
    MPI_Recv(msg_buf.data(), msg_buf.size(), MPI_PACKED, src, TAG_REQUEST, comm, MPI_STATUS_IGNORE);
    // fprintf(stderr, "[server-%d] packed message received\n", node_idx);

    bool shutdown;
    unpackRequest(shutdown, file_name, msg_buf, comm);
    // fprintf(stderr, "[server-%d] message unpacked\n", node_idx);
    if (shutdown) {
      // fprintf(stderr, "[server-%d] shutdown request from %d\n", node_idx, src);
      break;
    }
      
    // fprintf(stderr, "[server-%d] request from %d of %s\n", node_idx, src, file_name.c_str());

    // this sends data directly from the cached copy
    // In a more robust system, make sure the cached copy is not invalidated between the
    // time we check if we have a cached copy and when MPI_Send completes.
    int length;
    char *content;
    getFile(file_name, length, content);
    
    // fprintf(stderr, "[server-%d] gotFile %d / %p\n", node_idx, length, content);

    MPI_Send(&length, 1, MPI_INT, src, TAG_FILE_LEN, comm);
    if (length >= 0) {
      // fprintf(stderr, "[server-%d] sending %p...\n", node_idx, content);
      MPI_Send(content, length, MPI_BYTE, src, TAG_FILE_CONTENT, comm);
      // fprintf(stderr, "[server-%d] %p sent.\n", node_idx, content);
    }
    
    // fprintf(stderr, "[server-%d] reply sent to %d\n", node_idx, src);
  }
  
  fprintf(stderr, "[server-%d] %d files served, %d cached, %.1f%% hit rate\n",
          node_idx, file_request_count, cached_request_count,
          100.0 * cached_request_count / file_request_count);
}


// Get the contents, either from cache or by reading the file.
// If the file cannot be read, set length to -1 and content to NULL.
void CacheServer::getFile(const string &name, int &length, char *&content) {

  // fail by default
  length = -1;
  content = 0;

  file_request_count++;

  // if the file is cached, just use that copy
  auto it = file_map.find(name);
  if (it != file_map.end()) {
    vector<char> &cached_copy = it->second;
    length = cached_copy.size();
    content = cached_copy.data();
    cached_request_count++;
    // fprintf(stderr, "[server-%d] serve cached copy of %s\n", node_idx, name.c_str());
    return;
  }

  // otherwise, read the file into the cache and then return a pointer to it

  struct stat statbuf;
  if (stat(name.c_str(), &statbuf)) {
    // file not found
    fprintf(stderr, "[server-%d] file not found: %s\n", node_idx, name.c_str());
    return;
  }

  long file_len = statbuf.st_size;

  int fd = open(name.c_str(), O_RDONLY);
  if (fd == -1) {
    // file not readable
    fprintf(stderr, "[server-%d] file not readable: %s\n", node_idx, name.c_str());
    return;
  }

  // create an empty entry in the hash map
  vector<char> content_tmp;
  std::pair<const string, vector<char>> insert_pair {name, content_tmp};
  auto inserted = file_map.insert(insert_pair);
  FileMap::iterator dest = inserted.first;
  vector<char> &content_in_map = dest->second;

  // read the file into the vector already in the hash map
  content_in_map.resize(file_len);
  ssize_t bytes_read = read(fd, content_in_map.data(), file_len);
  if (bytes_read != (ssize_t) file_len) {
    // read error
    fprintf(stderr, "[server-%d] read error: %s\n", node_idx, name.c_str());
    return;
  }

  length = file_len;
  content = content_in_map.data();
  // fprintf(stderr, "[server-%d] read %s into cache\n", node_idx, name.c_str());
}


/* Use MPI_Pack build a request message to the server */
void packRequest(vector<char> &buf, bool shutdown, const string &s, MPI_Comm comm) {

  int packed_msg_len = 0, item_len, pos = 0;

  // initial key, negative for shutdown
  MPI_Pack_size(1, MPI_INT, comm, &item_len);
  packed_msg_len += item_len;
  
  if (!shutdown) {
    MPI_Pack_size(1, MPI_INT, comm, &item_len);
    packed_msg_len += item_len;
    MPI_Pack_size(s.length(), MPI_BYTE, comm, &item_len);
    packed_msg_len += item_len;
  }
  
  buf.resize(packed_msg_len);

  int key;
  if (shutdown) {
    key = -1;
    MPI_Pack(&key, 1, MPI_INT, buf.data(), buf.size(), &pos, comm);
  } else {
    key = 0;
    MPI_Pack(&key, 1, MPI_INT, buf.data(), buf.size(), &pos, comm);
    int str_len = s.length();
    MPI_Pack(&str_len, 1, MPI_INT, buf.data(), buf.size(), &pos, comm);
    MPI_Pack(&s[0], str_len, MPI_BYTE, buf.data(), buf.size(), &pos, comm);
  }
}


/* Use MPI_Unpack to unpack a request message */
void unpackRequest(bool &shutdown, string &s, const vector<char> &buf, MPI_Comm comm) {
  int key, str_len, pos = 0;

  // fprintf(stderr, "unpacking %p, %d bytes\n", buf.data(), (int)buf.size());
  MPI_Unpack(buf.data(), buf.size(), &pos, &key, 1, MPI_INT, comm);
  // fprintf(stderr, "request key %d\n", key);
  if (key < 0) {
    shutdown = true;
    s.clear();
    return;
  }

  MPI_Unpack(buf.data(), buf.size(), &pos, &str_len, 1, MPI_INT, comm);
  s.resize(str_len);
  MPI_Unpack(buf.data(), buf.size(), &pos, &s[0], str_len, MPI_BYTE, comm);
}


// get the contents of a file from a cache server
// returns nonzero if the file couldn't be read
int CacheClient::getFile(vector<char> &content, string name) {
  int server_idx = getFileServer(name);
  assert(0 <= server_idx && server_idx < (int)server_ranks.size());
  int server_rank = server_ranks[server_idx];
  vector<char> request_buf;
  
  packRequest(request_buf, false, name, comm);
  
  // printf("[%d] requesting %s from server %d, rank %d\n", rank, name.c_str(), server_idx, server_rank);
  MPI_Send(request_buf.data(), request_buf.size(), MPI_PACKED, server_rank, CacheServer::TAG_REQUEST, comm);

  int file_len;
  MPI_Recv(&file_len, 1, MPI_INT, server_rank, CacheServer::TAG_FILE_LEN, comm, MPI_STATUS_IGNORE);
  if (file_len >= 0) {
    content.resize(file_len);
    MPI_Recv(content.data(), file_len, MPI_BYTE, server_rank, CacheServer::TAG_FILE_CONTENT, comm, MPI_STATUS_IGNORE);
    return 0;
  } else {
    content.resize(0);
    return -1;
  }
  // printf("[%d] received %s, %d bytes\n", rank, name.c_str(), file_len);
}


// tells all cache server threads to shut down.
// should only be called by stopCacheServers()
void CacheClient::shutdown() {

  // if there is a server thread running in this process, send it a shutdown message,
  // and wait for it to finish
  if (server_thread) {
    vector<char> request_buf;
    packRequest(request_buf, true, "", comm);
    MPI_Send(request_buf.data(), request_buf.size(), MPI_BYTE, rank, CacheServer::TAG_REQUEST, comm);
    server_thread->join();
    // printf("[%d] joined server thread\n", rank);
    delete server_thread;
    server_thread = nullptr;
  }
}


int CacheClient::getFileServer(const string &filename) {
  unsigned hash = hashString(filename);
  return hash % server_ranks.size();
}


// FNV hash of a string
// http://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
// I'm using this hash algorithm just because the implementation is really simple.
unsigned CacheClient::hashString(const string &s) {
  unsigned hash = 2166136261u;
  for (size_t i=0; i < s.length(); i++) {
    hash = hash ^ s[i];
    hash = hash * 16777619;
  }
  return hash;
}
