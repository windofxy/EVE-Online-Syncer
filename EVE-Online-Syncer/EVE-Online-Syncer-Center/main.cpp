#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>

int main(int argc, char* argv[])
{
	HANDLE hProcessSnapShot = INVALID_HANDLE_VALUE;
	wchar_t processName[] = L"exefile.exe";
	PROCESSENTRY32 pe32 = {0};
	wchar_t coreDllPathRelate[] = L".\\EVE-Online-Syncer-Core.dll";
	wchar_t coreDllPathAbsolute[MAX_PATH] = { 0 };

	GetFullPathName(coreDllPathRelate, MAX_PATH, coreDllPathAbsolute, NULL);

	hProcessSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (hProcessSnapShot == INVALID_HANDLE_VALUE) return 0;

	pe32.dwSize = sizeof(PROCESSENTRY32);

	DWORD processId[10] = { 0 };
	int index = 0;
	if (Process32First(hProcessSnapShot, &pe32))
	{
		do {
			if (!wcscmp(processName, pe32.szExeFile)) { processId[index] = pe32.th32ProcessID; ++index; continue; }
		} while (Process32Next(hProcessSnapShot, &pe32));
	}
	CloseHandle(hProcessSnapShot);

	DWORD64 filePathSize = sizeof(char) + lstrlen(coreDllPathAbsolute) * sizeof(wchar_t);

	HMODULE hKernel32 = GetModuleHandleW(TEXT("kernel32.dll"));
	if (hKernel32 == NULL)
	{
		MessageBox(NULL, TEXT("GetModuleHandleW failed!"), TEXT(""), MB_OK);
		return 0;
	}

	FARPROC pFunc_LoadLibraryW = GetProcAddress(hKernel32, "LoadLibraryW");
	if (pFunc_LoadLibraryW == NULL)
	{
		MessageBox(NULL, TEXT("CreateRemoteThread failed!"), TEXT(""), MB_OK);
		return 0;
	}

	for (int i = 0; i < index; i++)
	{
		if (processId[i] != 0)
		{
			HANDLE eveHandle = OpenProcess(PROCESS_ALL_ACCESS, NULL, processId[i]);
			if (eveHandle == NULL) MessageBox(NULL, TEXT("OpenProcess failed!"), NULL, MB_OK);
			LPVOID pCoreDllPathAbsolute = VirtualAllocEx(eveHandle, NULL, filePathSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
			if (pCoreDllPathAbsolute == NULL) return 0;
			if (WriteProcessMemory(eveHandle, pCoreDllPathAbsolute, coreDllPathAbsolute, filePathSize, NULL) == FALSE)
			{
				MessageBox(NULL, TEXT("WriteProcessMemory failed!"), TEXT(""), MB_OK);
				return 0;
			}
			wchar_t coreDllPathAbsolute_write[MAX_PATH] = { 0 };
			if (ReadProcessMemory(eveHandle, pCoreDllPathAbsolute, coreDllPathAbsolute_write, filePathSize, NULL) == FALSE)
			{
				MessageBox(NULL, TEXT("WriteProcessMemory failed!"), TEXT(""), MB_OK);
				return 0;
			}
			HANDLE hRemoteThread = CreateRemoteThread(eveHandle, NULL, NULL, (LPTHREAD_START_ROUTINE)pFunc_LoadLibraryW, pCoreDllPathAbsolute, NULL, NULL);
			if (hRemoteThread == INVALID_HANDLE_VALUE)
			{
				MessageBox(NULL, TEXT("CreateRemoteThread failed!"), TEXT(""), MB_OK);
				return 0;
			}

			CloseHandle(eveHandle);

			std::cout << "PID: " << processId[i] << " injected!" << std::endl;
		}
	}

	return 0;
}