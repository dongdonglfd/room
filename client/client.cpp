#include"friend.h"
#include"group.h"
class Client :public Friend ,public Group
{
private:
    int sockfd;
    std::string server_addr;
    int port;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    bool is_authenticated = false;
    std::string current_user;

    // 密码输入辅助函数
    std::string getPassword(const char* prompt) {
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~ECHO;
        
        std::cout << prompt;
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        
        std::string password;
        std::getline(std::cin, password);
        
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        std::cout << std::endl;
        return password;
    }

    // 显示登录菜单
    void showLoginMenu() {
        std::cout << "\n========= 欢迎使用聊天系统 =========\n";
        std::cout << "1. 登录\n";
        std::cout << "2. 注册\n";
        std::cout << "3. 忘记密码\n";
        std::cout << "4. 退出\n";
        std::cout << "请选择操作 (1-4): ";
    }

    // 处理登录流程
    bool processLogin() {
        std::string username, password;
        
        std::cout << "\n====== 用户登录 ======\n";
        std::cout << "用户名: ";
        std::getline(std::cin, username);
        password = getPassword("密码: ");

        // 构造登录请求
        json request;
        request["type"] = "login";
        request["username"] = username;
        request["password"] = password;
        sendMessage(request.dump());

        // 等待服务器响应
        std::string response = receiveMessage();
        json result = json::parse(response);
        
        if (result["success"]) {
            current_user = username;
            is_authenticated = true;
            std::cout << "\n登录成功! 欢迎 " << username << std::endl;
            checkUnreadMessages(username,sockfd);
            checkgroupUnreadMessages(username,sockfd);
            return true;
        } else {
            std::cerr << "\n登录失败: " << result["message"] << std::endl;
            return false;
        }
    }

    // 处理注册流程
    void processRegistration() {
        std::string username, password,qq;
        
        std::cout << "\n====== 用户注册 ======\n";
        std::cout << "用户名: ";
        std::getline(std::cin, username);
        std::cout << "QQ号码: ";
        std::getline(std::cin, qq);
        password = getPassword("密码: ");
        std::string confirm_pwd = getPassword("确认密码: ");

        if (password != confirm_pwd) {
            std::cerr << "\n错误: 两次输入的密码不一致\n";
            return;
        }
        
        json request;
        request["type"] = "register";
        request["username"] = username;
        request["password"] = password;
        request["qq"] = qq;
        request["email"] = qq + "@qq.com"; // 自动绑定QQ邮箱
        sendMessage(request.dump());

        std::string response = receiveMessage();
        json result = json::parse(response);
        
        if (result["success"]) {
            std::cout << "\n注册成功! 请登录\n";
        } else {
            std::cerr << "\n注册失败: " << result["message"] << std::endl;
        }
    }
    //忘记密码流程
    void handleForgotPassword()
    {
        // 第一步：输入QQ号
    std::string qq;
    std::cout << "\n====== 密码找回 ======\n";
    std::cout << "请输入QQ号：";
    std::getline(std::cin, qq);

    // 发送验证码请求
    json codeReq;
    codeReq["type"] = "send_code";
    codeReq["qq"] = qq;
    sendMessage(codeReq.dump());
    
    // 处理响应
    std::string response = receiveMessage();
    json result = json::parse(response);
    if(!result["success"]) {
        std::cerr << "错误：" << result["message"] << std::endl;
        return;
    }

    // 第二步：输入验证码
    std::string code;
    std::cout << "请输入6位验证码：";
    std::getline(std::cin, code);

    // 验证验证码
    json verifyReq;
    verifyReq["type"] = "verify_code";
    verifyReq["qq"] = qq;
    verifyReq["code"] = code;
    sendMessage(verifyReq.dump());
    
    response = receiveMessage();
    result = json::parse(response);
    if(!result["success"]) {
        std::cerr << "错误：" << result["message"] << std::endl;
        return;
    }

    // 第三步：设置新密码
    std::string newPwd, confirmPwd;
    do {
        newPwd = getPassword("请输入新密码：");
        confirmPwd = getPassword("确认新密码：");
        
        if(newPwd != confirmPwd) {
            std::cerr << "错误：两次输入不一致" <<std:: endl;
        }  else {
            break;
        }
    } while(true);

    // 最终提交重置
    json resetReq;
    resetReq["type"] = "reset_password";
    resetReq["token"] = result["token"];
    resetReq["new_password"] = newPwd;
    sendMessage(resetReq.dump());
    
    response = receiveMessage();
    result = json::parse(response);
    std::cout << (result["success"] ? "密码重置成功" : "重置失败") << std::endl;

    }

public:
    Client(const std::string& addr, int p) : server_addr(addr), port(p) 
    {
        // 创建套接字
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }

