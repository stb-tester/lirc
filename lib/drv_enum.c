/**
 *  @brief Implemenents drv_enum.h
 *  @file drv_enum.c
 *  @author Alec Leamas
 *  @license GPL2 or later
 *  @date December 2016
 *  @ingroup driver_api
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glob.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#ifdef HAVE_LIBUDEV_H
#include <libudev.h>
#endif

#ifdef HAVE_USB_H
#include <usb.h>
#endif

#include "drv_enum.h"
#include "driver.h"
#include "lirc_log.h"

static const logchannel_t logchannel = LOG_LIB;



/** Allocation chunk in glob_t_* routines. */
static const int GLOB_CHUNK_SIZE = 32;


void glob_t_init(glob_t* glob)
{
	memset(glob, 0, sizeof(glob_t));
	glob->gl_offs = GLOB_CHUNK_SIZE;
	glob->gl_pathv = (char**) calloc(glob->gl_offs, sizeof(char*));
}


void glob_t_add_path(glob_t* glob, const char* path)
{
	if (path == NULL)
		return;
	if (glob->gl_pathc >= glob->gl_offs) {
		glob->gl_offs += GLOB_CHUNK_SIZE;
		glob->gl_pathv = realloc(glob->gl_pathv,
					 glob->gl_offs * sizeof(char*));
	}
	glob->gl_pathv[glob->gl_pathc] = strdup(path);
	glob->gl_pathc += 1;
}


/** Free memory allocated by for a glob_t. */
void drv_enum_free(glob_t* glob)
{
	int i;

	if (glob == NULL)
		return;
	for (i = 0; i < glob->gl_pathc; i += 1)
		free(glob->gl_pathv[i]);
	free(glob->gl_pathv);
}

#ifdef HAVE_LIBUDEV_H

static const char* get_sysattr(struct udev_device* device, const char* attr);


/** Return struct udev_device or null for given /dev/X path. */
static struct udev_device* udev_from_dev_path(struct udev* udev,
					      const char* path)
{
	struct stat statbuf;
	char dev_id[64];

	if (stat(path, &statbuf) != 0) {
		log_perror_debug("Cannot stat device %s", path);
		return NULL;
	}
	if (!S_ISCHR(statbuf.st_mode)) {
		log_debug("Ignoring non-character device %s", path);
		return NULL;
	}
	snprintf(dev_id, sizeof(dev_id), "c%d:%d",
		major(statbuf.st_rdev), minor(statbuf.st_rdev));
	return udev_device_new_from_device_id(udev, dev_id);
}


/**
 * Given a udev device retrieve vendor and product. Return parent
 * of class "usb".
 */
static struct udev_device* get_some_info(struct udev_device* device,
					 const char** idVendor,
					 const char** idProduct)
{
	struct udev_device* usb_device = NULL;
	const char* subsystem = udev_device_get_subsystem(device);

	if (subsystem && (strcmp(subsystem, "usb") != 0)) {
		usb_device = udev_device_get_parent_with_subsystem_devtype(
			device, "usb", "usb_device");
		if (!usb_device) {
			log_error("Unable to find parent usb device.");
		}
	}
	*idVendor = udev_device_get_sysattr_value(device, "idVendor");
	*idProduct = udev_device_get_sysattr_value(device, "idProduct");
	if (!*idProduct && usb_device)
		*idProduct = get_sysattr(usb_device, "idProduct");
	if (!*idVendor && usb_device)
		*idVendor = get_sysattr(usb_device, "idVendor");
	return usb_device ? usb_device : device;
}


