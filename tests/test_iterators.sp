var arr = [10, 20, 30];
print("FOR IN ARRAY:");
for (var i in arr) {
    print(i);
}

print("FOR OF ARRAY:");
for (var v of arr) {
    print(v);
}

print("FOREACH ARRAY:");
foreach (var v in arr) {
    print(v);
}
print("Before map!");
var m = {"a": 1, "b": 2};
print("After map!");
print("FOR IN MAP:");
for (var k in m) {
    print(k);
}

print("FOR OF MAP:");
for (var v of m) {
    print(v);
}
print("Test passed.");