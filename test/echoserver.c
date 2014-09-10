
#include	"../lib/lirc_client.h"

const char const*  prog = "echoserver";


int main(int argc, char** argv)
{
	char* code_str;
	int fd;
	char remote[64];
        char keysym[16];

	if (lirc_init("client_test", 1) == -1) {
		fprintf(stderr, "Cannot do lirc_init\n");
		exit(1);
	};
	fd = lirc_get_local_socket("var/lircd.socket", 0);
	while (lirc_nextcode(&code_str) == 0) {
		if (code_str == NULL) {
			continue;
		}
		printf(code_str);
		fflush(stdout);
		sscanf(code_str, "%*x %*x %16s %64s", keysym, remote);
		lirc_send_one(fd, remote, keysym);
	}
	lirc_deinit();
}
