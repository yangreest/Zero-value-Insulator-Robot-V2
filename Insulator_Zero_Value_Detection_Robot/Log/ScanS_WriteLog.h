#pragma once

#include <string>

class CWriteLog
{
public:
	//创建一个对象后，可以在多个线程中通过此对象写日志
	//日志文件按“日期 + 固定大小（1 MB）”滚动分片：
	//  实际生成的文件名形如 <basename>_YYYYMMDD_NNN<ext>，例如：
	//      Log\DeviceLog_20261015_001.txt
	//      Log\DeviceLog_20261015_002.txt   // 前一分片写满 1 MB 后自动新建
	//      Log\DeviceLog_20261016_001.txt   // 跨天时自动新建

	//strPath：路径+文件名（作为分片文件的命名模板，如 "Log\\DeviceLog.txt"）
	//dwMaxRowCount：保留参数（历史 API 兼容），当前实现不再按行数循环覆盖
	//dwMaxColCount：单条消息内容的最大字节数（超出会被截断），建议 64 ~ 8192
	//nSync  1:同步   0:异步
	CWriteLog(
		const std::string& strPath, int dwMaxRowCount, int dwMaxColCount,
		int nSync = 0);

	~CWriteLog();

	//构造函数之后调用
	void BeginWork();

	//析构函数之前调用
	void EndWork();

	//参数nSync为1时会立即往文件中同步写入（包括之前的待写入的行）
	//参数nSync为0时，根据构造实例时的参数，进行同步或异步写入
	//szMessage必须是'\0'结尾
	void Write(const std::string& strMessage);

	void Write_Sync(const std::string& s);

	void WriteFormat(const char* fmt, ...);

	void WriteFormat_Sync(const char* fmt, ...);

private:
	void* m_pLog;
};
