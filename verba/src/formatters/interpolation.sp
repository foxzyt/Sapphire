// Verba Interpolation
// Suporta substituição dinâmica de chaves (ex: "Olá {nome}") escaneando a string original.

function interpolate(template, args) {
    if (args == nil) { return template; }
    
    var finalResult = "";
    var len = stringLength(template);
    var i = 0;
    while (i < len) {
        var ch = stringCharAt(template, i);
        if (ch == "{") {
            var endIdx = -1;
            for (var j = i + 1; j < len; j = j + 1) {
                if (stringCharAt(template, j) == "}") {
                    endIdx = j;
                    break;
                }
            }
            if (endIdx != -1) {
                var key = stringSubstring(template, i + 1, endIdx);
                var val = args[key];
                
                if (val != nil) {
                    finalResult = finalResult + valueToString(val);
                } else {
                    // Mantém a chave não preenchida
                    finalResult = finalResult + "{" + key + "}";
                }
                i = endIdx + 1;
            } else {
                finalResult = finalResult + ch;
                i = i + 1;
            }
        } else {
            finalResult = finalResult + ch;
            i = i + 1;
        }
    }
    return finalResult;
}
