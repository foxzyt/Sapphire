import "src/utils/json_parser.sp";

// Verba File Loader
// Carrega os arquivos .json do sistema de arquivos e os converte em mapas usando o parser JSON
class JsonFileLoader {
    function init(basePath) {
        this.basePath = basePath;
        this.cache = lruCreate(50); // Cache de até 50 arquivos lidos e parseados
    }

    function load(locale) {
        if (lruHas(this.cache, locale)) {
            return lruGet(this.cache, locale);
        }

        var path = this.basePath + "/" + locale + ".json";
        if (exists(path)) {
            var content = readFile(path);
            var parsed = parseJSON(content);
            lruPut(this.cache, locale, parsed);
            return parsed;
        } else {
            return nil;
        }
    }
}
