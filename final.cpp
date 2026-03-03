#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <tlhelp32.h>
#include <commdlg.h>
#include <thread>
#include <string>
#include <stdio.h>

HWND janelaPrincipal, caixaTextoProcesso, botaoSelecionar, caixaTextoDll, botaoProcurarDll;
HWND botaoApenasInjetar, botaoApenasOtimizar, botaoAmbos, textoStatus;
HWND janelaListaProcessos, caixaListaProcessos, botaoAtualizarLista, botaoConfirmarLista;

bool VerificarCompatibilidade(HANDLE manipuladorProcesso) {
    BOOL ehWow64 = FALSE;
    IsWow64Process(manipuladorProcesso, &ehWow64);

    SYSTEM_INFO infoSistema;
    GetNativeSystemInfo(&infoSistema);
    
    bool sistemaEh64Bits = (infoSistema.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 || 
                            infoSistema.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64);
    
    bool jogoEh32Bits = sistemaEh64Bits ? (ehWow64 != FALSE) : true;

    #if defined(_WIN64)
        bool injetorEh32Bits = false; 
    #else
        bool injetorEh32Bits = true;  
    #endif

    if (jogoEh32Bits != injetorEh32Bits) {
        if (injetorEh32Bits) {
            MessageBoxA(NULL, "Erro: O injetor é 32 bits, mas o jogo é 64 bits!", "Incompatibilidade de arquitetura", MB_ICONERROR);
        } else {
            MessageBoxA(NULL, "Erro: O injetor é 64 bits, mas o jogo é 32 bits!", "Incompatibilidade de arquitetura", MB_ICONERROR);
        }
        return false;
    }
    return true; 
}

DWORD ObterIdProcessoPorNome(const char* nomeProcesso) {
    PROCESSENTRY32 entradaProcesso;
    HANDLE snapshotProcessos = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    entradaProcesso.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(snapshotProcessos, &entradaProcesso)) {
        do {
            if (!_stricmp(entradaProcesso.szExeFile, nomeProcesso)) {
                CloseHandle(snapshotProcessos);
                return entradaProcesso.th32ProcessID;
            }
        } while (Process32Next(snapshotProcessos, &entradaProcesso));
    }
    CloseHandle(snapshotProcessos);
    return 0;
}

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

