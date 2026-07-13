import "sapphire_grad.sp";
var init_obj = Init();
var i = 0;
while (i < 2) {
    print("i = " + valueToString(i));
    var u = init_obj.uniform(-1.0, 1.0);
    print(valueToString(u));
    i = i + 1;
}
