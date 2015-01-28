/****************************************************************************
** xmode2.c ****************************************************************
****************************************************************************
*
* xmode2 - shows the ir waveform of an IR signal
*
* patched together on Feb. 18th 1999 by
* Heinrich Langos <heinrich@mad.scientist.com>
*
* This program is based on the smode2.c file by Sinkovics Zoltan
* <sinko@szarvas.hu> which is a part of the LIRC distribution. It is
* just a conversion from svga to X with some basic support for resizing.
* I copied most of this comment.
*
* This program is based on the mode2.c file which is a part of the
* LIRC distribution. The main purpose of this program is to check
* operation of LIRC receiver hardware, and to see the IR waveform of
* the remote controller without an expensive oscilloscope. The time
* division is variable from 1 ms/div to extremely high values (integer
* type) but there is no point increasing this value above 20 ms/div,
* because one pulse is about 1 ms. I think this kind of presentation
* is much more exciting than the simple pulse&space output showed by
* mode2.
*
* Usage: xmode2 [-t (ms/div)] , default division is 5 ms/div
*
*
* compile: gcc -o xmode2 xmode2.c -L/usr/X11R6/lib -lX11
*
* version 0.01  Feb 18 1999
*   initial release
*
* version 0.02  Aug 24 1999
*   using select() to make the whole thing more responsive
* */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

#include "lirc_private.h"

const char* const  MSG_USE_RAW =
	"Please use the --raw option to access the device directly instead.";

const char* const MSG_BAD_DRIVER =
	"This program does not work for this driver and hardware.\n"
	"(the driver is not a LIRC_MODE2 type driver)";

Display* d1;
Window w0, w1;			/*w0 = root */
char w1_wname[] = "xmode2";
char w1_iname[] = "xmode2";
char font1_name[] = "-misc-fixed-*-r-*-*-12-*-*-*-*-*-iso8859-1";

int w1_x = 0, w1_y = 0;
unsigned int w1_w = 640, w1_h = 480, w1_border = 0;

XFontStruct* f1_str;
XColor xc1, xc2;
Colormap cm1;
XGCValues gcval1;
GC gc1, gc2;
XSetWindowAttributes winatt1;

long event_mask1;
XEvent event_return1;

static int point = 0;
static int showText = 0;
static int div_ = 5;
static int dmode = 0;
static struct stat s;
static int use_stdin = 0;
static int use_raw_access = 0;

static const char* device = "";
static char* geometry = NULL;

static struct option options[] = {
	{"help",      no_argument,	 NULL, 'h'},
	{"version",   no_argument,	 NULL, 'v'},
	{"device",    required_argument, NULL, 'd'},
	{"driver",    required_argument, NULL, 'H'},
	{"keep-root", required_argument, NULL, 'k'},
	{"geometry", required_argument, NULL, 'g'},
	{"timediv", required_argument, NULL, 't'},
	{"mode", no_argument, NULL, 'm'},
	{"raw", no_argument, NULL, 'r'},
	{"plugindir", required_argument, NULL, 'U'},
	{"driver-options", required_argument, NULL, 'A'},
	{0, 0, 0, 0}
};


static const char* const help =
	"\nUsage: xmode2 [options]\n"
	"\nOptions:\n"
	"    -d --device=device\t\tread from given device\n"
	"    -H --driver=driver\t\tuse given driver\n"
	"    -U --plugindir=dir\t\tLoad drivers from given dir\n"
	"    -g --geometry=geometry\twindow geometry\n"
	"    -t --timediv=value\t\tms per unit\n"
	"    -m --mode\t\t\tenable alternative display mode\n"
	"    -k --keep-root\t\tkeep root privileges\n"
	"    -r --raw\t\t\taccess device directly\n"
	"    -h --help\t\t\tdisplay usage summary\n"
	"    -v --version\t\tdisplay version\n"
	"    -A --driver-options=key:value[|key:value...]\n"
	"\t\t\t\tSet driver options\n"
	"The window responds to the following keys:\n"
	"    .1, .2, .5, 1, 2 & 5 set the timebase (ms)\n"
	"    m to toggle the display mode\n"
	"    q to quit\n";


