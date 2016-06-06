/****************************************************************************
** irrecord.c **************************************************************
****************************************************************************
*
* irrecord -  application for recording IR-codes for usage with lircd
*
* Copyright (C) 1998,99 Christoph Bartelmus <lirc@bartelmus.de>
*
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <unistd.h>

#include "lirc_private.h"
#include "irrecord.h"

static const logchannel_t logchannel = LOG_APP;

static const int NOISE_LIMIT = 500;
static const int NOISE_TIMEOUT_US = 3000000;
static const int DEFAULT_GAP = 50000;


#define USAGE       "Usage: irrecord [options] [config file]\n" \
	"	    irrecord -u <config file>\n" \
	"	    irrecord -a <config file>\n" \
	"	    irrecord -l\n"

static const char* const help =
	USAGE
	"\nOptions:\n"
	"\t -H --driver=driver\tUse given driver\n"
	"\t -d --device=device\tRead from given device\n"
	"\t -a --analyse\t\tAnalyse raw_codes config files\n"
	"\t -k --keep-root\t\tDon't drop root privileges\n"
	"\t -l --list-namespace\tList valid button names\n"
	"\t -u --update\t\tAmend buttons to existing file\n"
	"\t -U --plugindir=dir\tLoad drivers from dir\n"
	"\t -f --force\t\tForce raw mode\n"
	"\t -n --disable-namespace\tDisable namespace checks\n"
	"\t -Y --dynamic-codes\tEnable dynamic codes\n"
	"\t -O --options-file\tUse alternative lirc_options.conf file\n"
	"\t -D --loglevel=level\t'error', 'info', 'notice',... or 3..10\n"
	"\t -h --help\t\tDisplay this message\n"
	"\t -v --version\t\tDisplay version\n";

static const struct option long_options[] = {
	{"help",	      no_argument,	 NULL, 'h'},
	{"version",	      no_argument,	 NULL, 'v'},
	{"analyse",	      no_argument,	 NULL, 'a'},
	{"device",	      required_argument, NULL, 'd'},
	{"options-file",      required_argument, NULL, 'O'},
	{"debug",	      required_argument, NULL, 'D'},
	{"loglevel",	      required_argument, NULL, 'D'},
	{"driver",	      required_argument, NULL, 'H'},
	{"force",	      no_argument,	 NULL, 'f'},
	{"disable-namespace", no_argument,	 NULL, 'n'},
	{"keep-root",	      no_argument,	 NULL, 'k'},
	{"list-namespace",    no_argument,	 NULL, 'l'},
	{"update",	      required_argument, NULL, 'u'},
	{"plugindir",	      required_argument, NULL, 'U'},
	{"dynamic-codes",     no_argument,	 NULL, 'Y'},
	{"pre",		      no_argument,	 NULL, 'p'},
	{"post",	      no_argument,	 NULL, 'P'},
	{"test",	      no_argument,	 NULL, 't'},
	{"invert",	      no_argument,	 NULL, 'i'},
	{"trail",	      no_argument,	 NULL, 'T'},
	{0,		      0,		 0,	0  }
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
	"to  <lirc@bartelmus.de> so it can be made available to others.";

static const char* const MSG_DEVINPUT =
	"Usually you should not create a new config file for devinput\n"
	"devices. LIRC is installed with a devinput.lircd.conf file which \n"
	"is built for the current system which works with all remotes \n"
	"supported by the kernel. There might be a need to update \n"
	"this file so it matches the current kernel. For this, use the \n"
	"lirc-make-devinput(1) script.";

static const char* const MSG_TOGGLE_BIT_INTRO =
	"Checking for toggle bit mask.\n"
	"Please press an arbitrary button repeatedly as fast as possible.\n"
	"Make sure you keep pressing the SAME button and that you DON'T HOLD\n"
	"the button down!.\n"
	"If you can't see any dots appear, wait a bit between button presses.\n\n"
	"Press RETURN to continue.";

static const char* MSG_LENGTHS_INIT =
	"Now start pressing buttons on your remote control.\n\n"
	"It is very important that you press many different buttons randomly\n"
	"and hold them down for approximately one second. Each button should\n"
	"generate at least one dot but never more than ten dots of output.\n"
	"Don't stop pressing buttons until two lines of dots (2x80) have\n"
	"been generated.\n";

static const char* const MSG_NOISE_INTRO =
	"Checking for ambient light  creating too much disturbances.\n"
	"Please don't press any buttons, just wait a few seconds...";

static const char* const MSG_TOO_MUCH_NOISE =
	"Running irrecord with this level of noise will not give good results.\n"
	"Please try to turn off fluorescent lamps or tubes and other sources\n"
	"of variable IR radiation and restart. If nothing else works, you\n"
	"might have to mask the receiving IR diode. You REALLY should press\n"
	"ctrl-C at this point, but it's technically possible to proceed\n"
	"by pressing RETURN";

static const char* const MSG_NO_BUTTONS =
	"No recorded buttons saved. This config file makes no sense, but\n"
	"you can add button definitions to it next time you run irrecord.\n";

static const char* const MSG_TOO_FEW_BUTTONS =
	"You have only recorded one button in a non-raw configuration file.\n"
	"This file doesn't really make much sense, you should record at\n"
	"least two or three buttons to get meaningful results. You can add\n"
	"more buttons next time you run irrecord.\n";

static const char* const MSG_DONT_FORCE_UNLESS_UPDATE =
	"File \"%s\" already exists\n"
	"You cannot use the --force option together with a template file\n"
	"unless also using --update";

static const char* const MSG_NO_GAP_FOUND_WARNING =
	"Cannot find any gap, using an arbitrary 50 ms one. If you have a\n"
        "regular remote for e. g., a TV or such this is probably a point\n"
        "where you hit control-C. However, technical hardware like air \n"
	"condition gear often works without any gap. If you think it's\n"
	"reasonable that your remote lacks gap you can proceed. ";

/** Result code from init(). */
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


