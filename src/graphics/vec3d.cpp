#include "vec3d.h"
#include <cmath>
#include <iostream>

static SapphireValue vec3d_add(int arg_count, SapphireValue* args) {
    if (arg_count != 1) return SapphireValue();
    ObjInstance* self = (ObjInstance*)std::get<Obj*>(args[-1]._value);
    ObjInstance* other = (ObjInstance*)std::get<Obj*>(args[0]._value);

    double x1 = std::get<double>(self->fields["x"]._value);
    double y1 = std::get<double>(self->fields["y"]._value);
    double z1 = std::get<double>(self->fields["z"]._value);
    
    double x2 = std::get<double>(other->fields["x"]._value);
    double y2 = std::get<double>(other->fields["y"]._value);
    double z2 = std::get<double>(other->fields["z"]._value);

    self->fields["x"] = SapphireValue(x1 + x2);
    self->fields["y"] = SapphireValue(y1 + y2);
    self->fields["z"] = SapphireValue(z1 + z2);
    return SapphireValue((Obj*)self);
}

static SapphireValue vec3d_length(int arg_count, SapphireValue* args) {
    ObjInstance* self = (ObjInstance*)std::get<Obj*>(args[-1]._value);
    double x = std::get<double>(self->fields["x"]._value);
    double y = std::get<double>(self->fields["y"]._value);
    double z = std::get<double>(self->fields["z"]._value);
    return SapphireValue(std::sqrt(x * x + y * y + z * z));
}

void register_vec3d_class(VM* vm) {
    ObjClass* vec3d_class = new_class(vm, new_string(vm, "Vec3D"));

    vec3d_class->methods["add"] = SapphireValue((Obj*)new_native(vm, vec3d_add));
    vec3d_class->methods["length"] = SapphireValue((Obj*)new_native(vm, vec3d_length));

    vm->globals["Vec3D"] = SapphireValue((Obj*)vec3d_class);
}
