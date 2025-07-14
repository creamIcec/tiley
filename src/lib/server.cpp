#include "include/server.hpp"
#include "include/image_util.hpp"
#include <memory>
#include <mutex>
#include <wayland-util.h>

using namespace tiley;

// 静态成员初始化
std::unique_ptr<_TileyServer, _TileyServer::ServerDeleter> _TileyServer::INSTANCE = nullptr;
std::once_flag _TileyServer::onceFlag;

_TileyServer::_TileyServer(){

}

_TileyServer::~_TileyServer(){
    
}

_TileyServer& _TileyServer::getInstance(){
    std::call_once(onceFlag, [](){
        INSTANCE.reset(new _TileyServer());
    });
    return *INSTANCE;
}