// #include <iostream>
// #include <string>
// #include <unordered_map>
// #include <queue>
// #include <mutex>
// #include <condition_variable>
// #include <thread>
// #include <mysql/mysql.h>
// #include <nlohmann/json.hpp>
// #include <sys/epoll.h>
// #include <sys/socket.h>
// #include <arpa/inet.h>
// #include <unistd.h>
// #include <fcntl.h>
// #include"threadpool.h"
// #include </usr/include/mysql_driver.h>       // MySQL驱动头文件
// #include <mysql_connection.h>  // 连接类头文件
// #include <cppconn/prepared_statement.h> // 预处理语句
// #include </usr/include/x86_64-linux-gnu/curl/curl.h>
// #include <time.h>
// #include <fstream>
// #include <openssl/sha.h>  // 提供 SHA256_CTX 定义
// #include <openssl/bio.h>
// #include <openssl/evp.h>
// #include <iomanip>
// #include <sstream>
// #include <openssl/buffer.h>
// #include <sys/stat.h>
#include "chatserver.h"



class groupchat:public Chat
{
    Chat group;
private:
    
    int sock;
    mutex groupMutex;
    mutex session;
    json sendRequest(const json& request) {
        // 序列化请求为JSON字符串
        string requestStr = request.dump();
        
        //发送请求到服务器
            
            if (send(sock, requestStr.c_str(), requestStr.size(), 0) < 0) {
                cerr << "发送请求失败" << endl;
                return {{"success", false}, {"message", "发送请求失败"}};
            }
        return {{"success", true}};
    }
public:
    void handleGroupMessage(int client_sock, const json& data)
    {   
        sock=client_sock;
       string sender = data["sender"];
       int groupid = data["groupID"];
       string message = data["message"]; 
       time_t timestamp = static_cast<time_t>(data["timestamp"]);
       unique_ptr<sql::Connection> con(getDBConnection());
       //判读sender是否在群内
       unique_ptr<sql::PreparedStatement> checkStmt(
            con->prepareStatement(
                "SELECT COUNT(*) AS count FROM group_members "
                // "SELECT * FROM group_members "
                "WHERE group_id = ? AND user = ?"
            )
        );
        checkStmt->setInt(1, groupid);
        checkStmt->setString(2, sender);

        unique_ptr<sql::ResultSet> res(checkStmt->executeQuery());
        
        if (!res) {
            //int count = res->getInt("count");
            //if (count == 0) {
                cout<<"5555"<<endl;
                // 发送者不在群组内
                json errorResponse = {
                    {"success", "false"},
                    {"message", "您不在该群组中，无法发送消息"}
                };
                sendRequest(errorResponse);
                return;
            //}
        }
        
       unique_ptr<sql::PreparedStatement> msgStmt(
            con->prepareStatement(
                "INSERT INTO group_messages "
                "(group_id, sender, message, timestamp) "
                "VALUES (?, ?, ?, ?)"
            )
        );
        msgStmt->setInt(1, groupid);
        msgStmt->setString(2, sender);
        msgStmt->setString(3, message);
        msgStmt->setUInt64(4, timestamp);
        msgStmt->executeUpdate(); 
        
        json broadcast = {
            {"type", "group_messages"},
            {"group_id", groupid},
            {"sender", sender},
            {"message", message},
            {"timestamp", timestamp}
        };
        broadcastToGroup(groupid, broadcast, sender);
        json response = {
            {"type", "group_message_ack"},
            {"success", true},
            {"group_id", groupid},
            {"timestamp", timestamp}
        };
        sendRequest(response);


    }
    void broadcastToGroup(int group_id, const json& message, const string& excludeUser = "")
    {   
        lock_guard<mutex> lock(groupMutex);
        unique_ptr<sql::Connection> con(getDBConnection());
        
        // 准备查询群组成员的SQL语句
        unique_ptr<sql::PreparedStatement> membersStmt(
            con->prepareStatement(
                "SELECT user "
                "FROM group_members "
                "WHERE group_id = ?"
            )
        );
        membersStmt->setInt(1, group_id);
        
        // 执行查询
        unique_ptr<sql::ResultSet> membersRes(membersStmt->executeQuery());
        
        // 存储群组成员列表
        vector<string> members;
        while (membersRes->next()) {
            string username = membersRes->getString("user");
            members.push_back(username);
        }
        for (const string& member : members) 
        {   
            if (member == excludeUser)
            {cout<<"34567"<<endl;
                continue;
            } 
            bool isOnline = false;
            int fd=-1;
            {   cout<<"876"<<endl;
                lock_guard<mutex> lock(session);
                // if (userSessions.find(member) != userSessions.end() && 
                //     userSessions[member].isOnline) {
                //     isOnline = true;
                //     sock = userSessions[member].sock;
                // }
                auto it = online_users.find(member);
                if (it != online_users.end())
                {
                    isOnline = true;
                    fd = it->second;
                    cout<<"qq"<<fd<<endl;
                }
                else{
                    cout<<"456"<<endl;
                }
                
            }
            
            if (isOnline) {
                cout<<"987"<<endl;
                // 用户在线，直接发送
                sendResponse(fd, message);
            } else {
                // 用户离线，存储消息
                storeOfflineMessage(member, group_id, message);
            }
        }


    }
    void storeOfflineMessage(const string& recipient, int group_id, const json& message)
    {
        
        unique_ptr<sql::Connection> con(getDBConnection());
        unique_ptr<sql::PreparedStatement> storeStmt(
            con->prepareStatement(
                "INSERT INTO offline_messages "
                "(recipient_username , group_id, sender_username , message, timestamp) "
                "VALUES (?, ?, ?, ?, ?)"
            )
        );
        storeStmt->setString(1, recipient);
        storeStmt->setInt(2, group_id);
        storeStmt->setString(3, message["sender"].get<string>());
        storeStmt->setString(4, message["message"].get<string>());
        storeStmt->setUInt64(5, message["timestamp"].get<uint64_t>());
        storeStmt->executeUpdate();
        cout<<"456"<<endl;
    }

};