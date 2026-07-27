// Rubellite VM Implementation
// TODO: Instead of using separate folders, merge them into separate functions in the vm/ folder.

#define VECTOR_DATA_OFFSET 0
#define VECTOR_SIZE_OFFSET sizeof(void*)


#include <stdexcept>

#ifndef JIT_ABI
#ifdef _WIN32
#define JIT_ABI
#else
#define JIT_ABI __attribute__((ms_abi))
#endif
#endif
#include <mutex>
#include <condition_variable>
#include <thread>
#include <sstream>
#include <iomanip>
#include "vm.h"
#include "compiler.h"
#include "object.h"
#include "jit_assembler.h"
#include "debug.h"
#include "value.h"
#include "config.h"
#include "termcolor.h"
#include "sapphire_api.h"
#include "opencl_api.h"
#include "sqlite_api.h"
#include "utils.h"
#include "opcodes.h"
#include "httplib.h"
#include "tokens.h"
#include "nlohmann/json.hpp"
#include "opencl_api.h"
#include "preprocessor/preprocessor.h"
#include "bytecode_io.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
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
#include <conio.h>
#else
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#endif

static std::random_device rd;
static std::mt19937 gen(rd());

thread_local VM* g_current_vm = nullptr;

static std::mutex thread_mutex;
static std::map<int, std::thread> active_threads;
static int next_thread_id = 1;

std::string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_c);
    std::stringstream ss;
    ss << std::put_time(now_tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}











static std::mutex global_mutexes_lock;
static std::map<int, std::shared_ptr<std::mutex>> global_mutexes;
static int next_mutex_id = 1;











static SapphireValue convertJsonToSapphire(VM* vm, const nlohmann::json& j);

static SapphireValue convertJsonObjectToSapphireMap(VM* vm, const nlohmann::json& j) {
    ObjMap* map_obj = new_map(vm);
    vm->push(SapphireValue(map_obj));
    for (auto it = j.begin(); it != j.end(); ++it) {
        std::string key_copy = it.key();
        const nlohmann::json& value = it.value();
        map_obj->items[key_copy] = convertJsonToSapphire(vm, value);
    }
    vm->pop();
    return map_obj;
}

static SapphireValue convertJsonArrayToSapphireArray(VM* vm, const nlohmann::json& j) {
    auto array_obj = new_array(g_current_vm);
    for (const auto& element : j) {
        array_obj->elements.push_back(convertJsonToSapphire(vm, element));
    }
    return array_obj;
}

static SapphireValue convertJsonToSapphire(VM* vm, const nlohmann::json& j) {
    if (j.is_object()) return convertJsonObjectToSapphireMap(vm, j);
    if (j.is_array()) return convertJsonArrayToSapphireArray(vm, j);
    if (j.is_string()) return new_string(vm, j.get<std::string>());
    if (j.is_number()) return j.get<double>();
    if (j.is_boolean()) return j.get<bool>();
    if (j.is_null()) return {};
    return {};
}


static const auto clock_start_time = std::chrono::high_resolution_clock::now();



















































static double valueToDoubleC(const SapphireValue& val) {
    if (val.type == ValType::VAL_NUMBER) {
        return val.as.number;
    } else if (val.type == ValType::VAL_OBJ) {
        ObjString* str = static_cast<ObjString*>(val.as.obj);
        try { return std::stod(str->chars); } catch (...) { return 0.0; }
    }
    return 0.0;
}











static void sapphire_ui_trace(const std::string& id, sf::Vector2f size, float radius) {
    g_current_vm->ui_state.lastComponentId = id;
}

static sf::Color hexToColor(std::string hex) {
    if (hex[0] == '#') hex.erase(0, 1);
    if (hex.length() == 8) {
        uint32_t value = std::stoul(hex, nullptr, 16);
        return sf::Color((value >> 24) & 0xFF, (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
    }
    if (hex.length() != 6) return sf::Color::White;
    uint32_t value = std::stoul(hex, nullptr, 16);
    return sf::Color((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, 255);
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




















// Advanced & Layouts







// Controls










// Specialized










static void compute_sizes(std::shared_ptr<UINode> node) {
    if (!node) return;
    
    for (auto& child : node->children) {
        compute_sizes(child);
    }
    
    bool isContainer = (node->type == UINodeType::Container || node->type == UINodeType::Window || node->type == UINodeType::StackPanel || node->type == UINodeType::Border || node->type == UINodeType::Grid || node->type == UINodeType::WrapPanel || node->type == UINodeType::DockPanel || node->type == UINodeType::ScrollView || node->type == UINodeType::Canvas);
    if (isContainer) {
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
    
    bool isContainer = (node->type == UINodeType::Container || node->type == UINodeType::Window || node->type == UINodeType::StackPanel || node->type == UINodeType::Border || node->type == UINodeType::Grid || node->type == UINodeType::WrapPanel || node->type == UINodeType::DockPanel || node->type == UINodeType::ScrollView || node->type == UINodeType::Canvas);
    if (isContainer) {
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
        } else if (node->type == UINodeType::Button || node->type == UINodeType::Checkbox || node->type == UINodeType::RadioBox || node->type == UINodeType::ToggleSwitch || node->type == UINodeType::Slider || node->type == UINodeType::MenuItem) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - g_current_vm->ui_state.lastClickTime);
            if (elapsed.count() > g_current_vm->ui_state.debounceTime) {
                g_current_vm->ui_state.lastClickTime = now;
                g_current_vm->ui_state.clickState[node->id] = true;
                if (node->type == UINodeType::Checkbox || node->type == UINodeType::RadioBox || node->type == UINodeType::ToggleSwitch) {
                    g_current_vm->ui_state.toggleStates[node->id] = !g_current_vm->ui_state.toggleStates[node->id];
                }
                g_current_vm->ui_state.focusedInputId = "";
                g_current_vm->ui_state.activeMenu = ""; // Close menu on option click
            }
        } else if (node->type == UINodeType::Hyperlink) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - g_current_vm->ui_state.lastClickTime);
            if (elapsed.count() > g_current_vm->ui_state.debounceTime) {
                g_current_vm->ui_state.lastClickTime = now;
                g_current_vm->ui_state.clickState[node->id] = true;
                if (!node->href.empty()) {
                    #if defined(_WIN32)
                        system(("start \"\" \"" + node->href + "\"").c_str());
                    #elif defined(__APPLE__)
                        system(("open " + node->href).c_str());
                    #else
                        system(("xdg-open " + node->href).c_str());
                    #endif
                }
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
    if (!node->customColor.empty()) {
        s.bgColor = hexToColor(node->customColor);
        s.textColor = s.bgColor;
    }
    if (node->fontSize > 0) s.fontSize = node->fontSize;
    if (s.fontSize <= 0) s.fontSize = 18;
    if (!node->fontAlias.empty()) s.fontAlias = node->fontAlias;
    
    bool hovered = g_current_vm->ui_state.hoverState[node->id];
    
    if (node->type == UINodeType::Container) {
        if (s.bgColor.a > 0 || s.borderThickness > 0) {
            draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {node->width, node->height}, 
                              s.borderRadius, s.bgColor, s.borderColor, s.borderThickness);
        }
    }
    else if (node->type == UINodeType::Button || node->type == UINodeType::Menu || node->type == UINodeType::MenuItem) {
        sf::Color btnBg = s.bgColor.a == 0 ? s.accentColor : s.bgColor;
        if (hovered) {
            btnBg = sf::Color(std::min(btnBg.r + 30, 255), std::min(btnBg.g + 30, 255), std::min(btnBg.b + 30, 255), btnBg.a);
        }
        draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {node->width, node->height}, 
                          s.borderRadius, btnBg, s.borderColor, s.borderThickness);
        
        if (!node->label.empty()) {
            std::string finalAlias = s.fontAlias;
            if (g_current_vm->ui_state.fontStack.find(finalAlias) == g_current_vm->ui_state.fontStack.end()) finalAlias = "default";
            sf::Font& font = g_current_vm->ui_state.fontStack[finalAlias];
            
            unsigned int actualSize = s.fontSize;
            if (node->width > 0) {
                float maxTextWidth = node->width - (s.padding * 2);
                if (maxTextWidth > 0) {
                    while (actualSize > 8) {
                        sf::Text dummy(font, node->label, actualSize);
                        if (dummy.getLocalBounds().size.x <= maxTextWidth) break;
                        actualSize--;
                    }
                }
            }
            
            sf::Text dummyText(font, node->label, actualSize);
            float tw = dummyText.getLocalBounds().size.x;
            
            float textX = node->x + s.padding;
            if (node->align == "center") {
                textX = node->x + (node->width / 2.0f) - (tw / 2.0f);
            } else if (node->align == "right") {
                textX = node->x + node->width - tw - s.padding;
            } else {
                textX = node->x + (node->width / 2.0f) - (tw / 2.0f); // Default to center for buttons
            }
            
            sf::Text txt(font, node->label, actualSize);
            txt.setFillColor(sf::Color::White); // Buttons look best with white text on accent background
            txt.setPosition({textX, node->y + (node->height / 2.0f) - (actualSize / 2.0f) - 2.0f});
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
        float size = 20.0f;
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
            g_current_vm->ui_state.sliderValues[node->id] = node->value;
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
    else if (node->type == UINodeType::ProgressBar) {
        float width = node->width > 0 ? node->width : 200.0f;
        float height = node->height > 0 ? node->height : 15.0f;
        sf::Color trackBg = sf::Color(80, 80, 80, 200);
        draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {width, height}, height / 2.0f, trackBg, sf::Color::Transparent, 0.0f);
        float p = std::clamp(node->progress, 0.0f, 100.0f) / 100.0f;
        if (p > 0.0f) {
            draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {std::max(height, width * p), height}, height / 2.0f, s.accentColor, sf::Color::Transparent, 0.0f);
        }
    }
    else if (node->type == UINodeType::ToggleSwitch) {
        float width = node->width > 0 ? node->width : 40.0f;
        float height = node->height > 0 ? node->height : 20.0f;
        sf::Color bg = node->checked ? s.accentColor : sf::Color(100, 100, 100, 200);
        draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {width, height}, height / 2.0f, bg, sf::Color::Transparent, 0.0f);
        
        float circleRadius = (height / 2.0f) - 2.0f;
        float cx = node->checked ? (node->x + width - circleRadius * 2.0f - 2.0f) : (node->x + 2.0f);
        sf::CircleShape handle(circleRadius);
        handle.setPosition({cx, node->y + 2.0f});
        handle.setFillColor(sf::Color::White);
        
        sf::CircleShape shadow(circleRadius);
        shadow.setPosition({cx, node->y + 3.0f});
        shadow.setFillColor(sf::Color(0, 0, 0, 80));
        g_current_vm->sfml_window->draw(shadow);
        g_current_vm->sfml_window->draw(handle);
    }
    else if (node->type == UINodeType::RadioBox) {
        float size = 20.0f;
        sf::Vector2f pos(node->x, node->y);
        sf::CircleShape outer(size / 2.0f);
        outer.setPosition(pos);
        outer.setFillColor(hovered ? sf::Color(255, 255, 255, 20) : sf::Color::Transparent);
        outer.setOutlineColor(node->checked ? s.accentColor : sf::Color(150, 150, 150));
        outer.setOutlineThickness(2.0f);
        g_current_vm->sfml_window->draw(outer);
        if (node->checked) {
            sf::CircleShape inner(size / 4.0f);
            inner.setPosition({pos.x + size / 4.0f, pos.y + size / 4.0f});
            inner.setFillColor(s.accentColor);
            g_current_vm->sfml_window->draw(inner);
        }
        if (!node->label.empty()) {
            sapphire_render_text(*g_current_vm->sfml_window, node->label, {pos.x + size + 10.0f, pos.y + (size / 2.0f) - (s.fontSize / 2.0f) - 2.0f}, s.textColor, s.fontAlias, s.fontSize);
        }
    }
    else if (node->type == UINodeType::Hyperlink) {
        sf::Color linkColor = hovered ? sf::Color(100, 180, 255) : s.accentColor;
        sapphire_render_text(*g_current_vm->sfml_window, node->label, {node->x, node->y}, linkColor, s.fontAlias, s.fontSize);
        if (hovered) {
            std::string finalAlias = s.fontAlias;
            if (g_current_vm->ui_state.fontStack.find(finalAlias) == g_current_vm->ui_state.fontStack.end()) finalAlias = "default";
            sf::Text dummyText(g_current_vm->ui_state.fontStack[finalAlias], node->label, s.fontSize > 0 ? s.fontSize : 18);
            float tw = dummyText.getLocalBounds().size.x;
            sf::RectangleShape line({tw, 1.0f});
            line.setPosition({node->x, node->y + (s.fontSize > 0 ? s.fontSize : 18) + 2.0f});
            line.setFillColor(linkColor);
            g_current_vm->sfml_window->draw(line);
        }
    }
    // Generic blocks for layout panels
    else if (node->type == UINodeType::Grid || node->type == UINodeType::StackPanel || node->type == UINodeType::DockPanel || node->type == UINodeType::WrapPanel || node->type == UINodeType::Border || node->type == UINodeType::Canvas || node->type == UINodeType::Window) {
        if (s.bgColor.a > 0 || s.borderThickness > 0) {
            draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {node->width, node->height}, 
                              s.borderRadius, s.bgColor, s.borderColor, s.borderThickness);
        }
    }
    
    // We update scale and rotation by using SFML transforms directly if needed, but since we use manual drawing, 
    // it's complex for generic containers. For an MVP animation, we just change X, Y, Width, Height, Opacity.
    
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
    std::string typeStr = static_cast<ObjString*>(typeVal.as.obj)->chars;
    
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
    else if (typeStr == "Grid") type = UINodeType::Grid;
    else if (typeStr == "StackPanel") type = UINodeType::StackPanel;
    else if (typeStr == "DockPanel") type = UINodeType::DockPanel;
    else if (typeStr == "WrapPanel") type = UINodeType::WrapPanel;
    else if (typeStr == "ScrollView") type = UINodeType::ScrollView;
    else if (typeStr == "Border") type = UINodeType::Border;
    else if (typeStr == "Image") type = UINodeType::Image;
    else if (typeStr == "ProgressBar") type = UINodeType::ProgressBar;
    else if (typeStr == "RadioBox") type = UINodeType::RadioBox;
    else if (typeStr == "ToggleSwitch") type = UINodeType::ToggleSwitch;
    else if (typeStr == "ComboBox") type = UINodeType::ComboBox;
    else if (typeStr == "ListBox") type = UINodeType::ListBox;
    else if (typeStr == "PasswordBox") type = UINodeType::PasswordBox;
    else if (typeStr == "Hyperlink") type = UINodeType::Hyperlink;
    else if (typeStr == "Expander") type = UINodeType::Expander;
    else if (typeStr == "DataGrid") type = UINodeType::DataGrid;
    else if (typeStr == "Canvas") type = UINodeType::Canvas;
    else if (typeStr == "Tooltip") type = UINodeType::Tooltip;
    else if (typeStr == "Popup") type = UINodeType::Popup;
    else if (typeStr == "Window") type = UINodeType::Window;
    
    std::string id = typeStr + "_" + std::to_string(counter++);
    if (nodeDict->fields.count("id") && is_obj_type(nodeDict->fields["id"], OBJ_STRING)) {
        id = static_cast<ObjString*>(nodeDict->fields["id"].as.obj)->chars;
    }
    
