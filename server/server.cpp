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


using namespace std;
using json = nlohmann::json;
using namespace sql;



// 数据库配置
const string DB_HOST = "tcp://127.0.0.1:3306";
const string DB_USER = "chatuser";   // 数据库账户名
const string DB_PASS = "123";  // 数据库账户密码
const string DB_NAME = "chat";

class ChatServer {
private:
    int server_fd;
    int epoll_fd;
    ThreadPool pool;
    mutex online_mutex;
    unordered_map<string, int> online_users; // 在线用户表

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

    // 处理客户端请求
    void handleClient(int client_fd) {
        char buffer[4096];
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
        if(bytes_read <= 0) {
            close(client_fd);
            return;
        }

        try {
            json req = json::parse(string(buffer, bytes_read));
            string type = req["type"];

            if(type == "register") {
                handleRegister(client_fd, req);
            } else if(type == "login") {
                handleLogin(client_fd, req);
            } else if(type == "msg") {
                handleMessage(client_fd, req);
            }
        } catch(const exception& e) {
            json err;
            err["success"] = false;
            err["message"] = "Invalid request format";
            send(client_fd, err.dump().c_str(), err.dump().size(), 0);
        }
    }

    // 处理注册请求
    void handleRegister(int fd, const json& req) {
        string username = req["username"];
        string password = req["password"];
        cout<<username<<password<<endl;
        unique_ptr<Connection> con(getDBConnection());
        unique_ptr<PreparedStatement> stmt(
            con->prepareStatement("INSERT INTO users (username, password) VALUES (?, ?)")
        );

        json response;
        try {
            stmt->setString(1, username);
            stmt->setString(2, password);
            stmt->executeUpdate();
            response["success"] = true;
            response["message"] = "注册成功";
        } catch(SQLException &e) {
            response["success"] = false;
            response["message"] = (e.getErrorCode() == 1062) ? 
                                 "用户名已存在" : "注册失败";
        }
        send(fd, response.dump().c_str(), response.dump().size(), 0);
    }

    // 处理登录请求
    void handleLogin(int fd, const json& req) {
        string username = req["username"];
        string password = req["password"];

        unique_ptr<Connection> con(getDBConnection());
        unique_ptr<PreparedStatement> stmt(
            con->prepareStatement("SELECT password FROM users WHERE username = ?")
        );

        json response;
        try {
            stmt->setString(1, username);
            unique_ptr<ResultSet> res(stmt->executeQuery());
            
            if(res->next()) {
                if(res->getString("password") == password) {
                    lock_guard<mutex> lock(online_mutex);
                    online_users[username] = fd;
                    response["success"] = true;
                    response["username"] = username;
                } else {
                    response["success"] = false;
                    response["message"] = "密码错误";
                }
            } else {
                response["success"] = false;
                response["message"] = "用户不存在";
            }
        } catch(SQLException &e) {
            response["success"] = false;
            response["message"] = "数据库错误";
        }
        send(fd, response.dump().c_str(), response.dump().size(), 0);
    }

    // 处理消息转发
    void handleMessage(int fd, const json& req) {
        lock_guard<mutex> lock(online_mutex);
        string target = req["target"];
        if(online_users.count(target)) {
            string msg = req.dump();
            send(online_users[target], msg.c_str(), msg.size(), 0);
        } else {
            json err;
            err["success"] = false;
            err["message"] = "用户不在线";
            send(fd, err.dump().c_str(), err.dump().size(), 0);
        }
    }

public:
    ChatServer(int port) : pool(4) {
        // 创建TCP套接字
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // 绑定地址
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        bind(server_fd, (sockaddr*)&addr, sizeof(addr));
        
        // 开始监听
        listen(server_fd, 5);

        // 创建epoll实例
        epoll_fd = epoll_create1(0);
        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = server_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);
    }

    void run() {
        while(true) {
            epoll_event events[10];
            int n = epoll_wait(epoll_fd, events, 10, -1);
            
            for(int i = 0; i < n; ++i) {
                if(events[i].data.fd == server_fd) {
                    // 接受新连接
                    sockaddr_in client_addr;
                    socklen_t len = sizeof(client_addr);
                    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &len);
                    
                    // 设置非阻塞模式
                    int flags = fcntl(client_fd, F_GETFL, 0);
                    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
                    
                    // 添加到epoll
                    epoll_event ev;
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                } else {
                    // 提交任务到线程池
                    pool.enqueue([this, fd = events[i].data.fd] {
                        handleClient(fd);
                    });
                }
            }
        }
    }
};

int main(int argc, char* argv[]) {
    if(argc < 2) {
        cerr << "Usage: " << argv[0] << " <port>" << endl;
        return 1;
    }

    ChatServer server(atoi(argv[1]));
    //ChatServer server(8080);
    server.run();
    return 0;
}# chatroom
