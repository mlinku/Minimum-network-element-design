//Nettester �Ĺ����ļ�
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




//----------------�����ķָ��ߣ�һЩͳ���õ�ȫ�ֱ���----------------
//����Ϊ��Ҫ�ı���
U8* sendbuf;        //������֯�������ݵĻ��棬��СΪMAX_BUFFER_SIZE,���������������������ƣ��γ��ʺϵĽṹ��������û��ʹ�ã�ֻ������һ��
int nextSendTime = 1;//��¼�´ζ�ʱ����·�ɱ��ʱ��
int printCount = 0; //��ӡ����
int spin = 0;  //��ӡ��̬��Ϣ����
int mode; // modeΪ0��ʾ��̬·�ɣ�Ϊ1��ʾ��̬·��
//------�����ķָ��ߣ�һЩͳ���õ�ȫ�ֱ���------------
int iSndTotal = 0;  //������������
int iSndTotalCount = 0; //���������ܴ���
int iSndErrorCount = 0;  //���ʹ������
int iRcvForward = 0;     //ת����������
int iRcvForwardCount = 0; //ת�������ܴ���
int iRcvToUpper = 0;      //�ӵͲ�ݽ��߲���������
int iRcvToUpperCount = 0;  //�ӵͲ�ݽ��߲������ܴ���
int iRcvUnknownCount = 0;  //�յ�������Դ�����ܴ���
int upperIciLen = 2;
int lowerIciLen = 1;
int headerLen = 6;
int CLOCK = 0;
U8 ttl_max = 20;
string strIP;  // IP��ַ
U8 IP[10][2]; //���豸���е�IP��ַ����ӿ�0��IPΪIP[0]
U8 Gateway[2];
//const U8* rPort; // ����·�����Ķ˿�
string rPort; // ����·�����Ķ˿�

//----------------�����ķָ��ߣ�һЩ��Ҫ�Ľṹ��----------------
// �������ϲ���յ�ICI
struct Net_Upper_ICI {
	U8 IciIP[2];
};

// �������²���յ�ICI
struct Net_Lower_ICI {
	U8 IciMAC;
};

// ����㱨��ͷ��
struct Net_Header {
	U8 SrcIP[2];
	U8 DesIP[2];
	U8 TTL;
	U8 Flag;
};

// ·�ɱ�
struct Routing_Table {
	int len; //����
	U8 desip[MAX_LEN_R]; // Ŀ������
	U8 next_hop[MAX_LEN_R][2]; // �����
	int iface[MAX_LEN_R]; // ���ӿ�
	int metric[MAX_LEN_R];// ����
	int metric_ltime[MAX_LEN_R]; // ������Чʱ��
};

// ARP��
struct ARP_Table {
	int len;
	U8 desip[MAX_LEN_ARP][2];
	U8 desMAC[MAX_LEN_ARP];
};
// ��������
list <list<U8*>> ourbuffer;
list <list<U8*>>* buffer = &ourbuffer;

//������Ĵ���
Routing_Table routing_table;
Routing_Table* Routing = &routing_table;
ARP_Table arp_table;
ARP_Table* ARP = &arp_table;


void copy(U8* a, U8* b, int len) {
	for (int i = 0; i < len; i++) {
		a[i] = b[i];
	}
}

// Ϊ��������ͷ��
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

