//Nettester 的功能文件
#include <iostream>
#include <conio.h>
#include <list>  
#include "winsock.h"
#include "stdio.h"
#include "CfgFileParms.h"
#include "function.h"
#pragma warning(disable:4996)
using namespace std;


// 网络层从上层接收的ICI
struct Net_Upper_ICI {
	U8 IciIP[2];
};

// 网络层从下层接收的ICI
struct Net_Lower_ICI {
	U8 IciMAC;
};

// 网络层报文头部
struct Net_Header {
	U8 SrcIP[2];
	U8 DesIP[2];
	U8 TTL;
	U8 Flag;
};

// 路由表
struct Routing_Table {
	int len;
	U8 desip[MAX_LEN_ARP];
	U8 next_hop[MAX_LEN_ARP][2];
	int iface[MAX_LEN_ARP];
};

// ARP表
struct ARP_Table {
	int len;
	U8 desip[MAX_LEN_R][2];
	U8 desMAC[MAX_LEN_R];
};


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
int iRcvUnknownCount = 0;  //收到不明来源数据总次数
int upperIciLen = 2;//上层ici长度
int lowerIciLen = 1;//下层ici长度
int headerLen = 6;//报文头部长度
int CLOCK = 0;//用于ARP重发的时钟
int group_clock;//用于分组发送的时钟
int	recLen = 0;//分组中用于接收的长度
int INDEX = 0;//分组中用于记录的序号
U8 ttl_max = 'a';//最大跳数
string strIP;  // IP地址
U8 IP[10][2]; //本设备所有的IP地址，如接口0的IP为IP[0]
U8 Gateway[2];//网关
U8* all_buf;//分组合并的数组

// 缓存区域
list<list<U8*>> ourbuffer;
list<list<U8*>>* buffer = &ourbuffer;

// 分组缓存
list<U8*> zu;
list<U8*>* groups = &zu;

//两个表的创建
Routing_Table routing_table;
Routing_Table* Routing = &routing_table;
ARP_Table arp_table;
ARP_Table* ARP = &arp_table;


//数组复制函数
void copy(U8* a, U8* b, int len) {
	for (int i = 0; i < len; i++) {
		a[i] = b[i];
	}
}

//查找路由表
void find_in_routingtable(Routing_Table* Routing, U8* desip, U8* nextip, int* iface) {
	for (int i = 0; i < Routing->len; i++) {
		if (desip[0] == Routing->desip[i]) {
			//直接采用出接口
			if (Routing->next_hop[i][0] == NULL && Routing->next_hop[i][1] == NULL) {
				nextip[0] = desip[0];
				nextip[1] = desip[1];
				*iface = Routing->iface[i];
			}
			else {//利用下一跳
				nextip[0] = Routing->next_hop[i][0];
				nextip[1] = Routing->next_hop[i][1];
				*iface = Routing->iface[i];
			}
			return;
		}
	}
	*iface = -1;
}

// 为数据增加头部
void addHeader(U8* bufSend, U8 desMAC, U8* srcip, U8* desip, U8 ttl, U8 flag) {
	Net_Lower_ICI* lowerIci = (Net_Lower_ICI*)bufSend;
	lowerIci->IciMAC = desMAC;
	Net_Header* header = (Net_Header*)&bufSend[lowerIciLen];
	header->SrcIP[0] = srcip[0];
	header->SrcIP[1] = srcip[1];
	header->DesIP[0] = desip[0];
	header->DesIP[1] = desip[1];
	header->TTL = ttl;
	header->Flag = flag;
}

//获得自己的端口IP
void init_interface_ip(Routing_Table* Routing, string strIP, U8(*IP)[2]) {
	const char* p = strIP.c_str();
	int num = 0, i = 0;
	for (; num < lowerNumber; i++) {
		if (p[i] == '.') {
			IP[num][0] = p[i - 1];
			IP[num][1] = p[i + 1];
			num++;
		}
	}

	//将IP写入到路由表中
	for (int j = 0; j < lowerNumber; j++) {
		Routing->desip[Routing->len] = IP[j][0];
		Routing->iface[Routing->len] = j;
		Routing->next_hop[Routing->len][0] = NULL;
		Routing->next_hop[Routing->len][1] = NULL;
		Routing->len++;
	}
}

