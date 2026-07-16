// Infinitum - NumPy-like library for Sapphire
// Version: 1.0.0
// Author: Bernardo Alvim

// Save reference to native rand function before shadowing it
var native_rand = rand;

// Vector operations
function zeros(n) {
    var result = [];
    var i = 0;
    while (i < n) {
        result[len(result)] = 0.0;
        i = i + 1;
    }
    return result;
}

function ones(n) {
    var result = [];
    var i = 0;
    while (i < n) {
        result[len(result)] = 1.0;
        i = i + 1;
    }
    return result;
}

function arange(start, stop, step) {
    var result = [];
    var current = start;
    while (current < stop) {
        result[len(result)] = current;
        current = current + step;
    }
    return result;
}

function linspace(start, stop, num) {
    var result = [];
    var step = (stop - start) / (num - 1);
    var i = 0;
    while (i < num) {
        result[i] = start + (i * step);
        i = i + 1;
    }
    return result;
}

// Vector math operations
function add(v1, v2) {
    var result = [];
    var i = 0;
    var n = len(v1);
    while (i < n) {
        result[i] = v1[i] + v2[i];
        i = i + 1;
    }
    return result;
}

function sub(v1, v2) {
    var result = [];
    var i = 0;
    var n = len(v1);
    while (i < n) {
        result[i] = v1[i] - v2[i];
        i = i + 1;
    }
    return result;
}

function mul(v1, v2) {
    var result = [];
    var i = 0;
    var n = len(v1);
    while (i < n) {
        result[i] = v1[i] * v2[i];
        i = i + 1;
    }
    return result;
}

function div(v1, v2) {
    var result = [];
    var i = 0;
    var n = len(v1);
    while (i < n) {
        result[i] = v1[i] / v2[i];
        i = i + 1;
    }
    return result;
}

function scale(v, scalar) {
    var result = [];
    var i = 0;
    var n = len(v);
    while (i < n) {
        result[i] = v[i] * scalar;
        i = i + 1;
    }
    return result;
}

// Reduction operations
function sum(v) {
    var result = 0.0;
    var i = 0;
    var n = len(v);
    while (i < n) {
        result = result + v[i];
        i = i + 1;
    }
    return result;
}

function mean(v) {
    return sum(v) / len(v);
}

function max(v) {
    var result = v[0];
    var i = 1;
    var n = len(v);
    while (i < n) {
        if (v[i] > result) {
            result = v[i];
        }
        i = i + 1;
    }
    return result;
}

function min(v) {
    var result = v[0];
    var i = 1;
    var n = len(v);
    while (i < n) {
        if (v[i] < result) {
            result = v[i];
        }
        i = i + 1;
    }
    return result;
}

// Matrix operations (2D arrays)
function zeros_matrix(rows, cols) {
    var result = [];
    var i = 0;
    while (i < rows) {
        result[i] = zeros(cols);
        i = i + 1;
    }
    return result;
}

function ones_matrix(rows, cols) {
    var result = [];
    var i = 0;
    while (i < rows) {
        result[i] = ones(cols);
        i = i + 1;
    }
    return result;
}

function identity(n) {
    var result = zeros_matrix(n, n);
    var i = 0;
    while (i < n) {
        result[i][i] = 1.0;
        i = i + 1;
    }
    return result;
}

function dot(v1, v2) {
    var result = 0.0;
    var i = 0;
    var n = len(v1);
    while (i < n) {
        result = result + (v1[i] * v2[i]);
        i = i + 1;
    }
    return result;
}

// Statistical functions
function std(v) {
    var m = mean(v);
    var variance = 0.0;
    var i = 0;
    var n = len(v);
    while (i < n) {
        variance = variance + ((v[i] - m) * (v[i] - m));
        i = i + 1;
    }
    return sqrt(variance / n);
}

function variance(v) {
    var m = mean(v);
    var variance = 0.0;
    var i = 0;
    var n = len(v);
    while (i < n) {
        variance = variance + ((v[i] - m) * (v[i] - m));
        i = i + 1;
    }
    return variance / n;
}

// Utility functions
function abs(v) {
    var result = [];
    var i = 0;
    var n = len(v);
    while (i < n) {
        if (v[i] < 0) {
            result[i] = -v[i];
        } else {
            result[i] = v[i];
        }
        i = i + 1;
    }
    return result;
}

function pow(v, exponent) {
    var result = [];
    var i = 0;
    var n = len(v);
    while (i < n) {
        result[i] = v[i] ^ exponent;
        i = i + 1;
    }
    return result;
}

function sqrt_list(v) {
    var result = [];
    var i = 0;
    var n = len(v);
    while (i < n) {
        result[i] = sqrt(v[i]);
        i = i + 1;
    }
    return result;
}

// Sorting
function sort(v) {
    var result = v;
    var n = len(result);
    var i = 0;
    while (i < n) {
        var j = i + 1;
        while (j < n) {
            if (result[i] > result[j]) {
                var temp = result[i];
                result[i] = result[j];
                result[j] = temp;
            }
            j = j + 1;
        }
        i = i + 1;
    }
    return result;
}

function reverse(v) {
    var result = [];
    var i = len(v) - 1;
    while (i >= 0) {
        result[len(v) - 1 - i] = v[i];
        i = i - 1;
    }
    return result;
}

// Print vector for debugging
function print_vector(v) {
    print("[");
    var i = 0;
    var n = len(v);
    while (i < n) {
        print(v[i]);
        if (i < n - 1) {
            print(", ");
        }
        i = i + 1;
    }
    print("]");
}

