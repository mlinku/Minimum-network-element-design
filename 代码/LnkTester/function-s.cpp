//Nettester 的功能文件
#include <iostream>
#include <conio.h>
#include "winsock.h"
#include "stdio.h"
#include "CfgFileParms.h"
#include "function.h"
using namespace std;
#define PRINT 0 //打印信息
#define broadcast 'F' //广播帧目的地址

//以下为重要的变量
U8* sendbuf;        //用来组织发送数据的缓存，大小为MAX_BUFFER_SIZE,可以在这个基础上扩充设计，形成适合的结构，例程中没有使用，只是提醒一下
int printCount = 0; //打印控制
int spin = 0;  //打印动态信息控制

//------华丽的分割线，一些统计用的全局变量------------
int iSndTotal = 0;  //发送数据总量
int iSndTotalCount = 0; //发送数据总次数
int iSndErrorCount = 0;  //发送错误次数
int iRcvForward = 0;     //转发数据总量
int iRcvForwardCount = 0; //转发数据总次数
int iRcvToUpper = 0;      //从低层递交高层数据总量
int iRcvToUpperCount = 0;  //从低层递交高层数据总次数
int CLOCK = 200; //根节点竞选时钟
int CLOCK_BPDU = 150; //BPDU帧的时钟
int port_status[20];//端口状态
int BPDU_port = -1; //判断BPDU是否接收过
U8 belongto; //判断自己属于那个根交换机
int iRcvUnknownCount = 0;  //收到不明来源数据总次数
//转发表
typedef struct LIST {
	U8 content[60][2];
	int length = 0;
}list;
list change_list;

