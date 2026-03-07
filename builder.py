import tkinter as tk
from tkinter import colorchooser, messagebox
import os
import subprocess


CPP_TEMPLATE = """#include <windows.h>
#include <vector>
#include <cmath>
#include <gl/GL.h>
#include "include/MinHook.h"

const int TRAIL_LENGTH = {trail_length};
const float BLUR_BASE_SIZE = {blur_size}f;
const float MAX_OPACITY = {max_opacity}f;
const float COLOR_R = {color_r}f;
const float COLOR_G = {color_g}f;
const float COLOR_B = {color_b}f;
const float OPACITY_DECAY = {opacity_decay}f;
const float SIZE_DECAY = {size_decay}f;
const float WHITE_CENTER_RATIO = {white_center_ratio}f; 

const bool ENABLE_GLOW = {enable_glow};
const bool ENABLE_WHITE_CENTER = {enable_white_center};

struct Vec2 {{ int x; int y; }};
std::vector<Vec2> cursorHistory;

typedef BOOL(WINAPI* twglSwapBuffers)(HDC hdc);
twglSwapBuffers o_wglSwapBuffers = nullptr;

BOOL WINAPI hkwglSwapBuffers(HDC hdc) {{
    HWND window = WindowFromDC(hdc);
    if (!window) window = FindWindowA(NULL, "osu!");

    POINT p;
    if (GetCursorPos(&p) && window != NULL) {{
        ScreenToClient(window, &p);
        cursorHistory.push_back({{ p.x, p.y }});
        if (cursorHistory.size() > TRAIL_LENGTH) cursorHistory.erase(cursorHistory.begin());
    }}

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    
    RECT rect;
    GetClientRect(window, &rect);
    glOrtho(0, rect.right, rect.bottom, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POLYGON_SMOOTH); 

    
    for (size_t i = 0; i + 1 < cursorHistory.size(); i++) {{
        float alphaRatio = (float)i / (float)(cursorHistory.size() - 1);
        float easeRatio = powf(alphaRatio, OPACITY_DECAY);
        float alpha = (MAX_OPACITY / 255.0f) * easeRatio;
        float size = BLUR_BASE_SIZE * powf(alphaRatio, SIZE_DECAY);

        float cx = (float)cursorHistory[i].x;
        float cy = (float)cursorHistory[i].y;

        if (ENABLE_GLOW) {{
            glBlendFunc(GL_SRC_ALPHA, GL_ONE); 
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(COLOR_R, COLOR_G, COLOR_B, alpha * 0.4f); 
            glVertex2f(cx, cy);
            glColor4f(COLOR_R, COLOR_G, COLOR_B, 0.0f);  
            for (int j = 0; j <= 32; j++) {{
                float angle = 2.0f * 3.14159f * (float)j / 32.0f;
                glVertex2f(cx + cosf(angle) * (size * 1.5f), cy + sinf(angle) * (size * 1.5f));
            }}
            glEnd();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
        }}

        glBegin(GL_TRIANGLE_FAN);
        glColor4f(COLOR_R, COLOR_G, COLOR_B, alpha); 
        glVertex2f(cx, cy);
        glColor4f(COLOR_R, COLOR_G, COLOR_B, 0.0f);  
        for (int j = 0; j <= 32; j++) {{
            float angle = 2.0f * 3.14159f * (float)j / 32.0f;
            glVertex2f(cx + cosf(angle) * size, cy + sinf(angle) * size);
        }}
        glEnd();
    }}

   
    if (!cursorHistory.empty()) {{
        float cx = (float)cursorHistory.back().x;
        float cy = (float)cursorHistory.back().y;
        float outerRadius = BLUR_BASE_SIZE;           
        float innerRadius = BLUR_BASE_SIZE * WHITE_CENTER_RATIO; 
        float headAlpha = MAX_OPACITY / 255.0f;

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (ENABLE_GLOW) {{
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(COLOR_R, COLOR_G, COLOR_B, headAlpha * 0.35f);
            glVertex2f(cx, cy);
            glColor4f(COLOR_R, COLOR_G, COLOR_B, 0.0f);
            for (int j = 0; j <= 32; j++) {{
                float angle = 2.0f * 3.14159f * (float)j / 32.0f;
                glVertex2f(cx + cosf(angle) * (outerRadius * 1.5f), cy + sinf(angle) * (outerRadius * 1.5f));
            }}
            glEnd();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }}

    
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(COLOR_R, COLOR_G, COLOR_B, headAlpha);
        glVertex2f(cx, cy);
        for (int j = 0; j <= 32; j++) {{
            float angle = 2.0f * 3.14159f * (float)j / 32.0f;
            glVertex2f(cx + cosf(angle) * outerRadius, cy + sinf(angle) * outerRadius);
        }}
        glEnd();

        if (ENABLE_WHITE_CENTER) {{
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(1.0f, 1.0f, 1.0f, headAlpha); 
            glVertex2f(cx, cy);
            for (int j = 0; j <= 32; j++) {{
                float angle = 2.0f * 3.14159f * (float)j / 32.0f;
                glVertex2f(cx + cosf(angle) * innerRadius, cy + sinf(angle) * innerRadius);
            }}
            glEnd();
        }}
    }}

    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glPopAttrib();

    return o_wglSwapBuffers(hdc);
}}

DWORD WINAPI MainThread(LPVOID lpReserved) {{
    if (MH_Initialize() != MH_OK) return FALSE;
    
    HMODULE hOpengl = GetModuleHandleA("opengl32.dll");
    if (hOpengl) {{
        void* p_wglSwapBuffers = (void*)GetProcAddress(hOpengl, "wglSwapBuffers");
        if (p_wglSwapBuffers) {{
            MH_CreateHook(p_wglSwapBuffers, (LPVOID)&hkwglSwapBuffers, reinterpret_cast<LPVOID*>(&o_wglSwapBuffers));
            MH_EnableHook(p_wglSwapBuffers);
        }}
    }}

    while (!GetAsyncKeyState(VK_END)) {{
        Sleep(100); 
    }}

    MH_DisableHook(MH_ALL_HOOKS);
    MH_RemoveHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    Sleep(100);

    FreeLibraryAndExitThread((HMODULE)lpReserved, 0);
    return TRUE;
}}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {{
    if (fdwReason == DLL_PROCESS_ATTACH) {{
        DisableThreadLibraryCalls(hinstDLL);
        CreateThread(nullptr, 0, MainThread, hinstDLL, 0, nullptr);
    }}
    return TRUE;
}}
"""

class DllBuilderApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Osu! Custom Trail Builder")
        self.root.geometry("380x620")
        
        self.trail_len = tk.IntVar(value=30)
        self.blur_size = tk.DoubleVar(value=20.0)
        self.max_opacity = tk.DoubleVar(value=200.0)
        self.opacity_decay = tk.DoubleVar(value=1.5)
        self.size_decay = tk.DoubleVar(value=0.8)
        self.white_center_ratio = tk.DoubleVar(value=0.85)
        self.color_rgb = (76/255, 240/255, 187/255) 
        
        self.enable_glow = tk.BooleanVar(value=False)
        self.enable_white_center = tk.BooleanVar(value=False)

        tk.Label(root, text="Tamanho da Cauda").pack()
        tk.Scale(root, variable=self.trail_len, from_=5, to=100, orient="horizontal").pack()

        tk.Label(root, text="Grossura do Rastro").pack()
        tk.Scale(root, variable=self.blur_size, from_=5, to=100, resolution=0.1, orient="horizontal").pack()

        tk.Label(root, text="Opacidade Máxima (0-255)").pack()
        tk.Scale(root, variable=self.max_opacity, from_=10, to=255, orient="horizontal").pack()

        tk.Label(root, text="Decaimento da Opacidade (Suavidade)").pack()
        tk.Scale(root, variable=self.opacity_decay, from_=0.1, to=3.0, resolution=0.1, orient="horizontal").pack()

        tk.Label(root, text="Afinamento da Ponta").pack()
        tk.Scale(root, variable=self.size_decay, from_=0.1, to=3.0, resolution=0.1, orient="horizontal").pack()

        tk.Label(root, text="Tamanho do Centro").pack()
        tk.Scale(root, variable=self.white_center_ratio, from_=0.10, to=0.99, resolution=0.01, orient="horizontal").pack()

        tk.Checkbutton(root, text="Adicionar Efeito de Brilho ", variable=self.enable_glow).pack(pady=2)
        tk.Checkbutton(root, text="Cursor c/ Centro Branco (Estilo Print)", variable=self.enable_white_center).pack(pady=2)

        self.btn_color = tk.Button(root, text="Escolher Cor", command=self.choose_color)
        self.btn_color.pack(pady=10)

        tk.Button(root, text="GERAR DLL", bg="green", fg="white", font=("Arial", 12, "bold"), command=self.build_dll).pack(pady=15)

    def choose_color(self):
        color_code = colorchooser.askcolor(title="Escolha a cor do cursor")
        if color_code[0]:
            self.color_rgb = (color_code[0][0]/255, color_code[0][1]/255, color_code[0][2]/255)
            self.btn_color.config(bg=color_code[1])

    def build_dll(self):
        cpp_code = CPP_TEMPLATE.format(
            trail_length=self.trail_len.get(),
            blur_size=self.blur_size.get(),
            max_opacity=self.max_opacity.get(),
            color_r=self.color_rgb[0],
            color_g=self.color_rgb[1],
            color_b=self.color_rgb[2],
            opacity_decay=self.opacity_decay.get(),
            size_decay=self.size_decay.get(),
            white_center_ratio=self.white_center_ratio.get(),
            enable_glow="true" if self.enable_glow.get() else "false",
            enable_white_center="true" if self.enable_white_center.get() else "false"
        )

        with open("custom_trail.cpp", "w") as f:
            f.write(cpp_code)

        command = "g++ -m32 -O2 -shared -static-libgcc -static-libstdc++ -static -o MyCustomTrail.dll custom_trail.cpp src/buffer.c src/hook.c src/trampoline.c src/hde/hde32.c -lopengl32 -luser32 -lgdi32"
        
        try:
            print("Compilando...")
            subprocess.run(command, shell=True, check=True, capture_output=True, text=True)
            messagebox.showinfo("Sucesso", "MyCustomTrail.dll gerada com sucesso!\nPara desinjetar no jogo, aperte a tecla END.")
        except subprocess.CalledProcessError as e:
            messagebox.showerror("Erro de Compilação", f"Erro:\n{e.stderr}")

if __name__ == "__main__":
    root = tk.Tk()
    app = DllBuilderApp(root)
    root.mainloop()
 # CD cd C:/Users/Usuario/OneDrive/"Área de Trabalho"/Desktop/C_C++/dll
