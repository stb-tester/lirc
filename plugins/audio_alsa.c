/****************************************************************************
** hw_audio_alsa.c *********************************************************
****************************************************************************
*
* routines for using a IR receiver connected to soundcard ADC.
* Uses ALSA sound interface which is going to become standard
* in the 2.6 series of kernels. It does the same as ir_audio,
* but is linux-specific and does not require any exotic libraries
* for doing such simple work like recording an audio stream.
* Besides, its a lot more optimal since it uses 8kHz 8-bit
* mono sampling rather than 44KHz stereo 16-bit (a lot less CPU usage).
*
* Copyright (C) 2003 Andrew Zabolotny <andyz@users.sourceforge.net>
*
* Distribute under GPL version 2 or later.
*
* A detailed (:-) description of hardware can be found in the doc directory
* in the file ir-audio.html. Usage manual is in audio-alsa.html.
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>

#include "lirc_driver.h"

/* SHORT DRIVER DESCRIPTION:
 *
 * This driver implements an adaptive input analyzer that does not depend
 * of the input signal level, but for the sake of noise separation it is
 * desired the level of input signal to be relatively strong (without
 * clipping although it does not hurt).
 *
 * This driver works as following: it creates a named pipe (called usually
 * /dev/lirc) and opens it. The handle is handed then to the receive.c module
 * which reads or writes data from it. The client HAS to use non-blocking
 * I/O otherwise we don't get a chance to run ever (this could be fixed
 * by creating a secondary thread and doing all audio stuff there).
 * If one day this driver will support sending commands, we'll have to use
 * asyncronous I/O (O_ASYNC and a signal handler) so that we get control
 * right after client writes to the pipe.
 *
 * For usage documentation see audio-alsa.html.
 */

/* The following structure contains current sound card setup */
static struct {
	/* ALSA PCM handle */
	snd_pcm_t*		handle;
	/* Sampling rate */
	unsigned		rate;
	/* Data format */
	snd_pcm_format_t	format;
	/* The audio buffer size in microseconds */
	unsigned		buffer_time;
	/* The FIFO handle for reading and writing */
	int			fd;
	/* The asynchronous I/O signal handler object */
	snd_async_handler_t*	sighandler;
	/* Value indicating number of channels for capture */
	unsigned char		num_channels;
	/* Value indicating which channel to look for signal 0=right 1=left */
	unsigned char		channel;
} alsa_hw = {
	NULL,
	/* Desired sampling frequency */
	8000,
	/* Desired PCM format */
	SND_PCM_FORMAT_U8,
	/* Reserve buffer for 0.1 secs of sampled data.
	 * If we use a larger buffer, our routine is called too seldom,
	 * and record.c thinks there is a time gap between data (and drops
	 * the repeat count).
	 */
	100000,	-1, NULL, 1, 0 /*Use left channel by default */
};

/* Return the absolute difference between two unsigned 8-bit samples */
#define U8_ABSDIFF(s1, s2) (((s1) >= (s2)) ? ((s1) - (s2)) : ((s2) - (s1)))

/** Command to list available alsa hw devices. */
#define LIST_DEVICES \
" aplay -l | sed -n" \
" -e '/^card/s/card \\([0-9]\\)\\([^,]*\\),[^:]*\\([0-9]\\)/hw:\\1,\\3 /'" \
" -e 's/ : .*//p'"

/* Forward declarations */
static int audio_alsa_init(void);
static int audio_alsa_deinit(void);
static void alsa_sig_io(snd_async_handler_t* h);

static const logchannel_t logchannel = LOG_DRIVER;

static int mywaitfordata(long maxusec)
{
	int  ret, poll_ms;
	struct timeval start, now;

	while (1) {
		do {
			struct pollfd pfd  = {
				.fd = drv.fd, .events = POLLIN, .revents = 0
			};

			gettimeofday(&start, NULL);
			poll_ms = MAX(0, maxusec / 1000);
			if (drv.fd == -1) {
				/* try to reconnect */
				if (poll_ms > 1000 || poll_ms == 0)
					poll_ms = 1000;
			}
			ret = curl_poll(&pfd, 1, poll_ms == 0 ? -1 : poll_ms);
			if (ret == -1 && errno != EINTR) {
				log_perror_err("curl_poll() failed");
				continue;
			}
			gettimeofday(&now, NULL);
			if (maxusec > 0) {
				if (ret == 0 || time_elapsed(&start, &now) >= maxusec)
					return 0;
				maxusec -= time_elapsed(&start, &now);
			}
		} while (ret == -1 && errno == EINTR);

		if (drv.fd == -1) {
			loglevel_t saved_level = loglevel;

			lirc_log_setlevel(LIRC_NOLOG);
			audio_alsa_init();
			lirc_log_setlevel(saved_level);
		} else if (ret >= 1) {
			/* we will read later */
			return 1;
		}
	}
}