//和校验计算（放在add_head中使用）
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
		*length = U8(j + 4 + add_len);
	}
	else {
		frame[j] = E;
		frame[j + add_len] = mac_des;
		frame[j + 1 + add_len] = sum_data;
		*length = U8(j + 3 + add_len);
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

	U8* bit_real = (U8*)malloc(len);
	if (bit_real == NULL) {
		return;
	}

	U8* buf = (U8*)malloc(len / 8 + 1);
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
//交换机逆学习
void learning(U8 mac_src, int ifNo) {
	for (int i = 0; i < change_list.length; i++) {
		if (change_list.content[i][0] == mac_src) {
			change_list.content[i][1] = (U8)ifNo;
			return;
		}
	}
	change_list.content[change_list.length][0] = mac_src;
	change_list.content[change_list.length][1] = (U8)ifNo;
	change_list.length++;
}

//查询转发表
void find_how_to_give(U8 mac_des, int* ifNo) {
	for (int i = 0; i < change_list.length; i++) {
		if (change_list.content[i][0] == mac_des) {
			*ifNo = int(change_list.content[i][1]);
			return;
		}
	}
	*ifNo = 404;
}

//发送根节点竞选帧
void voteroot() {
	U8 data = 'R';
	U8* bufSend;
	U8* vote_root = (U8*)malloc(20);
	int vote_len = 0;
	U8 root = strDevID.c_str()[0];
	add_head(&data, 1, vote_root, &vote_len, 'R', &root, root, root);
	bufSend = (char*)malloc(vote_len * 8 + 16);
	ByteArrayToBitArray(bufSend, vote_len * 8, vote_root, vote_len);
	if (PRINT) {
		cout << "*********<<<<交换机生成树竞选根节点开始>>>>********" << endl << endl;
	}
	for (int j = 0; j < 5; j++) {
		for (int i = 0; i < lowerNumber; i++) {
			SendtoLower(bufSend, vote_len * 8, i); //参数依次为数据缓冲，长度，接口号
		}
	}
	if (vote_root != NULL) {
		free(vote_root);
	}
	if (bufSend != NULL) {
		free(bufSend);
	}
}

//发送BPDU帧
void send_BPDU() {
	U8 data = 'U';
	U8* bufSend;
	U8* BPDU_send = (U8*)malloc(20);
	int BPDU_len = 0;
	U8 root = strDevID.c_str()[0];
	add_head(&data, 1, BPDU_send, &BPDU_len, 'U', &root, root, root);
	bufSend = (char*)malloc(BPDU_len * 8 + 16);
	ByteArrayToBitArray(bufSend, BPDU_len * 8, BPDU_send, BPDU_len);
	if (PRINT) {
		cout << "*********<<<<发送BPDU>>>>********" << endl << endl;
	}
	for (int j = 0; j < 5; j++) {
		for (int i = 0; i < lowerNumber; i++) {
			SendtoLower(bufSend, BPDU_len * 8, i); //参数依次为数据缓冲，长度，接口号
		}
	}
	if (BPDU_send != NULL) {
		free(BPDU_send);
	}
	if (bufSend != NULL) {
		free(bufSend);
	}
}

//打印统计信息
void print_statistics();

void menu();

//***************重要函数提醒******************************
//名称：InitFunction
//功能：初始化功能面，由main函数在读完配置文件，正式进入驱动机制前调用
//输入：
//输出：
void InitFunction(CCfgFileParms& cfgParms)
{
	sendbuf = (char*)malloc(1000);
	if (sendbuf == NULL) {
		cout << "内存不够" << endl;
		//这个，计算机也太，退出吧
		exit(0);
	}
	belongto = strDevID.c_str()[0];
	for (int i = 0; i < lowerNumber; i++) {
		port_status[i] = 1;
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
	if (sendbuf != NULL)
		free(sendbuf);
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
void TimeOut()
{
	if (CLOCK++ == 300 && belongto == strDevID.c_str()[0]) {
		voteroot();
		CLOCK = 0;
	}
	if (CLOCK_BPDU++ == 300 && belongto == strDevID.c_str()[0]) {
		send_BPDU();
		CLOCK_BPDU = 0;
	}
	printCount++;
	if (_kbhit()) {
		//键盘有动作，进入菜单模式
		menu();
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
{
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
void RecvfromLower(U8* buf, int len, int ifNo)
{
	U8 mac_des;
	U8 mac_src;
	int ifNo_to = 404;
	U8 index;
	U8 flag;
	int iSndRetval;
	U8* bufSend = NULL;

	bufSend = (U8*)malloc(len / 8 + 1);
	if (bufSend == NULL) {
		cout << "内存空间不够，导致数据没有被处理" << endl;
		return;
	}

	//校验和解封
	get_data(buf, len, bufSend, &iSndRetval, &flag, &index, &mac_src, &mac_des);


	//校验正确
	if (iSndRetval != 0) {

		if (flag == 'R') {
			if (belongto <= index) {
				return;
			}
			else {
				belongto = index;
				for (int i = 0; i < lowerNumber; i++) {
					SendtoLower(buf, len, i);
				}
				if (PRINT) {
					cout << "<<<<<<<<<<归属变更>>>>>>>>>>" << endl;
					cout << "此时的归属为：" << belongto << endl << endl;
				}
				for (int i = 0; i < lowerNumber; i++) {
					port_status[i] = 1;
				}
				BPDU_port = -1;
				return;
			}
		}
		if (port_status[ifNo] == 1) {
			if (flag == 'U') {
				if (belongto == strDevID.c_str()[0]) {
					return;
				}
				if (BPDU_port != ifNo && BPDU_port != -1) {
					port_status[ifNo] = 0;
					if (PRINT) {
						cout << "::::::::::接收BPDU:::::::::::" << endl;
						cout << "将端口" << ifNo << "堵塞" << endl << endl;
					}
				}
				else if (BPDU_port == -1 || BPDU_port == ifNo) {
					BPDU_port = ifNo;
					for (int i = 0; i < lowerNumber; i++) {
						if (i == ifNo) {
							continue;
						}
						SendtoLower(buf, len, i);
					}
				}
			}
			else {

				//打印信息
				if (PRINT)
				{
					cout << endl;
					cout << "///////////////////////////////////接收帧///////////////////////////////////" << endl << endl;
					cout << "帧的序号为：" << int(index) << endl << endl;
					cout << "帧的模式为：" << flag << endl << endl;
					cout << "源地址为：" << mac_src << endl << endl;
					cout << "目的地址为：" << mac_des << endl << endl;
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

					U8* bit_buf = (U8*)malloc(iSndRetval * 8);
					ByteArrayToBitArray(bit_buf, iSndRetval * 8, bufSend, iSndRetval);
					cout << "信息的比特流为：";
					for (int w = 0; w < iSndRetval * 8; w++) {
						cout << int(bit_buf[w]);
						if (w % 4 == 3) {
							cout << " ";
						}
					}
					free(bit_buf);
					cout << endl;
				}


				learning(mac_src, ifNo);
				//打印查看转发表
				if (PRINT)
				{
					cout << "----------" << "转发表" << "-----------" << endl;
					for (int i = 0; i < change_list.length; i++) {
						cout << "目的地址：" << change_list.content[i][0] << "  转发端口：" << int(change_list.content[i][1]) << endl;
					}
					cout << "---------------------------" << endl;
				}

				find_how_to_give(mac_des, &ifNo_to);

				//广播
				if (ifNo_to == 404 || mac_des == broadcast) {
					for (int i = 0; i < lowerNumber; i++) {
						if (ifNo == i) {
							continue;
						}
						iSndRetval = SendtoLower(buf, len, i);
					}
				}//单播
				else {
					iSndRetval = SendtoLower(buf, len, ifNo_to);
				}
			}
		}

	}

	//校验错误
	else {

		//打印信息
		if (PRINT)
		{
			cout << endl;
			cout << "///////////////////////////////////错误帧///////////////////////////////////" << endl << endl;
			cout << "接收的内容是：";
			for (int w = 0; w < len / 8; w++) {
				cout << bufSend[w];
			}
			cout << endl << endl;
			cout << "接收的bit流为：";
			for (int w = 0; w < len; w++) {
				cout << int(buf[w]);
				if (w % 4 == 3) {
					cout << " ";
				}
			}
			cout << endl;
		}

		//统计
		if (iSndRetval <= 0) {
			iSndErrorCount++;
		}
		else {
			iRcvToUpper += iSndRetval;
			iRcvToUpperCount++;
		}
		//如果需要重传等机制，可能需要将buf或bufSend中的数据另外申请空间缓存起来
	}

	if (bufSend != NULL) {
		//缓存bufSend数据，如果有必要的话
		//本例程中没有停等协议，bufSend的空间在用完以后需要释放
		free(bufSend);
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
			bufSend = (U8*)malloc(len * 8);

			iSndRetval = ByteArrayToBitArray(bufSend, len * 8, kbBuf, len);
			iSndRetval = SendtoLower(bufSend, iSndRetval, port);
			free(bufSend);
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