//���·�ɱ�
void add_routing(Routing_Table* Routing, U8 desip, int iface, U8* nexthop, int metric, int flag) { // flag��־�Ƿ�Ϊ��פ·��
	Routing->desip[Routing->len] = desip; // Ŀ������
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
		Routing->metric_ltime[Routing->len] = 7 * SEND_TIME; // ��פ·��
	else
		Routing->metric_ltime[Routing->len] = 6 * SEND_TIME;

	Routing->len++;
}
// ����·�ɱ� srcipΪ·�ɱ��ͷ� desipΪ·�ɱ������һ�� metricΪ·�ɱ���Ķ���
int update_routing(Routing_Table* Routing, U8* srcip, U8 desip, int metric, int recvface) {
	if (metric < 0) return 0;
	int i;
	int flag = 0; // ��־�Ƿ�������
	int exist = 0; // ��־�Ƿ���·�ɱ����
	for (i = 0; i < Routing->len; i++) {
		if (Routing->desip[i] == desip) { // ���ڸ�·�ɱ���
			exist = 1;
			if (desip != IP[recvface][0]) {
				if (Routing->next_hop[i][0] == srcip[0] && Routing->next_hop[i][1] == srcip[1]) {// ���·�ɱ���һ��Ϊ·�ɱ�ķ��ͷ�
					if (Routing->metric[i] == MAX_METRIC + 1 && (metric == MAX_METRIC + 1 || metric == MAX_METRIC)) // ��������� �Ӳ��ɴ����Ϊ���ɴ� û��Ҫ���¼�ʱ��
						continue;
					if (Routing->metric[i] != metric + 1) {
						cout << "����ǰ·�ɱ���Ϣ	Ŀ������" << desip << "	��һ��" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "	����" << Routing->metric[i] << endl;
						flag = 1;
						if (metric == MAX_METRIC + 1)
							Routing->metric[i] = MAX_METRIC + 1;
						else
							Routing->metric[i] = metric + 1; // ֱ�Ӹ���·�ɱ���
						cout << "���º�·�ɱ���Ϣ	Ŀ������" << desip << "	��һ��" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "	����" << Routing->metric[i] << endl;
					}
					Routing->metric_ltime[i] = 6 * SEND_TIME; // ��ʱ�����¼�ʱ
				}
				else {  // ���·�ɱ���һ���·�ɱ�ķ��ͷ�
					if (Routing->metric[i] > metric + 1) { // ������ֵ����ʱ����
						cout << "����ǰ·�ɱ���Ϣ	Ŀ������" << desip << "	��һ��" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "	����" << Routing->metric[i] << endl;
						Routing->metric[i] = metric + 1; // ֱ�Ӹ���·�ɱ���
						Routing->next_hop[i][0] = srcip[0]; // ������һ��
						Routing->next_hop[i][1] = srcip[1]; // ������һ��
						Routing->iface[i] = recvface;
						Routing->metric_ltime[i] = 6 * SEND_TIME; // ��ʱ�����¼�ʱ
						flag = 1;
						cout << "���º�·�ɱ���Ϣ	Ŀ������" << desip << "	��һ��" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "	����" << Routing->metric[i] << endl;
					}
				}
			}

		}
	}
	if (i == Routing->len && !exist) { //����������ڸ�·�ɱ���
		if (metric <= MAX_METRIC) { // ������С��������� ֱ�Ӽ������
			add_routing(Routing, desip, recvface, srcip, metric + 1, 0);
			flag = 1;
			cout << "���·�ɱ���Ϣ	Ŀ������" << desip << "	��һ��" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "	����" << Routing->metric[i] << endl;

		}
	}

	return flag;

}
//����·�ɱ�
void find_in_routingtable(Routing_Table* Routing, U8* desip, U8* nextip, int* iface) {
	for (int i = 0; i < Routing->len; i++) {
		if (desip[0] == Routing->desip[i]) {
			//ֱ�Ӳ��ó��ӿ�
			if (Routing->next_hop[i][0] == NULL && Routing->next_hop[i][1] == NULL) {
				nextip[0] = desip[0];
				nextip[1] = desip[1];
				*iface = Routing->iface[i];
			}
			else {//������һ��
				nextip[0] = Routing->next_hop[i][0];
				nextip[1] = Routing->next_hop[i][1];
				*iface = Routing->iface[i];
			}
			return;
		}
	}
	*iface = -1;
}


