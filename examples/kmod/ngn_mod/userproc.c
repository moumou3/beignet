
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include <sys/ioctl.h>
#include <CL/cl.h>


int main(){

  int fd = 0;

  fd = open("/dev/comgpu", O_RDWR);             // Open the device with read/write access

  int retr = ioctl(fd, 1001, NULL); //test start
  //user space network
 //process_to_gpu 

}
