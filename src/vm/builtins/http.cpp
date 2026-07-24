#include "builtins.h"
#include "httplib.h"
extern std::mutex thread_mutex;

static SapphireValue convert_response_to_map(const httplib::Result& res) {
    if (!res) return SapphireValue(new_map(g_current_vm));
    ObjMap* map_obj = new_map(g_current_vm);
    g_current_vm->push(SapphireValue(map_obj));
    
    map_obj->items["status"] = SapphireValue((double)res->status);
    map_obj->items["body"] = SapphireValue(new_string(g_current_vm, res->body));
    
    ObjMap* headers_map = new_map(g_current_vm);
    for (const auto& pair : res->headers) {
        headers_map->items[pair.first] = SapphireValue(new_string(g_current_vm, pair.second));
    }
    map_obj->items["headers"] = SapphireValue(headers_map);
    
    g_current_vm->pop();
    return SapphireValue(map_obj);
}


static httplib::Headers parse_headers(SapphireValue headers_val) {
    httplib::Headers headers;
    if (is_obj_type(headers_val, OBJ_MAP)) {
        ObjMap* map_obj = static_cast<ObjMap*>(headers_val.as.obj);
        for (const auto& pair : map_obj->items) {
            SapphireValue str_val = native_value_to_string(1, const_cast<SapphireValue*>(&pair.second));
            if (is_obj_type(str_val, OBJ_STRING)) {
                headers.insert({pair.first, static_cast<ObjString*>(str_val.as.obj)->chars});
            }
        }
    }
    return headers;
}

#include "../object.h"
#include "../value.h"


SapphireValue native_http_get(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: HTTP.get() expects 1 string type argument (the URL)." << std::endl;
        }
        return {};
    }

    ObjString* url_obj = static_cast<ObjString*>(args[0].as.obj);
    std::string url_str = url_obj->chars;
    std::string host, path;

    size_t host_start = url_str.find("://");
    if (host_start == std::string::npos) {
        if (!g_current_vm->soft_mode) {
            std::cerr << "Runtime Error: Invalid URL." << std::endl;
        }
        return {};
    }
    host_start += 3;
    size_t path_start = url_str.find('/', host_start);
    if (path_start == std::string::npos) {
        host = url_str;
        path = "/";
    } else {
        host = url_str.substr(0, path_start);
        path = url_str.substr(path_start);
    }

    try {
        httplib::Client cli(host.c_str());
        cli.set_follow_location(true);
        auto res = cli.Get(path.c_str());

        if (res && res->status == 200) {
            return new_string(g_current_vm, res->body);
        }
    } catch (...) {}

    return {};
}

SapphireValue native_http_ping(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return false;

    std::string url_str = static_cast<ObjString*>(args[0].as.obj)->chars;

    try {
        httplib::Client cli(url_str.c_str());
        cli.set_connection_timeout(std::chrono::seconds(2));
        if (auto res = cli.Get("/")) {
            return res->status == 200;
        }
    } catch (...) {}

    return false;
}

SapphireValue native_http_post(int arg_count, SapphireValue* args) {
    if (arg_count < 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) return {};

    std::string url_str = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string body = static_cast<ObjString*>(args[1].as.obj)->chars;
    std::string content_type = (arg_count == 3 && is_obj_type(args[2], OBJ_STRING))
                               ? static_cast<ObjString*>(args[2].as.obj)->chars
                               : "application/json";

    try {
        httplib::Client cli(url_str.c_str());
        if (auto res = cli.Post("/", body, content_type.c_str())) {
            return new_string(g_current_vm, res->body);
        }
    } catch (...) {}

    return {};
}

SapphireValue native_http_download(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) return false;

    std::string url_str = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string path = static_cast<ObjString*>(args[1].as.obj)->chars;

    try {
        httplib::Client cli(url_str.c_str());
        auto res = cli.Get("/");
        if (res && res->status == 200) {
            std::ofstream file(path, std::ios::binary);
            if (!file.is_open()) return false;
            file << res->body;
            file.close();
            return true;
        }
    } catch (...) {}

    return false;
}

