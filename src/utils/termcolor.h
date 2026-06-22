#ifndef SAPPHIRE_TERMCOLOR_H
#define SAPPHIRE_TERMCOLOR_H

#include <string>

// Call this once at the start of the application
void init_terminal();

// Color helpers
std::string tc_reset();
std::string tc_red();
std::string tc_green();
std::string tc_yellow();
std::string tc_blue();
std::string tc_cyan();
std::string tc_bold();

#endif // SAPPHIRE_TERMCOLOR_H
