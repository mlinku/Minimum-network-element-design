// OneTouch2Go.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <string>
#include <fstream>
#include <atlstr.h>
#include <vector>
using namespace std;

//层次名与执行程序名的对应表,填写这个表以后，本软件就能根据配置文件中的参数自动打开相应的程序，并提供启动参数
//命令行的格式为：程序名 设备号 实体号
struct functionMap_t {
	string LayerName;
	string funcitonName;
	int len; //层次名字字符串的长度
}aFunctionMap[10];
struct typeMap_t {
	string typeName;
	vector<string> DevID;
	int len;
}aTypeMap[2];
string getFunction(string strName)
{
	int i;
	for (i = 0; i < 10; i++)
	{
		if (aFunctionMap[i].LayerName.empty() || strName.empty()) {
			continue;
		}
		if (strName.compare(aFunctionMap[i].LayerName) == 0) {
			//找到
			return aFunctionMap[i].funcitonName;
		}
	}
	//没找到
	return "";
}
string getType(string strName)
{
	int i;
	for (i = 0; i < 2; i++)
	{
		if (aTypeMap[i].typeName.empty() || strName.empty()) {
			continue;
		}
		for (int j = 0; j < aTypeMap[i].DevID.size(); j++)
			if (strName.compare(aTypeMap[i].DevID[j]) == 0) {
				//找到
				return aTypeMap[i].typeName;
			}
	}
	//没找到
	return "PC";
}
/*
string getFunction(char* cpName)
{
	int i;
	for (i = 0; i < 7; i++)
	{
		if (aFunctionMap[i].LayerName.empty()) {
			continue;
		}
		if (strncmp(aFunctionMap[i].LayerName.c_str(), cpName, strlen(aFunctionMap[i].LayerName.c_str())) == 0) {
			//找到
			return aFunctionMap[i].funcitonName;
		}
	}
	//没找到
	return NULL;
}
*/
//从设备文件中需要读出：
//1、有多少个设备，每个设备的编号（以整数保留）
//2、每个设备有多少个层次，每个层次的名称，查映射表，以准备system命令
//3、每个层次有多少个实体，每个实体的编号（以整数保存）
//4、逐个执行各条命令，并给与设备号，实体号参数，（层次对应程序本身）
//注，程序打开后如果要调整位置，根据设备文件中的布局参数完成
vector <string*> archSection;

void myStrcpy(string& dst, string& src) //只保留ASCII码大于32的字符，32为空格，以下的都是控制字符
{
	size_t i;
	const char* cpSrc;
	dst.clear();

	if (src.empty()) {
		return dst.clear();
	}
	cpSrc = src.c_str();
	for (i = 0; i < strlen(cpSrc); i++) {
		if (cpSrc[i] > 32 || cpSrc[i] < 0) {
			dst.append(1, cpSrc[i]);
		}
	}
}
void split(const string& src, const string& separator, vector<string>& dest)
{
	string str = src;
	string substring;
	string::size_type start = 0, index;
	dest.clear();
	index = str.find_first_of(separator, start);
	do
	{
		if (index != string::npos)
		{
			substring = str.substr(start, index - start);
			dest.push_back(substring);
			start = index + separator.size();
			index = str.find(separator, start);
			if (start == string::npos) break;
		}
	} while (index != string::npos);

	//the last part
	substring = str.substr(start);
	dest.push_back(substring);
}


void readMapFile(ifstream& f)
{
	vector<string> Data;
	string strTmp;
	string csLeft;
	string csRight;
	string csLayer;
	string csFunc;
	int i = 0;
	while (!f.eof()) {
		getline(f, strTmp);
		if (strTmp.empty()) {
			continue;
		}
		if (strTmp.find("=") < 0) {
			continue;
		}
		csLeft = strTmp.substr(0, strTmp.find("=") - 1);

		csRight = strTmp.substr(strTmp.find("=") + 1, strTmp.length() - strTmp.find("="));

		myStrcpy(csLayer, csLeft);

		myStrcpy(csFunc, csRight);

		aFunctionMap[i].LayerName = csLayer;
		aFunctionMap[i].funcitonName = csFunc;
		i++;
	}
}
void readTypeFile(ifstream& f)
{
	string strTmp;
	string csLeft;
	string csRight;
	string csType;
	string csId;
	int i = 0;
	while (!f.eof()) {
		getline(f, strTmp);
		if (strTmp.empty()) {
			continue;
		}
		if (strTmp.find("=") < 0) {
			continue;
		}
		csLeft = strTmp.substr(0, strTmp.find("=") - 1);

		csRight = strTmp.substr(strTmp.find("=") + 1, strTmp.length() - strTmp.find("="));

		myStrcpy(csType, csLeft);

		myStrcpy(csId, csRight);

		aTypeMap[i].typeName = csType;
		//cout << csType << endl;
		split(csId, ",", aTypeMap[i].DevID);
		//for (int j = 0; j < aTypeMap[i].DevID.size(); j++)
		//{
		//	cout << aTypeMap[i].DevID[j] << endl;
		//}
		i++;
	}
}
//判断本行是否为分割行，依据是连续的----
bool isSplitLine(string* pstr)
{
	int retval;
	// TODO: 在此处添加实现代码.
	retval = (int)pstr->find("--------");
	if (retval < 0) {
		return false;
	}
	return true;
}
//判断本行是否是参数行，依据是开头的#
bool isParmsLine(string* pstr)
{
	// TODO: 在此处添加实现代码.
	if (pstr->at(0) == '#' || isSplitLine(pstr)) {
		return false;
	}
	return true;
}
// 从描述字串中提取设备号，如果字串以空格开始，则提取不到。
string getDev(string* pstr)
{
	int retval;
	// TODO: 在此处添加实现代码.
	retval = (int)pstr->find_first_of(" ");
	if (retval <= 0) {
		return string();
	}
	return pstr->substr(0, retval);
}
//从截取好的实体描述字段中，提取层次号和实体号
int getLayerAndEnt(string* str, string& strLayer, string& strEnt)
{
	int retval;
	int i;
	//要从str中找出实体号字段
	retval = (int)str->find("@");
	if (retval == -1) {
		//没有IP地址，最后1到两个字符是数字
		strEnt = str->c_str();
	}
	else {
		//取@之前
		strEnt = str->substr(0, retval);
	}
	for (i = (int)strEnt.length() - 1; i >= 0; i--) {
		if (strEnt.at(i) < '0' || strEnt.at(i) > '9')
			break;
	}
	strEnt = strEnt.substr(i + 1, strEnt.length() - i - 1);
	//cout << strEnt << endl;
	strLayer = str->substr(0, i + 1);
	//cout << strLayer << endl;


	return 0;
}

