/*
 * lirc_driver.h - linux infrared remote control header file
 *
 * Basic interface for user-space drivers, aimed to be included
 * in each driver. It provides basic functionality for sending,
 * receiving and logging.
 *
 */

#ifndef _LIRC_DRIVER_H
#define _LIRC_DRIVER_H

#include "lirc/ir_remote_types.h"
#include "lirc/lirc_log.h"
#include "lirc/hardware.h"
#include "lirc/ir_remote.h"
#include "lirc/receive.h"
#include "lirc/transmit.h"

#endif