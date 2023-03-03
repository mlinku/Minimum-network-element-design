//apptester�Ĺ����ļ�
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

//----------------�����ķָ��ߣ�һЩͳ���õ�ȫ�ֱ���----------------
//====================���ݿ������====================
U8* autoSendBuf;  //������֯�Զ��������ݵĻ���
U8* sendBuf;     // ������֯�����ı����ݵĻ���
int ltime = 0;  // ȫ��ʱ�����ڴ���ʱ�ش�
int endtime = -1; // ��¼����ʱ�� �����Ĵλ���
int reportTime; //��ʱ�ش���ʱ��
int spin = 1;  //  ��ӡ��̬��Ϣ����
string outPath; // ��¼�����ļ����ļ�·��
string readPath; // ��¼�����ļ����ļ�·��
int iSndTotal = 0;  // ��¼������������
int iSndTotalCount = 0; // ��¼���������ܴ���
int iSndErrorCount = 0;  // ��¼���ʹ������
int iRcvTotal = 0;     // ��¼������������
int iRcvTotalCount = 0; // ��¼���������ܴ���
//=====================��Ŵ������=====================
int cwnd = 1; // ӵ�����ڴ�С 
int ssthresh = 8; // ����������
int winacc = 0; // ��¼�յ���֡��Ŀ������ӵ������Ĵ��ڿ���
U8 lastRecvNo = 255; // ��¼ǰһ���յ����ĵ����
U8 lastUpNo = 0; // ��¼��һ���Ϸ������ 
U8 lastRecvACK = 255; // ��¼ǰһ������ACK���
int noNums = 0; // ��¼ͬһ���ACK�յ��Ĵ���
int status = 0; // �ж�ӵ���Ƿ��� �������Ϊ1 δ����Ϊ0 
int cNums = 0; // �����ж���ž�������ѭ�� ���ڷ��ʹ��ڵĳ��� 
int fNums = 0; // ��λʱ�����յ���֡��Ŀ ����ģ����������
U8 endACK = 255; // ������ACK��� ���ڴ����Ĵλ���
vector<string> mid_buffer; // ���ڴ���и�ĵĻ�����
int mid_len;
//======================���ķ������======================
int headerLen = 4; // ͷ������
int iciLen = 2; // ICI����
string strIP; // ���豸IP��ַ
U8 uConnectIP[2]; // ��¼��ǰ���ӵ�IP��ַ ����һλΪ0��ʾ������
U8 uAutoIP[2]; //  ��¼�Զ����͵�IP��ַ ����һλΪ0��ʾ������
U8 save; // ���ڴ��һЩż��Ҫ��ʱ�洢������
int autoTime = 0; // ��¼�Զ����͵Ĵ���
//======================��־��¼���======================
int lastRecord = -1; // ��һ����¼����־������
string msgRecord[50]; // ���ڴ����־���ڴ�����ʾ

//----------------�����ķָ��ߣ�һЩ��Ҫ�Ľṹ��----------------
// APP�㷢���²��ICI
struct APP_SndICI {
    U8 uDesIP[2]; // Ŀ��IP��ַ
};

// APP���յ��²��ICI
struct APP_RcvICI {
    U8 uSrcIP[2]; // ��ԴIP��ַ
};

// APP�㱨��ͷ��
struct APP_Header {
    U8 Sequence_Number;  // ���͵ı������к� 
    U8 Acknowledgment_Number; // ȷ���յ��ı������к� ֻ�б�־λACKΪ1ʱ��Ч
    U8 Flags; // ��־λ 
    U8 Rnums; // ʣ�����֡���� 
};

// ���ʹ���
struct send_queue { // ��ָ��ѭ�����л�������� ���ڱض���Ȼ�����С ��Ȼ�����~
    int front; // ָ���ѷ��͵���δ�յ�ȷ�ϵĵ�һ���ֽ����к�
    int nxt;  //ָ��ɷ���δ���͵ĵ�һ���ֽ����к�
    int rear; //ָ�򲻿ɷ���δ���͵ĵ�һ���ֽ����к� ����
    int size; // ָ�򻺳��������һ������		
    U8* data[MAX_SEND]; // ���������ݼ���
    int len[MAX_SEND]; // ������ÿ�����ݵĳ���
    int reTime[MAX_SEND];  // ���������ݵĳ�ʱ�ط���¼
    int reNo[MAX_SEND]; // ���������ݳ�������
};

// ���մ���
struct recv_queue { // ����ͨͨ��ѭ������
    int front; // ָ�򴰿��ڵ�һ������
    int rear; // ָ�򴰿������һ�����ݵĺ�һλ
    int len[MAX_RECV]; // ������ÿ�����ݵĳ���
    U8* data[MAX_RECV]; // ���������ݼ���
};

// ��ʼ�����ʹ���
struct send_queue send_buffer;
// ��ʼ�����մ���
struct recv_queue recv_buffer;


//----------------�����ķָ��ߣ��и�Ĵ�����صĺ���----------------

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


//----------------�����ķָ��ߣ�������صĺ���----------------
// ���ʹ��ںͽ��մ��ڵĳ�ʼ��
void initqueue(struct send_queue* sendque, struct recv_queue* recvque)
{
    sendque->front = sendque->nxt = sendque->size = 0;
    sendque->rear = cwnd;
    recvque->front = recvque->rear = 0;
}

// ���ʹ��ںͽ��մ��ڵ��ͷ�
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

// �жϴ����Ƿ���  
bool isfullque(struct send_queue* que) {
    if ((que->size + 1) % MAX_SEND == que->front)
        return true;
    return false;
}

// �ж�ӵ�������Ƿ���
bool isfullwin(struct send_queue* que) {
    if (que->nxt == que->rear)
        return true;
    return false;
}

// ���ʹ�����Ӻ���
int enSendQueue(struct send_queue* que, U8* buf, int len)
{
    // ������жϻ�����
    if ((que->size + 1) % MAX_SEND == que->front)
        return -1;
    que->data[que->size] = buf;
    que->len[que->size] = len;
    que->reTime[que->size] = -1;
    que->reNo[que->size] = 0;
    que->size = (que->size + 1) % MAX_SEND;
    return 0;
}

// ���ʹ��ڳ��Ӻ��� �����ƶ�
U8* deSendQueue(struct send_queue* que)
{
    //cout << "����ǰ" << endl;
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
    ////cout << "���Ӻ�" << endl;
    //cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl;
    return buf;
}

