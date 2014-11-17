
/****************************************************************************
 ** ir_remote.c *************************************************************
 ****************************************************************************
 *
 * ir_remote.c - sends and decodes the signals from IR remotes
 *
 * Copyright (C) 1996,97 Ralph Metzler (rjkm@thp.uni-koeln.de)
 * Copyright (C) 1998 Christoph Bartelmus (lirc@bartelmus.de)
 *
 */

/**
 * @file    ir_remote.c
 * @authors  Ralph Metzler, Christoph Bartelmus
 * @copyright
 * Copyright (C) 1996,97 Ralph Metzler (rjkm@thp.uni-koeln.de)
 * Copyright (C) 1998 Christoph Bartelmus (lirc@bartelmus.de)
 * @brief Implements ir_remote.h
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>

#include <sys/ioctl.h>

#include "include/media/lirc.h"
#include "lirc/ir_remote.h"
#include "lirc/driver.h"
#include "lirc/release.h"
#include "lirc/lirc_log.h"

/** Const data sent for EOF condition.  */
static struct ir_ncode NCODE_EOF =
	{"__EOF", LIRC_EOF, 1, NULL, NULL, NULL, 0};

/** Const packet sent for EOF condition. */
static const char* const PACKET_EOF = "0000000008000000 00 __EOF lirc\n";

/** Const dummy remote used for lirc internal decoding. */
static struct ir_remote lirc_internal_remote = {"lirc"};

struct ir_remote* decoding = NULL;

struct ir_remote* last_remote = NULL;

struct ir_remote* repeat_remote = NULL;

struct ir_ncode *repeat_code;

static int dyncodes = 0;

void ir_remote_init(int use_dyncodes)
{
	dyncodes= use_dyncodes;
}

static  lirc_t time_left(struct timeval *current, struct timeval *last, lirc_t gap)
{
	unsigned long secs, diff;

	secs = current->tv_sec - last->tv_sec;

	diff = 1000000 * secs + current->tv_usec - last->tv_usec;

	return ((lirc_t) (diff < gap ? gap - diff : 0));
}

static int match_ir_code(struct ir_remote *remote, ir_code a, ir_code b)
{
	return ((remote->ignore_mask | a) == (remote->ignore_mask | b)
		|| (remote->ignore_mask | a) == (remote->ignore_mask | (b ^ remote->toggle_bit_mask)));
}

/**
 *
 * @param remotes
 * @param min_freq
 * @param max_freq
 */
void get_frequency_range(const struct ir_remote *remotes, unsigned int *min_freq, unsigned int *max_freq)
{
	const struct ir_remote *scan;

	/* use remotes carefully, it may be changed on SIGHUP */
	scan = remotes;
	if (scan == NULL) {
		*min_freq = 0;
		*max_freq = 0;
	} else {
		*min_freq = scan->freq;
		*max_freq = scan->freq;
		scan = scan->next;
	}
	while (scan) {
		if (scan->freq != 0) {
			if (scan->freq > *max_freq) {
				*max_freq = scan->freq;
			} else if (scan->freq < *min_freq) {
				*min_freq = scan->freq;
			}
		}
		scan = scan->next;
	}
}

/**
 *
 * @param remotes
 * @param max_gap_lengthp
 * @param min_pulse_lengthp
 * @param min_space_lengthp
 * @param max_pulse_lengthp
 * @param max_space_lengthp
 */
void get_filter_parameters(const struct ir_remote *remotes, lirc_t * max_gap_lengthp, lirc_t * min_pulse_lengthp,
			   lirc_t * min_space_lengthp, lirc_t * max_pulse_lengthp, lirc_t * max_space_lengthp)
{
	const struct ir_remote *scan = remotes;
	lirc_t max_gap_length = 0;
	lirc_t min_pulse_length = 0, min_space_length = 0;
	lirc_t max_pulse_length = 0, max_space_length = 0;

	while (scan) {
		lirc_t val;
		val = upper_limit(scan, scan->max_gap_length);
		if (val > max_gap_length) {
			max_gap_length = val;
		}
		val = lower_limit(scan, scan->min_pulse_length);
		if (min_pulse_length == 0 || val < min_pulse_length) {
			min_pulse_length = val;
		}
		val = lower_limit(scan, scan->min_space_length);
		if (min_space_length == 0 || val > min_space_length) {
			min_space_length = val;
		}
		val = upper_limit(scan, scan->max_pulse_length);
		if (val > max_pulse_length) {
			max_pulse_length = val;
		}
		val = upper_limit(scan, scan->max_space_length);
		if (val > max_space_length) {
			max_space_length = val;
		}
		scan = scan->next;
	}
	*max_gap_lengthp = max_gap_length;
	*min_pulse_lengthp = min_pulse_length;
	*min_space_lengthp = min_space_length;
	*max_pulse_lengthp = max_pulse_length;
	*max_space_lengthp = max_space_length;
}

