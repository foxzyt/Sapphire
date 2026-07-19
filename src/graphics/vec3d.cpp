#include "vec3d.h"
#include <cmath>
#include <iostream>

static SapphireValue vec3d_add(int arg_count, SapphireValue* args) {
    if (arg_count != 1) return SapphireValue();
    ObjInstance* self = (ObjInstance*)args[-1].as.obj;
    ObjInstance* other = (ObjInstance*)args[0].as.obj;

    double x1 = self->fields["x"].as.number;
    double y1 = self->fields["y"].as.number;
    double z1 = self->fields["z"].as.number;
    
    double x2 = other->fields["x"].as.number;
    double y2 = other->fields["y"].as.number;
    double z2 = other->fields["z"].as.number;

    self->fields["x"] = SapphireValue(x1 + x2);
    self->fields["y"] = SapphireValue(y1 + y2);
    self->fields["z"] = SapphireValue(z1 + z2);
    return SapphireValue((Obj*)self);
}

static SapphireValue vec3d_length(int arg_count, SapphireValue* args) {
    ObjInstance* self = (ObjInstance*)args[-1].as.obj;
    double x = self->fields["x"].as.number;
    double y = self->fields["y"].as.number;
    double z = self->fields["z"].as.number;
    return SapphireValue(std::sqrt(x * x + y * y + z * z));
}

void register_vec3d_class(VM* vm) {
    ObjClass* vec3d_class = new_class(vm, new_string(vm, "Vec3D"));

    vec3d_class->methods["add"] = SapphireValue((Obj*)new_native(vm, vec3d_add));
    vec3d_class->methods["length"] = SapphireValue((Obj*)new_native(vm, vec3d_length));

    vm->globals["Vec3D"] = SapphireValue((Obj*)vec3d_class);
}