static int alsa_error(const char* errstr, int errcode)
{
	if (errcode < 0) {
		log_error("ALSA function snd_pcm_%s returned error: %s", errstr, snd_strerror(errcode));
		log_perror_err(errstr);
		return -1;
	}
	return 0;
}

static int alsa_set_hwparams(void)
{
	snd_pcm_hw_params_t* hwp;
	snd_pcm_sw_params_t* swp;
	int dir = 1;
	unsigned period_time;
	snd_pcm_uframes_t buffer_size, period_size;

	snd_pcm_hw_params_alloca(&hwp);
	snd_pcm_sw_params_alloca(&swp);

	// ALSA bug? If we request 44100 Hz, it rounds the value up to 48000...
	alsa_hw.rate--;

	if (alsa_error("hw_params_any", snd_pcm_hw_params_any(alsa_hw.handle, hwp))
	    || alsa_error("hw_params_set_format", snd_pcm_hw_params_set_format(alsa_hw.handle, hwp, alsa_hw.format))
	    || alsa_error("hw_params_set_channels",
			  snd_pcm_hw_params_set_channels(alsa_hw.handle, hwp, alsa_hw.num_channels))
	    || alsa_error("hw_params_set_rate_near",
			  snd_pcm_hw_params_set_rate_near(alsa_hw.handle, hwp, &alsa_hw.rate, &dir))
	    || alsa_error("hw_params_set_access",
			  snd_pcm_hw_params_set_access(alsa_hw.handle, hwp, SND_PCM_ACCESS_RW_INTERLEAVED))
	    || alsa_error("hw_params_set_buffer_time_near",
			  snd_pcm_hw_params_set_buffer_time_near(alsa_hw.handle, hwp, &alsa_hw.buffer_time, 0)))
		return -1;

	/* How often to call our SIGIO handler (~40Hz) */
	period_time = alsa_hw.buffer_time / 4;
	if (alsa_error
		    ("hw_params_set_period_time_near",
		    snd_pcm_hw_params_set_period_time_near(alsa_hw.handle, hwp, &period_time, &dir))
	    || alsa_error("hw_params_get_buffer_size", snd_pcm_hw_params_get_buffer_size(hwp, &buffer_size))
	    || alsa_error("hw_params_get_period_size", snd_pcm_hw_params_get_period_size(hwp, &period_size, 0))
	    || alsa_error("hw_params", snd_pcm_hw_params(alsa_hw.handle, hwp)))
		return -1;

	snd_pcm_sw_params_current(alsa_hw.handle, swp);
	if (alsa_error("sw_params_set_start_threshold",
			snd_pcm_sw_params_set_start_threshold(alsa_hw.handle,
							      swp,
							      period_size))
	    || alsa_error("sw_params_set_avail_min",
			  snd_pcm_sw_params_set_avail_min(alsa_hw.handle,
							  swp,
							  period_size))
	    || alsa_error("sw_params", snd_pcm_sw_params(alsa_hw.handle,
							 swp)))
		return -1;

	return 0;
}

