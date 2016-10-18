/**
 * @file lircd-uinput.c
 * This file implements the uinput forwarding service.
 */

#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <string>
#include <set>
#include <unordered_map>

#include <linux/input.h>
#include "lirc/input_map.h"

#include "lirc_private.h"
#include "line_buffer.h"


static const logchannel_t logchannel = LOG_APP;

/** The name used to register the uinput device. */
static const char* const DEVNAME = "lircd-uinput";

static const char* const HELP =
	"\nUsage: lircd-uinput [options] [socket]\n\n"
	"Argument:\n"
	"\t socket: lircd output socket or test file [" LIRCD "]\n"
	"\nOptions:\n"
	"\t -u --uinput=uinput \t\tuinput device [/dev/uinput]\n"
	"\t -r --release-suffix=suffix \tRelease events suffix [_UP]\n"
	"\t -R --repeat=delay[,period]\tSet kernel repeat parameters [none]\n"
	"\t -a --add-release-events\tAdd synthetic release events [no]\n"
	"\t -d --disable=file\t\tDisable buttons listed in file\n"
        "\t -D[level] --loglevel[=level]\t"
			"'info', 'warning', 'notice', etc., or 3..10.\n"
	"\t -L --logfile=file\t\tLog file path (default: use syslog)'\n"
	"\t -O --options-file\t\tOptions file"
			" [" SYSCONFDIR "/lirc/lirc_options.conf]\n"
	"\t -h --help\t\t\tDisplay this message\n"
	"\t -v --version\t\t\tDisplay version\n";


static const struct option cli_options[] = {
	{ "help",	        no_argument,       NULL, 'h' },
	{ "version",	        no_argument,       NULL, 'v' },
	{ "options-file",       required_argument, NULL, 'O' },
	{ "uinput",	        required_argument, NULL, 'u' },
	{ "release-suffix",     required_argument, NULL, 'r' },
	{ "repeat",             required_argument, NULL, 'R' },
	{ "add-release-events", no_argument,       NULL, 'a' },
	{ "loglevel",	        optional_argument, NULL, 'D' },
	{ "disable",	        required_argument, NULL, 'd' },
	{ "logfile",	        required_argument, NULL, 'L' },
	{ 0,		        0,		   0,    0   }
};


/** Used in parse_options(), matches cli_options above. */
static const char* const optstring = "ad:D::hO:r:R:u:L:v";

/** Keys in lirc_options db and thus in lirc_options.conf config file. */
static const char* const DEBUG_OPT    =	"lircd:debug";
static const char* const LOGFILE_OPT  =	"lircd-uinput:logfile";
static const char* const UINPUT_OPT   =	"lircd-uinput:uinput";
static const char* const SUFFIX_OPT   =	"lircd-uinput:release-suffix";
static const char* const TIMEOUT_OPT  =	"lircd-uinput:release-timeout";
static const char* const RELEASE_OPT  =	"lircd-uinput:add-release-events";
static const char* const REPEAT_OPT   =	"lircd-uinput:repeat";
static const char* const INPUT_ARG    =	"lircd-uinput:output";
static const char* const DISABLED_OPT =	"lircd-uinput:disabled";

/** Runtime options decoded from command line, config file and defaults. */
struct options {
	const char* uinput_path;    /**< The --uinput option. */
	const char* input_path;     /**< The socket argument. */
	const char* release_suffix; /**< The --release-suffix option. */
	unsigned release_timeout;   /**< Hidden release-timeout option. */
	unsigned repeat_delay;      /**< delay part of --repeat option. */
	unsigned repeat_period;     /**< period part of --repeat option. */
	bool add_release_events;    /**< The --add-release-events option. */
	loglevel_t loglevel;        /**< The --loglevel option. */
	const char* logfile;        /**< The --logfile option. */
	const char* disabled_path;  /**< The --disable file path option. */
	std::set<std::string>
		disabled_buttons;   /**< Disabled button names. */
	int inputfd;		    /**< input open  file descriptor. */
	int uinputfd;               /**< output open uinput descriptor. */
};