void ExecutarAcao(std::string nomeProcesso, std::string caminhoDll, bool flagInjetar, bool flagOtimizar) {
    std::string mensagemStatus = "Status: Aguardando " + nomeProcesso + "...";
    SetWindowTextA(textoStatus, mensagemStatus.c_str());
    
    DWORD idProcesso = 0;
    while (!idProcesso) {
        idProcesso = ObterIdProcessoPorNome(nomeProcesso.c_str());
        if (!idProcesso) Sleep(1000);
    }

    HANDLE manipuladorProcesso = OpenProcess(PROCESS_ALL_ACCESS, FALSE, idProcesso);
    if (manipuladorProcesso) {
        SetWindowTextA(textoStatus, "Status: Processo encontrado. Aplicando...");

        if (flagOtimizar) {
            SetPriorityClass(manipuladorProcesso, HIGH_PRIORITY_CLASS);

            SYSTEM_INFO infoSistema;
            GetSystemInfo(&infoSistema);
            DWORD_PTR afinidade = (1ULL << infoSistema.dwNumberOfProcessors) - 1;
            SetProcessAffinityMask(manipuladorProcesso, afinidade);

            // 5 gb
            SIZE_T memoriaAlvo = 5ULL * 1024 * 1024 * 1024; 
            // 256 mb
            SIZE_T tamanhoBloco = 256 * 1024 * 1024;        
            int quantidadeBlocos = memoriaAlvo / tamanhoBloco;

            for (int i = 0; i < quantidadeBlocos; i++) {
                LPVOID memoriaRemota = VirtualAllocEx(manipuladorProcesso, NULL, tamanhoBloco, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                if (memoriaRemota) {
                    char valorFicticio = 1;
                    WriteProcessMemory(manipuladorProcesso, memoriaRemota, &valorFicticio, 1, NULL);
                }
            }
        }

        if (flagInjetar) {
            if (!VerificarCompatibilidade(manipuladorProcesso)) {
                SetWindowTextA(textoStatus, "Status: Falha - Arquiteturas incompatíveis.");
                CloseHandle(manipuladorProcesso);
                return;
            }

            if (!InjetarDll(manipuladorProcesso, caminhoDll.c_str())) {
                SetWindowTextA(textoStatus, "Status: Falha ao injetar a DLL.");
                CloseHandle(manipuladorProcesso);
                return;
            }
        }

        SetWindowTextA(textoStatus, "Sucesso");
        CloseHandle(manipuladorProcesso);
    } else {
        SetWindowTextA(textoStatus, "Status: ERRO! Execute como administrador.");
    }
}

void IniciarThread(bool flagInjetar, bool flagOtimizar) {
    char bufferProcesso[MAX_PATH];
    GetWindowTextA(caixaTextoProcesso, bufferProcesso, MAX_PATH);
    std::string nomeProcesso(bufferProcesso);

    char bufferDll[MAX_PATH];
    GetWindowTextA(caixaTextoDll, bufferDll, MAX_PATH);
    std::string caminhoDll(bufferDll);

    if (nomeProcesso.empty()) {
        SetWindowTextA(textoStatus, "Status: Selecione um processo primeiro!");
        return;
    }
    
    if (flagInjetar && caminhoDll.empty()) {
        SetWindowTextA(textoStatus, "Status: Selecione uma DLL primeiro!");
        return;
    }

    std::thread(ExecutarAcao, nomeProcesso, caminhoDll, flagInjetar, flagOtimizar).detach();
}

void SelecionarArquivoDll() {
    OPENFILENAMEA estruturaArquivo;
    char arquivoSelecionado[MAX_PATH] = {0};

    ZeroMemory(&estruturaArquivo, sizeof(estruturaArquivo));
    estruturaArquivo.lStructSize = sizeof(estruturaArquivo);
    estruturaArquivo.hwndOwner = janelaPrincipal;
    estruturaArquivo.lpstrFile = arquivoSelecionado;
    estruturaArquivo.nMaxFile = sizeof(arquivoSelecionado);
    estruturaArquivo.lpstrFilter = "Arquivos DLL (*.dll)\0*.dll\0Todos os arquivos (*.*)\0*.*\0";
    estruturaArquivo.nFilterIndex = 1;
    estruturaArquivo.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&estruturaArquivo) == TRUE) {
        SetWindowTextA(caixaTextoDll, estruturaArquivo.lpstrFile);
    }
}

void AtualizarListaProcessos() {
    SendMessage(caixaListaProcessos, LB_RESETCONTENT, 0, 0); 
    HANDLE snapshotProcessos = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 entradaProcesso;
    entradaProcesso.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshotProcessos, &entradaProcesso)) {
        do {
            char bufferTexto[512];
            snprintf(bufferTexto, sizeof(bufferTexto), "[%04d] %s", entradaProcesso.th32ProcessID, entradaProcesso.szExeFile);
            SendMessageA(caixaListaProcessos, LB_ADDSTRING, 0, (LPARAM)bufferTexto);
        } while (Process32Next(snapshotProcessos, &entradaProcesso));
    }
    CloseHandle(snapshotProcessos);
}

void ConfirmarSelecaoLista() {
    int indiceSelecionado = SendMessage(caixaListaProcessos, LB_GETCURSEL, 0, 0);
    if (indiceSelecionado != LB_ERR) {
        char bufferTexto[512];
        SendMessageA(caixaListaProcessos, LB_GETTEXT, indiceSelecionado, (LPARAM)bufferTexto);
        
        char* nomeExecutavel = strchr(bufferTexto, ']');
        if (nomeExecutavel) {
            nomeExecutavel += 2; 
            SetWindowTextA(caixaTextoProcesso, nomeExecutavel);
            ShowWindow(janelaListaProcessos, SW_HIDE); 
        }
    }
}

