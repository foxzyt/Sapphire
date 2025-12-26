#include <stdexcept>
#include <mutex>
#include <condition_variable>
#include "vm.h"
#include "compiler.h"
#include "object.h"
#include "debug.h"
#include "value.h"
#include "config.h"
#include "utils.h"
#include "opcodes.h"
#include "httplib.h"
#include "tokens.h"
#include "nlohmann/json.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <cmath>
#include <variant>
#include <sstream>
#include <mutex>
#include <condition_variable>
#include <set>
#include <random>
#include "sfml_graphics_natives.h"

static std::random_device rd;
static std::mt19937 gen(rd());

VM* g_current_vm = nullptr;

static SapphireValue convertJsonToSapphire(VM* vm, const nlohmann::json& j, ObjClass* json_object_class);

static SapphireValue convertJsonObjectToSapphireInstance(VM* vm, const nlohmann::json& j, ObjClass* json_object_class) {
    ObjInstance* instance = new_instance(vm, json_object_class);
    for (auto it = j.begin(); it != j.end(); ++it) {
        std::string key_copy = it.key();
        const nlohmann::json& value = it.value();
        instance->fields[key_copy] = convertJsonToSapphire(vm, value, json_object_class);
    }
    return instance;
}

static SapphireValue convertJsonArrayToSapphireArray(VM* vm, const nlohmann::json& j, ObjClass* json_object_class) {
    auto array_obj = std::make_shared<SapphireArray>();
    for (const auto& element : j) {
        array_obj->elements.push_back(convertJsonToSapphire(vm, element, json_object_class));
    }
    return array_obj;
}

static SapphireValue convertJsonToSapphire(VM* vm, const nlohmann::json& j, ObjClass* json_object_class) {
    if (j.is_object()) return convertJsonObjectToSapphireInstance(vm, j, json_object_class);
    if (j.is_array()) return convertJsonArrayToSapphireArray(vm, j, json_object_class);
    if (j.is_string()) return new_string(vm, j.get<std::string>());
    if (j.is_number()) return j.get<double>();
    if (j.is_boolean()) return j.get<bool>();
    if (j.is_null()) return {};
    return {};
}


static const auto clock_start_time = std::chrono::high_resolution_clock::now();
static SapphireValue clock_native(int arg_count, SapphireValue* args) {
    if (arg_count != 0) return {};
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = now - clock_start_time;
    return diff.count();
}

static UIStyle resolve_style(const std::string& id) {
    UIStyle base = g_current_vm->ui_state.activeStyle ? *g_current_vm->ui_state.activeStyle : g_current_vm->ui_state.defaultStyle;
    auto it = g_current_vm->ui_state.idOverrides.find(id);
    if (it != g_current_vm->ui_state.idOverrides.end()) {
        const auto& props = it->second;
        if (props.bgColor) base.bgColor = *props.bgColor;
        if (props.textColor) base.textColor = *props.textColor;
        if (props.accentColor) base.accentColor = *props.accentColor;
        if (props.borderRadius) base.borderRadius = *props.borderRadius;
        if (props.fontSize) base.fontSize = *props.fontSize;
        if (props.padding) base.padding = *props.padding;
    }
    return base;
}

static SapphireValue io_readline_native(int arg_count, SapphireValue* args) {
    if (arg_count != 0) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: IO.readLine() expects 0 arguments." << std::endl;
        }
        return {};
    }
    std::string line;
    std::getline(std::cin, line);
    return new_string(g_current_vm, line);
}

static SapphireValue native_io_write_file(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) {
        return false;
    }
    std::string path = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    std::string content = static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars;

    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << content;
    file.close();
    return true;
}

static SapphireValue native_io_read_file(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string path = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;

    std::ifstream file(path);
    if (!file.is_open()) return {};

    std::stringstream buffer;
    buffer << file.rdbuf();
    return new_string(g_current_vm, buffer.str());
}

static SapphireValue native_io_exists(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return false;
    std::string path = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    std::ifstream file(path);
    return file.good();
}

static SapphireValue native_io_print_color(int arg_count, SapphireValue* args) {
    if (arg_count < 2 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string color = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;

    std::string code = "\033[0m";
    if (color == "red") code = "\033[31m";
    else if (color == "green") code = "\033[32m";
    else if (color == "yellow") code = "\033[33m";
    else if (color == "blue") code = "\033[34m";
    else if (color == "cyan") code = "\033[36m";

    std::cout << code;
    print_value(args[1]);
    std::cout << "\033[0m" << std::endl;
    return {};
}

static SapphireValue native_io_delete_file(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return false;
    std::string path = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    return std::remove(path.c_str()) == 0;
}

static SapphireValue native_io_append_file(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) {
        return false;
    }
    std::string path = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    std::string content = static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars;

    std::ofstream file(path, std::ios_base::app);
    if (!file.is_open()) return false;
    file << content;
    file.close();
    return true;
}

static SapphireValue native_math_sqrt(int arg_count, SapphireValue* args) {
    if (arg_count != 1) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: sqrt() expects 1 argument." << std::endl;
        }
        return {};
    }
    if (!std::holds_alternative<double>(args[0]._value)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Argument for sqrt() must be a number." << std::endl;
        }
        return {};
    }
    double number = std::get<double>(args[0]._value);
    return sqrt(number);
}





static SapphireValue native_string_char_at(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !std::holds_alternative<double>(args[1]._value)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: stringCharAt() expects a string and a number (index)." << std::endl;
        }
        return {};
    }

    ObjString* str_obj = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    int index = static_cast<int>(std::get<double>(args[1]._value));

    if (index < 0 || index >= str_obj->chars.length()) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Index out of bounds for string." << std::endl;
        }
        return {};
    }

    return new_string(g_current_vm, std::string(1, str_obj->chars[index]));
}

static SapphireValue native_value_to_string(int arg_count, SapphireValue* args) {
    if (arg_count != 1) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: valueToString() expects 1 argument." << std::endl;
        }
        return new_string(g_current_vm, "");
    }

    std::stringstream ss;
    auto old_buf = std::cout.rdbuf(ss.rdbuf());
    print_value(args[0]);
    std::cout.rdbuf(old_buf);

    return new_string(g_current_vm, ss.str());
}

static void sapphire_ui_trace(const std::string& id, sf::Vector2f size, float radius) {
    g_current_vm->ui_state.lastComponentId = id;
}

static sf::Color hexToColor(std::string hex) {
    if (hex[0] == '#') hex.erase(0, 1);
    if (hex.length() != 6) return sf::Color::White;
    uint32_t value = std::stoul(hex, nullptr, 16);
    uint8_t r = static_cast<uint8_t>((value >> 16) & 0xFF);
    uint8_t g = static_cast<uint8_t>((value >> 8) & 0xFF);
    uint8_t b = static_cast<uint8_t>(value & 0xFF);
    return sf::Color(r, g, b, 255);
}

static UIStyle* get_style() {
    return g_current_vm->ui_state.activeStyle ? g_current_vm->ui_state.activeStyle : &g_current_vm->ui_state.defaultStyle;
}

static void sapphire_render_text(sf::RenderWindow& window, const std::string& content, sf::Vector2f pos, sf::Color color, std::string fontAlias, int fontSize) {
    UIStyle* s = get_style();

    std::string finalAlias = (fontAlias != "") ? fontAlias : s->fontAlias;
    unsigned int finalSize = (fontSize > 0) ? (unsigned int)fontSize : s->fontSize;

    if (g_current_vm->ui_state.fontStack.find(finalAlias) == g_current_vm->ui_state.fontStack.end()) {
        finalAlias = "default";
    }

    sf::Text text(g_current_vm->ui_state.fontStack[finalAlias], content, finalSize);
    text.setFillColor(color);
    text.setPosition(pos);
    window.draw(text);
}

