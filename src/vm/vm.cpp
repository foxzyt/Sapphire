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
#include "preprocessor/preprocessor.h"
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
#include "engine.h"
#include "vec2d.h"
#include "vec3d.h"
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

static std::random_device rd;
static std::mt19937 gen(rd());

thread_local VM* g_current_vm = nullptr;

static std::mutex thread_mutex;
static std::map<int, std::thread> active_threads;
static int next_thread_id = 1;

static SapphireValue native_spawn(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return SapphireValue();
    std::string script_path = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    
    int tid;
    {
        std::lock_guard<std::mutex> lock(thread_mutex);
        tid = next_thread_id++;
        active_threads[tid] = std::thread([script_path]() {
            try {
                std::string src = load_file_as_string(script_path);
                ScriptConfig config;
                VM thread_vm(config, false, nullptr);
                thread_vm.interpret(src);
            } catch (...) {}
        });
    }
    return SapphireValue((double)tid);
}

static SapphireValue native_join(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !std::holds_alternative<double>(args[0]._value)) return SapphireValue(false);
    int tid = (int)std::get<double>(args[0]._value);
    
    std::thread t;
    {
        std::lock_guard<std::mutex> lock(thread_mutex);
        if (active_threads.count(tid)) {
            t = std::move(active_threads[tid]);
            active_threads.erase(tid);
        }
    }
    
    if (t.joinable()) {
        t.join();
        return SapphireValue(true);
    }
    return SapphireValue(false);
}

static SapphireValue native_system_core_count(int arg_count, SapphireValue* args) {
    unsigned int cores = std::thread::hardware_concurrency();
    if (cores == 0) cores = 4;
    return SapphireValue((double)cores);
}

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

static UIStyle resolve_style(const std::string& id, const std::string& styleName = "") {
    UIStyle base = g_current_vm->ui_state.defaultStyle;
    if (!styleName.empty()) {
        auto sIt = g_current_vm->ui_state.stylesheets.find(styleName);
        if (sIt != g_current_vm->ui_state.stylesheets.end()) {
            base = sIt->second;
        }
    } else if (g_current_vm->ui_state.activeStyle) {
        base = *g_current_vm->ui_state.activeStyle;
    }
    
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

static SapphireValue native_io_open_file_dialog(int arg_count, SapphireValue* args) {
#ifdef _WIN32
    char filename[MAX_PATH];
    filename[0] = '\0';

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "All Files\0*.*\0Sapphire Scripts (*.sp)\0*.sp\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = "sp";

    if (GetOpenFileNameA(&ofn)) {
        return new_string(g_current_vm, std::string(filename));
    }
#endif
    return new_string(g_current_vm, "");
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

static std::string valueToStringC(const SapphireValue& val) {
    std::stringstream ss;
    if (std::holds_alternative<std::monostate>(val._value)) {
        ss << "nil";
    } else if (std::holds_alternative<bool>(val._value)) {
        ss << (std::get<bool>(val._value) ? "true" : "false");
    } else if (std::holds_alternative<double>(val._value)) {
        double d = std::get<double>(val._value);
        double int_part;
        if (modf(d, &int_part) == 0.0) ss << static_cast<long long>(d);
        else ss << d;
    } else if (std::holds_alternative<Obj*>(val._value)) {
        Obj* obj = std::get<Obj*>(val._value);
        if (obj->type == OBJ_STRING) ss << static_cast<ObjString*>(obj)->chars;
        else ss << "[object]";
    } else {
        ss << "[unknown]";
    }
    return ss.str();
}

static double valueToDoubleC(const SapphireValue& val) {
    if (val._value.index() == 2) {
        return std::get<double>(val._value);
    } else if (val._value.index() == 3) {
        ObjString* str = static_cast<ObjString*>(std::get<Obj*>(val._value));
        try { return std::stod(str->chars); } catch (...) { return 0.0; }
    }
    return 0.0;
}

static SapphireValue native_value_to_string(int arg_count, SapphireValue* args) {
    if (arg_count != 1) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: valueToString() expects 1 argument." << std::endl;
        }
        return new_string(g_current_vm, "");
    }
    return new_string(g_current_vm, valueToStringC(args[0]));
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







static SapphireValue create_declarative_node(const std::string& type, int arg_count, SapphireValue* args) {
    ObjInstance* node = new_instance(g_current_vm, g_current_vm->ui_component_class);
    node->fields["type"] = new_string(g_current_vm, type);
    
    // Check if the first argument is positional (not named), just in case backward compatibility is needed
    if (arg_count > 0 && !is_obj_type(args[0], OBJ_NAMED_ARG)) {
        if (type == "Text" || type == "Display") node->fields["text"] = new_string(g_current_vm, valueToStringC(args[0]));
        else node->fields["label"] = new_string(g_current_vm, valueToStringC(args[0]));
    }
    
    // Parse all named arguments (overriding positional if specified)
    for (int i = 0; i < arg_count; i++) {
        if (is_obj_type(args[i], OBJ_NAMED_ARG)) {
            ObjNamedArg* narg = static_cast<ObjNamedArg*>(std::get<Obj*>(args[i]._value));
            node->fields[narg->name->chars] = narg->value;
        }
    }
    return node;
}

static SapphireValue native_ui_button(int arg_count, SapphireValue* args) { return create_declarative_node("Button", arg_count, args); }
static SapphireValue native_ui_text(int arg_count, SapphireValue* args) { return create_declarative_node("Text", arg_count, args); }
static SapphireValue native_ui_display(int arg_count, SapphireValue* args) { return create_declarative_node("Display", arg_count, args); }
static SapphireValue native_ui_flex(int arg_count, SapphireValue* args) { return create_declarative_node("Container", arg_count, args); }
static SapphireValue native_ui_checkbox(int arg_count, SapphireValue* args) { return create_declarative_node("Checkbox", arg_count, args); }
static SapphireValue native_ui_slider(int arg_count, SapphireValue* args) { return create_declarative_node("Slider", arg_count, args); }
static SapphireValue native_ui_input(int arg_count, SapphireValue* args) { return create_declarative_node("Input", arg_count, args); }
static SapphireValue native_ui_separator(int arg_count, SapphireValue* args) { return create_declarative_node("Separator", arg_count, args); }
static SapphireValue native_ui_menu(int arg_count, SapphireValue* args) { return create_declarative_node("Menu", arg_count, args); }
static SapphireValue native_ui_menuitem(int arg_count, SapphireValue* args) { return create_declarative_node("MenuItem", arg_count, args); }

static SapphireValue native_ui_style(int arg_count, SapphireValue* args) {
    if (arg_count == 0) return {};
    
    if (arg_count == 1 && is_obj_type(args[0], OBJ_STRING)) {
        std::string path = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "[SAPPHIRE ERROR] Could not open style file: " << path << std::endl;
            return {};
        }
        nlohmann::json j;
        file >> j;
        
        for (auto& [styleName, styleData] : j.items()) {
            UIStyle style = g_current_vm->ui_state.defaultStyle;
            if (styleData.contains("bgColor")) style.bgColor = hexToColor(styleData["bgColor"].get<std::string>());
            if (styleData.contains("textColor")) style.textColor = hexToColor(styleData["textColor"].get<std::string>());
            if (styleData.contains("accentColor")) style.accentColor = hexToColor(styleData["accentColor"].get<std::string>());
            if (styleData.contains("hoverColor")) style.hoverColor = hexToColor(styleData["hoverColor"].get<std::string>());
            if (styleData.contains("borderColor")) style.borderColor = hexToColor(styleData["borderColor"].get<std::string>());
            if (styleData.contains("borderThickness")) style.borderThickness = styleData["borderThickness"].get<float>();
            if (styleData.contains("borderRadius")) style.borderRadius = styleData["borderRadius"].get<float>();
            if (styleData.contains("padding")) style.padding = styleData["padding"].get<float>();
            if (styleData.contains("fontAlias")) style.fontAlias = styleData["fontAlias"].get<std::string>();
            if (styleData.contains("fontSize")) style.fontSize = styleData["fontSize"].get<unsigned int>();
            if (styleData.contains("width")) style.width = styleData["width"].get<float>();
            if (styleData.contains("height")) style.height = styleData["height"].get<float>();
            if (styleData.contains("margin")) style.margin = styleData["margin"].get<float>();
            if (styleData.contains("thickness")) style.thickness = styleData["thickness"].get<float>();
            
            g_current_vm->ui_state.stylesheets[styleName] = style;
        }
    } else if (arg_count > 0) {
        std::string styleName = "";
        if (!is_obj_type(args[0], OBJ_NAMED_ARG) && is_obj_type(args[0], OBJ_STRING)) {
            styleName = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
        }
        UIStyle style = g_current_vm->ui_state.defaultStyle;
        for (int i = 0; i < arg_count; i++) {
            if (is_obj_type(args[i], OBJ_NAMED_ARG)) {
                ObjNamedArg* narg = static_cast<ObjNamedArg*>(std::get<Obj*>(args[i]._value));
                std::string key = narg->name->chars;
                if (key == "name" && is_obj_type(narg->value, OBJ_STRING)) styleName = static_cast<ObjString*>(std::get<Obj*>(narg->value._value))->chars;
                else if (key == "bgColor" && is_obj_type(narg->value, OBJ_STRING)) style.bgColor = hexToColor(static_cast<ObjString*>(std::get<Obj*>(narg->value._value))->chars);
                else if (key == "textColor" && is_obj_type(narg->value, OBJ_STRING)) style.textColor = hexToColor(static_cast<ObjString*>(std::get<Obj*>(narg->value._value))->chars);
                else if (key == "accentColor" && is_obj_type(narg->value, OBJ_STRING)) style.accentColor = hexToColor(static_cast<ObjString*>(std::get<Obj*>(narg->value._value))->chars);
                else if (key == "hoverColor" && is_obj_type(narg->value, OBJ_STRING)) style.hoverColor = hexToColor(static_cast<ObjString*>(std::get<Obj*>(narg->value._value))->chars);
                else if (key == "borderColor" && is_obj_type(narg->value, OBJ_STRING)) style.borderColor = hexToColor(static_cast<ObjString*>(std::get<Obj*>(narg->value._value))->chars);
                else if (key == "borderThickness" && std::holds_alternative<double>(narg->value._value)) style.borderThickness = (float)std::get<double>(narg->value._value);
                else if (key == "borderRadius" && std::holds_alternative<double>(narg->value._value)) style.borderRadius = (float)std::get<double>(narg->value._value);
                else if (key == "padding" && std::holds_alternative<double>(narg->value._value)) style.padding = (float)std::get<double>(narg->value._value);
                else if (key == "fontAlias" && is_obj_type(narg->value, OBJ_STRING)) style.fontAlias = static_cast<ObjString*>(std::get<Obj*>(narg->value._value))->chars;
                else if (key == "fontSize" && std::holds_alternative<double>(narg->value._value)) style.fontSize = (unsigned int)std::get<double>(narg->value._value);
                else if (key == "width" && std::holds_alternative<double>(narg->value._value)) style.width = (float)std::get<double>(narg->value._value);
                else if (key == "height" && std::holds_alternative<double>(narg->value._value)) style.height = (float)std::get<double>(narg->value._value);
                else if (key == "margin" && std::holds_alternative<double>(narg->value._value)) style.margin = (float)std::get<double>(narg->value._value);
                else if (key == "thickness" && std::holds_alternative<double>(narg->value._value)) style.thickness = (float)std::get<double>(narg->value._value);
            }
        }
        if (!styleName.empty()) {
            g_current_vm->ui_state.stylesheets[styleName] = style;
        }
    }
    return {};
}

