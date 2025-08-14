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
#include <sys/stat.h>

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

constexpr UInt32 IPC_COMMAND = 0;
constexpr UInt32 IPC_GET_WORKSPACES = 4;
constexpr UInt32 IPC_SUBSCRIBE = 2;

IPCManager::IPCManager() : m_socket_fd(-1), m_listen_event_source(nullptr) {
    LLog::log("[IPC] IPCManager 构造函数调用");
}

IPCManager& IPCManager::getInstance() {
    std::call_once(onceFlag, []() {
        LLog::log("[IPC] 创建 IPCManager 单例实例");
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

    LLog::log("[IPC] 数据包创建完成，总大小: %zu 字节", packet.size());
    return packet;
}

json createWorkspaceList(UInt32 currentWorkspace, UInt32 totalWorkspaces) {
    json workspaces = json::array();

    for (UInt32 i = 0; i < totalWorkspaces; ++i) {
        bool is_focused = (i == currentWorkspace);
        
        // 注意：sway/i3 的工作区编号通常从 1 开始，而你的实现从 0 开始。
        // 我们在 JSON 中使用 i + 1 作为 num 和 name，以更好地兼容 waybar。
        UInt32 workspace_num = i + 1;

        json workspace = json::object({
            {"id", i}, // id 可以是任意唯一标识符
            {"num", workspace_num},
            {"name", std::to_string(workspace_num)},
            {"visible", is_focused}, // 或者根据你的合成器逻辑判断
            {"focused", is_focused},
            {"urgent", false},
            {"rect", {
                {"x", 0},
                {"y", 0},
                {"width", 1920},
                {"height", 1080}
            }},
            {"output", "eDP-1"}, // 保持简单
            {"representation", "H[]"} // 保持简单
            // ... 你可以根据需要添加其他字段，但以上是核心字段
        });
        workspaces.push_back(workspace);
    }
    
    LLog::log("[IPC] 创建了包含 %u 个工作区的完整列表", totalWorkspaces);
    return workspaces;
}

json createWorkspaceEvent(UInt32 currentWorkspace, UInt32 totalWorkspaces) {
    json workspace_list = createWorkspaceList(currentWorkspace, totalWorkspaces);
    json current_ws = nullptr;

    // 找到 focused=true 的工作区
    for (const auto& ws : workspace_list) {
        if (ws["focused"] == true) {
            current_ws = ws;
            break;
        }
    }

    // 构建事件
    json event = {
        {"change", "init"},
        {"current", current_ws},
        {"old", nullptr}
    };

    return event;
}

void IPCManager::initialize() {
    LLog::log("[IPC] ========== IPC 管理器初始化开始 ==========");

    // 创建socket
    LLog::log("[IPC] 创建 UNIX socket...");
    m_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_socket_fd < 0) {
        LLog::fatal("[IPC] 无法创建Socket连接: %s", strerror(errno));
        return;
    }
    LLog::log("[IPC] Socket 创建成功，fd: %d", m_socket_fd);
    
    // 设置socket选项
    int optval = 1;
    if (setsockopt(m_socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        LLog::warning("[IPC] 设置 SO_REUSEADDR 失败: %s", strerror(errno));
    }
    
    // 设置socket为非阻塞模式
    int flags = fcntl(m_socket_fd, F_GETFL, 0);
    if (fcntl(m_socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        LLog::warning("[IPC] 设置非阻塞模式失败: %s", strerror(errno));
    }
    LLog::log("[IPC] Socket 配置完成（非阻塞 + SO_REUSEADDR）");

    // 绑定到文件系统路径
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    
    const char* xdg_runtime = getenv("XDG_RUNTIME_DIR");
    std::string socket_path;
    
    if (xdg_runtime) {
        socket_path = std::string(xdg_runtime) + "/sway-ipc.sock";
    } else {
        socket_path = "/tmp/sway-ipc.sock";
    }
    
    LLog::log("[IPC] Socket 路径: %s", socket_path.c_str());
    LLog::log("[IPC] XDG_RUNTIME_DIR: %s", xdg_runtime ? xdg_runtime : "未设置");
    
    // 设置环境变量让 waybar 能找到我们
    setenv("SWAYSOCK", socket_path.c_str(), 1);
    LLog::log("[IPC] SWAYSOCK 环境变量设置为: %s", socket_path.c_str());
    
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
    
    // 删除旧socket文件
    if (unlink(socket_path.c_str()) == 0) {
        LLog::log("[IPC] 删除旧的 socket 文件");
    }

    if (bind(m_socket_fd, (const sockaddr*)&addr, sizeof(addr)) < 0) {
        LLog::fatal("[IPC] 无法绑定到Socket: %s (路径: %s)", strerror(errno), socket_path.c_str());
        close(m_socket_fd);
        m_socket_fd = -1;
        return;
    }
    LLog::log("[IPC] Socket 绑定成功");

    // 设置socket文件权限
    if (chmod(socket_path.c_str(), 0666) < 0) {
        LLog::warning("[IPC] 设置socket权限失败: %s", strerror(errno));
    } else {
        LLog::log("[IPC] Socket 文件权限设置为 0666");
    }

    // 开始监听
    if (listen(m_socket_fd, 10) < 0) {  // 增加队列长度
        LLog::fatal("[IPC] 无法监听Socket: %s", strerror(errno));
        close(m_socket_fd);
        m_socket_fd = -1;
        return;
    }
    LLog::log("[IPC] Socket 开始监听，队列长度: 10");

    // 注册到Louvre事件循环
    m_listen_event_source = wl_event_loop_add_fd(
        compositor()->eventLoop(),
        m_socket_fd,
        WL_EVENT_READABLE,
        &IPCManager::handleNewConnection,
        this
    );

    if (!m_listen_event_source) {
        LLog::fatal("[IPC] 无法注册到事件循环");
        close(m_socket_fd);
        m_socket_fd = -1;
        return;
    }

    // 验证socket文件
    struct stat st;
    if (stat(socket_path.c_str(), &st) == 0) {
        LLog::log("[IPC] Socket 文件验证成功 - 大小: %ld, 权限: %o", st.st_size, st.st_mode & 0777);
    } else {
        LLog::error("[IPC] Socket 文件验证失败: %s", strerror(errno));
    }

    LLog::log("[IPC] ========== IPC 服务器启动完成 ==========");
    LLog::log("[IPC] 监听地址: %s", socket_path.c_str());
    LLog::log("[IPC] 服务器socket fd: %d", m_socket_fd);
    LLog::log("[IPC] 等待客户端连接...");
}

IPCManager::~IPCManager() {
    LLog::log("[IPC] IPCManager 析构开始");
    
    if (m_listen_event_source) {
        wl_event_source_remove(m_listen_event_source);
        LLog::log("[IPC] 事件源已移除");
    }
    
    if (m_socket_fd >= 0) {
        close(m_socket_fd);
        LLog::log("[IPC] 服务器socket已关闭");
        
        // 清理socket文件
        const char* xdg_runtime = getenv("XDG_RUNTIME_DIR");
        std::string socket_path = xdg_runtime ? 
            std::string(xdg_runtime) + "/sway-ipc.sock" : "/tmp/sway-ipc.sock";
        unlink(socket_path.c_str());
        LLog::log("[IPC] Socket文件已清理: %s", socket_path.c_str());
    }
    
    // 清理所有客户端连接
    LLog::log("[IPC] 清理 %zu 个客户端连接", m_clients.size());
    for (auto& client : m_clients) {
        if (client.read_event_source) {
            wl_event_source_remove(client.read_event_source);
        }
        if (client.fd >= 0) {
            close(client.fd);
        }
    }
    m_clients.clear();
    
    LLog::log("[IPC] IPCManager 析构完成");
}

int IPCManager::handleNewConnection(int fd, uint32_t mask, void *data) {
    L_UNUSED(fd);
    L_UNUSED(mask);

    LLog::log("[IPC] ========== 新连接事件触发 ==========");
    
    IPCManager* self = static_cast<IPCManager*>(data);
    
    // 接受连接前的状态检查
    LLog::log("[IPC] 服务器socket fd: %d, 当前客户端数量: %zu", self->m_socket_fd, self->m_clients.size());
    
    int client_fd = accept(self->m_socket_fd, nullptr, nullptr);
    
    if (client_fd < 0) {
        LLog::error("[IPC] 接受客户端连接失败: %s (errno: %d)", strerror(errno), errno);
        return 0;
    }
    
    LLog::log("[IPC] ✓ 新客户端连接成功 (fd: %d)", client_fd);
    
    // 获取socket详细信息
    struct sockaddr_un client_addr;
    socklen_t addr_len = sizeof(client_addr);
    if (getpeername(client_fd, (struct sockaddr*)&client_addr, &addr_len) == 0) {
        LLog::log("[IPC] 客户端信息 - family: %d", client_addr.sun_family);
    }
    
    // 设置客户端socket为非阻塞模式
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        LLog::warning("[IPC] 设置客户端非阻塞模式失败: %s", strerror(errno));
    } else {
        LLog::log("[IPC] 客户端socket设置为非阻塞模式");
    }
    
    // 创建客户端对象
    IPCClient& client = self->m_clients.emplace_back();
    client.fd = client_fd;
    client.subscribed_to_workspace = false;
    
    // 注册客户端读事件
    client.read_event_source = wl_event_loop_add_fd(
        compositor()->eventLoop(),
        client_fd,
        WL_EVENT_READABLE,
        &IPCManager::handleClientMessage,
        self
    );
    
    if (!client.read_event_source) {
        LLog::error("[IPC] 无法注册客户端读事件");
        close(client_fd);
        self->m_clients.pop_back();
        return 0;
    }
    
    LLog::log("[IPC] 客户端已注册到事件循环，总客户端数: %zu", self->m_clients.size());
    LLog::log("[IPC] ========== 新连接处理完成 ==========");
    
    return 0;
}

int IPCManager::handleClientMessage(int fd, uint32_t mask, void *data) {
    L_UNUSED(mask);
    
    LLog::log("[IPC] ========== 客户端消息事件 (fd: %d) ==========", fd);
    
    IPCManager* self = static_cast<IPCManager*>(data);
    
    LLog::log("[IPC] 总客户端数: %zu", self->m_clients.size());
    LLog::log("[IPC] 查找目标客户端 fd: %d", fd);
    
    auto it = std::find_if(self->m_clients.begin(), self->m_clients.end(),
                          [fd](const IPCClient& client) { return client.fd == fd; });
    
    if (it == self->m_clients.end()) {
        LLog::error("[IPC] 未找到客户端 fd: %d", fd);
        
        // 显示所有现有的 fd 用于调试
        LLog::log("[IPC] 现有客户端 fd 列表:");
        for (const auto& client : self->m_clients) {
            LLog::log("[IPC]   - fd: %d (subscribed: %s)", 
                      client.fd, client.subscribed_to_workspace ? "是" : "否");
        }
        return 0;
    }
    
    IPCClient& client = *it;
    LLog::log("[IPC] 找到目标客户端 (fd: %d)", client.fd);

    // 读取消息头
    char header[14];
    ssize_t header_bytes = recv(fd, header, 14, MSG_PEEK);
    
    LLog::log("[IPC] MSG_PEEK 结果: %zd 字节", header_bytes);
    
    if (header_bytes <= 0) {
        if (header_bytes == 0) {
            LLog::log("[IPC] 客户端正常关闭连接 (fd: %d)", fd);
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            LLog::log("[IPC] 暂无数据可读 (fd: %d)", fd);
            return 0;
        } else {
            LLog::log("[IPC] 读取错误 (fd: %d): %s", fd, strerror(errno));
        }
        self->disconnectClient(client);
        return 0;
    }
    
    if (header_bytes < 14) {
        LLog::log("[IPC] 消息头不完整，等待更多数据: %zd/14 字节", header_bytes);
        return 0;
    }
    
    LLog::log("[IPC] 收到完整消息头，开始解析...");
    
    // 打印原始消息头
    LLog::log("[IPC] 原始消息头十六进制:");
    for (int i = 0; i < 14; i++) {
        printf("%02x ", (unsigned char)header[i]);
    }
    printf("\n");
    
    // 检查是否是关闭消息
    if (memcmp(header, "close-", 6) == 0) {
        LLog::log("[IPC] 收到客户端关闭消息 (fd: %d)", fd);
        char discard[14];
        recv(fd, discard, 14, 0);
        self->disconnectClient(client);
        return 0;
    }
    
    // 检查标准的i3-ipc消息
    if (memcmp(header, "i3-ipc", 6) != 0) {
        LLog::error("[IPC] 无效的消息格式 - 魔数错误");
        LLog::error("[IPC] 期望: i3-ipc, 实际: %.6s", header);
        self->disconnectClient(client);
        return 0;
    }
    
    LLog::log("[IPC] 魔数验证通过：i3-ipc");
    
    // 实际读取消息头
    ssize_t actual_read = recv(fd, header, 14, 0);
    if (actual_read != 14) {
        LLog::error("[IPC] 读取消息头失败: %zd/14 字节", actual_read);
        self->disconnectClient(client);
        return 0;
    }
    
    LLog::log("[IPC] 消息头读取完成: %zd 字节", actual_read);
    
    uint32_t length, type;
    memcpy(&length, header + 6, 4);
    memcpy(&type, header + 10, 4);

    LLog::log("[IPC] 消息头解析:");
    LLog::log("[IPC]   魔数: %.6s", header);
    LLog::log("[IPC]   长度字节: %02x %02x %02x %02x → %u", 
              header[6], header[7], header[8], header[9], length);
    LLog::log("[IPC]   类型字节: %02x %02x %02x %02x → %u", 
              header[10], header[11], header[12], header[13], type);

    // 验证消息长度
    if (length > 65536) {  // 64KB 限制
        LLog::error("[IPC] 消息长度异常: %u 字节，拒绝处理", length);
        self->disconnectClient(client);
        return 0;
    }
    
    LLog::log("[IPC] 消息长度验证通过: %u 字节", length);

    // 读取消息体
    std::string payload;
    if (length > 0) {
        LLog::log("[IPC] 开始读取消息体...");
        std::vector<char> buffer(length);
        ssize_t body_bytes = recv(fd, buffer.data(), length, 0);
        
        LLog::log("[IPC] 消息体读取结果: %zd/%u 字节", body_bytes, length);
        
        if (body_bytes != (ssize_t)length) {
            LLog::error("[IPC] 读取消息体失败: %zd/%u 字节", body_bytes, length);
            if (body_bytes < 0) {
                LLog::error("[IPC] 读取错误: %s", strerror(errno));
            }
            self->disconnectClient(client);
            return 0;
        }
        payload = std::string(buffer.data(), length);
        LLog::log("[IPC] 消息体读取成功");
    } else {
        LLog::log("[IPC] 无消息体（长度为0）");
    }
    
    LLog::log("[IPC] 完整消息接收成功:");
    LLog::log("[IPC]   类型: %u", type);
    LLog::log("[IPC]   长度: %u", length);
    LLog::log("[IPC]   内容: '%s'", payload.c_str());
    
    // 🔥 修复：创建 IPCMessage 结构体并调用正确的方法
    IPCMessage message;
    message.type = type;
    message.payload = payload;
    
    LLog::log("[IPC] 开始处理消息...");
    self->handleMessage(client, message);
    LLog::log("[IPC] ========== 客户端消息处理完成 ==========");
    
    return 0;
}

void IPCManager::handleMessage(IPCClient& client, const IPCMessage& message) {
    LLog::log("[IPC] ========== 消息处理开始 ==========");
    LLog::log("[IPC] 消息类型: %u, 负载: '%s'", message.type, message.payload.c_str());
    
    switch (message.type) {
        case 1:
            LLog::log("[IPC] 🔄 处理 GET_WORKSPACES 请求 (类型 1)");
            handleGetWorkspaces(client);
            break;
            
        case IPC_SUBSCRIBE:  // 2
            LLog::log("[IPC] 🔄 处理 SUBSCRIBE 请求");
            handleSubscribe(client, message.payload);
            break;
            
        case 3:  // GET_OUTPUTS
            LLog::log("[IPC] 🔄 处理 GET_OUTPUTS 请求 (类型 3)");
            handleGetOutputs(client);
            break;
            
        case 4:  // GET_TREE
            LLog::log("[IPC] 🔄 处理 GET_TREE 请求 (类型 4)");
            handleGetTree(client);
            break;
            
        default: {
            LLog::error("[IPC] ❌ 未知的消息类型: %u", message.type);
            
            json error_json = json::object();
            error_json["success"] = false;
            error_json["error"] = "Unknown message type";
            
            std::string response = error_json.dump();
            std::string packet = createIPCPacket(message.type, response);
            sendMessage(client, packet);
            break;
        }
    }
    
    LLog::log("[IPC] ========== 消息处理结束 ==========");
}

void IPCManager::handleGetOutputs(IPCClient& client) {
    LLog::log("[IPC] 处理 GET_OUTPUTS 请求");
    
    json outputs = json::array();
    json output = json::object();
    output["name"] = "eDP-1";
    output["make"] = "Unknown";
    output["model"] = "Unknown";
    output["serial"] = "Unknown";
    output["active"] = true;
    output["primary"] = true;
    output["scale"] = 1.0;
    output["subpixel_hinting"] = "rgb";
    output["transform"] = "normal";
    
    json rect = json::object();
    rect["x"] = 0;
    rect["y"] = 0;
    rect["width"] = 1920;
    rect["height"] = 1080;
    output["rect"] = rect;
    
    outputs.push_back(output);
    
    std::string response = outputs.dump();
    std::string packet = createIPCPacket(3, response);
    sendMessage(client, packet);
}

void IPCManager::handleGetTree(IPCClient& client) {
    LLog::log("[IPC] 处理 GET_TREE 请求");
    
    json tree = json::object();
    tree["id"] = 1;
    tree["name"] = "root";
    tree["type"] = "root";
    tree["nodes"] = json::array();
    
    std::string response = tree.dump();
    std::string packet = createIPCPacket(4, response);
    sendMessage(client, packet);
}



void IPCManager::handleGetWorkspaces(IPCClient& client) {
    LLog::log("[IPC] ========== GET_WORKSPACES 处理开始 ==========");
    
    try {
        auto& manager = TileyWindowStateManager::getInstance();
        UInt32 current = manager.currentWorkspace();
        current += 1;
        
        json workspaces = createWorkspaceList(current, manager.WORKSPACES);
        
        if (!workspaces.is_array()) {
            LLog::error("[IPC] ❌ 返回的不是数组！");
            throw std::runtime_error("Not an array");
        }
        
        std::string response = workspaces.dump();
        LLog::log("[IPC] 📋 GET_WORKSPACES 响应 JSON:");
        LLog::log("[IPC] %s", response.c_str());
        
        std::string packet = createIPCPacket(1, response);
        sendMessage(client, packet);
        
        LLog::log("[IPC] GET_WORKSPACES 响应已发送");
        
    } catch (const std::exception& e) {
        LLog::error("[IPC] GET_WORKSPACES 处理异常: %s", e.what());
        
        // 紧急响应
        std::string emergency_response = R"([{"id":1,"name":"1","focused":true,"visible":true,"urgent":false,"output":"eDP-1","num":1}])";
        std::string packet = createIPCPacket(1, emergency_response);
        sendMessage(client, packet);
    }
    
    LLog::log("[IPC] ========== GET_WORKSPACES 处理完成 ==========");
}

void IPCManager::handleSubscribe(IPCClient& client, const std::string& payload) {
    LLog::log("[IPC] ========== SUBSCRIBE 处理开始 ==========");
    LLog::log("[IPC] 订阅请求负载: '%s'", payload.c_str());
    
    bool subscribed_to_workspace_event = false;

    try {
        auto events = json::parse(payload);
        if (events.is_array()) {
            for (const auto& event : events) {
                if (event.is_string() && event.get<std::string>() == "workspace") {
                    client.subscribed_to_workspace = true;
                    subscribed_to_workspace_event = true;
                    LLog::log("[IPC] 客户端 (fd: %d) 订阅了 workspace 事件", client.fd);
                }
            }
        }
        
        // 发送订阅成功的回应
        std::string response = "{\"success\": true}";
        std::string packet = createIPCPacket(IPC_REPLY_SUBSCRIBE, response);
        sendMessage(client, packet);
        LLog::log("[IPC] 订阅成功响应已发送给 fd: %d", client.fd);
        
        if (subscribed_to_workspace_event) {
            LLog::log("[IPC] 触发向新订阅者发送初始工作区状态...");
            auto& manager = TileyWindowStateManager::getInstance();
            broadcastWorkspaceUpdate(manager.currentWorkspace(), manager.WORKSPACES, &client);
        }
        
    } catch (const std::exception& e) {
        LLog::error("[IPC] 订阅处理异常: %s", e.what());
    }
    
    LLog::log("[IPC] ========== SUBSCRIBE 处理完成 ==========");
}



void IPCManager::sendMessage(IPCClient& client, const std::string& message) {
    LLog::log("[IPC] ========== 发送消息开始 (fd: %d) ==========", client.fd);
    
    if (client.fd < 0) {
        LLog::error("[IPC] 无效的客户端 fd: %d", client.fd);
        return;
    }
    
    LLog::log("[IPC] 消息大小: %zu 字节", message.size());
    
    // 详细的十六进制输出
    LLog::log("[IPC] 消息十六进制内容:");
    for (size_t i = 0; i < std::min(message.size(), size_t(64)); i++) {
        printf("%02x ", (unsigned char)message[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (message.size() > 64) {
        printf("... (truncated, total: %zu bytes)", message.size());
    }
    printf("\n");
    
    // 解析发送的包头
    if (message.size() >= 14) {
        uint32_t sent_length, sent_type;
        memcpy(&sent_length, message.data() + 6, 4);
        memcpy(&sent_type, message.data() + 10, 4);
        LLog::log("[IPC] 发送包头 - 魔数: %.6s, 长度: %u, 类型: %u", 
                  message.data(), sent_length, sent_type);
    }
    
    // 检查socket状态
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(client.fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
        if (error != 0) {
            LLog::error("[IPC] Socket 有错误状态: %s", strerror(error));
            disconnectClient(client);
            return;
        }
    }
    
    LLog::log("[IPC] 开始发送数据...");
    
    // 发送数据
    size_t sent_total = 0;
    int retry_count = 0;
    const int max_retries = 5;
    
    while (sent_total < message.size() && retry_count < max_retries) {
        ssize_t sent = send(client.fd, message.data() + sent_total, 
                           message.size() - sent_total, MSG_NOSIGNAL);
        
        LLog::log("[IPC] send() 返回: %zd", sent);
        
        if (sent < 0) {
            LLog::log("[IPC] 发送错误: %s (errno: %d)", strerror(errno), errno);
            
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LLog::log("[IPC] Socket 缓冲区满，等待重试... (第 %d 次)", retry_count + 1);
                usleep(1000);  // 等待1毫秒
                retry_count++;
                continue;
            } else if (errno == EPIPE || errno == ECONNRESET) {
                LLog::log("[IPC] 连接已断开: %s", strerror(errno));
                disconnectClient(client);
                return;
            } else {
                LLog::error("[IPC] 发送失败: %s", strerror(errno));
                disconnectClient(client);
                return;
            }
        }
        
        sent_total += sent;
        LLog::log("[IPC] 已发送: %zu/%zu 字节", sent_total, message.size());
        retry_count = 0;  // 重置重试计数
    }
    
    if (sent_total == message.size()) {
        LLog::log("[IPC] 消息发送成功: %zu 字节", sent_total);
        fsync(client.fd);
    } else {
        LLog::error("[IPC] 发送不完整: %zu/%zu 字节", sent_total, message.size());
        disconnectClient(client);
        return;
    }
    
    LLog::log("[IPC] ========== 发送消息完成 ==========");
}

void IPCManager::broadcastWorkspaceUpdate(UInt32 currentWorkspace, UInt32 totalWorkspaces, IPCClient* targetClient) {
    if (!targetClient && m_clients.empty()) {
        LLog::log("[IPC] 无客户端连接，跳过广播");
        return;
    }

    /*
    json workspaces_payload = createWorkspaceList(currentWorkspace, totalWorkspaces);
    std::string payload = workspaces_payload.dump();
    std::string packet = createIPCPacket(IPC_EVENT_WORKSPACE, payload);
    */
    
    json event_payload = createWorkspaceEvent(currentWorkspace, totalWorkspaces);
    std::string payload = event_payload.dump();
    std::string packet = createIPCPacket(IPC_EVENT_WORKSPACE, payload);

    if (targetClient) {
        // 发送给单个目标客户端
        if (targetClient->subscribed_to_workspace) {
            LLog::log("[IPC] 向刚订阅的客户端 (fd: %d) 发送初始工作区状态", targetClient->fd);
            sendMessage(*targetClient, packet);
        }
    } else {
        // 广播给所有订阅的客户端
        LLog::log("[IPC] ========== 广播工作区更新给所有订阅者 ==========");
        int subscribed_count = 0;
        for (auto& client : m_clients) {
            if (client.subscribed_to_workspace) {
                LLog::log("[IPC] 向客户端 (fd: %d) 发送工作区事件", client.fd);
                sendMessage(client, packet);
                subscribed_count++;
            }
        }
        LLog::log("[IPC] 工作区更新广播完成 - 发送给 %d/%zu 个客户端", subscribed_count, m_clients.size());
    }
}

void IPCManager::disconnectClient(IPCClient& client) {
    LLog::log("[IPC] ========== 客户端断开连接 (fd: %d) ==========", client.fd);
    
    if (client.read_event_source) {
        wl_event_source_remove(client.read_event_source);
        client.read_event_source = nullptr;
        LLog::log("[IPC] 客户端事件源已移除");
    }
    
    if (client.fd >= 0) {
        LLog::log("[IPC] 关闭客户端socket: %d", client.fd);
        if (close(client.fd) < 0) {
            LLog::warning("[IPC] 关闭socket失败: %s", strerror(errno));
        }
        client.fd = -1;
    }
    
    // 从客户端列表中移除
    auto it = std::find_if(m_clients.begin(), m_clients.end(),
                          [&client](const IPCClient& c) { return &c == &client; });
    if (it != m_clients.end()) {
        bool was_subscribed = it->subscribed_to_workspace;
        m_clients.erase(it);
        LLog::log("[IPC] 客户端已从列表移除 (曾订阅: %s)", was_subscribed ? "是" : "否");
    }
    
    LLog::log("[IPC] 剩余客户端数量: %zu", m_clients.size());
    LLog::log("[IPC] ========== 客户端断开处理完成 ==========");
}