/** Cached code lookups for button names. */
class CodeCache {

	private:
		struct Entry {
			linux_input_code code;
			bool is_release;  /**< Button is a release event. */
		};

		typedef std::unordered_map<std::string, struct Entry> code_map;
		code_map cache;

	public:
		void add(const char* button, linux_input_code c, bool release)
		{
			Entry* entry = new Entry();
			entry->code = c;
			entry->is_release = release;
			cache[button] = *entry;
		};

		bool lookup(const std::string button_name,
			    linux_input_code* code,
			    bool* is_release)
		{
			auto it = cache.find(button_name);
			if (it == cache.end())
				return false;
			*code = it->second.code;
			*is_release = it->second.is_release;
			return true;
		};

		CodeCache() { cache = code_map(); }
};


/** Cache for keycode lookups. */
static CodeCache code_cache = CodeCache();

/** Set by send_message(), used when sending release events. */
static std::string last_button_press = std::string("");


/** Setup defaults for the CLI options parsing. */
static void add_defaults(void)
{
	char level[4];

	snprintf(level, sizeof(level), "%d", lirc_log_defaultlevel());
	const char* const suffix = options_getstring("lircd:release");
	const char* const socket = options_getstring("lircd:output");

	const char* const defaults[] = {
		DEBUG_OPT,		level,
		LOGFILE_OPT,		"syslog",
		UINPUT_OPT,		"/dev/uinput",
		REPEAT_OPT,	 	(const char*) NULL,
		SUFFIX_OPT,		suffix ? suffix : "_UP",
		TIMEOUT_OPT,		"200",
		INPUT_ARG,		socket ? socket : LIRCD,
		DISABLED_OPT,		(const char*)NULL,
		(const char*)NULL,	(const char*)NULL
	};
	options_add_defaults(defaults);
}


/** Parse options and store in lirc_options db, argument to options_load(). */
static void parse_options(int argc, char** const argv)
{
	int c;

	optind = 1;
	add_defaults();
	c = getopt_long(argc, argv, optstring, cli_options, NULL);
	while (c != -1) {
		switch (c) {
		case 'h':
			fputs(HELP, stdout);
			exit(EXIT_SUCCESS);
		case 'v':
			printf("lircd-uinput %s\n", VERSION);
			exit(EXIT_SUCCESS);
		case 'O':
			break;
		case 'r':
			options_set_opt(SUFFIX_OPT, optarg);
			break;
		case 'R':
			options_set_opt(REPEAT_OPT, optarg);
			break;
		case 'u':
			options_set_opt(UINPUT_OPT, optarg);
			break;
		case 'a':
			options_set_opt(RELEASE_OPT, "True");
			break;
		case 'd':
			options_set_opt(DISABLED_OPT, optarg);
			break;
		case 'D':
			options_set_opt(DEBUG_OPT, optarg);
			break;
		case 'L':
			options_set_opt(LOGFILE_OPT, optarg);
			break;
		default:
			fputs("Usage: lirc.uinput [options] [config-file]\n",
                              stderr);
			exit(EXIT_FAILURE);
		}
		c = getopt_long(argc, argv, optstring, cli_options, NULL);
	}
	if (optind == argc - 1) {
		options_set_opt(INPUT_ARG, argv[optind]);
	} else if (optind != argc) {
		fputs("lircd-uinput: too many arguments (max one)\n", stderr);
		exit(EXIT_FAILURE);
	}
}


/** Handle the --disabled option, initiates the set of disabled buttons. */
static void parse_disabled(const char* path, std::set<std::string>* buttons)
{
	FILE* f;
	char buff[128];

	f = fopen(path, "r");
	if (f == NULL) {
		fprintf(stderr, "Cannot open %s for reading", path);
		exit(EXIT_FAILURE);
	}
	*buttons = std::set<std::string>();
	while (fgets(buff, sizeof(buff), f) != NULL) {
		std::string* button = new std::string(buff);
		const size_t pos = button->rfind('\n');
		if (pos != std::string::npos)
			button->erase(pos, 1);
		for (auto& c: *button)
			c = toupper(c);
		buttons->insert(*button);
	}
	fclose(f);
}


