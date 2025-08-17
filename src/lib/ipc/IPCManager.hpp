#pragma once

#include <memory>
#include <mutex>
#include <list>
#include <string>
#include <wayland-server-core.h>

#include <LNamespaces.h>

using namespace Louvre;

namespace tiley {

    class IPCManager {
        public:
            struct IPCClient {
                int fd = -1;
                bool subscribed_to_workspace = false;
                struct wl_event_source* read_event_source = nullptr;
            };

            struct IPCMessage {
                uint32_t type;
                std::string payload;
            };

            struct IPCManagerDeleter {
                void operator()(IPCManager* ptr) const {
                    delete ptr;
                }
            };

            static IPCManager& getInstance();
            void initialize();
            void broadcastWorkspaceUpdate(UInt32 currentWorkspace, UInt32 totalWorkspaces, IPCClient* targetClient = nullptr);
        private:
            IPCManager();
            ~IPCManager();

            static std::unique_ptr<IPCManager, IPCManagerDeleter> INSTANCE;
            static std::once_flag onceFlag;

            int m_socket_fd;
            struct wl_event_source* m_listen_event_source;
            std::list<IPCClient> m_clients;

            static int handleNewConnection(int fd, uint32_t mask, void* data);
            static int handleClientMessage(int fd, uint32_t mask, void* data);

            void handleMessage(IPCClient& client, const IPCMessage& message);
            void handleGetWorkspaces(IPCClient& client);
            void handleGetTree(IPCClient& client);
            void handleGetOutputs(IPCClient& client);
            void handleSubscribe(IPCClient& client, const std::string& payload);
            void sendMessage(IPCClient& client, const std::string& message);
            void disconnectClient(IPCClient& client);
            
            std::string createIPCPacket(uint32_t type, const std::string& payload);
        };

}
