print("Start");

var t0 = clock();
var sum = 0;
for (var i = 0; i < 10; i = i + 1) {
    sum = sum + i;
}
var t1 = clock();

print("Result: " + (t1 - t0) + " ms");
