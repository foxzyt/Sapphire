import "src/formatters/interpolation.sp";
import "src/formatters/plurals.sp";

// Verba Translator Core
// Resolve chaves aninhadas via notação de ponto (ex: "home.header.title").
class Translator {
    function init(locale, data, fallbackData) {
        this.locale = locale;
        this.data = data;
        this.fallbackData = fallbackData;
    }

    function resolveKey(mapData, keyPath) {
        if (mapData == nil) { return nil; }
        var parts = stringSplit(keyPath, ".");
        var current = mapData;
        var len = listLength(parts);
        
        for (var i = 0; i < len; i = i + 1) {
            var part = listGet(parts, i);
            if (current == nil) { return nil; }
            
            // Tratamento try/catch para evitar crash caso current seja uma string e tentemos indexá-la como mapa
            try {
                current = current[part];
            } catch (e) {
                return nil;
            }
        }
        return current;
    }

    function translate(key, args) {
        var val = this.resolveKey(this.data, key);
        
        // Se não achou, tenta no fallback
        if (val == nil and this.fallbackData != nil) {
            val = this.resolveKey(this.fallbackData, key);
        }
        
        // Se ainda não achou, retorna a própria chave
        if (val == nil) {
            return key;
        }

        // Se houver count e a tradução for um objeto de plurais
        if (args != nil and args["count"] != nil) {
            var count = args["count"];
            var category = getPluralCategory(this.locale, count);
            var pluralForm = nil;
            
            try {
                pluralForm = val[category];
                if (pluralForm == nil) {
                    // Fallback para other caso a linguagem não defina todas
                    pluralForm = val["other"];
                }
            } catch(e) {
                // val é possivelmente uma string
                pluralForm = val;
            }
            
            if (pluralForm != nil) {
                val = pluralForm;
            }
        }

        // Garante que é tratado como string para interpolar
        try {
            var strVal = valueToString(val);
            return interpolate(strVal, args);
        } catch(e) {
            return val;
        }
    }
}
