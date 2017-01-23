/****************************************************************************
** lirc_client.c ***********************************************************
****************************************************************************
*
* lirc_client - common routines for lircd clients
*
* Copyright (C) 1998 Trent Piepho <xyzzy@u.washington.edu>
* Copyright (C) 1998 Christoph Bartelmus <lirc@bartelmus.de>
*
* System wide LIRCRC support by Michal Svec <rebel@atrey.karlin.mff.cuni.cz>
*/

/**
 * @file lirc_client.c
 * @brief Implements lirc_client.h
 * @author  Christoph Bartelmus, Trent Piepho,  Michal Svec
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <unistd.h>

#include "lirc_client.h"

/** Timeout in lirc_read_string. */
static const struct timeval CMD_TIMEOUT = { .tv_sec = 1, .tv_usec = 0 };


// Until we have working client logging...
#define logprintf(level, fmt, args ...)  syslog(level, fmt, ## args)
#define LIRC_WARNING    LOG_WARNING
#define LIRC_DEBUG      LOG_DEBUG
#define LIRC_NOTICE     LOG_NOTICE
#define LIRC_ERROR      LOG_ERR

/* internal defines */
#define MAX_INCLUDES 10
#define LIRC_READ 255
#define LIRC_PACKET_SIZE 255
/* three seconds */
#define LIRC_TIMEOUT 3

/* internal data structures */
struct filestack_t {
	FILE*			file;
	char*			name;
	int			line;
	struct filestack_t*	parent;
};


/** protocol state. */
enum packet_state {
	P_BEGIN,
	P_MESSAGE,
	P_STATUS,
	P_DATA,
	P_N,
	P_DATA_N,
	P_END
};


/*
 * lircrc_config relies on this function, hence don't make it static
 * but it's not part of the official interface, so there's no guarantee
 * that it will stay available in the future
 */
unsigned int lirc_flags(char* string);

static int lirc_lircd;
static int lirc_verbose = 0;
static char* lirc_prog = NULL;
static char* lirc_buffer = NULL;

char* prog;

/** Wrapper for write(2) which logs errors. */
static inline void
chk_write(int fd, const void* buf, size_t count, const char* msg)
{
	if (write(fd, buf, count) == -1)
		perror(msg);
}


int lirc_command_init(lirc_cmd_ctx* ctx, const char* fmt, ...)
{
	va_list ap;
	int n;

	memset(ctx, 0, sizeof(lirc_cmd_ctx));
	va_start(ap, fmt);
	n = vsnprintf(ctx->packet, PACKET_SIZE, fmt, ap);
	va_end(ap);
	if (n >= PACKET_SIZE) {
		logprintf(LIRC_NOTICE, "Message too big: %s", ctx->packet);
		return EMSGSIZE;
	}
	return 0;
}


void lirc_command_reply_to_stdout(lirc_cmd_ctx* ctx)
{
	ctx->reply_to_stdout = 1;
}


/** Read new data into ctx, update ctx->head. */
static int fill_string(int fd, lirc_cmd_ctx* cmd)
{
	ssize_t n;

	setsockopt(fd,
		   SOL_SOCKET,
		   SO_RCVTIMEO,
		   (const void*)&CMD_TIMEOUT,
		   sizeof(CMD_TIMEOUT));
	n = read(fd, cmd->buffer + cmd->head, PACKET_SIZE - cmd->head);
	if (n == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
			logprintf(LIRC_NOTICE, "fill_string: timeout\n");
			return EAGAIN;
		}
		cmd->head = 0;
		return errno;
	}
	cmd->head += n;
	return 0;
}


/** Get next string in *string, returns 0 or kernel error e. g., EAGAIN. */
static int read_string(lirc_cmd_ctx* cmd, int fd, const char** string)
{
	int r;
	int skip;

	/* Move remaining data to start of buffer, overwriting previous line. */
	if (cmd->next != NULL && cmd->next != cmd->buffer) {
		skip = cmd->next - cmd->buffer;
		memmove(cmd->buffer, cmd->next, cmd->head - skip);
		cmd->head -= skip;
		cmd->next = cmd->buffer;
		cmd->buffer[cmd->head] = '\0';
	}
	/* If no complete line is available, load more bytes from fd. */
	if (cmd->next == NULL || strchr(cmd->next, '\n') == NULL) {
		r = fill_string(fd, cmd);
		if (r > 0)
			return r;
		cmd->next = cmd->buffer;
	}
	/* cmd->next == cmd->buffer here in all cases. */
	*string = cmd->next;
	/* Separate current line from the remaining lines, if available. */
	cmd->next = strchr(cmd->next, '\n');
	if (cmd->next != NULL) {
		*(cmd->next) = '\0';
		cmd->next++;
	}
	return 0;
}


int lirc_command_run(lirc_cmd_ctx* ctx, int fd)
{
	int done, todo;
	const char* string = NULL;
	const char* data;
	char* endptr;
	enum packet_state state;
	int status, n, r;
	__u32 data_n = 0;

	todo = strlen(ctx->packet);
	data = ctx->packet;
	logprintf(LIRC_DEBUG, "lirc_command_run: Sending: %s", data);
	while (todo > 0) {
		done = write(fd, (void*)data, todo);
		if (done < 0) {
			logprintf(LIRC_WARNING,
				  "%s: could not send packet\n", prog);
			perror(prog);
			return done;
		}
		data += done;
		todo -= done;
	}

	/* get response */
	status = 0;
	n = 0;
	state = P_BEGIN;
	while (1) {
		do
			r = read_string(ctx, fd, &string);
		while (r == EAGAIN);
		if (!string || strlen(string) == 0)
			goto bad_packet;
		logprintf(LIRC_DEBUG,
			  "lirc_command_run, state: %d, input: \"%s\"\n",
			  state, string ? string : "(Null)");
		switch (state) {
		case P_BEGIN:
			if (strcasecmp(string, "BEGIN") != 0)
				break;
			state = P_MESSAGE;
			continue;
		case P_MESSAGE:
			if (strncasecmp(string, ctx->packet,
					strlen(string)) != 0
			    || strcspn(string, "\n")
					!= strcspn(ctx->packet, "\n")) {
				state = P_BEGIN;
				break;
			}
			state = P_STATUS;
			continue;
		case P_STATUS:
			if (strcasecmp(string, "SUCCESS") == 0) {
				status = 0;
			} else if (strcasecmp(string, "END") == 0) {
				logprintf(LIRC_NOTICE,
					  "lirc_command_run: status:END");
				return 0;
			} else if (strcasecmp(string, "ERROR") == 0) {
				logprintf(LIRC_WARNING,
					  "%s: command failed: %s",
					  prog, ctx->packet);
				status = EIO;
			} else {
				goto bad_packet;
			}
			state = P_DATA;
			break;
		case P_DATA:
			if (strcasecmp(string, "END") == 0) {
				logprintf(LIRC_NOTICE,
					  "lirc_command_run: data:END, status:%d",
					  status);
				return status;
			} else if (strcasecmp(string, "DATA") == 0) {
				state = P_N;
				break;
			}
			logprintf(LIRC_DEBUG,
				  "data: bad packet: %s\n",
				  string);
			goto bad_packet;
		case P_N:
			errno = 0;
			data_n = (__u32)strtoul(string, &endptr, 0);
			if (!*string || *endptr)
				goto bad_packet;
			if (data_n == 0)
				state = P_END;
			else
				state = P_DATA_N;
			break;
		case P_DATA_N:
			if (n == 0) {
				if (ctx->reply_to_stdout)
					puts("");
				else
					strcpy(ctx->reply, "");
			}
			if (ctx->reply_to_stdout) {
				chk_write(STDOUT_FILENO, string, strlen(string),
					  "reply (1)");
				chk_write(STDOUT_FILENO, "\n", 1, "reply (2)");
			} else {
				strncpy(ctx->reply,
					string,
					PACKET_SIZE - strlen(ctx->reply));
			}
			n++;
			if (n == data_n)
				state = P_END;
			break;
		case P_END:
			if (strcasecmp(string, "END") == 0) {
				logprintf(LIRC_NOTICE,
					  "lirc_command_run: status:END, status:%d",
					  status);
				return status;
			}
			goto bad_packet;
		}
	}
bad_packet:
	logprintf(LIRC_WARNING, "%s: bad return packet\n", prog);
	logprintf(LIRC_DEBUG, "State %d: bad packet: %s\n", status, string);
	return EPROTO;
}