        // 设置服务器地址结构
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        
        // 转换IP地址格式
        if (inet_pton(AF_INET, server_addr.c_str(), &serv_addr.sin_addr) <= 0) {
            perror("invalid address");
            exit(EXIT_FAILURE);
        }
    }

    void connectToServer() 
    {
        // 连接服务器
        if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("connection failed");
            exit(EXIT_FAILURE);
        }
        std::cout << "Connected to server " << server_addr << ":" << port << std::endl;
    }

    void sendMessage(const std::string& message) 
    {
        send(sockfd, message.c_str(), message.size(), 0);
    }

    std::string receiveMessage() 
    {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(sockfd, buffer, BUFFER_SIZE);
        if (valread <= 0) {
            if (valread == 0) {
                std::cout << "Server disconnected" << std::endl;
            } else {
                perror("read error");
            }
            exit(EXIT_FAILURE);
        }
        return std::string(buffer, valread);
    }

    void work() 
    {
        connectToServer();
        // 登录阶段
        while (!is_authenticated) {
            showLoginMenu();
            
            char choice;
            std::cin >> choice;
            std::cin.ignore(); // 清除输入缓冲

            switch (choice) {
                case '1':
                    if (processLogin()) {
                        showMainInterface();
                    }
                    break;
                case '2':
                    processRegistration();
                    break;
                case '3':
                    handleForgotPassword();    
                case '4':
                    close(sockfd);
                    exit(0);
                default:
                    std::cerr << "无效选项，请重新输入!\n";
            }
        }
        // 创建epoll实例
        int epoll_fd = epoll_create1(0);
        if (epoll_fd == -1) {
            perror("epoll_create1");
            exit(EXIT_FAILURE);
        }

        // 添加标准输入和socket到epoll
        struct epoll_event ev, events[MAX_EVENTS];
        
        // 监控标准输入
        ev.events = EPOLLIN;
        ev.data.fd = STDIN_FILENO;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1) {
            perror("epoll_ctl: stdin");
            exit(EXIT_FAILURE);
        }

        // 监控服务器socket
        ev.events = EPOLLIN;
        ev.data.fd = sockfd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
            perror("epoll_ctl: sockfd");
            exit(EXIT_FAILURE);
        }

        // 主循环
        while (true) {
            int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
            if (nfds == -1) {
                perror("epoll_wait");
                exit(EXIT_FAILURE);
            }

            for (int n = 0; n < nfds; ++n) {
                if (events[n].data.fd == STDIN_FILENO) {
                    // 处理用户输入
                    std::string input;
                    std::getline(std::cin, input);
                    sendMessage(input);
                } else if (events[n].data.fd == sockfd) {
                    // 处理服务器消息
                    std::string response = receiveMessage();
                    std::cout << "Server response: " << response << std::endl;
                }
            }
        }

        close(sockfd);
    }
    void showMainInterface() 
    {
        
        while(true) {
            std::cout << "\n==== 主菜单 ====\n"
                 << "1. 好友管理\n"
                 << "2. 群组管理\n"
                 << "3. 退出系统\n"
                 << "请选择操作: ";
            
            char choice;
            std::cin >> choice;
            
            switch(choice) {
                case '1': friendMenu(sockfd,current_user); break;
                case '2': groupMenu(sockfd,current_user); break;
                // case '3': chatMenu(); break;
                case '3': return;
                default: std::cout << "无效输入!\n";
            }
        }
    }
};
int main(int argc,char **argv)
{
    std::string addr="127.0.0.1";
    address="127.0.0.1";
    int port=0;
    port=PORT;
    if(argc >= 2)
    {
        addr=argv[1];
        address=argv[1];
    }
    if(argc >= 3)
    {
        port=std::stoi(argv[2]);
    }
    Client client(addr,port);
    client.work();
    return 0;
    
}