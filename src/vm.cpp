// TODO: Implementar JIT para otimizar a performance e implementar NaN Tagging.
// TODO: Criar novas coisas para jogos tipo Vector2D, Vector3D, Matrix, etc.

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

static UIStyle* get_style() {
    return g_current_vm->ui_state.activeStyle ? g_current_vm->ui_state.activeStyle : &g_current_vm->ui_state.defaultStyle;
}

// Total de horas desperdiçadas consertando draw_rounded_rect:

int total_hours_wasted_on_draw_rounded_rect = 14;

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

static void sapphire_render_text(sf::RenderWindow& window, const std::string& content, sf::Vector2f pos, sf::Color color, std::string fontAlias, int fontSize) {
    UIStyle* s = get_style();
    std::string finalAlias = (fontAlias != "") ? fontAlias : s->fontAlias;

    if (g_current_vm->ui_state.fontStack.find(finalAlias) == g_current_vm->ui_state.fontStack.end()) {
        finalAlias = "default";
    }

    // Se nem a default existe, abortamos para não desenhar lixo/pontinhos
    if (g_current_vm->ui_state.fontStack.count(finalAlias) == 0) return;

    sf::Text text(g_current_vm->ui_state.fontStack[finalAlias], content, (unsigned int)fontSize);
    text.setFillColor(color);
    text.setPosition(pos);
    window.draw(text);
}

// Helper para gerar IDs únicos se o usuário não fornecer
static std::string generate_node_id(const std::string& base) {
    static int counter = 0;
    return base + "_" + std::to_string(counter++);
}

static void calculate_flex_layout(LayoutNode* node, sf::Vector2f startPos) {
    if (!node) return;

    node->pos = startPos;

    // Se for Separator, definimos um tamanho fixo visual (ex: largura total, 10px altura)
    if (node->type == NODE_SEPARATOR) {
        float parentWidth = (node->parent) ? node->parent->size.x : g_current_vm->config.windowWidth;
        node->size = {parentWidth - (node->style.padding * 2), 10.0f}; // 10px de altura pro separador
        return;
    }

    // Se não for container/grid, assumimos que o tamanho já foi setado na criação (Button/Input)
    if (node->type != NODE_CONTAINER && node->type != NODE_GRID) return;

    float currentX = startPos.x + node->style.padding;
    float currentY = startPos.y + node->style.padding;
    float maxYInRow = 0.0f;
    float maxWidth = 0.0f;
    float totalHeight = 0.0f;

    // --- LÓGICA DE GRID (Colunas Fixas) ---
    if (node->type == NODE_GRID) {
        int cols = (int)node->style.width; // Usamos a propriedade width como numero de colunas
        if (cols < 1) cols = 1;

        // Largura da célula calculada dinamicamente ou fixa pelos filhos?
        // Vamos assumir que os filhos tem tamanho fixo por enquanto
        int index = 0;
        float rowHeight = 0.0f;

        // Posição inicial relativa ao grid
        float startGridX = currentX;
        float startGridY = currentY;

        for (auto child : node->children) {
            // Calcula posição baseada na coluna/linha
            int col = index % cols;
            int row = index / cols;

            // Se mudou de linha, avança Y
            if (col == 0 && row > 0) {
                 startGridY += rowHeight + node->style.gap;
                 rowHeight = 0.0f; // Reseta altura da linha
                 startGridX = currentX; // Volta pro começo da linha
            }

            // Define posição do filho
            child->pos = {startGridX + (col * (child->size.x + node->style.gap)), startGridY};

            // Recursão (caso tenha filhos dentro do grid)
            calculate_flex_layout(child, child->pos);

            rowHeight = std::max(rowHeight, child->size.y);
            maxWidth = std::max(maxWidth, (child->pos.x - startPos.x) + child->size.x);

            index++;
        }
        totalHeight = (startGridY + rowHeight) - startPos.y;
    }
    // --- LÓGICA DE FLEX (Row/Column) ---
    else {
        // ROW (0)
        if (node->style.layoutDirection == 0) {
            float availableWidth = node->style.width > 0 ? node->style.width : g_current_vm->config.windowWidth;

            for (auto child : node->children) {
                calculate_flex_layout(child, {currentX, currentY});

                // Wrap logic (quebra de linha se passar da largura)
                if (currentX + child->size.x > startPos.x + availableWidth - node->style.padding) {
                    currentX = startPos.x + node->style.padding;
                    currentY += maxYInRow + node->style.gap;
                    totalHeight += maxYInRow + node->style.gap;
                    maxYInRow = 0.0f;

                    child->pos = {currentX, currentY};
                    calculate_flex_layout(child, {currentX, currentY});
                }

                currentX += child->size.x + node->style.gap;
                maxYInRow = std::max(maxYInRow, child->size.y);
                maxWidth = std::max(maxWidth, currentX - startPos.x);
            }
            totalHeight += maxYInRow;
        }
        // COLUMN (1)
        else {
            for (auto child : node->children) {
                child->pos = {currentX, currentY};
                calculate_flex_layout(child, {currentX, currentY});

                currentY += child->size.y + node->style.gap;
                maxWidth = std::max(maxWidth, child->size.x);
            }
            totalHeight = currentY - startPos.y;
        }
    }

    // Auto-sizing do container
    if (node->style.width <= 0 && node->type != NODE_GRID) node->size.x = maxWidth + node->style.padding;
    else if (node->type != NODE_GRID) node->size.x = node->style.width;
    // Grid width já é tratado diferente, mas podemos definir um tamanho visual se quiser

    if (node->style.height <= 0) node->size.y = totalHeight + node->style.padding * 2;
    else node->size.y = node->style.height;
}