// ���մ�����Ӻ���
void enRecvQueue(struct recv_queue* que, U8* buf, int seq, int len) {

    cout << seq << endl;
    cout << "����ǰ���մ����ڱ��ĵ����" << endl;
    APP_Header* header1;
    if (que->front == que->rear)
    {
        cout << "��" << endl;
    }
    else
    {
        for (int i = que->front; i < que->rear; i++) {
            if (que->data[i] == NULL)
                cout << "��" << " ";
            else {
                header1 = (APP_Header*)&que->data[i][iciLen];
                cout << (int)header1->Sequence_Number << " ";
            }

        }
        cout << endl;
    }
    cout << seq << " " << (int)lastRecvNo << " " << que->front << " " << endl;
    //int shift = (seq - (int)lastRecvNo - 2 + MAX_RECV + que->front) % MAX_RECV; // �ñ���Ӧ���ڴ����ڷ��õ�λ��
    int shift = (seq - (int)lastRecvNo - 1 + MAX_RECV + que->front) % MAX_RECV; // �ñ���Ӧ���ڴ����ڷ��õ�λ��
    cout << shift << endl;
    if ((seq - (int)lastRecvNo - 2 + MAX_RECV) % MAX_RECV < (que->rear + MAX_RECV) % MAX_RECV) // �жϸñ����ǲ����ڽ��մ���ͷ����β���м�
    {
        que->data[shift] = buf;
        que->len[shift] = len;
    }
    else {
        for (int i = que->rear; i < shift; i++) {
            que->data[i] = NULL; // ����ñ���Ӧ���ڴ����ڷ��õ�λ�ò���β�������ֵ
        }
        que->data[shift] = buf;
        que->len[shift] = len;
        que->rear = (shift + 1) % MAX_RECV;
        cout << "�������մ����ڱ��ĵ����" << endl;
        cout << que->front << " " << que->rear << endl;
    }


    for (int i = que->front; i < que->rear; i++) {
        if (que->data[i] == NULL)
            cout << "��" << " ";
        else {
            header1 = (APP_Header*)&que->data[i][iciLen];
            cout << (int)header1->Sequence_Number << " ";
        }

    }
    cout << endl;
}

// ���մ��ڳ��Ӻ���
U8* deRecvQueue(struct recv_queue* que, int* len) {
    U8* buf;
    if (que->front == que->rear)
        return NULL;
    buf = que->data[que->front];
    *len = que->len[que->front];
    que->front = (que->front + 1) % MAX_RECV;
    return buf;
}

// �����մ����Ƿ���Ҫ����
void checkRecvQueue(struct recv_queue* que) {
    cout << "������ǰ���մ����ڱ��ĵ����" << endl;
    APP_Header* header;
    int len = 0;
    if (que->front == que->rear)
    {
        cout << "��" << endl;
    }
    else
    {
        for (int i = que->front; i < que->rear; i++) {
            if (que->data[i] == NULL)
                cout << "��" << " ";
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
            cout << int(header->Sequence_Number) << "����" << endl;
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
            break; //�������������Ϊ�ϴν������+1 ֱ���˳�ѭ��
        }
    }
    cout << "�����Ӻ���մ����ڱ��ĵ����" << endl;
    if (que->front == que->front)
        cout << "����Ϊ��" << endl;
    else
        for (int i = que->front; i < que->rear; i++) {
            if (que->data[i] == NULL)
                cout << "��" << " ";
            else {
                header = (APP_Header*)&que->data[i][iciLen];
                cout << (int)header->Sequence_Number << " ";
            }
        }
}



//----------------�����ķָ��ߣ��ı�������صĺ���----------------
// ��ȡ�ļ�����
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
    f.seekg(0, std::ios_base::beg);//���ļ�ָ���Ƶ����ļ�ͷλ��
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

