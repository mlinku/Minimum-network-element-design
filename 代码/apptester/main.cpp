// dear imgui - standalone example application for DirectX 11
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include<iostream>
#include <iostream>
#include <conio.h>
#include "winsock.h"
#include "stdio.h"
#include "dirent.h"
#include "CfgFileParms.h"
#include "function.h"
#include <thread>
#pragma comment (lib,"wsock32.lib")
using namespace std;
//============重点来了================================================================================================
//------------重要的控制参数------------------------------------------------------------
//以下从配置文件ne.txt中读取的重要参数
int lowerMode[10]; //如果低层是物理层模拟软件，这个数组放置的是每个接口对应的数据格式——0为比特数组，1为字节数组
int lowerNumber;   //低层实体数量，比如网络层之下可能有多个链路层
int iWorkMode = 0; //本层实体工作模式
int autoSendTime = DEFAULT_AUTO_SEND_TIME;
int autoSendSize = DEFAULT_AUTO_SEND_SIZE;
int recvSensitivity = DEFAULT_RECEIVE_SENSITIVITY;
string strDevID;    //设备号，字符串形式，从1开始
string strLayer;    //层次名
string strEntity;   //实体好，字符串形式，从0开始，可以通过atoi函数变成整数在程序中使用
int iLayOut = 1;		//布局编号，即拓扑中的节点数量
string IP;
//以下是一些重要的与通信有关的控制参数，但是也可以不用管
SOCKET sock;
struct sockaddr_in local_addr;      //本层实体地址
struct sockaddr_in upper_addr;      //上层实体地址，一般情况下，上层实体只有1个
struct sockaddr_in lower_addr[10];  //最多10个下层对象，数组下标就是下层实体的编号
sockaddr_in cmd_addr;         //统一管理平台地址

//------------华丽的分割线，以下是定时器--------------------------------------------
//基于select的定时器，目的是把数据的收发和定时都统一到一个事件驱动框架下
//可以有多个定时器，本设计实现了一个基准定时器，为周期性10ms定时，也可以当作是一种心跳计时器
//其余的定时器可以在这个基础上完成，可行的方案存在多种
//看懂设计思路后，自行扩展以满足需要
//基准定时器一开启就会立即触发一次
struct threadTimer_t {
    int iType;  //为0表示周期性定时器，定时达到后，会自动启动下一次定时
    ULONG ulInterval;
    LARGE_INTEGER llStopTime;
}sBasicTimer;  //全局唯一的计时器，默认是每10毫秒触发一次，设计者可以在这个计时器的基础上实现自己的各种定时。
               //比如计数100次以后，就是1秒。针对不同的事件设置两个变量，一个用来表示是否开始计时，一个用来表示计数到多大
