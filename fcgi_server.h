#ifndef FCGI_SERVER_
#define FCGI_SERVER_

#include <cstdio>
#include <poll.h>

#include "string__.h"
#include "Array.h"

#define FCGI_RESPONDER  1

#define FCGI_VERSION_1           1
#define FCGI_BEGIN_REQUEST       1
#define FCGI_ABORT_REQUEST       2
#define FCGI_END_REQUEST         3
#define FCGI_PARAMS              4
#define FCGI_STDIN               5
#define FCGI_STDOUT              6
#define FCGI_STDERR              7
#define FCGI_DATA                8
#define FCGI_GET_VALUES          9
#define FCGI_GET_VALUES_RESULT  10
#define FCGI_UNKNOWN_TYPE       11
#define FCGI_MAXTYPE            (FCGI_UNKNOWN_TYPE)
#define requestId               1

const int FCGI_SIZE_BUF = 4096;
//======================================================================
typedef struct {
    unsigned char type;
    int len;
    int paddingLen;
} fcgi_header;
//======================================================================
class FCGI_server
{
    int err = 0;
    char buf_out[FCGI_SIZE_BUF];
    const char *str_zero = "\0\0\0\0\0\0\0\0";
    int offset_out = 8, all_send = 0;
    int fcgi_sock;
    int TimeoutCGI;

    fcgi_header header = {0, 0, 0};

