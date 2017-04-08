/**
 *  irlog - logging wrapper for /dev/lirc devices.
 *
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ktime.h>
#include <linux/poll.h>
#include <linux/slab.h>

#define PULSE_BIT       0x01000000
#define PULSE_MASK      0x00FFFFFF


/** Name of device created under /sys/class. */
#define DEVICENAME      "irlog"

/** Default nr of created minor devices, the first being 0. */
#define NR_OF_DEVICES   2

/** Default device logged a. k a. device_to_log parameter. */
#define CHILD_DEVICE    "/dev/lirc0"


#define IR_WARN(fmt, args ...) \
	pr_warn("irlog (%d): " fmt "\n", current->pid, ## args)

#define IR_INFO(fmt, args ...) \
	pr_info("irlog (%d): " fmt "\n", current->pid, ## args)

#define IR_DEBUG(fmt, args ...) \
	{ if (debug) \
		  IR_INFO(fmt, ## args); }


const struct file_operations fops;

/** Persistent data structure for each allocated device. */
struct ir_device {
	struct file* filp;               /**< logged file */
	ktime_t start;
};


/** Number of allocated devices 0 < nr_of_devices <= 10 (parameter). */
static int nr_of_devices = NR_OF_DEVICES;

/** Enable/disable debug logging (parameter). */
static bool debug = 1;

/** Default device to log a. k. a. device_to_log parameter */
static char* device_to_log = CHILD_DEVICE;

/** Persistent data for each device. */
static struct ir_device* ir_devices;

static int dev_major;           /**< Allocated major device number. */
static struct cdev* ir_cdev;    /**< Opaque handle to kernel device. */
static struct class* class;     /**< /sys/class struct, set by init */


/** Module load time initialization, return boolean success.*/
static bool ir_device_init(struct ir_device* dev, int minor)
{

	IR_DEBUG("Loading device %d", minor);
	memset(dev, 0, sizeof(struct ir_device));
	return 1;
}


/** Dealloc what ir_device_init() created. */
static void ir_device_destroy(struct ir_device* d) {}



/** VFS layer open(). */
static struct file* file_open(const char* path, int flags, int rights)
{
	struct file* filp = NULL;
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);
	set_fs(oldfs);
	return filp;
}


/** VFS layer read(). */
static int
file_read(struct file* file, char* data, size_t size, loff_t* offset)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_read(file, data, size, offset);

	set_fs(oldfs);
	return ret;
}


/** VFS layer write(). */
static int
file_write(struct file* file, const char* data, size_t size, loff_t* offset)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_write(file, data, size, offset);

	set_fs(oldfs);
	return ret;
}


/** Kernel side of open(2). */
static int irlog_open(struct inode* inode, struct file* file)
{
	const int minor = iminor(inode);
	struct ir_device* dev;
	int err;

	IR_DEBUG("Opening file %s", device_to_log)
	if (minor > nr_of_devices - 1)
		return -ENODEV;
	file->private_data = &ir_devices[minor];
	dev = (struct ir_device*)file->private_data;
	dev->filp = file_open(device_to_log, O_RDWR, 0);
	if (IS_ERR(dev->filp)) {
		err = PTR_ERR(dev->filp);
		dev->filp = NULL;
		IR_WARN("Error opening %s: %d", device_to_log, err);
		return err;
	}
	dev->start = ktime_get();
	return 0;
}


/** Kernel side of close(2). */
static int irlog_close(struct inode* inode, struct file* file)
{
	struct ir_device* const dev = (struct ir_device*) file->private_data;

	IR_DEBUG("Closing file");
	if (dev->filp)
		filp_close(dev->filp, NULL);
	return 0;
}


/** kernel side of userspace read(2). */
static ssize_t
irlog_read(struct file* file, char* buff, size_t length, loff_t* ppos)
{
	struct ir_device* const dev = (struct ir_device*)file->private_data;
	char ints[64];
	char int_[16];
	uint32_t u32;
	int r;
	int i;
	int delta;

	r = file_read(dev->filp, buff, length, ppos);
	ints[0] = '\0';
	for (i = 0; i < r && i < 16; i += 4) {
		memcpy(&u32, &buff[i], 4);
		snprintf(int_, sizeof(int_), "%08x ", u32);
		strcat(ints, int_);
	}
	if (r > 16)
		strcat(ints,  "...");
	delta = ktime_to_ns(ktime_sub(ktime_get(), dev->start)) / 1000;
	IR_DEBUG("[%d] %d bytes of %d read: %s",
		 delta, r, (int) length, ints);
	return r;
}


/** Kernel side of userspace write(2). */
static ssize_t
irlog_write(struct file* file, const char* buff, size_t length, loff_t* ppos)
{
	struct ir_device* const dev = (struct ir_device*)file->private_data;
	int r;
	int delta;
	char ints[1024];
	char int_[16];
	uint32_t u32;
	int i;

	r = file_write(dev->filp, buff, length, ppos);
	delta = ktime_to_ns(ktime_sub(ktime_get(), dev->start)) / 1000;
	IR_DEBUG("[%d] %d bytes of %d (%d ints) written",
		 delta, r, (int) length, (int) length / 4);
	ints[0] = '\0';
	for (i = 0; i < r && i < 128; i += 4) {
		memcpy(&u32, &buff[i], 4);
		snprintf(int_, sizeof(int_), "%d ", u32);
		strcat(ints, int_);
	}
	IR_DEBUG("Data: %s", ints);

	return r;
}


/** Kernel side of userspace ioctl(2). */
long irlog_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
	struct ir_device* const dev = (struct ir_device*) file->private_data;
	int delta;

	delta = ktime_to_ns(ktime_sub(ktime_get(), dev->start)) / 1000;
	IR_DEBUG("[%d] ioctl file, cmd: %x, arg: %lx", delta, cmd, arg);
	return dev->filp->f_op->unlocked_ioctl(dev->filp, cmd, arg);
}