/** Try to add udev info to existing device-only entries i globbuf. */
void drv_enum_add_udev_info(glob_t* oldbuf)
{
	glob_t newbuf;
	int i;
	char line[256];
	char* device_path;
	struct udev* udev = udev_new();
	const char* idVendor;
	const char* idProduct;

	glob_t_init(&newbuf);
	for (i = 0; i < oldbuf->gl_pathc; i += 1) {
		device_path = strdup(oldbuf->gl_pathv[i]);
		device_path = strtok(device_path, "\n \t");
		struct udev_device* udev_device =
			udev_from_dev_path(udev, device_path);
		if (udev_device == NULL) {
			glob_t_add_path(&newbuf, oldbuf->gl_pathv[i]);
		} else {
			udev_device = get_some_info(udev_device,
						    &idVendor,
						    &idProduct);
			snprintf(line, sizeof(line),
				 "%s [%s:%s] %s %s version: %s serial: %s",
				 device_path,
				 idVendor,
				 idProduct,
				 get_sysattr(udev_device, "manufacturer"),
				 get_sysattr(udev_device, "product"),
				 get_sysattr(udev_device, "version"),
				 get_sysattr(udev_device, "serial")
			);
			if (idVendor == NULL && idProduct == NULL)
				glob_t_add_path(&newbuf, oldbuf->gl_pathv[i]);
			else
				glob_t_add_path(&newbuf, line);
		}
		free(device_path);
	}
	drv_enum_free(oldbuf);
	memcpy(oldbuf, &newbuf, sizeof(glob_t));
}

#else   // HAVE_LIBUDEV_H

void drv_enum_add_udev_info(glob_t* oldbuf) {}

#endif  // HAVE_LIBUDEV_H


int drv_enum_globs(glob_t* globbuf, const char* const* patterns)
{
	glob_t buff;
	int i;
	int flags;
	int r;

	if (!patterns)
		return DRV_ERR_BAD_VALUE;
	buff.gl_offs = 0;
	buff.gl_pathc = 0;
	buff.gl_pathv = NULL;
	glob_t_init(globbuf);

	for (flags = 0; *patterns; patterns++) {
		r = glob(*patterns, flags, NULL, &buff);
		if (r == GLOB_NOMATCH)
			continue;
		if (r != 0) {
			globfree(&buff);
			return DRV_ERR_BAD_STATE;
		}
		flags = GLOB_APPEND;
	}
	for (i = 0; i < buff.gl_pathc; i += 1) {
		glob_t_add_path(globbuf, buff.gl_pathv[i]);
	}
	globfree(&buff);
	drv_enum_add_udev_info(globbuf);
	return globbuf->gl_pathc == 0 ? DRV_ERR_ENUM_EMPTY : 0;
}


int drv_enum_glob(glob_t* globbuf, const char* const pattern)
{
	const char* globs[] = {pattern, NULL};

	return drv_enum_globs(globbuf, globs);
}


#ifdef HAVE_USB_H

int drv_enum_usb(glob_t* glob,
		 int (*is_device_ok)(uint16_t vendor, uint16_t product))
{
	struct usb_bus* usb_bus;
	struct usb_device* dev;
	char device_path[2 * MAXPATHLEN + 32];

	usb_init();
	usb_find_busses();
	usb_find_devices();
	glob_t_init(glob);
	for (usb_bus = usb_busses; usb_bus; usb_bus = usb_bus->next) {
		for (dev = usb_bus->devices; dev; dev = dev->next) {
			if (!is_device_ok(dev->descriptor.idVendor,
					  dev->descriptor.idProduct))
				continue;
			snprintf(device_path, sizeof(device_path),
				 "/dev/bus/usb/%s/%s     %04x:%04x",
				 dev->bus->dirname, dev->filename,
				 dev->descriptor.idVendor,
				 dev->descriptor.idProduct);
			glob_t_add_path(glob, device_path);
		}
	}
	drv_enum_add_udev_info(glob);
	return 0;
}

#else

int drv_enum_usb(glob_t* glob,
		 int (*is_device_ok)(uint16_t vendor, uint16_t product))
{
	return DRV_ERR_NOT_IMPLEMENTED;
}

#endif  // HAVE_USB_H


#ifdef HAVE_LIBUDEV_H

static const char* get_sysattr(struct udev_device* device, const char* attr)
{
	const char* s = udev_device_get_sysattr_value(device, attr);

	return s ? s : "?";
}


/** Format a printable string from given list entry. */
static bool format_udev_entry(struct udev* udev,
			      struct udev_list_entry* device,
			      char* buff,
			      size_t size)
{
	const char* const syspath = udev_list_entry_get_name(device);
	struct udev_device* udev_device =
		udev_device_new_from_syspath(udev, syspath);
	const char* const devnode = udev_device_get_devnode(udev_device);
	const char* idVendor;
	const char* idProduct;

	if (!devnode)
		return false;
	udev_device = get_some_info(udev_device, &idVendor, &idProduct);
	snprintf(buff, size, "%s [%s:%s] %s %s version: %s serial: %s",
		 devnode,
		 idVendor,
		 idProduct,
		 get_sysattr(udev_device, "manufacturer"),
		 get_sysattr(udev_device, "product"),
		 get_sysattr(udev_device, "version"),
		 get_sysattr(udev_device, "serial")
	);
	return true;
}


