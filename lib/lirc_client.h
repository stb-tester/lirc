/****************************************************************************
** lirc_client.h ***********************************************************
****************************************************************************
*
* Copyright (C) 1998 Trent Piepho <xyzzy@u.washington.edu>
* Copyright (C) 1998 Christoph Bartelmus <lirc@bartelmus.de>
*
*/

/**
 *  @file lirc_client.h
 *  @author Trent Piepho, Christoph Bartelmus
 *  @brief 3-rd party application interface.
 *  @ingroup  lirc_client
 */

/** @defgroup lirc_client   Client API
 *   @brief  Basic interface to 3-rd party applications.
 *
 *  The lirc_client interface is the basic interface for 3-rd party
 *  applications using lirc. It provides functions to retrieve , send
 *  and control button events to/from remotes.
 *
 *  Receiving events from remotes  could be done according to the following
 *  example, a stripped down version of the irexec(1) tool.
 *
 *         #include "lirc_client.h"
 *
 *         int main(int argc, char* argv[])
 *         {
 *             const char* lircrc_path;
 *             struct lirc_config* config;
 *             char* code;
 *             char* s;
 *
 *             // Check arguments... use argv[1] as lircrc config file path.
 *             lircrc_path = argc == 2 ? argv[1] : NULL;
 *
 *             if (lirc_init("mythtv", 1) == -1) {
 *                 // Process error and exit
 *             }
 *             if (lirc_readconfig(lircrc_path, &config, NULL) != 0) {
 *                 // Process error and exit.
 *             }
 *             while (lirc_nextcode(&code) == 0) {
 *                 if (code == NULL) continue;
 *                 while (lirc_code2char(config, code, &s) == 0 && s != NULL) {
 *                     // Do something with string s.
 *                 }
 *                 free(code);
 *             }
 *             lirc_freeconfig(config);
 *             lirc_deinit();
 *             exit(0);
 *         }
 *
 *  Some notes:
 *       - lirc_init() connects to the lircd daemon socket.
 *         The program given as argument is used when we later call
 *         lirc_readconfig() to determine what translation(s) to use. Thus it
 *         should match the `program = ...` items in the lircrc config file.
 *       - lirc_readconfig() parses the lircrc config file into &config. Using
 *         NULL as path means that the default lircrc file is used.
 *       - lirc_nextcode() reads the next button event from lircd. For the
 *         purposes here the returned code is just an opaque handle. It
 *         will sometimes return on a timeout without any data available, thus
 *         the `if (code == NULL) continue` statement.
 *       - lirc_code2char() translates the code handle to one ore more
 *         application specific string as defined in the licrrc file. Since
 *         more than one string can be returned  lirc_code2char() should be
 *         called until it returns a NULL string.
 *       - The complete source for this example is in @ref irexec.cpp
 *
 * Sending (blasting) is done according to following:
 *
 *     #include "lirc_client.h"
 *
 *     int main(int argc, char** argv)
 *     {
 *         int fd;
 *
 *         fd = lirc_get_local_socket(NULL, 0);
 *         if (fd < 0) {
 *             // Process error
 *         }
 *         if (lirc_send_one(fd, "name of remote", "Key symbol") == -1) {
 *             // Process errors
 *         };
 *     }
 *
 *  Notes:
 *    - Feeding NULL to lirc_get_local_socket() will make it use the default
 *      lircd socket. Doing so, it respects the LIRC_SOCKET_PATH environment
 *      variable.
 *    - "Name of remote" is the mandatory name attribute in the lircd.conf
 *      config file.
 *    - "Key symbol" is the name of a key definition in the lircd.conf file.
 *    - lirc_send_one() and lirc_simulate() are blocking. If  you need to do
 *      non-blocking IO and/or access other functionality available you need
 *      to use lirc_command_init() and lirc_command_run(). Example code is
 *      in @ref irsend.cpp.
 *
 *      @example irsend.cpp
 *      @example irexec.cpp
 * @{
 * @}
 */

#ifndef LIRC_CLIENT_H
#define LIRC_CLIENT_H

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "lirc/paths.h"
#include "lirc/lirc_config.h"
#include "lirc/curl_poll.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifndef __u32
typedef uint32_t __u32;
#endif

#define LIRC_RET_SUCCESS  (0)
#define LIRC_RET_ERROR   (-1)

