//apptester的功能文件
#include <iostream>
#include <conio.h>
#include "winsock.h"
#include "stdio.h"
#include "CfgFileParms.h"
#include "function.h"
#pragma warning(disable:4996)
using namespace std;

#define MAX_QUE 1024
#define MAX_NO 128
U8* autoSendBuf;  //用来组织发送数据的缓存，大小为MAX_BUFFER_SIZE,可以在这个基础上扩充设计，形成适合的结构，例程中没有使用，只是提醒一下
U8* SendBuf;
int ltime = 0; //全局时钟用于处理超时重传
int endtime = -1;
int spin = 1;  //打印动态信息控制
int z = 1;
//------华丽的分割线，一些统计用的全局变量------------
int iSndTotal = 0;  //发送数据总量
int iSndTotalCount = 0; //发送数据总次数
int iSndErrorCount = 0;  //发送错误次数
int iRcvTotal = 0;     //接收数据总量
int iRcvTotalCount = 0; //转发数据总次数
int iRcvUnknownCount = 0;  //收到不明来源数据总次数
int cwnd = 1; //拥塞窗口大小 
int ssthresh = 8; //慢启动门限
int reportTime = 500; // 超时重传的时间
int ACKlen = 4; // ACK数据长度为4
int SYN_ACKlen = 4; //SYN_ACK长度为4
int SYNlen = 4; // SYN长度为4
int FINlen = 4; // FIN长度为4
int FIN_ACKlen = 4; // SYN_ACK长度为4
int winacc = 0; // 记录收到的帧数目，用于拥塞避免
U8 lastNo = 255; // 前一个收到报文的序号
U8 lastACK = 255; //前一个接收ACK序号
U8 lastSeq = 0;
int noNums = 0; //开始计数同一序号ACK收到的次数
int status = 0; //判断拥塞是否发生 如果发生为1 未发生为0 
int isstart = 0; //用于判断系统是否开始运作
int cnums = 0; //用于判断序号经过几轮循环
int fnums = 0; // 单位时间内收到的帧数目
int headerLen = 4; // 头部长度
int isautoend = 0; // 判断是否自动停止
U8 endACK = 255;


