// Dear ImGui: standalone example application for DirectX 11

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include "ChatRoom.h"
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <iostream>
#pragma comment(lib, "ws2_32.lib")


#define DEFAULT_BUFFER_SIZE 1024
#define DEFAULT_PORT 65432



// Data
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// 网络相关全局变量
std::atomic<bool> running = true;                  // 客户端运行状态
std::string currentChatMode = "group";             // 默认群聊模式
std::string privateTargetUser = "";                // 当前私聊目标
std::vector<std::string> privateChannels;          // 私聊频道列表
std::vector<std::string> chatMessages;             // 聊天消息存储
std::mutex chatMutex;                              // 聊天消息的线程安全保护

SOCKET clientSocket;                               // 全局客户端套接字
std::string current_private_user;                  // 当前私聊用户
std::vector<std::string> group_messages; // 群聊消息
std::map<std::string, std::vector<std::string>> private_messages; // 私聊消息
std::vector<std::string> user_list; // 用户列表


std::vector<std::string> chatChannels; // 存储所有频道（群聊 + 私聊）
std::map<std::string, std::vector<std::string>> chatLogs; // 存储每个频道的聊天记录
std::string currentChannel = "Group Chat"; // 当前选中的聊天频道


std::mutex userListMutex;


bool hasEnteredUsername = false; // 是否已经输入用户名
std::string username;            // 存储用户名





//void initializeChatChannels() {
//    chatChannels.clear();
//    chatChannels.push_back("Group Chat"); // 默认的群聊频道
//
//    std::lock_guard<std::mutex> lock(userListMutex);
//
//    if (user_list.empty()) {
//        std::cerr << "[错误] 用户列表为空，无法初始化私聊频道。" << std::endl;
//        return;
//    }
//
//    if (user_list.size() == 1) {
//        std::cout << "[DEBUG] 当前只有一个用户，跳过私聊频道初始化。" << std::endl;
//        return;
//    }
//
//    for (size_t i = 0; i < user_list.size(); ++i) {
//        for (size_t j = i + 1; j < user_list.size(); ++j) {
//            std::string privateChannel = user_list[i] + " - " + user_list[j];
//
//            if (std::find(chatChannels.begin(), chatChannels.end(), privateChannel) == chatChannels.end()) {
//                chatChannels.push_back(privateChannel);
//
//                if (chatLogs.find(privateChannel) == chatLogs.end()) {
//                    chatLogs[privateChannel] = {}; // 初始化该频道的聊天记录
//                }
//
//                std::cout << "[DEBUG] 添加频道: " << privateChannel << std::endl;
//            }
//        }
//    }
//}


void initializeChatChannels() {
    chatChannels.clear();
    chatChannels.push_back("Group Chat");

    if (user_list.empty()) {
        std::cerr << "[错误] 用户列表为空，无法初始化私聊频道。" << std::endl;
        return;
    }

    if (user_list.size() == 1) {
        std::cout << "[DEBUG] 当前只有一个用户，跳过私聊频道初始化。" << std::endl;
        return;
    }

    for (size_t i = 0; i < user_list.size(); ++i) {
        for (size_t j = i + 1; j < user_list.size(); ++j) {
            std::string privateChannel = user_list[i] + " - " + user_list[j];
            if (std::find(chatChannels.begin(), chatChannels.end(), privateChannel) == chatChannels.end()) {
                chatChannels.push_back(privateChannel);

                if (chatLogs.find(privateChannel) == chatLogs.end()) {
                    chatLogs[privateChannel] = {}; // 初始化频道聊天记录
                }
            }
        }
    }
}










// =============== 处理新用户加入 =============== //
//void onNewUserJoined(const std::string& newUser) {
//    std::lock_guard<std::mutex> lock(userListMutex);
//    for (const auto& existingUser : user_list) {
//        if (existingUser != newUser) {
//            std::string privateChannel = existingUser + " - " + newUser;
//            chatChannels.push_back(privateChannel);
//            chatLogs[privateChannel] = {}; // 初始化聊天记录
//        }
//    }
//}


