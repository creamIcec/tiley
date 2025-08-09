#include <memory>
#include <mutex>
#include "src/lib/TileyServer.hpp"
#include "src/lib/scene/Scene.hpp"
#include "TileyCompositor.hpp"

using namespace tiley;

// 静态成员初始化
std::unique_ptr<TileyServer, TileyServer::ServerDeleter> TileyServer::INSTANCE = nullptr;
std::once_flag TileyServer::onceFlag;

TileyServer::TileyServer(){}
TileyServer::~TileyServer(){}

TileyServer& TileyServer::getInstance(){
    std::call_once(onceFlag, [](){
        INSTANCE.reset(new TileyServer());
    });
    return *INSTANCE;
}

Scene& TileyServer::scene() noexcept{
    return compositor()->scene;
}

LayerView* TileyServer::layers() noexcept{
    return scene().layers;
}