/** Add all device links for given list entry to globbuf. */
static void add_links(glob_t* globbuf,
		      struct udev* udev,
		      struct udev_list_entry* target_entry)
{
	char buff[128];
	char path[128];
	size_t pathlen;

	const char* const syspath = udev_list_entry_get_name(target_entry);
	struct udev_device* target_device =
		udev_device_new_from_syspath(udev, syspath);
	struct udev_list_entry* links =
		udev_device_get_devlinks_list_entry(target_device);

	while (links != NULL) {
		pathlen = readlink(udev_list_entry_get_name(links),
				   path,
				   sizeof(path) - 1);
		path[pathlen] = '\0';
		snprintf(buff, sizeof(buff), "%s -> %s",
			 udev_list_entry_get_name(links), path);
		links = udev_list_entry_get_next(links);
		glob_t_add_path(globbuf, buff);
	}
}


/** Check if given buff already exists as a path in globbuf. */
static bool is_dup(glob_t* globbuf, const char* buff)
{
	int i;

	for (i = 0; i < globbuf->gl_pathc; i += 1) {
		if (strcmp(globbuf->gl_pathv[i], buff) == 0)
			return true;
	}
	return false;
}


/** Check if what->parent_subsys test passes.  */
static bool check_parent_subsys(const struct drv_enum_udev_what* what,
				struct udev* udev,
				struct udev_list_entry* device_entry)
{
	if (!what->parent_subsys)
		return true;
	const char* const syspath = udev_list_entry_get_name(device_entry);
	struct udev_device* device =
		udev_device_new_from_syspath(udev, syspath);
	struct udev_device* parent =
	    udev_device_get_parent_with_subsystem_devtype(device,
							  "rc",
							  NULL);

	return parent != NULL;
}


/** Enumerate all devices matching {0}-terminated list of conditions. */
int drv_enum_udev(glob_t* globbuf,
		  const struct drv_enum_udev_what* what)
{
	const struct drv_enum_udev_what SENTINEL = {0};
	struct udev* udev;
	struct udev_enumerate* enumerate;
	struct udev_list_entry* devices;
	struct udev_list_entry* device;
	char buff[128];

	glob_t_init(globbuf);
	udev = udev_new();
	if (udev == NULL) {
		log_error("Cannot run udev_new()");
		return DRV_ERR_BAD_STATE;
	}
	while (memcmp(what,
		      &SENTINEL,
		      sizeof(struct drv_enum_udev_what)) != 0) {
		enumerate = udev_enumerate_new(udev);
		if (what->idVendor != NULL)
			udev_enumerate_add_match_sysattr(
				enumerate, "idVendor", what->idVendor);
		if (what->idProduct != NULL)
			udev_enumerate_add_match_sysattr(
				enumerate, "idProduct", what->idProduct);
		if (what->subsystem != NULL)
			udev_enumerate_add_match_subsystem(enumerate,
							   what->subsystem);
		udev_enumerate_scan_devices(enumerate);
		devices = udev_enumerate_get_list_entry(enumerate);
		udev_list_entry_foreach(device, devices) {
			if (!check_parent_subsys(what, udev, device))
				continue;
			if (!format_udev_entry(udev, device,
					       buff, sizeof(buff)))
				continue;
			if (is_dup(globbuf, buff))
				continue;
			glob_t_add_path(globbuf, (const char*)buff);
			add_links(globbuf, udev, device);
		}
		udev_enumerate_unref(enumerate);
		what++;
	}
	udev_unref(udev);
	return 0;
}

#else  // HAVE_LIBUDEV_H

int drv_enum_udev(glob_t* globbuf, const struct drv_enum_udev_what* what)
{
	return DRV_ERR_NOT_IMPLEMENTED;
}

#endif  // HAVE_LIBUDEV_H