static void draw_rounded_rect(sf::RenderWindow& window, sf::Vector2f pos, sf::Vector2f size, float radius, sf::Color color, sf::Color outline, float thickness) {
    if (size.x <= 0.0f || size.y <= 0.0f) return;

    float maxRadius = std::min(size.x, size.y) * 0.5f;
    float safeRadius = std::max(0.0f, std::min(radius, maxRadius));

    if (safeRadius < 0.5f) {
        sf::RectangleShape rect(size);
        rect.setPosition(pos);
        rect.setFillColor(color);
        rect.setOutlineColor(outline);
        rect.setOutlineThickness(thickness);
        window.draw(rect);
        return;
    }

    const size_t pointsPerCorner = 10;
    std::vector<sf::Vector2f> pts;
    pts.reserve(pointsPerCorner * 4);

    auto addUniquePoint = [&](sf::Vector2f p) {
        if (pts.empty()) {
            pts.push_back(p);
        } else {
            sf::Vector2f last = pts.back();
            float dx = p.x - last.x;
            float dy = p.y - last.y;
            if (dx * dx + dy * dy > 0.0001f) {
                pts.push_back(p);
            }
        }
    };

    const float PI_2 = 1.570796f;
    for (size_t i = 0; i < pointsPerCorner; i++) {
        float a = (float)i * PI_2 / (pointsPerCorner - 1);
        addUniquePoint({size.x - safeRadius + std::cos(a) * safeRadius, size.y - safeRadius + std::sin(a) * safeRadius});
    }
    for (size_t i = 0; i < pointsPerCorner; i++) {
        float a = (float)i * PI_2 / (pointsPerCorner - 1);
        addUniquePoint({safeRadius - std::sin(a) * safeRadius, size.y - safeRadius + std::cos(a) * safeRadius});
    }
    for (size_t i = 0; i < pointsPerCorner; i++) {
        float a = (float)i * PI_2 / (pointsPerCorner - 1);
        addUniquePoint({safeRadius - std::cos(a) * safeRadius, safeRadius - std::sin(a) * safeRadius});
    }
    for (size_t i = 0; i < pointsPerCorner; i++) {
        float a = (float)i * PI_2 / (pointsPerCorner - 1);
        addUniquePoint({size.x - safeRadius + std::sin(a) * safeRadius, safeRadius - std::cos(a) * safeRadius});
    }

    if (pts.size() > 1) {
        sf::Vector2f first = pts[0];
        sf::Vector2f last = pts.back();
        float dx = first.x - last.x;
        float dy = first.y - last.y;
        if (dx * dx + dy * dy < 0.0001f) pts.pop_back();
    }

    if (pts.size() < 3) return;

    sf::ConvexShape shape(pts.size());
    for (size_t i = 0; i < pts.size(); i++) shape.setPoint(i, pts[i]);

    shape.setFillColor(color);
    shape.setOutlineColor(outline);
    shape.setOutlineThickness(thickness);
    shape.setPosition(pos);
    window.draw(shape);
}
static void draw_element_box(sf::RenderWindow& window, sf::Vector2f pos, sf::Vector2f size, sf::Color color, sf::Color outline) {
    UIStyle* s = get_style();
    draw_rounded_rect(window, pos, size, s->borderRadius, color, outline, s->borderThickness);
}

static SapphireValue native_ui_begin(int arg_count, SapphireValue* args) {
    if (g_current_vm->sfml_window == nullptr) return {};

    while (const std::optional event = g_current_vm->sfml_window->pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            g_current_vm->sfml_window->close();
            exit(0);
        }

        if (const auto* resized = event->getIf<sf::Event::Resized>()) {
            sf::FloatRect visibleArea({0.f, 0.f}, {(float)resized->size.x, (float)resized->size.y});
            g_current_vm->sfml_window->setView(sf::View(visibleArea));
        }

        if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            if (keyPressed->code == sf::Keyboard::Key::Left) {
                if (g_current_vm->ui_state.cursorPos > 0) {
                    g_current_vm->ui_state.cursorPos--;
                }
            } else if (keyPressed->code == sf::Keyboard::Key::Right) {
                if (g_current_vm->ui_state.cursorPos < g_current_vm->ui_state.inputBuffer.length()) {
                    g_current_vm->ui_state.cursorPos++;
                }
            } else if (keyPressed->code == sf::Keyboard::Key::Delete) {
                if (g_current_vm->ui_state.cursorPos < g_current_vm->ui_state.inputBuffer.length()) {
                    g_current_vm->ui_state.inputBuffer.erase(g_current_vm->ui_state.cursorPos, 1);
                }
            }
        }

        if (const auto* textEntered = event->getIf<sf::Event::TextEntered>()) {
            if (textEntered->unicode < 128) {
                char c = static_cast<char>(textEntered->unicode);
                if (c == '\b') {
                    if (g_current_vm->ui_state.cursorPos > 0) {
                        g_current_vm->ui_state.inputBuffer.erase(g_current_vm->ui_state.cursorPos - 1, 1);
                        g_current_vm->ui_state.cursorPos--;
                    }
                } else if (c >= 32 && c <= 126) {
                    g_current_vm->ui_state.inputBuffer.insert(g_current_vm->ui_state.cursorPos, 1, c);
                    g_current_vm->ui_state.cursorPos++;
                }
            }
        }
    }

    sf::Color clearColor = g_current_vm->ui_state.currentStyleColor;

    if (g_current_vm->ui_state.activeStyle != nullptr) {
        clearColor = g_current_vm->ui_state.activeStyle->bgColor;
    }

    g_current_vm->sfml_window->clear(clearColor);

    g_current_vm->ui_state.nextPosX = 20.0f;
    g_current_vm->ui_state.nextPosY = 40.0f;
    g_current_vm->ui_state.lastItemHeight = 0.0f;

    return {};
}

static SapphireValue native_ui_style_component(int arg_count, SapphireValue* args) {
    if (arg_count < 7) return {};
    std::string id = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    ComponentProps props;
    props.bgColor = hexToColor(static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars);
    props.textColor = hexToColor(static_cast<ObjString*>(std::get<Obj*>(args[2]._value))->chars);
    props.accentColor = hexToColor(static_cast<ObjString*>(std::get<Obj*>(args[3]._value))->chars);
    props.borderRadius = (float)std::get<double>(args[4]._value);
    props.fontSize = (unsigned int)std::get<double>(args[5]._value);
    props.padding = (float)std::get<double>(args[6]._value);
    g_current_vm->ui_state.idOverrides[id] = props;
    return {};
}

static SapphireValue native_ui_set_bg_color(int arg_count, SapphireValue* args) {
    if (arg_count < 1) return {};
    std::string hex = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    g_current_vm->ui_state.currentStyleColor = hexToColor(hex);
    return {};
}

static SapphireValue native_ui_button(int arg_count, SapphireValue* args) {
    if (arg_count < 1) return false;
    ObjString* label = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    std::string id = (arg_count >= 4) ? static_cast<ObjString*>(std::get<Obj*>(args[3]._value))->chars : "";

    UIStyle s = resolve_style(id);

    float w = (arg_count >= 2) ? (float)std::get<double>(args[1]._value) : 150.0f;
    float h = (arg_count >= 3) ? (float)std::get<double>(args[2]._value) : 40.0f;

    sf::Vector2f pos(g_current_vm->ui_state.nextPosX, g_current_vm->ui_state.nextPosY);
    sf::Vector2i m = sf::Mouse::getPosition(*g_current_vm->sfml_window);
    bool hovered = sf::FloatRect(pos, {w, h}).contains(sf::Vector2f((float)m.x, (float)m.y));

    draw_rounded_rect(*g_current_vm->sfml_window, pos, {w, h}, s.borderRadius,
                      hovered ? s.hoverColor : s.bgColor, s.accentColor, s.borderThickness);

    sapphire_render_text(*g_current_vm->sfml_window, label->chars, {pos.x + s.padding, pos.y + (h/2.0f) - (s.fontSize/2.0f)}, s.textColor, s.fontAlias, s.fontSize);

    g_current_vm->ui_state.nextPosY += h + s.padding;

    if (hovered && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsed = now - g_current_vm->ui_state.lastClickTime;
        if (elapsed.count() > g_current_vm->ui_state.debounceTime) {
            g_current_vm->ui_state.lastClickTime = now;
            return true;
        }
    }
    return false;
}

static SapphireValue native_ui_sameline(int arg_count, SapphireValue* args) {
    float spacing = 10.0f;
    g_current_vm->ui_state.nextPosY -= (g_current_vm->ui_state.lastItemHeight + 10.0f);
    g_current_vm->ui_state.nextPosX += 130.0f;
    return {};
}

static SapphireValue native_ui_set_next_window_pos(int arg_count, SapphireValue* args) {
    if (arg_count >= 2) {
        g_current_vm->ui_state.nextPosX = (float)std::get<double>(args[0]._value);
        g_current_vm->ui_state.nextPosY = (float)std::get<double>(args[1]._value);
    }
    return {};
}

static SapphireValue native_ui_create_style(int arg_count, SapphireValue* args) {
    if (arg_count < 9) {
        std::cerr << "[SAPPHIRE ERROR] 'CreateStyle' requires 9 arguments: (name, bg, text, accent, thickness, hover, radius, fontAlias, fontSize)" << std::endl;
        return {};
    }

    std::string styleName = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    UIStyle style;

    style.bgColor         = hexToColor(static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars);
    style.textColor       = hexToColor(static_cast<ObjString*>(std::get<Obj*>(args[2]._value))->chars);
    style.accentColor     = hexToColor(static_cast<ObjString*>(std::get<Obj*>(args[3]._value))->chars);
    style.borderThickness = (float)std::get<double>(args[4]._value);
    style.hoverColor      = hexToColor(static_cast<ObjString*>(std::get<Obj*>(args[5]._value))->chars);
    style.borderRadius    = (float)std::get<double>(args[6]._value);
    style.fontSize        = static_cast<unsigned int>(std::get<double>(args[8]._value));

    std::string fontAlias = static_cast<ObjString*>(std::get<Obj*>(args[7]._value))->chars;

    if (g_current_vm->ui_state.fontStack.find(fontAlias) == g_current_vm->ui_state.fontStack.end()) {
        sf::Font newFont;
        std::string path = "data/fonts/" + fontAlias + ".ttf";

        if (newFont.openFromFile(path)) {
            g_current_vm->ui_state.fontStack[fontAlias] = newFont;
            std::cout << "[SAPPHIRE] Fonte carregada: " << fontAlias << std::endl;
        } else {
            std::cerr << "[SAPPHIRE WARNING] Arquivo nao encontrado: " << path << ". Usando default." << std::endl;
            fontAlias = "default";
        }
    }

    style.fontAlias = fontAlias;
    g_current_vm->ui_state.stylesheets[styleName] = style;
    return {};
}

