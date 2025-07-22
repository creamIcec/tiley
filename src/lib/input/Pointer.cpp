#include "Pointer.hpp"

#include "src/lib/TileyServer.hpp"
#include "src/lib/surface/Surface.hpp"
#include "src/lib/types.hpp"

#include <LNamespaces.h>
#include <LLog.h>
#include <LSeat.h>
#include <LPointer.h>
#include <LView.h>

using namespace tiley;

void Pointer::pointerButtonEvent(const LPointerButtonEvent &event){
    LPointer::pointerButtonEvent(event);
    
    LLog::log("鼠标点击");

    Surface* surface = static_cast<Surface*>(seat()->pointer()->focus());

    LView* targetWrapperView = nullptr;

    if(!surface){
        return;
    }

    if(!surface->parent() && surface->toplevel()){
        LLog::log("是窗口");
        targetWrapperView = surface->getWindowView();
    }else if(((Surface*)surface->topmostParent())->wrapperView){
        LLog::log("是窗口的subsurface");
        targetWrapperView = ((Surface*)surface->topmostParent())->wrapperView.get();
    }

    if(targetWrapperView){

        // 判断点击的目标窗口是不是活动窗口
        // 只有当不是活动窗口时才提升。已经是活动窗口则不提升。
        

        LLog::log("准备提升");
        // 我该干什么?
        LLog::log("父容器的子节点数: %zu", targetWrapperView->parent()->children().size());

        const auto& siblings = targetWrapperView->parent()->children();
        if(!siblings.empty() && siblings.back() != targetWrapperView){
            // 找到最顶上的那个兄弟
            LView* topMostSibling = siblings.back();
            // 把自己插到它的后面
            targetWrapperView->insertAfter(topMostSibling);
        }
    }
}