// ���ļ�д������
int writeData(string data, int len) {
    if (_access(outPath.c_str(), 0) == -1)
    {
        _mkdir(outPath.c_str());
    }
    cout << data.size() << endl;
    cout << len << endl;
    int file_len = int(data.at(0));
    string filename = data.substr(1, file_len);
    cout << endl << "�ɹ������ļ�" << filename << endl;
    string a = u8"  �����ļ� ���� ";
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


//----------------�����ķָ��ߣ����Ĵ�����صĺ���----------------
// Ϊ��������ͷ��
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


//----------------�����ķָ��ߣ�����������صĺ���----------------
// ʵ��ӵ������ �ܽ����������˵����ʱ�����ܽ��뻺���� ���н���Ĵ��ڼ���ͷ�������뷢�ʹ���
void  congestionControl(U8* buf, int len, U8* desIP, struct send_queue* que) // ifNo
{
    // ����Ӧ�ò㷢�͵����ݲ������
    //cout << "����ǰ��������" << endl;
    //print_data_bit(buf, len, 1);
    //cout << "����ǰ�ֽ�����" << endl;
    //print_data_byte(buf, len, 1);
    U8* abufSend;
    len += headerLen + iciLen;
    cout << "len	" << len << endl;
    while (len > MAX_SEND_DATA) { //�����и�
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
        // ���ݼ��뻺����
        strcpy(&abufSend[headerLen + iciLen], buf);
        enSendQueue(&send_buffer, abufSend, len);
    }

}

// ʵ���������� ��ӵ�����ڵĴ�С��������
void congestionSend(struct send_queue* que) {

    // ���nxt��rearָ�벻�غ� ˵�����ڿɷ�δ����ָ�� �����пɷ�δ����ȫ�����ͳ�ȥ
    //if ((que->nxt + MAX_SEND - que->front) >= (que->rear + MAX_SEND - que->front)) //������ڸı䵼��rear��nxt֮ǰ ��ֹͣ����
    //	return;
    // ���nxt��rearָ�벻�غ� ˵�����ڿɷ�δ����ָ�� �����пɷ�δ����ȫ�����ͳ�ȥ
    if (status) {
        cout << "\n ӵ������ ��˲������������� " << endl;
        cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << " ltime " << ltime << " �ȴ���ʱ ��ʱʱ��  " << send_buffer.reTime[send_buffer.front] << endl << endl;
        return;
    }
    if (que->nxt != que->rear && que->nxt != que->size) {
        cout << endl << endl << "======================================================================" << endl;

        // ��nxt��ʼ�ƶ� ���������rear��size��ֹͣ
        for (int i = que->nxt; i != que->rear && i != que->size; i = (i + 1) % MAX_SEND)
        {
            // ���ó�ʱ�ش���ʱ��
            que->reTime[i] = ltime + reportTime;
            APP_SndICI* ici = (APP_SndICI*)que->data[i];
            APP_Header* header = (APP_Header*)&(que->data[i])[iciLen];
            header->Rnums = recvSensitivity - fNums;
            // ���²��0�˿ڷ���
            int iSndRetval = SendtoLower(que->data[i], que->len[i], 0);
            //�Զ���ӡ����
            cout << "\n'''''''''''''''''''''''''��������'''''''''''''''''''''''''\n\nĿ��IP��ַΪ " << ici->uDesIP[0] << "." << ici->uDesIP[1] << " �������Ϊ " << int(header->Sequence_Number) << " ����Ϊ " << que->len[i] << " �ı���" << endl;
            if (que->len[i] < 500) {
                print_data_bit(que->data[i], que->len[i], 1);
                //cout << "buf[0] " << buf[0] << " int buf[0] " << int(buf[0]) << " len " << len << " strlen " << strlen(buf) << endl;
                print_data_byte(que->data[i], que->len[i], 1);
            }
            else
                cout << endl << "�������ݳ���̫��������ʾ" << endl;
            cout << endl << "~~~~~~~~~~~~~~~~~~~~~~~~�������~~~~~~~~~~~~~~~~~~~~~~~~" << endl << endl;
            // nxtָ���ƶ�
            que->nxt = (que->nxt + 1) % MAX_SEND;
            // ���ݼ�¼
            dataRecord(iSndRetval, 1);
            //free(que->data[i]);

            cout << "��������ֹͣ �ѷ���\ncwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
            addMsg(++lastRecord, 1, int(header->Flags), int(header->Sequence_Number), int(header->Acknowledgment_Number), ici->uDesIP, iSndRetval);
        }
        cout << endl << "======================================================================" << endl << endl;
    }

    else if (que->front != que->size) {
        cout << endl << endl << "======================================================================" << endl;
        cout << "\n����δ����\ncwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << " ltime " << ltime << " �ȴ���ʱ ��ʱʱ��  " << send_buffer.reTime[send_buffer.front] << endl << endl;
        cout << endl << "======================================================================" << endl << endl;
    }
    //else
        //cout << "\n�ȴ���һ������" << endl << endl;

}

// ����ӵ������ 
void congestionHappen(struct send_queue* que, int No) {
    cout << "����ӵ������" << endl;
    cout << "ӵ������ǰ \nque->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << " cwnd " << cwnd << endl << endl;
    status = 1; // ����ӵ������
    // ������Զ�����ģʽ �Ȱ��Զ����͹ر���
    if (uAutoIP[0]) {
        save = uAutoIP[0];
        uAutoIP[0] = 0;
    }
    cwnd = max(cwnd / 2, 1); // �������ڴ�С
    ssthresh = max(cwnd, 6); // ��������������
    No += cNums * MAX_NO; // ��Noת��Ϊ��ȷ��λ��
    que->nxt = No;  // ����N֡���� ֱ�ӴӶ�ʧ֡��ʼȫ���ط�
    que->rear = que->front + cwnd;  // ���ڱ仯
    cout << "ӵ�������� \nque->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << " cwnd " << cwnd << " ssthresh " << ssthresh << endl << endl;
}

// ���������ط������ó�ʱ�ط���ʱ��
void reSend(struct send_queue* que, int i) {
    if (++que->reNo[i] == 4) {
        cout << endl << endl << "�������������������������������ش������ﵽ���� �ر����� ���������ӣ�����������������������������" << endl << endl;
        uConnectIP[0] = 0;
        send_buffer.nxt = send_buffer.front;
        send_buffer.size = send_buffer.front; // ��ջ�����
        send_buffer.reTime[send_buffer.front] = -2; // ��ջ�����
        status = 0;
        msgRecord[++lastRecord % 50] = u8"   ���������ش��ﵽ���� ���������ӣ�������";
        return;
    }
    que->reTime[i] = ltime + reportTime;
    APP_SndICI* ici = (APP_SndICI*)que->data[i];
    APP_Header* header = (APP_Header*)&que->data[i][iciLen];
    header->Rnums = recvSensitivity - fNums;
    int iSndRetval = SendtoLower(que->data[i], que->len[i], 0);
    //cout << "�ط� zhen Ϊ " << i << "�ı���" << endl;
    cout << "�ط����ġ��� �������Ϊ" << int(header->Sequence_Number) << " �ı��� ltime " << ltime << " �´��ش�ʱ��  " << que->reTime[i] << " �ۼ��ش�����  " << que->reNo[i] << " que->front " << que->front << endl << endl;
    //cout << "�������Ϊ" << "�ı���" << endl;
    //cout << "que->len[i] " << que->len[i] << endl;
    cout << "\n�ط���������" << endl;
    print_data_byte(que->data[i], que->len[i], 1);
    print_data_bit(que->data[i], que->len[i], 1);
    cout << endl << "~~~~~~~~~~~~~~~~~~~�ط����ķ������~~~~~~~~~~~~~~~~~~~~~~" << endl << endl;
    dataRecord(iSndRetval, 1);
    addMsg(++lastRecord, 1, int(header->Flags), int(header->Sequence_Number), int(header->Acknowledgment_Number), ici->uDesIP, iSndRetval);
}

// ��������ӵ������Ĵ��ڵ���
int changeWin(struct send_queue* que) {
    if (cwnd + 2 >= MAX_SEND)
    {
        cout << endl << "����̫���� �ģ�" << endl;
        cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
        return -1;
    }
    if (cwnd < ssthresh)
    {
        cout << endl << "���������ڸı�ǰ" << endl;
        cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
        cwnd += 1;
        que->rear = (que->rear + 1) % MAX_SEND;
        cout << "���������ڸı��" << endl;
        cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
        return 0;
    }
    else {
        if (++winacc >= cwnd) {
            cout << endl << "ӵ�����ⴰ�ڸı�ǰ" << endl;
            cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
            cwnd += 1;
            winacc = 0;
            que->rear = (que->rear + 1) % MAX_SEND;
            cout << "ӵ�����ⴰ�ڸı��" << endl;
            cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
            return 0;
        }
        cout << endl << "ӵ������ ���ڻ�û��" << " winacc " << winacc << " cwnd " << cwnd << endl << endl;
        return 1;

    }

}

// �����ش����� 
void qResume(struct send_queue* que, int No) {
    cout << "���������ش�" << endl;
    cout << "**********************��ʼ���ش�********************************" << endl;
    //noNums = 0;
    cout << "���ش��ı�ǰ" << endl;
    cout << "cwnd " << cwnd << endl;
    cwnd = ssthresh + 3;
    que->rear = que->front + cwnd;  // ���ڱ仯
    cout << "���ش��ı��" << endl;
    cout << "cwnd " << cwnd << " No " << No << endl;
    reSend(&send_buffer, No); // �ط�������֡
    //iWorkMode = 10 + iWorkMode % 10; // �����Զ�����
}

// ӵ�����ָ����� 
void lResume(struct send_queue* que, int No) {
    cout << "����ӵ���ָ�" << endl;
    cout << "**********************��ʼӵ���ָ�********************************" << endl;
    //noNums = 0;
    cout << "ӵ���ָ��ı�ǰ" << endl;
    cout << "cwnd " << cwnd << endl << endl;
    cwnd = 1;
    que->rear = que->front + cwnd;  // ���ڱ仯
    //status = 0;
    cout << "ӵ���ָ��ı��" << endl;
    cout << "cwnd " << cwnd << " No " << No << endl << endl;
    reSend(&send_buffer, No); // �ط�������֡
    //iWorkMode = 10 + iWorkMode % 10; // �����Զ�����
}


//----------------�����ķָ��ߣ����ķ�����صĺ���----------------
// ����ACK
void sendACK(U8 ACK, U8* desIP) {
    U8* bufSend;
    string des;
    char a[10];
    //len = (int)strlen(kbBuf) + 1; //�ַ�������и�������
    bufSend = (U8*)malloc(headerLen + iciLen);
    if (bufSend != NULL) {
        addHeader(bufSend, desIP, 255, ACK, 2, recvSensitivity - fNums);
        cout << "\n.........................����ACK����........................." << endl;
        print_data_bit(bufSend, headerLen + iciLen, 1);
        int iSndRetval = SendtoLower(bufSend, headerLen + iciLen, 0);
        dataRecord(iSndRetval, 1);
        cout << "\n����ACK Acknowledgment_Number " << int(ACK) << endl;
        addMsg(++lastRecord, 1, 2, 255, ACK, desIP, iSndRetval);
    }


}

// ����SYN
void sendSYN(U8 SYN, U8* desIP, struct send_queue* que) {
    U8* bufSend;
    bufSend = (U8*)malloc(headerLen + iciLen);
    if (bufSend != NULL) {
        addHeader(bufSend, desIP, SYN, lastRecvNo, 4, recvSensitivity - fNums);
        cout << "\n.........................����SYN����........................." << endl;
        cout << "\n����SYN���� Sequence_Number " << int(SYN) << endl << endl;
        enSendQueue(&send_buffer, bufSend, headerLen + iciLen);
        congestionSend(&send_buffer);
    }
}

// ����SYN_ACK
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
        cout << "\n.........................����SYN_ACK����........................." << endl;
        cout << "\n����SYN_ACK���� Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
        enSendQueue(&send_buffer, bufSend, headerLen + iciLen);
        congestionSend(&send_buffer);
    }
}

