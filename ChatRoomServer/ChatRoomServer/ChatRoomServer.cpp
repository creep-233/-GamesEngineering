#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <algorithm>


#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT 65432
#define DEFAULT_BUFFER_SIZE 1024

std::map<SOCKET, std::string> clients; // 客户端列表 (SOCKET -> 用户名)
std::mutex clientsMutex;

std::map<std::string, std::pair<SOCKET, SOCKET>> privateChannels;  // 私聊频道映射：频道名 -> SOCKET 对


std::map<std::string, std::vector<std::string>> channelLogs; // 频道 -> 聊天记录
std::mutex channelLogsMutex; // 用于保护 channelLogs 的锁




void broadcastUserList() {
    std::string userList = "USERLIST:";
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (const auto& client : clients) {
            userList += client.second + ","; // 仅添加用户名
        }
    }

    // 如果用户列表不为空，移除最后一个逗号
    if (!clients.empty()) {
        userList.pop_back();
    }
    else {
        userList += "NONE"; // 没有用户时发送 "USERLIST:NONE"
    }

    userList += "\n";

    std::cout << "[DEBUG] 广播用户列表: " << userList;

    // 向所有客户端广播用户列表
    for (const auto& client : clients) {
        send(client.first, userList.c_str(), userList.size(), 0);
    }
}



// **广播消息到所有客户端**
//void broadcastMessage(const std::string& message, SOCKET excludeSocket = INVALID_SOCKET) {
//    std::lock_guard<std::mutex> lock(clientsMutex);
//    for (const auto& client : clients) {
//        if (client.first != excludeSocket) {
//            send(client.first, message.c_str(), message.size(), 0);
//        }
//    }
//}

int broadcastMessage(const std::string& message, SOCKET excludeSocket = INVALID_SOCKET) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    int errorCount = 0;

    for (const auto& client : clients) {
        if (client.first != excludeSocket) {
            int result = send(client.first, message.c_str(), message.size(), 0);
            if (result == SOCKET_ERROR) {
                std::cerr << "[ERROR] Failed to send message to client: " << client.second << std::endl;
                errorCount++;
            }
        }
    }
    return errorCount;
}



std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" ");
    size_t last = str.find_last_not_of(" ");
    return (first == std::string::npos || last == std::string::npos) ? "" : str.substr(first, last - first + 1);
}






