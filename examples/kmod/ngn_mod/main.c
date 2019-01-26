#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h> 
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/pid.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/siginfo.h>
#include "gpu_ioctl.h"

#define  DEVICE_NAME "comgpu"
#define CLASS_NAME "testgpu" 
#define DEV_GPU_PROCESS 1
#define DEV_GPU_PROCESS2 2
#define NPAGES 16





MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("KERNEL GPU TASK PROjCESS");
MODULE_AUTHOR("Motoya Tomoe");


static int majorNumber;
static char   message[256] = {0};           
static short  size_of_message;              
static int    numberOpens = 0; 
static struct device* comgpu;
static struct class*  testgpuClass  = NULL;
static void* kmalloc_ptr;
static int* kmalloc_area;
extern int m1[PACKET_NUM];
extern int m2[PACKET_NUM];
extern int summation[PACKET_NUM]; 

extern struct task_struct *gpud_task;
extern int gpud_flag;


static int dev_open(struct inode *inodep, struct file *filep);
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset);
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset);
static int dev_release(struct inode *inodep, struct file *filep);
static int mmap_kmem(struct file *filp, struct vm_area_struct *vma);
static long dev_ioctl(struct file* filep, unsigned int cmd, unsigned long arg); 
static void ioctl_gpuprocess(void __user *arg); 
static void ioctl_gpuprocess2(void __user *arg); 
static void ioctl_getsum(void __user *arg); 

static struct file_operations fops =
{
  .open = dev_open,
  .read = dev_read,
  .write = dev_write,
  .release = dev_release,
  .mmap = mmap_kmem,  
  .unlocked_ioctl = dev_ioctl
} ;



static int __init kspace_gpu_init(void)
{

  int ret = 0;
  int i;

  if ((kmalloc_ptr = kmalloc((NPAGES + 2) * PAGE_SIZE, GFP_KERNEL)) == NULL) {  
    ret = -ENOMEM;  
    goto out;  
  }
   /* round it up to the page bondary */
  kmalloc_area = (int *)((((unsigned long)kmalloc_ptr) + PAGE_SIZE - 1) & PAGE_MASK); 
  for (i = 0; i < NPAGES * PAGE_SIZE; i+= PAGE_SIZE) {  
    SetPageReserved(virt_to_page(((unsigned long)kmalloc_area) + i));  
  }  
  printk("kmalloc_area %p", kmalloc_area);
  /* store a pattern in the memory - the test application will check for it */  
  for (i = 0; i < (NPAGES * PAGE_SIZE / sizeof(int)); i += 2) {  
    kmalloc_area[i] = (0xdead << 16) + i;  
    kmalloc_area[i + 1] = (0xbeef << 16) + i;  
  } 

  majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
  if (majorNumber < 0){
    printk(KERN_ALERT "failed to register a major number\n");
    return majorNumber;
  }

  testgpuClass = class_create(THIS_MODULE, CLASS_NAME);
  if (IS_ERR(testgpuClass)){                // Check for error and clean up if there is
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_ALERT "Failed to register device class\n");
    return PTR_ERR(testgpuClass);          // Correct way to return an error on a pointer
  }
  comgpu = device_create(testgpuClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
  printk("Hello device");


  return ret;


out:  
  return ret; 


}

static void __exit kspace_gpu_exit(void)
{
  int i;
  device_destroy(testgpuClass, MKDEV(majorNumber, 0));     // remove the device
  class_unregister(testgpuClass);                          // unregister the device class
  class_destroy(testgpuClass);                             // remove the device class
  unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number

  /* unreserve the pages */  
  for (i = 0; i < NPAGES * PAGE_SIZE; i+= PAGE_SIZE) {  
    ClearPageReserved(virt_to_page(((unsigned long)kmalloc_area) + i));  
  }  
  /* free the memory areas */  
  kfree(kmalloc_ptr);  
  printk(KERN_INFO "EBBChar: Goodbye from the LKM!\n");

}

static int dev_open(struct inode *inodep, struct file *filep){
  numberOpens++;
  printk(KERN_INFO "EBBChar: Device has been opened %d time(s)\n", numberOpens);
  return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
  int error_count = 0;
  // copy_to_user has the format ( * to, *from, size) and returns 0 on success
  error_count = copy_to_user(buffer, message, size_of_message);

  if (error_count==0){            // if true then have success
    printk(KERN_INFO "EBBChar: Sent %d characters to the user\n", size_of_message);
    return (size_of_message=0);  // clear the position to the start and return 0
  }
  else {
    printk(KERN_INFO "EBBChar: Failed to send %d characters to the user\n", error_count);
    return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
  }
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){


  copy_from_user(message, buffer, len); 
  message[len] = '\0';

  //sprintf(message, "%s(%zu letters)", buffer, len);   // appending received string with its length
  size_of_message = strlen(message);                 // store the length of the stored message
  printk(KERN_INFO "EBBChar: Received %zu characters from the user\n", len);

  return len;
}


