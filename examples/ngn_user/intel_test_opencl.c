#include <stdio.h>
#include<malloc.h>
#include <sys/time.h>
#include <time.h>

static inline unsigned long long rdtsc() {
  unsigned long long ret;

  __asm__ volatile ("rdtsc" : "=A" (ret));

  return ret;
}

unsigned long long start, end;
void *h_a;


int main() {
  int sum = 0;
  // size_t memsize = (1 << 20);
  // int *array = (int*)malloc(sizeof(int) * 100); 
  
  char msg[BUFSIZ];
  int ret = setup_ocl((cl_uint)platform, (cl_uint)device, msg);
  
  // context
  context = clCreateContext(NULL, 1, &device_id[device], NULL, NULL, &ret);
  h_a = clSVMAlloc(context, CL_MEM_READ_WRITE, memsize, 0);
 
  for(size_t i = 0; i < 100; i++) 
     array[i] = 1;
  
  sum = 0;
  start = rdtsc();
  for(size_t i = 0; i < 100; ++i) 
    sum += array[i];
  end= rdtsc();
  printf("with cache %llu, sum:%d\n", end - start, sum);


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

  
  unsigned long long start3, end3;
  start3 = rdtsc();
#ifdef FINE_GRAIN_ON
  h_a = clSVMAlloc(context, CL_MEM_READ_WRITE|CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS, memsize, 0);
  printf("fine grain on\n\n");
#else
  h_a = clSVMAlloc(context, CL_MEM_READ_WRITE, memsize, 0);
  printf("coarse grain on\n\n");
#endif
  end3 = rdtsc();


  printf("svmalloc time %llu\n", end3 - start3);

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
