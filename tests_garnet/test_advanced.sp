// test_advanced.sp
// Advanced testing with Garnet Assertion Library and Lifecycle Hooks

var helper_value = 0;

function beforeAll() {
    helper_value = 100;
}

function afterAll() {
    helper_value = 0;
}

function beforeEach() {
    helper_value = helper_value + 1;
}

function afterEach() {
    helper_value = helper_value - 1;
}

function testEqualityAssertions() {
    assertEquals(10, 10, "Should match number");
    assertNotEquals(10, 20, "Should not match different number");
    assertEquals("Sapphire", "Sapphire", "Should match strings");
}

function testBooleanAssertions() {
    assertTrue(true, "Should be true");
    assertFalse(false, "Should be false");
}

function testNullAssertions() {
    assertNull(nil, "Should be nil");
    assertNotNull(50, "Should not be nil");
}

function badCall() {
    nonExistentFunction();
}

function testThrowsAssertion() {
    assertThrows(badCall, "Should throw since function doesn't exist");
}

function testLifecycleState() {
    // beforeEach incremented helper_value to 101 (100 from beforeAll + 1)
    assertEquals(101, helper_value, "Lifecycle helper_value should be 101");
}
