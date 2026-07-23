#include "postgres_api.h"
#include "../vm/vm.h"
#include "../vm/object.h"
#include "../vm/value.h"

#ifdef USE_POSTGRESQL
#include <libpq-fe.h>
#include <map>
#include <string>
#include <iostream>

static std::map<int, PGconn*> pg_connections;
static int next_pg_id = 1;

static SapphireValue postgres_api_connect(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        return SapphireValue(-1.0);
    }
    
    std::string conn_str = static_cast<ObjString*>(args[0].as.obj)->chars;
    PGconn* conn = PQconnectdb(conn_str.c_str());
    
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "PostgreSQL Connection Error: " << PQerrorMessage(conn) << std::endl;
        PQfinish(conn);
        return SapphireValue(-1.0);
    }

    int id = next_pg_id++;
    pg_connections[id] = conn;
    return SapphireValue((double)id);
}

static SapphireValue postgres_api_query(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || args[0].type != ValType::VAL_NUMBER || !is_obj_type(args[1], OBJ_STRING)) {
        return SapphireValue(new_array(g_current_vm));
    }
    
    int db_id = (int)args[0].as.number;
    if (pg_connections.find(db_id) == pg_connections.end()) {
        return SapphireValue(new_array(g_current_vm));
    }
    
    PGconn* conn = pg_connections[db_id];
    ObjString* query = static_cast<ObjString*>(args[1].as.obj);
    
    PGresult* res = PQexec(conn, query->chars.c_str());
    ExecStatusType status = PQresultStatus(res);
    
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        std::cerr << "PostgreSQL Query Error: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        return SapphireValue(new_array(g_current_vm));
    }
    
    if (status == PGRES_COMMAND_OK) {
        std::string tuples = PQcmdTuples(res);
        double affected = 0.0;
        if (!tuples.empty()) affected = std::stod(tuples);
        PQclear(res);
        return SapphireValue(affected);
    }
    
    ObjArray* rows_array = new_array(g_current_vm);
    g_current_vm->push(SapphireValue(rows_array));
    
    int nFields = PQnfields(res);
    int nTuples = PQntuples(res);
    
    for (int i = 0; i < nTuples; i++) {
        ObjMap* row_map = new_map(g_current_vm);
        g_current_vm->push(SapphireValue(row_map));
        for (int j = 0; j < nFields; j++) {
            std::string col_name = PQfname(res, j);
            std::string val = PQgetisnull(res, i, j) ? "" : PQgetvalue(res, i, j);
            row_map->items[col_name] = SapphireValue(new_string(g_current_vm, val));
        }
        g_current_vm->pop();
        rows_array->elements.push_back(SapphireValue(row_map));
    }
    
    PQclear(res);
    g_current_vm->pop();
    
    return SapphireValue(rows_array);
}

static SapphireValue postgres_api_close(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_NUMBER) return false;
    
    int db_id = (int)args[0].as.number;
    auto it = pg_connections.find(db_id);
    if (it != pg_connections.end()) {
        PQfinish(it->second);
        pg_connections.erase(it);
        return true;
    }
    return false;
}

#endif // USE_POSTGRESQL

void define_postgres_natives(VM* vm) {
#ifdef USE_POSTGRESQL
    vm->define_native("pgConnect", postgres_api_connect);
    vm->define_native("pgQuery", postgres_api_query);
    vm->define_native("pgClose", postgres_api_close);
#else
    (void)vm; // Silence unused warning
#endif
}
