//**********************************************************
//          Net Utility 1.0
//
//  Description:
//  网络 基本 操作
//
// Author: Liu ZhaoRui
//         liuzhaorui1@163.com
//**********************************************************

#include "net.h"

int 
readn(int fd, void *usrbuf, size_t n)
{
    int nleft = n;
    int nread = 0;
    char * buf = (char *)usrbuf;
    
    while(nleft > 0)
    {
        nread = read(fd, buf, nleft);
        if(nread == 0)
        {
            return -1;
        }
        else if(nread < 0)
        {
            if(errno == EAGAIN)
            {
                break;
            }
            if(errno == EINTR)
            {
                nread = 0;
                continue;
            }
            return -1;
        }
        nleft -= nread;
        buf += nread;
    }
    return n - nleft;
}

ssize_t 
recvn_to_ms(int fd, void *ptr, size_t nbytes, int msecs)
{
    struct timeval tv;
    struct timeval old_tv;
    ssize_t nread;
    int sockflag;
    int ret;
    int timeuse = 0;
    socklen_t oplen = (socklen_t) sizeof (tv);
    
    nread = recv(fd, ptr, nbytes, MSG_DONTWAIT);
    
    if(nread == 0)
        return 0;

    if((nread < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK) &&(errno != EINTR))
        return -1;
    if((size_t)nread == nbytes)
        return nread;
    if (nread < 0) 
    {
        nread = 0;
    }

    if (0 == msecs) 
    {
        errno = ETIMEDOUT;
        return -1;
    }
    if (msecs < 0) {
       msecs = INT_MAX; 
    }

    if(getsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &old_tv, &oplen) < 0)
        return -1;
    if((sockflag = fcntl(fd, F_GETFL, 0)) < 0)
        return -1;

    if (sockflag&O_NONBLOCK) 
    {
        if(fcntl(fd, F_SETFL, (sockflag)&(~O_NONBLOCK)) < 0)
            return -1;
    }

    struct timeval cur;
    struct timeval last;
    do 
    {
        tv.tv_sec = msecs/1000;
        tv.tv_usec = (msecs % 1000) * 1000;
        if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, oplen) < 0)
            return -1;
        gettimeofday(&cur, NULL);
        
        do 
        {
            ret = recv(fd, (char*)ptr + nread, nbytes - (size_t)nread, MSG_WAITALL); 
        } while (ret < 0 && errno == EINTR);
        
        if (ret < 0 && EAGAIN==errno) 
        {
            //没数据，超时了
            errno = ETIMEDOUT;
        }
        
        if (ret > 0 && nread + ret < (ssize_t) nbytes) 
        {
            gettimeofday(&last, NULL);
            //判断是不是真的超时了
            timeuse = ((last.tv_usec - cur.tv_usec)/1000 + (last.tv_sec - cur.tv_sec) * 1000);
            if (timeuse >= msecs) 
            {
                //真超时了
                errno = ETIMEDOUT;
                ret = -1;
                nread = -1;
            } 
            else 
            {
                //被中断了？继续读
                nread += ret;
                msecs -= timeuse;
            }
        }  
        else if (ret < 0) 
        {
            nread = -1;
        }
        else if (ret == 0)
        {
            ret = -1;
            nread= 0;
        } 
        else 
        {
            nread += ret;
        }
      //没写完，继续
    } while (ret > 0 && nbytes > (size_t)nread);

    if (sockflag & O_NONBLOCK) 
    {
        if(fcntl(fd, F_SETFL, sockflag) < 0)
            return -1;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &old_tv, oplen)<0 )
        return -1;

    return nread;
}

ssize_t
sendn(int fd, void *usrbuf, size_t n)
{
    int nleft = n;
    int nwrite = 0;
    char * buf = (char *)usrbuf;
    while(nleft > 0)
    {
        nwrite = write(fd, buf, nleft);
        if(nwrite < 0)
        {
            if(errno == EINTR)
            {
                nwrite = 0;
                continue;
            }
            if(errno == EAGAIN)
            {
                break;
            }
            return -1;
        }
        buf += nwrite;
        nleft -= nwrite;
    }
    return n - nleft;
}


int
readable_to(int fd, int msec)
{
    fd_set rset;
    struct timeval tv;
    
    FD_ZERO(&rset);
    FD_SET(fd, &rset);

    tv.tv_sec = msec/1000;
    tv.tv_usec = (msec%1000)*1000;

    return (select(fd+1, &rset, NULL, NULL, &tv));
}

int 
read_to_ms(int fd, void * ptr, size_t nbytes, int msec)
{
    int ret = readable_to(fd, msec);
    if(ret == 0)
    {
        errno= ETIMEDOUT;
        return -1;
    }
    ret = read(fd, ptr, nbytes);
    return ret;      
}

