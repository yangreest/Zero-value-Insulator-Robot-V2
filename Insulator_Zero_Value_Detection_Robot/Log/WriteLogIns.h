#pragma once

#include <thread>
#include <vector>
#include <algorithm>
#include <cstring>
using std::vector;

#include <string>
using std::string;

#include "ScanS_FC.h"

enum LogError
{
	LE_Succeed = 0, //成功
	LE_Init = 1, //初始化时参数错误
	LE_Open = 2, //打开日志文件错误
	LE_Overflow = 3 //写入的内容溢出（被截断）
};

class CLogInfo
{
public:
	string m_strMessage;
	SYSTEMTIME m_timeMsg;

public:
	CLogInfo(const char* szMessage, int nMaxMsgLen)
		: m_strMessage(szMessage, (std::min)((size_t)nMaxMsgLen, strlen(szMessage)))
	{
		GetLocalTime(&m_timeMsg);
	}
};

//按“日期 + 固定大小（默认 1 MB）”滚动的日志实现：
//  文件命名规则： <dir>\<basename>_YYYYMMDD_NNN<ext>
//  切换条件：    1) 本地日期发生变化；2) 当前分片文件大小 + 待写入字节 > MaxFileSize
//  写入模式：    纯追加，不再维护循环头，也不再按固定行宽对齐
class CWriteLogIns
{
private:
	static const int MaxRowCount_OneChunk = 256;
	static const DWORD MaxFileSize = 1024UL * 1024UL; //单个日志文件上限：1 MB
	static const int MaxSeqPerDay = 999;              //每天最多 999 个分片

private:
	string m_strDir;         //目录（含尾部分隔符），可能为空
	string m_strBaseName;    //文件基础名，不含扩展名
	string m_strExt;         //扩展名（含 '.'）
	string m_strCurDate;     //当前分片所属日期，格式 YYYYMMDD
	int    m_nCurSeq;        //当前分片序号，1 起
	string m_strCurFilePath; //当前正在写入的分片完整路径

	DWORD m_dwMaxColCount;   //单行消息最大字节数（用于截断）
	int   m_nSync;           //1:同步   0:异步

	//以下几个变量可以使外部程序的调用与内部的写操作分离
	//用于锁住 m_vecMessages_Chunk
	CCFRD_CriticalSection m_csLogMessages;
	vector<CLogInfo> m_vecMessages_Chunk;
	vector<CLogInfo> m_vecMessages_Write;
	vector<CLogInfo> m_vecMessages_Write_Sync;

private:
	//用于锁住文件的指针，这样外面的程序可以多个线程同时往同一个日志对象中写
	CCFRD_CriticalSection m_CS;

private:
	HANDLE m_hEventQuitWrite;
	HANDLE m_hEventWrite;
	std::thread* m_pThreadWrite;

public:
	//strPath：路径+文件名（如 "Log\\DeviceLog.txt"），实际写入时会替换为
	//         "Log\\DeviceLog_YYYYMMDD_NNN.txt" 形式的滚动分片文件
	//dwMaxRowCount：保留参数（历史 API 兼容），当前实现不再使用
	//dwMaxColCount：单条消息内容的最大字节数（超出会被截断）
	//nSync  1:同步   0:异步
	CWriteLogIns(
		const string& strPath,
		int dwMaxRowCount, int dwMaxColCount,
		int nSync);

	~CWriteLogIns();

	void BeginWork();

	void EndWork();

public:
	//参数 nSync 为 1 时会立即往文件中同步写入（包括之前的待写入的行）
	//参数 nSync 为 0 时，根据构造实例时的参数，进行同步或异步写入
	void Write(const char* szMessage, int nSync = 0);

private:
	void InitDirectory();

	//从传入路径中解析出目录 / 基础名 / 扩展名
	void ParsePath(const string& strPath);

	//把 SYSTEMTIME 格式化成 YYYYMMDD
	string FormatDate(const SYSTEMTIME& t) const;

	//根据日期与序号拼接分片文件完整路径
	string BuildFilePath(const string& strDate, int nSeq) const;

	//获取指定文件大小，不存在返回 0
	DWORD GetFileSizeByName(const string& strPath) const;

	//启动或跨天时用于定位今日应写入的起始分片：
	//  返回今日首个不存在、或存在但未满 1 MB 的分片序号
	int   FindInitialSeq(const string& strDate) const;

	//按当前时间/待写入字节数决定是否需要切换到新的分片
	void  RotateIfNeeded(const SYSTEMTIME& now, size_t nAppendBytes);

	UINT  Write_Multi(const vector<CLogInfo>& vecMessages);

private:
	void FuncWrite();
};
