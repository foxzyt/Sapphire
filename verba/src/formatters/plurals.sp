// Verba Plurals Formatter
// Define regras avançadas de pluralização (ICU-like) baseadas no locale.

function getPluralCategory(locale, count) {
    var loc = stringToLower(locale);
    
    // Regras em Inglês
    if (stringContains(loc, "en")) {
        if (count == 1) { return "one"; }
        return "other";
    }
    
    // Regras em Português
    if (stringContains(loc, "pt")) {
        if (count == 0) { return "zero"; }
        if (count == 1) { return "one"; }
        return "other";
    }
    
    // Regras em Espanhol
    if (stringContains(loc, "es")) {
        if (count == 1) { return "one"; }
        return "other";
    }
    
    // Regras em Francês (0 e 1 são singulares no francês)
    if (stringContains(loc, "fr")) {
        if (count == 0 or count == 1) { return "one"; }
        return "other";
    }
    
    // Regras em Árabe (muitas categorias)
    if (stringContains(loc, "ar")) {
        if (count == 0) { return "zero"; }
        if (count == 1) { return "one"; }
        if (count == 2) { return "two"; }
        if (count >= 3 and count <= 10) { return "few"; }
        if (count >= 11 and count <= 99) { return "many"; }
        return "other";
    }
    
    // Fallback genérico
    if (count == 1) { return "one"; }
    return "other";
}