/** Handle the --repeat option, sets repeat_period and repeat_delay. */
static bool parse_repeat(struct options* opts)
{
	const char* repeat_opt = options_getstring(REPEAT_OPT);
	char optbuff[64];
	char* opt = optbuff;
	long value;

	opts->repeat_delay = 0;
	opts->repeat_period = 0;
	if (repeat_opt == NULL)
		return true;

	// Parsing: [delay][,period]
	strncpy(optbuff, repeat_opt, sizeof(optbuff) - 1);
	if (isdigit(*opt)) {
		value = strtol(opt, &opt, 10);
		if (value == LONG_MAX || value == LONG_MIN )
			return false;
		opts->repeat_delay = static_cast<unsigned>(value);
	}
	if (opt == NULL || *opt == '\0')
		return true;
	if (*opt != ',')
		return false;
	opt++;
	if (*opt == '\0')
		return true; // Lets accept omitted period.
	if (!isdigit(*opt))
		return false;
	value = strtol(opt, &opt, 10);
	if (value == LONG_MAX || value == LONG_MIN)
		return false;
	if (opt != NULL && *opt != '\0')
		return false;
	opts->repeat_period = static_cast<unsigned>(value);
	return true;
}


/** Log some info on effective options. */
static void log_options(const struct options* opts)
{
	log_info("Reading data from %s, writing to %s",
		 opts->input_path, opts->uinput_path);
	if (opts->add_release_events) {
		log_info("Adding release events after a %d ms timeout",
			 opts->release_timeout);
	}
	if (opts->disabled_path != NULL) {
		log_info("Disabling %d key(s)",
			 opts->disabled_buttons.size());
	}
	if (opts->repeat_delay != 0) {
		log_info("Setting kernel repeat delay to %d ms",
			 opts->repeat_delay);
	}
	if (opts->repeat_period != 0) {
		log_info("Setting kernel repeat period to %d ms",
			 opts->repeat_period);
	}
	log_info("Using \"%s\" as release suffix", opts->release_suffix);
}


/** Get all options from lirc_options db and store in *opts. */
static void parse_options(struct options* opts)
{
	opts->input_path = options_getstring(INPUT_ARG);
	opts->uinput_path = options_getstring(UINPUT_OPT);
	opts->logfile = options_getstring(LOGFILE_OPT);
	opts->disabled_path = options_getstring(DISABLED_OPT);
	opts->release_timeout = options_getint(TIMEOUT_OPT);
	opts->release_suffix = options_getstring(SUFFIX_OPT);
	opts->add_release_events = options_getboolean(RELEASE_OPT);
	if( !parse_repeat(opts)) {
		fputs("Cannot parse --repeat option\n", stderr);
		opts->repeat_delay = opts->repeat_period = 0;
	}
	opts->disabled_path = options_getstring(DISABLED_OPT);
	if (opts->disabled_path != NULL)
		parse_disabled(opts->disabled_path, &opts->disabled_buttons);
	const char* opt = options_getstring(DEBUG_OPT);
	opts->loglevel = string2loglevel(opt);
	if (opts->loglevel == LIRC_BADLEVEL) {
		fprintf(stderr, "Bad configuration loglevel:%s\n", opt);
		fprintf(stderr, HELP);
		fprintf(stderr, "Falling back to 'info'\n");
		opts->loglevel = LIRC_INFO;
	}
	opts->inputfd = -1;
	opts->uinputfd = -1;
}


/**
 * Returns linux keycode for button name or KEY_RESERVED if no match,
 * sets is_release to reflect if this is a release event.
 */
static linux_input_code get_keycode(const char* button_name,
				    const char* suffix,
				    bool* is_release)
{
	linux_input_code input_code;
	*is_release = false;

	if (get_input_code(button_name, &input_code) != -1)
		return input_code;

	std::string s = button_name;
	size_t pos = s.rfind(suffix);
	if (pos == std::string::npos)
		return KEY_RESERVED;
	s = s.substr(0, pos);
	if (get_input_code(s.c_str(), &input_code) == -1)
		return KEY_RESERVED;
	*is_release = true;
	return input_code;
}


