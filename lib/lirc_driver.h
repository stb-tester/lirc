/**
 * @file lirc_driver.h
 * @brief Main include file for user space drivers.
 * @defgroup  driver_api  User-space driver API
 *
 * Basic interface for user-space drivers, aimed to be included
 * in each driver. It provides basic functionality for sending,
 * receiving and logging.
 */

#ifndef _LIRC_DRIVER_H
#define _LIRC_DRIVER_H

#define IN_DRIVER

#ifndef __u32
#include <stdint.h>
#define __u32 uint32_t
#endif

#include "lirc/ir_remote_types.h"
#include "lirc/lirc_log.h"
#include "lirc/driver.h"
#include "lirc/ir_remote.h"
#include "lirc/receive.h"
#include "lirc/transmit.h"

extern const struct driver* hardwares[];

#endif