// ѧϰ·�ɱ�
int learn_routing(Routing_Table* Routing, vector<string> data, U8* srcip, int recvface) {
	int flag = 0; //�ж��Ƿ����·�ɱ���
	for (int i = 0; i < data.size(); i++) { // ѧϰ����·�ɱ���
		if (!flag)
			flag = update_routing(Routing, srcip, data[i].at(0), (int)data[i].at(1), recvface);
		else
			update_routing(Routing, srcip, data[i].at(0), (int)data[i].at(1), recvface);
	}
	return flag;
}

// ����·�ɱ���
vector<string> get_routing(Routing_Table* Routing, U8* data, int len) {
	vector<string> routing_data;
	for (int i = 0; i < len; i += 2) {
		string a;
		a = a + data[i] + data[i + 1];
		routing_data.push_back(a);
	}
	return routing_data;
}

// ���·�ɱ��ϻ���ʱ��
int check_routing(Routing_Table* Routing) {
	// ˫ָ����·�ɱ����Ƿ���Ҫɾ��
	int i = 0, j = 0, flag = 0;
	for (; i < Routing->len; i++) {
		if (Routing->metric_ltime[i] != SEND_TIME * 7) // ��פ·��
			Routing->metric_ltime[i]--; //�ϻ�
		if (Routing->metric[i] == MAX_METRIC + 1 && Routing->metric_ltime[i] > 0)
			Routing->metric_ltime[i] = -SEND_TIME * 2;
		if (Routing->metric_ltime[i] == 0) {// ���������� ��ֵΪ���ɴ�
			Routing->desip[j] = Routing->desip[i];
			Routing->iface[j] = Routing->iface[i];
			Routing->metric[j] = MAX_METRIC + 1; //��ֵΪ���ɴ�
			Routing->next_hop[j][0] = Routing->next_hop[i][0];
			Routing->next_hop[j][1] = Routing->next_hop[i][1];
			Routing->metric_ltime[j] = Routing->metric_ltime[i];
			j++;
			cout << "·�ɱ����ϻ�" << "  Ŀ�����磺" << Routing->desip[i] << "||" << "��һ��ip��" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "||" << "���ӿ�:" << Routing->iface[i] << "||����:" << Routing->metric[i] << endl;
			flag = 1;
		}
		else if (Routing->metric_ltime[i] > -SEND_TIME * 4) { //��ʱ������-1200 ֱ�Ӹ�ֵ
			Routing->desip[j] = Routing->desip[i];
			Routing->iface[j] = Routing->iface[i];
			Routing->metric[j] = Routing->metric[i];
			Routing->next_hop[j][0] = Routing->next_hop[i][0];
			Routing->next_hop[j][1] = Routing->next_hop[i][1];
			Routing->metric_ltime[j] = Routing->metric_ltime[i];
			j++;
		}
		else
			cout << "·�ɱ����ϻ� ɾ��·�ɱ��� ����" << "  Ŀ�����磺" << Routing->desip[i] << "||" << "��һ��ip��" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "||" << "���ӿ�:" << Routing->iface[i] << "||����:" << Routing->metric[i] << endl;

	}
	Routing->len = j;
	return flag;
}

//��ARP�ɱ�
void find_in_ARP(ARP_Table* ARP, U8* desip, U8* desMAC) {
	for (int i = 0; i < ARP->len; i++) {
		if (desip[0] == ARP->desip[i][0] && desip[1] == ARP->desip[i][1]) {
			*desMAC = ARP->desMAC[i];
			return;
		}
	}
	*desMAC = NULL;
}

//ARP��ѧϰ
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

//����Լ��Ķ˿�IP
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
	//��IPд�뵽·�ɱ���
	for (int j = 0; j < lowerNumber; j++) {
		if (rPort.find(j + '0') == string::npos)
			add_routing(Routing, IP[j][0], j, NULL, 0, 1);
	}
}

//arp����
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

//arp�ظ�
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