// 发送窗口
struct send_queue { // 四指针缓冲区设计 窗口必定会比缓冲区小
    int front; // 指向已发送但是未收到确认的第一个字节序列号
    int nxt;  //指向可发送未发送的第一个字节序列号
    int rear; //指向不可发送未发送的第一个字节序列号 窗口
    int size; // 指向缓冲区的最后一个数据		
    U8* data[MAX_QUE]; // 缓冲区数据集合
    int len[MAX_QUE]; // 缓冲区每个数据的长度
    int retime[MAX_QUE];  // 缓冲区数据的超时重发记录
};
// APP层报文头部
struct APP_Header {
    char Sequence_Number;  // 发送的报文序列号 
    char Acknowledgment_Number; // 确认收到的报文序列号 只有标志位ACK为1时有效
    char Flags; // 标志位 
    char Rnums; // 剩余接收帧能力 
};
// 初始化结构体变量
struct send_queue my_buffer;
// 从文件内读取数据
int ReadData(const U8* file, U8* data) {
    ifstream ifs(file, ios::in);
    if (!ifs) {
        cerr << "Can't open the file." << endl;
        return -1;
    }
    string txt_data, line;
    while (getline(ifs, line))
    {
        txt_data.append(line.append("\\n"));
    }
    txt_data.pop_back();
    txt_data.pop_back();
    int len = txt_data.length();
    strcpy(data, txt_data.c_str());
    return len;
}
// 向文件写入数据
int WriteData(const U8* file, const U8* data, int len) {
    ofstream outfile(file, ios::ate); //ios::ate可以直接把指针放到文件末尾。ios::app不好用，要配合outfile.seekp(0,ios::end)才能把文件弄到文件末尾，默认是在文件头
    for (int i = 0; i < len; i++) {
        if (data[i] != '\\')
            outfile << data[i];
        else {
            if (i + 1 < len && data[i + 1] == 'n') {
                outfile << "\n";
                i++;
            }
        }
    }
    return 0;
}
// 为数据增加头部
void AddHeader(U8* bufSend, U8 seq, U8 ack, U8 flags, U8 rnums) {
    APP_Header* header = (APP_Header*)bufSend;
    header->Sequence_Number = seq;
    header->Acknowledgment_Number = ack;
    header->Flags = flags;
    header->Rnums = rnums;
}
// 入队函数
int enqueue(struct send_queue* que, U8* buf, int len)
{
    // 入队先判断缓冲区
    if ((que->size + 1) % MAX_QUE == que->front)
        return -1;
    que->data[que->size] = buf;
    que->len[que->size] = len;
    que->retime[que->size] = -1;
    que->size = (que->size + 1) % MAX_QUE;
    return 0;
}
// 发送FIN
void SendFIN(struct send_queue* que) {
    U8* bufSend;
    //len = (int)strlen(kbBuf) + 1; //字符串最后有个结束符
    bufSend = (U8*)malloc(ACKlen);
    U8 No = U8(que->size % MAX_NO);
    if (bufSend != NULL) {
        AddHeader(bufSend, No, lastNo, 8, recvSensitivity - fnums);
        cout << "\n.........................发送FIN报文........................." << endl;
        cout << "\n发送FIN报文 Sequence_Number " << int(No) << endl << endl;
        enqueue(&my_buffer, bufSend, FINlen);
        CongestionSend(&my_buffer);
    }
}
// 出队函数 窗口移动
U8* dequeue(struct send_queue* que)
{
    //cout << "出队前" << endl;
    //cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl;
    U8* buf;
    if (que->front == que->nxt)
        return NULL;

    buf = que->data[que->front];
    //*len = que->len[que->front];Time
    que->front = (que->front + 1) % MAX_QUE;
    que->rear = (que->rear + 1) % MAX_QUE;
    if (isstart != 0 && isautoend == 1 && que->front == que->nxt && que->nxt == que->size) {
        isautoend = 0;
        SendFIN(&my_buffer);
    }
    ////cout << "出队后" << endl;
    //cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl;
    return buf;
}
// 初始化队列
void initqueue(struct send_queue* que)
{
    que->front = que->nxt = que->size = 0;
    que->rear = cwnd;
}
// 释放队列
void freequeue(struct send_queue* que)
{
    int i;
    for (i = que->front; i != que->size; i = (i + 1) % MAX_QUE)
        free(que->data[i]);

}
// 判断队列是否满
bool isfullque(struct send_queue* que) {
    if ((que->size + 1) % MAX_QUE == que->front)
        return true;
    return false;
}
// 判断窗口是否满
bool isfullwin(struct send_queue* que) {
    if (que->nxt == que->rear)
        return true;
    return false;
}
// 慢启动和拥塞避免的窗口调整
int ChangeWin(struct send_queue* que) {
    if (cwnd + 2 >= MAX_QUE)
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
        que->rear = (que->rear + 1) % MAX_QUE;
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
            que->rear = (que->rear + 1) % MAX_QUE;
            cout << "拥塞避免窗口改变后" << endl;
            cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
            return 0;
        }
        cout << endl << "拥塞避免 窗口还没变" << " winacc " << winacc << " cwnd " << cwnd << endl << endl;
        return 1;

    }

}

