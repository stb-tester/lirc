// User-space substitute for HIDRAW on MacOSX
//
// osx_usbraw.c - Copyright (C) 2014 - Eric Anderson
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//


// The routines in the file the use IOHID Library to read IR receiver USB events
// and forward to lircd via a file descriptor pipe. Ideally this would look more/less
// identical to how hidraw works on Linux. However, unix pipes are buffered, while
// hidraw only returns one report at a time. To avoid issues, we use delimiters.
// For a first-pass, presume HID reports are shorter than 256 bytes, so just push
// (uchar) len + data through the pipe. The reading side must handle this.
//
// For threading we use pthread_create() to launch a listener thread that
// employs a standard OSX CFRunLoopRun() mechanism for processing USB events.
//
// Note that thread mutex locking is used to ensure that the shutdown can make
// a correct call to CRFunLoopStop() with a proper reference.

#include <pthread.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/hid/IOHIDLib.h>

#include "lirc_driver.h"


static int sonyir_init(void);
static int sonyir_deinit(void);
static char* sonyir_rec(struct ir_remote* remotes);
static int sonyir_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);


enum {
	RPT_UNKNOWN = -1,
	RPT_NO = 0,
	RPT_YES = 1,
};

struct hiddev_event {
	unsigned	hid;
	signed int	value;
};


#define USB_VENDOR_ID_SONY               0x054c
#define USB_DEVICE_ID_SONY_IR_RECEIVER   0x00d4
#define USB_DEVICE_ID_SONY_DS3           0x0268


static const logchannel_t logchannel = LOG_DRIVER;

// Variables copied from hiddev.c
int pre_code_length = 32;
int main_code_length = 32;
unsigned int pre_code;
signed int main_code = 0;
struct timeval start, last;

int repeat_state = RPT_UNKNOWN;

const struct driver hw_sony_osx = {
	.name		= "sonyir",
	.device		= "",                   // not used
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 32,
	.init_func	= sonyir_init,
	.deinit_func	= sonyir_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= sonyir_rec,
	.decode_func	= sonyir_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "MacOSX driver for hiddev devices",
	.device_hint    = "auto",
};


static int osx_iousb_open(void);
static void osx_iousb_shutdown(void);

static int
sonyir_init(void)
{
	log_info("Initializing via 'osx_iousb_open()'...");

	// Launch thread to communicate with USB device using kIOUSBDevice class.
	// Needed since OSX doesn't have the hidraw device.
	drv.fd = osx_iousb_open();
	if (drv.fd < 0)
		return 0;

	return 1;
}


int sonyir_deinit(void)
{
	if (drv.fd != -1) {
		log_info("Closing sonyir...");
		osx_iousb_shutdown();

		close(drv.fd);
		drv.fd = -1;
	}
	return 1;
}

int sonyir_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	log_trace("sonyir_decode");

	if (!map_code(remote, ctx, pre_code_length, pre_code, main_code_length, main_code, 0, 0))
		return 0;

	log_trace("lirc code: 0x%X", ctx->code);

	map_gap(remote, ctx, &start, &last, 0);
	/* override repeat */
	switch (repeat_state) {
	case RPT_NO:
		ctx->repeat_flag = 0;
		break;
	case RPT_YES:
		ctx->repeat_flag = 1;
		break;
	default:
		break;
	}

	return 1;
}



// Hack. Remap DS3 events to sony remote codes
#define KEY_UP                  0x10074
#define KEY_DOWN                0x10075
#define KEY_LEFT                0x10034
#define KEY_RIGHT               0x10033
#define KEY_ENTER               0x10065
#define X_KEY_RETURN            0x1a490e

#define KEY_VOLUMEUP            0x10012
#define KEY_VOLUMEDOWN          0x10013
#define KEY_MUTE                0x10014
#define KEY_MENU                0x10060
#define KEY_PAUSE               0x1ae219
#define KEY_DISPLAY             0x1ae241
#define KEY_POWER               0x1ae215
#define KEY_RED                 0x1ae267
#define KEY_BLUE                0x1ae266
#define X_KEY_SAT_POWER         0x170515


