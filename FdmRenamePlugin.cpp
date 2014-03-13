// FdmRenamePlugin.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "./include/fdm.h"
#include <stdio.h>
#include <io.h>		// for _access
#include <time.h>	// for time

#include "ChineseConvert.h"

#define DLL_NAME		"FdmRenamePlugin.dll"
#define DEBUG_FILE		"FdmRenamePluginDebug"
#define MAX_TRY			10

#define FILE_EXIST(a)	(_access(a, 0) != -1)

static bool g_exit = false;
static HANDLE g_hThread = NULL;

//debug and log
static FILE* g_fp = NULL;

void KillThread();

void DebugCheck()
{
	char dllDir[MAX_PATH] = {0};	
	GetModuleFileName(GetModuleHandle(DLL_NAME), dllDir, MAX_PATH);
	char* ptr = strrchr(dllDir, '\\');
	ptr[1] = '\0';

	string debugFilePath = string(dllDir) + DEBUG_FILE;
	if (FILE_EXIST(debugFilePath.c_str()))
	{
		string logFilePath = string(dllDir) + DLL_NAME + ".log";
		g_fp = fopen(logFilePath.c_str(), "a+");
	}
}

void Log2File(const char* format_str, ...)
{
	if (g_fp)
	{
		time_t t = time(NULL);
		struct tm *ptime = localtime(&t);
		va_list p_list;
		va_start(p_list, format_str);
		fprintf(g_fp, "[%04d%02d%02d-%02d:%02d:%02d]", 
			(1900+ptime->tm_year), (1+ptime->tm_mon), ptime->tm_mday,
            ptime->tm_hour, ptime->tm_min, ptime->tm_sec);
		vfprintf(g_fp, format_str, p_list);
		va_end(p_list);

		fflush(g_fp);
	}
}

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		DebugCheck();
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		KillThread();
		break;
	}
	return TRUE;
}

vmsFdmApi *_pFdmApi = NULL;

DWORD WINAPI _threadJob (LPVOID);

BOOL FdmApiPluginInitialize (vmsFdmApi* pFdmApi, DWORD* pdwSdkVersionPluginUse)
{
	*pdwSdkVersionPluginUse = (FDMSDK_MAJOR_VERSION << 32) | FDMSDK_MINOR_VERSION;
	if (pFdmApi)
	{
		_pFdmApi = pFdmApi;
		DWORD dw;
		g_hThread = CreateThread (NULL, 0, _threadJob, NULL, 0, &dw);
	}
	
	return TRUE;
}

void FdmApiPluginGetDescription (LPSTR pszPluginShortName, LPSTR pszPluginLongName, LPSTR pszPluginVersion, LPSTR pszPluginDescription)
{
	strcpy (pszPluginShortName, "FDMExPlug");
	strcpy (pszPluginLongName, "FDM Rename plugin");
	strcpy (pszPluginVersion, "1.0.0.3");
	strcpy (pszPluginDescription, "This plugin helps to adjust UTF-8 Chinese filename.");
}
// if `dst` file exist, src file will be rename to format like `dst (1)`  
string RenameEx(const char* src, const char* dst)
{
	string strNewFilePath = dst;
	if (FILE_EXIST(dst))
	{
		char tmpPath[MAX_PATH] = {0};
		const char* pExt = strrchr(dst, '.');
		string filename = string(dst).substr(0, pExt - dst);
		for (int i = 1; i <= MAX_TRY; i++)
		{
			sprintf(tmpPath, "%s (%d)%s", filename.c_str(), i, pExt);
			if (!FILE_EXIST(tmpPath))
			{
				strNewFilePath = tmpPath;
				break;
			}
		}
	}
	
	if (rename(src, strNewFilePath.c_str()) != 0)
	{
		strNewFilePath = "";
		Log2File("Rename '%s' -> '%s' failure, Error: %s\n", src, strNewFilePath.c_str(), strerror(errno));
	}
	else
	{
		Log2File("Rename '%s' -> '%s' success.\n", src, strNewFilePath.c_str());
	}

	return strNewFilePath;//return real file name
}

// UTF-8 To GB2312
void RenameFile()
{
	UINT n = _pFdmApi->getFdmBuildNumber();
	
	char szFilePath[1024] = {0};
	
	UINT nDownloadCount = _pFdmApi->getDownloadsMgr()->getDownloadCount();
		
	for (UINT i = 0; i < nDownloadCount; i++)
	{
		vmsFdmApiDownload *dld = _pFdmApi->getDownloadsMgr()->getDownload(i);
		dld->getFileName(szFilePath);
		UINT nStatCode = dld->getState();
		
		bool bFileExist = FILE_EXIST(szFilePath);
		bool bIsUtf8Filename = (strchr(szFilePath, '%') != NULL || CChineseConvert::IsUTF8String(szFilePath));
		if (nStatCode == FADS_FINISHED && bFileExist && bIsUtf8Filename)
		{// Download finished and file exist and exist '%' char in filename
			string strNewFilename = CChineseConvert::Url_Utf8ToGB2312(szFilePath);
			RenameEx(szFilePath, strNewFilename.c_str());			
		}
	}
}

void KillThread()
{
	g_exit = true;
	if (NULL != g_hThread)
	{
		TerminateThread(g_hThread,0);
		CloseHandle(g_hThread);
		g_hThread = NULL;
	}
	
	if (NULL != g_fp)
	{
		fclose(g_fp);
		g_fp = NULL;
	}
}

DWORD WINAPI _threadJob (LPVOID)
{
	while (!g_exit)
	{
		RenameFile();
		Sleep(500);
	}
	
	return 0;
}
