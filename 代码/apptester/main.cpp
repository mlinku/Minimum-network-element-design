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
//============�ص�����================================================================================================
//------------��Ҫ�Ŀ��Ʋ���------------------------------------------------------------
//���´������ļ�ne.txt�ж�ȡ����Ҫ����
int lowerMode[10]; //����Ͳ��������ģ����������������õ���ÿ���ӿڶ�Ӧ�����ݸ�ʽ����0Ϊ�������飬1Ϊ�ֽ�����
int lowerNumber;   //�Ͳ�ʵ�����������������֮�¿����ж����·��
int iWorkMode = 0; //����ʵ�幤��ģʽ
int autoSendTime = DEFAULT_AUTO_SEND_TIME;
int autoSendSize = DEFAULT_AUTO_SEND_SIZE;
int recvSensitivity = DEFAULT_RECEIVE_SENSITIVITY;
string strDevID;    //�豸�ţ��ַ�����ʽ����1��ʼ
string strLayer;    //�����
string strEntity;   //ʵ��ã��ַ�����ʽ����0��ʼ������ͨ��atoi������������ڳ�����ʹ��
int iLayOut = 1;		//���ֱ�ţ��������еĽڵ�����
string IP;
//������һЩ��Ҫ����ͨ���йصĿ��Ʋ���������Ҳ���Բ��ù�
SOCKET sock;
struct sockaddr_in local_addr;      //����ʵ���ַ
struct sockaddr_in upper_addr;      //�ϲ�ʵ���ַ��һ������£��ϲ�ʵ��ֻ��1��
struct sockaddr_in lower_addr[10];  //���10���²���������±�����²�ʵ��ı��
sockaddr_in cmd_addr;         //ͳһ����ƽ̨��ַ

//------------�����ķָ��ߣ������Ƕ�ʱ��--------------------------------------------
//����select�Ķ�ʱ����Ŀ���ǰ����ݵ��շ��Ͷ�ʱ��ͳһ��һ���¼����������
//�����ж����ʱ���������ʵ����һ����׼��ʱ����Ϊ������10ms��ʱ��Ҳ���Ե�����һ��������ʱ��
//����Ķ�ʱ�������������������ɣ����еķ������ڶ���
//�������˼·��������չ��������Ҫ
//��׼��ʱ��һ�����ͻ���������һ��
struct threadTimer_t {
    int iType;  //Ϊ0��ʾ�����Զ�ʱ������ʱ�ﵽ�󣬻��Զ�������һ�ζ�ʱ
    ULONG ulInterval;
    LARGE_INTEGER llStopTime;
}sBasicTimer;  //ȫ��Ψһ�ļ�ʱ����Ĭ����ÿ10���봥��һ�Σ�����߿����������ʱ���Ļ�����ʵ���Լ��ĸ��ֶ�ʱ��
               //�������100���Ժ󣬾���1�롣��Բ�ͬ���¼���������������һ��������ʾ�Ƿ�ʼ��ʱ��һ��������ʾ���������
//*************************************************
//���ƣ�StartTimerOnce
//���ܣ���ȫ�ּ�ʱ����Ϊ1���ԣ��������õ�ͬʱ��ʱҲ��ʼ������ʱ����ʱTimeOut������������
//���룺��ʱ���ʱ�䣬��λ΢�룬��ʱ����ʼ���Ǳ���������ʱ��
//�����ֱ�Ӹı�ȫ�ּ�ʱ������ sBasicTimer������
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

        //����'.'��'..'����Ŀ¼
        if (ptr->d_name[0] == '.')
            continue;
        //cout << ptr->d_name << endl;
        files.push_back(ptr->d_name);
    }

    return files;
}
//*************************************************
//���ƣ�StartTimerPeriodically
//���ܣ�����ȫ�ּ�ʱ���������Դ����ļ��ʱ�䣬ͬʱ�����Լ�ʱ��Ҳ��ʼ������ÿ�μ�ʱ����TimeOut����������1��
//      �����Լ�ʱ�����Խ�������һЩ�������Ϳ��������Ļ�����ʵ�ָ���ĸ��ֶ�ʱʱ��Ķ�ʱ����
//      ����Ĭ�������������Լ�ʱ������
//���룺��ʱ���ʱ�䣬��λ΢�룬��ʱ����ʼ���Ǳ���������ʱ��
//�����ֱ�Ӹı�ȫ�ּ�ʱ������ sBasicTimer����
void StartTimerPeriodically(ULONG ulInterval)
{
    LARGE_INTEGER llFreq;

    sBasicTimer.iType = 0;
    sBasicTimer.ulInterval = ulInterval;
    QueryPerformanceFrequency(&llFreq);
    QueryPerformanceCounter(&sBasicTimer.llStopTime);
    sBasicTimer.llStopTime.QuadPart += llFreq.QuadPart * sBasicTimer.ulInterval / 1000000;
}

