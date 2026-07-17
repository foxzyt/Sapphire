#ifndef MINE_CORE_SEMVER_HPP
#define MINE_CORE_SEMVER_HPP

// =============================================================================
// semver.hpp — Semantic Versioning utilities for Mine Package Manager
//
// Extraído de commands/install.hpp para permitir reuso tanto no subsistema
// de plugins quanto no novo subsistema de versão de binários da Sapphire.
// =============================================================================

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace mine {
namespace semver {

// ---------------------------------------------------------------------------
// semver_parse — Converte "1.2.3" ou "v1.2.3" para vetor de inteiros [1,2,3]
// ---------------------------------------------------------------------------
inline std::vector<int> parse(const std::string& version) {
    std::string v = version;
    // Remove prefixo 'v' se existir
    if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) {
        v = v.substr(1);
    }

    std::vector<int> parts;
    std::stringstream ss(v);
    std::string token;

    while (std::getline(ss, token, '.')) {
        try {
            // Suporta pre-release: "1.0.0-beta" -> pega só a parte numérica
            size_t dash = token.find('-');
            if (dash != std::string::npos) {
                token = token.substr(0, dash);
            }
            parts.push_back(std::stoi(token));
        } catch (...) {
            parts.push_back(0);
        }
    }

    return parts;
}

// ---------------------------------------------------------------------------
// compare — Compara duas versões SemVer
//   Retorna: -1 se v1 < v2 | 0 se v1 == v2 | 1 se v1 > v2
//   "latest" é sempre tratado como a versão mais alta possível
// ---------------------------------------------------------------------------
inline int compare(const std::string& v1, const std::string& v2) {
    if (v1 == "latest" && v2 == "latest") return 0;
    if (v1 == "latest") return 1;
    if (v2 == "latest") return -1;

    auto parts1 = parse(v1);
    auto parts2 = parse(v2);

    size_t max_size = std::max(parts1.size(), parts2.size());
    parts1.resize(max_size, 0);
    parts2.resize(max_size, 0);

    for (size_t i = 0; i < max_size; i++) {
        if (parts1[i] < parts2[i]) return -1;
        if (parts1[i] > parts2[i]) return 1;
    }

    return 0;
}

inline bool is_older(const std::string& v1, const std::string& v2) {
    return compare(v1, v2) < 0;
}

inline bool is_newer(const std::string& v1, const std::string& v2) {
    return compare(v1, v2) > 0;
}

inline bool is_equal(const std::string& v1, const std::string& v2) {
    return compare(v1, v2) == 0;
}

// ---------------------------------------------------------------------------
// satisfies — Verifica se uma versão satisfaz uma constraint SemVer
//
//   Constraints suportadas:
//     "latest"    → sempre verdadeiro
//     "1.0.0"     → match exato (mesma semântica que "=1.0.0")
//     "=1.0.0"    → match exato
//     ">1.0.0"    → estritamente maior
//     "<1.0.0"    → estritamente menor
//     ">=1.0.0"   → maior ou igual
//     "<=1.0.0"   → menor ou igual
//     "^1.0.0"    → compatível: mesmo MAJOR, qualquer MINOR/PATCH maior ou igual
//     "~1.0.0"    → mesmo MAJOR (alias de ^)
// ---------------------------------------------------------------------------
inline bool satisfies(const std::string& version, const std::string& constraint) {
    if (constraint.empty() || constraint == "latest") return true;

    // Remove aspas que o terminal possa passar (ex: "^1.0.0")
    std::string c = constraint;
    if (!c.empty() && c.front() == '"') c = c.substr(1);
    if (!c.empty() && c.back()  == '"') c.pop_back();

    std::string op;
    std::string target;

    if (c.size() >= 2 && (c.substr(0, 2) == ">=" || c.substr(0, 2) == "<=")) {
        op = c.substr(0, 2);
        target = c.substr(2);
    } else if (!c.empty() && (c[0] == '^' || c[0] == '>' || c[0] == '<' || c[0] == '~' || c[0] == '=')) {
        op = c.substr(0, 1);
        target = c.substr(1);
    } else {
        op = "=";
        target = c;
    }

    int cmp = compare(version, target);

    if (op == "=")  return cmp == 0;
    if (op == ">")  return cmp > 0;
    if (op == "<")  return cmp < 0;
    if (op == ">=") return cmp >= 0;
    if (op == "<=") return cmp <= 0;

    if (op == "^" || op == "~") {
        // Deve ser >= target E ter o mesmo MAJOR
        if (cmp < 0) return false;

        auto v_parts = parse(version);
        auto t_parts = parse(target);

        int v_major = v_parts.empty() ? 0 : v_parts[0];
        int t_major = t_parts.empty() ? 0 : t_parts[0];

        return v_major == t_major;
    }

    return false;
}

// ---------------------------------------------------------------------------
// resolve_best — Dado um vetor de versões e uma constraint, retorna a maior
//               versão que satisfaz a constraint, ou "" se nenhuma satisfaz.
//
//   versions: vetor de strings no formato "1.0.0" ou "v1.0.0"
//   constraint: qualquer constraint suportada por satisfies()
//   returns: a melhor versão (clean, sem prefixo 'v') ou "" se não encontrada
// ---------------------------------------------------------------------------
inline std::string resolve_best(
    const std::vector<std::string>& versions,
    const std::string& constraint
) {
    std::string best;

    for (const auto& ver : versions) {
        // Normaliza: remove 'v' para comparação
        std::string clean = ver;
        if (!clean.empty() && (clean[0] == 'v' || clean[0] == 'V')) {
            clean = clean.substr(1);
        }

        if (satisfies(clean, constraint)) {
            if (best.empty() || compare(clean, best) > 0) {
                best = clean;
            }
        }
    }

    return best;
}

// ---------------------------------------------------------------------------
// normalize — Garante que a versão tem o formato "X.Y.Z" (sem prefixo 'v')
// ---------------------------------------------------------------------------
inline std::string normalize(const std::string& version) {
    std::string v = version;
    if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) {
        v = v.substr(1);
    }
    return v;
}

// ---------------------------------------------------------------------------
// with_v_prefix — Retorna a versão com prefixo "v" (ex: "v1.0.6")
// ---------------------------------------------------------------------------
inline std::string with_v(const std::string& version) {
    if (version.empty()) return version;
    if (version[0] == 'v' || version[0] == 'V') return version;
    return "v" + version;
}

} // namespace semver
} // namespace mine

#endif // MINE_CORE_SEMVER_HPP
