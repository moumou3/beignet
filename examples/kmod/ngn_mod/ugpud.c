#include <stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include <sys/ioctl.h>
#include<unistd.h>
#include <CL/cl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/syscall.h>
#include  <sys/ipc.h>
#include "gpu_ioctl.h"

static inline unsigned long long rdtsc() {

  uint32_t hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (uint64_t)lo)|( ((uint64_t)hi)<<32 );

}

#define MAX_PLATFORMS (10)
#define MAX_DEVICES (10)
#define MAX_SOURCE_SIZE (100000)

int siggpu;
int fd;
cl_command_queue Queue;
cl_kernel k_vadd;
cl_context     context = NULL;
size_t memsize;

static int setup_ocl(cl_uint platform, cl_uint device, char* msg);

static void sig_gpu_handle() {
//gpu calc
  int platform = 0;
  int device = 0;
  char msg[BUFSIZ];
  int ret = 0;
  int *matrix1;
  int *matrix2;
  int *sum;
  struct gpu_ioctl_args args = {0}; 
  uint64_t rdt1;
  uint64_t rdt2;
  int packet_num = PACKET_NUM;


  memsize = sizeof(int) * PACKET_NUM;

  siggpu = 1;
  printf("gpu started\n");

  rdt1 = rdtsc();
  ret = setup_ocl((cl_uint)platform, (cl_uint)device, msg);
  rdt2 = rdtsc();
  printf("setup packet num %d %llu\n", PACKET_NUM, rdt2 - rdt1);


  if (ret < 0)
    printf("ret= %d , %s", ret, msg);

  rdt1 = rdtsc();
  matrix1 = clSVMAlloc(context, CL_MEM_READ_WRITE|CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS, memsize, 0);
  matrix2 = clSVMAlloc(context, CL_MEM_READ_WRITE|CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS, memsize, 0);
  sum = clSVMAlloc(context, CL_MEM_READ_WRITE|CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS, memsize, 0);
  rdt2 = rdtsc();
  printf("alloc %llu\n", rdt2 - rdt1);

  args.matrix1 = matrix1;
  args.matrix2 = matrix2;
  args.sum = sum;
  rdt1 = rdtsc();
  ioctl(fd, 1001, &args);
  rdt2 = rdtsc();
  printf("ioctl_get_matrix  %llu\n", rdt2 - rdt1);
  printf("matrix1= %d, %d\n",matrix1[0], matrix2[1]);

  rdt1 = rdtsc();
  clSetKernelArgSVMPointer(k_vadd, 0, matrix1);
  clSetKernelArgSVMPointer(k_vadd, 1, matrix2);
  clSetKernelArgSVMPointer(k_vadd, 2, sum);
  clSetKernelArg(k_vadd, 3, sizeof(packet_num), &packet_num);
  rdt2 = rdtsc();
  printf("set kernel arg %llu\n", rdt2 - rdt1);

  size_t local_item_size = 256;
  size_t global_item_size = 1;
  rdt1 = rdtsc();
  clEnqueueNDRangeKernel(Queue, k_vadd, 1, NULL, &global_item_size, &local_item_size, 0, NULL, NULL);
  clFinish(Queue);
  rdt2 = rdtsc();
  printf("clEnqueueNDRangeKernel  %llu\n", rdt2 - rdt1);

  printf("sum = %d, %d",sum[0], sum[1]);

  clReleaseKernel(k_vadd);
  clReleaseCommandQueue(Queue);
  clReleaseContext(context);

  ioctl(fd, 1000, &args);
  printf("gpu ended\n");
}

int main() {
  

  struct sigaction usr_action;
  siggpu = 0;
  
  printf("open device\n");
  fd = open("/dev/kgpud", O_RDWR);             // Open the device with read/write access
  

  printf("currentpid %d %d\n",syscall(SYS_gettid), fd);

  memset(&usr_action, 0, sizeof(usr_action));
  sigset_t block_mask;
  sigfillset(&block_mask);
  usr_action.sa_handler = sig_gpu_handle;
  usr_action.sa_mask = block_mask;
  usr_action.sa_flags = SA_NODEFER;
  sigaction(SIGUSR1, &usr_action, NULL);

  while(!siggpu);

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
  uint64_t rdt1;
  uint64_t rdt2;

  // alloc
  source_str = (char *)malloc(MAX_SOURCE_SIZE * sizeof(char));

  // platform
  rdt1 = rdtsc();
  clGetPlatformIDs(MAX_PLATFORMS, platform_id, &num_platforms);
  rdt2 = rdtsc();
  printf("clGetPlatformIDs %llu\n", rdt2 - rdt1);

  if (platform >= num_platforms) {
    sprintf(msg, "error : platform = %d (limit = %d)", platform, num_platforms - 1);
    return 1;
  }

  // device
  rdt1 = rdtsc();
  clGetDeviceIDs(platform_id[platform], CL_DEVICE_TYPE_ALL, MAX_DEVICES, device_id, &num_devices);
  rdt2 = rdtsc();
  printf("clGetDeviceIDs %llu\n", rdt2 - rdt1);
  if (device >= num_devices) {
    sprintf(msg, "error : device = %d (limit = %d)", device, num_devices - 1);
    return 1;
  }

  // device name (option)
  clGetDeviceInfo(device_id[device], CL_DEVICE_NAME, sizeof(str), str, &ret_size);
  sprintf(msg, "%s (platform = %d, device = %d)", str, platform, device);
  printf("ioctl start\n");
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
  rdt1 = rdtsc();
  context = clCreateContext(NULL, 1, &device_id[device], NULL, NULL, &ret);
  rdt2 = rdtsc();
  printf("clCreateContext%llu\n", rdt2 - rdt1);

  // command queue
  rdt1 = rdtsc();
  Queue = clCreateCommandQueue(context, device_id[device], 0, &ret);
  rdt2 = rdtsc();
  printf("clCreateQueue %llu\n", rdt2 - rdt1);

  char source[10] = "madd.cl";
  char kern_name[10] = "madd";

  printf("\nkernel name %s\n\n", source);

  if ((fp = fopen(source, "r")) == NULL) {
    sprintf(msg, "kernel source open error");
    return 1;
  }
  source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
  fclose(fp);

  // program
  rdt1 = rdtsc();
  program = clCreateProgramWithSource(context, 1, (const char **)&source_str, (const size_t *)&source_size, &ret);
  if (ret != CL_SUCCESS) {
    sprintf(msg, "clCreateProgramWithSource() error");
    return 1;
  }
  rdt2 = rdtsc();
  printf("clCreateProgramWithSource %llu\n", rdt2 - rdt1);

  rdt1 = rdtsc();
  // build
  if (clBuildProgram(program, 1, &device_id[device], NULL, NULL, NULL) != CL_SUCCESS) {
    sprintf(msg, "clBuildProgram() error");
    return 1;
  }
  rdt2 = rdtsc();
  printf("clBuildProgram%llu\n", rdt2 - rdt1);


  rdt1 = rdtsc();
  // kernel
  k_vadd = clCreateKernel(program, kern_name, &ret);
  if (ret != CL_SUCCESS) {
    sprintf(msg, "clCreateKernel() error");
    return 1;
  }
  rdt2 = rdtsc();
  printf("clCreateKernel %llu\n", rdt2 - rdt1);


  clReleaseProgram(program);
  free(source_str);

  return 0;


}
