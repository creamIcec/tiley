#include "IPCManager.hpp"
#include "LNamespaces.h"
#include "src/lib/TileyWindowStateManager.hpp"
#include <cstdint>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <nlohmann/json.hpp> 
#include <LLog.h>

using json = nlohmann::json;
using namespace tiley;
using namespace Louvre;

std::unique_ptr<IPCManager, IPCManager::IPCManagerDeleter> IPCManager::INSTANCE = nullptr;
std::once_flag IPCManager::onceFlag;

// IPC命令常量
const uint32_t IPC_COMMAND_GET_WORKSPACES = 1;
const uint32_t IPC_REPLY_GET_WORKSPACES = 1;
const uint32_t IPC_COMMAND_SUBSCRIBE = 2;
const uint32_t IPC_REPLY_SUBSCRIBE = 2;
const uint32_t IPC_EVENT_WORKSPACE = 0 | (1 << 31);  //0x80000000

IPCManager::IPCManager() : m_socket_fd(-1), m_listen_event_source(nullptr) {}

IPCManager& IPCManager::getInstance() {
    std::call_once(onceFlag, []() {
        INSTANCE.reset(new IPCManager());
    });
    return *INSTANCE;
}

// 创建符合i3/sway IPC协议的二进制数据包
std::string IPCManager::createIPCPacket(uint32_t type, const std::string& payload) {
    const char* magic = "i3-ipc";
    uint32_t payload_length = payload.length();

    LLog::log("[IPC] 创建数据包: 类型=%u, 长度=%u, payload=%s", 
              type, payload_length, payload.c_str());
    
    std::string packet;
    packet.reserve(14 + payload_length);
    packet.append(magic, 6);
    packet.append(reinterpret_cast<const char*>(&payload_length), 4);
    packet.append(reinterpret_cast<const char*>(&type), 4);
    packet.append(payload);

    return packet;
}

json createWorkspaceList(UInt32 currentWorkspace, UInt32 totalWorkspaces){
    json workspaces = json::array();
    
    for (UInt32 i = 1; i <= totalWorkspaces; i++) {
        json workspace = json::object();
        
        workspace["num"] = i - 1; 
        workspace["name"] = std::to_string(i);
        workspace["visible"] = true;  // 简化: 假设所有工作区都可见
        workspace["focused"] = (i == currentWorkspace);
        workspace["urgent"] = false;
        
        // rect 必须有所有4个字段
        json rect = json::object();
        rect["x"] = 0;
        rect["y"] = 0; 
        rect["width"] = 1920;
        rect["height"] = 1080;
        workspace["rect"] = rect;
        
        workspace["output"] = "HDMI-A-1";
        
        workspaces.push_back(workspace);
    }
    
    return workspaces;
}

// 修改初始化函数，确保使用正确的socket路径
void IPCManager::initialize() {

    // 创建socket
    m_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_socket_fd < 0) {
        LLog::fatal("[IPC] 无法创建Socket连接");
        return;
    }
    
    // 设置socket为非阻塞模式
    int flags = fcntl(m_socket_fd, F_GETFL, 0);
    fcntl(m_socket_fd, F_SETFL, flags | O_NONBLOCK);

    // 绑定到文件系统路径
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    
    // 使用 sway 兼容的路径命名
    const char* xdg_runtime = getenv("XDG_RUNTIME_DIR");
    std::string socket_path;
    
    if (xdg_runtime) {
        socket_path = std::string(xdg_runtime) + "/sway-ipc.sock";  // 改为 sway-ipc.sock
    } else {
        socket_path = "/tmp/sway-ipc.sock";
    }
    
    // 设置环境变量让 waybar 能找到我们
    setenv("SWAYSOCK", socket_path.c_str(), 1);
    
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
    unlink(socket_path.c_str());  // 删除旧socket文件

    if (bind(m_socket_fd, (const sockaddr*)&addr, sizeof(addr)) < 0) {
        LLog::fatal("[IPC] 无法绑定到Socket: %s", strerror(errno));
        close(m_socket_fd);
        m_socket_fd = -1;
        return;
    }

    // 开始监听
    if (listen(m_socket_fd, 5) < 0) {
        LLog::fatal("[IPC] 无法监听Socket: %s", strerror(errno));
        close(m_socket_fd);
        m_socket_fd = -1;
        return;
    }

    // 注册到Louvre事件循环
    m_listen_event_source = wl_event_loop_add_fd(
        compositor()->eventLoop(),
        m_socket_fd,
        WL_EVENT_READABLE,
        &IPCManager::handleNewConnection,
        this
    );

    LLog::log("[IPC] IPC服务器监听: %s", socket_path.c_str());
    
    // 显示环境变量, 帮助调试
    LLog::log("[IPC] IPC服务器监听: %s", socket_path.c_str());
    LLog::log("[IPC] SWAYSOCK 环境变量已设置为: %s", socket_path.c_str());
}

