#ifndef  TEST_UTIL
#define  TEST_UTIL

#include 	<netinet/in.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/un.h>


#include	<unistd.h>
#include	<string.h>


using namespace std;

const char* abspath(const char* relpath)
{
    char buff[256];

    if (relpath[0] == '/')
        return relpath;
    getcwd(buff, sizeof(buff));
    string path(buff);
    path +=  string("/") + relpath;
    return strdup(path.c_str());
};

void dummy_load(int argc, char** const argv) { return; };

#endif

// vim: set expandtab ts=4 sw=4:
