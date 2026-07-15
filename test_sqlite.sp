var sql = SQLite();
var db = sql.open("test.db");
if (db == -1) {
    print("Failed to open DB");
} else {
    print("DB opened successfully: " + db);
    print("Executing CREATE TABLE...");
    var res1 = sql.execute(db, "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT);");
    print("CREATE TABLE result: " + res1);
    
    print("Executing INSERT 1...");
    sql.execute(db, "INSERT INTO users (name) VALUES ('Alice');");
    print("Executing INSERT 2...");
    sql.execute(db, "INSERT INTO users (name) VALUES ('Bob');");
    
    print("Executing QUERY...");
    var users = sql.query(db, "SELECT * FROM users;");
    print("QUERY result: " + users);
    foreach (var user in users) {
        print("User: " + user.id + " - " + user.name);
    }
    
    sql.close(db);
}
