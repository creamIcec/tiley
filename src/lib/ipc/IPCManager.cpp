#include "IPCManager.hpp"
#include "src/lib/TileyWindowStateManager.hpp"
#include "LCompositor.h"
#include "LLog.h"

#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdint>
#include <cstdlib>

using json = nlohmann::json;
using namespace tiley;
using namespace Louvre;

std::unique_ptr<IPCManager, IPCManager::IPCManagerDeleter> IPCManager::INSTANCE = nullptr;
std::once_flag IPCManager::onceFlag;

constexpr UInt32 IPC_GET_WORKSPACES = 1;
constexpr UInt32 IPC_SUBSCRIBE = 2;
constexpr UInt32 IPC_GET_OUTPUTS = 3;
constexpr UInt32 IPC_GET_TREE = 4;
constexpr UInt32 IPC_REPLY_SUBSCRIBE = 2;
constexpr UInt32 IPC_EVENT_WORKSPACE = 0 | (1 << 31);

IPCManager::IPCManager() : m_socket_fd(-1), m_listen_event_source(nullptr) {}

IPCManager::~IPCManager() {
    if (m_listen_event_source) {
        wl_event_source_remove(m_listen_event_source);
    }

    if (m_socket_fd >= 0) {
        close(m_socket_fd);
        const char* xdg_runtime = getenv("XDG_RUNTIME_DIR");
        std::string socket_path = xdg_runtime ? 
            std::string(xdg_runtime) + "/sway-ipc.sock" : "/tmp/sway-ipc.sock";
        unlink(socket_path.c_str());
    }

    for (auto& client : m_clients) {
        if (client.read_event_source) {
            wl_event_source_remove(client.read_event_source);
        }
        if (client.fd >= 0) {
            close(client.fd);
        }
    }
    m_clients.clear();
}

IPCManager& IPCManager::getInstance() {
    std::call_once(onceFlag, []() {
        INSTANCE.reset(new IPCManager());
    });
    return *INSTANCE;
}

std::string IPCManager::createIPCPacket(uint32_t type, const std::string& payload) {
    const char* magic = "i3-ipc";
    uint32_t payload_length = payload.length();
    
    std::string packet;
    packet.reserve(14 + payload_length);
    packet.append(magic, 6);
    packet.append(reinterpret_cast<const char*>(&payload_length), 4);
    packet.append(reinterpret_cast<const char*>(&type), 4);
    packet.append(payload);
    return packet;
}

json createWorkspaceList(UInt32 currentWorkspace, UInt32 totalWorkspaces) {
    json workspaces = json::array();
    for (UInt32 i = 0; i < totalWorkspaces; ++i) {
        bool is_focused = (i == currentWorkspace);
        UInt32 workspace_num = i + 1;
        workspaces.push_back({
            {"id", i},
            {"num", workspace_num},
            {"name", std::to_string(workspace_num)},
            {"visible", is_focused},
            {"focused", is_focused},
            {"urgent", false},
            {"rect", {{"x", 0}, {"y", 0}, {"width", 1920}, {"height", 1080}}},
            {"output", "eDP-1"}
        });
    }
    return workspaces;
}

json createWorkspaceEvent(UInt32 currentWorkspace, UInt32 totalWorkspaces) {
    json workspace_list = createWorkspaceList(currentWorkspace, totalWorkspaces);
    json current_ws = nullptr;
    for (const auto& ws : workspace_list) {
        if (ws["focused"] == true) {
            current_ws = ws;
            break;
        }
    }
    return {
        {"change", "init"},
        {"current", current_ws},
        {"old", nullptr}
    };
}