//***************重要函数提醒******************************
//名称：InitFunction
//功能：初始化功能面，由main函数在读完配置文件，正式进入驱动机制前调用
//输入：
//输出：
void InitFunction(CCfgFileParms& cfgParms)
{
    int i;
    int retval;

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
    SendBuf = (char*)malloc(MAX_BUFFER_SIZE);
    if (autoSendBuf == NULL) {
        cout << "内存不够" << endl;
        //这个，计算机也太，退出吧
        exit(0);
    }
    initqueue(&my_buffer);
    for (i = 0; i < MAX_BUFFER_SIZE; i++) {
        autoSendBuf[i] = 'a'; //初始化数据全为字符'a',只是为了测试
    }
    return;
}
//***************重要函数提醒******************************
//名称：EndFunction
//功能：结束功能面，由main函数在收到exit命令，整个程序退出前调用
//输入：
//输出：
void EndFunction()
{
    if (autoSendBuf != NULL)
        free(autoSendBuf);
    return;
    freequeue(&my_buffer);
}
// 实现拥塞发送 能进入这个函数说明此时数据能进入缓冲区 等待窗口到达进行发送 下层固定比特数组
void  CongestionControl(U8* buf, int len, struct send_queue* que) // ifNo
{
    // 接收应用层发送的数据并入队列
    //cout << "处理前比特数组" << endl;
    //print_data_bit(buf, len, 1);
    //cout << "处理前字节数组" << endl;
    //print_data_byte(buf, len, 1);
    U8* abufSend;
    len += headerLen;

    abufSend = (U8*)malloc(len);
    if (abufSend != NULL) {
        AddHeader(abufSend, U8(que->size % MAX_NO), lastNo, 1, recvSensitivity - fnums);
        //cout << "abufSend[1] " << int(abufSend[1]) << endl;
        // 数据加入缓冲区
        strcpy(&abufSend[headerLen], buf);
        enqueue(&my_buffer, abufSend, len);
    }

}
//重发数据并重置超时重发的时间
void ReSend(struct send_queue* que, int i) {
    que->retime[i] = ltime + reportTime;
    APP_Header* header = (APP_Header*)que->data[i];
    header->Rnums = recvSensitivity - fnums;
    int iSndRetval = SendtoLower(que->data[i], que->len[i], 0);
    //cout << "重发 zhen 为 " << i << "的报文" << endl;
    cout << "重发报文—— 发送序号为" << int(header->Sequence_Number) << " 的报文 ltime " << ltime << " retime  " << que->retime[i] << " que->front " << que->front << endl << endl;
    //cout << "发送序号为" << "的报文" << endl;
    //cout << "que->len[i] " << que->len[i] << endl;
    cout << "\n重发报文数据" << endl;
    print_data_byte(que->data[i], que->len[i], 1);
    print_data_bit(que->data[i], que->len[i], 1);
    cout << endl << "~~~~~~~~~~~~~~~~~~~重发报文发送完成~~~~~~~~~~~~~~~~~~~~~~" << endl << endl;
    if (iSndRetval > 0) {
        iSndTotalCount++;
        iSndTotal += que->len[i];
    }
    else {
        iSndErrorCount++;
    }
}
// 快速重传机制 
void QResume(struct send_queue* que, int No) {
    cout << "启动快速重传" << endl;
    cout << "**********************开始快重传********************************" << endl;
    //noNums = 0;
    cout << "快重传改变前" << endl;
    cout << "cwnd " << cwnd << endl;
    cwnd = ssthresh + 3;
    que->rear = que->front + cwnd;  // 窗口变化
    status = 0;
    cout << "快重传改变后" << endl;
    cout << "cwnd " << cwnd << " No " << No << endl;
    ReSend(&my_buffer, No); // 重发该数据帧
    iWorkMode = 10 + iWorkMode % 10; // 启动自动发送
}
// 拥塞慢恢复机制 
void LResume(struct send_queue* que, int No) {
    cout << "启动拥塞恢复" << endl;
    cout << "**********************开始拥塞恢复********************************" << endl;
    //noNums = 0;
    cout << "拥塞恢复改变前" << endl;
    cout << "cwnd " << cwnd << endl << endl;
    cwnd = 1;
    que->rear = que->front + cwnd;  // 窗口变化
    status = 0;
    cout << "拥塞恢复改变后" << endl;
    cout << "cwnd " << cwnd << " No " << No << endl << endl;
    ReSend(&my_buffer, No); // 重发该数据帧
    iWorkMode = 10 + iWorkMode % 10; // 启动自动发送
}
// 发送ACK
void SendACK(U8 ACK) {
    U8* bufSend;
    string des;
    char a[10];
    //len = (int)strlen(kbBuf) + 1; //字符串最后有个结束符
    bufSend = (U8*)malloc(ACKlen);
    if (bufSend != NULL) {
        AddHeader(bufSend, 255, ACK, 2, recvSensitivity - fnums);
        cout << "\n.........................发送ACK报文........................." << endl;
        ReadData("1.txt", a);
        //cout << a << endl;
        if (a[1] != 'F') {
            des += a[1];
            des += a[0];
            //cout << des << endl;
            WriteData("1.txt", des.c_str(), des.size());
        }
        print_data_bit(bufSend, ACKlen, 1);
        int iSndRetval = SendtoLower(bufSend, ACKlen, 0);
        // 数据记录
        if (iSndRetval > 0) {
            iSndTotalCount++;
            iSndTotal += ACKlen * 8;
        }
        else {
            iSndErrorCount++;
        }
    }
    cout << "\n发送ACK Acknowledgment_Number " << int(ACK) << endl;
}
// 发送SYN
void SendSYN(U8 SYN, struct send_queue* que) {
    U8* bufSend;
    bufSend = (U8*)malloc(SYN_ACKlen);
    if (bufSend != NULL) {
        AddHeader(bufSend, SYN, lastNo, 4, recvSensitivity - fnums);
        cout << "\n.........................发送SYN报文........................." << endl;
        cout << "\n发送SYN报文 Sequence_Number " << int(SYN) << endl << endl;
        enqueue(&my_buffer, bufSend, SYNlen);
        CongestionSend(&my_buffer);
    }
}
// 发送SYN_ACK
void SendSYN_ACK(U8 ACK, struct send_queue* que) {
    U8* bufSend;
    bufSend = (U8*)malloc(SYN_ACKlen);
    U8 No = U8(my_buffer.size % MAX_NO);
    if (bufSend != NULL) {
        AddHeader(bufSend, No, ACK, 6, recvSensitivity - fnums);
        cout << "\n.........................发送SYN_ACK报文........................." << endl;
        cout << "\n发送SYN_ACK报文 Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
        enqueue(&my_buffer, bufSend, SYN_ACKlen);
        CongestionSend(&my_buffer);
    }
}
// 发送ACK_PUSH
void SendACK_DATA(U8 ACK, struct send_queue* que) {
    int i = que->nxt;
    // 向下层的0端口发送
    U8* bufSend = que->data[i];
    APP_Header* header = (APP_Header*)bufSend;
    header->Acknowledgment_Number = ACK;
    header->Flags = 3;
    U8 No = header->Sequence_Number;
    cout << "\n.........................发送ACK_PUSH报文........................." << endl;
    cout << "\n发送ACK_PUSH报文 Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
    CongestionSend(&my_buffer);
}

