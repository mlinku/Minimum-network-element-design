//apptester�Ĺ����ļ�
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
U8* autoSendBuf;  //������֯�������ݵĻ��棬��СΪMAX_BUFFER_SIZE,���������������������ƣ��γ��ʺϵĽṹ��������û��ʹ�ã�ֻ������һ��
U8* SendBuf;
int ltime = 0; //ȫ��ʱ�����ڴ���ʱ�ش�
int endtime = -1;
int spin = 1;  //��ӡ��̬��Ϣ����
int z = 1;
//------�����ķָ��ߣ�һЩͳ���õ�ȫ�ֱ���------------
int iSndTotal = 0;  //������������
int iSndTotalCount = 0; //���������ܴ���
int iSndErrorCount = 0;  //���ʹ������
int iRcvTotal = 0;     //������������
int iRcvTotalCount = 0; //ת�������ܴ���
int iRcvUnknownCount = 0;  //�յ�������Դ�����ܴ���
int cwnd = 1; //ӵ�����ڴ�С 
int ssthresh = 8; //����������
int reportTime = 500; // ��ʱ�ش���ʱ��
int ACKlen = 4; // ACK���ݳ���Ϊ4
int SYN_ACKlen = 4; //SYN_ACK����Ϊ4
int SYNlen = 4; // SYN����Ϊ4
int FINlen = 4; // FIN����Ϊ4
int FIN_ACKlen = 4; // SYN_ACK����Ϊ4
int winacc = 0; // ��¼�յ���֡��Ŀ������ӵ������
U8 lastNo = 255; // ǰһ���յ����ĵ����
U8 lastACK = 255; //ǰһ������ACK���
U8 lastSeq = 0;
int noNums = 0; //��ʼ����ͬһ���ACK�յ��Ĵ���
int status = 0; //�ж�ӵ���Ƿ��� �������Ϊ1 δ����Ϊ0 
int isstart = 0; //�����ж�ϵͳ�Ƿ�ʼ����
int cnums = 0; //�����ж���ž�������ѭ��
int fnums = 0; // ��λʱ�����յ���֡��Ŀ
int headerLen = 4; // ͷ������
int isautoend = 0; // �ж��Ƿ��Զ�ֹͣ
U8 endACK = 255;


