// test_math.sp
// Garnet Math Test Suite

function testAddition() {
    assert(1 + 1 == 2, "1 + 1 should equal 2");
}

function testSubtraction() {
    assert(10 - 4 == 6, "10 - 4 should equal 6");
}

function shouldFail() {
    assert(2 * 3 == 7, "Deliberate failure: 2 * 3 is not 7");
}
