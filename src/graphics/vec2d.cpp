#include "vec2d.h"
#include <cmath>
#include <iostream>

static SapphireValue vec2d_add(int arg_count, SapphireValue* args) {
    if (arg_count != 1) return SapphireValue();
    ObjInstance* self = (ObjInstance*)args[-1].as.obj;
    ObjInstance* other = (ObjInstance*)args[0].as.obj;

    double x1 = self->fields["x"].as.number;
    double y1 = self->fields["y"].as.number;
    double x2 = other->fields["x"].as.number;
    double y2 = other->fields["y"].as.number;

    self->fields["x"] = SapphireValue(x1 + x2);
    self->fields["y"] = SapphireValue(y1 + y2);
    return SapphireValue((Obj*)self); // Return self for chaining
}

static SapphireValue vec2d_sub(int arg_count, SapphireValue* args) {
    if (arg_count != 1) return SapphireValue();
    ObjInstance* self = (ObjInstance*)args[-1].as.obj;
    ObjInstance* other = (ObjInstance*)args[0].as.obj;

    double x1 = self->fields["x"].as.number;
    double y1 = self->fields["y"].as.number;
    double x2 = other->fields["x"].as.number;
    double y2 = other->fields["y"].as.number;

    self->fields["x"] = SapphireValue(x1 - x2);
    self->fields["y"] = SapphireValue(y1 - y2);
    return SapphireValue((Obj*)self);
}

static SapphireValue vec2d_length(int arg_count, SapphireValue* args) {
    ObjInstance* self = (ObjInstance*)args[-1].as.obj;
    double x = self->fields["x"].as.number;
    double y = self->fields["y"].as.number;
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






