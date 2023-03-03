//apptester的功能文件
#include <iostream>
#include <conio.h>
#include "winsock.h"
#include <vector>
#include "stdio.h"
#include "CfgFileParms.h"
#include "function.h"
#include "base64.h"
#include <io.h>
#include <direct.h>
#include <numeric>
#pragma warning(disable:4996)
using namespace std;

//----------------华丽的分割线，一些统计用的全局变量----------------
//====================数据控制相关====================
U8* autoSendBuf;  //用来组织自动发送数据的缓存
U8* sendBuf;     // 用来组织发送文本数据的缓存
int ltime = 0;  // 全局时钟用于处理超时重传
int endtime = -1; // 记录结束时间 用于四次挥手
int reportTime; //超时重传的时间
int spin = 1;  //  打印动态信息控制
string outPath; // 记录接收文件的文件路径
string readPath; // 记录发送文件的文件路径
int iSndTotal = 0;  // 记录发送数据总量
int iSndTotalCount = 0; // 记录发送数据总次数
int iSndErrorCount = 0;  // 记录发送错误次数
int iRcvTotal = 0;     // 记录接收数据总量
int iRcvTotalCount = 0; // 记录接收数据总次数
//=====================序号处理相关=====================
int cwnd = 1; // 拥塞窗口大小 
int ssthresh = 8; // 慢启动门限
int winacc = 0; // 记录收到的帧数目，用于拥塞避免的窗口控制
U8 lastRecvNo = 255; // 记录前一个收到报文的序号
U8 lastUpNo = 0; // 记录上一次上发的序号 
U8 lastRecvACK = 255; // 记录前一个接收ACK序号
int noNums = 0; // 记录同一序号ACK收到的次数
int status = 0; // 判断拥塞是否发生 如果发生为1 未发生为0 
int cNums = 0; // 用于判断序号经过几轮循环 用于发送窗口的出队 
int fNums = 0; // 单位时间内收到的帧数目 用于模拟流量控制
U8 endACK = 255; // 结束的ACK序号 用于处理四次挥手
vector<string> mid_buffer; // 用于存放切割报文的缓冲区
int mid_len;
//======================报文发送相关======================
int headerLen = 4; // 头部长度
int iciLen = 2; // ICI长度
string strIP; // 本设备IP地址
U8 uConnectIP[2]; // 记录当前连接的IP地址 若第一位为0表示无连接
U8 uAutoIP[2]; //  记录自动发送的IP地址 若第一位为0表示无连接
U8 save; // 用于存放一些偶尔要暂时存储的数据
int autoTime = 0; // 记录自动发送的次数
//======================日志记录相关======================
int lastRecord = -1; // 上一个记录到日志的数据
string msgRecord[50]; // 用于存放日志，在窗口显示

//----------------华丽的分割线，一些重要的结构体----------------
// APP层发往下层的ICI
struct APP_SndICI {
    U8 uDesIP[2]; // 目的IP地址
};

// APP层收到下层的ICI
struct APP_RcvICI {
    U8 uSrcIP[2]; // 来源IP地址
};

// APP层报文头部
struct APP_Header {
    U8 Sequence_Number;  // 发送的报文序列号 
    U8 Acknowledgment_Number; // 确认收到的报文序列号 只有标志位ACK为1时有效
    U8 Flags; // 标志位 
    U8 Rnums; // 剩余接收帧能力 
};

// 发送窗口
struct send_queue { // 四指针循环队列缓冲区设计 窗口必定会比缓冲区小 不然会出错~
    int front; // 指向已发送但是未收到确认的第一个字节序列号
    int nxt;  //指向可发送未发送的第一个字节序列号
    int rear; //指向不可发送未发送的第一个字节序列号 窗口
    int size; // 指向缓冲区的最后一个数据		
    U8* data[MAX_SEND]; // 缓冲区数据集合
    int len[MAX_SEND]; // 缓冲区每个数据的长度
    int reTime[MAX_SEND];  // 缓冲区数据的超时重发记录
    int reNo[MAX_SEND]; // 缓冲区数据超发次数
};

// 接收窗口
struct recv_queue { // 普普通通的循环队列
    int front; // 指向窗口内第一个数据
    int rear; // 指向窗口内最后一个数据的后一位
    int len[MAX_RECV]; // 缓冲区每个数据的长度
    U8* data[MAX_RECV]; // 缓存区数据集合
};

// 初始化发送窗口
struct send_queue send_buffer;
// 初始化接收窗口
struct recv_queue recv_buffer;


//----------------华丽的分割线，切割报文处理相关的函数----------------

void sendMidBuffer(vector<string>& data_buffer) {
    string alldata;
    //alldata = accumulate(data_buffer.begin(), data_buffer.end(), alldata);
    for (int i = 0; i < data_buffer.size(); i++) {
        alldata.append(data_buffer[i]);

    }
    data_buffer.clear();
    mid_len = 0;
    writeData(alldata, alldata.size());
}
void addMidBuffer(vector<string>& data_buffer, string data, int flag, int len) {
    data = data.substr(0, len);
    mid_len += len;
    data_buffer.push_back(data);
    if (flag == 1 || flag == 3)
        sendMidBuffer(data_buffer);
}


//----------------华丽的分割线，窗口相关的函数----------------
// 发送窗口和接收窗口的初始化
void initqueue(struct send_queue* sendque, struct recv_queue* recvque)
{
    sendque->front = sendque->nxt = sendque->size = 0;
    sendque->rear = cwnd;
    recvque->front = recvque->rear = 0;
}

// 发送窗口和接收窗口的释放
void freequeue(struct send_queue* sendque)
{
    int i;
    for (i = sendque->front; i != sendque->size; i = (i + 1) % MAX_SEND) {
        if (sendque->data[i] != NULL)
            free(sendque->data[i]);
    }
    free(sendque->len);
    free(sendque->reNo);
    free(sendque->reTime);
}

// 判断窗口是否满  
bool isfullque(struct send_queue* que) {
    if ((que->size + 1) % MAX_SEND == que->front)
        return true;
    return false;
}

// 判断拥塞窗口是否满
bool isfullwin(struct send_queue* que) {
    if (que->nxt == que->rear)
        return true;
    return false;
}

// 发送窗口入队函数
int enSendQueue(struct send_queue* que, U8* buf, int len)
{
    // 入队先判断缓冲区
    if ((que->size + 1) % MAX_SEND == que->front)
        return -1;
    que->data[que->size] = buf;
    que->len[que->size] = len;
    que->reTime[que->size] = -1;
    que->reNo[que->size] = 0;
    que->size = (que->size + 1) % MAX_SEND;
    return 0;
}

// 发送窗口出队函数 窗口移动
U8* deSendQueue(struct send_queue* que)
{
    //cout << "出队前" << endl;
    //cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl;
    U8* buf;
    if (que->front == que->nxt)
        return NULL;

    buf = que->data[que->front];
    //*len = que->len[que->front];Time
    que->front = (que->front + 1) % MAX_SEND;
    que->rear = (que->rear + 1) % MAX_SEND;
    //if (uConnectIP != 0 && isautoend == 1 && que->front == que->nxt && que->nxt == que->size) {
    //    isautoend = 0;
    //    sendFIN(&send_buffer, uConnectIP);
    //}
    ////cout << "出队后" << endl;
    //cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl;
    return buf;
}