static void render_node_tree(LayoutNode* node, sf::RenderWindow& window) {
    if (!node) return;

    g_current_vm->ui_state.widgetBounds[node->id] = sf::FloatRect(node->pos, node->size);

    if (node->type == NODE_CONTAINER || node->type == NODE_GRID) {
        if (node->style.bgColor.a > 0) {
            draw_rounded_rect(window, node->pos, node->size, node->style.borderRadius, node->style.bgColor, node->style.borderColor, node->style.borderThickness);
        }
    }
    else if (node->type == NODE_SEPARATOR) {
        sf::RectangleShape line({node->size.x, 1.0f});
        line.setPosition({node->pos.x, node->pos.y + (node->size.y / 2)});
        line.setFillColor(sf::Color(150, 150, 150, 80));
        window.draw(line);
    }
    else if (node->type == NODE_BUTTON) {
        sf::Vector2i m = sf::Mouse::getPosition(window);
        bool hovered = sf::FloatRect(node->pos, node->size).contains(sf::Vector2f((float)m.x, (float)m.y));

        draw_rounded_rect(window, node->pos, node->size, node->style.borderRadius, hovered ? node->style.hoverColor : node->style.bgColor, node->style.accentColor, node->style.borderThickness);

        float textX = node->pos.x + node->style.padding;
        float textY = node->pos.y + (node->size.y / 2.0f) - (node->style.fontSize / 2.0f);
        sapphire_render_text(window, node->text, {textX, textY}, node->style.textColor, node->style.fontAlias, node->style.fontSize);
    }
    else if (node->type == NODE_LABEL) {
        sapphire_render_text(window, node->text, node->pos, node->style.textColor, node->style.fontAlias, node->style.fontSize);
    }
    else if (node->type == NODE_INPUT) {
         draw_rounded_rect(window, node->pos, node->size, node->style.borderRadius, node->style.bgColor, node->style.accentColor, node->style.borderThickness);

         std::string displayText = node->text;
         if (node->id == g_current_vm->ui_state.lastComponentId) {
             displayText += "|";
         }

         float textY = node->pos.y + (node->size.y / 2.0f) - (node->style.fontSize / 2.0f);
         sapphire_render_text(window, displayText, {node->pos.x + node->style.padding, textY}, node->style.textColor, node->style.fontAlias, node->style.fontSize);
    }
    else if (node->type == NODE_CHECKBOX) {
         sf::Vector2i m = sf::Mouse::getPosition(window);
         bool hovered = sf::FloatRect(node->pos, node->size).contains(sf::Vector2f((float)m.x, (float)m.y));
         draw_rounded_rect(window, node->pos, node->size, node->style.borderRadius, hovered ? node->style.hoverColor : node->style.bgColor, node->style.accentColor, node->style.borderThickness);
         if (node->checked) {
             draw_rounded_rect(window, {node->pos.x + 4, node->pos.y + 4}, {node->size.x - 8, node->size.y - 8}, 2.0f, node->style.accentColor, sf::Color::Transparent, 0);
         }
         sapphire_render_text(window, node->text, {node->pos.x + node->size.x + 10.0f, node->pos.y}, node->style.textColor, node->style.fontAlias, 14);
    }

    for (auto child : node->children) {
        render_node_tree(child, window);
    }
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

static UIStyle resolve_style(const std::string& id) {
    UIStyle base = g_current_vm->ui_state.activeStyle ? *g_current_vm->ui_state.activeStyle : g_current_vm->ui_state.defaultStyle;

    auto it_global = g_current_vm->ui_state.stylesheets.find(id);
    if (it_global != g_current_vm->ui_state.stylesheets.end()) {
        base = it_global->second;
    }

    auto it_override = g_current_vm->ui_state.idOverrides.find(id);
    if (it_override != g_current_vm->ui_state.idOverrides.end()) {
        const auto& props = it_override->second;
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

    Obj** obj_ptr = std::get_if<Obj*>(&args[0]._value);
    if (!obj_ptr) return {};

    std::string path = static_cast<ObjString*>(*obj_ptr)->chars;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size <= 0) return new_string(g_current_vm, "");

    std::string buffer;
    buffer.resize(static_cast<size_t>(size));
    if (!file.read(&buffer[0], size)) return {};

    // Remove todos os '\r' para que o split("\n") funcione independente do SO
    buffer.erase(std::remove(buffer.begin(), buffer.end(), '\r'), buffer.end());

    return new_string(g_current_vm, buffer);
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
} // Se no CMD não aparecer cor, é porque o CMD não suporta ANSI aí aparece a cor literalmente, eu uso o Windows Terminal

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

static SapphireValue native_string_upper(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !is_obj_type(args[0], OBJ_STRING)) return {}; // Proteção

    ObjString* str_obj = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    std::string s = str_obj->chars;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return new_string(g_current_vm, s);
}

// 2. String.lower(str)
static SapphireValue native_string_lower(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !is_obj_type(args[0], OBJ_STRING)) return {};

    ObjString* str_obj = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    std::string s = str_obj->chars;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return new_string(g_current_vm, s);
}

// 3. String.trim(str)
static SapphireValue native_string_trim(int arg_count, SapphireValue* args) {
    if (arg_count < 1) return {};

    // Tenta pegar o ponteiro do Obj* com segurança
    Obj** obj_ptr = std::get_if<Obj*>(&args[0]._value);

    // Se não for um objeto OU se o objeto não for do tipo STRING, retorna nil em vez de crashar
    if (!obj_ptr || (*obj_ptr)->type != OBJ_STRING) {
        return {};
    }

    std::string s = static_cast<ObjString*>(*obj_ptr)->chars;
    const char* ws = " \t\n\r\f\v";
    s.erase(s.find_last_not_of(ws) + 1);
    s.erase(0, s.find_first_not_of(ws));
    return new_string(g_current_vm, s);
}

// 4. String.contains(str, sub)
static SapphireValue native_string_contains(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) return false;
    std::string haystack = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    std::string needle = static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars;
    return haystack.find(needle) != std::string::npos;
}

// 5. String.replace(str, old, new)
static SapphireValue native_string_replace(int arg_count, SapphireValue* args) {
    if (arg_count < 3 || !is_obj_type(args[0], OBJ_STRING) ||
        !is_obj_type(args[1], OBJ_STRING) || !is_obj_type(args[2], OBJ_STRING)) {
        return args[0];
    }

    std::string s = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    std::string from = static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars;
    std::string to = static_cast<ObjString*>(std::get<Obj*>(args[2]._value))->chars;

    if (from.empty()) return args[0];
    size_t start_pos = 0;
    while ((start_pos = s.find(from, start_pos)) != std::string::npos) {
        s.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return new_string(g_current_vm, s);
}

// 6. String.sub(str, start, length)
static SapphireValue native_string_sub(int arg_count, SapphireValue* args) {
    if (arg_count < 2) return new_string(g_current_vm, "");

    Obj** obj_ptr = std::get_if<Obj*>(&args[0]._value);
    double* start_ptr = std::get_if<double>(&args[1]._value);

    if (!obj_ptr || !(*obj_ptr) || (*obj_ptr)->type != OBJ_STRING || !start_ptr) {
        return new_string(g_current_vm, "");
    }

    std::string s = static_cast<ObjString*>(*obj_ptr)->chars;
    int start = (int)*start_ptr;
    int s_len = (int)s.length();

    if (start < 0 || start >= s_len) return new_string(g_current_vm, "");

    int len_val = s_len - start;
    if (arg_count == 3) {
        if (double* custom_len = std::get_if<double>(&args[2]._value)) {
            len_val = (int)*custom_len;
        }
    }

    if (len_val < 0) len_val = 0;
    if (start + len_val > s_len) len_val = s_len - start;

    return new_string(g_current_vm, s.substr(start, len_val));
}

// 7. String.split(str, delimiter) -> Retorna Array
static SapphireValue native_string_split(int arg_count, SapphireValue* args) {
    if (arg_count < 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) {
        return std::make_shared<SapphireArray>(); // Retorna lista vazia em vez de crashar
    }

    std::string s = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    std::string delimiter = static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars;

    auto array_obj = std::make_shared<SapphireArray>();
    if (delimiter.empty()) {
        array_obj->elements.push_back(new_string(g_current_vm, s));
        return array_obj;
    }

    size_t pos = 0;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        array_obj->elements.push_back(new_string(g_current_vm, s.substr(0, pos)));
        s.erase(0, pos + delimiter.length());
    }
    array_obj->elements.push_back(new_string(g_current_vm, s));

    return array_obj;
}