/**
 *
 * @param remotes
 * @param remote
 * @return
 */
const struct ir_remote *is_in_remotes(const struct ir_remote *remotes,
                     		      const struct ir_remote *remote)
{
	while (remotes != NULL) {
		if (remotes == remote) {
			return remote;
		}
		remotes = remotes->next;
	}
	return NULL;
}

/**
 *
 * @param remotes
 * @param name
 * @return
 */
struct ir_remote *get_ir_remote(const struct ir_remote *remotes,
                                const char *name)
{
	const struct ir_remote *all;

	/* use remotes carefully, it may be changed on SIGHUP */
	all = remotes;
	if (strcmp(name, "lirc") == 0) {
		return &lirc_internal_remote;
	}
	while (all) {
		if (strcasecmp(all->name, name) == 0) {
			return (struct ir_remote*) all;
		}
		all = all->next;
	}
	return (NULL);
}

/**
 *
 * @param remote
 * @param prep
 * @param codep
 * @param postp
 * @param pre_bits
 * @param pre
 * @param bits
 * @param code
 * @param post_bits
 * @param post
 * @return
 */
int map_code(const struct ir_remote* remote,
	     struct decode_ctx_t* ctx,
	     int pre_bits,
	     ir_code pre,
             int bits,
	     ir_code code,
             int post_bits,
             ir_code post)

{
	ir_code all;

	if (pre_bits + bits + post_bits != remote->pre_data_bits + remote->bits + remote->post_data_bits) {
		return (0);
	}
	all = (pre & gen_mask(pre_bits));
	all <<= bits;
	all |= (code & gen_mask(bits));
	all <<= post_bits;
	all |= (post & gen_mask(post_bits));

	ctx->post = (all & gen_mask(remote->post_data_bits));
	all >>= remote->post_data_bits;
	ctx->code = (all & gen_mask(remote->bits));
	all >>= remote->bits;
	ctx->pre = (all & gen_mask(remote->pre_data_bits));

	LOGPRINTF(1, "pre: %llx", (__u64) (ctx->pre));
	LOGPRINTF(1, "code: %llx", (__u64) (ctx->code));
	LOGPRINTF(1, "post: %llx", (__u64) (ctx->post));
	LOGPRINTF(1, "code:                   %016llx\n", code);

	return (1);
}

/**
 *
 * @param remote
 * @param start
 * @param last
 * @param signal_length
 * @param repeat_flagp
 * @param min_remaining_gapp
 * @param max_remaining_gapp
 */
void map_gap(const struct ir_remote *remote,
	     struct decode_ctx_t* ctx,
             const struct timeval *start,
	     const struct timeval *last,
	     lirc_t signal_length)
{
	// Time gap (us) between a keypress on the remote control and
	// the next one.
	lirc_t gap;

	// Check the time gap between the last keypress and this one.
	if (start->tv_sec - last->tv_sec >= 2) {
		// Gap of 2 or more seconds: this is not a repeated keypress.
		ctx->repeat_flag = 0;
		gap = 0;
	} else {
		// Calculate the time gap in microseconds.
		gap = time_elapsed(last, start);
		if (expect_at_most(remote, gap, remote->max_remaining_gap)) {
			// The gap is shorter than a standard gap
			// (with relative or aboslute tolerance): this
			// is a repeated keypress.
			ctx->repeat_flag = 1;
		} else {
			// Standard gap: this is a new keypress.
			ctx->repeat_flag = 0;
		}
	}

	// Calculate extimated time gap remaining for the next code.
	if (is_const(remote)) {
		// The sum (signal_length + gap) is always constant
		// so the gap is shorter when the code is longer.
		if (min_gap(remote) > signal_length) {
			ctx->min_remaining_gap = min_gap(remote) - signal_length;
			ctx->max_remaining_gap = max_gap(remote) - signal_length;
		} else {
			ctx->min_remaining_gap = 0;
			if (max_gap(remote) > signal_length) {
				ctx->max_remaining_gap = max_gap(remote) - signal_length;
			} else {
				ctx->max_remaining_gap = 0;
			}
		}
	} else {
		// The gap after the signal is always constant.
		// This is the case of Kanam Accent serial remote.
		ctx->min_remaining_gap = min_gap(remote);
		ctx->max_remaining_gap = max_gap(remote);
	}

	LOGPRINTF(1, "repeat_flagp:           %d", (ctx->repeat_flag));
	LOGPRINTF(1, "is_const(remote):       %d", is_const(remote));
	LOGPRINTF(1, "remote->gap range:      %lu %lu", (__u32) min_gap(remote), (__u32) max_gap(remote));
	LOGPRINTF(1, "remote->remaining_gap:  %lu %lu", (__u32) remote->min_remaining_gap,
		  (__u32) remote->max_remaining_gap);
	LOGPRINTF(1, "signal length:          %lu", (__u32) signal_length);
	LOGPRINTF(1, "gap:                    %lu", (__u32) gap);
	LOGPRINTF(1, "extim. remaining_gap:   %lu %lu", (__u32) (ctx->min_remaining_gap), (__u32)(ctx->max_remaining_gap));

}

