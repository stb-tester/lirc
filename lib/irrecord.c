/***************************************************************************
** irrecord.c **************************************************************
****************************************************************************
*
* irrecord - library for recording IR-codes for usage with lircd
*
* Copyright (C) 1998,99 Christoph Bartelmus <lirc@bartelmus.de>
*
*/

#define _GNU_SOURCE

#include <unistd.h>
#include <poll.h>

#include "lirc_private.h"
#include "irrecord.h"

/*    -------------------------- C file -------------------------------- */

static const logchannel_t logchannel = LOG_LIB;

// forwards
static lirc_t emulation_readdata(lirc_t timeout);

// Constants
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

static const int IR_CODE_NODE_SIZE = sizeof(struct ir_code_node);

// Globals

struct ir_remote remote;
unsigned int eps = 30;
lirc_t aeps = 100;

// Static data

static lirc_t signals[MAX_SIGNALS];
static struct ir_remote* emulation_data;
static struct ir_ncode* next_code = NULL;
static struct ir_ncode* current_code = NULL;
static int current_index = 0;
static int current_rep = 0;

static struct lengths* first_space = NULL;
static struct lengths* first_pulse = NULL;
static struct lengths* first_sum = NULL;
static struct lengths* first_gap = NULL;
static struct lengths* first_repeat_gap = NULL;
static struct lengths* first_signal_length = NULL;
static struct lengths* first_headerp = NULL;
static struct lengths* first_headers = NULL;
static struct lengths* first_1lead = NULL;
static struct lengths* first_3lead = NULL;
static struct lengths* first_trail = NULL;
static struct lengths* first_repeatp = NULL;
static struct lengths* first_repeats = NULL;

static __u32 lengths[MAX_SIGNALS];
static __u32 first_length, first_lengths, second_lengths;
static unsigned int count, count_spaces, count_signals;
static unsigned int count_3repeats, count_5repeats;


// Functions

/** snprintf-style message formatting into state->message. */
void btn_state_set_message(struct button_state* state, const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(state->message, sizeof(state->message), fmt, ap);
	va_end(ap);
}


static void fprint_copyright(FILE* fout)
{
	fprintf(fout, "\n"
		"# Please take the time to finish this file as described in\n"
		"# https://sourceforge.net/p/lirc-remotes/wiki/Checklist/\n"
		"# and make it available to others by sending it to\n"
		"# <lirc@bartelmus.de>\n");
}


/** Return 1 if there is available after running poll(2), else 0. */
int availabledata(void)
{
	struct pollfd pfd = {
		.fd = curr_driver->fd, .events = POLLIN, .revents = 0};
	int ret;

	do {
		do {
			ret = curl_poll(&pfd, 1, 0);
		} while (ret == -1 && errno == EINTR);
		if (ret == -1) {
			log_perror_err("availabledata: curl_poll() failed");
			continue;
		}
	} while (ret == -1);

	return pfd.revents & POLLIN ? 1 : 0;
}


/** Clear the driver input buffers. */
void flushhw(void)
{
	size_t size = 1;
	char buffer[PACKET_SIZE];

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
	while (read(curr_driver->fd, buffer, size) == size)
		;
}


/** Reset the hardware. Return 1 on OK, else 0 and possibly closes driver. */
int resethw(int started_as_root)
{
	int flags;

	if (started_as_root)
		if (seteuid(0) == -1)
			log_error("Cannot reset root uid");
	if (curr_driver->deinit_func)
		curr_driver->deinit_func();
	if (curr_driver->init_func) {
		if (!curr_driver->init_func()) {
			drop_sudo_root(seteuid);
			return 0;
		}
	}
	flags = fcntl(curr_driver->fd, F_GETFL, 0);
	if (flags == -1 ||
	    fcntl(curr_driver->fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		if (curr_driver->deinit_func)
			curr_driver->deinit_func();
		drop_sudo_root(seteuid);
		return 0;
	}
	drop_sudo_root(seteuid);
	return 1;
}


void gap_state_init(struct gap_state* state)
{
	memset(state, 0, sizeof(struct gap_state));
}


void lengths_state_init(struct lengths_state* state)
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
}


void toggle_state_init(struct toggle_state* state)
{
	memset(state, 0, sizeof(struct toggle_state));
	state->retries = 30;
	state->retval = EXIT_SUCCESS;
}


void button_state_init(struct button_state* state, int started_as_root)
{
	memset(state, 0, sizeof(struct button_state));
	state->started_as_root = started_as_root;
	state->retval = EXIT_SUCCESS;
}


static lirc_t calc_signal(struct lengths* len)
{
	if (len->count == 0)
		return 0;
	return (lirc_t)(len->sum / len->count);
}


static void set_toggle_bit_mask(struct ir_remote* remote, ir_code xor)
{
	ir_code mask;
	struct ir_ncode* codes;
	int bits;

	if (!remote->codes)
		return;

	bits = bit_count(remote);
	mask = ((ir_code)1) << (bits - 1);
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


void get_pre_data(struct ir_remote* remote)
{
	struct ir_ncode* codes;
	ir_code mask, last;
	int count, i;
	struct ir_code_node* n;

	if (remote->bits == 0)
		return;
	mask = (-1);
	codes = remote->codes;
	if (codes->name == NULL)
		return;         /* at least 2 codes needed */
	last = codes->code;
	codes++;
	if (codes->name == NULL)
		return;         /* at least 2 codes needed */
	while (codes->name != NULL) {
		mask &= ~(last ^ codes->code);
		last = codes->code;
		for (n = codes->next; n != NULL; n = n->next) {
			mask &= ~(last ^ n->code);
			last = n->code;
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
	if (count % 8 && (remote->bits - count) % 8)
		count -= count % 8;
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
			codes->code &= ~mask;
			for (n = codes->next; n != NULL; n = n->next)
				n->code &= ~mask;
			codes++;
		}
	}
}


void get_post_data(struct ir_remote* remote)
{
	struct ir_ncode* codes;
	ir_code mask, last;
	int count, i;
	struct ir_code_node* n;

	if (remote->bits == 0)
		return;

	mask = (-1);
	codes = remote->codes;
	if (codes->name == NULL)
		return;         /* at least 2 codes needed */
	last = codes->code;
	codes++;
	if (codes->name == NULL)
		return;         /* at least 2 codes needed */
	while (codes->name != NULL) {
		mask &= ~(last ^ codes->code);
		last = codes->code;
		for (n = codes->next; n != NULL; n = n->next) {
			mask &= ~(last ^ n->code);
			last = n->code;
		}
		codes++;
	}
	count = 0;
	while (mask & 0x1) {
		count++;
		mask = mask >> 1;
	}
	/* only "even" numbers should go to pre/post data */
	if (count % 8 && (remote->bits - count) % 8)
		count -= count % 8;
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
			codes->code = codes->code >> count;
			for (n = codes->next; n != NULL; n = n->next)
				n->code = n->code >> count;
			codes++;
		}
	}
}