static SapphireValue native_ui_text(int arg_count, SapphireValue* args) {
    if (arg_count < 1) return {};
    std::string content = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;

    std::string font = (arg_count >= 2) ? static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars : "";
    int size = (arg_count >= 3) ? (int)std::get<double>(args[2]._value) : -1;

    sapphire_render_text(*g_current_vm->sfml_window, content,
                         {g_current_vm->ui_state.nextPosX, g_current_vm->ui_state.nextPosY},
                         get_style()->textColor, font, size);

    g_current_vm->ui_state.nextPosY += (size > 0 ? size : get_style()->fontSize) + 10.0f;
    return {};
}

static SapphireValue native_ui_set_next_window_size(int arg_count, SapphireValue* args) {
    return {};
}

static SapphireValue native_ui_push_style(int arg_count, SapphireValue* args) {
    std::string name = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    auto it = g_current_vm->ui_state.stylesheets.find(name);
    if (it != g_current_vm->ui_state.stylesheets.end()) {
        g_current_vm->ui_state.activeStyle = &it->second;
    }
    return {};
}

static SapphireValue native_ui_separator(int arg_count, SapphireValue* args) {
    sf::RectangleShape line({(float)g_current_vm->config.windowWidth - 20.0f, 1.0f});
    line.setPosition({10.0f, g_current_vm->ui_state.nextPosY});
    line.setFillColor(sf::Color(150, 150, 150, 100));
    g_current_vm->sfml_window->draw(line);
    g_current_vm->ui_state.nextPosY += 10.0f;
    return {};
}

static SapphireValue native_ui_spacing(int arg_count, SapphireValue* args) {
    g_current_vm->ui_state.nextPosY += 20.0f;
    return {};
}

static SapphireValue native_ui_pop_style(int arg_count, SapphireValue* args) {
    g_current_vm->ui_state.activeStyle = nullptr;
    return {};
}
static SapphireValue native_ui_checkbox(int arg_count, SapphireValue* args) {
    ObjString* label = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    bool checked = std::get<bool>(args[1]._value);
    std::string id = (arg_count >= 3) ? static_cast<ObjString*>(std::get<Obj*>(args[2]._value))->chars : "";

    UIStyle s = resolve_style(id);
    float size = 20.0f;
    sf::Vector2f pos(g_current_vm->ui_state.nextPosX, g_current_vm->ui_state.nextPosY);

    sf::Vector2i m = sf::Mouse::getPosition(*g_current_vm->sfml_window);
    bool hovered = sf::FloatRect(pos, {size, size}).contains(sf::Vector2f((float)m.x, (float)m.y));

    if (hovered && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsed = now - g_current_vm->ui_state.lastClickTime;
        if (elapsed.count() > g_current_vm->ui_state.debounceTime) {
            g_current_vm->ui_state.lastClickTime = now;
            checked = !checked;
        }
    }

    draw_rounded_rect(*g_current_vm->sfml_window, pos, {size, size}, s.borderRadius, hovered ? s.hoverColor : s.bgColor, s.accentColor, s.borderThickness);

    if (checked) {
        float offset = 5.0f;
        draw_rounded_rect(*g_current_vm->sfml_window, {pos.x + offset, pos.y + offset}, {size - 10, size - 10}, s.borderRadius * 0.5f, s.accentColor, sf::Color::Transparent, 0);
    }

    sapphire_render_text(*g_current_vm->sfml_window, label->chars, {pos.x + size + 10.0f, pos.y}, s.textColor, s.fontAlias, 14);
    g_current_vm->ui_state.nextPosY += size + s.padding;
    return checked;
}

static bool g_menu_open = false;
static std::string g_active_menu = "";

static SapphireValue native_ui_menubar(int arg_count, SapphireValue* args) {
    UIStyle* s = get_style();
    float windowW = (float)g_current_vm->config.windowWidth;
    if (windowW <= 0) windowW = 800.0f;

    sf::RectangleShape bar({windowW, 30.0f});
    bar.setPosition({0, 0});
    bar.setFillColor(s->bgColor);
    g_current_vm->sfml_window->draw(bar);

    g_current_vm->ui_state.nextPosX = 10.0f;
    g_current_vm->ui_state.nextPosY = 5.0f;
    return {};
}

static SapphireValue native_ui_menu(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !is_obj_type(args[0], OBJ_STRING)) return false;
    ObjString* label = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    UIStyle* s = get_style();

    sf::Text txt(g_current_vm->sapphire_font, label->chars, 14);
    float textWidth = txt.getLocalBounds().size.x;
    float paddingX = 20.0f;
    float itemWidth = textWidth + paddingX * 2;
    float itemHeight = 30.0f;

    sf::Vector2u winSize = g_current_vm->sfml_window->getSize();
    float windowWidth = static_cast<float>(winSize.x);

    if (g_current_vm->ui_state.nextPosX + itemWidth > windowWidth - 10.0f) {
        g_current_vm->ui_state.nextPosX = 15.0f;
        g_current_vm->ui_state.nextPosY += itemHeight + 5.0f;
    }

    sf::Vector2f pos(g_current_vm->ui_state.nextPosX, g_current_vm->ui_state.nextPosY);
    sf::Vector2i mousePos = sf::Mouse::getPosition(*g_current_vm->sfml_window);
    sf::FloatRect menuRect(pos, {itemWidth, itemHeight});
    bool hovered = menuRect.contains(sf::Vector2f(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y)));

    if (hovered && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsed = now - g_current_vm->ui_state.lastClickTime;

        if (elapsed.count() > g_current_vm->ui_state.debounceTime) {
            g_current_vm->ui_state.lastClickTime = now;
            if (g_current_vm->ui_state.activeMenu == label->chars) {
                g_current_vm->ui_state.activeMenu = "";
            } else {
                g_current_vm->ui_state.activeMenu = label->chars;
            }
        }
    }

    bool isOpen = (g_current_vm->ui_state.activeMenu == label->chars);
    sf::Color bgColor = (hovered || isOpen) ? s->hoverColor : sf::Color::Transparent;

    draw_rounded_rect(*g_current_vm->sfml_window, pos, {itemWidth, itemHeight}, s->borderRadius * 0.4f, bgColor, sf::Color::Transparent, 0);

    txt.setPosition({pos.x + paddingX, pos.y + (itemHeight / 2.0f) - 10.0f});
    txt.setFillColor(s->textColor);
    g_current_vm->sfml_window->draw(txt);

    g_current_vm->ui_state.nextPosX += itemWidth + 5.0f;

    if (isOpen) {
        g_current_vm->ui_state.activeMenuPos = pos;
        g_current_vm->ui_state.activeMenuWidth = itemWidth;
        g_current_vm->ui_state.menuOffsetY = itemHeight + 5.0f;
    }

    return isOpen;
}

static SapphireValue native_ui_menu_item(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !is_obj_type(args[0], OBJ_STRING)) return false;
    ObjString* label = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    UIStyle* s = get_style();

    float width = 180.0f;
    float height = 28.0f;

    sf::Vector2f pos(g_current_vm->ui_state.activeMenuPos.x,
                     g_current_vm->ui_state.activeMenuPos.y + g_current_vm->ui_state.menuOffsetY);

    sf::Vector2i m = sf::Mouse::getPosition(*g_current_vm->sfml_window);
    sf::FloatRect rectBounds(pos, {width, height});
    bool hovered = rectBounds.contains(sf::Vector2f(static_cast<float>(m.x), static_cast<float>(m.y)));
    bool clicked = false;

    if (hovered && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsed = now - g_current_vm->ui_state.lastClickTime;

        if (elapsed.count() > g_current_vm->ui_state.debounceTime) {
            g_current_vm->ui_state.lastClickTime = now;
            g_current_vm->ui_state.activeMenu = "";
            clicked = true;
        }
    }

    sf::RectangleShape rect({width, height});
    rect.setPosition(pos);
    rect.setFillColor(hovered ? s->accentColor : s->bgColor);
    rect.setOutlineThickness(s->borderThickness);
    rect.setOutlineColor(s->borderColor);
    g_current_vm->sfml_window->draw(rect);

    sf::Text txt(g_current_vm->sapphire_font, label->chars, 13);
    txt.setPosition({pos.x + 10.0f, pos.y + 5.0f});
    txt.setFillColor(hovered ? sf::Color::White : s->textColor);
    g_current_vm->sfml_window->draw(txt);

    g_current_vm->ui_state.menuOffsetY += height;

    return clicked;
}

