// Verba Number Formatter
// Formatações customizadas de moeda e percentual por idioma.

function formatNumber(value, formatType, locale) {
    if (formatType == "currency") {
        if (stringContains(locale, "pt")) {
            return "R$ " + valueToString(value);
        } else if (stringContains(locale, "en-GB")) {
            return "£" + valueToString(value);
        } else if (stringContains(locale, "en")) {
            return "$" + valueToString(value);
        } else if (stringContains(locale, "es")) {
            return "€" + valueToString(value);
        }
    }
    
    if (formatType == "percent") {
        return valueToString(value * 100) + "%";
    }
    
    return valueToString(value);
}
