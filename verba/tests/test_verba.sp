import "main.sp";
import "src/utils/json_parser.sp";

function assert(condition, message) {
    if (!condition) {
        print("FAIL: " + message);
        throw "Assertion failed";
    }
}

print("Running Verba i18n tests...");

// 1. Test JSON Parser
var original = {"home": {"title": "Welcome", "count": 42, "active": true, "nullVal": nil}};
var jsonStr = JSON.stringify(original);
var parsed = parseJSON(jsonStr);
assert(parsed != nil, "Parser retornou nil");
assert(parsed["home"] != nil, "Objeto home não encontrado");
assert(parsed["home"]["title"] == "Welcome", "String incorreta");
assert(parsed["home"]["count"] == 42, "Number incorreto");
assert(parsed["home"]["active"] == true, "Boolean incorreto");

// 2. Test Core i18n
var i18n = Verba({
    "basePath": "locales",
    "defaultLocale": "en-US",
    "fallbackLocale": "en-US"
});

assert(i18n.getLocale() == "en-US", "Locale padrão falhou");

var t1 = i18n.t("home.title", nil);
assert(t1 == "Welcome to Verba", "Tradução simples falhou: " + t1);

var args = { "name": "Alice", "count": 2 };
var t2 = i18n.t("home.description", args);
assert(t2 == "Hello Alice, you have 2 new messages.", "Interpolação falhou: " + t2);

var t3 = i18n.t("home.messages", { "count": 1 });
assert(t3 == "1 message", "Plural inglês (one) falhou: " + t3);

var t4 = i18n.t("home.messages", { "count": 5 });
assert(t4 == "5 messages", "Plural inglês (other) falhou: " + t4);

i18n.setLocale("pt-BR");

var t5 = i18n.t("home.title", nil);
assert(t5 == "Bem-vindo ao Verba", "Mudança de idioma falhou: " + t5);

var t6 = i18n.t("home.messages", { "count": 0 });
assert(t6 == "nenhuma mensagem", "Plural pt-BR (zero) falhou: " + t6);

var t7 = i18n.t("home.messages", { "count": 1 });
assert(t7 == "1 mensagem", "Plural pt-BR (one) falhou: " + t7);

var t8 = i18n.t("home.messages", { "count": 10 });
assert(t8 == "10 mensagens", "Plural pt-BR (other) falhou: " + t8);

var t9 = i18n.t("home.not_found", nil);
assert(t9 == "home.not_found", "Fallback inexistente falhou: " + t9);

print("Test passed.");