//���뻺����
void add_to_buffer(list <list<U8*>>* buffer, U8* sendBuf, U8* desip, int newlen, U8* nowip, int iface) {
	U8* buf = (U8*)malloc(newlen + 4);
	U8* des_ip = (U8*)malloc(2);
	U8* nowIP = (U8*)malloc(2);
	U8* clock = (U8*)malloc(4);
	*((int*)clock) = 0;
	U8* ifNo = (U8*)malloc(4);
	*((int*)ifNo) = iface;
	sendBuf = sendBuf - 4;
	((int*)sendBuf)[0] = newlen;//������Ҳ���ȥ
	copy(buf, sendBuf, newlen + 4); // ������ͷ�װ��ͷ���� sendBuf����PDU
	copy(des_ip, desip, 2); // ������ͷ�װ��ͷ���� sendBuf����PDU
	copy(nowIP, nowip, 2); // ������ͷ�װ��ͷ���� sendBuf����PDU

	for (list<list<U8*>>::iterator i = (*buffer).begin(); i != (*buffer).end(); i++) {
		if (des_ip[0] == (*i).front()[0] && des_ip[1] == (*i).front()[1]) {//�ҵ���֮ǰ��ͬĿ��ip��������
			(*i).push_back(buf);
			sendBuf = sendBuf + 4;
			return;
		}
	}
	//û�в��ҵ�
	list<U8*>newbuff_part;
	newbuff_part.push_back(des_ip);
	newbuff_part.push_back(clock);
	newbuff_part.push_back(nowIP);
	newbuff_part.push_back(ifNo);
	newbuff_part.push_back(buf);
	(*buffer).push_back(newbuff_part);
	sendBuf = sendBuf + 4;
}

//�ӻ�����ɾ��
void send_buffer(list <list<U8*>>* buffer, U8* desip, U8* desMAC, int ifNo) {

	for (list<list<U8*>>::iterator i = (*buffer).begin(); i != (*buffer).end(); i++) {
		if (desip[0] == (*i).front()[0] && desip[1] == (*i).front()[1]) {//�ҵ���֮ǰ��ͬĿ��ip��������
			int index = 0;
			for (list<U8*>::iterator j = (*i).begin(); j != (*i).end(); j++, index++) {
				if (index < 4) {
					continue;
				}
				int newlen = ((int*)(*j))[0];//ȡ������
				(*j)[4] = *desMAC;//������ICI���desmac
				SendtoLower((*j) + 4, newlen, ifNo);
			}
			for (list<U8*>::iterator j = (*i).begin(); j != (*i).end(); j++) {
				if (*j) {
					free(*j);
				}
			}
			(*i).clear();
			(*buffer).erase(i);//ɾ��
			if (PRINT) {
				cout << "���������������������ͻ��������ݣ�����������������" << endl;
				cout << "���͵�IP�������һ��IPΪ��" << desip[0] << "." << desip[1] << endl;
				cout << endl;
			}
			return;
		}
	}
	if (PRINT) {
		cout << endl;
		cout << "(((((((((((((((((((((((( δ���ҵ����������� (((((((((((((((((((((" << endl;;
		cout << "û���ҵ������еĶ�Ӧip��������֮ǰ���͹�" << endl;
		cout << "��Ҫ����ipΪ�� " << desip[0] << "." << desip[1] << endl;
		cout << "****************��������������ip****************" << endl;
		for (list<list<U8*>>::iterator i = (*buffer).begin(); i != (*buffer).end(); i++) {
			cout << "ip:" << (*i).front()[0] << "." << (*i).front()[1] << endl;
		}
		cout << "************************************************" << endl << endl;
	}
}
// ���˿ڷ���·�ɱ�

