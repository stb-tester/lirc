
/****************************************************************************
 ** irrecord.c **************************************************************
 ****************************************************************************
 *
 * irrecord -  application for recording IR-codes for usage with lircd
 *
 * Copyright (C) 1998,99 Christoph Bartelmus <lirc@bartelmus.de>
 *
 */


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


#define min(a,b) (a>b ? b:a)
#define max(a,b) (a>b ? a:b)

#define BUTTON	     80+1
#define RETRIES        10

/* the longest signal I've seen up to now was 48-bit signal with header */
#define MAX_SIGNALS   200

/* some threshold values */
#define TH_SPACE_ENC   80	/* I want less than 20% mismatches */
#define TH_HEADER      90
#define TH_REPEAT      90
#define TH_TRAIL       90
#define TH_LEAD        90
#define TH_IS_BIT      10
#define TH_RC6_SIGNAL 550

#define MIN_GAP     20000
#define MAX_GAP    100000

#define SAMPLES        80

#define USAGE	    "Usage: irrecord [options] [config file]\n" \
		    "	    irrecord -a <config file> \n" \
		    "	    irrecord -l \n"


// forwards
static lirc_t emulation_readdata(lirc_t timeout);

// type declarations
typedef void (*remote_func) (struct ir_remote * remotes);

enum analyse_mode {MODE_GET_GAP, MODE_HAVE_GAP};

enum init_status {
	STS_INIT_NO_DRIVER,
	STS_INIT_BAD_DRIVER,
	STS_INIT_BAD_FILE,
	STS_INIT_ANALYZE,
	STS_INIT_TESTED,
	STS_INIT_FOPEN,
	STS_INIT_OK,
	STS_INIT_FORCE_TMPL,
	STS_INIT_HW_FAIL,
	STS_INIT_BAD_MODE,
	STS_INIT_O_NONBLOCK,
};