// 8. String.startsWith(str, prefix)
static SapphireValue native_string_starts_with(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) return false;
    std::string s = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    std::string prefix = static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars;
    return s.rfind(prefix, 0) == 0;
}

// 9. String.endsWith(str, suffix)
static SapphireValue native_string_ends_with(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) return false;
    std::string s = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    std::string suffix = static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars;
    if (suffix.length() > s.length()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}

// 10. String.index(str, needle)
static SapphireValue native_string_index_of(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) return -1.0;
    std::string s = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    std::string needle = static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars;
    size_t pos = s.find(needle);
    if (pos == std::string::npos) return -1.0;
    return (double)pos;
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


// TODO: Adicionar mais funções nativas aqui


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

    g_current_vm->ui_state.occupiedAreas.clear();
    g_current_vm->ui_state.nextPosX = 20.0f;
    g_current_vm->ui_state.nextPosY = 40.0f;
    g_current_vm->ui_state.lastItemHeight = 0.0f;

    g_current_vm->ui_state.nodeStack.clear();

    if (g_current_vm->ui_state.rootLayout) {
        delete g_current_vm->ui_state.rootLayout;
        g_current_vm->ui_state.rootLayout = nullptr;
    }

    return {};
}

static SapphireValue native_ui_style_component(int arg_count, SapphireValue* args) {
    std::string componentId = "";

    for (int i = 0; i < arg_count; i++) {
        if (!is_obj_type(args[i], OBJ_STRING)) continue;
        std::string s = static_cast<ObjString*>(std::get<Obj*>(args[i]._value))->chars;

        size_t idPos = s.find("id:");
        if (idPos != std::string::npos) {
            size_t start = idPos + 3;
            size_t end = s.find(';', start);
            if (end == std::string::npos) end = s.length();
            componentId = g_current_vm->trim(s.substr(start, end - start));
            break;
        }
    }

    if (componentId.empty()) return {};

    // Corrigindo o erro de UIStyle& vs ComponentProps
    // Primeiro, aplicamos ao override de propriedades
    ComponentProps& props = g_current_vm->ui_state.idOverrides[componentId];

    // Para usar apply_style_property, precisamos de um UIStyle temporário
    // ou mapear manualmente. Como apply_style_property altera UIStyle,
    // vamos garantir que o idOverrides e o stylesheets fiquem em sincronia.
    UIStyle& targetStyle = g_current_vm->ui_state.stylesheets[componentId];

    for (int i = 0; i < arg_count; i++) {
        std::string dummy;
        g_current_vm->apply_style_property(targetStyle, dummy, static_cast<ObjString*>(std::get<Obj*>(args[i]._value))->chars);

        // Sincroniza campos básicos com o ComponentProps
        props.bgColor = targetStyle.bgColor;
        props.textColor = targetStyle.textColor;
        props.borderRadius = targetStyle.borderRadius;
        props.fontSize = targetStyle.fontSize;
        props.padding = targetStyle.padding;
    }
    return {};
}



static SapphireValue native_ui_set_bg_color(int arg_count, SapphireValue* args) {
    if (arg_count < 1) return {};
    std::string hex = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    g_current_vm->ui_state.currentStyleColor = hexToColor(hex);
    return {};
}