static void lirc_printf(const char* format_str, ...)
{
	va_list ap;

	if (!lirc_verbose)
		return;

	va_start(ap, format_str);
	vfprintf(stderr, format_str, ap);
	va_end(ap);
}


static void lirc_perror(const char* s)
{
	if (!lirc_verbose)
		return;

	perror(s);
}


int lirc_init(const char* prog, int verbose)
{
	if (prog == NULL || lirc_prog != NULL)
		return -1;
	lirc_lircd = lirc_get_local_socket(NULL, !verbose);
	if (lirc_lircd >= 0) {
		lirc_verbose = verbose;
		lirc_prog = strdup(prog);
		if (lirc_prog == NULL) {
			lirc_printf("%s: out of memory\n", prog);
			return -1;
		}
		return lirc_lircd;
	}
	lirc_printf("%s: could not open socket: %s\n",
		    lirc_prog,
		    strerror(-lirc_lircd));
	return -1;
}


int lirc_deinit(void)
{
	if (lirc_prog != NULL) {
		free(lirc_prog);
		lirc_prog = NULL;
	}
	if (lirc_buffer != NULL) {
		free(lirc_buffer);
		lirc_buffer = NULL;
	}
	return close(lirc_lircd);
}


static int lirc_readline(char** line, FILE* f)
{
	char* newline;
	char* ret;
	char* enlargeline;
	int len;

	newline = (char*)malloc(LIRC_READ + 1);
	if (newline == NULL) {
		lirc_printf("%s: out of memory\n", lirc_prog);
		return -1;
	}
	len = 0;
	while (1) {
		ret = fgets(newline + len, LIRC_READ + 1, f);
		if (ret == NULL) {
			if (feof(f) && len > 0) {
				*line = newline;
			} else {
				free(newline);
				*line = NULL;
			}
			return 0;
		}
		len = strlen(newline);
		if (newline[len - 1] == '\n') {
			newline[len - 1] = 0;
			*line = newline;
			return 0;
		}

		enlargeline = (char*)realloc(newline, len + 1 + LIRC_READ);
		if (enlargeline == NULL) {
			free(newline);
			lirc_printf("%s: out of memory\n", lirc_prog);
			return -1;
		}
		newline = enlargeline;
	}
}


static char* lirc_trim(char* s)
{
	int len;

	while (s[0] == ' ' || s[0] == '\t')
		s++;
	len = strlen(s);
	while (len > 0) {
		len--;
		if (s[len] == ' ' || s[len] == '\t')
			s[len] = 0;
		else
			break;
	}
	return s;
}


/* parse standard C escape sequences + \@,\A-\Z is ^@,^A-^Z */
static char lirc_parse_escape(char** s, const char* name, int line)
{
	char c;
	unsigned int i, overflow, count;
	int digits_found, digit;

	c = **s;
	(*s)++;
	switch (c) {
	case 'a':
		return '\a';
	case 'b':
		return '\b';
	case 'e':
#if 0
	case 'E':               /* this should become ^E */
#endif
		return 033;
	case 'f':
		return '\f';
	case 'n':
		return '\n';
	case 'r':
		return '\r';
	case 't':
		return '\t';
	case 'v':
		return '\v';
	case '\n':
		return 0;
	case 0:
		(*s)--;
		return 0;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
		i = c - '0';
		count = 0;

		while (++count < 3) {
			c = *(*s)++;
			if (c >= '0' && c <= '7') {
				i = (i << 3) + c - '0';
			} else {
				(*s)--;
				break;
			}
		}
		if (i > (1 << CHAR_BIT) - 1) {
			i &= (1 << CHAR_BIT) - 1;
			lirc_printf(
				"%s: octal escape sequence out of range in %s:%d\n",
				lirc_prog, name, line);
		}
		return (char)i;
	case 'x':
	{
		i = 0;
		overflow = 0;
		digits_found = 0;
		for (;; ) {
			c = *(*s)++;
			if (c >= '0' && c <= '9') {
				digit = c - '0';
			} else if (c >= 'a' && c <= 'f') {
				digit = c - 'a' + 10;
			} else if (c >= 'A' && c <= 'F') {
				digit = c - 'A' + 10;
			} else {
				(*s)--;
				break;
			}
			overflow |= i ^ (i << 4 >> 4);
			i = (i << 4) + digit;
			digits_found = 1;
		}
		if (!digits_found)
			lirc_printf("%s: \\x used with no "
				    "following hex digits in %s:%d\n",
				    lirc_prog, name, line);
		if (overflow || i > (1 << CHAR_BIT) - 1) {
			i &= (1 << CHAR_BIT) - 1;
			lirc_printf("%s: hex escape sequence out "
				    "of range in %s:%d\n", lirc_prog, name,
				    line);
		}
		return (char)i;
	}
	default:
		if (c >= '@' && c <= 'Z')
			return c - '@';
		return c;
	}
}


static void lirc_parse_string(char* s, const char* name, int line)
{
	char* t;

	t = s;
	while (*s != 0) {
		if (*s == '\\') {
			s++;
			*t = lirc_parse_escape(&s, name, line);
			t++;
		} else {
			*t = *s;
			s++;
			t++;
		}
	}
	*t = 0;
}


static void lirc_parse_include(char* s, const char* name, int line)
{
	char last;
	size_t len;

	len = strlen(s);
	if (len < 2)
		return;
	last = s[len - 1];
	if (*s != '"' && *s != '<')
		return;
	if (*s == '"' && last != '"')
		return;
	else if (*s == '<' && last != '>')
		return;
	s[len - 1] = 0;
	memmove(s, s + 1, len - 2 + 1); /* terminating 0 is copied */
}


