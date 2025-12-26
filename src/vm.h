#ifndef SAPPHIRE_VM_H
#define SAPPHIRE_VM_H

#include "chunk.h"
#include "value.h"
#include "object.h"
#include <unordered_map>
#include <string>
#include <map>
#include <vector>
#include <optional>
#include <chrono>
#include "config.h"
#include "utils.h"
#include "tokens.h"
#include <SFML/Graphics.hpp>

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * 256)

struct UIStyle {
    sf::Color bgColor = sf::Color(60, 60, 60);
    sf::Color textColor = sf::Color::White;
    sf::Color hoverColor = sf::Color(80, 80, 80);
    sf::Color accentColor = sf::Color(50, 120, 240);
    sf::Color borderColor = sf::Color(100, 100, 100);
    float borderThickness = 1.0f;
    float borderRadius = 4.0f;
    float padding = 8.0f;
    std::string fontAlias;
    unsigned int fontSize;
};

struct ComponentProps {
    std::optional<sf::Color> bgColor;
    std::optional<sf::Color> textColor;
    std::optional<sf::Color> accentColor;
    std::optional<float> borderRadius;
    std::optional<unsigned int> fontSize;
    std::optional<float> padding;
};

struct UIState {
    float nextPosX = 10.0f;
    float nextPosY = 10.0f;
    float lastItemHeight = 0.0f;
    UIStyle defaultStyle;
    UIStyle* activeStyle = nullptr;
    std::map<std::string, UIStyle> stylesheets;
    std::map<std::string, ComponentProps> idOverrides;
    std::string inputBuffer = "";
    size_t cursorPos = 0;
    bool menuJustOpened = false;
    std::string activeMenu = "";
    sf::Vector2f activeMenuPos = {0, 0};
    float activeMenuWidth = 0.0f;
    float menuOffsetY = 0.0f;
    sf::Color currentStyleColor = sf::Color(30, 30, 35);
    unsigned int fontSize = 14;
    bool clickConsumedThisFrame = false;
    std::string activeFontName = "default";
    std::map<std::string, sf::Font> fontStack;
    std::string lastComponentId = "none";
    int widgetCount = 0;
    bool debugOverlay = false;
    std::chrono::steady_clock::time_point lastClickTime;
    float debounceTime = 0.2f;
};

struct CallFrame {
    ObjFunction* function;
    uint8_t* ip;
    SapphireValue* slots;
};

class VM;

template <typename T>
void register_object(VM* vm, T* object);

class VM {
public:
    template <typename T>
    friend void register_object(VM* vm, T* object);

    VM();
    VM(const ScriptConfig& config);
    VM(const ScriptConfig& config, bool init_ui, sf::RenderWindow* window);
    ~VM();

    SapphireValue interpret(const std::string& source);
    bool run_function(ObjFunction* function);
    void setGlobalNumber(const std::string& name, double value);
    void resetStack();
    void collect_garbage();
    ObjFunction* compile_module(const std::string& source);
    bool run_module(ObjFunction* module_function);
    bool call_and_run(ObjFunction* function);

    SapphireValue getGlobal(const std::string& name);
    SapphireValue pop();
    void push(const SapphireValue& value);
    bool call(ObjFunction* function, int arg_count);

    std::unordered_map<std::string, SapphireValue> globals;
    sf::RenderWindow* sfml_window;
    sf::Font sapphire_font;
    UIState ui_state;
    ScriptConfig config;
    bool soft_mode = false;
    SapphireValue stack[STACK_MAX];
    SapphireValue* stack_top;

private:
    CallFrame frames[FRAMES_MAX];
    int frame_count;
    friend void debug_print_stack(VM* vm);

    Obj* objects = nullptr;
    std::vector<Obj*> gray_stack;

    bool run();
    SapphireValue& peek(int distance);

    bool call_value(SapphireValue callee, int arg_count);
    std::vector<std::string> module_search_paths;
    std::string find_and_load_module(const std::string& module_name);

    void define_native(const std::string& name, NativeFn function);
    void define_ui_natives();
    void mark_roots();
    void trace_references();
    void sweep();
    void mark_object(Obj* object);
    void mark_value(SapphireValue value);
    void blacken_object(Obj* object);
};

extern VM* g_current_vm;

#endif