// ���ʹ���
struct send_queue { // ��ָ�뻺������� ���ڱض���Ȼ�����С
    int front; // ָ���ѷ��͵���δ�յ�ȷ�ϵĵ�һ���ֽ����к�
    int nxt;  //ָ��ɷ���δ���͵ĵ�һ���ֽ����к�
    int rear; //ָ�򲻿ɷ���δ���͵ĵ�һ���ֽ����к� ����
    int size; // ָ�򻺳��������һ������		
    U8* data[MAX_QUE]; // ���������ݼ���
    int len[MAX_QUE]; // ������ÿ�����ݵĳ���
    int retime[MAX_QUE];  // ���������ݵĳ�ʱ�ط���¼
};
// APP�㱨��ͷ��
struct APP_Header {
    char Sequence_Number;  // ���͵ı������к� 
    char Acknowledgment_Number; // ȷ���յ��ı������к� ֻ�б�־λACKΪ1ʱ��Ч
    char Flags; // ��־λ 
    char Rnums; // ʣ�����֡���� 
};
// ��ʼ���ṹ�����
struct send_queue my_buffer;
// ���ļ��ڶ�ȡ����
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
// ���ļ�д������
int WriteData(const U8* file, const U8* data, int len) {
    ofstream outfile(file, ios::ate); //ios::ate����ֱ�Ӱ�ָ��ŵ��ļ�ĩβ��ios::app�����ã�Ҫ���outfile.seekp(0,ios::end)���ܰ��ļ�Ū���ļ�ĩβ��Ĭ�������ļ�ͷ
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
// Ϊ��������ͷ��
void AddHeader(U8* bufSend, U8 seq, U8 ack, U8 flags, U8 rnums) {
    APP_Header* header = (APP_Header*)bufSend;
    header->Sequence_Number = seq;
    header->Acknowledgment_Number = ack;
    header->Flags = flags;
    header->Rnums = rnums;
}
// ��Ӻ���
int enqueue(struct send_queue* que, U8* buf, int len)
{
    // ������жϻ�����
    if ((que->size + 1) % MAX_QUE == que->front)
        return -1;
    que->data[que->size] = buf;
    que->len[que->size] = len;
    que->retime[que->size] = -1;
    que->size = (que->size + 1) % MAX_QUE;
    return 0;
}
// ����FIN
void SendFIN(struct send_queue* que) {
    U8* bufSend;
    //len = (int)strlen(kbBuf) + 1; //�ַ�������и�������
    bufSend = (U8*)malloc(ACKlen);
    U8 No = U8(que->size % MAX_NO);
    if (bufSend != NULL) {
        AddHeader(bufSend, No, lastNo, 8, recvSensitivity - fnums);
        cout << "\n.........................����FIN����........................." << endl;
        cout << "\n����FIN���� Sequence_Number " << int(No) << endl << endl;
        enqueue(&my_buffer, bufSend, FINlen);
        CongestionSend(&my_buffer);
    }
}
// ���Ӻ��� �����ƶ�
U8* dequeue(struct send_queue* que)
{
    //cout << "����ǰ" << endl;
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
    ////cout << "���Ӻ�" << endl;
    //cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl;
    return buf;
}
// ��ʼ������
void initqueue(struct send_queue* que)
{
    que->front = que->nxt = que->size = 0;
    que->rear = cwnd;
}
// �ͷŶ���
void freequeue(struct send_queue* que)
{
    int i;
    for (i = que->front; i != que->size; i = (i + 1) % MAX_QUE)
        free(que->data[i]);

}
// �ж϶����Ƿ���
bool isfullque(struct send_queue* que) {
    if ((que->size + 1) % MAX_QUE == que->front)
        return true;
    return false;
}
// �жϴ����Ƿ���
bool isfullwin(struct send_queue* que) {
    if (que->nxt == que->rear)
        return true;
    return false;
}
// ��������ӵ������Ĵ��ڵ���
int ChangeWin(struct send_queue* que) {
    if (cwnd + 2 >= MAX_QUE)
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
        que->rear = (que->rear + 1) % MAX_QUE;
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
            que->rear = (que->rear + 1) % MAX_QUE;
            cout << "ӵ�����ⴰ�ڸı��" << endl;
            cout << "cwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
            return 0;
        }
        cout << endl << "ӵ������ ���ڻ�û��" << " winacc " << winacc << " cwnd " << cwnd << endl << endl;
        return 1;

    }

}