    Array <String> Param;
    //------------------------------------------------------------------
    int fcgi_read_header()
    {
        if (err)
            return -1;
        int n;
        char buf[8];

        n = fcgi_read(buf, 8);
        if (n != 8)
            return -1;
        header.type = (unsigned char)buf[1];
        header.paddingLen = (unsigned char)buf[6];
        header.len = ((unsigned char)buf[4]<<8) | (unsigned char)buf[5];

        return n;
    }
    //------------------------------------------------------------------
    int fcgi_read(char *buf, int len)
    {
        if (err)
            return -1;
        int read_bytes = 0, ret;
        struct pollfd fdrd;
        char *p;

        fdrd.fd = fcgi_sock;
        fdrd.events = POLLIN;
        p = buf;

        while (len > 0)
        {
            ret = poll(&fdrd, 1, TimeoutCGI * 1000);
            if (ret == -1)
            {
                if (errno == EINTR)
                    continue;
                err = __LINE__;
                fprintf(stderr, "<%s:%d> Error poll(): %s\n", __func__, __LINE__, strerror(errno));
                return -1;
            }
            else if (!ret)
            {
                fprintf(stderr, "<%s:%d> Error poll(): timeout\n", __func__, __LINE__);
                err = __LINE__;
                return -1;
            }

            if (fdrd.revents & POLLIN)
            {
                ret = read(fcgi_sock, p, len);
                if (ret < 0)
                {
                    fprintf(stderr, "<%s:%d> Error read(): %s\n", __func__, __LINE__, strerror(errno));
                    err = __LINE__;
                    return -1;
                }
                else if (ret == 0)
                {
                    fprintf(stderr, "<%s:%d> Error read()=0\n", __func__, __LINE__);
                    err = __LINE__;
                    return -1;
                }
                else
                {
                    p += ret;
                    len -= ret;
                    read_bytes += ret;
                }
            }
            else
            {
                fprintf(stderr, "<%s:%d> Error revents=0x%x\n", __func__, __LINE__, fdrd.revents);
                err = __LINE__;
                return -1;
            }
        }
        return read_bytes;
    }
    //------------------------------------------------------------------
    void fcgi_send()
    {
        if (err)
            return;

        int write_bytes = 0, ret = 0, n_send = offset_out;
        struct pollfd fdwr;
        char *p = buf_out;

        fdwr.fd = fcgi_sock;
        fdwr.events = POLLOUT;

        while (offset_out > 0)
        {
            ret = poll(&fdwr, 1, TimeoutCGI * 1000);
            if (ret == -1)
            {
                if (errno == EINTR)
                    continue;
                err = __LINE__;
                break;
            }
            else if (!ret)
            {
                err = __LINE__;
                break;
            }

            if (fdwr.revents != POLLOUT)
            {
                fprintf(stderr, "<%s:%d> revents=0x%x; %d/%d\n", __func__, __LINE__, fdwr.revents, n_send, offset_out);
                err = __LINE__;
                break;
            }

            ret = write(fcgi_sock, p, offset_out);
            if (ret == -1)
            {
                if ((errno == EINTR) || (errno == EAGAIN))
                    continue;
                err = __LINE__;
                break;
            }

            write_bytes += ret;
            offset_out -= ret;
            p += ret;
        }

        all_send += write_bytes;
        offset_out = 8;
    }
    //------------------------------------------------------------------
    void fcgi_get_begin()
    {
        if (err)
            return;
        int ret = fcgi_read_header();
        if (ret != 8)
        {
            err = __LINE__;
            return;
        }

        if (header.type != FCGI_BEGIN_REQUEST)
        {
            err = __LINE__;
            return;
        }

        if (header.len == 8)
        {
            char buf[8];
            ret = fcgi_read(buf, 8);
            if (ret != 8)
                err = __LINE__;
        }
        else
            err = __LINE__;
    }
    //------------------------------------------------------------------
    FCGI_server() {}
public://===============================================================
    FCGI_server(int sock, int timeout = 10)
    {
        fcgi_sock = sock;
        TimeoutCGI = timeout;
        offset_out = 8;
        err = 0;
        fcgi_get_begin();
        fcgi_get_param();
    }
    //------------------------------------------------------------------
    FCGI_server & operator << (const char *s) // *** FCGI_STDOUT ***
    {
        if (err)
            return *this;

        if (!s)
        {
            err = __LINE__;
            return *this;
        }

        int n = 0, stdout_len = strlen(s);
        if (stdout_len == 0)
        {
            if (offset_out > 8)
            {
                fcgi_set_header(buf_out, FCGI_STDOUT, offset_out - 8);
                fcgi_send();
            }

            fcgi_set_header(buf_out, FCGI_STDOUT, 0);
            fcgi_set_header(buf_out + offset_out, FCGI_END_REQUEST, 8);
            offset_out += 8;
            memcpy(buf_out + offset_out, str_zero, 8);
            offset_out += 8;
            fcgi_send();
            return *this;
        }

        while (FCGI_SIZE_BUF < (offset_out + stdout_len))
        {
            int len = FCGI_SIZE_BUF - offset_out;
            memcpy(buf_out + offset_out, s + n, len);
            offset_out += len;
            stdout_len -= len;
            n += len;
            fcgi_set_header(buf_out, FCGI_STDOUT, offset_out - 8);
            fcgi_send();
            if (err)
            {
                return *this;
            }
        }

        memcpy(buf_out + offset_out, s + n, stdout_len);
        offset_out += stdout_len;
        return *this;
    }
    //------------------------------------------------------------------
    FCGI_server & operator << (const long long ll) // *** FCGI_STDOUT ***
    {
        if (err)
            return *this;

        char buf[21];
        snprintf(buf, sizeof(buf), "%lld", ll);
        *this << buf;
        return *this;
    }
    //------------------------------------------------------------------
    int error() const { return err; }
    int send_bytes() { return all_send; }
    int len_param() { return  Param.len(); }
    const char *param(int n)
    {
        if (n >= Param.len())
        {
            err = __LINE__;
            return NULL;
        }
        return Param.get(n)->c_str();
    }
    //------------------------------------------------------------------
    int fcgi_stdin(char *buf, int size)    // *** FCGI_STDIN ***
    {
        if (err)
            return -1;

        if (header.len == 0)
        {
            if (header.paddingLen > 0)
            {
                char s[256];
                if (fcgi_read(s, header.paddingLen) < 0)
                {
                    err = __LINE__;
                    return -1;
                }
            }

            int n = fcgi_read_header();
            if (n <= 0)
            {
                err = __LINE__;
                return -1;
            }

            if (header.type != FCGI_STDIN)
            {
                fprintf(stderr, "<%s:%d>  Error type != FCGI_STDIN (type=%d, len=%d)\n", __func__, __LINE__, header.type, header.len);
                err = __LINE__;
                return -1;
            }

            if (header.len == 0)
                return 0;
        }

        int rd = (header.len <= size) ? header.len : size;
        int ret = fcgi_read(buf, rd);
        if (ret <= 0)
        {
            fprintf(stderr, "! Error: fcgi_read FCGI_STDIN\n");
            err = __LINE__;
            return -1;
        }

        header.len -= ret;
        return ret;
    } 
    //------------------------------------------------------------------
    void fcgi_set_header(char *p, int type, int len)
    {
        *p++ = FCGI_VERSION_1;
        *p++ = (unsigned char)type;
        *p++ = (unsigned char) ((1 >> 8) & 0xff);
        *p++ = (unsigned char) ((1) & 0xff);

        *p++ = (unsigned char) ((len >> 8) & 0xff);
        *p++ = (unsigned char) ((len) & 0xff);

        *p++ = 0;

        *p = 0;
    }
    //------------------------------------------------------------------
    void fcgi_clean_stdout()
    {
        if (offset_out > 8)
        {
            fcgi_set_header(buf_out, FCGI_STDOUT, offset_out - 8);
            fcgi_send();
        }
    }
    //------------------------------------------------------------------
    void fcgi_end_request()
    {
        if (offset_out > 8)
        {
            fcgi_set_header(buf_out, FCGI_STDOUT, offset_out - 8);
            fcgi_send();
        }

        fcgi_set_header(buf_out, FCGI_STDOUT, 0);
        fcgi_set_header(buf_out + offset_out, FCGI_END_REQUEST, 8);
        offset_out += 8;
        memcpy(buf_out + offset_out, str_zero, 8);
        offset_out += 8;
        fcgi_send();
    }
    //------------------------------------------------------------------
    int fcgi_get_param()             // *** FCGI_PARAMS ***
    {
        if (err)
            return -1;
        Param.reserve(32);
        int num_par = 0, ret;
        if ((ret = fcgi_read_header()) != 8)
        {
            err = __LINE__;
            return -1;
        }

        if (header.type != FCGI_PARAMS)
        {
            err = __LINE__;
            return -1;
        }

        const int buf_size = 1024;
        char buf[buf_size], *p = buf;
        int rd;
        int data_len = 0;
        String s;
        while (header.len > 0)
        {
            if (header.len < (buf_size - data_len))
                rd = header.len;
            else
                rd = (buf_size - data_len);

            int n_read = fcgi_read(buf + data_len, rd);
            if ((n_read != rd) || (n_read == 0))
            {
                fprintf(stderr, "<%s:%d> n_read=%d, rd=%d, header.len=%d\n", __func__, __LINE__, n_read, rd, header.len);
                err = __LINE__;
                return -1;
            }

            p = buf;
            data_len += n_read;
            header.len -= n_read;
            while ((data_len > 0))
            {
                s = "";
                int name_len, val_len, param_size_len = 0;

                if ((unsigned char)*p < 128)
                {
                    name_len = (unsigned char)(*p++);
                    ++param_size_len;
                }
                else
                {
                    if (data_len < 4)
                        break;

                    name_len = ((unsigned char)(*p++) & 0x7f) << 24;
                    name_len += ((unsigned char)(*p++) << 16);
                    name_len += ((unsigned char)(*p++) << 8);
                    name_len += (unsigned char)(*p++);
                    param_size_len += 4;
                }

                if ((data_len - param_size_len) < 1)
                {
                    p -= param_size_len;
                    break;
                }

                if ((unsigned char)*p < 128)
                {
                    val_len = (unsigned char)(*p++);
                    ++param_size_len;
                }
                else
                {
                    if ((data_len -param_size_len) < 4)
                    {
                        p -= param_size_len;
                        break;
                    }
                    val_len = ((unsigned char)(*p++) & 0x7f) << 24;
                    val_len += ((unsigned char)(*p++) << 16);
                    val_len += ((unsigned char)(*p++) << 8);
                    val_len += (unsigned char)(*p++);
                    param_size_len += 4;
                }

                data_len -= param_size_len;

                if (name_len > data_len)
                {
                    s.append(p, data_len);
                    name_len -= data_len;
                    data_len = 0;

                    while (name_len > 0)
                    {
                        int rd;
                        if (name_len > buf_size)
                            rd = buf_size;
                        else
                            rd = name_len;

                        n_read = fcgi_read(buf, rd);
                        if (n_read != rd)
                        {
                            err = __LINE__;
                            return -1;
                        }

                        name_len -= n_read;
                        header.len -= n_read;
                        s.append(buf, n_read);
                    }
                }
                else
                {
                    s.append(p, name_len);
                    data_len -= name_len;
                    p += name_len;
                }

                s << "=";

                if (val_len > data_len)
                {
                    s.append(p, data_len);
                    val_len -= data_len;
                    data_len = 0;

                    while (val_len > 0)
                    {
                        int rd;
                        if (val_len > buf_size)
                            rd = buf_size;
                        else
                            rd = val_len;

                        n_read = fcgi_read(buf, rd);
                        if (n_read != rd)
                        {
                            err = __LINE__;
                            return -1;
                        }

                        val_len -= n_read;
                        header.len -= n_read;
                        s.append(buf, n_read);
                    }
                }
                else
                {
                    s.append(p, val_len);
                    data_len -= val_len;
                    p += val_len;
                }

                Param << s;
                num_par++;
            }

            for ( int i = 0; (i < data_len); i++)
                buf[i] = *(p + i);
            if ((data_len > 0) || (header.len > 0))
                continue;

            if (header.paddingLen)
            {
                char s[256];
                int n = fcgi_read(s, header.paddingLen);
                if (n <= 0)
                {
                    err = __LINE__;
                    return -1;
                }
                header.paddingLen -= n;
            }

            if ((ret = fcgi_read_header()) != 8)
            {
                err = __LINE__;
                return -1;
            }

            if (header.type != FCGI_PARAMS)
            {
                fprintf(stderr, "<%s:%d> Error fcgi type: %d\n", __func__, __LINE__, header.type);
                err = __LINE__;
                return -1;
            }
        }

        if ((header.paddingLen))
        {
            fprintf(stderr, "!!!<%s:%d> Error paddingLen: %d\n", __func__, __LINE__, header.paddingLen);
            err = __LINE__;
            return -1;
        }

        return num_par;
    }
};

#endif