static SapphireValue native_ui_slider(int arg_count, SapphireValue* args) {
    float val = (float)std::get<double>(args[0]._value);
    float min = (float)std::get<double>(args[1]._value);
    float max = (float)std::get<double>(args[2]._value);
    std::string id = (arg_count >= 4) ? static_cast<ObjString*>(std::get<Obj*>(args[3]._value))->chars : "";

    UIStyle s = resolve_style(id);
    float width = 200.0f;

    sf::RectangleShape bar({width, 6.0f});
    bar.setPosition({g_current_vm->ui_state.nextPosX, g_current_vm->ui_state.nextPosY + 10});
    bar.setFillColor(s.borderColor);
    g_current_vm->sfml_window->draw(bar);

    float pos = ((val - min) / (max - min)) * width;
    sf::CircleShape handle(8.0f);
    handle.setOrigin({8.0f, 8.0f});
    handle.setPosition({g_current_vm->ui_state.nextPosX + pos, g_current_vm->ui_state.nextPosY + 13.0f});
    handle.setFillColor(s.accentColor);

    sf::Vector2i m = sf::Mouse::getPosition(*g_current_vm->sfml_window);
    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        sf::FloatRect area({g_current_vm->ui_state.nextPosX, g_current_vm->ui_state.nextPosY}, {width, 25.0f});
        if (area.contains(sf::Vector2f((float)m.x, (float)m.y))) {
            float newPos = std::clamp((float)m.x - g_current_vm->ui_state.nextPosX, 0.0f, width);
            val = min + (newPos / width) * (max - min);
        }
    }

    g_current_vm->sfml_window->draw(handle);
    g_current_vm->ui_state.nextPosY += 30.0f;
    return (double)val;
}

static SapphireValue native_ui_input(int arg_count, SapphireValue* args) {
    ObjString* sapphireStr = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    std::string id = (arg_count >= 2) ? static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars : "";
    UIStyle s = resolve_style(id);

    sapphireStr->chars = g_current_vm->ui_state.inputBuffer;

    float width = 250.0f;
    float height = 35.0f;
    sf::Vector2f pos(g_current_vm->ui_state.nextPosX, g_current_vm->ui_state.nextPosY);

    draw_rounded_rect(*g_current_vm->sfml_window, pos, {width, height}, s.borderRadius, s.bgColor, s.accentColor, s.borderThickness);

    sf::Vector2u winSize = g_current_vm->sfml_window->getSize();
    if (winSize.x > 0 && winSize.y > 0 && width > 0 && height > 0) {
        sf::View oldView = g_current_vm->sfml_window->getView();
        sf::FloatRect viewportRect({pos.x / (float)winSize.x, pos.y / (float)winSize.y}, {width / (float)winSize.x, height / (float)winSize.y});
        sf::View inputView(sf::FloatRect({0.f, 0.f}, {width, height}));
        inputView.setViewport(viewportRect);
        g_current_vm->sfml_window->setView(inputView);

        std::string displayText = sapphireStr->chars;
        if (g_current_vm->ui_state.cursorPos > displayText.length()) {
            g_current_vm->ui_state.cursorPos = displayText.length();
        }
        displayText.insert(g_current_vm->ui_state.cursorPos, "|");

        sf::Text txt(g_current_vm->ui_state.fontStack["default"], displayText, s.fontSize);
        txt.setFillColor(s.textColor);

        float textWidth = txt.getLocalBounds().size.x;
        float maxVisibleWidth = width - 20.0f;
        float scrollOffset = (textWidth > maxVisibleWidth) ? textWidth - maxVisibleWidth : 0.0f;

        txt.setPosition({10.0f - scrollOffset, (height / 2.0f) - (s.fontSize / 2.0f)});
        g_current_vm->sfml_window->draw(txt);
        g_current_vm->sfml_window->setView(oldView);
    }

    g_current_vm->ui_state.nextPosY += height + s.padding;
    return args[0];
}

static SapphireValue native_ui_end(int arg_count, SapphireValue* args) {
    g_current_vm->sfml_window->display();
    return {};
}

static SapphireValue native_len(int arg_count, SapphireValue* args) {
    if (arg_count != 1) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: len() expects 1 argument." << std::endl;
        }
        return {};
    }

    SapphireValue value = args[0];

    if (is_obj_type(value, OBJ_STRING)) {
        ObjString* str = static_cast<ObjString*>(std::get<Obj*>(value._value));
        return (double)str->chars.length();
    }
    else if (std::holds_alternative<std::shared_ptr<SapphireArray>>(value._value)) {
        auto array_obj = std::get<std::shared_ptr<SapphireArray>>(value._value);
        return (double)array_obj->elements.size();
    }

    if (!g_current_vm->soft_mode) {
        std::cerr << "Runtime Error: len() argument must be a string or an array." << std::endl;
    }
    return {};
}

static SapphireValue native_http_get(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: HTTP.get() expects 1 string type argument (the URL)." << std::endl;
        }
        return {};
    }

    ObjString* url_obj = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    std::string url_str = url_obj->chars;
    std::string host, path;

    size_t host_start = url_str.find("://");
    if (host_start == std::string::npos) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Invalid URL." << std::endl;
        }
        return {};
    }
    host_start += 3;
    size_t path_start = url_str.find('/', host_start);
    if (path_start == std::string::npos) {
        host = url_str;
        path = "/";
    } else {
        host = url_str.substr(0, path_start);
        path = url_str.substr(path_start);
    }

    try {
        httplib::Client cli(host.c_str());
        cli.set_follow_location(true);
        auto res = cli.Get(path.c_str());

        if (res && res->status == 200) {
            return new_string(g_current_vm, res->body);
        }
    } catch (...) {}

    return {};
}

static SapphireValue native_http_ping(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return false;

    std::string url_str = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;

    try {
        httplib::Client cli(url_str.c_str());
        cli.set_connection_timeout(std::chrono::seconds(2));
        if (auto res = cli.Get("/")) {
            return res->status == 200;
        }
    } catch (...) {}

    return false;
}

static SapphireValue native_http_post(int arg_count, SapphireValue* args) {
    if (arg_count < 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) return {};

    std::string url_str = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    std::string body = static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars;
    std::string content_type = (arg_count == 3 && is_obj_type(args[2], OBJ_STRING))
                               ? static_cast<ObjString*>(std::get<Obj*>(args[2]._value))->chars
                               : "application/json";

    try {
        httplib::Client cli(url_str.c_str());
        if (auto res = cli.Post("/", body, content_type.c_str())) {
            return new_string(g_current_vm, res->body);
        }
    } catch (...) {}

    return {};
}

static SapphireValue native_http_download(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) return false;

    std::string url_str = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    std::string path = static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars;

    try {
        httplib::Client cli(url_str.c_str());
        auto res = cli.Get("/");
        if (res && res->status == 200) {
            std::ofstream file(path, std::ios::binary);
            if (!file.is_open()) return false;
            file << res->body;
            file.close();
            return true;
        }
    } catch (...) {}

    return false;
}

static SapphireValue native_json_parse(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: JSON.parse() expects 1 string argument." << std::endl;
        }
        return {};
    }

    ObjString* json_string_obj = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    const std::string& json_string = json_string_obj->chars;

    try {
        nlohmann::json parsed_json = nlohmann::json::parse(json_string);
        ObjClass* json_object_class = new_class(g_current_vm, new_string(g_current_vm, "JsonObject"));
        return convertJsonToSapphire(g_current_vm, parsed_json, json_object_class);
    } catch (const std::exception& e) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Failed to parse JSON string: " << e.what() << std::endl;
        }
        return {};
    }
}

static SapphireValue core_create_instance(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Core.createInstance() expects 1 string argument (class name)." << std::endl;
        }
        return {};
    }

    ObjString* class_name_obj = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    std::string class_name = class_name_obj->chars;

    auto it = g_current_vm->globals.find(class_name);
    if (it == g_current_vm->globals.end()) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Class '" << class_name << "' not found." << std::endl;
        }
        return {};
    }

    SapphireValue class_value = it->second;

    if (!std::holds_alternative<Obj*>(class_value._value) || std::get<Obj*>(class_value._value)->type != OBJ_CLASS) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Global variable '" << class_name << "' is not a class." << std::endl;
        }
        return {};
    }

    ObjClass* klass = static_cast<ObjClass*>(std::get<Obj*>(class_value._value));
    ObjInstance* instance = new_instance(g_current_vm, klass);
    return instance;
}

static SapphireValue native_list_util_create(int arg_count, SapphireValue* args) {
    if (arg_count != 0) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.create() expects 0 arguments." << std::endl;
        }
        return {};
    }
    return std::make_shared<SapphireArray>();
}

static SapphireValue native_list_util_append(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.append() expects a list and a value." << std::endl;
        }
        return {};
    }
    auto list_obj = std::get<std::shared_ptr<SapphireArray>>(args[0]._value);
    list_obj->elements.push_back(args[1]);
    return args[0];
}

static SapphireValue native_list_util_get(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value) || !std::holds_alternative<double>(args[1]._value)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.get() expects a list and an index (number)." << std::endl;
        }
        return {};
    }
    auto list_obj = std::get<std::shared_ptr<SapphireArray>>(args[0]._value);
    int index = static_cast<int>(std::get<double>(args[1]._value));

    if (index < 0 || index >= list_obj->elements.size()) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.get(): Index out of bounds." << std::endl;
        }
        return {};
    }
    return list_obj->elements[index];
}

