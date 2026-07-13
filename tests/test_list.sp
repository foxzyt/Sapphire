function main() void {
    var lst = listCreate();
    listAppend(lst, 100);
    listAppend(lst, 200);
    var val = listGet(lst, 1);
    if (val == 200) {
        print("List test passed.");
    }
}
main();
