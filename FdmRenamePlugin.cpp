// FdmRenamePlugin.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "./include/fdm.h"
#include <stdio.h>
#include <io.h>		// for _access

#include "ChineseConvert.h"

static bool g_exit = false;
static HANDLE g_hThread = NULL;

void KillThread();

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
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
	strcpy (pszPluginVersion, "1.0");
	strcpy (pszPluginDescription, "This plugin helps to adjust Chinese filename.");
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
		
		bool bFileExist = (_access(szFilePath, 0) != -1);
		bool bIsUtf8Filename = (strchr(szFilePath, '%') != NULL || CChineseConvert::IsUTF8String(szFilePath));
		if (nStatCode == FADS_FINISHED && bFileExist && bIsUtf8Filename)
		{// Download finished and file exist and exist '%' char in filename
			string strNewFilename = CChineseConvert::Url_Utf8ToGB2312(szFilePath);
			rename(szFilePath, strNewFilename.c_str());
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