/** Linked list of all recorded buttons. */
static struct ir_ncode* ncodes_root = NULL;


/** Add a ncode to list, returns new root.  List owns memory*/
static void ncode_list_add(struct ir_ncode* ncode)
{
	struct ir_ncode* new_ncode;
	struct ir_ncode* n;

	new_ncode = ncode_dup(ncode);
	if (new_ncode == NULL) {
		perror("No memory in ncode_list_add()");
		return;
	}
	new_ncode->next_ncode = NULL;
	if (ncodes_root == NULL) {
		ncodes_root = new_ncode;
		return;
	}
	for (n = ncodes_root; n->next_ncode != NULL; n = n->next_ncode)
		;
	n->next_ncode = new_ncode;
}


/** Find node with given name in list, or NULL. */
static const struct ir_ncode* ncode_list_find_name(const char* name)
{
	const struct ir_ncode* nc;

	for (nc = ncodes_root; nc != NULL; nc = nc->next_ncode) {
		if (strcmp(nc->name, name) == 0)
			return nc;
	}
	return NULL;
}


/** Remove item with given name from list, return boolean success/fail */
static int ncode_list_remove_name(const char* name)
{
	struct ir_ncode* nc;

	if (ncodes_root == NULL)
		return 0;
	if (strcmp(ncodes_root->name, name) == 0) {
		ncodes_root = ncodes_root->next_ncode;
		return 1;
	}
	for (nc = ncodes_root; nc->next_ncode != NULL; nc = nc->next_ncode) {
		if (strcmp(nc->next_ncode->name, name) == 0) {
			nc->next_ncode = nc->next_ncode->next_ncode;
			return 1;
		}
	}
	return 0;
}


/** Apply func(ncode, arg) on each ncode in list, break if func returns 0. */
static int ncode_list_for_each(int (*func)(struct ir_ncode*, void*),
			       void* arg)
{
	struct ir_ncode* nc;

	for (nc = ncodes_root; nc != NULL; nc = nc->next_ncode) {
		if (!func(nc, arg))
			return 0;
	}
	return 1;
}