ssize_t 
sendn_to_ms(int fd, const void *ptr, size_t nbytes, int msecs)
{
    struct timeval tv;
    struct timeval old_tv;
    ssize_t nwrite = 0;
    int ret = 0;
    int sockflag = 0;
    int timeuse = 0;
    socklen_t oplen = sizeof(tv);

    nwrite = send(fd, ptr, nbytes, MSG_DONTWAIT);

    if (nwrite==(ssize_t)nbytes)
        return nwrite;
    if ((nwrite < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK) && (errno != EINTR))
        return -1;
    if (nwrite == 0)
        return 0;
    if (nwrite < 0) 
    {
        nwrite = 0;
    }

    //0的时候不希望会被堵塞住
    if (0 == msecs) 
    {
        errno = ETIMEDOUT;
        return -1;
    }
    //负数的时候我们要堵塞
    if (msecs < 0) 
    {
       msecs = INT_MAX; 
    }

    if(getsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &old_tv, &oplen) < 0)
        return -1;
    if((sockflag = fcntl(fd, F_GETFL, 0)) < 0) 
        return -1;

    if (sockflag & O_NONBLOCK) 
    {
        if(fcntl(fd, F_SETFL, (sockflag)&(~O_NONBLOCK)) < 0)
            return -1;
    }
    
    struct timeval cur;
    struct timeval last;
    do 
    {
        tv.tv_sec = msecs/1000;
        tv.tv_usec = (msecs%1000)*1000;
        gettimeofday(&cur, NULL);
        if(setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, oplen)<0)
            return -1; 
        do 
        { 
            ret = send(fd, (char*)ptr + nwrite, nbytes-(size_t)nwrite, MSG_WAITALL); 
        } while (ret < 0 && EINTR == errno);

        if (ret < 0 && EAGAIN==errno) 
        {
            //没数据，超时了
            errno = ETIMEDOUT;
        }

        if (ret > 0 && nwrite + ret < (ssize_t)nbytes) 
        {
            //判断是否真是超时
            gettimeofday(&last, NULL);
            timeuse = ((last.tv_usec - cur.tv_usec)/1000+(last.tv_sec - cur.tv_sec)*1000);
            if (timeuse >= msecs) 
            {
                //真的超时了
                errno = ETIMEDOUT;
                ret = -1;
                nwrite = -1;
            } 
            else 
            {
                //不是超时，一般是被中断了, 继续写
                msecs -= timeuse;
                nwrite += ret;
            }
        }
        else if (ret < 0) 
        {
            nwrite = -1;
        } 
        else 
        {
            nwrite += ret;
        }
    } while (ret > 0 && nbytes > (size_t)nwrite);
    
    if (fcntl(fd, F_SETFL, sockflag) < 0)
        return  -1;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &old_tv, oplen) < 0)
        return -1;
    
    return nwrite;
}

int 
set_fd(int fd, int flags)
{
    int val;
    int sockflag;
    if ((val = fcntl(fd, F_GETFL, 0)) < 0)
    {
        close(fd);
        return -1;
    }

    val |= flags;
    if ((fcntl(fd, F_SETFL, val)) < 0)
    {
        close(fd);
        return -1;
    }
    return 0;
}

int
set_fd_noblock(int fd)
{
    return set_fd(fd, O_NONBLOCK);
}

int 
set_clc_fd(int fd, int flags)
{
    int val;
    int sockflag;
    if ( (val = fcntl(fd, F_GETFL, 0)) < 0)
    {
        close(fd);
        return -1;
    }
    
    sockflag = val;
    val &= ~flags;
    if ((fcntl(fd, F_SETFL, val)) < 0)
    {
        close(fd);
        return -1;
    }
    return sockflag;
}

int
set_fd_block(int fd)
{
    return set_clc_fd(fd, O_NONBLOCK);
}

int
net_accept(int sockfd, struct sockaddr *sa, socklen_t * addrlen)
{
    int connfd = 0;
    do
    {
        connfd = accept(sockfd, sa, addrlen);
        if (connfd < 0) 
        {
            if (errno == EINTR)
                continue; 
            else 
                return -1;
        }
    } while(connfd < 0);
    return connfd;
}

int
net_tcplisten(int port, int queue)
{
    int listenfd;
    const int on = 1;
    struct sockaddr_in soin;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        return -1;
    }

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
        return -1;

    bzero(&soin, sizeof(soin));

    soin.sin_family = AF_INET;
    soin.sin_addr.s_addr = htonl(INADDR_ANY);
    soin.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *) &soin, sizeof(soin)) < 0) 
    {
        close(listenfd);
        return -1;
    }
    
    if(queue <= 0)
        queue = 5;

    if (listen(listenfd, queue) < 0) 
    {
        close(listenfd);
        return -1;
    }
    return listenfd;
}