static SapphireValue native_ui_button(int arg_count, SapphireValue* args) {
    if (arg_count < 2) return false;
    ObjString* label = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    std::string id = static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars;
    UIStyle s = resolve_style(id);

    float btnW = s.width;
    float btnH = s.height;
    unsigned int dynamicFontSize = s.fontSize;

    if (label->chars.length() > 0) {
        float availableW = btnW - (s.padding * 2);
        float availableH = btnH - (s.padding * 2);
        float sizeByWidth = (availableW / (float)label->chars.length()) * 1.6f;
        float sizeByHeight = availableH * 0.8f;
        dynamicFontSize = static_cast<unsigned int>(std::min(sizeByWidth, sizeByHeight));
        if (s.fontSize > 0 && dynamicFontSize > s.fontSize) dynamicFontSize = s.fontSize;
        if (dynamicFontSize < 8) dynamicFontSize = 8;
    }

    if (!g_current_vm->ui_state.nodeStack.empty()) {
        LayoutNode* node = new LayoutNode();
        node->type = NODE_BUTTON;
        node->text = label->chars;
        node->id = id;
        node->style = s;
        node->style.fontSize = dynamicFontSize;
        node->size = {btnW, btnH};
        node->parent = g_current_vm->ui_state.nodeStack.back();
        g_current_vm->ui_state.nodeStack.back()->children.push_back(node);

        if (g_current_vm->ui_state.widgetBounds.count(id)) {
            sf::FloatRect bounds = g_current_vm->ui_state.widgetBounds[id];
            sf::Vector2i m = sf::Mouse::getPosition(*g_current_vm->sfml_window);
            if (bounds.contains(sf::Vector2f((float)m.x, (float)m.y)) && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
                auto now = std::chrono::steady_clock::now();
                if ((now - g_current_vm->ui_state.lastClickTime).count() > g_current_vm->ui_state.debounceTime) {
                    g_current_vm->ui_state.lastClickTime = now;
                    return true;
                }
            }
        }
        return false;
    }

    sf::Vector2f pos = g_current_vm->calculate_element_pos(btnW, btnH);
    sf::Vector2i m = sf::Mouse::getPosition(*g_current_vm->sfml_window);
    bool hovered = sf::FloatRect(pos, {btnW, btnH}).contains(sf::Vector2f((float)m.x, (float)m.y));

    draw_rounded_rect(*g_current_vm->sfml_window, pos, {btnW, btnH}, s.borderRadius, hovered ? s.hoverColor : s.bgColor, s.accentColor, s.borderThickness);

    sf::Text tempText(g_current_vm->ui_state.fontStack[s.fontAlias != "" ? s.fontAlias : "default"], label->chars, dynamicFontSize);
    sf::FloatRect textBounds = tempText.getLocalBounds();

    // SFML 3.0 usa .position e .size
    tempText.setOrigin({textBounds.position.x + textBounds.size.x / 2.0f, textBounds.position.y + textBounds.size.y / 2.0f});
    tempText.setPosition({pos.x + (btnW / 2.0f), pos.y + (btnH / 2.0f)});
    tempText.setFillColor(s.textColor);

    g_current_vm->sfml_window->draw(tempText);

    if (hovered && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        auto now = std::chrono::steady_clock::now();
        if ((now - g_current_vm->ui_state.lastClickTime).count() > g_current_vm->ui_state.debounceTime) {
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
    UIStyle style = g_current_vm->ui_state.defaultStyle;
    std::string styleName = "";

    for (int i = 0; i < arg_count; i++) {
        if (!is_obj_type(args[i], OBJ_STRING)) continue;
        std::string input = static_cast<ObjString*>(std::get<Obj*>(args[i]._value))->chars;
        g_current_vm->apply_style_property(style, styleName, input);
    }

    if (!styleName.empty()) {
        g_current_vm->ui_state.stylesheets[styleName] = style;
    }
    return {};
}

std::string VM::trim(const std::string& s) {
    size_t first = s.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) return s;
    size_t last = s.find_last_not_of(" \t\n\r");
    return s.substr(first, (last - first + 1));
}

void VM::apply_style_property(UIStyle& style, std::string& id_out, const std::string& full_str) {
    std::stringstream ss(full_str);
    std::string segment;

    while (std::getline(ss, segment, ';')) {
        size_t colonPos = segment.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = trim(segment.substr(0, colonPos));
        std::string val = trim(segment.substr(colonPos + 1));

        if (key == "id") id_out = val;
        else if (key == "bg") style.bgColor = hexToColor(val);
        else if (key == "color") style.textColor = hexToColor(val);
        else if (key == "accent") style.accentColor = hexToColor(val);
        else if (key == "thickness") style.borderThickness = std::stof(val);
        else if (key == "radius") style.borderRadius = std::stof(val);
        else if (key == "padding") style.padding = std::stof(val);
        else if (key == "font-size") style.fontSize = (unsigned int)std::stoi(val);
        else if (key == "font") style.fontAlias = val;
        else if (key == "width" || key == "w") style.width = std::stof(val);
        else if (key == "height" || key == "h") style.height = std::stof(val);
        else if (key == "gap") style.gap = std::stof(val);
        else if (key == "direction") {
            if (val == "row") style.layoutDirection = 0;
            else if (val == "column") style.layoutDirection = 1;
        }
    }
}

sf::Vector2f VM::calculate_element_pos(float w, float h) {
    // Se não houver container, comporta-se como um bloco simples (fallback)
    if (layoutStack.empty()) {
        sf::Vector2f pos = { ui_state.nextPosX, ui_state.nextPosY };
        ui_state.nextPosY += h + ui_state.defaultStyle.padding;
        return pos;
    }

    // Retornamos um vetor vazio, pois a posição final será calculada no EndFlex
    return { 0, 0 };
}

static SapphireValue native_ui_begin_flex(int arg_count, SapphireValue* args) {
    LayoutNode* node = new LayoutNode();
    node->type = NODE_CONTAINER;
    node->style = g_current_vm->ui_state.defaultStyle;

    std::string styleId;
    for (int i = 0; i < arg_count; i++) {
        if (is_obj_type(args[i], OBJ_STRING))
            g_current_vm->apply_style_property(node->style, styleId, static_cast<ObjString*>(std::get<Obj*>(args[i]._value))->chars);
    }

    node->id = styleId.empty() ? generate_node_id("flex") : styleId;

    if (!g_current_vm->ui_state.nodeStack.empty()) {
        node->parent = g_current_vm->ui_state.nodeStack.back();
        g_current_vm->ui_state.nodeStack.back()->children.push_back(node);
    } else {
        g_current_vm->ui_state.rootLayout = node;
    }

    g_current_vm->ui_state.nodeStack.push_back(node);
    return {};
}

static SapphireValue native_ui_end_flex(int arg_count, SapphireValue* args) {
    if (g_current_vm->ui_state.nodeStack.empty()) return {};

    LayoutNode* finishedNode = g_current_vm->ui_state.nodeStack.back();
    g_current_vm->ui_state.nodeStack.pop_back();

    if (g_current_vm->ui_state.nodeStack.empty()) {
        calculate_flex_layout(finishedNode, {g_current_vm->ui_state.nextPosX, g_current_vm->ui_state.nextPosY});
        render_node_tree(finishedNode, *g_current_vm->sfml_window);

        g_current_vm->ui_state.nextPosY += finishedNode->size.y + 10.0f;
        g_current_vm->ui_state.nextPosX = 20.0f;
    }

    return {};
}
static SapphireValue native_ui_begin_grid(int arg_count, SapphireValue* args) {
    LayoutNode* node = new LayoutNode();
    node->type = NODE_GRID; // Usa o novo tipo
    node->style = g_current_vm->ui_state.defaultStyle;
    node->style.width = 2; // Default colunas

    std::string styleId;
    for (int i = 0; i < arg_count; i++) {
        if (is_obj_type(args[i], OBJ_STRING))
            g_current_vm->apply_style_property(node->style, styleId, static_cast<ObjString*>(std::get<Obj*>(args[i]._value))->chars);
    }
    node->id = styleId.empty() ? generate_node_id("grid") : styleId;

    if (!g_current_vm->ui_state.nodeStack.empty()) {
        node->parent = g_current_vm->ui_state.nodeStack.back();
        g_current_vm->ui_state.nodeStack.back()->children.push_back(node);
    } else {
        // Grid na raiz (raro mas possível)
        g_current_vm->ui_state.rootLayout = node;
    }

    // Empilha para que os próximos botões sejam filhos deste grid
    g_current_vm->ui_state.nodeStack.push_back(node);
    return {};
}

static SapphireValue native_ui_end_grid(int arg_count, SapphireValue* args) {
    if (!g_current_vm->ui_state.nodeStack.empty()) {
        g_current_vm->ui_state.nodeStack.pop_back();
    }
    return {};
}


static SapphireValue native_ui_text(int arg_count, SapphireValue* args) {
    if (arg_count < 1) return {};
    std::string content = static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars;
    std::string id = (arg_count >= 2) ? static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars : generate_node_id("txt");
    UIStyle s = resolve_style(id);

    sf::Text tempText(g_current_vm->ui_state.fontStack["default"], content, s.fontSize);
    float textW = tempText.getLocalBounds().size.x + s.padding;
    float textH = (float)s.fontSize + s.padding;

    if (!g_current_vm->ui_state.nodeStack.empty()) {
        LayoutNode* node = new LayoutNode();
        node->type = NODE_LABEL;
        node->text = content;
        node->id = id;
        node->style = s;
        node->size = {textW, textH};
        node->parent = g_current_vm->ui_state.nodeStack.back();
        g_current_vm->ui_state.nodeStack.back()->children.push_back(node);
        return {};
    }

    sf::Vector2f pos = g_current_vm->calculate_element_pos(textW, textH);
    sapphire_render_text(*g_current_vm->sfml_window, content, pos, s.textColor, s.fontAlias, s.fontSize);
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
    if (g_current_vm->ui_state.nodeStack.empty()) return {}; // Ignora se fora de layout

    LayoutNode* node = new LayoutNode();
    node->type = NODE_SEPARATOR;
    node->style = g_current_vm->ui_state.defaultStyle; // Pega padding/margin default

    node->parent = g_current_vm->ui_state.nodeStack.back();
    g_current_vm->ui_state.nodeStack.back()->children.push_back(node);

    return {};
}

static SapphireValue native_ui_spacing(int arg_count, SapphireValue* args) {
    // Spacing agora atua como um elemento invisível de 20px de altura
    g_current_vm->calculate_element_pos(0.0f, 20.0f);
    return {};
}

static SapphireValue native_ui_pop_style(int arg_count, SapphireValue* args) {
    g_current_vm->ui_state.activeStyle = nullptr;
    return {};
}
static SapphireValue native_ui_checkbox(int arg_count, SapphireValue* args) {
    if (arg_count < 3) return false;
    ObjString* label = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    bool checked = std::get<bool>(args[1]._value);
    std::string id = static_cast<ObjString*>(std::get<Obj*>(args[2]._value))->chars;
    UIStyle s = resolve_style(id);

    float size = 20.0f;
    sf::Vector2f pos = g_current_vm->calculate_element_pos(size + 100.0f, size);

    sf::Vector2i m = sf::Mouse::getPosition(*g_current_vm->sfml_window);
    bool hovered = sf::FloatRect(pos, {size, size}).contains(sf::Vector2f((float)m.x, (float)m.y));

    if (hovered && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        auto now = std::chrono::steady_clock::now();
        if ((now - g_current_vm->ui_state.lastClickTime).count() > g_current_vm->ui_state.debounceTime) {
            g_current_vm->ui_state.lastClickTime = now;
            checked = !checked;
        }
    }

    draw_rounded_rect(*g_current_vm->sfml_window, pos, {size, size}, s.borderRadius,
                      hovered ? s.hoverColor : s.bgColor, s.accentColor, s.borderThickness);
    if (checked) {
        draw_rounded_rect(*g_current_vm->sfml_window, {pos.x + 4, pos.y + 4}, {size - 8, size - 8}, 2.0f, s.accentColor, sf::Color::Transparent, 0);
    }
    sapphire_render_text(*g_current_vm->sfml_window, label->chars, {pos.x + size + 10.0f, pos.y}, s.textColor, s.fontAlias, 14);
    return checked;
}
static bool g_menu_open = false;
static std::string g_active_menu = "";

static SapphireValue native_ui_menubar(int arg_count, SapphireValue* args) {
    std::string id = (arg_count >= 1) ? static_cast<ObjString*>(std::get<Obj*>(args[0]._value))->chars : "menubar";
    UIStyle s = resolve_style(id);

    float windowW = (float)g_current_vm->config.windowWidth;
    if (windowW <= 0) windowW = 800.0f;

    sf::RectangleShape bar({windowW, 30.0f});
    bar.setPosition({0, 0});
    bar.setFillColor(s.bgColor);
    g_current_vm->sfml_window->draw(bar);

    g_current_vm->ui_state.nextPosX = 10.0f;
    g_current_vm->ui_state.nextPosY = 5.0f;
    return {};
}

static SapphireValue native_ui_menu(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !is_obj_type(args[0], OBJ_STRING)) return false;
    ObjString* label = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    std::string id = (arg_count >= 2) ? static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars : "menu";

    UIStyle s = resolve_style(id);

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
    sf::Color bgColor = (hovered || isOpen) ? s.hoverColor : sf::Color::Transparent;

    draw_rounded_rect(*g_current_vm->sfml_window, pos, {itemWidth, itemHeight}, s.borderRadius * 0.4f, bgColor, sf::Color::Transparent, 0);

    txt.setPosition({pos.x + paddingX, pos.y + (itemHeight / 2.0f) - 10.0f});
    txt.setFillColor(s.textColor);
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
    std::string id = (arg_count >= 2) ? static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars : "menuitem";

    UIStyle s = resolve_style(id);

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
    rect.setFillColor(hovered ? s.accentColor : s.bgColor);
    rect.setOutlineThickness(s.borderThickness);
    rect.setOutlineColor(s.borderColor);
    g_current_vm->sfml_window->draw(rect);

    sf::Text txt(g_current_vm->sapphire_font, label->chars, 13);
    txt.setPosition({pos.x + 10.0f, pos.y + 5.0f});
    txt.setFillColor(hovered ? sf::Color::White : s.textColor);
    g_current_vm->sfml_window->draw(txt);

    g_current_vm->ui_state.menuOffsetY += height;

    return clicked;
}

