#define UNICODE
#define _UNICODE

#include <windows.h>
#include <stdio.h>
#include <wchar.h>

#include "Y720CoolingMonitor.h"

#define IDI_Y720COOLINGMONITOR 101

#define TIMER_ID       1
#define TIMER_INTERVAL 1000

#define BUTTON_COOLING_ON  1001
#define BUTTON_COOLING_OFF 1002


static HWND g_window = NULL;

static HWND g_button_on = NULL;
static HWND g_button_off = NULL;

static GAMEZONE_DATA g_data;

static HFONT g_title_font = NULL;
static HFONT g_normal_font = NULL;
static HFONT g_value_font = NULL;
static HFONT g_button_font = NULL;


/*
 * Application icons.
 *
 * The icon is embedded in the executable by the .rc file.
 *
 * We keep separate large and small HICON handles so that
 * Windows can use the appropriate size for the window and
 * taskbar/title bar.
 */
static HICON g_app_icon_big = NULL;
static HICON g_app_icon_small = NULL;


/*
 * Last command sent by the GUI.
 *
 * -1 = no command sent yet
 *  0 = last command was OFF
 *  1 = last command was ON
 *
 * This is intentionally NOT presented as the physical
 * cooling state because Lenovo's GetFanCoolingStatus
 * method is unreliable on this machine.
 */
static int g_command_state = -1;


/* ---------------------------------------------------------
   Update telemetry
   --------------------------------------------------------- */

static void update_data(void)
{
    ZeroMemory(
        &g_data,
        sizeof(g_data)
    );

    GameZoneRead(
        &g_data
    );

    InvalidateRect(
        g_window,
        NULL,
        TRUE
    );
}


/* ---------------------------------------------------------
   Draw text
   --------------------------------------------------------- */

static void draw_text(
    HDC hdc,
    int x,
    int y,
    const wchar_t *text,
    HFONT font,
    COLORREF color)
{
    HFONT old_font;
    COLORREF old_color;

    old_font = SelectObject(
        hdc,
        font
    );

    old_color = SetTextColor(
        hdc,
        color
    );

    SetBkMode(
        hdc,
        TRANSPARENT
    );

    TextOutW(
        hdc,
        x,
        y,
        text,
        (int)wcslen(text)
    );

    SetTextColor(
        hdc,
        old_color
    );

    SelectObject(
        hdc,
        old_font
    );
}


/* ---------------------------------------------------------
   Draw label/value row
   --------------------------------------------------------- */

static void draw_label_value(
    HDC hdc,
    int y,
    const wchar_t *label,
    const wchar_t *value)
{
    draw_text(
        hdc,
        45,
        y,
        label,
        g_normal_font,
        RGB(190, 190, 200)
    );

    draw_text(
        hdc,
        250,
        y,
        value,
        g_value_font,
        RGB(245, 245, 250)
    );
}


/* ---------------------------------------------------------
   Number formatter
   --------------------------------------------------------- */

static void format_number(
    wchar_t *buffer,
    size_t size,
    LONG value,
    const wchar_t *suffix)
{
    swprintf(
        buffer,
        size,
        L"%ld%s",
        (long)value,
        suffix
    );
}


/* ---------------------------------------------------------
   Draw panel
   --------------------------------------------------------- */

static void draw_panel(
    HDC hdc,
    int top,
    int bottom)
{
    HBRUSH panel;
    RECT r;

    panel = CreateSolidBrush(
        RGB(28, 28, 34)
    );

    r.left   = 25;
    r.top    = top;
    r.right  = 575;
    r.bottom = bottom;

    FillRect(
        hdc,
        &r,
        panel
    );

    DeleteObject(panel);
}


/* ---------------------------------------------------------
   Paint GUI
   --------------------------------------------------------- */

