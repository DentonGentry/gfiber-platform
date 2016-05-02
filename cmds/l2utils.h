#include <string>
#include <tr1/unordered_map>

#ifndef L2UTILS_H
#define L2UTILS_H

typedef std::tr1::unordered_map<std::string, std::string> L2Map;
extern void get_l2_map(L2Map *l2map);

#endif  // L2UTILS_H
