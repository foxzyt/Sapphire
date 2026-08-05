print("HELLO WORLD");

function bench() {
    var sum = 0;
    for (var i = 0; i < 100; i = i + 1) {
        sum = sum + i;
    }
    return sum;
}

bench();
bench();
bench();

print("Done");