int
sony_ds_remap(uint8_t* msg)
{
	// Filter flaky 0x99 (HID report issue?)
	if (msg[3] == 0x99)
		return 0;

#if DEBUG
	if (msg[0] || msg[1] || msg[2] || msg[3])
		printf("sony_ds_remap(): %02x %02x %02x %02x\n", msg[0], msg[1], msg[2], msg[3]);

#endif

	// Left stick
	if (msg[3] & 0x01)
		return KEY_UP;
	else if (msg[3] & 0x04)
		return KEY_DOWN;
	else if (msg[3] & 0x02)
		return KEY_RIGHT;
	else if (msg[3] & 0x08)
		return KEY_LEFT;

	// Left pad
	else if (msg[0] & 0x10)
		return KEY_UP;
	else if (msg[0] & 0x20)
		return KEY_DOWN;
	else if (msg[0] & 0x40)
		return KEY_RIGHT;
	else if (msg[0] & 0x80)
		 return KEY_LEFT;
	else if (msg[0] & 0x02)
		return KEY_RED;
	else if (msg[0] & 0x04)
		return KEY_BLUE;

	// Trigger
	else if (msg[1] & 0x40)
		return KEY_ENTER;
	else if (msg[1] & 0x20)
		return X_KEY_RETURN;
	else if (msg[1] & 0x10)
		return KEY_MENU;
	else if (msg[1] & 0x80)
		return KEY_DISPLAY;

	// Volume
	else if (msg[1] & 0x08)
		return KEY_VOLUMEUP;
	else if (msg[1] & 0x04)
		return KEY_VOLUMEDOWN;

	// Pause
	else if (msg[0] & 0x08)
		return KEY_PAUSE;

	// Power
	else if (msg[2] & 0x01)
		return KEY_POWER;
	else if (msg[0] & 0x01)
		return X_KEY_SAT_POWER;

	// Not currently mapped
	else
		return 0;
}



// [Only difference with hiddev_decode is reading of length delimiter.]
static char* sonyir_rec(struct ir_remote* remotes)
{
	struct hiddev_event ev;
	int rd;
	unsigned char rd_len = 255;
	unsigned char msg[16];

	log_trace("sonyir_rec");

	// Read length delimiter from socket. If we were accessing the device
	// directly, this would be a bit easier.
	rd = read(drv.fd, &rd_len, 1);
	if (rd != 1)
		return 0;

	// Sony IR receiver has 3 reports:
	//   0x01 - 5B
	//   0x02 - 1B
	//   0x03 - 8B
	//
	rd = read(drv.fd, msg, rd_len);

	switch (rd_len) {
	// Sony DS3 - 4B manufactured report
	case 0x4:
		ev.value = sony_ds_remap(msg);
		if (ev.value == 0)
			return 0;
		break;

	// Sony IR Receiver - 6B report
	case 0x6:
		if (msg[0] != 0x1)
			return 0;

		// Ignore release message
		if ((msg[2] & 0x80) == 0x80)
			return 0;

		// Construct event
		ev.value = (msg[3] << 16) | (msg[4] << 8) | ((msg[2] & 0x7f) << 0);
		break;

	// Discard
	default:
		return 0;
	}


	pre_code_length = 0;
	pre_code = 0;
	main_code = ev.value;
	repeat_state = RPT_NO;

	return decode_all(remotes);
}


/////////////////////////////
// Begin OSX-Specific Code //
/////////////////////////////

#define BUF_SIZE 256

struct device_params {
	int	vendor_id;
	int	device_id;
};


static IOHIDDeviceRef setup_hid_thread(struct device_params* params);

static void* osx_usb_thread(void* data);

static void usb_hid_report_callback(void* context, IOReturn result,
				    void* sender, IOHIDReportType report_type,
				    uint32_t report_id, uint8_t* report_data,
				    CFIndex report_length);


// Message buffer
static UInt8 g_msg[BUF_SIZE];

// Local variables
static int fds[2];
#define READ_FD  fds[0]
#define WRITE_FD fds[1]

// Thread IPC
static int child_run_loop_mutex_inited = 0;
static pthread_mutex_t child_run_loop_mutex;
static CFRunLoopRef child_run_loop;

static enum {
	STATE_HALT,
	STATE_RUN,
} child_run_state;


void
osx_iousb_shutdown()
{
	// Request child to stop
	pthread_mutex_lock(&child_run_loop_mutex);
	child_run_state = STATE_HALT;
	if (child_run_loop) {
		CFRunLoopStop(child_run_loop);
		child_run_loop = NULL;
	}
	pthread_mutex_unlock(&child_run_loop_mutex);
}

int
osx_iousb_open()
{
	int result;
	int threadError;
	int returnVal;

	if (!child_run_loop_mutex_inited) {
		pthread_mutex_init(&child_run_loop_mutex, NULL);
		child_run_loop_mutex_inited = true;
	}

	// Clean-up (shouldn't be needed!)
	osx_iousb_shutdown();

	// Ask child to start
	pthread_mutex_lock(&child_run_loop_mutex);
	child_run_state = STATE_RUN;
	pthread_mutex_unlock(&child_run_loop_mutex);

	// Create a pipe
	result = pipe(fds);
	if (result < 0) {
		log_error("pipe() returned %d\n", result);
		return result;
	}

	// Launch a new thread
	pthread_attr_t attr;
	pthread_t posixThreadID;

	returnVal = pthread_attr_init(&attr);
	assert(!returnVal);
	returnVal = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	assert(!returnVal);

	threadError = pthread_create(&posixThreadID, &attr, osx_usb_thread, NULL);
	if (threadError != 0) {
		log_error("thread error???\n");
		return threadError;
	}

	returnVal = pthread_attr_destroy(&attr);
	assert(!returnVal);

	return READ_FD;
}



