//Nettester �Ĺ����ļ�
#include <iostream>
#include <conio.h>
#include "winsock.h"
#include "stdio.h"
#include "CfgFileParms.h"
#include "function.h"
using namespace std;
#define PRINT 0 //��ӡ��Ϣ
#define broadcast 'F' //�㲥֡Ŀ�ĵ�ַ

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
int CLOCK = 200; //���ڵ㾺ѡʱ��
int CLOCK_BPDU = 150; //BPDU֡��ʱ��
int port_status[20];//�˿�״̬
int BPDU_port = -1; //�ж�BPDU�Ƿ���չ�
U8 belongto; //�ж��Լ������Ǹ���������
int iRcvUnknownCount = 0;  //�յ�������Դ�����ܴ���
//ת����
typedef struct LIST {
	U8 content[60][2];
	int length = 0;
}list;
list change_list;

//��У����㣨����add_head��ʹ�ã�
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
//��������ѧϰ
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

//��ѯת����
void find_how_to_give(U8 mac_des, int* ifNo) {
	for (int i = 0; i < change_list.length; i++) {
		if (change_list.content[i][0] == mac_des) {
			*ifNo = int(change_list.content[i][1]);
			return;
		}
	}
	*ifNo = 404;
}

//���͸��ڵ㾺ѡ֡
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
		cout << "*********<<<<��������������ѡ���ڵ㿪ʼ>>>>********" << endl << endl;
	}
	for (int j = 0; j < 5; j++) {
		for (int i = 0; i < lowerNumber; i++) {
			SendtoLower(bufSend, vote_len * 8, i); //��������Ϊ���ݻ��壬���ȣ��ӿں�
		}
	}
	if (vote_root != NULL) {
		free(vote_root);
	}
	if (bufSend != NULL) {
		free(bufSend);
	}
}

//����BPDU֡
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
		cout << "*********<<<<����BPDU>>>>********" << endl << endl;
	}
	for (int j = 0; j < 5; j++) {
		for (int i = 0; i < lowerNumber; i++) {
			SendtoLower(bufSend, BPDU_len * 8, i); //��������Ϊ���ݻ��壬���ȣ��ӿں�
		}
	}
	if (BPDU_send != NULL) {
		free(BPDU_send);
	}
	if (bufSend != NULL) {
		free(bufSend);
	}
}

//��ӡͳ����Ϣ
void print_statistics();

void menu();

//***************��Ҫ��������******************************
//���ƣ�InitFunction
//���ܣ���ʼ�������棬��main�����ڶ��������ļ�����ʽ������������ǰ����
//���룺
//�����
void InitFunction(CCfgFileParms& cfgParms)
{
	sendbuf = (char*)malloc(1000);
	if (sendbuf == NULL) {
		cout << "�ڴ治��" << endl;
		//����������Ҳ̫���˳���
		exit(0);
	}
	belongto = strDevID.c_str()[0];
	for (int i = 0; i < lowerNumber; i++) {
		port_status[i] = 1;
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
		cout << "�ڴ�ռ䲻������������û�б�����" << endl;
		return;
	}

	//У��ͽ��
	get_data(buf, len, bufSend, &iSndRetval, &flag, &index, &mac_src, &mac_des);


	//У����ȷ
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
					cout << "<<<<<<<<<<�������>>>>>>>>>>" << endl;
					cout << "��ʱ�Ĺ���Ϊ��" << belongto << endl << endl;
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
						cout << "::::::::::����BPDU:::::::::::" << endl;
						cout << "���˿�" << ifNo << "����" << endl << endl;
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

				//��ӡ��Ϣ
				if (PRINT)
				{
					cout << endl;
					cout << "///////////////////////////////////����֡///////////////////////////////////" << endl << endl;
					cout << "֡�����Ϊ��" << int(index) << endl << endl;
					cout << "֡��ģʽΪ��" << flag << endl << endl;
					cout << "Դ��ַΪ��" << mac_src << endl << endl;
					cout << "Ŀ�ĵ�ַΪ��" << mac_des << endl << endl;
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

					U8* bit_buf = (U8*)malloc(iSndRetval * 8);
					ByteArrayToBitArray(bit_buf, iSndRetval * 8, bufSend, iSndRetval);
					cout << "��Ϣ�ı�����Ϊ��";
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
				//��ӡ�鿴ת����
				if (PRINT)
				{
					cout << "----------" << "ת����" << "-----------" << endl;
					for (int i = 0; i < change_list.length; i++) {
						cout << "Ŀ�ĵ�ַ��" << change_list.content[i][0] << "  ת���˿ڣ�" << int(change_list.content[i][1]) << endl;
					}
					cout << "---------------------------" << endl;
				}

				find_how_to_give(mac_des, &ifNo_to);

				//�㲥
				if (ifNo_to == 404 || mac_des == broadcast) {
					for (int i = 0; i < lowerNumber; i++) {
						if (ifNo == i) {
							continue;
						}
						iSndRetval = SendtoLower(buf, len, i);
					}
				}//����
				else {
					iSndRetval = SendtoLower(buf, len, ifNo_to);
				}
			}
		}

	}

	//У�����
	else {

		//��ӡ��Ϣ
		if (PRINT)
		{
			cout << endl;
			cout << "///////////////////////////////////����֡///////////////////////////////////" << endl << endl;
			cout << "���յ������ǣ�";
			for (int w = 0; w < len / 8; w++) {
				cout << bufSend[w];
			}
			cout << endl << endl;
			cout << "���յ�bit��Ϊ��";
			for (int w = 0; w < len; w++) {
				cout << int(buf[w]);
				if (w % 4 == 3) {
					cout << " ";
				}
			}
			cout << endl;
		}

		//ͳ��
		if (iSndRetval <= 0) {
			iSndErrorCount++;
		}
		else {
			iRcvToUpper += iSndRetval;
			iRcvToUpperCount++;
		}
		//�����Ҫ�ش��Ȼ��ƣ�������Ҫ��buf��bufSend�е�������������ռ仺������
	}

	if (bufSend != NULL) {
		//����bufSend���ݣ�����б�Ҫ�Ļ�
		//��������û��ͣ��Э�飬bufSend�Ŀռ��������Ժ���Ҫ�ͷ�
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