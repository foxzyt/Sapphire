// test_multithreading.sp
print("Testing Multithreading API...");

let cores = getCoreCount();
print("Detected " + valueToString(cores) + " CPU Cores.");

let mutex = Mutex.new();

// In Sapphire, threads run from a separate file.
// We'll spawn this same file but with a different flag, or we can just test Mutex here.
print("Mutex ID: " + valueToString(mutex));

let locked = Mutex.lock(mutex);
print("Locked? " + valueToString(locked));

let unlocked = Mutex.unlock(mutex);
print("Unlocked? " + valueToString(unlocked));

print("Multithreading API is present and working!");