LRESULT CALLBACK ProcedimentoJanelaLista(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            caixaListaProcessos = CreateWindowA("LISTBOX", NULL,
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_NOTIFY,
                10, 10, 260, 200, hwnd, (HMENU)100, NULL, NULL);

            botaoAtualizarLista = CreateWindowA("BUTTON", "Atualizar",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                10, 220, 100, 30, hwnd, (HMENU)101, NULL, NULL);

            botaoConfirmarLista = CreateWindowA("BUTTON", "Confirmar",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                170, 220, 100, 30, hwnd, (HMENU)102, NULL, NULL);
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == 101) AtualizarListaProcessos();
            if (LOWORD(wParam) == 102) ConfirmarSelecaoLista();
            if (LOWORD(wParam) == 100 && HIWORD(wParam) == LBN_DBLCLK) ConfirmarSelecaoLista();
            break;

        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ProcedimentoJanelaPrincipal(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            CreateWindowA("STATIC", "Processo alvo:", WS_VISIBLE | WS_CHILD, 20, 15, 250, 20, hwnd, NULL, NULL, NULL);
            caixaTextoProcesso = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 20, 35, 170, 25, hwnd, NULL, NULL, NULL);
            botaoSelecionar = CreateWindowA("BUTTON", "Selecionar", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 200, 35, 70, 25, hwnd, (HMENU)4, NULL, NULL);

            CreateWindowA("STATIC", "Caminho da DLL:", WS_VISIBLE | WS_CHILD, 20, 70, 250, 20, hwnd, NULL, NULL, NULL);
            caixaTextoDll = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 20, 90, 170, 25, hwnd, NULL, NULL, NULL);
            botaoProcurarDll = CreateWindowA("BUTTON", "Procurar", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 200, 90, 70, 25, hwnd, (HMENU)5, NULL, NULL);

            botaoApenasInjetar = CreateWindowA("BUTTON", "Apenas injetar", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, 130, 250, 35, hwnd, (HMENU)1, NULL, NULL);
            botaoApenasOtimizar = CreateWindowA("BUTTON", "Apenas otimizar CPU", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, 175, 250, 35, hwnd, (HMENU)2, NULL, NULL);
            botaoAmbos = CreateWindowA("BUTTON", "Injetar e otimizar", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, 220, 250, 35, hwnd, (HMENU)3, NULL, NULL);

            textoStatus = CreateWindowA("STATIC", "Status: Aguardando...", WS_VISIBLE | WS_CHILD, 20, 265, 250, 40, hwnd, NULL, NULL, NULL);
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == 1) IniciarThread(true, false);
            if (LOWORD(wParam) == 2) IniciarThread(false, true);
            if (LOWORD(wParam) == 3) IniciarThread(true, true);
            
            if (LOWORD(wParam) == 4) { 
                AtualizarListaProcessos();
                ShowWindow(janelaListaProcessos, SW_SHOW);
                SetForegroundWindow(janelaListaProcessos);
            }

            if (LOWORD(wParam) == 5) SelecionarArquivoDll();
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE instanciaAplicativo, HINSTANCE instanciaAnterior, LPSTR linhaComando, int estadoExibicao) {
    WNDCLASSA classePrincipal = {0};
    classePrincipal.lpfnWndProc = ProcedimentoJanelaPrincipal;
    classePrincipal.hInstance = instanciaAplicativo;
    classePrincipal.lpszClassName = "ClasseOtimizadorGenerico";
    classePrincipal.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    RegisterClassA(&classePrincipal);

    WNDCLASSA classeLista = {0};
    classeLista.lpfnWndProc = ProcedimentoJanelaLista;
    classeLista.hInstance = instanciaAplicativo;
    classeLista.lpszClassName = "ClasseListaProcessos";
    classeLista.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    RegisterClassA(&classeLista);

    janelaPrincipal = CreateWindowA("ClasseOtimizadorGenerico", "Necomizer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, 
        CW_USEDEFAULT, CW_USEDEFAULT, 310, 360,
        NULL, NULL, instanciaAplicativo, NULL);

    janelaListaProcessos = CreateWindowA("ClasseListaProcessos", "Selecione o processo",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 300,
        janelaPrincipal, NULL, instanciaAplicativo, NULL);

    ShowWindow(janelaPrincipal, estadoExibicao);

    MSG mensagemSistema;
    while (GetMessage(&mensagemSistema, NULL, 0, 0)) {
        TranslateMessage(&mensagemSistema);
        DispatchMessage(&mensagemSistema);
    }
    return 0;
}
//cp: g++ -o necomizer.exe necomizer.cpp -static -lkernel32 -luser32 -lwinmm -mwindows
