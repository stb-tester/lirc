/**
 *  irpipe - lirc test device.
 *
 * This is mostly an implementation of a traditional kernel fifo
 * device. Ihe purpose is to provide the same interface as the
 * kernel LIRC drivers.
 *
 * The driver creates some (default 2) devices which can be read and
 * written as a regular fifos. The udev rule can be used to let udev
 * set up the devices /dev/irpipe[0-9].
 *
 * The differences to a regular kernel fifo:
 *
 *    - A subset of the LIRC ioctl commands are supported, see
 *      irpipe_ioctl().
 *    - If the readers goes away during write(2) this fifo blocks
 *      until a reader is back instead of generating a EPIPE error.
 *    - Some extra ioctl commands for testing are in irpipe.h
 *    - No asynchronous mechanisms.
 *    - The select()/poll() support is only implemented on the
 *      reading side. Writing will always block.
 *    - The device does not report itself as a fifo when using
 *      stat().
 *    - Read buffer needs to be at least 2048 bytes (why?)
 *
 * The LIRC attributes reflected by the related ioctls are reset when
 * the device is opened for write.
 *
 * Parameters include nr of devices, buffer size and debug logging. See
 * the 'modinfo irpipe.ko' output.
 *
 * See:
 *    -  http://lwn.net/Kernel/LDD3
 *    -  fifo(7)
 */

#include <asm/ioctls.h>
#include <asm/current.h>
#include <asm-generic/ioctl.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#ifdef USE_BUNDLED_LIRC_H
#include <media/lirc.h>
#else
#include <linux/lirc.h>
#endif

#include "irpipe.h"


/** Name of device created under /sys/class. */
#define DEVICENAME      "irpipe"

/** Default nr of created minor devices, the first being 0. */
#define NR_OF_DEVICES   2

/** Default main buffer size. */
#define DEFAULT_BUFSIZE 8196


#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#define MAX(a, b)       ((a) > (b) ? (a) : (b))