SapphireValue native_http_get_full(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !is_obj_type(args[0], OBJ_STRING)) return SapphireValue();
    std::string url_str = static_cast<ObjString*>(args[0].as.obj)->chars;
    
    std::string host, path = "/";
    size_t host_start = url_str.find("://");
    if (host_start != std::string::npos) {
        host_start += 3;
        size_t path_start = url_str.find("/", host_start);
        if (path_start == std::string::npos) {
            host = url_str;
        } else {
            host = url_str.substr(0, path_start);
            path = url_str.substr(path_start);
        }
    } else {
        size_t path_start = url_str.find("/");
        if (path_start == std::string::npos) {
            host = url_str;
        } else {
            host = url_str.substr(0, path_start);
            path = url_str.substr(path_start);
        }
    }
    
    httplib::Headers headers;
    if (arg_count >= 2) headers = parse_headers(args[1]);
    
    try {
        httplib::Client cli(host.c_str());
        cli.set_follow_location(true);
        if (auto res = cli.Get(path.c_str(), headers)) {
            return convert_response_to_map(res);
        }
    } catch (...) {}
    
    return SapphireValue(new_map(g_current_vm));
}

SapphireValue native_http_post_full(int arg_count, SapphireValue* args) {
    if (arg_count < 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) return SapphireValue();
    std::string url_str = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string body = static_cast<ObjString*>(args[1].as.obj)->chars;
    
    std::string host, path = "/";
    size_t host_start = url_str.find("://");
    if (host_start != std::string::npos) {
        host_start += 3;
        size_t path_start = url_str.find("/", host_start);
        if (path_start == std::string::npos) {
            host = url_str;
        } else {
            host = url_str.substr(0, path_start);
            path = url_str.substr(path_start);
        }
    }
    
    httplib::Headers headers;
    std::string content_type = "application/json";
    if (arg_count >= 3) {
        headers = parse_headers(args[2]);
        auto it = headers.find("Content-Type");
        if (it != headers.end()) {
            content_type = it->second;
        }
    }
    
    try {
        httplib::Client cli(host.c_str());
        if (auto res = cli.Post(path.c_str(), headers, body, content_type.c_str())) {
            return convert_response_to_map(res);
        }
    } catch (...) {}
    return SapphireValue(new_map(g_current_vm));
}

SapphireValue native_http_put_full(int arg_count, SapphireValue* args) {
    if (arg_count < 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) return SapphireValue();
    std::string url_str = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string body = static_cast<ObjString*>(args[1].as.obj)->chars;
    
    std::string host, path = "/";
    size_t host_start = url_str.find("://");
    if (host_start != std::string::npos) {
        host_start += 3;
        size_t path_start = url_str.find("/", host_start);
        if (path_start == std::string::npos) {
            host = url_str;
        } else {
            host = url_str.substr(0, path_start);
            path = url_str.substr(path_start);
        }
    }
    
    httplib::Headers headers;
    std::string content_type = "application/json";
    if (arg_count >= 3) {
        headers = parse_headers(args[2]);
        auto it = headers.find("Content-Type");
        if (it != headers.end()) {
            content_type = it->second;
        }
    }
    
    try {
        httplib::Client cli(host.c_str());
        if (auto res = cli.Put(path.c_str(), headers, body, content_type.c_str())) {
            return convert_response_to_map(res);
        }
    } catch (...) {}
    return SapphireValue(new_map(g_current_vm));
}