int
net_connect_to_ms(int sockfd, struct sockaddr *sa, 
                  socklen_t socklen, int msecs, int isclose)
{
    if (msecs <= 0) 
        return net_connect_to_tv(sockfd, sa, socklen, NULL, isclose);
    else 
    {
        struct timeval tv;
        tv.tv_sec = msecs / 1000;
        tv.tv_usec = (msecs % 1000) * 1000;
        return net_connect_to_tv(sockfd, sa, socklen, &tv, isclose);
    }
    return 0;
}

int
net_connect_to_tv(int fd, struct sockaddr * sa, 
                socklen_t socklen, timeval * tv, int isclose)
{
    int sockflag;
    int n, error;
    socklen_t len;
    fd_set rset, wset;

    if(sa == NULL)
    {    
        close(fd);
        return -1;
    }

    error = 0;
    sockflag = set_fd_noblock(fd);

    n = connect(fd, sa, (socklen_t) socklen);
    if (n < 0) 
    {
        if (errno != EINPROGRESS) 
        {
            error = 1;
            goto done;
        }
    }
    if (n == 0) 
        goto done;

    FD_ZERO(&rset);
    FD_SET(fd, &rset);
    wset = rset;
    if((n = select(fd + 1, &rset, &wset, NULL, tv)) ==0)
    {
        errno = ETIMEDOUT;
        error = 1;
        goto done;
    }
    if (FD_ISSET(fd, &rset) || FD_ISSET(fd, &wset))
    {
        len = sizeof(error);
        if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
        {
            errno = error;
            error = 1;
            goto done;
        }
    }

  done:
    fcntl(fd, F_SETFL, sockflag);
    if (error) 
    {
        if(isclose)
            close(fd);
        return -1;
    }
    return 0;
}

int
set_tcp_sockaddr(const char * addr, int port, 
                 struct sockaddr_in * soin)
{
    soin->sin_family = AF_INET;
    soin->sin_port = htons(port);
    return inet_pton(AF_INET, addr, &(soin->sin_addr));
}

const char *
get_tcp_sockaddr(char * addr, int * port, 
                 struct sockaddr_in * soin, int len)
{
    *port = ntohs(soin->sin_port);
    return inet_ntop(AF_INET, &(soin->sin_addr), addr, len);
}

int 
net_ep_create(int size)
{
    return epoll_create(size);    
}

int 
net_ep_add(int epfd, 
           int fd, 
           int events, 
           void * ptr,
           int op)
{
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.data.ptr = ptr;
    ev.events = events | EPOLLHUP | EPOLLERR;
    return epoll_ctl(epfd, op, fd, &ev);
}

int 
net_ep_add_in(int epfd, int fd)
{
    int events = EPOLLIN | EPOLLHUP | EPOLLERR;
    return net_ep_add(epfd, fd, events);
}

int 
net_ep_add_in1(int epfd, int fd)
{
    int events = EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLONESHOT;
    return net_ep_add(epfd, fd, events);
}

int 
net_ep_del(int epfd, int fd)
{
    return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
}

void 
default_hand(int sig)
{
    std::cout << "default sig fun" << std::endl;
    return;
}

int
set_linger(int fd, int val)
{
    struct linger li;
    li.l_onoff = 1;
    li.l_linger = val;
    return setsockopt(fd, SOL_SOCKET, SO_LINGER, 
                      (const char*)&li, sizeof(li));
}

int
sendProto(int fd,
          void * head, 
          int hsize, 
          void * data, 
          int dsize)
{
    struct msghdr sData;
    memset(&sData, 0, sizeof(sData));
    struct iovec  ve[2];
    ve[0].iov_base = head;
    ve[0].iov_len  = hsize;
    ve[1].iov_base = data;
    ve[1].iov_len  = dsize;

    sData.msg_iov = ve;
    sData.msg_iovlen = 2;
    
    return sendmsg(fd, &sData, MSG_WAITALL);
}

int
find_line(char * req, int end, char label)
{
    for(int i = 0; i < end; i++)
        if(*(req + i) == label)
            return i;
    return -1;
}

void
move_forward(char * req, int start, int end)
{
    int j = 0;
    for(int i = start; i <= end; i++)
    {
        *(req + j) = *(req + i);
        j++;
    }
    return;
}


string int2string(uint32_t user_id)
{
    std::stringstream ss;
    ss << user_id;
    return ss.str();
}


std::vector<std::string> 
split(std::string s, std::string sep)
{
    std::vector<std::string> result;
    if(s.empty())
        return result;

    std::string::size_type start = s.find_first_not_of(sep);
    std::string::size_type common = 0;
    while(start != std::string::npos)
    {
        string tmp;
        common = s.find(sep, start);
        if( common != std::string::npos)
        {
            tmp = s.substr(start, common - start);
            start = common + sep.size();
        }
        else
        {
            tmp = s.substr(start);
            start = common;
        }
        if(!tmp.empty()) 
            result.push_back(tmp); 
    }
    return result;
}