//***************��Ҫ��������******************************
//���ƣ�SendtoUpper
//���ܣ���߲�ʵ��ݽ�����ʱ��ʹ���������
//���룺U8 * buf,׼���ݽ������ݣ� int len�����ݳ��ȣ���λ�ֽڣ�int ifNo
//�������������ֵ�Ƿ��͵�������
int SendtoUpper(U8* buf, int len)
{
    int sendlen;
    sendlen = sendto(sock, buf, len, 0, (sockaddr*)&(upper_addr), sizeof(sockaddr_in));
    return sendlen;
}
//***************��Ҫ��������******************************
//���ƣ�SendtoLower
//���ܣ���Ͳ�ʵ���·�����ʱ��ʹ���������
//���룺U8 * buf,׼���·������ݣ� int len�����ݳ��ȣ���λ�ֽ�,int ifNo,�����ĵͲ�ӿں�
//�������������ֵ�Ƿ��͵�������
int SendtoLower(U8* buf, int len, int ifNo)
{
    int sendlen;
    if (ifNo < 0 || ifNo >= lowerNumber)
        return 0;
    sendlen = sendto(sock, buf, len, 0, (sockaddr*)&(lower_addr[ifNo]), sizeof(sockaddr_in));
    return sendlen;
}
//***************��Ҫ��������******************************
//���ƣ�SendtoCommander
//���ܣ���ͳһ����ƽ̨����״̬����ʱ��ʹ���������
//���룺U8 * buf,׼���·������ݣ� int len�����ݳ��ȣ���λ�ֽ�
//�������������ֵ�Ƿ��͵�������
int SendtoCommander(U8* buf, int len)
{
    int sendlen;
    sendlen = sendto(sock, buf, len, 0, (sockaddr*)&(cmd_addr), sizeof(sockaddr_in));
    return sendlen;
}

