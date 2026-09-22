/* std.gfx — software canvas, a window, and sound. The game-loop verbs. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "buraaq_std.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

#define BQ_GFX_MAX_W 1920
#define BQ_GFX_MAX_H 1080

static int32_t g_w = 0;
static int32_t g_h = 0;
static uint32_t *g_px = NULL;
static uint32_t g_ink = 0x00FFFFFFu;
static int32_t g_pulse = 0;
static int g_quit = 0;
static int g_own_px = 0;

#ifdef _WIN32
static HWND g_hwnd = NULL;
static int g_class = 0;

static void gfx_blit(HDC hdc) {
    if (!g_px || g_w <= 0 || g_h <= 0) return;
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_w;
    bmi.bmiHeader.biHeight = -g_h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(hdc, 0, 0, g_w, g_h, 0, 0, g_w, g_h, g_px, &bmi, DIB_RGB_COLORS, SRCCOPY);
}

static LRESULT CALLBACK bq_gfx_wnd(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CLOSE) {
        g_quit = 1;
        DestroyWindow(h);
        return 0;
    }
    if (m == WM_DESTROY) {
        g_hwnd = NULL;
        return 0;
    }
    if (m == WM_ERASEBKGND) {
        return 1;
    }
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        gfx_blit(hdc);
        EndPaint(h, &ps);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

static int vk_of(const char *name) {
    if (!name || !name[0]) return 0;
    if (strcmp(name, "escape") == 0 || strcmp(name, "quit") == 0) return VK_ESCAPE;
    if (strcmp(name, "left") == 0) return VK_LEFT;
    if (strcmp(name, "right") == 0) return VK_RIGHT;
    if (strcmp(name, "up") == 0) return VK_UP;
    if (strcmp(name, "down") == 0) return VK_DOWN;
    if (strcmp(name, "space") == 0) return VK_SPACE;
    if (strcmp(name, "enter") == 0) return VK_RETURN;
    if (strcmp(name, "shift") == 0) return VK_SHIFT;
    if (strcmp(name, "ctrl") == 0) return VK_CONTROL;
    if (name[1] == 0) {
        char c = name[0];
        if (c >= 'a' && c <= 'z') return (int)(c - 'a' + 'A');
        if (c >= 'A' && c <= 'Z') return (int)c;
        if (c >= '0' && c <= '9') return (int)c;
    }
    return 0;
}

static void gfx_free_window(void) {
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
    }
}

static int gfx_headless(void) {
    const char *e = getenv("BURAAQ_GFX_HEADLESS");
    return e && e[0] && e[0] != '0' && e[0] != 'n' && e[0] != 'N' && e[0] != 'f' && e[0] != 'F';
}

static void gfx_pump(void) {
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) g_quit = 1;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

static int gfx_open_window(const char *title) {
    if (gfx_headless()) return 0;
    HINSTANCE inst = GetModuleHandleA(NULL);
    if (!g_class) {
        WNDCLASSA wc;
        memset(&wc, 0, sizeof(wc));
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = bq_gfx_wnd;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = "BqGfx";
        ATOM at = RegisterClassA(&wc);
        if (!at) {
            DWORD err = GetLastError();
            if (err != ERROR_CLASS_ALREADY_EXISTS) {
                fprintf(stderr, "gfx: RegisterClass failed (%lu)\n", (unsigned long)err);
                return 0;
            }
        }
        g_class = 1;
    }
    RECT r;
    r.left = 0;
    r.top = 0;
    r.right = g_w;
    r.bottom = g_h;
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    int ww = r.right - r.left;
    int hh = r.bottom - r.top;
    if (ww < 160) ww = 160;
    if (hh < 120) hh = 120;
    g_hwnd = CreateWindowExA(
        WS_EX_APPWINDOW,
        "BqGfx",
        title && title[0] ? title : "buraaq",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        80,
        80,
        ww,
        hh,
        NULL,
        NULL,
        inst,
        NULL);
    if (!g_hwnd) {
        fprintf(stderr, "gfx: CreateWindow failed (%lu)\n", (unsigned long)GetLastError());
        return 0;
    }
    ShowWindow(g_hwnd, SW_SHOWNORMAL);
    SetWindowPos(g_hwnd, HWND_TOP, 80, 80, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(g_hwnd);
    UpdateWindow(g_hwnd);
    gfx_pump();
    return 1;
}
#endif

static uint32_t pack_rgb(int32_t r, int32_t g, int32_t b) {
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

static void put_px(int32_t x, int32_t y) {
    if (!g_px || x < 0 || y < 0 || x >= g_w || y >= g_h) return;
    g_px[(size_t)y * (size_t)g_w + (size_t)x] = g_ink;
}

int32_t buraaq_gfx_canvas(int32_t w, int32_t h, const char *title) {
    if (w < 8) w = 8;
    if (h < 8) h = 8;
    if (w > BQ_GFX_MAX_W) w = BQ_GFX_MAX_W;
    if (h > BQ_GFX_MAX_H) h = BQ_GFX_MAX_H;
#ifdef _WIN32
    gfx_free_window();
#endif
    if (g_own_px && g_px) free(g_px);
    g_px = NULL;
    g_own_px = 0;
    g_w = w;
    g_h = h;
    g_pulse = 0;
    g_quit = 0;
    g_ink = 0x00FFFFFFu;
    g_px = (uint32_t *)calloc((size_t)w * (size_t)h, sizeof(uint32_t));
    if (!g_px) {
        g_w = 0;
        g_h = 0;
        return 0;
    }
    g_own_px = 1;
    buraaq_gfx_wipe();
#ifdef _WIN32
    if (!gfx_open_window(title)) {
        fprintf(stderr, "gfx: no window — playing in this terminal only. unset BURAAQ_GFX_HEADLESS if set.\n");
    }
#else
    (void)title;
#endif
    return 1;
}

void buraaq_gfx_ink(int32_t r, int32_t g, int32_t b) {
    g_ink = pack_rgb(r, g, b);
}

void buraaq_gfx_wipe(void) {
    if (!g_px || g_w <= 0 || g_h <= 0) return;
    size_t n = (size_t)g_w * (size_t)g_h;
    for (size_t i = 0; i < n; i++) g_px[i] = g_ink;
}

void buraaq_gfx_plot(int32_t x, int32_t y) {
    put_px(x, y);
}

void buraaq_gfx_box(int32_t x, int32_t y, int32_t w, int32_t h) {
    if (w < 0) {
        x = x + w;
        w = -w;
    }
    if (h < 0) {
        y = y + h;
        h = -h;
    }
    int32_t x1 = x + w;
    int32_t y1 = y + h;
    int32_t yy = y;
    while (yy < y1) {
        int32_t xx = x;
        while (xx < x1) {
            put_px(xx, yy);
            xx = xx + 1;
        }
        yy = yy + 1;
    }
}

void buraaq_gfx_dash(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    int32_t dx = x1 - x0;
    int32_t dy = y1 - y0;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t err = dx - dy;
    for (;;) {
        put_px(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int32_t e2 = err + err;
        if (e2 > -dy) {
            err = err - dy;
            x0 = x0 + sx;
        }
        if (e2 < dx) {
            err = err + dx;
            y0 = y0 + sy;
        }
    }
}

void buraaq_gfx_flip(void) {
    g_pulse = g_pulse + 1;
#ifdef _WIN32
    gfx_pump();
    if (g_hwnd) {
        HDC hdc = GetDC(g_hwnd);
        if (hdc) {
            gfx_blit(hdc);
            ReleaseDC(g_hwnd, hdc);
        }
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
#endif
}

int32_t buraaq_gfx_held(const char *name) {
    if (g_quit) {
        if (!name || !name[0]) return 1;
        if (strcmp(name, "escape") == 0 || strcmp(name, "quit") == 0) return 1;
    }
#ifdef _WIN32
    if (g_hwnd) {
        if (GetForegroundWindow() != g_hwnd) {
            return 0;
        }
    }
    int vk = vk_of(name);
    if (vk == 0) return 0;
    return (GetAsyncKeyState(vk) & 0x8000) ? 1 : 0;
#else
    (void)name;
    return 0;
#endif
}

int32_t buraaq_gfx_pulse(void) {
    return g_pulse;
}

void buraaq_gfx_play(const char *path) {
#ifdef _WIN32
    if (!path || !path[0]) return;
    PlaySoundA(path, NULL, SND_FILENAME | SND_ASYNC);
#else
    (void)path;
#endif
}

void buraaq_gfx_tone(int32_t hz, int32_t ms) {
#ifdef _WIN32
    if (hz < 37) hz = 37;
    if (hz > 32767) hz = 32767;
    if (ms < 1) ms = 1;
    if (ms > 5000) ms = 5000;
    Beep((DWORD)hz, (DWORD)ms);
#else
    (void)hz;
    (void)ms;
#endif
}

void buraaq_gfx_hush(void) {
#ifdef _WIN32
    PlaySoundA(NULL, NULL, 0);
#endif
}
