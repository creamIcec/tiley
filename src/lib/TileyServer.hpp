#pragma once

#include "src/lib/input/Seat.hpp"
#include "src/lib/client/views/LayerView.hpp"
#include "src/lib/scene/Scene.hpp"
#include "src/lib/client/render/Shader.hpp"

#include <GLES2/gl2.h>
#include <LOutput.h>
#include <mutex>

#include "TileyCompositor.hpp"


namespace tiley{
    class TileyServer{
        public:

            Seat* seat() noexcept{
                return (Seat*)Louvre::seat();
            }

            TileyCompositor *compositor() noexcept{
                return (TileyCompositor*)Louvre::compositor();
            }

            Scene &scene() noexcept;

            LayerView* layers() noexcept;

            static TileyServer& getInstance();

            // 记录是否合成器修饰键被按下
            bool is_compositor_modifier_down = false;

            // TODO: 配置加载函数
            bool load_config();

            // TODO: 统一管理shader着色脚本
            // 获取窗口圆角的渲染脚本
            inline Shader* roundedCornerShader() const { return m_roundedCornerShader.get(); }

            GLuint quadVBO() const { return m_quadVBO; }
            GLuint quadEBO() const { return m_quadEBO; }

            // 初始化OpenGL渲染脚本
            void initOpenGLResources();
            void uninitOpenGLResources();

        private:
            struct ServerDeleter {
                void operator()(TileyServer* p) const {
                    delete p;
                }
            };

            friend struct ServerDeleter;

            TileyServer();
            ~TileyServer();
            static std::unique_ptr<TileyServer, ServerDeleter> INSTANCE;
            static std::once_flag onceFlag;
            TileyServer(const TileyServer&) = delete;
            TileyServer& operator=(const TileyServer&) = delete;

            std::unique_ptr<Shader> m_roundedCornerShader;
            GLuint m_quadVBO { 0 }; // 顶点缓冲对象
            GLuint m_quadEBO { 0 }; // 索引缓冲对象
    };
}