void onNewUserJoined(const std::string& newUser) {
    {
        std::lock_guard<std::mutex> lock(userListMutex);
        if (std::find(user_list.begin(), user_list.end(), newUser) == user_list.end()) {
            user_list.push_back(newUser);
        }
    }
    initializeChatChannels(); // 更新频道列表
}




// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


void receiveMessages(SOCKET clientSocket) {
    char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
    std::string currentChannel; // 当前正在处理的频道
    bool isReceivingLog = false; // 是否正在接收日志

    while (running) {
        int bytesReceived = recv(clientSocket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string message(buffer);
            std::cout << "[DEBUG] Received message: " << message << std::endl; // 添加调试日志

            if (message.rfind("USERLIST:", 0) == 0) {
                // 解析用户列表
                {
                    std::lock_guard<std::mutex> lock(userListMutex);
                    user_list.clear();

                    std::string users = message.substr(9);
                    if (users == "NONE" || users.empty()) {
                        std::cout << "[DEBUG] 无其他用户在线，跳过初始化私聊频道。" << std::endl;
                        continue;
                    }

                    size_t start = 0, end;
                    while ((end = users.find(',', start)) != std::string::npos) {
                        user_list.push_back(users.substr(start, end - start));
                        start = end + 1;
                    }
                    if (start < users.size()) {
                        user_list.push_back(users.substr(start));
                    }

                    initializeChatChannels(); // 更新频道列表
                }

                std::cout << "[DEBUG] 解析用户列表完成: ";
                for (const auto& user : user_list) {
                    std::cout << user << " ";
                }
                std::cout << std::endl;
            }
            //else if (message.rfind("LOG_START:", 0) == 0) {
            //    // 开始接收频道日志
            //    currentChannel = message.substr(10); // 获取频道名称
            //    std::lock_guard<std::mutex> lock(chatMutex);
            //    chatLogs[currentChannel].clear(); // 清空当前频道的日志
            //    isReceivingLog = true;
            //}
            //else if (message.rfind("LOG_END:", 0) == 0) {
            //    // 日志接收结束
            //    std::string endChannel = message.substr(8); // 获取结束的频道名称
            //    if (endChannel == currentChannel) {
            //        std::cout << "[DEBUG] 日志接收完成: " << currentChannel << std::endl;
            //        isReceivingLog = false;
            //        currentChannel.clear();
            //    }
            //}
            //else if (isReceivingLog) {
            //    // 正在接收日志，将消息存入当前频道
            //    std::lock_guard<std::mutex> lock(chatMutex);
            //    chatLogs[currentChannel].push_back(message);
            //}


            else if (message.rfind("LOG_START:", 0) == 0) {
                currentChannel = message.substr(10); // 获取频道名称
                std::lock_guard<std::mutex> lock(chatMutex);
                chatLogs[currentChannel].clear(); // 清空当前频道日志
                isReceivingLog = true;
            }
            else if (message.rfind("LOG_END:", 0) == 0) {
                std::string endChannel = message.substr(8); // 获取结束频道名称
                if (endChannel == currentChannel) {
                    isReceivingLog = false;
                    std::cout << "[DEBUG] Log reception complete for channel: " << currentChannel << std::endl;
                }
            }
            else if (isReceivingLog) {
                // 将接收到的日志存入当前频道
                std::lock_guard<std::mutex> lock(chatMutex);
                chatLogs[currentChannel].push_back(message);
            }

            else if (message.rfind("DM:", 0) == 0) {
                // 处理私聊消息
                size_t firstColon = message.find(':', 3);
                size_t secondColon = message.find(':', firstColon + 1);

                if (firstColon != std::string::npos && secondColon != std::string::npos) {
                    std::string sender = message.substr(3, firstColon - 3);
                    std::string content = message.substr(secondColon + 1);
                    std::string targetUser = message.substr(firstColon + 1, secondColon - firstColon - 1);

                    // 确保频道名称与服务器逻辑一致
                    std::string channelName = "DM:" + sender + "-" + targetUser;
                    if (sender > targetUser) {
                        channelName = "DM:" + targetUser + "-" + sender;
                    }

                    {
                        std::lock_guard<std::mutex> lock(chatMutex);
                        chatLogs[channelName].push_back(sender + ": " + content);
                    }

                    std::cout << "[DEBUG] Received DM from " << sender << " to " << targetUser << ": " << content << std::endl;
                }
            }
            else {
                // 处理群聊消息
                std::lock_guard<std::mutex> lock(chatMutex);
                chatLogs["Group Chat"].push_back(message);
            }
        }
        else {
            // 处理服务器断开连接
            std::lock_guard<std::mutex> lock(chatMutex);
            chatLogs["Group Chat"].push_back("[System] Server disconnected.");
            running = false;
            break;
        }
    }
}