/**
 *
 * @param remote
 * @param name
 * @return
 */
struct ir_ncode *get_code_by_name(const struct ir_remote *remote, const char *name)
{
	const struct ir_ncode *all;

	all = remote->codes;
	if (strcmp(remote->name, "lirc") == 0) {
		 return strcmp(name, "__EOF") == 0 ? &NCODE_EOF : 0;
	}
	while (all->name != NULL) {
		if (strcasecmp(all->name, name) == 0) {
			return (struct ir_ncode*) all;
		}
		all++;
	}
	return (0);
}

static struct ir_ncode *get_code(struct ir_remote *remote, ir_code pre, ir_code code, ir_code post,
			  int *repeat_flag, ir_code * toggle_bit_mask_statep)
{
	ir_code pre_mask, code_mask, post_mask, toggle_bit_mask_state, all;
	int found_code, have_code;
	struct ir_ncode *codes, *found;

	pre_mask = code_mask = post_mask = 0;

	if (has_toggle_bit_mask(remote)) {
		pre_mask = remote->toggle_bit_mask >> (remote->bits + remote->post_data_bits);
		post_mask = remote->toggle_bit_mask & gen_mask(remote->post_data_bits);
	}
	if (has_ignore_mask(remote)) {
		pre_mask |= remote->ignore_mask >> (remote->bits + remote->post_data_bits);
		post_mask |= remote->ignore_mask & gen_mask(remote->post_data_bits);
	}
	if (has_toggle_mask(remote) && remote->toggle_mask_state % 2) {
		ir_code *affected, mask, mask_bit;
		int bit, current_bit;

		affected = &post;
		mask = remote->toggle_mask;
		for (bit = current_bit = 0; bit < bit_count(remote); bit++, current_bit++) {
			if (bit == remote->post_data_bits) {
				affected = &code;
				current_bit = 0;
			}
			if (bit == remote->post_data_bits + remote->bits) {
				affected = &pre;
				current_bit = 0;
			}
			mask_bit = mask & 1;
			(*affected) ^= (mask_bit << current_bit);
			mask >>= 1;
		}
	}
	if (has_pre(remote)) {
		if ((pre | pre_mask) != (remote->pre_data | pre_mask)) {
			LOGPRINTF(1, "bad pre data");
			LOGPRINTF(2, "%llx %llx", pre, remote->pre_data);
			return (0);
		}
		LOGPRINTF(1, "pre");
	}

	if (has_post(remote)) {
		if ((post | post_mask) != (remote->post_data | post_mask)) {
			LOGPRINTF(1, "bad post data");
			LOGPRINTF(2, "%llx %llx", post, remote->post_data);
			return (0);
		}
		LOGPRINTF(1, "post");
	}

	all = gen_ir_code(remote, pre, code, post);

	if(*repeat_flag && has_repeat_mask(remote)) {
	        all ^= remote->repeat_mask;
	}

	toggle_bit_mask_state = all & remote->toggle_bit_mask;

	found = NULL;
	found_code = 0;
	have_code = 0;
	codes = remote->codes;
	if (codes != NULL) {
		while (codes->name != NULL) {
			ir_code next_all;

			next_all =
			    gen_ir_code(remote, remote->pre_data, get_ir_code(codes, codes->current),
					remote->post_data);
			if (match_ir_code(remote, next_all, all) ||
			    (*repeat_flag && has_repeat_mask(remote) &&
			     match_ir_code(remote, next_all, all ^ remote->repeat_mask))) {
				found_code = 1;
				if (codes->next != NULL) {
					if (codes->current == NULL) {
						codes->current = codes->next;
					} else {
						codes->current = codes->current->next;
					}
				}
				if (!have_code) {
					found = codes;
					if (codes->current == NULL) {
						have_code = 1;
					}
				}
			} else {
				/* find longest matching sequence */
				struct ir_code_node *search;

				search = codes->next;
				if (search == NULL || (codes->next != NULL && codes->current == NULL)) {
					codes->current = NULL;
				} else {
					int sequence_match = 0;

					while (search != codes->current->next) {
						struct ir_code_node *prev, *next;
						int flag = 1;

						prev = NULL;	/* means codes->code */
						next = search;
						while (next != codes->current) {
							if (get_ir_code(codes, prev) != get_ir_code(codes, next)) {
								flag = 0;
								break;
							}
							prev = get_next_ir_code_node(codes, prev);
							next = get_next_ir_code_node(codes, next);
						}
						if (flag == 1) {
							next_all =
							    gen_ir_code(remote, remote->pre_data,
									get_ir_code(codes, prev), remote->post_data);
							if (match_ir_code(remote, next_all, all)) {
								codes->current = get_next_ir_code_node(codes, prev);
								sequence_match = 1;
								found_code = 1;
								if (!have_code) {
									found = codes;
								}
								break;
							}
						}
						search = search->next;
					}
					if (!sequence_match)
						codes->current = NULL;
				}
			}
			codes++;
		}
	}
	if (!found_code && dyncodes) {
		if (remote->dyncodes[remote->dyncode].code != code) {
			remote->dyncode++;
			remote->dyncode %= 2;
		}
		remote->dyncodes[remote->dyncode].code = code;
		found = &(remote->dyncodes[remote->dyncode]);
		found_code = 1;
	}
	if (found_code && found != NULL && has_toggle_mask(remote)) {
		if (!(remote->toggle_mask_state % 2)) {
			remote->toggle_code = found;
			LOGPRINTF(1, "toggle_mask_start");
		} else {
			if (found != remote->toggle_code) {
				remote->toggle_code = NULL;
				return (NULL);
			}
			remote->toggle_code = NULL;
		}
	}
	*toggle_bit_mask_statep = toggle_bit_mask_state;
	return (found);
}

