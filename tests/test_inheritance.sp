class Animal {
    function speak() {
        print "Animal speaks";
    }

    function move() {
        print "Animal moves";
    }
}

class Dog extends Animal {
    function speak() {
        print "Dog barks";
        super.move();
    }
}

var animal = Animal();
animal.speak();
animal.move();

print "---";

var dog = Dog();
dog.speak();
dog.move();
print("Test passed.");