static SapphireValue native_list_util_set(int arg_count, SapphireValue* args) {
    if (arg_count != 3 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value) || !std::holds_alternative<double>(args[1]._value)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.set() expects a list, an index (number), and a value." << std::endl;
        }
        return {};
    }
    auto list_obj = std::get<std::shared_ptr<SapphireArray>>(args[0]._value);
    int index = static_cast<int>(std::get<double>(args[1]._value));
    SapphireValue new_value = args[2];

    if (index < 0 || index >= list_obj->elements.size()) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.set(): Index out of bounds." << std::endl;
        }
        return {};
    }
    list_obj->elements[index] = new_value;
    return args[0];
}

static SapphireValue native_list_util_length(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.length() expects 1 list argument." << std::endl;
        }
        return 0.0;
    }
    auto list_obj = std::get<std::shared_ptr<SapphireArray>>(args[0]._value);
    return (double)list_obj->elements.size();
}

static SapphireValue native_list_util_remove_at(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value) || !std::holds_alternative<double>(args[1]._value)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.removeAt() expects a list and an index (number)." << std::endl;
        }
        return {};
    }
    auto list_obj = std::get<std::shared_ptr<SapphireArray>>(args[0]._value);
    int index = static_cast<int>(std::get<double>(args[1]._value));

    if (index < 0 || index >= list_obj->elements.size()) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.removeAt(): Index out of bounds." << std::endl;
        }
        return {};
    }

    SapphireValue removed_value = list_obj->elements[index];
    list_obj->elements.erase(list_obj->elements.begin() + index);
    return removed_value;
}

static SapphireValue native_list_util_contains(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: ListUtil.contains() expects a list and a value." << std::endl;
        }
        return false;
    }
    auto list_obj = std::get<std::shared_ptr<SapphireArray>>(args[0]._value);
    SapphireValue value_to_find = args[1];

    for (const auto& element : list_obj->elements) {
        if (element._value == value_to_find._value) {
            return true;
        }
    }
    return false;
}

static SapphireValue native_math_rand(int arg_count, SapphireValue* args) {
    if (arg_count > 2) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Math.rand() expects 0, 1 or 2 arguments." << std::endl;
        }
        return {};
    }

    double min_val = 0.0;
    double max_val = 1.0;

    if (arg_count == 1) {
        if (!std::holds_alternative<double>(args[0]._value)) {
            if (!g_current_vm->soft_mode) {
                std::cerr << "Runtime Error: Math.rand(max) expects a number for max value." << std::endl;
            }
            return {};
        }
        max_val = std::get<double>(args[0]._value);
    } else if (arg_count == 2) {
        if (!std::holds_alternative<double>(args[0]._value) || !std::holds_alternative<double>(args[1]._value)) {
            if (!g_current_vm->soft_mode) {
                std::cerr << "Runtime Error: Math.rand(min, max) expects numbers for min and max values." << std::endl;
            }
            return {};
        }
        min_val = std::get<double>(args[0]._value);
        max_val = std::get<double>(args[1]._value);
    }

    if (min_val > max_val) {
        std::swap(min_val, max_val);
    }

    std::uniform_real_distribution<double> distrib(min_val, max_val);
    return distrib(gen);
}

static SapphireValue native_string_to_double(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        return 0.0;
    }
    ObjString* str = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    try {
        return std::stod(str->chars);
    } catch (const std::exception&) {
        return 0.0;
    }
}

static SapphireValue native_evaluate(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        return new_string(g_current_vm, "Error");
    }

    ObjString* source_string = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    std::string source_to_run = source_string->chars;

    VM* previous_vm = g_current_vm;

    ScriptConfig temp_config;
    VM temp_vm;
    temp_vm.globals = previous_vm->globals;

    g_current_vm = &temp_vm;
    SapphireValue result = temp_vm.interpret(source_to_run);
    g_current_vm = previous_vm;

    if (std::holds_alternative<std::monostate>(result._value)) {
        return new_string(g_current_vm, "Error");
    }
    else if (std::holds_alternative<bool>(result._value)) {
        return new_string(g_current_vm, std::get<bool>(result._value) ? "true" : "false");
    }
    else if (std::holds_alternative<double>(result._value)) {
        double num = std::get<double>(result._value);
        std::string s = std::to_string(num);
        s.erase(s.find_last_not_of('0') + 1, std::string::npos);
        if (s.back() == '.') {
            s.pop_back();
        }
        return new_string(g_current_vm, s);
    }
    else if (is_obj_type(result, OBJ_STRING)) {
        ObjString* str_obj = static_cast<ObjString*>(std::get<Obj*>(result._value));
        return new_string(g_current_vm, str_obj->chars);
    }

    return new_string(g_current_vm, "Error");
}



static SapphireValue native_system_sleep(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !std::holds_alternative<double>(args[0]._value)) {
        return {};
    }
    int ms = static_cast<int>(std::get<double>(args[0]._value));
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return {};
}

static SapphireValue native_system_get_os(int arg_count, SapphireValue* args) {
#ifdef _WIN32
    return new_string(g_current_vm, "Windows");
#elif __APPLE__
    return new_string(g_current_vm, "MacOS");
#elif __linux__
    return new_string(g_current_vm, "Linux");
#else
    return new_string(g_current_vm, "Unknown");
#endif
}

static SapphireValue native_system_get_env(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !is_obj_type(args[0], OBJ_STRING)) {
        return {};
    }

    std::string var_name = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    const char* env_value = std::getenv(var_name.c_str());

    if (env_value) {
        return new_string(g_current_vm, env_value);
    }

    if (arg_count == 2) {
        return args[1];
    }

    return {};
}

static SapphireValue native_system_get_clipboard(int arg_count, SapphireValue* args) {
    std::string text = sf::Clipboard::getString().toAnsiString();
    return new_string(g_current_vm, text);
}

static SapphireValue native_debug_print_stack(int arg_count, SapphireValue* args) {
    std::cout << "--- SAPPHIRE STACK DUMP ---" << std::endl;
    for (SapphireValue* slot = g_current_vm->stack; slot < g_current_vm->stack_top; slot++) {
        std::cout << "[ ";
        print_value(*slot);
        std::cout << " ]" << std::endl;
    }
    std::cout << "--- END OF STACK ---" << std::endl;
    return {};
}

static SapphireValue native_debug_dump_globals(int arg_count, SapphireValue* args) {
    std::cout << "--- SAPPHIRE GLOBALS DUMP ---" << std::endl;
    for (auto const& [name, value] : g_current_vm->globals) {
        std::cout << name << " => ";
        print_value(value);
        std::cout << std::endl;
    }
    return {};
}

static SapphireValue native_math_abs(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !std::holds_alternative<double>(args[0]._value)) return 0.0;
    return std::abs(std::get<double>(args[0]._value));
}

static SapphireValue native_math_floor(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !std::holds_alternative<double>(args[0]._value)) return 0.0;
    return std::floor(std::get<double>(args[0]._value));
}

static SapphireValue native_math_ceil(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !std::holds_alternative<double>(args[0]._value)) return 0.0;
    return std::ceil(std::get<double>(args[0]._value));
}

static SapphireValue native_math_sin(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !std::holds_alternative<double>(args[0]._value)) return 0.0;
    return std::sin(std::get<double>(args[0]._value));
}

static SapphireValue native_math_cos(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !std::holds_alternative<double>(args[0]._value)) return 0.0;
    return std::cos(std::get<double>(args[0]._value));
}

static SapphireValue native_math_pow(int arg_count, SapphireValue* args) {
    if (arg_count < 2) return 0.0;
    return std::pow(std::get<double>(args[0]._value), std::get<double>(args[1]._value));
}

static SapphireValue native_math_min(int arg_count, SapphireValue* args) {
    if (arg_count < 2) return args[0];
    double a = std::get<double>(args[0]._value);
    double b = std::get<double>(args[1]._value);
    return std::min(a, b);
}

static SapphireValue native_math_max(int arg_count, SapphireValue* args) {
    if (arg_count < 2) return args[0];
    double a = std::get<double>(args[0]._value);
    double b = std::get<double>(args[1]._value);
    return std::max(a, b);
}

static SapphireValue native_math_clamp(int arg_count, SapphireValue* args) {
    if (arg_count < 3) return args[0];
    double v = std::get<double>(args[0]._value);
    double lo = std::get<double>(args[1]._value);
    double hi = std::get<double>(args[2]._value);
    return std::clamp(v, lo, hi);
}

static SapphireValue native_math_lerp(int arg_count, SapphireValue* args) {
    if (arg_count < 3) return args[0];
    double a = std::get<double>(args[0]._value);
    double b = std::get<double>(args[1]._value);
    double t = std::get<double>(args[2]._value);
    return a + t * (b - a);
}

static SapphireValue native_color_hex_to_rgb(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !is_obj_type(args[0], OBJ_STRING)) return {};

    std::string hex = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    if (hex[0] == '#') hex.erase(0, 1);
    if (hex.length() != 6) return {};

    uint32_t value = std::stoul(hex, nullptr, 16);
    auto array_obj = std::make_shared<SapphireArray>();
    array_obj->elements.push_back((double)((value >> 16) & 0xFF)); // R
    array_obj->elements.push_back((double)((value >> 8) & 0xFF));  // G
    array_obj->elements.push_back((double)(value & 0xFF));         // B

    return array_obj;
}

