// test_clean.sp
// A completely clean Sapphire script following all Citrine Linter rules.

class MyAwesomeClass {
    function calculateSomething() {
        const maxItems = 10;
        var sum = 0;

        for (var i = 0; i < maxItems; i++) {
            sum = sum + i;
        }

        if (sum > 0) {
            print("Positive sum!");
        } else {
            print("Zero or negative sum!");
        }

        return sum;
    }
}

var instance = MyAwesomeClass();
instance.calculateSomething();
