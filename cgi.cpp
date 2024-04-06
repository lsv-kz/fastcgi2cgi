#include "main.h"

using namespace std;
//======================================================================
int wait_pid(Connect *conn)
{
    int n = waitpid(conn->pid, NULL, WNOHANG); // no blocking
    if (n == -1)
    {
        fprintf(stderr, "<%s:%d> Error waitpid(%d): %s\n", __func__, __LINE__, conn->pid, strerror(errno));
        conn->err = -1;
    }
    else if (n == 0)
    {
        if (kill(conn->pid, SIGKILL) == 0)
            waitpid(conn->pid, NULL, 0);
        else
        {
            fprintf(stderr, "<%s:%d> Error kill(%d): %s\n", __func__, __LINE__, conn->pid, strerror(errno));
            conn->err = -1;
        }
    }

    return conn->err;
}
//======================================================================
void kill_script(Connect *conn)
{
    if (kill(conn->pid, SIGKILL) == 0)
        waitpid(conn->pid, NULL, 0);
    else
        fprintf(stderr, "<%s:%d> Error kill(%d): %s\n", __func__, __LINE__, conn->pid, strerror(errno));
}
//======================================================================
int cgi_fork(FCGI_server& fcgi, Connect *conn)
{
    //--------------------------- fork ---------------------------------
    conn->pid = fork();
    if (conn->pid < 0)
    {
        fprintf(stderr, "<%s:%d> Error fork(): %s\n", __func__, __LINE__, strerror(errno));
        return RS500;
    }
    else if (conn->pid == 0)
    {
        //----------------------- child --------------------------------       
        close(conn->cgi_out[0]);
        close(conn->cgi_err[0]);

        if (conn->cgi_out[1] != STDOUT_FILENO)
        {
            if (dup2(conn->cgi_out[1], STDOUT_FILENO) < 0)
                goto to_pipe;
            if (close(conn->cgi_out[1]) < 0)
                goto to_stdout;
        }

        if (conn->cgi_err[1] != STDERR_FILENO)
        {
            if (dup2(conn->cgi_err[1], STDERR_FILENO) < 0)
                goto to_stdout;
            if (close(conn->cgi_err[1]) < 0)
                goto to_stdout;
        }

        if (conn->cgi_in[1] > 0)
        {
            close(conn->cgi_in[1]);
            if (conn->cgi_in[0] != STDIN_FILENO)
            {
                if (dup2(conn->cgi_in[0], STDIN_FILENO) < 0)
                    goto to_stdout;
                if (close(conn->cgi_in[0]) < 0)
                    goto to_stdout;
            }
        }

        setenv("PATH", "/bin:/usr/bin:/usr/local/bin", 1);
        setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);

        for (int i = 0, len = fcgi.len_param(); i < len; ++i)
        {
            string s = fcgi.param(i);
            std::size_t n;
            if ((n = s.find('=')) != std::string::npos)
            {
                string name = s.substr(0, n);
                string val = s.substr(n+1);
                setenv(name.c_str(), val.c_str(), 1);
                //fprintf(stderr, "[%s: %s]\n", name.c_str(), val.c_str());
            }
            else
            {
                fprintf(stderr, "<%s:%d> Error '=' not found\n", __func__, __LINE__);
            }
        }

        execl(conn->path.c_str(), conn->path.c_str(), NULL);
        fprintf(stderr, "<%s:%d> Error execl(%s)\n", __func__, __LINE__, conn->path.c_str());
    to_stdout:
        printf( "Status: 500 Internal Server Error\r\n"
                "Content-type: text/html; charset=UTF-8\r\n"
                "\r\n"
                "<!DOCTYPE html>\n"
                "<html>\n"
                " <head>\n"
                "  <title>500 Internal Server Error</title>\n"
                "  <meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n"
                " </head>\n"
                " <body>\n"
                "  <h3> 500 Internal Server Error</h3>\n"
                "  <p>no exec: %s(%d)</p>\n"
                "  <hr>\n"
                "  %s\n"
                " </body>\n"
                "</html>", strerror(errno), errno, get_time().c_str());
        exit(EXIT_FAILURE);
        
    to_pipe:
        char err_msg[] = "Status: 500 Internal Server Error\r\n"
                "Content-type: text/html; charset=UTF-8\r\n"
                "\r\n"
                "<!DOCTYPE html>\n"
                "<html>\n"
                " <head>\n"
                "  <title>500 Internal Server Error</title>\n"
                "  <meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n"
                " </head>\n"
                " <body>\n"
                "  <h3> 500 Internal Server Error</h3>\n"
                " </body>\n"
                "</html>";
        write(conn->cgi_out[1], err_msg, strlen(err_msg));
        exit(EXIT_FAILURE);
    }
    else
    {
    //--------------------------- parent -------------------------------
        if (conn->cgi_in[0] > 0)
        {
            close(conn->cgi_in[0]);
            conn->cgi_in[0] = -1;
        }

        close(conn->cgi_out[1]);
        conn->cgi_out[1] = -1;
        
        close(conn->cgi_err[1]);
        conn->cgi_err[1] = -1;
        return 0;
    }
}
//======================================================================
int cgi(FCGI_server& fcgi, Connect *conn)
{
    int n;

    n = pipe(conn->cgi_out);
    if (n == -1)
    {
        fprintf(stderr, "<%s:%d> Error pipe()=%d\n", __func__, __LINE__, n);
        return RS500;
    }
    
    const int flags = 1;
    ioctl(conn->cgi_out[0], FIONBIO, &flags);
    ioctl(conn->cgi_out[1], FIONBIO, &flags);
    ioctl(conn->cgi_out[0], FIOCLEX);

    n = pipe(conn->cgi_err);
    if (n == -1)
    {
        fprintf(stderr, "<%s:%d> Error pipe()=%d\n", __func__, __LINE__, n);
        return RS500;
    }

    ioctl(conn->cgi_err[0], FIONBIO, &flags);
    ioctl(conn->cgi_err[1], FIONBIO, &flags);
    ioctl(conn->cgi_err[0], FIOCLEX);

    if (conn->str_http_method == "POST")
    {
        if (conn->str_content_length.size() == 0)
        {
            fprintf(stderr, "<%s:%d> Content-Type \?\n", __func__, __LINE__);
            return RS400;
        }

        if (conn->content_type.size() == 0)
        {
            fprintf(stderr, "<%s:%d> 411 Length Required\n", __func__, __LINE__);
            return RS411;
        }
        
        n = pipe(conn->cgi_in);
        if (n == -1)
        {
            fprintf(stderr, "<%s:%d> Error pipe()=%d\n", __func__, __LINE__, n);
            return RS500;
        }

        ioctl(conn->cgi_in[1], FIOCLEX);
    }
    else if (conn->str_http_method == "GET")
    {
        
    }
    else if (conn->str_http_method == "HEAD")
    {
        
    }
    else
    {
        fprintf(stderr, "<%s:%d> Error HTTP Method: %s\n", __func__, __LINE__, conn->str_http_method.c_str());
        return RS501;
    }
    
    if (conf->mode == MULTI_SCRIPTS)
    {
        struct stat st;
        conn->path = conf->cgi_path + conn->script_name;
        if (stat(conn->path.c_str(), &st) == -1)
        {
            fprintf(stderr, "<%s:%d> script (%s) not found\n", __func__, __LINE__, conn->path.c_str());
            return RS404;
        }
    }
    else
        conn->path = conf->cgi_path;

    return cgi_fork(fcgi, conn);
}