static SapphireValue native_ui_slider(int arg_count, SapphireValue* args) {
    if (arg_count < 4) return 0.0;
    float val = (float)std::get<double>(args[0]._value);
    float min = (float)std::get<double>(args[1]._value);
    float max = (float)std::get<double>(args[2]._value);
    std::string id = static_cast<ObjString*>(std::get<Obj*>(args[3]._value))->chars;
    UIStyle s = resolve_style(id);

    float width = s.width;
    sf::Vector2f pos = g_current_vm->calculate_element_pos(width, 25.0f);

    sf::RectangleShape bar({width, 6.0f});
    bar.setPosition({pos.x, pos.y + 10});
    bar.setFillColor(s.accentColor);
    g_current_vm->sfml_window->draw(bar);

    float handlePos = ((val - min) / (max - min)) * width;
    sf::CircleShape handle(8.0f);
    handle.setOrigin({8.0f, 8.0f});
    handle.setPosition({pos.x + handlePos, pos.y + 13.0f});
    handle.setFillColor(sf::Color::White);

    sf::Vector2i m = sf::Mouse::getPosition(*g_current_vm->sfml_window);
    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        sf::FloatRect area(pos, {width, 25.0f});
        if (area.contains(sf::Vector2f((float)m.x, (float)m.y))) {
            float newX = std::clamp((float)m.x - pos.x, 0.0f, width);
            val = min + (newX / width) * (max - min);
        }
    }
    g_current_vm->sfml_window->draw(handle);
    return (double)val;
}