// Shape manipulation
function reshape(v, new_rows, new_cols) {
    var result = zeros_matrix(new_rows, new_cols);
    var idx = 0;
    var i = 0;
    while (i < new_rows) {
        var j = 0;
        while (j < new_cols) {
            result[i][j] = v[idx];
            idx = idx + 1;
            j = j + 1;
        }
        i = i + 1;
    }
    return result;
}

function transpose(matrix) {
    var rows = len(matrix);
    var cols = len(matrix[0]);
    var result = zeros_matrix(cols, rows);
    var i = 0;
    while (i < rows) {
        var j = 0;
        while (j < cols) {
            result[j][i] = matrix[i][j];
            j = j + 1;
        }
        i = i + 1;
    }
    return result;
}

function flatten(matrix) {
    var result = [];
    var i = 0;
    var rows = len(matrix);
    while (i < rows) {
        var j = 0;
        var cols = len(matrix[i]);
        while (j < cols) {
            result[len(result)] = matrix[i][j];
            j = j + 1;
        }
        i = i + 1;
    }
    return result;
}

// Slicing
function slice(v, start, end) {
    var result = [];
    var i = start;
    while (i < end) {
        result[len(result)] = v[i];
        i = i + 1;
    }
    return result;
}

// Boolean indexing
function filter(v, threshold) {
    var result = [];
    var i = 0;
    var n = len(v);
    while (i < n) {
        if (v[i] > threshold) {
            result[len(result)] = v[i];
        }
        i = i + 1;
    }
    return result;
}

// Advanced linear algebra
function matmul(m1, m2) {
    var rows1 = len(m1);
    var cols1 = len(m1[0]);
    var cols2 = len(m2[0]);
    var result = zeros_matrix(rows1, cols2);
    var i = 0;
    while (i < rows1) {
        var j = 0;
        while (j < cols2) {
            var sum = 0.0;
            var k = 0;
            while (k < cols1) {
                sum = sum + (m1[i][k] * m2[k][j]);
                k = k + 1;
            }
            result[i][j] = sum;
            j = j + 1;
        }
        i = i + 1;
    }
    return result;
}

function determinant(matrix) {
    var n = len(matrix);
    if (n == 1) {
        return matrix[0][0];
    }
    if (n == 2) {
        return (matrix[0][0] * matrix[1][1]) - (matrix[0][1] * matrix[1][0]);
    }
    var det = 0.0;
    var j = 0;
    while (j < n) {
        var sign = (-1) ^ j;
        var minor = zeros_matrix(n - 1, n - 1);
        var row = 0;
        var col = 0;
        var i = 1;
        while (i < n) {
            var k = 0;
            while (k < n) {
                if (k != j) {
                    minor[row][col] = matrix[i][k];
                    col = col + 1;
                }
                k = k + 1;
            }
            row = row + 1;
            col = 0;
            i = i + 1;
        }
        det = det + (sign * matrix[0][j] * determinant(minor));
        j = j + 1;
    }
    return det;
}

function inverse(matrix) {
    var n = len(matrix);
    var det = determinant(matrix);
    if (det == 0) {
        return nil;
    }
    var result = zeros_matrix(n, n);
    var i = 0;
    while (i < n) {
        var j = 0;
        while (j < n) {
            var minor = zeros_matrix(n - 1, n - 1);
            var row = 0;
            var col = 0;
            var mi = 0;
            while (mi < n) {
                var mj = 0;
                while (mj < n) {
                    if (mi != i && mj != j) {
                        minor[row][col] = matrix[mi][mj];
                        col = col + 1;
                        if (col == n - 1) {
                            col = 0;
                            row = row + 1;
                        }
                    }
                    mj = mj + 1;
                }
                mi = mi + 1;
            }
            var sign = (-1) ^ (i + j);
            result[j][i] = (sign * determinant(minor)) / det;
            j = j + 1;
        }
        i = i + 1;
    }
    return result;
}

// Broadcasting - scalar operations
function add_scalar(v, scalar) {
    var result = [];
    var i = 0;
    var n = len(v);
    while (i < n) {
        result[len(result)] = v[i] + scalar;
        i = i + 1;
    }
    return result;
}

function sub_scalar(v, scalar) {
    var result = [];
    var i = 0;
    var n = len(v);
    while (i < n) {
        result[len(result)] = v[i] - scalar;
        i = i + 1;
    }
    return result;
}

function mul_scalar(v, scalar) {
    var result = [];
    var i = 0;
    var n = len(v);
    while (i < n) {
        result[len(result)] = v[i] * scalar;
        i = i + 1;
    }
    return result;
}

function div_scalar(v, scalar) {
    var result = [];
    var i = 0;
    var n = len(v);
    while (i < n) {
        result[len(result)] = v[i] / scalar;
        i = i + 1;
    }
    return result;
}

// Random number generation
function rand(n) {
    var result = [];
    var i = 0;
    while (i < n) {
        result[len(result)] = native_rand();
        i = i + 1;
    }
    return result;
}

function randn(n) {
    var result = [];
    var i = 0;
    while (i < n) {
        // Box-Muller transform for normal distribution
        var u1 = native_rand();
        var u2 = native_rand();
        var z0 = sqrt(-2 * ln(u1)) * cos(2 * 3.14159265359 * u2);
        result[len(result)] = z0;
        i = i + 1;
    }
    return result;
}

function randint(min, max, n) {
    var result = [];
    var i = 0;
    while (i < n) {
        result[len(result)] = floor(native_rand() * (max - min + 1)) + min;
        i = i + 1;
    }
    return result;
}

function choice(v, n) {
    var result = [];
    var i = 0;
    while (i < n) {
        var idx = floor(native_rand() * len(v));
        result[len(result)] = v[idx];
        i = i + 1;
    }
    return result;
}