SapphireValue native_http_delete_full(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || !is_obj_type(args[0], OBJ_STRING)) return SapphireValue();
    std::string url_str = static_cast<ObjString*>(args[0].as.obj)->chars;
    
    std::string host, path = "/";
    size_t host_start = url_str.find("://");
    if (host_start != std::string::npos) {
        host_start += 3;
        size_t path_start = url_str.find("/", host_start);
        if (path_start == std::string::npos) {
            host = url_str;
        } else {
            host = url_str.substr(0, path_start);
            path = url_str.substr(path_start);
        }
    }
    
    httplib::Headers headers;
    if (arg_count >= 2) headers = parse_headers(args[1]);
    
    try {
        httplib::Client cli(host.c_str());
        if (auto res = cli.Delete(path.c_str(), headers)) {
            return convert_response_to_map(res);
        }
    } catch (...) {}
    return SapphireValue(new_map(g_current_vm));
}

SapphireValue native_http_serve(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || args[0].type != ValType::VAL_NUMBER || args[1].type != ValType::VAL_OBJ) {
        if (!g_current_vm->soft_mode) std::cerr << "Runtime Error: httpServer() expects a number and a function/closure." << std::endl;
        return false;
    }
    
    int port = (int)args[0].as.number;
    SapphireValue callback = args[1];
    
    httplib::Server svr;
    VM* original_vm = g_current_vm;
    
    auto handler = [callback, original_vm](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(thread_mutex);
        
        VM* prev_vm = g_current_vm;
        g_current_vm = original_vm;
        
        ObjMap* req_map = new_map(g_current_vm);
        req_map->items["method"] = new_string(g_current_vm, req.method);
        req_map->items["path"] = new_string(g_current_vm, req.path);
        req_map->items["body"] = new_string(g_current_vm, req.body);
        
        int target_frame_count = g_current_vm->frame_count;
        g_current_vm->push(callback);
        g_current_vm->push(SapphireValue(req_map));
        
        if (g_current_vm->call_value(callback, 1)) {
            if (g_current_vm->run(target_frame_count)) {
                SapphireValue result = g_current_vm->pop();
                if (is_obj_type(result, OBJ_STRING)) {
                    res.set_content(static_cast<ObjString*>(result.as.obj)->chars, "text/plain");
                } else if (is_obj_type(result, OBJ_MAP)) {
                    ObjMap* map_res = static_cast<ObjMap*>(result.as.obj);
                    std::string body = "";
                    std::string ctype = "text/plain";
                    if (map_res->items.count("body")) {
                        if (is_obj_type(map_res->items["body"], OBJ_STRING)) {
                            body = static_cast<ObjString*>(map_res->items["body"].as.obj)->chars;
                        }
                    }
                    if (map_res->items.count("status")) {
                        res.status = (int)map_res->items["status"].as.number;
                    }
                    if (map_res->items.count("contentType")) {
                        if (is_obj_type(map_res->items["contentType"], OBJ_STRING)) {
                            ctype = static_cast<ObjString*>(map_res->items["contentType"].as.obj)->chars;
                        }
                    }
                    
                    if (map_res->items.count("headers")) {
                        if (is_obj_type(map_res->items["headers"], OBJ_MAP)) {
                            ObjMap* headers_map = static_cast<ObjMap*>(map_res->items["headers"].as.obj);
                            for (auto const& [key, val] : headers_map->items) {
                                if (is_obj_type(val, OBJ_STRING)) {
                                    std::string header_val = static_cast<ObjString*>(val.as.obj)->chars;
                                    res.set_header(key, header_val);
                                }
                            }
                        }
                    }

                    res.set_content(body, ctype);
                }
            }
        }
        
        g_current_vm = prev_vm;
    };

    svr.Get(".*", handler);
    svr.Post(".*", handler);
    svr.Put(".*", handler);
    svr.Patch(".*", handler);
    svr.Delete(".*", handler);
    svr.Options(".*", handler);

    std::cout << "[Sapphire] HTTP Server natively listening on port " << port << "..." << std::endl;
    bool success = svr.listen("0.0.0.0", port);
    return success;
}