static SapphireValue native_ui_input(int arg_count, SapphireValue* args) {
    // 1. Validação de argumentos
    if (arg_count < 2) return {};

    ObjString* sapphireStr = static_cast<ObjString*>(std::get<Obj*>(args[0]._value));
    std::string id = static_cast<ObjString*>(std::get<Obj*>(args[1]._value))->chars;
    UIStyle s = resolve_style(id);

    // 2. Sincronização de Dados (Buffer -> Variável do Script)
    // Se este componente for o que está em foco atualmente, atualizamos a string do script
    // com o que está no buffer de digitação global da VM.
    if (g_current_vm->ui_state.lastComponentId == id) {
        sapphireStr->chars = g_current_vm->ui_state.inputBuffer;
    }

    // 3. MODO LAYOUT (Flex/Grid) - É aqui que seu script estava falhando antes
    if (!g_current_vm->ui_state.nodeStack.empty()) {
        LayoutNode* node = new LayoutNode();
        node->type = NODE_INPUT; // Certifique-se que NODE_INPUT está no enum em vm.h
        node->text = sapphireStr->chars; // Texto atual para renderizar
        node->id = id;
        node->style = s;

        // Define tamanho (usa o do estilo ou um padrão se não definido)
        float w = s.width > 0 ? s.width : 200.0f;
        float h = s.height > 0 ? s.height : 30.0f;
        node->size = {w, h};

        // Adiciona à árvore de layout
        node->parent = g_current_vm->ui_state.nodeStack.back();
        g_current_vm->ui_state.nodeStack.back()->children.push_back(node);

        // Lógica de Foco (Clique) usando a posição do frame anterior
        if (g_current_vm->ui_state.widgetBounds.count(id)) {
            sf::FloatRect bounds = g_current_vm->ui_state.widgetBounds[id];
            sf::Vector2i m = sf::Mouse::getPosition(*g_current_vm->sfml_window);

            // Verifica clique para dar foco
            if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) &&
                bounds.contains(sf::Vector2f((float)m.x, (float)m.y))) {

                g_current_vm->ui_state.lastComponentId = id;
                g_current_vm->ui_state.inputBuffer = sapphireStr->chars; // Carrega texto atual no buffer
                g_current_vm->ui_state.cursorPos = g_current_vm->ui_state.inputBuffer.length(); // Move cursor p/ final
            }
        }
        return args[0]; // Retorna a string (possivelmente atualizada)
    }

    // 4. MODO IMEDIATO (Fallback para quando não usar Layout System)
    // Isso mantém compatibilidade se você usar UI.Input fora de um BeginFlex
    float width = s.width > 0 ? s.width : 200.0f;
    float height = s.height > 0 ? s.height : 30.0f;

    sf::Vector2f pos = g_current_vm->calculate_element_pos(width, height);

    // Desenha background
    draw_rounded_rect(*g_current_vm->sfml_window, pos, {width, height}, s.borderRadius, s.bgColor, s.accentColor, s.borderThickness);

    // Lógica de foco no modo imediato
    sf::Vector2i m = sf::Mouse::getPosition(*g_current_vm->sfml_window);
    sf::FloatRect area(pos, {width, height});

    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) &&
        area.contains(sf::Vector2f((float)m.x, (float)m.y))) {

        g_current_vm->ui_state.lastComponentId = id;
        g_current_vm->ui_state.inputBuffer = sapphireStr->chars;
        g_current_vm->ui_state.cursorPos = g_current_vm->ui_state.inputBuffer.length();
    }

    // Renderiza texto com cursor visual simples se estiver focado
    std::string displayText = sapphireStr->chars;
    if (g_current_vm->ui_state.lastComponentId == id) {
        displayText += "|";
    }

    float textY = pos.y + (height / 2.0f) - (s.fontSize / 2.0f);
    sapphire_render_text(*g_current_vm->sfml_window, displayText, {pos.x + 10.0f, textY}, s.textColor, s.fontAlias, s.fontSize);

    return args[0];
}

static SapphireValue native_ui_end(int arg_count, SapphireValue* args) {
    g_current_vm->sfml_window->display();
    return {};
}

static SapphireValue native_len(int arg_count, SapphireValue* args) {
    if (arg_count != 1) return 0.0;

    if (std::holds_alternative<Obj*>(args[0]._value)) {
        Obj* obj = std::get<Obj*>(args[0]._value);
        if (obj != nullptr && obj->type == OBJ_STRING) {
            return (double)static_cast<ObjString*>(obj)->chars.length();
        }
    }

    if (std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value)) {
        auto array = std::get<std::shared_ptr<SapphireArray>>(args[0]._value);
        if (array != nullptr) {
            return (double)array->elements.size();
        }
    }

    return 0.0;
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

static SapphireValue native_array_create(int arg_count, SapphireValue* args) {
    return std::make_shared<SapphireArray>();
}

static SapphireValue native_array_append(int arg_count, SapphireValue* args) {
    if (arg_count < 2 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value)) return args[0];
    auto list_obj = std::get<std::shared_ptr<SapphireArray>>(args[0]._value);
    list_obj->elements.push_back(args[1]);
    return args[0];
}

static SapphireValue native_array_get(int arg_count, SapphireValue* args) {
    if (arg_count < 2) return {};

    // 1. Tenta pegar o ponteiro do Array com segurança
    auto* array_ptr = std::get_if<std::shared_ptr<SapphireArray>>(&args[0]._value);
    if (!array_ptr) return {};

    // 2. Tenta pegar o índice como double com segurança
    double* index_ptr = std::get_if<double>(&args[1]._value);
    if (!index_ptr) return {};

    int index = static_cast<int>(*index_ptr);
    auto& elements = (*array_ptr)->elements;

    if (index < 0 || index >= (int)elements.size()) return {};

    return elements[index];
}