/** Send a struct input_event to an uinput fd, return success. */
static bool write_event(int fd, unsigned type, linux_input_code code, int val)
{
	struct input_event event;

	memset(&event, 0, sizeof(event));
	event.type = type;
	event.code = code;
	event.value = val;
	return write(fd, &event, sizeof(event)) == sizeof(event);
}


/** Given a button, format and send struct input_events to /dev/uinput. */
static void send_message(const struct options* opts, const char* button)
{
	linux_input_code code;
	bool is_release;

	button = button == NULL ? "(null)" : button;
	if (!code_cache.lookup(button, &code, &is_release)) {
		log_trace("Cache miss for %s", button);
		code = get_keycode(button, opts->release_suffix, &is_release);
		code_cache.add(button, code, is_release);
	}
	if (code == KEY_RESERVED) {
		log_info("Dropping non-standard symbol %s", button);
		return;
	}
	// event.value: 0 => release, 1 => press, 2 => autorepeat(not used).
	const int value = is_release ? 0 : 1;
	log_debug("Sending %s as %d:%d", button, code, value);

	if (!write_event(opts->uinputfd, EV_KEY, code, value))
		log_perror_err("Writing regular event to uinput failed");
	if (!write_event(opts->uinputfd, EV_SYN, SYN_REPORT, 0))
		log_perror_err("Writing EV_SYN to uinput failed");
	// send_release_event() needs to know if an event is required
	if (opts->add_release_events && !is_release)
		last_button_press = std::string(button);
	else
		last_button_press = "";
}


/** Process a single line of input from the socket (or test file). */
static void process_line(const struct options* opts, std::string line)
{
	int r;
	char button[PACKET_SIZE + 1];
	char remote[PACKET_SIZE + 1];
	int reps;

	r = sscanf(line.c_str(), "%*x %x %s %s\n", &reps, button, remote);
	if (r != 3) {
		log_warn("Cannot parse line: %s", line.c_str());
		return;
	}
	if (opts->disabled_path && opts->disabled_buttons.count(button) == 1)
		log_debug("Skipping disabled key %s", button)
	else
		send_message(opts, button);
}


/** Process all available lines in line_buffer. */
static void process_lines(const struct options* opts, LineBuffer* line_buffer)
{
	std::string line;

	while (line_buffer->has_lines()) {
		line = line_buffer->get_next_line();
		log_trace("Input: %s", line.c_str());
		process_line(opts, line);
	}
}


/** If required, send a release event after a read timeout. */
static void send_release_event(const struct options* opts)
{
	if (!opts->add_release_events)
		return;
	if (last_button_press == "")
		return;
	std::string button_name = last_button_press + opts->release_suffix;
	send_message(opts, button_name.c_str());
	last_button_press = "";
}


/** The main worker. */
static void lircd_uinput(const struct options* opts)
{
	int r;
        char buffer[PACKET_SIZE + 1];
	LineBuffer line_buffer;
	struct pollfd fds;
	int timeout = opts->add_release_events ? opts->release_timeout : -1;

	while (true) {
		fds.fd = opts->inputfd;
		fds.events = POLLIN;
		fds.revents = 0;
		r = curl_poll(&fds, 1, timeout);
		if (r == 0) {
			send_release_event(opts);
			continue;
		}
		if ((fds.revents & POLLERR) != 0 || r < 0 ) {
			log_notice("POLLERR or curl_poll() error, exiting.");
			exit(EXIT_FAILURE);
		}
		r = read(opts->inputfd, buffer, PACKET_SIZE);
		if (r > 0) {
			line_buffer.append(buffer,
					   static_cast<size_t>(r));
			process_lines(opts, &line_buffer);
		} else if (r == -1) {
			log_perror_warn("lircd_uinput(): read() error");
		} else if (r == 0 || (fds.revents & POLLHUP) != 0) {
			log_debug("POLLHUP or no data: exiting .");
			exit(0);
		}
	}
}