// 发送FIN_ACK
void SendFIN_ACK(U8 ACK, struct send_queue* que) {
    endACK = que->front;
    U8* bufSend;
    bufSend = (U8*)malloc(FIN_ACKlen);
    U8 No = U8(my_buffer.size % MAX_NO);
    if (bufSend != NULL) {
        AddHeader(bufSend, No, ACK, 10, recvSensitivity - fnums);
        cout << "\n.........................发送FIN_ACK报文........................." << endl;
        cout << "\n发送FIN_ACK报文 Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
        enqueue(&my_buffer, bufSend, FIN_ACKlen);
        CongestionSend(&my_buffer);
    }
}
// 阻塞发送
void CongestionSend(struct send_queue* que) {
    cout << endl << endl << "======================================================================" << endl;

    // 如果nxt和rear指针不重合 说明存在可发未发的指针 将所有可发未发的全部发送出去
    //if ((que->nxt + MAX_QUE - que->front) >= (que->rear + MAX_QUE - que->front)) //如果窗口改变导致rear在nxt之前 则停止发生
    //	return;
    // 如果nxt和rear指针不重合 说明存在可发未发的指针 将所有可发未发的全部发送出去
    if (status) {
        cout << "\n 拥塞发生 因此不进行阻塞发送 " << endl;
        cout << " ltime " << ltime << " 等待超时 超时时间 " << my_buffer.retime[my_buffer.front] << endl << endl;
        return;
    }
    if (que->nxt != que->rear && que->nxt != que->size) {
        // 从nxt开始移动 如果不碰到rear或size不停止
        for (int i = que->nxt; i != que->rear && i != que->size; i = (i + 1) % MAX_QUE)
        {
            // 设置超时重传的时间
            que->retime[i] = ltime + reportTime;
            APP_Header* header = (APP_Header*)que->data[i];
            header->Rnums = recvSensitivity - fnums;
            // 向下层的0端口发送
            int iSndRetval = SendtoLower(que->data[i], que->len[i], 0);
            //自动打印数据
            cout << "\n'''''''''''''''''''''''''发送数据'''''''''''''''''''''''''\n\n发送序号为 " << int(header->Sequence_Number) << " 长度为 " << que->len[i] << " 的报文" << endl;
            print_data_byte(que->data[i], que->len[i], 1);
            print_data_bit(que->data[i], que->len[i], 1);
            cout << endl << "~~~~~~~~~~~~~~~~~~~~~~~~发送完成~~~~~~~~~~~~~~~~~~~~~~~~" << endl << endl;
            // nxt指针移动
            que->nxt = (que->nxt + 1) % MAX_QUE;
            // 数据记录
            if (iSndRetval > 0) {
                iSndTotalCount++;
                iSndTotal += que->len[i] * 8;
            }
            else {
                iSndErrorCount++;
            }
            //free(que->data[i]);

            cout << "阻塞发送停止 已发送\ncwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
        }
    }

    else if (que->front != que->size)
        cout << "\n阻塞未发送\ncwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << " ltime " << ltime << " 等待超时 超时时间  " << my_buffer.retime[my_buffer.front] << endl << endl;
    else
        cout << "\n等待进一步操作" << endl << endl;
    cout << endl << "======================================================================" << endl << endl;

}
// 拥塞发生
void CongestionHappen(struct send_queue* que, int No) {
    cout << "启动拥塞发生" << endl;
    cout << "拥塞发生前 \nque->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << " cwnd " << cwnd << endl << endl;
    status = 1; // 设置拥塞发生
    iWorkMode = iWorkMode % 10; // 关闭自动发送
    cwnd = max(cwnd / 2, 1); // 调整窗口大小
    ssthresh = max(cwnd, 6); // 调整慢启动门限
    while (No < que->front) {
        No += MAX_NO;
    }
    que->nxt = No;  // 回退N帧机制 直接从丢失帧开始全部重发
    que->rear = que->front + cwnd;  // 窗口变化
    cout << "拥塞发生后 \nque->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << " cwnd " << cwnd << " ssthresh " << ssthresh << endl << endl;
}

