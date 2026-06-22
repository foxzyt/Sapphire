#include "vec2d.h"
#include <cmath>
#include <iostream>

static SapphireValue vec2d_add(int arg_count, SapphireValue* args) {
    if (arg_count != 1) return SapphireValue();
    ObjInstance* self = (ObjInstance*)std::get<Obj*>(args[-1]._value);
    ObjInstance* other = (ObjInstance*)std::get<Obj*>(args[0]._value);

    double x1 = std::get<double>(self->fields["x"]._value);
    double y1 = std::get<double>(self->fields["y"]._value);
    double x2 = std::get<double>(other->fields["x"]._value);
    double y2 = std::get<double>(other->fields["y"]._value);

    self->fields["x"] = SapphireValue(x1 + x2);
    self->fields["y"] = SapphireValue(y1 + y2);
    return SapphireValue((Obj*)self); // Return self for chaining
}

static SapphireValue vec2d_sub(int arg_count, SapphireValue* args) {
    if (arg_count != 1) return SapphireValue();
    ObjInstance* self = (ObjInstance*)std::get<Obj*>(args[-1]._value);
    ObjInstance* other = (ObjInstance*)std::get<Obj*>(args[0]._value);

    double x1 = std::get<double>(self->fields["x"]._value);
    double y1 = std::get<double>(self->fields["y"]._value);
    double x2 = std::get<double>(other->fields["x"]._value);
    double y2 = std::get<double>(other->fields["y"]._value);

    self->fields["x"] = SapphireValue(x1 - x2);
    self->fields["y"] = SapphireValue(y1 - y2);
    return SapphireValue((Obj*)self);
}

static SapphireValue vec2d_length(int arg_count, SapphireValue* args) {
    ObjInstance* self = (ObjInstance*)std::get<Obj*>(args[-1]._value);
    double x = std::get<double>(self->fields["x"]._value);
    double y = std::get<double>(self->fields["y"]._value);
    return SapphireValue(std::sqrt(x * x + y * y));
}

void register_vec2d_class(VM* vm) {
    ObjClass* vec2d_class = new_class(vm, new_string(vm, "Vec2D"));

    // native methods
    vec2d_class->methods["add"] = SapphireValue((Obj*)new_native(vm, vec2d_add));
    vec2d_class->methods["sub"] = SapphireValue((Obj*)new_native(vm, vec2d_sub));
    vec2d_class->methods["length"] = SapphireValue((Obj*)new_native(vm, vec2d_length));

    vm->globals["Vec2D"] = SapphireValue((Obj*)vec2d_class);
}