int lirc_mode(char* token, char* token2, char** mode,
	      struct lirc_config_entry** new_config,
	      struct lirc_config_entry** first_config,
	      struct lirc_config_entry** last_config,
	      int (check) (char* s),
	      const char* name,
	      int line)
{
	struct lirc_config_entry* new_entry;

	new_entry = *new_config;
	if (strcasecmp(token, "begin") == 0) {
		if (token2 == NULL) {
			if (new_entry == NULL) {
				new_entry = (struct lirc_config_entry*)
					    malloc(sizeof(struct lirc_config_entry));
				if (new_entry == NULL) {
					lirc_printf("%s: out of memory\n",
						    lirc_prog);
					return -1;
				}
				new_entry->prog = NULL;
				new_entry->code = NULL;
				new_entry->rep_delay = 0;
				new_entry->ign_first_events = 0;
				new_entry->rep = 0;
				new_entry->config = NULL;
				new_entry->change_mode = NULL;
				new_entry->flags = none;
				new_entry->mode = NULL;
				new_entry->next_config = NULL;
				new_entry->next_code = NULL;
				new_entry->next = NULL;
				*new_config = new_entry;
			} else {
				lirc_printf("%s: bad file format, %s:%d\n",
					    lirc_prog, name, line);
				return -1;
			}
		} else {
			if (new_entry == NULL && *mode == NULL) {
				*mode = strdup(token2);
				if (*mode == NULL)
					return -1;
			} else {
				lirc_printf("%s: bad file format, %s:%d\n",
					    lirc_prog, name, line);
				return -1;
			}
		}
	} else if (strcasecmp(token, "end") == 0) {
		if (token2 == NULL) {
			if (new_entry != NULL) {
#if 0
				if (new_entry->prog == NULL) {
					lirc_printf(
						"%s: prog missing in config before line %d\n", lirc_prog,
						line);
					lirc_freeconfigentries(new_entry);
					*new_config = NULL;
					return -1;
				}
				if (strcasecmp(new_entry->prog,
					       lirc_prog) != 0) {
					lirc_freeconfigentries(new_entry);
					*new_config = NULL;
					return 0;
				}
#endif
				new_entry->next_code = new_entry->code;
				new_entry->next_config = new_entry->config;
				if (*last_config == NULL) {
					*first_config = new_entry;
					*last_config = new_entry;
				} else {
					(*last_config)->next = new_entry;
					*last_config = new_entry;
				}
				*new_config = NULL;

				if (*mode != NULL) {
					new_entry->mode = strdup(*mode);
					if (new_entry->mode == NULL) {
						lirc_printf(
							"%s: out of memory\n",
							lirc_prog);
						return -1;
					}
				}

				if (check != NULL &&
				    new_entry->prog != NULL &&
				    strcasecmp(new_entry->prog,
					       lirc_prog) == 0) {
					struct lirc_list* list;

					list = new_entry->config;
					while (list != NULL) {
						if (check(list->string) == -1)
							return -1;
						list = list->next;
					}
				}

				if (new_entry->rep_delay == 0 &&
				    new_entry->rep > 0)
					new_entry->rep_delay = new_entry->rep -
							       1;
			} else {
				lirc_printf(
					"%s: %s:%d: 'end' without 'begin'\n",
					lirc_prog, name, line);
				return -1;
			}
		} else {
			if (*mode != NULL) {
				if (new_entry != NULL) {
					lirc_printf(
						"%s: %s:%d: missing 'end' token\n",
						lirc_prog, name, line);
					return -1;
				}
				if (strcasecmp(*mode, token2) == 0) {
					free(*mode);
					*mode = NULL;
				} else {
					lirc_printf("%s: \"%s\" doesn't "
						    "match mode \"%s\"\n",
						    lirc_prog, token2, *mode);
					return -1;
				}
			} else {
				lirc_printf(
					"%s: %s:%d: 'end %s' without 'begin'\n",
					lirc_prog, name, line, token2);
				return -1;
			}
		}
	} else {
		lirc_printf("%s: unknown token \"%s\" in %s:%d ignored\n",
			    lirc_prog, token, name, line);
	}
	return 0;
}


unsigned int lirc_flags(char* string)
{
	char* s;
	unsigned int flags;

	flags = none;
	s = strtok(string, " \t|");
	while (s) {
		if (strcasecmp(s, "once") == 0)
			flags |= once;
		else if (strcasecmp(s, "quit") == 0)
			flags |= quit;
		else if (strcasecmp(s, "mode") == 0)
			flags |= mode;
		else if (strcasecmp(s, "startup_mode") == 0)
			flags |= startup_mode;
		else if (strcasecmp(s, "toggle_reset") == 0)
			flags |= toggle_reset;
		else
			lirc_printf("%s: unknown flag \"%s\"\n", lirc_prog, s);
		s = strtok(NULL, " \t");
	}
	return flags;
}






/**
 *  Retrieve the $HOME path in a malloc'ed *  MAXPATHLEN long buffer.
 *  Returns NULL on malloc() failures, "/" as fallback if $HOME is empty.
 *  Otherwise the returned path has no trailing "/".
 */
static char* get_homepath(void)
{
	char* home;
	char* filename;

	filename = malloc(MAXPATHLEN);
	if (filename == NULL) {
		lirc_printf("%s: out of memory\n", lirc_prog);
		return NULL;
	}
	home = getenv("HOME");
	home = home == NULL ? "/" : home;
	strncpy(filename, home, MAXPATHLEN);
	if (filename[strlen(filename) - 1] == '/')
		filename[strlen(filename) - 1] = '\0';
	return filename;
}


/**
 *  Retrieve the freedesktop configuration file path in a malloc'ed
 *  MAXPATHLEN long buffer. Returns NULL on malloc() failure and ""
 *  if the file does not exist.
 */
static char* get_freedesktop_path(void)
{
	char* path;

	if (getenv("XDG_CONFIG_HOME") != NULL) {
		path = malloc(MAXPATHLEN);
		strncpy(path, getenv("XDG_CONFIG_HOME"), MAXPATHLEN);
		strncat(path, "/", MAXPATHLEN - strlen(path));
		strncat(path, CFG_LIRCRC, MAXPATHLEN - strlen(path));
	} else {
		path = get_homepath();
		if (path == NULL)
			return NULL;
		strncat(path, "/.config/lircrc", MAXPATHLEN - strlen(path) - 1);
	}
	if (access(path, R_OK) != 0)
		path[0] = '\0';
	return path;
}