//***************��Ҫ��������******************************
//���ƣ�InitFunction
//���ܣ���ʼ�������棬��main�����ڶ��������ļ�����ʽ������������ǰ����
//���룺
//�����
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
        cout << "�ڴ治��" << endl;
        //����������Ҳ̫���˳���
        exit(0);
    }
    initqueue(&my_buffer);
    for (i = 0; i < MAX_BUFFER_SIZE; i++) {
        autoSendBuf[i] = 'a'; //��ʼ������ȫΪ�ַ�'a',ֻ��Ϊ�˲���
    }
    return;
}
//***************��Ҫ��������******************************
//���ƣ�EndFunction
//���ܣ����������棬��main�������յ�exit������������˳�ǰ����
//���룺
//�����
void EndFunction()
{
    if (autoSendBuf != NULL)
        free(autoSendBuf);
    return;
    freequeue(&my_buffer);
}
// ʵ��ӵ������ �ܽ����������˵����ʱ�����ܽ��뻺���� �ȴ����ڵ�����з��� �²�̶���������
void  CongestionControl(U8* buf, int len, struct send_queue* que) // ifNo
{
    // ����Ӧ�ò㷢�͵����ݲ������
    //cout << "����ǰ��������" << endl;
    //print_data_bit(buf, len, 1);
    //cout << "����ǰ�ֽ�����" << endl;
    //print_data_byte(buf, len, 1);
    U8* abufSend;
    len += headerLen;

    abufSend = (U8*)malloc(len);
    if (abufSend != NULL) {
        AddHeader(abufSend, U8(que->size % MAX_NO), lastNo, 1, recvSensitivity - fnums);
        //cout << "abufSend[1] " << int(abufSend[1]) << endl;
        // ���ݼ��뻺����
        strcpy(&abufSend[headerLen], buf);
        enqueue(&my_buffer, abufSend, len);
    }

}
//�ط����ݲ����ó�ʱ�ط���ʱ��
void ReSend(struct send_queue* que, int i) {
    que->retime[i] = ltime + reportTime;
    APP_Header* header = (APP_Header*)que->data[i];
    header->Rnums = recvSensitivity - fnums;
    int iSndRetval = SendtoLower(que->data[i], que->len[i], 0);
    //cout << "�ط� zhen Ϊ " << i << "�ı���" << endl;
    cout << "�ط����ġ��� �������Ϊ" << int(header->Sequence_Number) << " �ı��� ltime " << ltime << " retime  " << que->retime[i] << " que->front " << que->front << endl << endl;
    //cout << "�������Ϊ" << "�ı���" << endl;
    //cout << "que->len[i] " << que->len[i] << endl;
    cout << "\n�ط���������" << endl;
    print_data_byte(que->data[i], que->len[i], 1);
    print_data_bit(que->data[i], que->len[i], 1);
    cout << endl << "~~~~~~~~~~~~~~~~~~~�ط����ķ������~~~~~~~~~~~~~~~~~~~~~~" << endl << endl;
    if (iSndRetval > 0) {
        iSndTotalCount++;
        iSndTotal += que->len[i];
    }
    else {
        iSndErrorCount++;
    }
}
// �����ش����� 
void QResume(struct send_queue* que, int No) {
    cout << "���������ش�" << endl;
    cout << "**********************��ʼ���ش�********************************" << endl;
    //noNums = 0;
    cout << "���ش��ı�ǰ" << endl;
    cout << "cwnd " << cwnd << endl;
    cwnd = ssthresh + 3;
    que->rear = que->front + cwnd;  // ���ڱ仯
    status = 0;
    cout << "���ش��ı��" << endl;
    cout << "cwnd " << cwnd << " No " << No << endl;
    ReSend(&my_buffer, No); // �ط�������֡
    iWorkMode = 10 + iWorkMode % 10; // �����Զ�����
}
// ӵ�����ָ����� 
void LResume(struct send_queue* que, int No) {
    cout << "����ӵ���ָ�" << endl;
    cout << "**********************��ʼӵ���ָ�********************************" << endl;
    //noNums = 0;
    cout << "ӵ���ָ��ı�ǰ" << endl;
    cout << "cwnd " << cwnd << endl << endl;
    cwnd = 1;
    que->rear = que->front + cwnd;  // ���ڱ仯
    status = 0;
    cout << "ӵ���ָ��ı��" << endl;
    cout << "cwnd " << cwnd << " No " << No << endl << endl;
    ReSend(&my_buffer, No); // �ط�������֡
    iWorkMode = 10 + iWorkMode % 10; // �����Զ�����
}
// ����ACK
void SendACK(U8 ACK) {
    U8* bufSend;
    string des;
    char a[10];
    //len = (int)strlen(kbBuf) + 1; //�ַ�������и�������
    bufSend = (U8*)malloc(ACKlen);
    if (bufSend != NULL) {
        AddHeader(bufSend, 255, ACK, 2, recvSensitivity - fnums);
        cout << "\n.........................����ACK����........................." << endl;
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
        // ���ݼ�¼
        if (iSndRetval > 0) {
            iSndTotalCount++;
            iSndTotal += ACKlen * 8;
        }
        else {
            iSndErrorCount++;
        }
    }
    cout << "\n����ACK Acknowledgment_Number " << int(ACK) << endl;
}
// ����SYN
void SendSYN(U8 SYN, struct send_queue* que) {
    U8* bufSend;
    bufSend = (U8*)malloc(SYN_ACKlen);
    if (bufSend != NULL) {
        AddHeader(bufSend, SYN, lastNo, 4, recvSensitivity - fnums);
        cout << "\n.........................����SYN����........................." << endl;
        cout << "\n����SYN���� Sequence_Number " << int(SYN) << endl << endl;
        enqueue(&my_buffer, bufSend, SYNlen);
        CongestionSend(&my_buffer);
    }
}
// ����SYN_ACK
void SendSYN_ACK(U8 ACK, struct send_queue* que) {
    U8* bufSend;
    bufSend = (U8*)malloc(SYN_ACKlen);
    U8 No = U8(my_buffer.size % MAX_NO);
    if (bufSend != NULL) {
        AddHeader(bufSend, No, ACK, 6, recvSensitivity - fnums);
        cout << "\n.........................����SYN_ACK����........................." << endl;
        cout << "\n����SYN_ACK���� Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
        enqueue(&my_buffer, bufSend, SYN_ACKlen);
        CongestionSend(&my_buffer);
    }
}
// ����ACK_PUSH
void SendACK_DATA(U8 ACK, struct send_queue* que) {
    int i = que->nxt;
    // ���²��0�˿ڷ���
    U8* bufSend = que->data[i];
    APP_Header* header = (APP_Header*)bufSend;
    header->Acknowledgment_Number = ACK;
    header->Flags = 3;
    U8 No = header->Sequence_Number;
    cout << "\n.........................����ACK_PUSH����........................." << endl;
    cout << "\n����ACK_PUSH���� Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
    CongestionSend(&my_buffer);
}