static void paint_gui(
    HWND hwnd,
    HDC hdc)
{
    RECT rect;

    wchar_t buffer[128];

    HBRUSH background;


    GetClientRect(
        hwnd,
        &rect
    );


    /* -----------------------------------------------------
       Background
       ----------------------------------------------------- */

    background = CreateSolidBrush(
        RGB(18, 18, 22)
    );

    FillRect(
        hdc,
        &rect,
        background
    );

    DeleteObject(background);


    /* -----------------------------------------------------
       Title
       ----------------------------------------------------- */

    draw_text(
        hdc,
        35,
        20,
        L"LEGION Y720 COOLING MONITOR",
        g_title_font,
        RGB(240, 240, 245)
    );


    /* -----------------------------------------------------
       EXTREME COOLING
       ----------------------------------------------------- */

    draw_panel(
        hdc,
        65,
        165
    );

    draw_text(
        hdc,
        45,
        80,
        L"EXTREME COOLING",
        g_normal_font,
        RGB(120, 170, 255)
    );


    /*
     * Last command status.
     */

    if (g_command_state == 1)
    {
        draw_text(
            hdc,
            45,
            112,
            L"Last command: ON",
            g_value_font,
            RGB(80, 220, 120)
        );
    }
    else if (g_command_state == 0)
    {
        draw_text(
            hdc,
            45,
            112,
            L"Last command: OFF",
            g_value_font,
            RGB(220, 120, 120)
        );
    }
    else
    {
        draw_text(
            hdc,
            45,
            112,
            L"No command sent yet",
            g_value_font,
            RGB(180, 180, 190)
        );
    }


    /*
     * Buttons are created as child controls.
     */


    /* -----------------------------------------------------
       FAN SPEEDS
       ----------------------------------------------------- */

    draw_panel(
        hdc,
        180,
        310
    );

    draw_text(
        hdc,
        45,
        195,
        L"FAN SPEED",
        g_normal_font,
        RGB(120, 170, 255)
    );


    format_number(
        buffer,
        128,
        g_data.fan1.data,
        L" RPM"
    );

    draw_label_value(
        hdc,
        235,
        L"Fan 1",
        g_data.fan1.success
            ? buffer
            : L"N/A"
    );


    format_number(
        buffer,
        128,
        g_data.fan2.data,
        L" RPM"
    );

    draw_label_value(
        hdc,
        275,
        L"Fan 2",
        g_data.fan2.success
            ? buffer
            : L"N/A"
    );


    /* -----------------------------------------------------
       TEMPERATURE
       ----------------------------------------------------- */

    draw_panel(
        hdc,
        325,
        425
    );

    draw_text(
        hdc,
        45,
        340,
        L"THERMAL",
        g_normal_font,
        RGB(120, 170, 255)
    );


    if (g_data.ir_temp.success &&
        g_data.ir_temp.data != 0)
    {
        format_number(
            buffer,
            128,
            g_data.ir_temp.data,
            L" C"
        );

        draw_label_value(
            hdc,
            380,
            L"IR / Thermal Sensor",
            buffer
        );
    }
    else
    {
        draw_label_value(
            hdc,
            380,
            L"IR / Thermal Sensor",
            L"N/A"
        );
    }


    /* -----------------------------------------------------
       Connection status
       ----------------------------------------------------- */

    draw_text(
        hdc,
        35,
        450,
        L"\x25CF  Lenovo GameZone WMI: CONNECTED",
        g_normal_font,
        RGB(80, 220, 120)
    );


    draw_text(
        hdc,
        410,
        450,
        L"Refresh: 1 second",
        g_normal_font,
        RGB(130, 130, 145)
    );
}


/* ---------------------------------------------------------
   Window procedure
   --------------------------------------------------------- */

