//#include <winsock2.h>
//#include <ws2tcpip.h>
//#include <iostream>
//#include <string>
//#include <thread>
//#include <atomic>
//#include <vector>
//
//#pragma comment(lib, "ws2_32.lib")
//
//#define DEFAULT_PORT 65432
//#define DEFAULT_BUFFER_SIZE 1024
//
//std::atomic<bool> running = true;
//std::string currentChatMode = "group"; // 默认是群聊模式
//std::string privateTargetUser = "";   // 当前的私聊目标
//std::vector<std::string> privateChannels; // 存储私聊频道名称
//
//
//// **接收消息的线程函数**
////void receiveMessages(SOCKET clientSocket) {
////    char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
////
////    while (running) {
////        int bytesReceived = recv(clientSocket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
////        if (bytesReceived > 0) {
////            buffer[bytesReceived] = '\0';
////            std::string message(buffer);
////
////            if (message.rfind("USERS:", 0) == 0) {
////                std::cout << "[系统消息] 在线用户: " << message.substr(6) << std::endl;
////            } else if (message.rfind("[私聊]", 0) == 0) {
////                std::cout << "[私聊消息] " << message.substr(4) << std::endl;
////            } else if (message.rfind("[群聊]", 0) == 0) {
////                std::cout << "[群聊消息] " << message.substr(4) << std::endl;
////            }
////        } else {
////            std::cout << "[系统] 服务器断开连接。\n";
////            running = false;
////            break;
////        }
////    }
////}
//
//
//// **接收消息的线程函数**
//void receiveMessages(SOCKET clientSocket) {
//    char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
//
//    while (running) {
//        int bytesReceived = recv(clientSocket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
//        if (bytesReceived > 0) {
//            buffer[bytesReceived] = '\0';
//            std::string message(buffer);
//
//            // 检查是否是私聊频道创建通知
//            if (message.rfind("[系统]", 0) == 0 && message.find("私聊频道") != std::string::npos) {
//                // 提取频道名称
//                std::string newChannel = message.substr(message.find_last_of(' ') + 1);
//                std::cout << "[系统消息] 新增私聊频道: " << newChannel << "\n";
//
//                // 添加到私聊频道列表
//                privateChannels.push_back(newChannel);
//            }
//            else if (message.rfind("[私聊]", 0) == 0) {
//                std::cout << message << std::endl;
//            }
//            else if (message.rfind("[群聊]", 0) == 0) {
//                std::cout << message << std::endl;
//            }
//            else {
//                std::cout << message << std::endl;
//            }
//        }
//        else {
//            std::cout << "[系统] 服务器断开连接。\n";
//            running = false;
//            break;
//        }
//    }
//}
//
//
//
//
//// **发送消息的函数**
//void sendMessage(SOCKET clientSocket, const std::string& message) {
//    if (send(clientSocket, message.c_str(), message.size(), 0) == SOCKET_ERROR) {
//        std::cerr << "[错误] 发送失败，错误码: " << WSAGetLastError() << std::endl;
//    }
//    else {
//        std::cout << "[发送] " << message << std::endl;
//    }
//}
//
//// **客户端启动函数**
//void startClient() {
//    WSADATA wsaData;
//    WSAStartup(MAKEWORD(2, 2), &wsaData);
//
//    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//    sockaddr_in serverAddr{};
//    serverAddr.sin_family = AF_INET;
//    serverAddr.sin_port = htons(DEFAULT_PORT);
//    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
//
//    connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
//
//    std::string username;
//    std::cout << "请输入用户名: ";
//    std::getline(std::cin, username);
//    send(clientSocket, username.c_str(), username.size(), 0);
//
//    std::thread recvThread(receiveMessages, clientSocket);
//    recvThread.detach();
//
//    while (running) {
//        std::string message;
//        std::getline(std::cin, message);
//
//        if (message == "!bye") break;
//
//        // 切换私聊目标
//        if (message.rfind("/dm ", 0) == 0) {
//            privateTargetUser = message.substr(4);
//            currentChatMode = "private";
//            std::cout << "[系统] 已切换到与 " << privateTargetUser << " 的私聊模式。\n";
//            continue;
//        }
//
//        // 切换回群聊模式
//        if (message == "/group") {
//            currentChatMode = "group";
//            privateTargetUser = "";
//            std::cout << "[系统] 已切换到群聊模式。\n";
//            continue;
//        }
//
//        // 根据模式发送消息
//        if (currentChatMode == "private") {
//            std::string privateMessage = "DM:" + privateTargetUser + ":" + message;
//            send(clientSocket, privateMessage.c_str(), privateMessage.size(), 0);
//        }
//        else {
//            send(clientSocket, message.c_str(), message.size(), 0);
//        }
//    }
//
//    closesocket(clientSocket);
//    WSACleanup();
//}
//
////int main() {
////    startClient();
////}
