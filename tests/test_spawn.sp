function thread_work() {
    print "Inside thread!";
    var sum = 0;
    for (var i = 0; i < 10000; i = i + 1) {
        sum = sum + i;
    }
    print "Thread sum: " + sum;
}

print "Before spawn";
spawn thread_work;
print "After spawn";

// Sleep a bit using a loop to give thread time to print
var x = 0;
for (var i = 0; i < 500000; i = i + 1) {
    x = x + 1;
}
print "Done waiting. Main thread exiting.";