static void compute_sizes(std::shared_ptr<UINode> node) {
    if (!node) return;
    
    for (auto& child : node->children) {
        compute_sizes(child);
    }
    
    if (node->type == UINodeType::Container) {
        float maxChildWidth = 0.0f;
        float maxChildHeight = 0.0f;
        float sumWidth = 0.0f;
        float sumHeight = 0.0f;
        
        for (auto& child : node->children) {
            if (child->width > maxChildWidth) maxChildWidth = child->width;
            if (child->height > maxChildHeight) maxChildHeight = child->height;
            sumWidth += child->width;
            sumHeight += child->height;
        }
        
        float totalGap = node->children.empty() ? 0 : (node->children.size() - 1) * node->gap;
        
        if (node->width <= 0) {
            if (node->direction == "row") node->width = sumWidth + totalGap;
            else node->width = maxChildWidth;
        }
        if (node->height <= 0) {
            if (node->direction == "column") node->height = sumHeight + totalGap;
            else node->height = maxChildHeight;
        }
    }
}

static void place_children(std::shared_ptr<UINode> node, float startX, float startY) {
    if (!node) return;
    node->x = startX;
    node->y = startY;
    
    if (node->type == UINodeType::Container) {
        float currentX = startX;
        float currentY = startY;
        
        float freeSpaceX = node->width;
        float freeSpaceY = node->height;
        float totalGap = node->children.empty() ? 0 : (node->children.size() - 1) * node->gap;
        
        for (auto& child : node->children) {
            if (node->direction == "row") freeSpaceX -= child->width;
            else freeSpaceY -= child->height;
        }
        if (node->direction == "row") freeSpaceX -= totalGap;
        else freeSpaceY -= totalGap;
        
        float gapExtraX = 0;
        float gapExtraY = 0;
        
        if (node->direction == "row") {
            if (node->justify == "center") currentX += freeSpaceX / 2.0f;
            else if (node->justify == "flex-end") currentX += freeSpaceX;
            else if (node->justify == "space-between" && node->children.size() > 1) gapExtraX = freeSpaceX / (node->children.size() - 1);
        } else {
            if (node->justify == "center") currentY += freeSpaceY / 2.0f;
            else if (node->justify == "flex-end") currentY += freeSpaceY;
            else if (node->justify == "space-between" && node->children.size() > 1) gapExtraY = freeSpaceY / (node->children.size() - 1);
        }
        
        for (auto& child : node->children) {
            float childX = currentX;
            float childY = currentY;
            
            if (node->direction == "row") {
                if (node->align == "stretch") {
                    child->height = node->height;
                } else if (node->align == "center") {
                    childY = startY + (node->height / 2.0f) - (child->height / 2.0f);
                } else if (node->align == "flex-end") {
                    childY = startY + node->height - child->height;
                }
            } else {
                if (node->align == "stretch") {
                    child->width = node->width;
                } else if (node->align == "center") {
                    childX = startX + (node->width / 2.0f) - (child->width / 2.0f);
                } else if (node->align == "flex-end") {
                    childX = startX + node->width - child->width;
                }
            }
            
            place_children(child, childX, childY);
            
            if (node->direction == "row") {
                currentX += child->width + node->gap + gapExtraX;
            } else {
                currentY += child->height + node->gap + gapExtraY;
            }
        }
    } else if (node->type == UINodeType::Menu) {
        float currentY = startY + node->height;
        for (auto& child : node->children) {
            place_children(child, startX, currentY);
            currentY += child->height;
        }
    }
}

