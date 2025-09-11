#include "GridLayout.hpp"
#include "LNamespaces.h"

#include <LLog.h>
#include <LCursor.h>
#include <cmath>

using namespace tiley;

void GridLayout::increaseRowAndColumn(){

    if(row_next >= row_max){
        LLog::log("Maximum count of windows in one workspace reached.");
        // TODO: Stack windows that are not fit in layout
        return;
    }

    if(column_next >= column_max - 1){
        column_next = 0;
        row_next += 1;
    } else {
        column_next += 1;
    }

}

bool GridLayout::insert(UInt32 workspace, GridContainer* newWindowContainer){
    // 1. get next anchor for container
    // 2. insert into vector
    // 3. recalculate

    if(containers.size() == 0){
        newWindowContainer->anchor = {0,0};
        newWindowContainer->size = {1,1};
        containers[workspace].push_back(newWindowContainer);
    } else {
        newWindowContainer->anchor = {column_next, row_next};
        newWindowContainer->size = {1,1};
        containers[workspace].push_back(newWindowContainer);
    }

    increaseRowAndColumn();

    return true;
}

bool GridLayout::recalculate(){

    // Get first container of a workspace
    Output* rootOutput = nullptr;

    rootOutput = 
    static_cast<ToplevelRole*>(containers[CURRENT_WORKSPACE].at(0)->getWindow())->output;

    if (!rootOutput) {
        rootOutput = static_cast<Output*>(cursor()->output());
    }
    if (!rootOutput) {
        LLog::warning("[recalculate]: no monitor available for workspace id %u, giving up", CURRENT_WORKSPACE);
        return false;
    }

    const LRect& availableGeometry = rootOutput->availableGeometry();

    bool reflowSuccess = false;
    reflow(CURRENT_WORKSPACE, availableGeometry, reflowSuccess);

    if(reflowSuccess){
        // TODO
    }

    return reflowSuccess;
}

void GridLayout::reflow(UInt32 workspace, const LRect& region, bool& success){

    for(auto container : containers[workspace]){

        UInt32 column = container->anchor.x();
        UInt32 row = container->anchor.y();
        UInt32 count = containers[workspace].size();

        bool flagRowFill = floor((row * column_max + column) / column_max) == 
                           floor(count / column_max);
        bool flagColumnFill = (row * column_max + column + 1 == count);

        container->size.setH(flagColumnFill ? (row_max - row) : 1);
        container->size.setW(flagRowFill ? (column_max - column) : 1);

        // TODO: reapply geometry and distribute geometry changed event to client
        

        success = true;

    }
}