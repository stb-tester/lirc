/****************************************************************************
 ** irrecord.c **************************************************************
 ****************************************************************************
 *
 * irrecord -  application for recording IR-codes for usage with lircd
 *
 * Copyright (C) 1998,99 Christoph Bartelmus <lirc@bartelmus.de>
 *
 */

#include <ctype.h>

#include "lirc_private.h"
#include "irrecord.h"


#define USAGE	    "Usage: irrecord [options] [config file]\n" \
		    "	    irrecord -a <config file> \n" \
		    "	    irrecord -l \n"

static const char *const help =
    USAGE
    "\nOptions:\n"
    "\t -H --driver=driver\tUse given driver\n"
    "\t -d --device=device\tRead from given device\n"
    "\t -a --analyse\t\tAnalyse raw_codes config files\n"
    "\t -k --keep-root\tDon't drop root privileges\n"
    "\t -l --list-namespace\tList valid button names\n"
    "\t -U --plugindir=dir\tLoad drivers from dir\n"
    "\t -f --force\t\tForce raw mode\n"
    "\t -n --disable-namespace\tDisable namespace checks\n"
    "\t -Y --dynamic-codes\tEnable dynamic codes\n"
    "\t -D --loglevel=level\t'error', 'info', 'notice',... or 0..10\n"
    "\t -h --help\t\tDisplay this message\n" "\t -v --version\t\tDisplay version\n";

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
	{"keep-root", no_argument, NULL, 'k'},
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

const char *const MSG_WELCOME =
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
    "to  <lirc@bartelmus.de> so it can be made available to others.";

static const char *const MSG_DEVINPUT =
    "Usually it's not necessary to create a new config file for devinput\n"
    "devices. A generic config file can be found at:\n"
    "https://sf.net/p/lirc-remotes/code/ci/master/tree/remotes/devinput/devinput.lircd.conf\n"
    "It can be downloaded using irdb-get(1)\n"
    "Please try this config file before creating your own.";

static const char *const MSG_TOGGLE_BIT_INTRO =
    "Checking for toggle bit mask.\n"
    "Please press an arbitrary button repeatedly as fast as possible.\n"
    "Make sure you keep pressing the SAME button and that you DON'T HOLD\n"
    "the button down!.\n"
    "If you can't see any dots appear, wait a bit between button presses.\n\n"
    "Press RETURN to continue.";

static const char *MSG_LENGTHS_INIT =
    "Now start pressing buttons on your remote control.\n\n"
    "It is very important that you press many different buttons and hold them\n"
    "down for approximately one second. Each button should generate at least one\n"
    "dot but in no case more than ten dots of output.\n"
    "Don't stop pressing buttons until two lines of dots (2x80) have been\n" "generated.\n";


/** Set up default values for all command line options + filename. */
static void add_defaults(void)
{
	char level[4];
	snprintf(level, sizeof(level), "%d", lirc_log_defaultlevel());
	const char *const defaults[] = {
		"lircd:plugindir", PLUGINDIR,
		"irrecord:driver", "devinput",
		"irrecord:device", LIRC_DRIVER_DEVICE,
		"irrecord:analyse", "False",
		"irrecord:force", "False",
		"irrecord:disable-namespace", "False",
		"irrecord:dynamic-codes", "False",
		"irrecord:list_namespace", "False",
		"irrecord:filename", "irrecord.conf",
		"lircd:debug", level,
		(const char *)NULL, (const char *)NULL
	};
	options_add_defaults(defaults);
};


/** Stuff command line into the single string buff. */
static void get_commandline(int argc, char **argv, char *buff, size_t size)
{
	int i;
	int j;
	int dest = 0;
	if (size == 0)
		return;
	for (i = 1; i < argc; i += 1) {
		for (j = 0; argv[i][j] != '\0'; j += 1) {
			if (dest + 1 >= size)
				break;
			buff[dest++] = argv[i][j];
		}
		if (dest + 1 >= size)
			break;
		buff[dest++] = ' ';
	}
	buff[--dest] = '\0';
}