static LRESULT CALLBACK window_proc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
        case WM_CREATE:
        {
            g_window = hwnd;


            /* -------------------------------------------------
               Fonts
               ------------------------------------------------- */

            g_title_font = CreateFontW(
                24,
                0,
                0,
                0,
                FW_BOLD,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_SWISS,
                L"Segoe UI"
            );


            g_normal_font = CreateFontW(
                15,
                0,
                0,
                0,
                FW_NORMAL,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_SWISS,
                L"Segoe UI"
            );


            g_value_font = CreateFontW(
                16,
                0,
                0,
                0,
                FW_BOLD,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_SWISS,
                L"Segoe UI"
            );


            g_button_font = CreateFontW(
                15,
                0,
                0,
                0,
                FW_BOLD,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_SWISS,
                L"Segoe UI"
            );


            /* -------------------------------------------------
               Extreme Cooling ON button
               ------------------------------------------------- */

            g_button_on = CreateWindowExW(
                0,
                L"BUTTON",
                L"EXTREME COOLING ON",
                WS_CHILD |
                WS_VISIBLE |
                WS_TABSTOP |
                BS_PUSHBUTTON,
                325,
                80,
                220,
                35,
                hwnd,
                (HMENU)(INT_PTR)BUTTON_COOLING_ON,
                (HINSTANCE)GetWindowLongPtrW(
                    hwnd,
                    GWLP_HINSTANCE
                ),
                NULL
            );


            /* -------------------------------------------------
               Extreme Cooling OFF button
               ------------------------------------------------- */

            g_button_off = CreateWindowExW(
                0,
                L"BUTTON",
                L"EXTREME COOLING OFF",
                WS_CHILD |
                WS_VISIBLE |
                WS_TABSTOP |
                BS_PUSHBUTTON,
                325,
                120,
                220,
                35,
                hwnd,
                (HMENU)(INT_PTR)BUTTON_COOLING_OFF,
                (HINSTANCE)GetWindowLongPtrW(
                    hwnd,
                    GWLP_HINSTANCE
                ),
                NULL
            );


            if (g_button_on)
            {
                SendMessageW(
                    g_button_on,
                    WM_SETFONT,
                    (WPARAM)g_button_font,
                    TRUE
                );
            }


            if (g_button_off)
            {
                SendMessageW(
                    g_button_off,
                    WM_SETFONT,
                    (WPARAM)g_button_font,
                    TRUE
                );
            }


            /* -------------------------------------------------
               Initial telemetry
               ------------------------------------------------- */

            update_data();


            /* -------------------------------------------------
               Refresh once per second
               ------------------------------------------------- */

            SetTimer(
                hwnd,
                TIMER_ID,
                TIMER_INTERVAL,
                NULL
            );

            return 0;
        }


        /* -----------------------------------------------------
           Button commands
           ----------------------------------------------------- */

        case WM_COMMAND:
        {
            int command = LOWORD(wParam);


            /* -------------------------------------------------
               Extreme Cooling ON
               ------------------------------------------------- */

            if (command == BUTTON_COOLING_ON)
            {
                if (GameZoneSetFanCooling(1))
                {
                    g_command_state = 1;

                    update_data();
                }
                else
                {
                    MessageBoxW(
                        hwnd,
                        L"Extreme Cooling could not be enabled.\n\n"
                        L"The Lenovo GameZone WMI method did not "
                        L"accept the command.",
                        L"Extreme Cooling",
                        MB_ICONERROR
                    );
                }

                return 0;
            }


            /* -------------------------------------------------
               Extreme Cooling OFF
               ------------------------------------------------- */

            if (command == BUTTON_COOLING_OFF)
            {
                if (GameZoneSetFanCooling(0))
                {
                    g_command_state = 0;

                    update_data();
                }
                else
                {
                    MessageBoxW(
                        hwnd,
                        L"Extreme Cooling could not be disabled.\n\n"
                        L"The Lenovo GameZone WMI method did not "
                        L"accept the command.",
                        L"Extreme Cooling",
                        MB_ICONERROR
                    );
                }

                return 0;
            }

            break;
        }


        /* -----------------------------------------------------
           Timer
           ----------------------------------------------------- */

        case WM_TIMER:

            if (wParam == TIMER_ID)
            {
                update_data();
            }

            return 0;


        /* -----------------------------------------------------
           Paint
           ----------------------------------------------------- */

        case WM_PAINT:
        {
            PAINTSTRUCT ps;

            HDC hdc = BeginPaint(
                hwnd,
                &ps
            );

            paint_gui(
                hwnd,
                hdc
            );

            EndPaint(
                hwnd,
                &ps
            );

            return 0;
        }


        /* -----------------------------------------------------
           Destroy
           ----------------------------------------------------- */

        case WM_DESTROY:

            KillTimer(
                hwnd,
                TIMER_ID
            );


            if (g_title_font)
                DeleteObject(
                    g_title_font
                );


            if (g_normal_font)
                DeleteObject(
                    g_normal_font
                );


            if (g_value_font)
                DeleteObject(
                    g_value_font
                );


            if (g_button_font)
                DeleteObject(
                    g_button_font
                );


            /*
             * The icon handles are destroyed after the message
             * loop exits, not here.
             */

            GameZoneShutdown();


            PostQuitMessage(0);

            return 0;
    }


    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam
    );
}


