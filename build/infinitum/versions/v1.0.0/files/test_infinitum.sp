// Test file for Infinitum plugin
import "main.sp"

function main() {
    print("=== Testing Infinitum v1.0.0 ===\n");
    
    // Test vector creation
    print("Testing zeros(5):");
    var z = zeros(5);
    print_vector(z);
    print("\n");
    
    print("Testing ones(3):");
    var o = ones(3);
    print_vector(o);
    print("\n");
    
    print("Testing arange(0, 10, 2):");
    var a = arange(0.0, 10.0, 2.0);
    print_vector(a);
    print("\n");
    
    print("Testing linspace(0, 1, 5):");
    var l = linspace(0.0, 1.0, 5);
    print_vector(l);
    print("\n");
    
    // Test vector operations
    var v1 = [1.0, 2.0, 3.0];
    var v2 = [4.0, 5.0, 6.0];
    
    print("Testing add([1,2,3], [4,5,6]):");
    var s = add(v1, v2);
    print_vector(s);
    print("\n");
    
    print("Testing sub([1,2,3], [4,5,6]):");
    var d = sub(v1, v2);
    print_vector(d);
    print("\n");
    
    print("Testing mul([1,2,3], [4,5,6]):");
    var m = mul(v1, v2);
    print_vector(m);
    print("\n");
    
    print("Testing scale([1,2,3], 2.0):");
    var sc = scale(v1, 2.0);
    print_vector(sc);
    print("\n");
    
    // Test reduction operations
    print("Testing sum([1,2,3,4,5]):");
    var test_vec = [1.0, 2.0, 3.0, 4.0, 5.0];
    print(sum(test_vec));
    print("\n");
    
    print("Testing mean([1,2,3,4,5]):");
    print(mean(test_vec));
    print("\n");
    
    print("Testing max([1,2,3,4,5]):");
    print(max(test_vec));
    print("\n");
    
    print("Testing min([1,2,3,4,5]):");
    print(min(test_vec));
    print("\n");
    
    // Test statistical functions
    print("Testing std([1,2,3,4,5]):");
    print(std(test_vec));
    print("\n");
    
    print("Testing variance([1,2,3,4,5]):");
    print(variance(test_vec));
    print("\n");
    
    // Test matrix operations
    print("Testing identity(3):");
    var I = identity(3);
    var i = 0;
    while (i < 3) {
        print_vector(I[i]);
        print("\n");
        i = i + 1;
    }
    
    print("Testing dot([1,2,3], [4,5,6]):");
    print(dot(v1, v2));
    print("\n");
    
    // Test utility functions
    print("Testing abs([-1, -2, 3, -4, 5]):");
    var neg = [-1.0, -2.0, 3.0, -4.0, 5.0];
    print_vector(abs(neg));
    print("\n");
    
    print("Testing sort([5, 2, 8, 1, 9]):");
    var unsorted = [5.0, 2.0, 8.0, 1.0, 9.0];
    print_vector(sort(unsorted));
    print("\n");
    
    print("Testing reverse([1, 2, 3, 4, 5]):");
    print_vector(reverse(test_vec));
    print("\n");
    
    // Test shape manipulation
    print("Testing reshape([1,2,3,4,5,6], 2, 3):");
    var flat = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0];
    var reshaped = reshape(flat, 2, 3);
    var ri = 0;
    while (ri < 2) {
        print_vector(reshaped[ri]);
        print("\n");
        ri = ri + 1;
    }
    
    print("Testing transpose([[1,2],[3,4]]):");
    var mat = [[1.0, 2.0], [3.0, 4.0]];
    var transposed = transpose(mat);
    var ti = 0;
    while (ti < 2) {
        print_vector(transposed[ti]);
        print("\n");
        ti = ti + 1;
    }
    
    print("Testing flatten([[1,2],[3,4]]):");
    print_vector(flatten(mat));
    print("\n");
    
    // Test slicing
    print("Testing slice([1,2,3,4,5], 1, 4):");
    print_vector(slice(test_vec, 1, 4));
    print("\n");
    
    print("Testing filter([1,2,3,4,5], 3):");
    print_vector(filter(test_vec, 3.0));
    print("\n");
    
    // Test broadcasting
    print("Testing add_scalar([1,2,3], 5):");
    print_vector(add_scalar(v1, 5.0));
    print("\n");
    
    print("Testing mul_scalar([1,2,3], 2):");
    print_vector(mul_scalar(v1, 2.0));
    print("\n");
    
    // Test random
    print("Testing rand(5):");
    print_vector(rand(5));
    print("\n");
    
    print("Testing randint(1, 10, 3):");
    print_vector(randint(1, 10, 3));
    print("\n");
    
    print("Testing choice([1,2,3,4,5], 3):");
    print_vector(choice([1.0, 2.0, 3.0, 4.0, 5.0], 3));
    print("\n");
    
    print("=== All tests completed ===");
}

main();
