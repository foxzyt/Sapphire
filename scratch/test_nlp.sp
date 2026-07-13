var thread_ids = listCreate();
print("Spawning thread 0...");
var tid0 = spawn("worker_0.sp");
listAppend(thread_ids, tid0);
print("Spawning thread 1...");
var tid1 = spawn("worker_1.sp");
listAppend(thread_ids, tid1);
print("Spawning thread 2...");
var tid2 = spawn("worker_2.sp");
listAppend(thread_ids, tid2);
print("Spawning thread 3...");
var tid3 = spawn("worker_3.sp");
listAppend(thread_ids, tid3);
print("Spawning thread 4...");
var tid4 = spawn("worker_4.sp");
listAppend(thread_ids, tid4);

var k = 0;
while (k < 5) {
    var tid = listGet(thread_ids, k);
    join(tid);
    k = k + 1;
}
print("All threads completed!");