// 接收窗口入队函数
void enRecvQueue(struct recv_queue* que, U8* buf, int seq, int len) {

    cout << seq << endl;
    cout << "放入前接收窗口内报文的序号" << endl;
    APP_Header* header1;
    if (que->front == que->rear)
    {
        cout << "无" << endl;
    }
    else
    {
        for (int i = que->front; i < que->rear; i++) {
            if (que->data[i] == NULL)
                cout << "无" << " ";
            else {
                header1 = (APP_Header*)&que->data[i][iciLen];
                cout << (int)header1->Sequence_Number << " ";
            }

        }
        cout << endl;
    }
    cout << seq << " " << (int)lastRecvNo << " " << que->front << " " << endl;
    //int shift = (seq - (int)lastRecvNo - 2 + MAX_RECV + que->front) % MAX_RECV; // 该报文应该在窗口内放置的位置
    int shift = (seq - (int)lastRecvNo - 1 + MAX_RECV + que->front) % MAX_RECV; // 该报文应该在窗口内放置的位置
    cout << shift << endl;
    if ((seq - (int)lastRecvNo - 2 + MAX_RECV) % MAX_RECV < (que->rear + MAX_RECV) % MAX_RECV) // 判断该报文是不是在接收窗口头部和尾部中间
    {
        que->data[shift] = buf;
        que->len[shift] = len;
    }
    else {
        for (int i = que->rear; i < shift; i++) {
            que->data[i] = NULL; // 如果该报文应该在窗口内放置的位置不在尾部放入空值
        }
        que->data[shift] = buf;
        que->len[shift] = len;
        que->rear = (shift + 1) % MAX_RECV;
        cout << "放入后接收窗口内报文的序号" << endl;
        cout << que->front << " " << que->rear << endl;
    }


    for (int i = que->front; i < que->rear; i++) {
        if (que->data[i] == NULL)
            cout << "无" << " ";
        else {
            header1 = (APP_Header*)&que->data[i][iciLen];
            cout << (int)header1->Sequence_Number << " ";
        }

    }
    cout << endl;
}

// 接收窗口出队函数
U8* deRecvQueue(struct recv_queue* que, int* len) {
    U8* buf;
    if (que->front == que->rear)
        return NULL;
    buf = que->data[que->front];
    *len = que->len[que->front];
    que->front = (que->front + 1) % MAX_RECV;
    return buf;
}

// 检查接收窗口是否需要出队
void checkRecvQueue(struct recv_queue* que) {
    cout << "检查出队前接收窗口内报文的序号" << endl;
    APP_Header* header;
    int len = 0;
    if (que->front == que->rear)
    {
        cout << "无" << endl;
    }
    else
    {
        for (int i = que->front; i < que->rear; i++) {
            if (que->data[i] == NULL)
                cout << "无" << " ";
            else {
                header = (APP_Header*)&que->data[i][iciLen];
                cout << (int)header->Sequence_Number << " ";
            }
        }
        cout << endl;
    }
    for (int i = que->front + 1; i < que->rear && que->data[i] != NULL; i++) {
        header = (APP_Header*)&que->data[i][iciLen];
        if (lastRecvNo + 1 == header->Sequence_Number) {
            cout << int(header->Sequence_Number) << "出队" << endl;
            lastRecvNo = header->Sequence_Number;
            que->front++;
            if (header->Flags == U8(1)) {
                if (que->data[i][iciLen + headerLen])
                    writeData(deRecvQueue(que, &len), len);
                else {
                    print_data_byte(&que->data[i][iciLen + headerLen + 1], que->len[i] - iciLen - headerLen - 1, 1);
                }
                lastUpNo = header->Sequence_Number;
            }
            else {
                addMidBuffer(mid_buffer, &que->data[i][iciLen + headerLen + 1], int(header->Flags), que->len[i] - iciLen - headerLen);
            }



        }
        else {
            if (i != que->front + 1)
                que->front++;
            break; //如果不满足该序号为上次接收序号+1 直接退出循环
        }
    }
    cout << "检查出队后接收窗口内报文的序号" << endl;
    if (que->front == que->front)
        cout << "窗口为空" << endl;
    else
        for (int i = que->front; i < que->rear; i++) {
            if (que->data[i] == NULL)
                cout << "无" << " ";
            else {
                header = (APP_Header*)&que->data[i][iciLen];
                cout << (int)header->Sequence_Number << " ";
            }
        }
}



//----------------华丽的分割线，文本处理相关的函数----------------
// 读取文件数据
int readData(string file, U8* data) {
    ifstream f(readPath + file, ios::in | ios::binary);
    if (!f) {
        cerr << "Can't open the file." << endl;
        return -1;
    }
    f.seekg(0, std::ios_base::end);
    std::streampos sp = f.tellg();
    int size = sp;
    //cout << size << endl;
    char* buffer = (char*)malloc(sizeof(char) * size);
    f.seekg(0, std::ios_base::beg);//把文件指针移到到文件头位置
    f.read(buffer, size);
    //char* bufSend = (char*)malloc(sizeof(char) * size + 1 + file.size());
    //cout << "xxx" << file.size() << endl;
    //bufSend[0] = U8(file.size());
    //strcpy(&bufSend[1], file.c_str());
    //strcpy(&bufSend[1 + file.size()], buffer);

    //cout << "file size:" << size << endl;
    string imgBase64;
    imgBase64 = imgBase64 + U8(file.size()) + file + base64_encode(buffer, size);
    cout << "blen	" << base64_encode(buffer, size).size() << endl;
    //cout << base64_encode(buffer, size) << endl;
    //string imgBase64 = base64_encode(bufSend, size);
    //cout << sizeof(file.size()) << endl << U8(file.size()) << endl;
    //cout << "img base64 encode size:" << imgBase64.size() << endl;
    f.close();
    int len = imgBase64.size();
    cout << "l2en	" << len << endl;

    strcpy(data, imgBase64.c_str());
    return len;
}

// 向文件写入数据
int writeData(string data, int len) {
    if (_access(outPath.c_str(), 0) == -1)
    {
        _mkdir(outPath.c_str());
    }
    cout << data.size() << endl;
    cout << len << endl;
    int file_len = int(data.at(0));
    string filename = data.substr(1, file_len);
    cout << endl << "成功接收文件" << filename << endl;
    string a = u8"  接收文件 —— ";
    a.append(filename);
    msgRecord[++lastRecord % 50] = a;
    cout << "len11	" << len - 1 - file_len << endl;
    //cout << data.substr(1 + file_len, len - 1 - file_len) << endl;
    string data_decode64 = base64_decode(data.substr(1 + file_len, len - 1 - file_len));
    ofstream f(outPath + filename, ios::out | ios::binary);
    f << data_decode64;
    f.close();

    return 0;
}


//----------------华丽的分割线，报文处理相关的函数----------------
// 为数据增加头部
void addHeader(U8* bufSend, U8* desIP, U8 seq, U8 ack, U8 flags, U8 rnums) {
    APP_SndICI* ICI = (APP_SndICI*)bufSend;
    ICI->uDesIP[0] = desIP[0];
    ICI->uDesIP[1] = desIP[1];
    APP_Header* header = (APP_Header*)&bufSend[iciLen];
    header->Sequence_Number = seq;
    header->Acknowledgment_Number = ack;
    header->Flags = flags;
    header->Rnums = rnums;
    return;
}