void remove_pre_data(struct ir_remote* remote)
{
	struct ir_ncode* codes;
	struct ir_code_node* n;

	if (remote->pre_data_bits == 0
	    || remote->pre_p != 0
	    || remote->pre_s != 0)
		return;
	for (codes = remote->codes; codes->name != NULL; codes++) {
		codes->code |= remote->pre_data << remote->bits;
		for (n = codes->next; n != NULL; n = n->next)
			n->code |= remote->pre_data << remote->bits;
	}
	remote->bits += remote->pre_data_bits;
	remote->pre_data = 0;
	remote->pre_data_bits = 0;
}


void remove_post_data(struct ir_remote* remote)
{
	struct ir_ncode* codes;
	struct ir_code_node* n;

	if (remote->post_data_bits == 0)
		return;
	for (codes = remote->codes; codes->name != NULL; codes++) {
		codes->code <<= remote->post_data_bits;
		codes->code |= remote->post_data;
		for (n = codes->next; n != NULL; n = n->next) {
			n->code <<= remote->post_data_bits;
			n->code |= remote->post_data;
		}
	}
	remote->bits += remote->post_data_bits;
	remote->post_data = 0;
	remote->post_data_bits = 0;
}


void invert_data(struct ir_remote* remote)
{
	struct ir_ncode* codes;
	ir_code mask;
	lirc_t p, s;
	struct ir_code_node* n;

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

	if (remote->bits == 0)
		return;

	/* invert codes */
	mask = gen_mask(remote->bits);
	for (codes = remote->codes; codes->name != NULL; codes++) {
		codes->code ^= mask;
		for (n = codes->next; n != NULL; n = n->next)
			n->code ^= mask;
	}
}


void remove_trail(struct ir_remote* remote)
{
	int extra_bit;

	if (!is_space_enc(remote))
		return;
	if (remote->ptrail == 0)
		return;
	if (expect(remote, remote->pone, remote->pzero)
	    || expect(remote, remote->pzero, remote->pone))
		return;
	if (!(expect(remote, remote->sone, remote->szero)
	      && expect(remote, remote->szero, remote->sone)))
		return;
	if (expect(remote, remote->ptrail, remote->pone))
		extra_bit = 1;
	else if (expect(remote, remote->ptrail, remote->pzero))
		extra_bit = 0;
	else
		return;

	remote->post_data_bits++;
	remote->post_data <<= 1;
	remote->post_data |= extra_bit;
	remote->ptrail = 0;
}


void for_each_remote(struct ir_remote* remotes, remote_func func)
{
	struct ir_remote* remote;

	remote = remotes;
	while (remote != NULL) {
		func(remote);
		remote = remote->next;
	}
}


static int mywaitfordata(__u32 maxusec)
{
	int ret;
	struct pollfd pfd  = {
		.fd = curr_driver->fd, .events = POLLIN, .revents = 0};

	do {
		ret = curl_poll(&pfd, 1, maxusec / 1000);
	} while (ret == -1 && errno == EINTR);

	if (ret == -1 && errno != EINTR)
		log_perror_err("mywaitfordata: curl_poll() failed");
	return (pfd.revents & POLLIN) != 0;
}


static lirc_t emulation_readdata(lirc_t timeout)
{
	static lirc_t sum = 0;
	lirc_t data = 0;

	if (current_code == NULL) {
		data = 1000000;
		if (next_code)
			current_code = next_code;
		else
			current_code = emulation_data->codes;
		current_rep = 0;
		sum = 0;
	} else {
		if (current_code->name == NULL) {
			log_warn("%s: no data found", emulation_data->name);
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
				if (is_const(emulation_data))
					data = emulation_data->gap - sum;
				else
					data = emulation_data->gap;
			}

			sum = 0;
		} else {
			data = current_code->signals[current_index];
			if ((current_index % 2) == 0)
				data |= PULSE_BIT;
			current_index++;
			sum += data & PULSE_MASK;
		}
	}
	log_debug("delivering: %c%u\n",
		  data & PULSE_BIT ? 'p':'s', data & PULSE_MASK);
	return data;
}


static struct lengths* new_length(lirc_t length)
{
	struct lengths* l;

	l = malloc(sizeof(struct lengths));
	if (l == NULL)
		return NULL;
	l->count = 1;
	l->sum = length;
	l->lower_bound = length / 100 * 100;
	l->upper_bound = length / 100 * 100 + 99;
	l->min = l->max = length;
	l->next = NULL;
	return l;
}


void unlink_length(struct lengths** first, struct lengths* remove)
{
	struct lengths* last;
	struct lengths* scan;

	if (remove == *first) {
		*first = remove->next;
		remove->next = NULL;
		return;
	}
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
	log_error("unlink_length(): report this bug!");
}


int add_length(struct lengths** first, lirc_t length)
{
	struct lengths* l;
	struct lengths* last;

	if (*first == NULL) {
		*first = new_length(length);
		if (*first == NULL)
			return 0;
		return 1;
	}
	l = *first;
	while (l != NULL) {
		if (l->lower_bound <= length && length <= l->upper_bound) {
			l->count++;
			l->sum += length;
			l->min = min(l->min, length);
			l->max = max(l->max, length);
			return 1;
		}
		last = l;
		l = l->next;
	}
	last->next = new_length(length);
	if (last->next == NULL)
		return 0;
	return 1;
}


