#include <iostream>
// #include <cstdint>
#include "lustre_scan_files.hh"

using std::string;
using std::vector;
using std::cout;

int main(int argc, char **argv) {
  vector<string> input_files;

  for (int i=1; i < argc; i++)
    input_files.push_back(argv[i]);

  OSTContentMapper mapper;
  FileSet all_files;
  int file_count = scanLustreFiles(input_files, &mapper, all_files);

  cout << file_count << " files scanned\n";

  for (auto ost_iter : mapper.ost_content) {
    int ost_idx = ost_iter.first;
    const vector<StridedContent> &content_list = ost_iter.second;

    cout << "OST " << ost_idx << '\n';

    for (const StridedContent &c : content_list) {
      cout << "  (" << c.offset << "," << c.length << "," << c.stride
           << "," << c.file_size << ") " << c.file_name << "\n";
    }
  }

  return 0;
}
