async function yield_point() {
    return 0; // Async function returns immediately, wrapping in promise.
}

async function taskA() {
    print "A 1";
    await yield_point();
    print "A 2";
    await yield_point();
    print "A 3";
    return "A done";
}

async function taskB() {
    print "B 1";
    await yield_point();
    print "B 2";
    await yield_point();
    print "B 3";
    return "B done";
}

var p1 = taskA();
var p2 = taskB();

print "Main script ending. Event loop should take over!";