void free_lengths(struct lengths** firstp)
{
	struct lengths* first;
	struct lengths* next;

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


void free_all_lengths(void)
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


static void merge_lengths(struct lengths* first)
{
	struct lengths* l;
	struct lengths* inner;
	struct lengths* last;
	__u32 new_sum;
	int new_count;

	l = first;
	while (l != NULL) {
		last = l;
		inner = l->next;
		while (inner != NULL) {
			new_sum = l->sum + inner->sum;
			new_count = l->count + inner->count;

			if ((l->max <= new_sum / new_count + aeps
			     && l->min + aeps >= new_sum / new_count
			     && inner->max <= new_sum / new_count + aeps
			     && inner->min + aeps >= new_sum / new_count)
			    || (l->max <= new_sum / new_count * (100 + eps)
				&& l->min >= new_sum / new_count * (100 - eps)
				&& inner->max <= new_sum / new_count *
				(100 + eps)
				&& inner->min >= new_sum / new_count *
				(100 - eps))) {
				l->sum = new_sum;
				l->count = new_count;
				l->upper_bound = max(l->upper_bound,
						     inner->upper_bound);
				l->lower_bound = min(l->lower_bound,
						     inner->lower_bound);
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
	for (l = first; l != NULL; l = l->next) {
		log_debug("%d x %u [%u,%u]",
			  l->count, (__u32)calc_signal(l),
			  (__u32)l->min, (__u32)l->max);
	}
}


/**
 *  Scan the list first for the item with highest count value.
 *  Returns pointer to highest item. Also updates *sump
 *  (if non-null) to the sum of all count values in list.
 */
static struct lengths* get_max_length(struct lengths*	first,
				      unsigned int*	sump)
{
	unsigned int sum;
	struct lengths* scan;
	struct lengths* max_length;

	if (first == NULL)
		return NULL;
	max_length = first;
	sum = first->count;

	if (first->count > 0)
		log_debug("%u x %u", first->count,
			  (__u32)calc_signal(first));
	scan = first->next;
	while (scan) {
		if (scan->count > max_length->count)
			max_length = scan;
		sum += scan->count;
		log_debug(
			  "%u x %u",
			  scan->count,
			  (__u32)calc_signal(scan));
		scan = scan->next;
	}
	if (sump != NULL)
		*sump = sum;
	return max_length;
}


int get_trail_length(struct ir_remote* remote, int interactive)
{
	unsigned int sum = 0, max_count;
	struct lengths* max_length;

	if (is_biphase(remote))
		return 1;

	max_length = get_max_length(first_trail, &sum);
	max_count = max_length->count;
	log_debug(
		  "get_trail_length(): sum: %u, max_count %u",
		  sum, max_count);
	if (max_count >= sum * TH_TRAIL / 100) {
		log_debug("Found trail pulse: %lu",
			  (__u32)calc_signal(max_length));
		remote->ptrail = calc_signal(max_length);
		return 1;
	}
	log_debug("No trail pulse found.");
	return 1;
}


int get_lead_length(struct ir_remote* remote, int interactive)
{
	unsigned int sum = 0, max_count;
	struct lengths* first_lead;
	struct lengths* max_length;
	struct lengths* max2_length;
	lirc_t a, b, swap;

	if (!is_biphase(remote) || has_header(remote))
		return 1;
	if (is_rc6(remote))
		return 1;

	first_lead = has_header(remote) ? first_3lead : first_1lead;
	max_length = get_max_length(first_lead, &sum);
	max_count = max_length->count;
	log_debug(
		  "get_lead_length(): sum: %u, max_count %u",
		  sum, max_count);
	if (max_count >= sum * TH_LEAD / 100) {
		log_debug(
			  "Found lead pulse: %lu",
			  (__u32)calc_signal(max_length));
		remote->plead = calc_signal(max_length);
		return 1;
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
		log_debug(
			  "Found hidden lead pulse: %lu",
			  (__u32)a);
		remote->plead = a;
		return 1;
	}
	log_debug("No lead pulse found.");
	return 1;
}


int get_header_length(struct ir_remote* remote, int interactive)
{
	unsigned int sum, max_count;
	lirc_t headerp, headers;
	struct lengths* max_plength;
	struct lengths* max_slength;

	if (first_headerp != NULL) {
		max_plength = get_max_length(first_headerp, &sum);
		max_count = max_plength->count;
	} else {
		log_debug("No header data.");
		return 1;
	}
	log_debug(
		  "get_header_length(): sum: %u, max_count %u",
		  sum, max_count);

	if (max_count >= sum * TH_HEADER / 100) {
		max_slength = get_max_length(first_headers, &sum);
		max_count = max_slength->count;
		log_debug(
			  "get_header_length(): sum: %u, max_count %u",
			  sum, max_count);
		if (max_count >= sum * TH_HEADER / 100) {
			headerp = calc_signal(max_plength);
			headers = calc_signal(max_slength);

			log_debug(
				  "Found possible header: %lu %lu",
				  (__u32)headerp,
				  (__u32)headers);
			remote->phead = headerp;
			remote->shead = headers;
			if (first_lengths < second_lengths) {
				log_debug(
					  "Header is not being repeated.");
				remote->flags |= NO_HEAD_REP;
			}
			return 1;
		}
	}
	log_debug("No header found.");
	return 1;
}


int get_repeat_length(struct ir_remote* remote, int interactive)
{
	unsigned int sum = 0, max_count;
	lirc_t repeatp, repeats, repeat_gap;
	struct lengths* max_plength;
	struct lengths* max_slength;

	if (!((count_3repeats > SAMPLES / 2 ?  1 : 0) ^
	      (count_5repeats > SAMPLES / 2 ? 1 : 0))) {
		if (count_3repeats > SAMPLES / 2
		    || count_5repeats > SAMPLES / 2) {
			log_warn("Repeat inconsistency.");
			return 0;
		}
		log_debug("No repeat code found.");
		return 1;
	}

	max_plength = get_max_length(first_repeatp, &sum);
	max_count = max_plength->count;
	log_debug(
		  "get_repeat_length(): sum: %u, max_count %u",
		  sum, max_count);
	if (max_count >= sum * TH_REPEAT / 100) {
		max_slength = get_max_length(first_repeats, &sum);
		max_count = max_slength->count;
		log_debug(
			  "get_repeat_length(): sum: %u, max_count %u",
			  sum, max_count);
		if (max_count >= sum * TH_REPEAT / 100) {
			if (count_5repeats > count_3repeats
			    && !has_header(remote)) {
				log_warn(
					  "Repeat code has header,"
					  " but no header found!");
				return 0;
			}
			if (count_5repeats > count_3repeats
			    && has_header(remote))
				remote->flags |= REPEAT_HEADER;
			repeatp = calc_signal(max_plength);
			repeats = calc_signal(max_slength);

			log_debug(
				  "Found repeat code: %lu %lu",
				  (__u32)repeatp,
				  (__u32)repeats);
			remote->prepeat = repeatp;
			remote->srepeat = repeats;
			if (!(remote->flags & CONST_LENGTH)) {
				max_slength = get_max_length(first_repeat_gap,
							     NULL);
				repeat_gap = calc_signal(max_slength);
				log_debug(
					  "Found repeat gap: %lu",
					  (__u32)repeat_gap);
				remote->repeat_gap = repeat_gap;
			}
			return 1;
		}
	}
	log_debug("No repeat header found.");
	return 1;
}


void get_scheme(struct ir_remote* remote, int interactive)
{
	unsigned int i, length = 0, sum = 0;
	struct lengths* maxp;
	struct lengths* max2p;
	struct lengths* maxs;
	struct lengths* max2s;

	for (i = 1; i < MAX_SIGNALS; i++) {
		if (lengths[i] > lengths[length])
			length = i;
		sum += lengths[i];
		if (lengths[i] > 0)
			log_debug("%u: %u", i, lengths[i]);
	}
	log_debug("get_scheme(): sum: %u length: %u signals: %u"
		  " first_lengths: %u second_lengths: %u\n",
		  sum, length + 1, lengths[length],
		  first_lengths, second_lengths);
	/* FIXME !!! this heuristic is too bad */
	if (lengths[length] >= TH_SPACE_ENC * sum / 100) {
		length++;
		log_debug(
			  "Space/pulse encoded remote control found.");
		log_debug("Signal length is %u.", length);
		/* this is not yet the number of bits */
		remote->bits = length;
		set_protocol(remote, SPACE_ENC);
		return;
	}
	maxp = get_max_length(first_pulse, NULL);
	unlink_length(&first_pulse, maxp);
	if (first_pulse == NULL)
		first_pulse = maxp;
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
		    && (calc_signal(maxp) < TH_RC6_SIGNAL
			|| calc_signal(max2p) < TH_RC6_SIGNAL)
		    && (calc_signal(maxs) < TH_RC6_SIGNAL
			|| calc_signal(max2s) < TH_RC6_SIGNAL)) {
			log_debug("RC-6 remote control found.");
			set_protocol(remote, RC6);
		} else {
			log_debug("RC-5 remote control found.");
			set_protocol(remote, RC5);
		}
		return;
	}
	length++;
	log_debug("Suspicious data length: %u.", length);
	/* this is not yet the number of bits */
	remote->bits = length;
	set_protocol(remote, SPACE_ENC);
}


int get_data_length(struct ir_remote* remote, int interactive)
{
	unsigned int sum = 0, max_count;
	lirc_t p1, p2, s1, s2;
	struct lengths* max_plength;
	struct lengths* max_slength;
	struct lengths* max2_plength;
	struct lengths* max2_slength;

	max_plength = get_max_length(first_pulse, &sum);
	max_count = max_plength->count;
	log_debug("get_data_length(): sum: %u, max_count %u",
		  sum, max_count);

	if (max_count >= sum * TH_IS_BIT / 100) {
		unlink_length(&first_pulse, max_plength);

		max2_plength = get_max_length(first_pulse, NULL);
		if (max2_plength != NULL)
			if (max2_plength->count < max_count * TH_IS_BIT / 100)
				max2_plength = NULL;
		log_debug("Pulse candidates: ");
		log_debug("%u x %u", max_plength->count,
			  (__u32)calc_signal(max_plength));
		if (max2_plength)
			log_debug(", %u x %u",
				  max2_plength->count,
				  (__u32)calc_signal(max2_plength));

		max_slength = get_max_length(first_space, &sum);
		max_count = max_slength->count;
		log_debug(
			  "get_data_length(): sum: %u, max_count %u",
			  sum, max_count);
		if (max_count >= sum * TH_IS_BIT / 100) {
			unlink_length(&first_space, max_slength);

			max2_slength = get_max_length(first_space, NULL);
			if (max2_slength != NULL)
				if (max2_slength->count <
				    max_count * TH_IS_BIT / 100)
					max2_slength = NULL;
			if (max_count >= sum * TH_IS_BIT / 100) {
				log_debug("Space candidates: ");
				log_debug(
					  "%u x %u",
					  max_slength->count,
					  (__u32)calc_signal(max_slength));
				if (max2_slength) {
					log_debug(
						"%u x %u",
						max2_slength->count,
						(__u32)calc_signal(max2_slength));
				}
			}
			remote->eps = eps;
			remote->aeps = aeps;
			if (is_biphase(remote)) {
				if (max2_plength == NULL
				    || max2_slength == NULL) {
					log_notice(
						  "Unknown encoding found.");
					return 0;
				}
				log_debug(
					  "Signals are biphase encoded.");
				p1 = calc_signal(max_plength);
				p2 = calc_signal(max2_plength);
				s1 = calc_signal(max_slength);
				s2 = calc_signal(max2_slength);

				remote->pone =
					(min(p1, p2) + max(p1, p2) / 2) / 2;
				remote->sone =
					(min(s1, s2) + max(s1, s2) / 2) / 2;
				remote->pzero = remote->pone;
				remote->szero = remote->sone;
			} else {
				if (max2_plength == NULL
				    && max2_slength == NULL) {
					log_notice(
						  "No encoding found");
					return 0;
				}
				if (max2_plength && max2_slength) {
					log_notice(
						  "Unknown encoding found.");
					return 0;
				}
				p1 = calc_signal(max_plength);
				s1 = calc_signal(max_slength);
				if (max2_plength) {
					p2 = calc_signal(max2_plength);
					log_debug("Signals are pulse encoded.");
					remote->pone = max(p1, p2);
					remote->sone = s1;
					remote->pzero = min(p1, p2);
					remote->szero = s1;
					if (expect(remote, remote->ptrail, p1)
					    || expect(remote, remote->ptrail,
						      p2))
						remote->ptrail = 0;
				} else {
					s2 = calc_signal(max2_slength);
					log_debug("Signals are space encoded.");
					remote->pone = p1;
					remote->sone = max(s1, s2);
					remote->pzero = p1;
					remote->szero = min(s1, s2);
				}
			}
			if (has_header(remote)
			    && (!has_repeat(remote)
				|| remote->flags & NO_HEAD_REP)) {
				if (!is_biphase(remote)
				    && ((expect(remote, remote->phead,
						remote->pone)
					 && expect(remote,
						   remote->shead,
						   remote->sone))
					|| (expect(remote,
						   remote->phead,
						   remote->pzero)
					    && expect(remote,
						      remote->shead,
						      remote->szero)))) {
					remote->phead = remote->shead = 0;
					remote->flags &= ~NO_HEAD_REP;
					log_debug(
						  "Removed header.");
				}
				if (is_biphase(remote)
				    && expect(remote,
					      remote->shead,
					      remote->sone)) {
					remote->plead = remote->phead;
					remote->phead = remote->shead = 0;
					remote->flags &= ~NO_HEAD_REP;
					log_debug(
						  "Removed header.");
				}
			}
			if (is_biphase(remote)) {
				struct lengths* signal_length;
				lirc_t data_length;

				signal_length =
					get_max_length(first_signal_length,
						       NULL);
				data_length =
					calc_signal(signal_length) -
					remote->plead - remote->phead -
					remote->shead +
					/* + 1/2 bit */
					(remote->pone + remote->sone) / 2;
				remote->bits = data_length / (remote->pone +
							      remote->sone);
				if (is_rc6(remote))
					remote->bits--;
			} else {
				remote->bits =
					(remote->bits -
					 (has_header(remote) ? 2 : 0) + 1 -
					 (remote->ptrail > 0 ? 2 : 0)) / 2;
			}
			log_debug(
				  "Signal length is %d",
				  remote->bits);
			free_lengths(&max_plength);
			free_lengths(&max_slength);
			return 1;
		}
		free_lengths(&max_plength);
	}
	log_notice("Could not find data lengths.");
	return 0;
}


enum get_gap_status get_gap_length(struct gap_state*	state,
				   struct ir_remote*	remote)
{
	while (availabledata())
		curr_driver->rec_func(NULL);
	if (!mywaitfordata(10000000)) {
		free_lengths(&(state->gaps));
		return STS_GAP_TIMEOUT;
	}
	gettimeofday(&(state->start), NULL);
	while (availabledata())
		curr_driver->rec_func(NULL);
	gettimeofday(&(state->end), NULL);
	if (state->flag) {
		state->gap = time_elapsed(&(state->last), &(state->start));
		add_length(&(state->gaps), state->gap);
		merge_lengths(state->gaps);
		state->maxcount = 0;
		state->scan = state->gaps;
		while (state->scan) {
			state->maxcount = max(state->maxcount,
					      state->scan->count);
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

	if (!is_rc6(remote))
		return 0;
	if (remote->codes) {
		codes = remote->codes;
		while (codes->name != NULL) {
			if (codes->next)
				/* asume no toggle bit mask when key
				 * sequences are used */
				return 0;
			codes++;
		}
	}
	return 1;
}


/* Compute lengths from four recorded signals. */
static void compute_lengths_4_signals(void)
{
	add_length(&first_repeatp, signals[0]);
	merge_lengths(first_repeatp);
	add_length(&first_repeats, signals[1]);
	merge_lengths(first_repeats);
	add_length(&first_trail, signals[2]);
	merge_lengths(first_trail);
	add_length(&first_repeat_gap, signals[3]);
	merge_lengths(first_repeat_gap);
}


/* Compute lengths from six recorded signals. */
static void compute_lengths_6_signals(void)
{
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
}

/* Compute lengths from more than six recorded signals. */
static void compute_lengths_many_signals(struct lengths_state* state)
{
	int i;

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
	    || (first_length > 2
		&& first_length - 2 != state->count - 2)) {
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
	} else if (state->first_signal == 0
		   && first_length - 2 == state->count - 2) {
		lengths[state->count - 2]--;
		lengths[state->count - 2 + 2]++;
		second_lengths++;
	}
}


static struct lengths* scan_gap1(struct lengths_state*	state,
				 struct ir_remote*	remote,
				 int*			maxcount,
				 enum lengths_status*	again)
{
	struct lengths* scan;

	for (scan = first_sum; scan; scan = scan->next) {
		*maxcount = max(*maxcount, scan->count);
		if (scan->count > SAMPLES) {
			remote->gap = calc_signal(scan);
			remote->flags |= CONST_LENGTH;
			state->mode = MODE_HAVE_GAP;
			log_debug("Found gap: %u", remote->gap);
			*again = STS_LEN_AGAIN_INFO;
			break;
		}
	}
	return scan;
}


static struct lengths* scan_gap2(struct lengths_state*	state,
				 struct ir_remote*	remote,
				 int*			maxcount,
				 enum lengths_status*	again)
{
	struct lengths* scan;

	for (scan = first_gap; scan; scan = scan->next) {
		*maxcount = max(*maxcount, scan->count);
		if (scan->count > SAMPLES) {
			remote->gap = calc_signal(scan);
			state->mode = MODE_HAVE_GAP;
			log_debug("Found gap: %u", remote->gap);
			*again = STS_LEN_AGAIN_INFO;
			break;
		}
	}
	return scan;
}


enum lengths_status get_lengths(struct lengths_state* state,
				struct ir_remote* remote,
				int force, int interactive)
{
	struct lengths* scan;
	int maxcount = 0;
	static int lastmaxcount = 0;
	enum lengths_status again = STS_LEN_AGAIN;

	state->data = curr_driver->readdata(10000000);
	if (!state->data) {
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
				&& state->data > 5 * state->maxspace / 2)) {
				add_length(&first_sum, state->sum);
				merge_lengths(first_sum);
				add_length(&first_gap, state->data);
				merge_lengths(first_gap);
				state->sum = 0;
				count_spaces = 0;
				state->average = 0;
				state->maxspace = 0;

				maxcount = 0;
				scan = scan_gap1(state,
						 remote,
						 &maxcount,
						 &again);
				if (scan == NULL) {
					scan = scan_gap2(state,
							 remote,
							 &maxcount,
							 &again);
				}
				if (scan != NULL) {
					state->mode = MODE_HAVE_GAP;
					state->sum = 0;
					state->count = 0;
					state->remaining_gap =
						is_const(remote) ?
						(remote->gap >
						 state->data ?
						 remote->gap - state->data : 0)
						: (has_repeat_gap(remote) ?
						   remote->
						   repeat_gap : remote->gap);
					if (force) {
						state->retval = 0;
						return STS_LEN_RAW_OK;
					}
					return STS_LEN_AGAIN_INFO;
				}
				lastmaxcount = maxcount;
				state->keypresses = lastmaxcount;
				return again;
			}
			state->average =
				(state->average * count_spaces + state->data)
				/ (count_spaces + 1);
			count_spaces++;
			if (state->data > state->maxspace)
				state->maxspace = state->data;
		}
		if (state->count > SAMPLES * MAX_SIGNALS * 2) {
			state->retval = 0;
			return STS_LEN_NO_GAP_FOUND;
		}
		state->keypresses = lastmaxcount;
		return STS_LEN_AGAIN;
	} else if (state->mode == MODE_HAVE_GAP) {
		if (state->count <= MAX_SIGNALS) {
			signals[state->count - 1] = state->data & PULSE_MASK;
		} else {
			state->retval = 0;
			return STS_LEN_TOO_LONG;
		}
		if (is_const(remote))
			state->remaining_gap =
				remote->gap > state->sum ?
					remote->gap - state->sum : 0;
		else
			state->remaining_gap = remote->gap;
		state->sum += state->data & PULSE_MASK;

		if (state->count > 2
			&& ((state->data & PULSE_MASK) >=
				state->remaining_gap * (100 - eps) / 100
			    || (state->data & PULSE_MASK) >=
				state->remaining_gap - aeps)) {
			if (is_space(state->data)) {
				/* signal complete */
				state->keypresses += 1;
				if (state->count == 4) {
					count_3repeats++;
					compute_lengths_4_signals();
				} else if (state->count == 6) {
					count_5repeats++;
					compute_lengths_6_signals();
				} else if (state->count > 6) {
					count_signals++;
					compute_lengths_many_signals(state);
				}
				state->count = 0;
				state->sum = 0;
			}
			/* such long pulses may appear with
			 * crappy hardware (receiver? / remote?)
			 */
			else {
				remote->gap = 0;
				return STS_LEN_NO_GAP_FOUND;
			}

			if (count_signals >= SAMPLES) {
				get_scheme(remote, interactive);
				if (!get_header_length(remote, interactive)
				    || !get_trail_length(remote, interactive)
				    || !get_lead_length(remote, interactive)
				    || !get_repeat_length(remote, interactive)
				    || !get_data_length(remote, interactive))
					state->retval = 0;
				return state->retval ==
				       0 ? STS_LEN_FAIL : STS_LEN_OK;
			}
			if ((state->data & PULSE_MASK) <=
			    (state->remaining_gap + state->header) *
			    (100 + eps) / 100
			    || (state->data & PULSE_MASK) <=
			    (state->remaining_gap + state->header) + aeps) {
				state->first_signal = 0;
				state->header = 0;
			} else {
				state->first_signal = 1;
			}
		}
	}
	return STS_LEN_AGAIN;
}