IPCManager::~IPCManager() {
    if (m_listen_event_source) {
        wl_event_source_remove(m_listen_event_source);
    }
    
    if (m_socket_fd >= 0) {
        close(m_socket_fd);
        unlink("/tmp/tiley-ipc.sock");
    }
    
    // 清理所有客户端连接
    for (auto& client : m_clients) {
        if (client.read_event_source)
            wl_event_source_remove(client.read_event_source);
        if (client.fd >= 0)
            close(client.fd);
    }
    m_clients.clear();
}

int IPCManager::handleNewConnection(int fd, uint32_t mask, void *data) {
    L_UNUSED(fd);
    L_UNUSED(mask);

    IPCManager* self = static_cast<IPCManager*>(data);
    int client_fd = accept(self->m_socket_fd, nullptr, nullptr);
    
    if (client_fd < 0) {
        LLog::warning("[IPC] 接受客户端连接失败");
        return 0;
    }
    
    // 设置非阻塞模式
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    LLog::log("[IPC] 接受来自客户端的连接 (fd: %d)", client_fd);
    
    IPCClient& client = self->m_clients.emplace_back();
    client.fd = client_fd;
    client.read_event_source = wl_event_loop_add_fd(
        compositor()->eventLoop(),
        client_fd,
        WL_EVENT_READABLE,
        &IPCManager::handleClientMessage,
        self
    );
    
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
    
    // 读取消息头
    char header[14];
    ssize_t header_bytes = recv(fd, header, 14, MSG_PEEK);
    if (header_bytes <= 0) {
        if (header_bytes == 0 || errno != EAGAIN) {
            LLog::log("[IPC] 客户端断开连接 (fd: %d)", fd);
            self->disconnectClient(client);
        }
        return 0;
    }
    
    if (header_bytes < 14) {
        return 0; // 等待完整消息头
    }
    
    // 检查是否是关闭消息
    if (memcmp(header, "close-", 6) == 0) {
        LLog::log("[IPC] 收到客户端关闭消息，优雅断开连接 (fd: %d)", fd);
        // 读取并丢弃这个消息
        char discard[14];
        recv(fd, discard, 14, 0);
        
        // 优雅地关闭连接
        self->disconnectClient(client);
        return 0;
    }
    
    // 检查标准的i3-ipc消息
    if (memcmp(header, "i3-ipc", 6) != 0) {
        LLog::warning("[IPC] 无效的消息格式 - 魔数错误: %.6s", header);
        self->disconnectClient(client);
        return 0;
    }
    
    // 正常读取并处理i3-ipc消息
    recv(fd, header, 14, 0);  // 实际读取消息头
    
    uint32_t length, type;
    memcpy(&length, header + 6, 4);
    memcpy(&type, header + 10, 4);

    LLog::log("[IPC] 解析消息头 - 原始字节: %02x %02x %02x %02x (长度), %02x %02x %02x %02x (类型)", 
            header[6], header[7], header[8], header[9],
            header[10], header[11], header[12], header[13]);
    LLog::log("[IPC] 解析结果: length=%u, type=%u", length, type);

    // 读取消息体
    std::string payload;
    if (length > 0) {
        std::vector<char> buffer(length);
        ssize_t body_bytes = recv(fd, buffer.data(), length, 0);
        if (body_bytes != (ssize_t)length) {
            LLog::warning("[IPC] 读取消息体失败");
            self->disconnectClient(client);
            return 0;
        }
        payload = std::string(buffer.data(), length);
    }
    
    LLog::log("[IPC] 收到消息类型: %u, 长度: %u, 内容: %s", 
              type, length, payload.c_str());
    
    self->handleMessage(client, type, payload);
    return 0;
}

void IPCManager::handleMessage(IPCClient& client, uint32_t type, const std::string& payload) {
    LLog::log("[IPC] 开始处理消息: type=%u, payload='%s'", type, payload.c_str());
    
    try {
        switch (type) {
            case 1:  // GET_WORKSPACES
                LLog::log("[IPC] 处理GET_WORKSPACES请求");
                handleGetWorkspaces(client);
                LLog::log("[IPC] GET_WORKSPACES处理完成");
                break;
                
            case 2:  // SUBSCRIBE
                LLog::log("[IPC] 处理SUBSCRIBE请求");
                handleSubscribe(client, payload);
                LLog::log("[IPC] SUBSCRIBE处理完成");
                break;
                
            default:
                LLog::warning("[IPC] 未知的消息类型: %u", type);
                std::string error_response = "{\"success\":false,\"error\":\"Unknown message type\"}";
                std::string packet = createIPCPacket(type, error_response);
                sendMessage(client, packet);
                break;
        }
    } catch (const std::exception& e) {
        LLog::error("[IPC] 处理消息时发生异常: %s", e.what());
    }
    
    LLog::log("[IPC] 消息处理结束");
}

