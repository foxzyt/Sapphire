// test_errors.sp
// A script containing several bad practices to trigger the Citrine Linter.

// 1. Security (Hardcoded secrets and bad functions)
// citrine-disable-next-line SEC01
let my_secret = "AKIA1234567890ABCDEF";
let aws_secret_key = "1234567890123456789012345678901234567890";
eval("print('Bad practice')");

// 2. Performance (Inefficient loops)
for (var i = 0; i < 10; i++) {
    print("i++ is slower than ++i");
    let name = stringOne + stringTwo;
}

// 3. Style (Bad naming conventions, missing spaces, trailing whitespace)
class badClassName {
    function BadFunctionName() {
        var BadVariableName = true;
        if (BadVariableName == true) {
            print("Missing space and redundant boolean check");
        }
    }
}

// 4. Syntax & Architecture
while (true) {
    // Infinite loop warning
}