//void handleClient(SOCKET clientSocket) {
//    char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
//    std::string clientName;
//
//    // **接收用户名**
//    int bytesReceived = recv(clientSocket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
//    if (bytesReceived > 0) {
//        buffer[bytesReceived] = '\0';
//        clientName = buffer;
//
//        // **去除 "USERNAME:" 前缀**
//        if (clientName.rfind("USERNAME:", 0) == 0) {
//            clientName = clientName.substr(9);
//        }
//
//        {
//            std::lock_guard<std::mutex> lock(clientsMutex);
//            clients[clientSocket] = clientName;
//        }
//
//        std::cout << "[DEBUG] User connected: " << clientName << std::endl;
//
//        // **广播用户列表**
//        broadcastUserList();
//        broadcastMessage("GroupChat " + clientName + " joined!\n", clientSocket);
//
//        {
//            std::lock_guard<std::mutex> logLock(channelLogsMutex);
//            channelLogs["Group Chat"].push_back("GroupChat " + clientName + " joined!");
//        }
//    }
//
//    while (true) {
//        memset(buffer, 0, DEFAULT_BUFFER_SIZE);
//        int bytesReceived = recv(clientSocket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
//
//        if (bytesReceived <= 0) {
//            std::cout << "[DEBUG] User " << clientName << " disconnected.\n";
//            closesocket(clientSocket);
//
//            {
//                std::lock_guard<std::mutex> lock(clientsMutex);
//                clients.erase(clientSocket);
//            }
//
//            broadcastUserList();
//            broadcastMessage("GroupChat " + clientName + " left.\n", clientSocket);
//
//            {
//                std::lock_guard<std::mutex> logLock(channelLogsMutex);
//                channelLogs["Group Chat"].push_back("GroupChat " + clientName + " left.");
//            }
//            break;
//        }
//
//        buffer[bytesReceived] = '\0';
//        std::string message(buffer);
//
//        message.erase(std::remove(message.begin(), message.end(), '\n'), message.end());
//        message.erase(std::remove(message.begin(), message.end(), '\r'), message.end()); // 兼容 Windows \r\n
//
//        std::cout << "[DEBUG] Received message from " << clientName << ": " << message << std::endl;
//
//        // **私聊消息解析**
//        if (message.rfind("DM:", 0) == 0) {
//            size_t firstColon = message.find(':');
//            size_t secondColon = message.find(':', firstColon + 1);
//
//            if (secondColon != std::string::npos) {
//                std::string targetUser = trim(message.substr(firstColon + 1, secondColon - firstColon - 1));
//                std::string privateMessage = message.substr(secondColon + 1);
//
//                std::cout << "[DEBUG] Parsed targetUser: [" << targetUser << "] from message: [" << message << "]" << std::endl;
//
//                if (targetUser.empty() || targetUser.find('-') != std::string::npos) {
//                    std::cerr << "[ERROR] Invalid private chat target user extracted: " << targetUser << std::endl;
//                    return;
//                }
//
//                // **查找 targetUser 的 SOCKET**
//                SOCKET targetSocket = INVALID_SOCKET;
//                {
//                    std::lock_guard<std::mutex> lock(clientsMutex);
//                    auto it = std::find_if(clients.begin(), clients.end(),
//                        [&targetUser](const auto& pair) { return pair.second == targetUser; });
//                    if (it != clients.end()) {
//                        targetSocket = it->first;
//                    }
//                }
//
//
//                std::string channelName = (clientName < targetUser)
//                    ?  clientName + " - " + targetUser
//                    :  targetUser + " - " + clientName;
//
//
//                // **存入私聊日志**
//                {
//                    std::lock_guard<std::mutex> logLock(channelLogsMutex);
//                    channelLogs[channelName].push_back(clientName + ": " + privateMessage);
//                    std::cout << "[DEBUG] Stored DM in log: " << channelName
//                        << " | Message: " << clientName << ": " << privateMessage << std::endl;
//                }
//
//                // **发送消息给目标用户**
//                if (targetSocket != INVALID_SOCKET) {
//                    std::string formattedMessage = "DM:" + clientName + ":" + privateMessage;
//                    int sendResult = send(targetSocket, formattedMessage.c_str(), formattedMessage.size(), 0);
//                    if (sendResult == SOCKET_ERROR) {
//                        std::cerr << "[ERROR] Failed to send DM to " << targetUser << " | Error: " << WSAGetLastError() << std::endl;
//                    }
//                    else {
//                        std::cout << "[DEBUG] Sent DM to " << targetUser << ": " << formattedMessage << std::endl;
//                    }
//                }
//
//                // **给自己返回消息**
//                std::string selfMessage = "DM:" + clientName + ":" + privateMessage;
//                send(clientSocket, selfMessage.c_str(), selfMessage.size(), 0);
//            }
//            else {
//                std::cerr << "[ERROR] Invalid DM format: " << message << std::endl;
//            }
//        }
//        // **请求聊天日志**
//        else if (message.rfind("REQUEST_LOG:", 0) == 0) {
//            std::string channel = message.substr(12);
//
//            std::string logData = "LOG_START:" + channel + "\n";
//            {
//                std::lock_guard<std::mutex> logLock(channelLogsMutex);
//                if (channelLogs.find(channel) != channelLogs.end()) {
//                    for (const auto& log : channelLogs[channel]) {
//                        logData += log + "\n";
//                    }
//                }
//            }
//            logData += "LOG_END:" + channel + "\n";
//
//            if (send(clientSocket, logData.c_str(), logData.size(), 0) == SOCKET_ERROR) {
//                std::cerr << "[ERROR] Failed to send logs for channel: " << channel << std::endl;
//            }
//            else {
//                std::cout << "[DEBUG] Sent logs for channel: " << channel << std::endl;
//            }
//        }
//        // **群聊消息**
//        else {
//            std::string fullMessage = "GroupChat " + clientName + ": " + message;
//
//            {
//                std::lock_guard<std::mutex> logLock(channelLogsMutex);
//                channelLogs["Group Chat"].push_back(clientName + ": " + message);
//            }
//
//            if (broadcastMessage(fullMessage, clientSocket) == SOCKET_ERROR) {
//                std::cerr << "[ERROR] Failed to broadcast group message!" << std::endl;
//            }
//            else {
//                std::cout << "[DEBUG] Sent GroupChat message: " << fullMessage << std::endl;
//            }
//        }
//    }
//}
//