void send_routing(Routing_Table* Routing) {
	//U8* 
	U8* srcip; // ��Ҫ����·�ɱ�˿ڵ�IP��ַ
	U8 nextip[2];// Ŀ��·������IP��ַ
	U8 desmac; // Ŀ��·������MAC��ַ
	int iface; // Ŀ��·�����Ľӿ�
	for (int i = 0; i < rPort.size(); i++) {// ������������·�������Զ���·�������Ķ˿�
		string data; // ���ڴ洢·�ɱ���
		int len = Routing->len * 2 + headerLen + lowerIciLen; //���ĳ���
		U8* bufSend = (U8*)malloc(len); //��������
		srcip = IP[int(rPort[i]) - '0']; // ���ݷ��Ͷ˿��ҳ��˿�Դip
		find_in_routingtable(Routing, srcip, nextip, &iface); // ��ѯ·�ɱ� �ҳ�Ŀ��·������IP��ַ nextipΪ��һ��ip��ַ
		find_in_ARP(ARP, nextip, &desmac); // ��ѯARP�� �ҳ�Ŀ��·�����˿ڵ�MAC��ַ
		for (int i = 0; i < Routing->len; i++)  // ����·�ɱ�
		{
			if (Routing->desip[i] == nextip[0] && Routing->next_hop[i][0] == nextip[0]) // ���·�ɱ�����Ŀ��IP����һ����Ŀ��·�������� ��������Ϊ16 ʵ�ֶ��Է�תF
			{
				data = data + Routing->desip[i] + (char)16;
			}
			else
			{
				data = data + Routing->desip[i] + (char)Routing->metric[i];
			}

		}
		cout << endl << "---------����·�ɱ���Ϣ���˿� " << iface << " ---------" << endl << endl;
		//δ��ѯ��mac
		if (desmac == NULL) {
			if (data.size() != 0) {
				addHeader(bufSend, NULL, srcip, nextip, ttl_max, '1');
				//strcpy(&bufSend[headerLen + lowerIciLen], data.data()); //��������
				//strcpy(&bufSend[headerLen + lowerIciLen], strdup(data.c_str()));//�������� �����Ҳ٣�
				for (int i = 0; i < data.size(); i++) {
					bufSend[i + lowerIciLen + headerLen] = data[i];
				}
			}
			//���뻺����
			add_to_buffer(buffer, bufSend, nextip, len, IP[int(rPort[i]) - '0'], iface);

			//��ӡ������
			if (PRINT) {
				cout << "****************��������������ip****************" << endl;
				for (list<list<U8*>>::iterator i = (*buffer).begin(); i != (*buffer).end(); i++) {
					cout << "ip:" << (*i).front()[0] << "." << (*i).front()[1] << endl;
				}
				cout << "************************************************" << endl << endl;
			}

			//����arp���� 'F'Ϊ�㲥
			send_arp('F', IP[int(rPort[i]) - '0'], nextip, ttl_max, iface);
			if (PRINT) {
				cout << "<<<<<<<<<<<<<<<<<<<<��ARP��δ��ѯ���������ARP������>>>>>>>>>>>>>>>>>" << endl;
				cout << "����IP��" << nextip[0] << "." << nextip[1] << endl << endl;
			}

		}
		else {
			if (data.size() != 0) {
				addHeader(bufSend, desmac, srcip, nextip, ttl_max, '1');
				//strcpy(&bufSend[headerLen + lowerIciLen], strdup(data.c_str()));//�������� �����Ҳ٣�
				for (int i = 0; i < data.size(); i++) {
					bufSend[i + lowerIciLen + headerLen] = data[i];
				}
			}
			SendtoLower(bufSend, len, iface);
		}
	}
}
// ��ӡ·�ɱ�

