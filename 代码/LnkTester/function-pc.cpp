//Nettester 的功能文件
#include <iostream>
#include <conio.h>
#include "winsock.h"
#include "stdio.h"
#include "CfgFileParms.h"
#include "function.h"
using namespace std;

#define true 1
#define false 0
#define SW_SIZE 5120 //发送窗口的大小
#define RW_SIZE 5120 // 接收窗口大小
#define BUF_SIZE 10240 // 发送缓冲区的大小
#define MAX_LEN 80 //分帧长度
#define PRINT 0 //打印信息

//以下为重要的变量
U8* sendbuf;        //用来组织发送数据的缓存，大小为MAX_BUFFER_SIZE,可以在这个基础上扩充设计，形成适合的结构，例程中没有使用，只是提醒一下
int printCount = 0; //打印控制
int spin = 0;  //打印动态信息控制

int wrongseqlist[RW_SIZE];//接收错误帧的序号列表
int wrong_index = -1;//错误帧序号列表的索引
int last_recvmac = 0; // 上一次收到的MAC地址
int last_sendmac = 0; // 上一次发往的MAC地址
int last_upseq = -1;//上一次发送到上层数据的序号
int last_firstseq = -1; // 上次首帧序号
int last_firsttime = -1; // 上次首帧时间

//------华丽的分割线，一些统计用的全局变量------------
int iSndTotal = 0;  //发送数据总量
int iSndTotalCount = 0; //发送数据总次数
int iSndErrorCount = 0;  //发送错误次数
int iRcvForward = 0;     //转发数据总量
int iRcvForwardCount = 0; //转发数据总次数
int iRcvToUpper = 0;      //从低层递交高层数据总量
int iRcvToUpperCount = 0;  //从低层递交高层数据总次数
int iRcvUnknownCount = 0;  //收到不明来源数据总次数

int recLen = 0; //用于分组的长度判断
U8(*TEMP)[MAX_LEN - 1] = NULL; //分帧缓存
int TEMP_INDEX = 0; //分帧的序号


typedef struct {
	U8* data[BUF_SIZE];
	int front; //指向发送窗口的前一格
	int rear; //指向发送窗口的最后一格
	int mid; // 指向最后一个已发送但未确认的数据
	int buf_area; // 进入缓冲区，发送窗口放不下，未发送数据的最后一格
	int len[BUF_SIZE]; //帧的长度
	int seq[BUF_SIZE]; //帧的序号
	int tickTack[BUF_SIZE]; //定时，决定是否重发
}send_window;

typedef struct {
	U8* data[RW_SIZE]; //就是接收窗口的大小
	int front; // 接收窗口的前一格
	int rear; // 接收窗口中未被上层取走的数据的最后一格
	int len[RW_SIZE];
	int seq[RW_SIZE];
	int sign[RW_SIZE];
}receive_window;

// 发送窗口的初始化
void init_sw(send_window* q)
{
	q->front = -1; //发送窗口的左边沿
	q->rear = (q->front + SW_SIZE) % BUF_SIZE; //发送窗口的右边沿
	q->mid = -1; //发送窗口中间区分是否已发送的
	q->buf_area = q->rear; //未在窗口内但加入了缓冲区的指示
}

// *q：发送窗口指针；buf：待加入发送缓冲区的数据；seq：待加入数据的序号
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

