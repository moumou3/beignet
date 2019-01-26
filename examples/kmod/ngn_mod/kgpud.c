#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h> 
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/pid.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "gpu_ioctl.h"

#define  DEVICE_NAME "kgpud"
#define CLASS_NAME "kgpud_class" 

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("GPU DAMON");
MODULE_AUTHOR("Motoya Tomoe");



struct task_struct *gpud_task;
EXPORT_SYMBOL(gpud_task);
int gpud_flag;
EXPORT_SYMBOL(gpud_flag);

int m1[PACKET_NUM];
EXPORT_SYMBOL(m1);
int m2[PACKET_NUM];
EXPORT_SYMBOL(m2);
int summation[PACKET_NUM];
EXPORT_SYMBOL(summation);


static int dev_open(struct inode *inodep, struct file *filep);
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset);
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset);
static int dev_release(struct inode *inodep, struct file *filep);
static long dev_ioctl(struct file* filep, unsigned int cmd, unsigned long arg); 
static void ioctl_endgpu(void __user* uarg); 
extern struct gpu_ioctl_args args;

static int majorNumber;
static char   message[256] = {0};           
static short  size_of_message;              
static int    numberOpens = 0; 
static struct device* kgpud;
static struct class*  testgpuClass  = NULL;


static struct file_operations fops =
{
  .open = dev_open,
  .read = dev_read,
  .write = dev_write,
  .release = dev_release,
  .unlocked_ioctl = dev_ioctl,
} ;

static int __init kgpud_init(void) {

  //setup_kocl
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
  kgpud = device_create(testgpuClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
  printk("Hello device");
  return 0;

}
static void __exit kgpud_exit(void) {

  device_destroy(testgpuClass, MKDEV(majorNumber, 0));     // remove the device
  class_unregister(testgpuClass);                          // unregister the device class
  class_destroy(testgpuClass);                             // remove the device class
  unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
  printk(KERN_INFO "EBBChar: Goodbye from the LKM!\n");


}

static int dev_open(struct inode *inodep, struct file *filep){
  numberOpens++;
  printk(KERN_INFO "EBBChar: Device has been opened %d time(s)\n", numberOpens);
  printk(KERN_INFO "current pid%d\n", current->pid);
  gpud_task = current;
  gpud_flag = 0;
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

  size_of_message = strlen(message);                 // store the length of the stored message

  printk(KERN_INFO "EBBChar: Received %zu characters from the user\n", len);


  return len;
}

static int dev_release(struct inode *inodep, struct file *filep){
  printk(KERN_INFO "EBBChar: Device successfully closed\n");
  return 0;
}

static void ioctl_endgpu(void __user* uarg) {
  struct gpu_ioctl_args karg;
  copy_from_user(&karg, uarg, sizeof(struct gpu_ioctl_args));
  copy_from_user(summation, karg.sum, sizeof(int)*PACKET_NUM);
  gpud_flag = 1;

  printk("gpu end\n");
} 

static void ioctl_readmatrix(void __user* uarg) {
  struct gpu_ioctl_args karg;
  //printk("m1 = %d %d\n", m1[0], m1[1]);
  copy_from_user(&karg, uarg, sizeof(struct gpu_ioctl_args));
  copy_to_user(karg.matrix1, m1, sizeof(int) * PACKET_NUM);
  copy_to_user(karg.matrix2, m2, sizeof(int) * PACKET_NUM);
} 
static long dev_ioctl(struct file* filep, unsigned int cmd, unsigned long arg) {

  void __user* uarg =  (void __user*) arg;
  printk(KERN_INFO "dev_ioctl %d\n", cmd);

  switch (cmd) {
    case 1000:
      ioctl_endgpu(uarg);
      break;
    case 1001:
      ioctl_readmatrix(uarg);
      break;
  }
  return 0;
  
}

module_init(kgpud_init);
module_exit(kgpud_exit);