static char* lirc_getfilename(const char* file, const char* current_file)
{
	char* filename;

	if (file == NULL) {
		filename = get_freedesktop_path();
		if (filename == NULL) {
			return NULL;
		} else if (strlen(filename) == 0) {
			free(filename);
			filename = get_homepath();
			if (filename == NULL)
				return NULL;
			strcat(filename, "/" LIRCRC_USER_FILE);
		}
		filename = realloc(filename, strlen(filename) + 1);
	} else if (strncmp(file, "~/", 2) == 0) {
		filename = get_homepath();
		if (filename == NULL)
			return NULL;
		strcat(filename, file + 1);
		filename = realloc(filename, strlen(filename) + 1);
	} else if (file[0] == '/' || current_file == NULL) {
		/* absolute path or root */
		filename = strdup(file);
		if (filename == NULL) {
			lirc_printf("%s: out of memory\n", lirc_prog);
			return NULL;
		}
	} else {
		/* get path from parent filename */
		int pathlen = strlen(current_file);

		while (pathlen > 0 && current_file[pathlen - 1] != '/')
			pathlen--;
		filename = (char*)malloc(pathlen + strlen(file) + 1);
		if (filename == NULL) {
			lirc_printf("%s: out of memory\n", lirc_prog);
			return NULL;
		}
		memcpy(filename, current_file, pathlen);
		filename[pathlen] = 0;
		strcat(filename, file);
	}
	return filename;
}


static FILE* lirc_open(const char*	file,
		       const char*	current_file,
		       char**		full_name)
{
	FILE* fin;
	char* filename;

	filename = lirc_getfilename(file, current_file);
	if (filename == NULL)
		return NULL;

	fin = fopen(filename, "r");
	if (fin == NULL && (file != NULL || errno != ENOENT)) {
		lirc_printf("%s: could not open config file %s\n", lirc_prog,
			    filename);
		lirc_perror(lirc_prog);
	} else if (fin == NULL) {
		const char* root_file = LIRCRC_ROOT_FILE;

		fin = fopen(root_file, "r");
		if (fin == NULL && errno == ENOENT) {
			int save_errno = errno;

			root_file = LIRCRC_OLD_ROOT_FILE;
			fin = fopen(root_file, "r");
			errno = save_errno;
		}
		if (fin == NULL && errno != ENOENT) {
			lirc_printf("%s: could not open config file %s\n",
				    lirc_prog, LIRCRC_ROOT_FILE);
			lirc_perror(lirc_prog);
		} else if (fin == NULL) {
			lirc_printf("%s: could not open config files "
				    "%s and %s\n", lirc_prog, filename,
				    LIRCRC_ROOT_FILE);
			lirc_perror(lirc_prog);
		} else {
			free(filename);
			filename = strdup(root_file);
			if (filename == NULL) {
				fclose(fin);
				lirc_printf("%s: out of memory\n", lirc_prog);
				return NULL;
			}
		}
	}
	if (full_name && fin != NULL)
		*full_name = filename;
	else
		free(filename);
	return fin;
}


static struct filestack_t* stack_push(struct filestack_t* parent)
{
	struct filestack_t* entry;

	entry = malloc(sizeof(struct filestack_t));
	if (entry == NULL) {
		lirc_printf("%s: out of memory\n", lirc_prog);
		return NULL;
	}
	entry->file = NULL;
	entry->name = NULL;
	entry->line = 0;
	entry->parent = parent;
	return entry;
}


static struct filestack_t* stack_pop(struct filestack_t* entry)
{
	struct filestack_t* parent = NULL;

	if (entry) {
		parent = entry->parent;
		if (entry->name)
			free(entry->name);
		free(entry);
	}
	return parent;
}


static void stack_free(struct filestack_t* entry)
{
	while (entry)
		entry = stack_pop(entry);
}


static char* lirc_startupmode(struct lirc_config_entry* first)
{
	struct lirc_config_entry* scan;
	char* startupmode;

	startupmode = NULL;
	scan = first;
	/* Set a startup mode based on flags=startup_mode */
	while (scan != NULL) {
		if (scan->flags & startup_mode) {
			if (scan->change_mode != NULL) {
				startupmode = scan->change_mode;
				/* Remove the startup mode or it confuses lirc mode system */
				scan->change_mode = NULL;
				break;
			}
			lirc_printf("%s: startup_mode flags requires 'mode ='\n", lirc_prog);
		}
		scan = scan->next;
	}

	/* Set a default mode if we find a mode = client app name */
	if (startupmode == NULL) {
		scan = first;
		while (scan != NULL) {
			if (scan->mode != NULL
			    && strcasecmp(lirc_prog, scan->mode) == 0) {
				startupmode = lirc_prog;
				break;
			}
			scan = scan->next;
		}
	}

	if (startupmode == NULL)
		return NULL;
	scan = first;
	while (scan != NULL) {
		if (scan->change_mode != NULL
		    && scan->flags & once
		    && strcasecmp(startupmode, scan->change_mode) == 0)
			scan->flags |= ecno;
		scan = scan->next;
	}
	return startupmode;
}


static void lirc_freeconfigentries(struct lirc_config_entry* first)
{
	struct lirc_config_entry* c;
	struct lirc_config_entry* config_temp;
	struct lirc_list* list;
	struct lirc_list* list_temp;
	struct lirc_code* code;
	struct lirc_code* code_temp;

	c = first;
	while (c != NULL) {
		if (c->prog)
			free(c->prog);
		if (c->change_mode)
			free(c->change_mode);
		if (c->mode)
			free(c->mode);

		code = c->code;
		while (code != NULL) {
			if (code->remote != NULL && code->remote != LIRC_ALL)
				free(code->remote);
			if (code->button != NULL && code->button != LIRC_ALL)
				free(code->button);
			code_temp = code->next;
			free(code);
			code = code_temp;
		}

		list = c->config;
		while (list != NULL) {
			if (list->string)
				free(list->string);
			list_temp = list->next;
			free(list);
			list = list_temp;
		}
		config_temp = c->next;
		free(c);
		c = config_temp;
	}
}


static void
parse_shebang(char* line, int depth, const char* path, char* buff, size_t size)
{
	char* token;
	char my_path[128];
	const char* const SHEBANG_MSG =
		"Warning: Use of deprecated lircrc shebang."
		" Use lircrc_class instead.\n";

	token = strtok(line, "#! ");
	buff[0] = '\0';
	if (depth > 1) {
		lirc_printf("Warning: ignoring shebang in included file.");
		return;
	}
	if (strcmp(token, "lircrc") == 0) {
		strncpy(my_path, path, sizeof(my_path) - 1);
		strncat(buff, basename(my_path), size - 1);
		lirc_printf(SHEBANG_MSG);
	} else {
		lirc_printf("Warning: bad shebang (ignored)");
	}
}