void IPCManager::initialize() {
    m_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_socket_fd < 0) {
        LLog::fatal("[IPCManager] unable to create socket connection address : %s", strerror(errno));
        return;
    }

    int optval = 1;
    setsockopt(m_socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    int flags = fcntl(m_socket_fd, F_GETFL, 0);
    fcntl(m_socket_fd, F_SETFL, flags | O_NONBLOCK);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    const char* xdg_runtime = getenv("XDG_RUNTIME_DIR");
    std::string socket_path = xdg_runtime ? 
        std::string(xdg_runtime) + "/sway-ipc.sock" : "/tmp/sway-ipc.sock";
    
    setenv("SWAYSOCK", socket_path.c_str(), 1);
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
    
    unlink(socket_path.c_str());

    if (bind(m_socket_fd, (const sockaddr*)&addr, sizeof(addr)) < 0) {
        LLog::fatal("[IPCManager]: unable to bind socket : %s (path: %s)", strerror(errno), socket_path.c_str());
        close(m_socket_fd);
        m_socket_fd = -1;
        return;
    }

    chmod(socket_path.c_str(), 0666);

    if (listen(m_socket_fd, 10) < 0) {
        LLog::fatal("[IPCManager]: unable to listen to socket: %s", strerror(errno));
        close(m_socket_fd);
        m_socket_fd = -1;
        return;
    }

    m_listen_event_source = wl_event_loop_add_fd(
        compositor()->eventLoop(),
        m_socket_fd,
        WL_EVENT_READABLE,
        &IPCManager::handleNewConnection,
        this
    );

    if (!m_listen_event_source) {
        LLog::fatal("[IPCManager]: Unable to initialize socket connection eventloop");
        close(m_socket_fd);
        m_socket_fd = -1;
    }
}

int IPCManager::handleNewConnection(int fd, uint32_t mask, void *data) {
    L_UNUSED(fd);
    L_UNUSED(mask);
    IPCManager* self = static_cast<IPCManager*>(data);
    
    int client_fd = accept(self->m_socket_fd, nullptr, nullptr);
    if (client_fd < 0) {
        LLog::error("[IPCManager] Unable to handle client connection request: %s", strerror(errno));
        return 0;
    }
    
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    IPCClient& client = self->m_clients.emplace_back();
    client.fd = client_fd;
    client.subscribed_to_workspace = false;
    
    client.read_event_source = wl_event_loop_add_fd(
        compositor()->eventLoop(),
        client_fd,
        WL_EVENT_READABLE,
        &IPCManager::handleClientMessage,
        self
    );
    
    if (!client.read_event_source) {
        LLog::error("[IPCManager]: Unable to register client socket to wayland");
        close(client_fd);
        self->m_clients.pop_back();
    }
    return 0;
}

int IPCManager::handleClientMessage(int fd, uint32_t mask, void *data) {
    L_UNUSED(mask);
    IPCManager* self = static_cast<IPCManager*>(data);
    
    auto it = std::find_if(self->m_clients.begin(), self->m_clients.end(),
                          [fd](const IPCClient& client) { return client.fd == fd; });
    
    if (it == self->m_clients.end()) {
        return 0;
    }
    
    IPCClient& client = *it;
    char header[14];
    ssize_t header_bytes = recv(fd, header, 14, MSG_PEEK);
    
    if (header_bytes <= 0) {
        self->disconnectClient(client);
        return 0;
    }
    
    if (header_bytes < 14) {
        return 0;
    }
    
    if (memcmp(header, "i3-ipc", 6) != 0) {
        self->disconnectClient(client);
        return 0;
    }
    
    ssize_t actual_read = recv(fd, header, 14, 0);
    if (actual_read != 14) {
        self->disconnectClient(client);
        return 0;
    }
    
    uint32_t length, type;
    memcpy(&length, header + 6, 4);
    memcpy(&type, header + 10, 4);

    if (length > 65536) {
        self->disconnectClient(client);
        return 0;
    }

    std::string payload;
    if (length > 0) {
        std::vector<char> buffer(length);
        ssize_t body_bytes = recv(fd, buffer.data(), length, 0);
        if (body_bytes != (ssize_t)length) {
            self->disconnectClient(client);
            return 0;
        }
        payload = std::string(buffer.data(), length);
    }
    
    IPCMessage message {type, payload};
    self->handleMessage(client, message);
    return 0;
}

void IPCManager::handleMessage(IPCClient& client, const IPCMessage& message) {
    switch (message.type) {
        case IPC_GET_WORKSPACES:
            handleGetWorkspaces(client);
            break;
        case IPC_SUBSCRIBE:
            handleSubscribe(client, message.payload);
            break;
        case IPC_GET_OUTPUTS:
            handleGetOutputs(client);
            break;
        case IPC_GET_TREE:
            handleGetTree(client);
            break;
        default: {
            json error_json { {"success", false}, {"error", "Unknown message type"} };
            std::string packet = createIPCPacket(message.type, error_json.dump());
            sendMessage(client, packet);
            break;
        }
    }
}

