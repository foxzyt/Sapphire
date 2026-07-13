async function yield_point() int {
    print "Yield point running!";
    return 0; 
}

async function taskA() int {
    print "A 1";
    await yield_point();
    print "A 2";
    return 1;
}

var p1 = taskA();
print "Main script ending.";
