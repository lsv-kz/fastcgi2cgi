#include <sys/un.h>
#include "main.h"

//======================================================================
int create_server_socket(const char *host, const char *port, int backlog)
{
    int sockfd, n;
    const int sock_opt = 1;
    struct addrinfo  hints, *result, *rp;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((n = getaddrinfo(host, port, &hints, &result)) != 0)
    {
        fprintf(stderr, "Error getaddrinfo(%s:%s): %s\n", host, port, gai_strerror(n));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1)
            continue;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt));

        if (bind(sockfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;
        close(sockfd);
    }

    freeaddrinfo(result);

    if (rp == NULL)
    {
        fprintf(stderr, "Error: failed to bind\n");
        return -1;
    }

    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void *)&sock_opt, sizeof(sock_opt)); // SOL_TCP

    int flags = fcntl(sockfd, F_GETFD);
    if (flags == -1)
    {
        fprintf(stderr, "<%s:%d> Error fcntl(F_GETFD): %s\n", __func__, __LINE__, strerror(errno));
        close(sockfd);
        return -1;
    }

    flags |= FD_CLOEXEC;
    if (fcntl(sockfd, F_SETFD, flags) == -1)
    {
        fprintf(stderr, "<%s:%d> Error fcntl(F_SETFD, FD_CLOEXEC): %s\n", __func__, __LINE__, strerror(errno));
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, backlog) == -1)
    {
        fprintf(stderr, "Error listen(): %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }

    return sockfd;
}
//======================================================================
int write_timeout(int fd, const char *buf, int len, int timeout)
{
    int write_bytes = 0, ret;
    struct pollfd fdwr;
    
    fdwr.fd = fd;
    fdwr.events = POLLOUT;

    while (len > 0)
    {
        ret = poll(&fdwr, 1, timeout * 1000);
        if (ret == -1)
        {
            printf("<%s:%d> Error poll(): %s\n", __func__, __LINE__, strerror(errno));
            if (errno == EINTR)
                continue;
            return -1;
        }
        else if (!ret)
        {
            printf("<%s:%d> TimeOut poll(), tm=%d\n", __func__, __LINE__, timeout);
            return -1;
        }
        
        if (fdwr.revents != POLLOUT)
        {
            printf("<%s:%d> 0x%02x\n", __func__, __LINE__, fdwr.revents);
            return -1;
        }
        
        ret = write(fd, buf, len);
        if (ret == -1)
        {
            printf("<%s:%d> Error write(): %s\n", __func__, __LINE__, strerror(errno));
            if ((errno == EINTR) || (errno == EAGAIN))
                continue;
            return -1;
        }

        write_bytes += ret;
        len -= ret;
        buf += ret;
    }

    return write_bytes;
}
//======================================================================
int poll_in(int fd1, int fd2, int *ret1, int *ret2, int timeout)
{
    int tm, num_fd = 0;
    struct pollfd fdrd[2];

    *ret1 = *ret2 = 0;

    tm = (timeout == -1) ? -1 : (timeout * 1000);

    if (fd1 > 0)
    {
        fdrd[num_fd].fd = fd1;
        fdrd[num_fd].events = POLLIN;
        ++num_fd;
    }

    if (fd2 > 0)
    {
        fdrd[num_fd].fd = fd2;
        fdrd[num_fd].events = POLLIN;
        ++num_fd;
    }

    while (1)
    {
        int n = poll(fdrd, num_fd, tm);
        if (n == -1)
        {
            fprintf(stderr, "<%s:%d> Error poll(): %s\n", __func__, __LINE__, strerror(errno));
            if (errno == EINTR)
                continue;
            return -1;
        }
        else if (!n)
            return -1;

        if (fdrd[0].revents)
        {
            --n;
            if (fdrd[0].revents & POLLIN)
                *ret1 = 1;
            else
            {
                *ret1 = -1;
                //fprintf(stderr, "<%s:%d> stdout revents=0x%x\n", __func__, __LINE__, fdrd[0].revents);
            }
            if (n == 0)
                return 0;
        }

        if (fdrd[1].revents)
        {
            --n;
            if (fdrd[1].revents & POLLIN)
                *ret2 = 1;
            else
            {
                *ret2 = -1;
                //fprintf(stderr, "<%s:%d> stderr revents=0x%x\n", __func__, __LINE__, fdrd[1].revents);
            }
            return 0;
        }

        printf("<%s:%d> Error\n", __func__, __LINE__);
        break;
    }

    return -1;
}