// ����FIN_ACK
void SendFIN_ACK(U8 ACK, struct send_queue* que) {
    endACK = que->front;
    U8* bufSend;
    bufSend = (U8*)malloc(FIN_ACKlen);
    U8 No = U8(my_buffer.size % MAX_NO);
    if (bufSend != NULL) {
        AddHeader(bufSend, No, ACK, 10, recvSensitivity - fnums);
        cout << "\n.........................����FIN_ACK����........................." << endl;
        cout << "\n����FIN_ACK���� Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
        enqueue(&my_buffer, bufSend, FIN_ACKlen);
        CongestionSend(&my_buffer);
    }
}
// ��������
void CongestionSend(struct send_queue* que) {
    cout << endl << endl << "======================================================================" << endl;

    // ���nxt��rearָ�벻�غ� ˵�����ڿɷ�δ����ָ�� �����пɷ�δ����ȫ�����ͳ�ȥ
    //if ((que->nxt + MAX_QUE - que->front) >= (que->rear + MAX_QUE - que->front)) //������ڸı䵼��rear��nxt֮ǰ ��ֹͣ����
    //	return;
    // ���nxt��rearָ�벻�غ� ˵�����ڿɷ�δ����ָ�� �����пɷ�δ����ȫ�����ͳ�ȥ
    if (status) {
        cout << "\n ӵ������ ��˲������������� " << endl;
        cout << " ltime " << ltime << " �ȴ���ʱ ��ʱʱ�� " << my_buffer.retime[my_buffer.front] << endl << endl;
        return;
    }
    if (que->nxt != que->rear && que->nxt != que->size) {
        // ��nxt��ʼ�ƶ� ���������rear��size��ֹͣ
        for (int i = que->nxt; i != que->rear && i != que->size; i = (i + 1) % MAX_QUE)
        {
            // ���ó�ʱ�ش���ʱ��
            que->retime[i] = ltime + reportTime;
            APP_Header* header = (APP_Header*)que->data[i];
            header->Rnums = recvSensitivity - fnums;
            // ���²��0�˿ڷ���
            int iSndRetval = SendtoLower(que->data[i], que->len[i], 0);
            //�Զ���ӡ����
            cout << "\n'''''''''''''''''''''''''��������'''''''''''''''''''''''''\n\n�������Ϊ " << int(header->Sequence_Number) << " ����Ϊ " << que->len[i] << " �ı���" << endl;
            print_data_byte(que->data[i], que->len[i], 1);
            print_data_bit(que->data[i], que->len[i], 1);
            cout << endl << "~~~~~~~~~~~~~~~~~~~~~~~~�������~~~~~~~~~~~~~~~~~~~~~~~~" << endl << endl;
            // nxtָ���ƶ�
            que->nxt = (que->nxt + 1) % MAX_QUE;
            // ���ݼ�¼
            if (iSndRetval > 0) {
                iSndTotalCount++;
                iSndTotal += que->len[i] * 8;
            }
            else {
                iSndErrorCount++;
            }
            //free(que->data[i]);

            cout << "��������ֹͣ �ѷ���\ncwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << endl << endl;
        }
    }

    else if (que->front != que->size)
        cout << "\n����δ����\ncwnd " << cwnd << " que->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << " ltime " << ltime << " �ȴ���ʱ ��ʱʱ��  " << my_buffer.retime[my_buffer.front] << endl << endl;
    else
        cout << "\n�ȴ���һ������" << endl << endl;
    cout << endl << "======================================================================" << endl << endl;

}
// ӵ������
void CongestionHappen(struct send_queue* que, int No) {
    cout << "����ӵ������" << endl;
    cout << "ӵ������ǰ \nque->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << " cwnd " << cwnd << endl << endl;
    status = 1; // ����ӵ������
    iWorkMode = iWorkMode % 10; // �ر��Զ�����
    cwnd = max(cwnd / 2, 1); // �������ڴ�С
    ssthresh = max(cwnd, 6); // ��������������
    while (No < que->front) {
        No += MAX_NO;
    }
    que->nxt = No;  // ����N֡���� ֱ�ӴӶ�ʧ֡��ʼȫ���ط�
    que->rear = que->front + cwnd;  // ���ڱ仯
    cout << "ӵ�������� \nque->front " << que->front << " que->nxt " << que->nxt << " que->rear " << que->rear << " que->size " << que->size << " cwnd " << cwnd << " ssthresh " << ssthresh << endl << endl;
}

