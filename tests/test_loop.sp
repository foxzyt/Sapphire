function main() {
    var i = 0;
    var sum = 0;
    while (i < 10) {
        sum = sum + i;
        i = i + 1;
    }
    print("Loop test passed. Sum=" + valueToString(sum));
}
main();
