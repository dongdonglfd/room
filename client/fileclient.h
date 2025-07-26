#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/epoll.h>
#include<string>
#include <termios.h> // 密码输入处理
#include <iomanip>
#include <nlohmann/json.hpp>
#include </usr/include/x86_64-linux-gnu/curl/curl.h>
#include<vector>
#include <map>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <openssl/sha.h>  // 提供 SHA256_CTX 定义
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <iomanip>
#include <sstream>
#include <openssl/buffer.h>
#include <sys/sendfile.h>
#include <fcntl.h> 
#include <sys/stat.h>
namespace fs = std::filesystem;  
using json = nlohmann::json;
using namespace std;
#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_EVENTS 10
string address;
struct FileInfo {
    int id;
    std::string sender;
    std::string file_name;
    uint64_t file_size;
    uint64_t created_at;
    std::string file_path;
};
class FileTransferClient
{
private:

    int sock;
    int cfd;
    string currentUser;
    string friendName;
    string filePath;
    json sendRequest(int sock, const json& request) {
        string requestStr = request.dump();
        send(sock, requestStr.c_str(), requestStr.size(), 0);
        
        char buffer[4096];
        ssize_t bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead > 0) {
            // return {{"success",true}, {"message", "接收响应失败"}};
            return {{"success",true}};
        }
        //return json::parse(buffer);
        
    }
    json sendreq(int sock, const json& request) 
    {
        // 序列化请求
        std::string requestStr = request.dump();
        
        // 发送请求
        send(sock, requestStr.c_str(), requestStr.size(), 0);
        
        // 接收响应
        char buffer[4096] = {0};
        ssize_t bytes = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            throw std::runtime_error("接收响应失败");
        }
        
        return json::parse(std::string(buffer, bytes));
    }
    