//***************��Ҫ��������******************************
//���ƣ�TimeOut
//���ܣ�������������ʱ����ζ��sBasicTimer�����õĳ�ʱʱ�䵽�ˣ�
//      �������ݿ���ȫ���滻Ϊ������Լ����뷨
//      ������ʵ���˼���ͬʱ���й��ܣ����ο�
//      1)����iWorkMode����ģʽ���ж��Ƿ񽫼�����������ݷ��ͣ������Զ����͡������������ʵ��Ӧ�ò��
//        ��Ϊscanf�����������¼�ʱ���ڵȴ����̵�ʱ����ȫʧЧ������ʹ��_kbhit()������������ϵ��ڼ�ʱ�Ŀ������жϼ���״̬�������Get��û��
//      2)����ˢ�´�ӡ����ͳ��ֵ��ͨ����ӡ���Ʒ��Ŀ��ƣ�����ʼ�ձ�����ͬһ�д�ӡ��Get��
//      3)�����iWorkMode����Ϊ�Զ����ͣ���ÿ����autoSendTime * DEFAULT_TIMER_INTERVAL ms����ӿ�0����һ��
//���룺ʱ�䵽�˾ʹ�����ֻ��ͨ��ȫ�ֱ�����������
//���������Ǹ�����Ŭ���ɻ����ʵ����
void TimeOut()
{
    int iSndRetval;
    ltime++;
    if (_kbhit()) {
        //�����ж���������˵�ģʽ
        menu();
    }
    if (ltime % (10 * autoSendTime) == 0)
        fnums = max(0, fnums - 3); // ģ�⴦��
    if (isstart) {
        //cout << "ltime " << ltime << endl;
        if (endtime != -1 && ltime >= endtime) {
            endtime = -1;
            isstart = 0; //�ر�ϵͳ
            cout << "\n-------------------------�ر�����-------------------------" << endl << endl;
            return;
        }
        //δ����ӵ��ʱ�Ž��м���Ƿ�ʱ ����ӵ��ʱ �ȴ���һ֡���ط��ж����ӵ��ؽ�
        if (status == 0 && my_buffer.retime[my_buffer.front] <= ltime && my_buffer.retime[my_buffer.front] >= 0 && my_buffer.front != my_buffer.nxt)
        {
            cout << "��ʱ ӵ������\nque->front " << my_buffer.front << " que->nxt " << my_buffer.nxt << " que->rear " << my_buffer.rear << " que->size " << my_buffer.size << endl;
            cout << "isstart " << isstart << " status " << status << " my_buffer.retime[my_buffer.front] " << my_buffer.retime[my_buffer.front] << endl;
            cout << "ltime " << ltime << endl;
            // ӵ������
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
            cout << "ӵ�������³�ʱ ��ʼӵ���ָ�" << endl;
            LResume(&my_buffer, int(lastACK + 1));

        }
        switch (iWorkMode / 10) {
        case 0:
            break;
        case 1:
            int len;
            U8 word[] = "i love duan sir";
            if (ltime % autoSendTime == 0) {
                //cout << "��ʼ�Զ�����" << endl;
                //��ʱ����ǰ���жϻ������Ƿ��� �������ֹ�ط� 		�˴��Ȳ�����MSS ��ʵ�ַ��鼼�� Ҳ�����Ƕ�ʱ���¿�ʼ�Զ�����
                if (isfullque(&my_buffer)) {
                    cout << "||||||||||||||||||||||||||||�������Ѿ����� �ģ�|||||||||||||||||||||||||||||||||||||" << endl;
                    break;
                }
                //��ʱ����, ÿ���autoSendTime * DEFAULT_TIMER_INTERVAL ms ����һ��
                //if (printCount % autoSendTime == 0) {
                strcpy(autoSendBuf, word);
                len = strlen(word); //ÿ�η�������
                CongestionControl(autoSendBuf, len, &my_buffer);
                //iSndRetval = CongestionControl(bufSend, iSndRetval, 0);
                //free(bufSend);
            }
            break;

        }
        if (ltime % (autoSendTime + 10) == 0)
            // ӵ������ 
            CongestionSend(&my_buffer);
    }
}