static int lirc_readconfig_only_internal(const char*		file,
					 struct lirc_config**	config,
					 int			(check)(char* s),
					 char**			full_name)
{
	const char* const INCLUDED_LIRCRC_CLASS =
		"Warning: lirc_class in included file (ignored)";
	char* string;
	char* eq;
	char* token;
	char* token2;
	char* token3;
	struct filestack_t* filestack;
	struct filestack_t* stack_tmp;
	int open_files;
	char lircrc_class[128] = { '\0' };
	struct lirc_config_entry* new_entry;
	struct lirc_config_entry* first;
	struct lirc_config_entry* last;
	char* mode;
	char* remote;
	int ret = 0;
	int firstline = 1;
	char* save_full_name = NULL;

	filestack = stack_push(NULL);
	if (filestack == NULL)
		return -1;
	filestack->file = lirc_open(file, NULL, &(filestack->name));
	if (filestack->file == NULL) {
		stack_free(filestack);
		return -1;
	}
	filestack->line = 0;
	open_files = 1;

	first = new_entry = last = NULL;
	mode = NULL;
	remote = LIRC_ALL;
	while (filestack) {
		ret = lirc_readline(&string, filestack->file);
		if (ret == -1 || string == NULL) {
			fclose(filestack->file);
			if (open_files == 1 && full_name != NULL) {
				save_full_name = filestack->name;
				filestack->name = NULL;
			}
			filestack = stack_pop(filestack);
			open_files--;
			continue;
		}
		/* check for sha-bang */
		if (firstline) {
			firstline = 0;
			if (strncmp(string, "#!", 2) == 0) {
				parse_shebang(string,
					      open_files,
					      file,
					      lircrc_class,
					      sizeof(lircrc_class));
			}
		}
		filestack->line++;
		eq = strchr(string, '=');
		if (eq == NULL) {
			token = strtok(string, " \t");
			if (token == NULL) {
				/* ignore empty line */
			} else if (token[0] == '#') {
				/* ignore comment */
			} else if (strcasecmp(token, "lircrc_class") == 0) {
				token2 = lirc_trim(strtok(NULL, ""));
				if (strlen(token2) == 0) {
					lirc_printf(
						"Warning: no lircrc_class");
				} else if (open_files == 1) {
					strncpy(lircrc_class,
						token2,
						sizeof(lircrc_class) - 1);
				} else {
					lirc_printf(INCLUDED_LIRCRC_CLASS);
				}
			} else if (strcasecmp(token, "include") == 0) {
				if (open_files >= MAX_INCLUDES) {
					lirc_printf("%s: too many files "
						    "included at %s:%d\n",
						    lirc_prog, filestack->name,
						    filestack->line);
					ret = -1;
				} else {
					token2 = strtok(NULL, "");
					token2 = lirc_trim(token2);
					lirc_parse_include(token2,
							   filestack->name,
							   filestack->line);
					stack_tmp = stack_push(filestack);
					if (stack_tmp == NULL) {
						ret = -1;
					} else {
						stack_tmp->file =
							lirc_open(token2,
								  filestack->name,
								  &(stack_tmp->
								    name));
						stack_tmp->line = 0;
						if (stack_tmp->file) {
							open_files++;
							filestack = stack_tmp;
						} else {
							stack_pop(stack_tmp);
							ret = -1;
						}
					}
				}
			} else {
				token2 = strtok(NULL, " \t");
				if (token2)
					token3 = strtok(NULL, " \t");
				if (token2 != NULL && token3 != NULL) {
					lirc_printf("%s: unexpected token in line %s:%d\n",
						    lirc_prog, filestack->name, filestack->line);
				} else {
					ret = lirc_mode(token, token2, &mode,
							&new_entry, &first,
							&last,
							check, filestack->name,
							filestack->line);
					if (ret == 0) {
						if (remote != LIRC_ALL)
							free(remote);
						remote = LIRC_ALL;
					} else {
						if (mode != NULL) {
							free(mode);
							mode = NULL;
						}
						if (new_entry != NULL) {
							lirc_freeconfigentries(
								new_entry);
							new_entry = NULL;
						}
					}
				}
			}
		} else {
			eq[0] = 0;
			token = lirc_trim(string);
			token2 = lirc_trim(eq + 1);
			if (token[0] == '#') {
				/* ignore comment */
			} else if (new_entry == NULL) {
				lirc_printf("%s: bad file format, %s:%d\n",
					    lirc_prog, filestack->name,
					    filestack->line);
				ret = -1;
			} else {
				token2 = strdup(token2);
				if (token2 == NULL) {
					lirc_printf("%s: out of memory\n",
						    lirc_prog);
					ret = -1;
				} else if (strcasecmp(token, "prog") == 0) {
					if (new_entry->prog != NULL)
						free(new_entry->prog);
					new_entry->prog = token2;
				} else if (strcasecmp(token, "remote") == 0) {
					if (remote != LIRC_ALL)
						free(remote);

					if (strcasecmp("*", token2) == 0) {
						remote = LIRC_ALL;
						free(token2);
					} else {
						remote = token2;
					}
				} else if (strcasecmp(token, "button") == 0) {
					struct lirc_code* code;

					code = (struct lirc_code*)
					       malloc(sizeof(struct lirc_code));
					if (code == NULL) {
						free(token2);
						lirc_printf(
							"%s: out of memory\n",
							lirc_prog);
						ret = -1;
					} else {
						code->remote = remote;
						if (strcasecmp("*",
							       token2) == 0) {
							code->button = LIRC_ALL;
							free(token2);
						} else {
							code->button = token2;
						}
						code->next = NULL;

						if (new_entry->code == NULL)
							new_entry->code = code;
						else
							new_entry->next_code->
							next = code;
						new_entry->next_code = code;
						if (remote != LIRC_ALL) {
							remote = strdup(remote);
							if (remote == NULL) {
								lirc_printf(
									"%s: out of memory\n",
									lirc_prog);
								ret = -1;
							}
						}
					}
				} else if (strcasecmp(token, "delay") == 0) {
					char* end;

					errno = ERANGE + 1;
					new_entry->rep_delay = strtoul(token2,
								       &end, 0);
					if ((new_entry->rep_delay ==
					     ULONG_MAX && errno == ERANGE)
					    || end[0] != 0 || strlen(token2) ==
					    0)
						lirc_printf("%s: \"%s\" not"
							    " a  valid number for delay\n", lirc_prog,
							    token2);
					free(token2);
				} else if (strcasecmp(token, "ignore_first_events") == 0) {
					char* end;

					errno = ERANGE + 1;
					new_entry->ign_first_events = strtoul(
						token2, &end, 0);
					if ((new_entry->ign_first_events ==
					     ULONG_MAX && errno == ERANGE)
					    || end[0] != 0 || strlen(token2) ==
					    0)
						lirc_printf("%s: \"%s\" not"
							    " a  valid number for ignore_first_events\n",
							    lirc_prog, token2);
					free(token2);
				} else if (strcasecmp(token, "repeat") == 0) {
					char* end;

					errno = ERANGE + 1;
					new_entry->rep =
						strtoul(token2, &end, 0);
					if ((new_entry->rep == ULONG_MAX &&
					     errno == ERANGE)
					    || end[0] != 0 || strlen(token2) ==
					    0)
						lirc_printf("%s: \"%s\" not"
							    " a  valid number for repeat\n", lirc_prog,
							    token2);
					free(token2);
				} else if (strcasecmp(token, "config") == 0) {
					struct lirc_list* new_list;

					new_list = (struct lirc_list*)
						   malloc(sizeof(struct lirc_list));
					if (new_list == NULL) {
						free(token2);
						lirc_printf(
							"%s: out of memory\n",
							lirc_prog);
						ret = -1;
					} else {
						lirc_parse_string(token2,
								  filestack->name,
								  filestack->line);
						new_list->string = token2;
						new_list->next = NULL;
						if (new_entry->config == NULL)
							new_entry->config =
								new_list;
						else
							new_entry->next_config->
							next = new_list;
						new_entry->next_config =
							new_list;
					}
				} else if (strcasecmp(token, "mode") == 0) {
					if (new_entry->change_mode != NULL)
						free(new_entry->change_mode);
					new_entry->change_mode = token2;
				} else if (strcasecmp(token, "flags") == 0) {
					new_entry->flags = lirc_flags(token2);
					free(token2);
				} else {
					free(token2);
					lirc_printf(
						"%s: unknown token \"%s\" in %s:%d ignored\n",
						lirc_prog, token, filestack->name,
						filestack->line);
				}
			}
		}
		free(string);
		if (ret == -1)
			break;
	}
	if (remote != LIRC_ALL)
		free(remote);
	if (new_entry != NULL) {
		if (ret == 0) {
			ret = lirc_mode("end", NULL, &mode, &new_entry, &first,
					&last, check, "", 0);
			lirc_printf(
				"%s: warning: end token missing at end of file\n",
				lirc_prog);
		} else {
			lirc_freeconfigentries(new_entry);
			new_entry = NULL;
		}
	}
	if (mode != NULL) {
		if (ret == 0)
			lirc_printf(
				"%s: warning: no end token found for mode \"%s\"\n", lirc_prog,
				mode);
		free(mode);
	}
	if (ret == 0) {
		char* startupmode;

		*config = (struct lirc_config*)
			  malloc(sizeof(struct lirc_config));
		if (*config == NULL) {
			lirc_printf("%s: out of memory\n", lirc_prog);
			lirc_freeconfigentries(first);
			return -1;
		}
		(*config)->first = first;
		(*config)->next = first;
		startupmode = lirc_startupmode((*config)->first);
		(*config)->current_mode =
			startupmode ? strdup(startupmode) : NULL;
		if (lircrc_class[0] != '\0')
			(*config)->lircrc_class = strdup(lircrc_class);
		else
			(*config)->lircrc_class = NULL;
		(*config)->sockfd = -1;
		if (full_name != NULL) {
			*full_name = save_full_name;
			save_full_name = NULL;
		}
	} else {
		*config = NULL;
		lirc_freeconfigentries(first);
	}
	if (filestack)
		stack_free(filestack);
	if (save_full_name)
		free(save_full_name);
	return ret;
}


