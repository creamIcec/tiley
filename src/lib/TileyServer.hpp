#pragma once

#include "src/lib/input/Seat.hpp"
#include "src/lib/scene/LayerView.hpp"
#include "src/lib/scene/Scene.hpp"

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
    };
}