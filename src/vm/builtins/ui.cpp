#include "builtins.h"
#include <fstream>
#include "nlohmann/json.hpp"

void draw_rounded_rect(sf::RenderWindow& window, sf::Vector2f pos, sf::Vector2f size, float radius, sf::Color color, sf::Color outline, float thickness);
float lerp_val(float a, float b, float t);
sf::Color lerp_color(sf::Color a, sf::Color b, float t);
// === NEW v1.0.9 ===
static float apply_easing(float t, const std::string& easing);
static void draw_gradient_rect(sf::RenderWindow& window, sf::Vector2f pos, sf::Vector2f size, float radius, sf::Color c1, sf::Color c2, const std::string& dir);
static void draw_shadow(sf::RenderWindow& window, sf::Vector2f pos, sf::Vector2f size, float radius, sf::Color shadowCol, float blur, float offX, float offY);



void sapphire_ui_trace(const std::string& id, sf::Vector2f size, float radius) {
    g_current_vm->ui_state.lastComponentId = id;
}

UIStyle* get_style() {
    return g_current_vm->ui_state.activeStyle ? g_current_vm->ui_state.activeStyle : &g_current_vm->ui_state.defaultStyle;
}

void draw_element_box(sf::RenderWindow& window, sf::Vector2f pos, sf::Vector2f size, sf::Color color, sf::Color outline) {
    UIStyle* s = get_style();
    draw_rounded_rect(window, pos, size, s->borderRadius, color, outline, s->borderThickness);
}


