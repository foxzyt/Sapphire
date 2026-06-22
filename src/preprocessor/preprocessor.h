#pragma once
#include <string>
#include <vector>
#include <unordered_map>

class Preprocessor {
public:
    std::string process(const std::string& source);

private:
    struct MacroDef {
        std::string name;
        std::vector<std::string> params;
        std::string body;
        bool is_function_like = false;
    };

    std::unordered_map<std::string, MacroDef> macros;


    std::string expand_macro(const MacroDef& def, const std::vector<std::string>& args);
    std::vector<std::string> parse_args(const std::string& s, size_t& pos);
    std::string expand_line(const std::string& line);

    static bool is_ident_char(char c) { return isalnum((unsigned char)c) || c == '_'; }
    static std::string trim(const std::string& s);
};
