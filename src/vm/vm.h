#ifndef SAPPHIRE_VM_H
#define SAPPHIRE_VM_H

#include "chunk.h"
#include "config.h"
#include "object.h"
#include "tokens.h"
#include "utils.h"
#include "value.h"
#include <SFML/Graphics.hpp>
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * 256)

struct UIStyle {
  sf::Color bgColor = sf::Color(0, 0, 0, 0);
  sf::Color textColor = sf::Color::White;
  sf::Color hoverColor = sf::Color(80, 80, 80);
  sf::Color accentColor = sf::Color(50, 120, 240);
  sf::Color borderColor = sf::Color(100, 100, 100);
  float borderThickness = 0.0f;
  float borderRadius = 4.0f;
  float padding = 8.0f;
  std::string fontAlias;
  unsigned int fontSize = 0;
  float width = 0.0f;
  float height = 0.0f;
  float margin = 0.0f;
  float thickness = 0.0f;
  float opacity = 1.0f;
  float scaleX = 1.0f;
  float scaleY = 1.0f;
  float rotation = 0.0f;
};

struct ComponentProps {
  std::optional<sf::Color> bgColor;
  std::optional<sf::Color> textColor;
  std::optional<sf::Color> accentColor;
  std::optional<float> borderRadius;
  std::optional<unsigned int> fontSize;
  std::optional<float> padding;
};

struct Keyframe {
  float timeOffset; // from 0.0 to 1.0
  std::map<std::string, float> numericProps;
  std::map<std::string, sf::Color> colorProps;
};

struct Animation {
  std::string id;
  float duration = 1.0f; // in seconds
  bool loop = false;
  std::string easing = "linear";
  std::vector<Keyframe> keyframes;
};

struct ActiveAnimation {
  std::string animId;
  float elapsedTime = 0.0f;
};

#include "ui_node.h"

struct UIState {
  float nextPosX = 10.0f;
  float nextPosY = 10.0f;
  float lastItemHeight = 0.0f;
  UIStyle defaultStyle;
  UIStyle *activeStyle = nullptr;
  std::map<std::string, UIStyle> stylesheets;
  std::string activeStyleName = "";
  std::map<std::string, ComponentProps> idOverrides;
  std::string inputBuffer = "";
  std::map<std::string, size_t> cursorPositions; // per-input cursor position
  bool menuJustOpened = false;
  std::string activeMenu = "";
  sf::Vector2f activeMenuPos = {0, 0};
  float activeMenuWidth = 0.0f;
  float menuOffsetY = 0.0f;
  sf::Color currentStyleColor = sf::Color(0, 0, 0, 0);
  unsigned int fontSize = 14;
  bool clickConsumedThisFrame = false;
  std::string activeFontName = "default";
  std::map<std::string, sf::Font> fontStack;
  std::string lastComponentId = "none";
  int widgetCount = 0;
  bool debugOverlay = false;
  std::chrono::steady_clock::time_point lastClickTime;
  float debounceTime = 0.2f;
  
  // Retained Mode UI State
  std::shared_ptr<UINode> rootNode = nullptr;
  UINode* currentBuildNode = nullptr;
  int buildIndexCounter = 0;
  bool layoutEngineEnabled = true;
  
  std::map<std::string, bool> hoverState;
  std::map<std::string, bool> clickState;
  std::map<std::string, SapphireValue> clickHandlers;
  
  std::string focusedInputId = "";
  std::map<std::string, std::string> inputTexts;
  std::map<std::string, std::string> lastPassedText;
  std::map<std::string, bool> textChangedState;
  std::map<std::string, SapphireValue> changeHandlers;
  std::map<std::string, bool> toggleStates;
  std::map<std::string, float> sliderValues;
  bool mouseJustClicked = false;
  sf::Vector2f mouseClickPos = {0, 0};

  // Animations
  std::map<std::string, Animation> animations;
  std::map<std::string, ActiveAnimation> activeAnimations;
  std::chrono::steady_clock::time_point lastRenderTime;
  bool firstRender = true;
};

struct CallFrame {
  ObjFunction *function; // Voltando para ObjFunction estÃ¡vel
  uint8_t *ip;
  SapphireValue *slots;
};

struct CatchBlock {
  int frame_count;
  SapphireValue* stack_top;
  uint8_t* catch_ip;
};

enum class PromiseState { PENDING, FULFILLED, REJECTED };

struct ObjPromise : Obj {
  PromiseState state;
  SapphireValue value;

  ObjFunction* function = nullptr;
  std::vector<SapphireValue> args;

  std::vector<SapphireValue> saved_stack;
  std::vector<CallFrame> saved_frames;
  std::vector<CatchBlock> saved_catch_blocks;
  
  std::vector<ObjPromise*> awaiters;
};

class VM {
public:
  VM();
  VM(const ScriptConfig &config);
  VM(const ScriptConfig &config, bool init_ui, sf::RenderWindow *window);
  ~VM();

  SapphireValue interpret(const std::string &source);
  bool run_function(ObjFunction *function);
  void setGlobalNumber(const std::string &name, double value);
  void resetStack();
  void step_gc();
  void write_barrier(Obj* object, SapphireValue value);
  void add_module_search_path(const std::string& path);
  ObjFunction *compile_module(const std::string &source);
  bool run_module(ObjFunction *module_function);
  bool call_and_run(ObjFunction *function);

  SapphireValue getGlobal(const std::string &name);
  SapphireValue pop();
  void push(const SapphireValue &value);
  bool call(ObjFunction *function, int arg_count);
  bool run(int target_frame_count = 0);
  bool call_value(SapphireValue callee, int arg_count);

  std::unordered_map<std::string, SapphireValue> globals;
  sf::RenderWindow *sfml_window;
  sf::Font sapphire_font;
  UIState ui_state;
  ScriptConfig config;
  bool soft_mode = false;
  SapphireValue stack[STACK_MAX];
  Obj *objects = nullptr;
  SapphireValue *stack_top;
  
  ObjPromise* current_promise = nullptr;
  std::vector<ObjPromise*> event_loop_queue;

  size_t bytes_allocated = 0;
  size_t next_gc_threshold = 1024 * 1024; // 1MB threshold inicial
  size_t max_memory_limit = 500 * 1024 * 1024; // 500 MB limit

  CallFrame frames[FRAMES_MAX];
  int frame_count;


  enum class GCState {
    GC_IDLE,
    GC_MARK_ROOTS,
    GC_TRACE,
    GC_SWEEP
  };
  GCState gc_state = GCState::GC_IDLE;
  Obj* sweep_previous = nullptr;
  Obj* sweep_current = nullptr;

private:

  CatchBlock catch_blocks[64];
  int catch_count;

  std::vector<Obj *> gray_stack;

  SapphireValue &peek(int distance);

  std::vector<std::string> module_search_paths;
  std::unordered_set<std::string> loaded_modules;
  std::string find_and_load_module(const std::string &module_name, std::string& out_resolved_path);

  void define_native(const std::string &name, NativeFn function);
  void define_ui_natives();
  void mark_roots();
  void trace_references();
  void sweep();
  void mark_object(Obj *object);
  void mark_value(SapphireValue value);
  void blacken_object(Obj *object);

  friend void debug_print_stack(VM *vm);
public:
  ObjClass* ui_component_class = nullptr;
};

extern thread_local VM *g_current_vm;

#endif