void draw_rounded_rect(sf::RenderWindow& window, sf::Vector2f pos, sf::Vector2f size, float radius, sf::Color color, sf::Color outline, float thickness) {
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

void sapphire_render_text(sf::RenderWindow& window, const std::string& content, sf::Vector2f pos, sf::Color color, std::string fontAlias, int fontSize) {
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


std::vector<std::shared_ptr<UINode>> deferred_render_nodes;

std::string valueToStringC(const SapphireValue& val) {
    std::stringstream ss;
    if (val.type == ValType::VAL_NIL) {
        ss << "nil";
    } else if (val.type == ValType::VAL_BOOL) {
        ss << (val.as.boolean ? "true" : "false");
    } else if (val.type == ValType::VAL_NUMBER) {
        double d = val.as.number;
        double int_part;
        if (modf(d, &int_part) == 0.0) ss << static_cast<long long>(d);
        else ss << d;
    } else if (val.type == ValType::VAL_OBJ) {
        Obj* obj = val.as.obj;
        if (obj->type == OBJ_STRING) ss << static_cast<ObjString*>(obj)->chars;
        else if (obj->type == OBJ_MAP) {
            ObjMap* map = static_cast<ObjMap*>(obj);
            ss << "{";
            auto it = map->items.begin();
            while (it != map->items.end()) {
                ss << it->first << ": " << valueToStringC(it->second);
                if (std::next(it) != map->items.end()) ss << ", ";
                ++it;
            }
            ss << "}";
        }
        else ss << "[object]";
    } else if (is_obj_type(val, OBJ_ARRAY)) {
        auto arr = static_cast<ObjArray*>(val.as.obj);
        ss << "[";
        for (size_t i = 0; i < arr->elements.size(); ++i) {
            ss << valueToStringC(arr->elements[i]);
            if (i < arr->elements.size() - 1) ss << ", ";
        }
        ss << "]";
    } else {
        ss << "[unknown]";
    }
    return ss.str();
}

sf::Color hexToColor(std::string hex) {
    if (hex[0] == '#') hex.erase(0, 1);
    if (hex.length() == 8) {
        uint32_t value = std::stoul(hex, nullptr, 16);
        return sf::Color((value >> 24) & 0xFF, (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
    }
    if (hex.length() != 6) return sf::Color::White;
    uint32_t value = std::stoul(hex, nullptr, 16);
    return sf::Color((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, 255);
}

std::shared_ptr<UINode> build_ui_tree(ObjInstance* nodeDict, int& counter) {
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
    // === NEW v1.0.9 ===
    else if (typeStr == "Card")    type = UINodeType::Card;
    else if (typeStr == "Badge")   type = UINodeType::Badge;
    else if (typeStr == "Tag")     type = UINodeType::Tag;
    else if (typeStr == "Stepper") type = UINodeType::Stepper;
    else if (typeStr == "Spinner") type = UINodeType::Spinner;
    
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

    // === NEW v1.0.9: Gradient, Shadow, Glow ===
    get_str("gradientFrom",  node->gradientFrom);
    get_str("gradientTo",    node->gradientTo);
    get_str("gradientDir",   node->gradientDir);
    get_str("shadowColor",   node->shadowColor);
    get_num("shadowBlur",    node->shadowBlur);
    get_num("shadowOffsetX", node->shadowOffsetX);
    get_num("shadowOffsetY", node->shadowOffsetY);
    get_str("glowColor",     node->glowColor);

    // === NEW v1.0.9: Badge / Stepper / Spinner ===
    float fBadge = 0; get_num("count", fBadge); if (fBadge > 0) node->badgeCount = (int)fBadge;
    float fSteps = 3; get_num("steps", fSteps); node->steps = (int)fSteps;
    float fCurStep = 0; get_num("current", fCurStep); node->currentStep = (int)fCurStep;
    // stepLabels
    if (nodeDict->fields.count("labels") && is_obj_type(nodeDict->fields["labels"], OBJ_ARRAY)) {
        auto arr = static_cast<ObjArray*>(nodeDict->fields["labels"].as.obj);
        for (auto& val : arr->elements) {
            if (is_obj_type(val, OBJ_STRING))
                node->stepLabels.push_back(static_cast<ObjString*>(val.as.obj)->chars);
        }
    }
    // Persistir spinAngle entre frames
    if (node->type == UINodeType::Spinner) {
        if (g_current_vm->ui_state.spinnerAngles.count(node->id))
            node->spinAngle = g_current_vm->ui_state.spinnerAngles[node->id];
    }

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

void apply_animations_to_tree(std::shared_ptr<UINode> node, float dt) {
    if (!node) return;

    // === NEW v1.0.9: Spinner auto-rotation ===
    if (node->type == UINodeType::Spinner) {
        node->spinAngle += dt * 280.0f; // graus por segundo
        if (node->spinAngle >= 360.0f) node->spinAngle -= 360.0f;
        g_current_vm->ui_state.spinnerAngles[node->id] = node->spinAngle;
    }

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
            // === NEW v1.0.9: Apply easing to t ===
            float easedT = apply_easing(t, anim.easing);

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
                float localEased = apply_easing(localT, anim.easing);

                for (auto& [prop, val1] : kf1.numericProps) {
                    if (kf2.numericProps.count(prop)) {
                        float val2 = kf2.numericProps.at(prop);
                        float interpolated = lerp_val(val1, val2, localEased);
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
                        sf::Color interpolated = lerp_color(c1, c2, localEased);
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


void compute_sizes(std::shared_ptr<UINode> node) {
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

void place_children(std::shared_ptr<UINode> node, float startX, float startY) {
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

void hit_test_tree(std::shared_ptr<UINode> node, sf::Vector2i m, bool mouseJustClicked) {
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

void render_ui_tree(std::shared_ptr<UINode> node) {
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
    // === NEW v1.0.9: Card ===
    else if (node->type == UINodeType::Card) {
        float r = s.borderRadius > 0 ? s.borderRadius : 12.0f;
        float w = node->width  > 0 ? node->width  : 200.0f;
        float h = node->height > 0 ? node->height : 120.0f;
        // Sombra
        if (node->shadowBlur > 0.0f || !node->shadowColor.empty()) {
            sf::Color sc = hexToColor(node->shadowColor.empty() ? "#00000060" : node->shadowColor);
            draw_shadow(*g_current_vm->sfml_window, {node->x, node->y}, {w, h}, r,
                        sc, node->shadowBlur > 0 ? node->shadowBlur : 6.0f,
                        node->shadowOffsetX != 0 ? node->shadowOffsetX : 0.0f,
                        node->shadowOffsetY != 0 ? node->shadowOffsetY : 6.0f);
        }
        // Gradiente ou cor sólida
        if (!node->gradientFrom.empty() && !node->gradientTo.empty()) {
            draw_gradient_rect(*g_current_vm->sfml_window, {node->x, node->y}, {w, h}, r,
                               hexToColor(node->gradientFrom), hexToColor(node->gradientTo),
                               node->gradientDir.empty() ? "vertical" : node->gradientDir);
            draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {w, h}, r,
                              sf::Color::Transparent, s.borderColor, s.borderThickness);
        } else {
            sf::Color bg = s.bgColor.a == 0 ? sf::Color(30, 30, 46, 255) : s.bgColor;
            draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {w, h}, r,
                              bg, s.borderColor, s.borderThickness);
        }
        // Glow no hover
        if (hovered && !node->glowColor.empty()) {
            sf::Color gc = hexToColor(node->glowColor);
            gc.a = 60;
            draw_rounded_rect(*g_current_vm->sfml_window, {node->x - 3, node->y - 3}, {w + 6, h + 6}, r + 3,
                              sf::Color::Transparent, gc, 3.0f);
        }
        // Título do card (label)
        if (!node->label.empty()) {
            std::string fa = s.fontAlias;
            if (!g_current_vm->ui_state.fontStack.count(fa)) fa = "default";
            if (g_current_vm->ui_state.fontStack.count(fa)) {
                sf::Text txt(g_current_vm->ui_state.fontStack[fa], node->label, s.fontSize > 0 ? s.fontSize : 16);
                txt.setFillColor(s.textColor);
                txt.setPosition({node->x + s.padding, node->y + s.padding});
                g_current_vm->sfml_window->draw(txt);
            }
        }
    }
    // === NEW v1.0.9: Badge ===
    else if (node->type == UINodeType::Badge) {
        float r = (node->height > 0 ? node->height : 22.0f) / 2.0f;
        float w = node->width > 0 ? node->width : r * 2.0f;
        sf::Color bg = s.bgColor.a == 0 ? (
            !node->customColor.empty() ? hexToColor(node->customColor) : sf::Color(220, 53, 69, 255)
        ) : s.bgColor;
        draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {w, r * 2.0f}, r, bg, sf::Color::Transparent, 0.0f);
        std::string txt = node->label.empty() ? std::to_string(node->badgeCount) : node->label;
        if (!txt.empty()) {
            std::string fa = s.fontAlias; if (!g_current_vm->ui_state.fontStack.count(fa)) fa = "default";
            if (g_current_vm->ui_state.fontStack.count(fa)) {
                unsigned int fs = s.fontSize > 0 ? s.fontSize : 11;
                sf::Text t(g_current_vm->ui_state.fontStack[fa], txt, fs);
                float tw = t.getLocalBounds().size.x;
                t.setFillColor(sf::Color::White);
                t.setPosition({node->x + w / 2.0f - tw / 2.0f, node->y + r - fs / 2.0f - 1.0f});
                g_current_vm->sfml_window->draw(t);
            }
        }
    }
    // === NEW v1.0.9: Tag / Chip ===
    else if (node->type == UINodeType::Tag) {
        float h = node->height > 0 ? node->height : 26.0f;
        float r = h / 2.0f;
        sf::Color bg = !node->customColor.empty() ? hexToColor(node->customColor) : sf::Color(99, 102, 241, 255);
        float w = node->width;
        if (w <= 0 && !node->label.empty()) {
            std::string fa = s.fontAlias; if (!g_current_vm->ui_state.fontStack.count(fa)) fa = "default";
            if (g_current_vm->ui_state.fontStack.count(fa)) {
                sf::Text dm(g_current_vm->ui_state.fontStack[fa], node->label, s.fontSize > 0 ? s.fontSize : 13);
                w = dm.getLocalBounds().size.x + 20.0f;
            } else w = 80.0f;
        }
        draw_rounded_rect(*g_current_vm->sfml_window, {node->x, node->y}, {w, h}, r, bg, sf::Color::Transparent, 0.0f);
        if (!node->label.empty()) {
            std::string fa = s.fontAlias; if (!g_current_vm->ui_state.fontStack.count(fa)) fa = "default";
            if (g_current_vm->ui_state.fontStack.count(fa)) {
                unsigned int fs = s.fontSize > 0 ? s.fontSize : 13;
                sf::Text t(g_current_vm->ui_state.fontStack[fa], node->label, fs);
                float tw = t.getLocalBounds().size.x;
                t.setFillColor(sf::Color::White);
                t.setPosition({node->x + w / 2.0f - tw / 2.0f, node->y + h / 2.0f - fs / 2.0f - 1.0f});
                g_current_vm->sfml_window->draw(t);
            }
        }
    }
    // === NEW v1.0.9: Stepper ===
    else if (node->type == UINodeType::Stepper) {
        int n = node->steps > 0 ? node->steps : 3;
        float w = node->width > 0 ? node->width : 300.0f;
        float cy = node->y + (node->height > 0 ? node->height : 40.0f) / 2.0f;
        float stepW = w / (float)(n > 1 ? n - 1 : 1);
        float circleR = 14.0f;
        std::string fa = s.fontAlias; if (!g_current_vm->ui_state.fontStack.count(fa)) fa = "default";
        for (int i = 0; i < n; ++i) {
            float cx = node->x + (n > 1 ? stepW * i : w / 2.0f);
            bool done = (i < node->currentStep);
            bool active = (i == node->currentStep);
            // Linha conectora
            if (i < n - 1) {
                float lineX = cx + circleR;
                float lineW = stepW - circleR * 2.0f;
                sf::Color lc = done ? sf::Color(139, 92, 246, 255) : sf::Color(80, 80, 100, 255);
                sf::RectangleShape line({lineW, 2.0f});
                line.setPosition({lineX, cy - 1.0f});
                line.setFillColor(lc);
                g_current_vm->sfml_window->draw(line);
            }
            // Círculo da etapa
            sf::Color circleCol = done   ? sf::Color(139, 92, 246, 255) :
                                  active ? sf::Color(167, 139, 250, 255) :
                                           sf::Color(60, 60, 80, 255);
            sf::Color outlineCol = active ? sf::Color(167, 139, 250, 255) : sf::Color::Transparent;
            sf::CircleShape circle(circleR);
            circle.setOrigin({circleR, circleR});
            circle.setPosition({cx, cy});
            circle.setFillColor(circleCol);
            circle.setOutlineColor(outlineCol);
            circle.setOutlineThickness(active ? 2.5f : 0.0f);
            g_current_vm->sfml_window->draw(circle);
            // Número ou checkmark
            if (g_current_vm->ui_state.fontStack.count(fa)) {
                std::string lbl = done ? "✓" : std::to_string(i + 1);
                unsigned int fs = 11;
                sf::Text t(g_current_vm->ui_state.fontStack[fa], lbl, fs);
                float tw = t.getLocalBounds().size.x;
                t.setFillColor(sf::Color::White);
                t.setPosition({cx - tw / 2.0f, cy - fs / 2.0f - 1.0f});
                g_current_vm->sfml_window->draw(t);
            }
            // Rótulo abaixo
            if (!node->stepLabels.empty() && (size_t)i < node->stepLabels.size() && g_current_vm->ui_state.fontStack.count(fa)) {
                unsigned int fs = 11;
                sf::Text lt(g_current_vm->ui_state.fontStack[fa], node->stepLabels[i], fs);
                float tw = lt.getLocalBounds().size.x;
                sf::Color tc = active ? sf::Color(167, 139, 250, 255) : sf::Color(180, 180, 200, 255);
                lt.setFillColor(tc);
                lt.setPosition({cx - tw / 2.0f, cy + circleR + 4.0f});
                g_current_vm->sfml_window->draw(lt);
            }
        }
    }
    // === NEW v1.0.9: Spinner ===
    else if (node->type == UINodeType::Spinner) {
        float r = (node->width > 0 ? node->width : 36.0f) / 2.0f;
        float cx = node->x + r;
        float cy = node->y + r;
        float thickness = node->thickness > 0 ? node->thickness : 4.0f;
        // Fundo (arco cinza)
        const int SEGMENTS = 48;
        float startAngle = node->spinAngle * 3.14159265f / 180.0f;
        float arcSpan = 270.0f * 3.14159265f / 180.0f; // 270 graus de arco ativo
        sf::Color arcColor = !node->customColor.empty() ? hexToColor(node->customColor) : sf::Color(139, 92, 246, 255);
        sf::Color trackColor = sf::Color(60, 60, 80, 200);
        // Track (fundo do spinner)
        sf::VertexArray track(sf::PrimitiveType::TriangleStrip, SEGMENTS * 2 + 2);
        for (int i = 0; i <= SEGMENTS; ++i) {
            float angle = startAngle + (float)i / SEGMENTS * 2.0f * 3.14159265f;
            float cosA = std::cos(angle), sinA = std::sin(angle);
            track[i*2+0] = { {cx + cosA * (r - thickness), cy + sinA * (r - thickness)}, trackColor };
            track[i*2+1] = { {cx + cosA * r,               cy + sinA * r},               trackColor };
        }
        g_current_vm->sfml_window->draw(track);
        // Arco ativo
        sf::VertexArray arc(sf::PrimitiveType::TriangleStrip, SEGMENTS * 2 + 2);
        int activeSegs = (int)(SEGMENTS * arcSpan / (2.0f * 3.14159265f));
        for (int i = 0; i <= activeSegs; ++i) {
            float angle = startAngle + (float)i / SEGMENTS * 2.0f * 3.14159265f;
            float ratio = (float)i / (float)activeSegs;
            sf::Color c = lerp_color(arcColor, sf::Color(arcColor.r, arcColor.g, arcColor.b, 80), ratio);
            float cosA = std::cos(angle), sinA = std::sin(angle);
            arc[i*2+0] = { {cx + cosA * (r - thickness), cy + sinA * (r - thickness)}, c };
            arc[i*2+1] = { {cx + cosA * r,               cy + sinA * r},               c };
        }
        g_current_vm->sfml_window->draw(arc);
    }

    // We update scale and rotation by using SFML transforms directly if needed, but since we use manual drawing, 
    // it's complex for generic containers. For an MVP animation, we just change X, Y, Width, Height, Opacity.
    
    if (node->type != UINodeType::Menu) {
        for (auto& child : node->children) {
            render_ui_tree(child);
        }
    }
}





float lerp_val(float a, float b, float t) { return a + (b - a) * t; }
sf::Color lerp_color(sf::Color a, sf::Color b, float t) {
    return sf::Color(
        (uint8_t)lerp_val(a.r, b.r, t),
        (uint8_t)lerp_val(a.g, b.g, t),
        (uint8_t)lerp_val(a.b, b.b, t),
        (uint8_t)lerp_val(a.a, b.a, t)
    );
}

// ============================================================
// === NEW v1.0.9: Easing Functions ===========================
// ============================================================
static float apply_easing(float t, const std::string& easing) {
    t = std::clamp(t, 0.0f, 1.0f);
    if (easing == "ease-in-quad")     return t * t;
    if (easing == "ease-out-quad")    return t * (2.0f - t);
    if (easing == "ease-in-out" || easing == "ease-in-out-cubic") {
        return t < 0.5f ? 4.0f*t*t*t : (t-1.0f)*(2.0f*t-2.0f)*(2.0f*t-2.0f)+1.0f;
    }
    if (easing == "ease-out-bounce") {
        if (t < 1.0f/2.75f) return 7.5625f*t*t;
        if (t < 2.0f/2.75f) { t -= 1.5f/2.75f;   return 7.5625f*t*t + 0.75f; }
        if (t < 2.5f/2.75f) { t -= 2.25f/2.75f;  return 7.5625f*t*t + 0.9375f; }
        t -= 2.625f/2.75f; return 7.5625f*t*t + 0.984375f;
    }
    if (easing == "ease-out-elastic") {
        if (t == 0.0f || t == 1.0f) return t;
        float p = 0.3f;
        return std::pow(2.0f, -10.0f*t) * std::sin((t - p/4.0f) * (2.0f * 3.14159265f) / p) + 1.0f;
    }
    return t; // linear default
}

// ============================================================
// === NEW v1.0.9: Gradient Rendering =========================
// ============================================================
static void draw_gradient_rect(
    sf::RenderWindow& window,
    sf::Vector2f pos, sf::Vector2f size,
    float radius,
    sf::Color c1, sf::Color c2,
    const std::string& dir = "vertical")
{
    if (size.x <= 0.0f || size.y <= 0.0f) return;

    // Use a VertexArray with sf::PrimitiveType::Triangles for gradient fill
    // We paint a clipped quad divided into many horizontal/vertical slices
    const int SLICES = 32;
    sf::VertexArray va(sf::PrimitiveType::Triangles, SLICES * 6);
    for (int i = 0; i < SLICES; ++i) {
        float t0 = (float)i       / (float)SLICES;
        float t1 = (float)(i + 1) / (float)SLICES;
        sf::Color col0 = lerp_color(c1, c2, t0);
        sf::Color col1 = lerp_color(c1, c2, t1);

        sf::Vector2f tl, tr, bl, br;
        if (dir == "horizontal") {
            tl = { pos.x + size.x * t0, pos.y };
            tr = { pos.x + size.x * t1, pos.y };
            bl = { pos.x + size.x * t0, pos.y + size.y };
            br = { pos.x + size.x * t1, pos.y + size.y };
        } else { // vertical (default)
            tl = { pos.x,          pos.y + size.y * t0 };
            tr = { pos.x + size.x, pos.y + size.y * t0 };
            bl = { pos.x,          pos.y + size.y * t1 };
            br = { pos.x + size.x, pos.y + size.y * t1 };
            col0 = lerp_color(c1, c2, t0);
            col1 = lerp_color(c1, c2, t1);
        }
        int base = i * 6;
        va[base+0] = { tl, col0 };
        va[base+1] = { tr, (dir=="horizontal"?col1:col0) };
        va[base+2] = { bl, (dir=="horizontal"?col0:col1) };
        va[base+3] = { tr, (dir=="horizontal"?col1:col0) };
        va[base+4] = { br, col1 };
        va[base+5] = { bl, (dir=="horizontal"?col0:col1) };
    }
    window.draw(va);
    // Draw rounded border on top (transparent fill, just outline)
    if (radius > 0.5f)
        draw_rounded_rect(window, pos, size, radius, sf::Color::Transparent,
                          sf::Color::Transparent, 0.0f);
}

// ============================================================
// === NEW v1.0.9: Shadow Rendering ===========================
// ============================================================
static void draw_shadow(
    sf::RenderWindow& window,
    sf::Vector2f pos, sf::Vector2f size,
    float radius,
    sf::Color shadowCol,
    float blur,
    float offX, float offY)
{
    if (blur <= 0.0f && offX == 0.0f && offY == 0.0f) return;
    int layers = std::max(1, (int)blur);
    for (int i = layers; i >= 1; --i) {
        float ratio = (float)i / (float)layers;
        float expand = ratio * blur * 0.4f;
        sf::Color c = shadowCol;
        c.a = (uint8_t)(shadowCol.a * (1.0f - ratio * 0.6f));
        draw_rounded_rect(window,
            { pos.x + offX - expand*0.5f, pos.y + offY - expand*0.5f },
            { size.x + expand, size.y + expand },
            radius + expand * 0.3f, c, sf::Color::Transparent, 0.0f);
    }
}







































SapphireValue native_ui_button(int arg_count, SapphireValue* args) { return create_declarative_node("Button", arg_count, args); }

SapphireValue native_ui_text(int arg_count, SapphireValue* args) { return create_declarative_node("Text", arg_count, args); }

SapphireValue native_ui_display(int arg_count, SapphireValue* args) { return create_declarative_node("Display", arg_count, args); }

SapphireValue native_ui_flex(int arg_count, SapphireValue* args) { return create_declarative_node("Container", arg_count, args); }

SapphireValue native_ui_checkbox(int arg_count, SapphireValue* args) { return create_declarative_node("Checkbox", arg_count, args); }

SapphireValue native_ui_slider(int arg_count, SapphireValue* args) { return create_declarative_node("Slider", arg_count, args); }

SapphireValue native_ui_input(int arg_count, SapphireValue* args) { return create_declarative_node("Input", arg_count, args); }

SapphireValue native_ui_separator(int arg_count, SapphireValue* args) { return create_declarative_node("Separator", arg_count, args); }

SapphireValue native_ui_menu(int arg_count, SapphireValue* args) { return create_declarative_node("Menu", arg_count, args); }

SapphireValue native_ui_menuitem(int arg_count, SapphireValue* args) { return create_declarative_node("MenuItem", arg_count, args); }

SapphireValue native_ui_grid(int arg_count, SapphireValue* args) { return create_declarative_node("Grid", arg_count, args); }

SapphireValue native_ui_stackpanel(int arg_count, SapphireValue* args) { return create_declarative_node("StackPanel", arg_count, args); }

SapphireValue native_ui_dockpanel(int arg_count, SapphireValue* args) { return create_declarative_node("DockPanel", arg_count, args); }

SapphireValue native_ui_wrappanel(int arg_count, SapphireValue* args) { return create_declarative_node("WrapPanel", arg_count, args); }

SapphireValue native_ui_scrollview(int arg_count, SapphireValue* args) { return create_declarative_node("ScrollView", arg_count, args); }

SapphireValue native_ui_border(int arg_count, SapphireValue* args) { return create_declarative_node("Border", arg_count, args); }

SapphireValue native_ui_image(int arg_count, SapphireValue* args) { return create_declarative_node("Image", arg_count, args); }

SapphireValue native_ui_progressbar(int arg_count, SapphireValue* args) { return create_declarative_node("ProgressBar", arg_count, args); }

SapphireValue native_ui_radiobox(int arg_count, SapphireValue* args) { return create_declarative_node("RadioBox", arg_count, args); }

SapphireValue native_ui_toggleswitch(int arg_count, SapphireValue* args) { return create_declarative_node("ToggleSwitch", arg_count, args); }

SapphireValue native_ui_combobox(int arg_count, SapphireValue* args) { return create_declarative_node("ComboBox", arg_count, args); }

SapphireValue native_ui_listbox(int arg_count, SapphireValue* args) { return create_declarative_node("ListBox", arg_count, args); }

SapphireValue native_ui_passwordbox(int arg_count, SapphireValue* args) { return create_declarative_node("PasswordBox", arg_count, args); }

SapphireValue native_ui_hyperlink(int arg_count, SapphireValue* args) { return create_declarative_node("Hyperlink", arg_count, args); }

SapphireValue native_ui_expander(int arg_count, SapphireValue* args) { return create_declarative_node("Expander", arg_count, args); }

SapphireValue native_ui_datagrid(int arg_count, SapphireValue* args) { return create_declarative_node("DataGrid", arg_count, args); }

SapphireValue native_ui_canvas(int arg_count, SapphireValue* args) { return create_declarative_node("Canvas", arg_count, args); }

SapphireValue native_ui_tooltip(int arg_count, SapphireValue* args) { return create_declarative_node("Tooltip", arg_count, args); }

SapphireValue native_ui_popup(int arg_count, SapphireValue* args) { return create_declarative_node("Popup", arg_count, args); }

SapphireValue native_ui_window(int arg_count, SapphireValue* args) { return create_declarative_node("Window", arg_count, args); }

SapphireValue native_ui_animate(int arg_count, SapphireValue* args) {
    std::cout << "native_ui_animate called with arg_count=" << arg_count << std::endl;
    if (arg_count < 2) return false;
    if (!is_obj_type(args[0], OBJ_STRING)) { std::cout << "arg0 not string" << std::endl; return false; }
    if (!is_obj_type(args[1], OBJ_MAP)) { std::cout << "arg1 not map" << std::endl; return false; }

    std::string id = static_cast<ObjString*>(args[0].as.obj)->chars;
    ObjMap* dict = static_cast<ObjMap*>(args[1].as.obj);
    std::cout << "native_ui_animate id=" << id << std::endl;

    Animation anim;
    anim.id = id;
    if (dict->items.count("duration") && dict->items["duration"].type == ValType::VAL_NUMBER)
        anim.duration = (float)dict->items["duration"].as.number;
    
    if (dict->items.count("loop") && dict->items["loop"].type == ValType::VAL_BOOL)
        anim.loop = dict->items["loop"].as.boolean;
    
    if (dict->items.count("easing") && is_obj_type(dict->items["easing"], OBJ_STRING))
        anim.easing = static_cast<ObjString*>(dict->items["easing"].as.obj)->chars;
    
    if (dict->items.count("keyframes") && is_obj_type(dict->items["keyframes"], OBJ_ARRAY)) {
        auto kfs = static_cast<ObjArray*>(dict->items["keyframes"].as.obj);
        for (auto& kfVal : kfs->elements) {
            if (is_obj_type(kfVal, OBJ_MAP)) {
                ObjMap* kfInst = static_cast<ObjMap*>(kfVal.as.obj);
                Keyframe kf;
                if (kfInst->items.count("time") && kfInst->items["time"].type == ValType::VAL_NUMBER)
                    kf.timeOffset = (float)kfInst->items["time"].as.number;
                
                for (auto& [k, v] : kfInst->items) {
                    if (k == "time") continue;
                    if (v.type == ValType::VAL_NUMBER) {
                        kf.numericProps[k] = (float)v.as.number;
                        std::cout << "Parsed numeric prop: " << k << " = " << kf.numericProps[k] << std::endl;
                    } else if (is_obj_type(v, OBJ_STRING)) {
                        kf.colorProps[k] = hexToColor(static_cast<ObjString*>(v.as.obj)->chars);
                        std::cout << "Parsed color prop: " << k << std::endl;
                    } else {
                        std::cout << "Keyframe value for " << k << " has unknown type." << std::endl;
                    }
                }
                anim.keyframes.push_back(kf);
            } else if (is_obj_type(kfVal, OBJ_INSTANCE)) {
                ObjInstance* kfInst = static_cast<ObjInstance*>(kfVal.as.obj);
                Keyframe kf;
                if (kfInst->fields.count("time") && kfInst->fields["time"].type == ValType::VAL_NUMBER)
                    kf.timeOffset = (float)kfInst->fields["time"].as.number;
                
                for (auto& [k, v] : kfInst->fields) {
                    if (k == "time") continue;
                    if (v.type == ValType::VAL_NUMBER) {
                        kf.numericProps[k] = (float)v.as.number;
                        std::cout << "Parsed numeric prop (instance): " << k << " = " << kf.numericProps[k] << std::endl;
                    } else if (is_obj_type(v, OBJ_STRING)) {
                        kf.colorProps[k] = hexToColor(static_cast<ObjString*>(v.as.obj)->chars);
                        std::cout << "Parsed color prop (instance): " << k << std::endl;
                    } else {
                        std::cout << "Keyframe instance value for " << k << " has unknown type." << std::endl;
                    }
                }
                anim.keyframes.push_back(kf);
            } else {
                std::cout << "kfVal is NOT OBJ_MAP or OBJ_INSTANCE. type index: " << (int)kfVal.type << std::endl;
            }
        }
    }

    g_current_vm->ui_state.animations[id] = anim;
    
    ActiveAnimation aa;
    aa.animId = id;
    aa.elapsedTime = 0.0f;
    g_current_vm->ui_state.activeAnimations[id] = aa;

    return true;
}

SapphireValue native_ui_style(int arg_count, SapphireValue* args) {
    if (arg_count == 0) return {};
    
    if (arg_count == 1 && is_obj_type(args[0], OBJ_STRING)) {
        std::string path = static_cast<ObjString*>(args[0].as.obj)->chars;
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
            styleName = static_cast<ObjString*>(args[0].as.obj)->chars;
        }
        UIStyle style = g_current_vm->ui_state.defaultStyle;
        for (int i = 0; i < arg_count; i++) {
            if (is_obj_type(args[i], OBJ_NAMED_ARG)) {
                ObjNamedArg* narg = static_cast<ObjNamedArg*>(args[i].as.obj);
                std::string key = narg->name->chars;
                if (key == "name" && is_obj_type(narg->value, OBJ_STRING)) styleName = static_cast<ObjString*>(narg->value.as.obj)->chars;
                else if (key == "bgColor" && is_obj_type(narg->value, OBJ_STRING)) style.bgColor = hexToColor(static_cast<ObjString*>(narg->value.as.obj)->chars);
                else if (key == "textColor" && is_obj_type(narg->value, OBJ_STRING)) style.textColor = hexToColor(static_cast<ObjString*>(narg->value.as.obj)->chars);
                else if (key == "accentColor" && is_obj_type(narg->value, OBJ_STRING)) style.accentColor = hexToColor(static_cast<ObjString*>(narg->value.as.obj)->chars);
                else if (key == "hoverColor" && is_obj_type(narg->value, OBJ_STRING)) style.hoverColor = hexToColor(static_cast<ObjString*>(narg->value.as.obj)->chars);
                else if (key == "borderColor" && is_obj_type(narg->value, OBJ_STRING)) style.borderColor = hexToColor(static_cast<ObjString*>(narg->value.as.obj)->chars);
                else if (key == "borderThickness" && narg->value.type == ValType::VAL_NUMBER) style.borderThickness = (float)narg->value.as.number;
                else if (key == "borderRadius" && narg->value.type == ValType::VAL_NUMBER) style.borderRadius = (float)narg->value.as.number;
                else if (key == "padding" && narg->value.type == ValType::VAL_NUMBER) style.padding = (float)narg->value.as.number;
                else if (key == "fontAlias" && is_obj_type(narg->value, OBJ_STRING)) style.fontAlias = static_cast<ObjString*>(narg->value.as.obj)->chars;
                else if (key == "fontSize" && narg->value.type == ValType::VAL_NUMBER) style.fontSize = (unsigned int)narg->value.as.number;
                else if (key == "width" && narg->value.type == ValType::VAL_NUMBER) style.width = (float)narg->value.as.number;
                else if (key == "height" && narg->value.type == ValType::VAL_NUMBER) style.height = (float)narg->value.as.number;
                else if (key == "margin" && narg->value.type == ValType::VAL_NUMBER) style.margin = (float)narg->value.as.number;
                else if (key == "thickness" && narg->value.type == ValType::VAL_NUMBER) style.thickness = (float)narg->value.as.number;
            }
        }
        if (!styleName.empty()) {
            g_current_vm->ui_state.stylesheets[styleName] = style;
        }
    }
    return {};
}

SapphireValue native_ui_get_input_text(int arg_count, SapphireValue* args) {
    if (arg_count == 1 && is_obj_type(args[0], OBJ_STRING)) {
        std::string id = static_cast<ObjString*>(args[0].as.obj)->chars;
        if (g_current_vm->ui_state.inputTexts.count(id)) {
            return new_string(g_current_vm, g_current_vm->ui_state.inputTexts[id]);
        }
    }
    return new_string(g_current_vm, "");
}

SapphireValue native_ui_render(int arg_count, SapphireValue* args) {
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
    auto rootNode = build_ui_tree(static_cast<ObjInstance*>(args[0].as.obj), counter);
    g_current_vm->ui_state.rootNode = rootNode;

    auto now = std::chrono::steady_clock::now();
    float dt = 0.016f;
    if (!g_current_vm->ui_state.firstRender) {
        dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - g_current_vm->ui_state.lastRenderTime).count();
    }
    g_current_vm->ui_state.firstRender = false;
    g_current_vm->ui_state.lastRenderTime = now;

    apply_animations_to_tree(g_current_vm->ui_state.rootNode, dt);

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

    // === NEW v1.0.9: Render notifications (toast) ===
    {
        sf::Vector2u winSz = g_current_vm->sfml_window->getSize();
        float notifW = 280.0f, notifH = 54.0f, notifGap = 10.0f;
        float notifX = winSz.x - notifW - 16.0f;
        float notifY = 16.0f;

        auto& queue = g_current_vm->ui_state.notificationQueue;
        for (auto& n : queue) {
            // Atualiza alpha (fade-in rápido, fade-out no final)
            float fadeIn  = 0.3f;
            float fadeOut = n.lifetime - 0.4f;
            if (n.elapsed < fadeIn)
                n.alpha = (n.elapsed / fadeIn) * 255.0f;
            else if (n.elapsed > fadeOut)
                n.alpha = std::max(0.0f, (1.0f - (n.elapsed - fadeOut) / 0.4f)) * 255.0f;
            else
                n.alpha = 255.0f;

            uint8_t a = (uint8_t)std::clamp(n.alpha, 0.0f, 255.0f);

            sf::Color notifBg(22, 22, 35, a);
            sf::Color notifBorder(60, 60, 80, a);
            sf::Color accentCol;
            if      (n.type == "success") accentCol = sf::Color(34, 197, 94,  a);
            else if (n.type == "error")   accentCol = sf::Color(239, 68, 68,  a);
            else if (n.type == "warning") accentCol = sf::Color(251, 191, 36, a);
            else                          accentCol = sf::Color(99, 102, 241, a); // info

            // Background
            draw_rounded_rect(*g_current_vm->sfml_window, {notifX, notifY}, {notifW, notifH}, 8.0f,
                              notifBg, notifBorder, 1.0f);
            // Barra colorida esquerda
            draw_rounded_rect(*g_current_vm->sfml_window, {notifX, notifY}, {4.0f, notifH}, 4.0f,
                              accentCol, sf::Color::Transparent, 0.0f);

            // Texto da mensagem
            std::string fa = "default";
            if (g_current_vm->ui_state.fontStack.count(fa)) {
                sf::Text t(g_current_vm->ui_state.fontStack[fa], n.message, 14);
                t.setFillColor(sf::Color(220, 220, 230, a));
                t.setPosition({notifX + 14.0f, notifY + notifH / 2.0f - 9.0f});
                g_current_vm->sfml_window->draw(t);
            }
            notifY += notifH + notifGap;
        }

        // Avança tempo e remove expiradas
        for (auto& n : queue) n.elapsed += dt;
        queue.erase(std::remove_if(queue.begin(), queue.end(),
            [](const NotificationEntry& n) { return n.elapsed >= n.lifetime; }), queue.end());
    }

    return {};
}




UIStyle resolve_style(const std::string& id, const std::string& styleName) {
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

SapphireValue create_declarative_node(const std::string& type, int arg_count, SapphireValue* args) {
    ObjInstance* node = new_instance(g_current_vm, g_current_vm->ui_component_class);
    node->fields["type"] = new_string(g_current_vm, type);
    
    if (arg_count > 0 && is_obj_type(args[0], OBJ_MAP)) {
        ObjMap* map = static_cast<ObjMap*>(args[0].as.obj);
        for (auto& pair : map->items) {
            node->fields[pair.first] = pair.second;
        }
    }
    // Check if the first argument is positional (not named), just in case backward compatibility is needed
    if (arg_count > 0 && is_obj_type(args[0], OBJ_STRING)) {
        if (type == "Text" || type == "Display") node->fields["text"] = new_string(g_current_vm, valueToStringC(args[0]));
        else node->fields["label"] = new_string(g_current_vm, valueToStringC(args[0]));
    }
    
    // Parse all named arguments (overriding positional if specified)
    for (int i = 0; i < arg_count; i++) {
        if (is_obj_type(args[i], OBJ_NAMED_ARG)) {
            ObjNamedArg* narg = static_cast<ObjNamedArg*>(args[i].as.obj);
            node->fields[narg->name->chars] = narg->value;
        }
    }
    return node;
}

// ============================================================
// === NEW v1.0.9: Novos Componentes Nativos ==================
// ============================================================

// Card — container elevado com gradiente e sombra embutidos
SapphireValue native_ui_card(int arg_count, SapphireValue* args) {
    return create_declarative_node("Card", arg_count, args);
}

// Badge — bolinha com número
SapphireValue native_ui_badge(int arg_count, SapphireValue* args) {
    return create_declarative_node("Badge", arg_count, args);
}

// Tag / Chip — etiqueta colorida em cápsula
SapphireValue native_ui_tag(int arg_count, SapphireValue* args) {
    return create_declarative_node("Tag", arg_count, args);
}

// Stepper — indicador de progresso em etapas
SapphireValue native_ui_stepper(int arg_count, SapphireValue* args) {
    return create_declarative_node("Stepper", arg_count, args);
}

// Spinner — arco giratório animado
SapphireValue native_ui_spinner(int arg_count, SapphireValue* args) {
    return create_declarative_node("Spinner", arg_count, args);
}

// Notify — enfileira uma notificação toast
// Notify(message, type)   -- type: "success" | "error" | "warning" | "info"
SapphireValue native_ui_notify(int arg_count, SapphireValue* args) {
    if (arg_count < 1) return {};
    std::string message = "";
    std::string type    = "info";

    if (is_obj_type(args[0], OBJ_STRING))
        message = static_cast<ObjString*>(args[0].as.obj)->chars;

    if (arg_count >= 2 && is_obj_type(args[1], OBJ_STRING))
        type = static_cast<ObjString*>(args[1].as.obj)->chars;

    // Também aceita argumentos nomeados: Notify(message="...", type="success")
    for (int i = 0; i < arg_count; i++) {
        if (is_obj_type(args[i], OBJ_NAMED_ARG)) {
            ObjNamedArg* narg = static_cast<ObjNamedArg*>(args[i].as.obj);
            std::string key = narg->name->chars;
            if (key == "message" && is_obj_type(narg->value, OBJ_STRING))
                message = static_cast<ObjString*>(narg->value.as.obj)->chars;
            else if (key == "type" && is_obj_type(narg->value, OBJ_STRING))
                type = static_cast<ObjString*>(narg->value.as.obj)->chars;
        }
    }

    if (!message.empty()) {
        NotificationEntry entry;
        entry.message  = message;
        entry.type     = type;
        entry.lifetime = 3.0f;
        entry.elapsed  = 0.0f;
        entry.alpha    = 0.0f;
        g_current_vm->ui_state.notificationQueue.push_back(entry);
    }
    return {};
}

