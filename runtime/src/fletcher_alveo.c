// Copyright 2018 Delft University of Technology
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <memory.h>
#include <malloc.h>

#include <CL/opencl.h>
#include <CL/cl_ext.h>
#include <CL/cl_ext_xilinx.h>

#include "fletcher/fletcher.h"
#include "fletcher_alveo.h"

//da_t buffer_ptr = 0x0;


int load_file_to_memory(const char *filename, char **result)
{
    uint size = 0;
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        *result = NULL;
        return -1; // -1 means file opening fail
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *result = (char *)malloc(size+1);
    if (size != fread(*result, sizeof(char), size, f)) {
        free(*result);
        return -2; // -2 means file reading fail
    }
    fclose(f);
    (*result)[size] = 0;
    return size;
}

fstatus_t platformGetName(char *name, size_t size) {
  size_t len = strlen(FLETCHER_PLATFORM_NAME);
  if (len > size) {
    memcpy(name, FLETCHER_PLATFORM_NAME, size - 1);
    name[size - 1] = '\0';
  } else {
    memcpy(name, FLETCHER_PLATFORM_NAME, len + 1);
  }
  return FLETCHER_STATUS_OK;
}




// argv[0] -> .xclbin file.
// argv[1] -> Target device name.
//argv[2] -> Kernel name.