//------------�����ķָ��ߣ����������ݵ��շ�,--------------------------------------------

void RecvfromUpper(U8* buf, int len) {
    return;
}
//***************��Ҫ��������******************************
//���ƣ�RecvfromLower
//���ܣ�������������ʱ����ζ�ŵõ�һ�ݴӵͲ�ʵ��ݽ�����������
//      ��������ȫ�������滻���������Ҫ������
//      ���̹��ܽ��ܣ�
//          1)����ʵ����һ���򵥴ֱ���������Ĳ��ԣ����дӽӿ�0�����������ݶ�ֱ��ת�����ӿ�1�����ӿ�1�������Ͻ����߲㣬������ô����
//          2)ת�����Ͻ�ǰ���ж��ս����ĸ�ʽ��Ҫ���ͳ�ȥ�ĸ�ʽ�Ƿ���ͬ��������bite��������ֽ�������֮��ʵ��ת��
//            ע����Щ�жϲ������������ݱ�����������������������ļ������������ļ��Ĳ���д���ˣ��ж�Ҳ�ͻ�ʧ��
//          3)����iWorkMode���ж��Ƿ���Ҫ���������ݴ�ӡ
//���룺U8 * buf,�Ͳ�ݽ����������ݣ� int len�����ݳ��ȣ���λ�ֽڣ�int ifNo ���Ͳ�ʵ����룬�����������ĸ��Ͳ�
//�����
//��ӡͳ����Ϣ
//***************��Ҫ��������******************************
//���ƣ�CongestionControl
//���ܣ�������������ʱ����ζ��Ӧ�ò㿪ʼ���²㷢�����ݣ����Ͷ˽���ӵ�����ƣ��Է����������ʽ��е��ء�
//      ���̹��ܽ���
//         1)ͨ���Ͳ�����ݸ�ʽ����lowerMode���ж�Ҫ��Ҫ������ת����bit�����鷢�ͣ�����ֻ�����Ͳ�ӿ�0��
//           ��Ϊû���κοɹ��ο��Ĳ��ԣ���������Ӧ�ø���Ŀ�ĵ�ַ�ڶ���ӿ���ѡ��ת���ġ�
//         2)�ж�iWorkMode�������ǲ�����Ҫ�����͵��������ݶ���ӡ������ʱ���ԣ���ʽ����ʱ�����齫����ȫ����ӡ��
//���룺U8 * buf,�߲㴫���������ݣ� int len�����ݳ��ȣ���λ�ֽ�
//�����
//�������ҵ�Ӧ�ò����Ϊ���
// ������
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
        cout << endl << "*************************���Ķ�ʧ*************************" << endl << endl;
        return; // α�챨�Ķ�ʧ����

    }
    APP_Header* header = (APP_Header*)buf;
    flag = header->Flags;
    No = header->Sequence_Number;
    ACK = header->Acknowledgment_Number;
    Rnums = header->Rnums;
    cout << "\n////////////////////////���յ�����////////////////////////\n\n��־λ����flag " << int(flag) << " ���ȡ���len " << len << " ʣ����������Rnums " << int(Rnums) << endl;
    retval = len * 8;//�����λ,����ͳ��
    fnums++;
    cout << "\n����֡���� " << fnums << endl;
    cout << "\n���յ�����������" << endl;
    print_data_bit(buf, len, 1);
    //cout << "buf[0] " << buf[0] << " int buf[0] " << int(buf[0]) << " len " << len << " strlen " << strlen(buf) << endl;
    print_data_byte(buf, len, 1);
    ////�ж��ֽ����ݵ�һλ �ж������ݱ��Ļ���ACK���� 0ΪACK���� 1Ϊ���ݱ���
    switch (flag) {
    case 1: // PUSH����
        //if (No != (lastNo + 1) % MAX_NO) {
        //	cout << "*************************PUSH������ų���*************************" << endl;
        //	cout << "\n�յ����ݱ��Ĳ�����Ҫ��,���Ϊ " << int(No) << " Ҫ��Ϊ" << int(lastNo + 1) % MAX_NO << endl;
        //	SendACK(lastNo);
        //}
        //else {
        cout << "\n------------------------PUSH����------------------------" << endl << endl;
        originbuf = &buf[headerLen];
        cout << "\n�յ���ȷ�����ݱ���,���Ϊ" << int(No) << endl;
        cout << "\nȥ��ͷ���������" << endl;
        print_data_byte(originbuf, len - headerLen, 1);
        SendACK(No);
        lastNo = No;
        //WriteData("output.txt", originbuf, len - 2);
    //}
        break;
    case 2:
        //cout << "\n�յ�ACK�����Ϊ" << int(No) << endl;
        // �жϵõ�����ACK����
        // ���ACK����һ��ACK���һ�� ���ظ�
        if (ACK == lastACK) {
            cout << "*************************ACK��������ظ�*************************" << endl;
            cout << "\n�յ����ϴ�һ����ACK�����Ϊ" << int(ACK) << " ���ִ��� " << ++noNums << endl;
            if (status == 0 && noNums == 2)
                // ӵ������
                CongestionHappen(&my_buffer, lastACK + 2);
            // ������ָ�
            if (noNums == 4)
                QResume(&my_buffer, int(lastACK + 1));
        }
        else {

            cout << "\n------------------------ACK����------------------------" << endl;
            // ˵�����ݽ��ճɹ�
            if (ACK > lastACK + 1)
                cout << "\nû�յ�Ԥ��ACK " << lastACK + 1 << " ���յ�ACK���Ϊ " << int(ACK) << "��Ϊǰһ����Ҳ�յ�\n" << endl;
            else
                cout << "\n�յ���ȷ��ACK����,���Ϊ" << int(ACK) << endl;
            noNums = 1; //����
            if (status == 1) { //ӵ��״̬�յ�ACK���������Զ�����
                iWorkMode = 10 + iWorkMode % 10; // �����Զ�����
                status = 0; // ���ӵ������
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
                my_buffer.retime[i % MAX_QUE] = -2; //ȡ����ʱ�ش�����
            }
            if (status == 1 && my_buffer.front == my_buffer.nxt) {
                my_buffer.nxt = min(now + 1, my_buffer.rear);
            }
            if (my_buffer.retime[my_buffer.front] == -2)
                for (int i = my_buffer.front; my_buffer.retime[i] == -2 && i != my_buffer.nxt; i = (i + 1) % MAX_QUE)
                {
                    //cout << ""
                    dequeue(&my_buffer); // ���ȷ�ϵ��Ƕ��е�front֡ �����
                    //if (!status) // ��ӵ������״̬��Ҫ�ȴ���ָ� ����ֱ�Ӹı䴰��
                    ChangeWin(&my_buffer);
                }
            if (ACK == endACK) {
                endACK = 255;
                isstart = 0; //�ر�ϵͳ
                cout << "\n-------------------------�ر�����-------------------------" << endl << endl;
                return;
            }
            lastACK = ACK;
        }
        break;
    case 3:
        if (ACK == my_buffer.front % MAX_NO) {

            dequeue(&my_buffer); // ���ȷ�ϵ��Ƕ��е�front֡ �����
            my_buffer.retime[my_buffer.front % MAX_QUE] = -2; //ȡ����ʱ�ش�����
            ChangeWin(&my_buffer);
            isstart = 1;
            cout << "\n------------------------ACK_PUSH����------------------------" << endl << "Acknowledgment_Number " << int(ACK) << endl << endl;
            lastNo = No;
            lastACK = ACK;
            originbuf = &buf[headerLen];
            cout << "\n�յ���ȷ�����ݱ���,���Ϊ" << int(No) << endl;
            cout << "\nȥ��ͷ���������" << endl;
            print_data_byte(originbuf, len - 4, 1);
            //WriteData("output.txt", originbuf, len - 3);
            SendACK(No);
        }
        else
            cout << "*************************ACK_PUSH����ACK����*************************" << endl;
        break;
    case 4:
        if (isstart == 0) {
            cout << "\n------------------------SYN����------------------------" << endl << "Sequence_Number " << int(No) << endl << endl;
            lastNo = No;
            SendSYN_ACK(No, &my_buffer);
        }
        break;

    case 6:
        if (isstart == 0) {
            cout << "\n------------------------SYN_ACK����------------------------" << endl << "Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
            if (ACK == my_buffer.front % MAX_NO) {
                lastNo = No;
                isstart = 1; //����ϵͳ
                my_buffer.retime[my_buffer.front % MAX_QUE] = -2; //ȡ����ʱ�ش�����
                lastACK = ACK;
                dequeue(&my_buffer); // ���ȷ�ϵ��Ƕ��е�front֡ �����
                ChangeWin(&my_buffer);
                SendACK_DATA(No, &my_buffer);
            }
            else
                cout << "*************************SYN_ACK����ACK����*************************" << endl;
        }
        break;
    case 8:
        cout << "\n------------------------FIN����------------------------" << endl << "Sequence_Number " << int(No) << endl;
        SendACK(No);
        lastNo = No;
        SendFIN_ACK(No, &my_buffer);
        break;
    case 10:
        cout << "\n------------------------FIN_ACK����------------------------" << endl << "Sequence_Number " << int(No) << " Acknowledgment_Number " << int(ACK) << endl << endl;
        if (int(ACK) == (my_buffer.front + 128 - 1) % MAX_NO) {
            endtime = ltime + 50;
            lastNo = No;
            //cout << endtime << endl;
            SendACK(No);
        }
        break;
    default:
        cout << "\n������ �ģ�" << endl;
        cout << "\n��������" << endl;
        print_data_bit(buf, len, 1);
        break;
    }

    iRcvTotal += retval;
    iRcvTotalCount++;


    cout << endl << "======================================================================" << endl << endl;
    ////��ӡ
    //switch (iWorkMode % 10) {
    //case 1:
    //	cout << endl << "���սӿ� " << ifNo << " ���ݣ�" << endl;
    //	print_data_bit(buf, len, lowerMode[ifNo]);
    //	break;
    //case 2:
    //	cout << endl << "���սӿ� " << ifNo << " ���ݣ�" << endl;
    //	print_data_byte(buf, len, lowerMode[ifNo]);
    //	break;
    //case 0:
    //	break;
}

