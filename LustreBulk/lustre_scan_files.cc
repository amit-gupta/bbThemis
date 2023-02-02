#include <cstdio>
#include <cstring>
#include <errno.h>
#include <ftw.h>
#include <set>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "lustre_wrapper.h"
#include "lustre_scan_files.hh"
#include "canonical_path.hh"


using std::vector;
using std::string;


OSTContentHandler *global_ost_content_handler = 0;
int global_file_count = 0;
FileSet *global_all_files = 0;

static void scanFile(const char *filename, const struct stat *stat_buf) {
  int err, stripe_count;
  uint64_t stripe_size;

  string filename_str(filename);
  if (global_all_files->find(filename_str) != global_all_files->end()) {
    printf("Skipping %s (already scanned)\n", filename);
    return;
  }
  (*global_all_files)[filename_str] = stat_buf->st_size;
  
  global_file_count++;
  // printf("Scanning file %s\n", filename);
  
  err = lustre_get_striping(filename, &stripe_count, &stripe_size);
  if (err) {
    fprintf(stderr, "Error %d in lustre_get_striping(%s): %s\n",
            err, filename, strerror(err));
    return;
  }

  // printf("  count=%d size=%ld, osts={", stripe_count, (long)stripe_size);

  vector<int> osts(stripe_count);
  err = lustre_get_striping_details(filename, osts.size(), osts.data());
  if (err) {
    fprintf(stderr, "Error %d in lustre_get_striping_details(%s): %s\n",
            err, filename, strerror(err));
    return;
  }

  StridedContent content(filename, 0, stripe_size, stripe_size * stripe_count,
                         stat_buf->st_size);
  for (int i=0; i < stripe_count; i++) {
    int ost_idx = osts[i];
    // printf("%s%d", (i==0) ? "" : " ", ost_idx);
    content.offset = i * stripe_size;
    global_ost_content_handler->addContent(ost_idx, content);
  }
  // printf("}\n");
    
}


// called by nftw()
static int visitFn(const char *filename, const struct stat *stat_buf,
                   int type_flag, struct FTW *ftw) {

  // ignore anything except regular files
  if (type_flag != FTW_F) return 0;

  scanFile(filename, stat_buf);

  return 0;
}


int scanLustreFiles(const std::vector<std::string> &paths,
                    OSTContentHandler *ost_content_handler,
                    FileSet &all_files) {

  struct stat stat_buf;
  // std::set<string> scanned_files;

  global_ost_content_handler = ost_content_handler;
  global_file_count = 0;
  global_all_files = &all_files;
  global_all_files->clear();
  
  for (const string &input_path : paths) {
    // check if this path has been scanned already
    string path_str = canonicalPath(input_path.c_str());
    const char *path = path_str.c_str();

    // is this a file, directory, or other?
    if (stat(path, &stat_buf)) {
      fprintf(stderr, "Error %d calling stat(\"%s\"): %s\n",
              errno, path, strerror(errno));
      continue;
    }

    // regular file
    if (S_ISREG(stat_buf.st_mode)) {
      scanFile(path, &stat_buf);
    }

    // directory
    else if (S_ISDIR(stat_buf.st_mode)) {
      if (nftw(path, visitFn, 20, 0)) {
        fprintf(stderr, "Error %d traversing %s: %s\n", errno, path,
                strerror(errno));
      }
    }

    else {
      fprintf(stderr, "Unrecognized file type: %s\n", path);
    }
  }

  global_all_files = 0;
    
  return global_file_count;
}

