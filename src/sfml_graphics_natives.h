#ifndef SAPPHIRE_SFML_GRAPHICS_NATIVES_H
#define SAPPHIRE_SFML_GRAPHICS_NATIVES_H

#include "vm.h"
#include "value.h"
#include "tokens.h"
#include <SFML/Graphics.hpp>

void register_sfml_graphics_natives(VM* vm);

#endif // SAPPHIRE_SFML_GRAPHICS_NATIVES_H
