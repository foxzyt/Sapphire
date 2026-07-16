// Test: ListUtil module functions
function main() {
    var lst = listCreate();
    listAppend(lst, 10);
    listAppend(lst, 20);
    listAppend(lst, 30);
    
    if (listLength(lst) != 3) { print("FAIL: listLength"); return; }
    if (listGet(lst, 0) != 10) { print("FAIL: listGet index 0"); return; }
    if (listGet(lst, 1) != 20) { print("FAIL: listGet index 1"); return; }
    if (listGet(lst, 2) != 30) { print("FAIL: listGet index 2"); return; }
    
    listSet(lst, 1, 25);
    if (listGet(lst, 1) != 25) { print("FAIL: listSet"); return; }
    
    if (!listContains(lst, 25)) { print("FAIL: listContains true"); return; }
    if (listContains(lst, 99)) { print("FAIL: listContains false"); return; }
    
    listRemoveAt(lst, 0);
    if (listLength(lst) != 2) { print("FAIL: listRemoveAt length"); return; }
    if (listGet(lst, 0) != 25) { print("FAIL: listRemoveAt shift"); return; }
    
    print("Test passed.");
}
main();