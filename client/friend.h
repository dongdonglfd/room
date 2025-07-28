#include"chatclient.h"
#include"fileclient.h"
class Friend
{
    private:
    int sock;
    std::string currentUser;        // 当前登录用户
    std::vector<std::string> friendRequests; // 待处理的好友请求
    Chat chat;
    FileTransferClient file;
    // 发送请求并获取响应
    json sendRequest(const json& req) {
        std::string requestStr = req.dump();
        send(sock, requestStr.c_str(), requestStr.size(), 0);
        char buffer[4096] = {0};
        recv(sock, buffer, 4096, 0);
        return json::parse(buffer);
    }
    json sendReq(const json& req,int fd) {
        int sock=fd;
        std::string requestStr = req.dump();
        ssize_t bytes_sent = send(sock, requestStr.c_str(), requestStr.size(), 0);
        if (bytes_sent == -1) {
        perror("send failed");
        exit(EXIT_FAILURE);
    } 
        char buffer[4096] = {0};
        recv(sock, buffer, 4096, 0);
        return json::parse(buffer);
    }
    public:
    void friendMenu(int sockfd,std::string current_user) 
    {
        sock=sockfd;
        currentUser=current_user;
        while(true) {
            std::cout << "\n==== 好友管理 ====\n"
                 << "1. 添加好友\n"
                 << "2. 删除好友\n"
                 << "3. 屏蔽好友\n"
                 << "4. 查看好友列表\n"
                 << "5. 处理请求\n"
                 << "6. 与好友聊天\n"
                 << "7. 发送文件\n"
                 << "8. 接收文件\n"
                 << "9. 返回上级\n"
                 << "请选择操作: ";
            
            char choice;
            std::cin >> choice;
            
            switch(choice) {
                case '1': addFriend(); break;
                case '2': deleteFriend(); break;
                case '3': blockFriend(); break;//处理屏蔽
                case '4': showFriends(); break;
                case '5': processRequest();break;//处理请求
                case '6': showFriends();chat.privateChat(sock,currentUser);break;
                case '7': showFriends();file.sendFile(sock,currentUser);break;
                case '8': file.getUndeliveredFiles(sock,currentUser);break;
                case '9': return;
                default: std::cout << "无效输入!\n";
            }
        }
    }
    void addFriend()
    {
        std::cout << "\n=== 添加好友 ===\n";
        std::cout << "输入好友用户名：";
        std::string target;
        std::cin >> target;

        json req;
        req["type"] = "add_friend";
        req["from"] = currentUser;
        req["to"] = target;

        json res = sendRequest(req);
        
        if (res["success"]) {
            std::cout << "✔ 请求已发送\n";
            if (res["online"]) {
                std::cout << "（对方在线，已实时通知）\n";
            }
        } else {
            std::cerr << "✘ 错误：" << res["message"] << std::endl;
        }

    }
    void deleteFriend()
    {
        std::cout << "\n=== 删除好友 ===\n";
        std::cout << "输入要删除的好友用户名: ";
        std::string friendName;
        std::cin >> friendName;
        
        // 确认操作
        std::cout << "确定要删除好友 " << friendName << " 吗? (y/n): ";
        char confirm;
        std::cin >> confirm;
        if (confirm != 'y' && confirm != 'Y') {
            std::cout << "操作取消\n";
            return;
        }
        json req;
        req["type"] = "delete_friend";
        req["user"] = currentUser;
        req["friend"] = friendName;
        
        // 发送请求
        json res = sendRequest(req);
        
        if (res["success"]) {
            std::cout << "好友已删除\n";
        } else {
            std::cerr << "删除失败: " << res["message"] << std::endl;
        }

    }
    void blockFriend()
    {
        std::cout << "\n=== 屏蔽用户 ===\n";
        std::cout << "输入要屏蔽的用户名: ";
        std::string blockName;
        std::cin >> blockName;
        
        // 确认操作
        std::cout << "确定要屏蔽用户 " << blockName << " 吗? (y/n): ";
        char confirm;
        std::cin >> confirm;
        if (confirm != 'y' && confirm != 'Y') {
            std::cout << "操作取消\n";
            return;
        }
        // 构建屏蔽请求
        json req;
        req["type"] = "block_user";
        req["user"] = currentUser;
        req["blocked_user"] = blockName;
        
        // 发送请求
        json res = sendRequest(req);
        
        if (res["success"]) {
            std::cout << "✓ 用户已屏蔽\n";
        } else {
            std::cerr << "✘ 屏蔽失败: " << res["message"] << std::endl;
        }
    }
    void showFriends()
    {
        // 构建获取好友列表请求
        json req;
        req["type"] = "get_friends";
        req["user"] = currentUser;

        // 发送请求并获取响应
        json res = sendRequest(req);

        // 检查响应是否成功
        if (!res["success"]) {
            std::cerr << "获取好友列表失败: " << res["message"] << std::endl;
            return;
        }

        // 解析好友列表
        auto friendsList = res["friends"];
        
        if (friendsList.empty()) {
            std::cout << "暂无好友，去添加新朋友吧！\n";
            return;
        }
        // 显示好友列表标题
        std::cout << "\n==== 好 友 列 表 =====\n";
        std::cout << "序号\t用户名\t状态(0表示离线|1表示在线)\n";
        std::cout << "-------------------------------------\n";
        for(int i=0;i<friendsList.size();i++)
        {
            std::cout<<i+1<<"\t"<<friendsList[i]["username"].get<std::string>()<<"\t"<<friendsList[i]["online"].get<bool>()<<std::endl;
        }
    }
    void processRequest()
    {
        checkNotifications();
        if (friendRequests.empty()) {
            std::cout << "当前没有待处理的请求\n";
            return;
        }

        showPendingRequests();
        std::cout << "选择请求编号 (0取消): ";
        int choice;
        std::cin >> choice;

        if (choice <1 || choice>friendRequests.size()) return;

        std::string requester = friendRequests[choice-1];
        std::cout << "1. 接受\n2. 拒绝\n选择操作：";
        int action;
        std::cin >> action;

        json req;
        req["type"] = "process_request";
        req["from"] = requester;
        req["to"] = currentUser;
        req["action"] = (action ==1) ? "accept" : "reject";

        json res = sendRequest(req);
        if (res["success"]) {
            std::cout << "✔ 操作成功\n";
            if (action ==1) {
                std::cout << requester << " 已加入好友列表\n";
            }
        } else {
            std::cerr << "✘ 错误：" << res["message"] << std::endl;
        }
    }
    // 处理接收到的请求（后台线程）
    void checkNotifications() {
        json req;
        req["type"] = "check_requests";
        req["user"] = currentUser;

        json res = sendRequest(req);
        friendRequests = res["requests"].get<std::vector<std::string>>();
    }

