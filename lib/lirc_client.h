
/****************************************************************************
 ** lirc_client.h ***********************************************************
 ****************************************************************************
 *
 * lirc_client - common routines for lircd clients
 *
 * Copyright (C) 1998 Trent Piepho <xyzzy@u.washington.edu>
 * Copyright (C) 1998 Christoph Bartelmus <lirc@bartelmus.de>
 *
 */

#ifndef LIRC_CLIENT_H
#define LIRC_CLIENT_H

#include <stddef.h>
#include <stdint.h>
#include <syslog.h>

#include "lirc/lirc_config.h"

#ifdef	__cplusplus
extern "C" {
#endif


#ifndef __u32
typedef uint32_t __u32;
#endif

#define LIRC_RET_SUCCESS  (0)
#define LIRC_RET_ERROR   (-1)

#define LIRC_ALL ((char* ) (-1))


	enum lirc_flags { none = 0x00,
		once = 0x01,
		quit = 0x02,
		mode = 0x04,
		ecno = 0x08,
		startup_mode = 0x10,
		toggle_reset = 0x20,
	};

	struct lirc_list {
		char* string;
		struct lirc_list* next;
	};

	struct lirc_code {
		char* remote;
		char* button;
		struct lirc_code* next;
	};

	struct lirc_config {
		char* current_mode;
		struct lirc_config_entry* next;
		struct lirc_config_entry* first;

		int sockfd;
	};

	struct lirc_config_entry {
		char* prog;
		struct lirc_code* code;
		unsigned int rep_delay;
		unsigned int rep;
		struct lirc_list* config;
		char* change_mode;
		unsigned int flags;

		char* mode;
		struct lirc_list* next_config;
		struct lirc_code* next_code;

		struct lirc_config_entry* next;
	};

	/** The data needed to run a command on remote server. */
	typedef struct {
		char packet[PACKET_SIZE + 1];   /**< The packet to send. */
		char buffer[PACKET_SIZE + 1];   /**< Reply IO buffer. */
		char reply[PACKET_SIZE + 1];    /**< Command reply payload. */
		int head;			/**< First free buffer index.*/
		int reply_to_stdout;    /**< If true, write reply on stdout. */
		char* next;	/**< Next newline-separated word in buffer.*/
	} lirc_cmd_ctx;


	/**
	* Initial setup: connect to lircd socket.
	* @param prog Name of client in logging contexts.
	* @param verbose: Amount of debug info on stdout.
	* @return positive file descriptor or -1 + error in global errno.
	*/
	int lirc_init(const char* prog, int verbose);

	/** Relinquish resources, basically disconnect from socket. */
	int lirc_deinit(void);


	int lirc_readconfig(const char* file,
			    struct lirc_config** config,
			    int (check) (char* s));

	void lirc_freeconfig(struct lirc_config* config);

/* obsolete */
	char* lirc_nextir(void);
/* obsolete */
	char* lirc_ir2char(struct lirc_config* config, char* code);
/**
 * Get next available code from the lircd daemon.
 *
 * @param code Undefined on enter. On exit either NULL if no complete
 *     code was available, else a pointer to a malloc()'d code string.
 *     Caller should eventually free() this.
 * @return -1 on errors, else 0 indicating either a complete code in
 *     *code or that nothing was available.
 */
int lirc_nextcode(char** code);

int lirc_code2char(struct lirc_config* config, char* code, char** string);

/* new interface for client daemon */
	int lirc_readconfig_only(const char* file,
				 struct lirc_config** config,
				 int (check) (char* s));

	int lirc_code2charprog(struct lirc_config* config,
			       char* code, char** string,
			       char** prog);

	size_t lirc_getsocketname(const char* filename,
 				  char* buf,
 				  size_t size);

	const char* lirc_getmode(struct lirc_config* config);

	const char* lirc_setmode(struct lirc_config* config,
				 const char* mode);

/* 0.9.2: New interface for sending data. */

/**
 * Initiate a lirc_cmd_ctx to run a command.
 * @param ctx Undefined om input, ready to execute on exit.
 * @param fmt,... printf-style formatting for command. Dont forget
 *     trailing "\n"!
 * @return 0 on OK, else a kernel error code.
 * @note  Simple example: lirc_command_init(&ctx, "CODE %s\n", code);
 */

int lirc_command_init(lirc_cmd_ctx* ctx, const char* fmt, ...);


/**
 * Run a command in non-blocking mode.
 *
 * @param ctx Initiated data on enter, possibly reply payload in ctx->reply
 *     on exit.
 * @param fd Open file connected to a lircd output socket.
 * @return  0 on OK, else a kernel error code (possibly EAGAIN).
 *
 */
int lirc_command_run(lirc_cmd_ctx* ctx, int fd);

/**
 * Set commnd_ctx write_to_stdout flag. When set, the replyu payload is
 * written to stdout instead of the default behavior to store it in
 * lirc_cmd_ctx.reply.
 */
void lirc_command_reply_to_stdout(lirc_cmd_ctx* ctx);

/**
 * Send keysym using given remote. This call might block for some time
 * since it involves communication with lircd.
 *
 * @param fd File descriptor for lircd socket. This must not be the
 *     descriptor returned by lirc_init; open the socket on a new
 *     fd instead.
 * @param remote Name of remote, the 'name' attribute in the config file.
 * @param keysym The code to send, as defined in the config file.
 * @return -1 on errors, else 0.
 * */
int lirc_send_one(int fd, const char* remote, const char* keysym);


/**
 * Send a simulated lirc event.This call might block for some time
 * since it involves communication with lircd.
 *
 * @param fd File descriptor for lircd socket. This must not be the
 *     descriptor returned by lirc_init; open the socket on a new
 *     fd instead.
 * @param remote Name of remote, the 'name' attribute in the config file.
 * @param keysym The code to send, as defined in the config file.
 * @param scancode The code bound the keysym in teh config file.
 * @param repeat Number indicating how many times this code has been
 *     repeated, starts at 0, increased for each repetition.
 * @return -1 on errors, else 0.
 */
int lirc_simulate(int fd,
		  const char* remote,
                  const char* keysym,
                  int scancode,
                  int repeat);


/**
 * Return an opened and connected file descriptor to remote lircd socket.
 *
 * @param address Remote host to connect to.
 * @param port TCP port. If <= 0 uses hardcoded default LIRC_INET_PORT.
 * @param quiet If true, don't write error messages on stderr.
 * @return positive file descriptor  on success, else a negated kernel
 *     error code.
 */
int lirc_get_remote_socket(const char* address, int port, int quiet);


/**
 * Return an opened and connected file descriptor to local lircd socket.
 *
 * @param path Path to lircd socket. If NULL use LIRC_SOCKET_PATH in
 *     environment, falling back to a hardcoded default.
 * @param quiet If true, don't write error messages on stderr.
 * @return positive file descriptor  on success, else a negated kernel
 *     error code.
 */
int lirc_get_local_socket(const char* path, int quiet);


/** @}
 */


#ifdef __cplusplus
}
#endif

#endif
