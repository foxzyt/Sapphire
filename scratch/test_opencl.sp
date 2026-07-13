// test_opencl.sp
print("Testing OpenCL API...");

var init = OpenCL.init();
print("OpenCL Init: " + valueToString(init));

if (init == true) {
    var buf1 = OpenCL.createBuffer(100.0);
    var buf2 = OpenCL.createBuffer(100.0);
    print("Buffer 1 Handle: " + valueToString(buf1));
    print("Buffer 2 Handle: " + valueToString(buf2));

    var array = listCreate();
    var i = 0.0;
    while (i < 100.0) {
        listAppend(array, i);
        i = i + 1.0;
    }
    
    var wrote = OpenCL.writeBuffer(buf1, array);
    print("Buffer Write Success: " + valueToString(wrote));

    var source = "
    __kernel void vector_add(__global const double *A, __global double *C) {
        int i = get_global_id(0);
        C[i] = A[i] + 1.0;
    }
    ";
    
    var kernel = OpenCL.compile(source, "vector_add");
    print("Kernel Handle: " + valueToString(kernel));
    
    if (kernel != -1.0) {
        var exec = OpenCL.execute(kernel, 100.0, buf1, buf2);
        print("Kernel Executed: " + valueToString(exec));
        
        var out_array = listCreate();
        var j = 0.0;
        while (j < 100.0) {
            listAppend(out_array, 0.0);
            j = j + 1.0;
        }
        
        var read = OpenCL.readBuffer(buf2, out_array);
        print("Buffer Read: " + valueToString(read));
        print("Output Array First Element: " + valueToString(listGet(out_array, 0.0)));
        print("Output Array Second Element: " + valueToString(listGet(out_array, 1.0)));
    }
}
print("Done OpenCL Test.");
