#include "netReactor.h"
#include "netConn.h"

void ReactorAccept(int listenFd, short listenEv, void * arg)
{
    NetReactor * nrc = (NetReactor *)arg;
    if(nrc->isStop())
    {
        event lis = nrc->getListenEv();
        event_del(&lis);
        Log::NOTICE("SVR is STOP, don not accept");
        return;
    }

    struct sockaddr sa;
    socklen_t slen = sizeof(sa);
    int connfd = net_accept(listenFd, &sa, &slen);

    struct bufferevent ** pool;
    pool = nrc->getFdPool();
    int maxConnected = nrc->getMaxConnected(); 
    in_addr ipaddr = ((struct sockaddr_in *)&sa)->sin_addr;
    char *peer_ip = inet_ntoa(ipaddr);

    if(connfd > maxConnected)
    {
        Log::ERROR("Too much user, connfd : %d", connfd);
        close(connfd);
        return;
    }
   
    NetConnManager * cMag = NetConnManager::getInstance();  

    if(cMag->IsUserConnExist(connfd))
    {
        Log::ERROR("assign a working fd, connfd : %d, will clean it", 
                   connfd);
    }

    struct bufferevent * bev;
    set_fd_noblock(connfd);
    bev = bufferevent_new(connfd, 
                          EvReadCallback, 
                          NULL, 
                          EvErrorCallback, 
                          nrc);
    bev->timeout_read = nrc->getReadTo();
    bev->timeout_write = nrc->getWriteTo();
    bufferevent_enable(bev, EV_READ|EV_WRITE);    


    std::unique_ptr<Iconn> con(new UserConn(nrc, bev)); 
    cMag->setConn(connfd, std::move(con));

    /* use for test */
    //cMag->addAppConnFd(connfd); 

    Log::NOTICE("ACCEPT Connect SUCC, connfd : %d, UserIP : %s", connfd, peer_ip); 
}

void EvReadCallback(struct bufferevent * bev, void * arg)
{
    NetReactor * nrc = (NetReactor *)arg;
    int connfd = (bev->ev_read).ev_fd;
    NetConnManager * cMag = NetConnManager::getInstance();
    Iconn * conn =  cMag->getConn(connfd);
    if(conn != NULL) 
        conn->OnRead(NULL, 0);
    else
        Log::ERROR("buffer %d conn not exists match userconn", connfd);

    return;
}

void EvWriteCallback(struct bufferevent * bev,void * arg)
{
    printf("write callback %d\n", (bev->output)->off);
}

void EvErrorCallback(struct bufferevent * bev,
                     short  error, 
                     void * arg)
{
    int connfd = (bev->ev_read).ev_fd;
    NetReactor * nrc = (NetReactor *)arg;
    NetConnManager * cmag = NetConnManager::getInstance();

    if(error & EVBUFFER_EOF)
    {
        Log::NOTICE("Client close connection connfd: %d", connfd);
        printf("Client close connection connfd %d\n", connfd);
        if(cmag->IsUserConnExist(connfd))
            if(cmag->delConn(connfd) > 0)
                return;
        close(connfd);
    }
    else if(error & EVBUFFER_ERROR)
    {
        Log::WARN("connection error connfd: %d error: ", connfd, strerror(errno));
        if(cmag->IsUserConnExist(connfd))
            if(cmag->delConn(connfd)>0)
                return;
        close(connfd);
    }
    else if(error & EVBUFFER_TIMEOUT)
    {
        Log::WARN("connection Timeout connfd: %d", connfd);
        if(cmag->IsUserConnExist(connfd))
            if(cmag->delConn(connfd)>0)
                return;
        close(connfd);
    }
    return ;
}



NetReactor::NetReactor(const Section &sec)
{
    _fdPool = NULL;
    _status = IReactor::NONE;
    _port = (int)atoi((sec.get("listenPort").c_str()));
    _maxConnected = (int)atoi((sec.get("maxConnect").c_str()));
    maxConnected  = _maxConnected;
    _logicIP.push_back(sec.get("logicSvr"));
    _readTimeout = ((int)atoi(sec.get("readTimeout").c_str())); 
    _writeTimeout = ((int)atoi(sec.get("writeTimeout").c_str())); 
}


NetReactor::~NetReactor()
{}

int 
NetReactor::NetReactor::init()
{
    event_init();

    Log::NOTICE("Listen Port %d", _port);

    _listenFd = net_tcplisten(_port, 100);
    set_fd_noblock(_listenFd);

    event_set(&_listener, _listenFd, EV_READ|EV_PERSIST, ReactorAccept, this);
    event_add(&_listener, NULL);

    /********test app conn ********/

    NetConnManager * cMag = NetConnManager::getInstance();
    string appSvrIp("127.0.0.1");
    struct sockaddr_in svr_address1, svr_address2;
    set_tcp_sockaddr(appSvrIp.c_str(),
                     1090,
                     &svr_address1);
    set_tcp_sockaddr(appSvrIp.c_str(),
                     1090,
                     &svr_address2);

    int appFd1 = socket(AF_INET, SOCK_STREAM, 0);
    int appFd2 = socket(AF_INET, SOCK_STREAM, 0);
    int res = net_connect_to_ms(appFd1,
                               (struct sockaddr *)&svr_address1,
                                sizeof(svr_address1),
                                1000, 1); 
    if(res < 0)
    {
        Log::ERROR("INIT ERROR: CAN not connect to appSvr");
        close(appFd1);
        return -1;
    }
    res = net_connect_to_ms(appFd2,
                           (struct sockaddr *)&svr_address2,
                            sizeof(svr_address2),
                            1000, 1); 
    if(res < 0)
    {
        Log::ERROR("INIT ERROR: CAN not connect to appSvr");
        close(appFd2);
        return -1;
    }
    vector<int> fdVec;
    vector<struct sockaddr_in> fdAddr;
    fdVec.push_back(appFd1);
    fdVec.push_back(appFd2);
    fdAddr.push_back(svr_address1);
    fdAddr.push_back(svr_address2);

    for(int i = 0; i < fdVec.size(); i++)
    {
        printf("init conn  %d %d \n", fdVec[0],fdVec[1]);
        struct bufferevent * bev;
        bev = bufferevent_new(fdVec[i],
                              EvReadCallback,
                              NULL,
                              EvErrorCallback,
                              this);
        bev->timeout_read = 1800;
        bev->timeout_write = 1800;
        bufferevent_enable(bev, EV_READ|EV_WRITE);

        
        std::unique_ptr<Iconn> con(new AppConn(this, bev, appSvrIp, 1090)); 
        cMag->setConn(fdVec[i], std::move(con));
        cMag->addAppConnFd(fdVec[i]); 
    }
    Log::NOTICE("Connect to App Svr succ"); 

    /* end  test app conn*/
}

int
NetReactor::run()
{
    event_dispatch();
    _status = IReactor::RUNNING;
}

int
NetReactor::Stop()
{
    _status = IReactor::STOP;
}

bool
NetReactor::isStop()
{
    if(_status == IReactor::STOP)
        return true;
    return false;
}

int
NetReactor::status()
{
    return _status;
}