void print_routing(Routing_Table* Routing) {
	cout << endl << "����������������������!·�ɱ�!��������������������" << endl;
	if (mode) {
		for (int i = 0; i < Routing->len; i++) {
			if (Routing->next_hop[i][0] != NULL) {
				cout << "  Ŀ�����磺" << Routing->desip[i] << "||" << "��һ��ip��" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "||" << "���ӿ�:" << Routing->iface[i] << "||����:" << Routing->metric[i] << endl;
			}
			else {
				cout << "  Ŀ�����磺" << Routing->desip[i] << "||" << "��һ��ip��" << "N" << "." << "N" << "||" << "���ӿ�:" << Routing->iface[i] << "||����:" << Routing->metric[i] << endl;
			}
		}
	}
	else {
		for (int i = 0; i < Routing->len; i++) {
			if (Routing->next_hop[i][0] != NULL) {
				cout << "  Ŀ�����磺" << Routing->desip[i] << "||" << "��һ��ip��" << Routing->next_hop[i][0] << "." << Routing->next_hop[i][1] << "||" << "���ӿ�:" << Routing->iface[i] << endl;
			}
			else {
				cout << "  Ŀ�����磺" << Routing->desip[i] << "||" << "��һ��ip��" << "N" << "." << "N" << "||" << "���ӿ�:" << Routing->iface[i] << endl;
			}
		}
	}
	cout << "  ��" << Routing->len << "��·����Ϣ" << endl;
	cout << "����������������������������������������������������" << endl << endl;
}
//��ӡͳ����Ϣ

void print_statistics();

void load_rPort(string& data) {
	U8 nexthop[2];
	deleteMark(data, ","); // ȥ���ָ����
	deleteMark(data, "-"); // ȥ���ָ����
	deleteMark(data, "."); // ȥ���ָ����
	deleteMark(data, " "); // ȥ���ָ����
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
					cout << "<<<<<<<<<<<<<<<<<<<<��ARP��ʱ������ARP������>>>>>>>>>>>>>>>>>" << endl;
					cout << "����IP��" << desip[0] << "." << desip[1] << endl << endl;
				}
			}
		}
	}
}

