#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;
namespace tiley{

    // A class for representing user configuration
    class Config{
        public:
            // access to a config item
            template<typename T> 
            T get(const std::string& key, const T& default_value = T{});

            // change value of a config item, changing may not be common inside compositor but outside somewhere
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