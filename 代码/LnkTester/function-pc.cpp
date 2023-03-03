//Nettester �Ĺ����ļ�
#include <iostream>
#include <conio.h>
#include "winsock.h"
#include "stdio.h"
#include "CfgFileParms.h"
#include "function.h"
using namespace std;

#define true 1
#define false 0
#define SW_SIZE 5120 //���ʹ��ڵĴ�С
#define RW_SIZE 5120 // ���մ��ڴ�С
#define BUF_SIZE 10240 // ���ͻ������Ĵ�С
#define MAX_LEN 80 //��֡����
#define PRINT 0 //��ӡ��Ϣ

//����Ϊ��Ҫ�ı���
U8* sendbuf;        //������֯�������ݵĻ��棬��СΪMAX_BUFFER_SIZE,���������������������ƣ��γ��ʺϵĽṹ��������û��ʹ�ã�ֻ������һ��
int printCount = 0; //��ӡ����
int spin = 0;  //��ӡ��̬��Ϣ����

int wrongseqlist[RW_SIZE];//���մ���֡������б�
int wrong_index = -1;//����֡����б������
int last_recvmac = 0; // ��һ���յ���MAC��ַ
int last_sendmac = 0; // ��һ�η�����MAC��ַ
int last_upseq = -1;//��һ�η��͵��ϲ����ݵ����
int last_firstseq = -1; // �ϴ���֡���
int last_firsttime = -1; // �ϴ���֡ʱ��

//------�����ķָ��ߣ�һЩͳ���õ�ȫ�ֱ���------------
int iSndTotal = 0;  //������������
int iSndTotalCount = 0; //���������ܴ���
int iSndErrorCount = 0;  //���ʹ������
int iRcvForward = 0;     //ת����������
int iRcvForwardCount = 0; //ת�������ܴ���
int iRcvToUpper = 0;      //�ӵͲ�ݽ��߲���������
int iRcvToUpperCount = 0;  //�ӵͲ�ݽ��߲������ܴ���
int iRcvUnknownCount = 0;  //�յ�������Դ�����ܴ���

int recLen = 0; //���ڷ���ĳ����ж�
U8(*TEMP)[MAX_LEN - 1] = NULL; //��֡����
int TEMP_INDEX = 0; //��֡�����


typedef struct {
	U8* data[BUF_SIZE];
	int front; //ָ���ʹ��ڵ�ǰһ��
	int rear; //ָ���ʹ��ڵ����һ��
	int mid; // ָ�����һ���ѷ��͵�δȷ�ϵ�����
	int buf_area; // ���뻺���������ʹ��ڷŲ��£�δ�������ݵ����һ��
	int len[BUF_SIZE]; //֡�ĳ���
	int seq[BUF_SIZE]; //֡�����
	int tickTack[BUF_SIZE]; //��ʱ�������Ƿ��ط�
}send_window;

typedef struct {
	U8* data[RW_SIZE]; //���ǽ��մ��ڵĴ�С
	int front; // ���մ��ڵ�ǰһ��
	int rear; // ���մ�����δ���ϲ�ȡ�ߵ����ݵ����һ��
	int len[RW_SIZE];
	int seq[RW_SIZE];
	int sign[RW_SIZE];
}receive_window;

// ���ʹ��ڵĳ�ʼ��
void init_sw(send_window* q)
{
	q->front = -1; //���ʹ��ڵ������
	q->rear = (q->front + SW_SIZE) % BUF_SIZE; //���ʹ��ڵ��ұ���
	q->mid = -1; //���ʹ����м������Ƿ��ѷ��͵�
	q->buf_area = q->rear; //δ�ڴ����ڵ������˻�������ָʾ
}

// *q�����ʹ���ָ�룻buf�������뷢�ͻ����������ݣ�seq�����������ݵ����
int enqueue_sw(send_window* q, U8* buf, int len, int seq)
{
	if (q != NULL)
	{
		if ((q->buf_area + 1) % BUF_SIZE == q->front)
		{
			printf("\nThe buffer is full\n");
			return false;
		}

		else
		{
			if (q->mid != q->rear)
			{
				q->mid = (q->mid + 1) % BUF_SIZE;
				q->data[q->mid] = buf;
				q->len[q->mid] = len;
				q->seq[q->mid] = seq;
				q->tickTack[q->mid] = 0;
				return true;
			}
			else
			{
				q->buf_area = (q->buf_area + 1) % BUF_SIZE;
				q->data[q->buf_area] = buf;
				q->len[q->buf_area] = len;
				q->seq[q->buf_area] = seq;
				q->tickTack[q->buf_area] = 0;
				return true;
			}
		}
	}
	else
	{
		printf("\nThe parameter is not valid\n");
		return false;

	}
}