static SapphireValue native_array_set(int arg_count, SapphireValue* args) {
    if (arg_count < 3 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value) ||
        !std::holds_alternative<double>(args[1]._value)) return args[0];

    auto list_obj = std::get<std::shared_ptr<SapphireArray>>(args[0]._value);
    int index = static_cast<int>(std::get<double>(args[1]._value));

    if (index >= 0 && index < (int)list_obj->elements.size()) {
        list_obj->elements[index] = args[2];
    }
    return args[0];
}

static SapphireValue native_array_len(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value)) return 0.0;
    return (double)std::get<std::shared_ptr<SapphireArray>>(args[0]._value)->elements.size();
}

static SapphireValue native_array_remove_at(int arg_count, SapphireValue* args) {
    if (arg_count < 2 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value) ||
        !std::holds_alternative<double>(args[1]._value)) return {};

    auto list_obj = std::get<std::shared_ptr<SapphireArray>>(args[0]._value);
    int index = static_cast<int>(std::get<double>(args[1]._value));

    if (index < 0 || index >= (int)list_obj->elements.size()) return {};
    SapphireValue res = list_obj->elements[index];
    list_obj->elements.erase(list_obj->elements.begin() + index);
    return res;
}

static SapphireValue native_array_contains(int arg_count, SapphireValue* args) {
    if (arg_count < 2 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value)) return false;
    auto list_obj = std::get<std::shared_ptr<SapphireArray>>(args[0]._value);
    for (const auto& el : list_obj->elements) {
        if (el._value == args[1]._value) return true;
    }
    return false;
}


static SapphireValue native_array_pop(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value)) return {};
    auto list_obj = std::get<std::shared_ptr<SapphireArray>>(args[0]._value);
    if (list_obj->elements.empty()) return {};
    SapphireValue val = list_obj->elements.back();
    list_obj->elements.pop_back();
    return val;
}

static SapphireValue native_array_clear(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value)) return args[0];
    std::get<std::shared_ptr<SapphireArray>>(args[0]._value)->elements.clear();
    return args[0];
}

static SapphireValue native_array_insert(int arg_count, SapphireValue* args) {
    if (arg_count < 3 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value) ||
        !std::holds_alternative<double>(args[1]._value)) return args[0];

    auto list_obj = std::get<std::shared_ptr<SapphireArray>>(args[0]._value);
    int index = static_cast<int>(std::get<double>(args[1]._value));

    if (index >= 0 && index <= (int)list_obj->elements.size()) {
        list_obj->elements.insert(list_obj->elements.begin() + index, args[2]);
    }
    return args[0];
}

static SapphireValue native_array_index_of(int arg_count, SapphireValue* args) {
    if (arg_count < 2 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value)) return -1.0;
    auto list_obj = std::get<std::shared_ptr<SapphireArray>>(args[0]._value);
    for (size_t i = 0; i < list_obj->elements.size(); i++) {
        if (list_obj->elements[i]._value == args[1]._value) return (double)i;
    }
    return -1.0;
}

static SapphireValue native_array_reverse(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !std::holds_alternative<std::shared_ptr<SapphireArray>>(args[0]._value)) return args[0];
    auto list_obj = std::get<std::shared_ptr<SapphireArray>>(args[0]._value);
    std::reverse(list_obj->elements.begin(), list_obj->elements.end());
    return args[0];
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

    // --- Módulo Math ---
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

    // --- Módulo Array ---
    ObjClass* array_class = new_class(this, new_string(this, "Array"));
    ObjInstance* array_module = new_instance(this, array_class);

    array_module->fields["create"]   = new_native(this, native_array_create);
    array_module->fields["append"]   = new_native(this, native_array_append);
    array_module->fields["get"]      = new_native(this, native_array_get);
    array_module->fields["set"]      = new_native(this, native_array_set);
    array_module->fields["len"]      = new_native(this, native_array_len);
    array_module->fields["length"]   = new_native(this, native_array_len); // Alias
    array_module->fields["removeAt"] = new_native(this, native_array_remove_at);
    array_module->fields["contains"] = new_native(this, native_array_contains);

    array_module->fields["pop"]      = new_native(this, native_array_pop);
    array_module->fields["push"]     = new_native(this, native_array_append); // Alias de append
    array_module->fields["clear"]    = new_native(this, native_array_clear);
    array_module->fields["insert"]   = new_native(this, native_array_insert);
    array_module->fields["indexOf"]  = new_native(this, native_array_index_of);
    array_module->fields["reverse"]  = new_native(this, native_array_reverse);

    globals["Array"] = array_module;

    // --- Módulo System ---
    ObjClass* system_class = new_class(this, new_string(this, "System"));
    ObjInstance* system_object = new_instance(this, system_class);
    system_object->fields["getEnv"] = new_native(this, native_system_get_env);
    system_object->fields["getOS"] = new_native(this, native_system_get_os);
    system_object->fields["sleep"] = new_native(this, native_system_sleep);
    system_object->fields["getClipboard"] = new_native(this, native_system_get_clipboard);
    globals["System"] = system_object;

    // --- Módulo HTTP ---
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

    // --- Módulo String ---
    ObjClass* string_class = new_class(this, new_string(this, "String"));
    ObjInstance* string_object = new_instance(this, string_class);

    string_object->fields["upper"] = new_native(this, native_string_upper);
    string_object->fields["lower"] = new_native(this, native_string_lower);
    string_object->fields["trim"] = new_native(this, native_string_trim);
    string_object->fields["contains"] = new_native(this, native_string_contains);
    string_object->fields["replace"] = new_native(this, native_string_replace);
    string_object->fields["sub"] = new_native(this, native_string_sub);
    string_object->fields["split"] = new_native(this, native_string_split);
    string_object->fields["startsWith"] = new_native(this, native_string_starts_with);
    string_object->fields["endsWith"] = new_native(this, native_string_ends_with);
    string_object->fields["index"] = new_native(this, native_string_index_of);

    globals["String"] = string_object;

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
        ui_object->fields["BeginFlex"] = new_native(this, native_ui_begin_flex);
        ui_object->fields["EndFlex"] = new_native(this, native_ui_end_flex);
        ui_object->fields["BeginGrid"] = new_native(this, native_ui_begin_grid);
        ui_object->fields["EndGrid"] = new_native(this, native_ui_end_grid);
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
    uint8_t* ip = frame->ip;
    SapphireValue* slots = frame->slots;
    SapphireValue* top = stack_top;

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
        dispatch_table[OP_MODULO] = &&op_modulo;
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
        if (!this->soft_mode) return false;
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
    auto it = globals.find(name_tmp->chars);
    if (it != globals.end()) it->second = top[-1];
    else if (!this->soft_mode) return false;
    NEXT_CODE();
}

