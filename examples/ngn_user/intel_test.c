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
int main() {
  int sum = 0;
  size_t memsize = (1 << 20);
  int *array = (int*)malloc(sizeof(int) * 100); 
 
  start = rdtsc();
  for(size_t i = 0; i < 100; i++) 
     array[i] = 1;
  end= rdtsc();
  printf("without cache %llu, sum:%d\n", end - start, sum);
  sum = 0;



  start = rdtsc();
  for(size_t i = 0; i < 100; ++i) 
    sum += array[i];
  end= rdtsc();
  printf("with cache %llu, sum:%d\n", end - start, sum);

  start = rdtsc();
  for(size_t i = 0; i < 100; ++i) 
    sum += array[i];
  end= rdtsc();
  printf("with cache 2nd %llu, sum:%d\n", end - start, sum);


}
