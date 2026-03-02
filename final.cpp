#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <vector>
#include <tlhelp32.h>
#include <mmsystem.h>
#include <powrprof.h>
#include <winternl.h>
#include <thread>

struct MemoryFlag {
    const char* name;
    uintptr_t offset;
    int value;
};

// Adicionado hBtnDll
HWND hBtnDll, hBtn1, hBtn2, hStatus;

void AplicarPrioridadePersistente() {
    HKEY hKey;
    const wchar_t* path = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\RobloxPlayerBeta.exe\\PerfOptions";
    LSTATUS status = RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);

    if (status == ERROR_SUCCESS) {
        DWORD priority = 3; 
        RegSetValueExW(hKey, L"CpuPriorityClass", 0, REG_DWORD, (const BYTE*)&priority, sizeof(priority));
        RegCloseKey(hKey);
    }
}

uintptr_t GetModuleBaseAddress(DWORD procId, const wchar_t* modName) {
    uintptr_t modBaseAddr = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
    if (hSnap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W modEntry;
        modEntry.dwSize = sizeof(modEntry);
        if (Module32FirstW(hSnap, &modEntry)) {
            do {
                if (!_wcsicmp(modEntry.szModule, modName)) {
                    modBaseAddr = (uintptr_t)modEntry.modBaseAddr;
                    break;
                }
            } while (Module32NextW(hSnap, &modEntry));
        }
    }
    CloseHandle(hSnap);
    return modBaseAddr;
}

bool InjetarDLL(HANDLE hProc, const char* dllNome) {
    char caminhoCompleto[MAX_PATH];
    if (GetFullPathNameA(dllNome, MAX_PATH, caminhoCompleto, NULL) == 0) {
        return false;
    }

    LPVOID pDllPath = VirtualAllocEx(hProc, 0, strlen(caminhoCompleto) + 1, MEM_COMMIT, PAGE_READWRITE);
    if (!pDllPath) return false;

    WriteProcessMemory(hProc, pDllPath, (LPVOID)caminhoCompleto, strlen(caminhoCompleto) + 1, 0);

    HMODULE hKernel32 = GetModuleHandleA("Kernel32.dll");
    LPVOID pLoadLibrary = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryA");

    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pDllPath, 0, NULL);
    if (!hThread) {
        VirtualFreeEx(hProc, pDllPath, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, pDllPath, 0, MEM_RELEASE);
    
    return true;
}

// Nova função apenas para o botão de injeção
void ExecutarInjecao() {
    SetWindowTextA(hStatus, "Status: Aguardando Roblox para Injetar DLL...");
    
    DWORD procId = 0;
    while (!procId) {
        HWND hwnd = FindWindowA(NULL, "Roblox");
        if (hwnd) GetWindowThreadProcessId(hwnd, &procId);
        Sleep(1000);
    }

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId);
    if (hProc) {
        if (InjetarDLL(hProc, "memalloc.dll")) {
            SetWindowTextA(hStatus, "Status: memalloc.dll Injetada com Sucesso!");
        } else {
            SetWindowTextA(hStatus, "Status: Falha ao injetar memalloc.dll.");
        }
        CloseHandle(hProc);
    } else {
        SetWindowTextA(hStatus, "Status: ERRO! Execute como Administrador.");
    }
}