//----------------华丽的分割线，流量控制相关的函数----------------
// 实现拥塞发送 能进入这个函数说明此时数据能进入缓冲区 所有进入的窗口加上头部并放入发送窗口
void  congestionControl(U8* buf, int len, U8* desIP, struct send_queue* que) // ifNo
{
    // 接收应用层发送的数据并入队列
    //cout << "处理前比特数组" << endl;
    //print_data_bit(buf, len, 1);
    //cout << "处理前字节数组" << endl;
    //print_data_byte(buf, len, 1);
    U8* abufSend;
    len += headerLen + iciLen;
    cout << "len	" << len << endl;
    while (len > MAX_SEND_DATA) { //进行切割
        abufSend = (U8*)malloc(MAX_SEND_DATA);
        addHeader(abufSend, desIP, U8(que->size % MAX_NO), lastRecvNo, U8(16), recvSensitivity - fNums);
        strncpy(&abufSend[headerLen + iciLen], buf, MAX_SEND_DATA - iciLen - headerLen);
        enSendQueue(&send_buffer, abufSend, MAX_SEND_DATA);
        len = len - MAX_SEND_DATA + iciLen + headerLen;
        buf = &buf[MAX_SEND_DATA - iciLen - headerLen];
    }
    abufSend = (U8*)malloc(len);
    if (abufSend != NULL) {
        addHeader(abufSend, desIP, U8(que->size % MAX_NO), lastRecvNo, 1, recvSensitivity - fNums);
        //cout << "abufSend[1] " << int(abufSend[1]) << endl;
        // 数据加入缓冲区
        strcpy(&abufSend[headerLen + iciLen], buf);
        enSendQueue(&send_buffer, abufSend, len);
    }

}

// 实现阻塞发送 按拥塞窗口的大小发送数据
void congestionSend(struct send_queue* que) {

    // 如果nxt和rear指针不重合 说明存在可发未发的指针 将所有可发未发的全部发送出去
    //if ((que->nxt + MAX_SEND - que->front) >= (que->rear + MAX_SEND - que->front)) //如果窗口改变导致rear在nxt之前 则停止发生
    //	return;
    // 如果nxt和rear指针不重合 说明存在可发未发的指针 将所有可发未发的全部发送出去
    if (status) {
        cout << "\n 拥塞发生 因此不进行阻塞发送 " << endl;
        cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << " ltime " << ltime << " 等待超时 超时时间  " << send_buffer.reTime[send_buffer.front] << endl << endl;
        return;
    }
    if (que->nxt != que->rear && que->nxt != que->size) {
        cout << endl << endl << "======================================================================" << endl;

        // 从nxt开始移动 如果不碰到rear或size不停止
        for (int i = que->nxt; i != que->rear && i != que->size; i = (i + 1) % MAX_SEND)
        {
            // 设置超时重传的时间
            que->reTime[i] = ltime + reportTime;
            APP_SndICI* ici = (APP_SndICI*)que->data[i];
            APP_Header* header = (APP_Header*)&(que->data[i])[iciLen];
            header->Rnums = recvSensitivity - fNums;
            // 向下层的0端口发送
            int iSndRetval = SendtoLower(que->data[i], que->len[i], 0);
            //自动打印数据
            cout << "\n'''''''''''''''''''''''''发送数据'''''''''''''''''''''''''\n\n目的IP地址为 " << ici->uDesIP[0] << "." << ici->uDesIP[1] << " 发送序号为 " << int(header->Sequence_Number) << " 长度为 " << que->len[i] << " 的报文" << endl;
            if (que->len[i] < 500) {
                print_data_bit(que->data[i], que->len[i], 1);
                //cout << "buf[0] " << buf[0] << " int buf[0] " << int(buf[0]) << " len " << len << " strlen " << strlen(buf) << endl;
                print_data_byte(que->data[i], que->len[i], 1);
            }
            else
                cout << endl << "发送数据长度太长，不显示" << endl;
            cout << endl << "~~~~~~~~~~~~~~~~~~~~~~~~发送完成~~~~~~~~~~~~~~~~~~~~~~~~" << endl << endl;
            // nxt指针移动
            que->nxt = (que->nxt + 1) % MAX_SEND;
            // 数据记录
            dataRecord(iSndRetval, 1);
            //free(que->data[i]);

            cout << "阻塞发送停止 已发送\ncwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
            addMsg(++lastRecord, 1, int(header->Flags), int(header->Sequence_Number), int(header->Acknowledgment_Number), ici->uDesIP, iSndRetval);
        }
        cout << endl << "======================================================================" << endl << endl;
    }

    else if (que->front != que->size) {
        cout << endl << endl << "======================================================================" << endl;
        cout << "\n阻塞未发送\ncwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << " ltime " << ltime << " 等待超时 超时时间  " << send_buffer.reTime[send_buffer.front] << endl << endl;
        cout << endl << "======================================================================" << endl << endl;
    }
    //else
        //cout << "\n等待进一步操作" << endl << endl;

}

// 处理拥塞发生 
void congestionHappen(struct send_queue* que, int No) {
    cout << "启动拥塞发生" << endl;
    cout << "拥塞发生前 \nque->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << " cwnd " << cwnd << endl << endl;
    status = 1; // 设置拥塞发生
    // 如果是自动发送模式 先把自动发送关闭了
    if (uAutoIP[0]) {
        save = uAutoIP[0];
        uAutoIP[0] = 0;
    }
    cwnd = max(cwnd / 2, 1); // 调整窗口大小
    ssthresh = max(cwnd, 6); // 调整慢启动门限
    No += cNums * MAX_NO; // 将No转化为正确的位置
    que->nxt = No;  // 回退N帧机制 直接从丢失帧开始全部重发
    que->rear = que->front + cwnd;  // 窗口变化
    cout << "拥塞发生后 \nque->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << " cwnd " << cwnd << " ssthresh " << ssthresh << endl << endl;
}

// 处理数据重发并重置超时重发的时间
void reSend(struct send_queue* que, int i) {
    if (++que->reNo[i] == 4) {
        cout << endl << endl << "！！！！！！！！！！！！！！！重传次数达到上限 关闭连接 请重新连接！！！！！！！！！！！！！！！" << endl << endl;
        uConnectIP[0] = 0;
        send_buffer.nxt = send_buffer.front;
        send_buffer.size = send_buffer.front; // 清空缓冲区
        send_buffer.reTime[send_buffer.front] = -2; // 清空缓冲区
        status = 0;
        msgRecord[++lastRecord % 50] = u8"   ！！！！重传达到上线 请重新连接！！！！";
        return;
    }
    que->reTime[i] = ltime + reportTime;
    APP_SndICI* ici = (APP_SndICI*)que->data[i];
    APP_Header* header = (APP_Header*)&que->data[i][iciLen];
    header->Rnums = recvSensitivity - fNums;
    int iSndRetval = SendtoLower(que->data[i], que->len[i], 0);
    //cout << "重发 zhen 为 " << i << "的报文" << endl;
    cout << "重发报文—— 发送序号为" << int(header->Sequence_Number) << " 的报文 ltime " << ltime << " 下次重传时间  " << que->reTime[i] << " 累计重传次数  " << que->reNo[i] << " que->front " << que->front << endl << endl;
    //cout << "发送序号为" << "的报文" << endl;
    //cout << "que->len[i] " << que->len[i] << endl;
    cout << "\n重发报文数据" << endl;
    print_data_byte(que->data[i], que->len[i], 1);
    print_data_bit(que->data[i], que->len[i], 1);
    cout << endl << "~~~~~~~~~~~~~~~~~~~重发报文发送完成~~~~~~~~~~~~~~~~~~~~~~" << endl << endl;
    dataRecord(iSndRetval, 1);
    addMsg(++lastRecord, 1, int(header->Flags), int(header->Sequence_Number), int(header->Acknowledgment_Number), ici->uDesIP, iSndRetval);
}

// 慢启动和拥塞避免的窗口调整
int changeWin(struct send_queue* que) {
    if (cwnd + 2 >= MAX_SEND)
    {
        cout << endl << "窗口太大了 寄！" << endl;
        cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
        return -1;
    }
    if (cwnd < ssthresh)
    {
        cout << endl << "慢启动窗口改变前" << endl;
        cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
        cwnd += 1;
        que->rear = (que->rear + 1) % MAX_SEND;
        cout << "慢启动窗口改变后" << endl;
        cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
        return 0;
    }
    else {
        if (++winacc >= cwnd) {
            cout << endl << "拥塞避免窗口改变前" << endl;
            cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
            cwnd += 1;
            winacc = 0;
            que->rear = (que->rear + 1) % MAX_SEND;
            cout << "拥塞避免窗口改变后" << endl;
            cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
            return 0;
        }
        cout << endl << "拥塞避免 窗口还没变" << " winacc " << winacc << " cwnd " << cwnd << endl << endl;
        return 1;

    }

}

