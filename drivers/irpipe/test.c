#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <asm-generic/ioctls.h>
#include <asm-generic/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <media/lirc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "irpipe.h"

static const char* const DEVICE =
	"/dev/irpipe0";

static char* const WELCOME =
	"Please enter 1 to write\n"
	"             2 to read\n"
	"             3 to ioctl\n"
	"             4 to open reader fd\n"
	"             5 to open writer fd\n"
	"             6 to close reader fd\n"
	"             7 to close writer fd\n"
	"             8 to open read/write fd\n"
	"             9 to write read/write fd\n"
	"             10 to read read/write fd\n"
	"             11 to close read/write fd\n"
	"             12 to stat() read/write fd\n"
	"             13 to select() read/write fd\n"
	"             14 to run ioctl(LIRC_GET_FEATURES) on read/write fd\n";

struct ioctl_cmd {const char* name; const unsigned int value; };


struct ioctl_cmd ioctl_cmds[] = {
	{"FIOQSIZE",		FIOQSIZE},
	{"LIRC_GET_FEATURES",	LIRC_GET_FEATURES},
	{"LIRC_GET_SEND_MODE",	LIRC_GET_SEND_MODE},
	{"LIRC_GET_REC_MODE",	LIRC_GET_REC_MODE},
	{"LIRC_GET_LENGTH",	LIRC_GET_LENGTH},
	{"LIRC_SET_FEATURES",	LIRC_SET_FEATURES},
	{"LIRC_SET_SEND_MODE",	LIRC_SET_SEND_MODE},
	{"LIRC_SET_REC_MODE",	LIRC_SET_REC_MODE},
	{"LIRC_SET_LENGTH",	LIRC_SET_LENGTH},
	{0,			0}
};

static int fdw = -1;
static int fdr = -1;
static int fdrw = -1;
static int debug = 1;

int write_device(void)
{
	int write_length = 0;
	ssize_t ret;
	char data[1024];

	printf("Enter data to write into device: ");
	scanf(" %[^\n]", data); /* a space added after"so that it reads
				   white space, %[^\n] is addeed so that
				   it takes input until new line*/
	write_length = strlen(data);

	ret = write(fdw, data, write_length);
	if (ret == -1)
		perror("write failed");
	else
		printf("write OK (%d bytes)\n", ret);
	if (debug)
		fflush(stdout); /*not to miss any log*/
	return 0;
}


int read_device(void)
{
	char data[2048];
	size_t length;
	ssize_t ret;

	do {
		printf("read length: ");
	} while	(scanf("%d", &length) != 1);
	if (debug)
		printf("Reading %d bytes from fd: %d\n", length, fdr);
	ret = read(fdr, data, length);
	if (ret < 0) {
		printf("Error code: %d", errno);
		perror("read failed");
	} else {
		ret  = ret < sizeof(data) ? ret : ret - 1;
		data[ret] = '\0';
		printf("Read OK (%d bytes): %s\n", ret, data);
	}
	if (debug)
		fflush(stdout); /*not to miss any log*/
	return 0;
}


int ioctl_device(void)
{
	char cmd_name[64];
	int cmd = -1;
	unsigned long arg = 0;
	ssize_t ret;
	int i;


	do {
		printf("Enter the ioctl command:\n");
		if (scanf("%63s", cmd_name) != 1)
			continue;
		if (strchr(cmd_name, '\n') != NULL)
			*strchr(cmd_name, '\n') = '\0';
		if (strlen(cmd_name) == 0)
			continue;
		for (i = 0; ioctl_cmds[i].name != NULL; i += 1) {
			if (strcmp(ioctl_cmds[i].name, cmd_name) == 0) {
				cmd = ioctl_cmds[i].value;
				break;
			}
		}
		if (cmd == -1) {
			printf("No such command\n");
			continue;
		}
		do {
			printf("Enter argument:");
		} while (scanf("%lu", &arg) != 1);
	} while (cmd == -1);
	if (debug)
		printf("Running cmd %d (0x%x) with arg %u\n", cmd, cmd, arg);
	ret = ioctl(fdr, cmd, arg);
	if (ret < 0) {
		printf("(errno: %d) ", errno);
		perror("ioctl failed");
	} else
		printf("IOCTL:  %d (0x%x)\n", ret, ret);
	if (debug)
		fflush(stdout); /*not to miss any log*/
	return 0;
}


void open_reader(void)
{
	printf("Opening read device...");
	fflush(stdout);
	fdr = open(DEVICE, O_RDONLY);
	if (fdr < 0) {
		perror("When opening read device");
	} else {
		printf("done (fd: %d)\n", fdr);
	}
	fflush(stdout);
}


