#include <stdio.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
  char card_name[20];
  sprintf(card_name, "/dev/dri/renderD128");
  if (access(card_name, R_OK)!=0) 
      printf("access error");
  int fd = open(card_name,  O_RDWR);
  printf("card %d", fd);
  return 0;
}
