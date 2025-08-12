#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <LClient.h>

using namespace Louvre;

class Client final : public LClient{
    public:
        using LClient::LClient;
        // wlroots窗口无响应检测机制迁移
        void pong(UInt32 serial) override;
        
};


#endif //__CLIENT_H__