static int audio_alsa_init(void)
{
	int fd, err;
	char* pcm_rate;
	char tmp_name[20];

	rec_buffer_init();

	/* Create a temporary filename for our FIFO,
	 * Use mkstemp() instead of mktemp() although we need a FIFO not a
	 * regular file. We do this since glibc barfs at mktemp() and this
	 * scares the users :-)
	 */
	strcpy(tmp_name, "/tmp/lircXXXXXX");
	fd = mkstemp(tmp_name);
	close(fd);

	/* Start the race! */
	unlink(tmp_name);
	if (mknod(tmp_name, S_IFIFO | S_IRUSR | S_IWUSR, 0)) {
		log_error("could not create FIFO %s", tmp_name);
		log_perror_err("audio_alsa_init ()");
		return 0;
	}
	/* Phew, we won the race ... */

	/* Open the pipe and hand it to LIRC ... */
	drv.fd = open(tmp_name, O_RDWR);
	if (drv.fd < 0) {
		log_error("could not open pipe %s", tmp_name);
		log_perror_err("audio_alsa_init ()");
error:          unlink(tmp_name);
		audio_alsa_deinit();
		return 0;
	}

	/* Open the other end of the pipe and hand it to ALSA code.
	 * We're opening it in non-blocking mode to avoid lockups.
	 */
	alsa_hw.fd = open(tmp_name, O_RDWR | O_NONBLOCK);
	/* Ok, we don't need the FIFO visible in the filesystem anymore ... */
	unlink(tmp_name);

	/* Examine the device name, if it contains a sample rate */
	strncpy(tmp_name, drv.device, sizeof(tmp_name) - 1);
	pcm_rate = strchr(tmp_name, '@');
	if (pcm_rate) {
		int rate;
		char* stereo_channel;

		/* Examine if we need to capture in stereo
		 * looking for an 'l' or 'r' character to indicate
		 * which channel to look at.*/
		stereo_channel = strchr(pcm_rate, ',');

		if (stereo_channel) {
			/* Syntax in device string indicates we need
			 * to use stereo */
			alsa_hw.num_channels = 2;
			/* As we are requesting stereo now, use the
			 * more common signed 16bit samples */
			alsa_hw.format = SND_PCM_FORMAT_S16_LE;

			if (stereo_channel[1] == 'l')
				alsa_hw.channel = 0;
			else if (stereo_channel[1] == 'r')
				alsa_hw.channel = 1;
			else
				log_warn("don't understand which channel to use - defaulting to left\n");
		}

		/* Remove the sample rate from device name (and
		 * channel indicator if present) */
		*pcm_rate++ = 0;
		/* See if rate is meaningful */
		rate = atoi(pcm_rate);
		if (rate > 0)
			alsa_hw.rate = rate;
	}

	/* Open the audio card in non-blocking mode */
	err = snd_pcm_open(&alsa_hw.handle, tmp_name, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
	if (err < 0) {
		log_error("could not open audio device %s: %s", drv.device, snd_strerror(err));
		log_perror_err("audio_alsa_init ()");
		goto error;
	}

	/* Set up the I/O signal handler */
	if (alsa_error("async_add_handler",
			 snd_async_add_pcm_handler(&alsa_hw.sighandler,
						   alsa_hw.handle,
						   alsa_sig_io, NULL)))
		goto error;

	/* Set sampling parameters */
	if (alsa_set_hwparams())
		goto error;

	log_trace("hw_audio_alsa: Using device '%s', sampling rate %dHz\n", tmp_name, alsa_hw.rate);

	/* Start sampling data */
	if (alsa_error("start", snd_pcm_start(alsa_hw.handle)))
		goto error;

	return 1;
}

int audio_alsa_deinit(void)
{
	if (alsa_hw.sighandler) {
		snd_async_del_handler(alsa_hw.sighandler);
		alsa_hw.sighandler = NULL;
	}
	if (alsa_hw.handle) {
		snd_pcm_close(alsa_hw.handle);
		alsa_hw.handle = NULL;
	}
	if (alsa_hw.fd != -1) {
		close(alsa_hw.fd);
		alsa_hw.fd = -1;
	}
	if (drv.fd != -1) {
		close(drv.fd);
		drv.fd = -1;
	}
	return 1;
}

/*
 * ALSA calls this callback when some data is available for reading.
 * The detection algorithm is somewhat sophisticated but it should give
 * good practical results. The algorithm works as follows:
 *
 * Sampled data is converted to unsigned form (e.g. 0x80 is zero).
 *
 * The current "middle" value is constantly tracked (e.g. signal
 * could deviate from the 0x80 by a certain amount due to soundcard
 * entry capacitance). Then we subtract that middle from every sample
 * to get a signed value (to know whether it is less or more than current
 * tracked "zero" value). This is called 'current sample'.
 *
 * The absolute value of current sample is integrated over time to get
 * automatic level correction (e.g. to smooth the difference between
 * different hardware which can have different output levels). This is
 * called 'signal level'.
 *
 * Then the algorithm waits for a substantial change in the level of
 * input signals (since IR module outputs a square wave). When this
 * substantial change crosses our "virtual zero", it is considered
 * a real level change, and the type of signal is toggled
 * (space <-> pulse).
 */

#define READ_BUFFER_SIZE (2 * 4096)

static void alsa_sig_io(snd_async_handler_t* h)
{
	/* Previous sample */
	static unsigned char ps = 0x80;
	/* Count samples with similar level (to detect pule/space
	 * length), 24.8 fp */
	static unsigned sample_count = 0;
	/* Current signal level (dynamically changes) */
	static unsigned signal_level = 0;
	/* Current state (pulse or space) */
	static unsigned signal_state = 0;
	/* Signal maximum and minimum (used for "zero" detection) */
	static unsigned char signal_max = 0x80, signal_min = 0x80;
	/* Non-zero if we're in zero crossing waiting state */
	static char waiting_zerox = 0;
	/* Store sample size, as our sample buffer will represent
	 * shorts or chars */
	unsigned char bytes_per_sample = (alsa_hw.format == SND_PCM_FORMAT_S16_LE ? 2 : 1);

	int i, err;
	char buff[READ_BUFFER_SIZE];
	snd_pcm_sframes_t count;

	/* The value to multiply with number of samples to get microseconds
	 * (fixed-point 24.8 bits).
	 */
	unsigned mulconst = 256000000 / alsa_hw.rate;
	/* Maximal number of samples that can be multiplied by mulconst */
	unsigned maxcount = (((PULSE_MASK << 8) | 0xff) / mulconst) << 8;

	/* First of all, check for underrun. This happens, for example, when
	 * the X11 server starts. If we won't, recording will stop forever.
	 */
	snd_pcm_state_t state = snd_pcm_state(alsa_hw.handle);

	switch (state) {
	case SND_PCM_STATE_SUSPENDED:
		while ((err = snd_pcm_resume(alsa_hw.handle)) == -EAGAIN)
			/* wait until the suspend flag is released */
			sleep(1);
		if (err >= 0)
			goto var_reset;
	/* Fallthrough */
	case SND_PCM_STATE_XRUN:
		alsa_error("prepare", snd_pcm_prepare(alsa_hw.handle));
		alsa_error("start", snd_pcm_start(alsa_hw.handle));
var_reset:                      /* Reset variables */
		sample_count = 0;
		waiting_zerox = 0;
		signal_level = 0;
		signal_state = 0;
		signal_max = signal_min = 0x80;
		break;
	default:
		/* Stream is okay */
		break;
	}

	/* Read all available data */
	count = snd_pcm_avail_update(alsa_hw.handle);
	if (count > 0) {
		if (count > (READ_BUFFER_SIZE / (bytes_per_sample * alsa_hw.num_channels)))
			count = READ_BUFFER_SIZE / (bytes_per_sample * alsa_hw.num_channels);
		count = snd_pcm_readi(alsa_hw.handle, buff, count);

		/*Loop around samples, if stereo we are
		 * only interested in one channel*/
		for (i = 0; i < count; i++) {
			/* cs == current sample */
			unsigned char cs, as, sl, sz, xz;
			unsigned short stmp;

			if (bytes_per_sample == 2) {
				int ix = i * bytes_per_sample * alsa_hw.num_channels +
					     bytes_per_sample * alsa_hw.channel;
				memcpy(&stmp, &buff[ix], sizeof(stmp));
				cs = stmp >> 8;
				cs ^= 0x80;
			} else {
				cs = buff[i];

				/* Convert signed samples to unsigned */
				if (alsa_hw.format == SND_PCM_FORMAT_S8)
					cs ^= 0x80;
			}

			/* Track signal middle value (it could differ from 0x80) */
			sz = (signal_min + signal_max) / 2;
			if (cs <= sz)
				signal_min = (signal_min * 7 + cs) / 8;
			if (cs >= sz)
				signal_max = (signal_max * 7 + cs) / 8;

			/* Compute the absolute signal deviation from middle */
			as = U8_ABSDIFF(cs, sz);

			/* Integrate incoming signal (auto level adjustment) */
			signal_level = (signal_level * 7 + as) / 8;

			/* Don't let too low signal levels as it makes us sensible to noise */
			sl = signal_level;
			if (sl < 16)
				sl = 16;

			/* Detect crossing current "zero" level */
			xz = ((cs - sz) ^ (ps - sz)) & 0x80;

			/* Don't wait for zero crossing for too long */
			if (waiting_zerox && !xz)
				waiting_zerox--;

			/* Detect significant signal level changes */
			if ((abs(cs - ps) > sl / 2) && xz)
				waiting_zerox = 2;

			/* If we have crossed zero with a substantial level change, go */
			if (waiting_zerox && xz) {
				lirc_t x;

				waiting_zerox = 0;

				if (sample_count >= maxcount) {
					x = PULSE_MASK;
					sample_count = 0;
				} else {
					/**
					 * Try to interpolate the samples and determine where exactly
					 * the zero crossing point was. This is required as the
					 * remote signal frequency is relatively close to our sampling
					 * frequency thus a sampling error of 1 sample can lead to
					 * substantial time differences.
					 *
					 *     slope = (x2 - x1) / (y2 - y1)
					 *     x = x1 + (y - y1) * slope
					 *
					 * where x1=-1, x2=0, y1=ps, y2=cs, y=sz, thus:
					 *
					 *     x = -1 + (y - y1) / (y2 - y1), or
					 * ==> x = (y - y2) / (y2 - y1)
					 *
					 * y2 (cs) cannot be equal to y1 (ps), otherwise we wouldn't
					 * get here.
					 */
					int delta = (((int)sz - (int)cs) << 8) / ((int)cs - (int)ps);
					/* This expression can easily overflow the 'long' value since it
					 * multiplies two 24.8 values (and we get a 24.16 instead).
					 * To avoid this we cast the intermediate value to "long long".
					 */
					x = (((long long)sample_count + delta) * mulconst) >> 16;
					/* The rest of the quantum is on behalf of next pulse. Note that
					 * sample_count can easily be assigned here a negative value (in
					 * the case zero crossing occurs during the next quantum).
					 */
					sample_count = -delta;
				}

				/* Consider impossible pulses with length greater than
				 * 0.02 seconds, thus it is a space (desynchronization).
				 */
				if ((x > 20000) && signal_state) {
					signal_state = 0;
					log_trace("Pulse/space desynchronization fixed - len %u", x);
				}

				x |= signal_state;

				/* Write the LIRC code to the FIFO */
				chk_write(alsa_hw.fd, &x, sizeof(x));

				signal_state ^= PULSE_BIT;
			}

			/* Remember previous sample */
			ps = cs;

			/* Count number of samples with the same level.
			 * sample_count can be less than zero at the start of pulse
			 * (due to interpolation) so we have to consider them.
			 */
			if ((sample_count < UINT_MAX - 0x400)
			    || (sample_count > UINT_MAX - 0x200))
				sample_count += 0x100;
		}
	}
}

lirc_t audio_alsa_readdata(lirc_t timeout)
{
	lirc_t data;
	int ret;

	if (!mywaitfordata((long)timeout))
		return 0;

	ret = read(drv.fd, &data, sizeof(data));

	if (ret != sizeof(data)) {
		log_perror_err("Error reading from lirc device");
		raise(SIGTERM);
		return 0;
	}
	return data;
}

char* audio_alsa_rec(struct ir_remote* remotes)
{
	if (!rec_buffer_clear())
		return NULL;
	return decode_all(remotes);
}

#define audio_alsa_decode receive_decode

const struct driver hw_audio_alsa = {
	.name		= "audio_alsa",
	.device		= "hw",
	.fd		= -1,
	.features	= LIRC_CAN_REC_MODE2,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_MODE2,
	.code_length	= 0,
	.init_func	= audio_alsa_init,
	.deinit_func	= audio_alsa_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= audio_alsa_rec,
	.decode_func	= audio_alsa_decode,
	.drvctl_func	= NULL,
	.readdata	= audio_alsa_readdata,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "See file://" PLUGINDOCS "/audio-alsa.html",
	.device_hint    = "/bin/sh " LIST_DEVICES,
};

const struct driver* hardwares[] = { &hw_audio_alsa, (const struct driver*)NULL };