// 快速重传机制 
void qResume(struct send_queue* que, int No) {
    cout << "启动快速重传" << endl;
    cout << "**********************开始快重传********************************" << endl;
    //noNums = 0;
    cout << "快重传改变前" << endl;
    cout << "cwnd " << cwnd << endl;
    cwnd = ssthresh + 3;
    que->rear = que->front + cwnd;  // 窗口变化
    cout << "快重传改变后" << endl;
    cout << "cwnd " << cwnd << " No " << No << endl;
    reSend(&send_buffer, No); // 重发该数据帧
    //iWorkMode = 10 + iWorkMode % 10; // 启动自动发送
}

// 拥塞慢恢复机制 
void lResume(struct send_queue* que, int No) {
    cout << "启动拥塞恢复" << endl;
    cout << "**********************开始拥塞恢复********************************" << endl;
    //noNums = 0;
    cout << "拥塞恢复改变前" << endl;
    cout << "cwnd " << cwnd << endl << endl;
    cwnd = 1;
    que->rear = que->front + cwnd;  // 窗口变化
    //status = 0;
    cout << "拥塞恢复改变后" << endl;
    cout << "cwnd " << cwnd << " No " << No << endl << endl;
    reSend(&send_buffer, No); // 重发该数据帧
    //iWorkMode = 10 + iWorkMode % 10; // 启动自动发送
}


//----------------华丽的分割线，报文发送相关的函数----------------
// 发送ACK
void sendACK(U8 ACK, U8* desIP) {
    U8* bufSend;
    string des;
    char a[10];
    //len = (int)strlen(kbBuf) + 1; //字符串最后有个结束符
    bufSend = (U8*)malloc(headerLen + iciLen);
    if (bufSend != NULL) {
        addHeader(bufSend, desIP, 255, ACK, 2, recvSensitivity - fNums);
        cout << "\n.........................发送ACK报文........................." << endl;
        print_data_bit(bufSend, headerLen + iciLen, 1);
        int iSndRetval = SendtoLower(bufSend, headerLen + iciLen, 0);
        dataRecord(iSndRetval, 1);
        cout << "\n发送ACK Acknowledgment_Number " << int(ACK) << endl;
        addMsg(++lastRecord, 1, 2, 255, ACK, desIP, iSndRetval);
    }


}

// 发送SYN
void sendSYN(U8 SYN, U8* desIP, struct send_queue* que) {
    U8* bufSend;
    bufSend = (U8*)malloc(headerLen + iciLen);
    if (bufSend != NULL) {
        addHeader(bufSend, desIP, SYN, lastRecvNo, 4, recvSensitivity - fNums);
        cout << "\n.........................发送SYN报文........................." << endl;
        cout << "\n发送SYN报文 Sequence_Number " << int(SYN) << endl << endl;
        enSendQueue(&send_buffer, bufSend, headerLen + iciLen);
        congestionSend(&send_buffer);
    }
}

// 发送SYN_ACK
void sendSYN_ACK(U8 ACK, U8* desIP, struct send_queue* que) {
    U8* bufSend;
    bufSend = (U8*)malloc(headerLen + iciLen);
    //for (int i = send_buffer.nxt; i != send_buffer.size; i = (i + 1) % MAX_SEND) {
    //	APP_Header* header = (APP_Header*)
    //	if(send_buffer.data)
    //}
    U8 No = U8(send_buffer.size % MAX_NO);
    if (bufSend != NULL) {
        addHeader(bufSend, desIP, No, ACK, 6, recvSensitivity - fNums);
        cout << "\n.........................发送SYN_ACK报文........................." << endl;
        cout << "\n发送SYN_ACK报文 Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
        enSendQueue(&send_buffer, bufSend, headerLen + iciLen);
        congestionSend(&send_buffer);
    }
}

// 发送ACK_PUSH
void sendACK_DATA(U8 ACK, U8* desIP, struct send_queue* que) {
    int i = que->nxt;
    //for (i = send_buffer.nxt; i != send_buffer.size; i = (i + 1) % MAX_SEND) {
    //    APP_Header* header = (APP_Header*)&send_buffer.data[i][iciLen];
    //    if (header->Flags == 1)
    //        break;
    //}
    // 向下层的0端口发送
    U8* bufSend = que->data[i];
    APP_SndICI* ici = (APP_SndICI*)bufSend;
    ici->uDesIP[0] = desIP[0];
    ici->uDesIP[1] = desIP[1];
    APP_Header* header = (APP_Header*)&bufSend[iciLen];
    header->Acknowledgment_Number = ACK;
    header->Flags = 3;
    U8 No = header->Sequence_Number;
    cout << "\n.........................发送ACK_PUSH报文........................." << endl;
    cout << "\n发送ACK_PUSH报文 Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
    congestionSend(&send_buffer);
}

// 发送ACK_MID
void sendACK_MID(U8 ACK, U8* desIP, struct send_queue* que) {
    int i = que->nxt;
    //for (i = send_buffer.nxt; i != send_buffer.size; i = (i + 1) % MAX_SEND) {
    //    APP_Header* header = (APP_Header*)&send_buffer.data[i][iciLen];
    //    if (header->Flags == 1)
    //        break;
    //}
    // 向下层的0端口发送
    U8* bufSend = que->data[i];
    APP_SndICI* ici = (APP_SndICI*)bufSend;
    ici->uDesIP[0] = desIP[0];
    ici->uDesIP[1] = desIP[1];
    APP_Header* header = (APP_Header*)&bufSend[iciLen];
    header->Acknowledgment_Number = ACK;
    header->Flags = 17;
    U8 No = header->Sequence_Number;
    cout << "\n.........................发送ACK_MID报文........................." << endl;
    cout << "\n发送ACK_MID报文 Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
    congestionSend(&send_buffer);
}

// 发送FIN
void sendFIN(struct send_queue* que, U8* desIP) {
    U8* bufSend;
    //len = (int)strlen(kbBuf) + 1; //字符串最后有个结束符
    bufSend = (U8*)malloc(headerLen + iciLen);
    U8 No = U8(que->size % MAX_NO);
    if (bufSend != NULL) {
        addHeader(bufSend, desIP, No, lastRecvNo, 8, recvSensitivity - fNums);
        cout << "\n.........................发送FIN报文........................." << endl;
        cout << "\n发送FIN报文 Sequence_Number " << int(No) << endl << endl;
        enSendQueue(&send_buffer, bufSend, headerLen + iciLen);
        congestionSend(&send_buffer);
    }
}

// 发送FIN_ACK
void sendFIN_ACK(U8 ACK, U8* desIP, struct send_queue* que) {
    endACK = que->front;
    U8* bufSend;
    bufSend = (U8*)malloc(headerLen + iciLen);
    U8 No = U8(send_buffer.size % MAX_NO);
    if (bufSend != NULL) {
        addHeader(bufSend, desIP, No, ACK, 10, recvSensitivity - fNums);
        cout << "\n.........................发送FIN_ACK报文........................." << endl;
        cout << "\n发送FIN_ACK报文 Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
        enSendQueue(&send_buffer, bufSend, headerLen + iciLen);
        congestionSend(&send_buffer);
    }
}




