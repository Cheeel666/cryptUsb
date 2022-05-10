#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/fs.h>

#include <linux/fs_struct.h>

#include <linux/proc_fs.h>
#include <linux/mount.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/syscalls.h>
#include <linux/notifier.h>

#define USB_FOLDER ""
#define PASSWORD_FILE ""

#define ENCRYPT = true;




MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ilya Chelyadinov");
MODULE_VERSION("1.0");




// Declare a struct with a list in a compile time
LIST_HEAD(connected_devices);
static struct crypto_usb 
{
    struct usb_device_id dev_id;
};

typedef struct int_usb_device
{
    struct usb_device_id dev_id;
    struct list_head list_node;
} int_usb_device_t;

struct crypto_usb known_devices[] = {
    { .dev_id = {USB_DEVICE(0x1B1C, 0x1A0F)}}
};


// Read password from USB device
static char *read_file(char *filename)
{
    struct kstat *stat;
    struct file *fp;
    mm_segment_t fs;
    loff_t pos = 0;
    char *buf;
    int size;

    fp = filp_open(filename, O_RDWR, 0644);
    if (IS_ERR(fp))
    {
        return NULL;
    }

    fs = get_fs();
    set_fs(KERNEL_DS);
    
    stat = (struct kstat *)kmalloc(sizeof(struct kstat), GFP_KERNEL);
    if (!stat)
    {
        return NULL;
    }

    vfs_stat(filename, stat);
    size = stat->size;

    buf = kmalloc(size, GFP_KERNEL);
    if (!buf) 
    {
        kfree(stat);
        return NULL;
    }

    kernel_read(fp, buf, size, &pos);

    filp_close(fp, NULL);
    set_fs(fs);
    kfree(stat);
    buf[size]='\0';
    return buf;
}

// Call decryption from user space
static int call_decryption(void) {
    printk(KERN_INFO "USB MODULE: Call_decrypt\n");

    char path[80];
    char *data = read_file(path);

    char *argv[] = {asd
        "/home/parallels/Desktop/c_dev/cryptUsb",
        data,
        NULL };

    static char *envp[] = {
        "HOME=/",
        "TERM=linux",
        "PATH=/sbin:/bin:/usr/sbin:/usr/bin", 
        NULL };

    if (call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC) < 0) 
    {
        return -1;
    }

    return 0;
}

// Call encryption from user space
static int call_encryption(void) {
    printk(KERN_INFO "USB MODULE: Call_encrypt\n");
    char *argv[] = {
        "/home/jasur/projects/courseWork-OS/code/crypto",
        NULL };

    static char *envp[] = {
        "HOME=/",
        "TERM=linux",
        "PATH=/sbin:/bin:/usr/sbin:/usr/bin", 
        NULL };

    if (call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC) < 0) 
    {
        return -1;
    }

    return 0;
}

static bool device_from_list(struct usb_device_id *device, const struct usb_device_id *allowed_device) {
    if (device->idVendor != allowed_device->idVendor || device->idProduct != allowed_device->idProduct) {
        return false;
    }    
    return true;
}

static bool is_known_usb(struct usb_device_id *device) {
    unsigned long usb_count = sizeof(known_devices) / sizeof(struct usb_device_id);
    int i;
    for (i = 0; i < usb_count; i++) {
        if (device_from_list(device, &known_devices[i].dev_id)) {
            return true;
        }
    }
    return false;
}

// Match device with device id.
static bool is_dev_matched(struct usb_device *dev, const struct usb_device_id *dev_id)
{
    // Check idVendor and idProduct, which are used.
    if (dev_id->idVendor != dev->descriptor.idVendor || dev_id->idProduct != dev->descriptor.idProduct)
    {
        return false;
    }

    return true;
}

//start tracking new usb
static void new_usb_dev(struct usb_device *device) 
{
    int_usb_device_t *new_dev = (int_usb_device_t *)kmalloc(sizeof(int_usb_device_t), GFP_KERNEL);
    struct usb_device_id new_id = {USB_DEVICE(device->descriptor.idVendor, device->descriptor.idProduct)};
    new_dev->dev_id = new_id;
    list_add_tail(&new_dev->list_node, &connected_devices);
}