/** Set up default values for all command line options + filename. */
static void add_defaults(void)
{
	const char* const device = options_getstring("lircd:device");
	const char* const driver = options_getstring("lircd:driver");
	const char* const level = options_getstring("lircd:debug");
	const char* const plugindir = options_getstring("lircd:plugindir");
	char default_level[4];

	snprintf(default_level, sizeof(default_level), "%d",
		 lirc_log_defaultlevel());
	const char* const defaults[] = {
		"lircd:plugindir",      plugindir ? plugindir : PLUGINDIR,
		"irrecord:driver",      driver ? driver : "default",
		"irrecord:device",      device ? device : LIRC_DRIVER_DEVICE,
		"irrecord:analyse",     "False",
		"irrecord:force",       "False",
		"irrecord:update",      "False",
		"irrecord:disable-namespace",
					"False",
		"irrecord:dynamic-codes",
					"False",
		"irrecord:list-namespace",
					"False",
		"irrecord:filename",    "irrecord.lircd.conf",
		"irrecord:debug",	level ? level : default_level,
		(const char*)NULL,	(const char*)NULL
	};
	options_add_defaults(defaults);
};


/** Stuff command line into the single string buff. */
static void get_commandline(int argc, char** argv, char* buff, size_t size)
{
	int i;
	int j;
	unsigned int dest = 0;
	buff[0] = '\0';

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
	if (dest > 0)
		buff[--dest] = '\0';
}