//*************************************************
//名称：StartTimerOnce
//功能：把全局计时器改为1次性，本函调用的同时计时也开始，当定时到达时TimeOut函数将被调用
//输入：计时间隔时间，单位微秒，计时的起始就是本函数调用时。
//输出：直接改变全局计时器变量 sBasicTimer的内容
void StartTimerOnce(ULONG ulInterval)
{
    LARGE_INTEGER llFreq;

    sBasicTimer.iType = 1;
    sBasicTimer.ulInterval = ulInterval;
    QueryPerformanceFrequency(&llFreq);
    QueryPerformanceCounter(&sBasicTimer.llStopTime);
    sBasicTimer.llStopTime.QuadPart += llFreq.QuadPart * sBasicTimer.ulInterval / 1000000;
}
static void HelpMarker(const char* d, const char* desc)
{
    ImGui::SameLine();
    ImGui::TextDisabled(d);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(10 * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
vector<string>getname(string PATH) {
    struct dirent* ptr;
    DIR* dir;
    dir = opendir(PATH.c_str());
    vector<string> files;
    while ((ptr = readdir(dir)) != NULL)
    {

        //跳过'.'和'..'两个目录
        if (ptr->d_name[0] == '.')
            continue;
        //cout << ptr->d_name << endl;
        files.push_back(ptr->d_name);
    }

    return files;
}
//*************************************************
//名称：StartTimerPeriodically
//功能：重设全局计时器的周期性触发的间隔时间，同时周期性计时器也开始工作，每次计时到达TimeOut函数被调用1次
//      周期性计时器可以将间隔设短一些，这样就可以在它的基础上实现更多的各种定时时间的定时器了
//      例程默认是启动周期性计时器机制
//输入：计时间隔时间，单位微秒，计时的起始就是本函数调用时。
//输出：直接改变全局计时器变量 sBasicTimer内容
void StartTimerPeriodically(ULONG ulInterval)
{
    LARGE_INTEGER llFreq;

    sBasicTimer.iType = 0;
    sBasicTimer.ulInterval = ulInterval;
    QueryPerformanceFrequency(&llFreq);
    QueryPerformanceCounter(&sBasicTimer.llStopTime);
    sBasicTimer.llStopTime.QuadPart += llFreq.QuadPart * sBasicTimer.ulInterval / 1000000;
}

//***************重要函数提醒******************************
//名称：SendtoUpper
//功能：向高层实体递交数据时，使用这个函数
//输入：U8 * buf,准备递交的数据， int len，数据长度，单位字节，int ifNo
//输出：函数返回值是发送的数据量
int SendtoUpper(U8* buf, int len)
{
    int sendlen;
    sendlen = sendto(sock, buf, len, 0, (sockaddr*)&(upper_addr), sizeof(sockaddr_in));
    return sendlen;
}
//***************重要函数提醒******************************
//名称：SendtoLower
//功能：向低层实体下发数据时，使用这个函数
//输入：U8 * buf,准备下发的数据， int len，数据长度，单位字节,int ifNo,发往的低层接口号
//输出：函数返回值是发送的数据量
int SendtoLower(U8* buf, int len, int ifNo)
{
    int sendlen;
    if (ifNo < 0 || ifNo >= lowerNumber)
        return 0;
    sendlen = sendto(sock, buf, len, 0, (sockaddr*)&(lower_addr[ifNo]), sizeof(sockaddr_in));
    return sendlen;
}
//***************重要函数提醒******************************
//名称：SendtoCommander
//功能：向统一管理平台发送状态数据时，使用这个函数
//输入：U8 * buf,准备下发的数据， int len，数据长度，单位字节
//输出：函数返回值是发送的数据量
int SendtoCommander(U8* buf, int len)
{
    int sendlen;
    sendlen = sendto(sock, buf, len, 0, (sockaddr*)&(cmd_addr), sizeof(sockaddr_in));
    return sendlen;
}

//------------华丽的分割线，以下是一些数据处理的工具函数，可以用，没必要改------------------------------
//*************************************************
//名称：ByteArrayToBitArray
//功能：将字节数组流放大为比特数组流
//输入： int iBitLen——位流长度, U8* byteA——被放大字节数组, int iByteLen——字节数组长度
//输出：函数返回值是转出来有多少位；
//      U8* bitA,比特数组，注意比特数组的空间（声明）大小至少应是字节数组的8倍
int ByteArrayToBitArray(U8* bitA, int iBitLen, U8* byteA, int iByteLen)
{
    int i;
    int len;

    len = min(iByteLen, iBitLen / 8);
    for (i = 0; i < len; i++) {
        //每次编码8位
        code(byteA[i], &(bitA[i * 8]), 8);
    }
    return len * 8;
}
//*************************************************
//名称：BitArrayToByteArray
//功能：将字节数组流放大为比特数组流
//输入：U8* bitA,比特数组，int iBitLen——位流长度,  int iByteLen——字节数组长度
//      注意比特数组的空间（声明）大小至少应是字节数组的8倍
//输出：返回值是转出来有多少个字节，如果位流长度不是8位整数倍，则最后1字节不满；
//      U8* byteA——缩小后的字节数组,，
int BitArrayToByteArray(U8* bitA, int iBitLen, U8* byteA, int iByteLen)
{
    int i;
    int len;
    int retLen;

    len = min(iByteLen * 8, iBitLen);
    if (iBitLen > iByteLen * 8) {
        //截断转换
        retLen = iByteLen;
    }
    else {
        if (iBitLen % 8 != 0)
            retLen = iBitLen / 8 + 1;
        else
            retLen = iBitLen / 8;
    }

    for (i = 0; i < len; i += 8) {
        byteA[i / 8] = (U8)decode(bitA + i, 8);
    }
    return retLen;
}
//*************************************************
//名称：print_data_bit
//功能：按比特流形式打印数据缓冲区内容
//输入：U8* A——比特数组, int length——位数, int iMode——原始数据格式，0为比特流数组，1为字节数组
//输出：直接屏幕打印
void print_data_bit(U8* A, int length, int iMode)
{
    int i, j;
    U8 B[8];
    int lineCount = 0;
    cout << endl << "数据的位流：" << endl;
    if (iMode == 0) {
        for (i = 0; i < length; i++) {
            lineCount++;
            if (A[i] == 0) {
                printf("0 ");
            }
            else {
                printf("1 ");
            }
            if (lineCount % 8 == 0) {
                printf(" ");
            }
            if (lineCount >= 40) {
                printf("\n");
                lineCount = 0;
            }
        }
    }
    else {
        for (i = 0; i < length; i++) {
            lineCount++;
            code(A[i], B, 8);
            for (j = 0; j < 8; j++) {
                if (B[j] == 0) {
                    printf("0 ");
                }
                else {
                    printf("1 ");
                }
                lineCount++;
            }
            printf(" ");
            if (lineCount >= 40) {
                printf("\n");
                lineCount = 0;
            }
        }
    }
    printf("\n");
}
//*************************************************
//名称：print_data_byte
//功能：按字节流数组形式打印数据缓冲区内容,同时打印字符和十六进制数两种格式
//输入：U8* A——比特数组, int length——位数, int iMode——原始数据格式，0为比特流数组，1为字节数组
//输出：直接屏幕打印
void print_data_byte(U8* A, int length, int iMode)
{
    int linecount = 0;
    int i;

    if (iMode == 0) {
        length = BitArrayToByteArray(A, length, A, length);
    }
    cout << endl << "数据的字符流及十六进制字节流:" << endl;
    for (i = 0; i < length; i++) {
        linecount++;
        printf("%c ", A[i]);
        if (linecount >= 40) {
            printf("\n");
            linecount = 0;
        }
    }
    printf("\n");
    linecount = 0;
    for (i = 0; i < length; i++) {
        linecount++;
        printf("%02x ", (unsigned char)A[i]);
        if (linecount >= 40) {
            printf("\n");
            linecount = 0;
        }
    }
    printf("\n");
}
//end=========重要的就这些，真正需要动手改的“只有”TimeOut，RecvFromUpper，RecvFromLower=========================

//------------华丽的分割线，以下到main以前，都不用管了----------------------------
void initTimer(int interval)
{
    sBasicTimer.iType = 0;
    sBasicTimer.ulInterval = interval;//10ms,单位是微秒，10ms相对误差较小，但是也挺耗费CPU
    QueryPerformanceCounter(&sBasicTimer.llStopTime);
}
//根据系统当前时间设置select函数要用的超时时间——to，每次在select前使用
void setSelectTimeOut(timeval* to, struct threadTimer_t* sT)
{
    LARGE_INTEGER llCurrentTime;
    LARGE_INTEGER llFreq;
    LONGLONG next;
    //取系统当前时间
    QueryPerformanceFrequency(&llFreq);
    QueryPerformanceCounter(&llCurrentTime);
    if (llCurrentTime.QuadPart >= sT->llStopTime.QuadPart) {
        to->tv_sec = 0;
        to->tv_usec = 0;
        //		sT->llStopTime.QuadPart += llFreq.QuadPart * sT->ulInterval / 1000000;
    }
    else {
        next = sT->llStopTime.QuadPart - llCurrentTime.QuadPart;
        next = next * 1000000 / llFreq.QuadPart;
        to->tv_sec = (long)(next / 1000000);
        to->tv_usec = long(next % 1000000);
    }

}
//根据系统当前时间判断定时器sT是否超时，可每次在select后使用，返回值true表示超时，false表示没有超时
bool isTimeOut(struct threadTimer_t* sT)
{
    LARGE_INTEGER llCurrentTime;
    LARGE_INTEGER llFreq;
    //取系统当前时间
    QueryPerformanceFrequency(&llFreq);
    QueryPerformanceCounter(&llCurrentTime);

    if (llCurrentTime.QuadPart >= sT->llStopTime.QuadPart) {
        if (sT->iType == 0) {
            //定时器是周期性的，重置定时器
            sT->llStopTime.QuadPart += llFreq.QuadPart * sT->ulInterval / 1000000;
        }
        return true;
    }
    else {
        return false;
    }
}
//名称：code
//功能：长整数x中的指定位数，放大到A[]这个比特数组中，建议按8的倍数做
//输入：x，被放大的整数，里面包含length长度的位数
//输出：A[],放大后的比特数组
void code(unsigned long x, U8 A[], int length)
{
    unsigned long test;
    int i;
    //高位在前
    test = 1;
    test = test << (length - 1);
    for (i = 0; i < length; i++) {
        if (test & x) {
            A[i] = 1;
        }
        else {
            A[i] = 0;
        }
        test = test >> 1; //本算法利用了移位操作和"与"计算，逐位测出x的每一位是0还是1.
    }
}
//名称：decode
//功能：把比特数组A[]里的各位（元素），缩小放回到一个整数中，长度是length位，建议按8的倍数做
//输入：比特数组A[],需要变化的位长
//输出：缩小后，还原的整数
unsigned long decode(U8 A[], int length)
{
    unsigned long x;
    int i;

    x = 0;
    for (i = 0; i < length; i++) {
        if (A[i] == 0) {
            x = x << 1;;
        }
        else {
            x = x << 1;
            x = x | 1;
        }
    }
    return x;
}
//设置控制台字符串颜色，这样可以区分窗口

void SetColor(int ForgC)
{
    WORD wColor;
    //We will need this handle to get the current background attribute
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    //We use csbi for the wAttributes word.
    if (GetConsoleScreenBufferInfo(hStdOut, &csbi))
    {
        //Mask out all but the background attribute, and add in the forgournd color
        wColor = (csbi.wAttributes & 0xF0) + (ForgC & 0x0F);
        SetConsoleTextAttribute(hStdOut, wColor);
    }
}
//------------华丽的分割线，根据布局计算显示位置-----------------
//布局因子，以左上角位坐标原点
#define MAX_LAYOUTS	9 //最大支持9个节点的布局
struct Layout_t {
    float xRate;
    float yRate;
};
//按照习惯，没有0号节点，节点都从1开始编号，但是接口号等实体号是可以从0开始编号的
struct Layout_t gsLayout[MAX_LAYOUTS + 1][MAX_LAYOUTS + 1] =
{ {},{},
  {{0.0f,0.0f},{0.25f,0.50f},{0.75f,0.50f},},
  {{0.0f,0.0f},{0.25f,0.25f},{0.50f,0.55f},{0.75f,0.25f},},
  {{0.0f,0.0f},{0.25f,0.25f},{0.75f,0.25f},{0.75f,0.75f},{0.25f,0.75f},},
  {{0.0f,0.0f},{0.25f,0.25f},{0.75f,0.25f},{0.75f,0.75f},{0.25f,0.75f},{0.50f,0.50f},},
  {{0.0f,0.0f},{0.25f,0.25f},{0.50f,0.25f},{0.75f,0.25f},{0.75f,0.75f},{0.50f,0.75f},{0.25f,0.75f},},
  {{0.0f,0.0f},{0.25f,0.33f},{0.50f,0.25f},{0.75f,0.33f},{0.75f,0.66f},{0.50f,0.75f},{0.25f,0.66f},{0.50f,0.50f},},
  {{0.0f,0.0f},{0.17f,0.25f},{0.50f,0.25f},{0.83f,0.25f},{0.83f,0.75f},{0.50f,0.75f},{0.17f,0.75f},{0.33f,0.50f},{0.66f,0.50f},},
  {{0.0f,0.0f},{0.25f,0.25f},{0.50f,0.25f},{0.75f,0.25f},{0.75f,0.50f},{0.75f,0.75f},{0.50f,0.75f},{0.25f,0.75f},{0.25f,0.50f},{0.50f,0.50f}}
};
void move2DispPos(bool bShow)
{
    int cx, cy;
    RECT myRect;
    int iLeft, iTop;
    int iWidth, iHeight;
    HWND hWnd = GetConsoleWindow();
    int iDevID;
    int iEntity;

    iDevID = atoi(strDevID.c_str());
    iEntity = atoi(strEntity.c_str());

    cx = ::GetSystemMetrics(SM_CXFULLSCREEN);
    cy = ::GetSystemMetrics(SM_CYFULLSCREEN);

    ShowWindow(hWnd, SW_NORMAL);
    GetWindowRect(hWnd, &myRect);
    iWidth = myRect.right - myRect.left;
    iHeight = myRect.bottom - myRect.top;

    iLeft = (int)(gsLayout[iLayOut][iDevID].xRate * cx) - 60 + iEntity * (120 + 15);
    iTop = (int)((gsLayout[iLayOut][iDevID].yRate * cy) + 45);
    if (iLeft > cx - iWidth) {
        iLeft = cx - iWidth;
        iTop += iHeight * iEntity;
        if (iTop > cy) {
            iTop = cy - iHeight;
        }
    }

    MoveWindow(hWnd, iLeft, iTop, iWidth, iHeight, TRUE);

    if (bShow) {
        ShowWindow(hWnd, SW_NORMAL);
    }
    else {
        ShowWindow(hWnd, SW_MINIMIZE);
    }
}
// Data
static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void win() {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
    ::RegisterClassEx(&wc);
    //HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Dear ImGui DirectX11 Example"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);
    HWND hwnd = ::CreateWindowEx(WS_EX_LAYERED, wc.lpszClassName, _T("Dear ImGui DirectX11 Example"), WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, wc.hInstance, NULL);
    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);
    ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\msyh.ttc", 24.0f, NULL, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    //ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    float clear_color[] = { 0, 0, 0, 0 };
    // Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    bool done = true;
    int item_current = -1;
    vector<string> files = getname("./send/");
    int num = files.size();
    const char** names = new const char* [num];
    for (int i = 0; i < files.size(); ++i)
    {
        names[i] = files[i].c_str();
    }
    while (done) {
        if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();


        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {

            static float f = 0.0f;
            static int counter = 0;
            ImGui::Begin("APP Control", &done, ImGuiWindowFlags_NoCollapse);                          // Create a window called "Hello, world!" and append into it.
            string name = u8" 设备号:   ";
            name = name + strDevID;
            string layer = u8" IP地址: ";
            layer = layer + IP;
            string connectIP = getIP();
            ImGui::BulletText(name.c_str());               // Display some text (you can use a format strings too)
            ImGui::BulletText(layer.c_str());               // Display some text (you can use a format strings too)
            ImGui::BulletText(connectIP.c_str());               // Display some text (you can use a format strings too)
            static char sendbuf[255] = "";
            static char desbuf[32] = "";
            ImGui::BulletText(u8" 发 送 文 件 ");  // Display some text (you can use a format strings too)
            ImGui::SameLine();
            if (ImGui::Combo(" ", &item_current, names, files.size()))
            {
                if (item_current != -1)
                    strcpy(sendbuf, names[item_current]);
            }
            ImGui::BulletText(u8" 目的IP地址 ");               // Display some text (you can use a format strings too)、
            ImGui::SameLine();
            ImGui::InputText(" 目的", desbuf, IM_ARRAYSIZE(desbuf));
            HelpMarker("(?)", u8"输入目的设备的IP地址 如1.2 2.2");
            ImGui::Text("                           ");
            ImGui::SameLine();
            if (ImGui::Button(u8" 文件发送 ", ImVec2(120, 40)))selectFile(sendbuf, desbuf);                 // Buttons return true when clicked (most widgets return true when edited/activated)
            ImGui::SameLine();
            ImGui::Text("               ");
            ImGui::SameLine();
            if (ImGui::Button(u8" 断开连接 ", ImVec2(120, 40)))breakConnection();                            // Buttons return true when clicked (most widgets return true when edited/activated)
            // 在滚动区域中显示内容
            ImGui::TextColored(ImVec4(1, 0, 0, 1), u8"  日志\n");
            //ImGui::TextColored(ImVec4(1, 0, 0, 1), "  DATA");
            ImGui::BeginChild("Scrolling");
            ImGui::Text(" ");
            for (int n = 0; n < 50; n++)
                ImGui::Text(getStr((n)).c_str());
            ImGui::EndChild();
            ImGui::End();
        }


        // Rendering
        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        //g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, 0, AC_SRC_ALPHA };
        POINT pt = { 0, 0 };
        SIZE sz = { 0, 0 };
        IDXGISurface1* pSurface = NULL;
        HDC hDC = NULL;
        g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
        g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pSurface));

        DXGI_SURFACE_DESC desc;
        pSurface->GetDesc(&desc);
        sz.cx = desc.Width;
        sz.cy = desc.Height;

        pSurface->GetDC(FALSE, &hDC);
        ::UpdateLayeredWindow(hwnd, nullptr, nullptr, &sz, hDC, &pt, 0, &blend, ULW_COLORKEY);
        pSurface->ReleaseDC(nullptr);
        pSurface->Release();
    }
    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);
}
// Main code
int main(int argc, char* argv[])
{
    U8* buf;          //存放从高层、低层、各方面来的数据的缓存，大小为MAX_BUFFER_SIZE
    int len;           //buf里有效数据的大小，单位是字节
    int iRecvIntfNo;
    struct sockaddr_in remote_addr;
    WSAData wsa;
    int retval;
    fd_set readfds;
    timeval timeout;
    unsigned long arg;
    string s1, s2, s3;
    int i;
    string strTmp;
    int port;
    int tmpInt;
    buf = (char*)malloc(MAX_BUFFER_SIZE);
    if (buf == NULL) {
        //free(buf);
        cout << "内存不够" << endl;
        return 0;
    }


    if (argc == 4) {
        s1 = argv[1];
        s2 = argv[2];
        s3 = argv[3];
    }
    else if (argc == 3) {
        s1 = argv[1];
        s2 = "APP";
        s3 = argv[2];
    }
    else {
        //从键盘读取
        cout << "请输入设备号：";
        cin >> s1;
        //cout << "请输入层次名（大写）：";
        //cin >> s2;
        s2 = "APP";
        cout << "请输入实体号：";
        cin >> s3;
    }
    WSAStartup(0x101, &wsa);
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == SOCKET_ERROR)
        return 0;
    CCfgFileParms cfgParms(s1, s2, s3);

    //cfgParms.setDeviceID(s1);
    //cfgParms.setLayer(s2);
    //cfgParms.setEntityID(s3);
    //cfgParms.read();
    if (!cfgParms.isConfigExist) {
        cout << "设备文件中没有本实体" << endl;
        return 0;
    }
    if (cfgParms.getLayer().compare("APP") == 0) {
        SetColor(2);
    }

    cfgParms.print(); //打印出来看看是不是读出来了
    strDevID = cfgParms.getDeviceID();
    strLayer = cfgParms.getLayer();
    strEntity = cfgParms.getEntity();
    IP = cfgParms.getValueStr("IP");

    if (!cfgParms.isConfigExist) {
        //从键盘输入，需要连接的API端口号
        printf("Please input this Layer port: ");
        scanf_s("%d", &port);

        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
        local_addr.sin_port = htons(port);
        if (bind(sock, (sockaddr*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR) {
            return 0;
        }

        lower_addr[0].sin_family = AF_INET;
        //人工输入参数时，假设物理层模拟软件在本地,q且只有1个
        lower_addr[0].sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);

        //从键盘输入，需要连接的物理层模拟软件的端口号
        printf("Please input Lower Layer port: ");
        scanf_s("%d", &port);
        lower_addr[0].sin_port = htons(port);

        //从键盘输入，下层接口类型，除了物理层，默认都是1，物理层也要与模拟软件的upperMode一致
        printf("Please input Lower Layer mode: ");
        scanf_s("%d", &lowerMode[0]);

        //从键盘输入，工作方式
        printf("Please input Working Mode: ");
        scanf_s("%d", &iWorkMode);
        if (iWorkMode / 10 == 1) {
            //自动发送
            //从键盘输入，发送间隔和发送大小
            printf("Please input send time interval（ms）: ");
            scanf_s("%d", &autoSendTime);
            printf("Please input send size（bit）: ");
            scanf_s("%d", &autoSendSize);
        }
    }
    else {

        //取本层实体参数，并设置
        local_addr = cfgParms.getUDPAddr(CCfgFileParms::LOCAL, 0);
        local_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
        if (bind(sock, (sockaddr*)&local_addr, sizeof(sockaddr_in)) != 0) {
            printf("参数错误\n");
            return 0;

        }
        //获取工作参数
        retval = cfgParms.getValueInt(iWorkMode, (char*)"workMode");
        if (retval == -1) {
            iWorkMode = 0;
        }

        //读上层实体参数
        upper_addr = cfgParms.getUDPAddr(CCfgFileParms::UPPER, 0);

        //取下层实体参数，并设置
        //先取数量
        lowerNumber = cfgParms.getUDPAddrNumber(CCfgFileParms::LOWER);
        if (0 > lowerNumber) {
            printf("参数错误\n");
            return 0;
        }
        //逐个读取
        for (i = 0; i < lowerNumber; i++) {
            lower_addr[i] = cfgParms.getUDPAddr(CCfgFileParms::LOWER, i);

            //低层接口是Byte或者是bit,默认是字节流
            strTmp = "lowerMode";
            strTmp += std::to_string(i);
            retval = cfgParms.getValueInt(lowerMode[i], strTmp.c_str());
            if (0 > retval) {
                //没找到局部专设的参数，则再找全局值
                retval = cfgParms.getValueInt(lowerMode[i], (char*)"lowerMode");
                if (0 > retval) {
                    lowerMode[i] = 1; //默认都是字节流
                }
            }
        }

    }
    cmd_addr = cfgParms.getUDPAddr(CCfgFileParms::CMDER, 0);;

    //移动窗口到确定位置
    retval = cfgParms.getValueInt(tmpInt, "layOut");
    if (retval != -1) {
        iLayOut = tmpInt;
    }
    move2DispPos(false);
    move2DispPos(false);

    //心跳定时器
    retval = cfgParms.getValueInt(tmpInt, "heartBeatingTime");
    if (retval == 0)
        tmpInt = tmpInt * 1000;
    else {
        tmpInt = DEFAULT_TIMER_INTERVAL * 1000;
    }
    initTimer(tmpInt);

    //设置套接字为非阻塞态
    arg = 1;
    ioctlsocket(sock, FIONBIO, &arg);

    InitFunction(cfgParms);
    // Create application window
    //WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
    //::RegisterClassEx(&wc);
    ////HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Dear ImGui DirectX11 Example"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);
    //HWND hwnd = ::CreateWindowEx(WS_EX_LAYERED, wc.lpszClassName, _T("Dear ImGui DirectX11 Example"), WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, wc.hInstance, NULL);
    //// Initialize Direct3D
    //if (!CreateDeviceD3D(hwnd))
    //{
    //    CleanupDeviceD3D();
    //    ::UnregisterClass(wc.lpszClassName, wc.hInstance);
    //    return 1;
    //}

    //// Show the window
    //::ShowWindow(hwnd, SW_SHOWDEFAULT);
    //::UpdateWindow(hwnd);

    //// Setup Dear ImGui context
    //IMGUI_CHECKVERSION();
    //ImGui::CreateContext();
    //ImGuiIO& io = ImGui::GetIO(); (void)io;
    ////io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    ////io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    //// Setup Dear ImGui style
    //ImGui::StyleColorsDark();
    ////ImGui::StyleColorsClassic();

    //// Setup Platform/Renderer bindings
    //ImGui_ImplWin32_Init(hwnd);
    //ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    //// Load Fonts
    //// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    //// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    //// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    //// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    //// - Read 'docs/FONTS.txt' for more instructions and details.
    //// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    ////io.Fonts->AddFontDefault();
    ////io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    ////io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    ////io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    ////io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    ////ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    ////IM_ASSERT(font != NULL);
    ////ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\msyh.ttc", 24.0f, NULL, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    //// Our state
    //bool show_demo_window = true;
    //bool show_another_window = false;
    ////ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    //float clear_color[] = { 0, 0, 0, 0 };
    //// Main loop
    //MSG msg;
    //ZeroMemory(&msg, sizeof(msg));
    bool done = true;
    //int item_current = -1;
    //vector<string> files = getname("./send/");
    //int num = files.size();
    //const char** names = new const char* [num];
    //for (int i = 0; i < files.size(); ++i)
    //{
    //    names[i] = files[i].c_str();
    //}
    thread first(win);
    first.detach();
    while (done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
       // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
       // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
       // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
       // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        //if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        //{
        //    ::TranslateMessage(&msg);
        //    ::DispatchMessage(&msg);
        //    continue;
        //}

        //// Start the Dear ImGui frame
        //ImGui_ImplDX11_NewFrame();
        //ImGui_ImplWin32_NewFrame();
        //ImGui::NewFrame();


        //// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        //{

        //    static float f = 0.0f;
        //    static int counter = 0;
        //    ImGui::Begin("APP Control", &done, ImGuiWindowFlags_NoCollapse);                          // Create a window called "Hello, world!" and append into it.
        //    //ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.
        //    //string name = u8" 设备号:   ";
        //    string name = " Device:   ";
        //    name = name + strDevID;
        //    //string layer = u8" IP地址: ";
        //    string layer = " IP: ";
        //    layer = layer + cfgParms.getValueStr("IP");
        //    string connectIP = getIP();
        //    ImGui::BulletText(name.c_str());               // Display some text (you can use a format strings too)
        //    ImGui::BulletText(layer.c_str());               // Display some text (you can use a format strings too)
        //    ImGui::BulletText(connectIP.c_str());               // Display some text (you can use a format strings too)
        //    static char sendbuf[255] = "";
        //    static char desbuf[32] = "";
        //    //ImGui::BulletText(u8" 发 送 文 件 ");  // Display some text (you can use a format strings too)
        //    ImGui::BulletText(" Send Data ");  // Display some text (you can use a format strings too)
        //    ImGui::SameLine();
        //    if (ImGui::Combo(" s", &item_current, names, files.size()))
        //    {
        //        if (item_current != -1)
        //            strcpy(sendbuf, names[item_current]);
        //    }
        //    //ImGui::BulletText(u8" 目的IP地址 ");               // Display some text (you can use a format strings too)、
        //    ImGui::BulletText(" Destination IP ");               // Display some text (you can use a format strings too)、
        //    ImGui::SameLine();
        //    ImGui::InputText(" ", desbuf, IM_ARRAYSIZE(desbuf));
        //    //HelpMarker("(?)", u8"输入目的设备的IP地址 如1.2 2.2");
        //    //ImGui::SetColumnOffset(50, 10);
        //    ImGui::Text("                           ");
        //    ImGui::SameLine();
        //    //if (ImGui::Button(u8" 文件发送 ", ImVec2(120, 40)))selectFile(sendbuf, desbuf);                 // Buttons return true when clicked (most widgets return true when edited/activated)
        //    if (ImGui::Button(" Data Send ", ImVec2(120, 40)))selectFile(sendbuf, desbuf);                 // Buttons return true when clicked (most widgets return true when edited/activated)
        //    ImGui::SameLine();
        //    ImGui::Text("               ");
        //    ImGui::SameLine();
        //    //if (ImGui::Button(u8" 断开连接 ", ImVec2(120, 40)))breakConnection();                            // Buttons return true when clicked (most widgets return true when edited/activated)
        //    if (ImGui::Button(" Break Link ", ImVec2(120, 40)))breakConnection();                            // Buttons return true when clicked (most widgets return true when edited/activated)
        //    // 在滚动区域中显示内容
        //    //ImGui::TextColored(ImVec4(1, 0, 0, 1), u8"  日志");
        //    ImGui::TextColored(ImVec4(1, 0, 0, 1), "  DATA");
        //    ImGui::BeginChild("Scrolling");
        //    ImGui::Text(" ");
        //    for (int n = 0; n < 50; n++)
        //        ImGui::Text(getStr((n)).c_str());
        //    ImGui::EndChild();
        //    ImGui::End();
        //}


        //// Rendering
        //ImGui::Render();
        //g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        ////g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
        //g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        //ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        //BLENDFUNCTION blend = { AC_SRC_OVER, 0, 0, AC_SRC_ALPHA };
        //POINT pt = { 0, 0 };
        //SIZE sz = { 0, 0 };
        //IDXGISurface1* pSurface = NULL;
        //HDC hDC = NULL;
        //g_pSwapChain->Present(1, 0); // Present with vsync
        ////g_pSwapChain->Present(0, 0); // Present without vsync
        //g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pSurface));

        //DXGI_SURFACE_DESC desc;
        //pSurface->GetDesc(&desc);
        //sz.cx = desc.Width;
        //sz.cy = desc.Height;

        //pSurface->GetDC(FALSE, &hDC);
        //::UpdateLayeredWindow(hwnd, nullptr, nullptr, &sz, hDC, &pt, 0, &blend, ULW_COLORKEY);
        //pSurface->ReleaseDC(nullptr);
        //pSurface->Release();
        FD_ZERO(&readfds);
        //采用了基于select机制，不断发送测试数据，和接收测试数据，也可以采用多线程，一线专发送，一线专接收的方案
        //设定超时时间
        //cout << 1 << endl;
        if (sock > 0) {
            FD_SET(sock, &readfds);
        }
        setSelectTimeOut(&timeout, &sBasicTimer);
        retval = select(0, &readfds, NULL, NULL, &timeout);
        if (true == isTimeOut(&sBasicTimer)) {

            TimeOut();

            continue;
        }
        if (!FD_ISSET(sock, &readfds)) {
            continue;
        }
        len = sizeof(sockaddr_in);
        retval = recvfrom(sock, buf, MAX_BUFFER_SIZE, 0, (sockaddr*)&remote_addr, &len); //超过这个大小就不能愉快地玩耍了，因为缓冲不够大
        if (retval == 0) {
            closesocket(sock);
            sock = 0;
            printf("close a socket\n");
            continue;
        }
        else if (retval == -1) {
            retval = WSAGetLastError();
            if (retval == WSAEWOULDBLOCK || retval == WSAECONNRESET)
                continue;
            closesocket(sock);
            sock = 0;
            printf("close a socket\n");
            continue;
        }
        //收到数据后,通过源头判断是上层、下层、还是统一管理平台的命令
        if (remote_addr.sin_port == upper_addr.sin_port) {
            //IP地址也应该比对的，偷个懒
            RecvfromUpper(buf, retval);
        }
        else {
            for (iRecvIntfNo = 0; iRecvIntfNo < lowerNumber; iRecvIntfNo++) {
                //下层收到的数据,检查是哪个接口的
                if (remote_addr.sin_port == lower_addr[iRecvIntfNo].sin_port) {
                    RecvfromLower(buf, retval, iRecvIntfNo);
                    break;
                }
            }
            if (iRecvIntfNo >= lowerNumber) {
                //检查是不是控制口命令
                if (remote_addr.sin_port == cmd_addr.sin_port) {
                    if (strncmp(buf, "exit", 5) == 0) {
                        //收到退出命令
                        goto ret;
                    }
                }
                else {
                    //从其他临时性端口来的命令
                    if (strncmp(buf, "selected", 8) == 0) {
                        //收到要求突出显示的命令
                        move2DispPos(true);
                    }

                }
            }
        }
    }




ret:
    EndFunction();
    //free(buf);
    if (sock > 0)
        closesocket(sock);
    WSACleanup();
    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    RECT rcWnd;
    GetWindowRect(hWnd, &rcWnd);
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    //sd.BufferDesc.Width = 0;
    //sd.BufferDesc.Height = 0;
    //sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.Width = rcWnd.right - rcWnd.left;
    sd.BufferDesc.Height = rcWnd.bottom - rcWnd.top;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    //sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE;
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
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

// Win32 message handler
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
