#include "citrine.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <iostream>

namespace citrine {

struct RuleDef {
    std::string id;
    Category category;
    WarningLevel level;
    std::string pattern;
    std::string message;
    std::string explanation;
    std::string correction;
};

static std::vector<RuleDef> g_rules = {
    // -----------------------------------------
    // CATEGORY 1: SECURITY (40 Rules)
    // -----------------------------------------
    {"SEC01", Category::SECURITY, WarningLevel::ERR, "\\bAKIA[0-9A-Z]{16}\\b", "Hardcoded AWS Access Key detected.", "Hardcoding AWS keys is a critical vulnerability.", "Use env.get_var(\"AWS_ACCESS_KEY\")"},
    {"SEC02", Category::SECURITY, WarningLevel::ERR, "(?i)aws_secret(?:_key)?\\s*=\\s*[\"'][^\"']{40}[\"']", "Hardcoded AWS Secret Key detected.", "Hardcoding AWS secrets exposes your infrastructure.", "Use env.get_var(\"AWS_SECRET_KEY\")"},
    {"SEC03", Category::SECURITY, WarningLevel::ERR, "ghp_[0-9a-zA-Z]{36}", "GitHub Classic Token detected.", "GitHub tokens should not be stored in source code.", "Use environment variables"},
    {"SEC04", Category::SECURITY, WarningLevel::ERR, "github_pat_[0-9a-zA-Z_]{82}", "GitHub Fine-Grained Token detected.", "Source control is no place for API tokens.", "Use a secrets manager"},
    {"SEC05", Category::SECURITY, WarningLevel::ERR, "AIza[0-9A-Za-z\\\\-_]{35}", "Google API Key detected.", "Google keys can lead to massive quota abuse.", "Use secure environment variables"},
    {"SEC06", Category::SECURITY, WarningLevel::ERR, "xoxb-[0-9]{11}-[0-9]{11}-[0-9a-zA-Z]{24}", "Slack Bot Token detected.", "Slack tokens allow workspace access.", "Store in .env"},
    {"SEC07", Category::SECURITY, WarningLevel::ERR, "xoxp-[0-9]{11}-[0-9]{11}-[0-9a-zA-Z]{24}", "Slack User Token detected.", "Exposes user permissions on Slack.", "Store in .env"},
    {"SEC08", Category::SECURITY, WarningLevel::ERR, "https://hooks.slack.com/services/T[a-zA-Z0-9_]{8}/B[a-zA-Z0-9_]{8}/[a-zA-Z0-9_]{24}", "Slack Webhook detected.", "Allows unauthorized messages to your Slack.", "Use environment variables"},
    {"SEC09", Category::SECURITY, WarningLevel::ERR, "sk_live_[0-9a-zA-Z]{24}", "Stripe Standard API Key detected.", "Stripe keys can allow financial fraud.", "Store in a secure vault"},
    {"SEC10", Category::SECURITY, WarningLevel::ERR, "rk_live_[0-9a-zA-Z]{24}", "Stripe Restricted API Key detected.", "Restricted keys still pose a risk.", "Use secrets management"},
    {"SEC11", Category::SECURITY, WarningLevel::ERR, "SK[0-9a-fA-F]{32}", "Twilio API Key detected.", "Twilio keys can lead to SMS spoofing and charges.", "Use .env variables"},
    {"SEC12", Category::SECURITY, WarningLevel::ERR, "[0-9a-f]{32}-us[0-9]{1,2}", "Mailchimp API Key detected.", "Exposes your mailing lists.", "Use .env variables"},
    {"SEC13", Category::SECURITY, WarningLevel::ERR, "SG\\.[0-9a-zA-Z_]{22}\\.[0-9a-zA-Z_\\-]{43}", "SendGrid API Key detected.", "Allows unauthorized mass mailing.", "Use .env variables"},
    {"SEC14", Category::SECURITY, WarningLevel::ERR, "sq0atp-[0-9A-Za-z\\\\-_]{22}", "Square Access Token detected.", "Square tokens are sensitive financial keys.", "Use environment variables"},
    {"SEC15", Category::SECURITY, WarningLevel::ERR, ".*firebaseio\\.com", "Firebase Database URL detected.", "Direct DB URLs are risky if unprotected.", "Validate access rules"},
    {"SEC16", Category::SECURITY, WarningLevel::ERR, "(?i)(?:password|passwd|pwd)\\s*=\\s*[\"'][^\"']{3,}[\"']", "Generic Password Hardcoded.", "Never hardcode passwords.", "Use a credential manager"},
    {"SEC17", Category::SECURITY, WarningLevel::ERR, "(?i)(?:secret|token|api_key)\\s*=\\s*[\"'][^\"']{8,}[\"']", "Generic Secret Hardcoded.", "Secrets should be managed externally.", "Use env.get_var()"},
    {"SEC18", Category::SECURITY, WarningLevel::ERR, "\\beval\\s*\\(", "Use of eval() detected.", "eval() executes arbitrary code and is highly dangerous.", "Use JSON.parse() or specific parsers"},
    {"SEC19", Category::SECURITY, WarningLevel::ERR, "\\bexec\\s*\\(", "Use of exec() detected.", "Can lead to Command Injection.", "Use child process spawning with array arguments"},
    {"SEC20", Category::SECURITY, WarningLevel::ERR, "\\bsystem\\s*\\(", "Use of system() detected.", "Prone to shell injection.", "Use direct API calls instead of shell"},
    {"SEC21", Category::SECURITY, WarningLevel::WARNING, "\\bmd5\\s*\\(", "MD5 Hashing detected.", "MD5 is cryptographically broken.", "Use sha256() or stronger"},
    {"SEC22", Category::SECURITY, WarningLevel::WARNING, "\\bsha1\\s*\\(", "SHA1 Hashing detected.", "SHA1 is no longer considered secure.", "Use sha256()"},
    {"SEC23", Category::SECURITY, WarningLevel::ERR, "\\bMath\\.random\\s*\\(", "Insecure Randomness (Math.random).", "Not suitable for cryptography.", "Use crypto.random()"},
    {"SEC24", Category::SECURITY, WarningLevel::ERR, "[\"']http://", "Insecure HTTP Protocol detected.", "Traffic is sent in plaintext.", "Use https://"},
    {"SEC25", Category::SECURITY, WarningLevel::ERR, "[\"']ftp://", "Insecure FTP Protocol detected.", "FTP is not encrypted.", "Use sftp://"},
    {"SEC26", Category::SECURITY, WarningLevel::ERR, "[\"']ws://", "Insecure WebSocket Protocol detected.", "WS traffic is plaintext.", "Use wss://"},
    {"SEC27", Category::SECURITY, WarningLevel::ERR, "base64_encode\\s*\\(\\s*password", "Password encoded with Base64.", "Base64 is encoding, not hashing.", "Use bcrypt or argon2"},
    {"SEC28", Category::SECURITY, WarningLevel::ERR, "[\"'](?i)SELECT.*[\"']\\s*\\+", "SQL Injection risk: Concatenated SELECT query.", "String concatenation in queries is unsafe.", "Use parameterized queries (?)"},
    {"SEC29", Category::SECURITY, WarningLevel::ERR, "[\"'](?i)UPDATE.*[\"']\\s*\\+", "SQL Injection risk: Concatenated UPDATE query.", "Unsafe DB modification.", "Use parameterized queries (?)"},
    {"SEC30", Category::SECURITY, WarningLevel::ERR, "[\"'](?i)DELETE.*[\"']\\s*\\+", "SQL Injection risk: Concatenated DELETE query.", "Attackers could delete the whole table.", "Use parameterized queries (?)"},
    {"SEC31", Category::SECURITY, WarningLevel::ERR, "[\"'](?i)INSERT.*[\"']\\s*\\+", "SQL Injection risk: Concatenated INSERT query.", "Could allow malicious records.", "Use parameterized queries (?)"},
    {"SEC32", Category::SECURITY, WarningLevel::ERR, "[\"'](?i)DROP.*[\"']\\s*\\+", "SQL Injection risk: Concatenated DROP query.", "Extremely dangerous.", "Never concatenate DROP statements"},
    {"SEC33", Category::SECURITY, WarningLevel::ERR, "[\"'](?i)ALTER.*[\"']\\s*\\+", "SQL Injection risk: Concatenated ALTER query.", "Schema manipulation via injection.", "Avoid dynamic schema generation"},
    {"SEC34", Category::SECURITY, WarningLevel::ERR, "chmod\\s*\\(.*777", "Insecure file permissions (777).", "Grants read/write/execute to everyone.", "Use 644 or 755"},
    {"SEC35", Category::SECURITY, WarningLevel::ERR, "Set-Cookie.*(?i)HttpOnly=false", "Cookie lacking HttpOnly flag.", "Prone to XSS stealing.", "Set HttpOnly=true"},
    {"SEC36", Category::SECURITY, WarningLevel::ERR, "Set-Cookie.*(?i)Secure=false", "Cookie lacking Secure flag.", "Cookie sent over plaintext HTTP.", "Set Secure=true"},
    {"SEC37", Category::SECURITY, WarningLevel::ERR, "Access-Control-Allow-Origin\\s*:\\s*\\\\*", "Permissive CORS policy (*).", "Allows any site to read data.", "Specify exact origin domains"},
    {"SEC38", Category::SECURITY, WarningLevel::ERR, "verify_ssl\\s*=\\s*false", "SSL Verification disabled.", "Allows Man-In-The-Middle attacks.", "Set verify_ssl=true"},
    {"SEC39", Category::SECURITY, WarningLevel::ERR, "\\binnerHTML\\s*=", "Assignment to innerHTML.", "Potential DOM-based XSS vulnerability.", "Use textContent or innerText"},
    {"SEC40", Category::SECURITY, WarningLevel::ERR, "\\bdocument\\.write\\s*\\(", "Use of document.write().", "Can overwrite the entire page or inject XSS.", "Use DOM manipulation APIs"},

    // -----------------------------------------
    // CATEGORY 2: PERFORMANCE (40 Rules)
    // -----------------------------------------
    {"PERF01", Category::PERFORMANCE, WarningLevel::WARNING, "([a-zA-Z0-9_]+)\\s*=\\s*\\1\\s*\\+\\s*1\\b", "Use postfix increment instead of manual addition.", "Post-increment is more concise and idiomatic.", "i++"},
    {"PERF02", Category::PERFORMANCE, WarningLevel::WARNING, "\\bfor\\s*\\(.*\\.length\\(\\).*\\)", "Evaluating .length() in loop condition.", "Evaluated on every iteration.", "Cache length in a local variable"},
    {"PERF03", Category::PERFORMANCE, WarningLevel::WARNING, "\\bstring\\s*[a-zA-Z0-9_]+\\s*\\+\\s*string\\b", "Inefficient string concatenation.", "Creates temporary string objects.", "Use StringBuilder or stream"},
    {"PERF04", Category::PERFORMANCE, WarningLevel::WARNING, "\\bnew\\s+[a-zA-Z0-9_]+\\(\\)", "Unnecessary heap allocation.", "Heap allocations are slow.", "Use stack allocation or object pools"},
    {"PERF05", Category::PERFORMANCE, WarningLevel::WARNING, "\\bstd::endl\\b", "Use of std::endl flushes buffer unnecessarily.", "Constant flushing slows down I/O.", "Use '\\n' instead"},
    {"PERF06", Category::PERFORMANCE, WarningLevel::PEDANTIC, "\\bpush_back\\b", "Check if reserve() was called before push_back().", "Frequent reallocations degrade performance.", "Call vector.reserve() first"},
    {"PERF07", Category::PERFORMANCE, WarningLevel::WARNING, "catch\\s*\\([a-zA-Z0-9_]+\\s+[a-zA-Z0-9_]+\\)", "Catching exceptions by value.", "Causes unnecessary copying.", "Catch by const reference: catch(const Exception& e)"},
    {"PERF08", Category::PERFORMANCE, WarningLevel::WARNING, "for\\s*\\([a-zA-Z0-9_:]+\\s+[a-zA-Z0-9_]+\\s*:\\s*[a-zA-Z0-9_]+\\)", "Iterating over collections by value.", "Copies every element.", "Iterate by const reference: for(const auto& item : items)"},
    {"PERF09", Category::PERFORMANCE, WarningLevel::INFO, "\\breturn\\s+std::move\\b", "Redundant std::move in return.", "Prevents Return Value Optimization (RVO).", "Return the local variable directly"},
    {"PERF10", Category::PERFORMANCE, WarningLevel::WARNING, "std::string\\s+[a-zA-Z0-9_]+\\s*=\\s*\"[^\"]*\"", "Copy-initialization of std::string.", "May create unnecessary copies.", "Use direct initialization or std::string_view"},
    {"PERF11", Category::PERFORMANCE, WarningLevel::WARNING, "\\bfind\\(\\)\\s*==\\s*end\\(\\)", "Inefficient existence check.", "Using find() just to check existence.", "Use .contains() if available"},
    {"PERF12", Category::PERFORMANCE, WarningLevel::WARNING, "\\bcount\\(\\)\\s*>\\s*0", "Inefficient map/set count check.", "Count requires traversal or lookup.", "Use .contains()"},
    {"PERF13", Category::PERFORMANCE, WarningLevel::INFO, "\\bsize\\(\\)\\s*==\\s*0", "Inefficient size() check for emptiness.", "size() might be O(N) for some structures.", "Use .empty()"},
    {"PERF14", Category::PERFORMANCE, WarningLevel::INFO, "\\bsize\\(\\)\\s*>\\s*0", "Inefficient size() check for emptiness.", "size() might be O(N).", "Use !.empty()"},
    {"PERF15", Category::PERFORMANCE, WarningLevel::WARNING, "\\bstd::bind\\b", "Use of std::bind.", "std::bind adds overhead.", "Use C++11 lambdas instead"},
    {"PERF16", Category::PERFORMANCE, WarningLevel::WARNING, "vector<bool>", "Use of vector<bool>.", "Not a standard container, proxies slow down access.", "Use vector<char> or std::bitset"},
    {"PERF17", Category::PERFORMANCE, WarningLevel::PEDANTIC, "std::shared_ptr<", "Overuse of std::shared_ptr.", "Reference counting adds atomic overhead.", "Use std::unique_ptr when ownership isn't shared"},
    {"PERF18", Category::PERFORMANCE, WarningLevel::WARNING, "\\bnew\\b.*std::shared_ptr", "Constructing shared_ptr with raw new.", "Causes two allocations (control block + object).", "Use std::make_shared"},
    {"PERF19", Category::PERFORMANCE, WarningLevel::WARNING, "\\bnew\\b.*std::unique_ptr", "Constructing unique_ptr with raw new.", "Less safe and potentially less efficient.", "Use std::make_unique"},
    {"PERF20", Category::PERFORMANCE, WarningLevel::WARNING, "\\bmemset\\(.*sizeof", "Using memset on C++ objects.", "Bypasses constructors and breaks vtables.", "Use default constructor or std::fill"},
    {"PERF21", Category::PERFORMANCE, WarningLevel::WARNING, "\\bmemcpy\\(.*sizeof", "Using memcpy on non-trivially copyable objects.", "Can break complex objects.", "Use std::copy"},
    {"PERF22", Category::PERFORMANCE, WarningLevel::WARNING, "virtual.*\\binline\\b", "Virtual inline function.", "Virtual calls usually cannot be inlined.", "Remove inline or virtual"},
    {"PERF23", Category::PERFORMANCE, WarningLevel::INFO, "\\bauto\\b.*\\bnew\\b", "auto used with raw new.", "Masks raw pointer ownership.", "Use smart pointers"},
    {"PERF24", Category::PERFORMANCE, WarningLevel::INFO, "\\bvolatile\\b", "Use of volatile.", "Often misused for threading.", "Use std::atomic for threading synchronization"},
    {"PERF25", Category::PERFORMANCE, WarningLevel::WARNING, "\\bdynamic_cast\\b", "Use of dynamic_cast.", "Requires RTTI which is slow.", "Use static_cast if types are known, or virtual functions"},
    {"PERF26", Category::PERFORMANCE, WarningLevel::WARNING, "\\bSleep\\s*\\(", "Blocking the main thread.", "Sleep halts the entire thread execution.", "Use async/await or asynchronous timers"},
    {"PERF27", Category::PERFORMANCE, WarningLevel::WARNING, "\\bstd::regex\\b.*\\(.*\\)", "Regex compiled inline.", "Compiling regex in hot paths is very slow.", "Make regex static or global"},
    {"PERF28", Category::PERFORMANCE, WarningLevel::WARNING, "\\bArray\\.prototype\\.splice\\b", "Using splice on large arrays.", "Splice shifts all subsequent elements (O(N)).", "Consider using maps/sets or filter()"},
    {"PERF29", Category::PERFORMANCE, WarningLevel::WARNING, "\\bJSON\\.parse\\(JSON\\.stringify\\(", "Deep copy via JSON.", "Extremely slow deep copy mechanism.", "Use structuredClone() or specific copy functions"},
    {"PERF30", Category::PERFORMANCE, WarningLevel::INFO, "\\.map\\(.*\\)\\.filter\\(", "Chaining map and filter.", "Iterates the array multiple times.", "Combine them into a single reduce() or map()"},
    {"PERF31", Category::PERFORMANCE, WarningLevel::INFO, "SELECT\\s+\\\\*\\s+FROM", "Using SELECT *.", "Fetches more data than needed.", "Select specific columns"},
    {"PERF32", Category::PERFORMANCE, WarningLevel::WARNING, "ORDER BY RAND\\(\\)", "SQL Order By Rand.", "Very slow on large tables.", "Fetch randomly in application logic"},
    {"PERF33", Category::PERFORMANCE, WarningLevel::WARNING, "LIKE\\s+['\"]%.*['\"]", "Leading wildcard in SQL LIKE.", "Cannot use indexes (O(N) table scan).", "Use full-text search or trailing wildcards only"},
    {"PERF34", Category::PERFORMANCE, WarningLevel::WARNING, "document\\.querySelectorAll\\(.*\\)[0]", "Inefficient DOM querying.", "Queries all elements then takes first.", "Use document.querySelector()"},
    {"PERF35", Category::PERFORMANCE, WarningLevel::WARNING, "window\\.addEventListener\\(['\"]scroll['\"]", "Un-debounced scroll listener.", "Fires hundreds of times per second.", "Use a debounce/throttle function"},
    {"PERF36", Category::PERFORMANCE, WarningLevel::WARNING, "setTimeout\\(.*,\\s*0\\)", "setTimeout 0.", "Can cause macro-task queue bottlenecking.", "Use queueMicrotask or Promises"},
    {"PERF37", Category::PERFORMANCE, WarningLevel::WARNING, "\\+=\\s*[\"'][^\"']*[\"']", "Repeated string += concatenation.", "String immutability causes allocations.", "Use array.join() or StringBuilders"},
    {"PERF38", Category::PERFORMANCE, WarningLevel::WARNING, "\\bdelete\\s+[a-zA-Z0-9_]+\\[", "Using delete on array indices.", "Leaves holes (undefined) in arrays and de-optimizes V8 arrays.", "Use splice() or null assignments"},
    {"PERF39", Category::PERFORMANCE, WarningLevel::INFO, "\\barguments\\b", "Using the 'arguments' object.", "Prevents certain V8 engine optimizations.", "Use rest parameters (...args)"},
    {"PERF40", Category::PERFORMANCE, WarningLevel::INFO, "\\bwith\\s*\\(", "Using 'with' statement.", "De-optimizes variable lookups.", "Avoid 'with' entirely"},

    // -----------------------------------------
    // CATEGORY 3: STYLE (40 Rules)
    // -----------------------------------------
    {"STY01", Category::STYLE, WarningLevel::WARNING, "\\bclass\\s+[a-z][a-zA-Z0-9_]*", "Class name must be PascalCase.", "Consistent casing helps readability.", "Change to PascalCase"},
    {"STY02", Category::STYLE, WarningLevel::PEDANTIC, "\\bfunction\\s+[A-Z][a-zA-Z0-9_]*", "Function name must be camelCase.", "Standard naming convention.", "Change to camelCase"},
    {"STY03", Category::STYLE, WarningLevel::PEDANTIC, "\\bvar\\s+[A-Z][a-zA-Z0-9_]*", "Variable name must be camelCase.", "Standard naming convention.", "Change to camelCase"},
    {"STY04", Category::STYLE, WarningLevel::PEDANTIC, "[ \\t]+$", "Trailing whitespace detected.", "Creates noisy git diffs.", "Remove trailing spaces"},
    {"STY05", Category::STYLE, WarningLevel::INFO, "==\\s*true", "Redundant boolean comparison (== true).", "Implicit truthiness is cleaner.", "Use 'if (condition)'"},
    {"STY06", Category::STYLE, WarningLevel::INFO, "==\\s*false", "Redundant boolean comparison (== false).", "Implicit falsiness is cleaner.", "Use 'if (!condition)'"},
    {"STY07", Category::STYLE, WarningLevel::PEDANTIC, "\\bif\\(", "Missing space after 'if'.", "Spaces improve readability.", "Use 'if ('"},
    {"STY08", Category::STYLE, WarningLevel::PEDANTIC, "\\)\\{", "Missing space before brace '{'.", "Standard formatting.", "Use ') {'"},
    {"STY09", Category::STYLE, WarningLevel::PEDANTIC, "\\bfor\\(", "Missing space after 'for'.", "Standard formatting.", "Use 'for ('"},
    {"STY10", Category::STYLE, WarningLevel::PEDANTIC, "\\bwhile\\(", "Missing space after 'while'.", "Standard formatting.", "Use 'while ('"},
    {"STY11", Category::STYLE, WarningLevel::PEDANTIC, "\\bcatch\\(", "Missing space after 'catch'.", "Standard formatting.", "Use 'catch ('"},
    {"STY12", Category::STYLE, WarningLevel::PEDANTIC, "\\bswitch\\(", "Missing space after 'switch'.", "Standard formatting.", "Use 'switch ('"},
    {"STY13", Category::STYLE, WarningLevel::WARNING, "\\bNULL\\b", "Using NULL macro.", "Modern C++ uses nullptr.", "Use 'nullptr'"},
    {"STY14", Category::STYLE, WarningLevel::WARNING, "\\btypedef\\s+", "Using typedef instead of using.", "C++11 aliases are more readable.", "Use 'using Name = Type;'"},
    {"STY15", Category::STYLE, WarningLevel::WARNING, "^\\s*#define\\s+[A-Za-z0-9_]+\\s+[^\\s]", "Using macros for constants.", "Macros have no scope or type safety.", "Use 'constexpr' or 'const'"},
    {"STY16", Category::STYLE, WarningLevel::PEDANTIC, "\\belse\\s*\\\\n\\s*\\{", "Brace on new line after else.", "Use standard K&R style formatting.", "Use '} else {'"},
    {"STY17", Category::STYLE, WarningLevel::PEDANTIC, "\\bif\\s*\\([^)]+\\)\\s*\\\\n\\s*\\{", "Brace on new line after if.", "Use standard K&R style.", "Use 'if (...) {'"},
    {"STY18", Category::STYLE, WarningLevel::WARNING, "^\\s*using\\s+namespace\\s+std;", "using namespace std;", "Pollutes global namespace.", "Use explicit std:: prefixes"},
    {"STY19", Category::STYLE, WarningLevel::PEDANTIC, ",[^\\s]", "Missing space after comma.", "Reduces readability.", "Add a space after comma"},
    {"STY20", Category::STYLE, WarningLevel::PEDANTIC, ";[^\\s\\\\n]", "Missing space after semicolon.", "Reduces readability.", "Add a space after semicolon"},
    {"STY21", Category::STYLE, WarningLevel::WARNING, "TODO\\s*:", "TODO comment found.", "Pending task left in code.", "Resolve or track in Jira/GitHub"},
    {"STY22", Category::STYLE, WarningLevel::WARNING, "FIXME\\s*:", "FIXME comment found.", "Broken code left in place.", "Fix the code"},
    {"STY23", Category::STYLE, WarningLevel::INFO, "XXX\\s*:", "XXX comment found.", "Attention marker left in code.", "Address the warning"},
    {"STY24", Category::STYLE, WarningLevel::PEDANTIC, "\\bcout\\b", "Using cout instead of logger.", "Production code should use structured logging.", "Use a Logger"},
    {"STY25", Category::STYLE, WarningLevel::PEDANTIC, "\\bcerr\\b", "Using cerr instead of logger.", "Production code should use structured logging.", "Use a Logger"},
    {"STY26", Category::STYLE, WarningLevel::WARNING, "\\blet\\b", "Use of unsupported keyword 'let'.", "Sapphire only supports 'var' and 'const'.", "Use 'var' or 'const'"},
    {"STY27", Category::STYLE, WarningLevel::PEDANTIC, "===", "Strict equality missing?", "Wait, in C++, it's ==. This is a generic check.", "N/A"},
    {"STY28", Category::STYLE, WarningLevel::PEDANTIC, "!\\s+[a-zA-Z0-9_]+", "Space after negation operator.", "Unconventional style.", "Remove space after '!'"},
    {"STY29", Category::STYLE, WarningLevel::INFO, "magic_number", "Magic numbers should be named constants.", "Increases maintainability.", "const int MAX_USERS = 10;"},
    {"STY30", Category::STYLE, WarningLevel::WARNING, "\\bgoto\\b", "Use of goto statement.", "Spaghetti code.", "Refactor into loops/functions"},
    {"STY31", Category::STYLE, WarningLevel::PEDANTIC, "_{2,}", "Consecutive underscores in names.", "Looks like __reserved__ macros.", "Use single underscores"},
    {"STY32", Category::STYLE, WarningLevel::WARNING, "std::cout\\s*<<\\s*\"\\\\n\"", "Using std::cout for newlines.", "Clutters code.", "Include \\n in the main string"},
    {"STY33", Category::STYLE, WarningLevel::WARNING, "const\\s+int\\s+const", "Duplicate const specifier.", "Redundant and confusing.", "Remove one const"},
    {"STY34", Category::STYLE, WarningLevel::PEDANTIC, "\\bint\\s+\\\\*", "Pointer star attached to type.", "Conventional C++ uses Type* var.", "Use int* ptr"},
    {"STY35", Category::STYLE, WarningLevel::INFO, "if\\s*\\([^)]+\\)\\s*[a-zA-Z0-9_]+;", "Single line if without braces.", "Can lead to bugs during refactoring.", "Add { } braces"},
    {"STY36", Category::STYLE, WarningLevel::INFO, "while\\s*\\([^)]+\\)\\s*[a-zA-Z0-9_]+;", "Single line while without braces.", "Can lead to bugs.", "Add { } braces"},
    {"STY37", Category::STYLE, WarningLevel::INFO, "for\\s*\\([^)]+\\)\\s*[a-zA-Z0-9_]+;", "Single line for without braces.", "Can lead to bugs.", "Add { } braces"},
    {"STY38", Category::STYLE, WarningLevel::PEDANTIC, "\\bclass\\s+[a-zA-Z0-9_]+\\s*\\\\n\\s*\\{", "Brace on new line for class.", "K&R style recommends brace on same line.", "class MyClass {"},
    {"STY39", Category::STYLE, WarningLevel::PEDANTIC, "\\bstruct\\s+[a-zA-Z0-9_]+\\s*\\\\n\\s*\\{", "Brace on new line for struct.", "Use brace on same line.", "struct MyStruct {"},
    {"STY40", Category::STYLE, WarningLevel::INFO, "\\breturn\\s+\\([^)]+\\)\\s*;", "Returning expression in parentheses.", "Redundant parentheses.", "Remove parentheses"},

    // -----------------------------------------
    // CATEGORY 4: SYNTAX (40 Rules)
    // -----------------------------------------
    {"SYN01", Category::SYNTAX, WarningLevel::ERR, "\\bcatch\\s*\\([^)]+\\)\\s*\\{\\s*\\}", "Empty catch block.", "Silently ignores exceptions.", "Handle or log the error"},
    {"SYN02", Category::SYNTAX, WarningLevel::ERR, "\\bif\\s*\\([^=]+=[^=]+\\)", "Assignment inside if condition.", "Usually a typo for ==.", "Use =="},
    {"SYN03", Category::SYNTAX, WarningLevel::ERR, "\\bwhile\\s*\\(true\\)", "Infinite loop (while true).", "Potential CPU lock if no break.", "Ensure there is a break condition"},
    {"SYN04", Category::SYNTAX, WarningLevel::WARNING, "\\bdelete\\s+[a-zA-Z0-9_]+", "Manual memory deletion.", "Risk of memory leaks or double free.", "Use std::unique_ptr"},
    {"SYN05", Category::SYNTAX, WarningLevel::ERR, "\\bdelete\\[\\]\\s+[a-zA-Z0-9_]+", "Manual array deletion.", "Use std::vector.", "Use std::vector"},
    {"SYN06", Category::SYNTAX, WarningLevel::ERR, "==\\s*NaN", "Equality check with NaN.", "NaN != NaN. Will always fail.", "Use isNaN() or std::isnan()"},
    {"SYN07", Category::SYNTAX, WarningLevel::ERR, "=\\s*=\\s*=", "Strict equality in C++?", "Invalid syntax in C++.", "Use =="},
    {"SYN08", Category::SYNTAX, WarningLevel::ERR, "!==", "Strict inequality in C++?", "Invalid syntax in C++.", "Use !="},
    {"SYN09", Category::SYNTAX, WarningLevel::WARNING, "std::terminate\\(\\)", "Explicit call to std::terminate.", "Hard crashes the application.", "Throw an exception instead"},
    {"SYN10", Category::SYNTAX, WarningLevel::WARNING, "std::exit\\(\\)", "Explicit call to std::exit.", "Bypasses local destructors.", "Return from main()"},
    {"SYN11", Category::SYNTAX, WarningLevel::WARNING, "abort\\(\\)", "Explicit call to abort.", "Generates core dump.", "Handle gracefully"},
    {"SYN12", Category::SYNTAX, WarningLevel::ERR, "\\bauto\\s+[a-zA-Z0-9_]+\\s*;", "Uninitialized auto.", "auto requires an initializer.", "Provide a value"},
    {"SYN13", Category::SYNTAX, WarningLevel::WARNING, "void\\s*\\(\\s*\\)", "Empty void param list in C++.", "In C++ '()' implies void.", "Remove 'void'"},
    {"SYN14", Category::SYNTAX, WarningLevel::ERR, "case\\s+.*:\\s*case", "Fallthrough in switch.", "Missing break statement.", "Add 'break;' or '[[fallthrough]];'"},
    {"SYN15", Category::SYNTAX, WarningLevel::ERR, "\\bdefault\\b.*\\bbreak\\b.*\\bdefault\\b", "Multiple default cases in switch.", "Syntax error.", "Have only one default"},
    {"SYN16", Category::SYNTAX, WarningLevel::ERR, "new\\s+[a-zA-Z0-9_]+\\(\\).*new\\s+[a-zA-Z0-9_]+\\(\\)", "Multiple news in one statement.", "Can cause memory leaks if one throws.", "Use smart pointers"},
    {"SYN17", Category::SYNTAX, WarningLevel::WARNING, "throw\\s+new", "Throwing a pointer.", "Requires manual deletion by catcher.", "Throw by value"},
    {"SYN18", Category::SYNTAX, WarningLevel::WARNING, "catch\\s*\\([a-zA-Z0-9_]+\\s*\\\\*\\s*[a-zA-Z0-9_]+\\)", "Catching a pointer.", "Only catch values/references.", "Catch by const reference"},
    {"SYN19", Category::SYNTAX, WarningLevel::ERR, "\\bvirtual\\s+.*\\boverride\\b.*=0", "Pure virtual function marked override?", "Rare and usually a design flaw.", "Remove override or =0"},
    {"SYN20", Category::SYNTAX, WarningLevel::ERR, "\\bconst_cast\\b", "Use of const_cast.", "Casting away constness is undefined behavior.", "Refactor to avoid const_cast"},
    {"SYN21", Category::SYNTAX, WarningLevel::ERR, "\\breinterpret_cast\\b", "Use of reinterpret_cast.", "Highly unsafe type punning.", "Use std::bit_cast or unions"},
    {"SYN22", Category::SYNTAX, WarningLevel::WARNING, "\\bscanf\\b", "Use of scanf.", "Buffer overflow risks.", "Use std::cin or safely sized buffers"},
    {"SYN23", Category::SYNTAX, WarningLevel::WARNING, "\\bprintf\\b", "Use of printf.", "Lack of type safety.", "Use std::cout or std::format"},
    {"SYN24", Category::SYNTAX, WarningLevel::ERR, "\\bgets\\b", "Use of gets.", "gets() is removed from standard.", "Use std::getline()"},
    {"SYN25", Category::SYNTAX, WarningLevel::WARNING, "\\bstrcpy\\b", "Use of strcpy.", "Buffer overflow risk.", "Use strncpy or std::string"},
    {"SYN26", Category::SYNTAX, WarningLevel::WARNING, "\\bstrcat\\b", "Use of strcat.", "Buffer overflow risk.", "Use strncat or std::string::append"},
    {"SYN27", Category::SYNTAX, WarningLevel::WARNING, "\\bsprintf\\b", "Use of sprintf.", "Buffer overflow risk.", "Use snprintf or std::ostringstream"},
    {"SYN28", Category::SYNTAX, WarningLevel::ERR, "sizeof\\s*\\(\\s*[a-zA-Z0-9_]+\\s*\\\\*\\s*\\)", "sizeof(pointer).", "Returns pointer size, not array size.", "Use sizeof(*ptr)"},
    {"SYN29", Category::SYNTAX, WarningLevel::WARNING, "\\bfree\\s*\\(", "Use of free().", "Mixes C and C++ memory management.", "Use delete or smart pointers"},
    {"SYN30", Category::SYNTAX, WarningLevel::WARNING, "\\bmalloc\\s*\\(", "Use of malloc().", "Does not call C++ constructors.", "Use new"},
    {"SYN31", Category::SYNTAX, WarningLevel::WARNING, "\\bcalloc\\s*\\(", "Use of calloc().", "Does not call C++ constructors.", "Use new"},
    {"SYN32", Category::SYNTAX, WarningLevel::WARNING, "\\brealloc\\s*\\(", "Use of realloc().", "Does not work with C++ objects.", "Use std::vector"},
    {"SYN33", Category::SYNTAX, WarningLevel::ERR, "\\bassert\\s*\\(.*\\+\\+.*\\)", "State mutation inside assert().", "Code is removed in release builds.", "Extract mutation before assert"},
    {"SYN34", Category::SYNTAX, WarningLevel::ERR, "\\bassert\\s*\\(.*=.*\\)", "Assignment inside assert().", "Code is removed in release builds.", "Use =="},
    {"SYN35", Category::SYNTAX, WarningLevel::ERR, "\\\\#include\\s+[\"<][^>\"]+\\.cpp[\">]", "Including a .cpp file.", "Causes duplicate symbols.", "Include .h or .hpp"},
    {"SYN36", Category::SYNTAX, WarningLevel::WARNING, "std::endl\\s*<<\\s*std::endl", "Multiple std::endl.", "Flushes multiple times.", "Use '\\n\\n'"},
    {"SYN37", Category::SYNTAX, WarningLevel::WARNING, "\\bauto_ptr\\b", "Use of std::auto_ptr.", "Deprecated in C++11, removed in C++17.", "Use std::unique_ptr"},
    {"SYN38", Category::SYNTAX, WarningLevel::WARNING, "\\bregister\\b", "Use of register keyword.", "Deprecated in C++11, removed in C++17.", "Remove keyword"},
    {"SYN39", Category::SYNTAX, WarningLevel::WARNING, "\\b__declspec\\(dllexport\\)", "Raw __declspec.", "Not cross-platform.", "Use a macro like API_EXPORT"},
    {"SYN40", Category::SYNTAX, WarningLevel::ERR, "return\\s+[a-zA-Z0-9_]+\\s*=\\s*[a-zA-Z0-9_]+;", "Returning an assignment.", "Confusing and prone to logic errors.", "Separate the assignment and return"},

    // -----------------------------------------
    // CATEGORY 5: ARCHITECTURE (40 Rules)
    // -----------------------------------------
    {"ARCH01", Category::ARCHITECTURE, WarningLevel::INFO, "\\bglobal_[a-zA-Z0-9_]+", "Use of global variable.", "Globals break encapsulation.", "Pass by reference/injection"},
    {"ARCH02", Category::ARCHITECTURE, WarningLevel::INFO, "\\bgetInstance\\(\\)", "Singleton pattern detected.", "Singletons act as global state.", "Use Dependency Injection"},
    {"ARCH03", Category::ARCHITECTURE, WarningLevel::INFO, "\\bclass.*Manager\\b", "God class 'Manager'.", "Classes should have specific responsibilities.", "Split into specialized classes"},
    {"ARCH04", Category::ARCHITECTURE, WarningLevel::INFO, "\\bclass.*Helper\\b", "God class 'Helper'.", "Helper is a vague term.", "Rename to reflect exact utility"},
    {"ARCH05", Category::ARCHITECTURE, WarningLevel::INFO, "\\bclass.*Utils\\b", "God class 'Utils'.", "Utility classes easily become dump grounds.", "Use namespaces and free functions"},
    {"ARCH06", Category::ARCHITECTURE, WarningLevel::WARNING, "friend\\s+class\\s+", "Use of friend class.", "Tightly couples two classes.", "Use public interfaces"},
    {"ARCH07", Category::ARCHITECTURE, WarningLevel::WARNING, "public:\\s*[a-zA-Z0-9_]+\\s+[a-zA-Z0-9_]+\\s*;", "Public data member.", "Breaks encapsulation.", "Make private and use getters/setters"},
    {"ARCH08", Category::ARCHITECTURE, WarningLevel::WARNING, "\\bmutable\\s+", "Use of mutable.", "Bypasses const correctness.", "Refactor logic to not require state change"},
    {"ARCH09", Category::ARCHITECTURE, WarningLevel::INFO, "\\\\#include\\s+[\"<]windows.h[\">]", "Including windows.h directly in header.", "Pollutes global namespace.", "Include in .cpp files only"},
    {"ARCH10", Category::ARCHITECTURE, WarningLevel::WARNING, "protected:\\s*[a-zA-Z0-9_]+\\s+[a-zA-Z0-9_]+\\s*;", "Protected data member.", "Fragile base class problem.", "Make private and use protected getters"},
    {"ARCH11", Category::ARCHITECTURE, WarningLevel::INFO, "namespace\\s*\\{", "Anonymous namespace in header.", "Causes duplicate symbols and bloat.", "Use static or move to .cpp"},
    {"ARCH12", Category::ARCHITECTURE, WarningLevel::INFO, "\\bextern\\s+[a-zA-Z0-9_]+", "Use of extern variables.", "Global state linking.", "Use singleton or injection"},
    {"ARCH13", Category::ARCHITECTURE, WarningLevel::INFO, "using\\s+namespace\\s+.*\\s*;", "using namespace in header.", "Forces namespace on all includers.", "Remove from header file"},
    {"ARCH14", Category::ARCHITECTURE, WarningLevel::WARNING, "\\\\#define\\s+[a-zA-Z0-9_]+\\s+.*\\\\n\\\\#define\\s+[a-zA-Z0-9_]+\\s+", "Excessive macros.", "Use enums or constexpr.", "Use enum class"},
    {"ARCH15", Category::ARCHITECTURE, WarningLevel::WARNING, "static\\s+[a-zA-Z0-9_]+\\s+[a-zA-Z0-9_]+\\s*;", "Static class members.", "Global state in disguise.", "Use instances"},
    {"ARCH16", Category::ARCHITECTURE, WarningLevel::INFO, "catch\\s*\\(\\.\\.\\.\\)", "Catch-all handler.", "Hides critical errors.", "Catch specific exception types"},
    {"ARCH17", Category::ARCHITECTURE, WarningLevel::INFO, "throw\\s+[a-zA-Z0-9_]+\\s*;", "Throwing non-std exception.", "Custom exceptions should inherit std::exception.", "Inherit from std::runtime_error"},
    {"ARCH18", Category::ARCHITECTURE, WarningLevel::INFO, "\\bvirtual\\s+.*~[a-zA-Z0-9_]+\\(\\)\\s*=\\s*0;", "Pure virtual destructor.", "Requires an implementation anyway.", "Make virtual, not pure virtual"},
    {"ARCH19", Category::ARCHITECTURE, WarningLevel::INFO, "class\\s+[a-zA-Z0-9_]+\\s*:\\s*public\\s+std::vector", "Inheriting from std::vector.", "STL containers do not have virtual destructors.", "Use composition (contain a vector)"},
    {"ARCH20", Category::ARCHITECTURE, WarningLevel::INFO, "class\\s+[a-zA-Z0-9_]+\\s*:\\s*public\\s+std::string", "Inheriting from std::string.", "Not meant for inheritance.", "Use composition"},
    {"ARCH21", Category::ARCHITECTURE, WarningLevel::INFO, "class\\s+[a-zA-Z0-9_]+\\s*:\\s*public\\s+std::map", "Inheriting from std::map.", "Not meant for inheritance.", "Use composition"},
    {"ARCH22", Category::ARCHITECTURE, WarningLevel::WARNING, "template\\s*<.*>\\s*class\\s+[a-zA-Z0-9_]+\\s*\\\\n\\{", "Complex templates in headers.", "Increases compilation time.", "Consider type erasure or pimpl"},
    {"ARCH23", Category::ARCHITECTURE, WarningLevel::WARNING, "void\\s*\\\\*\\s*", "Use of void pointers.", "Loses all type safety.", "Use std::any or templates"},
    {"ARCH24", Category::ARCHITECTURE, WarningLevel::INFO, "\\bunion\\b", "Use of union.", "Unsafe type punning.", "Use std::variant"},
    {"ARCH25", Category::ARCHITECTURE, WarningLevel::WARNING, "reinterpret_cast<.*>\\(.*\\)", "reinterpret_cast.", "Highly unsafe.", "Rethink architecture to avoid type punning"},
    {"ARCH26", Category::ARCHITECTURE, WarningLevel::INFO, "virtual\\s+void\\s+[a-zA-Z0-9_]+\\(\\)\\s*;", "Virtual function without override/final.", "Missing context.", "Use 'override' or 'final' keywords"},
    {"ARCH27", Category::ARCHITECTURE, WarningLevel::INFO, "\\bconst\\s+expr\\b", "Typo in constexpr.", "Syntax error.", "Use constexpr"},
    {"ARCH28", Category::ARCHITECTURE, WarningLevel::INFO, "\\\\#include\\s+<iostream>", "iostream in header.", "Causes heavy compile bloat.", "Include <iosfwd> in header instead"},
    {"ARCH29", Category::ARCHITECTURE, WarningLevel::WARNING, "shared_ptr<[a-zA-Z0-9_]+>\\s+[a-zA-Z0-9_]+\\s*=\\s*this", "shared_ptr to this.", "Double free risk.", "Inherit from enable_shared_from_this"},
    {"ARCH30", Category::ARCHITECTURE, WarningLevel::INFO, "delete\\s+this;", "delete this.", "Extremely fragile design pattern.", "Use smart pointer management"},
    {"ARCH31", Category::ARCHITECTURE, WarningLevel::WARNING, "std::bind\\s*\\(", "std::bind creates complex signatures.", "Lambdas are cleaner.", "Use [](){}"},
    {"ARCH32", Category::ARCHITECTURE, WarningLevel::INFO, "std::function<.*>\\s+[a-zA-Z0-9_]+\\s*;", "Uninitialized std::function.", "Throws bad_function_call if invoked.", "Initialize to nullptr"},
    {"ARCH33", Category::ARCHITECTURE, WarningLevel::INFO, "std::thread\\s+[a-zA-Z0-9_]+\\s*\\(", "Raw std::thread creation.", "Manual thread lifecycle management.", "Use thread pools or async"},
    {"ARCH34", Category::ARCHITECTURE, WarningLevel::WARNING, "\\bjoin\\(\\)", "Blocking join().", "Can cause deadlocks.", "Ensure thread safety or detach appropriately"},
    {"ARCH35", Category::ARCHITECTURE, WarningLevel::WARNING, "\\bmutex\\.lock\\(\\)", "Manual mutex lock.", "Risk of missing unlock on exception.", "Use std::lock_guard or std::unique_lock"},
    {"ARCH36", Category::ARCHITECTURE, WarningLevel::WARNING, "\\bmutex\\.unlock\\(\\)", "Manual mutex unlock.", "Not exception safe.", "Use RAII locks"},
    {"ARCH37", Category::ARCHITECTURE, WarningLevel::INFO, "new\\s+[a-zA-Z0-9_]+\\s*\\[.*\\]", "Raw array allocation.", "Manual memory management.", "Use std::vector"},
    {"ARCH38", Category::ARCHITECTURE, WarningLevel::INFO, "\\bint\\b\\s+[a-zA-Z0-9_]+\\s*\\[.*\\]", "C-style arrays.", "Lacks bounds checking.", "Use std::array"},
    {"ARCH39", Category::ARCHITECTURE, WarningLevel::WARNING, "\\\\#pragma\\s+once.*\\\\#ifndef", "Mixing #pragma once and include guards.", "Redundant.", "Use one or the other"},
    {"ARCH40", Category::ARCHITECTURE, WarningLevel::INFO, "inline\\s+namespace", "Inline namespace.", "Can confuse ABI versions.", "Use carefully for versioning only"}
};

static std::string level_to_string(WarningLevel level) {
    switch (level) {
        case WarningLevel::INFO: return "INFO";
        case WarningLevel::PEDANTIC: return "PEDANTIC";
        case WarningLevel::WARNING: return "WARNING";
        case WarningLevel::ERR: return "ERROR";
    }
    return "UNKNOWN";
}

static std::string cat_to_string(Category cat) {
    switch (cat) {
        case Category::SYNTAX: return "SYNTAX";
        case Category::STYLE: return "STYLE";
        case Category::PERFORMANCE: return "PERFORMANCE";
        case Category::SECURITY: return "SECURITY";
        case Category::ARCHITECTURE: return "ARCHITECTURE";
    }
    return "UNKNOWN";
}

void export_errors(const std::vector<LinterIssue>& issues, const std::string& filepath) {
    std::ofstream out(filepath);
    if (!out) return;
    out << "[\n";
    for (size_t i = 0; i < issues.size(); ++i) {
        out << "  {\n";
        out << "    \"line\": " << issues[i].line << ",\n";
        out << "    \"level\": \"" << level_to_string(issues[i].level) << "\",\n";
        out << "    \"message\": \"" << issues[i].message << "\"\n";
        out << "  }" << (i == issues.size() - 1 ? "" : ",") << "\n";
    }
    out << "]\n";
}

std::string get_replacement(const std::string& correction) {
    if (correction == "Use ') {'") return ") {";
    if (correction == "Remove trailing spaces") return "";
    if (correction == "Use 'if ('") return "if (";
    if (correction == "Use 'for ('") return "for (";
    if (correction == "Use 'while ('") return "while (";
    if (correction == "Use 'catch ('") return "catch (";
    if (correction == "Use 'switch ('") return "switch (";
    if (correction == "Use 'nullptr'") return "nullptr";
    if (correction == "i++") return "$1++";
    if (correction == "Use 'var' or 'const'") return "var";
    if (correction == "Use 'if (condition)'") return "";
    return "__NOT_FIXABLE__";
}

bool is_suppressed(const std::vector<std::string>& lines, int line_idx, const std::string& rule_id) {
    if (line_idx < lines.size()) {
        const std::string& curr = lines[line_idx];
        if (curr.find("citrine-disable-line") != std::string::npos) {
            if (curr.find(rule_id) != std::string::npos || curr.find("all") != std::string::npos) {
                return true;
            }
        }
    }
    if (line_idx > 0 && line_idx - 1 < lines.size()) {
        const std::string& prev = lines[line_idx - 1];
        if (prev.find("citrine-disable-next-line") != std::string::npos) {
            if (prev.find(rule_id) != std::string::npos || prev.find("all") != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

std::string get_category_icon(Category cat) {
    switch (cat) {
        case Category::SECURITY:     return "\x1b[31m🛡️  SECURITY    \x1b[0m";
        case Category::PERFORMANCE:  return "\x1b[33m⚡  PERFORMANCE \x1b[0m";
        case Category::STYLE:        return "\x1b[35m🎨  STYLE       \x1b[0m";
        case Category::SYNTAX:       return "\x1b[36m⚙️  SYNTAX      \x1b[0m";
        case Category::ARCHITECTURE: return "\x1b[34m🏛️  ARCHITECTURE\x1b[0m";
    }
    return "❓  UNKNOWN";
}

void print_ascii_header() {
    std::cout << "\x1b[36m";
    std::cout << "   ______  _  __         _               \n";
    std::cout << "  / ____/ (_)/ /_ _____ (_)____   ___    \n";
    std::cout << " / /     / // __// ___// // __ \\ / _ \\   \n";
    std::cout << "/ /___  / // /_ / /   / // / / //  __/   \n";
    std::cout << "\\____/ /_/ \\__//_/   /_//_/ /_/ \\___/    \n";
    std::cout << "\x1b[35m=========================================\x1b[0m\n";
}

void run_lint(const std::string& filepath, LintMode mode, const FilterConfig& config) {
    std::string safe_cache_path = filepath;
    for (auto& c : safe_cache_path) {
        if (c == '/' || c == '\\' || c == ':') c = '_';
    }
    safe_cache_path = ".CitrineCache." + safe_cache_path;

    if (mode == MODE_UNDO) {
        std::ifstream cache_file(safe_cache_path);
        if (!cache_file) {
            std::cerr << "Citrine Linter: No undo history found for " << filepath << "\n";
            return;
        }

        std::vector<std::string> cached_lines;
        std::string cline;
        while (std::getline(cache_file, cline)) {
            cached_lines.push_back(cline);
        }
        cache_file.close();

        std::ofstream target_file(filepath);
        if (!target_file) {
            std::cerr << "Citrine Linter: Error could not write back to " << filepath << "\n";
            return;
        }
        for (size_t i = 0; i < cached_lines.size(); ++i) {
            target_file << cached_lines[i] << (i == cached_lines.size() - 1 ? "" : "\n");
        }
        target_file.close();

        std::cout << "Undo successful! Restored " << filepath << " to its previous state.\n";
        return;
    }

    std::ifstream file(filepath);
    if (!file) {
        std::cerr << "Citrine Linter: Error could not read " << filepath << "\n";
        return;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();

    std::vector<LinterIssue> found_issues;
    std::vector<std::string> modified_lines = lines;
    bool has_modifications = false;

    // Track statistics for codebase health score card
    int count_sec = 0, count_perf = 0, count_style = 0, count_syntax = 0, count_arch = 0;
    int total_penalty = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        for (const auto& rule : g_rules) {
            // Apply inline suppression check
            if (is_suppressed(lines, i, rule.id)) {
                continue;
            }

            try {
                std::regex re(rule.pattern);
                std::smatch match;
                if (std::regex_search(lines[i], match, re)) {
                    // Check CLI filters
                    if (config.has_category_filter && config.category_filter != rule.category) {
                        continue;
                    }
                    if (config.has_level_filter && config.level_filter > rule.level) {
                        continue;
                    }

                    LinterIssue issue;
                    issue.line = i + 1;
                    issue.level = rule.level;
                    issue.category = rule.category;
                    issue.message = rule.message;
                    issue.explanation = rule.explanation;
                    issue.correction = rule.correction;
                    issue.col = match.position();
                    issue.length = match.length();
                    found_issues.push_back(issue);

                    // Severity Penalty Metrics
                    int penalty = 1;
                    if (rule.level == WarningLevel::PEDANTIC) penalty = 2;
                    else if (rule.level == WarningLevel::WARNING) penalty = 5;
                    else if (rule.level == WarningLevel::ERR) penalty = 15;
                    total_penalty += penalty;

                    if (rule.category == Category::SECURITY) count_sec++;
                    else if (rule.category == Category::PERFORMANCE) count_perf++;
                    else if (rule.category == Category::STYLE) count_style++;
                    else if (rule.category == Category::SYNTAX) count_syntax++;
                    else if (rule.category == Category::ARCHITECTURE) count_arch++;
                }
            } catch (const std::exception& e) {
                // skip bad regex
            }
        }
    }

    print_ascii_header();
    std::cout << "\x1b[1mAnalyzing: \x1b[32m" << filepath << "\x1b[0m\n\n";

    if (found_issues.empty()) {
        std::cout << "✨ \x1b[32m\x1b[1mNo issues found! Your codebase is pristine.\x1b[0m\n\n";
        return;
    }

    for (const auto& issue : found_issues) {
        if (issue.line - 1 >= lines.size()) continue;

        if (mode == MODE_FIX) {
            std::string repl = get_replacement(issue.correction);
            if (repl == "__NOT_FIXABLE__") {
                continue;
            }
        }

        std::cout << "\x1b[36m[Line " << issue.line << "]\x1b[0m ";
        std::cout << get_category_icon(issue.category) << " ";
        
        if (issue.level == WarningLevel::ERR) std::cout << "\x1b[31m[ERROR]\x1b[0m ";
        else if (issue.level == WarningLevel::WARNING) std::cout << "\x1b[33m[WARNING]\x1b[0m ";
        else if (issue.level == WarningLevel::INFO) std::cout << "\x1b[36m[INFO]\x1b[0m ";
        else std::cout << "\x1b[35m[PEDANTIC]\x1b[0m ";

        std::cout << "\x1b[1m" << issue.message << "\x1b[0m\n";

        if (mode == MODE_EXPLAIN) {
            std::cout << "    \x1b[34m=> Explanation:\x1b[0m " << issue.explanation << "\n";
            if (!issue.correction.empty()) {
                std::cout << "    \x1b[32m=> Suggestion:\x1b[0m " << issue.correction << "\n";
            }
        } else if (mode == MODE_FIX) {
            std::string repl = get_replacement(issue.correction);
            if (repl != "__NOT_FIXABLE__") {
                std::cout << "    \x1b[34m=> Explanation:\x1b[0m " << issue.explanation << "\n";
                std::cout << "    \x1b[33m[Before]:\x1b[0m " << lines[issue.line - 1] << "\n";
                
                std::string actual_pattern;
                for (const auto& r : g_rules) {
                    if (r.message == issue.message) {
                        actual_pattern = r.pattern;
                        break;
                    }
                }
                if (!actual_pattern.empty()) {
                    try {
                        std::regex actual_re(actual_pattern);
                        std::string proposed = std::regex_replace(lines[issue.line - 1], actual_re, repl);
                        std::cout << "    \x1b[32m[After]: \x1b[0m " << proposed << "\n";
                        
                        std::cout << "  ? Apply this fix? (y/n): ";
                        std::string confirm;
                        std::getline(std::cin, confirm);
                        if (confirm == "y") {
                            modified_lines[issue.line - 1] = proposed;
                            has_modifications = true;
                            std::cout << "    \x1b[32m[Fixed]\x1b[0m\n";
                        }
                    } catch (...) {}
                }
            }
        }
    }

    if (has_modifications) {
        std::ofstream cache_out(safe_cache_path);
        if (cache_out) {
            for (size_t i = 0; i < lines.size(); ++i) {
                cache_out << lines[i] << (i == lines.size() - 1 ? "" : "\n");
            }
            cache_out.close();
            
#ifdef _WIN32
            SetFileAttributesA(safe_cache_path.c_str(), FILE_ATTRIBUTE_HIDDEN);
#endif
        }

        std::ofstream out(filepath);
        for (size_t i = 0; i < modified_lines.size(); ++i) {
            out << modified_lines[i] << (i == modified_lines.size() - 1 ? "" : "\n");
        }
        out.close();
        std::cout << "\n\x1b[32mApplied corrections saved to " << filepath << "\x1b[0m\n";
    }

    // Render Health Score Summary Card
    int health_score = 100 - total_penalty;
    if (health_score < 0) health_score = 0;

    std::string grade = "F";
    std::string grade_color = "\x1b[31m"; // Red
    if (health_score >= 95) { grade = "A+"; grade_color = "\x1b[32m"; }
    else if (health_score >= 90) { grade = "A"; grade_color = "\x1b[32m"; }
    else if (health_score >= 80) { grade = "B"; grade_color = "\x1b[33m"; }
    else if (health_score >= 70) { grade = "C"; grade_color = "\x1b[33m"; }
    else if (health_score >= 60) { grade = "D"; grade_color = "\x1b[35m"; }

    std::cout << "\n\x1b[35m====================================================\x1b[0m\n";
    std::cout << "              \x1b[1mCODEBASE HEALTH REPORT\x1b[0m\n";
    std::cout << "\x1b[35m====================================================\x1b[0m\n";
    
    auto render_bar = [](int count, const std::string& color) {
        int blocks = count;
        if (blocks > 15) blocks = 15;
        std::string bar = "";
        for (int b = 0; b < blocks; ++b) bar += "█";
        for (int b = blocks; b < 15; ++b) bar += "░";
        return color + bar + "\x1b[0m";
    };

    std::cout << get_category_icon(Category::SECURITY)     << " : [" << count_sec    << "] " << render_bar(count_sec, "\x1b[31m") << "\n";
    std::cout << get_category_icon(Category::PERFORMANCE)  << " : [" << count_perf   << "] " << render_bar(count_perf, "\x1b[33m") << "\n";
    std::cout << get_category_icon(Category::STYLE)        << " : [" << count_style  << "] " << render_bar(count_style, "\x1b[35m") << "\n";
    std::cout << get_category_icon(Category::SYNTAX)       << " : [" << count_syntax << "] " << render_bar(count_syntax, "\x1b[36m") << "\n";
    std::cout << get_category_icon(Category::ARCHITECTURE) << " : [" << count_arch   << "] " << render_bar(count_arch, "\x1b[34m") << "\n";
    
    std::cout << "\x1b[35m----------------------------------------------------\x1b[0m\n";
    std::cout << "\x1b[1mCode Health Score :\x1b[0m " << grade_color << health_score << "%\x1b[0m (Grade: " << grade_color << grade << "\x1b[0m)\n";
    std::cout << "\x1b[35m====================================================\x1b[0m\n\n";
}

} // namespace citrine
