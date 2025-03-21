// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <Python.h>
#include <Windows.h>
#include <Shlwapi.h>
#include <thread>
#include <string>
#include <codecvt>
#include "resource.h"

#pragma comment(lib, "shlwapi.lib")

typedef void(*_Py_Initialize_)(void);
typedef int(*_Py_IsInitialized_)(void);
typedef void(*_PyEval_InitThreads_)(void);
typedef PyGILState_STATE(*_PyGILState_Ensure_)(void);
typedef void(*_PyGILState_Release_)(PyGILState_STATE);
typedef int(*_PyRun_SimpleStringFlags_)(const char*, PyCompilerFlags*);
typedef int(*_PyRun_SimpleFileEx_)(FILE*, const char*, int);
typedef PyObject* (*_PyImport_ImportModule_)(const char*);
typedef PyObject* (*_Py_BuildValue_)(const char*, ...);
typedef FILE* (*_PyFile_AsFile_)(PyObject*);
typedef PyObject* (*_PyFile_FromString_)(char*, char*);

_Py_Initialize_ pFunc_Py_Initialize;
_Py_IsInitialized_ pFunc_Py_IsInitialized;
_PyEval_InitThreads_ pFunc_PyEval_InitThreads;
_PyGILState_Ensure_ pFunc_PyGILState_Ensure;
_PyGILState_Release_ pFunc_PyGILState_Release;
_PyRun_SimpleStringFlags_ pFunc_PyRun_SimpleStringFlags;
_PyRun_SimpleFileEx_ pFunc_PyRun_SimpleFileEx;
_PyImport_ImportModule_ pFunc_PyImport_ImportModule;
_Py_BuildValue_ pFunc_Py_BuildValue;
_PyFile_AsFile_ pFunc_PyFile_AsFile;
_PyFile_FromString_ pFunc_PyFile_FromString;

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

DWORD currentProcessId = 0;
HWND currentGameWindow = NULL;

HMODULE coreDllHandle;
HMODULE pythonDllHandle;

typedef struct
{
    HWND hwndWindow; // 窗口句柄
    DWORD dwProcessID; // 进程ID
} EnumWindowsArg;

void WaitForPythonLoaded()
{
    while (true)
    {
        pythonDllHandle = GetModuleHandle(L"python27.dll");
        if (pythonDllHandle == NULL) { Sleep(1000); continue; }
        pFunc_Py_IsInitialized = (_Py_IsInitialized_)GetProcAddress(pythonDllHandle, "Py_IsInitialized");
        pFunc_PyEval_InitThreads = (_PyEval_InitThreads_)GetProcAddress(pythonDllHandle, "PyEval_InitThreads");
        pFunc_PyGILState_Ensure = (_PyGILState_Ensure_)GetProcAddress(pythonDllHandle, "PyGILState_Ensure");
        pFunc_PyGILState_Release = (_PyGILState_Release_)GetProcAddress(pythonDllHandle, "PyGILState_Release");
        pFunc_PyRun_SimpleStringFlags = (_PyRun_SimpleStringFlags_)GetProcAddress(pythonDllHandle, "PyRun_SimpleStringFlags");
        pFunc_PyRun_SimpleFileEx = (_PyRun_SimpleFileEx_)GetProcAddress(pythonDllHandle, "PyRun_SimpleFileEx");
        pFunc_PyImport_ImportModule = (_PyImport_ImportModule_)GetProcAddress(pythonDllHandle, "PyImport_ImportModule");
        pFunc_Py_BuildValue = (_Py_BuildValue_)GetProcAddress(pythonDllHandle, "Py_BuildValue");
        pFunc_PyFile_AsFile = (_PyFile_AsFile_)GetProcAddress(pythonDllHandle, "PyFile_AsFile");
        pFunc_PyFile_FromString = (_PyFile_FromString_)GetProcAddress(pythonDllHandle, "PyFile_FromString");
        break;
    }
}

std::string pythonScriptContent;

bool LoadPythonScript()
{
    HRSRC hResInfo;
    HGLOBAL hResData;
    DWORD dwSize;

    hResInfo = FindResource(coreDllHandle, MAKEINTRESOURCE(IDR_PYTHONSCRIPT1), TEXT("PythonScript"));
    if (hResInfo == NULL) return false;

    dwSize = SizeofResource(coreDllHandle, hResInfo);
    hResData = LoadResource(coreDllHandle, hResInfo);
    if (hResData == NULL) return false;

    const char* pResData = (const char*)LockResource(hResData);
    pythonScriptContent = std::string(pResData);

    UnlockResource(hResData);

    return true;
}

wchar_t coreFilePath[MAX_PATH];
wchar_t coreFilePathWithoutName[MAX_PATH];
bool volatile core_Inited = false;
bool volatile core_exit = false;

void Syncer_Core_Work()
{
    WaitForPythonLoaded();

    bool loadSucceed = LoadPythonScript();

    if (!loadSucceed) return;

    if (!core_Inited)
    {
        core_Inited = true;

        if (pFunc_Py_IsInitialized() == FALSE) pFunc_Py_Initialize();

        pFunc_PyEval_InitThreads();
        PyGILState_STATE state = pFunc_PyGILState_Ensure();

        bool succeed = false;

        int returnCode = pFunc_PyRun_SimpleStringFlags(pythonScriptContent.c_str(), NULL);
        if (0 == returnCode) succeed = true;
        
        pFunc_PyGILState_Release(state);

        wchar_t message[256] = { 0 };
        lstrcpy(message, TEXT("程序运行错误，错误码："));
        wchar_t buf[128];
        _itow(returnCode, buf, 10);
        lstrcat(message, buf);
        if(succeed) MessageBox(currentGameWindow, TEXT("Patch succeed!"), NULL, MB_OK);
        else MessageBox(currentGameWindow, message, NULL, MB_OK);

        FreeLibraryAndExitThread(coreDllHandle, 0);
    }

    while (true)
    {
        if (core_exit) break;
        Sleep(1000);
    }
}

std::thread syncerCoreWorkThread;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        coreDllHandle = hModule;
        currentProcessId = GetCurrentProcessId();
        if(!GetModuleFileNameW(hModule, coreFilePath, MAX_PATH)) break;
        lstrcpy(coreFilePathWithoutName, coreFilePath);
        PathRemoveFileSpec(coreFilePathWithoutName);

        EnumWindowsArg args;
        args.dwProcessID = currentProcessId;
        args.hwndWindow = NULL;

        EnumWindows(EnumWindowsProc, (LPARAM)&args);
        if (!args.hwndWindow) break;

        currentGameWindow = args.hwndWindow;

        syncerCoreWorkThread = std::thread(Syncer_Core_Work);
        
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        core_exit = true;
        syncerCoreWorkThread.join();
        break;
    }
    return TRUE;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    EnumWindowsArg* pArg = (EnumWindowsArg*)lParam;
    DWORD dwProcessID = 0;
    // 通过窗口句柄取得进程ID
    ::GetWindowThreadProcessId(hwnd, &dwProcessID);
    if (dwProcessID == pArg->dwProcessID)
    {
        pArg->hwndWindow = hwnd;
        // 找到了返回FALSE
        return FALSE;
    }
    // 没找到，继续找，返回TRUE
    return TRUE;
}