//***************重要函数提醒******************************
//名称：TimeOut
//功能：本函数被调用时，意味着sBasicTimer中设置的超时时间到了，
//      函数内容可以全部替换为设计者自己的想法
//      例程中实现了几个同时进行功能，供参考
//      1)根据iWorkMode工作模式，判断是否将键盘输入的数据发送，还是自动发送——这个功能其实是应用层的
//        因为scanf会阻塞，导致计时器在等待键盘的时候完全失效，所以使用_kbhit()无阻塞、不间断地在计时的控制下判断键盘状态，这个点Get到没？
//      2)不断刷新打印各种统计值，通过打印控制符的控制，可以始终保持在同一行打印，Get？
//      3)如果工iWorkMode设置为自动发送，就每经过autoSendTime * DEFAULT_TIMER_INTERVAL ms，向接口0发送一次
//输入：时间到了就触发，只能通过全局变量供给输入
//输出：这就是个不断努力干活的老实孩子
void TimeOut()
{
    int iSndRetval;
    ltime++;
    if (_kbhit()) {
        //键盘有动作，进入菜单模式
        menu();
    }
    if (ltime % (10 * autoSendTime) == 0)
        fnums = max(0, fnums - 3); // 模拟处理
    if (isstart) {
        //cout << "ltime " << ltime << endl;
        if (endtime != -1 && ltime >= endtime) {
            endtime = -1;
            isstart = 0; //关闭系统
            cout << "\n-------------------------关闭连接-------------------------" << endl << endl;
            return;
        }
        //未发生拥塞时才进行检测是否超时 发生拥塞时 等待第一帧的重发判断连接的重建
        if (status == 0 && my_buffer.retime[my_buffer.front] <= ltime && my_buffer.retime[my_buffer.front] >= 0 && my_buffer.front != my_buffer.nxt)
        {
            cout << "超时 拥塞发生\nque->front " << my_buffer.front << " que->nxt " << my_buffer.nxt << " que->rear " << my_buffer.rear << " que->size " << my_buffer.size << endl;
            cout << "isstart " << isstart << " status " << status << " my_buffer.retime[my_buffer.front] " << my_buffer.retime[my_buffer.front] << endl;
            cout << "ltime " << ltime << endl;
            // 拥塞发生
            CongestionHappen(&my_buffer, my_buffer.front + 1);
            if (noNums > 4) {
                QResume(&my_buffer, int(lastACK + 1) + cnums * MAX_NO);
            }
            else {
                LResume(&my_buffer, int(lastACK + 1) + cnums * MAX_NO);
            }
        }
        else if (status == 1 && my_buffer.retime[my_buffer.front] <= ltime)
        {
            cout << "拥塞发生下超时 开始拥塞恢复" << endl;
            LResume(&my_buffer, int(lastACK + 1));

        }
        switch (iWorkMode / 10) {
        case 0:
            break;
        case 1:
            int len;
            U8 word[] = "i love duan sir";
            if (ltime % autoSendTime == 0) {
                //cout << "开始自动发送" << endl;
                //定时发送前先判断缓冲区是否满 如果满中止重发 		此处先不考虑MSS 不实现分组技术 也不考虑定时重新开始自动发送
                if (isfullque(&my_buffer)) {
                    cout << "||||||||||||||||||||||||||||缓冲区已经满咯 寄！|||||||||||||||||||||||||||||||||||||" << endl;
                    break;
                }
                //定时发送, 每间隔autoSendTime * DEFAULT_TIMER_INTERVAL ms 发送一次
                //if (printCount % autoSendTime == 0) {
                strcpy(autoSendBuf, word);
                len = strlen(word); //每次发送数量
                CongestionControl(autoSendBuf, len, &my_buffer);
                //iSndRetval = CongestionControl(bufSend, iSndRetval, 0);
                //free(bufSend);
            }
            break;

        }
        if (ltime % (autoSendTime + 10) == 0)
            // 拥塞发送 
            CongestionSend(&my_buffer);
    }
}

//------------华丽的分割线，以下是数据的收发,--------------------------------------------

