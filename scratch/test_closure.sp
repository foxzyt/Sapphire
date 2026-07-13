function outer() void {
    var a = 10;
    function inner() void {
        print "a is: " + a;
    }
    var myObj = Tensor(); // wait, we need a dummy class
}
class Dummy {
    function run() void {
        var x = 42;
        function inner() void {
            print "x is: " + x;
        }
        this.f = inner;
    }
}
var d = Dummy();
d.run();
d.f();
