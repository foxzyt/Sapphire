import "src/loaders/file_loader.sp";
import "src/core/translator.sp";
import "src/formatters/datetime.sp";
import "src/formatters/number.sp";

// Classe Principal do Verba
class Verba {
    function init(config) {
        this.basePath = config["basePath"];
        if (this.basePath == nil) { this.basePath = "locales"; }
        
        this.defaultLocale = config["defaultLocale"];
        if (this.defaultLocale == nil) { this.defaultLocale = "en-US"; }
        
        this.fallbackLocale = config["fallbackLocale"];
        if (this.fallbackLocale == nil) { this.fallbackLocale = this.defaultLocale; }
        
        this.loader = JsonFileLoader(this.basePath);
        this.currentLocale = this.defaultLocale;
        
        this._loadLocales();
    }
    
    function setLocale(locale) {
        this.currentLocale = locale;
        this._loadLocales();
    }
    
    function getLocale() {
        return this.currentLocale;
    }
    
    function _loadLocales() {
        this.currentData = this.loader.load(this.currentLocale);
        if (this.fallbackLocale != this.currentLocale) {
            this.fallbackData = this.loader.load(this.fallbackLocale);
        } else {
            this.fallbackData = nil;
        }
        this.translator = Translator(this.currentLocale, this.currentData, this.fallbackData);
    }
    
    function t(key, args) {
        return this.translator.translate(key, args);
    }
    
    // Utilitários de Formatação Expostos
    function formatTime(dateStr, formatType) {
        return formatDateTime(dateStr, formatType, this.currentLocale);
    }
    
    function formatNum(value, formatType) {
        return formatNumber(value, formatType, this.currentLocale);
    }
}