/** Return from one attempt to determine lengths in get_lengths().*/
enum lengths_status {
	STS_LEN_OK,
	STS_LEN_FAIL,
	STS_LEN_RAW_OK,
	STS_LEN_TIMEOUT,
	STS_LEN_AGAIN,
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


/* analyse stuff */
struct lengths {
	unsigned int count;
	lirc_t sum, upper_bound, lower_bound, min, max;
	struct lengths *next;
};


/** Parsed run-time options, reflects long_options. */
struct opts {
	int dynamic_codes;
	int analyse;
	int force;
	int disable_namespace;
	const char *device;
	int get_pre;
	int get_post;
	int test;
	int invert;
	int trail;
	int list_namespace;
	const char* filename;
	const char* driver;
	loglevel_t loglevel;
};


/** Overall state in main. */
struct main_state {
	FILE *fout;
	FILE *fin;
	struct decode_ctx_t decode_ctx;
	struct ir_remote *remotes;
	int using_template;
	char commandline[128];
};


/** State in get_gap_length(). */
struct gap_state {
	struct lengths* scan;
	struct lengths* gaps;
	struct timeval start;
	struct timeval end;
	struct timeval last;
	int flag;
	int maxcount;
	int lastmaxcount;
	lirc_t gap;
};


/**State in get_lengths() (which also uses lot's of global state. */
struct lengths_state {
	int retval;
	int count;
	lirc_t data;
	lirc_t average;
	lirc_t maxspace;
	lirc_t sum;
	lirc_t remaining_gap;
	lirc_t header;
	int first_signal;
	enum analyse_mode mode;
	int keypresses_done;   /**< Number of printed keypresses. */
	int keypresses;    /**< Number of counted button presses. */
};


/** State in get_togggle_bit_mask(). */
struct toggle_state {
	struct decode_ctx_t decode_ctx;
	int retval;
	int retries;
	int flag;
	int  success;
	ir_code first;
	ir_code last;
	int seq;
	int repeats;
	int found;
	int inited;
};


/** State while recording buttons. */
struct button_state {
	int retval;
	char buffer[BUTTON];
	char* string;
	lirc_t data;
	lirc_t sum;
	unsigned int count;
	int flag;
	int no_data;
	int retries;
	char message[128];
};


// Constants
static const char* const help =
USAGE
"\nOptions:\n"
"\t -H --driver=driver\tUse given driver\n"
"\t -d --device=device\tRead from given device\n"
"\t -a --analyse\t\tAnalyse raw_codes config files\n"
"\t -l --list-namespace\tList valid button names\n"
"\t -U --plugindir=dir\tLoad drivers from dir\n"
"\t -f --force\t\tForce raw mode\n"
"\t -n --disable-namespace\tDisable namespace checks\n"
"\t -Y --dynamic-codes\tEnable dynamic codes\n"
"\t -D --loglevel=level\t'error', 'info', 'notice',... or 0..10\n"
"\t -h --help\t\tDisplay this message\n"
"\t -v --version\t\tDisplay version\n";

static const struct option long_options[] = {
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{"analyse", no_argument, NULL, 'a'},
	{"device", required_argument, NULL, 'd'},
	{"options-file", required_argument, NULL, 'O'},
	{"debug", required_argument, NULL, 'D'},
	{"loglevel", required_argument, NULL, 'D'},
	{"driver", required_argument, NULL, 'H'},
	{"force", no_argument, NULL, 'f'},
	{"disable-namespace", no_argument, NULL, 'n'},
	{"list-namespace", no_argument, NULL, 'l'},
	{"plugindir", required_argument, NULL, 'U'},
	{"dynamic-codes", no_argument, NULL, 'Y'},
	{"pre", no_argument, NULL, 'p'},
	{"post", no_argument, NULL, 'P'},
	{"test", no_argument, NULL, 't'},
	{"invert", no_argument, NULL, 'i'},
	{"trail", no_argument, NULL, 'T'},
	{0, 0, 0, 0}
};


const char* const MSG_WELCOME =
	"\nirrecord -  application for recording IR-codes" " for usage with lirc\n"
	"Copyright (C) 1998,1999 Christoph Bartelmus" "(lirc@bartelmus.de)\n"
	"\n"
	"This program will record the signals from your remote control\n"
	"and create a config file for lircd.\n"
	"\n"
	"A proper config file for lircd is maybe the most vital part of this\n"
	"package, so you should invest some time to create a working config\n"
	"file. Although I put a good deal of effort in this program it is often\n"
	"not possible to automatically recognize all features of a remote\n"
	"control. Often short-comings of the receiver hardware make it nearly\n"
	"impossible. If you have problems to create a config file READ THE\n"
	"DOCUMENTATION at https://sf.net/p/lirc-remotes/wiki\n"
	"\n"
	"If there already is a remote control of the same brand available at\n"
	"http://sf.net/p/lirc-remotes you might want to try using such a\n"
	"remote as a template. The config files already contains all\n"
	"parameters of the protocol used by remotes of a certain brand and\n"
	"knowing these parameters makes the job of this program much\n"
	"easier. There are also template files for the most common protocols\n"
	"available. Templates can be downloaded using irdb-get(1). You use a\n"
	"template file by providing the path of the file as a command line\n"
	"parameter.\n"
	"\n"
	"Please take the time to finish the file as described in\n"
	"https://sourceforge.net/p/lirc-remotes/wiki/Checklist/ an send it\n"
	"to  <lirc@bartelmus.de> so it can be made available to others.\n";

static const char* const MSG_DEVINPUT =
	"Usually it's not necessary to create a new config file for devinput\n"
	"devices. A generic config file can be found at:\n"
	"https://sf.net/p/lirc-remotes/code/ci/master/tree/remotes/devinput/devinput.lircd.conf\n"
	"It can be downloaded using irdb-get(1)\n"
	"Please try this config file before creating your own.\n";

static const char* const MSG_TOGGLE_BIT_INTRO =
	"Checking for toggle bit mask.\n"
	"Please press an arbitrary button repeatedly as fast as possible.\n"
	"Make sure you keep pressing the SAME button and that you DON'T HOLD\n"
	"the button down!.\n"
	"If you can't see any dots appear, wait a bit between button presses.\n\n"
	"Press RETURN to continue.";


static const struct driver hw_emulation = {
	.name		= "emulation",
	.device		= "/dev/null",
	.features	= LIRC_CAN_REC_MODE2,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_MODE2,
	.code_length	= 0,
	.init_func	= NULL,
	.deinit_func	= NULL,
	.send_func	= NULL,
	.rec_func	= NULL,
	.decode_func	= NULL,
	.drvctl_func	= NULL,
	.readdata	= emulation_readdata,
	.open_func	= default_open,
	.close_func	= default_close,
	.api_version	= 2,
	.driver_version = "0.9.2"
};


// globals
static struct ir_remote remote;
static struct ir_ncode ncode;

static lirc_t signals[MAX_SIGNALS];

static unsigned int eps = 30;
static lirc_t aeps = 100;

static struct ir_remote *emulation_data;
static struct ir_ncode *next_code = NULL;
static struct ir_ncode *current_code = NULL;
static int current_index = 0;
static int current_rep = 0;

static struct lengths *first_space = NULL, *first_pulse = NULL;
static struct lengths *first_sum = NULL, *first_gap = NULL, *first_repeat_gap = NULL;
static struct lengths *first_signal_length = NULL;
static struct lengths *first_headerp = NULL, *first_headers = NULL;
static struct lengths *first_1lead = NULL, *first_3lead = NULL, *first_trail = NULL;
static struct lengths *first_repeatp = NULL, *first_repeats = NULL;

static __u32 lengths[MAX_SIGNALS];
static __u32 first_length, first_lengths, second_lengths;
static unsigned int count, count_spaces, count_3repeats, count_5repeats, count_signals;


static int i_printf(int interactive, char *format_str, ...)
{
	va_list ap;
	int ret = 0;

	if (interactive && lirc_log_is_enabled_for(LIRC_DEBUG))
	{
		va_start(ap, format_str);
		ret = vfprintf(stdout, format_str, ap);
		va_end(ap);
	}
	return ret;
}


static void btn_state_set_message(struct button_state* state, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(state->message, sizeof(state->message), fmt, ap);
	va_end(ap);
}


static void fprint_copyright(FILE * fout)
{
	fprintf(fout, "\n"
		"# Please take the time to finish this file as described in\n"
		"# https://sourceforge.net/p/lirc-remotes/wiki/Checklist/\n"
		"# and make it available to others by sending it to \n"
		"# <lirc@bartelmus.de>\n");
}


static int availabledata(void)
{
	fd_set fds;
	int ret;
	struct timeval tv;

	FD_ZERO(&fds);
	FD_SET(curr_driver->fd, &fds);
	do {
		do {
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			ret = select(curr_driver->fd + 1, &fds, NULL, NULL, &tv);
		}
		while (ret == -1 && errno == EINTR);
		if (ret == -1) {
			logprintf(LIRC_ERROR, "select() failed\n");
			logperror(LIRC_ERROR, NULL);
			continue;
		}
	}
	while (ret == -1);

	if (FD_ISSET(curr_driver->fd, &fds)) {
		return (1);
	}
	return (0);
}


static void flushhw(void)
{
	size_t size = 1;
	char buffer[sizeof(ir_code)];

	switch (curr_driver->rec_mode) {
	case LIRC_MODE_MODE2:
		while (availabledata())
			curr_driver->readdata(0);
		return;
	case LIRC_MODE_LIRCCODE:
		size = curr_driver->code_length / CHAR_BIT;
		if (curr_driver->code_length % CHAR_BIT)
			size++;
		break;
	}
	while (read(curr_driver->fd, buffer, size) == size) ;
}


static int resethw(void)
{
	int flags;

	if (curr_driver->deinit_func)
		curr_driver->deinit_func();
	if (curr_driver->init_func) {
		if (!curr_driver->init_func())
			return (0);
	}
	flags = fcntl(curr_driver->fd, F_GETFL, 0);
	if (flags == -1 || fcntl(curr_driver->fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		if (curr_driver->deinit_func)
			curr_driver->deinit_func();
		return (0);
	}
	return (1);
}


void gap_state_init(struct gap_state* state)
{
       memset(state, 0, sizeof(struct gap_state));
}


void lengths_state_init(struct lengths_state* state, int interactive)
{
	count = 0;
	count_spaces = 0;
	count_3repeats = 0;
	count_5repeats = 0;
	count_signals = 0;
	first_length = 0;
	first_lengths = 0;
	second_lengths = 0;
	memset(state, 0, sizeof(struct lengths_state));
	state->first_signal = -1;
	state->retval = 1;

	if (interactive) {
		printf("Now start pressing buttons on your remote control.\n\n");
		printf("It is very important that you press many different buttons and hold them\n"
		       "down for approximately one second. Each button should generate at least one\n"
		       "dot but in no case more than ten dots of output.\n"
		       "Don't stop pressing buttons until two lines of dots (2x80) have been\n" "generated.\n\n");
		printf("Press RETURN now to start recording.");
		fflush(stdout);
		getchar();
		flushhw();
	}
}


void toggle_state_init(struct toggle_state* state)
{
	memset(state, 0, sizeof(struct toggle_state));
	state->retries = 30;
	state->retval = EXIT_SUCCESS;
}


void button_state_init(struct button_state* state)
{
	memset(state, 0, sizeof(struct button_state));
	state->retval = EXIT_SUCCESS;
}


static void get_commandline(int argc, char** argv, char* buff, size_t size)
{
	int i;
	int j;
	int dest = 0;
	if (size == 0)
		return;
	for (i = 1; i < argc; i += 1 ) {
		for (j=0; argv[i][j] != '\0'; j += 1) {
			if (dest  + 1 >= size)
				break;
			buff[dest++] = argv[i][j];
		}
		if (dest  + 1 >= size)
			break;
		buff[dest++] = ' ';
	}
	buff[--dest] = '\0';
}


static void add_defaults(void)
{
	char level[4];
	snprintf(level, sizeof(level), "%d", lirc_log_defaultlevel());
	const char* const defaults[] = {
		"lircd:plugindir",		PLUGINDIR,
		"irrecord:driver",		"devinput",
		"irrecord:device",		 LIRC_DRIVER_DEVICE,
		"irrecord:analyse",		"False",
		"irrecord:force",		"False",
		"irrecord:disable-namespace",	"False",
		"irrecord:dynamic-codes",	"False",
		"irrecord:list_namespace",	"False",
		"irrecord:filename",		"irrecord.conf",
		"lircd:debug",			 level,
		(const char*)NULL,	(const char*)NULL
	};
	options_add_defaults(defaults);
}


static void parse_options(int argc, char** const argv)
{
	int c;

	const char* const optstring = "had:D:H:fnlO:pPtiTU:vY";

	add_defaults();
	optind = 1;
	while ((c = getopt_long(argc, argv, optstring, long_options, NULL))
		!= -1 )
	{
		switch (c) {
		case 'a':
			options_set_opt("irrecord:analyse", "True");
			break;
		case 'D':
			if (string2loglevel(optarg) == LIRC_BADLEVEL){
				fprintf(stderr,
					"Bad debug level: %s\n", optarg);
				exit(EXIT_FAILURE);
			}
			options_set_opt("lircd:debug", optarg);
			break;
		case 'd':
			options_set_opt("irrecord:device", optarg);
			break;
		case 'f':
			options_set_opt("irrecord:force", "True");
			break;
		case 'H':
			options_set_opt("irrecord:driver", optarg);
			break;
		case 'h':
			fputs(help, stdout);
			exit(EXIT_SUCCESS);
		case 'i':
			options_set_opt("irrecord:invert", "True");
			break;
		case 'l':
			options_set_opt("irrecord:list-namespace", "True");
			break;
		case 'n':
			options_set_opt("irrecord:disable-namespace", "True");
			break;
		case 'O':
			return;
		case 'P':
			options_set_opt("irrecord:post", "True");
			break;
		case 'p':
			options_set_opt("irrecord:pre", "True");
			break;
		case 't':
			options_set_opt("irrecord:test", "True");
			break;
		case 'T':
			options_set_opt("irrecord:trail", "True");
			break;
		case 'U':
			options_set_opt("lircd:plugindir", optarg);
			break;
		case 'Y':
			options_set_opt("lircd:dynamic-codes", "True");
			break;
		case 'v':
			printf("irrecord %s\n",  VERSION);
			exit(EXIT_SUCCESS);
		default:
			fputs(USAGE, stderr);
			exit(EXIT_FAILURE);
		}
	}
	if (optind == argc - 1) {
		options_set_opt("irrecord:filename", argv[optind]);
	} else if (optind != argc) {
		fputs("irrecord: invalid argument count\n", stderr);
		exit(EXIT_FAILURE);
	}
}


static lirc_t calc_signal(struct lengths *len)
{
	return ((lirc_t) (len->sum / len->count));
}


static void set_toggle_bit_mask(struct ir_remote *remote, ir_code xor)
{
	ir_code mask;
	struct ir_ncode *codes;
	int bits;

	if (!remote->codes)
		return;

	bits = bit_count(remote);
	mask = ((ir_code) 1) << (bits - 1);
	while (mask) {
		if (mask == xor)
			break;
		mask = mask >> 1;
	}
	if (mask) {
		remote->toggle_bit_mask = xor;

		codes = remote->codes;
		while (codes->name != NULL) {
			codes->code &= ~xor;
			codes++;
		}
	}
	/* Sharp, Denon and some others use a toggle_mask */
	else if (bits == 15 && xor == 0x3ff) {
		remote->toggle_mask = xor;
	} else {
		remote->toggle_bit_mask = xor;
	}
}


static void get_pre_data(struct ir_remote *remote)
{
	struct ir_ncode *codes;
	ir_code mask, last;
	int count, i;

	if (remote->bits == 0)
		return;

	mask = (-1);
	codes = remote->codes;
	if (codes->name == NULL)
		return;		/* at least 2 codes needed */
	last = codes->code;
	codes++;
	if (codes->name == NULL)
		return;		/* at least 2 codes needed */
	while (codes->name != NULL) {
		struct ir_code_node *loop;

		mask &= ~(last ^ codes->code);
		last = codes->code;
		for (loop = codes->next; loop != NULL; loop = loop->next) {
			mask &= ~(last ^ loop->code);
			last = loop->code;
		}

		codes++;
	}
	count = 0;
	while (mask & 0x8000000000000000LL) {
		count++;
		mask = mask << 1;
	}
	count -= sizeof(ir_code) * CHAR_BIT - remote->bits;

	/* only "even" numbers should go to pre/post data */
	if (count % 8 && (remote->bits - count) % 8) {
		count -= count % 8;
	}
	if (count > 0) {
		mask = 0;
		for (i = 0; i < count; i++) {
			mask = mask << 1;
			mask |= 1;
		}
		remote->bits -= count;
		mask = mask << (remote->bits);
		remote->pre_data_bits = count;
		remote->pre_data = (last & mask) >> (remote->bits);

		codes = remote->codes;
		while (codes->name != NULL) {
			struct ir_code_node *loop;

			codes->code &= ~mask;
			for (loop = codes->next; loop != NULL; loop = loop->next) {
				loop->code &= ~mask;
			}
			codes++;
		}
	}
}


static void get_post_data(struct ir_remote *remote)
{
	struct ir_ncode *codes;
	ir_code mask, last;
	int count, i;

	if (remote->bits == 0)
		return;

	mask = (-1);
	codes = remote->codes;
	if (codes->name == NULL)
		return;		/* at least 2 codes needed */
	last = codes->code;
	codes++;
	if (codes->name == NULL)
		return;		/* at least 2 codes needed */
	while (codes->name != NULL) {
		struct ir_code_node *loop;

		mask &= ~(last ^ codes->code);
		last = codes->code;
		for (loop = codes->next; loop != NULL; loop = loop->next) {
			mask &= ~(last ^ loop->code);
			last = loop->code;
		}
		codes++;
	}
	count = 0;
	while (mask & 0x1) {
		count++;
		mask = mask >> 1;
	}
	/* only "even" numbers should go to pre/post data */
	if (count % 8 && (remote->bits - count) % 8) {
		count -= count % 8;
	}
	if (count > 0) {
		mask = 0;
		for (i = 0; i < count; i++) {
			mask = mask << 1;
			mask |= 1;
		}
		remote->bits -= count;
		remote->post_data_bits = count;
		remote->post_data = last & mask;

		codes = remote->codes;
		while (codes->name != NULL) {
			struct ir_code_node *loop;

			codes->code = codes->code >> count;
			for (loop = codes->next; loop != NULL; loop = loop->next) {
				loop->code = loop->code >> count;
			}
			codes++;
		}
	}
}


static void remove_pre_data(struct ir_remote *remote)
{
	struct ir_ncode *codes;

	if (remote->pre_data_bits == 0 || remote->pre_p != 0 || remote->pre_s != 0) {
		remote = remote->next;
		return;
	}
	codes = remote->codes;
	while (codes->name != NULL) {
		struct ir_code_node *loop;

		codes->code |= remote->pre_data << remote->bits;
		for (loop = codes->next; loop != NULL; loop = loop->next) {
			loop->code |= remote->pre_data << remote->bits;
		}
		codes++;
	}
	remote->bits += remote->pre_data_bits;
	remote->pre_data = 0;
	remote->pre_data_bits = 0;
}


static void remove_post_data(struct ir_remote *remote)
{
	struct ir_ncode *codes;

	if (remote->post_data_bits == 0) {
		remote = remote->next;
		return;
	}
	codes = remote->codes;
	while (codes->name != NULL) {
		struct ir_code_node *loop;

		codes->code <<= remote->post_data_bits;
		codes->code |= remote->post_data;
		for (loop = codes->next; loop != NULL; loop = loop->next) {
			loop->code <<= remote->post_data_bits;
			loop->code |= remote->post_data;
		}
		codes++;
	}
	remote->bits += remote->post_data_bits;
	remote->post_data = 0;
	remote->post_data_bits = 0;
}


static void invert_data(struct ir_remote *remote)
{
	struct ir_ncode *codes;
	ir_code mask;
	lirc_t p, s;

	/* swap one, zero */
	p = remote->pone;
	s = remote->sone;
	remote->pone = remote->pzero;
	remote->sone = remote->szero;
	remote->pzero = p;
	remote->szero = s;

	/* invert pre_data */
	if (has_pre(remote)) {
		mask = gen_mask(remote->pre_data_bits);
		remote->pre_data ^= mask;
	}
	/* invert post_data */
	if (has_post(remote)) {
		mask = gen_mask(remote->post_data_bits);
		remote->post_data ^= mask;
	}

	if (remote->bits == 0) {
		remote = remote->next;
		return;
	}

	/* invert codes */
	mask = gen_mask(remote->bits);
	codes = remote->codes;
	while (codes->name != NULL) {
		struct ir_code_node *loop;

		codes->code ^= mask;
		for (loop = codes->next; loop != NULL; loop = loop->next) {
			loop->code ^= mask;
		}
		codes++;
	}
}


static void remove_trail(struct ir_remote *remote)
{
	int extra_bit;

	if (!is_space_enc(remote))
		return;
	if (remote->ptrail == 0)
		return;
	if (expect(remote, remote->pone, remote->pzero) || expect(remote, remote->pzero, remote->pone))
		return;
	if (!(expect(remote, remote->sone, remote->szero) && expect(remote, remote->szero, remote->sone)))
		return;
	if (expect(remote, remote->ptrail, remote->pone)) {
		extra_bit = 1;
	} else if (expect(remote, remote->ptrail, remote->pzero)) {
		extra_bit = 0;
	} else {
		return;
	}

	remote->post_data_bits++;
	remote->post_data <<= 1;
	remote->post_data |= extra_bit;
	remote->ptrail = 0;
}


static void for_each_remote(struct ir_remote *remotes, remote_func func)
{
	struct ir_remote *remote;

	remote = remotes;
	while (remote != NULL) {
		func(remote);
		remote = remote->next;
	}
}


static int mywaitfordata(__u32 maxusec)
{
	fd_set fds;
	int ret;
	struct timeval tv;

	while (1) {
		FD_ZERO(&fds);
		FD_SET(curr_driver->fd, &fds);
		do {
			do {
				if (maxusec > 0) {
					tv.tv_sec = maxusec / 1000000;
					tv.tv_usec = maxusec % 1000000;
					ret = select(curr_driver->fd + 1, &fds, NULL, NULL, &tv);
					if (ret == 0)
						return (0);
				} else {
					ret = select(curr_driver->fd + 1, &fds, NULL, NULL, NULL);
				}
			}
			while (ret == -1 && errno == EINTR);
			if (ret == -1) {
				logprintf(LIRC_ERROR, "select() failed\n");
				logperror(LIRC_ERROR, NULL);
				continue;
			}
		}
		while (ret == -1);

		if (FD_ISSET(curr_driver->fd, &fds)) {
			/* we will read later */
			return (1);
		}
	}
}


static lirc_t emulation_readdata(lirc_t timeout)
{
	static lirc_t sum = 0;
	lirc_t data = 0;

	if (current_code == NULL) {
		data = 1000000;
		if (next_code) {
			current_code = next_code;
		} else {
			current_code = emulation_data->codes;
		}
		current_rep = 0;
		sum = 0;
	} else {
		if (current_code->name == NULL) {
			fprintf(stderr, "%s: %s no data found\n", progname, emulation_data->name);
			data = 0;
		}
		if (current_index >= current_code->length) {
			if (next_code) {
				current_code = next_code;
			} else {
				current_rep++;
				if (current_rep > 2) {
					current_code++;
					current_rep = 0;
					data = 1000000;
				}
			}
			current_index = 0;
			if (current_code->name == NULL) {
				current_code = NULL;
				return emulation_readdata(timeout);
			}
			if (data == 0) {
				if (is_const(emulation_data)) {
					data = emulation_data->gap - sum;
				} else {
					data = emulation_data->gap;
				}
			}

			sum = 0;
		} else {
			data = current_code->signals[current_index];
			if ((current_index % 2) == 0) {
				data |= PULSE_BIT;
			}
			current_index++;
			sum += data & PULSE_MASK;
		}
	}
	/*
	   printf("delivering: %c%u\n", data&PULSE_BIT ? 'p':'s',
	   data&PULSE_MASK);
	 */
	return data;
}



static struct lengths *new_length(lirc_t length)
{
	struct lengths *l;

	l = malloc(sizeof(struct lengths));
	if (l == NULL)
		return (NULL);
	l->count = 1;
	l->sum = length;
	l->lower_bound = length / 100 * 100;
	l->upper_bound = length / 100 * 100 + 99;
	l->min = l->max = length;
	l->next = NULL;
	return (l);
}


static void unlink_length(struct lengths **first, struct lengths *remove)
{
	struct lengths *last, *scan;

	if (remove == *first) {
		*first = remove->next;
		remove->next = NULL;
		return;
	} else {
		scan = (*first)->next;
		last = *first;
		while (scan) {
			if (scan == remove) {
				last->next = remove->next;
				remove->next = NULL;
				return;
			}
			last = scan;
			scan = scan->next;
		}
	}
	printf("unlink_length(): report this bug!\n");
}


static int add_length(struct lengths **first, lirc_t length)
{
	struct lengths *l, *last;

	if (*first == NULL) {
		*first = new_length(length);
		if (*first == NULL)
			return (0);
		return (1);
	}
	l = *first;
	while (l != NULL) {
		if (l->lower_bound <= length && length <= l->upper_bound) {
			l->count++;
			l->sum += length;
			l->min = min(l->min, length);
			l->max = max(l->max, length);
			return (1);
		}
		last = l;
		l = l->next;
	}
	last->next = new_length(length);
	if (last->next == NULL)
		return (0);
	return (1);
}


static void free_lengths(struct lengths **firstp)
{
	struct lengths *first, *next;

	first = *firstp;
	if (first == NULL)
		return;
	while (first != NULL) {
		next = first->next;
		free(first);
		first = next;
	}
	*firstp = NULL;
}


static void free_all_lengths(void)
{
	free_lengths(&first_space);
	free_lengths(&first_pulse);
	free_lengths(&first_sum);
	free_lengths(&first_gap);
	free_lengths(&first_repeat_gap);
	free_lengths(&first_signal_length);
	free_lengths(&first_headerp);
	free_lengths(&first_headers);
	free_lengths(&first_1lead);
	free_lengths(&first_3lead);
	free_lengths(&first_trail);
	free_lengths(&first_repeatp);
	free_lengths(&first_repeats);
}


static void merge_lengths(struct lengths *first)
{
	struct lengths *l, *inner, *last;
	__u32 new_sum;
	int new_count;

	l = first;
	while (l != NULL) {
		last = l;
		inner = l->next;
		while (inner != NULL) {
			new_sum = l->sum + inner->sum;
			new_count = l->count + inner->count;

			if ((l->max <= new_sum / new_count + aeps && l->min + aeps >= new_sum / new_count
			     && inner->max <= new_sum / new_count + aeps && inner->min + aeps >= new_sum / new_count)
			    || (l->max <= new_sum / new_count * (100 + eps)
				&& l->min >= new_sum / new_count * (100 - eps)
				&& inner->max <= new_sum / new_count * (100 + eps)
				&& inner->min >= new_sum / new_count * (100 - eps))) {
				l->sum = new_sum;
				l->count = new_count;
				l->upper_bound = max(l->upper_bound, inner->upper_bound);
				l->lower_bound = min(l->lower_bound, inner->lower_bound);
				l->min = min(l->min, inner->min);
				l->max = max(l->max, inner->max);

				last->next = inner->next;
				free(inner);
				inner = last;
			}
			last = inner;
			inner = inner->next;
		}
		l = l->next;
	}
	if (lirc_log_is_enabled_for(LIRC_DEBUG)) {
		l = first;
		while (l != NULL) {
			printf("%d x %u [%u,%u]\n",
			       l->count, (__u32) calc_signal(l), (__u32) l->min, (__u32) l->max);
			l = l->next;
		}
	}
}


static struct lengths *get_max_length(struct lengths *first, unsigned int *sump)
{
	unsigned int sum;
	struct lengths *scan, *max_length;

	if (first == NULL)
		return (NULL);
	max_length = first;
	sum = first->count;

	if (lirc_log_is_enabled_for(LIRC_DEBUG)) {
		if (first->count > 0)
			printf("%u x %u\n", first->count, (__u32) calc_signal(first));
	}
	scan = first->next;
	while (scan) {
		if (scan->count > max_length->count) {
			max_length = scan;
		}
		sum += scan->count;
		if (lirc_log_is_enabled_for(LIRC_DEBUG) && scan->count > 0) {
			printf("%u x %u\n", scan->count, (__u32) calc_signal(scan));
		}
		scan = scan->next;
	}
	if (sump != NULL)
		*sump = sum;
	return (max_length);
}


static int get_trail_length(struct ir_remote *remote, int interactive)
{
	unsigned int sum = 0, max_count;
	struct lengths *max_length;

	if (is_biphase(remote))
		return (1);

	max_length = get_max_length(first_trail, &sum);
	max_count = max_length->count;
	if (lirc_log_is_enabled_for(LIRC_DEBUG)) {
		logprintf(LIRC_DEBUG, "get_trail_length(): sum: %u, max_count %u\n",
			  sum, max_count);
	}
	if (max_count >= sum * TH_TRAIL / 100) {
		i_printf(interactive, "Found trail pulse: %lu\n", (__u32) calc_signal(max_length));
		remote->ptrail = calc_signal(max_length);
		return (1);
	}
	i_printf(interactive, "No trail pulse found.\n");
	return (1);
}


static int get_lead_length(struct ir_remote *remote, int interactive)
{
	unsigned int sum = 0, max_count;
	struct lengths *first_lead, *max_length, *max2_length;
	lirc_t a, b, swap;

	if (!is_biphase(remote) || has_header(remote))
		return (1);
	if (is_rc6(remote))
		return (1);

	first_lead = has_header(remote) ? first_3lead : first_1lead;
	max_length = get_max_length(first_lead, &sum);
	max_count = max_length->count;
	if (lirc_log_is_enabled_for(LIRC_DEBUG)) {
		printf("get_lead_length(): sum: %u, max_count %u\n", sum, max_count);
	}
	if (max_count >= sum * TH_LEAD / 100) {
		i_printf(interactive, "Found lead pulse: %lu\n", (__u32) calc_signal(max_length));
		remote->plead = calc_signal(max_length);
		return (1);
	}
	unlink_length(&first_lead, max_length);
	max2_length = get_max_length(first_lead, &sum);
	max_length->next = first_lead;
	first_lead = max_length;

	a = calc_signal(max_length);
	b = calc_signal(max2_length);
	if (a > b) {
		swap = a;
		a = b;
		b = swap;
	}
	if (abs(2 * a - b) < b * eps / 100 || abs(2 * a - b) < aeps) {
		i_printf(interactive, "Found hidden lead pulse: %lu\n", (__u32) a);
		remote->plead = a;
		return (1);
	}
	i_printf(interactive, "No lead pulse found.\n");
	return (1);
}


static int get_header_length(struct ir_remote *remote, int interactive)
{
	unsigned int sum, max_count;
	lirc_t headerp, headers;
	struct lengths *max_plength, *max_slength;

	if (first_headerp != NULL) {
		max_plength = get_max_length(first_headerp, &sum);
		max_count = max_plength->count;
	} else {
		i_printf(interactive, "No header data.\n");
		return (1);
	}
	if (lirc_log_is_enabled_for(LIRC_DEBUG)) {
		printf("get_header_length(): sum: %u, max_count %u\n", sum, max_count);
	}

	if (max_count >= sum * TH_HEADER / 100) {
		max_slength = get_max_length(first_headers, &sum);
		max_count = max_slength->count;
		if (lirc_log_is_enabled_for(LIRC_DEBUG)) {
			printf("get_header_length(): sum: %u, max_count %u\n", sum, max_count);
		}
		if (max_count >= sum * TH_HEADER / 100) {
			headerp = calc_signal(max_plength);
			headers = calc_signal(max_slength);

			i_printf(interactive, "Found possible header: %lu %lu\n", (__u32) headerp, (__u32) headers);
			remote->phead = headerp;
			remote->shead = headers;
			if (first_lengths < second_lengths) {
				i_printf(interactive, "Header is not being repeated.\n");
				remote->flags |= NO_HEAD_REP;
			}
			return (1);
		}
	}
	i_printf(interactive, "No header found.\n");
	return (1);
}


static int get_repeat_length(struct ir_remote *remote, int interactive)
{
	unsigned int sum = 0, max_count;
	lirc_t repeatp, repeats, repeat_gap;
	struct lengths *max_plength, *max_slength;

	if (!((count_3repeats > SAMPLES / 2 ? 1 : 0) ^ (count_5repeats > SAMPLES / 2 ? 1 : 0))) {
		if (count_3repeats > SAMPLES / 2 || count_5repeats > SAMPLES / 2) {
			printf("Repeat inconsitentcy.\n");
			return (0);
		}
		i_printf(interactive, "No repeat code found.\n");
		return (1);
	}

	max_plength = get_max_length(first_repeatp, &sum);
	max_count = max_plength->count;
	if (lirc_log_is_enabled_for(LIRC_DEBUG)) {
		printf("get_repeat_length(): sum: %u, max_count %u\n", sum, max_count);
	}

	if (max_count >= sum * TH_REPEAT / 100) {
		max_slength = get_max_length(first_repeats, &sum);
		max_count = max_slength->count;
		if (lirc_log_is_enabled_for(LIRC_DEBUG)) {
			printf("get_repeat_length(): sum: %u, max_count %u\n", sum, max_count);
		}
		if (max_count >= sum * TH_REPEAT / 100) {
			if (count_5repeats > count_3repeats && !has_header(remote)) {
				printf("Repeat code has header," " but no header found!\n");
				return (0);
			}
			if (count_5repeats > count_3repeats && has_header(remote)) {
				remote->flags |= REPEAT_HEADER;
			}
			repeatp = calc_signal(max_plength);
			repeats = calc_signal(max_slength);

			i_printf(interactive, "Found repeat code: %lu %lu\n", (__u32) repeatp, (__u32) repeats);
			remote->prepeat = repeatp;
			remote->srepeat = repeats;
			if (!(remote->flags & CONST_LENGTH)) {
				max_slength = get_max_length(first_repeat_gap, NULL);
				repeat_gap = calc_signal(max_slength);
				i_printf(interactive, "Found repeat gap: %lu\n", (__u32) repeat_gap);
				remote->repeat_gap = repeat_gap;

			}
			return (1);
		}
	}
	i_printf(interactive, "No repeat header found.\n");
	return (1);
}


static void get_scheme(struct ir_remote *remote, int interactive)
{
	unsigned int i, length = 0, sum = 0;

	for (i = 1; i < MAX_SIGNALS; i++) {
		if (lengths[i] > lengths[length]) {
			length = i;
		}
		sum += lengths[i];
		if (lirc_log_is_enabled_for(LIRC_DEBUG)) {
			if (lengths[i] > 0)
				printf("%u: %u\n", i, lengths[i]);
		}
	}
	if (lirc_log_is_enabled_for(LIRC_DEBUG)) {
		printf("get_scheme(): sum: %u length: %u signals: %u\n"
		       "first_lengths: %u second_lengths: %u\n",
			sum, length + 1, lengths[length], first_lengths, second_lengths);
	}
	/* FIXME !!! this heuristic is too bad */
	if (lengths[length] >= TH_SPACE_ENC * sum / 100) {
		length++;
		i_printf(interactive, "Space/pulse encoded remote control found.\n");
		i_printf(interactive, "Signal length is %u.\n", length);
		/* this is not yet the
		   number of bits */
		remote->bits = length;
		set_protocol(remote, SPACE_ENC);
		return;
	} else {
		struct lengths *maxp, *max2p, *maxs, *max2s;

		maxp = get_max_length(first_pulse, NULL);
		unlink_length(&first_pulse, maxp);
		if (first_pulse == NULL) {
			first_pulse = maxp;
		} else {
			max2p = get_max_length(first_pulse, NULL);
			maxp->next = first_pulse;
			first_pulse = maxp;

			maxs = get_max_length(first_space, NULL);
			unlink_length(&first_space, maxs);
			if (first_space == NULL) {
				first_space = maxs;
			} else {
				max2s = get_max_length(first_space, NULL);
				maxs->next = first_space;
				first_space = maxs;

				maxs = get_max_length(first_space, NULL);

				if (length > 20
				    && (calc_signal(maxp) < TH_RC6_SIGNAL || calc_signal(max2p) < TH_RC6_SIGNAL)
				    && (calc_signal(maxs) < TH_RC6_SIGNAL || calc_signal(max2s) < TH_RC6_SIGNAL)) {
					i_printf(interactive, "RC-6 remote control found.\n");
					set_protocol(remote, RC6);
				} else {
					i_printf(interactive, "RC-5 remote control found.\n");
					set_protocol(remote, RC5);
				}
				return;
			}
		}
	}
	length++;
	i_printf(interactive, "Suspicious data length: %u.\n", length);
	/* this is not yet the number of bits */
	remote->bits = length;
	set_protocol(remote, SPACE_ENC);
}


static int get_data_length(struct ir_remote *remote, int interactive)
{
	unsigned int sum = 0, max_count;
	lirc_t p1, p2, s1, s2;
	struct lengths *max_plength, *max_slength;
	struct lengths *max2_plength, *max2_slength;

	max_plength = get_max_length(first_pulse, &sum);
	max_count = max_plength->count;
	if (lirc_log_is_enabled_for(LIRC_DEBUG)) {
		printf("get_data_length(): sum: %u, max_count %u\n", sum, max_count);
	}

	if (max_count >= sum * TH_IS_BIT / 100) {
		unlink_length(&first_pulse, max_plength);

		max2_plength = get_max_length(first_pulse, NULL);
		if (max2_plength != NULL) {
			if (max2_plength->count < max_count * TH_IS_BIT / 100)
				max2_plength = NULL;
		}
		if (lirc_log_is_enabled_for(LIRC_DEBUG)) {
			printf("Pulse candidates: ");
			printf("%u x %u", max_plength->count, (__u32) calc_signal(max_plength));
			if (max2_plength)
				printf(", %u x %u", max2_plength->count, (__u32)
				       calc_signal(max2_plength));
			printf("\n");
		}

		max_slength = get_max_length(first_space, &sum);
		max_count = max_slength->count;
		if (lirc_log_is_enabled_for(LIRC_DEBUG)) {
			printf("get_data_length(): sum: %u, max_count %u\n", sum, max_count);
		}
		if (max_count >= sum * TH_IS_BIT / 100) {
			unlink_length(&first_space, max_slength);

			max2_slength = get_max_length(first_space, NULL);
			if (max2_slength != NULL) {
				if (max2_slength->count < max_count * TH_IS_BIT / 100)
					max2_slength = NULL;
			}
			if (max_count >= sum * TH_IS_BIT / 100 &&
                            lirc_log_is_enabled_for(LIRC_DEBUG)) {
				printf("Space candidates: ");
				printf("%u x %u", max_slength->count, (__u32) calc_signal(max_slength));
				if (max2_slength)
					printf(", %u x %u",
					       max2_slength->count, (__u32) calc_signal(max2_slength));
				printf("\n");
			}
			remote->eps = eps;
			remote->aeps = aeps;
			if (is_biphase(remote)) {
				if (max2_plength == NULL || max2_slength == NULL) {
					printf("Unknown encoding found.\n");
					return (0);
				}
				i_printf(interactive, "Signals are biphase encoded.\n");
				p1 = calc_signal(max_plength);
				p2 = calc_signal(max2_plength);
				s1 = calc_signal(max_slength);
				s2 = calc_signal(max2_slength);

				remote->pone = (min(p1, p2) + max(p1, p2) / 2) / 2;
				remote->sone = (min(s1, s2) + max(s1, s2) / 2) / 2;
				remote->pzero = remote->pone;
				remote->szero = remote->sone;
			} else {
				if (max2_plength == NULL && max2_slength == NULL) {
					printf("No encoding found.\n");
					return (0);
				}
				if (max2_plength && max2_slength) {
					printf("Unknown encoding found.\n");
					return (0);
				}
				p1 = calc_signal(max_plength);
				s1 = calc_signal(max_slength);
				if (max2_plength) {
					p2 = calc_signal(max2_plength);
					i_printf(interactive, "Signals are pulse encoded.\n");
					remote->pone = max(p1, p2);
					remote->sone = s1;
					remote->pzero = min(p1, p2);
					remote->szero = s1;
					if (expect(remote, remote->ptrail, p1) || expect(remote, remote->ptrail, p2)) {
						remote->ptrail = 0;
					}
				} else {
					s2 = calc_signal(max2_slength);
					i_printf(interactive, "Signals are space encoded.\n");
					remote->pone = p1;
					remote->sone = max(s1, s2);
					remote->pzero = p1;
					remote->szero = min(s1, s2);
				}
			}
			if (has_header(remote) && (!has_repeat(remote) || remote->flags & NO_HEAD_REP)
			    ) {
				if (!is_biphase(remote)
				    &&
				    ((expect(remote, remote->phead, remote->pone)
				      && expect(remote, remote->shead, remote->sone))
				     || (expect(remote, remote->phead, remote->pzero)
					 && expect(remote, remote->shead, remote->szero)))) {
					remote->phead = remote->shead = 0;
					remote->flags &= ~NO_HEAD_REP;
					i_printf(interactive, "Removed header.\n");
				}
				if (is_biphase(remote) && expect(remote, remote->shead, remote->sone)) {
					remote->plead = remote->phead;
					remote->phead = remote->shead = 0;
					remote->flags &= ~NO_HEAD_REP;
					i_printf(interactive, "Removed header.\n");
				}
			}
			if (is_biphase(remote)) {
				struct lengths *signal_length;
				lirc_t data_length;

				signal_length = get_max_length(first_signal_length, NULL);
				data_length =
				    calc_signal(signal_length) - remote->plead - remote->phead - remote->shead +
				    /* + 1/2 bit */
				    (remote->pone + remote->sone) / 2;
				remote->bits = data_length / (remote->pone + remote->sone);
				if (is_rc6(remote))
					remote->bits--;
			} else {
				remote->bits =
				    (remote->bits - (has_header(remote) ? 2 : 0) + 1 -
				     (remote->ptrail > 0 ? 2 : 0)) / 2;
			}
			i_printf(interactive, "Signal length is %d\n", remote->bits);
			free_lengths(&max_plength);
			free_lengths(&max_slength);
			return (1);
		}
		free_lengths(&max_plength);
	}
	printf("Could not find data lengths.\n");
	return (0);
}


static enum get_gap_status
get_gap_length(struct gap_state *state, struct ir_remote *remote)
{
	while (availabledata()) {
		curr_driver->rec_func(NULL);
	}
	if (!mywaitfordata(10000000)) {
		free_lengths(&(state->gaps));
		return STS_GAP_TIMEOUT;
	}
	gettimeofday(&(state->start), NULL);
	while (availabledata()) {
		curr_driver->rec_func(NULL);
	}
	gettimeofday(&(state->end), NULL);
	if (state->flag) {
		state->gap = time_elapsed(&(state->last),
					  &(state->start));
		add_length(&(state->gaps), state->gap);
		merge_lengths(state->gaps);
		state->maxcount = 0;
		state->scan = state->gaps;
		while (state->scan) {
			state->maxcount = max(state->maxcount, state->scan->count);
			if (state->scan->count > SAMPLES) {
				remote->gap = calc_signal(state->scan);
				free_lengths(&(state->gaps));
				return STS_GAP_FOUND;
			}
			state->scan = state->scan->next;
		}
		if (state->maxcount > state->lastmaxcount) {
			state->lastmaxcount = state->maxcount;
			return STS_GAP_GOT_ONE_PRESS;
		}
	} else {
		state->flag = 1;
	}
	state->last = state->end;
	return STS_GAP_AGAIN;
}


/** Return true if a given remote needs to compute toggle_mask. */
int needs_toggle_mask(struct ir_remote* remote)
{
	struct ir_ncode* codes;

	if (! is_rc6(remote))
		return 0;
	if (remote->codes) {
		codes = remote->codes;
		while (codes->name != NULL) {
			if (codes->next) {
				/* asume no toggle bit mask when key
				   sequences are used */
				return 0;
			}
			codes++;
		}
	}
	return 1;
}




/** Check options, possibly run simple ones. Returns status. */
static enum init_status init(struct opts* opts, struct main_state* state)
{
	char filename_new[256];
	char logpath[256];
	int flags;

	hw_choose_driver(NULL);
	if (!opts->analyse && hw_choose_driver(opts->driver) != 0) {
		return STS_INIT_BAD_DRIVER;
	}
	ir_remote_init(opts->dynamic_codes);
	lirc_log_get_clientlog("irrecord", logpath, sizeof(logpath));
	lirc_log_set_file(logpath);
	lirc_log_open("irrecord", 0, opts->loglevel);
	curr_driver->open_func(opts->device);
	if (strcmp(curr_driver->name, "null") == 0 && !opts->analyse) {
		return STS_INIT_NO_DRIVER;
	}
	state->fin = fopen(opts->filename, "r");
	if (state->fin != NULL) {

		if (opts->force) {
			return STS_INIT_FORCE_TMPL;
		}
		state->remotes = read_config(state->fin, opts->filename);
		fclose(state->fin);
		if (state->remotes == (void *)-1 || state->remotes == NULL) {
			return STS_INIT_BAD_FILE;
		}
		state->using_template = 1;
		if (opts->analyse) {
			return STS_INIT_ANALYZE;
		}
		if (opts->test) {
			if (opts->trail)
				for_each_remote(state->remotes, remove_trail);
			for_each_remote(state->remotes, remove_pre_data);
			for_each_remote(state->remotes, remove_post_data);
			if (opts->get_pre)
				for_each_remote(state->remotes, get_pre_data);
			if (opts->get_post)
				for_each_remote(state->remotes, get_post_data);
			if (opts->invert)
				for_each_remote(state->remotes, invert_data);

			fprint_remotes(stdout, state->remotes, state->commandline);
			free_config(state->remotes);
			return (STS_INIT_TESTED);
		}
		remote = *(state->remotes);
		remote.name = opts->filename;
		remote.codes = NULL;
		remote.last_code = NULL;
		remote.next = NULL;
		if (remote.pre_p == 0 && remote.pre_s == 0 && remote.post_p == 0 && remote.post_s == 0) {
			remote.bits = bit_count(&remote);
			remote.pre_data_bits = 0;
			remote.post_data_bits = 0;
		}
		if (state->remotes->next != NULL) {
			fprintf(stderr, "%s: only first remote definition in file \"%s\" used\n", progname,
				opts->filename);
		}
		snprintf(filename_new, sizeof(filename_new), "%s.conf", opts->filename);
		opts->filename = strdup(filename_new);
	} else {
		if (opts->analyse) {
			fprintf(stderr, "%s: no input file given, ignoring analyse flag\n", progname);
			opts->analyse = 0;
		}
	}
	state->fout = fopen(opts->filename, "w");
	if (state->fout == NULL) {
		return STS_INIT_FOPEN;
	}
	if (curr_driver->init_func) {
		if (!curr_driver->init_func()) {
			fclose(state->fout);
			unlink(opts->filename);
			return STS_INIT_HW_FAIL;
		}
	}
	aeps = (curr_driver->resolution > aeps ? curr_driver->resolution : aeps);
	if (curr_driver->rec_mode != LIRC_MODE_MODE2 && curr_driver->rec_mode != LIRC_MODE_LIRCCODE) {
		return STS_INIT_BAD_MODE;
		fclose(state->fout);
		unlink(opts->filename);
		if (curr_driver->deinit_func)
			curr_driver->deinit_func();
		return STS_INIT_BAD_MODE;
	}

	flags = fcntl(curr_driver->fd, F_GETFL, 0);
	if (flags == -1 || fcntl(curr_driver->fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		fprintf(stderr, "%s: could not set O_NONBLOCK flag\n", progname);
		fclose(state->fout);
		unlink(opts->filename);
		if (curr_driver->deinit_func)
			curr_driver->deinit_func();
		return STS_INIT_O_NONBLOCK;
	}

	return STS_INIT_OK;
}


static enum lengths_status get_lengths(struct lengths_state* state,
				       struct ir_remote *remote,
				       int force, int interactive)
{
	int i;
	struct lengths *scan;
	int maxcount = 0;
	static int lastmaxcount = 0;

	state->data = curr_driver->readdata(10000000);
	if (!state->data) {
		fprintf(stderr, "%s: no data for 10 secs, aborting\n", progname);
		state->retval = 0;
		return STS_LEN_TIMEOUT;
	}
	state->count++;
	if (state->mode == MODE_GET_GAP) {
		state->sum += state->data & PULSE_MASK;
		if (state->average == 0 && is_space(state->data)) {
			if (state->data > 100000) {
				state->sum = 0;
				return STS_LEN_AGAIN;
			}
			state->average = state->data;
			state->maxspace = state->data;
		} else if (is_space(state->data)) {
			if (state->data > MIN_GAP
				|| state->data > 100 * state->average
				/* this MUST be a gap */
				|| (state->data >= 5000 && count_spaces > 10
					&& state->data > 5 * state->average)
				|| (state->data < 5000 && count_spaces > 10
					&& state->data > 5 * state->maxspace / 2))
			{
				add_length(&first_sum, state->sum);
				merge_lengths(first_sum);
				add_length(&first_gap, state->data);
				merge_lengths(first_gap);
				state->sum = 0;
				count_spaces = 0;
				state->average = 0;
				state->maxspace = 0;

				maxcount = 0;
				scan = first_sum;
				while (scan) {
					maxcount = max(maxcount, scan->count);
					if (scan->count > SAMPLES) {
						remote->gap = calc_signal(scan);
						remote->flags |= CONST_LENGTH;
						logprintf(LIRC_DEBUG,
							  "Found const length: %u",
							  (__u32) remote->gap);
						break;
					}
					scan = scan->next;
				}
				if (scan == NULL) {
					scan = first_gap;
					while (scan) {
						maxcount = max(maxcount, scan->count);
						if (scan->count > SAMPLES) {
							remote->gap = calc_signal(scan);
							state->mode = MODE_HAVE_GAP;
							logprintf(LIRC_DEBUG,
								  "Found gap: %u",
								  (__u32) remote->gap);
							break;
						}
						scan = scan->next;
					}
				}
				if (scan != NULL) {
					i_printf(interactive,
						 "Please keep on pressing buttons like described above.\n");
					state->mode = MODE_HAVE_GAP;
					state->sum = 0;
					state->count = 0;
					state->remaining_gap =
						 is_const(remote) ?
							(remote->gap > state->data ?
								remote->gap - state->data : 0)
							:(has_repeat_gap(remote) ?
								remote-> repeat_gap : remote->gap);
					if (force) {
						state->retval = 0;
						return STS_LEN_RAW_OK;
					}
					return STS_LEN_AGAIN;
				}
				lastmaxcount = maxcount;
				state->keypresses = lastmaxcount;
				return STS_LEN_AGAIN;
			}
			state->average = (state->average * count_spaces + state->data)
			    / (count_spaces + 1);
			count_spaces++;
			if (state->data > state->maxspace) {
				state->maxspace = state->data;
			}
		}
		if (state->count > SAMPLES * MAX_SIGNALS * 2) {
			state->retval = 0;
			return STS_LEN_NO_GAP_FOUND;
		} else {
			state->keypresses = lastmaxcount;
			return STS_LEN_AGAIN;
		}
	} else if (state->mode == MODE_HAVE_GAP) {
		if (state->count <= MAX_SIGNALS) {
			signals[state->count - 1] = state->data & PULSE_MASK;
		} else {
			state->retval = 0;
			return STS_LEN_TOO_LONG;
		}
		if (is_const(remote)) {
			state->remaining_gap = remote->gap > state->sum ? remote->gap - state->sum : 0;
		} else {
			state->remaining_gap = remote->gap;
		}
		state->sum += state->data & PULSE_MASK;

		if (state->count > 2
		    && ((state->data & PULSE_MASK) >= state->remaining_gap * (100 - eps) / 100
			|| (state->data & PULSE_MASK) >= state->remaining_gap - aeps)) {
			if (is_space(state->data)) {
				/* signal complete */
				state->keypresses += 1;
				if (state->count == 4) {
					count_3repeats++;
					add_length(&first_repeatp, signals[0]);
					merge_lengths(first_repeatp);
					add_length(&first_repeats, signals[1]);
					merge_lengths(first_repeats);
					add_length(&first_trail, signals[2]);
					merge_lengths(first_trail);
					add_length(&first_repeat_gap, signals[3]);
					merge_lengths(first_repeat_gap);
				} else if (state->count == 6) {
					count_5repeats++;
					add_length(&first_headerp, signals[0]);
					merge_lengths(first_headerp);
					add_length(&first_headers, signals[1]);
					merge_lengths(first_headers);
					add_length(&first_repeatp, signals[2]);
					merge_lengths(first_repeatp);
					add_length(&first_repeats, signals[3]);
					merge_lengths(first_repeats);
					add_length(&first_trail, signals[4]);
					merge_lengths(first_trail);
					add_length(&first_repeat_gap, signals[5]);
					merge_lengths(first_repeat_gap);
				} else if (state->count > 6) {
					count_signals++;
					add_length(&first_1lead, signals[0]);
					merge_lengths(first_1lead);
					for (i = 2; i < state->count - 2; i++) {
						if (i % 2) {
							add_length(&first_space, signals[i]);
							merge_lengths(first_space);
						} else {
							add_length(&first_pulse, signals[i]);
							merge_lengths(first_pulse);
						}
					}
					add_length(&first_trail, signals[state->count - 2]);
					merge_lengths(first_trail);
					lengths[state->count - 2]++;
					add_length(&first_signal_length, state->sum - state->data);
					merge_lengths(first_signal_length);
					if (state->first_signal == 1
					    || (first_length > 2 && first_length - 2 != state->count - 2)) {
						add_length(&first_3lead, signals[2]);
						merge_lengths(first_3lead);
						add_length(&first_headerp, signals[0]);
						merge_lengths(first_headerp);
						add_length(&first_headers, signals[1]);
						merge_lengths(first_headers);
					}
					if (state->first_signal == 1) {
						first_lengths++;
						first_length = state->count - 2;
						state->header = signals[0] + signals[1];
					} else if (state->first_signal == 0 && first_length - 2 == state->count - 2) {
						lengths[state->count - 2]--;
						lengths[state->count - 2 + 2]++;
						second_lengths++;
					}
				}
				state->count = 0;
				state->sum = 0;
			}

			/* such long pulses may appear with
			   crappy hardware (receiver? / remote?)
			 */
			else {
				remote->gap = 0;
				return STS_LEN_NO_GAP_FOUND;
			}

			if (count_signals >= SAMPLES) {
				i_printf(interactive, "\n");
				get_scheme(remote, interactive);
				if (!get_header_length(remote, interactive)
				    || !get_trail_length(remote, interactive)
				    || !get_lead_length(remote, interactive)
				    || !get_repeat_length(remote, interactive)
				    || !get_data_length(remote, interactive)) {
					state->retval = 0;
				}
				return state->retval == 0 ? STS_LEN_FAIL : STS_LEN_OK;
			}
			if ((state->data & PULSE_MASK) <= (state->remaining_gap + state->header) * (100 + eps) / 100
			    || (state->data & PULSE_MASK) <= (state->remaining_gap + state->header) + aeps) {
				state->first_signal = 0;
				state->header = 0;
			} else {
				state->first_signal = 1;
			}
		}
	}
	return STS_LEN_AGAIN;
}


enum toggle_status
get_toggle_bit_mask(struct toggle_state* state, struct ir_remote* remote)
{
	struct decode_ctx_t decode_ctx;
	int i;
	ir_code mask;

	if (!state->inited) {
		sleep(1);
		while (availabledata()) {
			curr_driver->rec_func(NULL);
		}
		state->inited = 1;
	}
	if (state->retries <= 0) {
		if (!state->found)
			return STS_TGL_NOT_FOUND;
		if (state->seq > 0)
			remote->min_repeat = state->repeats / state->seq;
       		logprintf(LIRC_DEBUG, "min_repeat=%d\n", remote->min_repeat);
		return STS_TGL_FOUND;
	}
	if (!mywaitfordata(10000000))
		return STS_TGL_TIMEOUT;
	curr_driver->rec_func(remote);
	if (is_rc6(remote) && remote->rc6_mask == 0) {
		for (i = 0, mask = 1; i < remote->bits; i++, mask <<= 1) {
			remote->rc6_mask = mask;
			state->success = curr_driver->decode_func(remote,
								  &decode_ctx);
			if (state->success) {
				remote->min_remaining_gap =
					decode_ctx.min_remaining_gap;
				remote->max_remaining_gap =
					decode_ctx.max_remaining_gap;
				break;
			}
		}
		if (!state->success)
			remote->rc6_mask = 0;
	} else {
		state->success =
			curr_driver->decode_func(remote,
						 &decode_ctx);
		if (state->success) {
			remote->min_remaining_gap =
				decode_ctx.min_remaining_gap;
			remote->max_remaining_gap =
				decode_ctx.max_remaining_gap;
		}
	}
	if (state->success) {
		if (state->flag == 0) {
			state->flag = 1;
			state->first = decode_ctx.code;
		} else if (!decode_ctx.repeat_flag
			   || decode_ctx.code != state->last) {
			state->seq++;
			if (!state->found && state->first ^ decode_ctx.code) {
				set_toggle_bit_mask(remote, state->first ^ decode_ctx.code);
				state->found = 1;
				if (state->seq > 0)
					remote->min_repeat = state->repeats / state->seq;
			}
			state->retries--;
			state->last = decode_ctx.code;
			return STS_TGL_GOT_ONE_PRESS;
		} else {
			state->repeats++;
		}
		state->last = decode_ctx.code;
	} else {
		state->retries--;
		while (availabledata())
			curr_driver->rec_func(NULL);
	}
	return STS_TGL_AGAIN;
}


/** analyse non-interactive  get_lengths. */
void analyse_get_lengths(struct lengths_state* lengths_state)
{
	enum lengths_status status = STS_LEN_AGAIN;

	while (status == STS_LEN_AGAIN) {
		status = get_lengths(lengths_state, &remote, 0, 0);
		switch (status) {
		case STS_LEN_AGAIN:
			break;
		case STS_LEN_OK:
			break;
		case STS_LEN_FAIL:
			logprintf(LIRC_ERROR, "get_lengths() failure");
			return;
		case STS_LEN_RAW_OK:
			logprintf(LIRC_ERROR, "raw analyse result?!");
			return;
		case STS_LEN_TIMEOUT:
			logprintf(LIRC_ERROR, "analyse timeout?!");
			return;
		case STS_LEN_NO_GAP_FOUND:
			logprintf(LIRC_ERROR, "analyse, no gap?!");
			return;
		case STS_LEN_TOO_LONG:
			logprintf(LIRC_ERROR, "analyse, signal too long?!");
			return;
		default:
			printf("Cannot read raw data (%d)\n", status);
			exit(EXIT_FAILURE);
		}
	}
}


static void analyse_remote(struct ir_remote *raw_data)
{
	struct ir_ncode *codes;
	struct decode_ctx_t decode_ctx;
	struct lengths_state lengths_state;
	int code;
	int code2;
	struct ir_ncode *new_codes;
	size_t new_codes_count = 100;
	int new_index = 0;
	int ret;

	if (!is_raw(raw_data)) {
		fprintf(stderr, "%s: remote %s not in raw mode, ignoring\n", progname, raw_data->name);
		return;
	}
	aeps = raw_data->aeps;
	eps = raw_data->eps;
	emulation_data = raw_data;
	next_code = NULL;
	current_code = NULL;
	current_index = 0;
	memset(&remote, 0, sizeof(remote));
	lengths_state_init(&lengths_state, 0);
	analyse_get_lengths(&lengths_state);

	if (is_rc6(&remote) && remote.bits >= 5) {
		/* have to assume something as it's very difficult to
		   extract the rc6_mask from the data that we have */
		remote.rc6_mask = ((ir_code) 0x1ll) << (remote.bits - 5);
	}

	remote.name = raw_data->name;
	remote.freq = raw_data->freq;

	new_codes = malloc(new_codes_count * sizeof(*new_codes));
	if (new_codes == NULL) {
		fprintf(stderr, "%s: out of memory\n", progname);
		return;
	}
	memset(new_codes, 0, new_codes_count * sizeof(*new_codes));
	codes = raw_data->codes;
	while (codes->name != NULL) {
		// printf("decoding %s\n", codes->name);
		current_code = NULL;
		current_index = 0;
		next_code = codes;

		rec_buffer_init();

		ret = receive_decode(&remote, &decode_ctx);
		if (!ret) {
			fprintf(stderr, "%s: decoding of %s failed\n", progname, codes->name);
		} else {
			if (new_index + 1 >= new_codes_count) {
				struct ir_ncode *renew_codes;

				new_codes_count *= 2;
				renew_codes = realloc(new_codes, new_codes_count * sizeof(*new_codes));
				if (renew_codes == NULL) {
					fprintf(stderr, "%s: out of memory\n", progname);
					free(new_codes);
					return;
				}
				memset(&new_codes[new_codes_count / 2], 0, new_codes_count / 2 * sizeof(*new_codes));
				new_codes = renew_codes;
			}

			rec_buffer_clear();
			code = decode_ctx.code;
			ret = receive_decode(&remote, &decode_ctx);
			code2 = decode_ctx.code;
			decode_ctx.code = code;
			if (ret && code2 != decode_ctx.code) {
				new_codes[new_index].next = malloc(sizeof(*(new_codes[new_index].next)));
				if (new_codes[new_index].next) {
					memset(new_codes[new_index].next, 0, sizeof(*(new_codes[new_index].next)));
					new_codes[new_index].next->code = code2;
				}
			}
			new_codes[new_index].name = codes->name;
			new_codes[new_index].code = decode_ctx.code;
			new_index++;
		}
		codes++;
	}
	new_codes[new_index].name = NULL;
	remote.codes = new_codes;
	fprint_remotes(stdout, &remote, (const char*)NULL);
	remote.codes = NULL;
	free(new_codes);
}


static int
get_options(int argc, char** argv, const char* filename, struct opts* options)
{
	options->force = 0;
	options_load(argc, argv, NULL, parse_options);
	options->analyse = options_getboolean("irrecord:analyse");
	options->device = options_getstring("irrecord:device");
	options->loglevel = string2loglevel(options_getstring("lircd:debug"));
	options->driver = options_getstring("irrecord:driver");
	options->force = options_getboolean("irrecord:force");
	options->disable_namespace = options_getboolean("irrecord:disable-namespace");
	options->dynamic_codes = options_getboolean("lircd:dynamic-codes");
	options->get_pre = options_getboolean("irrecord:pre");
	options->get_post = options_getboolean("irrecord:post");
	options->list_namespace = options_getboolean("irrecord:list_namespace");
	options->test = options_getboolean("irrecord:test");
	options->invert = options_getboolean("irrecord:invert");
	options->trail = options_getboolean("irrecord:trail");
	options->filename = options_getstring("irrecord:filename");
	return 1;
}

/** The --analyse wrapper. */
static void do_analyse(struct opts* opts, struct main_state* state)
{
	FILE* f;
	struct ir_remote* r;

	memcpy((void*)curr_driver, &hw_emulation, sizeof(struct driver));
	f  = fopen(opts->filename, "r");
	if (f == NULL) {
		fprintf(stderr, "Cannot open file: %s\n", opts->filename);
		exit(EXIT_FAILURE);
	}
	r = read_config(f, opts->filename);
	if (r == NULL) {
		fprintf(stderr, "Cannot parse file: %s\n", opts->filename);
		exit(EXIT_FAILURE);
	}
	for ( ; r != NULL; r = r->next) {
		analyse_remote(r);
	}
}


/** View part of init: run init() and handle results. Returns or exits. */
static void do_init(struct opts* opts, struct main_state* state)
{
	enum init_status sts;

	sts = init(opts, state);
	switch (sts) {
		case STS_INIT_BAD_DRIVER:
			fprintf(stderr, "Driver `%s' not found", opts->driver);
			fprintf(stderr, " (wrong or missing -U/--plugindir?).\n");
			hw_print_drivers(stderr);
			exit(EXIT_FAILURE);
		case STS_INIT_NO_DRIVER:
			fprintf(stderr,
			       "irrrecord: irrecord does not make sense without hardware\n");
			exit(EXIT_FAILURE);
		case STS_INIT_FORCE_TMPL:
			fprintf(stderr,
				"%s: file \"%s\" already exists\n" "%s: you cannot use the --force option "
				"together with a template file\n", progname, opts->filename, progname);
			exit(EXIT_FAILURE);
		case STS_INIT_BAD_FILE:
			fprintf(stderr,
				"%s: file \"%s\" does not contain valid data\n",
				progname, opts->filename);
			exit(EXIT_FAILURE);
		case STS_INIT_TESTED:
			exit(0);
		case STS_INIT_FOPEN:
			fprintf(stderr, "%s: could not open new config file %s\n",
				progname, opts->filename);
			perror(progname);
			exit(EXIT_FAILURE);
		case STS_INIT_HW_FAIL:
			fprintf(stderr,
				"%s: could not init hardware" " (lircd running ? --> close it, check permissions)\n",
				progname);
			exit(EXIT_FAILURE);
		case STS_INIT_BAD_MODE:
			fprintf(stderr, "%s: mode not supported\n", progname);
			exit(EXIT_FAILURE);
		case STS_INIT_O_NONBLOCK:
			fprintf(stderr, "%s: could not set O_NONBLOCK flag\n", progname);
			exit(EXIT_FAILURE);
		case STS_INIT_ANALYZE:
			do_analyse(opts, state);
			exit(EXIT_SUCCESS);
		case STS_INIT_OK:
			return;
	}
}


/** View part of get_toggle_bit_mask(). */
static void do_get_toggle_bit_mask(struct ir_remote* remote,
				   struct main_state* state,
				   struct opts* opts)
{
	const char* const MISSING_MASK_MSG =
		"But I know for sure that RC6 has a toggle bit!\n";
	enum toggle_status sts;
	struct toggle_state tgl_state;

	printf(MSG_TOGGLE_BIT_INTRO);
	fflush(stdout);
	getchar();
	flushhw();
	toggle_state_init(&tgl_state);
	sts = STS_TGL_AGAIN;
	while (1) {
		switch (sts) {
			case STS_TGL_TIMEOUT:
				fprintf(stderr,
					"Timeout (10 sec), giving up");
				exit(EXIT_FAILURE);
			case STS_TGL_GOT_ONE_PRESS:
				printf(".");
				fflush(stdout);
				sts = STS_TGL_AGAIN;
				continue;
			case STS_TGL_FOUND:
				printf("\nToggle bit mask is 0x%llx.\n",
				       (__u64) remote->toggle_bit_mask);
				if (is_rc6(remote)) {
					printf("RC6 mask is 0x%llx.\n",
					       (__u64) remote->rc6_mask);
				}
				fflush(stdout);
				return;
			case STS_TGL_NOT_FOUND:
				printf("Cannot find any toggle mask.\n");
				if (!is_rc6(remote))
					break;
				printf(MISSING_MASK_MSG);
				fclose(state->fout);
				unlink(opts->filename);
				if (curr_driver->deinit_func)
					curr_driver->deinit_func();
				exit(EXIT_FAILURE);
			case STS_TGL_AGAIN:
				break;
		}
		sts = get_toggle_bit_mask(&tgl_state, remote);
	}
}


void record_buttons(struct main_state* state,
		    struct button_state* btn_state,
		    struct opts* opts)

{
	flushhw();
	while (1) {
		if (btn_state->no_data) {
			fprintf(stderr, "%s: no data for 10 secs," " aborting\n", progname);
			printf("The last button did not seem to generate any signal.\n");
			printf("Press RETURN to continue.\n\n");
			getchar();
			btn_state->no_data = 0;
		}
		printf("\nPlease enter the name for the next button (press <ENTER> to finish recording)\n");
		btn_state->string = fgets(btn_state->buffer, BUTTON, stdin);

		if (btn_state->string != btn_state->buffer) {
			fprintf(stderr, "%s: fgets() failed\n", progname);
			btn_state->retval = EXIT_FAILURE;
			break;
		}
		btn_state->buffer[strlen(btn_state->buffer) - 1] = 0;
		if (strchr(btn_state->buffer, ' ') || strchr(btn_state->buffer, '\t')) {
			printf("The name must not contain any whitespace.\n");
			printf("Please try again.\n");
			continue;
		}
		if (strcasecmp(btn_state->buffer, "begin") == 0 || strcasecmp(btn_state->buffer, "end") == 0) {
			printf("'%s' is not allowed as button name\n", btn_state->buffer);
			printf("Please try again.\n");
			continue;
		}
		if (strlen(btn_state->buffer) == 0) {
			break;
		}
		if (!opts->disable_namespace && !is_in_namespace(btn_state->buffer)) {
			printf("'%s' is not in name space (use --disable-namespace to disable checks)\n", btn_state->buffer);
			printf("Use '%s --list-namespace' to see a full list of valid button names\n", progname);
			printf("Please try again.\n");
			continue;
		}

		if (is_raw(&remote)) {
			flushhw();
		} else {
			while (availabledata()) {
				curr_driver->rec_func(NULL);
			}
		}
		printf("\nNow hold down button \"%s\".\n", btn_state->buffer);
		fflush(stdout);
		flushhw();

		if (is_raw(&remote)) {
			btn_state->count = 0;
			btn_state->sum = 0;
			while (btn_state->count < MAX_SIGNALS) {
				__u32 timeout;

				if (btn_state->count == 0)
					timeout = 10000000;
				else
					timeout = remote.gap * 5;
				btn_state->data = curr_driver->readdata(timeout);
				if (!btn_state->data) {
					if (btn_state->count == 0) {
						btn_state->no_data = 1;
						break;
					}
					btn_state->data = remote.gap;
				}
				if (btn_state->count == 0) {
					if (!is_space(btn_state->data) || btn_state->data < remote.gap - remote.gap * remote.eps / 100) {
						printf("Sorry, something went wrong.\n");
						sleep(3);
						printf("Try again.\n");
						flushhw();
						btn_state->count = 0;
						continue;
					}
				} else {
					if (is_space(btn_state->data)
					    && (is_const(&remote) ? btn_state->data >
						(remote.gap > btn_state->sum ? (remote.gap - btn_state->sum) * (100 - remote.eps) / 100 : 0)
						: btn_state->data > remote.gap * (100 - remote.eps) / 100)) {
						printf("Got it.\n");
						printf("Signal length is %d\n", btn_state->count - 1);
						if (btn_state->count % 2) {
							printf("That's weird because the signal length "
							       "must be odd!\n");
							sleep(3);
							printf("Try again.\n");
							flushhw();
							btn_state->count = 0;
							continue;
						} else {
							ncode.name = btn_state->buffer;
							ncode.length = btn_state->count - 1;
							ncode.signals = signals;
							fprint_remote_signal(state->fout, &remote, &ncode);
							break;
						}
					}
					signals[btn_state->count - 1] = btn_state->data & PULSE_MASK;
					btn_state->sum += btn_state->data & PULSE_MASK;
				}
				btn_state->count++;
			}
			if (btn_state->count == MAX_SIGNALS) {
				printf("Signal is too long.\n");
			}
			if (btn_state->retval == EXIT_FAILURE)
				break;
			continue;
		}
		btn_state->retries = RETRIES;
		while (btn_state->retries > 0) {
			if (!mywaitfordata(10000000)) {
				btn_state->no_data = 1;
				break;
			}
			last_remote = NULL;
			btn_state->flag = 0;
			sleep(1);
			while (availabledata()) {
				curr_driver->rec_func(NULL);
				if (curr_driver->decode_func(&remote, &(state->decode_ctx))) {
					btn_state->flag = 1;
					break;
				}
			}
			if (btn_state->flag) {
				ir_code code2;

				ncode.name = btn_state->buffer;
				ncode.code = state->decode_ctx.code;
				curr_driver->rec_func(NULL);
				if (curr_driver->decode_func(&remote, &(state->decode_ctx))) {
					code2 = state->decode_ctx.code;
					state->decode_ctx.code = ncode.code;
					if (state->decode_ctx.code != code2) {
						ncode.next = malloc(sizeof(*(ncode.next)));
						if (ncode.next) {
							memset(ncode.next, 0, sizeof(*(ncode.next)));
							ncode.next->code = code2;
						}
					}
				}
				fprint_remote_signal(state->fout, &remote, &ncode);
				if (ncode.next) {
					free(ncode.next);
					ncode.next = NULL;
				}
				break;
			} else {
				printf("Something went wrong. ");
				if (btn_state->retries > 1) {
					fflush(stdout);
					sleep(3);
					if (!resethw()) {
						fprintf(stderr, "%s: Could not reset hardware.\n", progname);
						btn_state->retval = EXIT_FAILURE;
						break;
					}
					flushhw();
					printf("Please try again. (%d btn_state->retries left)\n", btn_state->retries - 1);
				} else {
					printf("\n");
					printf("Try using the -f option.\n");
				}
				btn_state->retries--;
				continue;
			}
		}
		if (btn_state->retries == 0)
			btn_state->retval = EXIT_FAILURE;
		if (btn_state->retval == EXIT_FAILURE)
			break;
	}
	fprint_remote_signal_foot(state->fout, &remote);
	fprint_remote_foot(state->fout, &remote);
	fclose(state->fout);

	if (btn_state->retval == EXIT_FAILURE) {
		if (curr_driver->deinit_func)
			curr_driver->deinit_func();
		exit(EXIT_FAILURE);
	}

	if (is_raw(&remote)) {
		return;
	}
	if (!resethw()) {
		fprintf(stderr, "%s: could not reset hardware.\n", progname);
		exit(EXIT_FAILURE);
	}

	state->fin = fopen(opts->filename, "r");
	if (state->fin == NULL) {
		fprintf(stderr, "%s: could not reopen config file\n", progname);
		if (curr_driver->deinit_func)
			curr_driver->deinit_func();
		exit(EXIT_FAILURE);
	}
	state->remotes = read_config(state->fin, opts->filename);
	fclose(state->fin);
	if (state->remotes == NULL) {
		fprintf(stderr, "%s: config file contains no valid remote control definition\n", progname);
		fprintf(stderr, "%s: this shouldn't ever happen!\n", progname);
		if (curr_driver->deinit_func)
			curr_driver->deinit_func();
		exit(EXIT_FAILURE);
	}
	if (state->remotes == (void *)-1) {
		fprintf(stderr, "%s: reading of config file failed\n", progname);
		fprintf(stderr, "%s: this shouldn't ever happen!\n", progname);
		if (curr_driver->deinit_func)
			curr_driver->deinit_func();
		exit(EXIT_FAILURE);
	}
	if (!has_toggle_bit_mask(state->remotes)) {
		if (!state->using_template && needs_toggle_mask(state->remotes)) {
			do_get_toggle_bit_mask(&remote, state, opts);
		}
	} else {
		set_toggle_bit_mask(state->remotes, state->remotes->toggle_bit_mask);
	}
	if (curr_driver->deinit_func)
		curr_driver->deinit_func();
	get_pre_data(state->remotes);
	get_post_data(state->remotes);
}


/** View part of get_lengths. */
static int mode2_get_lengths(struct opts* opts, struct main_state* state)
{
	enum lengths_status sts = STS_LEN_AGAIN;
	struct lengths_state lengths_state;
	int debug = lirc_log_is_enabled_for(LIRC_TRACE);
	int diff;
	int i;

	if (!state->using_template ) {
		lengths_state_init(&lengths_state, 1);
		sts = STS_LEN_AGAIN;
		while (sts == STS_LEN_AGAIN) {
			sts = get_lengths(&lengths_state, &remote, opts->force, debug);
			switch (sts) {
			case STS_LEN_OK:
				i_printf(1, "\n");
				return 1;
			case STS_LEN_FAIL:
				i_printf(1, "\n");
				return 0;
			case STS_LEN_RAW_OK:
				i_printf(1, "\n");
				set_protocol(&remote, RAW_CODES);
				remote.eps = eps;
				remote.aeps = aeps;
				return 1;
			case STS_LEN_TIMEOUT:
				fprintf(stderr,
					"%s: no data for 10 secs, aborting\n",
					progname);
				exit(EXIT_FAILURE);
			case STS_LEN_NO_GAP_FOUND:
				fprintf(stderr,
					"%s: gap not found, can't continue\n",
					progname);
				fclose(state->fout);
				unlink(opts->filename);
				if (curr_driver->deinit_func)
					curr_driver->deinit_func();
				exit(EXIT_FAILURE);
			case STS_LEN_TOO_LONG:
				fprintf(stderr,
					"%s: signal too long\n",
					progname);
				printf("Creating config file in raw mode.\n");
				set_protocol(&remote, RAW_CODES);
				remote.eps = eps;
				remote.aeps = aeps;
				break;
			case STS_LEN_AGAIN:
				diff = lengths_state.keypresses -
					lengths_state.keypresses_done;
				for (i = 0; i < diff; i += 1) {
					printf(".");
				}
				fflush(stdout);
				lengths_state.keypresses_done += diff;
				sts = STS_LEN_AGAIN;
				break;
			}
		}
		free_all_lengths();
	}
	if lirc_log_is_enabled_for(LIRC_DEBUG) {
		printf("%d %u %u %u %u %u %d %d %d %u\n",
			remote.bits, (__u32) remote.pone, (__u32) remote.sone, (__u32) remote.pzero,
			(__u32) remote.szero, (__u32) remote.ptrail, remote.flags, remote.eps,
			remote.aeps, (__u32) remote.gap);
	}
	return sts;
}


/** View part of get_gap(). */
void lirccode_get_lengths(struct opts* opts, struct main_state* state)
{
	struct gap_state gap_state;
	enum get_gap_status sts;

	remote.driver = curr_driver->name;
	remote.bits = curr_driver->code_length;
	remote.eps = eps;
	remote.aeps = aeps;
	if (state->using_template)
		return;
	flushhw();
	gap_state_init(&gap_state);
	sts = STS_GAP_INIT;
	while (1) {
		switch (sts) {
		case STS_GAP_INIT:
			printf("Hold down an arbitrary key\n");
			sts = STS_GAP_AGAIN;
			continue;
		case STS_GAP_TIMEOUT:
			fprintf(stderr,
				"Timeout (10 sec), giving  up.\n");
			fclose(state->fout);
			unlink(opts->filename);
			if (curr_driver->deinit_func)
				curr_driver->deinit_func();
			exit(EXIT_FAILURE);
		case STS_GAP_FOUND:
			printf("Found gap (%d us)\n", remote.gap);
			return;
		case STS_GAP_GOT_ONE_PRESS:
			printf(".");
			fflush(stdout);
			sts = STS_GAP_AGAIN;
			continue;
		case STS_GAP_AGAIN:
			break;
		}
		sts = get_gap_length(&gap_state, &remote);
	}
}


int main(int argc, char **argv)
{
	struct opts opts;
	struct main_state state;
	struct button_state btn_state;

	memset(&opts, 0, sizeof(opts));
	memset(&state, 0, sizeof(state));
	get_options(argc, argv, argv[optind], &opts);

	get_commandline(argc, argv, state.commandline, sizeof(state.commandline));
	if (opts.list_namespace) {
		fprint_namespace(stdout);
		exit(EXIT_SUCCESS);
	}
	do_init(&opts, &state);

	printf(MSG_WELCOME);
	if (curr_driver->name && strcmp(curr_driver->name, "devinput") == 0) {
		printf(MSG_DEVINPUT);
	}
	printf("Press RETURN to continue.\n");
	getchar();

	remote.name = opts.filename;
	switch (curr_driver->rec_mode) {
		case LIRC_MODE_MODE2:
			mode2_get_lengths(&opts, &state);
			break;
		case LIRC_MODE_LIRCCODE:
			lirccode_get_lengths(&opts, &state);
			break;
	}

	if (!state.using_template && needs_toggle_mask(&remote)) {
		do_get_toggle_bit_mask(&remote, &state, &opts);
	}
	printf("\nNow enter the names for the buttons.\n");
	fflush(stdout);

	fprint_copyright(state.fout);
	fprint_comment(state.fout, &remote, state.commandline);
	fprint_remote_head(state.fout, &remote);
	fprint_remote_signal_head(state.fout, &remote);
	button_state_init(&btn_state);
	record_buttons(&state, &btn_state, &opts);

	/* write final config file */
	state.fout = fopen(opts.filename, "w");
	if (state.fout == NULL) {
		fprintf(stderr, "%s: could not open final config file \"%s\"\n",
			progname, opts.filename);
		perror(progname);
		free_config(state.remotes);
		return (EXIT_FAILURE);
	}
	fprint_copyright(state.fout);
	fprint_remotes(state.fout, state.remotes, state.commandline);
	free_config(state.remotes);
	printf("Successfully written config file.\n");
	return (EXIT_SUCCESS);
}
