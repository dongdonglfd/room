#include <iostream>
#include <string>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <mysql/mysql.h>
#include <nlohmann/json.hpp>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include<fcntl.h>
#include"threadpool.h"
#include </usr/include/mysql_driver.h>       // MySQL驱动头文件
#include <mysql_connection.h>  // 连接类头文件
#include <cppconn/prepared_statement.h> // 预处理语句
#include </usr/include/x86_64-linux-gnu/curl/curl.h>
#include <time.h>


using namespace std;
using json = nlohmann::json;
using namespace sql;

// 数据库配置
const string DB_HOST = "tcp://127.0.0.1:3306";
const string DB_USER = "chatuser";   // 数据库账户名
const string DB_PASS = "123";  // 数据库账户密码
const string DB_NAME = "chat";

class Friendserver
{
public:
    mutex online_mutex;
    unordered_map<string, int> online_users; // 在线用户表
public:
    Connection* getDBConnection() {
    try {
        sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
        if(!driver) {
            cerr << "获取驱动实例失败" << endl;
            return nullptr;
        }

        sql::Connection *con = driver->connect(DB_HOST, DB_USER, DB_PASS);
        if(!con) {
            cerr << "创建连接失败" << endl;
            return nullptr;
        }

        con->setSchema(DB_NAME);
        cout << "数据库连接成功" << endl;
        return con;
    } catch (sql::SQLException &e) {
        cerr << "MySQL错误[" << e.getErrorCode() << "]: " << e.what() << endl;
    } catch (...) {
        cerr << "未知错误" << endl;
    }
    return nullptr;
}
    // 添加用户到在线列表
    void userOnline(const std::string& username, int socket) {
        std::lock_guard<std::mutex> lock(online_mutex);
        online_users[username] = socket;
    }
    void handleAddFriend(int fd, const json& req) 
    {
        string requester = req["from"];
        string target = req["to"];
        
        // 获取数据库连接
        unique_ptr<sql::Connection> con(getDBConnection());
        
        // 1. 检查目标用户是否存在
        unique_ptr<sql::PreparedStatement> checkUserStmt(
            con->prepareStatement("SELECT COUNT(*) AS cnt FROM users WHERE username = ?")
        );
        checkUserStmt->setString(1, target);
        unique_ptr<sql::ResultSet> userResult(checkUserStmt->executeQuery());
        
        if (!userResult->next() || userResult->getInt("cnt") == 0) {
            json response = {{"success", false}, {"message", "用户不存在"}};
            send(fd, response.dump().c_str(), response.dump().size(), 0);
            return;
        }

        // 2. 检查是否已经是好友
        unique_ptr<sql::PreparedStatement> checkFriendStmt(
            con->prepareStatement(
                "SELECT COUNT(*) AS cnt FROM friends "
                "WHERE (user1 = ? AND user2 = ?) OR (user1 = ? AND user2 = ?)"
            )
        );
        checkFriendStmt->setString(1, requester);
        checkFriendStmt->setString(2, target);
        checkFriendStmt->setString(3, target);
        checkFriendStmt->setString(4, requester);
        
        unique_ptr<sql::ResultSet> friendResult(checkFriendStmt->executeQuery());
        if (friendResult->next() && friendResult->getInt("cnt") > 0) {
            json response = {{"success", false}, {"message", "已经是好友"}};
            send(fd, response.dump().c_str(), response.dump().size(), 0);
            return;
        }

        // 3. 检查重复请求
        unique_ptr<sql::PreparedStatement> checkRequestStmt(
            con->prepareStatement(
                "SELECT COUNT(*) AS cnt FROM friend_requests "
                "WHERE requester = ? AND target = ? AND status = 'pending'"
            )
        );
        checkRequestStmt->setString(1, requester);
        checkRequestStmt->setString(2, target);
        
        unique_ptr<sql::ResultSet> requestResult(checkRequestStmt->executeQuery());
        if (requestResult->next() && requestResult->getInt("cnt") > 0) {
            json response = {{"success", false}, {"message", "已发送过好友请求"}};
            send(fd, response.dump().c_str(), response.dump().size(), 0);
            return;
        }

        // 4. 检查是否被屏蔽
        unique_ptr<sql::PreparedStatement> checkBlockStmt(
            con->prepareStatement(
                "SELECT COUNT(*) AS cnt FROM blocks "
                "WHERE (user = ? AND blocked_user = ?) OR (user = ? AND blocked_user = ?)"
            )
        );
        checkBlockStmt->setString(1, requester);
        checkBlockStmt->setString(2, target);
        checkBlockStmt->setString(3, target);
        checkBlockStmt->setString(4, requester);
        
        unique_ptr<sql::ResultSet> blockResult(checkBlockStmt->executeQuery());
        if (blockResult->next() && blockResult->getInt("cnt") > 0) {
            json response = {{"success", false}, {"message", "已被对方屏蔽"}};
            send(fd, response.dump().c_str(), response.dump().size(), 0);
            return;
        }

        // 5. 添加好友请求
        unique_ptr<sql::PreparedStatement> addRequestStmt(
            con->prepareStatement(
                "INSERT INTO friend_requests (requester, target, status) "
                "VALUES (?, ?, 'pending')"
            )
        );
        addRequestStmt->setString(1, requester);
        addRequestStmt->setString(2, target);
        addRequestStmt->executeUpdate();

        // 6. 检查目标用户是否在线
        bool is_online = false;
        {
            lock_guard<mutex> lock(online_mutex);
            if (online_users.find(target) != online_users.end()) {
                is_online = true;
                // 实际实现中，这里应该发送通知给目标用户
            }
        }

        json response = {{"success", true}, {"online", is_online}};
        send(fd, response.dump().c_str(), response.dump().size(), 0);
    }
    // 处理查看请求列表
    void handleCheckRequests(int fd, const json& req) 
    {
        string user = req["user"];
        
        unique_ptr<sql::Connection> con(getDBConnection());
        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "SELECT requester FROM friend_requests "
                "WHERE target = ? AND status = 'pending'"
            )
        );
        stmt->setString(1, user);
        
        unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        vector<string> requests;
        
        while (res->next()) {
            requests.push_back(res->getString("requester"));
        }
        
        json response = {{"success", true}, {"requests", requests}};
        send(fd, response.dump().c_str(), response.dump().size(), 0);
    }
    // 处理请求操作
    void handleProcessRequest(int fd, const json& req) 
    {
        string requester = req["from"];
        string target = req["to"];
        string action = req["action"];
        
        unique_ptr<sql::Connection> con(getDBConnection());
        
        // 1. 删除请求记录
        unique_ptr<sql::PreparedStatement> deleteStmt(
            con->prepareStatement(
                "DELETE FROM friend_requests "
                "WHERE requester = ? AND target = ? AND status = 'pending'"
            )
        );
        deleteStmt->setString(1, requester);
        deleteStmt->setString(2, target);
        deleteStmt->executeUpdate();
        
        // 2. 如果是接受请求，建立好友关系
        if (action == "accept") {
            unique_ptr<sql::PreparedStatement> addFriendStmt(
                con->prepareStatement(
                    "INSERT INTO friends (user1, user2) VALUES (?, ?)"
                )
            );
            addFriendStmt->setString(1, requester);
            addFriendStmt->setString(2, target);
            addFriendStmt->executeUpdate();
            
            // 添加反向好友关系
            unique_ptr<sql::PreparedStatement> addReverseStmt(
                con->prepareStatement(
                    "INSERT INTO friends (user1, user2) VALUES (?, ?)"
                )
            );
            addReverseStmt->setString(1, target);
            addReverseStmt->setString(2, requester);
            addReverseStmt->executeUpdate();
        }
        
        json response = {{"success", true}};
        send(fd, response.dump().c_str(), response.dump().size(), 0);
    }
    //展示好友列表
    void handleGetFriends(int fd, const json& req)
    {
        string user = req["user"];
        unique_ptr<sql::Connection> con(getDBConnection());
        unique_ptr<sql::PreparedStatement> stmt(
        con->prepareStatement(
            "SELECT user2 FROM friends "
            "WHERE user1=?"
        )
        );
        stmt->setString(1, user);
        unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        json friends = json::array();
        
        while (res->next()) {
            string user2=res->getString("user2");
            bool is_online = false;
            {
                lock_guard<mutex> lock(online_mutex);
                if (online_users.find(user2) != online_users.end()) {
                    is_online = true;
                    // 实际实现中，这里应该发送通知给目标用户
                }
            }
             friends.push_back({
                {"username", user2},
                {"online", is_online}
            });

        }
        json response;
        response["success"] = true;
        response["friends"] = friends;
        string responseStr = response.dump();
        send(fd, responseStr.c_str(), responseStr.size(), 0);
    }
    void handleDeleteFriend(int fd, const json& req)
    {
        string user = req["user"];
        string friendUser = req["friend"];
    
        unique_ptr<sql::Connection> con(getDBConnection());
        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "DELETE FROM friends WHERE "
                "(user1 = ? AND user2 = ?) OR "
                "(user1 = ? AND user2 = ?)"
            )
        );
        stmt->setString(1, user);
        stmt->setString(2, friendUser);
        stmt->setString(3, friendUser);
        stmt->setString(4, user);
        
        int affectedRows = stmt->executeUpdate();
        
        json response;
        if (affectedRows > 0) {
            response["success"] = true;
            response["message"] = "好友删除成功";
        } else {
            response["success"] = false;
            response["message"] = "未找到好友关系";
        }
        
        send(fd, response.dump().c_str(), response.dump().size(), 0);

    }
    void handleBlockUser(int fd, const json& req)
    {
        string user = req["user"];
        string blockedUser = req["blocked_user"];
    
        unique_ptr<sql::Connection> con(getDBConnection());
        try {
        // 添加屏蔽关系
        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "INSERT INTO blocks (user, blocked_user) "
                "VALUES (?, ?) "
                "ON DUPLICATE KEY UPDATE created_at = CURRENT_TIMESTAMP"
            )
        );
        stmt->setString(1, user);
        stmt->setString(2, blockedUser);
        
        stmt->executeUpdate();
        
        json response = {{"success", true}, 
                         {"message", "用户已屏蔽"}};
        send(fd, response.dump().c_str(), response.dump().size(), 0);
        
        } catch (sql::SQLException &e) {
            json response = {{"success", false}, 
                            {"message", "数据库错误: " + string(e.what())}};
            send(fd, response.dump().c_str(), response.dump().size(), 0);
        }

    }

};