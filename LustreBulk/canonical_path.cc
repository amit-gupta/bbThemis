/* Compute a canonical pathname 
   Ed Karrels, ed.karrels@gmail.com
*/

#include <cassert>
#include <cstring>
#include <limits.h>
#include <string>
#include <unistd.h>


static std::string currentDir() {
  // TODO this could be fancier, where first we try using a fix-size
  // buffer, then if that fails, retry with increasingly larger
  // buffers
  char cwd[PATH_MAX];
  if (!getcwd(cwd, PATH_MAX)) {
    fprintf(stderr, "In canonical_path.cc currentDir(), error calling getcwd(buf, %d): %s\n", PATH_MAX, strerror(errno));
    strcpy(cwd, ".");
  }
  return std::string(cwd);
}


/* Computes a canonical path name, such that it will begin with '/'
   and not contain '/..'. */
std::string canonicalPath(const char *path) {

  if (!*path) return currentDir();

  std::string result;
  const char *p = path;
  
  if (*p != '/') {
    // relative path
    result = currentDir();
  }

  // result will be in the form (/xxx)*

  while (*p) {
    // printf("    result=\"%s\"  p=\"%s\"\n", result.c_str(), p);
    
    // skip repeated slashes
    while (*p == '/') p++;
    if (!*p) break;

    // find end of this component, whether it's a slash or the end of the string
    const char *end = p+1;
    while (*end && *end != '/') end++;

    if (*p == '.') {
      
      // handle "./"
      if (end-p == 1) {
        p++;
        continue;
      }

      // handle "../"
      else if (p[1] == '.' && end-p==2) {
        p += 2;

        // cannot go higher than root directory
        if (result.length() == 0) continue;

        // if it isn't empty it must start with a slash, so find_last_of will not fail
        assert(result[0] == '/');
        size_t last_slash = result.find_last_of('/');

        result.resize(last_slash);
        continue;
      }
    }

    // append everything up to but not including the next slash
    result += '/';
    result.append(p, end);
    p = end;
  }

  if (result.length() == 0) result += '/';

  return result;
}
