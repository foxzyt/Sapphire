#include "sqlite_api.h"
#include "vm.h"
#include "object.h"
#include "value.h"
#include <sqlite3.h>
#include <map>
#include <string>
#include <iostream>

static std::map<int, sqlite3*> db_connections;
static int next_db_id = 1;

static SapphireValue sqlite_api_open(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        return SapphireValue(-1.0);
    }
    ObjString* path = static_cast<ObjString*>(args[0].as.obj);
    
    sqlite3* db;
    int rc = sqlite3_open(path->chars.c_str(), &db);
    
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return SapphireValue(-1.0);
    }
    
    int id = next_db_id++;
    db_connections[id] = db;
    return SapphireValue((double)id);
}

static SapphireValue sqlite_api_execute(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || args[0].type != ValType::VAL_NUMBER || !is_obj_type(args[1], OBJ_STRING)) {
        return SapphireValue(false);
    }
    
    int db_id = (int)args[0].as.number;
    if (db_connections.find(db_id) == db_connections.end()) return SapphireValue(false);
    
    sqlite3* db = db_connections[db_id];
    ObjString* query = static_cast<ObjString*>(args[1].as.obj);
    
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db, query->chars.c_str(), nullptr, nullptr, &err_msg);
    
    if (rc != SQLITE_OK) {
        if (err_msg) {
            std::cerr << "SQLite Error: " << err_msg << std::endl;
            sqlite3_free(err_msg);
        }
        return SapphireValue(false);
    }
    return SapphireValue(true);
}

static SapphireValue sqlite_api_query(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || args[0].type != ValType::VAL_NUMBER || !is_obj_type(args[1], OBJ_STRING)) {
        return SapphireValue(); // nil
    }
    
    int db_id = (int)args[0].as.number;
    if (db_connections.find(db_id) == db_connections.end()) return SapphireValue();
    
    sqlite3* db = db_connections[db_id];
    ObjString* query = static_cast<ObjString*>(args[1].as.obj);
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query->chars.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "SQLite Error: " << sqlite3_errmsg(db) << std::endl;
        return SapphireValue();
    }
    
    auto result_array = new_array(g_current_vm);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int cols = sqlite3_column_count(stmt);
        ObjMap* row_map = new_map(g_current_vm);
        
        for (int i = 0; i < cols; i++) {
            const char* col_name = sqlite3_column_name(stmt, i);
            std::string name_str = col_name ? col_name : "";
            
            SapphireValue val;
            int type = sqlite3_column_type(stmt, i);
            switch (type) {
                case SQLITE_INTEGER:
                    val = SapphireValue((double)sqlite3_column_int64(stmt, i));
                    break;
                case SQLITE_FLOAT:
                    val = SapphireValue(sqlite3_column_double(stmt, i));
                    break;
                case SQLITE_TEXT: {
                    const char* text = (const char*)sqlite3_column_text(stmt, i);
                    val = SapphireValue(new_string(g_current_vm, text ? text : ""));
                    break;
                }
                case SQLITE_NULL:
                default:
                    val = SapphireValue();
                    break;
            }
            row_map->items[name_str] = val;
        }
        result_array->elements.push_back(SapphireValue(row_map));
    }
    
    sqlite3_finalize(stmt);
    return SapphireValue(result_array);
}

static SapphireValue sqlite_api_close(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_NUMBER) return SapphireValue(false);
    
    int db_id = (int)args[0].as.number;
    if (db_connections.find(db_id) != db_connections.end()) {
        sqlite3_close(db_connections[db_id]);
        db_connections.erase(db_id);
        return SapphireValue(true);
    }
    return SapphireValue(false);
}

void define_sqlite_natives(VM* vm) {
    ObjString* sqlite_name = new_string(vm, "SQLite");
    ObjClass* sqlite_class = new_class(vm, sqlite_name);
    
    sqlite_class->methods["open"] = SapphireValue(new_native(vm, sqlite_api_open));
    sqlite_class->methods["execute"] = SapphireValue(new_native(vm, sqlite_api_execute));
    sqlite_class->methods["query"] = SapphireValue(new_native(vm, sqlite_api_query));
    sqlite_class->methods["close"] = SapphireValue(new_native(vm, sqlite_api_close));
    
    vm->globals["SQLite"] = SapphireValue(sqlite_class);
}