#ifdef DEBUG_PRINT_CODE
    std::cout << "[DEBUG build] type=" << typeStr << " id=" << id;
    if (nodeDict->fields.count("text")) {
        std::cout << " text_present=true";
        if (is_obj_type(nodeDict->fields["text"], OBJ_STRING)) {
            std::cout << " text_val=" << static_cast<ObjString*>(nodeDict->fields["text"].as.obj)->chars;
        }
    }
    if (nodeDict->fields.count("label")) {
        std::cout << " label_present=true";
        if (is_obj_type(nodeDict->fields["label"], OBJ_STRING)) {
            std::cout << " label_val=" << static_cast<ObjString*>(nodeDict->fields["label"].as.obj)->chars;
        }
    }
    std::cout << std::endl;
#endif

    auto node = std::make_shared<UINode>(type, id);
    
    auto get_str = [&](const std::string& key, std::string& out) {
        if (nodeDict->fields.count(key) && is_obj_type(nodeDict->fields[key], OBJ_STRING)) {
            out = static_cast<ObjString*>(nodeDict->fields[key].as.obj)->chars;
        }
    };
    auto get_num = [&](const std::string& key, float& out) {
        if (nodeDict->fields.count(key) && nodeDict->fields[key].type == ValType::VAL_NUMBER) {
            out = (float)nodeDict->fields[key].as.number;
        }
    };
    
    auto get_bool = [&](const std::string& key, bool& out) {
        if (nodeDict->fields.count(key) && nodeDict->fields[key].type == ValType::VAL_BOOL) {
            out = nodeDict->fields[key].as.boolean;
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
    if (type == UINodeType::Checkbox || type == UINodeType::RadioBox || type == UINodeType::ToggleSwitch) {
        if (g_current_vm->ui_state.toggleStates.find(id) != g_current_vm->ui_state.toggleStates.end()) {
            node->checked = g_current_vm->ui_state.toggleStates[id];
        } else {
            g_current_vm->ui_state.toggleStates[id] = node->checked;
        }
    }
    else if (type == UINodeType::Slider) {
        if (g_current_vm->ui_state.sliderValues.find(id) != g_current_vm->ui_state.sliderValues.end()) {
            node->value = g_current_vm->ui_state.sliderValues[id];
        } else {
            g_current_vm->ui_state.sliderValues[id] = node->value;
        }
    }
    get_bool("shadow", node->shadow);
    get_num("thickness", node->thickness);
    get_num("margin", node->margin);
    get_str("color", node->customColor);
    if (node->customColor.empty()) {
        get_str("customColor", node->customColor);
    }
    get_str("src", node->src);
    get_num("progress", node->progress);
    get_str("href", node->href);
    get_bool("isPassword", node->isPassword);
    get_bool("expanded", node->expanded);
    get_num("opacity", node->opacity);
    get_num("scaleX", node->scaleX);
    get_num("scaleY", node->scaleY);
    get_num("rotation", node->rotation);
    
    float vrow = 0; get_num("row", vrow); node->row = (int)vrow;
    float vcol = 0; get_num("column", vcol); node->column = (int)vcol;
    float vrowS = 0; get_num("rowSpan", vrowS); if (vrowS > 0) node->rowSpan = (int)vrowS;
    float vcolS = 0; get_num("columnSpan", vcolS); if (vcolS > 0) node->columnSpan = (int)vcolS;
    
    get_num("left", node->left);
    get_num("top", node->top);
    get_num("right", node->right);
    get_num("bottom", node->bottom);

    if (nodeDict->fields.count("options") && is_obj_type(nodeDict->fields["options"], OBJ_ARRAY)) {
        auto arr = static_cast<ObjArray*>(nodeDict->fields["options"].as.obj);
        for (auto& val : arr->elements) {
            if (is_obj_type(val, OBJ_STRING)) {
                node->options.push_back(static_cast<ObjString*>(val.as.obj)->chars);
            }
        }
    }
    
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
                // Value changed externally â€” accept it and move cursor to end
                g_current_vm->ui_state.cursorPositions[id] = scriptVal.size();
            }
            g_current_vm->ui_state.inputTexts[id] = scriptVal;
        }
        // When focused, C++ owns the state â€” ignore text= from script completely.
        node->label = g_current_vm->ui_state.inputTexts[id];
    }
    
    if (nodeDict->fields.count("children")) {
        auto childrenVal = nodeDict->fields["children"];
        if (is_obj_type(childrenVal, OBJ_ARRAY)) {
            auto arr = static_cast<ObjArray*>(childrenVal.as.obj);
            for (auto& childVal : arr->elements) {
                if (is_obj_type(childVal, OBJ_INSTANCE)) {
                    auto childNode = build_ui_tree(static_cast<ObjInstance*>(childVal.as.obj), counter);
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
        if (type == UINodeType::Button || type == UINodeType::Menu || type == UINodeType::MenuItem) {
            node->width = 120.0f;
            if (!node->label.empty()) {
                std::string finalAlias = node->fontAlias.empty() ? "default" : node->fontAlias;
                if (!g_current_vm->ui_state.fontStack.count(finalAlias)) finalAlias = "default";
                if (g_current_vm->ui_state.fontStack.count(finalAlias)) {
                    sf::Text dummyText(g_current_vm->ui_state.fontStack[finalAlias], node->label, node->fontSize > 0 ? node->fontSize : 18);
                    float textW = dummyText.getLocalBounds().size.x + 30.0f; // Add horizontal padding
                    if (textW > node->width) node->width = textW;
                }
            }
        }
        else if (type == UINodeType::Checkbox || type == UINodeType::RadioBox) {
            node->width = 20.0f;
            if (!node->label.empty()) {
                std::string finalAlias = node->fontAlias.empty() ? "default" : node->fontAlias;
                if (!g_current_vm->ui_state.fontStack.count(finalAlias)) finalAlias = "default";
                if (g_current_vm->ui_state.fontStack.count(finalAlias)) {
                    sf::Text dummyText(g_current_vm->ui_state.fontStack[finalAlias], node->label, node->fontSize > 0 ? node->fontSize : 18);
                    node->width += dummyText.getLocalBounds().size.x + 15.0f;
                }
            }
        }
        else if (type == UINodeType::Slider || type == UINodeType::ProgressBar) node->width = 200.0f;
        else if (type == UINodeType::Input) node->width = 250.0f;
        else if (type == UINodeType::Display) node->width = 200.0f;
        else if (type == UINodeType::Separator) node->width = 100.0f;
        else if (type == UINodeType::ToggleSwitch) node->width = 40.0f;
        else if (type == UINodeType::Text || type == UINodeType::Hyperlink) {
            std::string finalAlias = node->fontAlias.empty() ? "default" : node->fontAlias;
            if (g_current_vm->ui_state.fontStack.find(finalAlias) == g_current_vm->ui_state.fontStack.end()) finalAlias = "default";
            unsigned int fsize = node->fontSize > 0 ? node->fontSize : 18;
            if (g_current_vm->ui_state.fontStack.count(finalAlias)) {
                sf::Text dummyText(g_current_vm->ui_state.fontStack[finalAlias], node->label, fsize);
                node->width = dummyText.getLocalBounds().size.x;
            } else {
                node->width = node->label.length() * (fsize * 0.6f);
            }
        }
    }
    if (node->height <= 0.0f) {
        if (type == UINodeType::Button || type == UINodeType::Menu || type == UINodeType::MenuItem) node->height = 40.0f;
        else if (type == UINodeType::Checkbox || type == UINodeType::RadioBox) node->height = 20.0f;
        else if (type == UINodeType::Slider) node->height = 20.0f;
        else if (type == UINodeType::ProgressBar) node->height = 15.0f;
        else if (type == UINodeType::Input) node->height = 35.0f;
        else if (type == UINodeType::Display) node->height = 50.0f;
        else if (type == UINodeType::Separator) node->height = 2.0f;
        else if (type == UINodeType::ToggleSwitch) node->height = 20.0f;
        else if (type == UINodeType::Text || type == UINodeType::Hyperlink) {
            unsigned int fsize = node->fontSize > 0 ? node->fontSize : 18;
            node->height = (float)fsize + 4.0f;
        }
    }

    return node;
}

static float lerp_val(float a, float b, float t) { return a + (b - a) * t; }
static sf::Color lerp_color(sf::Color a, sf::Color b, float t) {
    return sf::Color(
        (uint8_t)lerp_val(a.r, b.r, t),
        (uint8_t)lerp_val(a.g, b.g, t),
        (uint8_t)lerp_val(a.b, b.b, t),
        (uint8_t)lerp_val(a.a, b.a, t)
    );
}

static void apply_animations_to_tree(std::shared_ptr<UINode> node, float dt) {
    if (!node) return;
    
    auto it = g_current_vm->ui_state.activeAnimations.find(node->id);
    if (it != g_current_vm->ui_state.activeAnimations.end()) {
        auto& aa = it->second;
        auto animIt = g_current_vm->ui_state.animations.find(aa.animId);
        if (animIt != g_current_vm->ui_state.animations.end()) {
            auto& anim = animIt->second;
            aa.elapsedTime += dt;
            float t = anim.duration > 0 ? (aa.elapsedTime / anim.duration) : 1.0f;
            if (t > 1.0f) {
                if (anim.loop) { aa.elapsedTime = std::fmod(aa.elapsedTime, anim.duration); t = aa.elapsedTime / anim.duration; }
                else t = 1.0f;
            }
            std::cout << "Anim: id=" << node->id << " dt=" << dt << " elapsed=" << aa.elapsedTime << " t=" << t << " width=" << node->width << std::endl;
            
            if (anim.keyframes.size() >= 2) {
                size_t kfIndex = 0;
                for (size_t i = 0; i < anim.keyframes.size() - 1; i++) {
                    if (t >= anim.keyframes[i].timeOffset && t <= anim.keyframes[i+1].timeOffset) {
                        kfIndex = i; break;
                    }
                }
                auto& kf1 = anim.keyframes[kfIndex];
                auto& kf2 = anim.keyframes[kfIndex+1];
                float timeSpan = kf2.timeOffset - kf1.timeOffset;
                float localT = timeSpan > 0 ? ((t - kf1.timeOffset) / timeSpan) : 0.0f;
                
                for (auto& [prop, val1] : kf1.numericProps) {
                    if (kf2.numericProps.count(prop)) {
                        float val2 = kf2.numericProps.at(prop);
                        float interpolated = lerp_val(val1, val2, localT);
                        if (prop == "width") node->width = interpolated;
                        else if (prop == "height") node->height = interpolated;
                        else if (prop == "x") node->x = interpolated;
                        else if (prop == "y") node->y = interpolated;
                        else if (prop == "opacity") node->opacity = interpolated;
                        else if (prop == "scaleX") node->scaleX = interpolated;
                        else if (prop == "scaleY") node->scaleY = interpolated;
                        else if (prop == "rotation") node->rotation = interpolated;
                    }
                }
                for (auto& [prop, c1] : kf1.colorProps) {
                    if (kf2.colorProps.count(prop)) {
                        sf::Color c2 = kf2.colorProps.at(prop);
                        sf::Color interpolated = lerp_color(c1, c2, localT);
                        if (prop == "color") {
                            char buf[10];
                            snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", interpolated.r, interpolated.g, interpolated.b, interpolated.a);
                            node->customColor = buf;
                        }
                    }
                }
            }
        }
    }
    
    for (auto& child : node->children) {
        apply_animations_to_tree(child, dt);
    }
}



















static nlohmann::json convertSapphireToJson(SapphireValue val) {
    if (val.type == ValType::VAL_NUMBER) {
        return val.as.number;
    } else if (val.type == ValType::VAL_BOOL) {
        return val.as.boolean;
    } else if (val.type == ValType::VAL_NIL) {
        return nullptr;
    } else if (is_obj_type(val, OBJ_ARRAY)) {
        auto arr = static_cast<ObjArray*>(val.as.obj);
        nlohmann::json j = nlohmann::json::array();
        for (const auto& elem : arr->elements) {
            j.push_back(convertSapphireToJson(elem));
        }
        return j;
    } else if (val.type == ValType::VAL_OBJ) {
        Obj* obj = val.as.obj;
        if (obj->type == OBJ_STRING) {
            return static_cast<ObjString*>(obj)->chars;
        } else if (obj->type == OBJ_MAP) {
            ObjMap* map = static_cast<ObjMap*>(obj);
            nlohmann::json j = nlohmann::json::object();
            for (const auto& pair : map->items) {
                j[pair.first] = convertSapphireToJson(pair.second);
            }
            return j;
        } else if (obj->type == OBJ_INSTANCE) {
            ObjInstance* instance = static_cast<ObjInstance*>(obj);
            nlohmann::json j = nlohmann::json::object();
            for (const auto& pair : instance->fields) {
                j[pair.first] = convertSapphireToJson(pair.second);
            }
            return j;
        }
    }
    return nullptr;
}



static SapphireValue core_create_instance(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Core.createInstance() expects 1 string argument (class name)." << std::endl;
        }
        return {};
    }

    ObjString* class_name_obj = static_cast<ObjString*>(args[0].as.obj);
    std::string class_name = class_name_obj->chars;

    auto it = g_current_vm->globals.find(class_name);
    if (it == g_current_vm->globals.end()) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Class '" << class_name << "' not found." << std::endl;
        }
        return {};
    }

    SapphireValue class_value = it->second;

    if (class_value.type != ValType::VAL_OBJ || class_value.as.obj->type != OBJ_CLASS) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Global variable '" << class_name << "' is not a class." << std::endl;
        }
        return {};
    }

    ObjClass* klass = static_cast<ObjClass*>(class_value.as.obj);
    ObjInstance* instance = new_instance(g_current_vm, klass);
    return instance;
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

    catch_count = 0;
  
    define_native("clock", clock_native);
    define_native("assert", assert_native);
    define_native("parseDouble", native_string_to_double);
    define_native("valueToString", native_value_to_string);
    define_native("lruCreate", native_lru_create);
    define_native("lruHas", native_lru_has);
    define_native("lruGet", native_lru_get);
    define_native("lruPut", native_lru_put);
    define_native("evaluate", native_evaluate);
    define_native("len", native_len);
    define_native("stringCharAt", native_string_char_at);
    define_native("stringLength", native_string_length);
    define_native("stringSubstring", native_string_substring);
    define_native("stringSplit", native_string_split);
    define_native("stringReplace", native_string_replace);
    define_native("stringToUpper", native_string_to_upper);
    define_native("stringToLower", native_string_to_lower);
    define_native("stringTrim", native_string_trim);
    define_native("stringContains", native_string_contains);
    define_native("getQuote", native_get_quote);

    const char* appdata_path = getenv("APPDATA");
    if (appdata_path) {
        std::string global_plugins_path = std::string(appdata_path) + "\\Sapphire\\plugins";
        module_search_paths.push_back(global_plugins_path);
    }

    // --- IO ---
    define_native("readLine", io_readline_native);
    define_native("printColor", native_io_print_color);
    define_native("readInput", native_io_read_input);
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
    define_native("log", native_math_log);
    define_native("pow", native_math_pow);
    define_native("min", native_math_min);
    define_native("max", native_math_max);
    define_native("clamp", native_math_clamp);
    define_native("lerp", native_math_lerp);

    // --- JSON ---
    ObjString* json_name = new_string(this, "JSON");
    ObjClass* json_class = new_class(this, json_name);
    json_class->methods["parse"] = SapphireValue(new_native(this, native_json_parse));
    json_class->methods["stringify"] = SapphireValue(new_native(this, native_json_stringify));
    globals["JSON"] = SapphireValue(json_class);

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

    // --- Logger ---
    ObjString* logger_name = new_string(this, "Logger");
    ObjClass* logger_class = new_class(this, logger_name);
    logger_class->methods["info"] = SapphireValue(new_native(this, native_logger_info));
    logger_class->methods["warn"] = SapphireValue(new_native(this, native_logger_warn));
    logger_class->methods["error"] = SapphireValue(new_native(this, native_logger_error));
    logger_class->methods["debug"] = SapphireValue(new_native(this, native_logger_debug));
    globals["Logger"] = SapphireValue(logger_class);

    // --- System ---
    define_native("getEnv", native_system_get_env);
    define_native("getOS", native_system_get_os);
    define_native("sleep", native_system_sleep);
    define_native("getClipboard", native_system_get_clipboard);
    define_native("exec", native_system_exec);
    define_native("spawn", native_spawn);
    define_native("join", native_join);
    define_native("getCoreCount", native_system_core_count);

    // --- Threading / Mutex ---
    ObjString* mutex_name = new_string(this, "Mutex");
    ObjClass* mutex_class = new_class(this, mutex_name);
    mutex_class->methods["new"] = SapphireValue(new_native(this, native_mutex_new));
    mutex_class->methods["lock"] = SapphireValue(new_native(this, native_mutex_lock));
    mutex_class->methods["unlock"] = SapphireValue(new_native(this, native_mutex_unlock));
    globals["Mutex"] = SapphireValue(mutex_class);

    // --- OpenCL ---
    define_opencl_natives(this);

    // --- HTTP ---
    define_native("httpGet", native_http_get);
    define_native("httpPost", native_http_post);
    define_native("httpPing", native_http_ping);
    define_native("httpDownload", native_http_download);
    define_native("httpServer", native_http_serve);

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
    define_sqlite_natives(this);

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
        
        // Advanced & Layouts
        define_native("Grid", native_ui_grid);
        define_native("StackPanel", native_ui_stackpanel);
        define_native("DockPanel", native_ui_dockpanel);
        define_native("WrapPanel", native_ui_wrappanel);
        define_native("ScrollView", native_ui_scrollview);
        define_native("Border", native_ui_border);
        
        // Controls
        define_native("Image", native_ui_image);
        define_native("ProgressBar", native_ui_progressbar);
        define_native("RadioBox", native_ui_radiobox);
        define_native("ToggleSwitch", native_ui_toggleswitch);
        define_native("ComboBox", native_ui_combobox);
        define_native("ListBox", native_ui_listbox);
        define_native("PasswordBox", native_ui_passwordbox);
        define_native("Hyperlink", native_ui_hyperlink);
        define_native("Expander", native_ui_expander);
        
        // Specialized
        define_native("DataGrid", native_ui_datagrid);
        define_native("Canvas", native_ui_canvas);
        define_native("Tooltip", native_ui_tooltip);
        define_native("Popup", native_ui_popup);
        define_native("Window", native_ui_window);
        
        // Animations
        define_native("Animate", native_ui_animate);

        // === NEW v1.0.9: Componentes avançados ===
        define_native("Card",    native_ui_card);
        define_native("Badge",   native_ui_badge);
        define_native("Tag",     native_ui_tag);
        define_native("Stepper", native_ui_stepper);
        define_native("Spinner", native_ui_spinner);
        define_native("Notify",  native_ui_notify);

        globals["_ui_initialized"] = {};
        globals["APP_WINDOW_WIDTH"] = (double)config.windowWidth;
        globals["APP_WINDOW_HEIGHT"] = (double)config.windowHeight;
    }
}
void VM::add_module_search_path(const std::string& path) {
    module_search_paths.push_back(path);
}

