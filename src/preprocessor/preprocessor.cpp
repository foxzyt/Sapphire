#include "preprocessor.h"
#include <sstream>
#include <cctype>

std::string Preprocessor::trim(const std::string& s) {
    size_t l = 0, r = s.size();
    while (l < r && isspace((unsigned char)s[l])) l++;
    while (r > l && isspace((unsigned char)s[r - 1])) r--;
    return s.substr(l, r - l);
}



std::vector<std::string> Preprocessor::parse_args(const std::string& s, size_t& pos) {
    std::vector<std::string> args;
    int depth = 1;
    std::string current;
    while (pos < s.size() && depth > 0) {
        char c = s[pos++];
        if (c == '(') { depth++; current += c; }
        else if (c == ')') {
            depth--;
            if (depth == 0) {
                std::string a = trim(current);
                if (!a.empty()) args.push_back(a);
            } else { current += c; }
        } else if (c == ',' && depth == 1) {
            args.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    return args;
}

std::string Preprocessor::expand_macro(const MacroDef& def, const std::vector<std::string>& args) {
    std::string result = def.body;
    for (size_t i = 0; i < def.params.size(); i++) {
        const std::string& param = def.params[i];
        const std::string& arg = (i < args.size()) ? args[i] : "";
        std::string buf;
        size_t pos = 0;
        while (pos < result.size()) {
            if (pos + 1 < result.size() && result[pos] == '#' && result[pos + 1] == '#') {
                buf += "##";
                pos += 2;
                continue;
            }
            if (result.compare(pos, param.size(), param) == 0) {
                bool left_ok = (pos == 0 || !is_ident_char(result[pos - 1]));
                bool right_ok = (pos + param.size() == result.size() || !is_ident_char(result[pos + param.size()]));
                if (left_ok && right_ok) {
                    buf += arg;
                    pos += param.size();
                    continue;
                }
            }
            buf += result[pos++];
        }
        result = std::move(buf);
    }

    std::string final_result;
    size_t pos = 0;
    while (pos < result.size()) {
        if (pos + 1 < result.size() && result[pos] == '#' && result[pos + 1] == '#') {
            while (!final_result.empty() && final_result.back() == ' ') final_result.pop_back();
            pos += 2;
            while (pos < result.size() && result[pos] == ' ') pos++;
        } else {
            final_result += result[pos++];
        }
    }
    return final_result;
}

std::string Preprocessor::expand_line(const std::string& line) {
    if (macros.empty()) return line;
    std::string result;
    size_t pos = 0;
    while (pos < line.size()) {
        if (pos + 1 < line.size() && line[pos] == '/' && line[pos + 1] == '/') {
            result += line.substr(pos);
            break;
        }
        if (line[pos] == '"') {
            result += '"';
            pos++;
            while (pos < line.size() && line[pos] != '"') {
                if (line[pos] == '\\' && pos + 1 < line.size()) {
                    result += line[pos++];
                }
                result += line[pos++];
            }
            if (pos < line.size()) { result += '"'; pos++; }
            continue;
        }
        if (isalpha((unsigned char)line[pos]) || line[pos] == '_') {
            size_t id_start = pos;
            while (pos < line.size() && is_ident_char(line[pos])) pos++;
            std::string id = line.substr(id_start, pos - id_start);

            auto it = macros.find(id);
            if (it != macros.end()) {
                const MacroDef& def = it->second;
                if (def.is_function_like) {
                    size_t saved = pos;
                    while (pos < line.size() && isspace((unsigned char)line[pos])) pos++;
                    if (pos < line.size() && line[pos] == '(') {
                        pos++;
                        auto args = parse_args(line, pos);
                        result += expand_line(expand_macro(def, args));
                        continue;
                    } else {
                        pos = saved;
                    }
                } else {
                    result += expand_line(expand_macro(def, {}));
                    continue;
                }
            }
            result += id;
            continue;
        }
        result += line[pos++];
    }
    return result;
}

std::string Preprocessor::process(const std::string& source) {
    std::string text = source;
    size_t pos = 0;
    while ((pos = text.find("macro ", pos)) != std::string::npos) {
        if (pos > 0 && is_ident_char(text[pos - 1])) {
            pos += 6;
            continue;
        }
        size_t macro_start = pos;
        pos += 6;
        
        while (pos < text.size() && isspace((unsigned char)text[pos])) pos++;
        size_t name_start = pos;
        while (pos < text.size() && is_ident_char(text[pos])) pos++;
        std::string name = text.substr(name_start, pos - name_start);
        
        MacroDef def;
        def.name = name;
        def.is_function_like = false;
        
        while (pos < text.size() && isspace((unsigned char)text[pos])) pos++;
        if (pos < text.size() && text[pos] == '(') {
            def.is_function_like = true;
            pos++;
            while (pos < text.size() && text[pos] != ')') {
                while (pos < text.size() && isspace((unsigned char)text[pos])) pos++;
                size_t p_start = pos;
                while (pos < text.size() && text[pos] != ')' && text[pos] != ',') pos++;
                std::string param = trim(text.substr(p_start, pos - p_start));
                if (!param.empty()) def.params.push_back(param);
                if (pos < text.size() && text[pos] == ',') pos++;
            }
            if (pos < text.size() && text[pos] == ')') pos++;
        }
        
        while (pos < text.size() && isspace((unsigned char)text[pos])) pos++;
        if (pos < text.size() && text[pos] == '{') {
            pos++;
            size_t body_start = pos;
            int depth = 1;
            while (pos < text.size() && depth > 0) {
                if (text[pos] == '{') depth++;
                else if (text[pos] == '}') depth--;
                pos++;
            }
            def.body = text.substr(body_start, pos - body_start - 1);
            def.body = trim(def.body);
            macros[def.name] = def;
            
            for (size_t i = macro_start; i < pos; i++) {
                if (text[i] != '\n' && text[i] != '\r') text[i] = ' ';
            }
        }
    }
    
    std::istringstream stream(text);
    std::string line;
    std::string output;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        output += expand_line(line) + '\n';
    }
    return output;
}