static int mmap_kmem(struct file *filp, struct vm_area_struct *vma)  
{  
  int ret;  
  long length = vma->vm_end - vma->vm_start;  
  printk("mmap called vm_start, %x", vma->vm_start);

  /* check length - do not allow larger mappings than the number of 
   *            pages allocated */  
  if (length > NPAGES * PAGE_SIZE)  
    return -EIO;  

  /* map the whole physically contiguous area in one piece */  
  if ((ret = remap_pfn_range(vma,  
                             vma->vm_start,  
                             virt_to_phys((void *)kmalloc_area) >> PAGE_SHIFT,  
                             length,  
                             vma->vm_page_prot)) < 0) {  
    return ret;  
  }  

  return 0;  
}

static void ioctl_gpuprocess(void __user *arg) {

  printk("kmalloc_area %x\n", kmalloc_area[0]);

}

static void ioctl_gpuprocess2(void __user *arg) {

  struct gpu_ioctl_args __user *uarg = (struct gpu_ioctl_args __user*)arg;
  struct gpu_ioctl_args karg;
  
  //kernel task for gpu
  int matrix1[PACKET_NUM] = {1,2,3};
  int matrix2[PACKET_NUM] = {1,2,3};

  printk(KERN_INFO "gpu proc2 \n");

  copy_from_user(&karg, uarg, sizeof(struct gpu_ioctl_args));

  void __user *mat1 = karg.matrix1;
  void __user *mat2 = karg.matrix2;
  copy_to_user(mat1, matrix1, sizeof(int) * PACKET_NUM);
  copy_to_user(mat2, matrix2, sizeof(int) * PACKET_NUM);




  //delete by compiler or human power ???
  /*
  for (int i = 0; i < 100; i++)
    sum[i] = matrix[i] + matrix2[i]; 
  */


}
static void ioctl_getsum(void __user *arg)
{
  int __user *uarg =  (int __user*) arg;
  int sum[PACKET_NUM] = {0};
  copy_from_user(sum, uarg, sizeof(int) * PACKET_NUM);
  printk(KERN_INFO "sum = %d, %d", sum[0], sum[1]);

}

static void send_signal(int signum, struct task_struct *sigtask) {

  struct siginfo info;
  memset(&info, 0, sizeof(struct siginfo));
  info.si_signo = signum;
  info.si_code = 0;
  info.si_int = 1234;

  int ret = send_sig_info(signum, &info, sigtask);
  if (ret < 0){
    printk("error sending signal %d", ret);
  }

}

static void ioctl_kstart(void __user *arg) 
{
  //kernel start e.g network packet processing
  //for (i=0; i < LNUM; i++) SIMD process;
  //set_gpu_kernel_program
  
  int matrix1[PACKET_NUM] = {1,2,3};
  int matrix2[PACKET_NUM] = {2,3,4};
  int sum[PACKET_NUM] = {0};
  int i=0;;



  memcpy(m1, matrix1, sizeof(int) * PACKET_NUM);
  memcpy(m2, matrix2, sizeof(int) * PACKET_NUM);

  send_signal(SIGUSR1, gpud_task);
  printk("sent signal to %d", gpud_task->pid);
  
  while(!gpud_flag && i++ < 1000){
    schedule();
    printk("signal handle end waiting %d", i);
  }
 

  printk("receive flag");
  memcpy(sum, summation, sizeof(int) * PACKET_NUM);
  printk(KERN_INFO "sum = %d, %d", sum[0], sum[1]);
  //alloc_svm 
  
 //schedule to process
 
//


}

static long dev_ioctl(struct file* filep, unsigned int cmd, unsigned long arg) {

  void __user* uarg =  (void __user*) arg;
  printk(KERN_INFO "dev_ioctl %d\n", cmd);

  switch (cmd) {
    case 1000:
      ioctl_gpuprocess(uarg);
      break;
    case 1001:
      printk("aaa");
      ioctl_gpuprocess2(uarg);
      break;
    case 1002:
      printk("bbb");
      ioctl_getsum(uarg);
      break;
    case 1003:
      printk("ccc");
      ioctl_kstart(uarg);
      break;
  }
  return 0;
  
}

static int dev_release(struct inode *inodep, struct file *filep){
  printk(KERN_INFO "EBBChar: Device successfully closed\n");
  return 0;
}
module_init(kspace_gpu_init);
module_exit(kspace_gpu_exit);