// ��Ӧ�ò�Ҫ��������ʱ�����øú��������ݷ��������ѭ�����У����ݴ���ʵ����������
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
    cout << "������ " << iSndTotal << " λ," << iSndTotalCount << " ��," << "���� " << iSndErrorCount << " �δ���;";
    cout << " ������ " << iRcvTotal << " λ," << iRcvTotalCount << " ��" << endl;
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

    //����|��ӡ��[���Ϳ��ƣ�0���ȴ��������룻1���Զ���][��ӡ���ƣ�0�������ڴ�ӡͳ����Ϣ��1����bit����ӡ���ݣ�2���ֽ�����ӡ����]
    cout << endl << endl << "�豸��:" << strDevID << ",    ���:" << strLayer << ",    ʵ���:" << strEntity;
    cout << endl << "1-�����Զ�����;" << endl << "2-ֹͣ�Զ�����; " << endl << "3-�����ı�; ";
    cout << endl << "4-���η�������; " << endl << "5-�ر�����;" << endl << "6-������������;";
    cout << endl << "0-ȡ��" << endl << "����������ѡ�����";
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
            cout << endl << "��������������ʱ�޷�����";
            break;
        }
        cout << "������Ҫ���͵��ı���";
        cin >> kbBuf;
        len = (int)strlen(kbBuf) + 1; //�ַ�������и�������
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
            cout << endl << "��������������ʱ�޷�����";
            break;
        }
        cout << "������Ҫ���͵��ı���";
        cin >> kbBuf;
        len = (int)strlen(kbBuf) + 1; //�ַ�������и�������
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
            cout << endl << "��������������ʱ�޷�����";
            break;
        }
        cout << "������Ҫ���͵����ݣ�";
        cin.getline(kbBuf, 2000);
        cout << "������Ҫ���͵ĵ�ַ��";
        cin >> des;
        des += strDevID;
        //cout << des << endl;
        WriteData("1.txt", des.c_str(), des.size());
        //char a[10];
        //ReadData("1.txt", a); // ��1.txt��ȡ���ݲ�����a��
        //cout << a << endl;
        len = (int)strlen(kbBuf) + 1; //�ַ�������и�������
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