static void hit_test_tree(std::shared_ptr<UINode> node, sf::Vector2i m, bool mouseJustClicked) {
    if (!node) return;
    
    bool hovered = sf::FloatRect({node->x, node->y}, {node->width, node->height}).contains(sf::Vector2f((float)m.x, (float)m.y));
    g_current_vm->ui_state.hoverState[node->id] = hovered;
    
    if (hovered && mouseJustClicked) {
        if (node->type == UINodeType::Input) {
            g_current_vm->ui_state.focusedInputId = node->id;
            // Calculate cursor position from click X
            const std::string& text = g_current_vm->ui_state.inputTexts[node->id];
            std::string finalAlias = "default";
            // Find best cursor position by measuring text widths
            UIStyle s = resolve_style(node->id, node->styleName);
            if (!s.fontAlias.empty() && g_current_vm->ui_state.fontStack.count(s.fontAlias))
                finalAlias = s.fontAlias;
            if (!g_current_vm->ui_state.fontStack.count(finalAlias)) finalAlias = "default";
            if (g_current_vm->ui_state.fontStack.count(finalAlias)) {
                auto& font = g_current_vm->ui_state.fontStack[finalAlias];
                float clickX = g_current_vm->ui_state.mouseClickPos.x - node->x - 10.0f;
                // Measure scroll offset same way renderer does
                sf::Text fullTxt(font, text, s.fontSize > 0 ? s.fontSize : 18);
                float textWidth = fullTxt.getLocalBounds().size.x;
                float maxVisible = (node->width > 0 ? node->width : 250.f) - 20.0f;
                float scrollOffset = (textWidth > maxVisible) ? textWidth - maxVisible : 0.0f;
                clickX += scrollOffset;
                size_t bestPos = 0;
                float bestDist = std::abs(clickX);
                for (size_t i = 1; i <= text.size(); i++) {
                    sf::Text t(font, text.substr(0, i), s.fontSize > 0 ? s.fontSize : 18);
                    float cx = t.getLocalBounds().size.x;
                    float dist = std::abs(clickX - cx);
                    if (dist < bestDist) { bestDist = dist; bestPos = i; }
                }
                g_current_vm->ui_state.cursorPositions[node->id] = bestPos;
            } else {
                g_current_vm->ui_state.cursorPositions[node->id] = text.length();
            }
        } else if (node->type == UINodeType::Button || node->type == UINodeType::Checkbox || node->type == UINodeType::Slider || node->type == UINodeType::MenuItem) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - g_current_vm->ui_state.lastClickTime);
            if (elapsed.count() > g_current_vm->ui_state.debounceTime) {
                g_current_vm->ui_state.lastClickTime = now;
                g_current_vm->ui_state.clickState[node->id] = true;
                g_current_vm->ui_state.focusedInputId = "";
                g_current_vm->ui_state.activeMenu = ""; // Close menu on option click
            }
        } else if (node->type == UINodeType::Menu) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - g_current_vm->ui_state.lastClickTime);
            if (elapsed.count() > g_current_vm->ui_state.debounceTime) {
                g_current_vm->ui_state.lastClickTime = now;
                g_current_vm->ui_state.clickState[node->id] = true;
                if (g_current_vm->ui_state.activeMenu == node->id) g_current_vm->ui_state.activeMenu = "";
                else g_current_vm->ui_state.activeMenu = node->id;
                g_current_vm->ui_state.focusedInputId = "";
            }
        }
    }
    
    if (node->type != UINodeType::Menu || g_current_vm->ui_state.activeMenu == node->id) {
        for (auto& child : node->children) {
            hit_test_tree(child, m, mouseJustClicked);
        }
    }
}

static std::vector<std::shared_ptr<UINode>> deferred_render_nodes;