void IPCManager::handleGetWorkspaces(IPCClient& client) {
    auto& manager = TileyWindowStateManager::getInstance();
    json workspaces = createWorkspaceList(manager.currentWorkspace(), manager.WORKSPACES);
    
    std::string response = workspaces.dump();
    LLog::log("[IPC] 发送工作区列表: %s", response.c_str());
    
    // 使用正确的回复类型
    std::string packet = createIPCPacket(IPC_REPLY_GET_WORKSPACES, response);  // GET_WORKSPACES的回复类型是1
    sendMessage(client, packet);
}

void IPCManager::handleSubscribe(IPCClient& client, const std::string& payload) {
    LLog::log("[IPC] 处理SUBSCRIBE请求: %s", payload.c_str());
    
    try {
        auto events = json::parse(payload);
        client.subscribed_to_workspace = false;
        bool subscribed_to_window = false;
        
        if (events.is_array()) {
            for (const auto& event : events) {
                if (event.is_string()) {
                    std::string event_name = event.get<std::string>();
                    if (event_name == "workspace") {
                        client.subscribed_to_workspace = true;
                        LLog::log("[IPC] 客户端订阅了workspace事件");
                    } else if (event_name == "window") {
                        subscribed_to_window = true;
                        LLog::log("[IPC] 客户端订阅了window事件");
                    }
                }
            }
        }
        
        // 只发送订阅成功响应, 不主动发送任何数据
        std::string response = "{\"success\":true}";
        std::string packet = createIPCPacket(2, response);
        sendMessage(client, packet);
        
        LLog::log("[IPC] 订阅完成，连接保持活跃，等待进一步请求");
        
    } catch (const std::exception& e) {
        LLog::warning("[IPC] 解析订阅消息失败: %s", e.what());
    }
}



// 修改sendMessage函数，确保所有数据都被写入
void IPCManager::sendMessage(IPCClient& client, const std::string& message) {
    if (client.fd < 0) return;
    
    // 添加详细的二进制输出
    LLog::log("[IPC] 准备发送消息，总长度: %zu 字节", message.size());
    LLog::log("[IPC] 消息的十六进制表示:");
    for (size_t i = 0; i < std::min(message.size(), size_t(50)); i++) {
        printf("%02x ", (unsigned char)message[i]);
        if ((i + 1) % 16 == 0) printf("\n                ");
    }
    printf("\n");
    
    // 解析我们自己发送的包头
    if (message.size() >= 14) {
        uint32_t sent_length, sent_type;
        memcpy(&sent_length, message.data() + 6, 4);
        memcpy(&sent_type, message.data() + 10, 4);
        LLog::log("[IPC] 发送包头解析: magic=%.*s, length=%u, type=%u", 
                  6, message.data(), sent_length, sent_type);
    }
    
    // 尝试一次性发送所有数据
    size_t sent_total = 0;
    while (sent_total < message.size()) {
        ssize_t sent = send(client.fd, message.data() + sent_total, 
                           message.size() - sent_total, MSG_NOSIGNAL);
        
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 如果socket缓冲区已满，等待一下再尝试
                usleep(1000);  // 等待1毫秒
                continue;
            } else {
                LLog::warning("[IPC] 发送消息失败 (fd: %d): %s", 
                             client.fd, strerror(errno));
                disconnectClient(client);
                return;
            }
        }
        
        sent_total += sent;
    }
    
    LLog::log("[IPC] 成功发送 %zu 字节的消息", message.size());
}

// 在broadcastWorkspaceUpdate中使用正确的事件类型
void IPCManager::broadcastWorkspaceUpdate(UInt32 currentWorkspace, UInt32 totalWorkspaces) {
    if (m_clients.empty()) return;
    
    json workspaces = createWorkspaceList(currentWorkspace, totalWorkspaces);
    std::string payload = workspaces.dump();
    
    // 使用正确的工作区事件类型
    std::string packet = createIPCPacket(IPC_EVENT_WORKSPACE, payload);
    
    LLog::log("[IPC] 广播工作区更新到 %zu 个客户端", m_clients.size());
    
    for (auto it = m_clients.begin(); it != m_clients.end();) {
        if (it->subscribed_to_workspace) {
            sendMessage(*it, packet);
            ++it;
        } else {
            ++it;
        }
    }
}

void IPCManager::disconnectClient(IPCClient& client) {
    LLog::log("[IPC] 断开客户端连接 (fd: %d)", client.fd);
    
    if (client.read_event_source) {
        wl_event_source_remove(client.read_event_source);
        client.read_event_source = nullptr;
    }
    
    if (client.fd >= 0) {
        close(client.fd);
        client.fd = -1;
    }
    
    // 从客户端列表中移除
    auto it = std::find_if(m_clients.begin(), m_clients.end(),
                          [&client](const IPCClient& c) { return &c == &client; });
    if (it != m_clients.end()) {
        m_clients.erase(it);
        LLog::log("[IPC] 客户端已从列表中移除，剩余客户端: %zu", m_clients.size());
    }
}