op_get_property: {
    Obj** obj_ptr = std::get_if<Obj*>(&top[-1]._value);
    if (!obj_ptr || !(*obj_ptr) || (*obj_ptr)->type != OBJ_INSTANCE) {
        top[-1] = SapphireValue();
    } else {
        ObjInstance* instance = (ObjInstance*)(*obj_ptr);
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
    val_tmp = POP(); top--; PUSH(val_tmp);
    NEXT_CODE();
}

op_equal: {
    SapphireValue b = POP();
    SapphireValue a = POP();

    PUSH(SapphireValue(a._value == b._value));

    NEXT_CODE();
}

op_greater: {
    SapphireValue b = POP();
    SapphireValue a = POP();
    double* aN = std::get_if<double>(&a._value);
    double* bN = std::get_if<double>(&b._value);

    if (aN && bN) {
        PUSH(SapphireValue(*aN > *bN));
    } else {
        if (!this->soft_mode) {
            std::cerr << "Runtime Error: Operands must be numbers (greater)." << std::endl;
            return false;
        }
        // No soft mode, falhamos silenciosamente para manter o loop vivo
        PUSH(SapphireValue(false));
    }
    NEXT_CODE();
}
op_less: {
    SapphireValue b = POP();
    SapphireValue a = POP();

    double* aN = std::get_if<double>(&a._value);
    double* bN = std::get_if<double>(&b._value);

    if (aN && bN) {

        PUSH(SapphireValue(*aN < *bN));
    } else {

        std::cerr << "Runtime Error: Operands of '<' must be numbers." << std::endl;

        if (!this->soft_mode) return false;
        PUSH(SapphireValue(false));
    }

    NEXT_CODE();
}

op_add: {
    SapphireValue b = POP();
    SapphireValue a = POP();

    double* aNum = std::get_if<double>(&a._value);
    double* bNum = std::get_if<double>(&b._value);

    if (aNum && bNum) {
        PUSH(*aNum + *bNum);
    } else {
        Obj** aObjPtr = std::get_if<Obj*>(&a._value);
        Obj** bObjPtr = std::get_if<Obj*>(&b._value);

        if (aObjPtr && bObjPtr && (*aObjPtr)->type == OBJ_STRING && (*bObjPtr)->type == OBJ_STRING) {
            std::string strA = static_cast<ObjString*>(*aObjPtr)->chars;
            std::string strB = static_cast<ObjString*>(*bObjPtr)->chars;
            PUSH(new_string(this, strA + strB));
        } else {
            std::cerr << "Runtime Error: Operands must be two numbers or two strings." << std::endl;
            return false;
        }
    }
    NEXT_CODE();
}

op_subtract: {
    SapphireValue b = POP(); SapphireValue a = POP();
    double* aN = std::get_if<double>(&a._value);
    double* bN = std::get_if<double>(&b._value);
    if (aN && bN) { PUSH(*aN - *bN); }
    else { std::cerr << "Runtime Error: Operands must be numbers (subtract)." << std::endl; return false; }
    NEXT_CODE();
}

op_multiply: {
    SapphireValue b = POP(); SapphireValue a = POP();
    double* aN = std::get_if<double>(&a._value);
    double* bN = std::get_if<double>(&b._value);
    if (aN && bN) { PUSH(*aN * *bN); }
    else { std::cerr << "Runtime Error: Operands must be numbers (multiply)." << std::endl; return false; }
    NEXT_CODE();
}

op_divide: {
    SapphireValue b = POP(); SapphireValue a = POP();
    double* aN = std::get_if<double>(&a._value);
    double* bN = std::get_if<double>(&b._value);
    if (aN && bN) {
        if (*bN == 0) { std::cerr << "Runtime Error: Division by zero." << std::endl; return false; }
        PUSH(*aN / *bN);
    } else { std::cerr << "Runtime Error: Operands must be numbers (divide)." << std::endl; return false; }
    NEXT_CODE();
}

op_modulo: {
    SapphireValue b = POP(); SapphireValue a = POP();
    double* aN = std::get_if<double>(&a._value);
    double* bN = std::get_if<double>(&b._value);
    if (aN && bN) {
        PUSH(std::fmod(*aN, *bN));
    }
    else {
        if (!this->soft_mode) {
            std::cerr << "Runtime Error: Operands must be numbers (modulo)." << std::endl;
            return false;
        }
        PUSH(0.0);
    }
    NEXT_CODE();
}
op_not:
    top[-1] = SapphireValue(is_falsey(top[-1]));
    NEXT_CODE();

op_negate: {
    double* num = std::get_if<double>(&top[-1]._value);
    if (num) { *num *= -1; }
    else { std::cerr << "Runtime Error: Operand must be a number." << std::endl; return false; }
    NEXT_CODE();
}

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
    ip -= READ_SHORT();
    NEXT_CODE();

op_call: {
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
    uint8_t count = READ_BYTE();
    auto arr = std::make_shared<SapphireArray>();
    for (int i = 0; i < count; i++) arr->elements.push_back(top[-count + i]);
    top -= count; PUSH(arr);
    NEXT_CODE();
}

op_import: {
    name_tmp = (ObjString*)std::get<Obj*>(frame->function->chunk.constants[READ_SHORT()]._value);
    frame->ip = ip; stack_top = top;
    src_tmp = find_and_load_module(name_tmp->chars);
    if (src_tmp.empty()) return false;
    func_tmp = compile(this, src_tmp);
    if (!func_tmp) return false;
    PUSH(new_closure(this, func_tmp));
    call_value(top[-1], 0);
    frame = &frames[frame_count - 1];
    ip = frame->ip; slots = frame->slots; top = stack_top;
    NEXT_CODE();
}

#undef READ_BYTE
#undef READ_SHORT
#undef PUSH
#undef POP
#undef NEXT_CODE
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

// TODO: Melhorar o GC (geral, tá uma m*rda)

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

// TODO: Otimizar blacken_object para conter computed gotos

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

// FIXME: O collect garbage é linear e pode causar stuttering em frames de UI.

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