static void render_ui_tree(std::shared_ptr<UINode> node) {
    if (!node) return;
    
    if (node->type == UINodeType::Menu && g_current_vm->ui_state.activeMenu == node->id) {
        for (auto& child : node->children) {
            deferred_render_nodes.push_back(child);
        }
    }
    
    UIStyle s = resolve_style(node->id, node->styleName);
    if (node->fontSize > 0) s.fontSize = node->fontSize;
    if (!node->fontAlias.empty()) s.fontAlias = node->fontAlias;
    
    bool hovered = g_current_vm->ui_state.hoverState[node->id];
    
    if (node->type == UINodeType::Container) {
        if (s.bgColor.a > 0 || s.borderThickness > 0) {
            draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {node->width, node->height}, 
                              s.borderRadius, s.bgColor, s.borderColor, s.borderThickness);
        }
    }
    else if (node->type == UINodeType::Button || node->type == UINodeType::Menu || node->type == UINodeType::MenuItem) {
        draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {node->width, node->height}, 
                          s.borderRadius, hovered ? s.hoverColor : s.bgColor, s.accentColor, s.borderThickness);
        
        if (!node->label.empty()) {
            float textX = node->x + s.padding;
            std::string finalAlias = s.fontAlias;
            if (g_current_vm->ui_state.fontStack.find(finalAlias) == g_current_vm->ui_state.fontStack.end()) finalAlias = "default";
            
            sf::Text dummyText(g_current_vm->ui_state.fontStack[finalAlias], node->label, s.fontSize);
            float tw = dummyText.getLocalBounds().size.x;
            
            if (node->align == "center") {
                textX = node->x + (node->width / 2.0f) - (tw / 2.0f);
            } else if (node->align == "right") {
                textX = node->x + node->width - tw - s.padding;
            }
            
            sf::Font& font = g_current_vm->ui_state.fontStack[finalAlias];
            sf::Text txt(font, node->label, s.fontSize);
            txt.setFillColor(s.textColor);
            txt.setPosition({textX, node->y + (node->height / 2.0f) - (s.fontSize / 2.0f) - 2.0f});
            g_current_vm->sfml_window->draw(txt);
        }
    }

    else if (node->type == UINodeType::Display) {
        if (s.bgColor.a > 0 || s.borderThickness > 0) {
            draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {node->width, node->height}, 
                              s.borderRadius, s.bgColor, s.borderColor, s.borderThickness);
        }
        
        std::string finalAlias = s.fontAlias;
        if (g_current_vm->ui_state.fontStack.find(finalAlias) == g_current_vm->ui_state.fontStack.end()) finalAlias = "default";
        
        sf::Text dummyText(g_current_vm->ui_state.fontStack[finalAlias], node->label, s.fontSize);
        float tw = dummyText.getLocalBounds().size.x;
        
        float textX = node->x + s.padding;
        float textY = node->y + s.padding;
        
        if (node->align == "center") {
            textX = node->x + (node->width / 2.0f) - (tw / 2.0f);
        } else if (node->align == "right" || node->align == "flex-end") {
            textX = node->x + node->width - tw - s.padding;
        }
        
        if (node->justify == "center") {
            textY = node->y + (node->height / 2.0f) - (s.fontSize / 2.0f);
        } else if (node->justify == "bottom" || node->justify == "flex-end") {
            textY = node->y + node->height - s.fontSize - s.padding;
        }

        if (node->shadow) {
            sapphire_render_text(*g_current_vm->sfml_window, node->label,
                                 {textX + 2.0f, textY + 2.0f},
                                 sf::Color(0, 0, 0, 150), s.fontAlias, s.fontSize);
        }
        
        sapphire_render_text(*g_current_vm->sfml_window, node->label,
                             {textX, textY},
                             s.textColor, s.fontAlias, s.fontSize);
    }
    else if (node->type == UINodeType::Text) {
        sapphire_render_text(*g_current_vm->sfml_window, node->label,
                             {node->x, node->y},
                             s.textColor, s.fontAlias, s.fontSize);
    }
    else if (node->type == UINodeType::Checkbox) {
        float size = node->width > 0 ? node->width : 20.0f;
        sf::Vector2f pos(node->x, node->y);
        draw_rounded_rect(*g_current_vm->sfml_window, pos, {size, size}, s.borderRadius, hovered ? s.hoverColor : s.bgColor, s.accentColor, s.borderThickness);
        if (node->checked) {
            float offset = 5.0f;
            draw_rounded_rect(*g_current_vm->sfml_window, {pos.x + offset, pos.y + offset}, {size - 10, size - 10}, s.borderRadius * 0.5f, s.accentColor, sf::Color::Transparent, 0);
        }
        if (!node->label.empty()) {
            sapphire_render_text(*g_current_vm->sfml_window, node->label, {pos.x + size + 10.0f, pos.y}, s.textColor, s.fontAlias, s.fontSize);
        }
    }
    else if (node->type == UINodeType::Slider) {
        float width = node->width > 0 ? node->width : 200.0f;
        sf::RectangleShape bar({width, 6.0f});
        bar.setPosition({node->x, node->y + 10.0f});
        bar.setFillColor(s.borderColor);
        g_current_vm->sfml_window->draw(bar);

        float valPos = ((node->value - node->min) / (node->max - node->min)) * width;
        if (std::isnan(valPos) || std::isinf(valPos)) valPos = 0.0f;
        
        sf::CircleShape handle(8.0f);
        handle.setOrigin({8.0f, 8.0f});
        handle.setPosition({node->x + valPos, node->y + 13.0f});
        handle.setFillColor(s.accentColor);
        
        if (hovered && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
            sf::Vector2i m = sf::Mouse::getPosition(*g_current_vm->sfml_window);
            float newPos = std::clamp((float)m.x - node->x, 0.0f, width);
            node->value = node->min + (newPos / width) * (node->max - node->min);
        }
        
        g_current_vm->sfml_window->draw(handle);
    }
    else if (node->type == UINodeType::Input) {
        float width = node->width > 0 ? node->width : 250.0f;
        float height = node->height > 0 ? node->height : 35.0f;
        sf::Vector2f pos(node->x, node->y);
        
        bool isFocused = (g_current_vm->ui_state.focusedInputId == node->id);
        
        // Draw border: accent color when focused, normal when not
        sf::Color borderCol = isFocused ? g_current_vm->ui_state.stylesheets.count(node->styleName) ?
            g_current_vm->ui_state.stylesheets[node->styleName].accentColor : sf::Color(0, 122, 204)
            : s.accentColor;
        draw_rounded_rect(*g_current_vm->sfml_window, pos, {width, height}, s.borderRadius, s.bgColor, borderCol, isFocused ? 2.0f : s.borderThickness);

        sf::Vector2u winSize = g_current_vm->sfml_window->getSize();
        if (winSize.x > 0 && winSize.y > 0 && width > 0 && height > 0) {
            sf::View oldView = g_current_vm->sfml_window->getView();
            sf::FloatRect viewportRect({pos.x / (float)winSize.x, pos.y / (float)winSize.y}, {width / (float)winSize.x, height / (float)winSize.y});
            sf::View inputView(sf::FloatRect({0.f, 0.f}, {width, height}));
            inputView.setViewport(viewportRect);
            g_current_vm->sfml_window->setView(inputView);
            
            std::string displayText = node->label;
            size_t cursorPos = g_current_vm->ui_state.cursorPositions.count(node->id)
                               ? g_current_vm->ui_state.cursorPositions[node->id] : 0;
            if (cursorPos > displayText.length()) {
                cursorPos = displayText.length();
                g_current_vm->ui_state.cursorPositions[node->id] = cursorPos;
            }
            
            std::string finalAlias = s.fontAlias;
            if (g_current_vm->ui_state.fontStack.find(finalAlias) == g_current_vm->ui_state.fontStack.end()) finalAlias = "default";
            auto& font = g_current_vm->ui_state.fontStack[finalAlias];
            unsigned int fsize = s.fontSize > 0 ? s.fontSize : 18;

            // Compute scroll offset based on cursor position
            sf::Text cursorMeasure(font, displayText.substr(0, cursorPos), fsize);
            float cursorX = cursorMeasure.getLocalBounds().size.x;
            sf::Text fullMeasure(font, displayText, fsize);
            float textWidth = fullMeasure.getLocalBounds().size.x;
            float maxVisible = width - 20.0f;

            // Keep cursor in view
            float scrollOffset = 0.0f;
            if (cursorX > maxVisible) scrollOffset = cursorX - maxVisible;
            else if (textWidth > maxVisible) scrollOffset = 0.0f; // let text extend to left
            
            sf::Text txt(font, displayText, fsize);
            txt.setFillColor(s.textColor);
            txt.setPosition({10.0f - scrollOffset, (height / 2.0f) - (fsize / 2.0f)});
            g_current_vm->sfml_window->draw(txt);
            
            // Draw cursor only when focused
            if (isFocused) {
                float cx = 10.0f + cursorX - scrollOffset;
                sf::RectangleShape cursor({2.0f, (float)fsize + 4.0f});
                cursor.setPosition({cx, (height / 2.0f) - (fsize / 2.0f) - 2.0f});
                cursor.setFillColor(s.textColor);
                g_current_vm->sfml_window->draw(cursor);
            }
            
            g_current_vm->sfml_window->setView(oldView);
        }
    }
    else if (node->type == UINodeType::Separator) {
        sf::RectangleShape line({node->width, node->thickness});
        line.setPosition({node->x, node->y + node->margin});
        line.setFillColor(s.borderColor);
        g_current_vm->sfml_window->draw(line);
    }
    
    if (node->type != UINodeType::Menu) {
        for (auto& child : node->children) {
            render_ui_tree(child);
        }
    }
}

