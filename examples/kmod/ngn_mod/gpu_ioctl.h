#define PACKET_NUM 256 * 1024

struct gpu_ioctl_args {
  int pid;
  void *uvaddr;
  int *matrix1;
  int *matrix2;
  int *sum;
}; 
