#include <windows.h>
#include "TlHelp32.h"
#include <string>
#include <vector>
#include <commdlg.h>
#include "resource.h"

HINSTANCE g_hInst;
HWND g_hComboBox, g_hDllPathEdit, g_hInjectButton, g_hStatusLabel;

bool InjectDLL(DWORD processId, const wchar_t* dllPath)
{
    HANDLE procHandle;
    LPVOID RemoteString;
    LPVOID LoadLibAddy;

    procHandle = OpenProcess(PROCESS_ALL_ACCESS, false, processId);
    if (!procHandle) return false;

    LoadLibAddy = GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW");
    RemoteString = VirtualAllocEx(procHandle, 0, (wcslen(dllPath) + 1) * sizeof(wchar_t), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(procHandle, RemoteString, dllPath, (wcslen(dllPath) + 1) * sizeof(wchar_t), 0);
    CreateRemoteThread(procHandle, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibAddy, RemoteString, 0, 0);
    CloseHandle(procHandle);

    return true;
}

std::vector<std::pair<DWORD, std::wstring>> GetHighPriorityProcesses() {
    std::vector<std::pair<DWORD, std::wstring>> highPriorityProcesses;

    HANDLE snapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapShot == INVALID_HANDLE_VALUE) {
        return highPriorityProcesses;
    }

    PROCESSENTRY32 processInfo;
    processInfo.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapShot, &processInfo)) {
        do {
            DWORD processID = processInfo.th32ProcessID;

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processID);
            if (hProcess) {
                DWORD priority = GetPriorityClass(hProcess);

                if (priority == NORMAL_PRIORITY_CLASS) {
                    highPriorityProcesses.emplace_back(processID, std::wstring(processInfo.szExeFile));
                }

                CloseHandle(hProcess);
            }

        } while (Process32Next(snapShot, &processInfo));
    }

    CloseHandle(snapShot);

    return highPriorityProcesses;
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static std::vector<std::pair<DWORD, std::wstring>> processList;
    static std::wstring selectedDllPath;

    switch (msg) {
    case WM_CREATE: {
        g_hComboBox = CreateWindow(L"COMBOBOX", nullptr,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL,
            10, 10, 300, 200, hwnd, nullptr, g_hInst, nullptr);

        processList = GetHighPriorityProcesses();
        for (const auto& process : processList) {
            std::wstring wstr = std::to_wstring(process.first) + L" - " + process.second;
            SendMessage(g_hComboBox, CB_ADDSTRING, 0, (LPARAM)wstr.c_str());
        }

        g_hDllPathEdit = CreateWindow(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            10, 50, 300, 25, hwnd, nullptr, g_hInst, nullptr);

        CreateWindow(L"BUTTON", L"Select DLL",
            WS_CHILD | WS_VISIBLE,
            320, 50, 80, 25, hwnd, (HMENU)1, g_hInst, nullptr);

        g_hInjectButton = CreateWindow(L"BUTTON", L"Inject",
            WS_CHILD | WS_VISIBLE,
            10, 90, 100, 30, hwnd, (HMENU)2, g_hInst, nullptr);

        g_hStatusLabel = CreateWindow(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE,
            10, 130, 400, 250, hwnd, nullptr, g_hInst, nullptr);
        break;
    }
    case WM_COMMAND:
        if (HIWORD(wParam) == CBN_DROPDOWN) {
            SendMessage(g_hComboBox, CB_RESETCONTENT, 0, 0);

            processList = GetHighPriorityProcesses();
            for (const auto& process : processList) {
                std::wstring wstr = std::to_wstring(process.first) + L" - " + process.second;
                SendMessage(g_hComboBox, CB_ADDSTRING, 0, (LPARAM)wstr.c_str());
            }
        }

        if (LOWORD(wParam) == 1) {
            OPENFILENAME ofn = { 0 };
            wchar_t filePath[MAX_PATH] = L"";
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = filePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"DLL Files\0*.dll\0";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn)) {
                selectedDllPath = std::wstring(filePath, filePath + wcslen(filePath));
                SetWindowText(g_hDllPathEdit, filePath);
            }
        }
        else if (LOWORD(wParam) == 2) {
            LRESULT selIndex = SendMessage(g_hComboBox, CB_GETCURSEL, 0, 0);
            if (selIndex == CB_ERR || selectedDllPath.empty()) {
                SetWindowText(g_hStatusLabel, L"Error: Select a process and DLL.");
                return 0;
            }

            DWORD processId = processList[static_cast<int>(selIndex)].first;
            if (InjectDLL(processId, selectedDllPath.c_str())) {
                SetWindowText(g_hStatusLabel, L"Injection successful.");
            }
            else {
                SetWindowText(g_hStatusLabel, L"Injection failed.");
            }
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0, 0, GetModuleHandle(nullptr),
        LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_ICON1)), nullptr, nullptr, nullptr, L"DLL Injector",
        LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_ICON1)) };
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindow(L"DLL Injector", L"DLL Injector",
        WS_OVERLAPPEDWINDOW, 100, 100, 450, 450, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterClass(L"DLL Injector", hInstance);
    return 0;
}