// ����ACK_PUSH
void sendACK_DATA(U8 ACK, U8* desIP, struct send_queue* que) {
    int i = que->nxt;
    //for (i = send_buffer.nxt; i != send_buffer.size; i = (i + 1) % MAX_SEND) {
    //    APP_Header* header = (APP_Header*)&send_buffer.data[i][iciLen];
    //    if (header->Flags == 1)
    //        break;
    //}
    // ���²��0�˿ڷ���
    U8* bufSend = que->data[i];
    APP_SndICI* ici = (APP_SndICI*)bufSend;
    ici->uDesIP[0] = desIP[0];
    ici->uDesIP[1] = desIP[1];
    APP_Header* header = (APP_Header*)&bufSend[iciLen];
    header->Acknowledgment_Number = ACK;
    header->Flags = 3;
    U8 No = header->Sequence_Number;
    cout << "\n.........................����ACK_PUSH����........................." << endl;
    cout << "\n����ACK_PUSH���� Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
    congestionSend(&send_buffer);
}

// ����ACK_MID
void sendACK_MID(U8 ACK, U8* desIP, struct send_queue* que) {
    int i = que->nxt;
    //for (i = send_buffer.nxt; i != send_buffer.size; i = (i + 1) % MAX_SEND) {
    //    APP_Header* header = (APP_Header*)&send_buffer.data[i][iciLen];
    //    if (header->Flags == 1)
    //        break;
    //}
    // ���²��0�˿ڷ���
    U8* bufSend = que->data[i];
    APP_SndICI* ici = (APP_SndICI*)bufSend;
    ici->uDesIP[0] = desIP[0];
    ici->uDesIP[1] = desIP[1];
    APP_Header* header = (APP_Header*)&bufSend[iciLen];
    header->Acknowledgment_Number = ACK;
    header->Flags = 17;
    U8 No = header->Sequence_Number;
    cout << "\n.........................����ACK_MID����........................." << endl;
    cout << "\n����ACK_MID���� Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
    congestionSend(&send_buffer);
}

// ����FIN
void sendFIN(struct send_queue* que, U8* desIP) {
    U8* bufSend;
    //len = (int)strlen(kbBuf) + 1; //�ַ�������и�������
    bufSend = (U8*)malloc(headerLen + iciLen);
    U8 No = U8(que->size % MAX_NO);
    if (bufSend != NULL) {
        addHeader(bufSend, desIP, No, lastRecvNo, 8, recvSensitivity - fNums);
        cout << "\n.........................����FIN����........................." << endl;
        cout << "\n����FIN���� Sequence_Number " << int(No) << endl << endl;
        enSendQueue(&send_buffer, bufSend, headerLen + iciLen);
        congestionSend(&send_buffer);
    }
}

