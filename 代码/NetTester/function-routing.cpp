//Nettester 的功能文件
#include <iostream>
#include <conio.h>
#include <list> 
#include"time.h"
#include "winsock.h"
#include "stdio.h"
#include "CfgFileParms.h"
#include "function.h"
#pragma warning(disable:4996)
using namespace std;




//----------------华丽的分割线，一些统计用的全局变量----------------
//以下为重要的变量
U8* sendbuf;        //用来组织发送数据的缓存，大小为MAX_BUFFER_SIZE,可以在这个基础上扩充设计，形成适合的结构，例程中没有使用，只是提醒一下
int nextSendTime = 1;//记录下次定时发送路由表的时间
int printCount = 0; //打印控制
int spin = 0;  //打印动态信息控制
int mode; // mode为0表示静态路由，为1表示动态路由
//------华丽的分割线，一些统计用的全局变量------------
int iSndTotal = 0;  //发送数据总量
int iSndTotalCount = 0; //发送数据总次数
int iSndErrorCount = 0;  //发送错误次数
int iRcvForward = 0;     //转发数据总量
int iRcvForwardCount = 0; //转发数据总次数
int iRcvToUpper = 0;      //从低层递交高层数据总量
int iRcvToUpperCount = 0;  //从低层递交高层数据总次数
int iRcvUnknownCount = 0;  //收到不明来源数据总次数
int upperIciLen = 2;
int lowerIciLen = 1;
int headerLen = 6;
int CLOCK = 0;
U8 ttl_max = 20;
string strIP;  // IP地址
U8 IP[10][2]; //本设备所有的IP地址，如接口0的IP为IP[0]
U8 Gateway[2];
//const U8* rPort; // 连接路由器的端口
string rPort; // 连接路由器的端口

//----------------华丽的分割线，一些重要的结构体----------------
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
	int len; //长度
	U8 desip[MAX_LEN_R]; // 目的网段
	U8 next_hop[MAX_LEN_R][2]; // 网络号
	int iface[MAX_LEN_R]; // 出接口
	int metric[MAX_LEN_R];// 度量
	int metric_ltime[MAX_LEN_R]; // 度量有效时间
};

// ARP表
struct ARP_Table {
	int len;
	U8 desip[MAX_LEN_ARP][2];
	U8 desMAC[MAX_LEN_ARP];
};
// 缓存区域
list <list<U8*>> ourbuffer;
list <list<U8*>>* buffer = &ourbuffer;

//两个表的创建
Routing_Table routing_table;
Routing_Table* Routing = &routing_table;
ARP_Table arp_table;
ARP_Table* ARP = &arp_table;


