var x = 2;

switch (x) {
    case 1:
        print "x is 1";
        break;
    case 2:
        print "x is 2";
        break;
    case 3:
        print "x is 3";
        break;
    default:
        print "x is something else";
        break;
}

x = 5;

switch (x) {
    case 1:
        print "x is 1";
        break;
    default:
        print "x is something else";
        break;
}
print("Test passed.");