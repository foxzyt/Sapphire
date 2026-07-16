// Test: Complex inheritance with method overriding
class Animal {
    function speak() {
        return "Generic sound";
    }
}

class Dog extends Animal {
    function speak() {
        return "Woof!";
    }
}

class Cat extends Animal {
    function speak() {
        return "Meow!";
    }
}

class Puppy extends Dog {
    function speak() {
        return "Yip!";
    }
}

function main() {
    var animal = Animal();
    var dog = Dog();
    var cat = Cat();
    var puppy = Puppy();
    
    if (animal.speak() != "Generic sound") { print("FAIL: Animal.speak"); return; }
    if (dog.speak() != "Woof!") { print("FAIL: Dog.speak"); return; }
    if (cat.speak() != "Meow!") { print("FAIL: Cat.speak"); return; }
    if (puppy.speak() != "Yip!") { print("FAIL: Puppy.speak"); return; }
    
    // Inheritance chain
    if (!(puppy is Dog)) { print("FAIL: puppy is Dog"); return; }
    if (!(puppy is Animal)) { print("FAIL: puppy is Animal"); return; }
    if (!(dog is Animal)) { print("FAIL: dog is Animal"); return; }
    if (animal is Dog) { print("FAIL: animal is Dog (should be false)"); return; }
    
    print("Complex inheritance tests passed.");
}
main();