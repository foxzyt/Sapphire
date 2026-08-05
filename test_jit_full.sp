function heavy_math(a, b) {
    var c = a + b;
    var d = c * 2;
    var e = d / 4;
    return e;
}

var result = 0;
// Sapphire doesn't have standard for loops, I will use while
var i = 0;
while (i < 150) {
    result = heavy_math(15, 3);
    i = i + 1;
}
print(result);
