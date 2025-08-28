#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <LClient.h>

using namespace Louvre;

class Client final : public LClient{
    public:
        using LClient::LClient;
        // TODO: relocate to a better class
        void pong(UInt32 serial) override;
        
};


#endif //__CLIENT_H__