//----------------华丽的分割线，初始化相关的函数----------------
// 进入系统的初始化 包括各种数据从ne.txt的读取及预定义
void InitFunction(CCfgFileParms& cfgParms)
{
    int i;
    int retval;
    uConnectIP[0] = 0;
    uConnectIP[1] = 0; // 初始化
    uAutoIP[0] = 0;
    uAutoIP[1] = 0; // 初始化
    mid_len = 0;
    readPath = "./send/";
    retval = cfgParms.getValueInt(autoSendTime, (char*)"autoSendTime");
    if (retval == -1 || autoSendTime == 0) {
        autoSendTime = DEFAULT_AUTO_SEND_TIME;
    }
    retval = cfgParms.getValueInt(autoSendSize, (char*)"autoSendSize");
    if (retval == -1 || autoSendSize == 0) {
        autoSendSize = DEFAULT_AUTO_SEND_SIZE;
    }
    retval = cfgParms.getValueInt(recvSensitivity, (char*)"recvSensitivity");
    if (retval == -1 || autoSendSize == 0) {
        autoSendSize = DEFAULT_RECEIVE_SENSITIVITY;
    }
    autoSendBuf = (char*)malloc(MAX_BUFFER_SIZE);
    sendBuf = (char*)malloc(MAX_BUFFER_SIZE);
    strIP = cfgParms.getValueStr("IP"); // IP地址读取
    cout << "IP地址 —— " << strIP << endl;
    outPath = "output-" + strIP + "/";
    if (autoSendBuf == NULL || sendBuf == NULL) {
        cout << "内存不够" << endl;
        //这个，计算机也太，退出吧
        exit(0);
    }
    reportTime = RESEND_TIME;
    initqueue(&send_buffer, &recv_buffer);
}

// 退出系统的处理 将各种数据释放
void EndFunction()
{
    if (autoSendBuf != NULL)
        free(autoSendBuf);
    if (sendBuf != NULL)
        free(sendBuf);
}


//----------------华丽的分割线，重要函数定时器！！！！----------------
// ！！！！！定时器 用于定时系统的处理！！！！！！
void TimeOut()
{

    int iSndRetval;
    ltime++;
    if (_kbhit()) {
        //键盘有动作，进入菜单模式
        menu();
    }
    if (ltime % (10 * autoSendTime) == 0)
        fNums = max(0, fNums - 3); // 模拟处理
     //未发生拥塞时才进行检测是否超时 发生拥塞时 等待第一帧的重发判断连接的重建
    if (status == 0 && send_buffer.reTime[send_buffer.front] <= ltime && send_buffer.reTime[send_buffer.front] >= 0 && send_buffer.front != send_buffer.nxt)
    {
        cout << "超时 拥塞发生\nque->front " << send_buffer.front << " que->nxt " << send_buffer.nxt << " que->rear " << send_buffer.rear << " que->size " << send_buffer.size << endl;
        cout << "uConnectIP " << uConnectIP << " status " << status << " send_buffer.retime[send_buffer.front] " << send_buffer.reTime[send_buffer.front] << endl;
        cout << "ltime " << ltime << endl;
        // 拥塞发生
        congestionHappen(&send_buffer, send_buffer.front + 1);
        if (noNums > 4) {
            qResume(&send_buffer, int(lastRecvACK + 1) + cNums * MAX_NO);
        }
        else {
            lResume(&send_buffer, int(lastRecvACK + 1) + cNums * MAX_NO);
        }
    }
    else if (status == 1 && send_buffer.reTime[send_buffer.front] <= ltime && send_buffer.reTime[send_buffer.front] >= 0)
    {
        cout << "拥塞发生下超时 开始拥塞恢复" << endl;
        lResume(&send_buffer, int(lastRecvACK + 1));

    }
    if (uAutoIP[0]) { // 如果是自动发送模式
        if (!autoTime) {
            cout << endl << "自动发送给" << uAutoIP[0] << "." << uAutoIP[1] << "的数据完成" << endl;
            sendFIN(&send_buffer, uAutoIP);
            autoTime = 0;
            uAutoIP[0] = 0;
            uAutoIP[1] = 0;
        }
        else {
            int len;
            string data;
            data = data + U8(1) + "i love duan sir";
            if (ltime % autoSendTime == 0) {
                autoTime--;
                //cout << "开始自动发送" << endl;
                //定时发送前先判断缓冲区是否满 如果满中止重发 		此处先不考虑MSS 不实现分组技术 也不考虑定时重新开始自动发送
                if (isfullque(&send_buffer)) {
                    cout << "||||||||||||||||||||||||||||缓冲区已经满咯 寄！|||||||||||||||||||||||||||||||||||||" << endl;
                }
                //定时发送, 每间隔autoSendTime * DEFAULT_TIMER_INTERVAL ms 发送一次
                //if (printCount % autoSendTime == 0) {
                else {
                    strcpy(autoSendBuf, data.c_str());
                    len = data.size(); //每次发送数量
                    congestionControl(autoSendBuf, len, uAutoIP, &send_buffer);
                }
            }
        }

    }
    if (uConnectIP[0]) { // 如果与设备号连接

        //cout << "ltime " << ltime << endl;
        if (endtime != -1 && ltime >= endtime) { // 判断是否需要关闭连接
            endtime = -1;
            uConnectIP[0] = 0; //关闭系统
            cout << "\n-------------------------关闭连接-------------------------" << endl << endl;
            msgRecord[++lastRecord % 50] = u8"   -----------------------关闭连接-----------------------";
            return;
        }

        if (ltime % (autoSendTime + 10) == 0)
            // 拥塞发送 
            congestionSend(&send_buffer);
    }


}


//------------华丽的分割线，以下是数据的收发--------------------------------------------
// 从上层接收数据 这里应用层不会从上层接收数据的啦
void RecvfromUpper(U8* buf, int len) {
    return;
}

