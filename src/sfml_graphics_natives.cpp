#include "sfml_graphics_natives.h"
#include <iostream>
#include "tokens.h"

using enum TokenType;

static sf::RenderWindow* get_sfml_window(VM* vm) {
    if (!vm || !vm->sfml_window || !vm->sfml_window->isOpen()) {
        if (vm && !vm->soft_mode) {
            std::cerr << "SFML window is not open or not set in VM." << std::endl;
        }
        return nullptr;
    }
    return vm->sfml_window;
}

static sf::Color get_sfml_color(int arg_count, SapphireValue* args, int start_index) {
    if (arg_count < start_index + 4) return sf::Color::White;

    uint8_t r = static_cast<uint8_t>(std::get<double>(args[start_index]._value));
    uint8_t g = static_cast<uint8_t>(std::get<double>(args[start_index + 1]._value));
    uint8_t b = static_cast<uint8_t>(std::get<double>(args[start_index + 2]._value));
    uint8_t a = static_cast<uint8_t>(std::get<double>(args[start_index + 3]._value));

    return sf::Color(r, g, b, a);
}

static SapphireValue native_sfml_is_window_open(int arg_count, SapphireValue* args) {
    if (arg_count != 0) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: SFML.isWindowOpen() expects 0 arguments." << std::endl;
        }
        return false;
    }
    sf::RenderWindow* window = g_current_vm->sfml_window;
    return (window != nullptr && window->isOpen());
}

static SapphireValue native_sfml_poll_events(int arg_count, SapphireValue* args) {
    if (arg_count != 0) {
        if (g_current_vm && !g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: SFML.pollEvents() expects 0 arguments." << std::endl;
        }
        return {};
    }

    sf::RenderWindow* window = get_sfml_window(g_current_vm);
    if (!window) return {};

    while (const std::optional event = window->pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window->close();
        }

    }

    return {};
}

static SapphireValue native_sfml_clear(int arg_count, SapphireValue* args) {
    if (arg_count != 4) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: SFML.clear() expects 4 arguments (r, g, b, a)." << std::endl;
        }
        return {};
    }
    sf::RenderWindow* window = get_sfml_window(g_current_vm);
    if (!window) return {};

    sf::Color color = get_sfml_color(arg_count, args, 0);
    window->clear(color);
    return {};
}

static SapphireValue native_sfml_draw_circle(int arg_count, SapphireValue* args) {
    sf::RenderWindow* window = get_sfml_window(g_current_vm);
    if (!window) return {};

    float x = static_cast<float>(std::get<double>(args[0]._value));
    float y = static_cast<float>(std::get<double>(args[1]._value));
    float radius = static_cast<float>(std::get<double>(args[2]._value));
    sf::Color color = get_sfml_color(arg_count, args, 3);

    sf::CircleShape circle(radius);
    circle.setPosition({x - radius, y - radius});
    circle.setFillColor(color);

    window->draw(circle);
    return {};
}

static SapphireValue native_sfml_display(int arg_count, SapphireValue* args) {
    if (arg_count != 0) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: SFML.display() expects 0 arguments." << std::endl;
        }
        return {};
    }
    sf::RenderWindow* window = get_sfml_window(g_current_vm);
    if (!window) return {};

    window->display();
    return {};
}


void register_sfml_graphics_natives(VM* vm) {
    ObjClass* sfml_class = new_class(vm, new_string(vm, "SFML"));
    ObjInstance* sfml_object = new_instance(vm, sfml_class);

    sfml_object->fields["isWindowOpen"] = new_native(vm, native_sfml_is_window_open);
    sfml_object->fields["pollEvents"] = new_native(vm, native_sfml_poll_events);
    sfml_object->fields["clear"] = new_native(vm, native_sfml_clear);
    sfml_object->fields["drawCircle"] = new_native(vm, native_sfml_draw_circle);
    sfml_object->fields["display"] = new_native(vm, native_sfml_display);

    vm->globals["SFML"] = sfml_object;

}
