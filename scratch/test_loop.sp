class Test {
    function get_val() double {
        return 1.0;
    }
}
function main() void {
    var t = Test();
    var i = 0;
    while (i < 2) {
        print("i = " + valueToString(i));
        var v = t.get_val();
        i = i + 1;
    }
}
main();
