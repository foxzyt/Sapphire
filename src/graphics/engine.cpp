#include "engine.h"
#include <iostream>
#include <vector>
#include <memory>
#include <SFML/Graphics.hpp>

static std::vector<std::unique_ptr<sf::Texture>> global_textures;

static sf::RenderWindow* get_window(VM* vm) {
    if (!vm || !vm->sfml_window || !vm->sfml_window->isOpen()) return nullptr;
    return vm->sfml_window;
}

// Graphics (Window & Render)
static SapphireValue graphics_isWindowOpen(int arg_count, SapphireValue* args) {
    sf::RenderWindow* window = get_window(g_current_vm);
    return SapphireValue(window != nullptr && window->isOpen());
}

static SapphireValue graphics_pollEvents(int arg_count, SapphireValue* args) {
    sf::RenderWindow* window = get_window(g_current_vm);
    if (!window) return SapphireValue();
    while (const std::optional event = window->pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window->close();
        }
    }
    return SapphireValue();
}

static SapphireValue graphics_clear(int arg_count, SapphireValue* args) {
    sf::RenderWindow* window = get_window(g_current_vm);
    if (!window) return SapphireValue();
    window->clear(sf::Color::Black); // Can be improved to take color args
    return SapphireValue();
}

static SapphireValue graphics_display(int arg_count, SapphireValue* args) {
    sf::RenderWindow* window = get_window(g_current_vm);
    if (!window) return SapphireValue();
    window->display();
    return SapphireValue();
}

static SapphireValue graphics_drawRect(int arg_count, SapphireValue* args) {
    if (arg_count != 4) return SapphireValue();
    sf::RenderWindow* window = get_window(g_current_vm);
    if (!window) return SapphireValue();
    double x = std::get<double>(args[0]._value);
    double y = std::get<double>(args[1]._value);
    double w = std::get<double>(args[2]._value);
    double h = std::get<double>(args[3]._value);
    sf::RectangleShape rect({(float)w, (float)h});
    rect.setPosition({(float)x, (float)y});
    window->draw(rect);
    return SapphireValue();
}

// Keyboard
static SapphireValue keyboard_isKeyPressed(int arg_count, SapphireValue* args) {
    if (arg_count != 1) return SapphireValue();
    std::string key_name = ((ObjString*)std::get<Obj*>(args[0]._value))->chars;
    sf::Keyboard::Key key = sf::Keyboard::Key::Unknown;
    
    if (key_name == "W" || key_name == "w") key = sf::Keyboard::Key::W;
    else if (key_name == "S" || key_name == "s") key = sf::Keyboard::Key::S;
    else if (key_name == "A" || key_name == "a") key = sf::Keyboard::Key::A;
    else if (key_name == "D" || key_name == "d") key = sf::Keyboard::Key::D;
    else if (key_name == "Up") key = sf::Keyboard::Key::Up;
    else if (key_name == "Down") key = sf::Keyboard::Key::Down;
    else if (key_name == "Left") key = sf::Keyboard::Key::Left;
    else if (key_name == "Right") key = sf::Keyboard::Key::Right;
    else if (key_name == "Space") key = sf::Keyboard::Key::Space;
    else if (key_name == "Enter") key = sf::Keyboard::Key::Enter;
    else if (key_name == "Escape") key = sf::Keyboard::Key::Escape;

    return SapphireValue(sf::Keyboard::isKeyPressed(key));
}

// Texture
static SapphireValue texture_load(int arg_count, SapphireValue* args) {
    if (arg_count != 1) return SapphireValue();
    ObjInstance* self = (ObjInstance*)std::get<Obj*>(args[-1]._value);
    std::string path = ((ObjString*)std::get<Obj*>(args[0]._value))->chars;
    
    auto tex = std::make_unique<sf::Texture>();
    if (tex->loadFromFile(path)) {
        global_textures.push_back(std::move(tex));
        self->fields["id"] = SapphireValue((double)(global_textures.size() - 1));
    }
    return SapphireValue((Obj*)self);
}

