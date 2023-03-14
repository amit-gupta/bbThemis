/*
  Simplified example of using a background thread on each node in MPI job
  to serve as a distributed cache.

  MPI must be initialized with MPI_THREAD_MULTIPLE, since background threads will
  be making MPI calls.

  Important: when making MPI calls in a background thread, if another thread
  calls MPI_Finalize the background thread will crash. Be sure to shut down
  the background thread before calling MPI_Finalize.

  Limitations:
   - This is just a demo designed for small files up to 10MB each.
   - The cache is not purged. Once a file is read, it will remain in the cache.
*/


#include <unordered_map>
#include <string>
#include <vector>
#include <thread>
#include <mpi.h>


using std::string;
using std::vector;


class CacheServer {
public:

  // tags for messages the server recognizes
  enum Tags {
    /* Request sent to a server thread.
       This is a packed message starting with an integer.
         If it's negative, then this is a command to the server to shut down.
         Otherwise it's a request for a file.
           Unpack a positive integer which will be the length of the filename.
           Then unpack the filename itself.
    */
    TAG_REQUEST,

    // response to TAG_GET_FILE - the contents of a file
    // TAG_FILE_LEN message is one int, the length of the file
    // TAG_FILE_CONTENT message is the contents of the file
    TAG_FILE_LEN,
    TAG_FILE_CONTENT
  };

  // Runs a CacheServer. This should be called in a background thread.
  static void runCacheServer(MPI_Comm comm, int node_idx);
    

  CacheServer(MPI_Comm comm_, int node_idx_) :
    comm(comm_),
    node_idx(node_idx_),
    file_request_count(0),
    cached_request_count(0)
  {}

  // wait for requests. Exits when TAG_EXIT message is received.
  void loop();
  
  
private:

  // The communicator used for all messages.
  const MPI_Comm comm;

  const int node_idx;

  // Get the contents, either from cache or by reading the file.
  // If the file cannot be read, set length to -1 and content to NULL.
  void getFile(const string &name, int &length, char *&content);
  
  // map filename to file contents
  using FileMap = std::unordered_map<string, vector<char>>;
  
  FileMap file_map;
  int file_request_count;
  int cached_request_count;
};


/* Client which communicates with multiple servers.
 */
class CacheClient {
 public:
  CacheClient(MPI_Comm comm_, const vector<int> &server_ranks_, std::thread *server_thread_)
    : comm(comm_), server_ranks(server_ranks_), server_thread(server_thread_) {
    MPI_Comm_rank(comm, &rank);
  }

  // get the contents of a file from a cache server
  // returns nonzero if the file couldn't be read
  int getFile(vector<char> &content, string name);

  // tells all cache server threads to shut down.
  // should only be called by stopCacheServers()
  void shutdown();

  MPI_Comm getCacheComm() const {return comm;}

  // use the hash of the file name to determine which server should have its data
  int getFileServer(const string &filename);

  // FNV hash of a string
  // http://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
  static unsigned hashString(const string &s);
    
 private:

  int rank;
  const MPI_Comm comm;
  const vector<int> server_ranks;

  // server thread on this process, only set if rank_on_node == 0
  std::thread *server_thread;
};


/* Split input on a given character. Skip empty lines. */
void splitString(vector<string> &output_list, const vector<char> &input, char split_char);

/* Reorder a vector randomly. */
template <class T> void shuffleVector(vector<T> &v);



/* Try to read a bunch of files.
   Read them in a random order, otherwise the accesses will get serialized.
   file_list_all - a list of input files, separated by newlines
   success_count - number of files successfully read
   error_count - number of files that couldn't be read
   bytes_read - total number of bytes read
*/
void readFiles(int rank, CacheClient *cache_client, const vector<char> &file_list_all,
               int &success_count, int &error_count, long &bytes_read);


/* Collective call across all ranks.
   Starts one CacheServer running on each node.
   Returns a CacheClient object.
   CacheClient should be deallocated by calling stopCacheServers(). */
CacheClient* startCacheServers();

/* Colletive call stopping cache servers and deallocating client objects. */
void stopCacheServers(CacheClient *&client);

/* Computes how processes have been mapped to nodes.
   node_count: total number of nodes
   node_idx: index (0..node_count-1) of this node
   node_size: number of ranks on this node
   rank_on_node: index (0..ranks_per_node-1) of this process on this node. */
void nodeMapping(int *node_count, int *node_idx, int *rank_on_node);

/* Use MPI_Pack build a request message to the server */
void packRequest(vector<char> &buf, bool shutdown, const string &s, MPI_Comm comm);

/* Use MPI_Unpack to unpack a request message */
void unpackRequest(bool &shutdown, string &s, const vector<char> &buf, MPI_Comm comm);
