//Nettester �Ĺ����ļ�
#include <iostream>
#include <conio.h>
#include <list>  
#include "winsock.h"
#include "stdio.h"
#include "CfgFileParms.h"
#include "function.h"
#pragma warning(disable:4996)
using namespace std;


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
	int len;
	U8 desip[MAX_LEN_ARP];
	U8 next_hop[MAX_LEN_ARP][2];
	int iface[MAX_LEN_ARP];
};

// ARP��
struct ARP_Table {
	int len;
	U8 desip[MAX_LEN_R][2];
	U8 desMAC[MAX_LEN_R];
};


//����Ϊ��Ҫ�ı���
U8* sendbuf;        //������֯�������ݵĻ��棬��СΪMAX_BUFFER_SIZE,���������������������ƣ��γ��ʺϵĽṹ��������û��ʹ�ã�ֻ������һ��
int printCount = 0; //��ӡ����
int spin = 0;  //��ӡ��̬��Ϣ����

//------�����ķָ��ߣ�һЩͳ���õ�ȫ�ֱ���------------
int iSndTotal = 0;  //������������
int iSndTotalCount = 0; //���������ܴ���
int iSndErrorCount = 0;  //���ʹ������
int iRcvForward = 0;     //ת����������
int iRcvForwardCount = 0; //ת�������ܴ���
int iRcvToUpper = 0;      //�ӵͲ�ݽ��߲���������
int iRcvToUpperCount = 0;  //�ӵͲ�ݽ��߲������ܴ���
int iRcvUnknownCount = 0;  //�յ�������Դ�����ܴ���
int upperIciLen = 2;//�ϲ�ici����
int lowerIciLen = 1;//�²�ici����
int headerLen = 6;//����ͷ������
int CLOCK = 0;//����ARP�ط���ʱ��
int group_clock;//���ڷ��鷢�͵�ʱ��
int	recLen = 0;//���������ڽ��յĳ���
int INDEX = 0;//���������ڼ�¼�����
U8 ttl_max = 'a';//�������
string strIP;  // IP��ַ
U8 IP[10][2]; //���豸���е�IP��ַ����ӿ�0��IPΪIP[0]
U8 Gateway[2];//����
U8* all_buf;//����ϲ�������

// ��������
list<list<U8*>> ourbuffer;
list<list<U8*>>* buffer = &ourbuffer;

// ���黺��
list<U8*> zu;
list<U8*>* groups = &zu;

//������Ĵ���
Routing_Table routing_table;
Routing_Table* Routing = &routing_table;
ARP_Table arp_table;
ARP_Table* ARP = &arp_table;


//���鸴�ƺ���
void copy(U8* a, U8* b, int len) {
	for (int i = 0; i < len; i++) {
		a[i] = b[i];
	}
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
		Routing->desip[Routing->len] = IP[j][0];
		Routing->iface[Routing->len] = j;
		Routing->next_hop[Routing->len][0] = NULL;
		Routing->next_hop[Routing->len][1] = NULL;
		Routing->len++;
	}
}

//ARP��غ���************
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

//ARP�ط�����
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

//��֡��غ���************
//�����ݷ��鹦��
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

//�����鷢��һ����
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