// ����FIN_ACK
void sendFIN_ACK(U8 ACK, U8* desIP, struct send_queue* que) {
    endACK = que->front;
    U8* bufSend;
    bufSend = (U8*)malloc(headerLen + iciLen);
    U8 No = U8(send_buffer.size % MAX_NO);
    if (bufSend != NULL) {
        addHeader(bufSend, desIP, No, ACK, 10, recvSensitivity - fNums);
        cout << "\n.........................����FIN_ACK����........................." << endl;
        cout << "\n����FIN_ACK���� Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
        enSendQueue(&send_buffer, bufSend, headerLen + iciLen);
        congestionSend(&send_buffer);
    }
}




//----------------�����ķָ��ߣ���ʼ����صĺ���----------------
// ����ϵͳ�ĳ�ʼ�� �����������ݴ�ne.txt�Ķ�ȡ��Ԥ����
void InitFunction(CCfgFileParms& cfgParms)
{
    int i;
    int retval;
    uConnectIP[0] = 0;
    uConnectIP[1] = 0; // ��ʼ��
    uAutoIP[0] = 0;
    uAutoIP[1] = 0; // ��ʼ��
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
    strIP = cfgParms.getValueStr("IP"); // IP��ַ��ȡ
    cout << "IP��ַ ���� " << strIP << endl;
    outPath = "output-" + strIP + "/";
    if (autoSendBuf == NULL || sendBuf == NULL) {
        cout << "�ڴ治��" << endl;
        //����������Ҳ̫���˳���
        exit(0);
    }
    reportTime = RESEND_TIME;
    initqueue(&send_buffer, &recv_buffer);
}

// �˳�ϵͳ�Ĵ��� �����������ͷ�
void EndFunction()
{
    if (autoSendBuf != NULL)
        free(autoSendBuf);
    if (sendBuf != NULL)
        free(sendBuf);
}


//----------------�����ķָ��ߣ���Ҫ������ʱ����������----------------
// ������������ʱ�� ���ڶ�ʱϵͳ�Ĵ�������������
void TimeOut()
{

    int iSndRetval;
    ltime++;
    if (_kbhit()) {
        //�����ж���������˵�ģʽ
        menu();
    }
    if (ltime % (10 * autoSendTime) == 0)
        fNums = max(0, fNums - 3); // ģ�⴦��
     //δ����ӵ��ʱ�Ž��м���Ƿ�ʱ ����ӵ��ʱ �ȴ���һ֡���ط��ж����ӵ��ؽ�
    if (status == 0 && send_buffer.reTime[send_buffer.front] <= ltime && send_buffer.reTime[send_buffer.front] >= 0 && send_buffer.front != send_buffer.nxt)
    {
        cout << "��ʱ ӵ������\nque->front " << send_buffer.front << " que->nxt " << send_buffer.nxt << " que->rear " << send_buffer.rear << " que->size " << send_buffer.size << endl;
        cout << "uConnectIP " << uConnectIP << " status " << status << " send_buffer.retime[send_buffer.front] " << send_buffer.reTime[send_buffer.front] << endl;
        cout << "ltime " << ltime << endl;
        // ӵ������
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
        cout << "ӵ�������³�ʱ ��ʼӵ���ָ�" << endl;
        lResume(&send_buffer, int(lastRecvACK + 1));

    }
    if (uAutoIP[0]) { // ������Զ�����ģʽ
        if (!autoTime) {
            cout << endl << "�Զ����͸�" << uAutoIP[0] << "." << uAutoIP[1] << "���������" << endl;
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
                //cout << "��ʼ�Զ�����" << endl;
                //��ʱ����ǰ���жϻ������Ƿ��� �������ֹ�ط� 		�˴��Ȳ�����MSS ��ʵ�ַ��鼼�� Ҳ�����Ƕ�ʱ���¿�ʼ�Զ�����
                if (isfullque(&send_buffer)) {
                    cout << "||||||||||||||||||||||||||||�������Ѿ����� �ģ�|||||||||||||||||||||||||||||||||||||" << endl;
                }
                //��ʱ����, ÿ���autoSendTime * DEFAULT_TIMER_INTERVAL ms ����һ��
                //if (printCount % autoSendTime == 0) {
                else {
                    strcpy(autoSendBuf, data.c_str());
                    len = data.size(); //ÿ�η�������
                    congestionControl(autoSendBuf, len, uAutoIP, &send_buffer);
                }
            }
        }

    }
    if (uConnectIP[0]) { // ������豸������

        //cout << "ltime " << ltime << endl;
        if (endtime != -1 && ltime >= endtime) { // �ж��Ƿ���Ҫ�ر�����
            endtime = -1;
            uConnectIP[0] = 0; //�ر�ϵͳ
            cout << "\n-------------------------�ر�����-------------------------" << endl << endl;
            msgRecord[++lastRecord % 50] = u8"   -----------------------�ر�����-----------------------";
            return;
        }

        if (ltime % (autoSendTime + 10) == 0)
            // ӵ������ 
            congestionSend(&send_buffer);
    }


}


//------------�����ķָ��ߣ����������ݵ��շ�--------------------------------------------
// ���ϲ�������� ����Ӧ�ò㲻����ϲ�������ݵ���
void RecvfromUpper(U8* buf, int len) {
    return;
}