static void add_defaults(void)
{
	char level[4];

	snprintf(level, sizeof(level), "%d", lirc_log_defaultlevel());
	const char* const defaults[] = {
		"lircd:plugindir",	    PLUGINDIR,
		"lircd:debug",		    level,
		"xmode2:driver",	    "devinput",
		"xmode2:analyse",	    "False",
		"xmode2:force",		    "False",
		"xmode2:disable-namespace", "False",
		"xmode2:dynamic-codes",	    "False",
		"xmode2:list_namespace",    "False",
		(const char*)NULL,	    (const char*)NULL
	};
	options_add_defaults(defaults);
}


static void parse_options(int argc, char** const argv)
{
	int c;

	add_defaults();
	char driver[64];
	const char* const optstring =  "U:hvd:H:g:t:mrA:";

	strcpy(driver, "default");
	while ((c = getopt_long(argc, argv, optstring, options, NULL)) != -1) {
		switch (c) {
		case 'h':
			puts(help);
			exit(EXIT_SUCCESS);
		case 'H':
			strncpy(driver, optarg, sizeof(driver) - 1);
			break;
		case 'v':
			printf("%s %s\n", progname, VERSION);
			exit(EXIT_SUCCESS);
		case 'd':
			device = optarg;
			break;
		case 'g':
			geometry = optarg;
			break;
		case 't':       /* timediv */
			div_ = strtol(optarg, NULL, 10);
			break;
		case 'k':
			unsetenv("SUDO_USER");
			break;
		case 'm':
			dmode = 1;
			break;
		case 'r':
			use_raw_access = 1;
			break;
		case 'U':
			options_set_opt("lircd:plugindir", optarg);
			break;
		case 'A':
			options_set_opt("lircd:driver-options", optarg);
			break;
		default:
			printf("Usage: %s [options]\n", progname);
			exit(EXIT_FAILURE);
		}
	}
	if (optind < argc) {
		fprintf(stderr, "%s: too many arguments\n", progname);
		exit(EXIT_FAILURE);
	}
	if (hw_choose_driver(driver) != 0) {
		fprintf(stderr, "Driver `%s' not found", driver);
		fputs(" (wrong or missing -U/--plugindir?)\n", stderr);
		hw_print_drivers(stderr);
		exit(EXIT_FAILURE);
	}
}


void initscreen(char* geometry)
{
	d1 = XOpenDisplay(0);
	if (d1 == NULL) {
		puts("Can't open display.");
		exit(0);
	}

	if (geometry != NULL)
		XParseGeometry(geometry, &w1_x, &w1_y, &w1_w, &w1_h);

	/*Aufbau der XWindowsAttribStr */
	w0 = DefaultRootWindow(d1);
	winatt1.background_pixel = BlackPixel(d1, 0);
	winatt1.backing_store = WhenMapped;
	winatt1.event_mask = KeyPressMask | StructureNotifyMask | ExposureMask;
	w1 = XCreateWindow(d1, w0, w1_x, w1_y, w1_w, w1_h, w1_border, CopyFromParent, InputOutput, CopyFromParent,
			   CWBackPixel | CWBackingStore | CWEventMask, &winatt1);

	XStoreName(d1, w1, w1_wname);
	XSetIconName(d1, w1, w1_iname);
	XMapWindow(d1, w1);

	cm1 = DefaultColormap(d1, 0);
	if (!XAllocNamedColor(d1, cm1, "blue", &xc1, &xc2))
		puts("couldn't allocate blue color");
	f1_str = XLoadQueryFont(d1, font1_name);
	if (f1_str == NULL) {
		puts("could't load font");
		exit(EXIT_FAILURE);
	}

	gcval1.foreground = xc1.pixel;
	gcval1.font = f1_str->fid;
	gcval1.line_style = LineSolid;
	gc1 = XCreateGC(d1, w1, GCForeground | GCLineStyle, &gcval1);
	gcval1.foreground = WhitePixel(d1, 0);
	gc2 = XCreateGC(d1, w1, GCForeground | GCLineStyle | GCFont, &gcval1);
}