    // 显示待处理请求
    void showPendingRequests() {
        std::cout << "\n=== 待处理请求 ===\n";
        for (size_t i=0; i<friendRequests.size(); ++i) {
            std::cout << "[" << i+1 << "] " << friendRequests[i] << std::endl;
        }
    }
    
    void checkUnreadMessages(std::string currentUser, int sockfd) 
    {
        json request = {
            {"type", "get_unread_messages"},
            {"user", currentUser}
        };

        json req = sendReq(request, sockfd);
        if (req.is_null() || !req.is_object()) {
            std::cerr << "Error: Invalid response from server!" << std::endl;
            return;
        }

        bool success = false;
        if (req.contains("success") && req["success"].is_boolean()) {
            success = req["success"].get<bool>();
        }

        if (success) {
            std::cout << "\n===== 私聊未读消息 =====" << std::endl;
            if (req.contains("messages") && req["messages"].is_array()) {
                for (const auto& msg : req["messages"]) {
                    // 处理 timestamp
                    // time_t timestamp = 0;
                    // if (msg.contains("timestamp") && msg["timestamp"].is_number()) {
                    //     timestamp = msg["timestamp"].get<time_t>();
                    // } else {
                    //     timestamp = std::time(nullptr); // 默认当前时间
                    // }

                    // // 转换为本地时间（线程安全）
                    // tm localTime;
                    // localtime_r(&timestamp, &localTime);

                    // char timeBuffer[80];
                    // strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", &localTime);

                    // 处理 sender
                    std::string sender = "未知用户";
                    // 解析时间戳
                    time_t timestamp = msg["timestamp"];
                    tm* localTime = localtime(&timestamp);
                    char timeBuffer[80];
                    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", localTime);
             if (msg.contains("sender") && msg["sender"].is_string()) {
                        sender = msg["sender"].get<std::string>();
                    }

                    std::cout <<"["<<msg["type"].get<std::string>()<<"]"<< "[" << timeBuffer << "] 有未读消息来自 " << sender << std::endl;
                }
            } else {
                std::cerr << "Error: 'messages' is missing or not an array!" << std::endl;
            }
            std::cout << "=======================" << std::endl;
        } else {
            std::cout << "\n===== 无未读消息 =====" << std::endl;
        }

    }
    void checkgroupUnreadMessages(std::string currentUser, int sockfd)
    {
        json request = {
            {"type", "get_unreadgroup_messages"},
            {"user", currentUser}
        };

        json req = sendReq(request, sockfd);
        if (req.is_null() || !req.is_object()) {
            std::cerr << "Error: Invalid response from server!" << std::endl;
            return;
        }

        bool success = false;
        if (req.contains("success") && req["success"].is_boolean()) {
            success = req["success"].get<bool>();
        }

        if (1) {
            std::cout << "\n===== 群聊未读消息 =====" << std::endl;
            if (req.contains("messages") && req["messages"].is_array()) {
                for (const auto& msg : req["messages"]) {
                    // 处理 timestamp
        //             time_t timestamp = 0;
        //             if (msg.contains("timestamp") && msg["timestamp"].is_number()) {
        //                 timestamp = msg["timestamp"].get<time_t>();
        //             } else {
        //                 timestamp = std::time(nullptr); // 默认当前时间
        //             }

        //             // 转换为本地时间（线程安全）
        //             // tm localTime;
        //             // localtime_r(&timestamp, &localTime);
        //             // 2. 转换为时间结构体
        // // 转换为本地时间结构体
        // tm timeInfo;
        // if (localtime_r(&timestamp, &timeInfo) == nullptr) {
        //     throw std::runtime_error("时间转换失败");
        // }

        //             char timeBuffer[80];
        //             strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", &timeInfo);
             // 解析时间戳
            time_t timestamp = msg["timestamp"];
            tm* localTime = localtime(&timestamp);
            char timeBuffer[80];
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", localTime);
            
                    // 处理 sender
                    std::string sender = "未知用户";
                    if (msg.contains("sender") && msg["sender"].is_string()) {
                        sender = msg["sender"].get<std::string>();
                    }

                    std::cout <<"["<<msg["type"].get<std::string>()<<msg["groupid"]<<"]"<< "[" << timeBuffer << "] 有未读消息来自 " << sender << std::endl;
                }
            } else {
                std::cerr << "Error: 'messages' is missing or not an array!" << std::endl;
            }
            std::cout << "=======================" << std::endl;
        } else {
            std::cout << "\n===== 无未读消息 =====" << std::endl;
        }
    }
    

};
