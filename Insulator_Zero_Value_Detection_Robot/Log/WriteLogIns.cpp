#include <io.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "WriteLogIns.h"

//CWriteLogIns
///////////////////////////////////////////////////////////////////////////////////////////////////
CWriteLogIns::CWriteLogIns(
	const string& strPath,
	int dwMaxRowCount, int dwMaxColCount,
	int nSync)
	: m_nCurSeq(1), m_dwMaxColCount((DWORD)dwMaxColCount), m_nSync(nSync)
{
	//dwMaxRowCount 为历史 API 保留参数，当前滚动实现按大小+日期分片，不再依赖行数
	(void)dwMaxRowCount;

	//单条消息至少留出 64 字节，最大不超过 8 KB，避免异常入参
	if (m_dwMaxColCount < 64) m_dwMaxColCount = 64;
	if (m_dwMaxColCount > 8192) m_dwMaxColCount = 8192;

	ParsePath(strPath);
	InitDirectory();

	//
	m_pThreadWrite = NULL;
	m_hEventWrite = NULL;
	m_hEventQuitWrite = NULL;

	//定位当前应写入的分片文件
	SYSTEMTIME now;
	GetLocalTime(&now);
	m_strCurDate = FormatDate(now);
	m_nCurSeq = FindInitialSeq(m_strCurDate);
	m_strCurFilePath = BuildFilePath(m_strCurDate, m_nCurSeq);
}

CWriteLogIns::~CWriteLogIns()
{
}

void CWriteLogIns::BeginWork()
{
	//1.
	m_hEventQuitWrite = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hEventWrite = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_pThreadWrite = new std::thread(&CWriteLogIns::FuncWrite, this);
}

void CWriteLogIns::EndWork()
{
	if (m_hEventQuitWrite == NULL || m_pThreadWrite == NULL)
	{
		return;
	}

	//1.通知写线程退出，并把尚未落盘的消息 flush 一次
	SetEvent(m_hEventQuitWrite);
	m_pThreadWrite->join();

	CloseHandle(m_hEventQuitWrite);
	delete m_pThreadWrite;
	CloseHandle(m_hEventWrite);

	m_hEventQuitWrite = NULL;
	m_hEventWrite = NULL;
	m_pThreadWrite = NULL;

	//退出前把残留消息同步写入，避免丢日志
	m_csLogMessages.Lock();
	{
		m_vecMessages_Write_Sync = m_vecMessages_Chunk;
		m_vecMessages_Chunk.clear();
	}
	m_csLogMessages.UnLock();

	if (!m_vecMessages_Write_Sync.empty())
	{
		Write_Multi(m_vecMessages_Write_Sync);
		m_vecMessages_Write_Sync.clear();
	}
}

void CWriteLogIns::FuncWrite()
{
	HANDLE hWaitArray[2];

	hWaitArray[0] = m_hEventQuitWrite;
	hWaitArray[1] = m_hEventWrite;

	while (true)
	{
		DWORD rc = WaitForMultipleObjects(2, hWaitArray, FALSE, INFINITE);

		if (rc == WAIT_OBJECT_0)
		{
			break;
		}
		else if (rc == WAIT_OBJECT_0 + 1)
		{
			m_csLogMessages.Lock();
			{
				m_vecMessages_Write = m_vecMessages_Chunk;
				m_vecMessages_Chunk.clear();
			}
			m_csLogMessages.UnLock();

			Write_Multi(m_vecMessages_Write);
		}
	}
}

void CWriteLogIns::ParsePath(const string& strPath)
{
	size_t nSlash = strPath.find_last_of("\\/");
	if (nSlash == string::npos)
	{
		m_strDir = "";
		m_strBaseName = strPath;
	}
	else
	{
		m_strDir = strPath.substr(0, nSlash + 1);
		m_strBaseName = strPath.substr(nSlash + 1);
	}

	size_t nDot = m_strBaseName.find_last_of('.');
	if (nDot != string::npos && nDot > 0)
	{
		m_strExt = m_strBaseName.substr(nDot);
		m_strBaseName = m_strBaseName.substr(0, nDot);
	}
	else
	{
		m_strExt = ".log";
	}

	if (m_strBaseName.empty())
	{
		m_strBaseName = "Log";
	}
}

void CWriteLogIns::InitDirectory()
{
	if (m_strDir.empty())
	{
		return;
	}

	const char* pcPath = m_strDir.c_str();
	if ((_access(pcPath, 0)) == -1)
	{
		char FilePath[MAX_PATH] = "";

		for (int nIndex = 0; nIndex < MAX_PATH; ++nIndex)
		{
			if (pcPath[nIndex] == 0)
			{
				break;
			}

			if (pcPath[nIndex] == '\\' || pcPath[nIndex] == '/')
			{
				DWORD dwAttributesA = GetFileAttributesA(FilePath);
				if (dwAttributesA == 0xFFFFFFFF && FilePath[0] != 0) //目录不存在则创建
				{
					CreateDirectoryA(FilePath, NULL);
				}
			}
			FilePath[nIndex] = pcPath[nIndex];
			FilePath[nIndex + 1] = 0;
		}

		//最后一层目录（末尾可能没有分隔符）
		DWORD dwAttr = GetFileAttributesA(m_strDir.c_str());
		if (dwAttr == 0xFFFFFFFF)
		{
			CreateDirectoryA(m_strDir.c_str(), NULL);
		}
	}
}

