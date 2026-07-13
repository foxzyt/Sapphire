print("Testing finally...");

print("Case 1: No exception");
try {
    print("  Inside try");
} catch (e) {
    print("  Inside catch: this should not print");
} finally {
    print("  Inside finally: this should print!");
}

print("---");

print("Case 2: With exception");
try {
    print("  Inside try 2");
    throw "Oops!";
} catch (e) {
    print("  Inside catch 2: ");
    print(e);
} finally {
    print("  Inside finally 2: this should print!");
}

print("Done!");
