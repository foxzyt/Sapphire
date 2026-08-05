import "Infinitum/versions/v2.0.0/files/vector.sp";
import "Infinitum/versions/v2.0.0/files/tensor.sp";
import "Infinitum/versions/v2.0.0/files/timeseries.sp";
import "Infinitum/versions/v2.0.0/files/combinatorics.sp";
import "Infinitum/versions/v2.0.0/files/chaos.sp";
import "Infinitum/versions/v2.0.0/files/calculus.sp";

print("========================================");
print("Infinitum 2.0.0 - Benchmark ABSOLUTO");
print("========================================");

var total_start = clock();
var t0 = 0.0;
var t1 = 0.0;

// 1. Vector Operations
t0 = clock();
var n = 100000;
var v1 = ones(n);
var v2 = ones(n);
var v3 = add(v1, v2);
var dp = dot(v1, v2);
t1 = clock();
print("[1] Vector Math (100k): " + (t1 - t0) + " ms");

// 2. Tensor Operations
t0 = clock();
var shape = [30, 30, 30]; // 27,000 elements
var tensor1 = tensor_ones(shape);
var tensor2 = tensor_ones(shape);
var tensor_sum = tensor_add(tensor1, tensor2);
t1 = clock();
print("[2] Tensor 3D Math (30x30x30): " + (t1 - t0) + " ms");

// 3. Combinatorics (Heavy Recursion / Looping)
t0 = clock();
var fib = fibonacci(30); 
var cat = catalan(15);
var pasc = pascal_triangle(20);
t1 = clock();
print("[3] Combinatorics (Fib 30, Catalan 15, Pascal 20): " + (t1 - t0) + " ms");

// 4. Chaos Theory & Fractals
t0 = clock();
var lorenz_traj = lorenz(1.0, 1.0, 1.0, 10.0, 28.0, 2.66, 0.01, 10000);
var logistic_traj = logistic_map(3.8, 0.5, 50000);
t1 = clock();
print("[4] Chaos (Lorenz 10k steps, Logistic 50k steps): " + (t1 - t0) + " ms");

// 5. Calculus (Numerical Integration & Differentiation)
t0 = clock();
var y_array = ones(50000);
var integral_trapz = trapz(y_array, 0.01);
var integral_simp = simpson(y_array, 0.01);
var grad = gradient(y_array, 0.01);
t1 = clock();
print("[5] Calculus (Integrate & Differentiate 50k pts): " + (t1 - t0) + " ms");

// 6. Time Series
t0 = clock();
var ts_m = ts_mean(logistic_traj);
var ts_v = ts_var(logistic_traj);
var ts_bb = bollinger_bands(logistic_traj, 20, 2.0);
t1 = clock();
print("[6] Time Series (Mean, Var, Bollinger on 50k): " + (t1 - t0) + " ms");

var total_end = clock();
print("========================================");
print("TEMPO TOTAL DE EXECUCAO: " + (total_end - total_start) + " ms");
print("========================================");
