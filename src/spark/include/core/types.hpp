#ifndef SPARK_TYPES_HPP
#define SPARK_TYPES_HPP

#include <string>
#include <vector>
#include <optional>

namespace spark {

struct Dependency {
    std::string name;
    std::string version; // Ex: "latest", "1.0.0"
    
    // Construtor helper
    Dependency(std::string n, std::string v) 
        : name(std::move(n)), version(std::move(v)) {}
    
    Dependency() = default;
};

struct PluginMeta {
    std::string name;
    std::string author;
    std::string version;
    std::string description;
    std::string repository;
    bool deprecated = false;
    std::string notice;
    std::vector<Dependency> dependencies; // Preenchido lendo DEPENDENCIES.txt
    
    bool is_valid() const { return !name.empty(); }
};

} // namespace spark

#endif // SPARK_TYPES_HPP
