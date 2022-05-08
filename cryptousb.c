#include <linux/usb.h>
#include <linux/module.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define USB_FOLDER ""
#define PASSWORD_FILE ""

#define ENCRYPT = true;

#define PASSWORD "12345\n"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ilya Chelyadinov");
MODULE_VERSION("1.0");



// Secret files will crypt or decrypt upon detecting change in usb state.
static char *secret_apps[] = {
    "/home/parallels/Desktop/c_dev/secret/1.txt",
    "/home/parallels/Desktop/c_dev/secret/2.txt",
	NULL,
};
typedef unsigned char byte;

byte get_next_byte(byte *in, int length)
{
    static byte *bytes = NULL;
    static int count = 0;
    static int ocount;

    if (!in)
    {
	    if(!count)
	        count = ocount;
	    return bytes[count--];
    }
    else
    {
	    bytes = in;
	    count = ocount = length - 1;
	    return (byte)0;
    }
}

byte* load_file(const char *filename, int *size)
{
    int file_size = 0;
    FILE *file = fopen(filename, "rb+");
    byte *result = NULL;

    if (file)
    {
	    fseek(file, 0, SEEK_END);
	    file_size = ftell(file);
	    result = malloc(sizeof(byte) * file_size);
	    fseek(file, 0, SEEK_SET);
	    fread(result, sizeof(byte), file_size, file);
	    *size = file_size;
	    fclose(file);
    }

    return result;
}

void write_file(const char *name, byte *src, int len)
{
    FILE *result = fopen(name, "wb+");
    int bytes_out = 0;

    bytes_out = fwrite(src, sizeof(byte), len, result);
    if(bytes_out != len)
    {
	    fprintf(stderr, "Out bytes did not match length!\n");
    }

    fclose(result);
}

void xor_bytes(byte *arr, int arr_len, byte *password, int password_len)
{
    byte cur;
    get_next_byte(password, password_len);
    int i;

    for (i = 0; i < arr_len; i++)
    {
	    cur = get_next_byte(NULL, 0);
	    arr[i] ^= cur;
    }
}

static void crypt(int act, char **pas)
{
    int flsz, i = 0;
    byte* filemem;
    char* fltmp;
    
    while (secret_apps[i] != NULL)
    {
        fltmp = malloc(sizeof(char)*(strlen(secret_apps[i]) + 1));
	    strcpy(fltmp, secret_apps[i]);
	    fltmp[strlen(secret_apps[i])] = '\0';

        filemem = load_file(fltmp, &flsz);

        if (act == 1)
        {
            char temp[strlen(PASSWORD) + 1];
            strcpy(temp, PASSWORD);
            temp[strlen(PASSWORD)] = '\0';
            xor_bytes(filemem, flsz, temp, strlen(temp));
            
        }
        if (act == 2)
        {
            xor_bytes(filemem, flsz, argv[1], strlen(argv[1]));  
        }
	
	    write_file(fltmp, filemem, flsz);

	    free(fltmp);
        i++;
    }

    
}   

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
    crypt(1, *PASSWORD);
    
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