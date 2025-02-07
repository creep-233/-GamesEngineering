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
#include <unordered_set>
#pragma comment(lib, "ws2_32.lib")

#include <windows.h>
#pragma comment(lib, "winmm.lib")



#define DEFAULT_BUFFER_SIZE 1024
#define DEFAULT_PORT 65432



// Data
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Global variables related to networking
std::atomic<bool> running = true;                  // Client running state
std::string currentChatMode = "group";             // Default chat mode (group chat)
std::string privateTargetUser = "";                // Current private chat target
std::vector<std::string> privateChannels;          // List of private chat channels
std::vector<std::string> chatMessages;             // Storage for chat messages
std::mutex chatMutex;                              // Mutex for thread-safe chat messages

SOCKET clientSocket;                               // Global client socket
std::string current_private_user;                  // Current private chat user
std::vector<std::string> group_messages;           // Group chat messages
std::map<std::string, std::vector<std::string>> private_messages; // Private chat messages
std::vector<std::string> user_list;                // User list

std::vector<std::string> chatChannels;             // Storage for all channels (group + private)
std::map<std::string, std::vector<std::string>> chatLogs; // Storage for chat logs of each channel
std::string currentChannel = "Group Chat";         // Currently selected chat channel

std::mutex userListMutex;

bool hasEnteredUsername = false;                   // Whether the username has been entered
std::string username;                              // Storage for username




void initializeChatChannels() {
    chatChannels.clear();
    chatChannels.push_back("Group Chat");

    if (user_list.empty()) {
        std::cerr << "UserList empty" << std::endl;
        return;
    }

    if (user_list.size() == 1) {
        std::cout << "[DEBUG] Only ont user" << std::endl;
        return;
    }

    for (size_t i = 0; i < user_list.size(); ++i) {
        for (size_t j = i + 1; j < user_list.size(); ++j) {

            std::string userA = user_list[i];
            std::string userB = user_list[j];

            // sort userA and userB
            if (userA > userB) std::swap(userA, userB);  

            std::string privateChannel = userA + " - " + userB;

            privateChannel.erase(std::remove(privateChannel.begin(), privateChannel.end(), '\n'), privateChannel.end());
            privateChannel.erase(std::remove(privateChannel.begin(), privateChannel.end(), '\r'), privateChannel.end());

            //std::string privateChannel = user_list[i] + " - " + user_list[j];

            if (std::find(chatChannels.begin(), chatChannels.end(), privateChannel) == chatChannels.end()) {
                chatChannels.push_back(privateChannel);

                if (chatLogs.find(privateChannel) == chatLogs.end()) {
                    chatLogs[privateChannel] = {}; // initialize empty log for this channel
                }
            }
        }
    }
}










std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" ");
    size_t last = str.find_last_not_of(" ");
    return (first == std::string::npos || last == std::string::npos) ? "" : str.substr(first, last - first + 1);
}


void onNewUserJoined(const std::string& newUser) {
    {
        std::lock_guard<std::mutex> lock(userListMutex);
        if (std::find(user_list.begin(), user_list.end(), newUser) == user_list.end()) {
            user_list.push_back(newUser);
        }
    }
    initializeChatChannels(); 
}




// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);