static std::shared_ptr<UINode> build_ui_tree(ObjInstance* nodeDict, int& counter) {
    if (!nodeDict) return nullptr;
    if (nodeDict->fields.find("type") == nodeDict->fields.end()) return nullptr;
    
    auto typeVal = nodeDict->fields["type"];
    if (!is_obj_type(typeVal, OBJ_STRING)) return nullptr;
    std::string typeStr = static_cast<ObjString*>(std::get<Obj*>(typeVal._value))->chars;
    
    UINodeType type = UINodeType::Container;
    if (typeStr == "Button") type = UINodeType::Button;
    else if (typeStr == "Text") type = UINodeType::Text;
    else if (typeStr == "Display") type = UINodeType::Display;
    else if (typeStr == "Checkbox") type = UINodeType::Checkbox;
    else if (typeStr == "Slider") type = UINodeType::Slider;
    else if (typeStr == "Input") type = UINodeType::Input;
    else if (typeStr == "Separator") type = UINodeType::Separator;
    else if (typeStr == "Menu") type = UINodeType::Menu;
    else if (typeStr == "MenuItem") type = UINodeType::MenuItem;
    
    std::string id = typeStr + "_" + std::to_string(counter++);
    if (nodeDict->fields.count("id") && is_obj_type(nodeDict->fields["id"], OBJ_STRING)) {
        id = static_cast<ObjString*>(std::get<Obj*>(nodeDict->fields["id"]._value))->chars;
    }
    
#ifdef DEBUG_PRINT_CODE
    std::cout << "[DEBUG build] type=" << typeStr << " id=" << id;
    if (nodeDict->fields.count("text")) {
        std::cout << " text_present=true";
        if (is_obj_type(nodeDict->fields["text"], OBJ_STRING)) {
            std::cout << " text_val=" << static_cast<ObjString*>(std::get<Obj*>(nodeDict->fields["text"]._value))->chars;
        }
    }
    if (nodeDict->fields.count("label")) {
        std::cout << " label_present=true";
        if (is_obj_type(nodeDict->fields["label"], OBJ_STRING)) {
            std::cout << " label_val=" << static_cast<ObjString*>(std::get<Obj*>(nodeDict->fields["label"]._value))->chars;
        }
    }
    std::cout << std::endl;
#endif

    auto node = std::make_shared<UINode>(type, id);
    
    auto get_str = [&](const std::string& key, std::string& out) {
        if (nodeDict->fields.count(key) && is_obj_type(nodeDict->fields[key], OBJ_STRING)) {
            out = static_cast<ObjString*>(std::get<Obj*>(nodeDict->fields[key]._value))->chars;
        }
    };
    auto get_num = [&](const std::string& key, float& out) {
        if (nodeDict->fields.count(key) && std::holds_alternative<double>(nodeDict->fields[key]._value)) {
            out = (float)std::get<double>(nodeDict->fields[key]._value);
        }
    };
    
    auto get_bool = [&](const std::string& key, bool& out) {
        if (nodeDict->fields.count(key) && std::holds_alternative<bool>(nodeDict->fields[key]._value)) {
            out = std::get<bool>(nodeDict->fields[key]._value);
        }
    };
    
    get_str("label", node->label);
    get_str("text", node->label);
    get_num("width", node->width);
    get_num("height", node->height);
    get_str("style", node->styleName);
    get_str("align", node->align);
    get_str("justify", node->justify);
    get_str("direction", node->direction);
    get_num("gap", node->gap);
    get_num("value", node->value);
    get_num("min", node->min);
    get_num("max", node->max);
    get_bool("checked", node->checked);
    get_bool("shadow", node->shadow);
    get_num("thickness", node->thickness);
    get_num("margin", node->margin);
    get_str("color", node->customColor);
    
    float fsize = 0;
    get_num("size", fsize);
    if (fsize > 0) node->fontSize = (unsigned int)fsize;
    
    if (nodeDict->fields.count("onClick")) {
        auto onClickVal = nodeDict->fields["onClick"];
        if (is_obj_type(onClickVal, OBJ_CLOSURE) || 
            is_obj_type(onClickVal, OBJ_BOUND_METHOD) || 
            is_obj_type(onClickVal, OBJ_NATIVE) || 
            is_obj_type(onClickVal, OBJ_FUNCTION)) {
            g_current_vm->ui_state.clickHandlers[id] = onClickVal;
        }
    }
    
    if (nodeDict->fields.count("onChange")) {
        auto onChangeVal = nodeDict->fields["onChange"];
        if (is_obj_type(onChangeVal, OBJ_CLOSURE) || 
            is_obj_type(onChangeVal, OBJ_BOUND_METHOD) || 
            is_obj_type(onChangeVal, OBJ_NATIVE) || 
            is_obj_type(onChangeVal, OBJ_FUNCTION)) {
            g_current_vm->ui_state.changeHandlers[id] = onChangeVal;
        }
    }
    
    if (type == UINodeType::Input) {
        bool isFocused = (g_current_vm->ui_state.focusedInputId == id);
        if (!isFocused) {
            // Script owns the value when the input is NOT focused.
            // This handles: initial value, Browse button updates, programmatic changes.
            const std::string& scriptVal = node->label;
            if (g_current_vm->ui_state.inputTexts[id] != scriptVal) {
                // Value changed externally — accept it and move cursor to end
                g_current_vm->ui_state.cursorPositions[id] = scriptVal.size();
            }
            g_current_vm->ui_state.inputTexts[id] = scriptVal;
        }
        // When focused, C++ owns the state — ignore text= from script completely.
        node->label = g_current_vm->ui_state.inputTexts[id];
    }
    
    if (nodeDict->fields.count("children")) {
        auto childrenVal = nodeDict->fields["children"];
        if (std::holds_alternative<std::shared_ptr<SapphireArray>>(childrenVal._value)) {
            auto arr = std::get<std::shared_ptr<SapphireArray>>(childrenVal._value);
            for (auto& childVal : arr->elements) {
                if (is_obj_type(childVal, OBJ_INSTANCE)) {
                    auto childNode = build_ui_tree(static_cast<ObjInstance*>(std::get<Obj*>(childVal._value)), counter);
                    if (childNode) {
                        childNode->parent = node.get();
                        node->children.push_back(childNode);
                    }
                }
            }
        }
    }
    if (!node->styleName.empty()) {
        auto sIt = g_current_vm->ui_state.stylesheets.find(node->styleName);
        if (sIt != g_current_vm->ui_state.stylesheets.end()) {
            if (node->width <= 0.0f) node->width = sIt->second.width;
            if (node->height <= 0.0f) node->height = sIt->second.height;
        }
    }

    if (node->width <= 0.0f) {
        if (type == UINodeType::Button || type == UINodeType::Menu || type == UINodeType::MenuItem) node->width = 120.0f;
        else if (type == UINodeType::Checkbox) node->width = 20.0f;
        else if (type == UINodeType::Slider) node->width = 200.0f;
        else if (type == UINodeType::Input) node->width = 250.0f;
        else if (type == UINodeType::Display) node->width = 200.0f;
        else if (type == UINodeType::Separator) node->width = 100.0f;
    }
    if (node->height <= 0.0f) {
        if (type == UINodeType::Button || type == UINodeType::Menu || type == UINodeType::MenuItem) node->height = 40.0f;
        else if (type == UINodeType::Checkbox) node->height = 20.0f;
        else if (type == UINodeType::Slider) node->height = 20.0f;
        else if (type == UINodeType::Input) node->height = 35.0f;
        else if (type == UINodeType::Display) node->height = 50.0f;
        else if (type == UINodeType::Separator) node->height = 2.0f;
    }

    return node;
}

static SapphireValue native_ui_get_input_text(int arg_count, SapphireValue* args) {
    if (arg_count == 1 && is_obj_type(args[0], OBJ_STRING)) {
        std::string id = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
        if (g_current_vm->ui_state.inputTexts.count(id)) {
            return new_string(g_current_vm, g_current_vm->ui_state.inputTexts[id]);
        }
    }
    return new_string(g_current_vm, "");
}

