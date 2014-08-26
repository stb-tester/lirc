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
#include <iostream>
#include "lirc_private.h"

#define USAGE "Usage: lirc-check-plugins <plugin path>\n"

using namespace std;

void print_plugin(const char* path,
                  int has_handle,
                  int has_drivers,
                  const char*  name)
{
        char buff[256];
        string s;

        snprintf(buff, sizeof(buff), "%-48s", path);
        s += string(buff);
        snprintf(buff, sizeof(buff), ": %-6s", has_handle ? "ok" : "Fail" );
        s += string(buff);
        snprintf(buff, sizeof(buff), ": %-6s", has_drivers ? "ok" : "Fail" );
        s += string(buff);
        snprintf(buff, sizeof(buff), ": %-20s", name );
        s += string(buff);
        cout << s << "\n";
}


struct driver* visit_plugin(const char* path, drv_guest_func f, void* arg)
{
        void* handle;
        struct driver** drivers;
        const char* name;

        (void) dlerror();
        handle = dlopen(path, RTLD_NOW);
        if (handle != NULL) {
                drivers = (struct driver**)dlsym(handle, "hardwares");
                if (drivers != NULL) {
                        for ( ; *drivers; drivers++) {
                                name = "(Null)";
                                if ((*drivers)->name) {
                                        name = (*drivers)->name;
                                }
                                print_plugin(path, 1, 1, name);
                        }
                } else {
                        print_plugin(path, 1, 0, "(Null)");
                        cout << "# Error: " << path  <<
                                ": Cannot find any hardwares table " << "\n";
                }
        }
        else {
                print_plugin(path, 0, 0, "(Null)");
                cout << "# Error: " << dlerror() << "\n";
        }
        return NULL;
}


int main(int argc, char** argv)
{
        const char* pluginpath;

        if (argc != 2) {
                fprintf( stderr, USAGE);
                exit(2);
        }
        if (string(argv[1]) == "-h" or string(argv[1]) == "--help") {
                cout << USAGE << "\n";
                exit(0);
        }
        pluginpath = argv[1];
        lirc_log_set_file("lirc-check-plugins.log");
        lirc_log_open("lirc-check-plugins", 1, LIRC_DEBUG);
        setenv("LIRC_PLUGINDIR", pluginpath, 1);
        printf("##%-46s%-8s%-8s%-20s\n", "path", "load", "table", "name");
        for_each_plugin(visit_plugin, NULL);
        return 0;
}
