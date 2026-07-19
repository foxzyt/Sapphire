#include "opencl_api.h"
#include "vm.h"
#include "object.h"
#include "value.h"

#ifdef HAS_OPENCL
#include <CL/cl.h>
#include <map>
#include <vector>
#include <iostream>

static cl_context context = nullptr;
static cl_command_queue queue = nullptr;
static cl_device_id device_id = nullptr;

static std::map<int, cl_mem> buffers;
static int next_buffer_id = 1;

static std::map<int, cl_kernel> kernels;
static int next_kernel_id = 1;

static SapphireValue ocl_init(int arg_count, SapphireValue* args) {
    if(context != nullptr) return SapphireValue(true);
    cl_platform_id platform_id = NULL;
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
    cl_int ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
    if(ret != CL_SUCCESS) return SapphireValue(false);
    
    ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, &ret_num_devices);
    if(ret != CL_SUCCESS) return SapphireValue(false);
    
    context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);
    if(ret != CL_SUCCESS) return SapphireValue(false);
    
    queue = clCreateCommandQueue(context, device_id, 0, &ret);
    if(ret != CL_SUCCESS) return SapphireValue(false);
    
    return SapphireValue(true);
}

static SapphireValue ocl_createBuffer(int arg_count, SapphireValue* args) {
    if(arg_count != 1 || args[0].type != ValType::VAL_NUMBER) return SapphireValue(-1.0);
    size_t size = (size_t)args[0].as.number * sizeof(double); // Assume double buffers for Sapphire
    
    cl_int ret;
    cl_mem mem_obj = clCreateBuffer(context, CL_MEM_READ_WRITE, size, NULL, &ret);
    if(ret != CL_SUCCESS) return SapphireValue(-1.0);
    
    int id = next_buffer_id++;
    buffers[id] = mem_obj;
    return SapphireValue((double)id);
}

static SapphireValue ocl_writeBuffer(int arg_count, SapphireValue* args) {
    if(arg_count != 2 || args[0].type != ValType::VAL_NUMBER) return SapphireValue(false);
    if(!is_obj_type(args[1], OBJ_ARRAY)) return SapphireValue(false);
    
    int id = (int)args[0].as.number;
    if(buffers.find(id) == buffers.end()) return SapphireValue(false);
    
    auto array = static_cast<ObjArray*>(args[1].as.obj);
    std::vector<double> host_data(array->elements.size());
    for(size_t i=0; i<array->elements.size(); ++i) {
        if(array->elements[i].type == ValType::VAL_NUMBER) {
            host_data[i] = array->elements[i].as.number;
        } else {
            host_data[i] = 0.0;
        }
    }
    
    cl_int ret = clEnqueueWriteBuffer(queue, buffers[id], CL_TRUE, 0, host_data.size() * sizeof(double), host_data.data(), 0, NULL, NULL);
    return SapphireValue(ret == CL_SUCCESS);
}

static SapphireValue ocl_readBuffer(int arg_count, SapphireValue* args) {
    if(arg_count != 2 || args[0].type != ValType::VAL_NUMBER) return SapphireValue(false);
    if(!is_obj_type(args[1], OBJ_ARRAY)) return SapphireValue(false);
    
    int id = (int)args[0].as.number;
    if(buffers.find(id) == buffers.end()) return SapphireValue(false);
    
    auto array = static_cast<ObjArray*>(args[1].as.obj);
    std::vector<double> host_data(array->elements.size());
    
    cl_int ret = clEnqueueReadBuffer(queue, buffers[id], CL_TRUE, 0, host_data.size() * sizeof(double), host_data.data(), 0, NULL, NULL);
    if(ret != CL_SUCCESS) return SapphireValue(false);
    
    for(size_t i=0; i<array->elements.size(); ++i) {
        array->elements[i] = SapphireValue(host_data[i]);
    }
    return SapphireValue(true);
}

static SapphireValue ocl_compile(int arg_count, SapphireValue* args) {
    if(arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING)) return SapphireValue(-1.0);
    
    ObjString* source_obj = static_cast<ObjString*>(args[0].as.obj);
    ObjString* name_obj = static_cast<ObjString*>(args[1].as.obj);
    
    const char* source_str = source_obj->chars.c_str();
    size_t source_size = source_obj->chars.size();
    
    cl_int ret;
    cl_program program = clCreateProgramWithSource(context, 1, (const char **)&source_str, (const size_t *)&source_size, &ret);
    if(ret != CL_SUCCESS) return SapphireValue(-1.0);
    
    ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
    if(ret != CL_SUCCESS) {
        return SapphireValue(-1.0);
    }
    
    cl_kernel kernel = clCreateKernel(program, name_obj->chars.c_str(), &ret);
    if(ret != CL_SUCCESS) return SapphireValue(-1.0);
    
    int id = next_kernel_id++;
    kernels[id] = kernel;
    return SapphireValue((double)id);
}

static SapphireValue ocl_execute(int arg_count, SapphireValue* args) {
    if(arg_count < 2 || args[0].type != ValType::VAL_NUMBER || args[1].type != ValType::VAL_NUMBER) return SapphireValue(false);
    
    int kernel_id = (int)args[0].as.number;
    if(kernels.find(kernel_id) == kernels.end()) return SapphireValue(false);
    cl_kernel kernel = kernels[kernel_id];
    
    size_t global_item_size = (size_t)args[1].as.number;
    
    for(int i=2; i<arg_count; ++i) {
        if(args[i].type == ValType::VAL_NUMBER) {
            int buf_id = (int)args[i].as.number;
            cl_mem mem = buffers[buf_id];
            clSetKernelArg(kernel, i-2, sizeof(cl_mem), (void *)&mem);
        }
    }
    
    cl_int ret = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_item_size, NULL, 0, NULL, NULL);
    return SapphireValue(ret == CL_SUCCESS);
}

void define_opencl_natives(VM* vm) {
    ObjString* opencl_name = new_string(vm, "OpenCL");
    ObjClass* opencl_class = new_class(vm, opencl_name);
    opencl_class->methods["init"] = SapphireValue(new_native(vm, ocl_init));
    opencl_class->methods["createBuffer"] = SapphireValue(new_native(vm, ocl_createBuffer));
    opencl_class->methods["writeBuffer"] = SapphireValue(new_native(vm, ocl_writeBuffer));
    opencl_class->methods["readBuffer"] = SapphireValue(new_native(vm, ocl_readBuffer));
    opencl_class->methods["compile"] = SapphireValue(new_native(vm, ocl_compile));
    opencl_class->methods["execute"] = SapphireValue(new_native(vm, ocl_execute));
    
    vm->globals["OpenCL"] = SapphireValue(opencl_class);
}

#else

void define_opencl_natives(VM* vm) {
    // OpenCL not enabled
}

#endif