enum toggle_status get_toggle_bit_mask(struct toggle_state*	state,
				       struct ir_remote*	remote)
{
	struct decode_ctx_t decode_ctx = { 0 };
	int i;
	ir_code mask;

	if (!state->inited) {
		sleep(1);
		while (availabledata())
			curr_driver->rec_func(NULL);
		state->inited = 1;
	}
	if (state->retries <= 0) {
		if (!state->found)
			return STS_TGL_NOT_FOUND;
		if (state->seq > 0) {
			remote->min_repeat = state->repeats / state->seq;
			log_debug("min_repeat=%d",
				  remote->min_repeat);
		}
		return STS_TGL_FOUND;
	}
	if (!mywaitfordata(10000000))
		return STS_TGL_TIMEOUT;
	curr_driver->rec_func(remote);
	if (is_rc6(remote) && remote->rc6_mask == 0) {
		for (i = 0, mask = 1; i < remote->bits; i++, mask <<= 1) {
			remote->rc6_mask = mask;
			state->success =
				curr_driver->decode_func(remote, &decode_ctx);
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
			curr_driver->decode_func(remote, &decode_ctx);
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
			mask = state->first ^ decode_ctx.code;
			if (!state->found && mask) {
				set_toggle_bit_mask(remote, mask);
				state->found = 1;
				if (state->seq > 0)
					remote->min_repeat =
						state->repeats / state->seq;
			}
			state->retries--;
			state->last = decode_ctx.code;
			return STS_TGL_GOT_ONE_PRESS;
		}
		state->repeats++;
		state->last = decode_ctx.code;
	} else {
		state->retries--;
		while (availabledata())
			curr_driver->rec_func(NULL);
	}
	return STS_TGL_AGAIN;
}