//void sendMessage(SOCKET clientSocket, const std::string& message, bool isUsername = false) {
//    std::string fullMessage = isUsername ? message : (username + ": " + message);
//    int result = send(clientSocket, fullMessage.c_str(), fullMessage.size(), 0);
//    if (result == SOCKET_ERROR) {
//        std::cerr << "[ERROR] Failed to send message: " << WSAGetLastError() << std::endl;
//    }
//    else {
//        std::cout << "[DEBUG] Sent message: " << fullMessage << std::endl;
//    }
//}


void sendMessage(SOCKET clientSocket, const std::string& message, bool isUsername = false, bool isDirectMessage = false) {
    std::string fullMessage;

    if (isUsername) {
        // 发送用户名
        fullMessage = message;
    }
    else if (isDirectMessage) {
        // 直接发送私聊格式消息
        fullMessage = message;
    }
    else {
        // 默认情况（群聊）
        fullMessage = username + ": " + message;
    }

    int result = send(clientSocket, fullMessage.c_str(), fullMessage.size(), 0);
    if (result == SOCKET_ERROR) {
        std::cerr << "[ERROR] Failed to send message: " << WSAGetLastError() << std::endl;
    }
    else {
        std::cout << "[DEBUG] Sent message: " << fullMessage << std::endl;
    }
}






void startClient() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(DEFAULT_PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "can not connect" << std::endl;
        running = false;
        return;
    }

    std::cout << "connected" << std::endl;

    // 启动接收线程
    std::thread recvThread(receiveMessages, clientSocket);
    recvThread.detach();
}



