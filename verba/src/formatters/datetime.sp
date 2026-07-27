// Verba Date/Time Formatter
// Utilitários de formatação de datas customizados por idioma.

function formatDateTime(dateStr, formatType, locale) {
    var parts = stringSplit(dateStr, "T");
    var datePart = dateStr;
    if (listLength(parts) > 0) {
        datePart = listGet(parts, 0); // Ex: "2026-07-26"
    }

    if (formatType == "short") {
        return datePart;
    }
    
    // Suporte simplificado para formatação (aumente de acordo com a biblioteca padrão no futuro)
    if (formatType == "long") {
        if (stringContains(locale, "pt")) {
            return datePart + " (Horário Local)";
        }
        return datePart + " (Local Time)";
    }
    
    return dateStr;
}