void RecvfromUpper(U8* buf, int len) {
    return;
}
//***************重要函数提醒******************************
//名称：RecvfromLower
//功能：本函数被调用时，意味着得到一份从低层实体递交上来的数据
//      函数内容全部可以替换成设计者想要的样子
//      例程功能介绍：
//          1)例程实现了一个简单粗暴不讲道理的策略，所有从接口0送上来的数据都直接转发到接口1，而接口1的数据上交给高层，就是这么任性
//          2)转发和上交前，判断收进来的格式和要发送出去的格式是否相同，否则，在bite流数组和字节流数组之间实现转换
//            注意这些判断并不是来自数据本身的特征，而是来自配置文件，所以配置文件的参数写错了，判断也就会失误
//          3)根据iWorkMode，判断是否需要把数据内容打印
//输入：U8 * buf,低层递交上来的数据， int len，数据长度，单位字节，int ifNo ，低层实体号码，用来区分是哪个低层
//输出：
//打印统计信息
//***************重要函数提醒******************************
//名称：CongestionControl
//功能：本函数被调用时，意味着应用层开始向下层发送数据，发送端进行拥塞控制，对发送数据速率进行调控。
//      例程功能介绍
//         1)通过低层的数据格式参数lowerMode，判断要不要将数据转换成bit流数组发送，发送只发给低层接口0，
//           因为没有任何可供参考的策略，讲道理是应该根据目的地址在多个接口中选择转发的。
//         2)判断iWorkMode，看看是不是需要将发送的数据内容都打印，调试时可以，正式运行时不建议将内容全部打印。
//输入：U8 * buf,高层传进来的数据， int len，数据长度，单位字节
//输出：
//在这里我的应用层设计为会从
// 当出现
void RecvfromLower(U8* buf, int len, int ifNo)
{
    cout << endl << endl << "======================================================================" << endl;
    int retval;
    U8 flag;
    U8 No;
    U8 ACK;
    U8 Rnums;
    U8* originbuf = NULL;
    if (fnums >= recvSensitivity)
    {
        cout << endl << "*************************报文丢失*************************" << endl << endl;
        return; // 伪造报文丢失现象

    }
    APP_Header* header = (APP_Header*)buf;
    flag = header->Flags;
    No = header->Sequence_Number;
    ACK = header->Acknowledgment_Number;
    Rnums = header->Rnums;
    cout << "\n////////////////////////接收到数据////////////////////////\n\n标志位——flag " << int(flag) << " 长度——len " << len << " 剩余能力——Rnums " << int(Rnums) << endl;
    retval = len * 8;//换算成位,进行统计
    fnums++;
    cout << "\n接收帧速率 " << fnums << endl;
    cout << "\n接收到的数据内容" << endl;
    print_data_bit(buf, len, 1);
    //cout << "buf[0] " << buf[0] << " int buf[0] " << int(buf[0]) << " len " << len << " strlen " << strlen(buf) << endl;
    print_data_byte(buf, len, 1);
    ////判断字节数据第一位 判断是数据报文还是ACK报文 0为ACK报文 1为数据报文
    switch (flag) {
    case 1: // PUSH报文
        //if (No != (lastNo + 1) % MAX_NO) {
        //	cout << "*************************PUSH报文序号出错*************************" << endl;
        //	cout << "\n收到数据报文不符合要求,序号为 " << int(No) << " 要求为" << int(lastNo + 1) % MAX_NO << endl;
        //	SendACK(lastNo);
        //}
        //else {
        cout << "\n------------------------PUSH报文------------------------" << endl << endl;
        originbuf = &buf[headerLen];
        cout << "\n收到正确的数据报文,序号为" << int(No) << endl;
        cout << "\n去掉头部后的数据" << endl;
        print_data_byte(originbuf, len - headerLen, 1);
        SendACK(No);
        lastNo = No;
        //WriteData("output.txt", originbuf, len - 2);
    //}
        break;
    case 2:
        //cout << "\n收到ACK，序号为" << int(No) << endl;
        // 判断得到的是ACK报文
        // 如果ACK与上一个ACK序号一样 则重复
        if (ACK == lastACK) {
            cout << "*************************ACK报文序号重复*************************" << endl;
            cout << "\n收到和上次一样的ACK，序号为" << int(ACK) << " 出现次数 " << ++noNums << endl;
            if (status == 0 && noNums == 2)
                // 拥塞发生
                CongestionHappen(&my_buffer, lastACK + 2);
            // 触发快恢复
            if (noNums == 4)
                QResume(&my_buffer, int(lastACK + 1));
        }
        else {

            cout << "\n------------------------ACK报文------------------------" << endl;
            // 说明数据接收成功
            if (ACK > lastACK + 1)
                cout << "\n没收到预计ACK " << lastACK + 1 << " 但收到ACK序号为 " << int(ACK) << "认为前一报文也收到\n" << endl;
            else
                cout << "\n收到正确的ACK报文,序号为" << int(ACK) << endl;
            noNums = 1; //计数
            if (status == 1) { //拥塞状态收到ACK重新启动自动发送
                iWorkMode = 10 + iWorkMode % 10; // 启动自动发送
                status = 0; // 解除拥塞发生
            }
            int last = int(lastACK);
            int now = int(ACK);
            last += cnums * MAX_NO;
            if (now - int(lastACK) < -120) {
                cnums++;
            }
            now += cnums * MAX_NO;
            if (cnums >= MAX_QUE / MAX_NO)
                cnums = 0;
            //cout << "now " << now << " last " << last << " my_buffer.front " << my_buffer.front << endl;
            //my_buffer.nxt = min(now + 1, my_buffer.rear);
            for (int i = now; i > last; i--) {
                my_buffer.retime[i % MAX_QUE] = -2; //取消超时重传机制
            }
            if (status == 1 && my_buffer.front == my_buffer.nxt) {
                my_buffer.nxt = min(now + 1, my_buffer.rear);
            }
            if (my_buffer.retime[my_buffer.front] == -2)
                for (int i = my_buffer.front; my_buffer.retime[i] == -2 && i != my_buffer.nxt; i = (i + 1) % MAX_QUE)
                {
                    //cout << ""
                    dequeue(&my_buffer); // 如果确认的是队列的front帧 则出队
                    //if (!status) // 在拥塞发生状态下要等待快恢复 否则直接改变窗口
                    ChangeWin(&my_buffer);
                }
            if (ACK == endACK) {
                endACK = 255;
                isstart = 0; //关闭系统
                cout << "\n-------------------------关闭连接-------------------------" << endl << endl;
                return;
            }
            lastACK = ACK;
        }
        break;
    case 3:
        if (ACK == my_buffer.front % MAX_NO) {

            dequeue(&my_buffer); // 如果确认的是队列的front帧 则出队
            my_buffer.retime[my_buffer.front % MAX_QUE] = -2; //取消超时重传机制
            ChangeWin(&my_buffer);
            isstart = 1;
            cout << "\n------------------------ACK_PUSH报文------------------------" << endl << "Acknowledgment_Number " << int(ACK) << endl << endl;
            lastNo = No;
            lastACK = ACK;
            originbuf = &buf[headerLen];
            cout << "\n收到正确的数据报文,序号为" << int(No) << endl;
            cout << "\n去掉头部后的数据" << endl;
            print_data_byte(originbuf, len - 4, 1);
            //WriteData("output.txt", originbuf, len - 3);
            SendACK(No);
        }
        else
            cout << "*************************ACK_PUSH报文ACK出错*************************" << endl;
        break;
    case 4:
        if (isstart == 0) {
            cout << "\n------------------------SYN报文------------------------" << endl << "Sequence_Number " << int(No) << endl << endl;
            lastNo = No;
            SendSYN_ACK(No, &my_buffer);
        }
        break;

    case 6:
        if (isstart == 0) {
            cout << "\n------------------------SYN_ACK报文------------------------" << endl << "Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
            if (ACK == my_buffer.front % MAX_NO) {
                lastNo = No;
                isstart = 1; //启动系统
                my_buffer.retime[my_buffer.front % MAX_QUE] = -2; //取消超时重传机制
                lastACK = ACK;
                dequeue(&my_buffer); // 如果确认的是队列的front帧 则出队
                ChangeWin(&my_buffer);
                SendACK_DATA(No, &my_buffer);
            }
            else
                cout << "*************************SYN_ACK报文ACK出错*************************" << endl;
        }
        break;
    case 8:
        cout << "\n------------------------FIN报文------------------------" << endl << "Sequence_Number " << int(No) << endl;
        SendACK(No);
        lastNo = No;
        SendFIN_ACK(No, &my_buffer);
        break;
    case 10:
        cout << "\n------------------------FIN_ACK报文------------------------" << endl << "Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
        if (int(ACK) == (my_buffer.front + 128 - 1) % MAX_NO) {
            endtime = ltime + 50;
            lastNo = No;
            //cout << endtime << endl;
            SendACK(No);
        }
        break;
    default:
        cout << "\n出错了 寄！" << endl;
        cout << "\n出错数据" << endl;
        print_data_bit(buf, len, 1);
        break;
    }

    iRcvTotal += retval;
    iRcvTotalCount++;


    cout << endl << "======================================================================" << endl << endl;
    ////打印
    //switch (iWorkMode % 10) {
    //case 1:
    //	cout << endl << "接收接口 " << ifNo << " 数据：" << endl;
    //	print_data_bit(buf, len, lowerMode[ifNo]);
    //	break;
    //case 2:
    //	cout << endl << "接收接口 " << ifNo << " 数据：" << endl;
    //	print_data_byte(buf, len, lowerMode[ifNo]);
    //	break;
    //case 0:
    //	break;
}

