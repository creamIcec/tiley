#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

// 表示一个用户配置集合。在合成器启动之前, 从各种文件资源加载配置, 组合成一个json对象, 保存在单例config对象中
// 在合成器运行时, 需要一个配置时, 只需要从这个对象中读取配置即可。

namespace tiley{
    class Config{
        public:
            // 获取一个配置的值
            template<typename T> 
            T get(const std::string& key, const T& default_value = T{});

            // 设置一个配置的值: 不常用, 除非合成器内部我们设置一个机制, 一般是外部程序修改json
            template<typename T>
            void set(const std::string& key, const T& value);

            Config(const char* configPath);
            Config(std::string configPath);
            Config();
            ~Config();
        private:
            std::unique_ptr<json> config;
            void load();
            void save();
    };
}