void receiveMessages(SOCKET clientSocket) {
    char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
    std::string currentChannel; // log in progress
    bool isReceivingLog = false; 

    while (running) {
        int bytesReceived = recv(clientSocket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string message(buffer);
            std::cout << "[DEBUG] Received message: " << message << std::endl; 

            PlaySound(TEXT("sound1.wav"), NULL, SND_FILENAME | SND_ASYNC);

            // parse online user list
            if (message.rfind("USERLIST:", 0) == 0) {
                {
                    std::lock_guard<std::mutex> lock(userListMutex);
                    user_list.clear();

                    std::string users = message.substr(9);
                    if (users == "NONE" || users.empty()) {
                        std::cout << "[DEBUG] Only one user online" << std::endl;
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

                    initializeChatChannels(); // update chat channels
                }

                std::cout << "[DEBUG] Userlist received";
                for (const auto& user : user_list) {
                    std::cout << user << " ";
                }
                std::cout << std::endl;
            }

            // processing log
            else if (message.rfind("LOG_START:", 0) == 0) {
                currentChannel = message.substr(10); // get channel name
                std::lock_guard<std::mutex> lock(chatMutex);
                chatLogs[currentChannel].clear(); // empty current channel log
                isReceivingLog = true;
                std::cout << "[DEBUG] Start receiving logs for channel: " << currentChannel << std::endl;
            }
            else if (message.rfind("LOG_END:", 0) == 0) {
                std::string endChannel = message.substr(8); // get end channel name
                if (endChannel == currentChannel) {
                    isReceivingLog = false;
                    std::cout << "[DEBUG] Finished receiving logs for channel: " << currentChannel << std::endl;
                }
            }
            else if (isReceivingLog) {
                // save received log to current channel
                std::lock_guard<std::mutex> lock(chatMutex);
                chatLogs[currentChannel].push_back(message);

                std::cout << "[DEBUG] Client storing log in [" << currentChannel << "]: " << message << std::endl;
            }

 


            else if (message.rfind("DM:", 0) == 0) {

                    // format: DM:sender-receiver:message
                    size_t firstColon = message.find(':');
                    size_t dash = message.find('-', firstColon + 1); 
                    size_t secondColon = message.find(':', dash + 1); 


                    std::cout << "[DEBUG] Parsing DM message: " << message << std::endl;
                    std::cout << "[DEBUG] Found positions - firstColon: " << firstColon
                        << ", dash: " << dash
                        << ", secondColon: " << secondColon << std::endl;

                    if (firstColon != std::string::npos && dash != std::string::npos && secondColon != std::string::npos && dash < secondColon) {
                        
                        // prcessing sender, receiver and privateMessage
                        std::string sender = trim(message.substr(firstColon + 1, dash - firstColon - 1));
                        std::string targetUser = trim(message.substr(dash + 1, secondColon - dash - 1));
                        std::string privateMessage = message.substr(secondColon + 1);

                        //  delete targetUser's newline and space
                        targetUser.erase(std::remove(targetUser.begin(), targetUser.end(), '\n'), targetUser.end());
                        targetUser.erase(std::remove(targetUser.begin(), targetUser.end(), '\r'), targetUser.end());
                        targetUser.erase(0, targetUser.find_first_not_of(" "));
                        targetUser.erase(targetUser.find_last_not_of(" ") + 1);


                    
                    std::string channelName = (sender < targetUser) ? (sender + " - " + targetUser) : (targetUser + " - " + sender);

                    std::cout << "[DEBUG] Parsed DM -> Sender: " << sender
                        << ", Target: " << targetUser
                        << ", Message: " << privateMessage
                        << ", Channel: " << channelName << std::endl;


                    // save to local log
                    std::lock_guard<std::mutex> lock(chatMutex);
                    chatLogs[channelName].push_back(sender + ": " + privateMessage);

                    std::cout << "[DEBUG] Stored DM in local chatLogs[" << channelName << "]: " << sender << ": " << privateMessage << std::endl;

                    std::cout << "[DEBUG] Current chatLogs[" << channelName << "] contents:" << std::endl;
                    for (const auto& msg : chatLogs[channelName]) {
                        std::cout << "[DEBUG] " << msg << std::endl;
                    }
                }
                else {
                    std::cout << "[ERROR] Invalid DM format received: " << message << std::endl;
                }
            }


            // process group chat messages

            else {
                std::lock_guard<std::mutex> lock(chatMutex);
                chatLogs["Group Chat"].push_back(message);
            }
        }
        else {

            std::lock_guard<std::mutex> lock(chatMutex);
            chatLogs["Group Chat"].push_back("[System] Server disconnected.");
            running = false;
            break;
        }
    }
}




void sendMessage(SOCKET clientSocket, const std::string& message, bool isUsername = false, bool isDirectMessage = false) {
    std::string fullMessage;

    if (isUsername) {
        fullMessage = message;
    }
    else if (isDirectMessage) {
        std::string targetUser;
        size_t separator = currentChannel.find('-');

        if (separator != std::string::npos) {
            std::string userA = currentChannel.substr(0, separator);
            std::string userB = currentChannel.substr(separator + 2);

            // delete space
            userA.erase(0, userA.find_first_not_of(" "));
            userB.erase(0, userB.find_first_not_of(" "));
            userA.erase(userA.find_last_not_of(" ") + 1);
            userB.erase(userB.find_last_not_of(" ") + 1);

            if (!userA.empty() && !userB.empty()) {
                targetUser = (username == userA) ? userB : userA;
            }
        }

        if (targetUser.empty()) {
            std::cerr << "[ERROR] Invalid private chat target user." << std::endl;
            return;
        }

        targetUser.erase(std::remove(targetUser.begin(), targetUser.end(), '\n'), targetUser.end());
        targetUser.erase(std::remove(targetUser.end(), targetUser.end(), '\r'), targetUser.end());
        targetUser.erase(0, targetUser.find_first_not_of(" "));
        targetUser.erase(targetUser.find_last_not_of(" ") + 1);

        // message format: DM:sender-receiver:message
        fullMessage = message;

        std::cout << username << ";" << targetUser << ";" << message;
    }
    else {
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




void sendDirectMessage(const std::string& sender, const std::string& targetUser, const std::string& message) {
    std::string formattedMessage = "DM:" + sender + ":" + targetUser + ":" + message;

    // send to server
    int result = send(clientSocket, formattedMessage.c_str(), formattedMessage.size(), 0);
    if (result == SOCKET_ERROR) {
        std::cerr << "[ERROR] Failed to send direct message: " << WSAGetLastError() << std::endl;
    }
    else {
        std::cout << "[DEBUG] Sent DM to server: " << formattedMessage << std::endl;

        // update local log
        std::string channelName = (sender < targetUser) ? ("DM:" + sender + " - " + targetUser) : ("DM:" + targetUser + " - " + sender);
        std::lock_guard<std::mutex> lock(chatMutex);
        chatLogs[channelName].push_back("Me: " + message);

        std::cout << "[DEBUG] Stored DM in local chatLogs[" << channelName << "]: " << message << std::endl;
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

    // luanch receive thread
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


     
        ImGui::SetNextWindowSize(ImVec2(1400, 900), ImGuiCond_FirstUseEver);
        ImGui::Begin("Chat Client", nullptr, ImGuiWindowFlags_NoResize);


    
        ImGui::Columns(3, "ChatColumns", true); 

        ImGui::SetColumnWidth(0, 250); 
        ImGui::SetColumnWidth(1, 800); 
        ImGui::SetColumnWidth(2, 230);



        // ===============left=============== //



        ImGui::SetColumnWidth(0, 250);
        ImGui::BeginChild("LeftPanel", ImVec2(0, 0), true);

        ImGui::Text("Chat Channels");
        ImGui::Separator();

        for (const auto& channel : chatChannels) {
            // filter out private channels that do not contain self
            if (channel != "Group Chat" && channel.find(username) == std::string::npos) {
                continue; // 跳过不包含自己名字的私聊频道
            }

            if (ImGui::Selectable(channel.c_str(), currentChannel == channel)) {
                currentChannel = channel; // change channel
                std::cout << "[DEBUG] Switched to channel: " << currentChannel << std::endl;

                currentChannel.erase(std::remove(currentChannel.begin(), currentChannel.end(), '\n'), currentChannel.end());
                currentChannel.erase(std::remove(currentChannel.begin(), currentChannel.end(), '\r'), currentChannel.end());

                std::cout << "[DEBUG] Checking ALL chatLogs[" << currentChannel << "]:\n";
                if (chatLogs.find(currentChannel) != chatLogs.end()) {
                    for (const auto& msg : chatLogs[currentChannel]) {
                        std::cout << "[DEBUG] Message: " << msg << std::endl;
                    }
                }
                else {
                    std::cout << "[DEBUG] chatLogs[" << currentChannel << "] NOT FOUND!\n";
                }

                std::cout << "[DEBUG] Current selected channel: " << currentChannel << std::endl;
                std::cout << "[DEBUG] Checking chatLogs keys:\n";
                std::unordered_set<std::string> keys;
                for (const auto& pair : chatLogs) {
                    if (keys.find(pair.first) != keys.end()) {
                        std::cout << "[ERROR] chatLogs key DUPLICATED: " << pair.first << std::endl;
                    }
                    keys.insert(pair.first);
                }
            }
        }

        ImGui::EndChild();
        ImGui::NextColumn();



        // =============== mid =============== //



        float middleColumnWidth = 800;
        ImGui::BeginChild("ChatWindow", ImVec2(0, 0), true);

        if (!hasEnteredUsername) {

            ImGui::Text("Choose GroupChat and DM channel on your left");
            ImGui::Text("DM channel will appear when other users join");

            ImGui::Text("------------------------------------------------------------------------");

            ImGui::Text("Please enter your username to continue:");
            static char username_input[256] = "";
            ImGui::InputText("Username", username_input, IM_ARRAYSIZE(username_input));

            if (ImGui::Button("Confirm")) {
                if (strlen(username_input) > 0) {
                    username = username_input;
                    hasEnteredUsername = true;

                   
                    sendMessage(clientSocket, username, true, false);

                   
                    std::cout << "[DEBUG] Username confirmed: " << username << std::endl;
                }
            }
        }
        else {
            // show the title of the current channel
            ImGui::Text("Chat - %s", currentChannel.c_str());
            ImGui::Separator();


            ImGui::BeginChild("ChatMessages", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
            {
                std::lock_guard<std::mutex> lock(chatMutex);

                if (chatLogs.find(currentChannel) == chatLogs.end()) {
                    //std::cout << "[DEBUG] chatLogs[" << currentChannel << "] NOT FOUND!" << std::endl;
                }
                else {
                    for (const auto& message : chatLogs[currentChannel]) {
                        ImGui::TextWrapped("%s", message.c_str());
                        //std::cout << "[DEBUG] Displaying Message in " << currentChannel << ": " << message << std::endl;
                    }
                }
            }
            ImGui::EndChild();

            // input box
            static char input_buffer[256] = "";
            ImGui::InputText("Message", input_buffer, IM_ARRAYSIZE(input_buffer));

            ImGui::SameLine();

            if (ImGui::Button("Send", ImVec2(80, 30))) {
                if (strlen(input_buffer) > 0) {
                    std::string messageToSend = input_buffer;

                    // DM or group chat
                    if (currentChannel != "Group Chat") {
                        std::string targetUser;
                        size_t separator = currentChannel.find('-');

                        if (separator != std::string::npos) {
                            // get private chat user
                            std::string userA = currentChannel.substr(0, separator);
                            std::string userB = currentChannel.substr(separator + 2);

                            // delete extra space
                            userA.erase(0, userA.find_first_not_of(" "));
                            userB.erase(0, userB.find_first_not_of(" "));
                            userA.erase(userA.find_last_not_of(" ") + 1);
                            userB.erase(userB.find_last_not_of(" ") + 1);

                            if (!userA.empty() && !userB.empty()) {
                             
                                targetUser = (username == userA) ? userB : userA;

                                // dm:sender-receiver:message
                                std::string formattedMessage = "DM:" + username + "-" + targetUser + ":" + messageToSend;

                         
                                sendMessage(clientSocket, formattedMessage, false, true);

                                // update local chat log
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
                        // group chat
                        sendMessage(clientSocket, messageToSend); 
                        std::lock_guard<std::mutex> lock(chatMutex);
                        chatLogs["Group Chat"].push_back("Me: " + messageToSend);
                    }

               
                    input_buffer[0] = '\0';
                }
            }
        }

        ImGui::EndChild();
        ImGui::NextColumn();




        // =============== right =============== //

        ImGui::SetColumnWidth(2, 230);

  
        ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);

        //user list area
        ImGui::BeginChild("UserListPanel", ImVec2(0, ImGui::GetContentRegionAvail().y - 50), true); 
        ImGui::Text("User list:");
        if (ImGui::BeginListBox("##user_list", ImVec2(-FLT_MIN, ImGui::GetContentRegionAvail().y))) {
            for (const auto& user : user_list) {
                if (ImGui::Selectable(user.c_str())) {
                    current_private_user = user; // open private chat
                }
            }
            ImGui::EndListBox();
        }
        ImGui::EndChild(); 

        // username display area
        ImGui::Separator(); 
        ImGui::BeginChild("UsernamePanel", ImVec2(0, 50), false); 
        ImGui::Text("Your Username: %s", username.c_str());
        ImGui::EndChild(); 

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
