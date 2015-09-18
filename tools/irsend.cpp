/*

  irsend -  application for sending IR-codes via lirc

  Copyright (C) 1998 Christoph Bartelmus (lirc@bartelmus.de)

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <getopt.h>
#include <errno.h>
#include <limits.h>

#include "lirc_log.h"
#include "lirc_client.h"

static const logchannel_t logchannel = LOG_APP;

static const char* const help =
	"\nSynopsis:\n"
	"    irsend [options] SEND_ONCE remote code [code...]\n"
	"    irsend [options] SEND_START remote code\n"
	"    irsend [options] SEND_STOP remote code\n"
	"    irsend [options] LIST remote\n"
	"    irsend [options] SET_TRANSMITTERS remote num [num...]\n"
	"    irsend [options] SIMULATE \"scancode repeat keysym remote\"\n"
	"Options:\n"
	"    -h --help\t\t\tdisplay usage summary\n"
	"    -v --version\t\tdisplay version\n"
	"    -d --device=device\t\tuse given lircd socket [" LIRCD "]\n"
	"    -a --address=host[:port]\tconnect to lircd at this address\n"
	"    -# --count=n\t\tsend command n times\n";

const char* prog;

int send_packet(lirc_cmd_ctx* ctx, int fd)
{
	int r;

	do {
		r = lirc_command_run(ctx, fd);
		if (r != 0 && r != EAGAIN)
			fprintf(stderr,
				"Error running command: %s\n", strerror(r));
	} while (r == EAGAIN);
	return r == 0 ? 0 : -1;
}

void reformat_simarg(char* code, char buffer[])
{
	unsigned int scancode;
	unsigned int repeat;
	char keysym[32];
	char remote[64];
	char trash[32];
	int r;

	r = sscanf(code, "%x %x %32s %64s %32s",
		   &scancode, &repeat, keysym, remote, trash);
	if (r != 4) {
		fprintf(stderr, "Bad simulate argument: %s\n", code);
		exit(EXIT_FAILURE);
	}
	snprintf(buffer, PACKET_SIZE, "%016x %02x %s %s",
		 scancode, repeat, keysym, remote);
}





int main(int argc, char** argv)
{
	char* directive;
	char* remote;
	char* code;
	const char* lircd = NULL;
	char* address = NULL;
	unsigned short port = LIRC_INET_PORT;
	unsigned long count = 1;
	int fd;
	char buffer[PACKET_SIZE + 1];
	int r;
	lirc_cmd_ctx ctx;

	prog = "irsend";

	while (1) {
		int c;
		static struct option long_options[] = {
			{ "help",    no_argument,	NULL, 'h' },
			{ "version", no_argument,	NULL, 'v' },
			{ "device",  required_argument, NULL, 'd' },
			{ "address", required_argument, NULL, 'a' },
			{ "count",   required_argument, NULL, '#' },
			{ 0,	     0,			0,    0	  }
		};
		c = getopt_long(argc, argv, "hvd:a:#:", long_options, NULL);
		if (c == -1)
			break;
		switch (c) {
		case 'h':
			fputs(help, stdout);
			return EXIT_SUCCESS;
		case 'v':
			printf("%s %s\n", prog, VERSION);
			return EXIT_SUCCESS;
		case 'd':
			lircd = optarg;
			break;
		case 'a':
		{
			char* p;
			char* end;
			unsigned long val;

			address = strdup(optarg);
			if (!address) {
				fprintf(stderr, "%s: out of memory\n", prog);
				return EXIT_FAILURE;
			}
			p = strchr(address, ':');
			if (p != NULL) {
				val = strtoul(p + 1, &end, 10);
				if (!(*(p + 1)) || *end || val < 1 || val > USHRT_MAX) {
					fprintf(stderr, "%s: invalid port number: %s\n", prog, p + 1);
					return EXIT_FAILURE;
				}
				port = (unsigned short)val;
				*p = 0;
			}
			break;
		}
		case '#':
		{
			char* end;

			count = strtoul(optarg, &end, 10);
			if (!*optarg || *end) {
				fprintf(stderr, "%s: invalid count value: %s\n", prog, optarg);
				return EXIT_FAILURE;
			}
			break;
		}
		default:
			return EXIT_FAILURE;
		}
	}
	if (optind + 2 > argc) {
		fprintf(stderr, "%s: not enough arguments\n", prog);
		return EXIT_FAILURE;
	}
	if (lircd == NULL)
		lircd = LIRCD;
	if (address == NULL)
		fd = lirc_get_local_socket(lircd ? lircd : NULL, 0);
	else
		fd = lirc_get_remote_socket(address, port, 0);
	if (fd < 0) {
		lircd = lircd ? lircd : getenv("LIRC_SOCKET_PATH");
		lircd = lircd ? lircd : LIRCD;
		perrorf("Cannot open socket %s", lircd);
		exit(EXIT_FAILURE);
	}
	if (address)
		free(address);
	address = NULL;

	directive = argv[optind++];

	if (strcasecmp(directive, "set_transmitters") == 0) {
		code = argv[optind++];
		if (strlen(directive) + strlen(code) + 2 < PACKET_SIZE) {
			sprintf(buffer, "%s %s", directive, code);
		} else {
			fprintf(stderr, "%s: input too long\n", prog);
			exit(EXIT_FAILURE);
		}
		while (optind < argc) {
			code = argv[optind++];
			if (strlen(buffer) + strlen(code) + 2 < PACKET_SIZE) {
				sprintf(buffer + strlen(buffer), " %s", code);
			} else {
				fprintf(stderr, "%s: input too long\n", prog);
				exit(EXIT_FAILURE);
			}
		}
		strcat(buffer, "\n");
		lirc_command_init(&ctx, buffer);
		if (send_packet(&ctx, fd) == -1)
			exit(EXIT_FAILURE);
	}
	if (strcasecmp(directive, "simulate") == 0) {
		code = argv[optind++];
		if (optind != argc) {
			fprintf(stderr, "%s: invalid argument count\n", prog);
			exit(EXIT_FAILURE);
		}
		reformat_simarg(code, buffer);
		r = lirc_command_init(&ctx, "%s %s\n", directive, buffer);
		if (r != 0) {
			fprintf(stderr, "%s: %s\n", prog, strerror(r));
			exit(EXIT_FAILURE);
		}
		if (send_packet(&ctx, fd) == -1)
			exit(EXIT_FAILURE);
	} else {
		remote = argv[optind++];

		if (optind == argc) {
			fprintf(stderr, "%s: not enough arguments\n", prog);
			exit(EXIT_FAILURE);
		}
		while (optind < argc) {
			code = argv[optind++];
			if (strcasecmp(directive, "SEND_ONCE") == 0 && count > 1)
				r = lirc_command_init(&ctx, "%s %s %s %lu\n",
						      directive, remote, code, count);
			else
				r = lirc_command_init(&ctx, "%s %s %s\n",
						      directive, remote, code);
			if (r != 0) {
				fprintf(stderr, "%s: input too long\n", prog);
				exit(EXIT_FAILURE);
			}
			lirc_command_reply_to_stdout(&ctx);
			if (send_packet(&ctx, fd) == -1)
				exit(EXIT_FAILURE);
		}
	}
	close(fd);
	return EXIT_SUCCESS;
}