#define LIRC_ALL ((char*)(-1))


enum lirc_flags { none = 0x00,
		  once = 0x01,
		  quit = 0x02,
		  mode = 0x04,
		  ecno = 0x08,
		  startup_mode = 0x10,
		  toggle_reset = 0x20, };

struct lirc_list {
	char*			string;
	struct lirc_list*	next;
};

struct lirc_code {
	char*			remote;
	char*			button;
	struct lirc_code*	next;
};

struct lirc_config {
	char*				lircrc_class; /**< The lircrc instance used, if any. */
	char*				current_mode;
	struct lirc_config_entry*	next;
	struct lirc_config_entry*	first;

	int				sockfd;
};

struct lirc_config_entry {
	char*				prog;
	struct lirc_code*		code;
	unsigned int			rep_delay;
	unsigned int			ign_first_events;
	unsigned int			rep;
	struct lirc_list*		config;
	char*				change_mode;
	unsigned int			flags;

	char*				mode;
	struct lirc_list*		next_config;
	struct lirc_code*		next_code;

	struct lirc_config_entry*	next;
};

/**
 * @addtogroup lirc_client
 * @{
 */

/** The data needed to run a command on remote server. */
typedef struct {
	char	packet[PACKET_SIZE + 1];        /**< The packet to send. */
	char	buffer[PACKET_SIZE + 1];        /**< Reply IO buffer. */
	char	reply[PACKET_SIZE + 1];         /**< Command reply payload. */
	int	head;                           /**< First free buffer index.*/
	int	reply_to_stdout;                /**< If true, write reply on stdout. */
	char*	next;                           /**< Next newline-separated word in buffer.*/
} lirc_cmd_ctx;

/**
 * Initial setup: connect to lircd socket.
 *
 * @param prog Name of client in logging contexts.
 * @param verbose Amount of debug info on stdout.
 * @return positive file descriptor or -1 + error in global errno.
 */
int lirc_init(const char* prog, int verbose);

/**
 * Release resources allocated by lirc_init(), basically disconnect
 * from socket.
 */
int lirc_deinit(void);

/**
 * Parse a lircrc configuration file. This function will also try to
 * connect to a lircrcd instance on the default socket which is
 * derived from path.
 *
 * @param path Path to lircrc config file. If  NULL the default
 *     file is used.
 * @param config Undefined omn enter, on successfull exit a pointer
 *     to an  allocated lirc_config instance.
 * @param check Callback function called with each configured
 *     application string as argument. Returns 0 if string is OK,
 *     else -1.
 * @return -1 on errors, else 0.
 */
int lirc_readconfig(const char* path,
		    struct lirc_config** config,
		    int (check) (char* s));

/** Deallocate an object retrieved using lirc_readconfig(). */
void lirc_freeconfig(struct lirc_config* config);

/** @deprecated  obsolete */
char* lirc_nextir(void);

/** @deprecated obsolete  */
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

/**
 * Translate a code string to an application string using .lircrc.
 * An translation might return more than one string so this function should
 * be called several times until *string == NULL.
 *
 * @param config Parsed lircrc data from e. g. lirc_readconfig().
 * @param code Code string e. g., as from lirc_nextcode().
 * @param string On successfull exit points to a static application
 *     string, NULL if no more translations are available.
 * @return -1 on errors, else 0.
 */
int lirc_code2char(struct lirc_config* config, char* code, char** string);


/* new interface for client daemon */
/**
 * Parse a lircrc configuration file without connecting to lircrcd.
 *
 * @param path Path to lircrc config file. If  NULL the default
 *     file is used.
 * @param config Undefined omn enter, on successfull  exit a pointer
 *     to an  allocated lirc_config instance.
 * @param check Callback function called with each configured
 *     application string as argument. Returns o if string is OK,
 *     else -1.
 * @return -1 on errors, else 0.
 */
int lirc_readconfig_only(const char* file,
			 struct lirc_config** config,
			 int (check) (char* s));

int lirc_code2charprog(struct lirc_config* config,
		       char* code,
		       char** string,
		       char** prog);

/**
 * Retrieve default lircrcd socket path.
 *
 * @param id Optional socket id, defaults (id == NULL) to "default".
 * @param buf Return buffer.
 * @param size Size of return buffer.
 * @return -1 on errors, else 0.
 */