//ʵ�ַ���ϲ���ֱ���滻SendtoUpper����
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
//***************��Ҫ��������******************************
//���ƣ�InitFunction
//���ܣ���ʼ�������棬��main�����ڶ��������ļ�����ʽ������������ǰ����
//���룺
//�����
void InitFunction(CCfgFileParms& cfgParms)
{
	strIP = cfgParms.getValueStr("IP"); // IP��ַ��ȡ
	init_interface_ip(Routing, strIP, IP);//����Լ����ж˿ڵ�IP��ַ
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
	if (CLOCK++ >= 20) {
		reget_ARP(buffer);//��ʱ����ARP
		CLOCK = 0;
	};
	if (group_clock++ == 5) {
		send_group(groups, 1);
		group_clock = 0;
	}
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
	if (len > MAX_group) {
		add_to_groups(groups, buf, len);
		return;
	}

	int newlen = len;//����ͷ����ĳ���
	Net_Upper_ICI* upperIci = (Net_Upper_ICI*)buf;
	U8 desip[2];//��ȡ�ϲ�ICI
	desip[0] = upperIci->IciIP[0];
	desip[1] = upperIci->IciIP[1];
	newlen += headerLen + lowerIciLen;
	U8* sendBuf = (U8*)malloc(newlen + 4);//����������;+4��Ϊ�˼��뻺�����õı�ɾ
	sendBuf = &sendBuf[4];//��ǰ���ĸ��ֽ������������ʱ����
	U8 desMAC;
	U8 nextip[2];

	//��ӡ���յ���Ϣ
	if (PRINT) {
		cout << "________________�����ϲ����ݱ���_______________" << endl;
		cout << "Ŀ��IPΪ:" << desip[0] << "." << desip[1] << endl;
		cout << "���ĳ���Ϊ��" << len << endl;
		cout << "������ϢΪ��";
		for (int i = 0; i < len; i++) {
			cout << buf[i];
		}
		cout << endl;
		cout << "_______________���²㷢�����ݱ���______________" << endl << endl;
	}

	//pc�ж�Ŀ�ĵ�ַ�Ƿ���Լ���ͬһ����
	if (desip[0] == IP[0][0]) {
		nextip[0] = desip[0];
		nextip[1] = desip[1];
	}
	else {//����ͬ���Σ���������
		nextip[0] = Gateway[0];
		nextip[1] = Gateway[1];
	}
	find_in_ARP(ARP, nextip, &desMAC);
	//δ��ѯ��mac
	if (desMAC == NULL) {
		if (sendBuf != NULL) {
			addHeader(sendBuf, NULL, IP[0], desip, ttl_max, '2');//mac��ַ���ÿ�
			copy(&sendBuf[headerLen + lowerIciLen], buf, len);// ������ͷ�װ��ͷ���� sendBuf����PDU
		}

		//���뻺����
		add_to_buffer(buffer, sendBuf, nextip, newlen, nextip, 0);

		//��ӡ������
		if (PRINT) {
			cout << "****************��������������ip****************" << endl;
			for (list<list<U8*>>::iterator i = (*buffer).begin(); i != (*buffer).end(); i++) {
				cout << "ip:" << (*i).front()[0] << "." << (*i).front()[1] << endl;
			}
			cout << "************************************************" << endl << endl;
		}

		//����arp���� 'F'Ϊ�㲥
		send_arp('F', IP[0], nextip, ttl_max, 0);
		if (PRINT) {
			cout << "<<<<<<<<<<<<<<<<<<<<��ARP��δ��ѯ���������ARP������>>>>>>>>>>>>>>>>>" << endl;
			cout << "����IP��" << nextip[0] << "." << nextip[1] << endl << endl;
		}

	}
	else {
		if (sendBuf != NULL) {
			addHeader(sendBuf, desMAC, IP[0], desip, ttl_max, '2');
			copy(&sendBuf[headerLen + lowerIciLen], buf, len); // ������ͷ�װ��ͷ���� sendBuf����PDU
		}
		SendtoLower(sendBuf, newlen, 0);
	}


	if (sendBuf - 4 != NULL) {
		free(sendBuf - 4);
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
{
	Net_Lower_ICI* lowerIci = (Net_Lower_ICI*)buf;// ȡ�²�ICI ����  
	Net_Upper_ICI* upperIci = (Net_Upper_ICI*)&buf[headerLen + lowerIciLen];//�ϲ�ICI
	Net_Header* head = (Net_Header*)&buf[lowerIciLen];
	int newlen = len - headerLen - lowerIciLen;
	U8* bufSend = (U8*)malloc(newlen);
	copy(bufSend, buf + headerLen + lowerIciLen, newlen);




	//PC�鿴��������Ƿ��Ƿ����Լ���,���Ǿ�ֱ����
	if (head->DesIP[0] != IP[ifNo][0] || head->DesIP[1] != IP[ifNo][1]) {
		return;
	}

	//����
	if (head->Flag == '2') {
		//��ӡ������Ϣ
		if (PRINT) {
			cout << "========================�����²����ݱ���========================" << endl;
			cout << "Ŀ��IPΪ:" << head->DesIP[0] << "." << head->DesIP[1] << endl;
			cout << "ԴIPΪ:" << head->SrcIP[0] << "." << head->SrcIP[1] << endl;
			cout << "������ϢΪ��";
			for (int i = 0; i < newlen; i++) {
				cout << bufSend[i];
			}
			cout << endl << "=======================���ϲ㷢�����ݱ���=======================" << endl;
			cout << endl;
		}

		bufSend[0] = head->SrcIP[0];
		bufSend[1] = head->SrcIP[1];

		send_to_upper(bufSend, newlen);
	}
	else if (head->Flag == '4') {//arp����

		if (PRINT) {
			cout << "========================����ARP����========================" << endl;
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
				cout << "ip:" << ARP->desip[i][0] << "." << ARP->desip[i][1] << "||" << "MAC:" << ARP->desMAC[i] << endl;
			}
			cout << "*************************************" << endl << endl;
		}

		//���ͺ���
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
