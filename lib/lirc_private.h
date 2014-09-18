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

#include "include/media/lirc.h"
#include "lirc/ir_remote_types.h"
#include "lirc/lirc_log.h"
#include "lirc/lirc_options.h"
#include "lirc/config_file.h"
#include "lirc/dump_config.h"
#include "lirc/input_map.h"
#include "lirc/driver.h"
#include "lirc/ir_remote_types.h"
#include "lirc/drv_admin.h"
#include "lirc/ir_remote.h"
#include "lirc/receive.h"
#include "lirc/release.h"
#include "lirc/serial.h"
#include "lirc/transmit.h"
#include "lirc/ciniparser.h"

#endif
