# Using local storage as a caching layer


Most of our sample applications do not access the same data repeatedly.
I suspect most of the benefits we can expect from a cache layer will be in the following ways:
 * Reading the same library from many processes.
 * Pre-reading many small files, reading them in bulk more efficiently than they can be read one-at-a-time.
 * Buffering many small writes so they can be converted into a few large writes.
 

## Components available now

* `PageCache`, a local page cache with variable consistency: 
[page_cache.h](https://github.com/bbThemis/ThemisIO/blob/main/include/page_cache.h)
and [page_cache.cc](https://github.com/bbThemis/ThemisIO/blob/main/src/page_cache.cc).
See comments at the top of page_cache.h for a description of how it works.

* Efficient bulk read or write of many files from/to a Lustre file system.
[LustreBulk](https://github.com/bbThemis/bbThemis/tree/master/LustreBulk)
This is a proof-of-concept. Most of the logic in bulk_lustre_read.cc.
The name is a bit misleading--it does both read and write modes.

## Additional parts needed

* Use alternative storage for PageCache data. PageCache currently uses a malloc'd region for
  its storage area. Change this to use other storage mediums such as a local SSD or burst buffer.
  If some nodes have more storage than others, you might want to have a mechanism for discovering
  what is available, or use an environment variable or initialization call to tell the system
  what storage to use.
* Intercept IO calls, transfer to page_cache. By some mechanism (trampoline, LD_PRELOAD, macros...)
  intercept calls to IO functions such as open/read/close and send them instead to PageCache::open,
  PageCache::read, PageCache::close, etc.
* Add a mechanism to define which files should be cached and which should be pre-loaded.
  I think the main benefit will be from pre-loading large input sets, but there may be
  a benefit in speeding up repeated accesses of the same data by multiple processes. Many of our sample applications
  are written in Python and every process reads around 3000 python library files on startup.
  It may be useful to cache these files after the first process reads them so other processes
  in the job can use cached copies. 
* Distributed cache. Assuming a job will use multiple nodes and more than one node will hold
  cached data, it will be necessary to determine which node will hold each piece of cached
  data. If we know ahead of time which data will be accessed by each node, we can arrange
  to always store the data for that node locally. However, applications are not that predictable.
  For more flexiblity, I suggest organizing the cache into fixed-size blocks of files and
  mapping those blocks to nodes.
  
  1. Choose a power-of-two cache block size. For flexibility, make this a configuration parameter
     that is set at start-up. Some experimentation may be needed to find a value that works
     well across many applications. I suggest an initial value in the range 64 KB to 4 MB.
  1. Create a cache server which will run on each node to handle remote accesses of cached data.
     For example, when node 4 wants data cached on node 3, it will contact the cache server
     on node 3. This may also be needed to support local accesses when multiple processes on
     the node are using the cache. For efficiency, the cache server could use shared memory
     when handling local accesses.
  1. Define a hash value that takes the canonical file name and block index as inputs.
     `PageCache::canonicalPath()` can help compute a canonical file name.
  1. With a read-only cache, we can use a modulo and/or lookup table to define a fixed mapping
     from hash values to node indices.
     If every node has the same size cache, you could map the hash value to a node index
     by taking the hash value modulo the node count. If the cache sizes vary between nodes,
     you could use a lookup table to help with the mapping. For example, if you have
     an 8-node job but only nodes 0, 3, and 5 have cache space, you could create a table
     that approximates the space distribution. Create a table with 256 entries, fill entries
     table[0..84] = 0, table[85..170] = 3, table[171..255] = 5.
     Then node_idx = table[hash(canonical_path, block_idx) & 255]
  1. Cache coherency. For an efficient read-write cache, it will be necessary to support
     a dynamic mapping of cache blocks to nodes. For example, say node 0 does many small
     writes to a single cache block. After the first write that cache block should reside
     on node 0 so the following writes can be done as efficiently as possible. If node
     1 then does many small writes to the same cache block, ownership of the cache block
     should move to node 1 so its writes will be efficient. This is a common problem in
     systems with mulitple CPU cores where each core has a small cache of main memory. We can
     use a simple cache coherency system such as [MESI](https://en.wikipedia.org/wiki/MESI_protocol)
     to handle this. This will require communication between cache servers at runtime.
     
Our sample applications don't write much data, and supporting writes
to cached data requires the cache coherency system in the bullet point
above, which will add a lot of time to the implemention.  I suggest starting with
a read-only cache with a fixed block_index-to-node_index mapping. This
will make it easier to build a prototype and should provide nice
performance improvements for jobs with many input files such as deep
learning applications.