fstatus_t platformInit(void *argv[]) {  
  debug_print("[FLETCHER_ALVEO] Initializing platform.       Arguments @ [host] %016lX.\n", (unsigned long) arg);
  // Check psl_server.dat is present

    char *target_device_name_pass;
    
    if(argv[1] != NULL){
        target_device_name_pass = (char *) argv[1];
    }
    
    else{
        target_device_name_pass = "_u250_xdma_201820_1";  // Look at this and correct it.
    }
    
    alveo_state.target_device_name = target_device_name_pass;
    
  // Get all platforms and then select Xilinx platform
    cl_platform_id platforms[16];       // platform id
    cl_uint platform_count;
    int platform_found = 0;
    alveo_state.err = clGetPlatformIDs(16, platforms, &platform_count);
    if (alveo_state.err != CL_SUCCESS) {
       printf("Error: Failed to find an OpenCL platform!\n");
       printf("Test failed\n");
       return EXIT_FAILURE;
    }
   printf("INFO: Found %d platforms\n", platform_count);

   // Find Xilinx Plaftorm
   for (unsigned int iplat=0; iplat<platform_count; iplat++) {
       alveo_state.err = clGetPlatformInfo(platforms[iplat], CL_PLATFORM_VENDOR, 1000, (void *) alveo_state.cl_platform_vendor,NULL);
       if (alveo_state.err != CL_SUCCESS) {
       printf(  "%s", platforms[iplat]);	
           printf("Error: clGetPlatformInfo(CL_PLATFORM_VENDOR) failed!\n");
           printf("Test failed\n");
           return EXIT_FAILURE;
       }
       if (strcmp(alveo_state.cl_platform_vendor, "Xilinx") == 0) {
           printf("INFO: Selected platform %d from %s\n", iplat, alveo_state.cl_platform_vendor);
           alveo_state.platform_id = platforms[iplat];
           platform_found = 1;
       }
   }
   if (!platform_found) {
       printf("ERROR: Platform Xilinx not found. Exit.\n");
       return EXIT_FAILURE;
   }

  // Get Accelerator compute device
   cl_uint num_devices;
   unsigned int device_found = 0;
   cl_device_id devices[16];  // compute device id
   char cl_device_name[1001];
   alveo_state.err = clGetDeviceIDs(alveo_state.platform_id, CL_DEVICE_TYPE_ACCELERATOR, 16, devices, &num_devices);
   printf("INFO: Found %d devices\n", num_devices);
   if (alveo_state.err != CL_SUCCESS) {
       printf("ERROR: Failed to create a device group!\n");
       printf("ERROR: Test failed\n");
       return -1;
   }

   //iterate all devices to select the target device.
   for (uint i=0; i<num_devices; i++) {
      alveo_state.err = clGetDeviceInfo(devices[i], CL_DEVICE_NAME, 1024, cl_device_name, 0);
      if (alveo_state.err != CL_SUCCESS) {
           printf("Error: Failed to get device name for device %d!\n", i);
           printf("Test failed\n");
           return EXIT_FAILURE;
       }

      printf("CL_DEVICE_NAME %s\n", cl_device_name);
      if(strstr(alveo_state.target_device_name, cl_device_name) != NULL) {
           alveo_state.device_id = devices[i];
           device_found = 1;
           printf("Selected %s as the target device\n", cl_device_name);
      
      }
   }
  if (!device_found) {
       printf("Target device %s not found. Exit.\n", alveo_state.target_device_name);
       return EXIT_FAILURE;
   }
   

   // Create a compute context
    alveo_state.context = clCreateContext(0, 1, &(alveo_state.device_id), NULL, NULL, &(alveo_state.err));
    if (!alveo_state.context) {
        printf("Error: Failed to create a compute context!\n");
        printf("Test failed\n");
        return EXIT_FAILURE;
    }

    // Create a command commands
    alveo_state.commands = clCreateCommandQueue(alveo_state.context, &(alveo_state.device_id), 0, &(alveo_state.err));
    if (!alveo_state.commands) {
        printf("Error: Failed to create a command commands!\n");
        printf("Error: code %i\n",alveo_state.err);
        printf("Test failed\n");
        return EXIT_FAILURE;
    }


    //Indicates whether the program binary for the device 
    //was loaded successfully or not.
    int status;  
    
    
    // Create Program Objects
    // Load binary from disk
    unsigned char *kernelbinary;
    alveo_state.xclbin = argv[0];
        
    printf("INFO: loading xclbin %s\n", alveo_state.xclbin);
    int n_i0 = load_file_to_memory(alveo_state.xclbin, (char **) &kernelbinary); //load xclbin to kernelbinary
    if (n_i0 < 0) {
     printf("failed to load kernel from xclbin: %s\n", xclbin);
     printf("Test failed\n");
     return EXIT_FAILURE;
    }

   
   

    size_t n0 = n_i0;  //size of the file loaded into memory.

    // Create the compute program from offline.
    
    //When the host application runs, it must load the FPGA binary(.xclbin) 
    //file using the following API.
    
    //Creates a program object for a context, and loads 
    //specified binary data into the program object.
    alveo_state.program = clCreateProgramWithBinary(context, 1, &alveo_state.device_id, &n0,
                                     (const unsigned char **) &kernelbinary, &status, &(alveo_state.err));

    if ((!program) || ((alveo_state.err)!=CL_SUCCESS)) {
     printf("Error: Failed to create compute program from binary %d!\n", alveo_state.err);
     printf("Test failed\n");
     return EXIT_FAILURE;
    }


   
   
    // Build the program executable.
    
    //Builds (compiles and links) a program executable(alveo_state.program) 
    //from the program source or binary.
    alveo_state.err = clBuildProgram(alveo_state.program, 0, NULL, NULL, NULL, NULL);
    if (alveo_state.err != CL_SUCCESS) {
      size_t len;
      char buffer[2048];

      printf("Error: Failed to build program executable!\n");
      clGetProgramBuildInfo(alveo_state.program, alveo_state.device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
      printf("%s\n", buffer);
      printf("Test failed\n");
      return EXIT_FAILURE;
    }



    //Once the OpenCL environment is initialized, the host application is ready to issue 
    //commands to the device and interact with the kernels.
    //These commands include:
    //      1.Setting up the kernels.
    //      2.Buffer transfer to/from FPGA.
    //      3.Kernel execution on FPGA.
    //      4.Event synchronization.




    //After setting up the OpenCL environment, such as identifying devices, creating the context,
    // command queue, and program, the host application should identify the kernels that will 
    //execute on the device, and set up the kernel arguments.
    
    
    // Create the compute kernel in the program we wish to run.
    
    const char *kernel_name = argv[2];
    
    alveo_state.kernel = clCreateKernel(alveo_state.program, "Vadd_A_B", &(alveo_state.err));
    if (!(alveo_state.kernel) || (alveo_state.err) != CL_SUCCESS) {
       printf("Error: Failed to create compute kernel!\n");
       printf("Test failed\n");
       return EXIT_FAILURE;
    }


  return FLETCHER_STATUS_OK;
}


fstatus_t platformWriteMMIO(uint64_t offset, uint32_t value) { 
  cl_mem_ext_ptr_t  ext; //Extension pointer.
  ext.param = alveo_state.kernel;
  ext.obj = nullptr;
  
  
  debug_print("[FLETCHER_SNAP] Writing MMIO register.       %04lu <= 0x%08X\n", offset, value);
  return FLETCHER_STATUS_OK;
}

fstatus_t platformReadMMIO(uint64_t offset, uint32_t *value) {
  *value = 0xDEADBEEF;
  // Sleep a few seconds in simulation mode to prevent status register polling spam
  if (snap_state.sim && offset == FLETCHER_REG_STATUS) {
    sleep(2);
  }
  snap_action_read32(snap_state.card_handle, FLETCHER_SNAP_ACTION_REG_OFFSET + 4*offset, value);
  debug_print("[FLETCHER_SNAP] Reading MMIO register.       %04lu => 0x%08X\n", offset, *value);
  return FLETCHER_STATUS_OK;
}

fstatus_t platformCopyHostToDevice(const uint8_t *host_source, da_t device_destination, int64_t size) {
  debug_print(
      "[FLETCHER_SNAP] Copying from host to device. [host] 0x%016lX --> [dev] 0x%016lX (%lu bytes) (NOT IMPLEMENTED)\n",
      (uint64_t) host_source,
      device_destination,
      size);
  return FLETCHER_STATUS_OK;
}

fstatus_t platformCopyDeviceToHost(da_t device_source, uint8_t *host_destination, int64_t size) {
  debug_print(
      "[FLETCHER_SNAP] Copying from device to host. [dev] 0x%016lX --> [host] 0x%016lX (%lu bytes) (NOT IMPLEMENTED)\n",
      device_source,
      (uint64_t) host_destination,
      size);
  return FLETCHER_STATUS_OK;
}

fstatus_t platformTerminate(void *arg) {
  debug_print("[FLETCHER SNAP] Terminating platform.        Arguments @ [host] 0x%016lX.\n", (uint64_t) arg);
  snap_detach_action(snap_state.action_handle);
  snap_card_free(snap_state.card_handle);
  return FLETCHER_STATUS_OK;
}

fstatus_t platformDeviceMalloc(da_t *device_address, int64_t size) {
  da_t ptr;
  posix_memalign((void**)(&ptr), FLETCHER_SNAP_DEVICE_ALIGNMENT, size);
  debug_print("[FLETCHER_SNAP] Allocating device memory.    [device] 0x%016lX (%10lu bytes).\n",
               ptr,
               size);
  *device_address = ptr;
  return FLETCHER_STATUS_OK;
}

fstatus_t platformDeviceFree(da_t device_address) {
  debug_print("[FLETCHER_SNAP] Freeing device memory.       [device] 0x%016lX. (NOT IMPLEMENTED)\n", device_address);
  return FLETCHER_STATUS_OK;
}

fstatus_t platformPrepareHostBuffer(const uint8_t *host_source, da_t *device_destination, int64_t size, int *alloced) {
  *device_destination = (da_t) host_source;
  *alloced = 0;
  debug_print("[FLETCHER_SNAP] Preparing buffer for device. [host] 0x%016lX --> 0x%016lX (%10lu bytes).\n",
              (unsigned long) host_source,
              (unsigned long) *device_destination,
              size);
  buffer_ptr += size;
  return FLETCHER_STATUS_OK;
}

fstatus_t platformCacheHostBuffer(const uint8_t *host_source, da_t *device_destination, int64_t size) {
  *device_destination = buffer_ptr;
  debug_print(
      "[FLETCHER_SNAP] Caching buffer on device.    [host] 0x%016lX --> 0x%016lX (%10lu bytes). (NOT IMPLEMENTED)\n",
      (unsigned long) host_source,
      (unsigned long) *device_destination,
      size);
  buffer_ptr += size;
  return FLETCHER_STATUS_OK;
}
© 2020 GitHub, Inc.
Terms
Privacy
Security
Status
Help
Contact GitHub
Pricing
API
Training
Blog
About