// �������������²�������� ���ڸ��ֱ��ĵ��ж��봦�� ����������
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
        cout << endl << "*************************���Ķ�ʧ*************************" << endl << endl;
        return; // α�챨�Ķ�ʧ����

    }
    APP_RcvICI* ici = (APP_RcvICI*)buf;
    APP_Header* header = (APP_Header*)&buf[iciLen];
    flag = header->Flags;
    No = header->Sequence_Number;
    ACK = header->Acknowledgment_Number;
    Rnums = header->Rnums;
    cout << "\n////////////////////////���յ�����////////////////////////\n\nԴIP��ַ���� " << ici->uSrcIP[0] << "." << ici->uSrcIP[1] << " ��־λ����flag " << int(flag) << " ���ȡ���len " << len << " ʣ����������Rnums " << int(Rnums) << endl;
    if (flag & 1 || flag & 16) // 1����PUSH 16����MID �������ݱ���
        fNums += 2;
    else
        fNums++;
    cout << "\n����֡���� " << fNums << endl;
    cout << "\n���յ�����������" << endl;
    if (len < 500) {
        print_data_bit(buf, len, 1);
        //cout << "buf[0] " << buf[0] << " int buf[0] " << int(buf[0]) << " len " << len << " strlen " << strlen(buf) << endl;
        print_data_byte(buf, len, 1);
    }
    else
        cout << endl << "�������ݳ���̫��������ʾ" << endl;

    ////�ж��ֽ����ݵ�һλ �ж������ݱ��Ļ���ACK���� 0ΪACK���� 1Ϊ���ݱ���
    addMsg(++lastRecord, 0, int(flag), int(No), int(ACK), ici->uSrcIP, len);
    switch (flag) {
    case 1: // PUSH����
        if (No != (lastRecvNo + 1) % MAX_NO) {
            cout << "*************************PUSH������ų���*************************" << endl;
            cout << "\n�յ����ݱ��Ĳ�����Ҫ��,���Ϊ " << int(No) << " Ҫ��Ϊ" << int(lastRecvNo + 1) % MAX_NO << endl;
            sendACK(lastRecvNo, uConnectIP);
            cout << "++++++++++++++++++++++++������մ���++++++++++++++++++++++++" << endl;
            sbuf = (U8*)malloc(len);
            strncpy(sbuf, buf, len);
            enRecvQueue(&recv_buffer, sbuf, int(No), len);
        }
        else {
            cout << "\n------------------------PUSH����------------------------" << endl << endl;
            originbuf = &buf[headerLen + iciLen];
            cout << "\n�յ���ȷ�����ݱ���,���Ϊ" << int(No) << endl;
            if (lastUpNo != lastRecvNo) { // �����������һ���Ϸ��Ĵ���
                cout << "++++++++++++++++++++++++�ָ�����һ�������뻺����++++++++++++++++++++++++" << endl;
                addMidBuffer(mid_buffer, originbuf, int(flag), len - iciLen - headerLen);
                lastRecvNo = No;
                lastUpNo = No;
            }
            else {
                lastRecvNo = No;
                lastUpNo = No;
                if (originbuf[0] != 1) {
                    if (len < 500) {
                        cout << "\nȥ��ͷ���������" << endl;
                        print_data_byte(originbuf, len - headerLen - iciLen, 1);
                    }
                    writeData(originbuf, len - headerLen - iciLen);
                }
                else if (len < 500)
                {
                    cout << "\nȥ��ͷ���������" << endl;
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
        //cout << "\n�յ�ACK�����Ϊ" << int(No) << endl;
        // �жϵõ�����ACK����
        // ���ACK����һ��ACK���һ�� ���ظ�
        if (ACK == lastRecvACK) {
            cout << "*************************ACK��������ظ�*************************" << endl;
            cout << "\n�յ����ϴ�һ����ACK�����Ϊ" << int(ACK) << " ���ִ��� " << ++noNums << endl;
            if (status == 0 && noNums == 2)
                // ӵ������
                congestionHappen(&send_buffer, lastRecvACK + 2);
            // ������ָ�
            if (noNums == 4)
                qResume(&send_buffer, int(lastRecvACK + 1));
        }
        else {
            cout << "\n------------------------ACK����------------------------" << endl;
            // ˵�����ݽ��ճɹ�
            if (ACK == lastRecvACK + 1)
                cout << "\n�յ���ȷ��ACK����,���Ϊ" << int(ACK) << endl;
            else if ((ACK + MAX_SEND - lastRecvACK) % MAX_SEND < 10) // ������Ϊ10�� ����Ĭ����ȡ
                cout << "\nû�յ�Ԥ��ACK " << lastRecvACK + 1 << " ���յ�ACK���Ϊ " << int(ACK) << "��Ϊǰһ����Ҳ�յ�\n" << endl;
            else
            {
                cout << "\n�յ������ACK����,���Ϊ" << int(ACK) << endl;
                return;
            }
            if (status == 1) { //ӵ��״̬�յ���ȷACK���������Զ�����
                //iWorkMode = 10 + iWorkMode % 10; // �����Զ�����
                status = 0; // ���ӵ������
                if (uAutoIP[1])
                    uAutoIP[0] = save; // ������Զ�����ģʽ ���������Զ�����
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
                send_buffer.reTime[i % MAX_SEND] = -2; //ȡ����ʱ�ش�����
            }
            //if (status == 1 && send_buffer.front == send_buffer.nxt) {
            //	send_buffer.nxt = min(now + 1, send_buffer.rear);
            //}
            if (send_buffer.reTime[send_buffer.front] == -2)
                for (int i = send_buffer.front; send_buffer.reTime[i] == -2 && i != send_buffer.nxt; i = (i + 1) % MAX_SEND)
                {
                    //cout << ""
                    deSendQueue(&send_buffer); // ���ȷ�ϵ��Ƕ��е�front֡ �����
                    //if (!status) // ��ӵ������״̬��Ҫ�ȴ���ָ� ����ֱ�Ӹı䴰��
                    changeWin(&send_buffer);
                }
            if (ACK == endACK) {
                endACK = 255;
                uConnectIP[0] = 0; //�ر�ϵͳ
                cout << "\n-------------------------�ر�����-------------------------" << endl << endl;
                msgRecord[++lastRecord % 50] = u8"   -----------------------�ر�����-----------------------";
                return;
            }
            lastRecvACK = ACK;
        }
        break;
    case 3:
        if (ACK == send_buffer.front % MAX_NO) {

            deSendQueue(&send_buffer); // ���ȷ�ϵ��Ƕ��е�front֡ �����
            send_buffer.reTime[send_buffer.front % MAX_SEND] = -2; //ȡ����ʱ�ش�����
            changeWin(&send_buffer);
            cout << "\n------------------------ACK_PUSH����------------------------" << endl << "Acknowledgment_Number " << int(ACK) << endl << endl;
            uConnectIP[0] = ici->uSrcIP[0];
            uConnectIP[1] = ici->uSrcIP[1];
            cout << "++++++++++++++++++++++++��������++++++++++++++++++++++++" << endl << "���ӵ�IP����" << uConnectIP[0] << "." << uConnectIP[1] << endl << endl;;
            msgRecord[++lastRecord % 50] = u8"   -----------------------��������-----------------------";
            lastRecvACK = ACK;
            originbuf = &buf[headerLen + iciLen];
            cout << "\n�յ���ȷ�����ݱ���,���Ϊ" << int(No) << endl;
            lastRecvNo = No;
            lastUpNo = No;
            if (originbuf[0] != 1) {
                if (len < 500) {
                    cout << "\nȥ��ͷ���������" << endl;
                    print_data_byte(originbuf, len - headerLen - iciLen, 1);
                }

                writeData(originbuf, len - headerLen - iciLen);
            }
            else if (len < 500)
            {
                cout << "\nȥ��ͷ���������" << endl;
                print_data_byte(&originbuf[1], len - headerLen - iciLen - 1, 1);

            }
            lastRecvNo = No;
            lastUpNo = No;
            //WriteData("output.txt", originbuf, len - 3);
            //base64toImg(originbuf);

            sendACK(No, uConnectIP);
        }
        else
            cout << "*************************ACK_PUSH����ACK����*************************" << endl;
        break;
    case 4:
        if (uConnectIP[0] == 0) {
            cout << "\n------------------------SYN����------------------------" << endl << "Sequence_Number " << int(No) << endl << endl;
            lastRecvNo = No;
            lastUpNo = No;
            sendSYN_ACK(No, ici->uSrcIP, &send_buffer);
        }
        else {
            cout << "\n\nĿǰ��IP " << uConnectIP << " �������ӣ��޷���������IP��SYN���� ���ȶϿ�����" << endl << endl;
        }
        break;

    case 6:
        if (uConnectIP[0] == 0) {
            cout << "\n------------------------SYN_ACK����------------------------" << endl << "Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
            if (ACK == send_buffer.front % MAX_NO) {
                lastRecvNo = No;
                lastUpNo = No;
                send_buffer.reTime[send_buffer.front % MAX_SEND] = -2; //ȡ����ʱ�ش�����
                lastRecvACK = ACK;
                deSendQueue(&send_buffer); // ���ȷ�ϵ��Ƕ��е�front֡ �����
                changeWin(&send_buffer);
                uConnectIP[0] = ici->uSrcIP[0];
                uConnectIP[1] = ici->uSrcIP[1];
                cout << "++++++++++++++++++++++++��������++++++++++++++++++++++++" << endl << "���ӵ�IP����" << uConnectIP[0] << "." << uConnectIP[1] << endl << endl;
                msgRecord[++lastRecord % 50] = u8"   -----------------------��������-----------------------";
                header = (APP_Header*)&send_buffer.data[send_buffer.nxt][iciLen];
                cout << (int)header->Flags << endl;
                if (header->Flags == 1)
                    sendACK_DATA(No, uConnectIP, &send_buffer);
                else if (header->Flags == 16)
                    sendACK_MID(No, uConnectIP, &send_buffer);
            }
            else
                cout << "*************************SYN_ACK����ACK����*************************" << endl;
        }
        else
            cout << "*************************�յ���������SYN_ACK����*************************" << endl;
        break;
    case 8:
        if (uConnectIP[0] == 0) {
            cout << "\n------------------------FIN����------------------------" << endl << "Sequence_Number " << int(No) << endl;
            sendACK(No, uConnectIP);
            lastRecvNo = No;
            sendFIN_ACK(No, uConnectIP, &send_buffer);
        }
        break;
    case 10:
        cout << "\n------------------------FIN_ACK����------------------------" << endl << "Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
        if (int(ACK) == (send_buffer.front + 128 - 1) % MAX_NO) {
            endtime = ltime + 50;
            lastRecvNo = No;
            //cout << endtime << endl;
            sendACK(No, uConnectIP);
        }
        break;
    case 16:
        cout << "\n------------------------MID����------------------------" << endl << "Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
        if (No != (lastRecvNo + 1) % MAX_NO) {
            cout << "*************************MID������ų���*************************" << endl;
            cout << "++++++++++++++++++++++++������մ���++++++++++++++++++++++++" << endl;
            sbuf = (U8*)malloc(len);
            strncpy(sbuf, buf, len);
            lastRecvNo = No;
            enRecvQueue(&recv_buffer, sbuf, int(No), len);
        }
        else {
            cout << "++++++++++++++++++++++++�����Ϸ�������++++++++++++++++++++++++" << endl;
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
            deSendQueue(&send_buffer); // ���ȷ�ϵ��Ƕ��е�front֡ �����
            send_buffer.reTime[send_buffer.front % MAX_SEND] = -2; //ȡ����ʱ�ش�����
            changeWin(&send_buffer);
            cout << "\n------------------------ACK_MID����------------------------" << endl << "Acknowledgment_Number " << int(ACK) << endl << endl;
            uConnectIP[0] = ici->uSrcIP[0];
            uConnectIP[1] = ici->uSrcIP[1];
            cout << "++++++++++++++++++++++++��������++++++++++++++++++++++++" << endl << "���ӵ�IP����" << uConnectIP[0] << "." << uConnectIP[1] << endl << endl;;
            msgRecord[++lastRecord % 50] = u8"   -----------------------��������-----------------------";
            lastRecvACK = ACK;
            originbuf = &buf[headerLen + iciLen];
            cout << "++++++++++++++++++++++++�����Ϸ�������++++++++++++++++++++++++" << endl;
            addMidBuffer(mid_buffer, &buf[iciLen + headerLen], int(flag), len - iciLen - headerLen);
            lastRecvNo = No;
            //WriteData("output.txt", originbuf, len - 3);
            //base64toImg(originbuf);

            sendACK(No, uConnectIP);
        }
        else
            cout << "*************************ACK_MID����ACK����*************************" << endl;
        break;
    default:
        cout << "\n������ �ģ�" << endl;
        cout << "\n��������" << endl;
        print_data_bit(buf, len, 1);
        break;
    }

    dataRecord(len, 0);
    cout << endl << "======================================================================" << endl << endl;

}


//------------�����ķָ��ߣ������Եĺ���--------------------------------------------
// ���ͽ������ݵļ�¼  flag ���� 0Ϊ�������� 1Ϊ��������
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

// ��ӡ��ǰ���ͽ������ݵ���Ϣ ���� ���ͽ������ݵĴ�����λ�����������ݵĴ���
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
    cout << "������ " << iSndTotal << " λ," << iSndTotalCount << " ��," << "���� " << iSndErrorCount << " �δ���;";
    cout << " ������ " << iRcvTotal << " λ," << iRcvTotalCount << " ��," << "��ǰʱ��" << ltime << endl;
    spin++;

}

// ���ڴ����ȡ���ļ����ݻ��ı����ݣ�������
void  SendData(U8* data, string des, int len, int flag) {
    char desIP[2];

    if (isfullque(&send_buffer)) {
        cout << endl << "��������������ʱ�޷�����";
        return;
    }
    setIP(desIP, des.c_str());
    if (uConnectIP[0] != 0 && strncmp(desIP, uConnectIP, 2))
    {
        string a;
        a = a + u8" Ŀǰ��IP " + uConnectIP[0] + "." + uConnectIP[1] + u8" �������ӣ��޷�������IP����  ���ȶϿ�����";
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
            cout << endl << "�޿ɷ��͵�����" << endl << endl;
        }
    }


}

