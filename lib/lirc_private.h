/*
 * lirc.h - linux infrared remote control header file
 * last modified 2010/06/03 by Jarod Wilson
 */

/**
 * @defgroup  private_api  Internal API
 * @file lirc_private.h
 * @brief Main include file for lirc applications.
 */


#ifndef _LIRC_PRIVATE_H
#define _LIRC_PRIVATE_H

#ifndef __u32
#include <stdint.h>
#define __u32 uint32_t
#endif

#ifdef HAVE_KERNEL_LIRC_H
#include <linux/lirc.h>
#else
#include "include/media/lirc.h"
#endif

#include "ir_remote_types.h"
#include "lirc_log.h"
#include "lirc_options.h"
#include "lirc-utils.h"
#include "curl_poll.h"
#include "config_file.h"
#include "dump_config.h"
#include "input_map.h"
#include "driver.h"
#include "ir_remote_types.h"
#include "drv_admin.h"
#include "ir_remote.h"
#include "receive.h"
#include "release.h"
#include "serial.h"
#include "transmit.h"
#include "ciniparser.h"

#endif
