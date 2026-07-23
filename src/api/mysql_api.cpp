#include "mysql_api.h"
#include "../vm/vm.h"
#include "../vm/object.h"
#include "../vm/value.h"

#ifdef USE_MYSQL
#include <mysql.h>
#include <map>
#include <string>
#include <iostream>

static std::map<int, MYSQL*> mysql_connections;
static int next_mysql_id = 1;

static SapphireValue mysql_api_connect(int arg_count, SapphireValue* args) {
    if (arg_count < 4 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING) ||
        !is_obj_type(args[2], OBJ_STRING) || !is_obj_type(args[3], OBJ_STRING)) {
        return SapphireValue(-1.0);
    }
    
    std::string host = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string user = static_cast<ObjString*>(args[1].as.obj)->chars;
    std::string pass = static_cast<ObjString*>(args[2].as.obj)->chars;
    std::string db = static_cast<ObjString*>(args[3].as.obj)->chars;
    int port = 3306;
    if (arg_count >= 5 && args[4].type == ValType::VAL_NUMBER) {
        port = (int)args[4].as.number;
    }

    MYSQL* mysql = mysql_init(NULL);
    if (!mysql) return SapphireValue(-1.0);

    if (!mysql_real_connect(mysql, host.c_str(), user.c_str(), pass.c_str(), db.c_str(), port, NULL, 0)) {
        std::cerr << "MySQL Connection Error: " << mysql_error(mysql) << std::endl;
        mysql_close(mysql);
        return SapphireValue(-1.0);
    }

    int id = next_mysql_id++;
    mysql_connections[id] = mysql;
    return SapphireValue((double)id);
}

static SapphireValue mysql_api_query(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || args[0].type != ValType::VAL_NUMBER || !is_obj_type(args[1], OBJ_STRING)) {
        return SapphireValue(new_array(g_current_vm));
    }
    
    int db_id = (int)args[0].as.number;
    if (mysql_connections.find(db_id) == mysql_connections.end()) {
        return SapphireValue(new_array(g_current_vm));
    }
    
    MYSQL* mysql = mysql_connections[db_id];
    ObjString* query = static_cast<ObjString*>(args[1].as.obj);
    
    if (mysql_query(mysql, query->chars.c_str())) {
        std::cerr << "MySQL Query Error: " << mysql_error(mysql) << std::endl;
        return SapphireValue(new_array(g_current_vm));
    }
    
    MYSQL_RES* result = mysql_store_result(mysql);
    if (!result) {
        if (mysql_field_count(mysql) == 0) {
            // It was a non-SELECT query (UPDATE/INSERT/DELETE)
            return SapphireValue((double)mysql_affected_rows(mysql));
        } else {
            std::cerr << "MySQL Store Result Error: " << mysql_error(mysql) << std::endl;
            return SapphireValue(new_array(g_current_vm));
        }
    }
    
    ObjArray* rows_array = new_array(g_current_vm);
    g_current_vm->push(SapphireValue(rows_array));
    
    int num_fields = mysql_num_fields(result);
    MYSQL_FIELD* fields = mysql_fetch_fields(result);
    MYSQL_ROW row;
    
    while ((row = mysql_fetch_row(result))) {
        ObjMap* row_map = new_map(g_current_vm);
        g_current_vm->push(SapphireValue(row_map));
        for (int i = 0; i < num_fields; i++) {
            std::string col_name = fields[i].name;
            std::string val = row[i] ? row[i] : "";
            row_map->items[col_name] = SapphireValue(new_string(g_current_vm, val));
        }
        g_current_vm->pop();
        rows_array->elements.push_back(SapphireValue(row_map));
    }
    
    mysql_free_result(result);
    g_current_vm->pop();
    
    return SapphireValue(rows_array);
}

static SapphireValue mysql_api_close(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_NUMBER) return false;
    
    int db_id = (int)args[0].as.number;
    auto it = mysql_connections.find(db_id);
    if (it != mysql_connections.end()) {
        mysql_close(it->second);
        mysql_connections.erase(it);
        return true;
    }
    return false;
}

#endif // USE_MYSQL

void define_mysql_natives(VM* vm) {
#ifdef USE_MYSQL
    vm->define_native("mysqlConnect", mysql_api_connect);
    vm->define_native("mysqlQuery", mysql_api_query);
    vm->define_native("mysqlClose", mysql_api_close);
#else
    (void)vm; // Silence unused warning
#endif
}