void closescreen(void)
{
	XUnmapWindow(d1, w1);
	XCloseDisplay(d1);
}

void drawGrid(int div)
{
	char textbuffer[80];
	int x;

	XClearWindow(d1, w1);
	for (x = 0; x < (int)w1_w; x += 10)
		XDrawLine(d1, w1, gc1,  x, 0,  x, w1_h);

	sprintf(textbuffer, "%5.3f ms/div", div/100.0);
	XDrawString(d1, w1, gc2, w1_w - 100, 10, textbuffer, strlen(textbuffer));
	XFlush(d1);
}

int main(int argc, char** argv)
{
	fd_set rfds;
	int xfd, maxfd;

	int fd;
	__u32 mode;
	lirc_t data = 0;
	lirc_t x1, y1, dx;
	int result;
	char textbuffer[80];
	const char* opt;

	lirc_log_open("xmode2", 0, LIRC_INFO);
	hw_choose_driver(NULL);
	options_load(argc, argv, NULL, parse_options);
	if (strcmp(device, LIRCD) == 0) {
		fprintf(stderr, "%s: refusing to connect to lircd socket\n", progname);
		return EXIT_FAILURE;
	}
	if (!isatty(STDIN_FILENO)) {
		use_stdin = 1;
		fd = STDIN_FILENO;
	} else if (use_raw_access) {
		fd = open(device, O_RDONLY);
		if (fd == -1) {
			fprintf(stderr, "%s: error opening %s\n", progname, device);
			perror(progname);
			exit(EXIT_FAILURE);
		}
		;

		if ((fstat(fd, &s) != -1) && (S_ISFIFO(s.st_mode))) {
			/* can't do ioctls on a pipe */
		} else if ((fstat(fd, &s) != -1) && (!S_ISCHR(s.st_mode))) {
			fprintf(stderr, "%s: %s is not a character device\n", progname, device);
			fprintf(stderr, "%s: use the -d option to specify the correct device\n", progname);
			close(fd);
			exit(EXIT_FAILURE);
		} else if (ioctl(fd, LIRC_GET_REC_MODE, &mode) == -1) {
			puts("This program is only intended for receivers supporting the pulse/space layer.");
			puts("Note that this is no error, but this program "
			     "simply makes no sense for your\n"
			     "receiver.");
			puts("In order to test your setup run lircd with "
			     "the --nodaemon option and\n"
			     "then check if the remote works with the irw tool.");
			close(fd);
			exit(EXIT_FAILURE);
		}
	} else {
		curr_driver->open_func(device);
		opt = options_getstring("lircd:driver-options");
		if (opt != NULL)
			if (drv_handle_options(opt) != 0) {
				fprintf(stderr,
				"Cannot set driver (%s) options (%s)\n",
				curr_driver->name, opt);
				return EXIT_FAILURE;
			}
		if (curr_driver->init_func  && !curr_driver->init_func()) {
			fputs("Cannot initialize hardware\n", stderr);
			exit(EXIT_FAILURE);
		}

		fd = curr_driver->fd;   /* please compiler */
		mode = curr_driver->rec_mode;
		if (mode != LIRC_MODE_MODE2) {
			if (strcmp(curr_driver->name, "default") == 0) {
				puts(MSG_USE_RAW);
			} else {
				puts(MSG_BAD_DRIVER);
			}
			exit(EXIT_FAILURE);
		}
	}

	if (geteuid() == 0)
		drop_root_cli(setuid);
	initscreen(geometry);
	xfd = XConnectionNumber(d1);
	maxfd = fd > xfd ? fd : xfd;
	y1 = 20;
	x1 = 0;
	drawGrid(div_);
	while (1) {
		while (XPending(d1) > 0) {
			XNextEvent(d1, &event_return1);
			switch (event_return1.type) {
			case KeyPress:
				if (1 == XLookupString(&event_return1.xkey,
						       textbuffer,
						       sizeof(textbuffer),
						       NULL, NULL)) {
					switch (textbuffer[0]) {
					case 'q':
					closescreen();
					exit(1);
					case 'm':
						dmode = !dmode;
						break;
					/*
					 * Switch the time-base, the screen is not cleared because it is useful to see
					 * the IR data with different time-bases on the same screen. The new time-base
					 * text is drawn to the screen and the waveform is set to start on a new line.
					 */
					case '.':
						point = 1;	/* Remember the decimal point */
						break;
					case '1':
						div_ = 100;	/* 1ms, 1000us per division / 10 pixels per division. */
						showText = 1;
						break;
					case '2':
						div_ = 200;
						showText = 1;
						break;
					case '5':
						div_ = 500;
						showText = 1;
						break;
					}
					if (showText) {
						showText = 0;
						if (point) {
							point = 0;
							div_ /= 10;
						}
						y1 += 25;
						sprintf(textbuffer, "%5.3f ms/div", div_ / 100.0);
						XDrawString(d1, w1, gc2,
							    w1_w - 100,
							    y1,
							    textbuffer,
							    strlen(textbuffer));
						y1 += 5;
						x1 = 0;
					}
				}
				break;
			case Expose:
			case ConfigureNotify:
				switch (event_return1.type) {
				case Expose:
					break;
				case ConfigureNotify:
					if ((int) w1_w == event_return1.xconfigure.width &&
					    (int) w1_h == event_return1.xconfigure.height)
						continue;

					w1_w = event_return1.xconfigure.width;
					w1_h = event_return1.xconfigure.height;
					break;
				}
				y1 = 20;
				x1 = 0;
				drawGrid(div_);
				break;
			default:
				break;
			}
		}

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		FD_SET(xfd, &rfds);

		select(maxfd + 1, &rfds, NULL, NULL, NULL);

		if (FD_ISSET(fd, &rfds)) {
			if (use_stdin) {
				static int space = 1;
				unsigned long scan;

				if (space)
					result = fscanf(stdin, "space %ld\n", &scan);
				else
					result = fscanf(stdin, "pulse %ld\n", &scan);
				if (result == 1) {
					data = (lirc_t)scan;
					if (!space)
						data |= PULSE_BIT;
				} else {
					fd = STDOUT_FILENO;
				}
				space = !space;
			} else {
				/*
				 * Must use the driver read function, the UDP driver reformats the data!
				 */
				data = curr_driver->readdata(0);
				if (data == 0) {
					fprintf(stderr, "readdata() failed\n");
					result = 0;
				} else {
					result = 1;
				}
			}
			if (result != 0) {
#ifdef DEBUG
				if (data & PULSE_BIT)
					printf("%.8x\t", data);
				else
					printf("%.8x\n", data);
#endif
				dx = (data & PULSE_MASK) / (div_);
				if (dx > 400) {
					if (!dmode)
						y1 += 15;
					else
						y1++;
					x1 = 0;
				} else {
					if (x1 == 0) {
						if (!dmode)
							XDrawLine(d1, w1, gc2, x1, y1 + 10, x1 + 10, y1 + 10);
						x1 += 10;
						if (!dmode)
							XDrawLine(d1, w1, gc2, x1, y1 + 10, x1, y1);
					}
					if (x1 < (int) w1_w) {
						if (dmode) {
							if (data & PULSE_BIT)
								XDrawLine(d1, w1, gc2, x1, y1, x1 + dx, y1);
							x1 += dx;
						} else {
							XDrawLine(d1, w1, gc2,
								x1,      ((data & PULSE_BIT) ? y1 : y1 + 10),
								x1 + dx, ((data & PULSE_BIT) ? y1 : y1 + 10));
							x1 += dx;
							XDrawLine(d1, w1, gc2,
								x1, ((data & PULSE_BIT) ? y1 : y1 + 10),
								x1, ((data & PULSE_BIT) ? y1 + 10 : y1));
						}
					}
				}
				if (y1 > (lirc_t) w1_h) {
					x1 = 0;
					drawGrid(div_);
				}
			}
			XFlush(d1);
		}
	}
	exit(EXIT_SUCCESS);
}
