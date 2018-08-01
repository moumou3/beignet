#include <cl.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <vmalloc.h>
#include <linux/time.h>
#include <linux/fs.h>






MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("KSPACE NIC GPU NIC SERVER");
MODULE_AUTHOR("Motoya Tomoe");



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
  source_str = (char *)vmalloc(MAX_SOURCE_SIZE * sizeof(char));

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

  // context
  context = clCreateContext(NULL, 1, &device_id[device], NULL, NULL, &ret);

  // command queue
  Queue = clCreateCommandQueue(context, device_id[device], 0, &ret);

  // source
  if ((fp = filp_open("vadd.cl", O_RDONLY, 0)) == NULL) {
    sprintf(msg, "kernel source open error");
    return 1;
  }
  source_size = kernel_read(fp, 0, source_str, MAX_SOURCE_SIZE);
  filp_close(fp, NULL);

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
  vfree(source_str);

  return 0;


}

static int __init kspace_ngn_init(void)
{

  int platform = 0;
  int device = 0;
  struct timeval tv;
  struct timeval tv_ngn_start, tv_ngn_end;


  N = 1000;


  // alloc host arrays
  size_t size = N * sizeof(float);
  A = (float *)vmalloc(size);
  B = (float *)vmalloc(size);
  C = (float *)vmalloc(size);

  // setup problem
  for (int i = 0; i < N; i++) {
    A[i] = (float)(1 + i);
    B[i] = (float)(1 + i);
  }

  // setup OpenCL
  char msg[BUFSIZ];
  int ret = setup_ocl((cl_uint)platform, (cl_uint)device, msg);
  printk("%s\n", msg);
  if (ret) {
    exit(1);
  }

  // timer
  do_gettimeofday(&tv_ngn_start);

  // copy host to device
  clEnqueueWriteBuffer(Queue, d_A, CL_TRUE, 0, size, A, 0, NULL, NULL);
  clEnqueueWriteBuffer(Queue, d_B, CL_TRUE, 0, size, B, 0, NULL, NULL);

  // run
  vadd_calc();

  // copy device to host
  clEnqueueReadBuffer(Queue, d_C, CL_TRUE, 0, size, C, 0, NULL, NULL);

  // timer
  do_gettimeofday(&tv_ngn_end);

  // output

  // release
  clReleaseMemObject(d_A);
  clReleaseMemObject(d_B);
  clReleaseMemObject(d_C);
  clReleaseKernel(k_vadd);
  clReleaseCommandQueue(Queue);

  // free
  vfree(A);
  vfree(B);
  vfree(C);

  return 0;

}

static void __exit kspace_ngn_exit(void)
{
}

module_init(kspace_ngn_init);
module_exit(kspace_ngn_exit);