// �Ͽ����� 
void breakConnection() {
    if (uConnectIP[0] == 0)
    {
        msgRecord[++lastRecord % 50] = u8"  �Ͽ�����ʧ�ܣ�Ŀǰû�н�������";
        cout << endl << "Ŀǰû�н�������" << endl << endl;
    }
    else
        sendFIN(&send_buffer, uConnectIP);
}

// �˵����� ��������ʵ�ֹ���
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
    //����|��ӡ��[���Ϳ��ƣ�0���ȴ��������룻1���Զ���][��ӡ���ƣ�0�������ڴ�ӡͳ����Ϣ��1����bit����ӡ���ݣ�2���ֽ�����ӡ����]
    cout << endl << endl << "�豸��:" << strDevID << ",    ���:" << strLayer << ",    ʵ���:" << strEntity;
    cout << endl << "1-�����Զ�����;" << endl << "2-ֹͣ�Զ�����; " << endl << "3-�����ļ�; ";
    cout << endl << "4-���͵����ı�; " << endl << "5-�ر�����;" << endl << "6-��ӡ��־;" << endl << "7-����ָ����ű���(���մ��ڵ���);";
    cout << endl << "0-ȡ��" << endl << "����������ѡ�����";
    cin >> selection;
    getchar();
    switch (selection) {
    case 0:
        break;
    case 1:
        cout << "������Ҫ�Զ����͵�IP��";
        cin >> des;
        cout << "������Ҫ�Զ����͵Ĵ�����";
        cin >> autoTime;
        if (!deleteMark(des, ".")) {
            cout << endl << "����������������ȷ��IP��ַ��������" << endl;
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
            cout << endl << "��������������ʱ�޷�����";
            break;
        }
        cout << "������Ҫ���͵��ļ���";
        cin >> file;
        cout << "������Ҫ���͵�IP��";
        cin >> des;
        if (!deleteMark(des, ".")) {
            cout << endl << "����������������ȷ��IP��ַ��������" << endl;
            break;
        }
        rlen = readData(file, sendBuf);
        if (rlen == -1) {
            cout << endl << "������������ȷ�����ļ����ƣ�������" << endl;
            break;
        }
        bufSend = (U8*)malloc(rlen);
        strcpy(bufSend, sendBuf);
        SendData(bufSend, des, rlen, 0);
        break;
    case 4:
        cout << "������Ҫ���͵��ı���";
        getline(cin, kbBuf);
        cout << "������Ҫ���͵�IP��";
        cin >> des;
        if (!deleteMark(des, ".")) {
            cout << endl << "����������������ȷ��IP��ַ��������" << endl;
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
        cout << "������Ҫ���͵�IP��";
        cin >> des;
        if (!deleteMark(des, ".")) {
            cout << endl << "����������������ȷ��IP��ַ��������" << endl;
            break;
        }
        char desIP[2];
        desIP[0] = des.c_str()[0];
        desIP[1] = des.c_str()[1];
        cout << "������Ҫ���͵���ţ�";
        cin >> no;

        bufSend = (U8*)malloc(iciLen + headerLen);
        addHeader(bufSend, desIP, U8(no) - '0', lastRecvNo, 1, recvSensitivity - fNums);
        print_data_byte(bufSend, iciLen + headerLen, 1);
        SendtoLower(bufSend, iciLen + headerLen, 0);
        break;
    default:
        cout << "����ȷ����" << endl << endl;
        break;
    }

}