string CWriteLogIns::FormatDate(const SYSTEMTIME& t) const
{
	char buf[16] = "";
	sprintf_s(buf, sizeof(buf), "%04d%02d%02d", t.wYear, t.wMonth, t.wDay);
	return string(buf);
}

string CWriteLogIns::BuildFilePath(const string& strDate, int nSeq) const
{
	std::ostringstream oss;
	oss << m_strDir << m_strBaseName << "_" << strDate << "_"
		<< std::setw(3) << std::setfill('0') << nSeq << m_strExt;
	return oss.str();
}

DWORD CWriteLogIns::GetFileSizeByName(const string& strPath) const
{
	WIN32_FILE_ATTRIBUTE_DATA fad;
	memset(&fad, 0, sizeof(fad));
	if (!GetFileAttributesExA(strPath.c_str(), GetFileExInfoStandard, &fad))
	{
		return 0;
	}
	return fad.nFileSizeLow;
}

int CWriteLogIns::FindInitialSeq(const string& strDate) const
{
	for (int seq = 1; seq <= MaxSeqPerDay; ++seq)
	{
		string path = BuildFilePath(strDate, seq);
		if ((_access(path.c_str(), 0)) != 0)
		{
			//文件不存在，直接从该序号开始新建
			return seq;
		}

		//文件已存在，若仍未写满则继续追加
		DWORD sz = GetFileSizeByName(path);
		if (sz < MaxFileSize)
		{
			return seq;
		}
	}
	return MaxSeqPerDay;
}

void CWriteLogIns::RotateIfNeeded(const SYSTEMTIME& now, size_t nAppendBytes)
{
	string strDate = FormatDate(now);
	if (strDate != m_strCurDate)
	{
		//跨天：切到新一天的首个可用分片
		m_strCurDate = strDate;
		m_nCurSeq = FindInitialSeq(strDate);
		m_strCurFilePath = BuildFilePath(m_strCurDate, m_nCurSeq);
		return;
	}

	DWORD sz = GetFileSizeByName(m_strCurFilePath);
	if (sz > 0 && ((ULONGLONG)sz + (ULONGLONG)nAppendBytes) > (ULONGLONG)MaxFileSize)
	{
		//当前分片再写这批就会超过 1 MB，切到下一个分片
		if (m_nCurSeq < MaxSeqPerDay)
		{
			++m_nCurSeq;
		}
		m_strCurFilePath = BuildFilePath(m_strCurDate, m_nCurSeq);
	}
}

void CWriteLogIns::Write(const char* szMessage, int nSync/* = 0*/)
{
	if (szMessage == NULL)
	{
		return;
	}

	CLogInfo logInfo(szMessage, (int)m_dwMaxColCount);

	m_csLogMessages.Lock();
	{
		if (m_vecMessages_Chunk.size() > (size_t)(MaxRowCount_OneChunk))
		{
			m_vecMessages_Chunk.clear();
		}

		m_vecMessages_Chunk.push_back(logInfo);
	}
	m_csLogMessages.UnLock();

	//3.
	if (m_nSync == 1 || nSync == 1)
	{
		m_csLogMessages.Lock();
		{
			m_vecMessages_Write_Sync = m_vecMessages_Chunk;
			m_vecMessages_Chunk.clear();
		}
		m_csLogMessages.UnLock();

		Write_Multi(m_vecMessages_Write_Sync);
	}
	else
	{
		if (m_hEventWrite != NULL)
		{
			SetEvent(m_hEventWrite);
		}
	}
}

UINT CWriteLogIns::Write_Multi(const vector<CLogInfo>& vecMessages)
{
	if (vecMessages.empty())
	{
		return LE_Succeed;
	}

	UINT uiFlg = LE_Succeed;

	m_CS.Lock();
	do
	{
		//1.先把本批消息拼成一个完整缓冲区，方便一次性追加写入
		std::string strBuf;
		strBuf.reserve(vecMessages.size() * 96);

		for (size_t i = 0; i < vecMessages.size(); ++i)
		{
			const CLogInfo& logInfo = vecMessages[i];

			string strMsg = logInfo.m_strMessage;
			if ((DWORD)strMsg.length() > m_dwMaxColCount)
			{
				strMsg = strMsg.substr(0, m_dwMaxColCount);
				uiFlg = LE_Overflow;
			}

			char head[64] = "";
			sprintf_s(head, sizeof(head), "[%04d-%02d-%02d %02d:%02d:%02d:%03d] ",
				logInfo.m_timeMsg.wYear, logInfo.m_timeMsg.wMonth, logInfo.m_timeMsg.wDay,
				logInfo.m_timeMsg.wHour, logInfo.m_timeMsg.wMinute, logInfo.m_timeMsg.wSecond,
				logInfo.m_timeMsg.wMilliseconds);

			strBuf.append(head);
			strBuf.append(strMsg);
			strBuf.append("\r\n");
		}

		//2.按最新一条消息的时间决定是否切换分片
		RotateIfNeeded(vecMessages.back().m_timeMsg, strBuf.size());

		//3.以追加二进制方式打开当前分片文件
		FILE* hFile = NULL;
		errno_t err = fopen_s(&hFile, m_strCurFilePath.c_str(), "ab");
		if (err != 0 || hFile == NULL)
		{
			uiFlg = LE_Open;
			break;
		}

		fwrite(strBuf.data(), 1, strBuf.size(), hFile);
		fflush(hFile);
		fclose(hFile);
	} while (0);
	m_CS.UnLock();

	return uiFlg;
}