// ！！！！！从下层接收数据 用于各种报文的判断与处理 ！！！！！
void RecvfromLower(U8* buf, int len, int ifNo)
{
    cout << endl << endl << "======================================================================" << endl;

    int retval;
    U8 lastbuffer;
    U8 flag;
    U8 No;
    U8 ACK;
    U8 Rnums;
    U8* originbuf = NULL;
    U8* sbuf = NULL;
    if (fNums >= recvSensitivity)
    {
        cout << endl << "*************************报文丢失*************************" << endl << endl;
        return; // 伪造报文丢失现象

    }
    APP_RcvICI* ici = (APP_RcvICI*)buf;
    APP_Header* header = (APP_Header*)&buf[iciLen];
    flag = header->Flags;
    No = header->Sequence_Number;
    ACK = header->Acknowledgment_Number;
    Rnums = header->Rnums;
    cout << "\n////////////////////////接收到数据////////////////////////\n\n源IP地址—— " << ici->uSrcIP[0] << "." << ici->uSrcIP[1] << " 标志位——flag " << int(flag) << " 长度——len " << len << " 剩余能力——Rnums " << int(Rnums) << endl;
    if (flag & 1 || flag & 16) // 1代表PUSH 16代表MID 都是数据报文
        fNums += 2;
    else
        fNums++;
    cout << "\n接收帧速率 " << fNums << endl;
    cout << "\n接收到的数据内容" << endl;
    if (len < 500) {
        print_data_bit(buf, len, 1);
        //cout << "buf[0] " << buf[0] << " int buf[0] " << int(buf[0]) << " len " << len << " strlen " << strlen(buf) << endl;
        print_data_byte(buf, len, 1);
    }
    else
        cout << endl << "接收数据长度太长，不显示" << endl;

    ////判断字节数据第一位 判断是数据报文还是ACK报文 0为ACK报文 1为数据报文
    addMsg(++lastRecord, 0, int(flag), int(No), int(ACK), ici->uSrcIP, len);
    switch (flag) {
    case 1: // PUSH报文
        if (No != (lastRecvNo + 1) % MAX_NO) {
            cout << "*************************PUSH报文序号出错*************************" << endl;
            cout << "\n收到数据报文不符合要求,序号为 " << int(No) << " 要求为" << int(lastRecvNo + 1) % MAX_NO << endl;
            sendACK(lastRecvNo, uConnectIP);
            cout << "++++++++++++++++++++++++放入接收窗口++++++++++++++++++++++++" << endl;
            sbuf = (U8*)malloc(len);
            strncpy(sbuf, buf, len);
            enRecvQueue(&recv_buffer, sbuf, int(No), len);
        }
        else {
            cout << "\n------------------------PUSH报文------------------------" << endl << endl;
            originbuf = &buf[headerLen + iciLen];
            cout << "\n收到正确的数据报文,序号为" << int(No) << endl;
            if (lastUpNo != lastRecvNo) { // 如果不等于上一次上发的窗口
                cout << "++++++++++++++++++++++++分割报文最后一个，放入缓冲区++++++++++++++++++++++++" << endl;
                addMidBuffer(mid_buffer, originbuf, int(flag), len - iciLen - headerLen);
                lastRecvNo = No;
                lastUpNo = No;
            }
            else {
                lastRecvNo = No;
                lastUpNo = No;
                if (originbuf[0] != 1) {
                    if (len < 500) {
                        cout << "\n去掉头部后的数据" << endl;
                        print_data_byte(originbuf, len - headerLen - iciLen, 1);
                    }
                    writeData(originbuf, len - headerLen - iciLen);
                }
                else if (len < 500)
                {
                    cout << "\n去掉头部后的数据" << endl;
                    print_data_byte(&originbuf[1], len - headerLen - iciLen - 1, 1);
                }
                checkRecvQueue(&recv_buffer);
            }
            sendACK(lastRecvNo, uConnectIP);
            //base64toImg(originbuf);
            //WriteData("output.txt", originbuf, len - 2);
        }
        break;
    case 2:
        //cout << "\n收到ACK，序号为" << int(No) << endl;
        // 判断得到的是ACK报文
        // 如果ACK与上一个ACK序号一样 则重复
        if (ACK == lastRecvACK) {
            cout << "*************************ACK报文序号重复*************************" << endl;
            cout << "\n收到和上次一样的ACK，序号为" << int(ACK) << " 出现次数 " << ++noNums << endl;
            if (status == 0 && noNums == 2)
                // 拥塞发生
                congestionHappen(&send_buffer, lastRecvACK + 2);
            // 触发快恢复
            if (noNums == 4)
                qResume(&send_buffer, int(lastRecvACK + 1));
        }
        else {
            cout << "\n------------------------ACK报文------------------------" << endl;
            // 说明数据接收成功
            if (ACK == lastRecvACK + 1)
                cout << "\n收到正确的ACK报文,序号为" << int(ACK) << endl;
            else if ((ACK + MAX_SEND - lastRecvACK) % MAX_SEND < 10) // 相差序号为10下 可以默认收取
                cout << "\n没收到预计ACK " << lastRecvACK + 1 << " 但收到ACK序号为 " << int(ACK) << "认为前一报文也收到\n" << endl;
            else
            {
                cout << "\n收到错误的ACK报文,序号为" << int(ACK) << endl;
                return;
            }
            if (status == 1) { //拥塞状态收到正确ACK重新启动自动发送
                //iWorkMode = 10 + iWorkMode % 10; // 启动自动发送
                status = 0; // 解除拥塞发生
                if (uAutoIP[1])
                    uAutoIP[0] = save; // 如果在自动发送模式 重新启动自动发送
            }
            int last = int(lastRecvACK);
            int now = int(ACK);
            last += cNums * MAX_NO;
            if (now - int(lastRecvACK) < -120) {
                cNums++;
            }
            now += cNums * MAX_NO;
            if (cNums >= MAX_SEND / MAX_NO)
                cNums = 0;
            //cout << "now " << now << " last " << last << " send_buffer.front " << send_buffer.front << endl;
            //send_buffer.nxt = min(now + 1, send_buffer.rear);
            for (int i = now; i > last; i--) {
                send_buffer.reTime[i % MAX_SEND] = -2; //取消超时重传机制
            }
            //if (status == 1 && send_buffer.front == send_buffer.nxt) {
            //	send_buffer.nxt = min(now + 1, send_buffer.rear);
            //}
            if (send_buffer.reTime[send_buffer.front] == -2)
                for (int i = send_buffer.front; send_buffer.reTime[i] == -2 && i != send_buffer.nxt; i = (i + 1) % MAX_SEND)
                {
                    //cout << ""
                    deSendQueue(&send_buffer); // 如果确认的是队列的front帧 则出队
                    //if (!status) // 在拥塞发生状态下要等待快恢复 否则直接改变窗口
                    changeWin(&send_buffer);
                }
            if (ACK == endACK) {
                endACK = 255;
                uConnectIP[0] = 0; //关闭系统
                cout << "\n-------------------------关闭连接-------------------------" << endl << endl;
                msgRecord[++lastRecord % 50] = u8"   -----------------------关闭连接-----------------------";
                return;
            }
            lastRecvACK = ACK;
        }
        break;
    case 3:
        if (ACK == send_buffer.front % MAX_NO) {

            deSendQueue(&send_buffer); // 如果确认的是队列的front帧 则出队
            send_buffer.reTime[send_buffer.front % MAX_SEND] = -2; //取消超时重传机制
            changeWin(&send_buffer);
            cout << "\n------------------------ACK_PUSH报文------------------------" << endl << "Acknowledgment_Number " << int(ACK) << endl << endl;
            uConnectIP[0] = ici->uSrcIP[0];
            uConnectIP[1] = ici->uSrcIP[1];
            cout << "++++++++++++++++++++++++建立连接++++++++++++++++++++++++" << endl << "连接的IP——" << uConnectIP[0] << "." << uConnectIP[1] << endl << endl;;
            msgRecord[++lastRecord % 50] = u8"   -----------------------建立连接-----------------------";
            lastRecvACK = ACK;
            originbuf = &buf[headerLen + iciLen];
            cout << "\n收到正确的数据报文,序号为" << int(No) << endl;
            lastRecvNo = No;
            lastUpNo = No;
            if (originbuf[0] != 1) {
                if (len < 500) {
                    cout << "\n去掉头部后的数据" << endl;
                    print_data_byte(originbuf, len - headerLen - iciLen, 1);
                }

                writeData(originbuf, len - headerLen - iciLen);
            }
            else if (len < 500)
            {
                cout << "\n去掉头部后的数据" << endl;
                print_data_byte(&originbuf[1], len - headerLen - iciLen - 1, 1);

            }
            lastRecvNo = No;
            lastUpNo = No;
            //WriteData("output.txt", originbuf, len - 3);
            //base64toImg(originbuf);

            sendACK(No, uConnectIP);
        }
        else
            cout << "*************************ACK_PUSH报文ACK出错*************************" << endl;
        break;
    case 4:
        if (uConnectIP[0] == 0) {
            cout << "\n------------------------SYN报文------------------------" << endl << "Sequence_Number " << int(No) << endl << endl;
            lastRecvNo = No;
            lastUpNo = No;
            sendSYN_ACK(No, ici->uSrcIP, &send_buffer);
        }
        else {
            cout << "\n\n目前与IP " << uConnectIP << " 建立连接，无法接收其他IP的SYN报文 请先断开连接" << endl << endl;
        }
        break;

    case 6:
        if (uConnectIP[0] == 0) {
            cout << "\n------------------------SYN_ACK报文------------------------" << endl << "Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
            if (ACK == send_buffer.front % MAX_NO) {
                lastRecvNo = No;
                lastUpNo = No;
                send_buffer.reTime[send_buffer.front % MAX_SEND] = -2; //取消超时重传机制
                lastRecvACK = ACK;
                deSendQueue(&send_buffer); // 如果确认的是队列的front帧 则出队
                changeWin(&send_buffer);
                uConnectIP[0] = ici->uSrcIP[0];
                uConnectIP[1] = ici->uSrcIP[1];
                cout << "++++++++++++++++++++++++建立连接++++++++++++++++++++++++" << endl << "连接的IP——" << uConnectIP[0] << "." << uConnectIP[1] << endl << endl;
                msgRecord[++lastRecord % 50] = u8"   -----------------------建立连接-----------------------";
                header = (APP_Header*)&send_buffer.data[send_buffer.nxt][iciLen];
                cout << (int)header->Flags << endl;
                if (header->Flags == 1)
                    sendACK_DATA(No, uConnectIP, &send_buffer);
                else if (header->Flags == 16)
                    sendACK_MID(No, uConnectIP, &send_buffer);
            }
            else
                cout << "*************************SYN_ACK报文ACK出错*************************" << endl;
        }
        else
            cout << "*************************收到不期望的SYN_ACK报文*************************" << endl;
        break;
    case 8:
        if (uConnectIP[0] == 0) {
            cout << "\n------------------------FIN报文------------------------" << endl << "Sequence_Number " << int(No) << endl;
            sendACK(No, uConnectIP);
            lastRecvNo = No;
            sendFIN_ACK(No, uConnectIP, &send_buffer);
        }
        break;
    case 10:
        cout << "\n------------------------FIN_ACK报文------------------------" << endl << "Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
        if (int(ACK) == (send_buffer.front + 128 - 1) % MAX_NO) {
            endtime = ltime + 50;
            lastRecvNo = No;
            //cout << endtime << endl;
            sendACK(No, uConnectIP);
        }
        break;
    case 16:
        cout << "\n------------------------MID报文------------------------" << endl << "Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
        if (No != (lastRecvNo + 1) % MAX_NO) {
            cout << "*************************MID报文序号出错*************************" << endl;
            cout << "++++++++++++++++++++++++放入接收窗口++++++++++++++++++++++++" << endl;
            sbuf = (U8*)malloc(len);
            strncpy(sbuf, buf, len);
            lastRecvNo = No;
            enRecvQueue(&recv_buffer, sbuf, int(No), len);
        }
        else {
            cout << "++++++++++++++++++++++++放入上发缓冲区++++++++++++++++++++++++" << endl;
            addMidBuffer(mid_buffer, &buf[iciLen + headerLen], int(flag), len - iciLen - headerLen);
            lastRecvNo = No;
            if (!uConnectIP[0])
                sendACK(No, ici->uSrcIP);
            else
                sendACK(No, uConnectIP);
        }
        break;
    case 17:
        if (ACK == send_buffer.front % MAX_NO) {
            deSendQueue(&send_buffer); // 如果确认的是队列的front帧 则出队
            send_buffer.reTime[send_buffer.front % MAX_SEND] = -2; //取消超时重传机制
            changeWin(&send_buffer);
            cout << "\n------------------------ACK_MID报文------------------------" << endl << "Acknowledgment_Number " << int(ACK) << endl << endl;
            uConnectIP[0] = ici->uSrcIP[0];
            uConnectIP[1] = ici->uSrcIP[1];
            cout << "++++++++++++++++++++++++建立连接++++++++++++++++++++++++" << endl << "连接的IP——" << uConnectIP[0] << "." << uConnectIP[1] << endl << endl;;
            msgRecord[++lastRecord % 50] = u8"   -----------------------建立连接-----------------------";
            lastRecvACK = ACK;
            originbuf = &buf[headerLen + iciLen];
            cout << "++++++++++++++++++++++++放入上发缓冲区++++++++++++++++++++++++" << endl;
            addMidBuffer(mid_buffer, &buf[iciLen + headerLen], int(flag), len - iciLen - headerLen);
            lastRecvNo = No;
            //WriteData("output.txt", originbuf, len - 3);
            //base64toImg(originbuf);

            sendACK(No, uConnectIP);
        }
        else
            cout << "*************************ACK_MID报文ACK出错*************************" << endl;
        break;
    default:
        cout << "\n出错了 寄！" << endl;
        cout << "\n出错数据" << endl;
        print_data_bit(buf, len, 1);
        break;
    }

    dataRecord(len, 0);
    cout << endl << "======================================================================" << endl << endl;

}