// ��������־������Ϣ mode ���� 0��ʾ�������� 1��ʾ���� ����
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
        msg.append(u8" ACK �� "); msg.append(to_string(ack));
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

// ���ڴ��ڶ�ȡ��ǰ��־����Ϣ ����������õ�
string getStr(int index) {
    if (lastRecord < 50)
        return msgRecord[index];
    else
        return msgRecord[(lastRecord % 49 + index) % 50];

}

// ɾ��string��ָ���ķ��� ��������ȥ��IP��ַ�е�.
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

// ���ڴ��ڶ�ȡ��ǰ���ӵ�IP ����������õ�
string getIP() {
    if (uConnectIP[0] == 0)
        //return " Now Links:  None";
        return u8" ��ǰ�����豸IP��ַ:  ��";
    string a;
    a = a + u8" ��ǰ�����豸IP��ַ:  " + uConnectIP[0] + '.' + uConnectIP[1];
    //a = a + " Now Links:  " + uConnectIP[0] + '.' + uConnectIP[1];
    return a;
}

// ���ڴ���ѡ��Ҫ���͵��ļ�������
void  selectFile(U8* file, string des) {
    U8* bufSend;
    int rlen = readData(file, sendBuf);
    if (rlen == -1) {
        msgRecord[++lastRecord % 50] = u8"   ����������������ȷ���ļ����ƣ�������";
        //msgRecord[++lastRecord % 50] = "  !!!!Please input correct file name!!!! ";
        return;
    }
    if (!deleteMark(des, ".")) {
        msgRecord[++lastRecord % 50] = u8"   ����������������ȷ��IP��ַ��������!";
        //msgRecord[++lastRecord % 50] = u8"  !!!!Please input correct IP!!!! ";
        return;
    }
    //string a = u8"  Send Data -- ";
    string a = u8"  �����ļ� -- ";
    a.append(file);
    msgRecord[++lastRecord % 50] = a;
    bufSend = (U8*)malloc(rlen);
    strcpy(bufSend, sendBuf);
    SendData(bufSend, des, rlen, 0);
}

// ��������IP��ַ
void setIP(U8* nIP, const U8* sIP) {
    nIP[0] = sIP[0];
    nIP[1] = sIP[1];
}
