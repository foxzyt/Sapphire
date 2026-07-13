import "sapphire_grad.sp";
var init_obj = Init();
print("Created Init");
var u = init_obj.uniform(-1.0, 1.0);
print("Uniform: " + valueToString(u));