void menu();
//***************��Ҫ��������******************************
//���ƣ�InitFunction
//���ܣ���ʼ�������棬��main�����ڶ��������ļ�����ʽ������������ǰ����
//���룺
//�����
void InitFunction(CCfgFileParms& cfgParms)
{
	srand((unsigned)time(NULL) + stoi(strDevID) * 1000000);
	nextSendTime += rand() % (SEND_TIME / 3);
	rPort = cfgParms.getValueStr("rPort");
	load_rPort(rPort);
	strIP = cfgParms.getValueStr("IP"); // IP��ַ��ȡ
	cout << "����" << lowerNumber << "���˿�" << " ����";
	for (int i = 0; i < rPort.size(); i++) {
		if (!i)
			cout << rPort[i];
		else
			cout << " " << rPort[i];

	}
	if (!rPort.size())
		cout << "��";
	cout << "�˿�����·����" << endl;
	init_interface_ip(Routing, strIP, IP);//����Լ����ж˿ڵ�IP��ַ
	int reval = cfgParms.getValueInt(mode, "mode"); //��ȡѡ���ģʽ
	if (reval == -1)
		mode = 0; // Ĭ��Ϊ��̬·��
	if (mode)
		cout << "·����ʹ�ö�̬·��Э��" << endl;
	else {
		cout << "·����ʹ�þ�̬·��Э��" << endl;
		string txt = "Static routing" + strDevID + ".txt";
		loadStaticRouter(txt);
	}

	if (PRINT) {
		print_routing(Routing);
	}
	//��������
	Gateway[0] = IP[0][0];
	Gateway[1] = '1';
	sendbuf = (char*)malloc(MAX_BUFFER_SIZE);
	if (sendbuf == NULL) {
		cout << "�ڴ治��" << endl;
		//����������Ҳ̫���˳���
		exit(0);
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
	if (sendbuf != NULL)
		free(sendbuf);
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
void TimeOut()
{
	if (mode) {
		if (check_routing(Routing)) {
			// ����Ƿ�����ϻ�
			srand((unsigned)time(NULL) + stoi(strDevID) * 1000000);
			// ������ø��¶�ʱ�� ��ֹ���·����ͬʱ����·�ɱ�����ɶ���
			nextSendTime = rand() % (SEND_TIME / 3) - (SEND_TIME / 6) + SEND_TIME;
			cout << "-------------·�ɱ�������-------------" << endl;
			send_routing(Routing);
		}
		if (!--nextSendTime) {

			cout << endl << "------------��ʱ����·�ɱ�-------------" << endl << endl;
			send_routing(Routing);
			srand((unsigned)time(NULL) + stoi(strDevID) * 1000000);
			// ������ø��¶�ʱ�� ��ֹ���·����ͬʱ����·�ɱ�����ɶ���
			nextSendTime = rand() % (SEND_TIME / 3) - (SEND_TIME / 6) + SEND_TIME;

		}
	}
	if (CLOCK >= 20) {
		reget_ARP(buffer);//��ʱ����ARP
		CLOCK = 0;
	};
	CLOCK++;
	printCount++;
	if (_kbhit()) {
		//�����ж���������˵�ģʽ
		menu();
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
{
	//��
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
{
	Net_Lower_ICI* lowerIci = (Net_Lower_ICI*)buf;// ȡ�²�ICI
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
	// ·�������±���
	if (head->Flag == '1') {
		if (PRINT) {
			cout << "=============����·�ɱ���=============" << endl;
			cout << "Ŀ��IPΪ:" << head->DesIP[0] << "." << head->DesIP[1] << endl;
			cout << "ԴIPΪ:" << head->SrcIP[0] << "." << head->SrcIP[1] << endl;
			cout << endl << endl;
		}
		vector<string> data = get_routing(Routing, &bufSend[lowerIciLen + headerLen], len - lowerIciLen - headerLen);
		if (learn_routing(Routing, data, head->SrcIP, ifNo)) { // ѧϰ·�ɱ� ����������� ��������
			srand((unsigned)time(NULL) + stoi(strDevID) * 1000000);
			// ������ø��¶�ʱ�� ��ֹ���·����ͬʱ����·�ɱ�����ɶ���
			nextSendTime = rand() % (SEND_TIME / 3) - (SEND_TIME / 6) + SEND_TIME;
			cout << "-------------·�ɱ�������-------------" << endl;
			send_routing(Routing);
		}
	}

	//����
	else if (head->Flag == '2') {
		//��ӡ������Ϣ
		if (PRINT) {
			cout << "=============�����²����ݱ���=============" << endl;
			cout << "Ŀ��IPΪ:" << head->DesIP[0] << "." << head->DesIP[1] << endl;
			cout << "ԴIPΪ:" << head->SrcIP[0] << "." << head->SrcIP[1] << endl;
			cout << "������ϢΪ��";
			for (int i = 0; i < len; i++) {
				cout << bufSend[i];
			}
			cout << endl << endl;
		}

		find_in_routingtable(Routing, head->DesIP, nextip, &iface);
		//·�ɱ���û����ñ���ƥ��ı�����߳��ӿ��뱨�Ľ���Ľӿ���ͬ��ֱ�Ӷ���
		if (iface == -1 || iface == ifNo)
		{
			return;
		}

		find_in_ARP(ARP, nextip, &desMAC);

		//δ��ѯ��mac
		if (desMAC == NULL)
		{
			if (PRINT) {
				cout << "+++++++++++++++ARP����δ��ѯ��+++++++++++++++" << endl;
				cout << "��ѯIPΪ:" << nextip[0] << "." << nextip[1] << endl;
				cout << endl << endl;
			}

			//���뻺����
			((Net_Header*)(bufSend + lowerIciLen))->TTL--;
			if (((Net_Header*)(bufSend + lowerIciLen))->TTL != 0) {
				add_to_buffer(buffer, bufSend, nextip, len, IP[iface], iface);
				//��ӡ������
				if (PRINT) {
					cout << "****************��������������ip****************" << endl;
					for (list<list<U8*>>::iterator i = (*buffer).begin(); i != (*buffer).end(); i++) {
						cout << "ip:" << (*i).front()[0] << "." << (*i).front()[1] << endl;
					}
					cout << "************************************************" << endl << endl;
				}

				//����arp���� 'F'Ϊ�㲥
				send_arp('F', IP[iface], nextip, ttl_max, iface);

				if (PRINT) {
					cout << "<<<<<<<<<<<<<<<<<<<<��ARP��δ��ѯ���������ARP������>>>>>>>>>>>>>>>>>" << endl;
					cout << "����IP��" << nextip[0] << "." << nextip[1] << endl << endl;
				}
			}
		}
		else
		{
			if (PRINT) {
				cout << "+++++++++++++++ARP���в�ѯ��MAC+++++++++++++++" << endl;
				cout << "��ѯIPΪ:" << nextip[0] << "." << nextip[1] << endl;
				cout << "��ѯMACΪ:" << desMAC << endl;
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
	{//arp����
		if (head->DesIP[0] == IP[ifNo][0] && head->DesIP[1] == IP[ifNo][1]) {

			if (PRINT) {
				cout << "=============����ARP����=============" << endl;
				cout << "�����IPΪ:" << head->DesIP[0] << "." << head->DesIP[1] << endl;
				cout << "ԴIPΪ:" << head->SrcIP[0] << "." << head->SrcIP[1] << endl;
				cout << endl << endl;
			}

			send_rearp(lowerIci->IciMAC, head->DesIP, head->SrcIP, ttl_max, ifNo);

			if (PRINT) {
				cout << "������������������������������ARP�ظ�������������������������" << endl;
				cout << "Ŀ��IPΪ:" << head->SrcIP[0] << "." << head->SrcIP[1] << endl;
				cout << "ԴIPΪ:" << head->DesIP[0] << "." << head->DesIP[1] << endl;
				cout << endl << endl;
			}
		}
	}
	else if (head->Flag == '8') {

		if (PRINT) {
			cout << "����������������ARP�ظ�������������" << endl;
			cout << "�ط�MAC:" << lowerIci->IciMAC << endl;
			cout << "ԴIPΪ:" << head->SrcIP[0] << "." << head->SrcIP[1] << endl;
			cout << endl << endl;
		}

		ARP_learning(ARP, head->SrcIP, &lowerIci->IciMAC);

		if (PRINT) {
			cout << "****************ARP��****************" << endl;
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
	cout << endl << "1-��ӡ·�ɱ�;" << endl << "2-ֹͣ�Զ����ͣ���Ч��; " << endl << "3-�Ӽ������뷢��; ";
	cout << endl << "4-����ӡͳ����Ϣ; " << endl << "5-����������ӡ��������;" << endl << "6-���ֽ�����ӡ��������;";
	cout << endl << "0-ȡ��" << endl << "����������ѡ�����";
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
			bufSend = (U8*)malloc(len * 8);

			iSndRetval = ByteArrayToBitArray(bufSend, len * 8, kbBuf, len);
			iSndRetval = SendtoLower(bufSend, iSndRetval, port);
			free(bufSend);
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
// ���ı����ؾ�̬·��
int loadStaticRouter(string file) {
	vector<string> vec = readData(file);
	char nextLoop[2];
	for (int i = 0; i < vec.size(); i++) {
		////���油�� ����·�ɱ�
		//cout << vec[i] << endl;//��i������ 
		//cout << vec[i].at(0) << endl; // Ŀ�������
		nextLoop[0] = vec[i].at(2);
		nextLoop[1] = vec[i].at(4); // ���������ӣ�����
		add_routing(Routing, vec[i].at(0), (int)vec[i].at(6) - '0', nextLoop, 0, 1);
		//cout << vec[i].at(2) << endl; // ��һ�������
		//cout << vec[i].at(4) << endl; // ��һ��������
		//cout << (int)vec[i].at(6) - '0' << endl; // ���ӿ�

	}
	return vec.size();
};