/*
 *      2 vadd_ocl_v2.c (test program of OpenCL, version 2)
 *           3  
 *                4 Compile + Link:
 *                     5  > cl /Ox vadd_ocl_v2.c OpenCL.lib
 *                          6 
 *                               7  Usage:
 *                                    8 > vadd_ocl_v2 <n> <loop> <platform> <device>
 *                                         9  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <CL/cl.h>

#define MAX_PLATFORMS (10)
#define MAX_DEVICES (10)
#define MAX_SOURCE_SIZE (100000)

static int setup_ocl(cl_uint, cl_uint, char *);
static void vadd_calc();
static void vadd();
static inline void tvsub(struct timeval *x,
                         struct timeval *y,
                         struct timeval *ret)
{
  ret->tv_sec = x->tv_sec - y->tv_sec;
  ret->tv_usec = x->tv_usec - y->tv_usec;
  if (ret->tv_usec < 0) {
    ret->tv_sec--;
    ret->tv_usec += 1000000;
  }
}
             

cl_command_queue Queue;
cl_kernel k_vadd;
int N;
float *A, *B, *C;
cl_mem d_A, d_B, d_C;
void *h_a;
size_t memsize = 1000000;

int OCL;

int main(int argc, char **argv)
{
  int platform = 0;
  int device = 0;
  int nloop = 1000;

  // arguments
  N = atoi(argv[1]);
  if (argc >= 5) {
    N        = atoi(argv[1]);
    nloop    = atoi(argv[2]);
    platform = atoi(argv[3]);
    device   = atoi(argv[4]);
  }
  OCL = (platform >= 0);

  // alloc host arrays
  size_t size = N * N * sizeof(float);
  A = (float *)malloc(size);
  B = (float *)malloc(size);
  C = (float *)malloc(size);

  // setup problem
  for (int i = 0; i < N * N; i++) {
    A[i] = (float)(1 + i);
    B[i] = (float)(1 + i);
  }

  // setup OpenCL
  if (OCL) {
    char msg[BUFSIZ];
    int ret = setup_ocl((cl_uint)platform, (cl_uint)device, msg);
    printf("%s\n", msg);
    if (ret) {
      exit(1);
    }
  }

  // timer
  clock_t t0 = clock();

  // copy host to device
  if (OCL) {
    clEnqueueWriteBuffer(Queue, d_A, CL_TRUE, 0, size, A, 0, NULL, NULL);
    clEnqueueWriteBuffer(Queue, d_B, CL_TRUE, 0, size, B, 0, NULL, NULL);
  }

  // run
  //for (int loop = 0; loop < nloop; loop++) {
    vadd_calc();
 // }

  // copy device to host
  if (OCL) {
    clEnqueueReadBuffer(Queue, d_C, CL_TRUE, 0, size, C, 0, NULL, NULL);
  }

  // timer
  clock_t t1 = clock();
  double cpu = (double)(t1 - t0) / CLOCKS_PER_SEC;

  // sum
  double sum = 0;
  for (int i = 0; i < N*N; i++) {
    sum += C[i];
  }

  // output
  double exact = N * (N + 1.0);
  printf("n=%d nloop=%d %e(%.6e) cpu[sec]=%.3f\n",N, nloop, sum, exact, cpu);

  // release
  if (OCL) {
    clReleaseMemObject(d_A);
    clReleaseMemObject(d_B);
    clReleaseMemObject(d_C);
    clReleaseKernel(k_vadd);
    clReleaseCommandQueue(Queue);
  }

  // free
  free(A);
  free(B);
  free(C);

  return 0;
}

// setup OpenCL
static int setup_ocl(cl_uint platform, cl_uint device, char *msg)
{
  cl_context     context = NULL;
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

  // source
#if 0
  char source[10] = "vadd.cl";
  char kern_name[10] = "vadd";
#else
  char source[10] = "mmul.cl";
  char kern_name[10] = "multiply";
#endif
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

  cl_bool is_system;
  is_system = 1;
  clSetKernelExecInfo(k_vadd, CL_KERNEL_EXEC_INFO_SVM_PTRS,sizeof(cl_bool), &is_system);  
  printf("is system %d", is_system);

  h_a = malloc(memsize);
  memset(h_a, 2, memsize);
  // memory object
  size = N * N * sizeof(float);
  d_A = clCreateBuffer(context, CL_MEM_READ_WRITE, size, NULL, &ret);
  d_B = clCreateBuffer(context, CL_MEM_READ_WRITE, size, NULL, &ret);
  d_C = clCreateBuffer(context, CL_MEM_READ_WRITE, size, NULL, &ret);


  // release
  clReleaseProgram(program);
  clReleaseContext(context);

  // free
  free(source_str);

  return 0;
}

// entry point
static void vadd_calc()
{
  if (OCL) {
    size_t global_item_size, local_item_size;

    // args
    clSetKernelArg(k_vadd, 0, sizeof(cl_mem), (void *)&d_A);
    clSetKernelArg(k_vadd, 1, sizeof(cl_mem), (void *)&d_B);
    clSetKernelArg(k_vadd, 2, sizeof(cl_mem), (void *)&d_C);
    clSetKernelArg(k_vadd, 3, sizeof(int),    (void *)&N);
    clSetKernelArgSVMPointer(k_vadd, 4, h_a);

    // work item
    local_item_size = 256;
    global_item_size = ((N + local_item_size - 1) / local_item_size) * local_item_size;

    struct timeval tv_enqueuek_start, tv_enqueuek_end, tv;
    struct timeval tv_enqueuek_start_cache, tv_enqueuek_end_cache, tv_cache;
    float enqueue_time;
    void* tmp = malloc(memsize);
    void* tmp2 = malloc(memsize);

    
    // run
  gettimeofday(&tv_enqueuek_start, NULL);
    clEnqueueNDRangeKernel(Queue, k_vadd, 1, NULL, &global_item_size, &local_item_size, 0, NULL, NULL);
  //clEnqueueSVMMap(Queue, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, h_a, 100, 0, NULL, NULL);
  gettimeofday(&tv_enqueuek_start_cache, NULL);
  memcpy(tmp, h_a, memsize);
  gettimeofday(&tv_enqueuek_end_cache, NULL);
  tvsub(&tv_enqueuek_end_cache, &tv_enqueuek_start_cache, &tv);
  enqueue_time = tv.tv_sec * 1000.0 + (float) tv.tv_usec / 1000.0;
  printf("time without cache:%f ", enqueue_time);

  unsigned int testing  = *(int*)h_a;
  printf("testing %x", testing);
  gettimeofday(&tv_enqueuek_start_cache, NULL);
  memcpy(tmp2, h_a, memsize);
  gettimeofday(&tv_enqueuek_end_cache, NULL);
  tvsub(&tv_enqueuek_end_cache, &tv_enqueuek_start_cache, &tv);
  enqueue_time = tv.tv_sec * 1000.0 + (float) tv.tv_usec / 1000.0;
  printf("time in cache:%f ", enqueue_time);
    clFinish(Queue);
  gettimeofday(&tv_enqueuek_end, NULL);
  tvsub(&tv_enqueuek_end, &tv_enqueuek_start, &tv);
  enqueue_time = tv.tv_sec * 1000.0 + (float) tv.tv_usec / 1000.0;
  printf("enqueue kernel: %f\n", enqueue_time);
  }
  else {
    // serial code
    vadd();
  }
}

// serial code
static void vadd()
{

  for (int i = 0; i < N; i++) {
    C[i] = A[i] + B[i];
  }

}