/** Parse command line, update the options dict. */
static void parse_options(int argc, char **const argv)
{
	int c;

	const char *const optstring = "had:D:H:fnlO:pPtiTU:vY";

	add_defaults();
	optind = 1;
	while ((c = getopt_long(argc, argv, optstring, long_options, NULL))
	       != -1) {
		switch (c) {
		case 'a':
			options_set_opt("irrecord:analyse", "True");
			break;
		case 'D':
			if (string2loglevel(optarg) == LIRC_BADLEVEL) {
				fprintf(stderr, "Bad debug level: %s\n", optarg);
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
		case 'k':
			unsetenv("SUDO_USER");
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
			printf("irrecord %s\n", VERSION);
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


/** Check options, possibly run simple ones. Returns status. */
static enum init_status init(struct opts *opts, struct main_state *state)
{
	char filename_new[256];
	char logpath[256];
	int flags;
	struct ir_remote *my_remote;
	FILE *f;

	hw_choose_driver(NULL);
	if (!opts->analyse && hw_choose_driver(opts->driver) != 0) {
		return STS_INIT_BAD_DRIVER;
	}
	ir_remote_init(opts->dynamic_codes);
	lirc_log_get_clientlog("irrecord", logpath, sizeof(logpath));
	(void) unlink(logpath);
	lirc_log_set_file(logpath);
	lirc_log_open("irrecord", 0, opts->loglevel);
	curr_driver->open_func(opts->device);
	if (strcmp(curr_driver->name, "null") == 0 && !opts->analyse) {
		return STS_INIT_NO_DRIVER;
	}
	f = fopen(opts->filename, "r");
	if (f != NULL) {

		if (opts->force) {
			return STS_INIT_FORCE_TMPL;
		}
		my_remote = read_config(f, opts->filename);
		fclose(f);
		if (my_remote == (void *)-1 || my_remote == NULL) {
			return STS_INIT_BAD_FILE;
		}
		opts->using_template = 1;
		if (opts->analyse) {
			return STS_INIT_ANALYZE;
		}
		if (opts->test) {
			if (opts->trail)
				for_each_remote(my_remote, remove_trail);
			for_each_remote(my_remote, remove_pre_data);
			for_each_remote(my_remote, remove_post_data);
			if (opts->get_pre)
				for_each_remote(my_remote, get_pre_data);
			if (opts->get_post)
				for_each_remote(my_remote, get_post_data);
			if (opts->invert)
				for_each_remote(my_remote, invert_data);

			fprint_remotes(stdout, my_remote, opts->commandline);
			free_config(my_remote);
			return (STS_INIT_TESTED);
		}
		remote = *my_remote;  //FIXME: Who owns this memory?
		remote.codes = NULL;
		remote.last_code = NULL;
		remote.next = NULL;
		if (remote.pre_p == 0 && remote.pre_s == 0 && remote.post_p == 0
		    && remote.post_s == 0) {
			remote.bits = bit_count(&remote);
			remote.pre_data_bits = 0;
			remote.post_data_bits = 0;
		}
		if (my_remote->next != NULL) {
			fprintf(stderr,
				"%s: only first remote definition in file \"%s\" used\n", progname,
				opts->filename);
		}
		snprintf(filename_new, sizeof(filename_new), "%s.conf", opts->filename);
		opts->filename = strdup(filename_new);
	} else {
		if (opts->analyse) {
			fprintf(stderr, "%s: no input file given, ignoring analyse flag\n",
				progname);
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


static int get_options(int argc, char **argv, const char *filename, struct opts *options)
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


/** View part of get_toggle_bit_mask(). */
static void do_get_toggle_bit_mask(struct ir_remote *remote, struct main_state *state,
				   const struct opts *opts)
{
	const char *const MISSING_MASK_MSG = "But I know for sure that RC6 has a toggle bit!";
	enum toggle_status sts;
	struct toggle_state tgl_state;

	fputs(MSG_TOGGLE_BIT_INTRO, stdout);
	fflush(stdout);
	getchar();
	flushhw();
	toggle_state_init(&tgl_state);
	sts = STS_TGL_AGAIN;
	while (1) {
		switch (sts) {
		case STS_TGL_TIMEOUT:
			fprintf(stderr, "Timeout (10 sec), giving up");
			exit(EXIT_FAILURE);
		case STS_TGL_GOT_ONE_PRESS:
			printf(".");
			fflush(stdout);
			sts = STS_TGL_AGAIN;
			continue;
		case STS_TGL_FOUND:
			printf("\nToggle bit mask is 0x%llx.\n", (__u64) remote->toggle_bit_mask);
			if (is_rc6(remote)) {
				printf("RC6 mask is 0x%llx.\n", (__u64) remote->rc6_mask);
			}
			fflush(stdout);
			return;
		case STS_TGL_NOT_FOUND:
			printf("Cannot find any toggle mask.\n");
			if (!is_rc6(remote))
				break;
			puts(MISSING_MASK_MSG);
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


/** View part of init: run init() and handle results. Returns or exits. */
static void do_init(struct opts *opts, struct main_state *state)
{
	enum init_status sts;

	sts = init(opts, state);
	switch (sts) {
	case STS_INIT_BAD_DRIVER:
		fprintf(stderr, "Driver `%s' not found", opts->driver);
		fputs(" (wrong or missing -U/--plugindir?).\n", stderr);
		hw_print_drivers(stderr);
		exit(EXIT_FAILURE);
	case STS_INIT_NO_DRIVER:
		fputs("irrecord does not make sense without hardware\n", stderr);
		exit(EXIT_FAILURE);
	case STS_INIT_FORCE_TMPL:
		fprintf(stderr,
			"File \"%s\" already exists\n"
			"You cannot use the --force option " "together with a template file\n",
			opts->filename);
		exit(EXIT_FAILURE);
	case STS_INIT_BAD_FILE:
		fprintf(stderr, "Could not open new config file %s\n", opts->filename);
		perror(progname);
		exit(EXIT_FAILURE);
	case STS_INIT_TESTED:
		exit(0);
	case STS_INIT_FOPEN:
		fprintf(stderr, "Could not open new config file %s\n", opts->filename);

	case STS_INIT_HW_FAIL:
		fputs("Could not init hardware"
		      " (lircd running ? --> close it, check permissions)\n", stderr);
		exit(EXIT_FAILURE);
	case STS_INIT_BAD_MODE:
		fputs("Mode not supported\n", stderr);
		exit(EXIT_FAILURE);
	case STS_INIT_O_NONBLOCK:
		fputs("Could not set O_NONBLOCK flag\n", stderr);
		exit(EXIT_FAILURE);
	case STS_INIT_ANALYZE:
		do_analyse(opts, state);
		exit(EXIT_SUCCESS);
	case STS_INIT_OK:
		return;
	}
}


/** View part: Record data for one button. */
static enum button_status get_button_data(struct button_state *btn_state,
					  struct main_state *state, const struct opts *opts)
{
	const char *const MSG_BAD_STS = "Bad status in get_button_data: %d\n";
	const char *const MSG_BAD_RETURN = "Bad return from  get_button_data";
	enum button_status sts = STS_BTN_INIT_DATA;

	btn_state->retries = 30;
	last_remote = NULL;
	sts = STS_BTN_INIT_DATA;
	sleep(1);
	while (btn_state->retries > 0) {
		switch (sts) {
		case STS_BTN_INIT_DATA:
			printf("\nNow hold down button \"%s\".\n", btn_state->buffer);
			fflush(stdout);
			flushhw();
			break;
		case STS_BTN_GET_DATA:
		case STS_BTN_GET_RAW_DATA:
			break;
		case STS_BTN_TIMEOUT:
			printf("Timeout (10 seconds), try again\n");
			sts = STS_BTN_INIT_DATA;
			continue;
		case STS_BTN_GET_TOGGLE_BITS:
			do_get_toggle_bit_mask(&remote, state, opts);
			return STS_BTN_ALL_DONE;
		case STS_BTN_SOFT_ERROR:
			printf("Something went wrong: ");
			fputs(btn_state->message, stdout);
			if (btn_state->retries <= 0 && !opts->force) {
				printf("Try using the -f option.\n");
				break;
			}
			printf("Please try again. (%d retries left)\n", btn_state->retries - 1);
			sts = STS_BTN_INIT_DATA;
			continue;
		case STS_BTN_BUTTON_DONE:
		case STS_BTN_HARD_ERROR:
		case STS_BTN_ALL_DONE:
			return sts;
		default:
			btn_state_set_message(btn_state, MSG_BAD_STS, sts);
			return STS_BTN_HARD_ERROR;
		}
		sts = record_buttons(btn_state, sts, state, opts);
	}
	btn_state_set_message(btn_state, MSG_BAD_RETURN, sts);
	return STS_BTN_HARD_ERROR;
}


/** View part of record_buttons. */
void do_record_buttons(struct main_state *state, const struct opts *opts)
{
	struct button_state btn_state;
	enum button_status sts = STS_BTN_INIT;
	char *s;

	button_state_init(&btn_state);
	flushhw();
	while (1) {
		switch (sts) {
		case STS_BTN_INIT:
			break;
		case STS_BTN_GET_NAME:
			printf("\nPlease enter the name for the next button"
			       " (press <ENTER> to finish recording)\n");
			s = fgets(btn_state.buffer, sizeof(btn_state.buffer), stdin);
			if (s != btn_state.buffer) {
				btn_state_set_message(&btn_state, "%s: fgets() failed\n", progname);
				sts = STS_BTN_HARD_ERROR;
				break;
			}
			s = strchr(s, '\n');
			if (s != NULL)
				*s = '\0';
			break;
		case STS_BTN_INIT_DATA:
			sts = get_button_data(&btn_state, state, opts);
			continue;
		case STS_BTN_GET_DATA:
		case STS_BTN_GET_RAW_DATA:
			printf("Oops (data states in record_buttons().");
			break;
		case STS_BTN_BUTTON_DONE:
			sts = STS_BTN_GET_NAME;
			continue;
		case STS_BTN_BUTTONS_DONE:
			break;
		case STS_BTN_ALL_DONE:
			return;
		case STS_BTN_TIMEOUT:
			printf("Illegal data-state timeout\n");
			sts = STS_BTN_INIT;
			continue;
		case STS_BTN_GET_TOGGLE_BITS:
			do_get_toggle_bit_mask(&remote, state, opts);
			return;
		case STS_BTN_SOFT_ERROR:
			fputs(btn_state.message, stdout);
			printf("Press RETURN to continue.\n\n");
			getchar();
			sts = STS_BTN_INIT;
			continue;
		case STS_BTN_HARD_ERROR:
			fprintf(stderr, "Unrecoverable error: ");
			fprintf(stderr, "%s\n", btn_state.message);
			fprintf(stderr, "Giving up\n");
			exit(EXIT_FAILURE);
		}
		sts = record_buttons(&btn_state, sts, state, opts);
	}
}


/** View part of get_lengths. */
static int mode2_get_lengths(const struct opts *opts, struct main_state *state)
{
	const char *const MSG_AGAIN = "\nPlease keep on pressing buttons like described above.";
	enum lengths_status sts = STS_LEN_AGAIN;
	struct lengths_state lengths_state;
	int debug = lirc_log_is_enabled_for(LIRC_TRACE);
	int diff;
	int i;

	if (!opts->using_template) {
		puts(MSG_LENGTHS_INIT);
		printf("Press RETURN now to start recording.");
		fflush(stdout);
		getchar();
		flushhw();
		sts = STS_LEN_AGAIN;
		lengths_state_init(&lengths_state);
		while (sts == STS_LEN_AGAIN) {
			sts = get_lengths(&lengths_state, &remote, opts->force, debug);
			switch (sts) {
			case STS_LEN_OK:
				puts("");
				return 1;
			case STS_LEN_FAIL:
				puts("");
				return 0;
			case STS_LEN_RAW_OK:
				puts("");
				set_protocol(&remote, RAW_CODES);
				remote.eps = eps;
				remote.aeps = aeps;
				return 1;
			case STS_LEN_TIMEOUT:
				fprintf(stderr, "%s: no data for 10 secs, aborting\n", progname);
				exit(EXIT_FAILURE);
			case STS_LEN_NO_GAP_FOUND:
				fprintf(stderr, "%s: gap not found, can't continue\n", progname);
				fclose(state->fout);
				unlink(opts->filename);
				if (curr_driver->deinit_func)
					curr_driver->deinit_func();
				exit(EXIT_FAILURE);
			case STS_LEN_TOO_LONG:
				fprintf(stderr, "%s: signal too long\n", progname);
				printf("Creating config file in raw mode.\n");
				set_protocol(&remote, RAW_CODES);
				remote.eps = eps;
				remote.aeps = aeps;
				break;
			case STS_LEN_AGAIN_INFO:
				printf("\nGot gap (%d us) ", remote.gap);
				puts(MSG_AGAIN);
				sts = STS_LEN_AGAIN;
				continue;
			case STS_LEN_AGAIN:
				diff = lengths_state.keypresses - lengths_state.keypresses_done;
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
	logprintf(LIRC_DEBUG, "%d %u %u %u %u %u %d %d %d %u\n",
		  remote.bits, (__u32) remote.pone, (__u32) remote.sone,
		  (__u32) remote.pzero, (__u32) remote.szero,
		  (__u32) remote.ptrail, remote.flags, remote.eps, remote.aeps, (__u32) remote.gap);
	return sts;
}


/** View part of get_gap(). */
void lirccode_get_lengths(const struct opts *opts, struct main_state *state)
{
	struct gap_state gap_state;
	enum get_gap_status sts;

	remote.driver = curr_driver->name;
	remote.bits = curr_driver->code_length;
	remote.eps = eps;
	remote.aeps = aeps;
	if (opts->using_template)
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
			fprintf(stderr, "Timeout (10 sec), giving  up.\n");
			fclose(state->fout);
			unlink(opts->filename);
			if (curr_driver->deinit_func)
				curr_driver->deinit_func();
			exit(EXIT_FAILURE);
		case STS_GAP_FOUND:
			printf("\nFound gap (%d us)\n", remote.gap);
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

void get_name(struct ir_remote* remote, struct opts* opts)
{
	char buff[256];
	char path[256];
	char* s;

	while (remote->name == NULL) {
again:
		fputs("Enter name of remote (only ascii, no spaces) :",
		      stdout);
		s = fgets(buff, sizeof(buff), stdin);
		if (s != buff) {
			puts("gets() failed (!)");
			continue;
		}
		s = strrchr(s, '\n');
		if (s != NULL)
			*s = '\0';
		for (s = buff; *s; s += 1) {
			if (*s == ' ' || !isalnum(*s) ){
				printf("Bad character: %c (x%x)\n", *s, *s);
				goto again;
			}
		}
		remote->name = strdup(buff);
	}
	snprintf(path, sizeof(path), "%s.lircd.conf", buff);
	opts->filename = strdup(path);  // FIXME: leak?
	printf("Using %s as output filename\n\n", opts->filename);
}


int main(int argc, char **argv)
{
	struct opts opts;
	struct main_state state;
	int r = 1;
	const char* new_user;

	memset(&opts, 0, sizeof(opts));
	memset(&state, 0, sizeof(state));
	get_options(argc, argv, argv[optind], &opts);

	get_commandline(argc, argv, opts.commandline, sizeof(opts.commandline));
	if (opts.list_namespace) {
		fprint_namespace(stdout);
		exit(EXIT_SUCCESS);
	}
	new_user = drop_sudo_root(seteuid);
	if (strcmp("root", new_user) == 0)
		puts("Warning: Running as root.");
	else if (strlen(new_user) == 0)
		puts("Warning: Cannot change uid.");
	else
		printf("Running as regular user %s\n", new_user);
	do_init(&opts, &state);

	puts(MSG_WELCOME);
	if (curr_driver->name && strcmp(curr_driver->name, "devinput") == 0) {
		puts(MSG_DEVINPUT);
	}
	printf("Press RETURN to continue.\n");
	getchar();

	if (remote.name == NULL)
		get_name(&remote, &opts);
	switch (curr_driver->rec_mode) {
	case LIRC_MODE_MODE2:
		mode2_get_lengths(&opts, &state);
		break;
	case LIRC_MODE_LIRCCODE:
		lirccode_get_lengths(&opts, &state);
		break;
	}
	if (!opts.using_template && is_rc6(&remote))
		do_get_toggle_bit_mask(&remote, &state, &opts);
	config_file_setup(&state, &opts);
	do_record_buttons(&state, &opts);
	if (!is_raw(&remote))
		r = config_file_finish(&state, &opts);
	printf("Successfully written config file %s\n", opts.filename);
	return r ? EXIT_SUCCESS : EXIT_FAILURE;
}