// 当应用层要接收数据时，调用该函数将数据放入这里的循环队列，根据窗口实现流量控制
void print_statistics()
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
    cout << " 共接收 " << iRcvTotal << " 位," << iRcvTotalCount << " 次" << endl;
    spin++;

}

void menu()
{
    int selection;
    unsigned short port;
    int iSndRetval;
    char kbBuf[2000];
    string des;
    U8* bufSend;
    int len;
    int rlen;

    //发送|打印：[发送控制（0，等待键盘输入；1，自动）][打印控制（0，仅定期打印统计信息；1，按bit流打印数据，2按字节流打印数据]
    cout << endl << endl << "设备号:" << strDevID << ",    层次:" << strLayer << ",    实体号:" << strEntity;
    cout << endl << "1-启动自动发送;" << endl << "2-停止自动发送; " << endl << "3-发送文本; ";
    cout << endl << "4-依次发送数据; " << endl << "5-关闭连接;" << endl << "6-单独发送数据;";
    cout << endl << "0-取消" << endl << "请输入数字选择命令：";
    cin >> selection;
    getchar();
    switch (selection) {
    case 0:
        break;
    case 1:
        iWorkMode = 10 + iWorkMode % 10;
        isstart = 1;
        break;
    case 2:
        iWorkMode = iWorkMode % 10;
        break;
    case 3:
        if (isfullque(&my_buffer)) {
            cout << endl << "缓冲区已满，暂时无法发送";
            break;
        }
        cout << "请输入要发送的文本：";
        cin >> kbBuf;
        len = (int)strlen(kbBuf) + 1; //字符串最后有个结束符
        rlen = ReadData(kbBuf, SendBuf);
        bufSend = (U8*)malloc(rlen);
        strcpy(bufSend, SendBuf);
        if (len > 0) {
            isautoend = 1;
            //lastSeq = char(rand() % 127);
            SendSYN(my_buffer.size % MAX_NO, &my_buffer);
            CongestionControl(bufSend, rlen, &my_buffer);
        }
        break;
    case 4:
        if (isfullque(&my_buffer)) {
            cout << endl << "缓冲区已满，暂时无法发送";
            break;
        }
        cout << "请输入要发送的文本：";
        cin >> kbBuf;
        len = (int)strlen(kbBuf) + 1; //字符串最后有个结束符
        rlen = ReadData(kbBuf, SendBuf);
        bufSend = (U8*)malloc(rlen);
        strcpy(bufSend, SendBuf);
        if (rlen > 0) {
            //cout << "isstart " << isstart << endl;
            if (isstart == 0)
                //lastSeq = char(rand() % 127);
                SendSYN(my_buffer.size % MAX_NO, &my_buffer);
            CongestionControl(bufSend, rlen, &my_buffer);
        }
        break;
    case 5:
        SendFIN(&my_buffer);
        break;
    case 6:
        if (isfullque(&my_buffer)) {
            cout << endl << "缓冲区已满，暂时无法发送";
            break;
        }
        cout << "请输入要发送的数据：";
        cin.getline(kbBuf, 2000);
        cout << "请输入要发送的地址：";
        cin >> des;
        des += strDevID;
        //cout << des << endl;
        WriteData("1.txt", des.c_str(), des.size());
        //char a[10];
        //ReadData("1.txt", a); // 从1.txt读取数据并放入a中
        //cout << a << endl;
        len = (int)strlen(kbBuf) + 1; //字符串最后有个结束符
        bufSend = (U8*)malloc(len);
        strcpy(bufSend, kbBuf);
        if (len > 0) {
            //if (isstart == 0)
                //lastSeq = char(rand() % 127);
                //SendSYN(my_buffer.size % MAX_NO, &my_buffer);
            isstart = 1;
            CongestionControl(bufSend, len, &my_buffer);
            CongestionSend(&my_buffer);
            isstart = 0;
        }
        break;
    case 7:
        print_statistics();
        break;
    }

}