void open_writer(void)
{
	printf("Opening write device...");
	fflush(stdout);
	fdw = open(DEVICE, O_WRONLY);
	if (fdw < 0) {
		perror("When opening write device");
	} else {
		printf("done\n");
	}
	fflush(stdout);
}

void open_read_write(void)
{
	printf("Opening read/write device...");
	fflush(stdout);
	fdrw = open(DEVICE, O_RDWR);
	if (fdrw < 0) {
		perror("When opening read/write device");
	} else {
		printf("done\n");
	}
	fflush(stdout);
}

int write_rw_device(void)
{
	int write_length = 0;
	ssize_t ret;
	char data[1024];

	printf("Enter data to write into r/w device: ");
	scanf(" %[^\n]", data); /* a space added after"so that it reads
				   white space, %[^\n] is addeed so that
				   it takes input until new line*/
	write_length = strlen(data);

	ret = write(fdrw, data, write_length);
	if (ret == -1)
		perror("write rw failed");
	else
		printf("write rw OK (%d bytes)\n", ret);
	if (debug)
		fflush(stdout); /*not to miss any log*/
	return 0;
}


int read_rw_device(void)
{
	char data[2048];
	size_t length;
	ssize_t ret;

	do {
		printf("read length: ");
	} while	(scanf("%d", &length) != 1);
	if (debug)
		printf("Reading %d bytes from fd: %d\n", length, fdr);
	ret = read(fdrw, data, length);
	if (ret < 0) {
		printf("Error code: %d", errno);
		perror("read failed");
	} else {
		ret  = ret < sizeof(data) ? ret : ret - 1;
		data[ret] = '\0';
		printf("Read OK (%d bytes): %s\n", ret, data);
	}
	if (debug)
		fflush(stdout); /*not to miss any log*/
	return 0;
}





void close_reader(void)
{
	int r;

	r = close(fdr);
	if (r < 0) {
		perror("While closing reader fd");
	} else {
		fdr = -1;
		printf("Reader closed OK\n");
	}
}


void close_writer(void)
{
	int r;

	r = close(fdw);
	if (r < 0) {
		perror("While closing writer fd");
	} else {
		fdw = -1;
		printf("Writer closed OK\n");
	}
}


void close_rw_device(void)
{
	int r;

	r = close(fdrw);
	if (r < 0) {
		perror("While closing read/write fd");
	} else {
		fdw = -1;
		printf("read/write fd closed OK\n");
	}
}

void stat_rw_device(void)
{
	int r;
	struct stat s;

	r = fstat(fdrw, &s);
	if (r < 0) {
		perror("While stat() on read/write fd");
		return;
	}
	printf("%-16s%d\n", "Inode:", s.st_ino);
	printf("%-16s%d\n", "is_fifo:", S_ISFIFO(s.st_mode));
	printf("%-16s%d\n", "Size:", s.st_size);
}


void select_rw_device(void)
{
	int r;
	fd_set fds;
	int maxfd;

	FD_ZERO(&fds);
	FD_SET(fdrw, &fds);
	maxfd = fdrw;
	r = select(maxfd + 1, &fds, NULL, NULL, NULL);
	if (r < 0) {
		perror("While select() on read/write fd");
		return;
	}
	printf("select() on read/write fd - OK\n");
}


void get_features_rw_device(void)
{
	int r;
	__u32 features;

	r = ioctl(fdrw, LIRC_GET_FEATURES, (void*) &features);
	if (r < 0) {
		perror("While ioctl(LIRC_GET_FEATURES)) on read/write fd");
		return;
	}
	printf("get_features on read/write fd: %0x\n", features);

}


int main(int argc, char** argv)
{
	int value = 0;

	if (access(DEVICE, F_OK) == -1) {
		printf("module %s not loaded\n", DEVICE);
		return 0;
	}
	printf("module %s loaded, will be used\n", DEVICE);
	while (1) {
		printf(WELCOME);
		scanf("%d", &value);
		switch (value) {
		case 1:
			write_device();
			break;
		case 2:
			read_device();
			break;
		case 3:
			ioctl_device();
			break;
		case 4:
			open_reader();
			break;
		case 5:
			open_writer();
			break;
		case 6:
			close_reader();
			break;
		case 7:
			close_writer();
			break;
		case 8:
			open_read_write();
			break;
		case 9:
			write_rw_device();
			break;
		case 10:
			read_rw_device();
			break;
		case 11:
			close_rw_device();
			break;
		case 12:
			stat_rw_device();
			break;
		case 13:
			select_rw_device();
			break;
		case 14:
			get_features_rw_device();
			break;
		default:
			printf(
				"unknown  option selected, please enter right one\n");
			break;
		}
	}
	return 0;
}
