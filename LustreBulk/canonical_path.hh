#ifndef __CANONICAL_PATH_HH__
#define __CANONICAL_PATH_HH__


/* Computes a canonical path name, such that it will begin with '/'
   and not contain '/..'. */
std::string canonicalPath(const char *path);


#endif // __CANONICAL_PATH_HH__
