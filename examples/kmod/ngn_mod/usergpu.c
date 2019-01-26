#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <CL/cl.h>
#include "gpu_ioctl.h"
 


#define MAX_PLATFORMS (10)
#define MAX_DEVICES (10)
#define MAX_SOURCE_SIZE (100000)
#define PAGE_SIZE 4096



static int setup_ocl(cl_uint, cl_uint, char*);

cl_command_queue Queue;
cl_kernel k_vadd;
cl_context     context = NULL;
size_t memsize;
int result;
int *test;
int lnum;

int main(){
  int ret, fd;
  int memfd;
  int platform = 0;
  int device = 0;
  char msg[BUFSIZ];
  void *temp;
  void* shared_addr;
  int packet_num = PACKET_NUM;
  
  struct gpu_ioctl_args args = {0}; 
  memsize = sizeof(int) * PACKET_NUM;

  fd = open("/dev/comgpu", O_RDWR);             // Open the device with read/write access
  if (fd < 0){
    perror("Failed to open the device...");
    return errno;
  }
  //posix_memalign(&temp, PAGE_SIZE, 3 * PAGE_SIZE);
  //printf("temp %x", temp);

  ret = setup_ocl((cl_uint)platform, (cl_uint)device, msg);
  if (ret > 0)
    printf("ret= %d , %s", ret, msg);
  
  
#ifdef COARSE_GRAIN_ON
  args.matrix1 = clSVMAlloc(context, CL_MEM_READ_WRITE, memsize, 0);
  args.matrix2 = clSVMAlloc(context, CL_MEM_READ_WRITE, memsize, 0);
  args.sum= clSVMAlloc(context, CL_MEM_READ_WRITE, memsize, 0);
  printf("adder matrix1 %x \n", args.matrix1);
  shared_addr = mmap(args.matrix1, 3 * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
  printf("shared_addr %x  value %x\n", shared_addr, *(int*)shared_addr);

#elif FINE_GRAIN_ON
  args.matrix1 = clSVMAlloc(context, CL_MEM_READ_WRITE|CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS, memsize, 0);
  args.matrix2 = clSVMAlloc(context, CL_MEM_READ_WRITE|CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS, memsize, 0);
  args.sum = clSVMAlloc(context, CL_MEM_READ_WRITE|CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS, memsize, 0);
#elif NON_SVM
  args.matrix1 = (int *)malloc(memsize);
  args.matrix2 = (int *)malloc(memsize);
  args.sum = (int *) malloc(memsize);
#endif

#ifndef NON_SVM
  clSetKernelArgSVMPointer(k_vadd, 0, args.matrix1);
  clSetKernelArgSVMPointer(k_vadd, 1, args.matrix2);
  clSetKernelArgSVMPointer(k_vadd, 2, args.sum);
  clSetKernelArg(k_vadd, 3, sizeof(packet_num), &packet_num);
#endif

#ifdef COARSE_GRAIN_ON
  clEnqueueSVMMap(Queue, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, args.matrix1, memsize, 0, NULL, NULL);
  clEnqueueSVMMap(Queue, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, args.matrix2, memsize, 0, NULL, NULL);
  memset(args.matrix2, 0x1, memsize);
  memset(args.sum, 0x0, memsize);
#endif

  //printf("ret %d, mat %d\n ", retr, args.matrix1[0]);


#ifdef COARSE_GRAIN_ON
  clEnqueueSVMUnmap(Queue, args.matrix1, 0, NULL, NULL);
  clEnqueueSVMUnmap(Queue, args.matrix2, 0, NULL, NULL);
#endif

  size_t local_item_size = 256;
  size_t global_item_size = ((10+ local_item_size - 1) / local_item_size) * local_item_size;
  clEnqueueNDRangeKernel(Queue, k_vadd, 1, NULL, &global_item_size, &local_item_size, 0, NULL, NULL);
  clFinish(Queue);
  printf("shared_addr %x  value %x\n", shared_addr, *(int*)shared_addr);
  ioctl(fd, 1000, NULL);


#ifdef COARSE_GRAIN_ON
  clEnqueueSVMMap(Queue, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, args.sum, memsize, 0, NULL, NULL);
#endif
  printf("sum = %d, %d",args.sum[0], args.sum[1]);


#ifdef COARSE_GRAIN_ON
  clEnqueueSVMUnmap(Queue, args.matrix1, 0, NULL, NULL);
#endif



  //printf("shared_addr %d pid %d\n", *shared_addr, getpid());

  clReleaseKernel(k_vadd);
  clReleaseCommandQueue(Queue);
  clReleaseContext(context);



  printf("End of the program\n");
  return 0;
}

static int setup_ocl(cl_uint platform, cl_uint device, char* msg)
{
  cl_program     program = NULL;
  cl_platform_id platform_id[MAX_PLATFORMS];
  cl_device_id   device_id[MAX_DEVICES];

  FILE *fp;
  char *source_str;
  char str[BUFSIZ];
  size_t source_size, ret_size, size;
  cl_uint num_platforms, num_devices;
  cl_int ret;

  // alloc
  source_str = (char *)malloc(MAX_SOURCE_SIZE * sizeof(char));

  // platform
  clGetPlatformIDs(MAX_PLATFORMS, platform_id, &num_platforms);
  if (platform >= num_platforms) {
    sprintf(msg, "error : platform = %d (limit = %d)", platform, num_platforms - 1);
    return 1;
  }

  // device
  clGetDeviceIDs(platform_id[platform], CL_DEVICE_TYPE_ALL, MAX_DEVICES, device_id, &num_devices);
  if (device >= num_devices) {
    sprintf(msg, "error : device = %d (limit = %d)", device, num_devices - 1);
    return 1;
  }

  // device name (option)
  clGetDeviceInfo(device_id[device], CL_DEVICE_NAME, sizeof(str), str, &ret_size);
  sprintf(msg, "%s (platform = %d, device = %d)", str, platform, device);
  char version[100];
  clGetPlatformInfo(platform_id[platform], CL_PLATFORM_VERSION, 100, version, &ret_size);
  printf("version, %s", version);

  //svm capability
  cl_device_svm_capabilities caps;
  clGetDeviceInfo(device_id[0], CL_DEVICE_SVM_CAPABILITIES, sizeof(cl_device_svm_capabilities), &caps, NULL);
  int svmCoarse     = 0!=(caps & CL_DEVICE_SVM_COARSE_GRAIN_BUFFER);
  int svmFineBuffer = 0!=(caps & CL_DEVICE_SVM_FINE_GRAIN_BUFFER);
  int svmFineSystem = 0!=(caps & CL_DEVICE_SVM_FINE_GRAIN_SYSTEM);
  int svmAtomics= 0!=(caps & CL_DEVICE_SVM_ATOMICS);
  printf("svmcoarse, %d", svmCoarse);
  printf("svmbuffer, %d", svmFineBuffer);
  printf("svmfinesys, %d", svmFineSystem);
  printf("svmAtomics, %d", svmAtomics);
  printf("svm cap, %d", caps);

  // context
  context = clCreateContext(NULL, 1, &device_id[device], NULL, NULL, &ret);

  // command queue
  Queue = clCreateCommandQueue(context, device_id[device], 0, &ret);
  char source[10] = "madd2.cl";
  char kern_name[10] = "madd";

  printf("\nkernel name %s\n\n", source);

  if ((fp = fopen(source, "r")) == NULL) {
    sprintf(msg, "kernel source open error");
    return 1;
  }
  source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
  fclose(fp);

  // program
  program = clCreateProgramWithSource(context, 1, (const char **)&source_str, (const size_t *)&source_size, &ret);
  if (ret != CL_SUCCESS) {
    sprintf(msg, "clCreateProgramWithSource() error");
    return 1;
  }

  // build
  if (clBuildProgram(program, 1, &device_id[device], NULL, NULL, NULL) != CL_SUCCESS) {
    sprintf(msg, "clBuildProgram() error");
    return 1;
  }

  // kernel
  k_vadd = clCreateKernel(program, kern_name, &ret);
  if (ret != CL_SUCCESS) {
    sprintf(msg, "clCreateKernel() error");
    return 1;
  }

  clReleaseProgram(program);
  free(source_str);

  return 0;


}