static SapphireValue native_ui_render(int arg_count, SapphireValue* args) {
    if (!g_current_vm->sfml_window) return {};
    if (arg_count < 1 || !is_obj_type(args[0], OBJ_INSTANCE)) return {};

    while (const std::optional event = g_current_vm->sfml_window->pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            g_current_vm->sfml_window->close();
            exit(0);
        }
        if (const auto* resized = event->getIf<sf::Event::Resized>()) {
            sf::FloatRect visibleArea({0.f, 0.f}, {(float)resized->size.x, (float)resized->size.y});
            g_current_vm->sfml_window->setView(sf::View(visibleArea));
        }
        // Capture click event (position + flag) for reliable cursor placement
        if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mb->button == sf::Mouse::Button::Left) {
                g_current_vm->ui_state.mouseJustClicked = true;
                g_current_vm->ui_state.mouseClickPos = sf::Vector2f((float)mb->position.x, (float)mb->position.y);
            }
        }
        if (!g_current_vm->ui_state.focusedInputId.empty() && g_current_vm->ui_state.inputTexts.count(g_current_vm->ui_state.focusedInputId)) {
            const std::string& fid = g_current_vm->ui_state.focusedInputId;
            std::string& focusedText = g_current_vm->ui_state.inputTexts[fid];
            size_t& cur = g_current_vm->ui_state.cursorPositions[fid];
            // Clamp cursor to valid range (text may have been modified externally)
            if (cur > focusedText.size()) cur = focusedText.size();
            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::Left) {
                    if (cur > 0) cur--;
                } else if (keyPressed->code == sf::Keyboard::Key::Right) {
                    if (cur < focusedText.size()) cur++;
                } else if (keyPressed->code == sf::Keyboard::Key::Home) {
                    cur = 0;
                } else if (keyPressed->code == sf::Keyboard::Key::End) {
                    cur = focusedText.size();
                } else if (keyPressed->code == sf::Keyboard::Key::Delete) {
                    if (cur < focusedText.size()) {
                        focusedText.erase(cur, 1);
                        g_current_vm->ui_state.textChangedState[fid] = true;
                    }
                }
            }
            if (const auto* textEntered = event->getIf<sf::Event::TextEntered>()) {
                if (textEntered->unicode < 128) {
                    char c = static_cast<char>(textEntered->unicode);
                    if (c == '\b') {
                        if (cur > 0) {
                            focusedText.erase(cur - 1, 1);
                            cur--;
                            g_current_vm->ui_state.textChangedState[fid] = true;
                        }
                    } else if (c >= 32 && c <= 126) {
                        focusedText.insert(cur, 1, c);
                        cur++;
                        g_current_vm->ui_state.textChangedState[g_current_vm->ui_state.focusedInputId] = true;
                    }
                }
            }
        }
    }

    g_current_vm->ui_state.clickHandlers.clear();
    int counter = 0;
    auto rootNode = build_ui_tree(static_cast<ObjInstance*>(std::get<Obj*>(args[0]._value)), counter);
    g_current_vm->ui_state.rootNode = rootNode;

    if (g_current_vm->ui_state.layoutEngineEnabled && g_current_vm->ui_state.rootNode) {
        sf::Vector2u winSize = g_current_vm->sfml_window->getSize();
        g_current_vm->ui_state.rootNode->width = (float)winSize.x;
        g_current_vm->ui_state.rootNode->height = (float)winSize.y;
        compute_sizes(g_current_vm->ui_state.rootNode);
        g_current_vm->ui_state.rootNode->width = (float)winSize.x;
        g_current_vm->ui_state.rootNode->height = (float)winSize.y;
        place_children(g_current_vm->ui_state.rootNode, 0.0f, 0.0f);
    }

    sf::Vector2i m = sf::Mouse::getPosition(*g_current_vm->sfml_window);
    bool mouseJustClicked = g_current_vm->ui_state.mouseJustClicked;
    g_current_vm->ui_state.mouseJustClicked = false; // consume the click
    hit_test_tree(g_current_vm->ui_state.rootNode, m, mouseJustClicked);

    sf::Color clearColor = g_current_vm->ui_state.currentStyleColor;
    if (g_current_vm->ui_state.activeStyle != nullptr) clearColor = g_current_vm->ui_state.activeStyle->bgColor;
    g_current_vm->sfml_window->clear(clearColor);
    deferred_render_nodes.clear();
    render_ui_tree(g_current_vm->ui_state.rootNode);
    for (auto& node : deferred_render_nodes) {
        render_ui_tree(node);
    }
    g_current_vm->sfml_window->display();

    for (const auto& [id, clicked] : g_current_vm->ui_state.clickState) {
        if (clicked && g_current_vm->ui_state.clickHandlers.count(id)) {
            g_current_vm->ui_state.clickState[id] = false;
            return g_current_vm->ui_state.clickHandlers[id];
        }
    }
    
    for (const auto& [id, changed] : g_current_vm->ui_state.textChangedState) {
        if (changed && g_current_vm->ui_state.changeHandlers.count(id)) {
            g_current_vm->ui_state.textChangedState[id] = false;
            return g_current_vm->ui_state.changeHandlers[id];
        }
    }

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

static SapphireValue native_system_exec(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: exec() expects 1 string argument (the command)." << std::endl;
        }
        return {};
    }
    std::string command = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    int result = std::system(command.c_str());
    return (double)result;
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

static SapphireValue native_check_collision(int arg_count, SapphireValue* args) {
    if (arg_count < 8) return false;
    double x1 = std::get<double>(args[0]._value);
    double y1 = std::get<double>(args[1]._value);
    double w1 = std::get<double>(args[2]._value);
    double h1 = std::get<double>(args[3]._value);
    double x2 = std::get<double>(args[4]._value);
    double y2 = std::get<double>(args[5]._value);
    double w2 = std::get<double>(args[6]._value);
    double h2 = std::get<double>(args[7]._value);

    return (x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2);
}

VM::VM() : VM(ScriptConfig{}) {
}

VM::VM(const ScriptConfig& config) : VM(config, false, nullptr) {
}

VM::VM(const ScriptConfig& config, bool init_ui, sf::RenderWindow* window) : config(config) {
    g_current_vm = this;
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

    // --- IO ---
    define_native("readLine", io_readline_native);
    define_native("printColor", native_io_print_color);
    define_native("writeFile", native_io_write_file);
    define_native("readFile", native_io_read_file);
    define_native("exists", native_io_exists);
    define_native("deleteFile", native_io_delete_file);
    define_native("appendFile", native_io_append_file);
    define_native("openFileDialog", native_io_open_file_dialog);

    // --- Math ---
    define_native("sqrt", native_math_sqrt);
    define_native("rand", native_math_rand);
    define_native("abs", native_math_abs);
    define_native("floor", native_math_floor);
    define_native("ceil", native_math_ceil);
    define_native("sin", native_math_sin);
    define_native("cos", native_math_cos);
    define_native("pow", native_math_pow);
    define_native("min", native_math_min);
    define_native("max", native_math_max);
    define_native("clamp", native_math_clamp);
    define_native("lerp", native_math_lerp);

    // --- JSON ---
    define_native("jsonParse", native_json_parse);

    // --- Core ---
    define_native("createInstance", core_create_instance);

    // --- ListUtil ---
    define_native("listCreate", native_list_util_create);
    define_native("listAppend", native_list_util_append);
    define_native("listGet", native_list_util_get);
    define_native("listSet", native_list_util_set);
    define_native("listLength", native_list_util_length);
    define_native("listRemoveAt", native_list_util_remove_at);
    define_native("listContains", native_list_util_contains);

    // --- System ---
    define_native("getEnv", native_system_get_env);
    define_native("getOS", native_system_get_os);
    define_native("sleep", native_system_sleep);
    define_native("getClipboard", native_system_get_clipboard);
    define_native("exec", native_system_exec);
    define_native("spawn", native_spawn);
    define_native("join", native_join);
    define_native("getCoreCount", native_system_core_count);

    // --- HTTP ---
    define_native("httpGet", native_http_get);
    define_native("httpPost", native_http_post);
    define_native("httpPing", native_http_ping);
    define_native("httpDownload", native_http_download);

    // --- Color ---
    define_native("hexToRGB", native_color_hex_to_rgb);

    // --- Debug ---
    define_native("printStack", native_debug_print_stack);
    define_native("dumpGlobals", native_debug_dump_globals);

    // --- checkCollision ---
    define_native("checkCollision", native_check_collision);

    // Registro SFML puro
    register_graphics_engine(this);
    register_vec2d_class(this);
    register_vec3d_class(this);

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

        this->ui_component_class = new_class(this, new_string(this, "UIComponent"));
        define_native("Render", native_ui_render);
        define_native("Style", native_ui_style);
        define_native("Flex", native_ui_flex);
        define_native("Button", native_ui_button);
        define_native("Text", native_ui_text);
        define_native("Display", native_ui_display);
        define_native("Checkbox", native_ui_checkbox);
        define_native("Slider", native_ui_slider);
        define_native("Input", native_ui_input);
        define_native("Separator", native_ui_separator);
        define_native("GetInputText", native_ui_get_input_text);
        define_native("Menu", native_ui_menu);
        define_native("MenuItem", native_ui_menuitem);

        globals["_ui_initialized"] = {};
        globals["APP_WINDOW_WIDTH"] = (double)config.windowWidth;
        globals["APP_WINDOW_HEIGHT"] = (double)config.windowHeight;
    }
}
void VM::add_module_search_path(const std::string& path) {
    module_search_paths.push_back(path);
}