int lirc_identify(int sockfd)
{
	lirc_cmd_ctx cmd;
	int ret;

	ret = lirc_command_init(&cmd, "IDENT %s\n", lirc_prog);
	if (ret != 0)
		return ret;
	do
		ret = lirc_command_run(&cmd, sockfd);
	while (ret == EAGAIN || ret == EWOULDBLOCK);
	return ret == 0 ? LIRC_RET_SUCCESS : -1;
}



int lirc_readconfig(const char* file,
		    struct lirc_config** config,
		    int (check)(char* s))
{
	struct sockaddr_un addr;
	int sockfd = -1;
	char* filename;
	char command[128];
	int ret;

	filename = NULL;
	if (lirc_readconfig_only_internal(file, config, check, &filename) == -1)
		return -1;

	if ((*config)->lircrc_class == NULL)
		goto lirc_readconfig_compat;

	/* connect to lircrcd */

	addr.sun_family = AF_UNIX;
	if (lirc_getsocketname((*config)->lircrc_class,
			       addr.sun_path,
			       sizeof(addr.sun_path)) > sizeof(addr.sun_path)) {
		lirc_printf("%s: WARNING: file name too long\n", lirc_prog);
		goto lirc_readconfig_compat;
	}
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd == -1) {
		lirc_printf("%s: WARNING: could not open socket\n", lirc_prog);
		lirc_perror(lirc_prog);
		goto lirc_readconfig_compat;
	}
	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) != -1) {
		(*config)->sockfd = sockfd;
		free(filename);

		/* tell daemon lirc_prog */
		if (lirc_identify(sockfd) == LIRC_RET_SUCCESS)
			/* we're connected */
			return 0;
		close(sockfd);
		lirc_freeconfig(*config);
		return -1;
	}
	close(sockfd);
	sockfd = -1;

	/* launch lircrcd */
	snprintf(command, sizeof(command),
		 "lircrcd %s", (*config)->lircrc_class);
	ret = system(command);
	if (ret == -1 || WEXITSTATUS(ret) != EXIT_SUCCESS)
		goto lirc_readconfig_compat;
	free(filename);

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd == -1) {
		lirc_printf("%s: WARNING: could not open socket\n", lirc_prog);
		lirc_perror(lirc_prog);
		goto lirc_readconfig_compat;
	}
	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) != -1) {
		if (lirc_identify(sockfd) == LIRC_RET_SUCCESS) {
			(*config)->sockfd = sockfd;
			return 0;
		}
	}
	close(sockfd);
	lirc_freeconfig(*config);
	return -1;

lirc_readconfig_compat:
	/* compat fallback */
	if (sockfd != -1)
		close(sockfd);
	return 0;
}


int lirc_readconfig_only(const char*		file,
			 struct lirc_config**	config,
			 int			(check) (char* s))
{
	return lirc_readconfig_only_internal(file, config, check, NULL);
}


void lirc_freeconfig(struct lirc_config* config)
{
	if (config != NULL) {
		if (config->sockfd != -1) {
			(void)close(config->sockfd);
			config->sockfd = -1;
		}
		if (config->lircrc_class != NULL)
			free(config->lircrc_class);
		lirc_freeconfigentries(config->first);
		free(config->current_mode);
		free(config);
	}
}


static void lirc_clearmode(struct lirc_config* config)
{
	struct lirc_config_entry* scan;

	if (config->current_mode == NULL)
		return;
	scan = config->first;
	while (scan != NULL) {
		if (scan->change_mode != NULL)
			if (strcasecmp(scan->change_mode,
				       config->current_mode) == 0)
				scan->flags &= ~ecno;
		scan = scan->next;
	}
	free(config->current_mode);
	config->current_mode = NULL;
}


static char* lirc_execute(struct lirc_config*		config,
			  struct lirc_config_entry*	scan)
{
	char* s;
	int do_once = 1;

