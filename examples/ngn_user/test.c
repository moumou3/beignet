#include <intel_driver.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
  char card_name[20];
  sprintf(card_name, "/dev/dri/renderD128");
  if (access(card_name, R_OK!=0) {
      printf("access error");
  if (intel_driver_init_render(intel, card_name))
    printf("render ok");
    
  return 0;
}
