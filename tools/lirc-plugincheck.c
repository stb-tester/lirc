/**
 * Simple  tool to check that all plugins in current path can be loaded
 * Accepts a single argument identical to the --plugindir option for
 * lircd
*/


#define __BEGIN_DECLS  extern "C" {
#define __END_DECLS    }


#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include "lirc_private.h"

#define USAGE "Usage: lirc-check-plugins <plugin path>\n"

#define LEGEND \
	"# Legend:\n" \
	"# Ok:\t\tdlopen(2) and loading of hardwares table OK.\n" \
	"# NoHw:\t\tdlopen OK, but no hardwares symbol found (is this a plugin?)\n" \
	"# Fail:\t\tplugin failed to load (unresolved references?).\n" \
	"# Load:\t\tPlugin load status; fail, NoHw or ok\n" \
	"# Table:\tThe driver list in plugin is located.\n" \
	"# Any:\t\tDriver can be used with any remote and/or capture device.\n" \
	"# Send:\t\tThe driver can send data. \n"


#define CAN_SEND  \
	LIRC_CAN_SEND_RAW | LIRC_CAN_SEND_PULSE | LIRC_CAN_SEND_MODE2
#define CAN_ANY  \
	LIRC_CAN_REC_RAW | LIRC_CAN_REC_PULSE | LIRC_CAN_REC_MODE2


static const char *get(const struct driver* drv, int what) {
        return what & drv->features ? "x" : "-";
}

struct driver* visit_plugin(const char* path, drv_guest_func f, void* arg)
{
        void* handle;
        struct driver** drivers;
        char name[128];
	char line[128];

        (void) dlerror();
	snprintf(line, sizeof(line), "%-50s", path);
        handle = dlopen(path, RTLD_NOW);
        if (handle != NULL) {
                drivers = (struct driver**)dlsym(handle, "hardwares");
                if (drivers != NULL) {
                        for ( ; *drivers; drivers++) {
                                strncpy(name, "(Null)", sizeof(name));
                                if ((*drivers)->name) {
                                  	strncpy(name,
					    	(*drivers)->name,
					    	sizeof(name));
                                }
				printf(line);
	        	        printf("%-6s", "Ok");
				printf("%-6s", get((*drivers), CAN_ANY));
				printf("%-5s", get((*drivers), CAN_SEND));
				printf(name, "\n");
				printf("\n");
                        }
                } else {
			printf(line);
			printf( "%-6s%-6s%-5s%s\n", "NoHw", "-", "-", "-" );
			printf("# Error: %s\n", dlerror());
                }
        }
        else {
		printf(line);
	        printf("%-6s%-6s%-5s%-6s\n", "Fail", "-", "-",  "-");
		printf("# Error: %s\n", dlerror());
        }
        return NULL;
}

static void print_header()
{
        printf("# %-48s%-6s%-6s %-4s%-7sName\n",
               "Path", "Load", "Table", "Any", "Send");
}

int main(int argc, char** argv)
{
        const char* pluginpath;

        if (argc != 2) {
                fprintf( stderr, USAGE);
                exit(2);
        }
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0){
                printf(USAGE "\n");
                exit(0);
        }
        pluginpath = argv[1];
        lirc_log_set_file("lirc-check-plugins.log");
        lirc_log_open("lirc-check-plugins", 1, LIRC_DEBUG);
        setenv("LIRC_PLUGINDIR", pluginpath, 1);
	print_header();
        printf("## %-46s%-6s%-5s%-7sName\n",
               "Path", "Load",  "Any", "Send");
        for_each_plugin(visit_plugin, NULL);
	printf("#\n#\n");
	print_header();
	printf("#\n");
	printf(LEGEND);
	printf("#\n");
        return 0;
}