// Match device id with device id.
static bool match_devices(struct usb_device_id *new_dev_id, const struct usb_device_id *dev_id)
{
    // Check idVendor and idProduct, which are used.
    if (dev_id->idVendor != new_dev_id->idVendor || dev_id->idProduct != new_dev_id->idProduct)
    {
    
        return false;
    }
    
    return true;
}




// Check if device is in allowed devices list.
static bool *is_dev_allowed(struct usb_device_id *dev)
{
    unsigned long allowed_devs_len = sizeof(known_devices) / sizeof(struct usb_device_id);

    int i;
    for (i = 0; i < allowed_devs_len; i++)
    {
        if (match_devices(dev, &known_devices[i].dev_id))
        {
            return true;
        }
    }

    return false;
}

// Check if changed device is acknowledged.
static int count_known_dev(void)
{
    int_usb_device_t *temp;
    int count = 0;

    list_for_each_entry(temp, &connected_devices, list_node)
    {
        if (is_dev_allowed(&temp->dev_id))
        {
            count++;
        }
    }
    printk(KERN_INFO "CRYPTOUSB: %d devices connected-------------");
    return count;
}

//  Delete device from list of tracked devices.
static void delete_usb(struct usb_device *dev)
{
    int_usb_device_t *device, *temp;
    list_for_each_entry_safe(device, temp, &connected_devices, list_node)
    {
        if (is_dev_matched(dev, &device->dev_id))
        {
            list_del(&device->list_node);
            kfree(device);
        }
    }
}


// Check if changed device is acknowledged.
static bool connected_bad_devices(void)
{
    int_usb_device_t *temp;

    list_for_each_entry(temp, &connected_devices, list_node)
    {
        if (!is_dev_allowed(&temp->dev_id))
        {
            return false;
        }
    }
    return true;
}


static void usb_dev_insert(struct usb_device *device)
{
    printk(KERN_INFO "CRYPTOUSB: Inserted USB with PID: %d and VID: %d", 
    device->descriptor.idProduct, device->descriptor.idVendor);

    new_usb_dev(device);
    int known_devices_connected = count_known_dev();

    
    if (!known_devices_connected) {
        return;
    }
    printk(KERN_INFO "CRYPTOUSB: %d good device. Decrypting", known_devices_connected);
    //crypt(1, *PASSWORD);
    call_decryption()
}


static void usb_dev_remove(struct usb_device *device)
{
    printk(KERN_INFO "CRYPTOUSB: Removed USB with PID: %d and VID: %d", 
    device->descriptor.idProduct, device->descriptor.idVendor);
    
    delete_usb(device);
    int known_devices_connected = count_known_dev();

    if (known_devices_connected) {
        printk(KERN_INFO "CRYPTOUSB: %d GOOD CONNECTED DEVICES: skipping encrypt", known_devices_connected);
        return;
    }
    
    printk(KERN_INFO "CRYPTOUSB: NO GOOD DEVICES CONNECTED. Starting encrypt");
}


static int notify(struct notifier_block *self, unsigned long action, void *dev)
{
    switch(action)
    {
        case USB_DEVICE_ADD:
            usb_dev_insert(dev);
            break;
        case USB_DEVICE_REMOVE:
            usb_dev_remove(dev);
            break;
        default:
            break;
    }
    
    return 0;
}


static struct notifier_block usb_notify = {
    .notifier_call = notify,
};


static int __init cryptousb_init(void) 
{

    g_task = kthread_create(kthread_read, NULL, "kthread_read");
	if (!IS_ERR(g_task))
	{
		// kthread_read doesn't wait for a termination event so we need to take a reference before
		// the thread has a chance to run as the thread terminates with do_exit() that calls put_task_struct()
		get_task_struct(g_task);
		wake_up_process(g_task);
	}	
    usb_register_notify(&usb_notify);
    printk(KERN_INFO "CRYPTOUSB: cryptousb module loaded\n");
    return 0;
}


static void __exit cryptousb_exit(void) 
{
    usb_unregister_notify(&usb_notify);
    printk(KERN_INFO "CRYPTOUSB: cryptousb module unloaded\n");
}


module_init(cryptousb_init);
module_exit(cryptousb_exit);