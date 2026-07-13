// Exceptions test

print("Testing handled exception...");
try {
    print("Inside try block");
    throw "Deu ruim!";
    print("This should not be printed");
} catch (e) {
    print("Caught an exception!");
    print(e);
}
print("After try/catch block");

print("---");

print("Testing unhandled exception...");
function foo() {
    throw 404;
}
foo();