public:
void sendFile(int sockfd,string currentuser)
    {
        sock=sockfd;
        currentUser=currentuser;
        
        // cout << "请输入对方用户名: ";
        // getline(cin, friendName);
        // cout<<"请输入文件路径"<<endl;
        // getline(cin, filePath);
        // if (!fs::exists(filePath)) {
        //     cerr << "文件不存在: " << filePath << endl;
        //     return;
        // }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    
        // 输入用户名
        cout << "请输入对方用户名: ";
        getline(cin, friendName);
        
        // 验证用户名
        if (friendName.empty()) {
            cerr << "用户名不能为空" << endl;
            return;
        }
        
        // 输入文件路径
        bool validPath = false;
        while (!validPath) {
            cout << "请输入文件路径: ";
            getline(cin, filePath);
            
            // 验证文件路径
            if (filePath.empty()) {
                cerr << "文件路径不能为空" << endl;
            } else {
                // 检查文件是否存在
                if (filesystem::exists(filePath)) {
                    validPath = true;
                } else {
                    cerr << "文件不存在，请重新输入" << endl;
                }
            }
        }
        //获取文件信息
        string fileName = fs::path(filePath).filename().string();
        uint64_t fileSize = fs::file_size(filePath);
        // 发送文件上传请求
        json request = {
            {"type", "file_upload_request"},
            {"user", currentUser},
            {"file_name", fileName},
            {"file_size", fileSize},
            {"filepath",filePath},
            {"recipient", friendName},
            {"timestamp", static_cast<uint64_t>(time(nullptr))}
        };
        json response = sendRequest(sock, request);
        if (!response.value("success", false)) {
            cerr << "文件上传请求失败: " 
                 << response.value("message", "未知错误") << endl;
            return;
        }

        
        // 上传文件到服务器
        uploadFileToServer(filePath,sockfd);

    }
    // void uploadFileToServer(const std::string& filePath, int sock) 
    // {
    //     // 1. 打开文件
    //     int fd = open(filePath.c_str(), O_RDONLY);
    //     if (fd == -1) {
    //         throw std::runtime_error("无法打开文件: " + filePath + " - " + strerror(errno));
    //     }
        
    //     // 2. 获取文件大小
    //     struct stat fileStat;
    //     if (fstat(fd, &fileStat) == -1) {
    //         close(fd);
    //         throw std::runtime_error("无法获取文件信息: " + filePath + " - " + strerror(errno));
    //     }
        
    //     uint64_t fileSize = fileStat.st_size;
    //     std::cout << "开始上传文件: " << filePath << " (" << fileSize << " 字节)" << std::endl;
        
    //     // 3. 发送开始消息
    //     json startMsg = {
    //         {"type", "file_start"},
    //         {"file_path", filePath},
    //         {"file_size", fileSize}
    //     };
    //     std::string startStr = startMsg.dump();
    //     json response = sendRequest(sock, startMsg);
        
       
        
    //     const size_t chunkSize = 4 * 1024 * 1024; // 4MB
    //     std::vector<char> buffer(chunkSize);
    //     uint64_t totalSent = 0;
        
    //     while (totalSent < fileSize) {
    //         // 读取一块数据
    //         size_t toRead = std::min(chunkSize, static_cast<size_t>(fileSize - totalSent));
    //         ssize_t bytesRead =read(fd,buffer.data(), toRead);
            
            
    //         if (bytesRead == 0) {
    //             break;
    //         }
            
    //         // 发送数据块
    //         if (!sendWithLengthPrefix(sock, buffer.data(), bytesRead)) {
    //             std::cerr << "发送文件数据失败" << std::endl;
    //         }
            
    //         totalSent += bytesRead;
            
    //         // 显示进度
    //         double progress = static_cast<double>(totalSent) / fileSize * 100;
    //         std::cout << "\r发送进度: " << std::fixed << std::setprecision(1) 
    //                   << progress << "% (" << totalSent << "/" << fileSize << " 字节)" 
    //                   << std::flush;
    //     }
    //     std::cout << std::endl;
    //     // 7. 关闭文件
    //     close(fd);
        
    //     // 8. 发送结束消息
    //     json endMsg = {
    //         {"type", "file_end"},
    //         {"file_path", filePath},
    //         {"file_size", fileSize},
    //         {"success", true}
    //         //{"bytes_sent", total_sent}
    //     };
    //     json endResponse = sendRequest(sock, endMsg);
        
    //     std::cout << "文件上传完成: " << filePath << std::endl;
    // }
    void pasvclient()
    {
        cfd=socket(AF_INET,SOCK_STREAM,0);
        if(cfd==-1)
        {
            perror("socket");
            exit(0);
        }
        //2.连接服务器
        struct sockaddr_in addr;
        addr.sin_family=AF_INET;//IPV4
        addr.sin_port=htons(8090);//网络字节序
        //变成大端
        inet_pton(AF_INET,address.c_str(),&addr.sin_addr.s_addr);
        int ret=connect(cfd,(struct sockaddr*)&addr,sizeof(addr));
        if(ret==-1)
        {
            perror("connect");
            exit(0);
        }
    }
    void uploadFileToServer(const std::string& filePath, int sock) 
    {
        string fileName = fs::path(filePath).filename().string();
        std::ifstream file(filePath, std::ios::binary);
        if (!file) throw std::runtime_error("文件不存在");
        uint64_t fileSize = fs::file_size(filePath);
        // 3. 发送开始消息
        json startMsg = {
            {"type", "file_start"},
            {"file_path", filePath},
            {"filesize",fileSize}
        };
        std::string startStr = startMsg.dump();
        json response = sendRequest(sock, startMsg);
        //uint16_t port=response["port"];
        pasvclient();
         char buffer[4096];
        ssize_t total = 0;
        
        while (file) {
            file.read(buffer, sizeof(buffer));
            std::streamsize bytes_read = file.gcount();
            
            if (bytes_read > 0) {
                ssize_t total_sent = 0;
                while (total_sent < bytes_read) {
                    ssize_t sent = send(cfd, 
                                    buffer + total_sent, 
                                    bytes_read - total_sent, 
                                    MSG_NOSIGNAL);
                    if (sent <= 0) {
                        throw std::runtime_error("发送失败");
                    }
                    total_sent += sent;
                }
                total += total_sent;
            }
        }
        std::cout << "上传完成: "<< " (" << total << " bytes)" << std::endl;
        sleep(1);
        // 8. 发送结束消息
        json endMsg = {
            {"type", "file_end"},
            {"success", true}
            //{"bytes_sent", total_sent}
        };
        json endResponse = sendRequest(sock, endMsg);
        close(cfd);
        
        std::cout << "文件上传完成: " << filePath << std::endl;
    }
    void getUndeliveredFiles(int sock, const std::string& username)
    {
        json request = {
            {"type", "get_undelivered_files"},
            {"username", username}
        };
        
        // 2. 发送请求
        json response = sendreq(sock, request);// 3. 检查响应状态
        if (!response.value("success", false)) {
            throw std::runtime_error("服务器错误: " + response.value("message", ""));
        }

        std::vector<FileInfo> filesstore;
        for (const auto & files :response["messages"])
        {
            FileInfo file;
            file.id = files["id"];
            file.sender = files["sender"];
            file.file_name = files["file_name"];
            file.file_size = files["file_size"];
            file.created_at = files["timestamp"];
            filesstore.push_back(file);
        
        }
        std::cout << "======================================================================" << std::endl;
        std::cout << "ID"<<"\t"<<"发送者"<<"\t"<<"文件名"<<"\t"<<"大小"<<"\t"<<"时间"<<"\t" << std::endl;
        std::cout << "----------------------------------------------------------------------" << std::endl;
        for(const auto & info :filesstore)
        {
            time_t timestamp = info.created_at;
            tm* localTime = localtime(&timestamp);
            char timeBuffer[80];
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", localTime);
            cout<<info.id<<"\t"<<info.sender<<"\t"<<info.file_name<<"\t"<<info.file_size<<"\t"<<timeBuffer<<"\t"<<endl;
        }
        cout<<"选择接收文件的id:"<<endl;
        int id;
        cin>>id;
        getchar();
        FileInfo selectedFile;
        if (id > 0) 
        {
            // 1. 在本地存储中查找文件信息
            bool found = false;
            
            for (const auto& info : filesstore) {
                if (info.id == id) {
                    selectedFile = info;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                std::cout << "未找到ID为 " << id << " 的文件" << std::endl;
                return;
            }
            
        }
        receivefile(selectedFile);
    }
    void receivefile(FileInfo &selectedFile)
    {   cout<<"123"<<endl;
        json downloadRequest = {
            {"type", "download_file"},
            {"file_id", selectedFile.id},
            {"filename",selectedFile.file_name}

        };
        json response = sendRequest(sock, downloadRequest);
        pasvclient();
        char buffer[4096];
        ssize_t total = 0;
        cout<<"开始接收"<<endl;
        std::ofstream file(selectedFile.file_name, std::ios::binary);
        if(!file) {
            cout<<"fail"<<endl;
            return;
        }
        while (selectedFile.file_size>total) {
            ssize_t bytes = recv(cfd, buffer, sizeof(buffer), 0);
            if (bytes < 0) {
                if (errno == EINTR) continue; // 处理中断
                cout<<"1234"<<endl;
                perror("recv error");
                break;
            }
            if (bytes == 0) break; // 客户端关闭连接
            
            file.write(buffer, bytes);
            if (!file) {
                //send_response("451 本地文件写入错误");
                break;
            }
            total += bytes;
            
            file.flush(); // 确保写入磁盘
        }
        cout<<"接收完成"<<endl;
        close(cfd);
    }

    
    
};