#define IR_WARN(fmt, args ...) \
	pr_warn("irpipe (%d): " fmt "\n", current->pid, ## args)

#define IR_INFO(fmt, args ...) \
	pr_info("irpipe (%d): " fmt "\n", current->pid, ## args)

#define IR_DEBUG(fmt, args ...) \
	{ if (debug) \
		  IR_INFO(fmt, ## args); }


const struct file_operations fops;

/** Persistent data structure for each allocated device. */
struct ir_device {
	spinlock_t		mutex;		  /**< Fifo guard. */
	char*			fifo;		  /**< Main buffer area. */
	unsigned int		tail;		  /**< Next byte to fetch */
	unsigned int		head;		  /**< Next pos to fill */
	atomic_t		size;		  /**< Fifo size */
	wait_queue_head_t	wait_for_reader;
	wait_queue_head_t	wait_for_writer;
	atomic_t		readers;
	atomic_t		writers;
	__u32			features;	  /**< LIRC_GET_FEATURES */
	__u32			rec_mode;	  /**< LIRC_GET_REC_MODE */
	__u32			send_mode;	  /**< LIRC_GET_SEND_MODE */
	__u32			code_length;	  /**< LIRC_GET_LENGTH */
};


/** Size of internal buffer (parameter). */
static int buffsize = DEFAULT_BUFSIZE;

/** Number of allocated devices 0 < nr_of_devices <= 10 (parameter). */
static int nr_of_devices = NR_OF_DEVICES;

/** Enable/disable debug logging (parameter). */
static bool debug = 1;

/** Persistent data for each device. */
static struct ir_device* ir_devices;

static int dev_major;           /**< Allocated major device number. */
static struct cdev* ir_cdev;    /**< Opaq handle to kernel device. */
static struct class* class;     /**< /sys/class struct, set by init */


/** Get available space in fifo (unlocked). */
static inline size_t fifo_get_space(struct ir_device* d)
{
	return buffsize - atomic_read(&d->size) - 1;
}


/** True if there is some available space in fifo (unlocked). */
static bool fifo_have_space(struct ir_device* device)
{
	return fifo_get_space(device) > 0;
}


/** True if there is some data available in fifo (unlocked).*/
static bool fifo_have_data(struct ir_device* device)
{
	return atomic_read(&device->size) > 0;
}


/**
 * Put at most count bytes from src into the fifo, return bytes written.
 * Needs locked mutex
 */
static size_t fifo_put(struct ir_device* d, const char* src, size_t count)
{
	int i;

	count = MIN(count, fifo_get_space(d));
	for (i = 0; i < count; i += 1) {
		d->fifo[d->head] = *src++;
		d->head = (d->head + 1) % buffsize;
	}
	atomic_add(count, &d->size);
	return count;
}


/**
 * Get at most count bytes from fifo into dest buffer, return bytes read.
 * Needs locked mutex.
 */
static size_t fifo_get(struct ir_device* d, char* dest, size_t count)
{
	int i;

	count = MIN(count, atomic_read(&d->size));
	for (i = 0; i < count; i += 1) {
		*dest++ = d->fifo[d->tail];
		d->tail = (d->tail + 1) % buffsize;
	}
	atomic_sub(count, &d->size);
	return count;
}


/** True if opened for write (and not closed) by at least one process.  */
static bool have_writers(struct ir_device* device)
{
	return atomic_read(&device->writers) > 0;
}


/** True if opened for read (and not closed) by at least one process.  */
static bool have_readers(struct ir_device* device)
{
	return atomic_read(&device->readers) > 0;
}


/**
 * File reset when opened for write: flush fifo, reset ioctl data
 * Needs mutex lock (unless on module load).
 */
static void ir_device_reset(struct ir_device* d, int minor)
{
	IR_DEBUG("Resetting device %d", minor);
	atomic_set(&d->size, 0);
	d->head = 0;
	d->tail = 0;
	d->features = LIRC_CAN_REC_MODE2 | LIRC_CAN_SEND_PULSE;
	d->send_mode = LIRC_MODE_PULSE;
	d->rec_mode = LIRC_MODE_MODE2;
	d->code_length = 24;
}


/** Module load time initialization, return boolean success.*/
static bool ir_device_init(struct ir_device* d, int minor)
{
	memset(d, 0, sizeof(struct ir_device));
	d->fifo = kmalloc(buffsize, GFP_KERNEL);
	if (d->fifo == NULL)
		return 0;
	atomic_set(&d->readers, 0);
	atomic_set(&d->writers, 0);
	spin_lock_init(&d->mutex);
	init_waitqueue_head(&d->wait_for_reader);
	init_waitqueue_head(&d->wait_for_writer);
	ir_device_reset(d, minor);
	return 1;
}


/** Dealloc what ir_device_init() created. */
static void ir_device_destroy(struct ir_device* d)
{
	kfree(d->fifo);
}


/** Unless cond(device) is true, invoke schedule() and cleanup. */
static void cond_wait(bool			(*cond)(struct ir_device*),
		      wait_queue_head_t*	queue,
		      struct ir_device*		device)
{
	wait_queue_t my_wait;

	init_wait(&my_wait);
	prepare_to_wait(queue, &my_wait, TASK_INTERRUPTIBLE);
	if (!cond(device))
		schedule();
	finish_wait(queue, &my_wait);
}


/** Do the open() work for devices opened O_RDWR. */
static int open_read_write(struct ir_device* dev, struct file* filp, int minor)
{
	IR_DEBUG("open_read/write, size: %d", atomic_read(&dev->size));
	spin_lock(&dev->mutex);
	ir_device_reset(dev, minor);
	atomic_inc(&dev->writers);
	atomic_inc(&dev->readers);
	spin_unlock(&dev->mutex);
	wake_up_interruptible(&dev->wait_for_reader);
	return 0;
}


/** Do the open() work for devices opened O_WRONLY. */
static int open_write(struct ir_device* dev, struct file* filp, int minor)
{
	IR_DEBUG("open_write, readers: %d, size: %d",
		 atomic_read(&dev->readers),
		 atomic_read(&dev->size));
	atomic_inc(&dev->writers);
	spin_lock(&dev->mutex);
	while (!have_readers(dev)) {
		spin_unlock(&dev->mutex);
		if (filp->f_flags & O_NONBLOCK)
			return -ENXIO;
		cond_wait(have_readers, &dev->wait_for_reader, dev);
		if (signal_pending(current)) {
			IR_DEBUG("signals pending on open - ERESTARTSYS");
			return -ERESTARTSYS;
		}
		spin_lock(&dev->mutex);
	}
	ir_device_reset(dev, minor);
	spin_unlock(&dev->mutex);
	wake_up_interruptible(&dev->wait_for_writer);
	IR_DEBUG("device opened for write");
	return 0;
}


/** Do the open() work for devices opened O_RDONLY. */
static int open_read(struct ir_device* dev, struct file* filp)
{
	IR_DEBUG("open_read, writers: %d", atomic_read(&dev->writers));
	atomic_inc(&dev->readers);
	while (!have_writers(dev)) {
		if (filp->f_flags & O_NONBLOCK)
			return 0;
		cond_wait(have_writers, &dev->wait_for_writer, dev);
		if (signal_pending(current))
			return -ERESTARTSYS;
	}
	wake_up_interruptible(&dev->wait_for_reader);
	IR_DEBUG("device opened for read");
	return 0;
}


/** Kernel side of open(2). */
static int irpipe_open(struct inode* inode, struct file* filp)
{
	const int minor = iminor(inode);

	if (minor > nr_of_devices - 1)
		return -ENODEV;
	filp->private_data = &ir_devices[minor];
	if (filp->f_mode & FMODE_READ && filp->f_mode & FMODE_WRITE)
		return open_read_write(&ir_devices[minor], filp, minor);
	else if (filp->f_mode & FMODE_WRITE)
		return open_write(&ir_devices[minor], filp, minor);
	else if (filp->f_mode & FMODE_READ)
		return open_read(&ir_devices[minor], filp);
	else
		return -EINVAL;
}


/** Kernel side of close(2). */
static int irpipe_close(struct inode* inode, struct file* filp)
{
	const int minor = iminor(inode);
	struct ir_device* const dev = (struct ir_device*)filp->private_data;

	IR_DEBUG("Closing, size: %d", atomic_read(&dev->size));
	if (filp->f_mode & FMODE_WRITE) {
		atomic_dec(&dev->writers);
		wake_up_interruptible(&dev->wait_for_writer);
	}
	if (filp->f_mode & FMODE_READ) {
		atomic_dec(&dev->readers);
		wake_up_interruptible(&dev->wait_for_reader);
	}
	IR_DEBUG("%s/%s device %d closed",
		 filp->f_mode & FMODE_READ ? "read" : "-",
		 filp->f_mode & FMODE_WRITE ? "write" : "-",
		 minor);
	return 0;
}


/** kernel side of userspace read(2). */
static ssize_t
irpipe_read(struct file* filp, char* buff, size_t length, loff_t* ppos)
{
	struct ir_device* const dev = (struct ir_device*)filp->private_data;
	char kbuff[buffsize];
	int bytes;

	spin_lock(&dev->mutex);
	while (!fifo_have_data(dev) || !have_writers(dev)) {
		spin_unlock(&dev->mutex);
		if (atomic_read(&dev->writers) <= 0)
			return 0;
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		cond_wait(fifo_have_data, &dev->wait_for_writer, dev);
		if (signal_pending(current)) {
			IR_DEBUG("Signals pending on read- ERESTARTSYS");
			return -ERESTARTSYS;
		}
		spin_lock(&dev->mutex);
	}
	bytes = fifo_get(dev, kbuff, length);
	spin_unlock(&dev->mutex);
	bytes -= copy_to_user(buff, kbuff, bytes);
	wake_up_interruptible(&dev->wait_for_reader);
	return bytes;
}


/** Kernel side of userspace write(2). */
static ssize_t
irpipe_write(struct file* filp, const char* buff, size_t length, loff_t* ppos)
{
	struct ir_device* const dev = (struct ir_device*)filp->private_data;
	char kbuff[buffsize];
	unsigned int written;

	length = MIN(length, buffsize - 1);
	length -= copy_from_user(kbuff, buff, length);
	spin_lock(&dev->mutex);
	while (!fifo_have_space(dev) || !have_readers(dev)) {
		spin_unlock(&dev->mutex);
		if (!have_readers(dev)) {
			IR_DEBUG("No readers on write: blocking.");
			cond_wait(have_readers, &dev->wait_for_reader, dev);
		}
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		cond_wait(fifo_have_space, &dev->wait_for_reader, dev);
		if (signal_pending(current)) {
			IR_DEBUG("Write: signals pending on write: ERESTARTSYS.");
			return -ERESTARTSYS;
		}
		spin_lock(&dev->mutex);
	}
	written = fifo_put(dev, kbuff, length);
	spin_unlock(&dev->mutex);

	wake_up_interruptible(&dev->wait_for_writer);
	return written;
}


/** Kernel side of userspace ioctl(2). */
long irpipe_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
	struct ir_device* const dev = (struct ir_device*)filp->private_data;
	void __user *argp = (void __user *)arg;
	long r = 0;

	IR_DEBUG("Running ioctl cmd %u (0x%x), arg: %lu\n", cmd, cmd, arg);
	spin_lock(&dev->mutex);
	switch (cmd) {
	case FIOQSIZE:
		r = atomic_read(&dev->size);
		IR_DEBUG("Handling FIOQSIZE, size: %ld", r);
		break;
	case LIRC_GET_FEATURES:
		r = dev->features;
		IR_DEBUG("Handling LIRC_GET_FEATURES, data: 0x%lx", r);
		if (arg == 0)
			break;
		if (copy_to_user(argp, &dev->features, sizeof(__u32)) != 0)
			r = -EFAULT;
		break;
	case LIRC_GET_SEND_MODE:
		r = dev->send_mode;
		break;
	case LIRC_GET_REC_MODE:
		r = dev->rec_mode;
		break;
	case LIRC_GET_LENGTH:
		r = dev->code_length;
		break;
	case LIRC_SET_FEATURES:
		dev->features = (__u32)arg;
		r = 0;
		break;
	case LIRC_SET_SEND_MODE:
		dev->send_mode = (__u32)arg;
		r = 0;
		break;
	case LIRC_SET_REC_MODE:
		dev->rec_mode = (__u32)arg;
		r = 0;
		break;
	case LIRC_SET_LENGTH:
		dev->code_length = (__u32)arg;
		r = 0;
		break;
	default:
		r = -ENOTTY;
		break;
	}
	spin_unlock(&dev->mutex);
	return r;
}


/** Kernel side of select() and poll(). */
static unsigned int irpipe_poll(struct file *filp, poll_table *wait)
{
	int mask = 0;
	struct ir_device* const dev = (struct ir_device*)filp->private_data;

	poll_wait(filp, &dev->wait_for_writer, wait);
	if (fifo_have_data(dev))
		mask = POLLIN | POLLRDNORM;
	return mask;
}


/** Module load. */
int irpipe_init(void)
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
void irpipe_exit(void)
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
	.open		= irpipe_open,
	.write		= irpipe_write,
	.unlocked_ioctl = irpipe_ioctl,
	.read		= irpipe_read,
	.release	= irpipe_close,
	.poll           = irpipe_poll,
};


MODULE_AUTHOR("Alec Leamas <leamas at gmail dot com>");
MODULE_DESCRIPTION("Simple pipe supporting some lirc ioctl commands");
MODULE_LICENSE("GPL");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, " Enable (default off) debug logging");

module_param(buffsize, int, S_IRUGO);
MODULE_PARM_DESC(buffsize, "Internal buffer size");

module_param(nr_of_devices, int, S_IRUGO);
MODULE_PARM_DESC(nr_of_devices, "Number of minor devices");

module_init(irpipe_init);
module_exit(irpipe_exit);