VM::VM() : VM(ScriptConfig{}) {
}

VM::VM(const ScriptConfig& config) : VM(config, false, nullptr) {
}

VM::VM(const ScriptConfig& config, bool init_ui, sf::RenderWindow* window) : config(config) {
    this->frame_count = 0;
    this->stack_top = stack;
    this->objects = nullptr;
    this->sfml_window = window;

    define_native("clock", clock_native);
    define_native("parseDouble", native_string_to_double);
    define_native("valueToString", native_value_to_string);
    define_native("evaluate", native_evaluate);
    define_native("len", native_len);
    define_native("stringCharAt", native_string_char_at);

    const char* appdata_path = getenv("APPDATA");
    if (appdata_path) {
        std::string global_plugins_path = std::string(appdata_path) + "\\Sapphire\\plugins";
        module_search_paths.push_back(global_plugins_path);
    }

    // --- Módulo IO ---
    ObjClass* io_class = new_class(this, new_string(this, "IO"));
    ObjInstance* io_object = new_instance(this, io_class);
    io_object->fields["readLine"] = new_native(this, io_readline_native);
    io_object->fields["printColor"] = new_native(this, native_io_print_color);
    io_object->fields["readFile"] = new_native(this, native_io_read_file);
    io_object->fields["writeFile"] = new_native(this, native_io_write_file);
    io_object->fields["exists"] = new_native(this, native_io_exists);
    io_object->fields["deleteFile"] = new_native(this, native_io_delete_file);
    io_object->fields["appendFile"] = new_native(this, native_io_append_file);
    globals["IO"] = io_object;

    // --- Módulo Math (Expandido) ---
    ObjClass* math_class = new_class(this, new_string(this, "Math"));
    ObjInstance* math_object = new_instance(this, math_class);
    math_object->fields["sqrt"] = new_native(this, native_math_sqrt);
    math_object->fields["rand"] = new_native(this, native_math_rand);
    math_object->fields["abs"] = new_native(this, native_math_abs);
    math_object->fields["floor"] = new_native(this, native_math_floor);
    math_object->fields["ceil"] = new_native(this, native_math_ceil);
    math_object->fields["sin"] = new_native(this, native_math_sin);
    math_object->fields["cos"] = new_native(this, native_math_cos);
    math_object->fields["pow"] = new_native(this, native_math_pow);
    math_object->fields["min"] = new_native(this, native_math_min);
    math_object->fields["max"] = new_native(this, native_math_max);
    math_object->fields["clamp"] = new_native(this, native_math_clamp);
    math_object->fields["lerp"] = new_native(this, native_math_lerp);
    globals["Math"] = math_object;

    // --- Módulo JSON ---
    ObjClass* json_class = new_class(this, new_string(this, "JSON"));
    ObjInstance* json_object = new_instance(this, json_class);
    json_object->fields["parse"] = new_native(this, native_json_parse);
    globals["JSON"] = json_object;

    // --- Módulo Core ---
    ObjClass* core_class = new_class(this, new_string(this, "Core"));
    ObjInstance* core_object = new_instance(this, core_class);
    core_object->fields["createInstance"] = new_native(this, core_create_instance);
    globals["Core"] = core_object;

    // --- Módulo ListUtil ---
    ObjClass* list_util_class = new_class(this, new_string(this, "ListUtil"));
    ObjInstance* list_util_object = new_instance(this, list_util_class);
    list_util_object->fields["create"] = new_native(this, native_list_util_create);
    list_util_object->fields["append"] = new_native(this, native_list_util_append);
    list_util_object->fields["get"] = new_native(this, native_list_util_get);
    list_util_object->fields["set"] = new_native(this, native_list_util_set);
    list_util_object->fields["length"] = new_native(this, native_list_util_length);
    list_util_object->fields["removeAt"] = new_native(this, native_list_util_remove_at);
    list_util_object->fields["contains"] = new_native(this, native_list_util_contains);
    globals["ListUtil"] = list_util_object;

    // --- Módulo System (Expandido) ---
    ObjClass* system_class = new_class(this, new_string(this, "System"));
    ObjInstance* system_object = new_instance(this, system_class);
    system_object->fields["getEnv"] = new_native(this, native_system_get_env);
    system_object->fields["getOS"] = new_native(this, native_system_get_os);
    system_object->fields["sleep"] = new_native(this, native_system_sleep);
    system_object->fields["getClipboard"] = new_native(this, native_system_get_clipboard);
    globals["System"] = system_object;

    // --- Módulo HTTP (Expandido) ---
    ObjClass* http_class = new_class(this, new_string(this, "HTTP"));
    ObjInstance* http_object = new_instance(this, http_class);
    http_object->fields["get"] = new_native(this, native_http_get);
    http_object->fields["post"] = new_native(this, native_http_post);
    http_object->fields["ping"] = new_native(this, native_http_ping);
    http_object->fields["download"] = new_native(this, native_http_download);
    globals["HTTP"] = http_object;

    // --- Módulo Color ---
    ObjClass* color_class = new_class(this, new_string(this, "Color"));
    ObjInstance* color_object = new_instance(this, color_class);
    color_object->fields["hexToRGB"] = new_native(this, native_color_hex_to_rgb);
    globals["Color"] = color_object;

    // --- Módulo Debug ---
    ObjClass* debug_class = new_class(this, new_string(this, "Debug"));
    ObjInstance* debug_object = new_instance(this, debug_class);
    debug_object->fields["printStack"] = new_native(this, native_debug_print_stack);
    debug_object->fields["dumpGlobals"] = new_native(this, native_debug_dump_globals);
    globals["Debug"] = debug_object;

    // Registro SFML puro
    register_sfml_graphics_natives(this);

    if (init_ui) {
        g_current_vm = this;

        std::vector<std::string> fontsToLoad = { "ARIAL.TTF", "Courier.ttf", "TimesNewRoman.ttf" };
        std::vector<std::string> aliases = { "Arial", "Courier", "TimesNewRoman" };

        for (size_t i = 0; i < fontsToLoad.size(); i++) {
            sf::Font font;
            std::string path = "data/fonts/" + fontsToLoad[i];
            if (font.openFromFile(path)) {
                this->ui_state.fontStack[aliases[i]] = font;
                if (i == 0) this->sapphire_font = font;
            }
        }

        if (this->ui_state.fontStack.find("default") == this->ui_state.fontStack.end()) {
            this->ui_state.fontStack["default"] = this->sapphire_font;
        }

        ObjClass* ui_class = new_class(this, new_string(this, "UI"));
        ObjInstance* ui_object = new_instance(this, ui_class);
        ui_object->fields["Begin"] = new_native(this, native_ui_begin);
        ui_object->fields["End"] = new_native(this, native_ui_end);
        ui_object->fields["SetBGColor"] = new_native(this, native_ui_set_bg_color);
        ui_object->fields["Text"] = new_native(this, native_ui_text);
        ui_object->fields["Button"] = new_native(this, native_ui_button);
        ui_object->fields["Checkbox"] = new_native(this, native_ui_checkbox);
        ui_object->fields["Slider"] = new_native(this, native_ui_slider);
        ui_object->fields["Input"] = new_native(this, native_ui_input);
        ui_object->fields["MenuBar"] = new_native(this, native_ui_menubar);
        ui_object->fields["Menu"] = new_native(this, native_ui_menu);
        ui_object->fields["MenuItem"] = new_native(this, native_ui_menu_item);
        ui_object->fields["SameLine"] = new_native(this, native_ui_sameline);
        ui_object->fields["Separator"] = new_native(this, native_ui_separator);
        ui_object->fields["Spacing"] = new_native(this, native_ui_spacing);
        ui_object->fields["SetNextWindowPos"] = new_native(this, native_ui_set_next_window_pos);
        ui_object->fields["SetNextWindowSize"] = new_native(this, native_ui_set_next_window_size);
        ui_object->fields["CreateStyle"] = new_native(this, native_ui_create_style);
        ui_object->fields["PushStyle"] = new_native(this, native_ui_push_style);
        ui_object->fields["PopStyle"] = new_native(this, native_ui_pop_style);
        ui_object->fields["StyleComponent"] = new_native(this, native_ui_style_component);
        globals["UI"] = ui_object;

        globals["_ui_initialized"] = {};
        globals["APP_WINDOW_WIDTH"] = (double)config.windowWidth;
        globals["APP_WINDOW_HEIGHT"] = (double)config.windowHeight;
    }
}
std::string VM::find_and_load_module(const std::string& module_name) {
    std::string content = load_file_as_string(module_name);
    if (!content.empty()) {
        std::cout << "[INFO] Module '" << module_name << "' loaded directly." << std::endl;
        return content;
    }

    for (const std::string& base_path : module_search_paths) {
        std::string path_sp = base_path + "\\" + module_name + ".sp";
        std::cout << "[INFO] find_and_load_module: Trying .sp path '" << path_sp << "'" << std::endl;
        content = load_file_as_string(path_sp);
        if (!content.empty()) {
            std::cout << "[INFO] Module '" << module_name << "' loaded from: " << path_sp << std::endl;
            return content;
        }
        std::cout << "[INFO] find_and_load_module: .sp path failed." << std::endl;

        std::string path_main_sp = base_path + "\\" + module_name + "\\main.sp";
        content = load_file_as_string(path_main_sp);
        if (!content.empty()) {
            std::cout << "[INFO] Module '" << module_name << "' loaded from: " << path_main_sp << std::endl;
            return content;
        }
    }

    return "";
}