//------------�����ķָ��ߣ�������һЩ���ݴ���Ĺ��ߺ����������ã�û��Ҫ��------------------------------
//*************************************************
//���ƣ�ByteArrayToBitArray
//���ܣ����ֽ��������Ŵ�Ϊ����������
//���룺 int iBitLen����λ������, U8* byteA�������Ŵ��ֽ�����, int iByteLen�����ֽ����鳤��
//�������������ֵ��ת�����ж���λ��
//      U8* bitA,�������飬ע���������Ŀռ䣨��������С����Ӧ���ֽ������8��
int ByteArrayToBitArray(U8* bitA, int iBitLen, U8* byteA, int iByteLen)
{
    int i;
    int len;

    len = min(iByteLen, iBitLen / 8);
    for (i = 0; i < len; i++) {
        //ÿ�α���8λ
        code(byteA[i], &(bitA[i * 8]), 8);
    }
    return len * 8;
}
//*************************************************
//���ƣ�BitArrayToByteArray
//���ܣ����ֽ��������Ŵ�Ϊ����������
//���룺U8* bitA,�������飬int iBitLen����λ������,  int iByteLen�����ֽ����鳤��
//      ע���������Ŀռ䣨��������С����Ӧ���ֽ������8��
//���������ֵ��ת�����ж��ٸ��ֽڣ����λ�����Ȳ���8λ�������������1�ֽڲ�����
//      U8* byteA������С����ֽ�����,��
int BitArrayToByteArray(U8* bitA, int iBitLen, U8* byteA, int iByteLen)
{
    int i;
    int len;
    int retLen;

    len = min(iByteLen * 8, iBitLen);
    if (iBitLen > iByteLen * 8) {
        //�ض�ת��
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
//���ƣ�print_data_bit
//���ܣ�����������ʽ��ӡ���ݻ���������
//���룺U8* A������������, int length����λ��, int iMode����ԭʼ���ݸ�ʽ��0Ϊ���������飬1Ϊ�ֽ�����
//�����ֱ����Ļ��ӡ
void print_data_bit(U8* A, int length, int iMode)
{
    int i, j;
    U8 B[8];
    int lineCount = 0;
    cout << endl << "���ݵ�λ����" << endl;
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
//���ƣ�print_data_byte
//���ܣ����ֽ���������ʽ��ӡ���ݻ���������,ͬʱ��ӡ�ַ���ʮ�����������ָ�ʽ
//���룺U8* A������������, int length����λ��, int iMode����ԭʼ���ݸ�ʽ��0Ϊ���������飬1Ϊ�ֽ�����
//�����ֱ����Ļ��ӡ
void print_data_byte(U8* A, int length, int iMode)
{
    int linecount = 0;
    int i;

    if (iMode == 0) {
        length = BitArrayToByteArray(A, length, A, length);
    }
    cout << endl << "���ݵ��ַ�����ʮ�������ֽ���:" << endl;
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
//end=========��Ҫ�ľ���Щ��������Ҫ���ָĵġ�ֻ�С�TimeOut��RecvFromUpper��RecvFromLower=========================

//------------�����ķָ��ߣ����µ�main��ǰ�������ù���----------------------------
void initTimer(int interval)
{
    sBasicTimer.iType = 0;
    sBasicTimer.ulInterval = interval;//10ms,��λ��΢�룬10ms�������С������Ҳͦ�ķ�CPU
    QueryPerformanceCounter(&sBasicTimer.llStopTime);
}
//����ϵͳ��ǰʱ������select����Ҫ�õĳ�ʱʱ�䡪��to��ÿ����selectǰʹ��
void setSelectTimeOut(timeval* to, struct threadTimer_t* sT)
{
    LARGE_INTEGER llCurrentTime;
    LARGE_INTEGER llFreq;
    LONGLONG next;
    //ȡϵͳ��ǰʱ��
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
//����ϵͳ��ǰʱ���ж϶�ʱ��sT�Ƿ�ʱ����ÿ����select��ʹ�ã�����ֵtrue��ʾ��ʱ��false��ʾû�г�ʱ
bool isTimeOut(struct threadTimer_t* sT)
{
    LARGE_INTEGER llCurrentTime;
    LARGE_INTEGER llFreq;
    //ȡϵͳ��ǰʱ��
    QueryPerformanceFrequency(&llFreq);
    QueryPerformanceCounter(&llCurrentTime);

    if (llCurrentTime.QuadPart >= sT->llStopTime.QuadPart) {
        if (sT->iType == 0) {
            //��ʱ���������Եģ����ö�ʱ��
            sT->llStopTime.QuadPart += llFreq.QuadPart * sT->ulInterval / 1000000;
        }
        return true;
    }
    else {
        return false;
    }
}
//���ƣ�code
//���ܣ�������x�е�ָ��λ�����Ŵ�A[]������������У����鰴8�ı�����
//���룺x�����Ŵ���������������length���ȵ�λ��
//�����A[],�Ŵ��ı�������
void code(unsigned long x, U8 A[], int length)
{
    unsigned long test;
    int i;
    //��λ��ǰ
    test = 1;
    test = test << (length - 1);
    for (i = 0; i < length; i++) {
        if (test & x) {
            A[i] = 1;
        }
        else {
            A[i] = 0;
        }
        test = test >> 1; //���㷨��������λ������"��"���㣬��λ���x��ÿһλ��0����1.
    }
}
//���ƣ�decode
//���ܣ��ѱ�������A[]��ĸ�λ��Ԫ�أ�����С�Żص�һ�������У�������lengthλ�����鰴8�ı�����
//���룺��������A[],��Ҫ�仯��λ��
//�������С�󣬻�ԭ������
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
//���ÿ���̨�ַ�����ɫ�������������ִ���

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
//------------�����ķָ��ߣ����ݲ��ּ�����ʾλ��-----------------
//�������ӣ������Ͻ�λ����ԭ��
#define MAX_LAYOUTS	9 //���֧��9���ڵ�Ĳ���
struct Layout_t {
    float xRate;
    float yRate;
};
//����ϰ�ߣ�û��0�Žڵ㣬�ڵ㶼��1��ʼ��ţ����ǽӿںŵ�ʵ����ǿ��Դ�0��ʼ��ŵ�
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
            string name = u8" �豸��:   ";
            name = name + strDevID;
            string layer = u8" IP��ַ: ";
            layer = layer + IP;
            string connectIP = getIP();
            ImGui::BulletText(name.c_str());               // Display some text (you can use a format strings too)
            ImGui::BulletText(layer.c_str());               // Display some text (you can use a format strings too)
            ImGui::BulletText(connectIP.c_str());               // Display some text (you can use a format strings too)
            static char sendbuf[255] = "";
            static char desbuf[32] = "";
            ImGui::BulletText(u8" �� �� �� �� ");  // Display some text (you can use a format strings too)
            ImGui::SameLine();
            if (ImGui::Combo(" ", &item_current, names, files.size()))
            {
                if (item_current != -1)
                    strcpy(sendbuf, names[item_current]);
            }
            ImGui::BulletText(u8" Ŀ��IP��ַ ");               // Display some text (you can use a format strings too)��
            ImGui::SameLine();
            ImGui::InputText(" Ŀ��", desbuf, IM_ARRAYSIZE(desbuf));
            HelpMarker("(?)", u8"����Ŀ���豸��IP��ַ ��1.2 2.2");
            ImGui::Text("                           ");
            ImGui::SameLine();
            if (ImGui::Button(u8" �ļ����� ", ImVec2(120, 40)))selectFile(sendbuf, desbuf);                 // Buttons return true when clicked (most widgets return true when edited/activated)
            ImGui::SameLine();
            ImGui::Text("               ");
            ImGui::SameLine();
            if (ImGui::Button(u8" �Ͽ����� ", ImVec2(120, 40)))breakConnection();                            // Buttons return true when clicked (most widgets return true when edited/activated)
            // �ڹ�����������ʾ����
            ImGui::TextColored(ImVec4(1, 0, 0, 1), u8"  ��־\n");
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
    U8* buf;          //��ŴӸ߲㡢�Ͳ㡢�������������ݵĻ��棬��СΪMAX_BUFFER_SIZE
    int len;           //buf����Ч���ݵĴ�С����λ���ֽ�
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
        cout << "�ڴ治��" << endl;
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
        //�Ӽ��̶�ȡ
        cout << "�������豸�ţ�";
        cin >> s1;
        //cout << "��������������д����";
        //cin >> s2;
        s2 = "APP";
        cout << "������ʵ��ţ�";
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
        cout << "�豸�ļ���û�б�ʵ��" << endl;
        return 0;
    }
    if (cfgParms.getLayer().compare("APP") == 0) {
        SetColor(2);
    }

    cfgParms.print(); //��ӡ���������ǲ��Ƕ�������
    strDevID = cfgParms.getDeviceID();
    strLayer = cfgParms.getLayer();
    strEntity = cfgParms.getEntity();
    IP = cfgParms.getValueStr("IP");

    if (!cfgParms.isConfigExist) {
        //�Ӽ������룬��Ҫ���ӵ�API�˿ں�
        printf("Please input this Layer port: ");
        scanf_s("%d", &port);

        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
        local_addr.sin_port = htons(port);
        if (bind(sock, (sockaddr*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR) {
            return 0;
        }

        lower_addr[0].sin_family = AF_INET;
        //�˹��������ʱ�����������ģ������ڱ���,q��ֻ��1��
        lower_addr[0].sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);

        //�Ӽ������룬��Ҫ���ӵ������ģ������Ķ˿ں�
        printf("Please input Lower Layer port: ");
        scanf_s("%d", &port);
        lower_addr[0].sin_port = htons(port);

        //�Ӽ������룬�²�ӿ����ͣ���������㣬Ĭ�϶���1�������ҲҪ��ģ�������upperModeһ��
        printf("Please input Lower Layer mode: ");
        scanf_s("%d", &lowerMode[0]);

        //�Ӽ������룬������ʽ
        printf("Please input Working Mode: ");
        scanf_s("%d", &iWorkMode);
        if (iWorkMode / 10 == 1) {
            //�Զ�����
            //�Ӽ������룬���ͼ���ͷ��ʹ�С
            printf("Please input send time interval��ms��: ");
            scanf_s("%d", &autoSendTime);
            printf("Please input send size��bit��: ");
            scanf_s("%d", &autoSendSize);
        }
    }
    else {

        //ȡ����ʵ�������������
        local_addr = cfgParms.getUDPAddr(CCfgFileParms::LOCAL, 0);
        local_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
        if (bind(sock, (sockaddr*)&local_addr, sizeof(sockaddr_in)) != 0) {
            printf("��������\n");
            return 0;

        }
        //��ȡ��������
        retval = cfgParms.getValueInt(iWorkMode, (char*)"workMode");
        if (retval == -1) {
            iWorkMode = 0;
        }

        //���ϲ�ʵ�����
        upper_addr = cfgParms.getUDPAddr(CCfgFileParms::UPPER, 0);

        //ȡ�²�ʵ�������������
        //��ȡ����
        lowerNumber = cfgParms.getUDPAddrNumber(CCfgFileParms::LOWER);
        if (0 > lowerNumber) {
            printf("��������\n");
            return 0;
        }
        //�����ȡ
        for (i = 0; i < lowerNumber; i++) {
            lower_addr[i] = cfgParms.getUDPAddr(CCfgFileParms::LOWER, i);

            //�Ͳ�ӿ���Byte������bit,Ĭ�����ֽ���
            strTmp = "lowerMode";
            strTmp += std::to_string(i);
            retval = cfgParms.getValueInt(lowerMode[i], strTmp.c_str());
            if (0 > retval) {
                //û�ҵ��ֲ�ר��Ĳ�����������ȫ��ֵ
                retval = cfgParms.getValueInt(lowerMode[i], (char*)"lowerMode");
                if (0 > retval) {
                    lowerMode[i] = 1; //Ĭ�϶����ֽ���
                }
            }
        }

    }
    cmd_addr = cfgParms.getUDPAddr(CCfgFileParms::CMDER, 0);;

    //�ƶ����ڵ�ȷ��λ��
    retval = cfgParms.getValueInt(tmpInt, "layOut");
    if (retval != -1) {
        iLayOut = tmpInt;
    }
    move2DispPos(false);
    move2DispPos(false);

    //������ʱ��
    retval = cfgParms.getValueInt(tmpInt, "heartBeatingTime");
    if (retval == 0)
        tmpInt = tmpInt * 1000;
    else {
        tmpInt = DEFAULT_TIMER_INTERVAL * 1000;
    }
    initTimer(tmpInt);

    //�����׽���Ϊ������̬
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
        //    //string name = u8" �豸��:   ";
        //    string name = " Device:   ";
        //    name = name + strDevID;
        //    //string layer = u8" IP��ַ: ";
        //    string layer = " IP: ";
        //    layer = layer + cfgParms.getValueStr("IP");
        //    string connectIP = getIP();
        //    ImGui::BulletText(name.c_str());               // Display some text (you can use a format strings too)
        //    ImGui::BulletText(layer.c_str());               // Display some text (you can use a format strings too)
        //    ImGui::BulletText(connectIP.c_str());               // Display some text (you can use a format strings too)
        //    static char sendbuf[255] = "";
        //    static char desbuf[32] = "";
        //    //ImGui::BulletText(u8" �� �� �� �� ");  // Display some text (you can use a format strings too)
        //    ImGui::BulletText(" Send Data ");  // Display some text (you can use a format strings too)
        //    ImGui::SameLine();
        //    if (ImGui::Combo(" s", &item_current, names, files.size()))
        //    {
        //        if (item_current != -1)
        //            strcpy(sendbuf, names[item_current]);
        //    }
        //    //ImGui::BulletText(u8" Ŀ��IP��ַ ");               // Display some text (you can use a format strings too)��
        //    ImGui::BulletText(" Destination IP ");               // Display some text (you can use a format strings too)��
        //    ImGui::SameLine();
        //    ImGui::InputText(" ", desbuf, IM_ARRAYSIZE(desbuf));
        //    //HelpMarker("(?)", u8"����Ŀ���豸��IP��ַ ��1.2 2.2");
        //    //ImGui::SetColumnOffset(50, 10);
        //    ImGui::Text("                           ");
        //    ImGui::SameLine();
        //    //if (ImGui::Button(u8" �ļ����� ", ImVec2(120, 40)))selectFile(sendbuf, desbuf);                 // Buttons return true when clicked (most widgets return true when edited/activated)
        //    if (ImGui::Button(" Data Send ", ImVec2(120, 40)))selectFile(sendbuf, desbuf);                 // Buttons return true when clicked (most widgets return true when edited/activated)
        //    ImGui::SameLine();
        //    ImGui::Text("               ");
        //    ImGui::SameLine();
        //    //if (ImGui::Button(u8" �Ͽ����� ", ImVec2(120, 40)))breakConnection();                            // Buttons return true when clicked (most widgets return true when edited/activated)
        //    if (ImGui::Button(" Break Link ", ImVec2(120, 40)))breakConnection();                            // Buttons return true when clicked (most widgets return true when edited/activated)
        //    // �ڹ�����������ʾ����
        //    //ImGui::TextColored(ImVec4(1, 0, 0, 1), u8"  ��־");
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
        //�����˻���select���ƣ����Ϸ��Ͳ������ݣ��ͽ��ղ������ݣ�Ҳ���Բ��ö��̣߳�һ��ר���ͣ�һ��ר���յķ���
        //�趨��ʱʱ��
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
        retval = recvfrom(sock, buf, MAX_BUFFER_SIZE, 0, (sockaddr*)&remote_addr, &len); //���������С�Ͳ���������ˣ�ˣ���Ϊ���岻����
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
        //�յ����ݺ�,ͨ��Դͷ�ж����ϲ㡢�²㡢����ͳһ����ƽ̨������
        if (remote_addr.sin_port == upper_addr.sin_port) {
            //IP��ַҲӦ�ñȶԵģ�͵����
            RecvfromUpper(buf, retval);
        }
        else {
            for (iRecvIntfNo = 0; iRecvIntfNo < lowerNumber; iRecvIntfNo++) {
                //�²��յ�������,������ĸ��ӿڵ�
                if (remote_addr.sin_port == lower_addr[iRecvIntfNo].sin_port) {
                    RecvfromLower(buf, retval, iRecvIntfNo);
                    break;
                }
            }
            if (iRecvIntfNo >= lowerNumber) {
                //����ǲ��ǿ��ƿ�����
                if (remote_addr.sin_port == cmd_addr.sin_port) {
                    if (strncmp(buf, "exit", 5) == 0) {
                        //�յ��˳�����
                        goto ret;
                    }
                }
                else {
                    //��������ʱ�Զ˿���������
                    if (strncmp(buf, "selected", 8) == 0) {
                        //�յ�Ҫ��ͻ����ʾ������
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
