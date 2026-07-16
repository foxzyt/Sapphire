// Test: SQLite module operations
function main() {
    var sql = SQLite();
    var db = sql.open(":memory:");
    
    // Create table
    sql.execute("CREATE TABLE users (id INTEGER, name TEXT, score INTEGER)");
    
    // Insert data
    sql.execute("INSERT INTO users VALUES (1, 'Alice', 100)");
    sql.execute("INSERT INTO users VALUES (2, 'Bob', 200)");
    
    // Query data
    var rows = sql.query("SELECT * FROM users WHERE score > 150");
    if (rows == nil) { print("FAIL: SQLite query returned nil"); return; }
    
    print("SQLite module tests passed.");
}
main();