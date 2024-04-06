#include "main.h"
#include <sys/ioctl.h>

using namespace std;
//======================================================================
const int backlog = 4096;

static Config c;
const Config* const conf = &c;

int cgi(FCGI_server& fcgi, Connect *conn);
int find_env_var(string& s, string& var);
//======================================================================
std::mutex mtx_thr;
std::condition_variable cond_exit_thr;
int count_thr = 0;
//----------------------------------------------------------------------
int start_thr(void)
{
unique_lock<mutex> lk(mtx_thr);
    ++count_thr;
    while (count_thr > conf->MaxThreads)
    {
        cond_exit_thr.wait(lk);
    }
    return count_thr;
}
//----------------------------------------------------------------------
void close_thr(void)
{
mtx_thr.lock();
    --count_thr;
mtx_thr.unlock();
    cond_exit_thr.notify_one();
}
//======================================================================
void response_(int sock, int count_conn);
unsigned int count_conn = 0;
//======================================================================
void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        printf("<%s:%d> ### SIGINT ### all_conn=%d\n", __func__, __LINE__, count_conn);
    }
    else
        printf("<%s:%d> sig=%d\n", __func__, __LINE__, sig);
}
//======================================================================
int check_path(string& path)
{
    struct stat st;

    int ret = stat(path.c_str(), &st);
    if (ret == -1)
    {
        fprintf(stderr, "<%s:%d> Error stat(%s): %s\n", __func__, __LINE__, path.c_str(), strerror(errno));
        char buf[2048];
        char *cwd = getcwd(buf, sizeof(buf));
        if (cwd)
            fprintf(stderr, "<%s:%d> cwd: %s\n", __func__, __LINE__, cwd);
        return -1;
    }

    if (!S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "<%s:%d> [%s] is not directory\n", __func__, __LINE__, path.c_str());
        return -1;
    }

    char path_[PATH_MAX] = "";
    if (!realpath(path.c_str(), path_))
    {
        fprintf(stderr, "<%s:%d> Error realpath(): %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    path = path_;

    return 0;
}
//======================================================================
void read_conf_file(FILE *fconf)
{
    printf("********** Read Config File **********\n");

    int line_ = 0, n = 0;
    while (1)
    {
        char buf[1024], name[16], val[1008];
        if (!fgets(buf, sizeof(buf), fconf))
        {
            if (feof(fconf))
                break;
            printf("        Error fgets(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        ++line_;

        if (sscanf(buf, "%s %s", name, val) != 2)
            continue;

        if (name[0] == '#')
        {
            continue;
        }
        else if (!strcmp(name, "OneScript"))
        {
            if (tolower(val[0]) == 'y')
                c.mode = ONE_SCRIPT;
            else if (tolower(val[0]) == 'n')
                c.mode = MULTI_SCRIPTS;
            else
            {
                printf("    ??? <%d> %s         OneScript: y/n\n", line_, buf);
                exit(EXIT_FAILURE);
            }
            ++n;
        }
        else if (!strcmp(name, "ScriptPath"))
        {
            c.cgi_path = val;
            ++n;
        }
        else if (!strcmp(name, "Host"))
        {
            c.Host = val;
            ++n;
        }
        else if (!strcmp(name, "Port"))
        {
            c.Port = val;
            ++n;
        }
        else if (!strcmp(name, "MaxThreads"))
        {
            if (sscanf(val, "%d", &c.MaxThreads) != 1)
            {
                printf("        <%d> Error sscanf(): %s\n", line_, val);
                exit(EXIT_FAILURE);
            }
            ++n;
        }
        else if (!strcmp(name, "TimeoutCGI"))
        {
            if (sscanf(val, "%d", &c.TimeoutCGI) != 1)
            {
                printf("        <%d> Error sscanf(): %s\n", line_, val);
                exit(EXIT_FAILURE);
            }
            ++n;
        }
        else
        {
            printf("    ??? <%d> %s\n", line_, buf);
            continue;
        }

        printf("        <%d> %s: %s\n", line_, name, val);
    }

    if (n != 6)
    {
        printf("    ???  Num Parameters != 6, [%d]\n", n);
        exit(EXIT_FAILURE);
    }

    if (conf->mode == MULTI_SCRIPTS)
    {
        if (check_path(c.cgi_path))
            exit(EXIT_FAILURE);
    }
    else
    {
        struct stat st;
        if (stat(conf->cgi_path.c_str(), &st) == -1)
        {
            fprintf(stderr, "<%s:%d> script (%s) not found\n", __func__, __LINE__, conf->cgi_path.c_str());
            exit(EXIT_FAILURE);
        }

        if (!S_ISREG(st.st_mode))
        {
            fprintf(stderr, "<%s:%d> [%s] is not script\n", __func__, __LINE__, conf->cgi_path.c_str());
            exit(EXIT_FAILURE);
        }
    }

    printf("**************************************\n\n");
    printf("ScriptPath: %s\n", conf->cgi_path.c_str());
}
//======================================================================
int main(int argc, char *argv[])
{
    printf("========== FastCGI > CGI ==========\n");

    FILE *fconf = fopen("fcgi2cgi.conf", "r");
    if (!fconf)
    {
        fprintf(stderr, "Error fopen(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    read_conf_file(fconf);
    fclose(fconf);

    printf("PID: %u; %s:%s\n", getpid(), conf->Host.c_str(), conf->Port.c_str());

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
        fprintf(stderr, "Error signal(SIGPIPE): %s\n", strerror(errno));
        return 1;
    }

    if (signal(SIGINT, signal_handler) == SIG_ERR)
    {
        fprintf(stderr, "Error signal(SIGINT): %s\n", strerror(errno));
        return 1;
    }

    for ( ; environ[0]; )
    {
        char *p, buf[512];
        if ((p = (char*)memccpy(buf, environ[0], '=', strlen(environ[0]))))
        {
            *(p - 1) = 0;
            unsetenv(buf);
        }
    }

    int fcgi_sock = create_server_socket(conf->Host.c_str(), conf->Port.c_str(), backlog);
    for ( ; ; )
    {
        //printf("<%s:%d>  ----------------- wait connect -----------------\n", __func__, __LINE__);
        int clientSock = accept(fcgi_sock, NULL, NULL);
        if (clientSock == -1)
        {
            fprintf(stderr, "Error accept(): %s\n", strerror(errno));
            if ((errno == EINTR) || (errno == EMFILE) || (errno == EAGAIN))
                continue;
            else
                break;
        }

        int opt = 1;
        ioctl(clientSock, FIONBIO, &opt);

        ++count_conn;

        thread thr;
        try
        {
            thr = thread(response_, clientSock, count_conn);
            thr.detach();
        }
        catch (...)
        {
            printf("<%s:%d> Error create thread: %s(%d)\n", __func__, __LINE__, strerror(errno), errno);
            exit(errno);
        }

        start_thr();
    }
    
    return 0;
}
//======================================================================
void response(int sock, Connect *conn, int count_conn);
//======================================================================
void response_(int sock, int count_conn)
{
    Connect conn;
    response(sock, &conn, count_conn);
    shutdown(sock, SHUT_RDWR);
    close(sock);

    if (conn.cgi_in[0] > 0)
        close(conn.cgi_in[0]);
    if (conn.cgi_in[1] > 0)
        close(conn.cgi_in[1]);
    if (conn.cgi_out[0] > 0)
        close(conn.cgi_out[0]);
    if (conn.cgi_out[1] > 0)
        close(conn.cgi_out[1]);
    if (conn.cgi_err[0] > 0)
        close(conn.cgi_err[0]);
    if (conn.cgi_err[1] > 0)
        close(conn.cgi_err[1]);

    if (conn.pid > 0)
    {
        wait_pid(&conn);
    }
    close_thr();
}
//======================================================================
void response(int fcgi_sock, Connect *conn, int count_conn)
{
    FCGI_server fcgi(fcgi_sock, conf->TimeoutCGI);
    int err = fcgi.error();
    if (err)
    {
        fprintf(stderr, "<%s:%d> Error create fcgi: %d\n", __func__, __LINE__, err);
        return;
    }

    for (int i = 0, d = 0, n = fcgi.len_param(); (i < n) && (d < 5); ++i)
    {
        string s = fcgi.param(i);
        const char *p = s.c_str();
        //printf("<%s:%d> %s\n", __func__, __LINE__, p);
        if (strstr(p, "REQUEST_METHOD"))
        {
            if (find_env_var(s, conn->str_http_method))
                return;
            ++d;
        }
        else if (strstr(p, "CONTENT_LENGTH"))
        {
            if (find_env_var(s, conn->str_content_length))
                return;
            ++d;
        }
        else if (strstr(p, "CONTENT_TYPE"))
        {
            if (find_env_var(s, conn->content_type))
                return;
            ++d;
        }
        else if (strstr(p, "QUERY_STRING"))
        {
            if (find_env_var(s, conn->query_string))
                return;
            ++d;
        }
        /*else if (strstr(p, "SCRIPT_NAME"))
        {
            if (find_env_var(s, conn->script_name))
                return;
            ++d;
        }*/
        else if (strstr(p, "REQUEST_URI"))
        {
            //printf("<%s:%d> %s\n", __func__, __LINE__, p);
            if (find_env_var(s, conn->script_name))
                return;
            std::size_t n;
            if ((n = conn->script_name.find('?')) != std::string::npos)
                conn->script_name.resize(n);
            //printf("<%s:%d> [%s]\n", __func__, __LINE__, conn->script_name.c_str());
            ++d;
        }
    }

    if (fcgi.error())
    {
        fprintf(stderr, "<%s:%d> Error fcgi_get_param()\n", __func__, __LINE__);
        return;
    }

    err = cgi(fcgi, conn);
    if (err > 0)
    {
        fcgi << "Status: " << status_resp(err) << "\r\n";
        fcgi << "Content-Type: text/html\r\n\r\n";
        fcgi << "<h3>" << status_resp(err) << "</h3>";
        fcgi.fcgi_end_request();
        return;
    }
    else if (err < 0)
        return;

    for ( ; ; )
    {// --------- fcgi_stdin --------
        const int size_buf = 4095;
        char buf[size_buf + 1];
        int ret = fcgi.fcgi_stdin(buf, size_buf);
        if (ret < 0)
        {
            fprintf(stderr, "<%s:%d> Error fcgi_stdin()\n", __func__, __LINE__);
            return;
        }
        else if (ret == 0)
        {
            break;
        }

        ret = write_timeout(conn->cgi_in[1], buf, ret, conf->TimeoutCGI);
        if (ret <= 0)
        {
            return;
        }
    }

    close(conn->cgi_in[1]);
    conn->cgi_in[1] = -1;

    int num_fd = 2;
    //int all_read = 0;
    while (1)
    {// ----- cgi_stdout, cgi_stderr -----
        int ret1, ret2;
        char buf_stdout[1024], buf_stderr[128];

        int n = poll_in(conn->cgi_out[0], conn->cgi_err[0], &ret1, &ret2, conf->TimeoutCGI);
        if (n < 0)
            break;
        if (ret1)
        {
            if (ret1 == 1)
            {
                n = read(conn->cgi_out[0], buf_stdout, sizeof(buf_stdout) - 1);
                if (n <= 0)
                {
                    if (n < 0)
                        fprintf(stderr, "<%s:%d> Error read(%d)=%d: %s\n", __func__, __LINE__, conn->cgi_out[0], n, strerror(errno));
                    close(conn->cgi_out[0]);
                    conn->cgi_out[0] = -1;
                    --num_fd;
                }
                else
                {
                    //all_read += n;
                    buf_stdout[n] = 0;
                    fcgi << buf_stdout;
                }
            }
            else
            {
                close(conn->cgi_out[0]);
                conn->cgi_out[0] = -1;
                --num_fd;
            }
        }

        if (ret2)
        {
            if (ret2 == 1)
            {
                n = read(conn->cgi_err[0], buf_stderr + 8, sizeof(buf_stderr) - 8);
                if (n <= 0)
                {
                    if (n < 0)
                        fprintf(stderr, "<%s:%d> Error read(%d)=%d: %s\n", __func__, __LINE__, conn->cgi_err[0], n, strerror(errno));
                    close(conn->cgi_err[0]);
                    conn->cgi_err[0] = -1;
                    --num_fd;
                }
                else
                {
                    fcgi.fcgi_set_header(buf_stderr, FCGI_STDERR, n);
                    n = write_timeout(fcgi_sock, buf_stderr, n + 8, conf->TimeoutCGI);
                    if (n <= 0)
                    {
                        close(conn->cgi_err[0]);
                        conn->cgi_err[0] = -1;
                        --num_fd;
                    }
                }
            }
            else
            {
                close(conn->cgi_err[0]);
                conn->cgi_err[0] = -1;
                --num_fd;
            }
        }

        if (num_fd <= 0)
        {
            if (num_fd < 0)
                fprintf(stderr, "<%s:%d> [%s] num_fd=%d\n", __func__, __LINE__, get_time().c_str(), num_fd);
            break;
        }
    }

    //fcgi << "";
    fcgi.fcgi_end_request();
    err = fcgi.error();
    if (err)
    {
        fprintf(stderr, "<%s:%d> Error (%d), send %d bytes\n", __func__, __LINE__, err, fcgi.send_bytes());
    }
}
//======================================================================
string get_time()
{
    struct tm t;
    char s[40];
    time_t now = time(NULL);

    gmtime_r(&now, &t);

    strftime(s, sizeof(s), "%a, %d %b %Y %H:%M:%S GMT", &t);
    return s;
}
//======================================================================
int find_env_var(string& s, string& var)
{
    std::size_t n;
    if ((n = s.find('=')) != std::string::npos)
        var = s.substr(n + 1);
    else
    {
        printf("<%s:%d> Error: '=' not found\n", __func__, __LINE__);
        return -1;
    }
    return 0;
}
//======================================================================
const char *status_resp(int st)
{
    switch (st)
    {
        case RS200:
            return "200 OK";
        case RS400:
            return "400 Bad Request";
        case RS403:
            return "403 Forbidden";
        case RS404:
            return "404 Not Found";
        case RS405:
            return "405 Method Not Allowed";
        case RS411:
            return "411 Length Required";
        case RS413:
            return "413 Request entity too large";
        case RS500:
            return "500 Internal Server Error";
        case RS501:
            return "501 Not Implemented";
        case RS502:
            return "502 Bad Gateway";
        case RS504:
            return "504 Gateway Time-out";
        default:
            return "500 Internal Server Error";
    }
    return "";
}
