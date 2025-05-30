#include <cstddef>
#include <iostream>

extern "C"{
 #include <wlr/util/log.h>
}

#define PROJECT_NAME "tiley"

int main(int argc, const char* argv[]){
    std::cout << "我们的桌面管理器从这里开始!" << std::endl;
    wlr_log_init(WLR_INFO, NULL);
    wlr_log(WLR_INFO, "这是一条来自wlroots的log打印, 如果你看到了这条打印信息, 说明wlroots依赖引用成功!");
}