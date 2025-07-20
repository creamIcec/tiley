#include "TileyWindowStateManager.hpp"

using namespace tiley;

void TileyWindowStateManager::reflow(UInt32 workspace, LSize& region){
    return;
}


TileyWindowStateManager& TileyWindowStateManager::getInstance(){
    std::call_once(onceFlag, [](){
        INSTANCE.reset(new TileyWindowStateManager());
    });
    return *INSTANCE;
}

std::unique_ptr<TileyWindowStateManager, TileyWindowStateManager::WindowStateManagerDeleter> TileyWindowStateManager::INSTANCE = nullptr;
std::once_flag TileyWindowStateManager::onceFlag;

TileyWindowStateManager::TileyWindowStateManager(){}
TileyWindowStateManager::~TileyWindowStateManager(){}