//------------华丽的分割线，功能性的函数--------------------------------------------
// 发送接收数据的记录  flag —— 0为接收数据 1为发送数据
void dataRecord(int iSndRetval, int flag) {
    if (flag)
        if (iSndRetval > 0) {
            iSndTotalCount++;
            iSndTotal += iSndRetval * 8;
        }
        else
            iSndErrorCount++;
    else
        if (iSndRetval > 0) {
            iRcvTotalCount++;
            iRcvTotal += iSndRetval * 8;
        }
}

// 打印当前发送接收数据的信息 包括 发送接收数据的次数、位数，错误数据的次数
void printStatistics()
{
    switch (spin) {
    case 1:
        printf("\r-");
        break;
    case 2:
        printf("\r\\");
        break;
    case 3:
        printf("\r|");
        break;
    case 4:
        printf("\r/");
        spin = 0;
        break;
    }
    cout << "共发送 " << iSndTotal << " 位," << iSndTotalCount << " 次," << "发生 " << iSndErrorCount << " 次错误;";
    cout << " 共接收 " << iRcvTotal << " 位," << iRcvTotalCount << " 次," << "当前时间" << ltime << endl;
    spin++;

}

// 用于处理读取的文件数据或文本数据，并发送
void  SendData(U8* data, string des, int len, int flag) {
    char desIP[2];

    if (isfullque(&send_buffer)) {
        cout << endl << "缓冲区已满，暂时无法发送";
        return;
    }
    setIP(desIP, des.c_str());
    if (uConnectIP[0] != 0 && strncmp(desIP, uConnectIP, 2))
    {
        string a;
        a = a + u8" 目前与IP " + uConnectIP[0] + "." + uConnectIP[1] + u8" 建立连接，无法向其他IP发送  请先断开连接";
        //a = a + " Link IP " + uConnectIP[0] + "." + uConnectIP[1];
        msgRecord[++lastRecord % 50] = a;
        cout << endl << endl << a << endl << endl;
        return;
    }
    if (data != NULL) {
        U8* bufSend;
        if (flag) {
            bufSend = (U8*)malloc(++len);
            bufSend[0] = 1;
            strcpy(&bufSend[1], data);
        }
        else {
            bufSend = (U8*)malloc(len);
            strcpy(bufSend, data);
        }
        if (len > 0) {
            if (uConnectIP[0] == 0)
                sendSYN(send_buffer.size % MAX_NO, desIP, &send_buffer);
            congestionControl(bufSend, len, desIP, &send_buffer);
        }
        else {
            cout << endl << "无可发送的数据" << endl << endl;
        }
    }


}

// 断开连接 
void breakConnection() {
    if (uConnectIP[0] == 0)
    {
        msgRecord[++lastRecord % 50] = u8"  断开连接失败，目前没有建立连接";
        cout << endl << "目前没有建立连接" << endl << endl;
    }
    else
        sendFIN(&send_buffer, uConnectIP);
}