void ExecutarOtimizador(int choice) {
    SetWindowTextA(hStatus, "Status: Aguardando RobloxPlayerBeta.exe...");

    std::vector<MemoryFlag> flags = {
{"TaskSchedulerTargetFps", 0x76A8730, 9999},

        {"RenderShadowmapBias", 0x695A144, -1},

       // {"DebugGraphicsDisableDirect3D11", 0x77BA708, 1},

        //{"DebugGraphicsPreferVulkan", 0x76CC9E0, 1},

        {"FastGPULightCulling3", 0x76E9700, 1},

        //{"DebugSkyGray", 0x76E9448, 1},

        {"DebugForceMSAASamples", 0x766F404, 1},

        {"FRMMaxGrassDistance", 0x6965C3C, 0},

        {"DebugFRMQualityLevelOverride", 0x767198C, 4},

        {"HandleAltEnterFullscreenManually", 0x77B89B8, 1},

        {"RakNetLoopMs", 0x695E16C, 1},

        {"RakNetPingFrequencyMillisecond", 0x695F328, 10},

        {"ClientPacketMaxDelayMs", 0x6959C90, 1},

        {"ClientPacketMaxFrameMicroseconds", 0x6959C98, 1},

        {"ClientPacketHealthyMsPerSecondLimit", 0x6959CA4, 1},

        {"MaxProcessPacketsJobScaling", 0x6959CC0, 2139999999},

        {"MaxProcessPacketsStepsAccumulated", 0x6959CC4, 0},

        {"MaxProcessPacketsStepsPerCyclic", 0x6959CC8, 2139999999},

        {"S2PhysicsSenderRate", 0x697176C, 1},

        {"WorldStepMax", 0x695DAAC, 1},

        {"MaxMissedWorldStepsRemembered", 0x695DAB4, 1},

        {"RakNetResendBufferArrayLength", 0x695F808, 128},

        {"RakNetResendMaxThresholdTimeInUs", 0x695F838, 10000},

        {"RakNetResendMinThresholdTimeInUs", 0x695F830, 5000},

        {"RakNetResendRttMultiple", 0x695F834, 1},

        {"RakNetResendScaleBackFactorHundredthsPercent", 0x7671418, 50},

        //{"DebugDeterministicParticles", 0x76E8780, 1},

        {"InterpolationMaxDelayMSec", 0x695DAF0, 1},

        {"InterpolationAwareTargetTimeLerpHundredth", 0x695DAF8, 100},

        {"InterpolationDtLimitForLod", 0x695DB70, 1}, //1

        {"DebugSimPrimalStiffnessMax", 0x69662DC, 0},

        //se isso fuder tudo tira:
        {"BloomFrmCutoff", 0x6959F9C, -1},

        {"RobloxGuiBlurIntensity", 0x6974F88, 0},

        {"RenderLocalLightUpdatesMax", 0x695A158, 1},

        {"ClientPacketExcessMicroseconds", 0x6959C94, 1},

        {"DiffractionSolverUsesGraphicsQualityLevel", 0x77839D8, 3},

        {"GameNetPVHeaderRotationOrientIdToleranceExponent", 0x6959D14, -1},

        {"RenderLocalLightUpdatesMin", 0x695A154, 1},

        {"RenderLocalLightFadeInMs", 0x695A150, 0},

        {"RenderReportGfxQualityLevel", 0x76E5180, 1},

        //{"TextureCompositorActiveJobs", 0x6959FB0, 0},


        //aq

        {"SimBlockLargeLocalToolWeldManipulationsThreshold", 0x6959C8C, -1}
    };

    DWORD procId = 0;
    while (!procId) {
        HWND hwnd = FindWindowA(NULL, "Roblox");
        if (hwnd) GetWindowThreadProcessId(hwnd, &procId);
        Sleep(1000);
    }

    uintptr_t baseAddr = GetModuleBaseAddress(procId, L"RobloxPlayerBeta.exe");
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId);

    if (hProc && baseAddr) {
        SetWindowTextA(hStatus, "Status: Otimizando Processo...");

        // tirei o bglh coloca dnv se der merda

        SetPriorityClass(hProc, HIGH_PRIORITY_CLASS);

        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        DWORD_PTR affinity = (1ULL << sysInfo.dwNumberOfProcessors) - 1;
        SetProcessAffinityMask(hProc, affinity);

        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\\Tasks\\Games", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, L"Priority", 0, REG_SZ, (BYTE*)L"6", 4);
            RegSetValueExW(hKey, L"Scheduling Category", 0, REG_SZ, (BYTE*)L"High", 8);
            RegSetValueExW(hKey, L"SFIO Priority", 0, REG_SZ, (BYTE*)L"High", 8);
            RegCloseKey(hKey);
        }

        typedef NTSTATUS (NTAPI *pNtSetInformationProcess)(HANDLE, ULONG, PVOID, ULONG);
        auto NtSet = (pNtSetInformationProcess)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtSetInformationProcess");
        
        ULONG quantum = 0xFF;
        NtSet(hProc, 29, &quantum, sizeof(quantum));

        ULONG disableParking = 1;
        NtSet(hProc, 0x1D, &disableParking, sizeof(disableParking));

        SIZE_T targetRAM = 5ULL * 1024 * 1024 * 1024; 
        SIZE_T chunkSize = 512 * 1024 * 1024;
        int numChunks = targetRAM / chunkSize;

        for (int i = 0; i < numChunks; i++) {
            LPVOID remoteMem = VirtualAllocEx(hProc, NULL, chunkSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (remoteMem) {
                char dummy = 1;
                WriteProcessMemory(hProc, remoteMem, &dummy, 1, NULL);
            }
        }

        if (choice == 1) {
            SetWindowTextA(hStatus, "Status: Dump injetado. Monitorando...");
            while (true) {
                DWORD exitCode;
                GetExitCodeProcess(hProc, &exitCode);
                if (exitCode != STILL_ACTIVE) break;

                for (const auto& flag : flags) {
                    uintptr_t target = baseAddr + flag.offset;
                    DWORD oldProtect;

                    if (VirtualProtectEx(hProc, (LPVOID)target, sizeof(int), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                        WriteProcessMemory(hProc, (LPVOID)target, &flag.value, sizeof(int), NULL);
                        VirtualProtectEx(hProc, (LPVOID)target, sizeof(int), oldProtect, &oldProtect);
                    }
                }
                Sleep(20000);
            }
            SetWindowTextA(hStatus, "Status: Jogo Fechado.");
        } else {
            SetWindowTextA(hStatus, "Status: Otimizado. Monitorando...");
            
            while (true) {
                DWORD exitCode;
                GetExitCodeProcess(hProc, &exitCode);
                if (exitCode != STILL_ACTIVE) break;
                Sleep(5000); 
            }
            SetWindowTextA(hStatus, "Status: Jogo Fechado.");
        }

        CloseHandle(hProc);
    } else {
        SetWindowTextA(hStatus, "Status: ERRO! Execute como Administrador.");
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            // .
            hBtnDll = CreateWindowA("BUTTON", "Injetar memalloc.dll",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                20, 20, 250, 40, hwnd, (HMENU)3, NULL, NULL);

            // 2
            hBtn1 = CreateWindowA("BUTTON", "1 - Otimizar e Colocar Bandeiras",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                20, 70, 250, 40, hwnd, (HMENU)1, NULL, NULL);
            
            hBtn2 = CreateWindowA("BUTTON", "2 - Apenas Otimizar",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                20, 120, 250, 40, hwnd, (HMENU)2, NULL, NULL);

            hStatus = CreateWindowA("STATIC", "Status: Aguardando escolha...",
                WS_VISIBLE | WS_CHILD,
                20, 175, 250, 40, hwnd, NULL, NULL, NULL);
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == 3) {
                // 3
                std::thread(ExecutarInjecao).detach();
            } 
            else if (LOWORD(wParam) == 1 || LOWORD(wParam) == 2) {
                // v
                EnableWindow(hBtn1, FALSE);
                EnableWindow(hBtn2, FALSE);
                std::thread(ExecutarOtimizador, LOWORD(wParam)).detach();
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    AplicarPrioridadePersistente();

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "RobloxOptClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);

    RegisterClassA(&wc);

    // so altura
    HWND hwnd = CreateWindowA("RobloxOptClass", "Roblox Optimizer Engine",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, 
        CW_USEDEFAULT, CW_USEDEFAULT, 310, 270,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

//cmp: g++ -o app.exe final.cpp -static -lkernel32 -luser32 -lwinmm -mwindows