void IPCManager::handleGetOutputs(IPCClient& client) {
    json outputs = json::array();
    outputs.push_back({
        {"name", "eDP-1"}, {"make", "Unknown"}, {"model", "Unknown"},
        {"serial", "Unknown"}, {"active", true}, {"primary", true},
        {"scale", 1.0}, {"subpixel_hinting", "rgb"}, {"transform", "normal"},
        {"rect", {{"x", 0}, {"y", 0}, {"width", 1920}, {"height", 1080}}}
    });
    std::string packet = createIPCPacket(IPC_GET_OUTPUTS, outputs.dump());
    sendMessage(client, packet);
}

void IPCManager::handleGetTree(IPCClient& client) {
    json tree {
        {"id", 1}, {"name", "root"}, {"type", "root"}, {"nodes", json::array()}
    };
    std::string packet = createIPCPacket(IPC_GET_TREE, tree.dump());
    sendMessage(client, packet);
}

void IPCManager::handleGetWorkspaces(IPCClient& client) {
    try {
        auto& manager = TileyWindowStateManager::getInstance();
        json workspaces = createWorkspaceList(manager.currentWorkspace(), manager.WORKSPACES);
        std::string packet = createIPCPacket(IPC_GET_WORKSPACES, workspaces.dump());
        sendMessage(client, packet);
    } catch (const std::exception& e) {
        LLog::error("[IPCManager] Exception in handleGetWorkspaces: %s", e.what());
        std::string emergency_response = R"([{"id":1,"name":"1","focused":true}])";
        std::string packet = createIPCPacket(IPC_GET_WORKSPACES, emergency_response);
        sendMessage(client, packet);
    }
}

void IPCManager::handleSubscribe(IPCClient& client, const std::string& payload) {
    bool subscribed_to_workspace_event = false;
    try {
        auto events = json::parse(payload);
        if (events.is_array()) {
            for (const auto& event : events) {
                if (event.is_string() && event.get<std::string>() == "workspace") {
                    client.subscribed_to_workspace = true;
                    subscribed_to_workspace_event = true;
                }
            }
        }
        
        std::string response = "{\"success\": true}";
        std::string packet = createIPCPacket(IPC_REPLY_SUBSCRIBE, response);
        sendMessage(client, packet);
        
        // Send workspace switching event
        if (subscribed_to_workspace_event) {
            auto& manager = TileyWindowStateManager::getInstance();
            broadcastWorkspaceUpdate(manager.currentWorkspace(), manager.WORKSPACES, &client);
        }
    } catch (const std::exception& e) {
        LLog::error("[IPCManager] Error occurs when processing client subscription: %s", e.what());
    }
}

void IPCManager::sendMessage(IPCClient& client, const std::string& message) {
    if (client.fd < 0) return;

    size_t sent_total = 0;
    while (sent_total < message.size()) {
        ssize_t sent = send(client.fd, message.data() + sent_total, 
                           message.size() - sent_total, MSG_NOSIGNAL);
        
        if (sent < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                disconnectClient(client);
                return;
            }
            usleep(1000);
            continue;
        }
        sent_total += sent;
    }
}

void IPCManager::broadcastWorkspaceUpdate(UInt32 currentWorkspace, UInt32 totalWorkspaces, IPCClient* targetClient) {
    if (!targetClient && m_clients.empty()) {
        return;
    }

    json event_payload = createWorkspaceEvent(currentWorkspace, totalWorkspaces);
    std::string packet = createIPCPacket(IPC_EVENT_WORKSPACE, event_payload.dump());

    if (targetClient) {
        if (targetClient->subscribed_to_workspace) {
            sendMessage(*targetClient, packet);
        }
    } else {
        for (auto& client : m_clients) {
            if (client.subscribed_to_workspace) {
                sendMessage(client, packet);
            }
        }
    }
}

void IPCManager::disconnectClient(IPCClient& client) {
    if (client.read_event_source) {
        wl_event_source_remove(client.read_event_source);
        client.read_event_source = nullptr;
    }
    
    if (client.fd >= 0) {
        close(client.fd);
        client.fd = -1;
    }
    
    auto it = std::find_if(m_clients.begin(), m_clients.end(),
                          [&client](const IPCClient& c) { return &c == &client; });
    if (it != m_clients.end()) {
        m_clients.erase(it);
    }
}
