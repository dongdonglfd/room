#include "groupchat.h"
class Group :public  groupchat
{
    private:
    int sock;
    std::string currentUser;

    json sendRequest(const json& req) {
        std::string requestStr = req.dump();
        send(sock, requestStr.c_str(), requestStr.size(), 0);

        char buffer[4096] = {0};
        recv(sock, buffer, 4096, 0);
        return json::parse(buffer);
    }
    public:
    void groupMenu(int sockfd, std::string current_user) 
    {
        sock = sockfd;
        currentUser = current_user;
        
        while(true) {
            std::cout << "\n==== 群组管理 ====\n"
                     << "1. 创建群组\n"
                     << "2. 解散群组\n"
                     << "3. 申请加入群组\n"
                     << "4. 查看我的群组\n"
                     << "5. 查看群组信息\n"
                     << "6. 退出群组\n"
                     << "7. 管理群成员\n"
                     << "8. 群组聊天\n"
                     << "9. 返回主菜单\n"
                     << "请选择操作: ";
            
            char choice;
            std::cin >> choice;
            
            switch(choice) {
                 case '1': createGroup(); break;
                 case '2': disbandGroup(); break;
                 case '3': joinGroup(); break;
                 case '4': showMyGroups(); break;
                 case '5': viewGroupInfo(); break;
                 case '6': leaveGroup(); break;
                 case '7': manageGroupMembers(); break;
                 case '8': groupChat(sock,currentUser); break;
                case '9': return;
                default: std::cout << "无效输入!\n";
            }
        }
    }
    void createGroup()
    {
        std::cout << "\n=== 创建群组 ===\n";
        std::cout << "输入群组名称: ";
        std::string groupName;
        std::cin.ignore();
        std::getline(std::cin, groupName);
        json req;
        req["type"] = "create_group";
        req["user"] = currentUser;
        req["group_name"] = groupName;
    
        json res = sendRequest(req);
        
        if (res["success"]) {
            std::cout << "✓ 群组创建成功! 群号: " << res["group_id"] << "\n";
        } else {
            std::cerr << "✘ 创建失败: " << res["message"] << std::endl;
        }
    
    }
    void disbandGroup()
    {
        std::cout << "\n=== 解散群组 ===\n";
        std::cout << "输入要解散的群号: ";
        int groupId;
        std::cin >> groupId;
        
        // 确认操作
        std::cout << "确定要解散群组吗? (y/n): ";
        char confirm;
        std::cin >> confirm;
        if (confirm != 'y' && confirm != 'Y') {
            std::cout << "操作取消\n";
            return;
        }
        json req;
        req["type"]="disband_group";
        req["user"]=currentUser;
        req["group_id"]=groupId;
        json res = sendRequest(req);
        if(res["success"])
        {
            std::cout<<"群组已解散\n";
        }
        else{
            std::cerr<<"解散失败："<<res["message"]<<std::endl;
        }
    }
    void joinGroup() 
    {
        std::cout << "\n=== 加入群组 ===\n";
        std::cout << "输入群号: ";
        int groupId;
        std::cin >> groupId;
        
        std::cout << "输入验证消息: ";
        std::string message;
        std::cin.ignore();
        std::getline(std::cin, message);
        
        json req;
        req["type"] = "join_group_request";
        req["user"] = currentUser;
        req["group_id"] = groupId;
        req["message"] = message;
        
        json res = sendRequest(req);
        
        if (res["success"]) {
            std::cout << "✓ 加入请求已发送\n";
        } else {
            std::cerr << "✘ 请求失败: " << res["message"] << std::endl;
        }
    }
    void showMyGroups() {
        json req;
        req["type"] = "get_my_groups";
        req["user"] = currentUser;
        
        json res = sendRequest(req);
        
        if (!res["success"]) {
            std::cerr << "✘ 获取失败: " << res["message"] << std::endl;
            return;
        }
        
        auto groups = res["groups"];
        if (groups.empty()) {
            std::cout << "您尚未加入任何群组\n";
            return;
        }
        
        std::cout << "\n==== 我的群组 ====\n";
        std::cout << "群号\t群名称\t身份\n";
        std::cout << "---------------------------------\n";
        
        for (auto& group : groups) {
            std::cout << group["id"] << "\t" 
                      << group["name"] << "\t"
                      << group["role"] << "\n";
        }
    }
    void viewGroupInfo() 
    {
        std::cout << "\n=== 查看群组信息 ===\n";
        std::cout << "输入群号: ";
        int groupId;
        std::cin >> groupId;
        
        json req;
        req["type"] = "get_group_info";
        req["group_id"] = groupId;
        
        json res = sendRequest(req);
        
        if (!res["success"]) {
            std::cerr << "✘ 获取失败: " << res["message"] << std::endl;
            return;
        }
        
        // 显示群组基本信息
        std::cout << "\n==== 群组信息 ====\n";
        std::cout << "群号: " << res["id"] << "\n";
        std::cout << "名称: " << res["name"] << "\n";
        std::cout << "创建者: " << res["creator"] << "\n";
        std::cout << "创建时间: " << res["created_at"] << "\n";
        std::cout << "成员数: " << res["member_count"] << "\n\n";
        
        // 显示管理员列表
        auto admins = res["admins"];
        std::cout << "管理员(" << admins.size() << "人):\n";
        for (auto& admin : admins) {
            std::cout << "  " << admin << "\n";
        }
        
        // 显示普通成员列表
        auto members = res["members"];
        std::cout << "\n成员(" << members.size() << "人):\n";
        for (auto& member : members) {
            std::cout << "  " << member << "\n";
        }
    }
    void leaveGroup()
    {
        std::cout << "\n=== 退出群组 ===\n";
        std::cout << "输入要退出的群号: ";
        int groupId;
        std::cin >> groupId;
        
        // 确认操作
        std::cout << "确定要退出该群组吗? (y/n): ";
        char confirm;
        std::cin >> confirm;
        if (confirm != 'y' && confirm != 'Y') {
            std::cout << "操作取消\n";
            return;
        }
        
        json req;
        req["type"] = "leave_group";
        req["user"] = currentUser;
        req["group_id"] = groupId;
        
        json res = sendRequest(req);
        
        if (res["success"]) {
            std::cout << "已退出群组\n";
        } else {
            std::cerr << "退出失败: " << res["message"] << std::endl;
        }
    }
    // 管理群成员 (群主/管理员权限)
    void manageGroupMembers() 
    {

        std::cout << "\n=== 群组管理 ===\n";
        std::cout << "输入群号: ";
        int groupId;
        std::cin >> groupId;
        json req;
        req["type"] = "get_user_role";
        req["user"] = currentUser;
        req["group_id"] = groupId;
        
        json res = sendRequest(req);
        std::string role= res["role"];
        if (role != "owner" && role != "admin") {
            std::cout << "\n✘ 您不是群主或管理员，无权管理该群组\n";
            return;
        }
        while (true) {
            std::cout<<"您的角色是： "<<(role=="owner" ? "群主" : "管理员")<<std::endl;
            std::cout << "\n==== 群成员管理 (" << groupId << ") ====\n"
                     << "1. 添加管理员\n"
                     << "2. 移除管理员\n"
                     << "3. 处理加入请求\n"
                     << "4. 移除成员\n"
                     << "5. 返回\n"
                     << "请选择操作: ";
            
            char choice;
            std::cin >> choice;
            
            switch(choice) {
                 case '1': addGroupAdmin(groupId); break;
                 case '2': removeGroupAdmin(groupId); break;
                 case '3': processJoinRequests(groupId); break;
                 case '4': removeGroupMember(groupId); break;
                 case '5': return;
                default: std::cout << "无效输入!\n";
            }
        }
    }
    void processJoinRequests(int groupId) 
    {
        // 获取待处理的加入请求
        json reqPending;
        reqPending["type"] = "get_group_join_requests";
        reqPending["group_id"] = groupId;
        reqPending["admin"] = currentUser;
        
        json resPending = sendRequest(reqPending);
        if (!resPending["success"]) {
            std::cerr << "✘ 获取请求失败: " << resPending["message"] << std::endl;
            return;
        }
        
        auto requests = resPending["requests"];
        if (requests.empty()) {
            std::cout << "没有待处理的加入请求\n";
            return;
        }
        
        std::cout << "\n==== 待处理的加入请求 ====\n";
        int index = 1;
        for (auto& request : requests) {
            std::cout << index++ << ". " << request["user"] 
                      << " (验证: " << request["message"] << ")\n";
        }
        
        std::cout << "选择要处理的请求编号 (0取消): ";
        int choice;
        std::cin >> choice;
        
        if (choice < 1 || choice > requests.size()) {
            return;
        }
        
        auto selectedRequest = requests[choice-1];
        std::string username = selectedRequest["user"];
        
        std::cout << "1. 通过\n2. 拒绝\n选择操作: ";
        int action;
        std::cin >> action;
        
        json req;
        req["type"] = "process_join_request";
        req["group_id"] = groupId;
        req["user"] = username;
        req["admin"] = currentUser;
        req["action"] = (action == 1) ? "accept" : "reject";
        
        json res = sendRequest(req);
        if (res["success"]) {
            std::cout << "✓ 操作成功\n";
        } else {
            std::cerr << "✘ 操作失败: " << res["message"] << std::endl;
        }
    }
    // 添加群管理员 (群主权限)
    void addGroupAdmin(int groupId) 
    {
        std::cout << "输入要设置为管理员的用户名: ";
        std::string username;
        std::cin >> username;
        
        json req;
        req["type"] = "add_group_admin";
        req["group_id"] = groupId;
        req["user"] = username;
        req["admin"] = currentUser; // 操作人
        
        json res = sendRequest(req);
        
        if (res["success"]) {
            std::cout << "✓ " << username << " 已设为管理员\n";
        } else {
            std::cerr << "✘ 设置失败: " << res["message"] << std::endl;
        }
    }
    // 移除群管理员 (群主权限)
    void removeGroupAdmin(int groupId) 
    {
        std::cout << "输入要移除管理权限的用户名: ";
        std::string username;
        std::cin >> username;
        
        json req;
        req["type"] = "remove_group_admin";
        req["group_id"] = groupId;
        req["user"] = username;
        req["admin"] = currentUser; // 操作人
        
        json res = sendRequest(req);
        
        if (res["success"]) {
            std::cout << "✓ " << username << " 已移除管理员\n";
        } else {
            std::cerr << "✘ 移除失败: " << res["message"] << std::endl;
        }
    }
    // 移除群成员 (群主/管理员权限)
    void removeGroupMember(int groupId) 
    {
        std::cout << "输入要移除的成员用户名: ";
        std::string username;
        std::cin >> username;
        
        // 确认操作
        std::cout << "确定要将 " << username << " 移出群组吗? (y/n): ";
        char confirm;
        std::cin >> confirm;
        if (confirm != 'y' && confirm != 'Y') {
            std::cout << "操作取消\n";
            return;
        }

        json req;
        req["type"] = "remove_group_member";
        req["group_id"] = groupId;
        req["user"] = username;
        req["admin"] = currentUser;
        
        json res = sendRequest(req);
        
        if (res["success"]) {
            std::cout << "✓ " << username << " 已移出群组\n";
        } else {
            std::cerr << "✘ 移除失败: " << res["message"] << std::endl;
        }
    }
    


};