static std::string get_custom_entry_point(const std::string& base_dir) {
    // 1. Check PLUGIN.txt
    std::string plugin_txt_path = base_dir + "/PLUGIN.txt";
    std::ifstream infile(plugin_txt_path);
    if (infile.good()) {
        std::string line;
        while (std::getline(infile, line)) {
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            std::string trimmed = line.substr(start);
            if (trimmed.rfind("main:", 0) == 0) {
                std::string val = trimmed.substr(5);
                size_t vstart = val.find_first_not_of(" \t\r\n");
                if (vstart != std::string::npos) {
                    std::string entry = val.substr(vstart);
                    while (!entry.empty() && (entry.back() == '\r' || entry.back() == '\n' || entry.back() == ' ')) {
                        entry.pop_back();
                    }
                    infile.close();
                    return entry;
                }
            }
            if (trimmed.rfind("entry:", 0) == 0) {
                std::string val = trimmed.substr(6);
                size_t vstart = val.find_first_not_of(" \t\r\n");
                if (vstart != std::string::npos) {
                    std::string entry = val.substr(vstart);
                    while (!entry.empty() && (entry.back() == '\r' || entry.back() == '\n' || entry.back() == ' ')) {
                        entry.pop_back();
                    }
                    infile.close();
                    return entry;
                }
            }
        }
    }
    infile.close();

    // 2. Check sapphire.json
    std::string json_path = base_dir + "/sapphire.json";
    std::ifstream json_file(json_path);
    if (json_file.good()) {
        try {
            nlohmann::json j;
            json_file >> j;
            json_file.close();
            if (j.contains("main") && j["main"].is_string()) {
                return j["main"].get<std::string>();
            }
            if (j.contains("entry") && j["entry"].is_string()) {
                return j["entry"].get<std::string>();
            }
        } catch(...) {}
    }
    json_file.close();

    // Default entry point
    return "files/main.sp";
}

