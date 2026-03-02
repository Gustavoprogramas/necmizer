#include <windows.h>
#include <vector>
#include <process.h>
#include <mmsystem.h> // p timeBeginPeriod

#pragma comment(lib, "winmm.lib")


void __cdecl ForcePowerState(void* pArgs) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    while (true) {
        volatile int anchor = 0;
        for(int i=0; i<100; i++) anchor++;
        Sleep(1); 
    }
}

DWORD WINAPI HardLockRAM(LPVOID lpParam) {
    SIZE_T totalTarget = 8ULL * 1024 * 1024 * 1024;
    std::vector<LPVOID> blocks;

    while ((blocks.size() * 256 * 1024 * 1024) < totalTarget) {
        LPVOID mem = VirtualAlloc(NULL, 256 * 1024 * 1024, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (mem) {
            FillMemory(mem, 256 * 1024 * 1024, 0x01);
            // m
            VirtualLock(mem, 256 * 1024 * 1024); 
            blocks.push_back(mem);
        } else break;
    }

    while (true) {
        for (LPVOID block : blocks) {
            // acesso super agressivo toca em cada página
            for (SIZE_T offset = 0; offset < 256 * 1024 * 1024; offset += 4096) {
                volatile char* ptr = (char*)block + offset;
                *ptr = *ptr ^ 0xFF; //inverte o bit e volta para manter o cpu ocupado
                *ptr = *ptr ^ 0xFF;
            }
        }
        Sleep(1000); 
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        
        // Otimiza o tyimer do sistema 
        timeBeginPeriod(1);

        // Prioridade maxj
        SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
        
        // Threads de performance
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        for (unsigned int i = 0; i < sysInfo.dwNumberOfProcessors; i++) {
            _beginthread(ForcePowerState, 0, NULL);
        }

        CreateThread(NULL, 0, HardLockRAM, NULL, 0, NULL);
        Beep(1500, 100);
    }
    return TRUE;
}

//g++ -shared -o memalloc.dll dllmain.cpp -static -lkernel32 -luser32 -lwinmm