size_t lirc_getsocketname(const char* id, char* buf, size_t size);

/**
 * Get mode  defined  in lircrc. Will use lircrcd if available, else
 * local data.
 *
 * @param config Parsed lircrc file as obtained from lirc_readconfig()
 *    or lirc_readconfig_only().
 * @return Current mode or NULL on errors.
 */
const char* lirc_getmode(struct lirc_config* config);

/**
 * Set mode  defined  in lircrc. Will use lircrcd if available, else
 * use local data.
 *
 * @param config Parsed lircrc file as obtained from lirc_readconfig()
 *    or lirc_readconfig_only().
 * @param mode: A new mode defined in lircrc.
 * @return New mode, should match mode unless there is errors.
 */
const char* lirc_setmode(struct lirc_config* config, const char* mode);

/* 0.9.2: New interface for sending data. */

/**
 * Initiate a lirc_cmd_ctx to run a command.
 *
 * @param ctx Undefined om input, ready to execute on exit.
 * @param fmt,... printf-style formatting for command. Don't forget
 *     trailing "\n"!
 * @return 0 on OK, else a kernel error code.
 * @note  Simple example: `lirc_command_init(&ctx, "CODE %s\n", code)`;
 * @since 0.9.2
 */
int lirc_command_init(lirc_cmd_ctx* ctx, const char* fmt, ...);


/**
 * Run a command in non-blocking mode.
 *
 * @param ctx Initiated data on enter, possibly reply payload in ctx->reply
 *     on exit.
 * @param fd Open file connected to a lircd output socket.
 * @return  0 on OK, else a kernel error code (possibly EAGAIN).
 * @since 0.9.2
 *
 */
int lirc_command_run(lirc_cmd_ctx* ctx, int fd);

/**
 * Set command_ctx write_to_stdout flag. When set, the reply payload is
 * written to stdout instead of the default behavior to store it in
 * ctx->reply.
 * @since 0.9.2
 */
void lirc_command_reply_to_stdout(lirc_cmd_ctx* ctx);

/**
 * Send keysym using given remote. This call might block for some time
 * since it involves communication with lircd.
 *
 * @param fd File descriptor for lircd socket. This must not be the
 *     descriptor returned by lirc_init(); open the socket using
 *     lirc_get_local_socket() or lirc_get_remote_socket()k instead.
 * @param remote Name of remote, the 'name' attribute in the config file.
 * @param keysym The code to send, as defined in the config file.
 * @return -1 on errors, else 0.
 * @since 0.9.2
 * */
int lirc_send_one(int fd, const char* remote, const char* keysym);


/**
 * Send a simulated lirc event.This call might block for some time
 * since it involves communication with lircd.
 *
 * @param fd File descriptor for lircd socket. This must not be the
 *     descriptor returned by lirc_init; open the socket using
 *     lirc_get_local_socket() or lirc_get_remote_socket() instead.
 * @param remote Name of remote, the 'name' attribute in the config file.
 * @param keysym The code to send, as defined in the config file.
 * @param scancode The code bound the keysym in the config file.
 * @param repeat Number indicating how many times this code has been
 *     repeated, starts at 0, increased for each repetition.
 * @return -1 on errors, else 0.
 * @since 0.9.2
 */
int lirc_simulate(int fd,
		  const char* remote,
		  const char* keysym,
		  int scancode,
		  int repeat);


/**
 * Return an opened and connected file descriptor to remote lirc socket.
 *
 * @param address Remote host to connect to.
 * @param port TCP port. If <= 0 uses hardcoded default LIRC_INET_PORT.
 * @param quiet If true, don't write error messages on stderr.
 * @return positive file descriptor  on success, else a negated kernel
 *     error code.
 * @since 0.9.2
 */
int lirc_get_remote_socket(const char* address, int port, int quiet);


/**
 * Return an opened and connected file descriptor to local lirc socket.
 *
 * @param path Path to socket. If NULL use LIRC_SOCKET_PATH in environment,
 * falling back to a hardcoded lircd default.
 * @param quiet If true, don't write error messages on stderr.
 * @return positive file descriptor  on success, else a negated kernel
 *     error code.
 * @since 0.9.2
 */
int lirc_get_local_socket(const char* path, int quiet);


/** @} */


#ifdef __cplusplus
}
#endif

#endif