/** analyse non-interactive get_lengths, returns boolean ok/fail. */
int analyse_get_lengths(struct lengths_state* lengths_state)
{
	enum lengths_status status = STS_LEN_AGAIN;

	while (status == STS_LEN_AGAIN) {
		status = get_lengths(lengths_state, &remote, 0, 0);
		switch (status) {
		case STS_LEN_AGAIN_INFO:
			status = STS_LEN_AGAIN;
			break;
		case STS_LEN_AGAIN:
			break;
		case STS_LEN_OK:
			break;
		case STS_LEN_FAIL:
			log_error("get_lengths() failure");
			return 0;
		case STS_LEN_RAW_OK:
			log_error("raw analyse result?!");
			return 0;
		case STS_LEN_TIMEOUT:
			log_error("analyse timeout?!");
			return 0;
		case STS_LEN_NO_GAP_FOUND:
			log_error("analyse, no gap?!");
			return 0;
		case STS_LEN_TOO_LONG:
			log_error("analyse, signal too long?!");
			return 0;
		default:
			log_error("Cannot read raw data (%d)",
				  status);
			return 0;
		}
	}
	return 1;
}


/** Implement the analyse task, return 1 for ok, 0 for errors. */
int analyse_remote(struct ir_remote* raw_data, const struct opts* opts)
{
	struct ir_ncode* codes;
	struct decode_ctx_t decode_ctx;
	struct lengths_state lengths_state;
	int code;
	int code2;
	struct ir_ncode* new_codes;
	size_t new_codes_count = 100;
	int new_index = 0;
	int ret;

	if (!is_raw(raw_data)) {
		log_error("remote %s not in raw mode, ignoring",
			  raw_data->name);
		return 0;
	}
	flushhw();
	aeps = raw_data->aeps;
	eps = raw_data->eps;
	emulation_data = raw_data;
	next_code = NULL;
	current_code = NULL;
	current_index = 0;
	memset(&remote, 0, sizeof(remote));
	lengths_state_init(&lengths_state);
	if (!analyse_get_lengths(&lengths_state))
		return 0;

	if (is_rc6(&remote) && remote.bits >= 5)
		/* have to assume something as it's very difficult to
		 * extract the rc6_mask from the data that we have */
		remote.rc6_mask = ((ir_code)0x1ll) << (remote.bits - 5);

	remote.name = raw_data->name;
	remote.freq = raw_data->freq;

	new_codes = malloc(new_codes_count * sizeof(*new_codes));
	if (new_codes == NULL) {
		log_error("Out of memory");
		return 0;
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
			log_warn(
				  "Decoding of %s failed", codes->name);
		} else {
			if (new_index + 1 >= new_codes_count) {
				struct ir_ncode* renew_codes;

				new_codes_count *= 2;
				renew_codes =
					realloc(new_codes,
						new_codes_count *
						sizeof(*new_codes));
				if (renew_codes == NULL) {
					log_error("Out of memory");
					free(new_codes);
					return 0;
				}
				memset(&new_codes[new_codes_count / 2],
				       0,
				       new_codes_count / 2 *
				       sizeof(*new_codes));
				new_codes = renew_codes;
			}

			rec_buffer_clear();
			code = decode_ctx.code;
			ret = receive_decode(&remote, &decode_ctx);
			code2 = decode_ctx.code;
			decode_ctx.code = code;
			if (ret && code2 != decode_ctx.code) {
				new_codes[new_index].next =
					malloc(IR_CODE_NODE_SIZE);
				if (new_codes[new_index].next) {
					memset(new_codes[new_index].next,
					       0,
					       IR_CODE_NODE_SIZE);
					new_codes[new_index].next->code =
						code2;
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
	fprint_remotes(stdout, &remote, opts->commandline);
	remote.codes = NULL;
	free(new_codes);
	return 1;
}


/** The --analyse wrapper. */
int do_analyse(const struct opts* opts, struct main_state* state)
{
	FILE* f;
	struct ir_remote* r;

	memcpy((void*)curr_driver, &hw_emulation, sizeof(struct driver));
	f = fopen(opts->filename, "r");
	if (f == NULL) {
		fprintf(stderr, "Cannot open file: %s\n", opts->filename);
		return 0;
	}
	r = read_config(f, opts->filename);
	if (r == NULL) {
		fprintf(stderr, "Cannot parse file: %s\n", opts->filename);
		return 0;
	}
	for (; r != NULL; r = r->next) {
		if (!is_raw(r)) {
			log_error("remote %s not in raw mode, ignoring",
				  r->name);
			continue;
		}
		analyse_remote(r, opts);
	}
	return 1;
}


ssize_t raw_read(void* buffer, size_t size, unsigned int timeout_us)
{
	if (!mywaitfordata(timeout_us))
		return 0;
	return read(curr_driver->fd, buffer, size);
}


static int raw_data_ok(struct button_state* btn_state)
{
	int r;
	int ref;

	if (!is_space(btn_state->data)) {
		r = 0;
	} else if (is_const(&remote)) {
		if (remote.gap > btn_state->sum) {
			ref = (remote.gap - btn_state->sum);
			ref *= (100 - remote.eps);
			ref /= 100;
		} else {
			ref = 0;
		}
		r = btn_state->data > ref;
	} else {
		r = btn_state->data > (remote.gap * (100 - remote.eps)) / 100;
	}
	return r;
}


enum button_status record_buttons(struct button_state*	btn_state,
				  enum button_status	last_status,
				  struct main_state*	state,
				  const struct opts*	opts)
{
	const char* const MSG_BAD_LENGTH =
		"Signal length is %d\n"
		"That's weird because the signal length must be odd!\n";
	ir_code code2;
	int decode_ok;
	__u32 timeout;
	int retries;
	struct ir_remote* my_remote;
	FILE* f;
	enum button_status sts;

	if (btn_state->no_data) {
		btn_state->no_data = 0;
		return STS_BTN_TIMEOUT;
	}
	switch (last_status) {
	case STS_BTN_INIT:
		return STS_BTN_GET_NAME;
	case STS_BTN_GET_NAME:
		if (strchr(btn_state->buffer, ' ') != NULL) {
			btn_state_set_message(
				btn_state,
				"The name must not contain any whitespace.");
			return STS_BTN_SOFT_ERROR;
		}
		if (strchr(btn_state->buffer, '\t') != NULL) {
			btn_state_set_message(
				btn_state,
				"The name must not contain any whitespace.");
			return STS_BTN_SOFT_ERROR;
		}
		if (strcasecmp(btn_state->buffer, "begin") == 0) {
			btn_state_set_message(
				btn_state,
				"'%s' is not allowed as button name\n",
				btn_state->buffer);
			return STS_BTN_SOFT_ERROR;
		}
		if (strcasecmp(btn_state->buffer, "end") == 0) {
			btn_state_set_message(
				btn_state,
				"'%s' is not allowed as button name\n",
				btn_state->buffer);
			return STS_BTN_SOFT_ERROR;
		}
		if (strlen(btn_state->buffer) == 0)
			return STS_BTN_RECORD_DONE;
		if (!opts->disable_namespace
		    && !is_in_namespace(btn_state->buffer)) {
			btn_state_set_message(
				btn_state,
				"'%s' is not in name space"
				" (use --disable-namespace to override)\n",
				btn_state->buffer);
			return STS_BTN_SOFT_ERROR;
		}
		return STS_BTN_INIT_DATA;
	case STS_BTN_INIT_DATA:
		if (opts->force)
			flushhw();
		else
			while (availabledata())
				curr_driver->rec_func(NULL);
		if (curr_driver->fd == -1)
			curr_driver->init_func();
		return opts->force ? STS_BTN_GET_RAW_DATA : STS_BTN_GET_DATA;
	case STS_BTN_GET_DATA:
		for (retries = RETRIES; retries > 0; ) {
			if (!mywaitfordata(10000000)) {
				btn_state->no_data = 1;
				return STS_BTN_TIMEOUT;
			}
			decode_ok = 0;
			last_remote = NULL;
			sleep(1);
			while (availabledata()) {
				curr_driver->rec_func(NULL);
				if (curr_driver->decode_func(
					    &remote,
					    &(state->decode_ctx))) {
					decode_ok = 1;
					break;
				}
			}
			if (!decode_ok) {
				if (!resethw(btn_state->started_as_root)) {
					btn_state_set_message(
						btn_state,
						"Could not reset hardware.\n");
					return STS_BTN_HARD_ERROR;
				}
				btn_state_set_message(btn_state,
						      "Cannot decode data\n");
				flushhw();
				return STS_BTN_SOFT_ERROR;
			}
			btn_state->ncode.name = btn_state->buffer;
			btn_state->ncode.code = state->decode_ctx.code;
			curr_driver->rec_func(NULL);
			if (!curr_driver->decode_func(&remote,
						      &(state->decode_ctx))) {
				code2 = state->decode_ctx.code;
				state->decode_ctx.code = btn_state->ncode.code;
				if (state->decode_ctx.code != code2) {
					btn_state->ncode.next =
						malloc(IR_CODE_NODE_SIZE);
					if (btn_state->ncode.next) {
						memset(btn_state->ncode.next,
						       0,
						       IR_CODE_NODE_SIZE);
						btn_state->ncode.next->code =
							code2;
					}
				}
			}
			break;
		}
		return STS_BTN_BUTTON_DONE;
	case STS_BTN_GET_RAW_DATA:
		btn_state->count = 0;
		btn_state->sum = 0;
		while (btn_state->count < MAX_SIGNALS) {
			if (btn_state->count == 0)
				timeout = 10000000;
			else
				timeout = remote.gap * 5;
			btn_state->data = curr_driver->readdata(timeout);
			if (!btn_state->data) {
				if (btn_state->count == 0)
					return STS_BTN_TIMEOUT;
				btn_state->data = remote.gap;
			}
			if (btn_state->count == 0) {
				if (!is_space(btn_state->data)
				    || btn_state->data <
				    remote.gap - remote.gap * remote.eps /
				    100) {
					sleep(3);
					flushhw();
					btn_state->count = 0;
					btn_state_set_message(
						btn_state,
						"Something went wrong.");
					return STS_BTN_SOFT_ERROR;
				}
			} else {
				if (raw_data_ok(btn_state)) {
					log_info("Got it.\n");
					log_info("Signal length is %d\n",
						 btn_state->count - 1);
					if (btn_state->count % 2) {
						btn_state_set_message(
							btn_state,
							MSG_BAD_LENGTH,
							btn_state->count - 1);
						sleep(3);
						flushhw();
						btn_state->count = 0;
						return STS_BTN_SOFT_ERROR;
					}
					btn_state->ncode.name =
						btn_state->buffer;
					btn_state->ncode.length =
						btn_state->count - 1;
					btn_state->ncode.signals = signals;
					break;
				}
				signals[btn_state->count - 1] =
					btn_state->data & PULSE_MASK;
				btn_state->sum +=
					btn_state->data & PULSE_MASK;
			}
			btn_state->count++;
		}
		if (btn_state->count == MAX_SIGNALS) {
			btn_state_set_message(btn_state,
					      "Signal is too long.\n");
			return STS_BTN_SOFT_ERROR;
		}
		return STS_BTN_BUTTON_DONE;
	case STS_BTN_RECORD_DONE:
		if (is_raw(&remote))
			return STS_BTN_ALL_DONE;
		if (!resethw(btn_state->started_as_root)) {
			btn_state_set_message(btn_state,
					      "Could not reset hardware.");
			return STS_BTN_HARD_ERROR;
		}
		return STS_BTN_BUTTONS_DONE;
	case STS_BTN_BUTTONS_DONE:
		f = fopen(opts->tmpfile, "r");
		if (f == NULL) {
			btn_state_set_message(btn_state,
					      "Could not reopen config file");
			return STS_BTN_HARD_ERROR;
		}
		my_remote = read_config(f, opts->filename);
		fclose(f);
		if (my_remote == NULL) {
			btn_state_set_message(
				btn_state,
				"Internal error: "
				"config file contains no valid remote");
			return STS_BTN_HARD_ERROR;
		}
		if (my_remote == (void*)-1) {
			btn_state_set_message(
				btn_state,
				"Internal error: "
				"Reading of config file failed");
			return STS_BTN_HARD_ERROR;
		}
		sts = STS_BTN_ALL_DONE;
		if (opts->force) {
			remote = *my_remote;
			return sts;
		}
		if (!has_toggle_bit_mask(my_remote)) {
			if (!opts->using_template
			    && strcmp(curr_driver->name, "devinput") != 0) {
				remote = *(my_remote);
				sts = STS_BTN_GET_TOGGLE_BITS;
			}
		} else {
			set_toggle_bit_mask(my_remote,
					    my_remote->toggle_bit_mask);
			if (curr_driver->deinit_func)
				curr_driver->deinit_func();
		}
		if (!opts->update) {
			get_pre_data(my_remote);
			get_post_data(my_remote);
		}
		remote = *my_remote;
		return sts;
	case STS_BTN_BUTTON_DONE:
		return STS_BTN_BUTTON_DONE;
	case STS_BTN_HARD_ERROR:
		return STS_BTN_HARD_ERROR;
	default:
		btn_state_set_message(btn_state,
				      "record_buttons(): bad state: %d\n",
				      last_status);
		return STS_BTN_HARD_ERROR;
	}
}


/** Write the provisionary config file. */
void config_file_setup(struct main_state* state, const struct opts* opts)
{
	state->fout = fopen(opts->tmpfile, "w");
	if (state->fout == NULL) {
		log_error("Could not open new config file %s", tmpfile);
		log_perror_err("While opening temporary file for write");
		return;
	}
	fprint_copyright(state->fout);
	fprint_comment(state->fout, &remote, opts->commandline);
	fprint_remote_head(state->fout, &remote);
	fprint_remote_signal_head(state->fout, &remote);
}



/** Write the final config file. */
int config_file_finish(struct main_state* state, const struct opts* opts)
{
	state->fout = fopen(opts->filename, "w");
	if (state->fout == NULL) {
		log_perror_err("While opening \"%s\" for write",
			       opts->filename);
		return 0;
	}
	fprint_copyright(state->fout);
	fprint_remotes(state->fout, &remote, opts->commandline);
	return 1;
}
