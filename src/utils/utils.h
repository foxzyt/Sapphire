#ifndef SAPPHIRE_UTILS_H
#define SAPPHIRE_UTILS_H

#include <string>
#include "tokens.h"

std::string load_file_as_string(const std::string& path);
bool check_for_soft_mode(const std::string& source);

#endif //SAPPHIRE_UTILS_H
