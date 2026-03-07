#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <cmath>
#include "include/MinHook.h" 
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_internal.h"


int   TRAIL_LENGTH = 60;
float BLUR_BASE_SIZE = 40.0f;
float MAX_OPACITY = 250.0f;
float MENU_COLOR[3] = { 1.0f, 1.0f, 1.0f }; 
float OPACITY_DECAY = 1.5f;
float SIZE_DECAY = 0.8f;


float OFFSET_X = 0.0f;
float OFFSET_Y = 0.0f;

bool showMenu = true; 

struct Vec2 { float x; float y; };
std::vector<Vec2> cursorHistory;

ID3D11Device* pDevice = nullptr;
ID3D11DeviceContext* pContext = nullptr;
ID3D11RenderTargetView* mainRenderTargetView = nullptr;
HWND window = nullptr;
bool init = false;

typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);
Present_t oPresent = nullptr;


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
WNDPROC oWndProc = nullptr;

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT) {
        showMenu = !showMenu;
        return 0; 
    }

    if (showMenu) {
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
            return TRUE; 
        }
    }
    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

void DrawSmoothGlow(ImDrawList* draw_list, ImVec2 center, float radius, ImU32 col_center, ImU32 col_edge) {
    const int segments = 32; 
    draw_list->PrimReserve(segments * 3, segments + 1);
    ImVec2 uv = draw_list->_Data->TexUvWhitePixel;
    ImDrawIdx vtx_base = draw_list->_VtxCurrentIdx;
    
    draw_list->PrimWriteVtx(center, uv, col_center); 
    
    for (int i = 0; i < segments; i++) {
        float angle = (float)i * (2.0f * 3.1415926f / segments);
        ImVec2 p = ImVec2(center.x + cosf(angle) * radius, center.y + sinf(angle) * radius);
        draw_list->PrimWriteVtx(p, uv, col_edge);
    }
    
    for (int i = 0; i < segments; i++) {
        draw_list->PrimWriteIdx(vtx_base);
        draw_list->PrimWriteIdx(vtx_base + 1 + i);
        draw_list->PrimWriteIdx(vtx_base + 1 + ((i + 1) % segments));
    }
}

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!init) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice))) {
            pDevice->GetImmediateContext(&pContext);
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            window = sd.OutputWindow;

            ID3D11Texture2D* pBackBuffer;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
            pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
            pBackBuffer->Release();

         
            oWndProc = (WNDPROC)SetWindowLongPtrA(window, GWLP_WNDPROC, (LONG_PTR)WndProc);

            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

            ImGui_ImplWin32_Init(window);
            ImGui_ImplDX11_Init(pDevice, pContext);

            init = true;
        } else {
            return oPresent(pSwapChain, SyncInterval, Flags);
        }
    }

  
    POINT p;
    if (GetCursorPos(&p) && window != NULL) {
        ScreenToClient(window, &p);
        
     
        RECT clientRect;
        GetClientRect(window, &clientRect);
        float clientW = (float)(clientRect.right - clientRect.left);
        float clientH = (float)(clientRect.bottom - clientRect.top);

        ID3D11Texture2D* pBackBuffer = nullptr;
        if (clientW > 0 && clientH > 0 && SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer))) {
            D3D11_TEXTURE2D_DESC bbDesc;
            pBackBuffer->GetDesc(&bbDesc);
            pBackBuffer->Release();

            float bbWidth = (float)bbDesc.Width;
            float bbHeight = (float)bbDesc.Height;

            
            float scaledX = ((float)p.x / clientW) * bbWidth;
            float scaledY = ((float)p.y / clientH) * bbHeight;

            cursorHistory.push_back({ scaledX, scaledY });
        }

        if (cursorHistory.size() > (size_t)TRAIL_LENGTH) {
            cursorHistory.erase(cursorHistory.begin());
        }
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    
    if (showMenu) {
        ImGui::SetNextWindowSize(ImVec2(400, 420), ImGuiCond_FirstUseEver);
        ImGui::Begin("Painel Lazer - DX11 (64 bits)", &showMenu);
        ImGui::Text("Aperte [INSERT] para fechar/abrir.");
        ImGui::Separator();
        
        ImGui::ColorEdit3("Cor", MENU_COLOR);
        
        ImGui::Spacing(); ImGui::Text("Ajustes Manuais (Resolucao)");
        ImGui::SliderFloat("Offset X (Lados)", &OFFSET_X, -200.0f, 200.0f, "%.1f px");
        ImGui::SliderFloat("Offset Y (Cima/Baixo)", &OFFSET_Y, -200.0f, 200.0f, "%.1f px");

        ImGui::Spacing(); ImGui::Text("Dimensoes da Cauda");
        ImGui::SliderInt("Comprimento", &TRAIL_LENGTH, 5, 100);
        ImGui::SliderFloat("Tamanho Base", &BLUR_BASE_SIZE, 5.0f, 100.0f);
        ImGui::SliderFloat("Opacidade Maxima", &MAX_OPACITY, 10.0f, 255.0f);
        
        ImGui::Spacing(); ImGui::Text("Fisica");
        ImGui::SliderFloat("Curva de Opacidade", &OPACITY_DECAY, 0.1f, 10.0f, "%.2f");
        ImGui::SliderFloat("Curva de Tamanho", &SIZE_DECAY, 0.1f, 3.0f, "%.2f");
        
        ImGui::End();
    }

    
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

   
    int cr = (int)(MENU_COLOR[0] * 255.0f);
    int cg = (int)(MENU_COLOR[1] * 255.0f);
    int cb = (int)(MENU_COLOR[2] * 255.0f);

    for (size_t i = 0; i < cursorHistory.size(); i++) {
        float ratio = (float)i / (float)(cursorHistory.size() - 1);
        
        float easeRatio = powf(ratio, OPACITY_DECAY); 
        int alpha = (int)(MAX_OPACITY * easeRatio); 
        
        float currentSize = BLUR_BASE_SIZE * powf(ratio, SIZE_DECAY); 

       
        float x = cursorHistory[i].x + OFFSET_X;
        float y = cursorHistory[i].y + OFFSET_Y;

        ImU32 col_center = IM_COL32(cr, cg, cb, alpha); 
        ImU32 col_edge   = IM_COL32(cr, cg, cb, 0);     
        
        DrawSmoothGlow(draw_list, ImVec2(x, y), currentSize, col_center, col_edge);
    }

    ImGui::Render();
    pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI MainThread(LPVOID lpReserved) {
    if (MH_Initialize() != MH_OK) return FALSE;

    HWND dummyWindow = CreateWindowA("STATIC", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    
    DXGI_SWAP_CHAIN_DESC sd = { 0 };
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = dummyWindow;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* pSwapChain = nullptr;
    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, featureLevels, 2, D3D11_SDK_VERSION, &sd, &pSwapChain, &pDevice, &featureLevel, &pContext))) {
        
        void** pVTable = *reinterpret_cast<void***>(pSwapChain);
        void* pPresent = pVTable[8]; 

        MH_CreateHook(pPresent, (LPVOID)&hkPresent, reinterpret_cast<LPVOID*>(&oPresent));
        MH_EnableHook(pPresent);

        pSwapChain->Release();
        pDevice->Release();
        pContext->Release();
    }

    DestroyWindow(dummyWindow);
    return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        CreateThread(nullptr, 0, MainThread, hinstDLL, 0, nullptr);
    }
    return TRUE;
}
//cp: g++ -m64 -O2 -shared -static-libgcc -static-libstdc++ -static -o blurDX11.dll motionblur64.cpp imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/imgui_impl_win32.cpp imgui/imgui_impl_dx11.cpp src/buffer.c src/hook.c src/trampoline.c src/hde/hde64.c -ld3d11 -ldxgi -ld3dcompiler -luser32 -lgdi32 -ldwmapi