	if (scan->flags & mode)
		lirc_clearmode(config);
	if (scan->change_mode != NULL) {
		free(config->current_mode);
		config->current_mode = strdup(scan->change_mode);
		if (scan->flags & once) {
			if (scan->flags & ecno)
				do_once = 0;
			else
				scan->flags |= ecno;
		}
	}
	if (scan->next_config != NULL
	    && scan->prog != NULL
	    && (lirc_prog == NULL || strcasecmp(scan->prog, lirc_prog) == 0)
	    && do_once == 1) {
		s = scan->next_config->string;
		scan->next_config = scan->next_config->next;
		if (scan->next_config == NULL)
			scan->next_config = scan->config;
		return s;
	}
	return NULL;
}

/**
 * Checks if the event needs to be generated, based on the "repeat",
 * "delay" and "ignore_first_events" parameters.
 * @param scan contains the config entry that describes the event
 * that matches the key being currently pressed.
 * @param rep is the current number of repeats that happened for that key.
 * @return 1 if the event should be generated, 0 if not.
 */
static int rep_filter(struct lirc_config_entry* scan, int rep)
{
	int delay_start, rep_delay;

	if (scan->ign_first_events) {
		if (scan->rep_delay && rep == 0)        /* warn user only once */
			lirc_printf(
				"%s: ignoring \"delay\" because \"ignore_first_events\" is also set\n",
				lirc_prog);
		rep_delay = scan->ign_first_events;
		delay_start = 0;
	} else {
		rep_delay = scan->rep_delay;
		delay_start = 1;
	}
	/* handle event before delay_start */
	if (rep < delay_start)
		return 1;
	/* special case: 1 event after delay when repeat is not set */
	if (scan->rep == 0 && rep_delay > 0 && rep == rep_delay + delay_start)
		return 1;
	/* handle repeat */
	if (scan->rep > 0 && rep >= rep_delay + delay_start) {
		rep -= rep_delay + delay_start;
		return (rep % scan->rep) == 0;
	}
	return 0;
}

static int lirc_iscode(struct lirc_config_entry*	scan,
		       char*				remote,
		       char*				button,
		       int				rep)
{
	struct lirc_code* codes;

	/* no remote/button specified */
	if (scan->code == NULL)
		return rep_filter(scan, rep);

	/* remote/button match? */
	if (scan->next_code->remote == LIRC_ALL
	    || strcasecmp(scan->next_code->remote, remote) == 0) {
		if (scan->next_code->button == LIRC_ALL
		    || strcasecmp(scan->next_code->button, button) == 0) {
			int iscode = 0;
			/* button sequence? */
			if (scan->code->next == NULL || rep == 0) {
				scan->next_code = scan->next_code->next;
				if (scan->code->next != NULL)
					iscode = 1;
			}
			/* sequence completed? */
			if (scan->next_code == NULL) {
				scan->next_code = scan->code;
				if (scan->code->next != NULL ||
				    rep_filter(scan, rep))
					iscode = 2;
			}
			return iscode;
		}
	}

	if (rep != 0)
		return 0;

	/* handle toggle_reset */
	if (scan->flags & toggle_reset)
		scan->next_config = scan->config;

	codes = scan->code;
	if (codes == scan->next_code)
		return 0;
	codes = codes->next;
	/* rebase code sequence */
	while (codes != scan->next_code->next) {
		struct lirc_code* prev;
		struct lirc_code* next;
		int flag = 1;

		prev = scan->code;
		next = codes;
		while (next != scan->next_code) {
			if (prev->remote == LIRC_ALL
			    || strcasecmp(prev->remote, next->remote) == 0) {
				if (prev->button == LIRC_ALL
				    || strcasecmp(prev->button,
						  next->button) == 0) {
					prev = prev->next;
					next = next->next;
				} else {
					flag = 0;
					break;
				}
			} else {
				flag = 0;
				break;
			}
		}
		if (flag == 1) {
			if (prev->remote == LIRC_ALL
			    || strcasecmp(prev->remote, remote) == 0) {
				if (prev->button == LIRC_ALL
				    || strcasecmp(prev->button, button) == 0) {
					if (rep == 0) {
						scan->next_code = prev->next;
						return 0;
					}
				}
			}
		}
		codes = codes->next;
	}
	scan->next_code = scan->code;
	return 0;
}


char* lirc_ir2char(struct lirc_config* config, char* code)
{
	static int warning = 1;
	char* string;

	if (warning) {
		fprintf(stderr, "%s: warning: lirc_ir2char() is obsolete\n",
			lirc_prog);
		warning = 0;
	}
	if (lirc_code2char(config, code, &string) == -1)
		return NULL;
	return string;
}


static int lirc_code2char_internal(struct lirc_config*	config,
				   char*		code,
				   char**		string,
				   char**		prog)
{
	int rep;
	char* backup;
	char* remote;
	char* button;
	char* s = NULL;
	struct lirc_config_entry* scan;
	int exec_level;
	int quit_happened;

	*string = NULL;
	if (sscanf(code, "%*x %x %*s %*s\n", &rep) == 1) {
		backup = strdup(code);
		if (backup == NULL)
			return -1;

		strtok(backup, " ");
		strtok(NULL, " ");
		button = strtok(NULL, " ");
		remote = strtok(NULL, "\n");

		if (button == NULL || remote == NULL) {
			free(backup);
			return 0;
		}

		scan = config->next;
		quit_happened = 0;
		while (scan != NULL) {
			exec_level = lirc_iscode(scan, remote, button, rep);
			if (exec_level > 0 &&
			    (scan->mode == NULL ||
			     (scan->mode != NULL &&
			      config->current_mode != NULL &&
			      strcasecmp(scan->mode,
					 config->current_mode) == 0)) &&
			    quit_happened == 0) {
				if (exec_level > 1) {
					s = lirc_execute(config, scan);
					if (s != NULL && prog != NULL)
						*prog = scan->prog;
				} else {
					s = NULL;
				}
				if (scan->flags & quit) {
					quit_happened = 1;
					config->next = NULL;
					scan = scan->next;
					continue;
				} else if (s != NULL) {
					config->next = scan->next;
					break;
				}
			}
			scan = scan->next;
		}
		free(backup);
		if (s != NULL) {
			*string = s;
			return 0;
		}
	}
	config->next = config->first;
	return 0;
}


int lirc_code2char(struct lirc_config* config, char* code, char** string)
{
	lirc_cmd_ctx cmd;
	static char static_buff[PACKET_SIZE];
	int ret;
	char* my_code;
	char* pos;

	my_code = strdup(code);
	pos = rindex(my_code, '\n');
	if (pos != NULL)
		*pos = '\0';
	ret = lirc_command_init(&cmd, "CODE %s\n", my_code);
	free(my_code);
	if (ret != 0)
		return -1;
	if (config->sockfd != -1) {
		do
			ret = lirc_command_run(&cmd, config->sockfd);
		while (ret == EAGAIN || ret == EWOULDBLOCK);
		if (ret == 0) {
			strncpy(static_buff, cmd.reply, PACKET_SIZE);
			*string = static_buff;
		}
		return ret == 0 ? 0 : -1;
	}
	return lirc_code2char_internal(config, code, string, NULL);
}