// Thread for monitoring USB+HID interfaces.
//
void*
osx_usb_thread(void* data)
{
	IOHIDDeviceRef irr_device = NULL;
	IOHIDDeviceRef ds3_device = NULL;
	bool do_run;

	struct device_params sony_irr_params;
	struct device_params sony_ds3_params;

	sony_irr_params.vendor_id = USB_VENDOR_ID_SONY;
	sony_irr_params.device_id = USB_DEVICE_ID_SONY_IR_RECEIVER;

	sony_ds3_params.vendor_id = USB_VENDOR_ID_SONY;
	sony_ds3_params.device_id = USB_DEVICE_ID_SONY_DS3;

	// TODO - Consider merging these into a unified function.
	irr_device = setup_hid_thread(&sony_irr_params);
	ds3_device = setup_hid_thread(&sony_ds3_params);

	// Launch run loop
	do_run = false;
	pthread_mutex_lock(&child_run_loop_mutex);
	if (child_run_state == STATE_RUN) {
		do_run = true;
		child_run_loop = CFRunLoopGetCurrent();
	}
	pthread_mutex_unlock(&child_run_loop_mutex);

	if (do_run)
		CFRunLoopRun();

	log_info("USB thread exiting...\n");

	// Close devices
	if (irr_device)
		IOHIDDeviceClose(irr_device, kIOHIDOptionsTypeNone);
	if (ds3_device)
		IOHIDDeviceClose(ds3_device, kIOHIDOptionsTypeNone);

	close(WRITE_FD);

	pthread_exit(NULL);
}


CFMutableDictionaryRef
my_create_matching_dictionary(SInt32 vendor, SInt32 device)
{
	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);

	CFDictionarySetValue(dict, CFSTR(kIOHIDVendorIDKey),
			     CFNumberCreate(kCFAllocatorDefault,
			     kCFNumberIntType, &vendor));
	CFDictionarySetValue(dict, CFSTR(kIOHIDProductIDKey),
			     CFNumberCreate(kCFAllocatorDefault,
			     kCFNumberIntType, &device));
	return dict;
}

int32_t
get_int_property(IOHIDDeviceRef device, CFStringRef key)
{
	CFTypeRef ref;
	int32_t value;

	ref = IOHIDDeviceGetProperty(device, key);
	if (ref) {
		if (CFGetTypeID(ref) == CFNumberGetTypeID()) {
			CFNumberGetValue((CFNumberRef)ref, kCFNumberSInt32Type, &value);
			return value;
		}
	}
	return 0;
}


// USB HID interface
//
IOHIDDeviceRef
setup_hid_thread(struct device_params* params)
{
	int i;
	IOReturn ret_value;
	IOReturn result;
	CFArrayRef matches = NULL;
	CFSetRef deviceCFSetRef = NULL;
	IOHIDManagerRef manager = NULL;
	IOHIDDeviceRef* hid_device_refs = NULL;
	CFIndex device_count = 0;
	IOHIDDeviceRef device;
	IOHIDDeviceRef my_device;
	UInt16 VID, PID, REL;

	// Set up device matching dictionary
	CFMutableDictionaryRef dict_usb_hid = my_create_matching_dictionary(params->vendor_id, params->device_id);
	CFMutableDictionaryRef dictionaries[] = { dict_usb_hid };

	matches = CFArrayCreate(kCFAllocatorDefault, (const void**)dictionaries, 1, NULL);


	manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
	if (manager == 0) {
		log_error("IOHidManagerCreate() failed\n");
		goto error;
	}

	// Set up device matching
	IOHIDManagerSetDeviceMatchingMultiple(manager, matches);

	// Open HID manager
	ret_value = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
	if (ret_value != 0) {
		log_error("IOHIDManagerOpen() returned %x\n", ret_value);
		goto error;
	}

	// Copy out matching devices
	deviceCFSetRef = IOHIDManagerCopyDevices(manager);
	if (deviceCFSetRef == 0) {
		printf("IOHIDManagerCopyDevices() returned NULL\n");
		goto error;
	}

	device_count = CFSetGetCount(deviceCFSetRef);

	// Get references
	hid_device_refs = (IOHIDDeviceRef*)malloc(sizeof(IOHIDDeviceRef) * device_count);
	if (hid_device_refs == NULL) {
		log_error("malloc failed\n");
		goto error;
	}

	CFSetGetValues(deviceCFSetRef, (const void**)hid_device_refs);
	CFRelease(deviceCFSetRef);
	deviceCFSetRef = NULL;

	for (i = 0; i < device_count; i++) {
		REL = 0;
		my_device = hid_device_refs[i];
		VID = get_int_property(my_device, CFSTR(kIOHIDVendorIDKey));
		PID = get_int_property(my_device, CFSTR(kIOHIDProductIDKey));
		REL = get_int_property(my_device, CFSTR(kIOHIDVersionNumberKey));
		log_info("Found device VID 0x%04X, PID 0x%04X, release %d\n", VID, PID, REL);
	}

	// Open 1st device
	device = hid_device_refs[0];
	result = IOHIDDeviceOpen(device, kIOHIDOptionsTypeSeizeDevice);
	if (result) {
		log_error("IOHIDDeviceOpen() returned %d\n", result);
		goto error;
	}

	IOHIDDeviceRegisterInputReportCallback(device, g_msg, BUF_SIZE, usb_hid_report_callback, params);

	IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

	if (hid_device_refs)
		free(hid_device_refs);
	return device;

error:
	if (matches)
		 CFRelease(matches);
	if (deviceCFSetRef)
		CFRelease(deviceCFSetRef);
	if (hid_device_refs)
		free(hid_device_refs);
	return NULL;
}