// 菜单函数 用于输入实现功能
void menu()
{
    int selection;
    int iSndRetval;
    string file;
    string kbBuf;
    string des;
    U8* bufSend;
    int rlen;
    U8 no;
    //发送|打印：[发送控制（0，等待键盘输入；1，自动）][打印控制（0，仅定期打印统计信息；1，按bit流打印数据，2按字节流打印数据]
    cout << endl << endl << "设备号:" << strDevID << ",    层次:" << strLayer << ",    实体号:" << strEntity;
    cout << endl << "1-启动自动发送;" << endl << "2-停止自动发送; " << endl << "3-发送文件; ";
    cout << endl << "4-发送单句文本; " << endl << "5-关闭连接;" << endl << "6-打印日志;" << endl << "7-发送指定序号报文(接收窗口调试);";
    cout << endl << "0-取消" << endl << "请输入数字选择命令：";
    cin >> selection;
    getchar();
    switch (selection) {
    case 0:
        break;
    case 1:
        cout << "请输入要自动发送的IP：";
        cin >> des;
        cout << "请输入要自动发送的次数：";
        cin >> autoTime;
        if (!deleteMark(des, ".")) {
            cout << endl << "！！！！请输入正确的IP地址！！！！" << endl;
            break;
        }
        setIP(uAutoIP, des.c_str());
        cout << uAutoIP << endl;
        sendSYN(send_buffer.size % MAX_NO, uAutoIP, &send_buffer);
        break;
    case 2:
        uAutoIP[0] = 0;
        uAutoIP[1] = 0;
        sendFIN(&send_buffer, uConnectIP);
        break;
    case 3:
        if (isfullque(&send_buffer)) {
            cout << endl << "缓冲区已满，暂时无法发送";
            break;
        }
        cout << "请输入要发送的文件：";
        cin >> file;
        cout << "请输入要发送的IP：";
        cin >> des;
        if (!deleteMark(des, ".")) {
            cout << endl << "！！！！请输入正确的IP地址！！！！" << endl;
            break;
        }
        rlen = readData(file, sendBuf);
        if (rlen == -1) {
            cout << endl << "！！！！请正确输入文件名称！！！！" << endl;
            break;
        }
        bufSend = (U8*)malloc(rlen);
        strcpy(bufSend, sendBuf);
        SendData(bufSend, des, rlen, 0);
        break;
    case 4:
        cout << "请输入要发送的文本：";
        getline(cin, kbBuf);
        cout << "请输入要发送的IP：";
        cin >> des;
        if (!deleteMark(des, ".")) {
            cout << endl << "！！！！请输入正确的IP地址！！！！" << endl;
            break;
        }
        cout << kbBuf << endl;
        bufSend = (U8*)malloc(kbBuf.size());
        strcpy(bufSend, kbBuf.data());
        SendData(bufSend, des, kbBuf.size(), 1);
        break;
    case 5:
        breakConnection();
        break;
    case 6:
        printStatistics();
        break;
    case 7:
        cout << "请输入要发送的IP：";
        cin >> des;
        if (!deleteMark(des, ".")) {
            cout << endl << "！！！！请输入正确的IP地址！！！！" << endl;
            break;
        }
        char desIP[2];
        desIP[0] = des.c_str()[0];
        desIP[1] = des.c_str()[1];
        cout << "请输入要发送的序号：";
        cin >> no;

        bufSend = (U8*)malloc(iciLen + headerLen);
        addHeader(bufSend, desIP, U8(no) - '0', lastRecvNo, 1, recvSensitivity - fNums);
        print_data_byte(bufSend, iciLen + headerLen, 1);
        SendtoLower(bufSend, iciLen + headerLen, 0);
        break;
    default:
        cout << "请正确输入" << endl << endl;
        break;
    }

}

// 用于向日志加入信息 mode —— 0表示接收数据 1表示发送 数据
void addMsg(int index, int mode, int type, int seq, int ack, U8* ip, int len) {
    string msg;
    if (mode == 0)
        msg = (u8"  [Recv]");
    else if (mode == 1)
        msg = (u8"  [Send]");
    switch (type)
    {
    case 1:
        msg.append(u8" [PUSH] NO -- "); msg.append(to_string(seq));
        break;
    case 2:
        msg.append(u8" [ACK] ACK -- "); msg.append(to_string(ack));
        break;
    case 3:
        msg.append(u8" [ACK_PUSH] NO -- "); msg.append(to_string(seq));
        msg.append(u8" ACK -- "); msg.append(to_string(ack));
        break;
    case 4:
        msg.append(u8" [SYN] NO -- "); msg.append(to_string(seq));
        break;
    case 6:
        msg.append(u8" [SYN_ACK] NO -- "); msg.append(to_string(seq));
        msg.append(u8" ACK — "); msg.append(to_string(ack));
        break;
    case 8:
        msg.append(u8" [FIN] NO -- "); msg.append(to_string(seq));
        break;
    case 10:
        msg.append(u8" [FIN_ACK] NO -- "); msg.append(to_string(seq));
        msg.append(u8" ACK -- "); msg.append(to_string(ack));
        break;
    case 16:
        msg.append(u8" [MID] NO -- "); msg.append(to_string(seq));
        break;
    case 17:
        msg.append(u8" [ACK_MID] NO -- "); msg.append(to_string(seq));
        msg.append(u8" ACK -- "); msg.append(to_string(ack));
        break;
    default:
        break;
    }

    if (mode == 0)
    {
        msg.append(u8" From IP -- "); msg.append(1, ip[0]); msg.append(1, '.'); msg.append(1, ip[1]);
    }
    else  if (mode == 1)
    {
        msg.append(u8"  To IP -- "); msg.append(1, ip[0]); msg.append(1, '.'); msg.append(1, ip[1]);
    }
    msg.append(u8"  Bytes -- "); msg.append(to_string(len));

    msgRecord[index % 50] = msg;
    return;


}

// 用于窗口读取当前日志的信息 不是这里会用的
string getStr(int index) {
    if (lastRecord < 50)
        return msgRecord[index];
    else
        return msgRecord[(lastRecord % 49 + index) % 50];

}

// 删除string中指定的符号 这里用于去掉IP地址中的.
bool deleteMark(string& s, const string& mark)
{
    if (s.at(1) != '.' || s.size() != 3)
        return false;
    size_t nSize = mark.size();
    while (1)
    {
        size_t pos = s.find(mark);
        if (pos == string::npos)
        {
            return true;
        }

        s.erase(pos, nSize);
    }
}

// 用于窗口读取当前连接的IP 不是这里会用的
string getIP() {
    if (uConnectIP[0] == 0)
        //return " Now Links:  None";
        return u8" 当前连接设备IP地址:  无";
    string a;
    a = a + u8" 当前连接设备IP地址:  " + uConnectIP[0] + '.' + uConnectIP[1];
    //a = a + " Now Links:  " + uConnectIP[0] + '.' + uConnectIP[1];
    return a;
}

// 用于窗口选择要发送的文件并发送
void  selectFile(U8* file, string des) {
    U8* bufSend;
    int rlen = readData(file, sendBuf);
    if (rlen == -1) {
        msgRecord[++lastRecord % 50] = u8"   ！！！！请输入正确的文件名称！！！！";
        //msgRecord[++lastRecord % 50] = "  !!!!Please input correct file name!!!! ";
        return;
    }
    if (!deleteMark(des, ".")) {
        msgRecord[++lastRecord % 50] = u8"   ！！！！请输入正确的IP地址！！！！!";
        //msgRecord[++lastRecord % 50] = u8"  !!!!Please input correct IP!!!! ";
        return;
    }
    //string a = u8"  Send Data -- ";
    string a = u8"  发送文件 -- ";
    a.append(file);
    msgRecord[++lastRecord % 50] = a;
    bufSend = (U8*)malloc(rlen);
    strcpy(bufSend, sendBuf);
    SendData(bufSend, des, rlen, 0);
}

// 用于设置IP地址
void setIP(U8* nIP, const U8* sIP) {
    nIP[0] = sIP[0];
    nIP[1] = sIP[1];
}