void handleClient(SOCKET clientSocket) {
    char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
    std::string clientName;

    // **接收用户名**
    int bytesReceived = recv(clientSocket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        clientName = buffer;

        // **去除 "USERNAME:" 前缀**
        if (clientName.rfind("USERNAME:", 0) == 0) {
            clientName = clientName.substr(9);
        }

        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients[clientSocket] = clientName;
        }

        std::cout << "[DEBUG] User connected: " << clientName << std::endl;

        // **广播用户列表**
        broadcastUserList();
        broadcastMessage("GroupChat " + clientName + " joined!\n", clientSocket);

        {
            std::lock_guard<std::mutex> logLock(channelLogsMutex);
            channelLogs["Group Chat"].push_back("GroupChat " + clientName + " joined!");
        }
    }

    while (true) {
        memset(buffer, 0, DEFAULT_BUFFER_SIZE);
        int bytesReceived = recv(clientSocket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);

        if (bytesReceived <= 0) {
            std::cout << "[DEBUG] User " << clientName << " disconnected.\n";
            closesocket(clientSocket);

            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clients.erase(clientSocket);
            }

            broadcastUserList();
            broadcastMessage("GroupChat " + clientName + " left.\n", clientSocket);

            {
                std::lock_guard<std::mutex> logLock(channelLogsMutex);
                channelLogs["Group Chat"].push_back("GroupChat " + clientName + " left.");
            }
            break;
        }

        buffer[bytesReceived] = '\0';
        std::string message(buffer);

        message.erase(std::remove(message.begin(), message.end(), '\n'), message.end());
        message.erase(std::remove(message.begin(), message.end(), '\r'), message.end()); // 兼容 Windows \r\n

        std::cout << "[DEBUG] Received message from " << clientName << ": " << message << std::endl;




        if (message.rfind("DM:", 0) == 0) {
            size_t firstColon = message.find(':');
            size_t dash = message.find('-', firstColon + 1); // 查找 `-`
            size_t secondColon = message.find(':', dash + 1); // 查找第二个冒号

            if (firstColon != std::string::npos && dash != std::string::npos && secondColon != std::string::npos && dash < secondColon) {
                // 解析 sender, receiver 和 privateMessage
                std::string sender = trim(message.substr(firstColon + 1, dash - firstColon - 1));
                std::string targetUser = trim(message.substr(dash + 1, secondColon - dash - 1));
                std::string privateMessage = message.substr(secondColon + 1);

                // 去除 targetUser 的换行符和空格
                targetUser.erase(std::remove(targetUser.begin(), targetUser.end(), '\n'), targetUser.end());
                targetUser.erase(std::remove(targetUser.begin(), targetUser.end(), '\r'), targetUser.end());
                targetUser.erase(0, targetUser.find_first_not_of(" "));
                targetUser.erase(targetUser.find_last_not_of(" ") + 1);

                std::cout << "[DEBUG] Parsed DM from [" << sender << "] to [" << targetUser << "] | Message: " << privateMessage << std::endl;

                if (targetUser.empty()) {
                    std::cerr << "[ERROR] Invalid private chat target user extracted: " << targetUser << std::endl;
                    return;
                }

                // **查找 targetUser 的 SOCKET**
                SOCKET targetSocket = INVALID_SOCKET;
                {
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    auto it = std::find_if(clients.begin(), clients.end(),
                        [&targetUser](const auto& pair) { return pair.second == targetUser; });
                    if (it != clients.end()) {
                        targetSocket = it->first;
                    }
                }

                // **确保私聊频道名称按字母顺序存储**
                std::string channelName = (sender < targetUser)
                    ? "DM:" + sender + " - " + targetUser
                    : "DM:" + targetUser + " - " + sender;

                // **存入服务器的私聊日志**
                {
                    std::lock_guard<std::mutex> logLock(channelLogsMutex);
                    channelLogs[channelName].push_back(sender + ": " + privateMessage);
                    std::cout << "[DEBUG] Stored DM in server log [" << channelName << "]: " << sender << ": " << privateMessage << std::endl;
                }

                // **发送消息给目标用户**
                //if (targetSocket != INVALID_SOCKET) {
                //    std::string formattedMessage = "DM:" + sender + ":" + privateMessage;
                //    int sendResult = send(targetSocket, formattedMessage.c_str(), formattedMessage.size(), 0);
                //    if (sendResult == SOCKET_ERROR) {
                //        std::cerr << "[ERROR] Failed to send DM to " << targetUser << " | Error: " << WSAGetLastError() << std::endl;
                //    }
                //    else {
                //        std::cout << "[DEBUG] Sent DM to " << targetUser << ": " << formattedMessage << std::endl;
                //    }
                //}

                if (targetSocket != INVALID_SOCKET) {
                    std::string formattedMessage = "DM:" + sender + "-" + targetUser + ":" + privateMessage;
                    std::cout << "formattedMessage :" << formattedMessage << std::endl;
                    int sendResult = send(targetSocket, formattedMessage.c_str(), formattedMessage.size(), 0);
                    if (sendResult == SOCKET_ERROR) {
                        std::cerr << "[ERROR] Failed to send DM to " << targetUser << " | Error: " << WSAGetLastError() << std::endl;
                    }
                    else {
                        std::cout << "[DEBUG] Sent DM to " << targetUser << ": " << formattedMessage << std::endl;
                    }
                }

                // **给自己返回消息**
                std::string selfMessage = "DM:" + sender + ":" + privateMessage;
                send(clientSocket, selfMessage.c_str(), selfMessage.size(), 0);
            }
            else {
                std::cerr << "[ERROR] Invalid DM format: " << message << std::endl;
            }
        }


        // **请求聊天日志**
        else if (message.rfind("REQUEST_LOG:", 0) == 0) {
            std::string channel = message.substr(12);

            std::string logData = "LOG_START:" + channel + "\n";

            std::cout << "[DEBUG] Processing REQUEST_LOG for channel: " << channel << std::endl;

            {
                std::lock_guard<std::mutex> logLock(channelLogsMutex);
                if (channelLogs.find(channel) != channelLogs.end()) {
                    std::cout << "[DEBUG] Found logs for channel: " << channel << std::endl;
                    for (const auto& log : channelLogs[channel]) {
                        logData += log + "\n";
                        std::cout << "[DEBUG] Log Entry: " << log << std::endl;  // 🔹 打印每条日志
                    }
                }
                else {
                    std::cout << "[WARNING] No logs found for channel: " << channel << std::endl;
                }
            }

            logData += "LOG_END:" + channel + "\n";

            if (send(clientSocket, logData.c_str(), logData.size(), 0) == SOCKET_ERROR) {
                std::cerr << "[ERROR] Failed to send logs for channel: " << channel << std::endl;
            }
            else {
                std::cout << "[DEBUG] Sent logs for channel: " << channel << std::endl;
            }
        }

        // **群聊消息**
        else {
            std::string fullMessage = "GroupChat " + clientName + ": " + message;

            {
                std::lock_guard<std::mutex> logLock(channelLogsMutex);
                channelLogs["Group Chat"].push_back(clientName + ": " + message);
            }

            if (broadcastMessage(fullMessage, clientSocket) == SOCKET_ERROR) {
                std::cerr << "[ERROR] Failed to broadcast group message!" << std::endl;
            }
            else {
                std::cout << "[DEBUG] Sent GroupChat message: " << fullMessage << std::endl;
            }
        }
    }
}













void sendPrivateMessage(const std::string& channel, const std::string& message) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = privateChannels.find(channel);
    if (it != privateChannels.end()) {
        std::pair<SOCKET, SOCKET> sockets = it->second;
        send(sockets.first, message.c_str(), message.size(), 0);
        send(sockets.second, message.c_str(), message.size(), 0);
    }
}




// **启动服务器**
void startServer() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(DEFAULT_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, SOMAXCONN);

    std::cout << "Listening " << DEFAULT_PORT << "...\n";

    while (true) {
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &addrLen);

        if (clientSocket != INVALID_SOCKET) {
            std::thread clientThread(handleClient, clientSocket);
            clientThread.detach();
        }
    }

    closesocket(serverSocket);
    WSACleanup();
}

int main() {
    startServer();
}
