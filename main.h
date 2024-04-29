#ifndef MAIN_H_
#define MAIN_H_
#define _FILE_OFFSET_BITS 64

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iomanip>

#include <mutex>
#include <thread>
#include <condition_variable>

#include <errno.h>
#include <signal.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>

#include <unistd.h>
#include <dirent.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "fcgi_server.h"

//======================================================================
int create_server_socket(const char *host, const char *port, int backlog);
int write_timeout(int fd, const char *buf, int len, int timeout);
int poll_in(int fd1, int fd2, int *ret1, int *ret2, int timeout);
std::string get_time();
//======================================================================
enum MODE { ONE_SCRIPT = 1, MULTI_SCRIPTS };

enum {
    RS101 = 101,
    RS200 = 200,RS204 = 204,RS206 = 206,
    RS301 = 301, RS302,
    RS400 = 400,RS401,RS402,RS403,RS404,RS405,RS406,RS407,
    RS408,RS411 = 411,RS413 = 413,RS414,RS415,RS416,RS417,RS418,
    RS500 = 500,RS501,RS502,RS503,RS504,RS505
};
//======================================================================
class Config
{
    Config(const Config&){}
    Config& operator=(const Config&);

public:
    Config(){}

    MODE mode;
    int MaxThreads;
    int TimeoutCGI;
    std::string  Host;
    std::string  Port;
    std::string  cgi_path;
};
//----------------------------------------------------------------------
extern char **environ;
extern const Config* const conf;
//======================================================================
struct Connect
{
    int err;
    int cgi_in[2], cgi_out[2], cgi_err[2];
    pid_t pid;

    std::string script_name;
    std::string str_http_method;
    std::string str_content_length;
    std::string content_type;
    std::string query_string;
    std::string path;

    Connect()
    {
        err = 0;
        pid = -1;
        cgi_in[0] = cgi_in[1] = -1;
        cgi_out[0] = cgi_out[1] = -1;
        cgi_err[0] = cgi_err[1] = -1;
    }
};
//======================================================================
int cgi(FCGI_server& fcgi, Connect *conn);
int find_env_var(std::string& s, std::string& var);
const char *status_resp(int st);
void kill_script(Connect *conn);
int wait_pid(Connect *conn);

#endif
