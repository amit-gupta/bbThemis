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

  OSTContentMap ost_content_map;
  int file_count = scanLustreFiles(input_files, &ost_content_map);

  cout << file_count << " files scanned\n";

  // OSTContent::const_iterator ost_iter;
  for (auto ost_iter : ost_content_map.ost_content) {
    int ost_idx = ost_iter.first;
    const vector<StridedContent> &content_list = ost_iter.second;

    cout << "OST " << ost_idx << '\n';

    for (const StridedContent &c : content_list) {
      cout << "  (" << c.offset << "," << c.length << "," << c.stride
           << ") " << c.file_name << "\n";
    }
  }

  return 0;
}
