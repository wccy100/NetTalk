#ifndef _CLIENT_H
#define _CLIENT_H

#include "../interface/thread.h"
#include "../interface/ireactor.h"

class ClientSvr
{

    public:
        ClientSvr(): _listenUserThread(NULL),
                     _coreAct(NULL)
        {};

        virtual ~ClientSvr();
    
        virtual int init(IReactor *, WorkerThread *);

        virtual int run();

        virtual int stop();
        
        virtual int destroy();

        virtual int extUserInput(void * data, int len);

    protected:
        WorkerThread * _listenUserThread;
        IReactor     * _coreAct; 
};

#endif