// Six-axis hysteresis
#define H_L0   0x40
#define H_L1   0x70
#define H_MED  0x80
#define H_H1   0x90
#define H_H0   0xc0

uint8_t
sixaxis_hysteresis(uint8_t x, uint8_t* last_x)
{
	switch (*last_x) {
	case H_MED:
		if (x >= H_H0)
			return *last_x = H_H0;
		if (x <= H_L0)
			return *last_x = H_L0;
		break;
	case H_H0:
		if (x < H_H1)
			return *last_x = H_MED;
		break;
	case 0x40:
		if (x > H_L1)
			return *last_x = H_MED;
		break;
	}
	return *last_x;
}

uint8_t
sixaxis_encode(uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry)
{
	uint8_t code = 0;

	if (ly == H_L0)
			code |= 0x1;
	if (lx == H_H0)

			code |= 0x2;
	if (ly == H_H0)
			code |= 0x4;
	if (lx == H_L0)
			code |= 0x8;

	if (ry == H_L0)
			code |= 0x10;
	if (rx == H_H0)
			code |= 0x20;
	if (ry == H_H0)
			code |= 0x40;
	if (rx == H_L0)
			code |= 0x80;

	return code;
}

// State variables (FIXME - for multiple devices, include in 'device_params')
uint32_t last_code = 0;
uint8_t last_lx = H_MED, last_ly = H_MED;
uint8_t last_rx = H_MED, last_ry = H_MED;

void
usb_hid_report_callback(void* context, IOReturn result, void* sender, IOHIDReportType report_type,
			uint32_t report_id, uint8_t* report_data, CFIndex report_length)
{
	uint8_t wr_len;
	struct device_params* params = context;

	if (params->device_id == USB_DEVICE_ID_SONY_DS3) {
		// For DS3, translate stick/button events into sonyir codes
		uint8_t msg[4];
		uint8_t lx = sixaxis_hysteresis(report_data[6], &last_lx);
		uint8_t ly = sixaxis_hysteresis(report_data[7], &last_ly);
		uint8_t rx = sixaxis_hysteresis(report_data[8], &last_rx);
		uint8_t ry = sixaxis_hysteresis(report_data[9], &last_ry);

		wr_len = 4;
		msg[0] = report_data[2];
		msg[1] = report_data[3];
		msg[2] = report_data[4];
		msg[3] = sixaxis_encode(lx, ly, rx, ry); // Deposit 'stick' event info into 4th byte

		uint32_t code = (msg[0] << 0) + (msg[1] << 8) + (msg[2] << 16) + (msg[3] << 24);

		// Only send deltas
		if (code != last_code) {
			last_code = code;
			write(WRITE_FD, &wr_len, 1);
			write(WRITE_FD, msg, wr_len);
		}
		return;
	}

	// Only handle reports smaller than 256B
	if (report_length > 255)
		return;
	wr_len = report_length;

	write(WRITE_FD, &wr_len, 1);
	write(WRITE_FD, report_data, wr_len);
}

///////////////////////////
// End OSX-Specific Code //
///////////////////////////

const struct driver* hardwares[] = { &hw_sony_osx,
				     (const struct driver*)NULL };
