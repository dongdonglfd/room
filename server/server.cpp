
#include"groupserver.h"





class ChatServer :public Friendserver,public groupserver
{
private:
    int server_fd;
    int epoll_fd;
    ThreadPool pool;
    
    string name;
    

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
                userOnline(name, client_fd);
            } else if(type == "msg") {
                handleMessage(client_fd, req);
            }else if(type == "send_code") {
                handleForgotPassword(client_fd, req);
            }else if(type=="add_friend"){
                handleAddFriend(client_fd, req);
            }else if (type == "check_requests") {
                handleCheckRequests(client_fd, req);
            }else if (type == "process_request") {
                handleProcessRequest(client_fd, req);
            }else if(type=="get_friends"){
                handleGetFriends(client_fd, req);
            }else if(type=="delete_friend"){
                handleDeleteFriend(client_fd, req);
            }else if(type=="block_user"){
                handleBlockUser(client_fd, req);
            }else if(type=="create_group"){
                handleCreateGroup(client_fd, req);
            }else if(type=="disband_group"){
                handleDisbandGroup(client_fd, req);
            }else if(type=="get_my_groups"){
                handleGetMyGroups(client_fd, req);
            }else if(type=="leave_group"){
                handleLeaveGroup(client_fd, req);
            }else if(type=="get_user_role"){
                handleGetUserRole(client_fd, req);
            }else if(type=="join_group_request"){
                handleJoinGroupRequest(client_fd, req);
            }else if(type=="get_group_join_requests"){
                handleGetGroupJoinRequests(client_fd, req);
            }else if(type=="process_join_request"){
                handleProcessJoinRequest(client_fd, req);
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
        string qq= req["qq"];
        string email=req["email"];
        cout<<username<<password<<qq<<endl;
        unique_ptr<Connection> con(getDBConnection());
        unique_ptr<PreparedStatement> stmt(
            con->prepareStatement("INSERT INTO users (username, password,qq,email) VALUES (?, ?, ?, ?)")
        );

        json response;
        try {
            stmt->setString(1, username);
            stmt->setString(2, password);
            stmt->setString(3, qq);
            stmt->setString(4, email);
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
        string name = req["username"];
        string password = req["password"];

        unique_ptr<Connection> con(getDBConnection());
        unique_ptr<PreparedStatement> stmt(
            con->prepareStatement("SELECT password FROM users WHERE username = ?")
        );

        json response;
        try {
            stmt->setString(1, name);
            unique_ptr<ResultSet> res(stmt->executeQuery());
            
            if(res->next()) {
                if(res->getString("password") == password) {
                    lock_guard<mutex> lock(online_mutex);
                    online_users[name] = fd;
                    response["success"] = true;
                    response["username"] = name;
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
    void handleForgotPassword(int fd, const json& req)
    {
        string qq = req["qq"];
        unique_ptr<sql::Connection> con(getDBConnection());
        unique_ptr<PreparedStatement> stmt(
        con->prepareStatement("SELECT email FROM users WHERE qq = ?")
        );
        stmt->setString(1, qq);
        json response;
        try {
        unique_ptr<ResultSet> res(stmt->executeQuery());
        if(res->next()) {
            string email = res->getString("email");
            string token = generateSecureToken(); // 生成安全令牌
            
            // 存储令牌到数据库
            unique_ptr<PreparedStatement> updateStmt(
                con->prepareStatement("UPDATE users SET token=?, token_expire=DATE_ADD(NOW(), INTERVAL 1 HOUR) WHERE qq=?")
            );
            updateStmt->setString(1, token);
            updateStmt->setString(2, qq);
            updateStmt->executeUpdate();

            // 发送QQ邮箱
            sendQQEmail(email, token);
            
            response["success"] = true;
            response["message"] = "重置邮件已发送至QQ邮箱";
        } else {
            response["success"] = false;
            response["message"] = "未找到关联的QQ账号";
        }
        } catch(SQLException &e) {
        response["success"] = false;
        response["message"] = "数据库错误";
        }
        send(fd, response.dump().c_str(), response.dump().size(), 0);
        char buffer[4096];
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
        if(bytes_read <= 0) {
            close(fd);
            return;
        }
        json reqq = json::parse(string(buffer, bytes_read));
        string type=reqq["type"];
        if(type=="verify_code")
        {
            handleVerifyCode(fd,reqq);
        }
        else
        {
            return;
        }
    }
    void handleVerifyCode(int fd, const json& req) 
    {
        string qq = req["qq"];
        string client_code = req["code"];
        
        unique_ptr<sql::Connection> con(getDBConnection());
        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "SELECT token FROM users "
                "WHERE qq=? AND token_expire > NOW()" // 同时检查有效期
            )
        );
        stmt->setString(1, qq);
        
        json response;
        try {
            unique_ptr<sql::ResultSet> res(stmt->executeQuery());
            if (res->next()) {
                string db_code = res->getString("token");
                
                if (db_code == client_code) {
                    // 标记验证通过（不生成令牌）
                    response["success"] = true;
                    response["message"] = "验证通过";
                } else {
                    response["success"] = false;
                    response["message"] = "验证码不匹配";
                }
            } else {
                response["success"] = false;
                response["message"] = "验证码已过期";
            }
        } catch (sql::SQLException &e) {
            response["success"] = false;
            response["message"] = "数据库错误：" + string(e.what());
        }
        send(fd, response.dump().c_str(), response.dump().size(), 0);

    }

    void sendQQEmail(const std::string& to, const std::string& code)
    {
        CURL* curl = curl_easy_init();
        if (curl) {
            // 配置发件人和SMTP信息
            const std::string from = "2281409362@qq.com";
            const std::string password = "your_smtp_password"; // QQ邮箱授权码

            // 构建邮件内容（纯文本）
            const std::string data =
                "From: " + from + "\r\n"
                "To: " + to + "\r\n"
                "Subject: 验证码通知\r\n\r\n"
                "您的验证码是：" + code;

            // 设置CURL选项
            curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.qq.com:465");
            curl_easy_setopt(curl, CURLOPT_USERNAME, from.c_str());
            curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
            curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());

            struct curl_slist* recipients = NULL;
            recipients = curl_slist_append(recipients, to.c_str());
            curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

            // 直接发送邮件内容（无需MIME）
            curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());

            // 启用SSL
            curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

            // 执行发送
            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                std::cerr << "邮件发送失败: " << curl_easy_strerror(res) << std::endl;
            }

            // 清理资源
            curl_slist_free_all(recipients);
            curl_easy_cleanup(curl);
        }
    }
    string generateSecureToken()
    {
        srand(time(NULL));
        string token;
        for (int i = 0; i < 6; i++) 
        {
            int digit = rand() % 10; // 生成0到9之间的随机数
            token += digit + '0';  // 将数字转换为字符
        }
        return token;
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
}
