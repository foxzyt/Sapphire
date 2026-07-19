#include "amethyst.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <regex>

namespace amethyst {

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

void get_brace_counts(const std::string& line, int& open_count, int& close_count) {
    bool in_string = false;
    char string_char = 0;
    bool in_comment = false;
    open_count = 0;
    close_count = 0;
    for (size_t i = 0; i < line.size(); ++i) {
        if (in_comment) continue;
        if (line[i] == '/' && i + 1 < line.size() && line[i+1] == '/') {
            in_comment = true;
            continue;
        }
        if (in_string) {
            if (line[i] == string_char && (i == 0 || line[i-1] != '\\')) {
                in_string = false;
            }
            continue;
        }
        if (line[i] == '"' || line[i] == '\'') {
            in_string = true;
            string_char = line[i];
            continue;
        }
        if (line[i] == '{') open_count++;
        else if (line[i] == '}') close_count++;
    }
}

std::string clean_spacing(std::string line) {
    line = std::regex_replace(line, std::regex("\\bif\\("), "if (");
    line = std::regex_replace(line, std::regex("\\bwhile\\("), "while (");
    line = std::regex_replace(line, std::regex("\\bfor\\("), "for (");
    line = std::regex_replace(line, std::regex("\\bcatch\\("), "catch (");
    line = std::regex_replace(line, std::regex("\\bswitch\\("), "switch (");
    
    line = std::regex_replace(line, std::regex("\\)\\{"), ") {");
    line = std::regex_replace(line, std::regex("\\belse\\s*\\{"), "else {");
    line = std::regex_replace(line, std::regex("\\btry\\s*\\{"), "try {");
    line = std::regex_replace(line, std::regex("\\bfinally\\s*\\{"), "finally {");
    
    return line;
}

bool format_file(const std::string& filepath, bool check_only) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "\x1b[31m[Error] Amethyst Formatter: Could not open \x1b[0m" << filepath << "\n";
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();

    int indent_level = 0;
    std::vector<std::string> formatted_lines;
    int consecutive_blank_lines = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string trimmed = trim(lines[i]);
        if (trimmed.empty()) {
            consecutive_blank_lines++;
            if (consecutive_blank_lines <= 1) {
                formatted_lines.push_back("");
            }
            continue;
        }
        consecutive_blank_lines = 0;

        int open_count = 0;
        int close_count = 0;
        get_brace_counts(trimmed, open_count, close_count);

        int starting_close_braces = 0;
        for (char c : trimmed) {
            if (c == ' ' || c == '\t') continue;
            if (c == '}') starting_close_braces++;
            else break;
        }

        indent_level -= starting_close_braces;
        if (indent_level < 0) indent_level = 0;

        std::string indent(indent_level * 4, ' ');
        std::string cleaned = clean_spacing(trimmed);

        formatted_lines.push_back(indent + cleaned);

        int remaining_close_braces = close_count - starting_close_braces;
        indent_level += open_count - remaining_close_braces;
        if (indent_level < 0) indent_level = 0;
    }

    // Check if there is any difference
    bool matches = true;
    if (lines.size() != formatted_lines.size()) {
        matches = false;
    } else {
        for (size_t i = 0; i < lines.size(); ++i) {
            if (lines[i] != formatted_lines[i]) {
                matches = false;
                break;
            }
        }
    }

    if (check_only) {
        return matches;
    }

    if (!matches) {
        std::ofstream out(filepath);
        if (!out.is_open()) {
            std::cerr << "\x1b[31m[Error] Amethyst Formatter: Could not write to \x1b[0m" << filepath << "\n";
            return false;
        }
        for (size_t i = 0; i < formatted_lines.size(); ++i) {
            out << formatted_lines[i] << (i == formatted_lines.size() - 1 ? "" : "\n");
        }
        out.close();
        std::cout << "✨ \x1b[32mFormatted in-place:\x1b[0m " << filepath << "\n";
    } else {
        std::cout << "✨ \x1b[32mFile already formatted:\x1b[0m " << filepath << "\n";
    }

    return true;
}

} // namespace amethyst
