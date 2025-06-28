#include "include/server.hpp"
#include "include/image_util.hpp"
#include <memory>
#include <mutex>
#include <wayland-util.h>

using namespace tiley;

// 静态成员初始化
std::unique_ptr<TileyServer, TileyServer::ServerDeleter> TileyServer::INSTANCE = nullptr;
std::once_flag TileyServer::onceFlag;

TileyServer::TileyServer(){

}

TileyServer::~TileyServer(){
    
}

TileyServer& TileyServer::getInstance(){
    std::call_once(onceFlag, [](){
        INSTANCE.reset(new TileyServer());
    });
    return *INSTANCE;
}