VM::~VM() {
    Obj* object = objects;
    while (object != nullptr) {
        Obj* next = object->next;
        free_object(object);
        object = next;
    }
    if (g_current_vm == this) {
        g_current_vm = nullptr;
    }
}

void VM::setGlobalNumber(const std::string& name, double value) {
    globals[name] = value;
}

void VM::define_native(const std::string& name, NativeFn function) {
    ObjNative* native = new_native(this, function);
    native->name = new_string(this, name);

    globals[name] = native;
}
void VM::push(const SapphireValue& value) {
    *stack_top = value;
    stack_top++;
}
SapphireValue VM::pop() {
    if (stack_top == stack) {
        std::cerr << "Runtime Error: Stack underflow." << std::endl;
        exit(70);
    }
    stack_top--;

    return *stack_top;
}
SapphireValue& VM::peek(int distance) {
    return stack_top[-1 - distance];
}

ObjFunction* VM::compile_module(const std::string& source) {
    return compile(this, source);
}

bool VM::call(ObjFunction* function, int arg_count) {
    if (function == nullptr) return false;

    if (arg_count != function->arity) {
        arg_count = function->arity;
    }

    if (frame_count == FRAMES_MAX) {
        if (!this->soft_mode) std::cerr << "Runtime Error: Stack overflow." << std::endl;
        return false;
    }

    CallFrame* frame = &frames[frame_count++];
    frame->function = function;
    frame->ip = &function->chunk.code[0];

    frame->slots = stack_top - arg_count - 1;
    return true;
}

bool VM::call_value(SapphireValue callee, int arg_count) {
    bool is_callable = std::holds_alternative<Obj*>(callee._value) &&
                      (std::get<Obj*>(callee._value)->type == OBJ_CLOSURE ||
                       std::get<Obj*>(callee._value)->type == OBJ_NATIVE ||
                       std::get<Obj*>(callee._value)->type == OBJ_CLASS);

    if (!is_callable) {
        for (int i = 1; i <= 4; i++) {
            SapphireValue potential = stack_top[-arg_count - 1 - i];
            if (std::holds_alternative<Obj*>(potential._value)) {
                Obj* o = std::get<Obj*>(potential._value);
                if (o->type == OBJ_CLOSURE || o->type == OBJ_NATIVE || o->type == OBJ_CLASS) {
                    callee = potential;
                    stack_top[-arg_count - 1] = callee;
                    is_callable = true;
                    break;
                }
            }
        }
    }

    if (is_callable) {
        Obj* obj = std::get<Obj*>(callee._value);
        switch (obj->type) {
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = (ObjBoundMethod*)obj;
                stack_top[-arg_count - 1] = bound->receiver;
                return call(bound->method->function, arg_count);
            }
            case OBJ_CLASS: {
                ObjClass* klass = (ObjClass*)obj;
                stack_top[-arg_count - 1] = new_instance(this, klass);
                return true;
            }
            case OBJ_CLOSURE:
                return call(((ObjClosure*)obj)->function, arg_count);
            case OBJ_NATIVE: {
                NativeFn native = ((ObjNative*)obj)->function;
                SapphireValue result = native(arg_count, stack_top - arg_count);
                stack_top -= arg_count + 1;
                push(result);
                return true;
            }
            default: break;
        }
    }

    if (!this->soft_mode) {
        std::cerr << "Runtime Error: Can only call functions and classes." << std::endl;
    }
    return false;
}

        // std::cout << "    [CALL_VALUE SPY] Despachando chamada para objeto tipo: " << obj->type << std::endl;
        // Eu nem tiro mais os debugs, vai que eu preciso ¯\_(ツ)_/¯

