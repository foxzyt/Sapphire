// Test break in while loop
var i = 0
while (i < 10) {
    if (i == 5) {
        break;
    }
    i = i + 1
}
if (i == 5) {
    print("While break passed");
} else {
    print("While break failed: " + i);
}

// Test continue in while loop
var j = 0
var sum = 0
while (j < 5) {
    j = j + 1
    if (j == 3) {
        continue;
    }
    sum = sum + j
}
// sum = 1 + 2 + 4 + 5 = 12
if (sum == 12) {
    print("While continue passed");
} else {
    print("While continue failed: " + sum);
}

// Test break in for loop
var for_break = 0
for (var k = 0; k < 10; k = k + 1) {
    if (k == 4) {
        break;
    }
    for_break = k
}
if (for_break == 3) {
    print("For break passed");
} else {
    print("For break failed: " + for_break);
}

// Test continue in for loop
var for_sum = 0
for (var m = 1; m <= 5; m = m + 1) {
    if (m == 3) {
        continue;
    }
    for_sum = for_sum + m
}
// for_sum = 1 + 2 + 4 + 5 = 12
if (for_sum == 12) {
    print("For continue passed");
} else {
    print("For continue failed: " + for_sum);
}

// Nested loops break
var outer = 0
var inner_runs = 0
while (outer < 3) {
    var inner = 0
    while (inner < 3) {
        if (inner == 1) {
            break; // Should break inner loop, not outer
        }
        inner_runs = inner_runs + 1
        inner = inner + 1
    }
    outer = outer + 1
}
// outer runs 3 times. inner runs 1 time each (0). So inner_runs = 3.
if (inner_runs == 3) {
    print("Test passed.");
} else {
    print("Nested break failed: " + inner_runs);
}
