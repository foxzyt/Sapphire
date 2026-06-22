#ifndef SAPPHIRE_CONFIG_H
#define SAPPHIRE_CONFIG_H

#include <string>
#include "tokens.h"

struct ScriptConfig {
    unsigned int windowWidth = 400;
    unsigned int windowHeight = 550;
    std::string windowTitle = "SapphireUI";
    bool windowBorderless = false;
};

#endif //SAPPHIRE_CONFIG_H