//ARP相关函数************
//查ARP由表
void find_in_ARP(ARP_Table* ARP, U8* desip, U8* desMAC) {
	for (int i = 0; i < ARP->len; i++) {
		if (desip[0] == ARP->desip[i][0] && desip[1] == ARP->desip[i][1]) {
			*desMAC = ARP->desMAC[i];
			return;
		}
	}
	*desMAC = NULL;
}

//ARP表学习
void ARP_learning(ARP_Table* ARP, U8* desip, U8* desMAC) {
	for (int i = 0; i < ARP->len; i++) {
		if (desip[0] == ARP->desip[i][0] && desip[1] == ARP->desip[i][1]) {
			ARP->desMAC[i] = *desMAC;
			return;
		}
	}
	ARP->desip[ARP->len][0] = desip[0];
	ARP->desip[ARP->len][1] = desip[1];
	ARP->desMAC[ARP->len] = *desMAC;
	ARP->len++;
}

//arp请求
void send_arp(U8 desMAC, U8* srcip, U8* desip, U8 ttl, int ifNo) {
	Net_Lower_ICI lowerIci;
	Net_Upper_ICI upperIci;
	Net_Header head;
	U8* sendarp = (U8*)malloc(lowerIciLen + headerLen);
	int lenarp = lowerIciLen + headerLen;
	addHeader(sendarp, desMAC, srcip, desip, ttl, '4');
	SendtoLower(sendarp, lenarp, ifNo);
	if (sendarp) {
		free(sendarp);
	}
}

//arp回复
void send_rearp(U8 desMAC, U8* srcip, U8* desip, U8 ttl, int ifNo) {
	Net_Lower_ICI lowerIci;
	Net_Upper_ICI upperIci;
	Net_Header head;
	U8* sendarp = (U8*)malloc(lowerIciLen + headerLen);
	int lenarp = lowerIciLen + headerLen;
	addHeader(sendarp, desMAC, srcip, desip, ttl, '8');
	SendtoLower(sendarp, lenarp, ifNo);
	if (sendarp) {
		free(sendarp);
	}
}

//ARP重发函数
void reget_ARP(list <list<U8*>>* buffer) {
	U8 srcip[2];
	U8 desip[2];
	int iface;
	for (list<list<U8*>>::iterator i = (*buffer).begin(); i != (*buffer).end(); i++) {
		int index = 0;
		for (list<U8*>::iterator j = (*i).begin(); index < 4; j++, index++) {
			if (index == 0) {
				copy(desip, (*j), 2);
			}
			else if (index == 1 && *(int*)(*j) < ARP_CLOCK) {
				*(int*)(*j) += 1;
				break;
			}
			else if (index == 2) {
				copy(srcip, (*j), 2);
			}
			else if (index == 3) {
				iface = *(int*)(*j);
				send_arp('F', srcip, desip, ttl_max, iface);
				*(int*)(*j) = 0;

				if (PRINT) {
					cout << "<<<<<<<<<<<<<<<<<<<<在ARP超时，发送ARP请求中>>>>>>>>>>>>>>>>>" << endl;
					cout << "请求IP：" << desip[0] << "." << desip[1] << endl << endl;
				}
			}
		}
	}
}

//加入缓存区
void add_to_buffer(list <list<U8*>>* buffer, U8* sendBuf, U8* desip, int newlen, U8* nowip, int iface) {
	U8* buf = (U8*)malloc(newlen + 4);
	U8* des_ip = (U8*)malloc(2);
	U8* nowIP = (U8*)malloc(2);
	U8* clock = (U8*)malloc(4);
	*((int*)clock) = 0;
	U8* ifNo = (U8*)malloc(4);
	*((int*)ifNo) = iface;
	sendBuf = sendBuf - 4;
	((int*)sendBuf)[0] = newlen;//将长度也存进去
	copy(buf, sendBuf, newlen + 4); // 到这里就封装好头部了 sendBuf便是PDU
	copy(des_ip, desip, 2); // 到这里就封装好头部了 sendBuf便是PDU
	copy(nowIP, nowip, 2); // 到这里就封装好头部了 sendBuf便是PDU

	for (list<list<U8*>>::iterator i = (*buffer).begin(); i != (*buffer).end(); i++) {
		if (des_ip[0] == (*i).front()[0] && des_ip[1] == (*i).front()[1]) {//找到了之前相同目的ip的数据们
			(*i).push_back(buf);
			sendBuf = sendBuf + 4;
			return;
		}
	}
	//没有查找到
	list<U8*>newbuff_part;
	newbuff_part.push_back(des_ip);
	newbuff_part.push_back(clock);
	newbuff_part.push_back(nowIP);
	newbuff_part.push_back(ifNo);
	newbuff_part.push_back(buf);
	(*buffer).push_back(newbuff_part);
	sendBuf = sendBuf + 4;
}