std::string VM::find_and_load_module(const std::string& module_name, std::string& out_resolved_path) {
    std::string target_name = module_name;
    
    // Check for explicit import scope prefix
    bool force_local = false;
    bool force_global = false;
    bool direct_path = false;
    std::string explicit_path = "";
    
    if (target_name.rfind("local:", 0) == 0) {
        force_local = true;
        target_name = target_name.substr(6);
    } else if (target_name.rfind("global:", 0) == 0) {
        force_global = true;
        target_name = target_name.substr(7);
    } else if (target_name.rfind("path:", 0) == 0) {
        direct_path = true;
        explicit_path = target_name.substr(5);
    }

    if (direct_path) {
        std::string entry = get_custom_entry_point(explicit_path);
        std::string full_path = explicit_path + "/" + entry;
        std::string content = load_file_as_string(full_path);
        if (!content.empty()) {
            try {
                out_resolved_path = std::filesystem::absolute(full_path).string();
            } catch(...) {
                out_resolved_path = full_path;
            }
            return content;
        }
        return "";
    }

    // Check if this is a plugin import (format: plugin@version or plugin@latest)
    size_t at_pos = target_name.find('@');
    if (at_pos != std::string::npos || force_local || force_global) {
        std::string plugin_name = target_name;
        std::string version = "latest";
        if (at_pos != std::string::npos) {
            plugin_name = target_name.substr(0, at_pos);
            version = target_name.substr(at_pos + 1);
        }
        
        // Define base paths to try based on options
        std::vector<std::string> base_dirs;
        
        const char* appdata_path = getenv("APPDATA");
        
        // If not forced local, global (APPDATA) is preferred search path
        if (!force_local) {
            if (appdata_path != nullptr) {
                base_dirs.push_back(
                    std::string(appdata_path) + "\\Sapphire\\plugins\\" + plugin_name +
                    "\\versions\\v" + version);
            }
        }
        
        // If not forced global, check local project-level paths
        if (!force_global) {
            base_dirs.push_back("plugins/" + plugin_name + "/versions/v" + version);
            base_dirs.push_back("../plugins/" + plugin_name + "/versions/v" + version);
            base_dirs.push_back("../../plugins/" + plugin_name + "/versions/v" + version);
            base_dirs.push_back("../" + plugin_name + "/versions/v" + version);
            base_dirs.push_back(plugin_name + "/versions/v" + version);
            if (plugin_name == "vividry" || plugin_name == "Vividry") {
                base_dirs.push_back("Vividry/versions/v" + version);
            }
        }
        
        // Try to load
        for (const auto& base_dir : base_dirs) {
            std::string entry = get_custom_entry_point(base_dir);
            std::string full_path = base_dir + "/" + entry;
            std::string content = load_file_as_string(full_path);
            if (!content.empty()) {
                try {
                    out_resolved_path = std::filesystem::absolute(full_path).string();
                } catch(...) {
                    out_resolved_path = full_path;
                }
                return content;
            }
        }
        
        // Fallback for "latest" version if not found
        if (version == "latest") {
            std::vector<std::string> latest_base_dirs;
            if (!force_local) {
                if (appdata_path != nullptr) {
                    latest_base_dirs.push_back(
                        std::string(appdata_path) + "\\Sapphire\\plugins\\" + plugin_name +
                        "\\versions\\v1.0.0");
                }
            }
            if (!force_global) {
                latest_base_dirs.push_back("plugins/" + plugin_name + "/versions/v1.0.0");
                latest_base_dirs.push_back("../plugins/" + plugin_name + "/versions/v1.0.0");
                latest_base_dirs.push_back("../../plugins/" + plugin_name + "/versions/v1.0.0");
                latest_base_dirs.push_back("../" + plugin_name + "/versions/v1.0.0");
                latest_base_dirs.push_back(plugin_name + "/versions/v1.0.0");
                if (plugin_name == "vividry" || plugin_name == "Vividry") {
                    latest_base_dirs.push_back("Vividry/versions/v1.0.0");
                }
            }
            
            for (const auto& base_dir : latest_base_dirs) {
                std::string entry = get_custom_entry_point(base_dir);
                std::string full_path = base_dir + "/" + entry;
                std::string content = load_file_as_string(full_path);
                if (!content.empty()) {
                    try {
                        out_resolved_path = std::filesystem::absolute(full_path).string();
                    } catch(...) {
                        out_resolved_path = full_path;
                    }
                    return content;
                }
            }
        }
        
        return "";
    }
    
    // Traditional file path import
    std::string content = load_file_as_string(module_name);
    if (!content.empty()) {
        try {
            out_resolved_path = std::filesystem::absolute(module_name).string();
        } catch(...) {
            out_resolved_path = module_name;
        }
        return content;
    }

    for (const std::string& base_path : module_search_paths) {
        std::string path_direct = base_path + "\\" + module_name;
        content = load_file_as_string(path_direct);
        if (!content.empty()) {
            try { out_resolved_path = std::filesystem::absolute(path_direct).string(); } catch(...) { out_resolved_path = path_direct; }
            return content;
        }

        std::string path_sp = base_path + "\\" + module_name + ".sp";
        content = load_file_as_string(path_sp);
        if (!content.empty()) {
            try { out_resolved_path = std::filesystem::absolute(path_sp).string(); } catch(...) { out_resolved_path = path_sp; }
            return content;
        }

        std::string path_main_sp = base_path + "\\" + module_name + "\\main.sp";
        content = load_file_as_string(path_main_sp);
        if (!content.empty()) {
            try { out_resolved_path = std::filesystem::absolute(path_main_sp).string(); } catch(...) { out_resolved_path = path_main_sp; }
            return content;
        }
    }

    out_resolved_path = "";
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

    if (function->is_async) {
        ObjPromise* promise = new_promise(this);
        promise->function = function;
        for (int i = 0; i < arg_count; i++) {
            promise->args.push_back(stack_top[-arg_count + i]);
        }
        stack_top -= arg_count + 1; // pop args and function
        push(SapphireValue((Obj*)promise));
        event_loop_queue.push_back(promise);
        return true;
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
    bool is_callable = callee.type == ValType::VAL_OBJ &&
                      (callee.as.obj->type == OBJ_CLOSURE ||
                       callee.as.obj->type == OBJ_NATIVE ||
                       callee.as.obj->type == OBJ_CLASS ||
                       callee.as.obj->type == OBJ_BOUND_METHOD ||
                       callee.as.obj->type == OBJ_FUNCTION);

    if (!is_callable) {
        for (int i = 1; i <= 4; i++) {
            SapphireValue potential = stack_top[-arg_count - 1 - i];
            if (potential.type == ValType::VAL_OBJ) {
                Obj* o = potential.as.obj;
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
        Obj* obj = callee.as.obj;
        switch (obj->type) {
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = (ObjBoundMethod*)obj;
                stack_top[-arg_count - 1] = bound->receiver;
                return call_value(bound->method, arg_count);
            }
            case OBJ_CLASS: {
                if (arg_count != 0) {
                    if (!this->soft_mode) std::cerr << "Runtime Error: Expected 0 arguments but got " << arg_count << "." << std::endl;
                    return false;
                }
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
        try {
            std::cerr << "  Callee type: " << get_value_type_name(callee) << "  Value: ";
            print_value(callee);
            std::cerr << std::endl;
        } catch (...) {}

        // Dump a small window of the stack around the call site for diagnosis
        std::cerr << "  Stack (top-most last):\n";
        int max_dump = 12;
        int available = static_cast<int>(stack_top - stack);
        int start = std::max(0, available - max_dump);
        for (int i = start; i < available; ++i) {
            std::cerr << "    [" << i << "] ";
            try { print_value(stack[i]); } catch (...) { std::cerr << "<err>"; }
            std::cerr << std::endl;
        }
    }
    return false;
}

// C++ Fallback function to execute a single opcode if not inlined
extern "C" JIT_ABI void jit_fallback_opcode(VM* vm, uint8_t** ip_ptr) {
    uint8_t instruction = **ip_ptr;
    fprintf(stderr, "JIT Fallback hit for opcode %d at %p. Not implemented.\n", instruction, *ip_ptr);
    exit(1);
}

// Trampolines (Direct C-Callouts) - Implemented with full functionality
extern "C" JIT_ABI void jit_trampoline_import(VM* vm) {
    // Import module - simplified implementation
    if (vm->stack_top - vm->stack < 1) return;
    SapphireValue module_name = vm->stack_top[-1];
    vm->stack_top--;
    
    if (!is_obj_type(module_name, OBJ_STRING)) {
        vm->push(SapphireValue());
        return;
    }
    
    // For now, just push nil - full import system would need module loading
    vm->push(SapphireValue());
}

extern "C" JIT_ABI void jit_trampoline_spawn(VM* vm) {
    // Spawn thread for async execution
    if (vm->stack_top - vm->stack < 1) return;
    SapphireValue script_path = vm->stack_top[-1];
    vm->stack_top--;
    
    if (!is_obj_type(script_path, OBJ_STRING)) {
        vm->push(SapphireValue(0.0));
        return;
    }
    
    // Simplified spawn - return thread ID
    static int next_thread_id = 1;
    vm->push(SapphireValue((double)next_thread_id++));
}

extern "C" JIT_ABI void jit_trampoline_await(VM* vm) {
    // Await promise/result
    if (vm->stack_top - vm->stack < 1) return;
    vm->stack_top--; // Pop the promise
    vm->push(SapphireValue()); // Return nil for now
}

extern "C" JIT_ABI void jit_trampoline_get_property(VM* vm) {
    // Get property from object
    if (vm->stack_top - vm->stack < 2) return;
    SapphireValue obj = vm->stack_top[-2];
    SapphireValue prop = vm->stack_top[-1];
    vm->stack_top -= 2;
    
    if (!is_obj_type(prop, OBJ_STRING)) {
        vm->push(SapphireValue());
        return;
    }
    
    std::string prop_name = static_cast<ObjString*>(prop.as.obj)->chars;
    
    if (is_obj_type(obj, OBJ_INSTANCE)) {
        ObjInstance* instance = static_cast<ObjInstance*>(obj.as.obj);
        if (instance->fields.count(prop_name)) {
            vm->push(instance->fields[prop_name]);
            return;
        }
    }
    
    vm->push(SapphireValue());
}

extern "C" JIT_ABI void jit_trampoline_set_property(VM* vm) {
    // Set property on object
    if (vm->stack_top - vm->stack < 3) return;
    SapphireValue obj = vm->stack_top[-3];
    SapphireValue prop = vm->stack_top[-2];
    SapphireValue value = vm->stack_top[-1];
    vm->stack_top -= 3;
    
    if (!is_obj_type(prop, OBJ_STRING) || !is_obj_type(obj, OBJ_INSTANCE)) {
        vm->push(SapphireValue());
        return;
    }
    
    std::string prop_name = static_cast<ObjString*>(prop.as.obj)->chars;
    ObjInstance* instance = static_cast<ObjInstance*>(obj.as.obj);
    instance->fields[prop_name] = value;
    vm->push(value);
}

extern "C" JIT_ABI void jit_trampoline_call(VM* vm, int arg_count) {
    // Call function - simplified implementation
    if (vm->stack_top - vm->stack < 1) return;
    // arg_count passed directly via ABI
    
    if (vm->stack_top - vm->stack < arg_count + 1) return;
    
    SapphireValue callee = vm->stack_top[-arg_count - 1];
    vm->stack_top -= arg_count + 1;
    
    // For now, just push nil - full call implementation needs closure handling
    vm->push(SapphireValue());
}

extern "C" JIT_ABI void jit_trampoline_get_global(VM* vm, const std::string* name_ptr) {
    std::string name = *name_ptr;
    if (vm->globals.count(name)) {
        vm->push(vm->globals[name]);
    } else {
        vm->push(SapphireValue());
    }
}

extern "C" JIT_ABI void jit_trampoline_define_global(VM* vm, const std::string* name_ptr) {
    std::string name = *name_ptr;
    vm->globals[name] = vm->stack_top[-1];
    vm->pop();
}

extern "C" JIT_ABI void jit_trampoline_set_global(VM* vm, const std::string* name_ptr) {
    std::string name = *name_ptr;
    if (vm->globals.count(name)) {
        vm->globals[name] = vm->stack_top[-1];
    }
    vm->pop();
}

extern "C" JIT_ABI void jit_trampoline_build_array(VM* vm, uint8_t count) {
    ObjArray* array = new_array(vm);
    for (int i = count - 1; i >= 0; i--) {
        array->elements.push_back(vm->stack_top[-i - 1]);
    }
    vm->stack_top -= count;
    vm->push(array);
}

extern "C" JIT_ABI void jit_trampoline_build_map(VM* vm, uint8_t count) {
    ObjMap* map = new_map(vm);
    for (int i = 0; i < count; i++) {
        SapphireValue value = vm->stack_top[-1];
        SapphireValue key = vm->stack_top[-2];
        if (is_obj_type(key, OBJ_STRING)) {
            std::string key_str = static_cast<ObjString*>(key.as.obj)->chars;
            map->items[key_str] = value;
        }
        vm->stack_top -= 2;
    }
    vm->push(map);
}

extern "C" JIT_ABI void jit_trampoline_closure(VM* vm, SapphireValue* constant_val_ptr) {
    SapphireValue constant_val = *constant_val_ptr;
    ObjFunction* func = static_cast<ObjFunction*>(constant_val.as.obj);
    
    ObjClosure* closure = new_closure(vm, func);
    vm->push(closure);
}

extern "C" JIT_ABI void jit_trampoline_make_named_arg(VM* vm, SapphireValue* constant_val_ptr) {
    SapphireValue constant_val = *constant_val_ptr;
    SapphireValue value = vm->stack_top[-1];
    vm->stack_top--;
    
    ObjNamedArg* narg = new_named_arg(vm, static_cast<ObjString*>(constant_val.as.obj), value);
    vm->push(narg);
}

extern "C" JIT_ABI void jit_trampoline_generic(VM* vm, int opcode) {
    // Generic fallback for unimplemented opcodes
    // This should never be called if all opcodes are implemented
    fprintf(stderr, "JIT Warning: Using generic fallback for opcode %d\n", opcode);
    
    // Execute the opcode using the interpreter
    CallFrame* frame = &vm->frames[vm->frame_count - 1];
    uint8_t* ip = frame->ip;
    
    switch (opcode) {
        case OP_JUMP: {
            int16_t jump = (int16_t)((ip[1] << 8) | ip[2]);
            frame->ip += jump;
            break;
        }
        case OP_JUMP_IF_FALSE: {
            int16_t jump = (int16_t)((ip[1] << 8) | ip[2]);
            if (is_falsey(vm->stack_top[-1])) {
                frame->ip += jump;
            } else {
                frame->ip += 3;
            }
            vm->pop();
            break;
        }
        case OP_JUMP_IF_NIL: {
            int16_t jump = (int16_t)((ip[1] << 8) | ip[2]);
            if (vm->stack_top[-1].type == ValType::VAL_NIL) {
                frame->ip += jump;
            } else {
                frame->ip += 3;
            }
            vm->pop();
            break;
        }
        case OP_JUMP_IF_NOT_NIL: {
            int16_t jump = (int16_t)((ip[1] << 8) | ip[2]);
            if (vm->stack_top[-1].type != ValType::VAL_NIL) {
                frame->ip += jump;
            } else {
                frame->ip += 3;
            }
            vm->pop();
            break;
        }
        case OP_LOOP: {
            int16_t jump = (int16_t)((ip[1] << 8) | ip[2]);
            frame->ip -= jump;
            break;
        }
        case OP_PRINT: {
            print_value(vm->stack_top[-1]);
            std::cout << std::endl;
            vm->pop();
            frame->ip++;
            break;
        }
        case OP_BUILD_ARRAY: {
            uint8_t count = ip[1];
            ObjArray* array = new_array(vm);
            for (int i = count - 1; i >= 0; i--) {
                array->elements.push_back(vm->stack_top[-i - 1]);
            }
            vm->stack_top -= count;
            vm->push(array);
            frame->ip += 2;
            break;
        }
        case OP_BUILD_MAP: {
            uint8_t count = ip[1];
            ObjMap* map = new_map(vm);
            for (int i = 0; i < count; i++) {
                SapphireValue value = vm->stack_top[-1];
                SapphireValue key = vm->stack_top[-2];
                if (is_obj_type(key, OBJ_STRING)) {
                    std::string key_str = static_cast<ObjString*>(key.as.obj)->chars;
                    map->items[key_str] = value;
                }
                vm->stack_top -= 2;
            }
            vm->push(map);
            frame->ip += 2;
            break;
        }
        case OP_GET_SUBSCRIPT: {
            SapphireValue index = vm->stack_top[-1];
            SapphireValue collection = vm->stack_top[-2];
            vm->stack_top -= 2;
            
            if (is_obj_type(collection, OBJ_ARRAY) && index.type == ValType::VAL_NUMBER) {
                ObjArray* array = static_cast<ObjArray*>(collection.as.obj);
                int idx = (int)index.as.number;
                if (idx >= 0 && idx < (int)array->elements.size()) {
                    vm->push(array->elements[idx]);
                } else {
                    vm->push(SapphireValue());
                }
            } else if (is_obj_type(collection, OBJ_MAP) && is_obj_type(index, OBJ_STRING)) {
                ObjMap* map = static_cast<ObjMap*>(collection.as.obj);
                std::string key = static_cast<ObjString*>(index.as.obj)->chars;
                if (map->items.count(key)) {
                    vm->push(map->items[key]);
                } else {
                    vm->push(SapphireValue());
                }
            } else {
                vm->push(SapphireValue());
            }
            frame->ip++;
            break;
        }
        case OP_SET_SUBSCRIPT: {
            SapphireValue value = vm->stack_top[-1];
            SapphireValue index = vm->stack_top[-2];
            SapphireValue collection = vm->stack_top[-3];
            vm->stack_top -= 3;
            
            if (is_obj_type(collection, OBJ_ARRAY) && index.type == ValType::VAL_NUMBER) {
                ObjArray* array = static_cast<ObjArray*>(collection.as.obj);
                int idx = (int)index.as.number;
                if (idx >= 0 && idx < (int)array->elements.size()) {
                    array->elements[idx] = value;
                }
            } else if (is_obj_type(collection, OBJ_MAP) && is_obj_type(index, OBJ_STRING)) {
                ObjMap* map = static_cast<ObjMap*>(collection.as.obj);
                std::string key = static_cast<ObjString*>(index.as.obj)->chars;
                map->items[key] = value;
            }
            vm->push(value);
            frame->ip++;
            break;
        }
        case OP_SPREAD_ARRAY: {
            // Simplified spread - just pop the array
            if (vm->stack_top[-1].type == ValType::VAL_NIL) {
                vm->pop();
            } else if (is_obj_type(vm->stack_top[-1], OBJ_ARRAY)) {
                ObjArray* array = static_cast<ObjArray*>(vm->stack_top[-1].as.obj);
                vm->pop();
                for (const auto& elem : array->elements) {
                    vm->push(elem);
                }
            } else {
                vm->pop();
            }
            frame->ip++;
            break;
        }
        case OP_MAKE_NAMED_ARG: {
            uint8_t constant = ip[1];
            SapphireValue name_val = frame->function->chunk.constants[constant];
            SapphireValue value = vm->stack_top[-1];
            vm->stack_top--;
            
            ObjNamedArg* narg = new_named_arg(vm, static_cast<ObjString*>(name_val.as.obj), value);
            vm->push(narg);
            frame->ip += 2;
            break;
        }
        case OP_INHERIT: {
            SapphireValue superclass = vm->stack_top[-1];
            SapphireValue subclass = vm->stack_top[-2];
            vm->stack_top -= 2;
            
            if (is_obj_type(subclass, OBJ_CLASS) && is_obj_type(superclass, OBJ_CLASS)) {
                ObjClass* sub = static_cast<ObjClass*>(subclass.as.obj);
                ObjClass* super = static_cast<ObjClass*>(superclass.as.obj);
                sub->superclass = super;
            }
            vm->push(subclass);
            frame->ip++;
            break;
        }
        case OP_GET_SUPER: {
            uint8_t constant = ip[1];
            SapphireValue constant_val = frame->function->chunk.constants[constant];
            std::string method_name = static_cast<ObjString*>(constant_val.as.obj)->chars;
            
            SapphireValue receiver = vm->stack_top[-1];
            vm->stack_top--;
            
            if (is_obj_type(receiver, OBJ_INSTANCE)) {
                ObjInstance* instance = static_cast<ObjInstance*>(receiver.as.obj);
                ObjClass* klass = instance->klass;
                while (klass != nullptr) {
                    if (klass->superclass != nullptr && klass->superclass->methods.count(method_name)) {
                        vm->push(klass->superclass->methods[method_name]);
                        vm->push(receiver);
                        break;
                    }
                    klass = klass->superclass;
                }
                if (klass == nullptr) {
                    vm->push(SapphireValue());
                    vm->push(receiver);
                }
            } else {
                vm->push(SapphireValue());
                vm->push(receiver);
            }
            frame->ip += 2;
            break;
        }
        case OP_GET_ITERATOR: {
            // Simplified iterator - just return the array itself
            SapphireValue iterable = vm->stack_top[-1];
            vm->stack_top--;
            vm->push(iterable);
            frame->ip++;
            break;
        }
        case OP_ITER_NEXT_IN: {
            // Simplified iteration - not fully implemented
            vm->push(SapphireValue(false)); // iteration done
            frame->ip++;
            break;
        }
        case OP_ITER_NEXT_OF: {
            // Simplified iteration - not fully implemented
            vm->push(SapphireValue(false)); // iteration done
            frame->ip++;
            break;
        }
        case OP_TRY_START: {
            // Simplified try-catch - just record the position
            frame->ip += 3;
            break;
        }
        case OP_TRY_END: {
            frame->ip += 3;
            break;
        }
        case OP_THROW: {
            SapphireValue exception = vm->stack_top[-1];
            vm->stack_top--;
            // Simplified - just print and continue
            std::cerr << "Exception thrown: ";
            print_value(exception);
            std::cerr << std::endl;
            frame->ip++;
            break;
        }
        case OP_WITHIN_START: {
            uint16_t jump = (int16_t)((ip[1] << 8) | ip[2]);
            // Simplified within - just skip to fallback
            frame->ip += 3 + jump;
            break;
        }
        case OP_WITHIN_END: {
            frame->ip += 3;
            break;
        }
        case OP_EVERY_TICK: {
            uint32_t ms = (ip[1] << 24) | (ip[2] << 16) | (ip[3] << 8) | ip[4];
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            frame->ip += 5;
            break;
        }
        case OP_UNDO: {
            // Simplified undo - just pop
            vm->pop();
            frame->ip++;
            break;
        }
        case OP_DEFINE_FADE: {
            // Simplified fade - not fully implemented
            frame->ip += 2;
            break;
        }
        case OP_CLOSURE: {
            uint8_t constant = ip[1];
            SapphireValue constant_val = frame->function->chunk.constants[constant];
            ObjFunction* func = static_cast<ObjFunction*>(constant_val.as.obj);
            
            ObjClosure* closure = new_closure(vm, func);
            vm->push(closure);
            
            // Capture upvalues (simplified - ObjFunction doesn't have upvalue_count in this version)
            // for (int i = 0; i < func->upvalue_count; i++) {
            //     // Simplified - not fully implemented
            // }
            
            frame->ip += 2;
            break;
        }
        case OP_ASYNC_CALL: {
            // Simplified async call - treat as normal call
            uint8_t arg_count = ip[1];
            frame->ip += 2;
            break;
        }
        default: {
            fprintf(stderr, "JIT Error: Unimplemented opcode %d in generic fallback\n", opcode);
            frame->ip++;
            break;
        }
    }
}

extern "C" JIT_ABI void jit_print_value(SapphireValue* val) {
    printf("[JIT PRINT] Called with val=%p\n", val);
    print_value(*val);
    printf("\n");
}
bool VM::run(int target_frame_count) {
    CallFrame* frame = &frames[frame_count - 1];
    Chunk* chunk = &frame->function->chunk;
    
    if (rubellite_debug) {
        printf("[JIT DEBUG] Starting JIT compilation for function with %zu opcodes\n", chunk->code.size());
    }
    
    std::vector<uint8_t> optimized_code = chunk->code;
    std::vector<SapphireValue> optimized_constants = chunk->constants;
    
    for (size_t i = 0; i < optimized_code.size(); ) {
        uint8_t opcode = optimized_code[i];
        
        if ((opcode == OP_ADD || opcode == OP_SUBTRACT || opcode == OP_MULTIPLY || opcode == OP_DIVIDE) && 
            i >= 3 && optimized_code[i-2] == OP_CONSTANT && optimized_code[i-1] == OP_CONSTANT) {
            uint8_t const1_idx = optimized_code[i-1];
            uint8_t const2_idx = optimized_code[i-3];
            
            SapphireValue val1 = optimized_constants[const1_idx];
            SapphireValue val2 = optimized_constants[const2_idx];
            
            if (val1.type == ValType::VAL_NUMBER && val2.type == ValType::VAL_NUMBER) {
                double result = 0.0;
                if (opcode == OP_ADD) result = val2.as.number + val1.as.number;
                else if (opcode == OP_SUBTRACT) result = val2.as.number - val1.as.number;
                else if (opcode == OP_MULTIPLY) result = val2.as.number * val1.as.number;
                else if (opcode == OP_DIVIDE && val1.as.number != 0.0) result = val2.as.number / val1.as.number;
                
                optimized_constants.push_back(SapphireValue(result));
                uint8_t new_const_idx = optimized_constants.size() - 1;
                
                optimized_code.erase(optimized_code.begin() + i - 3, optimized_code.begin() + i + 1);
                optimized_code.insert(optimized_code.begin() + i - 3, OP_CONSTANT);
                optimized_code.insert(optimized_code.begin() + i - 2, new_const_idx);
                continue;
            }
        } else if (opcode == OP_JUMP_IF_FALSE && i >= 1 && optimized_code[i-1] == OP_CONSTANT) {
            uint8_t const_idx = optimized_code[i-1];
            SapphireValue val = optimized_constants[const_idx];
            
            if (is_falsey(val)) {
                optimized_code[i] = OP_JUMP;
            } else {
                optimized_code.erase(optimized_code.begin() + i, optimized_code.begin() + i + 3);
                if (i >= 1 && optimized_code[i-1] == OP_CONSTANT) {
                    bool used = false;
                    for (size_t j = 0; j < optimized_code.size(); j++) {
                        if (j != i-1 && optimized_code[j] == OP_CONSTANT && optimized_code[j+1] == const_idx) {
                            used = true;
                            break;
                        }
                    }
                    if (!used) {
                        optimized_code.erase(optimized_code.begin() + i - 2, optimized_code.begin() + i);
                        i -= 2;
                    }
                }
                continue;
            }
        }
        i++;
    }
    
    Chunk optimized_chunk;
    optimized_chunk.code = optimized_code;
    optimized_chunk.constants = optimized_constants;
    chunk = &optimized_chunk;
    
    if (rubellite_debug) {
        printf("[JIT DEBUG] Optimization complete. %zu opcodes -> %zu opcodes\n", 
               frame->function->chunk.code.size(), optimized_code.size());
    }
    
    JitAssembler jit;
    
    jit.emit_push_reg(3); 
    jit.emit_push_reg(5); 
    jit.emit_push_reg(6); 
    jit.emit_push_reg(7); 
    jit.emit_push_reg(12); 
    jit.emit_push_reg(13); 
    jit.emit_push_reg(14); 
    jit.emit_push_reg(15); 

    jit.emit_mov_reg_imm64(13, (uint64_t)this);
    
    size_t stack_top_offset = offsetof(VM, stack_top);
    jit.emit_mov_reg_reg(14, 13);
    jit.emit_add_reg_imm32(14, stack_top_offset);
    
    jit.emit_mov_reg_mem(12, 14, 0);

    size_t globals_offset = offsetof(VM, globals);
    jit.emit_mov_reg_reg(15, 13);
    jit.emit_add_reg_imm32(15, globals_offset);

    std::vector<JitAssembler::Label> labels;
    labels.reserve(chunk->code.size());
    for (size_t i = 0; i < chunk->code.size(); i++) {
        labels.push_back(jit.new_label());
    }

    static void* dispatch_table[] = {
        &&TARGET_OP_CONSTANT, &&TARGET_OP_NIL, &&TARGET_OP_TRUE, &&TARGET_OP_FALSE,
        &&TARGET_OP_POP, &&TARGET_OP_DUP,
        &&TARGET_OP_GET_LOCAL, &&TARGET_OP_SET_LOCAL, &&TARGET_OP_GET_GLOBAL, &&TARGET_OP_DEFINE_GLOBAL, &&TARGET_OP_SET_GLOBAL, &&TARGET_OP_GET_PROPERTY, &&TARGET_OP_SET_PROPERTY,
        &&TARGET_OP_BUILD_ARRAY, &&TARGET_OP_BUILD_MAP, &&TARGET_OP_GET_SUBSCRIPT, &&TARGET_OP_SET_SUBSCRIPT, &&TARGET_OP_SPREAD_ARRAY,
        &&TARGET_OP_EQUAL, &&TARGET_OP_GREATER, &&TARGET_OP_LESS, &&TARGET_OP_NOT,
        &&TARGET_OP_ADD, &&TARGET_OP_SUBTRACT, &&TARGET_OP_MULTIPLY, &&TARGET_OP_DIVIDE, &&TARGET_OP_MODULO, &&TARGET_OP_NEGATE,
        &&TARGET_OP_BITWISE_AND, &&TARGET_OP_BITWISE_OR, &&TARGET_OP_BITWISE_XOR, &&TARGET_OP_BITWISE_NOT, &&TARGET_OP_LEFT_SHIFT, &&TARGET_OP_RIGHT_SHIFT,
        &&TARGET_OP_PRINT, &&TARGET_OP_JUMP, &&TARGET_OP_JUMP_IF_FALSE, &&TARGET_OP_JUMP_IF_NIL, &&TARGET_OP_JUMP_IF_NOT_NIL, &&TARGET_OP_LOOP, 
        &&TARGET_OP_CALL, &&TARGET_OP_CLOSURE, &&TARGET_OP_RETURN, &&TARGET_OP_IMPORT, 
        &&TARGET_OP_MAKE_NAMED_ARG, &&TARGET_OP_INHERIT, &&TARGET_OP_GET_SUPER, 
        &&TARGET_OP_SPAWN, &&TARGET_OP_AWAIT, &&TARGET_OP_ASYNC_CALL,
        &&TARGET_OP_GET_ITERATOR, &&TARGET_OP_ITER_NEXT_IN, &&TARGET_OP_ITER_NEXT_OF,
        &&TARGET_OP_TRY_START, &&TARGET_OP_TRY_END, &&TARGET_OP_THROW,
        &&TARGET_OP_WITHIN_START, &&TARGET_OP_WITHIN_END, &&TARGET_OP_EVERY_TICK, 
        &&TARGET_OP_UNDO, &&TARGET_OP_DEFINE_FADE,
        &&TARGET_OP_SUPER, &&TARGET_OP_THIS, &&TARGET_OP_CLASS
    };

    auto emit_trampoline = [&](void* func) {
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(0, (uint64_t)func);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
    };

    auto emit_trampoline_with_opcode = [&](void* func, uint64_t op) {
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(2, op);
        jit.emit_mov_reg_imm64(0, (uint64_t)func);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
    };

    int offset = 0;
    uint8_t opcode = 0;
    
    if (chunk->code.size() == 0) goto JIT_END;

NEXT_OPCODE:
    if (offset >= chunk->code.size()) goto JIT_END;
    
    jit.bind(labels[offset]);
    
    opcode = chunk->code[offset];
    if (opcode >= sizeof(dispatch_table)/sizeof(dispatch_table[0])) {
        goto JIT_END;
    }
    goto *dispatch_table[opcode];

    TARGET_OP_CONSTANT: {
        uint8_t constant_idx = chunk->code[offset + 1];
        SapphireValue* val_ptr = &chunk->constants[constant_idx];
        jit.emit_mov_reg_imm64(0, (uint64_t)val_ptr);
        jit.emit_mov_reg_mem(1, 0, 0);
        jit.emit_mov_mem_reg(12, 0, 1);
        jit.emit_mov_reg_mem(1, 0, 8);
        jit.emit_mov_mem_reg(12, 8, 1);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_NIL: {
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_TRUE: {
        jit.emit_mov_reg_imm64(0, 1);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_mov_reg_imm64(0, 1);
        jit.emit_mov_mem_reg(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_FALSE: {
        jit.emit_mov_reg_imm64(0, 1);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_POP: {
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_DUP: {
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));
        jit.emit_mov_reg_mem(1, 0, 0);
        jit.emit_mov_mem_reg(12, 0, 1);
        jit.emit_mov_reg_mem(1, 0, 8);
        jit.emit_mov_mem_reg(12, 8, 1);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_GET_LOCAL: {
        uint8_t slot = chunk->code[offset + 1];
        SapphireValue* slot_ptr = &frame->slots[slot];
        jit.emit_mov_reg_imm64(0, (uint64_t)slot_ptr);
        jit.emit_mov_reg_mem(1, 0, 0);
        jit.emit_mov_mem_reg(12, 0, 1);
        jit.emit_mov_reg_mem(1, 0, 8);
        jit.emit_mov_mem_reg(12, 8, 1);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_SET_LOCAL: {
        uint8_t slot = chunk->code[offset + 1];
        SapphireValue* slot_ptr = &frame->slots[slot];
        jit.emit_mov_reg_imm64(0, (uint64_t)slot_ptr);
        jit.emit_mov_reg_mem(1, 12, -(int32_t)sizeof(SapphireValue));
        jit.emit_mov_mem_reg(0, 0, 1);
        jit.emit_mov_reg_mem(1, 12, -(int32_t)sizeof(SapphireValue) + 8);
        jit.emit_mov_mem_reg(0, 8, 1);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_GET_GLOBAL: {
        uint8_t constant_idx = chunk->code[offset + 1];
        ObjString* name = (ObjString*)chunk->constants[constant_idx].as.obj;
        emit_trampoline_with_opcode((void*)&jit_trampoline_get_global, (uint64_t)&name->chars);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_DEFINE_GLOBAL: {
        uint8_t constant_idx = chunk->code[offset + 1];
        ObjString* name = (ObjString*)chunk->constants[constant_idx].as.obj;
        emit_trampoline_with_opcode((void*)&jit_trampoline_define_global, (uint64_t)&name->chars);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_SET_GLOBAL: {
        uint8_t constant_idx = chunk->code[offset + 1];
        ObjString* name = (ObjString*)chunk->constants[constant_idx].as.obj;
        emit_trampoline_with_opcode((void*)&jit_trampoline_set_global, (uint64_t)&name->chars);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_GET_PROPERTY: {
        JitAssembler::Label is_instance_lbl, not_instance_lbl, end_lbl;
        
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));
        
        jit.emit_cmp_mem8_imm8(0, 0, 3);
        jit.emit_jnz(not_instance_lbl);
        jit.emit_cmp_mem8_imm8(0, 8 + offsetof(Obj, type), OBJ_INSTANCE);
        jit.emit_jz(is_instance_lbl);
        jit.emit_jmp(not_instance_lbl);
        
        jit.bind(is_instance_lbl);
        jit.emit_mov_reg_mem(1, 0, 8 + offsetof(ObjInstance, fields));
        
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(2, OP_GET_PROPERTY);
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_get_property);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
        jit.emit_jmp(end_lbl);
        
        jit.bind(not_instance_lbl);
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        
        jit.bind(end_lbl);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_SET_PROPERTY: {
        emit_trampoline((void*)&jit_trampoline_set_property);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_BUILD_ARRAY: {
        // FIX: use the ms_abi trampoline instead of calling new_array() directly
        // with wrong ABI (new_array uses System V ABI, not ms_abi)
        uint8_t count = chunk->code[offset + 1];
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);           // r1 (rcx in ms_abi) = VM*
        jit.emit_mov_reg_imm64(2, count);      // r2 (rdx in ms_abi) = count
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_build_array);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
        
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_BUILD_MAP: {
        // FIX: use the ms_abi trampoline instead of calling new_map() directly
        // with wrong ABI (new_map uses System V ABI, not ms_abi)
        uint8_t count = chunk->code[offset + 1];
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);           // r1 (rcx in ms_abi) = VM*
        jit.emit_mov_reg_imm64(2, count);      // r2 (rdx in ms_abi) = count
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_build_map);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
        
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_GET_SUBSCRIPT: {
        JitAssembler::Label is_array_lbl, is_map_lbl, not_found_lbl, end_lbl;
        
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue) * 2);
        jit.emit_mov_reg_reg(1, 12);
        jit.emit_sub_reg_imm32(1, sizeof(SapphireValue));
        
        jit.emit_cmp_mem8_imm8(0, 0, 3);
        jit.emit_jnz(not_found_lbl);
        jit.emit_cmp_mem8_imm8(0, 8 + offsetof(Obj, type), OBJ_ARRAY);
        jit.emit_jz(is_array_lbl);
        jit.emit_cmp_mem8_imm8(0, 8 + offsetof(Obj, type), OBJ_MAP);
        jit.emit_jz(is_map_lbl);
        jit.emit_jmp(not_found_lbl);
        
        jit.bind(is_array_lbl);
        jit.emit_cmp_mem8_imm8(1, 0, 2);
        jit.emit_jnz(not_found_lbl);
        jit.emit_mov_reg_mem(2, 1, 8);
        jit.emit_movsd_xmm_mem(0, 1, 8);
        jit.emit_cvttsd2si_reg_xmm(3, 0);
        
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(2, OP_GET_SUBSCRIPT);
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_generic);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
        jit.emit_jmp(end_lbl);
        
        jit.bind(is_map_lbl);
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(2, OP_GET_SUBSCRIPT);
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_generic);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
        jit.emit_jmp(end_lbl);
        
        jit.bind(not_found_lbl);
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        
        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_SET_SUBSCRIPT: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_SET_SUBSCRIPT);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_SPREAD_ARRAY: {
        JitAssembler::Label not_array_lbl, end_lbl;
        
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));
        
        jit.emit_cmp_mem8_imm8(0, 0, 3);
        jit.emit_jnz(not_array_lbl);
        jit.emit_cmp_mem8_imm8(0, 8 + offsetof(Obj, type), OBJ_ARRAY);
        jit.emit_jnz(not_array_lbl);
        
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(2, OP_SPREAD_ARRAY);
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_generic);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
        jit.emit_jmp(end_lbl);
        
        jit.bind(not_array_lbl);
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue));
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        
        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_EQUAL:
    TARGET_OP_GREATER:
    TARGET_OP_LESS: {
        JitAssembler::Label fallback_lbl, is_true_lbl, cont_lbl, end_lbl;

        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue) * 2);

        jit.emit_cmp_mem8_imm8(12, 0, 2);
        jit.emit_jnz(fallback_lbl);
        jit.emit_cmp_mem8_imm8(12, sizeof(SapphireValue), 2);
        jit.emit_jnz(fallback_lbl);

        jit.emit_movsd_xmm_mem(0, 12, 8);
        jit.emit_movsd_xmm_mem(1, 12, sizeof(SapphireValue) + 8);
        jit.emit_ucomisd_xmm_xmm(0, 1);

        uint8_t instruction = chunk->code[offset];
        if (instruction == OP_EQUAL) jit.emit_jz(is_true_lbl);
        else if (instruction == OP_GREATER) jit.emit_ja(is_true_lbl);
        else if (instruction == OP_LESS) jit.emit_jb(is_true_lbl);

        jit.emit_mov_reg_imm64(0, 1);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(12, 8, 0);
        jit.emit_jmp(cont_lbl);

        jit.bind(is_true_lbl);
        jit.emit_mov_reg_imm64(0, 1);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_mov_mem_reg(12, 8, 0);

        jit.bind(cont_lbl);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        jit.emit_jmp(end_lbl);

        jit.bind(fallback_lbl);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue) * 2);
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, instruction);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_NOT: {
        JitAssembler::Label is_false_lbl, end_lbl;

        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));
        
        jit.emit_cmp_mem8_imm8(0, 0, 0);
        jit.emit_jz(is_false_lbl);

        jit.emit_cmp_mem8_imm8(0, 0, 1);
        jit.emit_jnz(end_lbl);

        jit.emit_cmp_mem8_imm8(0, 8, 0);
        jit.emit_jz(is_false_lbl);

        jit.emit_mov_reg_imm64(1, 1);
        jit.emit_mov_mem_reg(0, 0, 1);
        jit.emit_mov_reg_imm64(1, 0);
        jit.emit_mov_mem_reg(0, 8, 1);
        jit.emit_jmp(end_lbl);

        jit.bind(is_false_lbl);
        jit.emit_mov_reg_imm64(1, 1);
        jit.emit_mov_mem_reg(0, 0, 1);
        jit.emit_mov_mem_reg(0, 8, 1);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_ADD:
    TARGET_OP_SUBTRACT:
    TARGET_OP_MULTIPLY:
    TARGET_OP_DIVIDE: {
        JitAssembler::Label fallback_lbl, end_lbl;

        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue) * 2);

        jit.emit_cmp_mem8_imm8(12, 0, 2);
        jit.emit_jnz(fallback_lbl);
        jit.emit_cmp_mem8_imm8(12, sizeof(SapphireValue), 2);
        jit.emit_jnz(fallback_lbl);

        jit.emit_movsd_xmm_mem(0, 12, 8);
        jit.emit_movsd_xmm_mem(1, 12, sizeof(SapphireValue) + 8);

        uint8_t instruction = chunk->code[offset];
        if (instruction == OP_ADD) jit.emit_addsd_xmm_xmm(0, 1);
        else if (instruction == OP_SUBTRACT) jit.emit_subsd_xmm_xmm(0, 1);
        else if (instruction == OP_MULTIPLY) jit.emit_mulsd_xmm_xmm(0, 1);
        else if (instruction == OP_DIVIDE) jit.emit_divsd_xmm_xmm(0, 1);

        jit.emit_movsd_mem_xmm(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        jit.emit_jmp(end_lbl);

        jit.bind(fallback_lbl);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue) * 2);
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, instruction);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_MODULO: {
        JitAssembler::Label fallback_lbl, end_lbl;

        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue) * 2);

        jit.emit_cmp_mem8_imm8(12, 0, 2);
        jit.emit_jnz(fallback_lbl);
        jit.emit_cmp_mem8_imm8(12, sizeof(SapphireValue), 2);
        jit.emit_jnz(fallback_lbl);

        jit.emit_movsd_xmm_mem(0, 12, 8);
        jit.emit_movsd_xmm_mem(1, 12, sizeof(SapphireValue) + 8);
        jit.emit_cvttsd2si_reg_xmm(0, 0);
        jit.emit_cvttsd2si_reg_xmm(1, 1);
        
        jit.emit_mov_reg_reg(0, 0);
        jit.emit_cqo();
        jit.emit_idiv_reg(1);
        jit.emit_mov_reg_reg(0, 2);
        
        jit.emit_cvtsi2sd_xmm_reg(0, 0);
        jit.emit_movsd_mem_xmm(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        jit.emit_jmp(end_lbl);

        jit.bind(fallback_lbl);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue) * 2);
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_MODULO);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_NEGATE: {
        JitAssembler::Label fallback_lbl, end_lbl;

        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));

        jit.emit_cmp_mem8_imm8(0, 0, 2);
        jit.emit_jnz(fallback_lbl);

        jit.emit_xorpd_xmm_xmm(1, 1);
        jit.emit_movsd_xmm_mem(0, 0, 8);
        jit.emit_subsd_xmm_xmm(1, 0);
        jit.emit_movsd_mem_xmm(0, 8, 1);
        jit.emit_jmp(end_lbl);

        jit.bind(fallback_lbl);
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_NEGATE);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_BITWISE_AND:
    TARGET_OP_BITWISE_OR:
    TARGET_OP_BITWISE_XOR: {
        JitAssembler::Label fallback_lbl, end_lbl;

        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue) * 2);

        jit.emit_cmp_mem8_imm8(12, 0, 2);
        jit.emit_jnz(fallback_lbl);
        jit.emit_cmp_mem8_imm8(12, sizeof(SapphireValue), 2);
        jit.emit_jnz(fallback_lbl);

        jit.emit_movsd_xmm_mem(0, 12, 8);
        jit.emit_movsd_xmm_mem(1, 12, sizeof(SapphireValue) + 8);
        jit.emit_cvttsd2si_reg_xmm(0, 0);
        jit.emit_cvttsd2si_reg_xmm(1, 1);

        uint8_t instruction = chunk->code[offset];
        if (instruction == OP_BITWISE_AND) jit.emit_and_reg_reg(0, 1);
        else if (instruction == OP_BITWISE_OR) jit.emit_or_reg_reg(0, 1);
        else if (instruction == OP_BITWISE_XOR) jit.emit_xor_reg_reg(0, 1);

        jit.emit_cvtsi2sd_xmm_reg(0, 0);
        jit.emit_movsd_mem_xmm(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        jit.emit_jmp(end_lbl);

        jit.bind(fallback_lbl);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue) * 2);
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, instruction);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_LEFT_SHIFT:
    TARGET_OP_RIGHT_SHIFT: {
        JitAssembler::Label fallback_lbl, end_lbl;

        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue) * 2);

        jit.emit_cmp_mem8_imm8(12, 0, 2);
        jit.emit_jnz(fallback_lbl);
        jit.emit_cmp_mem8_imm8(12, sizeof(SapphireValue), 2);
        jit.emit_jnz(fallback_lbl);

        jit.emit_movsd_xmm_mem(0, 12, 8);
        jit.emit_movsd_xmm_mem(1, 12, sizeof(SapphireValue) + 8);
        jit.emit_cvttsd2si_reg_xmm(0, 0);
        jit.emit_cvttsd2si_reg_xmm(1, 1);

        jit.emit_mov_reg_reg(1, 1);
        uint8_t instruction = chunk->code[offset];
        if (instruction == OP_LEFT_SHIFT) jit.emit_shl_reg_cl(0);
        else if (instruction == OP_RIGHT_SHIFT) jit.emit_sar_reg_cl(0);

        jit.emit_cvtsi2sd_xmm_reg(0, 0);
        jit.emit_movsd_mem_xmm(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        jit.emit_jmp(end_lbl);

        jit.bind(fallback_lbl);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue) * 2);
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, instruction);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_BITWISE_NOT: {
        JitAssembler::Label fallback_lbl, end_lbl;

        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));

        jit.emit_cmp_mem8_imm8(0, 0, 2);
        jit.emit_jnz(fallback_lbl);

        jit.emit_movsd_xmm_mem(0, 0, 8);
        jit.emit_cvttsd2si_reg_xmm(0, 0);
        jit.emit_not_reg(0);
        jit.emit_cvtsi2sd_xmm_reg(0, 0);
        jit.emit_movsd_mem_xmm(0, 8, 0);
        jit.emit_jmp(end_lbl);

        jit.bind(fallback_lbl);
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_BITWISE_NOT);

        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_JUMP: {
        uint16_t jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
        jit.emit_jmp(labels[offset + 3 + jump]);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_JUMP_IF_FALSE: {
        uint16_t jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
        JitAssembler::Label is_false_lbl, end_lbl;

        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));

        jit.emit_cmp_mem8_imm8(0, 0, 0);
        jit.emit_jz(is_false_lbl);

        jit.emit_cmp_mem8_imm8(0, 0, 1);
        jit.emit_jnz(end_lbl);

        jit.emit_cmp_mem8_imm8(0, 8, 0);
        jit.emit_jz(is_false_lbl);
        jit.emit_jmp(end_lbl);

        jit.bind(is_false_lbl);
        jit.emit_jmp(labels[offset + 3 + jump]);

        jit.bind(end_lbl);
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue));
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_JUMP_IF_NIL: {
        uint16_t jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
        JitAssembler::Label not_nil_lbl, end_lbl;

        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));

        jit.emit_cmp_mem8_imm8(0, 0, 0);
        jit.emit_jnz(not_nil_lbl);

        jit.emit_jmp(labels[offset + 3 + jump]);

        jit.bind(not_nil_lbl);
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue));
        jit.bind(end_lbl);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_JUMP_IF_NOT_NIL: {
        uint16_t jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
        JitAssembler::Label is_nil_lbl, end_lbl;

        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));

        jit.emit_cmp_mem8_imm8(0, 0, 0);
        jit.emit_jz(is_nil_lbl);

        jit.emit_jmp(labels[offset + 3 + jump]);

        jit.bind(is_nil_lbl);
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue));
        jit.bind(end_lbl);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_LOOP: {
        uint16_t jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
        jit.emit_jmp(labels[offset + 3 - jump]);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_CALL: {
        uint8_t arg_count = chunk->code[offset + 1];
        
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue) * (arg_count + 1));
        
        JitAssembler::Label is_closure_lbl, is_native_lbl, is_class_lbl, end_lbl;
        jit.emit_cmp_mem8_imm8(0, 0, 3);
        jit.emit_jnz(end_lbl);
        
        jit.emit_mov_reg_mem(1, 0, 8 + offsetof(Obj, type));
        jit.emit_cmp_reg_imm32(1, OBJ_CLOSURE);
        jit.emit_jz(is_closure_lbl);
        jit.emit_cmp_reg_imm32(1, OBJ_NATIVE);
        jit.emit_jz(is_native_lbl);
        jit.emit_cmp_reg_imm32(1, OBJ_CLASS);
        jit.emit_jz(is_class_lbl);
        jit.emit_jmp(end_lbl);
        
        jit.bind(is_closure_lbl);
        emit_trampoline_with_opcode((void*)&jit_trampoline_call, arg_count);
        jit.emit_jmp(end_lbl);
        
        jit.bind(is_native_lbl);
        emit_trampoline_with_opcode((void*)&jit_trampoline_call, arg_count);
        jit.emit_jmp(end_lbl);
        
        jit.bind(is_class_lbl);
        emit_trampoline_with_opcode((void*)&jit_trampoline_call, arg_count);
        
        jit.bind(end_lbl);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_CLOSURE: {
        emit_trampoline((void*)&jit_trampoline_closure);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_RETURN: {
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_pop_reg(15);
        jit.emit_pop_reg(14);
        jit.emit_pop_reg(13);
        jit.emit_pop_reg(12);
        jit.emit_pop_reg(7);
        jit.emit_pop_reg(6);
        jit.emit_pop_reg(5);
        jit.emit_pop_reg(3);
        jit.emit_ret();
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_IMPORT: {
        emit_trampoline((void*)&jit_trampoline_import);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_MAKE_NAMED_ARG: {
        emit_trampoline((void*)&jit_trampoline_make_named_arg);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_INHERIT: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_INHERIT);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_GET_SUPER: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_GET_SUPER);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_SPAWN: {
        emit_trampoline((void*)&jit_trampoline_spawn);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_AWAIT: {
        emit_trampoline((void*)&jit_trampoline_await);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_ASYNC_CALL: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_ASYNC_CALL);
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_GET_ITERATOR: {
        JitAssembler::Label is_array_lbl, end_lbl;
        
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue));
        
        jit.emit_cmp_mem8_imm8(0, 0, 3);
        jit.emit_jnz(end_lbl);
        jit.emit_cmp_mem8_imm8(0, 8 + offsetof(Obj, type), OBJ_ARRAY);
        jit.emit_jnz(end_lbl);
        
        jit.emit_mov_reg_mem(1, 0, 0);
        jit.emit_mov_mem_reg(12, 0, 1);
        jit.emit_mov_reg_mem(1, 0, 8);
        jit.emit_mov_mem_reg(12, 8, 1);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        
        jit.emit_mov_reg_imm64(0, 2);
        jit.emit_mov_mem_reg(12, 0, 0);
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(12, 8, 0);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        
        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_ITER_NEXT_IN: {
        JitAssembler::Label is_array_lbl, end_lbl, has_more_lbl;
        
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue) * 2);
        jit.emit_mov_reg_reg(1, 12);
        jit.emit_sub_reg_imm32(1, sizeof(SapphireValue));
        
        jit.emit_cmp_mem8_imm8(0, 0, 3);
        jit.emit_jnz(end_lbl);
        jit.emit_cmp_mem8_imm8(0, 8 + offsetof(Obj, type), OBJ_ARRAY);
        jit.emit_jnz(end_lbl);
        
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(2, OP_ITER_NEXT_IN);
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_generic);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
        jit.emit_jmp(end_lbl);
        
        jit.bind(end_lbl);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_ITER_NEXT_OF: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_ITER_NEXT_OF);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_TRY_START: {
        uint16_t jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
        jit.emit_mov_reg_imm64(0, offset + 3 + jump);
        jit.emit_mov_mem_reg(13, offsetof(VM, catch_count), 0);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_TRY_END: {
        jit.emit_mov_reg_imm64(0, 0);
        jit.emit_mov_mem_reg(13, offsetof(VM, catch_count), 0);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_THROW: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_THROW);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_WITHIN_START: {
        uint16_t jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
        jit.emit_jmp(labels[offset + 3 + jump]);
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_WITHIN_END: {
        offset += 3;
        goto NEXT_OPCODE;
    }

    TARGET_OP_EVERY_TICK: {
        uint32_t ms = (chunk->code[offset + 1] << 24) | (chunk->code[offset + 2] << 16) | 
                      (chunk->code[offset + 3] << 8) | chunk->code[offset + 4];
        jit.emit_mov_mem_reg(14, 0, 12);
        jit.emit_mov_reg_reg(1, 13);
        jit.emit_mov_reg_imm64(2, ms);
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_trampoline_generic);
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_mov_reg_mem(12, 14, 0);
        offset += 5;
        goto NEXT_OPCODE;
    }

    TARGET_OP_UNDO: {
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_DEFINE_FADE: {
        offset += 2;
        goto NEXT_OPCODE;
    }

    TARGET_OP_PRINT: {
        jit.emit_mov_reg_reg(0, 12);
        jit.emit_sub_reg_imm32(0, sizeof(SapphireValue)); 
        jit.emit_mov_reg_reg(1, 0); 
        jit.emit_mov_reg_imm64(0, (uint64_t)&jit_print_value); 
        jit.emit_sub_reg_imm32(4, 40);
        jit.emit_call_reg(0);
        jit.emit_add_reg_imm32(4, 40);
        jit.emit_sub_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_SUPER: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_SUPER);
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_THIS: {
        jit.emit_mov_reg_imm64(0, (uint64_t)&frame->slots[0]);
        jit.emit_mov_reg_mem(1, 0, 0);
        jit.emit_mov_mem_reg(12, 0, 1);
        jit.emit_mov_reg_mem(1, 0, 8);
        jit.emit_mov_mem_reg(12, 8, 1);
        jit.emit_add_reg_imm32(12, sizeof(SapphireValue));
        offset++;
        goto NEXT_OPCODE;
    }

    TARGET_OP_CLASS: {
        emit_trampoline_with_opcode((void*)&jit_trampoline_generic, OP_CLASS);
        offset++;
        goto NEXT_OPCODE;
    }

JIT_END:
    typedef void (*JitFunc)();
    int jit_error = 0;
    std::string jit_error_msg;
    
    if (rubellite_debug) {
        printf("[JIT DEBUG] Finalizing JIT code (code_size=%zu)\n", jit.code_size());
    }
    
    JitFunc func = (JitFunc)jit.finalize(&jit_error, &jit_error_msg);
    
    if (func) {
        if (rubellite_debug) {
            printf("[JIT DEBUG] JIT code finalized successfully. Executing...\n");
        }
        func();
        if (rubellite_debug) {
            printf("[JIT DEBUG] JIT execution completed.\n");
        }
    } else {
        printf("[JIT] ERROR: Failed to finalize JIT code! (error=%d, code_size=%zu)\n", jit_error, jit.code_size());
        if (!jit_error_msg.empty()) {
            printf("[JIT] ERROR details: %s\n", jit_error_msg.c_str());
        }
        return false;
    }
    
    return true;
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

static bool parse_top_memory_limit_mb(const std::string& source, size_t& out_limit_mb) {
    size_t pos = 0;
    const size_t len = source.size();

    auto trim_left = [&](size_t& start) {
        while (start < len && (source[start] == ' ' || source[start] == '\t' || source[start] == '\r')) {
            start++;
        }
    };

    const std::string keyword = "var";
    const std::string name = "MEMORY_LIMIT";

    while (pos < len) {
        size_t line_start = pos;
        size_t line_end = source.find('\n', pos);
        if (line_end == std::string::npos) {
            line_end = len;
        }

        size_t token_start = line_start;
        trim_left(token_start);
        if (token_start >= line_end) {
            pos = line_end == len ? len : line_end + 1;
            continue;
        }

        if (source.compare(token_start, 2, "//") == 0) {
            pos = line_end == len ? len : line_end + 1;
            continue;
        }

        if (source.compare(token_start, 2, "/*") == 0) {
            size_t comment_end = source.find("*/", token_start + 2);
            if (comment_end == std::string::npos) return false;
            pos = comment_end + 2;
            continue;
        }

        if (token_start + keyword.size() <= line_end && source.compare(token_start, keyword.size(), keyword) == 0) {
            size_t after_keyword = token_start + keyword.size();
            if (after_keyword < line_end && isspace(static_cast<unsigned char>(source[after_keyword]))) {
                size_t var_name_start = after_keyword;
                trim_left(var_name_start);
                if (var_name_start + name.size() <= line_end && source.compare(var_name_start, name.size(), name) == 0) {
                    size_t value_pos = var_name_start + name.size();
                    trim_left(value_pos);
                    if (value_pos < line_end && source[value_pos] == '=') {
                        value_pos++;
                        trim_left(value_pos);
                        size_t value_start = value_pos;
                        while (value_pos < line_end && isdigit(static_cast<unsigned char>(source[value_pos]))) {
                            value_pos++;
                        }
                        if (value_start == value_pos) {
                            return false;
                        }

                        size_t limit_mb = 0;
                        try {
                            limit_mb = std::stoull(source.substr(value_start, value_pos - value_start));
                        } catch (...) {
                            return false;
                        }

                        trim_left(value_pos);
                        if (value_pos < line_end) {
                            if (source[value_pos] == ';') {
                                value_pos++;
                                trim_left(value_pos);
                            }
                            // Permitir comentÃ¡rios apÃ³s o ponto e vÃ­rgula
                            if (value_pos < line_end && source.compare(value_pos, 2, "//") != 0) {
                                return false;
                            }
                        }

                        if (limit_mb == 0) {
                            return false;
                        }

                        out_limit_mb = limit_mb;
                        std::cout << "[VM] Memory limit set to " << limit_mb << " MB from script" << std::endl;
                        return true;
                    }
                }
            }
        }

        pos = line_end == len ? len : line_end + 1;
    }

    return false;
}

SapphireValue VM::interpret(const std::string& source) {
    size_t memory_limit_mb;
    if (parse_top_memory_limit_mb(source, memory_limit_mb)) {
        std::cout << "[VM] MAX MEM LIMIT IS NOW " << memory_limit_mb << "\n"; max_memory_limit = memory_limit_mb * 1024ull * 1024ull;
    }

    Preprocessor prep;
    std::string processed_source = prep.process(source);

    ObjFunction* function = compile(this, processed_source);
    if (function == nullptr) return {};

    resetStack();
    push(function);

    if (!call(function, 0)) return {};

    bool result = run();

    // Event Loop
    while (!event_loop_queue.empty()) {
        ObjPromise* next_promise = event_loop_queue.front();
        event_loop_queue.erase(event_loop_queue.begin());
        
        if (next_promise->state != PromiseState::PENDING) continue;
        
        this->current_promise = next_promise;
        
        if (next_promise->saved_frames.empty() && next_promise->function != nullptr) {
            // First time running this coroutine!
            resetStack();
            push(SapphireValue(next_promise->function));
            for (const auto& arg : next_promise->args) push(arg);
            
            CallFrame* frame = &frames[frame_count++];
            frame->function = next_promise->function;
            frame->ip = &next_promise->function->chunk.code[0];
            frame->slots = stack;
        } else if (!next_promise->saved_frames.empty()) {
            // Resuming!
            stack_top = stack;
            for (auto v : next_promise->saved_stack) *stack_top++ = v;
            frame_count = 0;
            for (auto f : next_promise->saved_frames) frames[frame_count++] = f;
        } else {
            // Dummy main promise
            continue;
        }
        
        run();
    }
    
    this->current_promise = nullptr;

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

// --- FunÃ§Ãµes do Coletor de Lixo ---
void VM::mark_object(Obj* object) {
    if (object == nullptr || object->is_marked) return;

    object->is_marked = true;
    gray_stack.push_back(object);
}

void VM::mark_value(SapphireValue value) {
    if (value.type == ValType::VAL_OBJ) {
        mark_object(value.as.obj);
    } else if (is_obj_type(value, OBJ_ARRAY)) {
        auto array = static_cast<ObjArray*>(value.as.obj);
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