// *q：发送窗口指针；seq：收到ack的数据的序号
int dequeue_sw(send_window* q, int seq)
{
	int flag = 0; //判断发送窗口front-mid之间是否有此序号的数据
	if (q != NULL)
	{
		if (q->mid == q->front)// 发送窗口中没有数据
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
					//cout << "是不是0啊" << endl << endl;
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
		if (q->rear == q->front) //接收窗口是空的
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
	if (q1->mid == q1->rear) //此时发送窗口外可能有数据
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
	else //此时数据均在发送窗口内
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

//发送窗口放的是没有解封的、类型为bit数组的数据
//接收窗口放的是已经解封了的、类型为byte数组的数据

//打印统计信息
void print_statistics();
void menu();

//和校验计算（放在add_head中使用）
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

//和校验计算（放在add_head中使用）
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

//加入帧头部依次为
//input:原本的数据、原本数据长度、模式flag（为‘2’是数据，‘0’和‘1’代表nak和ack）、index序号
//output:frame加好头部的帧（定界符、校验位）、 length帧的长度
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

//从bit流中得出数据
//input:接收的bit数据、bit数据长度
//output:buf_real真实的byte数据,,length数据长度（如果为错则output=0）、flag这个数据的模式、index这个数据的序号
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
//发送ack或者nack（根据flag进行判断）
//input:flag为'1'是ack，为'0'是nck(),index为需要回发确认的数据序号
//output:无
//注意flag为U8，为‘1’才是ack，不是1
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
	//发送
	SendtoLower(bufSend, new_len * 8, 0); //参数依次为数据缓冲，长度，接口号
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

//分帧函数，用于将长数据分帧
void Framing(U8* buf, int len, U8(*frame)[MAX_LEN], int* num, int* last_len) {
	U8 F = 0x7e;
	int num_ = 0;//帧的数量
	int lastlen = MAX_LEN;//最后一帧的长度
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

//带有分帧整合功能，直接替换SendtoUpper
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

//***************重要函数提醒******************************
//名称：InitFunction
//功能：初始化功能面，由main函数在读完配置文件，正式进入驱动机制前调用
//输入：
//输出：
void InitFunction(CCfgFileParms& cfgParms)
{
	/*
	sendbuf = (char*)malloc(MAX_BUFFER_SIZE);
	if (sendbuf == NULL ) {
		cout << "内存不够" << endl;
		//这个，计算机也太，退出吧
		exit(0);
	}
	*/
	init_sw(window_s);
	init_rw(window_r);
	return;
}
//***************重要函数提醒******************************
//名称：EndFunction
//功能：结束功能面，由main函数在收到exit命令，整个程序退出前调用
//输入：
//输出：
void EndFunction()
{
	free_queue(window_s, window_r);
	return;
}

//***************重要函数提醒******************************
//名称：TimeOut
//功能：本函数被调用时，意味着sBasicTimer中设置的超时时间到了，
//      函数内容可以全部替换为设计者自己的想法
//      例程中实现了几个同时进行功能，供参考
//      1)根据iWorkMode工作模式，判断是否将键盘输入的数据发送，
//        因为scanf会阻塞，导致计时器在等待键盘的时候完全失效，所以使用_kbhit()无阻塞、不间断地在计时的控制下判断键盘状态，这个点Get到没？
//      2)不断刷新打印各种统计值，通过打印控制符的控制，可以始终保持在同一行打印，Get？
//输入：时间到了就触发，只能通过全局变量供给输入
//输出：这就是个不断努力干活的老实孩子
void TimeOut() //超时重传
{
	if (_kbhit()) {
		//键盘有动作，进入菜单模式
		menu();
	}

	int temp_front = window_s->front;
	while (temp_front != window_s->mid)
	{

		temp_front = (temp_front + 1) % BUF_SIZE;
		//cout << "-----------------------------超时序号是多少啊呜呜呜----------" << endl << endl;
		//cout << "此时的序号" << window_s->seq[temp_front] << endl << endl;
		window_s->tickTack[temp_front]++;

		if (window_s->tickTack[temp_front] == 10)//ms重传一次
		{
			SendtoLower(window_s->data[temp_front], window_s->len[temp_front], 0);
			window_s->tickTack[temp_front] = 0;

			//以下都作为打印信息用

			U8* bufSend;
			bufSend = (U8*)malloc(window_s->len[temp_front] / 8 + 40);
			U8 mac_src;
			U8 mac_des;
			int iSndRetval;
			char flag;
			char index;
			get_data(window_s->data[temp_front], window_s->len[temp_front], bufSend, &iSndRetval, &flag, &index, &mac_src, &mac_des);
			//打印信息（超时重传帧）
			if (PRINT)
			{
				cout << endl;
				cout << "///////////////////////////////////超时重传帧///////////////////////////////////" << window_s->len[temp_front] << endl << endl;
				cout << "帧的序号为：" << int(index) << endl << endl;
				cout << "信息的内容为：";
				for (int w = 0; w < iSndRetval; w++) {
					cout << bufSend[w];
				}
				cout << endl << endl;
				cout << "信息的十进制内容为：";
				for (int w = 0; w < iSndRetval; w++) {
					cout << int(bufSend[w]) << " ";
				}
				cout << endl << endl;

				U8* bit_buf = (U8*)malloc(iSndRetval * 8 + 80);
				ByteArrayToBitArray(bit_buf, iSndRetval * 8, bufSend, iSndRetval);
				cout << "信息的比特流为：";
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
//------------华丽的分割线，以下是数据的收发,--------------------------------------------

//***************重要函数提醒******************************
//名称：RecvfromUpper
//功能：本函数被调用时，意味着收到一份高层下发的数据
//      函数内容全部可以替换成设计者自己的
//      例程功能介绍
//         1)通过低层的数据格式参数lowerMode，判断要不要将数据转换成bit流数组发送，发送只发给低层接口0，
//           因为没有任何可供参考的策略，讲道理是应该根据目的地址在多个接口中选择转发的。
//         2)判断iWorkMode，看看是不是需要将发送的数据内容都打印，调试时可以，正式运行时不建议将内容全部打印。
//输入：U8 * buf,高层传进来的数据， int len，数据长度，单位字节
//输出：
void RecvfromUpper(U8* buf, int len)
{//从高层接收到的为字节数组
	int iSndRetval;

	U8* bufSend = NULL; //bufSend是bit数组
	U8* frame;
	U8(*frames)[MAX_LEN] = NULL;//帧数组



	U8 mac_src;//源mac
	U8 mac_des;//目的mac
	int new_len;
	char index = iSndTotalCount % 128;; //序号的字符形式
	int seq = int(index); //序号的整数形式
	int num;//多少个小帧
	int last_len;//最后的小帧长



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
	if (mac_des == last_sendmac) // 与上次发送的MAC地址相同
		//增加信息，封装成帧（字节形式时进行封装） buf->frame
		add_head(buf, len, frame, &new_len, '2', &index, mac_src, mac_des);
	else
		add_head(buf, len, frame, &new_len, '6', &index, mac_src, mac_des);
	last_sendmac = mac_des;

	bufSend = (char*)malloc(new_len * 8 + 40);
	if (bufSend == NULL) {
		return;
	}

	//将字节转换为比特，发送至下层 frame->bufSend
	iSndRetval = ByteArrayToBitArray(bufSend, new_len * 8, frame, new_len);

	//打印信息（发送帧）
	if (PRINT)
	{
		cout << endl;
		cout << "----------------------------------发送帧-----------------------------------" << endl << endl;
		cout << "帧的序号为：" << int(frame[1]) << endl << endl;
		cout << "帧的校验为：" << int(frame[new_len - 2]) << endl << endl;
		cout << "源地址为：" << mac_src << endl << endl;
		cout << "目的地址为：" << mac_des << endl << endl;
		cout << "信息的内容为：";
		for (int w = 3; w < new_len - 2; w++) {
			cout << frame[w];
		}
		cout << endl << endl;
		cout << "帧的十进制内容为：";
		for (int w = 0; w < new_len; w++) {
			cout << int(frame[w]) << " ";
		}
		cout << endl << endl;
		cout << "待发送比特流为：";
		for (int w = 0; w < new_len * 8; w++) {
			cout << int(bufSend[w]);
			if (w % 4 == 3) {
				cout << " ";
			}
		}
		cout << endl;
	}


	//发送
	SendtoLower(bufSend, iSndRetval, 0); //参数依次为数据缓冲，长度，接口号

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
	// 将收到的并进行封装后的数据放入发送窗口（放入发送窗口的为bit数组）
	enqueue_sw(window_s, bufSend, iSndRetval, seq);
	//cout << "发送窗口目前大小：" << (window_s->rear + 128 - window_r->front) % 128 << endl;
	if (PRINT) {
		cout << "发送窗口数据  window_s->front " << window_s->front << " window_s->mid " << window_s->mid << " window_s->rear " << window_s->rear << " window_s->buf_area " << window_s->buf_area << endl;
		for (int i = (window_s->front + 1) % BUF_SIZE; i != (window_s->mid + 1) % BUF_SIZE; i = (i + 1) % BUF_SIZE)
			cout << window_s->seq[i] << " ";
		cout << endl;
		cout << "按顺序打印发送窗口中的数据（从0开始）" << endl;
		for (int i = 0; i < SW_SIZE; i++)
			cout << window_s->seq[i] << " ";
		cout << endl;
	}
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
void RecvfromLower(U8* buf, int len, int ifNo)
{//从下层接收到的数据类型为bit数组
	int iSndRetval; //解封后的长度

	U8* bufSend;
	U8* buf_real; //解封后的buf
	U8* zero_buf = NULL; //放入接收窗口的空数据

	U8 mac_src;
	U8 mac_des;
	char flag; //判断buf是何种类型（数据 or ACK）
	char index; //序号的字符形式
	int seq; //序号的整数形式
	int sign; //接收到帧是否出错的标志 1：正确；0：错误
	int sign1 = 0; //判断某正确帧是否是出错帧的重传帧 1：是 0：不是
	int flag1 = 0; //判断某出错帧是否之前出错过了（是否已加入wrongseqlist） 1：是 0：否

	bufSend = (U8*)malloc(len + 80);
	buf_real = (U8*)malloc(len / 8 + 10);

	for (int i = 0; i < len; i++)
	{
		bufSend[i] = buf[i]; //新建一个bit数组对buf完全复制，减小系统影响
	}

	// 代码已做了较大改动，该注释部分需要修改
	// 对接收到的帧进行解封，
	// 判断是否错误：若错误，则丢弃，等超时重传后重发该帧，且之后接收的数据放入接收窗口，
	// 待再次收到该帧后，将该帧解封后转发到上层，将队列中的帧解封后均转发到上层；（记录下
	// 发送的序号进行解决）
	// 若未错误，判断是数据还是ack：
	// 如果是数据，就直接转发到上层（如果之前没有出错的数据），或入队于相应的序号处（之前有
	// 出错的数据），且向下层发送该序号的ack；
	// 如果是ack，就发送窗口出队某序号的帧；

	//获得解封后的，bufSend（为bit）->buf_real（为byte）
	get_data(bufSend, len, buf_real, &iSndRetval, &flag, &index, &mac_src, &mac_des);
	seq = (int)index;

	if (iSndRetval == 0) //数据出错，直接丢弃（不管）
	{
		if (PRINT) {
			cout << endl;
			cout << "///////////////////////////////////帧出错///////////////////////////////////" << endl << endl;
			cout << "出错的帧序号为：" << seq << endl << endl;
		}
		return;
	}
	else
	{
		//判断帧是否属于自己
		if (mac_des != strDevID.c_str()[0] && mac_des != 'F') {
			if (PRINT) {
				cout << "++++++++++++++++++++++++++++++++++++++++++++++++" << endl << endl;
				cout << "收到一个不属于自己的帧" << endl;
				cout << "帧的序号为：" << int(index) << endl << endl;
				cout << "源地址为：" << mac_src << endl << endl;
				cout << "目的地址为：" << mac_des << endl << endl;
				cout << "++++++++++++++++++++++++++++++++++++++++++++++++" << endl << endl;
				return;
			}
		}

		if (flag == '1') //是ack
		{
			if (PRINT) {
				cout << endl;
				cout << "///////////////////////////////////接受ACK帧///////////////////////////////////" << endl << endl;
				cout << "收到一个序号为" << int(index) << "的ACK帧" << endl << endl;
				//cout << "帧的序号为：" << int(index) << endl << endl;
				cout << "信息的内容为：";
				for (int w = 0; w < iSndRetval; w++) {
					cout << buf_real[w];
				}
				cout << endl << endl;
			}
			dequeue_sw(window_s, seq);
			//cout << "发送窗口目前大小：" << (window_s->rear + 128 - window_r->front) % 128 << endl;
			if (PRINT) {
				cout << "发送窗口数据  window_s->front " << window_s->front << " window_s->mid " << window_s->mid << " window_s->rear " << window_s->rear << " window_s->buf_area " << window_s->buf_area << endl;
				for (int i = (window_s->front + 1) % BUF_SIZE; i != (window_s->mid + 1) % BUF_SIZE; i = (i + 1) % BUF_SIZE)
					cout << window_s->seq[i] << " ";
				cout << endl;
				cout << "按顺序打印发送窗口中的数据（从0开始）" << endl;
				for (int i = 0; i < SW_SIZE; i++)
					cout << window_s->seq[i] << " ";
				cout << endl;
			}
			//}
		}
		else if (flag == '2')//是数据，且正确
		{
			cout << int(index) << endl;
			if (mac_src != last_recvmac) {
				if (PRINT) {
					cout << endl;
					cout << "///////////////////////////////////收到来自不期望MAC地址的数据帧///////////////////////////////////" << endl << endl;
					cout << "帧的序号为：" << int(index) << endl << endl;
					cout << "源地址为：" << mac_src << endl << endl;
					cout << "目的地址为：" << mac_des << endl << endl;
				}
			}
			else {
				//打印信息（接收帧）
				if (PRINT)
				{
					cout << endl;
					cout << "///////////////////////////////////接收帧///////////////////////////////////" << endl << endl;
					cout << "帧的序号为：" << int(index) << endl << endl;
					cout << "源地址为：" << mac_src << endl << endl;
					cout << "目的地址为：" << mac_des << endl << endl;
					cout << "信息的内容为：";
					for (int w = 0; w < iSndRetval; w++) {
						cout << buf_real[w];
					}
					cout << endl << endl;
					cout << "信息的十进制内容为：";
					for (int w = 0; w < iSndRetval; w++) {
						cout << int(buf_real[w]) << " ";
					}
					cout << endl << endl;

					U8* bit_buf = (U8*)malloc(iSndRetval * 80);
					ByteArrayToBitArray(bit_buf, iSndRetval * 8, buf_real, iSndRetval);
					cout << "信息的比特流为：";
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

				ack('1', index, strDevID.c_str()[0], mac_src);//该函数创建了ack，并发送ack

				if (PRINT) {
					cout << endl;
					cout << "///////////////////////////////////发送ACK帧///////////////////////////////////" << endl << endl;
					cout << "发送一个序号为" << int(index) << "的ACK帧" << endl << endl;
				}



				if (seq == (last_upseq + 1) % 128)
				{//该帧序号为上一次发送到上层数据序号加1，分两种情况
					if (window_r->front == window_r->rear)
					{//若接收窗口为空，说明此前已无错误数据，直接上发
						send_to_upper(buf_real, iSndRetval);
						//打印信息（向上层发送帧）
						if (PRINT)
						{
							cout << endl;
							cout << "///////////////////////////////////向上层发送帧///////////////////////////////////" << endl << endl;
							cout << "帧的序号为：" << int(index) << endl << endl;
							cout << "信息的内容为：";
							for (int w = 0; w < iSndRetval; w++) {
								cout << buf_real[w];
							}
							cout << endl << endl;
							cout << "信息的十进制内容为：";
							for (int w = 0; w < iSndRetval; w++) {
								cout << int(buf_real[w]) << " ";
							}
							cout << endl << endl;

							U8* bit_buf = (U8*)malloc(iSndRetval * 80);
							ByteArrayToBitArray(bit_buf, iSndRetval * 8, buf_real, iSndRetval);
							cout << "信息的比特流为：";
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
					{//若接受窗口不为空，说明此前有错误数据，那么该数据为错误重传帧，且
					 //对应序号在接收窗口第一位

						//修改第一个位置的值
						window_r->data[window_r->front + 1] = buf_real;
						window_r->len[window_r->front + 1] = iSndRetval;
						window_r->seq[window_r->front + 1] = seq;
						window_r->sign[window_r->front + 1] = 1;

						//从接收窗口第一个位置开始上发数据，直到循环到队尾或错误数据结束
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
									cout << "接收窗口目前大小：" << (window_r->rear + 128 - window_r->front) % 128 << endl;
									cout << "接收窗口数据  window_r->front " << window_r->front << " window_r->rear " << window_r->rear << endl;
									for (int i = (window_r->front + 1) % RW_SIZE; i != (window_r->rear + 1) % RW_SIZE; i = (i + 1) % RW_SIZE)
										cout << window_r->seq[i] << " ";
									cout << endl;
								}
								send_to_upper(send_buf, send_len);
								last_upseq = send_seq;

								//打印信息（向上层发送帧）	
								if (PRINT)
								{
									cout << endl;
									cout << "///////////////////////////////////向上层发送帧///////////////////////////////////" << endl << endl;
									cout << "帧的序号为：" << send_seq << endl << endl;
									cout << "信息的内容为：";
									for (int w = 0; w < send_len; w++) {
										cout << send_buf[w];
									}
									cout << endl << endl;
									//cout << "信息的十进制内容为：" << endl;
									//for (int w = 0; w < send_len; w++)
									//	cout << int(send_buf[w]) << " ";

									cout << endl << endl;
									U8* bit_buf = (U8*)malloc(send_len * 8 + 40);
									ByteArrayToBitArray(bit_buf, send_len * 8, send_buf, send_len);
									cout << "信息的比特流为：";
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
							else//循环到了错误数据，break退出循环
							{
								break;
							}

						}
					}

				}
				else//序号不为(last_upseq+1)%128
				{
					if (window_r->front == window_r->rear)//接收窗口为空
					{
						int zero_count = zero_count = (seq + 128 - last_upseq) % 128;;//统计需要入空队的次数，主要用于解决序号循环
						if (zero_count > 0 && zero_count < 20)
						{
							for (int i = (last_upseq + 1) % 128; zero_count > 1; i = (i + 1) % 128)
							{//入队空的数据占位置
								zero_count--;
								enqueue_rw(window_r, zero_buf, 0, i, 0);
								if (PRINT) {
									cout << "接收窗口目前大小：" << (window_r->rear + 128 - window_r->front) % 128 << endl;
									cout << "接收窗口数据  window_r->front " << window_r->front << " window_r->rear " << window_r->rear << endl;
									for (int i = (window_r->front + 1) % RW_SIZE; i != (window_r->rear + 1) % RW_SIZE; i = (i + 1) % RW_SIZE)
										cout << window_r->seq[i] << " ";
									cout << endl;
								}
							}
							enqueue_rw(window_r, buf_real, iSndRetval, seq, 1);//入队当前数据
							if (PRINT) {
								cout << "接收窗口目前大小：" << (window_r->rear + 128 - window_r->front) % 128 << endl;
								cout << "接收窗口数据  window_r->front " << window_r->front << " window_r->rear " << window_r->rear << endl;
								for (int i = (window_r->front + 1) % RW_SIZE; i != (window_r->rear + 1) % RW_SIZE; i = (i + 1) % RW_SIZE)
									cout << window_r->seq[i] << " ";
								cout << endl;
							}
						}

					}
					else//接收窗口不为空
					{
						int flag2 = 0;//判断该帧是否为超时重传帧，是：1 否：0
						int temp_front = window_r->front;
						while (temp_front != window_r->rear)
						{
							temp_front = (temp_front + 1) % RW_SIZE;
							if (window_r->seq[temp_front] == seq)
							{
								flag2 = 1;//若为超时重传帧，修改接收窗口中相应帧即可
								window_r->data[temp_front] = buf_real;
								window_r->len[temp_front] = iSndRetval;
								window_r->seq[temp_front] = seq;
								window_r->sign[temp_front] = 1;
								break;
							}
						}
						if (flag2 == 0) {//该帧不为超时重传帧（也有可能是超时重传帧，当ack出错时可能会出现超时重传帧）

							int zero_count;//统计需要入空队的次数，主要用于解决序号循环
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
								{//入队空的数据占位置
									zero_count--;
									enqueue_rw(window_r, zero_buf, 0, i, 0);
									if (PRINT) {
										cout << "接收窗口目前大小：" << (window_r->rear + 128 - window_r->front) % 128 << endl;
										cout << "接收窗口数据  window_r->front " << window_r->front << " window_r->rear " << window_r->rear << endl;
										for (int i = (window_r->front + 1) % RW_SIZE; i != (window_r->rear + 1) % RW_SIZE; i = (i + 1) % RW_SIZE)
											cout << window_r->seq[i] << " ";
										cout << endl;
									}
								}
								enqueue_rw(window_r, buf_real, iSndRetval, seq, 1);//入队当前数据
								if (PRINT) {
									cout << "接收窗口目前大小：" << (window_r->rear + 128 - window_r->front) % 128 << endl;
									cout << "接收窗口数据  window_r->front " << window_r->front << " window_r->rear " << window_r->rear << endl;
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
		else if (flag == '6')//是首帧，且正确
		{
			if (mac_src == last_recvmac && seq == last_firstseq) //加一个判断全局时间？？？
			{
				ack('1', index, strDevID.c_str()[0], mac_src);//该函数创建了ack，并发送ack
				if (PRINT) {
					cout << "+++++++++++++++++++++++++++++++首帧重复+++++++++++++++++++++++++++++++ " << endl;
					cout << "///////////////////////////////////发送ACK帧///////////////////////////////////" << endl << endl;
					cout << "发送一个序号为" << int(index) << "的ACK帧" << endl << endl;
				}
				return;
			}
			last_recvmac = mac_src;
			last_firstseq = seq;
			//last_sendmac = 0;
			//打印信息（接收帧）
			if (PRINT)
			{
				cout << endl;
				cout << "///////////////////////////////////接收首帧///////////////////////////////////" << endl << endl;
				cout << "帧的序号为：" << int(index) << endl << endl;
				cout << "源地址为：" << mac_src << endl << endl;
				cout << "目的地址为：" << mac_des << endl << endl;
				cout << "信息的内容为：";
				for (int w = 0; w < iSndRetval; w++) {
					cout << buf_real[w];
				}
				cout << endl << endl;
				cout << "信息的十进制内容为：";
				for (int w = 0; w < iSndRetval; w++) {
					cout << int(buf_real[w]) << " ";
				}
				cout << endl << endl;

				U8* bit_buf = (U8*)malloc(iSndRetval * 8 + 40);
				ByteArrayToBitArray(bit_buf, iSndRetval * 8, buf_real, iSndRetval);
				cout << "信息的比特流为：";
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

			ack('1', index, strDevID.c_str()[0], mac_src);//该函数创建了ack，并发送ack
			if (PRINT) {
				cout << endl;
				cout << "///////////////////////////////////发送ACK帧///////////////////////////////////" << endl << endl;
				cout << "发送一个序号为" << int(index) << "的ACK帧" << endl << endl;
			}


			if (window_r->front == window_r->rear)
			{//若接收窗口为空，说明此前已无错误数据，直接上发
				send_to_upper(buf_real, iSndRetval);
				last_upseq = seq;

				//打印信息（向上层发送帧）
				if (PRINT)
				{
					cout << endl;
					cout << "///////////////////////////////////向上层发送帧///////////////////////////////////" << endl << endl;
					cout << "帧的序号为：" << int(index) << endl << endl;
					cout << "信息的内容为：";
					for (int w = 0; w < iSndRetval; w++) {
						cout << buf_real[w];
					}
					cout << endl << endl;
					cout << "信息的十进制内容为：";
					for (int w = 0; w < iSndRetval; w++) {
						cout << int(buf_real[w]) << " ";
					}
					cout << endl << endl;

					U8* bit_buf = (U8*)malloc(iSndRetval * 8 + 80);
					ByteArrayToBitArray(bit_buf, iSndRetval * 8, buf_real, iSndRetval);
					cout << "信息的比特流为：";
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
			{//若接受窗口不为空，说明此前有错误数据，那个该数据直接放入窗口，且
			 //对应序号在接收窗口第一位

				enqueue_rw(window_r, buf_real, iSndRetval, seq, 1);//入队当前数据
				if (PRINT) {
					cout << "接收窗口目前大小：" << (window_r->rear + 128 - window_r->front) % 128 << endl;
					cout << "接收窗口数据  window_r->front " << window_r->front << " window_r->rear " << window_r->rear << endl;
					for (int i = (window_r->front + 1) % RW_SIZE; i != (window_r->rear + 1) % RW_SIZE; i = (i + 1) % RW_SIZE)
						cout << window_r->seq[i] << " ";
				}

			}
		}
		else//既不为数据，又不为ack，又没有出错，那是什么，这种情况应该不会出现。
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
		cout << "共转发 " << iRcvForward << " 位，" << iRcvForwardCount << " 次，" << "递交 " << iRcvToUpper << " 位，" << iRcvToUpperCount << " 次," << "发送 " << iSndTotal << " 位，" << iSndTotalCount << " 次，" << "发送不成功 " << iSndErrorCount << " 次,""收到不明来源 " << iRcvUnknownCount << " 次。";
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
	//发送|打印：[发送控制（0，等待键盘输入；1，自动）][打印控制（0，仅定期打印统计信息；1，按bit流打印数据，2按字节流打印数据]
	cout << endl << endl << "设备号:" << strDevID << ",    层次:" << strLayer << ",    实体号:" << strEntity;
	cout << endl << "1-启动自动发送(无效);" << endl << "2-停止自动发送（无效）; " << endl << "3-从键盘输入发送; ";
	cout << endl << "4-仅打印统计信息; " << endl << "5-按比特流打印数据内容;" << endl << "6-按字节流打印数据内容;";
	cout << endl << "0-取消" << endl << "请输入数字选择命令：";
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
		cout << "输入字符串(,不超过100字符)：";
		cin >> kbBuf;
		cout << "输入低层接口号：";
		cin >> port;

		len = (int)strlen(kbBuf) + 1; //字符串最后有个结束符
		if (port >= lowerNumber) {
			cout << "没有这个接口" << endl;
			return;
		}
		if (lowerMode[port] == 0) {
			//下层接口是比特流数组,需要一片新的缓冲来转换格式
			bufSend = (U8*)malloc(len * 8 + 40);

			iSndRetval = ByteArrayToBitArray(bufSend, len * 8, kbBuf, len);
			iSndRetval = SendtoLower(bufSend, iSndRetval, port);
			if (bufSend != NULL) {
				free(bufSend);
			}
		}
		else {
			//下层接口是字节数组，直接发送
			iSndRetval = SendtoLower(kbBuf, len, port);
			iSndRetval = iSndRetval * 8; //换算成位
		}
		//发送统计
		if (iSndRetval > 0) {
			iSndTotalCount++;
			iSndTotal += iSndRetval;
		}
		else {
			iSndErrorCount++;
		}
		//看要不要打印数据
		cout << endl << "向接口 " << port << " 发送数据：" << endl;
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