std::string VM::find_and_load_module(const std::string& module_name) {
    std::string content = load_file_as_string(module_name);
    if (!content.empty()) return content;

    for (const std::string& base_path : module_search_paths) {
        std::string path_direct = base_path + "\\" + module_name;
        content = load_file_as_string(path_direct);
        if (!content.empty()) return content;

        std::string path_sp = base_path + "\\" + module_name + ".sp";
        content = load_file_as_string(path_sp);
        if (!content.empty()) return content;

        std::string path_main_sp = base_path + "\\" + module_name + "\\main.sp";
        content = load_file_as_string(path_main_sp);
        if (!content.empty()) return content;
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
                       std::get<Obj*>(callee._value)->type == OBJ_CLASS ||
                       std::get<Obj*>(callee._value)->type == OBJ_BOUND_METHOD ||
                       std::get<Obj*>(callee._value)->type == OBJ_FUNCTION);

    if (!is_callable) {
        for (int i = 1; i <= 4; i++) {
            SapphireValue potential = stack_top[-arg_count - 1 - i];
            if (std::holds_alternative<Obj*>(potential._value)) {
                Obj* o = std::get<Obj*>(potential._value);
                if (o->type == OBJ_CLOSURE || o->type == OBJ_NATIVE || o->type == OBJ_CLASS || o->type == OBJ_BOUND_METHOD || o->type == OBJ_FUNCTION) {
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
                return call_value(bound->method, arg_count);
            }
            case OBJ_CLASS: {
                ObjClass* klass = (ObjClass*)obj;
                stack_top[-arg_count - 1] = new_instance(this, klass);
                return true;
            }
            case OBJ_CLOSURE:
                return call(((ObjClosure*)obj)->function, arg_count);
            case OBJ_FUNCTION:
                return call((ObjFunction*)obj, arg_count);
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
    uint8_t* ip = frame->ip;
    SapphireValue* slots = frame->slots;
    #define top stack_top

    static const void* dispatch_table[255];
    static bool table_initialized = false;

    // Variáveis auxiliares declaradas fora para evitar erro de inicialização cruzada
    ObjString* name_tmp;
    std::string src_tmp;
    ObjFunction* func_tmp;
    SapphireValue val_tmp;

    if (!table_initialized) {
        for (int i = 0; i < 255; i++) dispatch_table[i] = &&op_unknown;
        dispatch_table[OP_CONSTANT] = &&op_constant;
        dispatch_table[OP_NIL] = &&op_nil;
        dispatch_table[OP_TRUE] = &&op_true;
        dispatch_table[OP_FALSE] = &&op_false;
        dispatch_table[OP_POP] = &&op_pop;
        dispatch_table[OP_GET_LOCAL] = &&op_get_local;
        dispatch_table[OP_SET_LOCAL] = &&op_set_local;
        dispatch_table[OP_GET_GLOBAL] = &&op_get_global;
        dispatch_table[OP_DEFINE_GLOBAL] = &&op_define_global;
        dispatch_table[OP_SET_GLOBAL] = &&op_set_global;
        dispatch_table[OP_GET_PROPERTY] = &&op_get_property;
        dispatch_table[OP_SET_PROPERTY] = &&op_set_property;
        dispatch_table[OP_EQUAL] = &&op_equal;
        dispatch_table[OP_GREATER] = &&op_greater;
        dispatch_table[OP_LESS] = &&op_less;
        dispatch_table[OP_ADD] = &&op_add;
        dispatch_table[OP_SUBTRACT] = &&op_subtract;
        dispatch_table[OP_MULTIPLY] = &&op_multiply;
        dispatch_table[OP_DIVIDE] = &&op_divide;
        dispatch_table[OP_NOT] = &&op_not;
        dispatch_table[OP_NEGATE] = &&op_negate;
        dispatch_table[OP_PRINT] = &&op_print;
        dispatch_table[OP_JUMP] = &&op_jump;
        dispatch_table[OP_JUMP_IF_FALSE] = &&op_jump_if_false;
        dispatch_table[OP_LOOP] = &&op_loop;
        dispatch_table[OP_CALL] = &&op_call;
        dispatch_table[OP_CLOSURE] = &&op_closure;
        dispatch_table[OP_RETURN] = &&op_return;
        dispatch_table[OP_BUILD_ARRAY] = &&op_build_array;
        dispatch_table[OP_IMPORT] = &&op_import;
        dispatch_table[OP_MAKE_NAMED_ARG] = &&op_make_named_arg;
        table_initialized = true;
    }

#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
#define PUSH(val) (*(top++) = val)
#define POP() (*(--top))
#define NEXT_CODE() goto *dispatch_table[READ_BYTE()]

    NEXT_CODE();

op_unknown:
    return false;

op_constant:
    PUSH(frame->function->chunk.constants[READ_SHORT()]);
    NEXT_CODE();

op_nil:
    PUSH(SapphireValue());
    NEXT_CODE();

op_true:
    PUSH(SapphireValue(true));
    NEXT_CODE();

op_false:
    PUSH(SapphireValue(false));
    NEXT_CODE();

op_pop:
    top--;
    NEXT_CODE();

op_get_local:
    PUSH(slots[READ_BYTE()]);
    NEXT_CODE();

op_set_local:
    slots[READ_BYTE()] = top[-1];
    NEXT_CODE();

op_get_global: {
    name_tmp = (ObjString*)std::get<Obj*>(frame->function->chunk.constants[READ_SHORT()]._value);
    auto it = globals.find(name_tmp->chars);
    if (it == globals.end()) {
        if (!this->soft_mode) {
            std::cerr << "Runtime Error: Undefined variable '" << name_tmp->chars << "'." << std::endl;
            return false;
        }
        PUSH(SapphireValue());
    } else PUSH(it->second);
    NEXT_CODE();
}

op_define_global: {
    name_tmp = (ObjString*)std::get<Obj*>(frame->function->chunk.constants[READ_SHORT()]._value);
    globals[name_tmp->chars] = POP();
    NEXT_CODE();
}

op_set_global: {
    name_tmp = (ObjString*)std::get<Obj*>(frame->function->chunk.constants[READ_SHORT()]._value);
    globals[name_tmp->chars] = top[-1];
    NEXT_CODE();
}

op_get_property: {
    if (top[-1]._value.index() != 3) {
        if (!this->soft_mode) return false;
        top[-1] = SapphireValue();
    } else {
        ObjInstance* instance = (ObjInstance*)std::get<Obj*>(top[-1]._value);
        name_tmp = (ObjString*)std::get<Obj*>(frame->function->chunk.constants[READ_SHORT()]._value);
        auto it_f = instance->fields.find(name_tmp->chars);
        if (it_f != instance->fields.end()) {
            top[-1] = it_f->second;
        } else {
            auto it_m = instance->klass->methods.find(name_tmp->chars);
            if (it_m != instance->klass->methods.end()) {
                top[-1] = new_bound_method(this, top[-1], it_m->second);
            } else {
                top[-1] = SapphireValue();
            }
        }
    }
    NEXT_CODE();
}

op_set_property: {
    ObjInstance* instance = (ObjInstance*)std::get<Obj*>(top[-2]._value);
    name_tmp = (ObjString*)std::get<Obj*>(frame->function->chunk.constants[READ_SHORT()]._value);
    instance->fields[name_tmp->chars] = top[-1];
    write_barrier((Obj*)instance, top[-1]);
    val_tmp = POP(); top--; PUSH(val_tmp);
    NEXT_CODE();
}

op_equal: {
    {
        SapphireValue b = POP(); SapphireValue a = POP();
        bool isEqual = false;
        if (a._value.index() == b._value.index()) {
            if (std::holds_alternative<Obj*>(a._value) && std::holds_alternative<Obj*>(b._value)) {
                Obj* objA = std::get<Obj*>(a._value);
                Obj* objB = std::get<Obj*>(b._value);
                if (objA && objB && objA->type == OBJ_STRING && objB->type == OBJ_STRING) {
                    isEqual = (static_cast<ObjString*>(objA)->chars == static_cast<ObjString*>(objB)->chars);
                } else {
                    isEqual = (objA == objB);
                }
            } else {
                isEqual = (a._value == b._value);
            }
        }
        PUSH(SapphireValue(isEqual));
    }
    NEXT_CODE();
}

op_greater: {
    double b = std::get<double>(POP()._value);
    double a = std::get<double>(POP()._value);
    PUSH(SapphireValue(a > b));
    NEXT_CODE();
}

op_less: {
    double b = std::get<double>(POP()._value);
    double a = std::get<double>(POP()._value);
    PUSH(SapphireValue(a < b));
    NEXT_CODE();
}

op_add: {
    auto& v1 = top[-1]._value; auto& v2 = top[-2]._value;
    if (v1.index() == 2 && v2.index() == 2) {
        double res = std::get<double>(v2) + std::get<double>(v1);
        top -= 2; PUSH(SapphireValue(res));
    } else if (v1.index() == 3 || v2.index() == 3) {
        std::string s2 = valueToStringC(top[-2]);
        std::string s1 = valueToStringC(top[-1]);
        bool parsed = false;
        try {
            size_t p2 = 0, p1 = 0;
            double d2 = std::stod(s2, &p2);
            double d1 = std::stod(s1, &p1);
            if (p2 == s2.length() && p1 == s1.length()) {
                top -= 2; PUSH(SapphireValue(d2 + d1));
                parsed = true;
            }
        } catch (...) {}
        if (!parsed) {
            top -= 2; PUSH(new_string(this, s2 + s1));
        }
    } else { if (!this->soft_mode) return false; top -= 2; PUSH(SapphireValue()); }
    NEXT_CODE();
}

op_subtract: { double b = valueToDoubleC(POP()); double a = valueToDoubleC(POP()); PUSH(SapphireValue(a - b)); NEXT_CODE(); }
op_multiply: { double b = valueToDoubleC(POP()); double a = valueToDoubleC(POP()); PUSH(SapphireValue(a * b)); NEXT_CODE(); }
op_divide:   { double b = valueToDoubleC(POP()); double a = valueToDoubleC(POP()); PUSH(SapphireValue(a / b)); NEXT_CODE(); }

op_not:
    top[-1] = SapphireValue(is_falsey(top[-1]));
    NEXT_CODE();

op_negate:
    if (top[-1]._value.index() == 2) std::get<double>(top[-1]._value) *= -1;
    NEXT_CODE();

op_print:
    stack_top = top;
    print_value(POP());
    std::cout << std::endl;
    top = stack_top;
    NEXT_CODE();

op_jump:
    ip += READ_SHORT();
    NEXT_CODE();

op_jump_if_false: {
    uint16_t offset = READ_SHORT();
    if (is_falsey(top[-1])) ip += offset;
    NEXT_CODE();
}

op_loop:
    step_gc();
    ip -= READ_SHORT();
    NEXT_CODE();

op_call: {
    step_gc();
    int arg_count = READ_BYTE();
    frame->ip = ip;
    stack_top = top;
    if (!call_value(top[-arg_count - 1], arg_count)) return false;
    frame = &frames[frame_count - 1];
    ip = frame->ip; slots = frame->slots; top = stack_top;
    NEXT_CODE();
}

op_closure: {
    func_tmp = (ObjFunction*)std::get<Obj*>(frame->function->chunk.constants[READ_SHORT()]._value);
    PUSH(new_closure(this, func_tmp));
    NEXT_CODE();
}

op_return: {
    val_tmp = POP();
    frame_count--;
    if (frame_count == 0) { stack_top = top; return true; }
    top = frame->slots;
    PUSH(val_tmp);
    frame = &frames[frame_count - 1];
    ip = frame->ip; slots = frame->slots;
    NEXT_CODE();
}

op_build_array: {
    {
        uint8_t count = READ_BYTE();
        auto arr = std::make_shared<SapphireArray>();
        for (int i = 0; i < count; i++) arr->elements.push_back(top[-count + i]);
        top -= count; PUSH(arr);
    }
    NEXT_CODE();
}

op_import: {
    name_tmp = (ObjString*)std::get<Obj*>(frame->function->chunk.constants[READ_SHORT()]._value);
    frame->ip = ip; stack_top = top;
    src_tmp = find_and_load_module(name_tmp->chars);
    if (src_tmp.empty()) {
        std::cerr << "[SAPPHIRE ERROR] Module '" << name_tmp->chars << "' not found." << std::endl;
        return false;
    }
    func_tmp = compile(this, src_tmp);
    if (!func_tmp) return false;
    PUSH(new_closure(this, func_tmp));
    call_value(top[-1], 0);
    frame = &frames[frame_count - 1];
    ip = frame->ip; slots = frame->slots; top = stack_top;
    NEXT_CODE();
}

op_make_named_arg: {
    {
        SapphireValue value = POP();
        SapphireValue name = POP();
        ObjNamedArg* arg = new_named_arg(this, static_cast<ObjString*>(std::get<Obj*>(name._value)), value);
        PUSH(arg);
    }
    NEXT_CODE();
}

#undef READ_BYTE
#undef READ_SHORT
#undef PUSH
#undef POP
#undef NEXT_CODE
#undef top
}

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
    Preprocessor prep;
    std::string processed_source = prep.process(source);

    ObjFunction* function = compile(this, processed_source);
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
                mark_value(val);
            }
            break;
        }
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            mark_value(bound->receiver);
            mark_value(bound->method);
            break;
        }
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
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