void copy(U8* a, U8* b, int len) {
	for (int i = 0; i < len; i++) {
		a[i] = b[i];
	}
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

//添加路由表
void add_routing(Routing_Table* Routing, U8 desip, int iface, U8* nexthop, int metric, int flag) { // flag标志是否为常驻路由
	Routing->desip[Routing->len] = desip; // 目的网段
	Routing->iface[Routing->len] = iface;
	if (nexthop == NULL) {
		Routing->next_hop[Routing->len][0] = NULL;
		Routing->next_hop[Routing->len][1] = NULL;
	}
	else {
		Routing->next_hop[Routing->len][0] = nexthop[0];
		Routing->next_hop[Routing->len][1] = nexthop[1];
	}
	Routing->metric[Routing->len] = metric;
	if (flag)
		Routing->metric_ltime[Routing->len] = 7 * SEND_TIME; // 常驻路由
	else
		Routing->metric_ltime[Routing->len] = 6 * SEND_TIME;

	Routing->len++;
}
// 更新路由表 srcip为路由表发送方 desip为路由表项的下一跳 metric为路由表项的度量
int update_routing(Routing_Table* Routing, U8* srcip, U8 desip, int metric, int recvface) {
	if (metric < 0) return 0;
	int i;
	int flag = 0; // 标志是否发生更新
	int exist = 0; // 标志是否在路由表存在
	for (i = 0; i < Routing->len; i++) {
		if (Routing->desip[i] == desip) { // 存在该路由表项
			exist = 1;
			if (desip != IP[recvface][0]) {
				if (Routing->next_hop[i][0] == srcip[0] && Routing->next_hop[i][1] == srcip[1]) {// 如果路由表下一项为路由表的发送方
					if (Routing->metric[i] == MAX_METRIC + 1 && (metric == MAX_METRIC + 1 || metric == MAX_METRIC)) // 这种情况下 从不可达更新为不可达 没必要更新计时器
						continue;
					if (Routing->metric[i] != metric + 1) {
						cout << "更新前路由表信息	目的网段" << desip << "	下一跳" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "	度量" << Routing->metric[i] << endl;
						flag = 1;
						if (metric == MAX_METRIC + 1)
							Routing->metric[i] = MAX_METRIC + 1;
						else
							Routing->metric[i] = metric + 1; // 直接更新路由表项
						cout << "更新后路由表信息	目的网段" << desip << "	下一跳" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "	度量" << Routing->metric[i] << endl;
					}
					Routing->metric_ltime[i] = 6 * SEND_TIME; // 计时器重新计时
				}
				else {  // 如果路由表下一项不是路由表的发送方
					if (Routing->metric[i] > metric + 1) { // 当度量值减少时更新
						cout << "更新前路由表信息	目的网段" << desip << "	下一跳" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "	度量" << Routing->metric[i] << endl;
						Routing->metric[i] = metric + 1; // 直接更新路由表项
						Routing->next_hop[i][0] = srcip[0]; // 更新下一跳
						Routing->next_hop[i][1] = srcip[1]; // 更新下一跳
						Routing->iface[i] = recvface;
						Routing->metric_ltime[i] = 6 * SEND_TIME; // 计时器重新计时
						flag = 1;
						cout << "更新后路由表信息	目的网段" << desip << "	下一跳" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "	度量" << Routing->metric[i] << endl;
					}
				}
			}

		}
	}
	if (i == Routing->len && !exist) { //如果不存在于该路由表中
		if (metric <= MAX_METRIC) { // 若度量小于最大跳数 直接加入表项
			add_routing(Routing, desip, recvface, srcip, metric + 1, 0);
			flag = 1;
			cout << "添加路由表信息	目的网段" << desip << "	下一跳" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "	度量" << Routing->metric[i] << endl;

		}
	}

	return flag;

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


// 学习路由表
int learn_routing(Routing_Table* Routing, vector<string> data, U8* srcip, int recvface) {
	int flag = 0; //判断是否更新路由表项
	for (int i = 0; i < data.size(); i++) { // 学习所有路由表项
		if (!flag)
			flag = update_routing(Routing, srcip, data[i].at(0), (int)data[i].at(1), recvface);
		else
			update_routing(Routing, srcip, data[i].at(0), (int)data[i].at(1), recvface);
	}
	return flag;
}

// 加载路由报文
vector<string> get_routing(Routing_Table* Routing, U8* data, int len) {
	vector<string> routing_data;
	for (int i = 0; i < len; i += 2) {
		string a;
		a = a + data[i] + data[i + 1];
		routing_data.push_back(a);
	}
	return routing_data;
}

// 检查路由表老化计时器
int check_routing(Routing_Table* Routing) {
	// 双指针检查路由表项是否需要删除
	int i = 0, j = 0, flag = 0;
	for (; i < Routing->len; i++) {
		if (Routing->metric_ltime[i] != SEND_TIME * 7) // 常驻路由
			Routing->metric_ltime[i]--; //老化
		if (Routing->metric[i] == MAX_METRIC + 1 && Routing->metric_ltime[i] > 0)
			Routing->metric_ltime[i] = -SEND_TIME * 2;
		if (Routing->metric_ltime[i] == 0) {// 计数器等于 赋值为不可达
			Routing->desip[j] = Routing->desip[i];
			Routing->iface[j] = Routing->iface[i];
			Routing->metric[j] = MAX_METRIC + 1; //赋值为不可达
			Routing->next_hop[j][0] = Routing->next_hop[i][0];
			Routing->next_hop[j][1] = Routing->next_hop[i][1];
			Routing->metric_ltime[j] = Routing->metric_ltime[i];
			j++;
			cout << "路由表项老化" << "  目的网络：" << Routing->desip[i] << "||" << "下一跳ip：" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "||" << "出接口:" << Routing->iface[i] << "||度量:" << Routing->metric[i] << endl;
			flag = 1;
		}
		else if (Routing->metric_ltime[i] > -SEND_TIME * 4) { //计时器大于-1200 直接赋值
			Routing->desip[j] = Routing->desip[i];
			Routing->iface[j] = Routing->iface[i];
			Routing->metric[j] = Routing->metric[i];
			Routing->next_hop[j][0] = Routing->next_hop[i][0];
			Routing->next_hop[j][1] = Routing->next_hop[i][1];
			Routing->metric_ltime[j] = Routing->metric_ltime[i];
			j++;
		}
		else
			cout << "路由表项老化 删除路由表项 ——" << "  目的网络：" << Routing->desip[i] << "||" << "下一跳ip：" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "||" << "出接口:" << Routing->iface[i] << "||度量:" << Routing->metric[i] << endl;

	}
	Routing->len = j;
	return flag;
}

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
		if (rPort.find(j + '0') == string::npos)
			add_routing(Routing, IP[j][0], j, NULL, 0, 1);
	}
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
// 各端口发送路由表

void send_routing(Routing_Table* Routing) {
	//U8* 
	U8* srcip; // 想要发送路由表端口的IP地址
	U8 nextip[2];// 目的路由器的IP地址
	U8 desmac; // 目的路由器的MAC地址
	int iface; // 目的路由器的接口
	for (int i = 0; i < rPort.size(); i++) {// 遍历所有连接路由器（对端是路由器）的端口
		string data; // 用于存储路由表项
		int len = Routing->len * 2 + headerLen + lowerIciLen; //报文长度
		U8* bufSend = (U8*)malloc(len); //申请区间
		srcip = IP[int(rPort[i]) - '0']; // 根据发送端口找出端口源ip
		find_in_routingtable(Routing, srcip, nextip, &iface); // 查询路由表 找出目的路由器的IP地址 nextip为下一跳ip地址
		find_in_ARP(ARP, nextip, &desmac); // 查询ARP表 找出目的路由器端口的MAC地址
		for (int i = 0; i < Routing->len; i++)  // 遍历路由表
		{
			if (Routing->desip[i] == nextip[0] && Routing->next_hop[i][0] == nextip[0]) // 如果路由表表项的目的IP或下一跳是目的路由器网段 度量设置为16 实现毒性反转F
			{
				data = data + Routing->desip[i] + (char)16;
			}
			else
			{
				data = data + Routing->desip[i] + (char)Routing->metric[i];
			}

		}
		cout << endl << "---------发送路由表信息到端口 " << iface << " ---------" << endl << endl;
		//未查询到mac
		if (desmac == NULL) {
			if (data.size() != 0) {
				addHeader(bufSend, NULL, srcip, nextip, ttl_max, '1');
				//strcpy(&bufSend[headerLen + lowerIciLen], data.data()); //放置数据
				//strcpy(&bufSend[headerLen + lowerIciLen], strdup(data.c_str()));//放置数据 乱码我操！
				for (int i = 0; i < data.size(); i++) {
					bufSend[i + lowerIciLen + headerLen] = data[i];
				}
			}
			//放入缓存区
			add_to_buffer(buffer, bufSend, nextip, len, IP[int(rPort[i]) - '0'], iface);

			//打印缓冲区
			if (PRINT) {
				cout << "****************缓存数据有如下ip****************" << endl;
				for (list<list<U8*>>::iterator i = (*buffer).begin(); i != (*buffer).end(); i++) {
					cout << "ip:" << (*i).front()[0] << "." << (*i).front()[1] << endl;
				}
				cout << "************************************************" << endl << endl;
			}

			//发送arp请求 'F'为广播
			send_arp('F', IP[int(rPort[i]) - '0'], nextip, ttl_max, iface);
			if (PRINT) {
				cout << "<<<<<<<<<<<<<<<<<<<<在ARP中未查询到表项，发送ARP请求中>>>>>>>>>>>>>>>>>" << endl;
				cout << "请求IP：" << nextip[0] << "." << nextip[1] << endl << endl;
			}

		}
		else {
			if (data.size() != 0) {
				addHeader(bufSend, desmac, srcip, nextip, ttl_max, '1');
				//strcpy(&bufSend[headerLen + lowerIciLen], strdup(data.c_str()));//放置数据 乱码我操！
				for (int i = 0; i < data.size(); i++) {
					bufSend[i + lowerIciLen + headerLen] = data[i];
				}
			}
			SendtoLower(bufSend, len, iface);
		}
	}
}
// 打印路由表

void print_routing(Routing_Table* Routing) {
	cout << endl << "！！！！！！！！！！！!路由表！!！！！！！！！！！！" << endl;
	if (mode) {
		for (int i = 0; i < Routing->len; i++) {
			if (Routing->next_hop[i][0] != NULL) {
				cout << "  目的网络：" << Routing->desip[i] << "||" << "下一跳ip：" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "||" << "出接口:" << Routing->iface[i] << "||度量:" << Routing->metric[i] << endl;
			}
			else {
				cout << "  目的网络：" << Routing->desip[i] << "||" << "下一跳ip：" << "N" << "." << "N" << "||" << "出接口:" << Routing->iface[i] << "||度量:" << Routing->metric[i] << endl;
			}
		}
	}
	else {
		for (int i = 0; i < Routing->len; i++) {
			if (Routing->next_hop[i][0] != NULL) {
				cout << "  目的网络：" << Routing->desip[i] << "||" << "下一跳ip：" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "||" << "出接口:" << Routing->iface[i] << endl;
			}
			else {
				cout << "  目的网络：" << Routing->desip[i] << "||" << "下一跳ip：" << "N" << "." << "N" << "||" << "出接口:" << Routing->iface[i] << endl;
			}
		}
	}
	cout << "  共" << Routing->len << "条路由信息" << endl;
	cout << "！！！！！！！！！！！！！！！！！！！！！！！！！！" << endl << endl;
}
//打印统计信息

void print_statistics();

void load_rPort(string& data) {
	U8 nexthop[2];
	deleteMark(data, ","); // 去除分割符号
	deleteMark(data, "-"); // 去除分割符号
	deleteMark(data, "."); // 去除分割符号
	deleteMark(data, " "); // 去除分割符号
	string str_rport;
	for (int i = 0; i < data.size(); i += 3) {
		str_rport = str_rport + data.at(i);
		nexthop[0] = data.at(i + 1);
		nexthop[1] = data.at(i + 2);
		add_routing(Routing, data.at(i + 1), (int)data.at(i) - '0', nexthop, 0, 1);
	}
	data = str_rport;
}

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

void menu();
//***************重要函数提醒******************************
//名称：InitFunction
//功能：初始化功能面，由main函数在读完配置文件，正式进入驱动机制前调用
//输入：
//输出：
void InitFunction(CCfgFileParms& cfgParms)
{
	srand((unsigned)time(NULL) + stoi(strDevID) * 1000000);
	nextSendTime += rand() % (SEND_TIME / 3);
	rPort = cfgParms.getValueStr("rPort");
	load_rPort(rPort);
	strIP = cfgParms.getValueStr("IP"); // IP地址读取
	cout << "共有" << lowerNumber << "个端口" << " 其中";
	for (int i = 0; i < rPort.size(); i++) {
		if (!i)
			cout << rPort[i];
		else
			cout << " " << rPort[i];

	}
	if (!rPort.size())
		cout << "无";
	cout << "端口连接路由器" << endl;
	init_interface_ip(Routing, strIP, IP);//获得自己所有端口的IP地址
	int reval = cfgParms.getValueInt(mode, "mode"); //读取选择的模式
	if (reval == -1)
		mode = 0; // 默认为静态路由
	if (mode)
		cout << "路由器使用动态路由协议" << endl;
	else {
		cout << "路由器使用静态路由协议" << endl;
		string txt = "Static routing" + strDevID + ".txt";
		loadStaticRouter(txt);
	}

	if (PRINT) {
		print_routing(Routing);
	}
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
	if (mode) {
		if (check_routing(Routing)) {
			// 检查是否出现老化
			srand((unsigned)time(NULL) + stoi(strDevID) * 1000000);
			// 随机重置更新定时器 防止多个路由器同时发送路由表项造成丢包
			nextSendTime = rand() % (SEND_TIME / 3) - (SEND_TIME / 6) + SEND_TIME;
			cout << "-------------路由表触发更新-------------" << endl;
			send_routing(Routing);
		}
		if (!--nextSendTime) {

			cout << endl << "------------定时发送路由表-------------" << endl << endl;
			send_routing(Routing);
			srand((unsigned)time(NULL) + stoi(strDevID) * 1000000);
			// 随机重置更新定时器 防止多个路由器同时发送路由表项造成丢包
			nextSendTime = rand() % (SEND_TIME / 3) - (SEND_TIME / 6) + SEND_TIME;

		}
	}
	if (CLOCK >= 20) {
		reget_ARP(buffer);//定时发送ARP
		CLOCK = 0;
	};
	CLOCK++;
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
	//无
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
	Net_Lower_ICI* lowerIci = (Net_Lower_ICI*)buf;// 取下层ICI
	Net_Header* head = (Net_Header*)&buf[lowerIciLen];
	U8* bufSend = (U8*)malloc(len + 4);
	bufSend += 4;
	copy(bufSend, buf, len);
	U8 nextip[2];
	int iface;
	U8 desMAC;
	U8 desip[2];
	desip[0] = head->DesIP[0];
	desip[1] = head->DesIP[1];
	// 路由器更新报文
	if (head->Flag == '1') {
		if (PRINT) {
			cout << "=============接收路由表报文=============" << endl;
			cout << "目的IP为:" << head->DesIP[0] << "." << head->DesIP[1] << endl;
			cout << "源IP为:" << head->SrcIP[0] << "." << head->SrcIP[1] << endl;
			cout << endl << endl;
		}
		vector<string> data = get_routing(Routing, &bufSend[lowerIciLen + headerLen], len - lowerIciLen - headerLen);
		if (learn_routing(Routing, data, head->SrcIP, ifNo)) { // 学习路由表 如果发生更新 触发更新
			srand((unsigned)time(NULL) + stoi(strDevID) * 1000000);
			// 随机重置更新定时器 防止多个路由器同时发送路由表项造成丢包
			nextSendTime = rand() % (SEND_TIME / 3) - (SEND_TIME / 6) + SEND_TIME;
			cout << "-------------路由表触发更新-------------" << endl;
			send_routing(Routing);
		}
	}

	//数据
	else if (head->Flag == '2') {
		//打印数据信息
		if (PRINT) {
			cout << "=============接收下层数据报文=============" << endl;
			cout << "目的IP为:" << head->DesIP[0] << "." << head->DesIP[1] << endl;
			cout << "源IP为:" << head->SrcIP[0] << "." << head->SrcIP[1] << endl;
			cout << "报文信息为：";
			for (int i = 0; i < len; i++) {
				cout << bufSend[i];
			}
			cout << endl << endl;
		}

		find_in_routingtable(Routing, head->DesIP, nextip, &iface);
		//路由表中没有与该报文匹配的表项，或者出接口与报文进入的接口相同，直接丢弃
		if (iface == -1 || iface == ifNo)
		{
			return;
		}

		find_in_ARP(ARP, nextip, &desMAC);

		//未查询到mac
		if (desMAC == NULL)
		{
			if (PRINT) {
				cout << "+++++++++++++++ARP表中未查询到+++++++++++++++" << endl;
				cout << "查询IP为:" << nextip[0] << "." << nextip[1] << endl;
				cout << endl << endl;
			}

			//放入缓存区
			((Net_Header*)(bufSend + lowerIciLen))->TTL--;
			if (((Net_Header*)(bufSend + lowerIciLen))->TTL != 0) {
				add_to_buffer(buffer, bufSend, nextip, len, IP[iface], iface);
				//打印缓冲区
				if (PRINT) {
					cout << "****************缓存数据有如下ip****************" << endl;
					for (list<list<U8*>>::iterator i = (*buffer).begin(); i != (*buffer).end(); i++) {
						cout << "ip:" << (*i).front()[0] << "." << (*i).front()[1] << endl;
					}
					cout << "************************************************" << endl << endl;
				}

				//发送arp请求 'F'为广播
				send_arp('F', IP[iface], nextip, ttl_max, iface);

				if (PRINT) {
					cout << "<<<<<<<<<<<<<<<<<<<<在ARP中未查询到表项，发送ARP请求中>>>>>>>>>>>>>>>>>" << endl;
					cout << "请求IP：" << nextip[0] << "." << nextip[1] << endl << endl;
				}
			}
		}
		else
		{
			if (PRINT) {
				cout << "+++++++++++++++ARP表中查询到MAC+++++++++++++++" << endl;
				cout << "查询IP为:" << nextip[0] << "." << nextip[1] << endl;
				cout << "查询MAC为:" << desMAC << endl;
				cout << endl << endl;
			}

			((Net_Lower_ICI*)(bufSend))->IciMAC = desMAC;
			((Net_Header*)(bufSend + lowerIciLen))->TTL--;
			if (((Net_Header*)(bufSend + lowerIciLen))->TTL != 0) {
				SendtoLower(bufSend, len, iface);
			}
		}
	}
	else if (head->Flag == '4')
	{//arp请求
		if (head->DesIP[0] == IP[ifNo][0] && head->DesIP[1] == IP[ifNo][1]) {

			if (PRINT) {
				cout << "=============接收ARP请求=============" << endl;
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
				cout << "ip:" << ARP->desip[i][0] << "." << ARP->desip[i][1] << "||" << "MAC:" << ARP->desMAC[i] << endl;;
			}
			cout << endl << "*************************************" << endl << endl;
		}


		send_buffer(buffer, head->SrcIP, &lowerIci->IciMAC, ifNo);
	}

	if ((bufSend - 4) != NULL) {
		free(bufSend - 4);
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
	cout << endl << "1-打印路由表;" << endl << "2-停止自动发送（无效）; " << endl << "3-从键盘输入发送; ";
	cout << endl << "4-仅打印统计信息; " << endl << "5-按比特流打印数据内容;" << endl << "6-按字节流打印数据内容;";
	cout << endl << "0-取消" << endl << "请输入数字选择命令：";
	cin >> selection;
	switch (selection) {

	case 0:

		break;
	case 1:
		print_routing(Routing);
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
vector<string> readData(string file) {
	ifstream ifs(file, ios::in);
	vector<string> vec;
	if (!ifs) {
		cerr << "Can't open the file." << endl;
		return vec;
	}
	string  line;
	while (getline(ifs, line))
	{
		vec.push_back(line);
	}
	ifs.close();
	return vec;
}
// 从文本加载静态路由
int loadStaticRouter(string file) {
	vector<string> vec = readData(file);
	char nextLoop[2];
	for (int i = 0; i < vec.size(); i++) {
		////下面补充 加入路由表
		//cout << vec[i] << endl;//第i项内容 
		//cout << vec[i].at(0) << endl; // 目的网络号
		nextLoop[0] = vec[i].at(2);
		nextLoop[1] = vec[i].at(4); // 可能这样子？？？
		add_routing(Routing, vec[i].at(0), (int)vec[i].at(6) - '0', nextLoop, 0, 1);
		//cout << vec[i].at(2) << endl; // 下一跳网络号
		//cout << vec[i].at(4) << endl; // 下一跳主机号
		//cout << (int)vec[i].at(6) - '0' << endl; // 出接口

	}
	return vec.size();
};