// Sprite
static SapphireValue sprite_draw(int arg_count, SapphireValue* args) {
    ObjInstance* self = (ObjInstance*)std::get<Obj*>(args[-1]._value);
    sf::RenderWindow* window = get_window(g_current_vm);
    if (!window) return SapphireValue();

    if (!self->fields.contains("textureId") || !self->fields.contains("x") || !self->fields.contains("y")) return SapphireValue();

    double id_val = std::get<double>(self->fields["textureId"]._value);
    int tex_id = (int)id_val;
    if (tex_id >= 0 && tex_id < global_textures.size()) {
        sf::Sprite sprite(*(global_textures[tex_id]));
        double x = std::get<double>(self->fields["x"]._value);
        double y = std::get<double>(self->fields["y"]._value);
        sprite.setPosition({(float)x, (float)y});
        window->draw(sprite);
    }
    return SapphireValue();
}

static SapphireValue sprite_setTexture(int arg_count, SapphireValue* args) {
    ObjInstance* self = (ObjInstance*)std::get<Obj*>(args[-1]._value);
    ObjInstance* tex = (ObjInstance*)std::get<Obj*>(args[0]._value);
    self->fields["textureId"] = tex->fields["id"];
    return SapphireValue((Obj*)self);
}

static SapphireValue graphics_drawTextureRect(int arg_count, SapphireValue* args) {
    if (arg_count != 9) return SapphireValue();
    sf::RenderWindow* window = get_window(g_current_vm);
    if (!window) return SapphireValue();

    double id_val = std::get<double>(args[0]._value);
    int tex_id = (int)id_val;

    if (tex_id >= 0 && tex_id < global_textures.size()) {
        float sx = std::get<double>(args[1]._value);
        float sy = std::get<double>(args[2]._value);
        float sw = std::get<double>(args[3]._value);
        float sh = std::get<double>(args[4]._value);
        
        float dx = std::get<double>(args[5]._value);
        float dy = std::get<double>(args[6]._value);
        float dw = std::get<double>(args[7]._value);
        float dh = std::get<double>(args[8]._value);

        sf::Sprite sprite(*(global_textures[tex_id]));
        sprite.setTextureRect(sf::IntRect({(int)sx, (int)sy}, {(int)sw, (int)sh}));
        sprite.setPosition({dx, dy});
        sprite.setScale({dw / sw, dh / sh});

        window->draw(sprite);
    }
    return SapphireValue();
}

void register_graphics_engine(VM* vm) {
    // 1. Graphics API (Singleton methods -> Globals)
    vm->globals["isWindowOpen"] = SapphireValue((Obj*)new_native(vm, graphics_isWindowOpen));
    vm->globals["pollEvents"] = SapphireValue((Obj*)new_native(vm, graphics_pollEvents));
    vm->globals["clear"] = SapphireValue((Obj*)new_native(vm, graphics_clear));
    vm->globals["display"] = SapphireValue((Obj*)new_native(vm, graphics_display));
    vm->globals["drawRect"] = SapphireValue((Obj*)new_native(vm, graphics_drawRect));
    vm->globals["drawTextureRect"] = SapphireValue((Obj*)new_native(vm, graphics_drawTextureRect));

    // 2. Keyboard (Singleton methods -> Globals)
    vm->globals["isKeyPressed"] = SapphireValue((Obj*)new_native(vm, keyboard_isKeyPressed));

    // 3. Texture Class
    ObjClass* tex_class = new_class(vm, new_string(vm, "Texture"));
    tex_class->methods["load"] = SapphireValue((Obj*)new_native(vm, texture_load));
    vm->globals["Texture"] = SapphireValue((Obj*)tex_class);

    // 4. Sprite Class
    ObjClass* spr_class = new_class(vm, new_string(vm, "Sprite"));
    spr_class->methods["draw"] = SapphireValue((Obj*)new_native(vm, sprite_draw));
    spr_class->methods["setTexture"] = SapphireValue((Obj*)new_native(vm, sprite_setTexture));
    vm->globals["Sprite"] = SapphireValue((Obj*)spr_class);
}