//从缓存区删除
void send_buffer(list <list<U8*>>* buffer, U8* desip, U8* desMAC, int ifNo) {

	for (list<list<U8*>>::iterator i = (*buffer).begin(); i != (*buffer).end(); i++) {
		if (desip[0] == (*i).front()[0] && desip[1] == (*i).front()[1]) {//找到了之前相同目的ip的数据们
			int index = 0;
			for (list<U8*>::iterator j = (*i).begin(); j != (*i).end(); j++, index++) {
				if (index < 4) {
					continue;
				}
				int newlen = ((int*)(*j))[0];//取出长度
				(*j)[4] = *desMAC;//将他的ICI变成desmac
				SendtoLower((*j) + 4, newlen, ifNo);
			}
			for (list<U8*>::iterator j = (*i).begin(); j != (*i).end(); j++) {
				if (*j) {
					free(*j);
				}
			}
			(*i).clear();
			(*buffer).erase(i);//删除
			if (PRINT) {
				cout << "：：：：：：：：：发送缓存区内容：：：：：：：：：" << endl;
				cout << "发送的IP缓存的下一跳IP为：" << desip[0] << "." << desip[1] << endl;
				cout << endl;
			}
			return;
		}
	}
	if (PRINT) {
		cout << endl;
		cout << "(((((((((((((((((((((((( 未查找到缓存器数据 (((((((((((((((((((((" << endl;;
		cout << "没有找到缓存中的对应ip，可能是之前发送过" << endl;
		cout << "需要发送ip为： " << desip[0] << "." << desip[1] << endl;
		cout << "****************缓存数据有如下ip****************" << endl;
		for (list<list<U8*>>::iterator i = (*buffer).begin(); i != (*buffer).end(); i++) {
			cout << "ip:" << (*i).front()[0] << "." << (*i).front()[1] << endl;
		}
		cout << "************************************************" << endl << endl;
	}
}

//分帧相关函数************
//将数据分组功能
void add_to_groups(list<U8*>* groups, U8* buf, int len) {
	U8 desip[2];
	U8* temp;
	U8 flag_group[9] = "GGGGGGGG";
	copy(desip, buf, 2);
	int last_len;
	int num;

	if (len % (MAX_group - 2) == 0) {
		num = len / (MAX_group - 2);
		last_len = MAX_group - 2;
	}
	else {
		num = len / (MAX_group - 2) + 1;
		last_len = len % (MAX_group - 2);
	}


	U8* firstgroup = (U8*)malloc(MAX_group + 4);
	((int*)(firstgroup))[0] = 14;
	copy(firstgroup + 4, desip, 2);
	copy(firstgroup + 6, flag_group, 8);
	((int*)(firstgroup + 14))[0] = num;
	groups->push_back(firstgroup);

	for (int i = 0; i < num - 1; i++) {
		temp = (U8*)malloc(MAX_group + 4);
		((int*)(temp))[0] = MAX_group;
		copy(temp + 4, desip, 2);
		copy(temp + 6, buf + (MAX_group - 2) * i, MAX_group - 2);
		groups->push_back(temp);
	}

	temp = (U8*)malloc(MAX_group + 4);
	((int*)(temp))[0] = last_len + 2;
	copy(temp + 4, desip, 2);
	copy(temp + 6, buf + (MAX_group - 2) * (num - 1), last_len);
	groups->push_back(temp);
}

//将分组发送一部分
void send_group(list<U8*>* groups, int num) {
	int index = 0;
	for (list<U8*>::iterator i = (*groups).begin(); i != (*groups).end() && index < num; i++, index++) {
		int len = ((int*)(*i))[0];
		RecvfromUpper(((*i) + 4), len);
		if (*i != NULL) {
			free(*i);
		}
	}
	for (int i = 0; i < index; i++) {
		(*groups).pop_front();
	}
}