/** Kernel side of select() and poll(). */
static unsigned int irlog_poll(struct file* file, poll_table* wait)
{
	struct ir_device* const dev = (struct ir_device*) file->private_data;
	int r;

	r = dev->filp->f_op->poll(dev->filp, wait);
	IR_DEBUG("poll, result: %x", r);
	return r;
}


/** Module load. */
int irlog_init(void)
{
	dev_t dev;
	int r;
	int i;

	IR_DEBUG("start loading");
	ir_devices = kmalloc_array(nr_of_devices,
				   sizeof(struct ir_device),
				   GFP_KERNEL);
	if (ir_devices == NULL)
		return -ENOMEM;
	for (i = 0; i < nr_of_devices; i += 1) {
		if (!ir_device_init(&ir_devices[i], i)) {
			IR_WARN("Cannot allocate memory (giving up).");
			return -ENOMEM;
		}
	}
	r = alloc_chrdev_region(&dev, 0, nr_of_devices, DEVICENAME);
	if (r < 0) {
		IR_WARN("failed to allocate major number");
		return r;
	}
	dev_major = MAJOR(dev);

	class = class_create(THIS_MODULE, DEVICENAME);
	for (i = 0; i < nr_of_devices; i += 1)
		device_create(class, NULL, MKDEV(dev_major, i), NULL,
			      DEVICENAME "%d", i);
	ir_cdev = cdev_alloc();
	cdev_init(ir_cdev, &fops);
	ir_cdev->owner = THIS_MODULE;
	r = cdev_add(ir_cdev, dev, 2);
	if (r < 0) {
		IR_WARN("Cannot add kernel device\n");
		return r;
	}
	IR_INFO("Loading complete, major device nr:  %d", dev_major);
	return 0;
}


/** Module unload. */
void irlog_exit(void)
{
	int i;

	for (i = 0; i < nr_of_devices; i += 1) {
		ir_device_destroy(&ir_devices[i]);
		device_destroy(class, MKDEV(dev_major, i));
	}
	class_destroy(class);
	cdev_del(ir_cdev);
	unregister_chrdev_region(MKDEV(dev_major, 0), nr_of_devices);
	kfree(ir_devices);
	IR_DEBUG("Unloading device %d", dev_major);
}


const struct file_operations fops = {
	.owner		= THIS_MODULE,
	.open		= irlog_open,
	.write		= irlog_write,
	.unlocked_ioctl = irlog_ioctl,
	.read		= irlog_read,
	.release	= irlog_close,
	.poll           = irlog_poll
};


MODULE_AUTHOR("Alec Leamas <leamas at gmail dot com>");
MODULE_DESCRIPTION("Simple logging character device wrapper");
MODULE_LICENSE("GPL");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, " Enable (default on) debug logging");

module_param(nr_of_devices, int, S_IRUGO);
MODULE_PARM_DESC(nr_of_devices, "Number of minor devices");

module_param(device_to_log, charp, S_IRUGO);
MODULE_PARM_DESC(device_to_log, "The device logged and wrapped by irlog");

module_init(irlog_init);
module_exit(irlog_exit);
