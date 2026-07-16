function main() {
    var lst = listCreate();
    listAppend(lst, 100);
    listAppend(lst, 200);
    var val = listGet(lst, 1);
    if (val == 200) {
        print("List test passed.");
    }

    var values = [];
    values[len(values)] = 10.0;
    values[len(values)] = 20.0;
    if (len(values) == 2 && values[0] == 10.0 && values[1] == 20.0) {
        print("Array append semantics passed.");
    }
}
main();