static __u64 set_code(struct ir_remote * remote, struct ir_ncode * found, ir_code toggle_bit_mask_state, struct decode_ctx_t* ctx)
{
	struct timeval current;
	static struct ir_remote *last_decoded = NULL;

	LOGPRINTF(1, "found: %s", found->name);

	gettimeofday(&current, NULL);
	LOGPRINTF(1, "%lx %lx %lx %d %d %d %d %d %d %d",
		  remote, last_remote, last_decoded,
		  remote == last_decoded,
		  found == remote->last_code, found->next != NULL, found->current != NULL, ctx->repeat_flag,
		  time_elapsed(&remote->last_send, &current) < 1000000, (!has_toggle_bit_mask(remote)
									 || toggle_bit_mask_state ==
									 remote->toggle_bit_mask_state));
	if (remote->release_detected) {
		remote->release_detected = 0;
		if (ctx->repeat_flag) {
			LOGPRINTF(0, "repeat indicated although release was detected before");
		}
		ctx->repeat_flag = 0;
	}
	if (remote == last_decoded &&
	    (found == remote->last_code || (found->next != NULL && found->current != NULL)) &&
	    ctx->repeat_flag && time_elapsed(&remote->last_send, &current) < 1000000 && (!has_toggle_bit_mask(remote)
										    || toggle_bit_mask_state ==
										    remote->toggle_bit_mask_state)) {
		if (has_toggle_mask(remote)) {
			remote->toggle_mask_state++;
			if (remote->toggle_mask_state == 4) {
				remote->reps++;
				remote->toggle_mask_state = 2;
			}
		} else if (found->current == NULL) {
			remote->reps++;
		}
	} else {
		if (found->next != NULL && found->current == NULL) {
			remote->reps = 1;
		} else {
			remote->reps = 0;
		}
		if (has_toggle_mask(remote)) {
			remote->toggle_mask_state = 1;
			remote->toggle_code = found;
		}
		if (has_toggle_bit_mask(remote)) {
			remote->toggle_bit_mask_state = toggle_bit_mask_state;
		}
	}
	last_remote = remote;
	last_decoded = remote;
	if (found->current == NULL)
		remote->last_code = found;
	remote->last_send = current;
	remote->min_remaining_gap = ctx->min_remaining_gap;
	remote->max_remaining_gap = ctx->max_remaining_gap;

	ctx->code = 0;
	if (has_pre(remote)) {
		ctx->code |= remote->pre_data;
		ctx->code = ctx->code << remote->bits;
	}
	ctx->code |= found->code;
	if (has_post(remote)) {
		ctx->code = ctx->code << remote->post_data_bits;
		ctx->code |= remote->post_data;
	}
	if (remote->flags & COMPAT_REVERSE) {
		/* actually this is wrong: pre, code and post should
		   be rotated separately but we have to stay
		   compatible with older software
		 */
		ctx->code = reverse(ctx->code, bit_count(remote));
	}
	return (ctx->code);
}