bool VM::run() {
    CallFrame* frame = &frames[frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define BINARY_OP(value_type, op) \
    do { \
        if (!std::holds_alternative<double>(peek(0)._value) || !std::holds_alternative<double>(peek(1)._value)) { \
            if (!this->soft_mode) { \
                std::cerr << "Runtime Error: Operands must be numbers." << std::endl; \
                return false; \
            } \
            pop(); pop(); push({}); break; \
        } \
        double b = std::get<double>(pop()._value); \
        double a = std::get<double>(pop()._value); \
        push(value_type(a op b)); \
    } while (false)

    for (;;) {
        if (frame_count >= FRAMES_MAX - 1) {
            std::cout << "--- STACK TRACE ANTES DO OVERFLOW ---" << std::endl;
            std::cout << "Frames ativos: " << frame_count << std::endl;
            for (int i = 0; i < frame_count; i++) {
                std::cout << "  Frame [" << i << "]: "
                          << (frames[i].function->name ? frames[i].function->name->chars : "script")
                          << std::endl;
            }
            debug_print_stack(this);
        }
#ifdef DEBUG_TRACE_EXECUTION
        std::cout << "          ";
        debug_print_stack(this);
        disassemble_instruction(frame->function->chunk, (int)(frame->ip - &frame->function->chunk.code[0]));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT:      push(frame->function->chunk.constants[READ_SHORT()]); break;
            case OP_NIL:           push({}); break;
            case OP_TRUE:          push(true); break;
            case OP_FALSE:         push(false); break;
            case OP_POP:           pop(); break;
            case OP_GET_LOCAL:     push(frame->slots[READ_BYTE()]); break;
            case OP_SET_LOCAL:     frame->slots[READ_BYTE()] = peek(0); break;
            case OP_GET_GLOBAL: {
                ObjString* name = static_cast<ObjString*>(std::get<Obj*>(frame->function->chunk.constants[READ_SHORT()]._value));
                auto it = globals.find(name->chars);
                if (it == globals.end()) {
                    std::cout << "[VM_DEBUG] GLOBAL NAO ENCONTRADA: '" << name->chars << "'" << std::endl;
                    if (!this->soft_mode) {
                        std::cerr << "Runtime Error: Undefined global variable '" << name->chars << "'." << std::endl;
                        return false;
                    }
                    push({});
                } else {
                    push(it->second);
                }
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = static_cast<ObjString*>(std::get<Obj*>(frame->function->chunk.constants[READ_SHORT()]._value));
                globals[name->chars] = peek(0);

                pop();
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = static_cast<ObjString*>(std::get<Obj*>(frame->function->chunk.constants[READ_SHORT()]._value));
                auto it = globals.find(name->chars);
                if (it == globals.end()) {
                    if (!this->soft_mode) {
                        std::cerr << "Runtime Error: Undefined global variable for assignment '" << name->chars << "'." << std::endl;
                        return false;
                    }
                } else {
                    it->second = peek(0);
                }
                break;
            }
            case OP_GET_PROPERTY: {
                if (!is_obj_type(peek(0), OBJ_INSTANCE)) {
                    if (!this->soft_mode) {
                        std::cerr << "Runtime Error: Only instances have properties." << std::endl;
                        return false;
                    }
                    pop(); push({}); break;
                }
                ObjInstance* instance = static_cast<ObjInstance*>(std::get<Obj*>(peek(0)._value));
                ObjString* name = static_cast<ObjString*>(std::get<Obj*>(frame->function->chunk.constants[READ_SHORT()]._value));
                auto it_field = instance->fields.find(name->chars);
                if (it_field != instance->fields.end()) {
                    pop();
                    push(it_field->second);
                    break;
                }
                auto it_method = instance->klass->methods.find(name->chars);
                if (it_method != instance->klass->methods.end()) {
                    ObjClosure* method = it_method->second;
                    ObjBoundMethod* bound = new_bound_method(this, peek(0), method);
                    pop();
                    push(bound);
                    break;
                }
                pop();
                push({});
                break;
            }
            case OP_SET_PROPERTY: {
                if (!is_obj_type(peek(1), OBJ_INSTANCE)) {
                    if (!this->soft_mode) {
                        std::cerr << "Runtime Error: Only instances have fields." << std::endl;
                        return false;
                    }
                    pop(); pop(); push({}); break;
                }
                ObjInstance* instance = static_cast<ObjInstance*>(std::get<Obj*>(peek(1)._value));
                ObjString* name = static_cast<ObjString*>(std::get<Obj*>(frame->function->chunk.constants[READ_SHORT()]._value));
                instance->fields[name->chars] = peek(0);
                SapphireValue value = pop();
                pop();
                push(value);
                break;
            }
            case OP_BUILD_ARRAY: {
                uint8_t element_count = READ_BYTE();
                auto array_obj = std::make_shared<SapphireArray>();
                for (int i = 0; i < element_count; i++) {
                    array_obj->elements.push_back(peek(element_count - 1 - i));
                }
                for (int i = 0; i < element_count; i++) { pop(); }
                push(array_obj);
                break;
            }
            case OP_GET_SUBSCRIPT: {
                SapphireValue index_val = pop();
                SapphireValue array_val = pop();
                if (!std::holds_alternative<std::shared_ptr<SapphireArray>>(array_val._value)) {
                    if (!this->soft_mode) {
                         std::cerr << "Runtime Error: Subscript target must be an array." << std::endl;
                         return false;
                    }
                    push({}); break;
                }
                auto array_obj = std::get<std::shared_ptr<SapphireArray>>(array_val._value);
                if (!std::holds_alternative<double>(index_val._value)) {
                    if (!this->soft_mode) {
                        std::cerr << "Runtime Error: Array index must be a number." << std::endl;
                        return false;
                    }
                    push({}); break;
                }
                int index = static_cast<int>(std::get<double>(index_val._value));
                if (index < 0 || index >= array_obj->elements.size()) {
                     if (!this->soft_mode) {
                        std::cerr << "Runtime Error: Array index out of bounds." << std::endl;
                        return false;
                    }
                    push({}); break;
                }
                push(array_obj->elements[index]);
                break;
            }
            case OP_SET_SUBSCRIPT: {
                SapphireValue value = pop();
                SapphireValue index_val = pop();
                SapphireValue array_val = pop();
                if (!std::holds_alternative<std::shared_ptr<SapphireArray>>(array_val._value)) {
                    if (!this->soft_mode) {
                        std::cerr << "Runtime Error: Subscript target must be an array." << std::endl;
                        return false;
                    }
                    push(value); break;
                }
                auto array_obj = std::get<std::shared_ptr<SapphireArray>>(array_val._value);
                if (!std::holds_alternative<double>(index_val._value)) {
                    if(!this->soft_mode){
                        std::cerr << "Runtime Error: Array index must be a number." << std::endl;
                        return false;
                    }
                    push(value); break;
                }
                int index = static_cast<int>(std::get<double>(index_val._value));
                if (index < 0 || index >= array_obj->elements.size()) {
                    if(!this->soft_mode) {
                        std::cerr << "Runtime Error: Array index out of bounds for assignment." << std::endl;
                        return false;
                    }
                    push(value); break;
                }
                array_obj->elements[index] = value;
                push(value);
                break;
            }
            case OP_EQUAL: {
                SapphireValue b = pop();
                SapphireValue a = pop();
                push(a._value == b._value);
                break;
            }
            case OP_GREATER:  BINARY_OP(bool, >); break;
            case OP_LESS:     BINARY_OP(bool, <); break;
            case OP_ADD: {
                if (is_obj_type(peek(0), OBJ_STRING) && is_obj_type(peek(1), OBJ_STRING)) {
                    ObjString* b = static_cast<ObjString*>(std::get<Obj*>(pop()._value));
                    ObjString* a = static_cast<ObjString*>(std::get<Obj*>(pop()._value));
                    push(new_string(this, a->chars + b->chars));
                } else if (std::holds_alternative<double>(peek(0)._value) && std::holds_alternative<double>(peek(1)._value)) {
                    double b = std::get<double>(pop()._value);
                    double a = std::get<double>(pop()._value);
                    push(a + b);
                } else {
                    if (!this->soft_mode) {
                        std::cerr << "Runtime Error: Operands for '+' must be two numbers or two strings." << std::endl;
                        return false;
                    }
                    pop(); pop(); push({});
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(double, -); break;
            case OP_MULTIPLY: BINARY_OP(double, *); break;
            case OP_DIVIDE:   BINARY_OP(double, /); break;
            case OP_NOT:      push(is_falsey(pop())); break;
            case OP_NEGATE:
                if (!std::holds_alternative<double>(peek(0)._value)) {
                    if (!this->soft_mode) {
                        std::cerr << "Runtime Error: Operand for '-' must be a number." << std::endl;
                        return false;
                    }
                    push({});
                } else {
                    push(-std::get<double>(pop()._value));
                }
                break;
            case OP_PRINT: {
                print_value(pop());
                std::cout << std::endl;
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (is_falsey(peek(0))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }

            case OP_CALL: {
                int arg_count = READ_BYTE();
                SapphireValue callee = peek(arg_count);

                if (!call_value(callee, arg_count)) {
                    if (!this->soft_mode) return false;
                }

                frame = &frames[frame_count - 1];
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = static_cast<ObjFunction*>(std::get<Obj*>(frame->function->chunk.constants[READ_SHORT()]._value));
                ObjClosure* closure = new_closure(this, function);
                push(closure);
                break;
            }
            case OP_RETURN: {
                SapphireValue result = pop();
                frame_count--;

                if (frame_count == 0) {
                    return true;
                }
                stack_top = frame->slots;
                push(result);
                frame = &frames[frame_count - 1];
                break;
            }
            case OP_IMPORT: {
                ObjString* module_name_obj = static_cast<ObjString*>(std::get<Obj*>(frame->function->chunk.constants[READ_SHORT()]._value));

                std::string source = find_and_load_module(module_name_obj->chars);
                if (source.empty()) {
                    if (!this->soft_mode) {
                        std::cerr << "Runtime Error: Could not open module '" << module_name_obj->chars << "'." << std::endl;
                        return false;
                    }
                    push({}); break;
                }

                ObjFunction* module_function = compile(this, source);
                if (module_function == nullptr) {
                     if (!this->soft_mode) {
                        std::cerr << "Runtime Error: Compilation of module '" << module_name_obj->chars << "' failed." << std::endl;
                        return false;
                    }
                    push({}); break;
                }

                push(new_closure(this, module_function));
                call_value(peek(0), 0);
                frame = &frames[frame_count - 1];
                break;
            }
            default:
                std::cerr << "Runtime Error : Unknown opcode. " << (int)instruction << std::endl;
                return false;
        }
    }
}   // quem escreveu essa função não sabe usar indentação.. ah pera, sou eu
// eu tive que reescrever tudo dentro do switch porque a formatação estava toda errada...

bool VM::run_function(ObjFunction* function) {
    // std::cout << "  [VM DEBUG] Entrando em run_function..." << std::endl;
    if (function == nullptr) return false;
    resetStack();
    push(function);
    if (!call(function, 0)) {
        return false;
    }
    bool result = run();
    // std::cout << "  [VM DEBUG] Saindo de run_function." << std::endl;
    return result;
}

SapphireValue VM::interpret(const std::string& source) {
    ObjFunction* function = compile(this, source);
    if (function == nullptr) return {};

    resetStack();
    push(function);

    if (!call(function, 0)) return {};

    bool result = run();

    if (result && stack_top > stack) {
        return pop();
    }
    return {};
}

void VM::resetStack() {
    stack_top = stack;
    frame_count = 0;
    ui_state.activeMenu = "";
}

SapphireValue VM::getGlobal(const std::string& name) {
    auto it = globals.find(name);
    if (it != globals.end()) {
        return it->second;
    }
    return {};
}

// --- Funções do Coletor de Lixo ---
void VM::mark_object(Obj* object) {
    if (object == nullptr || object->is_marked) return;

    object->is_marked = true;
    gray_stack.push_back(object);
}

void VM::mark_value(SapphireValue value) {
    if (std::holds_alternative<Obj*>(value._value)) {
        mark_object(std::get<Obj*>(value._value));
    } else if (std::holds_alternative<std::shared_ptr<SapphireArray>>(value._value)) {
        auto array = std::get<std::shared_ptr<SapphireArray>>(value._value);
        for (SapphireValue& val : array->elements) {
            mark_value(val);
        }
    }
}

void VM::blacken_object(Obj* object) {
    switch (object->type) {
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            mark_object((Obj*)closure->function);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            mark_object((Obj*)function->name);
            for (SapphireValue& constant : function->chunk.constants) {
                mark_value(constant);
            }
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            mark_object((Obj*)instance->klass);
            for (auto const& [key, val] : instance->fields) {
                mark_value(val);
            }
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            mark_object((Obj*)klass->name);
            for (auto const& [key, val] : klass->methods) {
                mark_object((Obj*)val);
            }
            break;
        }
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            mark_value(bound->receiver);
            mark_object((Obj*)bound->method);
            break;
        }
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}

void VM::trace_references() {
    while (!gray_stack.empty()) {
        Obj* object = gray_stack.back();
        gray_stack.pop_back();
        blacken_object(object);
    }
}

void VM::mark_roots() {
    for (SapphireValue* slot = stack; slot < stack_top; slot++) {
        mark_value(*slot);
    }
    for (int i = 0; i < frame_count; i++) {
        mark_object((Obj*)frames[i].function);
    }
    for (auto const& [key, val] : globals) {
        mark_value(val);
    }
}

void VM::sweep() {
    Obj* previous = nullptr;
    Obj* object = objects;
    while (object != nullptr) {
        if (object->is_marked) {
            object->is_marked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous != nullptr) {
                previous->next = object;
            } else {
                objects = object;
            }
        }
    }
}

void VM::collect_garbage() {
    mark_roots();
    trace_references();
    sweep();
}

bool VM::call_and_run(ObjFunction* function) {
    if (function == nullptr) return false;

    SapphireValue* starting_stack = stack_top;

    push(function);
    if (!call(function, 0)) {
        stack_top = starting_stack;
        return false;
    }

    bool result = run();

    stack_top = starting_stack;
    frame_count = 0;

    return result;
}