// Main code
int main(int, char**)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX11 Example", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);


    startClient();

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window being minimized or screen locked
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();


        // 设置窗口大小
        ImGui::SetNextWindowSize(ImVec2(1400, 900), ImGuiCond_FirstUseEver);
        ImGui::Begin("Chat Client", nullptr, ImGuiWindowFlags_NoResize);


        // 创建水平布局
        ImGui::Columns(3, "ChatColumns", true); // 调整为三列布局

        ImGui::SetColumnWidth(0, 250); // 左侧列宽
        ImGui::SetColumnWidth(1, 800); // 中间列宽
        ImGui::SetColumnWidth(2, 230); // 右侧列宽



        // =============== 左侧：聊天频道 =============== //



        ImGui::SetColumnWidth(0, 250);
        ImGui::BeginChild("LeftPanel", ImVec2(0, 0), true);

        ImGui::Text("Chat Channels");
        ImGui::Separator();


        //for (const auto& channel : chatChannels) {
        //    if (ImGui::Selectable(channel.c_str(), currentChannel == channel)) {
        //        currentChannel = channel; // 切换频道
        //        std::cout << "[DEBUG] Switched to channel: " << currentChannel << std::endl; // 调试输出

        //        // 请求服务器发送记录
        //        std::string requestMessage = "REQUEST_LOG:" + currentChannel;
        //        sendMessage(clientSocket, requestMessage);
        //        std::cout << "[DEBUG] Sent message: " << requestMessage << std::endl;
        //    }
        //}

        for (const auto& channel : chatChannels) {
            if (ImGui::Selectable(channel.c_str(), currentChannel == channel)) {
                currentChannel = channel; // 切换频道
                std::cout << "[DEBUG] Switched to channel: " << currentChannel << std::endl;

                // 请求服务器发送该频道的日志
                std::string requestMessage = "REQUEST_LOG:" + currentChannel;
                sendMessage(clientSocket, requestMessage);

                std::cout << "[DEBUG] Sent log request for channel: " << currentChannel << std::endl;
            }
        }



        ImGui::EndChild();
        ImGui::NextColumn();



        // =============== 中间：聊天窗口 =============== //


        //float middleColumnWidth = 800;
        //ImGui::BeginChild("ChatWindow", ImVec2(0, 0), true);

        //if (!hasEnteredUsername) {
        //    ImGui::Text("Please enter your username to continue:");
        //    static char username_input[256] = "";
        //    ImGui::InputText("Username", username_input, IM_ARRAYSIZE(username_input));

        //    if (ImGui::Button("Confirm")) {
        //        if (strlen(username_input) > 0) {
        //            username = username_input;
        //            hasEnteredUsername = true;

        //            //sendMessage(clientSocket, "USERNAME:" + username); // 向服务器发送用户名
        //            sendMessage(clientSocket, username, false, true);// 向服务器发送用户名

        //            // 调试输出
        //            std::cout << "[DEBUG] Username confirmed: " << username << std::endl;
        //        }
        //    }
        //}
        //else {
        //    // 显示当前频道的标题
        //    ImGui::Text("Chat - %s", currentChannel.c_str());
        //    ImGui::Separator();

        //    // 显示当前频道的聊天记录
        //    ImGui::BeginChild("ChatMessages", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
        //    {
        //        std::lock_guard<std::mutex> lock(chatMutex);
        //        for (const auto& message : chatLogs[currentChannel]) {
        //            ImGui::TextWrapped("%s", message.c_str());

        //            // 调试输出
        //            std::cout << "[DEBUG] Loaded message in channel " << currentChannel << ": " << message << std::endl;
        //        }
        //    }
        //    ImGui::EndChild();

        //    // 输入框
        //    static char input_buffer[256] = "";
        //    ImGui::InputText("Message", input_buffer, IM_ARRAYSIZE(input_buffer));

        //    ImGui::SameLine();

        //    if (ImGui::Button("Send", ImVec2(80, 30))) {
        //        if (strlen(input_buffer) > 0) {
        //            std::string messageToSend = input_buffer;

        //            if (currentChannel != "Group Chat") {
        //                std::string targetUser;
        //                size_t separator = currentChannel.find('-');

        //                if (separator != std::string::npos) {
        //                    std::string userA = currentChannel.substr(0, separator);
        //                    std::string userB = currentChannel.substr(separator + 2);

        //                    userA.erase(0, userA.find_first_not_of(" "));
        //                    userB.erase(0, userB.find_first_not_of(" "));

        //                    if (!userA.empty() && !userB.empty()) {
        //                        targetUser = (username == userA) ? userB : userA;
        //                        std::string formattedMessage = "DM:" + username + "-" + targetUser + ":" + messageToSend;
        //                        sendMessage(clientSocket, formattedMessage);

        //                        std::lock_guard<std::mutex> lock(chatMutex);
        //                        chatLogs[currentChannel].push_back("Me: " + messageToSend);

        //                        std::cout << "[DEBUG] Sent DM to " << targetUser << ": " << formattedMessage << std::endl;
        //                    }
        //                    else {
        //                        std::cout << "[ERROR] Invalid private chat users." << std::endl;
        //                    }
        //                }
        //                else {
        //                    std::cout << "[ERROR] Invalid private channel format." << std::endl;
        //                }
        //            }

        //            else {
        //                sendMessage(clientSocket, messageToSend);
        //                std::lock_guard<std::mutex> lock(chatMutex);
        //                chatLogs["Group Chat"].push_back("Me: " + messageToSend);
        //            }
        //        }
        //    }    
        //}



        //ImGui::EndChild();
        //ImGui::NextColumn();


        float middleColumnWidth = 800;
        ImGui::BeginChild("ChatWindow", ImVec2(0, 0), true);

        if (!hasEnteredUsername) {
            ImGui::Text("Please enter your username to continue:");
            static char username_input[256] = "";
            ImGui::InputText("Username", username_input, IM_ARRAYSIZE(username_input));

            if (ImGui::Button("Confirm")) {
                if (strlen(username_input) > 0) {
                    username = username_input;
                    hasEnteredUsername = true;

                    // 正确发送用户名，使用 isUsername 参数
                    sendMessage(clientSocket, username, true, false);

                    // 调试输出
                    std::cout << "[DEBUG] Username confirmed: " << username << std::endl;
                }
            }
        }
        else {
            // 显示当前频道的标题
            ImGui::Text("Chat - %s", currentChannel.c_str());
            ImGui::Separator();

            // 显示当前频道的聊天记录
            ImGui::BeginChild("ChatMessages", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
            {
                std::lock_guard<std::mutex> lock(chatMutex);
                for (const auto& message : chatLogs[currentChannel]) {
                    ImGui::TextWrapped("%s", message.c_str());

                    // 调试输出
                    std::cout << "[DEBUG] Loaded message in channel " << currentChannel << ": " << message << std::endl;
                }
            }
            ImGui::EndChild();

            // 输入框
            static char input_buffer[256] = "";
            ImGui::InputText("Message", input_buffer, IM_ARRAYSIZE(input_buffer));

            ImGui::SameLine();

            if (ImGui::Button("Send", ImVec2(80, 30))) {
                if (strlen(input_buffer) > 0) {
                    std::string messageToSend = input_buffer;

                    // 判断当前是私聊还是群聊
                    if (currentChannel != "Group Chat") {
                        std::string targetUser;
                        size_t separator = currentChannel.find('-');

                        if (separator != std::string::npos) {
                            // 提取私聊用户
                            std::string userA = currentChannel.substr(0, separator);
                            std::string userB = currentChannel.substr(separator + 2);

                            // 去除前后多余空格
                            userA.erase(0, userA.find_first_not_of(" "));
                            userB.erase(0, userB.find_first_not_of(" "));
                            userA.erase(userA.find_last_not_of(" ") + 1);
                            userB.erase(userB.find_last_not_of(" ") + 1);

                            if (!userA.empty() && !userB.empty()) {
                                // 确定目标用户
                                targetUser = (username == userA) ? userB : userA;

                                // 组装私聊消息
                                std::string formattedMessage = "DM:" + username + "-" + targetUser + ":" + messageToSend;

                                // 使用 sendMessage 函数发送，明确为私聊
                                sendMessage(clientSocket, formattedMessage, false, true);

                                // 更新本地聊天日志
                                std::lock_guard<std::mutex> lock(chatMutex);
                                chatLogs[currentChannel].push_back("Me: " + messageToSend);

                                std::cout << "[DEBUG] Sent DM to " << targetUser << ": " << formattedMessage << std::endl;
                            }
                            else {
                                std::cout << "[ERROR] Invalid private chat users." << std::endl;
                            }
                        }
                        else {
                            std::cout << "[ERROR] Invalid private channel format." << std::endl;
                        }
                    }
                    else {
                        // 群聊逻辑
                        sendMessage(clientSocket, messageToSend); // 群聊无需特别标记
                        std::lock_guard<std::mutex> lock(chatMutex);
                        chatLogs["Group Chat"].push_back("Me: " + messageToSend);
                    }

                    // 清空输入框
                    input_buffer[0] = '\0';
                }
            }
        }

        ImGui::EndChild();
        ImGui::NextColumn();




        // =============== 右侧：在线用户列表 =============== //
        ImGui::SetColumnWidth(2, 230); // 设置右侧列宽
        ImGui::BeginChild("RightPanel", ImVec2(0, 0), true); // 自适应右侧窗口
        ImGui::Text("Users Online:");

        if (ImGui::BeginListBox("##user_list")) {
            for (const auto& user : user_list) {
                if (ImGui::Selectable(user.c_str())) {
                    current_private_user = user; // 打开私聊
                }
            }
            ImGui::EndListBox();
        }
        ImGui::EndChild();

        ImGui::End();




        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