/* ---------------------------------------------------------
   WinMain
   --------------------------------------------------------- */

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE previous,
    PWSTR command_line,
    int show_command)
{
    (void)previous;
    (void)command_line;


    /* -----------------------------------------------------
       Initialize Lenovo GameZone WMI
       ----------------------------------------------------- */

    if (!GameZoneInitialize())
    {
        MessageBoxW(
            NULL,
            L"Could not initialize Lenovo GameZone WMI.",
            L"Legion Y720 Cooling Monitor",
            MB_ICONERROR
        );

        return 1;
    }


    const wchar_t CLASS_NAME[] =
        L"Y720CoolingControlWindow";


    /* -----------------------------------------------------
       Load embedded application icon
       ----------------------------------------------------- */

    /*
     * Load the large icon.
     *
     * The icon is embedded by:
     *
     * IDI_Y720COOLINGMONITOR ICON
     * "../resources/Y720CoolingMonitor.ico"
     */

    g_app_icon_big =
        (HICON)LoadImageW(
            instance,
            MAKEINTRESOURCEW(IDI_Y720COOLINGMONITOR),
            IMAGE_ICON,
            32,
            32,
            0
        );


    /*
     * Load the small icon separately.
     */

    g_app_icon_small =
        (HICON)LoadImageW(
            instance,
            MAKEINTRESOURCEW(IDI_Y720COOLINGMONITOR),
            IMAGE_ICON,
            16,
            16,
            0
        );


    /*
     * Both icon handles should exist.
     */

    if (!g_app_icon_big ||
        !g_app_icon_small)
    {
        MessageBoxW(
            NULL,
            L"Could not load the embedded application icon.",
            L"Legion Y720 Cooling Monitor",
            MB_ICONERROR
        );


        if (g_app_icon_big)
        {
            DestroyIcon(
                g_app_icon_big
            );

            g_app_icon_big = NULL;
        }


        if (g_app_icon_small)
        {
            DestroyIcon(
                g_app_icon_small
            );

            g_app_icon_small = NULL;
        }


        GameZoneShutdown();

        return 1;
    }


    /* -----------------------------------------------------
       Window class
       ----------------------------------------------------- */

    WNDCLASSW wc;

    ZeroMemory(
        &wc,
        sizeof(wc)
    );


    wc.style =
        CS_HREDRAW |
        CS_VREDRAW;


    wc.lpfnWndProc =
        window_proc;


    wc.hInstance =
        instance;


    wc.lpszClassName =
        CLASS_NAME;


    /*
     * Use the embedded application icon as the class icon.
     *
     * We intentionally do not use wc.hIconSm because the
     * MinGW headers used by this project do not expose that
     * member in WNDCLASSW.
     */

    wc.hIcon =
        g_app_icon_big;


    wc.hCursor =
        LoadCursor(
            NULL,
            IDC_ARROW
        );


    wc.hbrBackground =
        NULL;


    if (!RegisterClassW(&wc))
    {
        MessageBoxW(
            NULL,
            L"Could not register the application window class.",
            L"Legion Y720 Cooling Monitor",
            MB_ICONERROR
        );


        DestroyIcon(
            g_app_icon_big
        );

        DestroyIcon(
            g_app_icon_small
        );

        g_app_icon_big = NULL;
        g_app_icon_small = NULL;


        GameZoneShutdown();

        return 1;
    }


    /* -----------------------------------------------------
       Main window
       ----------------------------------------------------- */

    HWND hwnd =
        CreateWindowExW(
            0,
            CLASS_NAME,
            L"Legion Y720 Cooling Monitor",
            WS_OVERLAPPED |
            WS_CAPTION |
            WS_SYSMENU |
            WS_MINIMIZEBOX,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            625,
            515,
            NULL,
            NULL,
            instance,
            NULL
        );


    if (!hwnd)
    {
        MessageBoxW(
            NULL,
            L"Could not create the application window.",
            L"Legion Y720 Cooling Monitor",
            MB_ICONERROR
        );


        DestroyIcon(
            g_app_icon_big
        );

        DestroyIcon(
            g_app_icon_small
        );

        g_app_icon_big = NULL;
        g_app_icon_small = NULL;


        GameZoneShutdown();

        return 1;
    }


    /* -----------------------------------------------------
       Explicitly assign window icons
       ----------------------------------------------------- */

    /*
     * Large icon:
     *
     * Used by the window/taskbar in contexts where Windows
     * requests the large representation.
     */

    SendMessageW(
        hwnd,
        WM_SETICON,
        ICON_BIG,
        (LPARAM)g_app_icon_big
    );


    /*
     * Small icon:
     *
     * Used by the title bar and other small-icon contexts.
     */

    SendMessageW(
        hwnd,
        WM_SETICON,
        ICON_SMALL,
        (LPARAM)g_app_icon_small
    );


    /* -----------------------------------------------------
       Show GUI
       ----------------------------------------------------- */

    ShowWindow(
        hwnd,
        show_command
    );


    UpdateWindow(
        hwnd
    );


    /* -----------------------------------------------------
       Message loop
       ----------------------------------------------------- */

    MSG message;


    while (GetMessageW(
        &message,
        NULL,
        0,
        0) > 0)
    {
        TranslateMessage(
            &message
        );

        DispatchMessageW(
            &message
        );
    }


    /* -----------------------------------------------------
       Cleanup
       ----------------------------------------------------- */

    if (g_app_icon_big)
    {
        DestroyIcon(
            g_app_icon_big
        );

        g_app_icon_big = NULL;
    }


    if (g_app_icon_small)
    {
        DestroyIcon(
            g_app_icon_small
        );

        g_app_icon_small = NULL;
    }


    return (int)message.wParam;
}