void VM::write_barrier(Obj* object, SapphireValue value) {
    if (gc_state == GCState::GC_TRACE) {
        if (object->is_marked) {
            mark_value(value);
        }
    }
}

void VM::step_gc() {
    if (gc_state == GCState::GC_IDLE) {
        if (bytes_allocated > next_gc_threshold) {
            gc_state = GCState::GC_MARK_ROOTS;
        } else {
            return;
        }
    }

    if (gc_state == GCState::GC_MARK_ROOTS) {
        mark_roots();
        gc_state = GCState::GC_TRACE;
        return;
    }

    if (gc_state == GCState::GC_TRACE) {
        int trace_limit = 500;
        while (!gray_stack.empty() && trace_limit > 0) {
            Obj* object = gray_stack.back();
            gray_stack.pop_back();
            blacken_object(object);
            trace_limit--;
        }
        if (gray_stack.empty()) {
            // Remark phase: rescan roots to catch anything mutated during tracing
            mark_roots();
            if (gray_stack.empty()) {
                gc_state = GCState::GC_SWEEP;
                sweep_previous = nullptr;
                sweep_current = objects;
            }
        }
        return;
    }

    if (gc_state == GCState::GC_SWEEP) {
        int sweep_limit = 500;
        while (sweep_current != nullptr && sweep_limit > 0) {
            Obj* object = sweep_current;
            if (object->is_marked) {
                object->is_marked = false;
                sweep_previous = object;
                sweep_current = object->next;
            } else {
                Obj* unreached = object;
                sweep_current = object->next;
                if (sweep_previous != nullptr) {
                    sweep_previous->next = sweep_current;
                } else {
                    objects = sweep_current;
                }
                
                size_t size = 0;
                switch (unreached->type) {
                    case OBJ_STRING: size = sizeof(ObjString); break;
                    case OBJ_FUNCTION: size = sizeof(ObjFunction); break;
                    case OBJ_NATIVE: size = sizeof(ObjNative); break;
                    case OBJ_CLOSURE: size = sizeof(ObjClosure); break;
                    case OBJ_CLASS: size = sizeof(ObjClass); break;
                    case OBJ_INSTANCE: size = sizeof(ObjInstance); break;
                    case OBJ_BOUND_METHOD: size = sizeof(ObjBoundMethod); break;
                    case OBJ_NAMED_ARG: size = sizeof(ObjNamedArg); break;
                }
                if (bytes_allocated >= size) bytes_allocated -= size;
                else bytes_allocated = 0;

                free_object(unreached);
            }
            sweep_limit--;
        }

        if (sweep_current == nullptr) {
            gc_state = GCState::GC_IDLE;
            next_gc_threshold = bytes_allocated * 2;
            if (next_gc_threshold < 1024 * 1024) next_gc_threshold = 1024 * 1024;
        }
        return;
    }
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