int lirc_code2charprog(struct lirc_config*	config,
		       char*			code,
		       char**			string,
		       char**			prog)
{
	char* backup;
	int ret;

	backup = lirc_prog;
	lirc_prog = NULL;

	ret = lirc_code2char_internal(config, code, string, prog);

	lirc_prog = backup;
	return ret;
}


char* lirc_nextir(void)
{
	static int warning = 1;
	char* code;
	int ret;

	if (warning) {
		fprintf(stderr, "%s: warning: lirc_nextir() is obsolete\n",
			lirc_prog);
		warning = 0;
	}
	ret = lirc_nextcode(&code);
	if (ret == -1)
		return NULL;
	return code;
}


int lirc_nextcode(char** code)
{
	static int packet_size = PACKET_SIZE;
	static int end_len = 0;
	ssize_t len = 0;
	char* end;
	char c;

	*code = NULL;
	if (lirc_buffer == NULL) {
		lirc_buffer = (char*)malloc(packet_size + 1);
		if (lirc_buffer == NULL) {
			lirc_printf("%s: out of memory\n", lirc_prog);
			return -1;
		}
		lirc_buffer[0] = 0;
	}
	while ((end = strchr(lirc_buffer, '\n')) == NULL) {
		if (end_len >= packet_size) {
			char* new_buffer;

			packet_size += PACKET_SIZE;
			new_buffer =
				(char*)realloc(lirc_buffer, packet_size + 1);
			if (new_buffer == NULL)
				return -1;
			lirc_buffer = new_buffer;
		}
		len = read(lirc_lircd, lirc_buffer + end_len,
			   packet_size - end_len);
		if (len <= 0) {
			if (len == -1 && errno == EAGAIN)
				return 0;
			else
				return -1;
		}
		end_len += len;
		lirc_buffer[end_len] = 0;
		/* return if next code not yet available completely */
		end = strchr(lirc_buffer, '\n');
		if (end == NULL)
			return 0;
	}
	/* copy first line to buffer (code) and move remaining chars to
	 * lirc_buffers start */
	end++;
	end_len = strlen(end);
	c = end[0];
	end[0] = 0;
	*code = strdup(lirc_buffer);
	end[0] = c;
	memmove(lirc_buffer, end, end_len + 1);
	if (*code == NULL)
		return -1;
	return 0;
}


size_t lirc_getsocketname(const char* id, char* buf, size_t size)
{
	id = id != NULL ? id : "default";
	snprintf(buf, size, VARRUNDIR "/%d-%s-lircrcd.socket", getuid(), id);
	return strlen(buf);
}



const char* lirc_getmode(struct lirc_config* config)
{
	lirc_cmd_ctx cmd;
	static char static_buff[PACKET_SIZE];
	int ret;

	if (config->sockfd != -1) {
		lirc_command_init(&cmd, "GETMODE\n");
		do
			ret = lirc_command_run(&cmd, config->sockfd);
		while (ret == EAGAIN || ret == EWOULDBLOCK);
		if (ret == 0) {
			strncpy(static_buff, cmd.reply, PACKET_SIZE);
			return static_buff;
		}
		return NULL;
	}
	return config->current_mode;
}


const char* lirc_setmode(struct lirc_config* config, const char* mode)
{
	lirc_cmd_ctx cmd;
	int r;
	static char static_buff[PACKET_SIZE];

	if (config->sockfd != -1) {
		if (mode != NULL)
			r = lirc_command_init(&cmd, "SETMODE %s\n", mode);
		else
			r = lirc_command_init(&cmd, "SETMODE\n");
		if (r != 0)
			return NULL;
		do
			r = lirc_command_run(&cmd, config->sockfd);
		while (r == EAGAIN || r == EWOULDBLOCK);
		if (r == 0) {
			strncpy(static_buff, cmd.reply, PACKET_SIZE);
			return static_buff;
		}
		return NULL;
	}
	free(config->current_mode);
	config->current_mode = mode ? strdup(mode) : NULL;
	return config->current_mode;
}


int lirc_send_one(int fd, const char* remote, const char* keysym)
{
	int r;
	lirc_cmd_ctx command;

	r = lirc_command_init(&command, "SEND_ONCE %s %s\n", remote, keysym);
	if (r != 0)
		return EMSGSIZE;
	do
		r = lirc_command_run(&command, fd);
	while (r == EAGAIN);
	return r;
}


int lirc_simulate(int		fd,
		  const char*	remote,
		  const char*	keysym,
		  int		scancode,
		  int		repeat)
{
	lirc_cmd_ctx cmd;
	int r;

	r = lirc_command_init(&cmd, "SIMULATE %016x %02x %s %s\n",
			      scancode, repeat, keysym, remote);
	if (r != 0)
		return EMSGSIZE;
	do
		r = lirc_command_run(&cmd, fd);
	while (r == EAGAIN);
	return r;
}


/** Create and connect() socket to addr, print errors unless quiet. */
static int
do_connect(int domain, struct sockaddr* addr, size_t size, int quiet)
{
	int fd;

	fd = socket(domain, SOCK_STREAM, 0);
	if (fd == -1) {
		if (!quiet) {
			fprintf(stderr, "do_connect: could not open socket\n");
			perror("open");
		}
		return -errno;
	}
	if (connect(fd, addr, size) == -1) {
		if (!quiet) {
			fprintf(stderr,
				"do_connect: could not connect to socket\n");
			perror("connect");
		}
		return -errno;
	}
	return fd;
}


int lirc_get_local_socket(const char* path, int quiet)
{
	const char* socket_path;
	struct sockaddr_un addr_un;

	socket_path = path ? path : getenv("LIRC_SOCKET_PATH");
	socket_path = socket_path ? socket_path : LIRCD;
	if (strlen(socket_path) + 1 > sizeof(addr_un.sun_path)) {
		/* path is longer than sockaddr_un.sun_path field (!) */
		if (!quiet)
			fprintf(stderr, "%s: socket name is too long\n", prog);
		return -ENAMETOOLONG;
	}
	addr_un.sun_family = AF_UNIX;
	strcpy(addr_un.sun_path, socket_path);
	return do_connect(AF_UNIX,
			  (struct sockaddr*)&addr_un,
			  sizeof(addr_un),
			  quiet);
}


int lirc_get_remote_socket(const char* address, int port, int quiet)
{
	struct addrinfo* addrinfos;
	struct addrinfo* a;
	char service[64];
	int r;

	snprintf(service, sizeof(service),
		 "%d", port > 0 ? port : LIRC_INET_PORT);
	r = getaddrinfo(address, service, NULL, &addrinfos);
	if (r < 0) {
		if (!quiet)
			fprintf(stderr, "get_remote_socket: host %s unknown\n",
				address);
		return -EADDRNOTAVAIL;
	}
	for (a = addrinfos; a != NULL; a = a->ai_next) {
		r = do_connect(a->ai_family, a->ai_addr, a->ai_addrlen, quiet);
		if (r >= 0)
			break;
	};
	freeaddrinfo(addrinfos);
	return r;
}