int main()
{
	int mode = 0; // 1为pc 2为switch 3为router
	//打开映射文件
	ifstream mapFile("map.txt");
	if (!mapFile.is_open())
	{
		cout << "没有找到map.txt文件";
		return 0;
	}
	readMapFile(mapFile); // 读取映射	
	mapFile.close();

	ifstream typeFile("type.txt");
	if (!typeFile.is_open())
	{
		cout << "没有找到type.txt文件";
		return 0;
	}
	readTypeFile(typeFile); // 读取映射

	typeFile.close();
	//打开配置文件
	ifstream cfgFile("ne.txt");
	if (!cfgFile.is_open())
	{
		return 0;
	}
	//遍历配置文件
	//通过设备号，层次名，和实体号，得到四个参数组:basic, lower , upper，peer
	string* pstrTmp;
	string strTmp;
	while (!cfgFile.eof()) {
		getline(cfgFile, strTmp);
		if (isSplitLine(&strTmp)) {
			break;
		}
	}
	if (cfgFile.eof()) {
		//没有读到有效内容
		//isConfigExist = 0;
		cfgFile.close();
		return -2;
	}
	//读入架构和地址区域内容
	while (!cfgFile.eof()) {
		getline(cfgFile, strTmp);
		if (isSplitLine(&strTmp)) {
			break;
		}
		if (!isParmsLine(&strTmp)) {
			continue;
		}
		pstrTmp = new string(strTmp.c_str());
		archSection.push_back(pstrTmp);
	}

	cfgFile.close();
	if (archSection.size() == 0) {
		return 0;
	}
	int NECount = 0;
	size_t index;
	string strDev;
	string strLayer;
	string strEntity;
	string strCmd;
	string strParm;
	string typeName;
	int begin;
	int end;

	for (index = 0; index < archSection.size(); index++) {
		//读出一个，执行一个
		pstrTmp = archSection[index];
		//读设备号
		begin = 0;
		end = (int)pstrTmp->find_first_of(" ");
		if (end < 0) {
			//这一行不对
			continue;
		}
		else if (end > 0) {
			//如果有设备号，就提取，否则就用之前的
			strDev = pstrTmp->substr(0, end);
		}
		//cout << strDev << endl;
		typeName = getType(strDev);
		cout << endl << typeName << endl;
		if (typeName == "PC")
			mode = 1;
		else if (typeName == "SWITCH")
			mode = 2;
		else if (typeName == "ROUTER")
			mode = 3;
		//for(int i<0;i< aTypeMap)
		begin = end;
		while (begin < (int)pstrTmp->length() && begin >= 0) {
			//跳过空格
			begin = (int)pstrTmp->find_first_not_of(' ', begin);
			if (begin == -1) {
				//跳过空行
				break;
			}
			//跳过注释行
			if ((int)pstrTmp->find('#', 0) >= 0) {
				break;
			}
			//截取
			end = (int)pstrTmp->find_first_of(' ', begin);
			if (end == -1) {
				//后面没有了
				end = (int)pstrTmp->length();
			}
			strTmp = pstrTmp->substr(begin, end - begin);
			begin = end;
			//cout << strTmp << endl;

			getLayerAndEnt(&strTmp, strLayer, strEntity);
			if (strLayer == "LNK") {
				switch (mode) {
				case 1:
					strCmd = getFunction(strLayer + "_PC");
					break;
				case 2:
					strCmd = getFunction(strLayer + "_SWITCH");
					break;
				case 3:
					strCmd = getFunction(strLayer + "_ROUTER");
					break;
				}
			}
			else if (strLayer == "NET") {
				switch (mode) {
				case 1:
					strCmd = getFunction(strLayer + "_PC");
					break;
				case 2:
					strCmd = getFunction(strLayer + "_SWITCH");
					break;
				case 3:
					strCmd = getFunction(strLayer + "_ROUTER");
					break;
				}
			}
			else
				strCmd = getFunction(strLayer);
			cout << strCmd << endl;
			strParm = strDev; // 设备号
			strParm += " ";
			strParm += strLayer; // 层次
			strParm += " ";
			strParm += strEntity; // 层次号
			//cout << strParm << endl;
			ShellExecute(NULL, _T("open"), strCmd.c_str(), strParm.c_str(), NULL, SW_MINIMIZE);
			////ShellExecute(NULL,_T("open"),csCmd,NULL,NULL,SW_SHOWNORMAL);
			cout << "启动网元 " << strDev << " 的 " << strLayer << " 层实体 " << strEntity << endl;
			Sleep(200);
			NECount++;
		}

	}

	//逐个运行程序
	cout << endl << "读出网元 " << NECount << " 个" << endl;

}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