/** Register all keys as active besides those --disabled. */
static bool register_keys(const options* opts, int fd)
{
	bool dummy;
	linux_input_code code;
	auto disabled_keys = std::set<linux_input_code>();

	for (auto button: opts->disabled_buttons) {
		code = get_keycode(std::string(button).c_str(),
				   opts->release_suffix,
				   &dummy);
		disabled_keys.insert(code);
	}
	for (int key = KEY_RESERVED; key <= KEY_MAX; key++) {
		if (disabled_keys.count(key) == 1) {
			log_debug("Cowardly refusing to register %d", key);
			continue;
		}
		if (ioctl(fd, UI_SET_KEYBIT, key) != 0)
			return false;
	}
	return true;
}


/** Try to setup the uinput output device fd. Returns valid fd or -1.  */
static int setup_uinputfd(const options* opts)
{
	int fd;
	struct uinput_user_dev dev;
	bool ok;

	fd = open(opts->uinput_path, O_RDWR);
	if (fd == -1) {
		log_perror_err("Cannot open uinput device: %s",
			       opts->uinput_path);
		return -1;
	}
	memset(&dev, 0, sizeof(dev));
	strncpy(dev.name, DEVNAME, sizeof(dev.name) - 1);
	ok = write(fd, &dev, sizeof(dev)) == sizeof(dev)
		&& ioctl(fd, UI_SET_EVBIT, EV_KEY) == 0
		&& ioctl(fd, UI_SET_EVBIT, EV_REP) == 0
		&& register_keys(opts, fd)
		&& ioctl(fd, UI_DEV_CREATE) == 0;
	if (ok)
		return fd;
	log_perror_err("could not setup uinput");
	close(fd);
	return -1;
}


/** Open the input file, a lircd socket or a plain file. Return fd or -1. */
static int open_input(const char* path)
{
	struct sockaddr_un addr;
	struct stat statbuf;
	int fd;

	if (stat(path, &statbuf) == -1) {
		log_perror_err("Cannot stat socket path %s", path);
		return -1;
	}
	if (S_ISREG(statbuf.st_mode)) {
		fd = open(path, O_RDONLY);
		if (fd >= 0)
			return fd;
		log_perror_err("Cannot open input file %s", path);
		return -1;
	}
	if (!S_ISSOCK(statbuf.st_mode)){
		log_perror_err("Unknown non-socket device %s", path);
		return -1;
	}
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		log_perror_err("socket() failure");
		return -1;
	}
	if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
		log_perror_err("Cannot connect to socket %s", addr.sun_path);
		return -1;
	}
	return fd;
}


static void set_kernel_repeat(const options* opts)
{
	if (opts->repeat_delay != 0) {
		log_debug("Setting kernel repeat delay to %d",
			  opts->repeat_delay);
		if (!write_event(opts->uinputfd,
				 EV_REP,
				 REP_DELAY,
				 opts->repeat_delay)) {
			log_warn("Cannot set kernel repeat delay");
		}
	}
	if (opts->repeat_period != 0) {
		log_debug("Setting kernel repeat period to %d",
			  opts->repeat_period);
		if (!write_event(opts->uinputfd,
				 EV_REP,
				 REP_PERIOD,
				 opts->repeat_period)) {
			log_warn("Cannot set kernel repeat period");
		}
	}
}


int main(int argc, char** argv)
{
	struct options opts = {0};

	options_load(argc, argv, NULL, parse_options);
	parse_options(&opts);
	lirc_log_set_file(opts.logfile);
	lirc_log_open("lircd-uinput", 1, opts.loglevel);
	log_options(&opts);

	opts.inputfd = open_input(opts.input_path);
        if (opts.inputfd == -1) {
		log_error("Cannot setup input file descriptor.")
		exit(1);
	}
        opts.uinputfd = setup_uinputfd(&opts);
        if (opts.uinputfd == -1) {
		log_error("Cannot setup uinput output file descriptor.")
		exit(1);
	}
	set_kernel_repeat(&opts);
	lircd_uinput(&opts);
}
