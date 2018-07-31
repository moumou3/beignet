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
#include <time.h>

#include <CL/cl.h>

#define MAX_PLATFORMS (10)
#define MAX_DEVICES (10)
#define MAX_SOURCE_SIZE (100000)

static int setup_ocl(cl_uint, cl_uint, char *);
static void vadd_calc();
static void vadd();

cl_command_queue Queue;
cl_kernel k_vadd;
int N;
float *A, *B, *C;
cl_mem d_A, d_B, d_C;
int OCL;

int main(int argc, char **argv)
{
  int platform = 0;
  int device = 0;
  int nloop = 1000;

  // arguments
  N = 1000;
  if (argc >= 5) {
    N        = atoi(argv[1]);
    nloop    = atoi(argv[2]);
    platform = atoi(argv[3]);
    device   = atoi(argv[4]);
  }
  OCL = (platform >= 0);

  // alloc host arrays
  size_t size = N * sizeof(float);
  A = (float *)malloc(size);
  B = (float *)malloc(size);
  C = (float *)malloc(size);

  // setup problem
  for (int i = 0; i < N; i++) {
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
  for (int loop = 0; loop < nloop; loop++) {
    vadd_calc();
  }

  // copy device to host
  if (OCL) {
    clEnqueueReadBuffer(Queue, d_C, CL_TRUE, 0, size, C, 0, NULL, NULL);
  }

  // timer
  clock_t t1 = clock();
  double cpu = (double)(t1 - t0) / CLOCKS_PER_SEC;

  // sum
  double sum = 0;
  for (int i = 0; i < N; i++) {
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

  printf("aaa");
  // platform
  clGetPlatformIDs(MAX_PLATFORMS, platform_id, &num_platforms);
  printf("bbb");
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

  // context
  context = clCreateContext(NULL, 1, &device_id[device], NULL, NULL, &ret);

  // command queue
  Queue = clCreateCommandQueue(context, device_id[device], 0, &ret);

  // source
  if ((fp = fopen("vadd.cl", "r")) == NULL) {
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
  k_vadd = clCreateKernel(program, "vadd", &ret);
  if (ret != CL_SUCCESS) {
    sprintf(msg, "clCreateKernel() error");
    return 1;
  }

  // memory object
  size = N * sizeof(float);
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

    // work item
    local_item_size = 256;
    global_item_size = ((N + local_item_size - 1) / local_item_size) * local_item_size;

    // run
    clEnqueueNDRangeKernel(Queue, k_vadd, 1, NULL, &global_item_size, &local_item_size, 0, NULL, NULL);
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