/**
 * Formats the arguments into a readable string (first argument, size the second argument).
 * The arguments start at the third position.) into a nice
 * @param buffer
 * @param size
 * @param remote_name
 * @param button_name
 * @param button_suffix
 * @param code
 * @param reps
 * @return
 */
int write_message(char *buffer, size_t size, const char *remote_name, const char *button_name,
		  const char *button_suffix, ir_code code, int reps)
{
	int len;

	len = snprintf(buffer, size, "%016llx %02x %s%s %s\n",
		(unsigned long long)code, reps, button_name, button_suffix, remote_name);

	return len;
}

/**
 * Tries to decode current signal trying all known remotes.
 * @param remotes
 * @return
 */
char *decode_all(struct ir_remote *remotes)
{
	struct ir_remote *remote;
	static char message[PACKET_SIZE + 1];
	struct ir_ncode *ncode;
	ir_code toggle_bit_mask_state;
	struct ir_remote *scan;
	struct ir_ncode *scan_ncode;
	struct decode_ctx_t ctx;

	/* use remotes carefully, it may be changed on SIGHUP */
	decoding = remote = remotes;
	while (remote) {
		LOGPRINTF(1, "trying \"%s\" remote", remote->name);
		if (curr_driver->decode_func(remote, &ctx)
		    && (ncode = get_code(remote, ctx.pre, ctx.code, ctx.post, &ctx.repeat_flag, &toggle_bit_mask_state))) {
			int len;
			int reps;
			if (ncode == &NCODE_EOF) {
				logprintf(LIRC_DEBUG, "decode all: returning EOF");
				strncpy(message, PACKET_EOF, sizeof(message));
				return message;
			}
			ctx.code = set_code(remote, ncode, toggle_bit_mask_state, &ctx);
			if ((has_toggle_mask(remote) && remote->toggle_mask_state % 2) || ncode->current != NULL) {
				decoding = NULL;
				return (NULL);
			}

			for (scan = decoding; scan != NULL; scan = scan->next) {
				for (scan_ncode = scan->codes; scan_ncode->name != NULL; scan_ncode++) {
					scan_ncode->current = NULL;
				}
			}
			if (is_xmp(remote)) {
				remote->last_code->current = remote->last_code->next;
			}
			reps = remote->reps - (ncode->next ? 1 : 0);
			if (reps > 0) {
				if (reps <= remote->suppress_repeat) {
					decoding = NULL;
					return NULL;
				} else {
					reps -= remote->suppress_repeat;
				}
			}
			register_button_press(remote, remote->last_code, ctx.code, reps);

			len =
			    write_message(message, PACKET_SIZE + 1, remote->name, remote->last_code->name, "", ctx.code,
					  reps);
			decoding = NULL;
			if (len >= PACKET_SIZE + 1) {
				logprintf(LIRC_ERROR, "message buffer overflow");
				return (NULL);
			} else {
				return (message);
			}
		} else {
			LOGPRINTF(1, "failed \"%s\" remote", remote->name);
		}
		remote->toggle_mask_state = 0;
		remote = remote->next;
	}
	decoding = NULL;
	last_remote = NULL;
	LOGPRINTF(1, "decoding failed for all remotes");
	return (NULL);
}

/**
 * Transmits the actual code in the second  argument by calling the current hardware driver.
 * @param remote Currently active remote, used as data base for timing, and as keeper of an internal state.
 * @param code IR code to be transmitted
 * @return nonzero if success
 */
int send_ir_ncode(struct ir_remote *remote, struct ir_ncode *code, int delay)
{
	int ret;

	if (delay) {
	/* insert pause when needed: */
		if (remote->last_code != NULL) {
			struct timeval current;
			unsigned long usecs;

			gettimeofday(&current, NULL);
			usecs = time_left(&current, 
                                          &remote->last_send, 
					  remote->min_remaining_gap * 2);
			if (usecs > 0) {
				if (repeat_remote == NULL || remote != repeat_remote 
                                        || remote->last_code != code) 
                                {
					usleep(usecs);
				}
			}
		}
	}

	ret = curr_driver->send_func(remote, code);

	if (ret) {
		gettimeofday(&remote->last_send, NULL);
		remote->last_code = code;
	}

	return ret;
}
