#pragma once

#include "src/lib/input/Seat.hpp"
#include "src/lib/client/views/LayerView.hpp"
#include "src/lib/scene/Scene.hpp"
#include "src/lib/client/render/Shader.hpp"

#include <GLES2/gl2.h>
#include <LOutput.h>
#include <mutex>
#include <vector>

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

            // temp
            bool is_compositor_modifier_down = false;

            // TODO: config loading function
            bool load_config();

            // TODO: manage shader scripts in one place
            // Window round corner shader
            inline Shader* roundedCornerShader() const { return m_roundedCornerShader.get(); }

            GLuint quadVBO() const { return m_quadVBO; }
            GLuint quadEBO() const { return m_quadEBO; }

            void initOpenGLResources();
            void uninitOpenGLResources();
            void initKeyEventHandlers();

            void populateStartupCMD(std::string cmd);
            
            inline const std::vector<std::string> getStartupCMD() const { return startUpCMD; }
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
            // command to execute after Tiley launches
            std::vector<std::string> startUpCMD;

            GLuint m_quadVBO { 0 }; // 顶点缓冲对象
            GLuint m_quadEBO { 0 }; // 索引缓冲对象
    };
}