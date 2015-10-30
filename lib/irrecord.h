/****************************************************************************
** irrecord.h **************************************************************
****************************************************************************
*
* irrecord.h - base library for irrrecord.
*
* Copyright (C) 1998,99 Christoph Bartelmus <lirc@bartelmus.de>
*
*/

#ifndef IRRECORD_H
#define IRRECORD_H

#ifdef __cplusplus
extern "C" {
#endif


#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <syslog.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "lirc_private.h"


#define min(a, b) (a > b ? b : a)
#define max(a, b) (a > b ? a : b)

#define BUTTON       (80 + 1)
#define RETRIES        10

/* the longest signal I've seen up to now was 48-bit signal with header */
#define MAX_SIGNALS   200

/* some threshold values */
#define TH_SPACE_ENC   80       /* I want less than 20% mismatches */
#define TH_HEADER      90
#define TH_REPEAT      90
#define TH_TRAIL       90
#define TH_LEAD        90
#define TH_IS_BIT      10
#define TH_RC6_SIGNAL 550

#define MIN_GAP     20000
#define MAX_GAP    100000

#define SAMPLES        80

// forwards

struct ir_remote;
struct main_state;
struct opts;

// type declarations

typedef void (*remote_func) (struct ir_remote* remotes);

enum analyse_mode { MODE_GET_GAP, MODE_HAVE_GAP };


/** Return from one attempt to determine lengths in get_lengths().*/
enum lengths_status {
	STS_LEN_OK,
	STS_LEN_FAIL,
	STS_LEN_RAW_OK,
	STS_LEN_TIMEOUT,
	STS_LEN_AGAIN,
	STS_LEN_AGAIN_INFO,
	STS_LEN_NO_GAP_FOUND,
	STS_LEN_TOO_LONG,
};


/** Return form one attempt to get gap in get_gap(). */
enum get_gap_status {
	STS_GAP_INIT,
	STS_GAP_TIMEOUT,
	STS_GAP_FOUND,
	STS_GAP_GOT_ONE_PRESS,
	STS_GAP_AGAIN
};


/** Return from one attempt in get_toggle_bit_mask(). */
enum toggle_status {
	STS_TGL_TIMEOUT,
	STS_TGL_GOT_ONE_PRESS,
	STS_TGL_NOT_FOUND,
	STS_TGL_FOUND,
	STS_TGL_AGAIN
};


/** Return from one pass in record_buttons(). */
enum button_status {
	STS_BTN_INIT,
	STS_BTN_GET_NAME,
	STS_BTN_INIT_DATA,
	STS_BTN_GET_RAW_DATA,
	STS_BTN_GET_DATA,
	STS_BTN_GET_TOGGLE_BITS,
	STS_BTN_RECORD_DONE,
	STS_BTN_BUTTON_DONE,
	STS_BTN_BUTTONS_DONE,
	STS_BTN_ALL_DONE,
	STS_BTN_SOFT_ERROR,
	STS_BTN_HARD_ERROR,
	STS_BTN_TIMEOUT,
};


/* analyse stuff */
struct lengths {
	unsigned int	count;
	lirc_t		sum, upper_bound, lower_bound, min, max;
	struct lengths* next;
};


/**
 * Parsed run-time options, reflects long_options and the command line,
 * mostly a const object.
 */
struct opts {
	int		dynamic_codes;
	int		analyse;
	int		force;
	int		disable_namespace;
	const char*	device;
	int		get_pre;
	int		get_post;
	int		test;
	int		invert;
	int		trail;
	int		list_namespace;
	int		update;
	const char*	filename;
	const char*	tmpfile;
	const char*	backupfile;
	const char*	driver;
	loglevel_t	loglevel;
	int		using_template;
	char		commandline[128];
};


/** Overall state in main. */
struct main_state {
	FILE*			fout;
	struct decode_ctx_t	decode_ctx;
	int                     started_as_root;
};


/** Private state in get_gap_length(). */
struct gap_state {
	struct lengths* scan;
	struct lengths* gaps;
	struct timeval	start;
	struct timeval	end;
	struct timeval	last;
	int		flag;
	int		maxcount;
	int		lastmaxcount;
	lirc_t		gap;
};


/** State in get_lengths(), private besides commented. */
struct lengths_state {
				/** Number of printed keypresses. */
	int			keypresses_done;
				/** Number of counted button presses. */
	int			keypresses;
	int			retval;
				/** Number of processed data items. */
	int			count;
	lirc_t			data;
	lirc_t			average;
	lirc_t			maxspace;
				/** Number of bits accounted for in signal.*/
	lirc_t			sum;
	lirc_t			remaining_gap;
	lirc_t			header;
	int			first_signal;
	enum analyse_mode	mode;
};


/** Private state in get_togggle_bit_mask(). */
struct toggle_state {
	struct decode_ctx_t	decode_ctx;
	int			retval;
	int			retries;
	int			flag;
	int			success;
	ir_code			first;
	ir_code			last;
	int			seq;
	int			repeats;
	int			found;
	int			inited;
};


/** State while recording buttons, privates besides commented. */
struct button_state {
			/** Recorded button, valid on STS_BTN_BUTTON_DONE. */
	struct ir_ncode ncode;
			/** Error message, valid on STS_BTN_*_ERROR. */
	char		message[128];
	int		retval;
	char		buffer[BUTTON];
	char*		string;
	lirc_t		data;
	lirc_t		sum;
	unsigned int	count;
	int		flag;
	int		no_data;
	int             started_as_root;  /** Started with euid == 0? */
};


// Globals

extern struct ir_remote remote;         /** Shared list of remotes. */
extern unsigned int eps;                /** Error tolerance in per cent. */
extern lirc_t aeps;                     /** Absolute error tolerance (us). */


// Functions

/** Try to read some bytes from the device, no decoding whatsoever. */
ssize_t raw_read(void* buffer, size_t size, unsigned int timeout_us);

/** Unconditionally apply func(remote) for all items in remotes list. */
void for_each_remote(struct ir_remote* remotes, remote_func func);

/** sprintf-style message formatting into state->message. */
void btn_state_set_message(struct button_state* state, const char* fmt, ...);

/** Clear the driver input buffers. */
void flushhw(void);

/** Initiate a pristine gap_state. */
void gap_state_init(struct gap_state* state);

/** Initiate a pristine lengths_state. */
void lengths_state_init(struct lengths_state* state);

/** Initiate a pristine toggle_state. */
void toggle_state_init(struct toggle_state* state);

/** Initiate a pristine button_state. */
void button_state_init(struct button_state* state, int started_as_root);

/** Try to find out gap length, returning gap_status. */
enum get_gap_status get_gap_length(struct gap_state* state,
				   struct ir_remote* remote);

/** Try to find out pre/post etc. lengths,  returning lengths_status. */
enum lengths_status get_lengths(struct lengths_state* state,
				struct ir_remote* remote,
				int force,
				int interactive);

/** Free heap data allocated by get_lengths().*/
void free_all_lengths(void);

/** Try to find out toggle_bit_mask, returning toggle_status. */
enum toggle_status
get_toggle_bit_mask(struct toggle_state* state, struct ir_remote* remote);

/** The --analyse wrapper, returns boolean ok/fail. */
int do_analyse(const struct opts* opts, struct main_state* state);

/** Try to record one button, returning button_status. */
enum button_status record_buttons(struct button_state* btn_state,
				  enum button_status last_status,
				  struct main_state* state,
				  const struct opts* opts);

/** Write the provisionary config file. */
void config_file_setup(struct main_state* state, const struct opts* opts);

/** Write the final config file. */
int config_file_finish(struct main_state* state, const struct opts* opts);

/** Test hook: Extract remote->pre_data from remote->bits. */
void get_pre_data(struct ir_remote* remote);

/** Test hook: Extract remote->post_data and post_data_bits from bits. */
void get_post_data(struct ir_remote* remote);

/** Test hook: Move remote->pre_data into remote->bits. */
void remove_pre_data(struct ir_remote* remote);

/** Test hook: Move remote->post_data into remote->bits. */
void remove_post_data(struct ir_remote* remote);

/** Test hook: Invert all data items in remote. */
void invert_data(struct ir_remote* remote);

/** Test hook: Move remote->trail into remote->bits. */
void remove_trail(struct ir_remote* remote);

#ifdef __cplusplus
}
#endif

#endif
