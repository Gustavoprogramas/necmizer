#include <windows.h>
#include <stdlib.h>

bool InjetarDll(HANDLE manipuladorProcesso, const char* caminhoDll) {
    char caminhoCompleto[MAX_PATH];
    if (GetFullPathNameA(caminhoDll, MAX_PATH, caminhoCompleto, NULL) == 0) return false;

    LPVOID enderecoDll = VirtualAllocEx(manipuladorProcesso, 0, strlen(caminhoCompleto) + 1, MEM_COMMIT, PAGE_READWRITE);
    if (!enderecoDll) return false;

    WriteProcessMemory(manipuladorProcesso, enderecoDll, (LPVOID)caminhoCompleto, strlen(caminhoCompleto) + 1, 0);

    HMODULE moduloKernel = GetModuleHandleA("Kernel32.dll");
    LPVOID enderecoLoadLibrary = (LPVOID)GetProcAddress(moduloKernel, "LoadLibraryA");

    HANDLE threadRemota = CreateRemoteThread(manipuladorProcesso, NULL, 0, (LPTHREAD_START_ROUTINE)enderecoLoadLibrary, enderecoDll, 0, NULL);
    if (!threadRemota) {
        VirtualFreeEx(manipuladorProcesso, enderecoDll, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(threadRemota, INFINITE);
    CloseHandle(threadRemota);
    VirtualFreeEx(manipuladorProcesso, enderecoDll, 0, MEM_RELEASE);
    return true;
}

int main(int argc, char* argv[]) {
    
    if (argc != 3) return 1; 

    DWORD idProcesso = (DWORD)atoi(argv[1]);
    const char* caminhoDll = argv[2];

    HANDLE manipuladorProcesso = OpenProcess(PROCESS_ALL_ACCESS, FALSE, idProcesso);
    if (!manipuladorProcesso) return 2;

    bool sucesso = InjetarDll(manipuladorProcesso, caminhoDll);
    CloseHandle(manipuladorProcesso);

    
    return sucesso ? 0 : 3; 
}
