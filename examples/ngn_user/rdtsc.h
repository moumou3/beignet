#ifndef RDTSC_H_
#define RDTSC_H_

inline unsigned long long rdtsc() {
  unsigned long long ret;

  __asm__ volatile ("rdtsc" : "=A" (ret));

  return ret;
}

#endif /* RDTSC_H_ */