// *q�����ʹ���ָ�룻seq���յ�ack�����ݵ����
int dequeue_sw(send_window* q, int seq)
{
	int flag = 0; //�жϷ��ʹ���front-mid֮���Ƿ��д���ŵ�����
	if (q != NULL)
	{
		if (q->mid == q->front)// ���ʹ�����û������
		{
			return false;
		}
		else
		{
			int temp_front = q->front;
			while (temp_front != q->mid)
			{
				temp_front = (temp_front + 1) % BUF_SIZE;
				if (temp_front == q->mid)
				{
					//cout << "------------------attention-----------------------" << endl << endl;
					//cout << q->seq[temp_front] << endl;
				}
				if (q->seq[temp_front] == seq)
				{
					//cout << "�ǲ���0��" << endl << endl;
					for (int j = temp_front; j != (q->front + 1) % BUF_SIZE; j = (j + BUF_SIZE - 1) % BUF_SIZE)
					{
						q->data[j] = q->data[(j + BUF_SIZE - 1) % BUF_SIZE];
						q->len[j] = q->len[(j + BUF_SIZE - 1) % BUF_SIZE];
						q->seq[j] = q->seq[(j + BUF_SIZE - 1) % BUF_SIZE];
						q->tickTack[j] = q->tickTack[(j + BUF_SIZE - 1) % BUF_SIZE];
					}
					flag = 1;
					break;
				}
			}
			if (flag == 1)
			{
				//cout << "---------------------------" << endl << endl;
				//cout << q->front << endl << endl;
				q->front = (q->front + 1) % BUF_SIZE;
				q->rear = (q->rear + 1) % BUF_SIZE;
				q->buf_area = (q->buf_area + 1) % BUF_SIZE;
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	else
	{
		printf("\nThe parameter is not valid\n");
		return false;
	}
}

void init_rw(receive_window* q)
{
	q->front = -1;
	q->rear = -1;
}

int enqueue_rw(receive_window* q, U8* buf, int len, int seq, int sign)
{
	if (q != NULL)
	{
		if ((q->rear + 1) % RW_SIZE == q->front)
		{
			printf("\nThe queue is full\n");
			return false;
		}
		else
		{
			q->rear = (q->rear + 1) % RW_SIZE;
			q->data[q->rear] = buf;
			q->len[q->rear] = len;
			q->seq[q->rear] = seq;
			q->sign[q->rear] = sign;
			return true;
		}
	}
	else
	{
		printf("\nThe parameter is not valid\n");
		return false;
	}
}

U8* dequeue_rw(receive_window* q, int* len, int* seq)
{
	U8* buf;
	if (q != NULL)
	{
		if (q->rear == q->front) //���մ����ǿյ�
		{
			cout << "The queue is empty" << endl;
			return NULL;
		}
		else
		{
			q->front = (q->front + 1) % RW_SIZE;
			*len = q->len[q->front];
			*seq = q->seq[q->front];
			buf = q->data[q->front];
			return buf;
		}
	}
	else
	{
		printf("\nThe parameter is not valid\n");
		return NULL;
	}
}

void free_queue(send_window* q1, receive_window* q2)
{
	int temp_front1 = q1->front;
	int temp_front2 = q2->front;
	if (q1->mid == q1->rear) //��ʱ���ʹ��������������
	{
		while (temp_front1 != q1->buf_area)
		{
			temp_front1 = (temp_front1 + 1) % BUF_SIZE;
			if (q1->data[temp_front1] != NULL)
			{
				free(q1->data[temp_front1]);
			}
		}
	}
	else //��ʱ���ݾ��ڷ��ʹ�����
	{
		while (temp_front1 != q1->mid)
		{
			temp_front1 = (temp_front1 + 1) % BUF_SIZE;
			if (q1->data[temp_front1] != NULL)
			{
				free(q1->data[temp_front1]);
			}
		}
	}
	while (temp_front2 != q2->rear)
	{
		temp_front2 = (temp_front2 + 1) % RW_SIZE;
		if (q2->data[temp_front2] != NULL)
		{
			free(q1->data[temp_front2]);
		}
	}

}

send_window window_send;
receive_window window_receive;
send_window* window_s = &window_send;
receive_window* window_r = &window_receive;

//���ʹ��ڷŵ���û�н��ġ�����Ϊbit���������
//���մ��ڷŵ����Ѿ�����˵ġ�����Ϊbyte���������

//��ӡͳ����Ϣ
void print_statistics();
void menu();

//��У����㣨����add_head��ʹ�ã�
int CheckSum_int(U8* Buf, int Len)
{
	int i = 0;
	int sum = 0;

	for (i = 0; i < Len; i++)
	{
		sum += *Buf++;
	}
	return sum;
}

//��У����㣨����add_head��ʹ�ã�
U8 CheckSum(U8* Buf, int Len)
{
	int i = 0;
	U8 sum = 0;
	U8 checksum = 0;

	for (i = 0; i < Len; i++)
	{
		sum += *Buf++;
	}
	checksum = sum & 0xff;
	return checksum;
}

//����֡ͷ������Ϊ
//input:ԭ�������ݡ�ԭ�����ݳ��ȡ�ģʽflag��Ϊ��2�������ݣ���0���͡�1������nak��ack����index���
//output:frame�Ӻ�ͷ����֡���������У��λ���� length֡�ĳ���
void add_head(U8* buf, int len, U8* frame, int* length, U8 flag, U8* index, U8 mac_src, U8 mac_des) {
	U8 F = 0x7e;
	U8 E = 0x10;
	U8 sum_data = 0;
	int add_len = 0;
	frame[0] = F;
	if (*index == F || *index == E) {
		frame[1] = E;
		frame[2] = *index;
		add_len++;
	}
	else {
		frame[1] = *index;
	}
	if (flag == F || flag == E) {
		frame[2 + add_len] = E;
		frame[3 + add_len] = flag;
		add_len++;
	}
	else {
		frame[2 + add_len] = flag;
	}
	if (mac_src == F || mac_src == E) {
		frame[3 + add_len] = E;
		frame[4 + add_len] = mac_src;
		add_len++;
	}
	else {
		frame[3 + add_len] = mac_src;
	}

	int i = 0, j = 4 + add_len;
	add_len = 0;
	for (; i < len; i++, j++) {
		if (buf[i] != F && buf[i] != E) {
			frame[j] = buf[i];
		}
		else {
			frame[j] = E;
			frame[j + 1] = buf[i];
			j++;
		}
	}

	if (mac_des == F || mac_des == E) {
		frame[j] = E;
		frame[j + 1] = mac_des;
		frame[j + 3] = F;
		add_len++;
	}
	else {
		frame[j] = mac_des;
		frame[j + 2] = F;
	}

	frame[j + 1 + add_len] = '0';
	sum_data = CheckSum(frame, j + 3 + add_len);
	if (sum_data == F || sum_data == E) {
		frame[j] = E;
		frame[j + add_len] = mac_des;
		frame[j + 1 + add_len] = E;
		frame[j + 2 + add_len] = sum_data;
		frame[j + 3 + add_len] = F;
		*length = j + 4 + add_len;
	}
	else {
		frame[j] = E;
		frame[j + add_len] = mac_des;
		frame[j + 1 + add_len] = sum_data;
		*length = j + 3 + add_len;
	}

	((int*)(frame + *length))[0] = CheckSum_int(frame, *length);
	*length += 4;
}

//��bit���еó�����
//input:���յ�bit���ݡ�bit���ݳ���
//output:buf_real��ʵ��byte����,,length���ݳ��ȣ����Ϊ����output=0����flag������ݵ�ģʽ��index������ݵ����
void get_data(U8* bit, int len, U8* buf_real, int* length, U8* flag, U8* index, U8* mac_src, U8* mac_des) {
	U8 sum = 0;
	U8 checksum = 0;
	int sum_2 = 0;
	int right_sum = 0;
	U8 F = 0x7e;
	U8 E = 0x10;
	int i = 0;
	int n = 0, m = 0;
	int new_len = 0;
	int add_len = 0;

	U8* bit_real = (U8*)malloc(len + 80);
	if (bit_real == NULL) {
		return;
	}

	U8* buf = (U8*)malloc(len / 8 + 10);
	if (buf == NULL) {
		return;
	}
	for (; i < len; i++) {
		if (bit[i] == 0 && bit[i + 1] == 1 && bit[i + 2] == 1 && bit[i + 3] == 1 && bit[i + 4] == 1 && bit[i + 5] == 1 && bit[i + 6] == 1 && bit[i + 7] == 0) {
			break;
		}
	}

	int j = i;
	for (; j < len; j++) {
		bit_real[j - i] = bit[j];
	}

	BitArrayToByteArray(bit_real, len - i, buf, (len - i) / 8 + 1);
	if (bit_real != NULL) {
		free(bit_real);
	}


	if (buf[1] == E) {
		*index = buf[2];
		sum += E + buf[2];
		add_len++;
	}
	else {
		*index = buf[1];
		sum += *index;
	}
	if (buf[2 + add_len] == E) {
		*flag = buf[3 + add_len];
		sum += E + buf[3 + add_len];
		add_len++;
	}
	else {
		*flag = buf[2 + add_len];
		sum += *flag;
	}
	if (buf[3 + add_len] == E) {
		*mac_src = buf[4 + add_len];
		sum += E + buf[4 + add_len];
		add_len++;
	}
	else {
		*mac_src = buf[3 + add_len];
		sum += *mac_src;
	}

	n = 4 + add_len;
	for (; buf[n] != F && n < (len - i) / 8 + 1; n++, m++) {
		if (buf[n] == E) {
			buf_real[m] = buf[n + 1];
			sum += buf[n] + buf[n + 1];
			n++;
		}
		else {
			sum += buf[n];
			buf_real[m] = buf[n];
		}
	}

	if (n == (len - i) / 8 + 1) {
		if (buf != NULL) {
			free(buf);
		}
		new_len = 0;
		*length = new_len;
		return;
	}

	sum_2 = CheckSum_int(buf, n + 1);
	right_sum = ((int*)(buf + n + 1))[0];

	sum -= buf[n - 1];
	sum += F + F + '0';
	if (buf[n - 1] == F || buf[n - 1] == E) {
		sum -= E;
	}
	checksum = sum & 0xff;

	new_len = m - 2;
	*mac_des = buf_real[new_len];
	buf_real[0] = *mac_src;
	if (checksum == buf[n - 1] && right_sum == sum_2) {
		if (buf != NULL) {
			free(buf);
		}
		*length = new_len;
	}
	else {
		if (buf != NULL) {
			free(buf);
		}
		new_len = 0;
		*length = new_len;
	}
}
//����ack����nack������flag�����жϣ�
//input:flagΪ'1'��ack��Ϊ'0'��nck(),indexΪ��Ҫ�ط�ȷ�ϵ��������
//output:��
//ע��flagΪU8��Ϊ��1������ack������1
void ack(U8 flag, U8 index, U8 mac_src, U8 mac_des) {

	int new_len;
	U8* bufSend = NULL;
	U8* frame = NULL;
	U8* buf = NULL;
	buf = (U8*)malloc(10);
	buf[0] = mac_des;
	buf[1] = 'a';

	frame = (char*)malloc(30);
	if (frame == NULL) {
		return;
	}

	add_head(buf, 2, frame, &new_len, flag, &index, mac_src, mac_des);

	bufSend = (char*)malloc(new_len * 8 + 32);
	if (bufSend == NULL) {
		return;
	}
	ByteArrayToBitArray(bufSend, new_len * 8, frame, new_len);
	//����
	SendtoLower(bufSend, new_len * 8, 0); //��������Ϊ���ݻ��壬���ȣ��ӿں�
	if (frame != NULL) {
		free(frame);
	}
	if (buf != NULL) {
		free(buf);
	}
	if (bufSend != NULL) {
		free(bufSend);
	}
}

//��֡���������ڽ������ݷ�֡
void Framing(U8* buf, int len, U8(*frame)[MAX_LEN], int* num, int* last_len) {
	U8 F = 0x7e;
	int num_ = 0;//֡������
	int lastlen = MAX_LEN;//���һ֡�ĳ���
	int index = 1;
	U8 mac_des = buf[0];
	if (len % (MAX_LEN - 1) != 0) {
		num_ = len / (MAX_LEN - 1) + 2;
		lastlen = len % (MAX_LEN - 1);
	}
	else {
		num_ = len / (MAX_LEN - 1) + 1;
	}
	frame[0][0] = mac_des;
	for (int j = 1; j < 9; j++) {
		frame[0][j] = F;
	}
	U8* temp_int = &frame[0][9];
	((int*)temp_int)[0] = (num_ - 1);

	for (int i = 1; i < num_ - 1; i++) {
		frame[i][0] = mac_des;
		for (int j = 1; j < MAX_LEN; j++) {
			frame[i][j] = buf[index];
			index++;
		}
	}

	frame[num_ - 1][0] = mac_des;
	for (int j = 1; index < len; index++, j++) {
		frame[num_ - 1][j] = buf[index];
	}

	*num = num_;
	*last_len = lastlen;
}

//���з�֡���Ϲ��ܣ�ֱ���滻SendtoUpper
void send_to_upper(U8* buf_real, int iSndRetval) {
	if (iSndRetval >= 9 && recLen == 0) {
		for (int i = 1; i < 9; i++) {
			if (buf_real[i] != 0x7e) {
				break;
			}
			if (i == 8) {
				U8* temp_int = buf_real + 9;
				recLen = ((int*)temp_int)[0];
				TEMP = (U8(*)[MAX_LEN - 1])malloc((MAX_LEN - 1) * (recLen + 4));
				TEMP++;
				return;
			}
		}
	}
	if (recLen != 0) {

		for (int i = 1; i < iSndRetval; i++) {
			TEMP[TEMP_INDEX][i - 1] = buf_real[i];
		}
		TEMP_INDEX++;


		if (recLen == TEMP_INDEX) {
			U8* bufSend = (U8*)TEMP;
			bufSend--;
			bufSend[0] = buf_real[0];

			SendtoUpper(bufSend, (TEMP_INDEX - 1) * (MAX_LEN - 1) + iSndRetval);

			TEMP--;
			if (TEMP != NULL) {
				free(TEMP);
			}
			TEMP_INDEX = 0;
			recLen = 0;
		}
		else {
			return;
		}
	}
	else {
		SendtoUpper(buf_real, iSndRetval);
	}
}

//***************��Ҫ��������******************************
//���ƣ�InitFunction
//���ܣ���ʼ�������棬��main�����ڶ��������ļ�����ʽ������������ǰ����
//���룺
//�����
void InitFunction(CCfgFileParms& cfgParms)
{
	/*
	sendbuf = (char*)malloc(MAX_BUFFER_SIZE);
	if (sendbuf == NULL ) {
		cout << "�ڴ治��" << endl;
		//����������Ҳ̫���˳���
		exit(0);
	}
	*/
	init_sw(window_s);
	init_rw(window_r);
	return;
}
//***************��Ҫ��������******************************
//���ƣ�EndFunction
//���ܣ����������棬��main�������յ�exit������������˳�ǰ����
//���룺
//�����
void EndFunction()
{
	free_queue(window_s, window_r);
	return;
}

//***************��Ҫ��������******************************
//���ƣ�TimeOut
//���ܣ�������������ʱ����ζ��sBasicTimer�����õĳ�ʱʱ�䵽�ˣ�
//      �������ݿ���ȫ���滻Ϊ������Լ����뷨
//      ������ʵ���˼���ͬʱ���й��ܣ����ο�
//      1)����iWorkMode����ģʽ���ж��Ƿ񽫼�����������ݷ��ͣ�
//        ��Ϊscanf�����������¼�ʱ���ڵȴ����̵�ʱ����ȫʧЧ������ʹ��_kbhit()������������ϵ��ڼ�ʱ�Ŀ������жϼ���״̬�������Get��û��
//      2)����ˢ�´�ӡ����ͳ��ֵ��ͨ����ӡ���Ʒ��Ŀ��ƣ�����ʼ�ձ�����ͬһ�д�ӡ��Get��
//���룺ʱ�䵽�˾ʹ�����ֻ��ͨ��ȫ�ֱ�����������
//���������Ǹ�����Ŭ���ɻ����ʵ����
void TimeOut() //��ʱ�ش�
{
	if (_kbhit()) {
		//�����ж���������˵�ģʽ
		menu();
	}

	int temp_front = window_s->front;
	while (temp_front != window_s->mid)
	{

		temp_front = (temp_front + 1) % BUF_SIZE;
		//cout << "-----------------------------��ʱ����Ƕ��ٰ�������----------" << endl << endl;
		//cout << "��ʱ�����" << window_s->seq[temp_front] << endl << endl;
		window_s->tickTack[temp_front]++;

		if (window_s->tickTack[temp_front] == 10)//ms�ش�һ��
		{
			SendtoLower(window_s->data[temp_front], window_s->len[temp_front], 0);
			window_s->tickTack[temp_front] = 0;

			//���¶���Ϊ��ӡ��Ϣ��

			U8* bufSend;
			bufSend = (U8*)malloc(window_s->len[temp_front] / 8 + 40);
			U8 mac_src;
			U8 mac_des;
			int iSndRetval;
			char flag;
			char index;
			get_data(window_s->data[temp_front], window_s->len[temp_front], bufSend, &iSndRetval, &flag, &index, &mac_src, &mac_des);
			//��ӡ��Ϣ����ʱ�ش�֡��
			if (PRINT)
			{
				cout << endl;
				cout << "///////////////////////////////////��ʱ�ش�֡///////////////////////////////////" << window_s->len[temp_front] << endl << endl;
				cout << "֡�����Ϊ��" << int(index) << endl << endl;
				cout << "��Ϣ������Ϊ��";
				for (int w = 0; w < iSndRetval; w++) {
					cout << bufSend[w];
				}
				cout << endl << endl;
				cout << "��Ϣ��ʮ��������Ϊ��";
				for (int w = 0; w < iSndRetval; w++) {
					cout << int(bufSend[w]) << " ";
				}
				cout << endl << endl;

				U8* bit_buf = (U8*)malloc(iSndRetval * 8 + 80);
				ByteArrayToBitArray(bit_buf, iSndRetval * 8, bufSend, iSndRetval);
				cout << "��Ϣ�ı�����Ϊ��";
				for (int w = 0; w < iSndRetval * 8; w++) {
					cout << int(bit_buf[w]);
					if (w % 4 == 3) {
						cout << " ";
					}
				}
				if (bit_buf != NULL) {
					free(bit_buf);
				}
				cout << endl;
			}
			if (bufSend) {
				free(bufSend);
			}
		}

	}
}
//------------�����ķָ��ߣ����������ݵ��շ�,--------------------------------------------

//***************��Ҫ��������******************************
//���ƣ�RecvfromUpper
//���ܣ�������������ʱ����ζ���յ�һ�ݸ߲��·�������
//      ��������ȫ�������滻��������Լ���
//      ���̹��ܽ���
//         1)ͨ���Ͳ�����ݸ�ʽ����lowerMode���ж�Ҫ��Ҫ������ת����bit�����鷢�ͣ�����ֻ�����Ͳ�ӿ�0��
//           ��Ϊû���κοɹ��ο��Ĳ��ԣ���������Ӧ�ø���Ŀ�ĵ�ַ�ڶ���ӿ���ѡ��ת���ġ�
//         2)�ж�iWorkMode�������ǲ�����Ҫ�����͵��������ݶ���ӡ������ʱ���ԣ���ʽ����ʱ�����齫����ȫ����ӡ��
//���룺U8 * buf,�߲㴫���������ݣ� int len�����ݳ��ȣ���λ�ֽ�
//�����
void RecvfromUpper(U8* buf, int len)
{//�Ӹ߲���յ���Ϊ�ֽ�����
	int iSndRetval;

	U8* bufSend = NULL; //bufSend��bit����
	U8* frame;
	U8(*frames)[MAX_LEN] = NULL;//֡����



	U8 mac_src;//Դmac
	U8 mac_des;//Ŀ��mac
	int new_len;
	char index = iSndTotalCount % 128;; //��ŵ��ַ���ʽ
	int seq = int(index); //��ŵ�������ʽ
	int num;//���ٸ�С֡
	int last_len;//����С֡��



	if (len > MAX_LEN) {
		frames = (U8(*)[MAX_LEN])malloc(MAX_LEN * (len / MAX_LEN + 10));
		Framing(buf, len, frames, &num, &last_len);
		RecvfromUpper(frames[0], 13);
		for (int i = 1; i < num - 1; i++) {
			RecvfromUpper(frames[i], MAX_LEN);
		}
		RecvfromUpper(frames[num - 1], last_len);
		if (frames != NULL) {
			free(frames);
		}
		return;
	}


	mac_des = buf[0];
	mac_src = strDevID.c_str()[0];


	frame = (char*)malloc(len * 2 + 40);
	if (frame == NULL) {
		return;
	}
	if (mac_des == last_sendmac) // ���ϴη��͵�MAC��ַ��ͬ
		//������Ϣ����װ��֡���ֽ���ʽʱ���з�װ�� buf->frame
		add_head(buf, len, frame, &new_len, '2', &index, mac_src, mac_des);
	else
		add_head(buf, len, frame, &new_len, '6', &index, mac_src, mac_des);
	last_sendmac = mac_des;

	bufSend = (char*)malloc(new_len * 8 + 40);
	if (bufSend == NULL) {
		return;
	}

	//���ֽ�ת��Ϊ���أ��������²� frame->bufSend
	iSndRetval = ByteArrayToBitArray(bufSend, new_len * 8, frame, new_len);

	//��ӡ��Ϣ������֡��
	if (PRINT)
	{
		cout << endl;
		cout << "----------------------------------����֡-----------------------------------" << endl << endl;
		cout << "֡�����Ϊ��" << int(frame[1]) << endl << endl;
		cout << "֡��У��Ϊ��" << int(frame[new_len - 2]) << endl << endl;
		cout << "Դ��ַΪ��" << mac_src << endl << endl;
		cout << "Ŀ�ĵ�ַΪ��" << mac_des << endl << endl;
		cout << "��Ϣ������Ϊ��";
		for (int w = 3; w < new_len - 2; w++) {
			cout << frame[w];
		}
		cout << endl << endl;
		cout << "֡��ʮ��������Ϊ��";
		for (int w = 0; w < new_len; w++) {
			cout << int(frame[w]) << " ";
		}
		cout << endl << endl;
		cout << "�����ͱ�����Ϊ��";
		for (int w = 0; w < new_len * 8; w++) {
			cout << int(bufSend[w]);
			if (w % 4 == 3) {
				cout << " ";
			}
		}
		cout << endl;
	}


	//����
	SendtoLower(bufSend, iSndRetval, 0); //��������Ϊ���ݻ��壬���ȣ��ӿں�

	if (iSndRetval <= 0) {
		iSndErrorCount++;
	}
	else {
		iSndTotal += iSndRetval;
		iSndTotalCount++;
	}
	if (frame != NULL) {
		free(frame);
	}
	// ���յ��Ĳ����з�װ������ݷ��뷢�ʹ��ڣ����뷢�ʹ��ڵ�Ϊbit���飩
	enqueue_sw(window_s, bufSend, iSndRetval, seq);
	//cout << "���ʹ���Ŀǰ��С��" << (window_s->rear + 128 - window_r->front) % 128 << endl;
	if (PRINT) {
		cout << "���ʹ�������  window_s->front " << window_s->front << " window_s->mid " << window_s->mid << " window_s->rear " << window_s->rear << " window_s->buf_area " << window_s->buf_area << endl;
		for (int i = (window_s->front + 1) % BUF_SIZE; i != (window_s->mid + 1) % BUF_SIZE; i = (i + 1) % BUF_SIZE)
			cout << window_s->seq[i] << " ";
		cout << endl;
		cout << "��˳���ӡ���ʹ����е����ݣ���0��ʼ��" << endl;
		for (int i = 0; i < SW_SIZE; i++)
			cout << window_s->seq[i] << " ";
		cout << endl;
	}
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
void RecvfromLower(U8* buf, int len, int ifNo)
{//���²���յ�����������Ϊbit����
	int iSndRetval; //����ĳ���

	U8* bufSend;
	U8* buf_real; //�����buf
	U8* zero_buf = NULL; //������մ��ڵĿ�����

	U8 mac_src;
	U8 mac_des;
	char flag; //�ж�buf�Ǻ������ͣ����� or ACK��
	char index; //��ŵ��ַ���ʽ
	int seq; //��ŵ�������ʽ
	int sign; //���յ�֡�Ƿ����ı�־ 1����ȷ��0������
	int sign1 = 0; //�ж�ĳ��ȷ֡�Ƿ��ǳ���֡���ش�֡ 1���� 0������
	int flag1 = 0; //�ж�ĳ����֡�Ƿ�֮ǰ������ˣ��Ƿ��Ѽ���wrongseqlist�� 1���� 0����

	bufSend = (U8*)malloc(len + 80);
	buf_real = (U8*)malloc(len / 8 + 10);

	for (int i = 0; i < len; i++)
	{
		bufSend[i] = buf[i]; //�½�һ��bit�����buf��ȫ���ƣ���СϵͳӰ��
	}

	// ���������˽ϴ�Ķ�����ע�Ͳ�����Ҫ�޸�
	// �Խ��յ���֡���н�⣬
	// �ж��Ƿ�����������������ȳ�ʱ�ش����ط���֡����֮����յ����ݷ�����մ��ڣ�
	// ���ٴ��յ���֡�󣬽���֡����ת�����ϲ㣬�������е�֡�����ת�����ϲ㣻����¼��
	// ���͵���Ž��н����
	// ��δ�����ж������ݻ���ack��
	// ��������ݣ���ֱ��ת�����ϲ㣨���֮ǰû�г�������ݣ������������Ӧ����Ŵ���֮ǰ��
	// ��������ݣ��������²㷢�͸���ŵ�ack��
	// �����ack���ͷ��ʹ��ڳ���ĳ��ŵ�֡��

	//��ý���ģ�bufSend��Ϊbit��->buf_real��Ϊbyte��
	get_data(bufSend, len, buf_real, &iSndRetval, &flag, &index, &mac_src, &mac_des);
	seq = (int)index;

	if (iSndRetval == 0) //���ݳ���ֱ�Ӷ��������ܣ�
	{
		if (PRINT) {
			cout << endl;
			cout << "///////////////////////////////////֡����///////////////////////////////////" << endl << endl;
			cout << "�����֡���Ϊ��" << seq << endl << endl;
		}
		return;
	}
	else
	{
		//�ж�֡�Ƿ������Լ�
		if (mac_des != strDevID.c_str()[0] && mac_des != 'F') {
			if (PRINT) {
				cout << "++++++++++++++++++++++++++++++++++++++++++++++++" << endl << endl;
				cout << "�յ�һ���������Լ���֡" << endl;
				cout << "֡�����Ϊ��" << int(index) << endl << endl;
				cout << "Դ��ַΪ��" << mac_src << endl << endl;
				cout << "Ŀ�ĵ�ַΪ��" << mac_des << endl << endl;
				cout << "++++++++++++++++++++++++++++++++++++++++++++++++" << endl << endl;
				return;
			}
		}

		if (flag == '1') //��ack
		{
			if (PRINT) {
				cout << endl;
				cout << "///////////////////////////////////����ACK֡///////////////////////////////////" << endl << endl;
				cout << "�յ�һ�����Ϊ" << int(index) << "��ACK֡" << endl << endl;
				//cout << "֡�����Ϊ��" << int(index) << endl << endl;
				cout << "��Ϣ������Ϊ��";
				for (int w = 0; w < iSndRetval; w++) {
					cout << buf_real[w];
				}
				cout << endl << endl;
			}
			dequeue_sw(window_s, seq);
			//cout << "���ʹ���Ŀǰ��С��" << (window_s->rear + 128 - window_r->front) % 128 << endl;
			if (PRINT) {
				cout << "���ʹ�������  window_s->front " << window_s->front << " window_s->mid " << window_s->mid << " window_s->rear " << window_s->rear << " window_s->buf_area " << window_s->buf_area << endl;
				for (int i = (window_s->front + 1) % BUF_SIZE; i != (window_s->mid + 1) % BUF_SIZE; i = (i + 1) % BUF_SIZE)
					cout << window_s->seq[i] << " ";
				cout << endl;
				cout << "��˳���ӡ���ʹ����е����ݣ���0��ʼ��" << endl;
				for (int i = 0; i < SW_SIZE; i++)
					cout << window_s->seq[i] << " ";
				cout << endl;
			}
			//}
		}
		else if (flag == '2')//�����ݣ�����ȷ
		{
			cout << int(index) << endl;
			if (mac_src != last_recvmac) {
				if (PRINT) {
					cout << endl;
					cout << "///////////////////////////////////�յ����Բ�����MAC��ַ������֡///////////////////////////////////" << endl << endl;
					cout << "֡�����Ϊ��" << int(index) << endl << endl;
					cout << "Դ��ַΪ��" << mac_src << endl << endl;
					cout << "Ŀ�ĵ�ַΪ��" << mac_des << endl << endl;
				}
			}
			else {
				//��ӡ��Ϣ������֡��
				if (PRINT)
				{
					cout << endl;
					cout << "///////////////////////////////////����֡///////////////////////////////////" << endl << endl;
					cout << "֡�����Ϊ��" << int(index) << endl << endl;
					cout << "Դ��ַΪ��" << mac_src << endl << endl;
					cout << "Ŀ�ĵ�ַΪ��" << mac_des << endl << endl;
					cout << "��Ϣ������Ϊ��";
					for (int w = 0; w < iSndRetval; w++) {
						cout << buf_real[w];
					}
					cout << endl << endl;
					cout << "��Ϣ��ʮ��������Ϊ��";
					for (int w = 0; w < iSndRetval; w++) {
						cout << int(buf_real[w]) << " ";
					}
					cout << endl << endl;

					U8* bit_buf = (U8*)malloc(iSndRetval * 80);
					ByteArrayToBitArray(bit_buf, iSndRetval * 8, buf_real, iSndRetval);
					cout << "��Ϣ�ı�����Ϊ��";
					for (int w = 0; w < iSndRetval * 8; w++) {
						cout << int(bit_buf[w]);
						if (w % 4 == 3) {
							cout << " ";
						}
					}
					if (bit_buf != NULL) {
						free(bit_buf);
					}
					cout << endl;
				}

				ack('1', index, strDevID.c_str()[0], mac_src);//�ú���������ack��������ack

				if (PRINT) {
					cout << endl;
					cout << "///////////////////////////////////����ACK֡///////////////////////////////////" << endl << endl;
					cout << "����һ�����Ϊ" << int(index) << "��ACK֡" << endl << endl;
				}



				if (seq == (last_upseq + 1) % 128)
				{//��֡���Ϊ��һ�η��͵��ϲ�������ż�1�����������
					if (window_r->front == window_r->rear)
					{//�����մ���Ϊ�գ�˵����ǰ���޴������ݣ�ֱ���Ϸ�
						send_to_upper(buf_real, iSndRetval);
						//��ӡ��Ϣ�����ϲ㷢��֡��
						if (PRINT)
						{
							cout << endl;
							cout << "///////////////////////////////////���ϲ㷢��֡///////////////////////////////////" << endl << endl;
							cout << "֡�����Ϊ��" << int(index) << endl << endl;
							cout << "��Ϣ������Ϊ��";
							for (int w = 0; w < iSndRetval; w++) {
								cout << buf_real[w];
							}
							cout << endl << endl;
							cout << "��Ϣ��ʮ��������Ϊ��";
							for (int w = 0; w < iSndRetval; w++) {
								cout << int(buf_real[w]) << " ";
							}
							cout << endl << endl;

							U8* bit_buf = (U8*)malloc(iSndRetval * 80);
							ByteArrayToBitArray(bit_buf, iSndRetval * 8, buf_real, iSndRetval);
							cout << "��Ϣ�ı�����Ϊ��";
							for (int w = 0; w < iSndRetval * 8; w++) {
								cout << int(bit_buf[w]);
								if (w % 4 == 3) {
									cout << " ";
								}
							}
							if (bit_buf != NULL) {
								free(bit_buf);
							}
							cout << endl;
						}
						last_upseq = seq;
					}
					else
					{//�����ܴ��ڲ�Ϊ�գ�˵����ǰ�д������ݣ���ô������Ϊ�����ش�֡����
					 //��Ӧ����ڽ��մ��ڵ�һλ

						//�޸ĵ�һ��λ�õ�ֵ
						window_r->data[window_r->front + 1] = buf_real;
						window_r->len[window_r->front + 1] = iSndRetval;
						window_r->seq[window_r->front + 1] = seq;
						window_r->sign[window_r->front + 1] = 1;

						//�ӽ��մ��ڵ�һ��λ�ÿ�ʼ�Ϸ����ݣ�ֱ��ѭ������β��������ݽ���
						int temp_front = window_r->front;
						while (temp_front != window_r->rear)
						{
							temp_front = (temp_front + 1) % RW_SIZE;
							if (window_r->sign[temp_front] != 0)
							{
								int send_len;
								int send_seq;
								U8* send_buf;
								send_len = window_r->len[temp_front];
								send_buf = (U8*)malloc(send_len + 40);
								send_buf = dequeue_rw(window_r, &send_len, &send_seq);
								if (PRINT) {
									cout << "���մ���Ŀǰ��С��" << (window_r->rear + 128 - window_r->front) % 128 << endl;
									cout << "���մ�������  window_r->front " << window_r->front << " window_r->rear " << window_r->rear << endl;
									for (int i = (window_r->front + 1) % RW_SIZE; i != (window_r->rear + 1) % RW_SIZE; i = (i + 1) % RW_SIZE)
										cout << window_r->seq[i] << " ";
									cout << endl;
								}
								send_to_upper(send_buf, send_len);
								last_upseq = send_seq;

								//��ӡ��Ϣ�����ϲ㷢��֡��	
								if (PRINT)
								{
									cout << endl;
									cout << "///////////////////////////////////���ϲ㷢��֡///////////////////////////////////" << endl << endl;
									cout << "֡�����Ϊ��" << send_seq << endl << endl;
									cout << "��Ϣ������Ϊ��";
									for (int w = 0; w < send_len; w++) {
										cout << send_buf[w];
									}
									cout << endl << endl;
									//cout << "��Ϣ��ʮ��������Ϊ��" << endl;
									//for (int w = 0; w < send_len; w++)
									//	cout << int(send_buf[w]) << " ";

									cout << endl << endl;
									U8* bit_buf = (U8*)malloc(send_len * 8 + 40);
									ByteArrayToBitArray(bit_buf, send_len * 8, send_buf, send_len);
									cout << "��Ϣ�ı�����Ϊ��";
									for (int w = 0; w < send_len * 8; w++) {
										cout << int(bit_buf[w]);
										if (w % 4 == 3) {
											cout << " ";
										}
									}
									if (bit_buf != NULL) {
										free(bit_buf);
									}
									cout << endl;
								}
							}
							else//ѭ�����˴������ݣ�break�˳�ѭ��
							{
								break;
							}

						}
					}

				}
				else//��Ų�Ϊ(last_upseq+1)%128
				{
					if (window_r->front == window_r->rear)//���մ���Ϊ��
					{
						int zero_count = zero_count = (seq + 128 - last_upseq) % 128;;//ͳ����Ҫ��նӵĴ�������Ҫ���ڽ�����ѭ��
						if (zero_count > 0 && zero_count < 20)
						{
							for (int i = (last_upseq + 1) % 128; zero_count > 1; i = (i + 1) % 128)
							{//��ӿյ�����ռλ��
								zero_count--;
								enqueue_rw(window_r, zero_buf, 0, i, 0);
								if (PRINT) {
									cout << "���մ���Ŀǰ��С��" << (window_r->rear + 128 - window_r->front) % 128 << endl;
									cout << "���մ�������  window_r->front " << window_r->front << " window_r->rear " << window_r->rear << endl;
									for (int i = (window_r->front + 1) % RW_SIZE; i != (window_r->rear + 1) % RW_SIZE; i = (i + 1) % RW_SIZE)
										cout << window_r->seq[i] << " ";
									cout << endl;
								}
							}
							enqueue_rw(window_r, buf_real, iSndRetval, seq, 1);//��ӵ�ǰ����
							if (PRINT) {
								cout << "���մ���Ŀǰ��С��" << (window_r->rear + 128 - window_r->front) % 128 << endl;
								cout << "���մ�������  window_r->front " << window_r->front << " window_r->rear " << window_r->rear << endl;
								for (int i = (window_r->front + 1) % RW_SIZE; i != (window_r->rear + 1) % RW_SIZE; i = (i + 1) % RW_SIZE)
									cout << window_r->seq[i] << " ";
								cout << endl;
							}
						}

					}
					else//���մ��ڲ�Ϊ��
					{
						int flag2 = 0;//�жϸ�֡�Ƿ�Ϊ��ʱ�ش�֡���ǣ�1 ��0
						int temp_front = window_r->front;
						while (temp_front != window_r->rear)
						{
							temp_front = (temp_front + 1) % RW_SIZE;
							if (window_r->seq[temp_front] == seq)
							{
								flag2 = 1;//��Ϊ��ʱ�ش�֡���޸Ľ��մ�������Ӧ֡����
								window_r->data[temp_front] = buf_real;
								window_r->len[temp_front] = iSndRetval;
								window_r->seq[temp_front] = seq;
								window_r->sign[temp_front] = 1;
								break;
							}
						}
						if (flag2 == 0) {//��֡��Ϊ��ʱ�ش�֡��Ҳ�п����ǳ�ʱ�ش�֡����ack����ʱ���ܻ���ֳ�ʱ�ش�֡��

							int zero_count;//ͳ����Ҫ��նӵĴ�������Ҫ���ڽ�����ѭ��
							if ((seq == 0 || seq == 1 || seq == 2 || seq == 3) && (seq + 128 - last_upseq) % 128 < 64)
							{
								zero_count = (seq + 128 - window_r->rear[window_r->seq]) % 128;
							}
							else
							{
								zero_count = seq - window_r->rear[window_r->seq];
							}
							if (zero_count > 0 && zero_count < 20)
							{
								for (int i = (window_r->rear[window_r->seq] + 1) % 128; zero_count > 1; i = (i + 1) % 128)
								{//��ӿյ�����ռλ��
									zero_count--;
									enqueue_rw(window_r, zero_buf, 0, i, 0);
									if (PRINT) {
										cout << "���մ���Ŀǰ��С��" << (window_r->rear + 128 - window_r->front) % 128 << endl;
										cout << "���մ�������  window_r->front " << window_r->front << " window_r->rear " << window_r->rear << endl;
										for (int i = (window_r->front + 1) % RW_SIZE; i != (window_r->rear + 1) % RW_SIZE; i = (i + 1) % RW_SIZE)
											cout << window_r->seq[i] << " ";
										cout << endl;
									}
								}
								enqueue_rw(window_r, buf_real, iSndRetval, seq, 1);//��ӵ�ǰ����
								if (PRINT) {
									cout << "���մ���Ŀǰ��С��" << (window_r->rear + 128 - window_r->front) % 128 << endl;
									cout << "���մ�������  window_r->front " << window_r->front << " window_r->rear " << window_r->rear << endl;
									for (int i = (window_r->front + 1) % RW_SIZE; i != (window_r->rear + 1) % RW_SIZE; i = (i + 1) % RW_SIZE)
										cout << window_r->seq[i] << " ";
									cout << endl;
								}
							}
						}
					}
				}
			}
		}
		else if (flag == '6')//����֡������ȷ
		{
			if (mac_src == last_recvmac && seq == last_firstseq) //��һ���ж�ȫ��ʱ�䣿����
			{
				ack('1', index, strDevID.c_str()[0], mac_src);//�ú���������ack��������ack
				if (PRINT) {
					cout << "+++++++++++++++++++++++++++++++��֡�ظ�+++++++++++++++++++++++++++++++ " << endl;
					cout << "///////////////////////////////////����ACK֡///////////////////////////////////" << endl << endl;
					cout << "����һ�����Ϊ" << int(index) << "��ACK֡" << endl << endl;
				}
				return;
			}
			last_recvmac = mac_src;
			last_firstseq = seq;
			//last_sendmac = 0;
			//��ӡ��Ϣ������֡��
			if (PRINT)
			{
				cout << endl;
				cout << "///////////////////////////////////������֡///////////////////////////////////" << endl << endl;
				cout << "֡�����Ϊ��" << int(index) << endl << endl;
				cout << "Դ��ַΪ��" << mac_src << endl << endl;
				cout << "Ŀ�ĵ�ַΪ��" << mac_des << endl << endl;
				cout << "��Ϣ������Ϊ��";
				for (int w = 0; w < iSndRetval; w++) {
					cout << buf_real[w];
				}
				cout << endl << endl;
				cout << "��Ϣ��ʮ��������Ϊ��";
				for (int w = 0; w < iSndRetval; w++) {
					cout << int(buf_real[w]) << " ";
				}
				cout << endl << endl;

				U8* bit_buf = (U8*)malloc(iSndRetval * 8 + 40);
				ByteArrayToBitArray(bit_buf, iSndRetval * 8, buf_real, iSndRetval);
				cout << "��Ϣ�ı�����Ϊ��";
				for (int w = 0; w < iSndRetval * 8; w++) {
					cout << int(bit_buf[w]);
					if (w % 4 == 3) {
						cout << " ";
					}
				}
				if (bit_buf != NULL) {
					free(bit_buf);
				}
				cout << endl;
			}

			ack('1', index, strDevID.c_str()[0], mac_src);//�ú���������ack��������ack
			if (PRINT) {
				cout << endl;
				cout << "///////////////////////////////////����ACK֡///////////////////////////////////" << endl << endl;
				cout << "����һ�����Ϊ" << int(index) << "��ACK֡" << endl << endl;
			}


			if (window_r->front == window_r->rear)
			{//�����մ���Ϊ�գ�˵����ǰ���޴������ݣ�ֱ���Ϸ�
				send_to_upper(buf_real, iSndRetval);
				last_upseq = seq;

				//��ӡ��Ϣ�����ϲ㷢��֡��
				if (PRINT)
				{
					cout << endl;
					cout << "///////////////////////////////////���ϲ㷢��֡///////////////////////////////////" << endl << endl;
					cout << "֡�����Ϊ��" << int(index) << endl << endl;
					cout << "��Ϣ������Ϊ��";
					for (int w = 0; w < iSndRetval; w++) {
						cout << buf_real[w];
					}
					cout << endl << endl;
					cout << "��Ϣ��ʮ��������Ϊ��";
					for (int w = 0; w < iSndRetval; w++) {
						cout << int(buf_real[w]) << " ";
					}
					cout << endl << endl;

					U8* bit_buf = (U8*)malloc(iSndRetval * 8 + 80);
					ByteArrayToBitArray(bit_buf, iSndRetval * 8, buf_real, iSndRetval);
					cout << "��Ϣ�ı�����Ϊ��";
					for (int w = 0; w < iSndRetval * 8; w++) {
						cout << int(bit_buf[w]);
						if (w % 4 == 3) {
							cout << " ";
						}
					}
					if (bit_buf != NULL) {
						free(bit_buf);
					}
					cout << endl;
				}
			}
			else
			{//�����ܴ��ڲ�Ϊ�գ�˵����ǰ�д������ݣ��Ǹ�������ֱ�ӷ��봰�ڣ���
			 //��Ӧ����ڽ��մ��ڵ�һλ

				enqueue_rw(window_r, buf_real, iSndRetval, seq, 1);//��ӵ�ǰ����
				if (PRINT) {
					cout << "���մ���Ŀǰ��С��" << (window_r->rear + 128 - window_r->front) % 128 << endl;
					cout << "���մ�������  window_r->front " << window_r->front << " window_r->rear " << window_r->rear << endl;
					for (int i = (window_r->front + 1) % RW_SIZE; i != (window_r->rear + 1) % RW_SIZE; i = (i + 1) % RW_SIZE)
						cout << window_r->seq[i] << " ";
				}

			}
		}
		else//�Ȳ�Ϊ���ݣ��ֲ�Ϊack����û�г�������ʲô���������Ӧ�ò�����֡�
		{
			return;
		}
	}
}

void print_statistics()
{
	if (printCount % 10 == 0) {
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
		cout << "��ת�� " << iRcvForward << " λ��" << iRcvForwardCount << " �Σ�" << "�ݽ� " << iRcvToUpper << " λ��" << iRcvToUpperCount << " ��," << "���� " << iSndTotal << " λ��" << iSndTotalCount << " �Σ�" << "���Ͳ��ɹ� " << iSndErrorCount << " ��,""�յ�������Դ " << iRcvUnknownCount << " �Ρ�";
		spin++;
	}

}
void menu()
{
	int selection;
	unsigned short port;
	int iSndRetval;
	char kbBuf[100];
	int len;
	U8* bufSend;
	//����|��ӡ��[���Ϳ��ƣ�0���ȴ��������룻1���Զ���][��ӡ���ƣ�0�������ڴ�ӡͳ����Ϣ��1����bit����ӡ���ݣ�2���ֽ�����ӡ����]
	cout << endl << endl << "�豸��:" << strDevID << ",    ���:" << strLayer << ",    ʵ���:" << strEntity;
	cout << endl << "1-�����Զ�����(��Ч);" << endl << "2-ֹͣ�Զ����ͣ���Ч��; " << endl << "3-�Ӽ������뷢��; ";
	cout << endl << "4-����ӡͳ����Ϣ; " << endl << "5-����������ӡ��������;" << endl << "6-���ֽ�����ӡ��������;";
	cout << endl << "0-ȡ��" << endl << "����������ѡ�����";
	cin >> selection;
	switch (selection) {
	case 0:

		break;
	case 1:
		iWorkMode = 10 + iWorkMode % 10;
		break;
	case 2:
		iWorkMode = iWorkMode % 10;
		break;
	case 3:
		cout << "�����ַ���(,������100�ַ�)��";
		cin >> kbBuf;
		cout << "����Ͳ�ӿںţ�";
		cin >> port;

		len = (int)strlen(kbBuf) + 1; //�ַ�������и�������
		if (port >= lowerNumber) {
			cout << "û������ӿ�" << endl;
			return;
		}
		if (lowerMode[port] == 0) {
			//�²�ӿ��Ǳ���������,��ҪһƬ�µĻ�����ת����ʽ
			bufSend = (U8*)malloc(len * 8 + 40);

			iSndRetval = ByteArrayToBitArray(bufSend, len * 8, kbBuf, len);
			iSndRetval = SendtoLower(bufSend, iSndRetval, port);
			if (bufSend != NULL) {
				free(bufSend);
			}
		}
		else {
			//�²�ӿ����ֽ����飬ֱ�ӷ���
			iSndRetval = SendtoLower(kbBuf, len, port);
			iSndRetval = iSndRetval * 8; //�����λ
		}
		//����ͳ��
		if (iSndRetval > 0) {
			iSndTotalCount++;
			iSndTotal += iSndRetval;
		}
		else {
			iSndErrorCount++;
		}
		//��Ҫ��Ҫ��ӡ����
		cout << endl << "��ӿ� " << port << " �������ݣ�" << endl;
		switch (iWorkMode % 10) {
		case 1:
			print_data_bit(kbBuf, len, 1);
			break;
		case 2:
			print_data_byte(kbBuf, len, 1);
			break;
		case 0:
			break;
		}
		break;
	case 4:
		iWorkMode = (iWorkMode / 10) * 10 + 0;
		break;
	case 5:
		iWorkMode = (iWorkMode / 10) * 10 + 1;
		break;
	case 6:
		iWorkMode = (iWorkMode / 10) * 10 + 2;
		break;
	}

}