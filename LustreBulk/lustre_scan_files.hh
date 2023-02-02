#ifndef __LUSTRE_SCAN_FILES_HH__
#define __LUSTRE_SCAN_FILES_HH__

#include <map>
#include <vector>
#include <string>
#include <cstdint>

/* Defines a strided range of data in a file.
   For example, given a file with 1 MiB blocks spread across
   OSTs {7, 3, 1, 10} the data residing on OST 7 would be:
     {offset=0, length=1048576, stride=4194304}
   and the data on OST 3 would be:
     {offset=1048576, length=1048576, stride=4194304}
*/
struct StridedContent {
  std::string file_name;

  // offset of the first block of data, in bytes
  uint64_t offset;

  // length of each block of data, in bytes
  uint64_t length;

  // offset between start of each block, in bytes.
  uint64_t stride;

  // total size of the file
  uint64_t file_size;

  StridedContent(const std::string &file_name_,
                 uint64_t offset_,
                 uint64_t length_,
                 uint64_t stride_,
                 uint64_t file_size_)
    : file_name(file_name_),
      offset(offset_),
      length(length_),
      stride(stride_),
      file_size(file_size_) {}

  // compute the number of bytes needed to scan my subset of this file
  uint64_t getSize() {
    uint64_t full_cycles = file_size / stride;
    uint64_t remainder = file_size - (full_cycles * stride);
    uint64_t extra;
    if (remainder <= offset) {
      extra = 0;
    } else if (remainder >= offset + length) {
      extra = length;
    } else {
      extra = remainder - offset;
    }
    return full_cycles * length + extra;
  }
};


/* Interface for a object that is given strided content and the OST on
   which it resides. This is to support implementations such as:
     - store content in a map indexed by OST
     - send contne to a process handling data for each OST
*/
class OSTContentHandler {
public:
  virtual void addContent(int ost_idx, const StridedContent &content) = 0;
};


using OSTContentMap = std::map<int, std::vector<StridedContent>>;


/* Implements OSTContentHandler to store data in a map indexed by OST. */
class OSTContentMapper : public OSTContentHandler {
public:
  OSTContentMap ost_content;
  
  virtual void addContent(int ost_idx, const StridedContent &content) {
    auto it = ost_content.find(ost_idx);
    if (it == ost_content.end()) {
      ost_content[ost_idx] = std::vector<StridedContent> {content};
    } else {
      it->second.push_back(content);
    }
  }
};


// Map filenames to their size.
using FileSet = std::map<std::string, uint64_t>;


/* Scans the list of file or directory names, retrieves the Lustre striping
   details for each, and calls ost_content_handler->addContent() for each.

   Files are scanned directly. Directories are traversed, following symlinks
   while avoiding infinite loops, scanning every file in the directory and
   its subdirectories.

   The filenames in each StridedContent object will be full canonical
   pathnames, and duplicates will be automatically eliminated.

   For example, given one file "foo" striped across two OSTs {3, 7}, this
   would call the handler with:
     3, ("/home/ed/foo", 0, 1048576, 2097152)
     7, ("/home/ed/foo", 1048576, 1048576, 2097152)

   all_files will be filled with every filename seen. It maps a filename
   to the file size.

   The number of files scanned is returned.
   Any error encountered are output to stderr.
*/
int scanLustreFiles(const std::vector<std::string> &paths,
                    OSTContentHandler *ost_content_handler,
                    FileSet &all_files);



#endif // __LUSTRE_SCAN_FILES_HH__