//实现分组合并，直接替换SendtoUpper（）
void send_to_upper(U8* buf_real, int iSndRetval) {
	if (iSndRetval >= 10 && recLen == 0) {
		for (int i = 2; i < 11; i++) {
			if (buf_real[i] != 'G') {
				break;
			}
			if (i == 8) {
				U8* temp_int = buf_real + 10;
				recLen = ((int*)temp_int)[0];
				all_buf = (U8*)malloc((MAX_group) * (recLen + 1));
				return;
			}
		}
	}
	if (recLen != 0) {

		copy(all_buf + INDEX * (MAX_group - 2), buf_real + 2, iSndRetval - 2);
		INDEX++;

		if (INDEX == 1) {
			copy(all_buf, buf_real, 2);
		}

		if (recLen == INDEX) {

			SendtoUpper(all_buf, (MAX_group - 2) * (recLen - 1) + iSndRetval - 2);

			if (all_buf != NULL) {
				free(all_buf);
			}
			recLen = 0;
			INDEX = 0;
		}
		else {
			return;
		}
	}
	else {
		SendtoUpper(buf_real, iSndRetval);
	}
}


void print_statistics();

void menu();
//***************重要函数提醒******************************
//名称：InitFunction
//功能：初始化功能面，由main函数在读完配置文件，正式进入驱动机制前调用
//输入：
//输出：
void InitFunction(CCfgFileParms& cfgParms)
{
	strIP = cfgParms.getValueStr("IP"); // IP地址读取
	init_interface_ip(Routing, strIP, IP);//获得自己所有端口的IP地址
	//网关配置
	Gateway[0] = IP[0][0];
	Gateway[1] = '1';
	sendbuf = (char*)malloc(MAX_BUFFER_SIZE);
	if (sendbuf == NULL) {
		cout << "内存不够" << endl;
		//这个，计算机也太，退出吧
		exit(0);
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
	if (CLOCK++ >= 20) {
		reget_ARP(buffer);//定时发送ARP
		CLOCK = 0;
	};
	if (group_clock++ == 5) {
		send_group(groups, 1);
		group_clock = 0;
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
	if (len > MAX_group) {
		add_to_groups(groups, buf, len);
		return;
	}

	int newlen = len;//加入头部后的长度
	Net_Upper_ICI* upperIci = (Net_Upper_ICI*)buf;
	U8 desip[2];//获取上层ICI
	desip[0] = upperIci->IciIP[0];
	desip[1] = upperIci->IciIP[1];
	newlen += headerLen + lowerIciLen;
	U8* sendBuf = (U8*)malloc(newlen + 4);//待发送数据;+4是为了加入缓存区用的别删
	sendBuf = &sendBuf[4];//将前面四个字节留出来缓存的时候用
	U8 desMAC;
	U8 nextip[2];

	//打印接收的信息
	if (PRINT) {
		cout << "________________接收上层数据报文_______________" << endl;
		cout << "目的IP为:" << desip[0] << "." << desip[1] << endl;
		cout << "报文长度为：" << len << endl;
		cout << "报文信息为：";
		for (int i = 0; i < len; i++) {
			cout << buf[i];
		}
		cout << endl;
		cout << "_______________向下层发送数据报文______________" << endl << endl;
	}

	//pc判断目的地址是否何自己在同一网段
	if (desip[0] == IP[0][0]) {
		nextip[0] = desip[0];
		nextip[1] = desip[1];
	}
	else {//不在同网段，发到网关
		nextip[0] = Gateway[0];
		nextip[1] = Gateway[1];
	}
	find_in_ARP(ARP, nextip, &desMAC);
	//未查询到mac
	if (desMAC == NULL) {
		if (sendBuf != NULL) {
			addHeader(sendBuf, NULL, IP[0], desip, ttl_max, '2');//mac地址先置空
			copy(&sendBuf[headerLen + lowerIciLen], buf, len);// 到这里就封装好头部了 sendBuf便是PDU
		}

		//放入缓存区
		add_to_buffer(buffer, sendBuf, nextip, newlen, nextip, 0);

		//打印缓冲区
		if (PRINT) {
			cout << "****************缓存数据有如下ip****************" << endl;
			for (list<list<U8*>>::iterator i = (*buffer).begin(); i != (*buffer).end(); i++) {
				cout << "ip:" << (*i).front()[0] << "." << (*i).front()[1] << endl;
			}
			cout << "************************************************" << endl << endl;
		}

		//发送arp请求 'F'为广播
		send_arp('F', IP[0], nextip, ttl_max, 0);
		if (PRINT) {
			cout << "<<<<<<<<<<<<<<<<<<<<在ARP中未查询到表项，发送ARP请求中>>>>>>>>>>>>>>>>>" << endl;
			cout << "请求IP：" << nextip[0] << "." << nextip[1] << endl << endl;
		}

	}
	else {
		if (sendBuf != NULL) {
			addHeader(sendBuf, desMAC, IP[0], desip, ttl_max, '2');
			copy(&sendBuf[headerLen + lowerIciLen], buf, len); // 到这里就封装好头部了 sendBuf便是PDU
		}
		SendtoLower(sendBuf, newlen, 0);
	}


	if (sendBuf - 4 != NULL) {
		free(sendBuf - 4);
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
{
	Net_Lower_ICI* lowerIci = (Net_Lower_ICI*)buf;// 取下层ICI 范例  
	Net_Upper_ICI* upperIci = (Net_Upper_ICI*)&buf[headerLen + lowerIciLen];//上层ICI
	Net_Header* head = (Net_Header*)&buf[lowerIciLen];
	int newlen = len - headerLen - lowerIciLen;
	U8* bufSend = (U8*)malloc(newlen);
	copy(bufSend, buf + headerLen + lowerIciLen, newlen);




	//PC查看这个报文是否是发给自己的,不是就直接扔
	if (head->DesIP[0] != IP[ifNo][0] || head->DesIP[1] != IP[ifNo][1]) {
		return;
	}

	//数据
	if (head->Flag == '2') {
		//打印数据信息
		if (PRINT) {
			cout << "========================接收下层数据报文========================" << endl;
			cout << "目的IP为:" << head->DesIP[0] << "." << head->DesIP[1] << endl;
			cout << "源IP为:" << head->SrcIP[0] << "." << head->SrcIP[1] << endl;
			cout << "报文信息为：";
			for (int i = 0; i < newlen; i++) {
				cout << bufSend[i];
			}
			cout << endl << "=======================向上层发送数据报文=======================" << endl;
			cout << endl;
		}

		bufSend[0] = head->SrcIP[0];
		bufSend[1] = head->SrcIP[1];

		send_to_upper(bufSend, newlen);
	}
	else if (head->Flag == '4') {//arp请求

		if (PRINT) {
			cout << "========================接收ARP请求========================" << endl;
			cout << "请求的IP为:" << head->DesIP[0] << "." << head->DesIP[1] << endl;
			cout << "源IP为:" << head->SrcIP[0] << "." << head->SrcIP[1] << endl;
			cout << endl << endl;
		}

		send_rearp(lowerIci->IciMAC, head->DesIP, head->SrcIP, ttl_max, ifNo);

		if (PRINT) {
			cout << "（（（（（（（（（（（（（发送ARP回复））））））））））））" << endl;
			cout << "目的IP为:" << head->SrcIP[0] << "." << head->SrcIP[1] << endl;
			cout << "源IP为:" << head->DesIP[0] << "." << head->DesIP[1] << endl;
			cout << endl << endl;
		}
	}
	else if (head->Flag == '8') {

		if (PRINT) {
			cout << "………………接收ARP回复………………" << endl;
			cout << "回发MAC:" << lowerIci->IciMAC << endl;
			cout << "源IP为:" << head->SrcIP[0] << "." << head->SrcIP[1] << endl;
			cout << endl << endl;
		}

		ARP_learning(ARP, head->SrcIP, &lowerIci->IciMAC);

		if (PRINT) {
			cout << "****************ARP表****************" << endl;
			for (int i = 0; i < ARP->len; i++) {
				cout << "ip:" << ARP->desip[i][0] << "." << ARP->desip[i][1] << "||" << "MAC:" << ARP->desMAC[i] << endl;
			}
			cout << "*************************************" << endl << endl;
		}

		//发送函数
		send_buffer(buffer, head->SrcIP, &lowerIci->IciMAC, ifNo);

	}

	if (bufSend != NULL) {
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

void deleteMark(string& s, const string& mark)
{
	size_t nSize = mark.size();
	while (1)
	{
		size_t pos = s.find(mark);
		if (pos == string::npos)
		{
			return;
		}

		s.erase(pos, nSize);
	}
}
