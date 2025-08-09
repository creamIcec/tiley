#include "Config.hpp"
#include <exception>
#include <iostream>

using namespace tiley;

Config::Config(){
    
}

template<typename T>
T Config::get(const std::string& key, const T& default_value){
    try{
        if(config->contains(key)){
            return (*config)[key].get<T>();
        }
    } catch (const std::exception& e){
        std::cerr << "获取配置项失败 '" << key << "': " << e.what() << std::endl;
    }
    return default_value;
}

template<typename T>
void Config::set(const std::string& key, const T& value){
    (*config)[key] = value;
}

void Config::load(){
    if(config){
        
    }
}