/** Parse command line, update the options dict. */
static void parse_options(int argc, char** const argv)
{
	int c;

	const char* const optstring = "had:D:H:fknlO:pPtiTU:uvY";

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
			options_set_opt("irrecord:debug", optarg);
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
		case 'u':
			options_set_opt("irrecord:update", "True");
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
static enum init_status init(struct opts* opts, struct main_state* state)
{
	char filename_new[256];
	char logpath[256];
	int flags;
	struct ir_remote* my_remote;
	FILE* f;
	struct ir_ncode* nc;
	int fd;

	if (opts->force) {
		printf("Using raw access on device %s\n",
		       opts->device);
	} else {
		printf("Using driver %s on device %s\n",
			opts->driver, opts->device);
	}
	hw_choose_driver(NULL);
	if (!opts->analyse && hw_choose_driver(opts->driver) != 0)
		return STS_INIT_BAD_DRIVER;
	ir_remote_init(opts->dynamic_codes);
	lirc_log_get_clientlog("irrecord", logpath, sizeof(logpath));
	(void)unlink(logpath);
	lirc_log_set_file(logpath);
	lirc_log_open("irrecord", 0, opts->loglevel);
	if (strcmp(curr_driver->name, "null") == 0 && !opts->analyse)
		return STS_INIT_NO_DRIVER;
	f = fopen(opts->filename, "r");
	if (f != NULL) {
		if (opts->force && !opts->update)
			return STS_INIT_FORCE_TMPL;
		my_remote = read_config(f, opts->filename);
		fclose(f);
		if (my_remote == (void*)-1 || my_remote == NULL)
			return STS_INIT_BAD_FILE;
		opts->using_template = 1;
		if (opts->analyse)
			return STS_INIT_ANALYZE;
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
			return STS_INIT_TESTED;
		}
		remote = *my_remote;  //FIXME: Who owns this memory?
		if (opts->update) {
			for (nc = remote.codes; nc->name != NULL; nc++)
				ncode_list_add(nc);
		}
		remote.codes = NULL;
		remote.last_code = NULL;
		remote.next = NULL;
		if (!opts->update
		    && remote.pre_p == 0 && remote.pre_s == 0
		    && remote.post_p == 0 && remote.post_s == 0) {
			remote.bits = bit_count(&remote);
			remote.pre_data_bits = 0;
			remote.post_data_bits = 0;
		}
		if (my_remote->next != NULL) {
			fprintf(stderr,
				"Only first remote definition in file \"%s\" used\n",
				opts->filename);
		}
		snprintf(filename_new,
			 sizeof(filename_new), "%s", opts->filename);
		opts->filename = strdup(filename_new);
	} else {
		if (opts->analyse) {
			fputs("No input file given, ignoring analyse flag\n",
			      stderr);
			opts->analyse = 0;
		}
	}
	strcpy(filename_new, "irrecord-tmp-XXXXXX");
	fd = mkstemp(filename_new);
	if (fd == -1) {
		log_perror_warn("Cannot open tmpfile");
		return STS_INIT_FOPEN;
	}
	opts->tmpfile = strdup(filename_new);
	state->fout = fdopen(fd, "w");
	if (state->fout == NULL) {
		log_perror_warn("Cannot fdopen tmpfile");
		return STS_INIT_FOPEN;
	}
	if (opts->update) {
		snprintf(filename_new, sizeof(filename_new),
			 "%s.bak", opts->filename);
		opts->backupfile = strdup(filename_new);
	} else {
		opts->backupfile = NULL;
	}
	if (state->started_as_root) {
		if (seteuid(0) == -1)
			log_error("Cannot reset root uid");
	}
	curr_driver->open_func(opts->device);
	if (curr_driver->init_func) {
		if (!curr_driver->init_func()) {
			fclose(state->fout);
			unlink(opts->tmpfile);
			return STS_INIT_HW_FAIL;
		}
	}
	drop_sudo_root(seteuid);

	aeps = ((int) curr_driver->resolution > aeps ?
		curr_driver->resolution : aeps);
	if (curr_driver->rec_mode != LIRC_MODE_MODE2
	    && curr_driver->rec_mode != LIRC_MODE_LIRCCODE) {
		fclose(state->fout);
		unlink(opts->tmpfile);
		if (curr_driver->deinit_func)
			curr_driver->deinit_func();
		return STS_INIT_BAD_MODE;
	}
	flags = fcntl(curr_driver->fd, F_GETFL, 0);
	if (flags == -1 ||
	    fcntl(curr_driver->fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		fputs("Could not set O_NONBLOCK flag\n", stderr);
		fclose(state->fout);
		unlink(opts->tmpfile);
		if (curr_driver->deinit_func)
			curr_driver->deinit_func();
		return STS_INIT_O_NONBLOCK;
	}
	return STS_INIT_OK;
}


static int get_options(int argc,
		       char** argv,
		       const char* filename,
		       struct opts* options)
{
	options->force = 0;
	options_load(argc, argv, NULL, parse_options);
	options->analyse = options_getboolean("irrecord:analyse");
	options->device = options_getstring("irrecord:device");
	options->loglevel =
		string2loglevel(options_getstring("irrecord:debug"));
	options->driver = options_getstring("irrecord:driver");
	options->force = options_getboolean("irrecord:force");
	options->disable_namespace =
		options_getboolean("irrecord:disable-namespace");
	options->dynamic_codes = options_getboolean("lircd:dynamic-codes");
	options->get_pre = options_getboolean("irrecord:pre");
	options->get_post = options_getboolean("irrecord:post");
	options->list_namespace =
		options_getboolean("irrecord:list-namespace");
	options->test = options_getboolean("irrecord:test");
	options->invert = options_getboolean("irrecord:invert");
	options->trail = options_getboolean("irrecord:trail");
	options->filename = options_getstring("irrecord:filename");
	options->update = options_getboolean("irrecord:update");
	return 1;
}


/** View part of get_toggle_bit_mask(). */
static void do_get_toggle_bit_mask(struct ir_remote* remote,
				   struct main_state* state,
				   const struct opts* opts)
{
	const char* const MISSING_MASK_MSG =
		"But I know for sure that RC6 has a toggle bit!";
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
			printf("\nToggle bit mask is 0x%llx.\n",
			       (__u64)remote->toggle_bit_mask);
			if (is_rc6(remote))
				printf("RC6 mask is 0x%llx.\n",
				       (__u64)remote->rc6_mask);
			fflush(stdout);
			return;
		case STS_TGL_NOT_FOUND:
			printf("Cannot find any toggle mask.\n");
			if (!is_rc6(remote))
				return;
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
static void
do_init(struct opts* opts, struct main_state* state)
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
		fputs("irrecord does not make sense without hardware\n",
		      stderr);
		exit(EXIT_FAILURE);
	case STS_INIT_FORCE_TMPL:
		fprintf(stderr,
			MSG_DONT_FORCE_UNLESS_UPDATE,
			opts->filename);
		exit(EXIT_FAILURE);
	case STS_INIT_BAD_FILE:
		fprintf(stderr, "Could not parse config file %s\n",
			opts->filename);
		exit(EXIT_FAILURE);
	case STS_INIT_TESTED:
		exit(0);
	case STS_INIT_FOPEN:
		fprintf(stderr,
			"Could not open new config file %s\n", opts->filename);

	case STS_INIT_HW_FAIL:
		fputs("Could not init hardware"
		      " (lircd running ? --> close it, check permissions)\n",
		      stderr);
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


static int printf_signal_func(struct ir_ncode* ncode, void* arg)
{
	fprint_remote_signal((FILE*) arg, &remote, ncode);
	return 1;
}


/** View part: Record data for one button. */
static enum button_status get_button_data(struct button_state* btn_state,
					  struct main_state* state,
					  const struct opts* opts)
{
	const char* const MSG_BAD_STS = "Bad status in get_button_data: %d\n";
	const char* const MSG_BAD_RETURN = "Bad return from  get_button_data";
	enum button_status sts = STS_BTN_INIT_DATA;
	unsigned int retries;

	retries = 30;
	last_remote = NULL;
	sts = STS_BTN_INIT_DATA;
	sleep(1);
	while (retries > 0) {
		switch (sts) {
		case STS_BTN_INIT_DATA:
			printf("\nNow hold down button \"%s\".\n",
			       btn_state->buffer);
			fflush(stdout);
			flushhw();
			break;
		case STS_BTN_GET_DATA:
		case STS_BTN_GET_RAW_DATA:
			break;
		case STS_BTN_TIMEOUT:
			retries--;
			if (retries <= 0) {
				puts("Too many timeouts, giving up.");
				return STS_BTN_HARD_ERROR;
			}
			printf("Timeout (10 seconds), try again");
			printf(" (%d retries left).\n", retries);
			sts = STS_BTN_INIT_DATA;
			continue;
		case STS_BTN_GET_TOGGLE_BITS:
			do_get_toggle_bit_mask(&remote, state, opts);
			return STS_BTN_ALL_DONE;
		case STS_BTN_SOFT_ERROR:
			retries--;
			fputs("Something went wrong: ", stdout);
			fputs(btn_state->message, stdout);
			if (retries > 0) {
				printf("Please try again. (%d retries left)\n",
				       retries - 1);
				sts = STS_BTN_INIT_DATA;
				continue;
			}
			puts("Too many errors.");
			puts("Check TROUBLESHOOTING in irrecord(1) manpage.");
			if (!opts->force)
				puts("Or try using the -f option.");
			return STS_BTN_HARD_ERROR;
		case STS_BTN_BUTTON_DONE:
			ncode_list_add(&(btn_state->ncode));
			return sts;
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


/** Get name of button to be recorded from user, handle multiple ones. */
static enum button_status get_button_name(struct button_state* btn_state)
{
	char* s;

	printf("\nPlease enter the name for the next button"
	       " (press <ENTER> to finish recording)\n");
	s = fgets(btn_state->buffer, sizeof(btn_state->buffer), stdin);
	if (s != btn_state->buffer) {
		btn_state_set_message(btn_state, "fgets() failed\n");
		return STS_BTN_HARD_ERROR;
	}
	s = strchr(s, '\n');
	if (s != NULL)
		*s = '\0';
	if (ncode_list_find_name(btn_state->buffer) == NULL)
		return STS_BTN_GET_NAME;
	printf("Button %s is already recorded, removing old data.\n",
	       btn_state->buffer);
	if (!(ncode_list_remove_name(btn_state->buffer))) {
		btn_state_set_message(btn_state,
				      "Cannot remove old button\n");
		return STS_BTN_HARD_ERROR;
	}
	return STS_BTN_GET_NAME;
}


/** View part of record_buttons. Leaves recorded state in global remote. */
void do_record_buttons(struct main_state* state, const struct opts* opts)
{
	struct button_state btn_state;
	enum button_status sts = STS_BTN_INIT;

	config_file_setup(state, opts);
	button_state_init(&btn_state, state->started_as_root);
	flushhw();
	while (1) {
		switch (sts) {
		case STS_BTN_INIT:
			break;
		case STS_BTN_GET_NAME:
			sts = get_button_name(&btn_state);
			if (sts != STS_BTN_GET_NAME)
				continue;
			else
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
		case STS_BTN_RECORD_DONE:
			ncode_list_for_each(printf_signal_func,
					    (void*) state->fout);
			fprint_remote_signal_foot(state->fout, &remote);
			fprint_remote_foot(state->fout, &remote);
			fclose(state->fout);
			break;
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
			fprintf(stderr, "Unrecoverable error: %s\n",
				btn_state.message);
			fputs("Giving up\n", stderr);
			fputs("Check TROUBLESHOOTING in irrecord manpage.\n",
			      stderr);
			exit(EXIT_FAILURE);
		}
		sts = record_buttons(&btn_state, sts, state, opts);
	}
}


/** Check that there is not "too" much ambient light & noise. */
void check_ambient_light(const struct opts* opts)
{
	char buff[4096];
	ssize_t count = 0;
	ssize_t sum = 0;
	struct timeval start;
	struct timeval now;

	puts(MSG_NOISE_INTRO);
	gettimeofday(&start, NULL);
	gettimeofday(&now, NULL);
	while (time_elapsed(&start, &now) < NOISE_TIMEOUT_US) {
		count = raw_read(buff, sizeof(buff), NOISE_TIMEOUT_US);
		if (count < 0)
			break;
		sum += count;
		gettimeofday(&now, NULL);
	}
	puts("");
	if (count == -1) {
		perror("Cannot read from device");
	} else if (sum < NOISE_LIMIT) {
		printf("No significant noise (received %d bytes)\n\n",
		       (int) sum);
	} else {
		printf("Here is a lof of noise (%d bytes received)\n",
		       (int) sum);
		puts(MSG_TOO_MUCH_NOISE);
		getchar();
	}
}


/** Report remote data to user and log it. */
static void remote_report(struct ir_remote* remote)
{
	printf("Signals are %s encoded.\n",
	       is_biphase(remote) ? "biphase" : "pulse");
	printf("Signal length is %d\n", remote->bits);
	if (is_rc5(remote)) printf("RC5 encoding\n");
	else if (is_rc6(remote)) printf("RC6 encoding\n");
	else if (is_rcmm(remote)) printf("RCMM encoding\n");
	else if (is_goldstar(remote)) printf("GOLDSTAR encoding\n");
	else if (is_grundig(remote)) printf("GRUNDIG encoding\n");
	else if (is_bo(remote)) printf("Bang & Olufsen encoding\n");
	else printf("Unknown encoding\n");
	log_debug("%d %u %u %u %u %u %d %d %d %u\n",
		  remote->bits, (__u32)remote->pone, (__u32)remote->sone,
		  (__u32)remote->pzero, (__u32)remote->szero,
		  (__u32)remote->ptrail, remote->flags, remote->eps,
		  remote->aeps, (__u32)remote->gap);
}


/** View part of get_lengths. */
static int mode2_get_lengths(const struct opts* opts, struct main_state* state)
{
	const char* const MSG_AGAIN =
		"\nPlease keep on pressing buttons like described above.";
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
			sts = get_lengths(&lengths_state,
					  &remote,
					  opts->force,
					  debug);
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
				fputs("No data for 10 secs, aborting\n",
				      stderr);
				exit(EXIT_FAILURE);
			case STS_LEN_NO_GAP_FOUND:
				puts(MSG_NO_GAP_FOUND_WARNING);
				remote.gap = DEFAULT_GAP;
				printf("Press RETURN to continue.\n");
				getchar();
				return 1;
			case STS_LEN_TOO_LONG:
				fputs("Signal too long\n", stderr);
				puts("Creating config file in raw mode.");
				set_protocol(&remote, RAW_CODES);
				remote.eps = eps;
				remote.aeps = aeps;
				break;
			case STS_LEN_AGAIN_INFO:
				printf("\nGot gap (%d us)}\n", remote.gap);
				puts(MSG_AGAIN);
				sts = STS_LEN_AGAIN;
				continue;
			case STS_LEN_AGAIN:
				diff = lengths_state.keypresses -
					lengths_state.keypresses_done;
				for (i = 0; i < diff; i += 1)
					printf(".");
				fflush(stdout);
				lengths_state.keypresses_done += diff;
				sts = STS_LEN_AGAIN;
				break;
			}
		}
		free_all_lengths();
	}
	remote_report(&remote);
	return sts;
}


/** View part of get_gap(). */
void lirccode_get_lengths(const struct opts* opts, struct main_state* state)
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
			unlink(opts->tmpfile);
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


int count_ncode(struct ir_ncode* ncode, void* arg)
{
	int* sumptr = (int*) arg;

	*sumptr += 1;
	return 1;
}


int ncode_free_func(struct ir_ncode* ncode, void* arg)
{
	ncode_free(ncode);
	return 1;
}


/** Return number of button definitions in *remote. */
static unsigned int remote_codes_length(const struct ir_remote* remote)
{
	unsigned int count = 0;

	ncode_list_for_each(count_ncode, &count);
	return count;
}


/** Check for too few buttons before saving data. */
static void check_too_few_buttons(const struct ir_remote* remote,
				  const struct opts* opts)
{
	unsigned int count = remote_codes_length(remote);

	if (count == 0) {
		puts(MSG_NO_BUTTONS);
	} else if (count == 1 && !opts->force) {
		puts(MSG_TOO_FEW_BUTTONS);
	}
}


/** Get name of a code from user, setup filename and backup file. */
void get_name(struct ir_remote* remote, struct opts* opts)
{
	char buff[256];
	char basename[256];
	char path[256];
	char* s;

	if (!opts->update)
		remote->name = NULL;
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
		if (strlen(buff) == 0)
			continue;
		for (s = buff; *s; s += 1) {
			if (isspace(*s) || !isascii(*s) || iscntrl(*s)) {
				printf("Bad character: %c (x%x)\n", *s, *s);
				goto again;
			}
		}
		strncpy(basename, opts->filename, sizeof(basename) - 1);
		if (strchr(basename, '.') != NULL)
			*strchr(basename, '.') = '\0';
		if (strcmp(basename, buff) == 0 && !opts->update) {
			fflush(stdout);
			puts("You cannot use the same name if not updating\n");
			puts("Please use another name\n");
			continue;
		}
		snprintf(path, sizeof(path), "%s.lircd.conf.bak", buff);
		if (access(path, F_OK) == 0) {
			printf("Backup file %s already exists.\n", path);
			puts("Choose another name or remove backup file.\n");
			continue;
		}
		remote->name = strdup(buff);
	}
	opts->backupfile = opts->update ? strdup(path) : NULL;
	snprintf(path, sizeof(path), "%s.lircd.conf", buff);
	opts->filename = strdup(path);
	printf("Using %s as output filename\n\n", opts->filename);
	if (opts->update && access(opts->filename, F_OK) == 0) {
		snprintf(buff, sizeof(buff),
			 "cp -p %s %s", path, opts->backupfile);
		if (system(buff) != 0)
			printf("Warning: Cannot create backup file.\n");
	} else if (opts->backupfile != NULL) {
		free((void*) opts->backupfile);
		opts->backupfile = NULL;
	}
}


int main(int argc, char** argv)
{
	struct opts opts = {0};
	struct main_state state = {0};
	int r = 1;

	get_options(argc, argv, argv[optind], &opts);
	if (opts.list_namespace) {
		fprint_namespace(stdout);
		exit(EXIT_SUCCESS);
	}
	get_commandline(argc, argv,
			opts.commandline, sizeof(opts.commandline));
	if (geteuid() == 0){
		state.started_as_root = 1;
		drop_root_cli(seteuid);
	}
	do_init(&opts, &state);

	puts(MSG_WELCOME);
	printf("\nPress RETURN to continue.\n");
	getchar();
	if (curr_driver->name && strcmp(curr_driver->name, "devinput") == 0) {
		puts("\n\n");
		puts(MSG_DEVINPUT);
		printf("\nPress RETURN to continue.\n");
		getchar();
	}
	check_ambient_light(&opts);
	if (remote.name == NULL || !opts.update)
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
	do_record_buttons(&state, &opts);
	check_too_few_buttons(&remote, &opts);
	if (is_raw(&remote)) {
		r = rename(opts.tmpfile, opts.filename);
		if (r != 0)
			perror("Cannot write final file");
		r = r == 0 ? 1 : 0;
	} else {
		r = config_file_finish(&state, &opts);
	}
	printf("\nSuccessfully written config file %s\n", opts.filename);
	if (access(opts.tmpfile, F_OK) == 0 && unlink(opts.tmpfile) != 0) {
		fprintf(stderr,
			"Cannot remove temporary file %s\n", opts.tmpfile);
	}
	if (opts.backupfile != NULL)
		printf("Saving old config file in %s\n", opts.backupfile);
	return r ? EXIT_SUCCESS : EXIT_FAILURE;
}
