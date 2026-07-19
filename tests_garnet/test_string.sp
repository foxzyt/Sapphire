// test_string.sp
// Garnet String Test Suite

function testConcatenation() {
    assert("hello " + "world" == "hello world", "Concatenation failed");
}

function testEquality() {
    var a = "Sapphire";
    var b = "Sapphire";
    assert(a == b, "Strings should be equal");
}
