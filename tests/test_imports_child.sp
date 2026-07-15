import "test_imports.sp"; // Circular dependency! Should not cause a stack overflow now.
print("Import child loaded!");
