#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <stdio.h>
#include <wchar.h>
#include <winreg.h>

#include "Y720CoolingController.h"
#include "theme.h"
#include "utils.h"

#pragma comment(lib, "Comctl32.lib")

#define TIMER_ID                1
#define TIMER_INTERVAL          1000
#define TRAY_RETRY_TIMER_ID     2
#define TRAY_RETRY_INTERVAL     3000

#define BUTTON_COOLING          1001
#define BUTTON_AUTO             1002

#define TRACKBAR_TRIGGER        1101
#define TRACKBAR_STOP           1102

#define LABEL_TRIGGER_VALUE     1201
#define LABEL_STOP_VALUE        1202
#define LABEL_AUTO_STATE        1203
#define CHECK_STARTUP           1204
#define BUTTON_UNINSTALL        1205
#define BUTTON_HELP             1206

#define WM_TRAYICON              (WM_APP + 1)
#define WM_SHOW_EXISTING         (WM_APP + 2)
#define TRAY_ICON_ID             5001
#define HOTKEY_EXTREME           5002
#define TRAY_TOGGLE_COOLING      5003
#define TRAY_TOGGLE_AUTO         5004
#define TRAY_SHOW                5005
#define TRAY_EXIT                5006

#define AUTO_TRIGGER_SECONDS    10
#define AUTO_STOP_SECONDS       10

#define MIN_TRIGGER_TEMP        45
#define MAX_TRIGGER_TEMP        100

#define MIN_STOP_TEMP           40
#define MAX_STOP_GAP            5

/* Temperature validation constants - these are duplicates of theme.h
 * to keep the GUI file self-contained for validation functions
 */
#define ABSOLUTE_MIN_TEMP       20
#define ABSOLUTE_MAX_TEMP       120

#define DEFAULT_TRIGGER_TEMP     70
#define DEFAULT_STOP_TEMP        65

#define CONFIG_DIRECTORY_NAME    L"LegionY720CoolingController"
#define CONFIG_FILE_NAME         L"settings.ini"
#define CONFIG_SECTION           L"Automatic Cooling"
#define CONFIG_TRIGGER_KEY       L"TriggerTemperature"
#define CONFIG_STOP_KEY          L"StopTemperature"
#define CONFIG_AUTO_KEY          L"AutomaticCooling"

static HWND g_window = NULL;
static HWND g_button_cooling = NULL;
static HWND g_button_auto = NULL;
static HWND g_button_uninstall = NULL;
static HWND g_button_help = NULL;
static HWND g_check_startup = NULL;

static HWND g_trackbar_trigger = NULL;
static HWND g_trackbar_stop = NULL;

static HWND g_label_trigger_value = NULL;
static HWND g_label_stop_value = NULL;
static HWND g_label_auto_state = NULL;

static GAMEZONE_DATA g_data;

static HFONT g_font_title = NULL;
static HFONT g_font_section = NULL;
static HFONT g_font_normal = NULL;
static HFONT g_font_value = NULL;
static HFONT g_font_button = NULL;
static HFONT g_font_status = NULL;
static HFONT g_font_status_bold = NULL;
static HFONT g_font_slider = NULL;

static HICON g_icon_large = NULL;
static HICON g_icon_small = NULL;
static NOTIFYICONDATAW g_tray_icon;
static BOOL g_tray_icon_added = FALSE;
static BOOL g_exiting = FALSE;
static UINT g_taskbar_created = 0;
static HANDLE g_instance_mutex = NULL;

#define WINDOW_CLASS_NAME		L"Y720CoolingControllerWindow"
#define INSTANCE_MUTEX_NAME      L"Local\\LegionY720CoolingController"

/*
 * Session state.
 *
 * g_command_state:
 *   -1 = no command sent by this application yet
 *    0 = last command sent by this application was OFF
 *    1 = last command sent by this application was ON
 */
static int g_command_state = -1;

/*
 * TRUE only when this application has successfully enabled
 * Extreme Cooling and has not subsequently disabled it.
 */
static BOOL g_extreme_cooling_enabled = FALSE;

/*
 * Automatic Cooling state.
 */
static BOOL g_auto_cooling_enabled = FALSE;
static BOOL g_auto_cooling_active = FALSE;

/*
 * Consecutive qualifying seconds. Positive values qualify for activation;
 * negative values qualify for deactivation.
 */
static int g_auto_condition_seconds = 0;

/*
 * User-configurable thresholds.
 */
static int g_trigger_temperature = DEFAULT_TRIGGER_TEMP;
static int g_stop_temperature = DEFAULT_STOP_TEMP;

/*
 * Polling state.
 */
static BOOL g_polling_active = FALSE;
static BOOL g_telemetry_available = FALSE;

#define STARTUP_TASK_NAME L"Legion Y720 Cooling Controller"
#define LEGACY_STARTUP_VALUE L"LegionY720CoolingController"
#define STARTUP_RUN_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"

static BOOL validate_temperature_threshold(
    int value,
    int minimum,
    int maximum
);

static BOOL get_config_path(
    wchar_t *path,
    size_t path_size
)
{
    wchar_t appdata[MAX_PATH];
    DWORD length;
    int written;

    if (!path || path_size == 0)
        return FALSE;

    length = GetEnvironmentVariableW(
        L"APPDATA",
        appdata,
        _countof(appdata)
    );

    if (!length || length >= _countof(appdata))
        return FALSE;

    written = _snwprintf(
        path,
        path_size,
        L"%s\\%s\\%s",
        appdata,
        CONFIG_DIRECTORY_NAME,
        CONFIG_FILE_NAME
    );

    if (written < 0 || written >= (int)path_size)
        return FALSE;

    return TRUE;
}


static BOOL ensure_config_directory(void)
{
    wchar_t appdata[MAX_PATH];
    wchar_t directory[MAX_PATH];
    DWORD length;
    int written;

    length = GetEnvironmentVariableW(
        L"APPDATA",
        appdata,
        _countof(appdata)
    );

    if (!length || length >= _countof(appdata))
        return FALSE;

    written = _snwprintf(
        directory,
        _countof(directory),
        L"%s\\%s",
        appdata,
        CONFIG_DIRECTORY_NAME
    );

    if (written < 0 || written >= (int)_countof(directory))
        return FALSE;

    if (CreateDirectoryW(directory, NULL))
        return TRUE;

    return GetLastError() == ERROR_ALREADY_EXISTS;
}


static void load_settings(void)
{
    wchar_t path[MAX_PATH];
    int trigger;
    int stop;
    int maximum_stop;
	int automatic_cooling;

    if (!get_config_path(path, _countof(path)))
        return;

    trigger = GetPrivateProfileIntW(
        CONFIG_SECTION,
        CONFIG_TRIGGER_KEY,
        DEFAULT_TRIGGER_TEMP,
        path
    );

    stop = GetPrivateProfileIntW(
        CONFIG_SECTION,
        CONFIG_STOP_KEY,
        DEFAULT_STOP_TEMP,
        path
    );
	
	automatic_cooling = GetPrivateProfileIntW(
		CONFIG_SECTION,
		CONFIG_AUTO_KEY,
		0,
		path
	);

    if (!validate_temperature_threshold(
            trigger,
            MIN_TRIGGER_TEMP,
            MAX_TRIGGER_TEMP))
    {
        trigger = DEFAULT_TRIGGER_TEMP;
    }

    if (!validate_temperature_threshold(
			stop,
			ABSOLUTE_MIN_TEMP,
			ABSOLUTE_MAX_TEMP))
	{
		stop = DEFAULT_STOP_TEMP;
	}

    maximum_stop = trigger - MAX_STOP_GAP;

    if (maximum_stop < MIN_STOP_TEMP)
        maximum_stop = MIN_STOP_TEMP;

    if (stop > maximum_stop)
        stop = maximum_stop;

    if (stop < MIN_STOP_TEMP)
        stop = MIN_STOP_TEMP;

    g_trigger_temperature = trigger;
    g_stop_temperature = stop;
	g_auto_cooling_enabled = automatic_cooling != 0 ? TRUE : FALSE;
}


static void save_settings(void)
{
    wchar_t path[MAX_PATH];
    wchar_t trigger[32];
    wchar_t stop[32];

    if (!ensure_config_directory())
        return;

    if (!get_config_path(path, _countof(path)))
        return;

    swprintf_s(
        trigger,
        _countof(trigger),
        L"%d",
        g_trigger_temperature
    );

    swprintf_s(
        stop,
        _countof(stop),
        L"%d",
        g_stop_temperature
    );

    WritePrivateProfileStringW(
        CONFIG_SECTION,
        CONFIG_TRIGGER_KEY,
        trigger,
        path
    );

    WritePrivateProfileStringW(
        CONFIG_SECTION,
        CONFIG_STOP_KEY,
        stop,
        path
    );
	
	WritePrivateProfileStringW(
		CONFIG_SECTION,
		CONFIG_AUTO_KEY,
		g_auto_cooling_enabled ? L"1" : L"0",
		path
	);
}

static void initialize_dpi_awareness(void)
{
    HMODULE user32;
    FARPROC procedure;
    BOOL (WINAPI *set_process_dpi_aware)(void);

    user32 = GetModuleHandleW(L"user32.dll");
    procedure = user32
        ? GetProcAddress(user32, "SetProcessDPIAware")
        : NULL;
    set_process_dpi_aware = NULL;

    if (procedure)
    {
        CopyMemory(
            &set_process_dpi_aware,
            &procedure,
            sizeof(set_process_dpi_aware)
        );
        if (set_process_dpi_aware)
            set_process_dpi_aware();
    }

}

static void center_window_on_active_monitor(HWND hwnd)
{
    POINT point;
    HMONITOR monitor;
    MONITORINFO info;
    RECT window_rect;
    int width;
    int height;
    int x;
    int y;

    if (!hwnd || !GetCursorPos(&point))
        return;

    monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    if (!monitor)
        return;

    ZeroMemory(&info, sizeof(info));
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info) ||
        !GetWindowRect(hwnd, &window_rect))
    {
        return;
    }

    width = window_rect.right - window_rect.left;
    height = window_rect.bottom - window_rect.top;
    x = info.rcWork.left +
        ((info.rcWork.right - info.rcWork.left) - width) / 2;
    y = info.rcWork.top +
        ((info.rcWork.bottom - info.rcWork.top) - height) / 2;

    SetWindowPos(
        hwnd,
        NULL,
        x,
        y,
        0,
        0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
    );
}

static void remove_legacy_startup_entry(void)
{
    HKEY key;

    if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            STARTUP_RUN_KEY,
            0,
            KEY_SET_VALUE,
            &key) == ERROR_SUCCESS)
    {
        RegDeleteValueW(key, LEGACY_STARTUP_VALUE);
        RegCloseKey(key);
    }
}

static void remove_configuration(void)
{
    wchar_t path[MAX_PATH];
    wchar_t directory[MAX_PATH];

    if (!get_config_path(path, _countof(path)))
        return;

    /*
     * Delete the configuration file.
     */
    DeleteFileW(path);

    /*
     * Remove the directory if it is now empty.
     *
     * ERROR_DIR_NOT_EMPTY simply means something else exists
     * there; that is harmless.
     */
    if (GetEnvironmentVariableW(
            L"APPDATA",
            directory,
            _countof(directory)) == 0)
    {
        return;
    }

    {
        wchar_t config_directory[MAX_PATH];
        int written;

        written = _snwprintf(
            config_directory,
            _countof(config_directory),
            L"%s\\%s",
            directory,
            CONFIG_DIRECTORY_NAME
        );

        if (written >= 0 &&
            written < (int)_countof(config_directory))
        {
            RemoveDirectoryW(config_directory);
        }
    }
}

static int run_schtasks(const wchar_t *arguments)
{
    wchar_t command_line[2048];
    wchar_t system_directory[MAX_PATH];
    STARTUPINFOW startup_info;
    PROCESS_INFORMATION process_info;
    DWORD exit_code;
    int written;
    UINT system_length;

    if (!arguments)
        return -1;

    system_length = GetSystemDirectoryW(
        system_directory,
        _countof(system_directory)
    );
    if (!system_length || system_length >= _countof(system_directory))
        return -1;

    written = _snwprintf(
        command_line,
        _countof(command_line),
        L"\"%s\\schtasks.exe\" %s",
        system_directory,
        arguments
    );
    if (written < 0 || written >= (int)_countof(command_line))
        return -1;

    ZeroMemory(&startup_info, sizeof(startup_info));
    ZeroMemory(&process_info, sizeof(process_info));
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESHOWWINDOW;
    startup_info.wShowWindow = SW_HIDE;

    if (!CreateProcessW(
            NULL,
            command_line,
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &startup_info,
            &process_info))
    {
        return -1;
    }

    WaitForSingleObject(process_info.hProcess, INFINITE);
    exit_code = 1;
    GetExitCodeProcess(process_info.hProcess, &exit_code);
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return (int)exit_code;
}

static BOOL startup_task_exists(void)
{
    wchar_t arguments[256];
    _snwprintf(
        arguments,
        _countof(arguments),
        L"/Query /TN \"%s\" /NH",
        STARTUP_TASK_NAME
    );
    return run_schtasks(arguments) == 0;
}

static BOOL set_startup_enabled(BOOL enabled)
{
    wchar_t executable[MAX_PATH];
    wchar_t arguments[2048];
    DWORD length;
    int written;
    int result;

    if (!enabled)
    {
        remove_legacy_startup_entry();

        if (!startup_task_exists())
            return TRUE;

        written = _snwprintf(
            arguments,
            _countof(arguments),
            L"/Delete /TN \"%s\" /F",
            STARTUP_TASK_NAME
        );
        if (written < 0 || written >= (int)_countof(arguments))
            return FALSE;

        return run_schtasks(arguments) == 0;
    }

    length = GetModuleFileNameW(NULL, executable, _countof(executable));
    if (!length || length >= _countof(executable))
        return FALSE;

    written = _snwprintf(
        arguments,
        _countof(arguments),
        L"/Create /TN \"%s\" /TR \"\\\"%s\\\" --startup\" /SC ONLOGON /RL HIGHEST /IT /F",
        STARTUP_TASK_NAME,
        executable
    );
    if (written < 0 || written >= (int)_countof(arguments))
        return FALSE;

    result = run_schtasks(arguments);
    if (result != 0)
        return FALSE;

    remove_legacy_startup_entry();
    return TRUE;
}

static BOOL is_startup_enabled(void)
{
    return startup_task_exists();
}

static BOOL schedule_uninstall(void)
{
    wchar_t executable[MAX_PATH];
    DWORD length;

    length = GetModuleFileNameW(NULL, executable, _countof(executable));
    if (!length || length >= _countof(executable))
        return FALSE;

    if (!MoveFileExW(
        executable,
        NULL,
        MOVEFILE_DELAY_UNTIL_REBOOT
    ))
        return FALSE;

    return set_startup_enabled(FALSE);
}

static BOOL add_tray_icon(HWND hwnd)
{
    ZeroMemory(&g_tray_icon, sizeof(g_tray_icon));
    g_tray_icon.cbSize = sizeof(g_tray_icon);
    g_tray_icon.hWnd = hwnd;
    g_tray_icon.uID = TRAY_ICON_ID;
    g_tray_icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray_icon.uCallbackMessage = WM_TRAYICON;
    g_tray_icon.hIcon = g_icon_small;
    lstrcpyW(g_tray_icon.szTip, L"Legion Y720 Cooling Controller");

    if (!g_tray_icon.hIcon ||
        !Shell_NotifyIconW(NIM_ADD, &g_tray_icon))
    {
        g_tray_icon_added = FALSE;
        return FALSE;
    }

    g_tray_icon.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_tray_icon);
    g_tray_icon_added = TRUE;
    return TRUE;
}

static void remove_tray_icon(void)
{
    if (g_tray_icon_added)
    {
        Shell_NotifyIconW(NIM_DELETE, &g_tray_icon);
        g_tray_icon_added = FALSE;
    }
}

static void show_main_window(HWND hwnd)
{
    ShowWindow(hwnd, SW_SHOW);
    ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
}

static void hide_to_tray(HWND hwnd)
{
    ShowWindow(hwnd, SW_HIDE);
}

static void signal_existing_instance(void)
{
    HWND existing_window;

    existing_window = FindWindowW(WINDOW_CLASS_NAME, NULL);
    if (existing_window)
        PostMessageW(existing_window, WM_SHOW_EXISTING, 0, 0);
}

static void show_tray_menu(HWND hwnd)
{
    HMENU menu;
    POINT point;
    UINT cooling_flags;
    UINT auto_flags;

    menu = CreatePopupMenu();
    if (!menu || !GetCursorPos(&point))
    {
        if (menu)
            DestroyMenu(menu);
        return;
    }

    cooling_flags = MF_STRING;
    if (g_extreme_cooling_enabled)
        cooling_flags |= MF_CHECKED;
    auto_flags = MF_STRING;
    if (g_auto_cooling_enabled)
        auto_flags |= MF_CHECKED;

    AppendMenuW(menu, cooling_flags, TRAY_TOGGLE_COOLING,
        g_extreme_cooling_enabled ? L"Turn Extreme Cooling Off" :
                                     L"Turn Extreme Cooling On");
    AppendMenuW(menu, auto_flags, TRAY_TOGGLE_AUTO,
        g_auto_cooling_enabled ? L"Turn Automatic Cooling Off" :
                                 L"Turn Automatic Cooling On");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, TRAY_SHOW, L"Open Cooling Controller");
    AppendMenuW(menu, MF_STRING, TRAY_EXIT, L"Exit");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        point.x,
        point.y,
        0,
        hwnd,
        NULL
    );
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

static void invalidate_region(int left, int top, int right, int bottom)
{
    RECT rect;

    if (!g_window)
        return;

    rect.left = left;
    rect.top = top;
    rect.right = right;
    rect.bottom = bottom;
    InvalidateRect(g_window, &rect, FALSE);
}

static void invalidate_telemetry_regions(void)
{
    invalidate_region(25, 82, 600, 180);
    invalidate_region(25, 195, 300, 325);
    invalidate_region(325, 195, 600, 325);
    invalidate_region(25, 520, 600, 650);
}

static void draw_action_button(const DRAWITEMSTRUCT *draw_item)
{
    COLORREF background;
    HBRUSH brush;
    HPEN pen;
    HPEN old_pen;
    HBRUSH old_brush;
    wchar_t text[128];
    RECT rect;

    if (!draw_item)
        return;

    background = COLOR_BUTTON_BG;
    if (draw_item->hwndItem == g_button_cooling && g_extreme_cooling_enabled)
        background = COLOR_BUTTON_ACTIVE;
    else if (draw_item->hwndItem == g_button_auto && g_auto_cooling_enabled)
        background = COLOR_BUTTON_AUTO;

    brush = gdi_cache_get_brush(background);
    pen = gdi_cache_get_pen(COLOR_SECONDARY_ACCENT, 1);
    if (!brush || !pen)
        return;

    rect = draw_item->rcItem;
    old_brush = (HBRUSH)SelectObject(draw_item->hDC, brush);
    old_pen = (HPEN)SelectObject(draw_item->hDC, pen);
    RoundRect(draw_item->hDC, rect.left, rect.top, rect.right, rect.bottom, 8, 8);
    SelectObject(draw_item->hDC, old_pen);
    SelectObject(draw_item->hDC, old_brush);

    GetWindowTextW(draw_item->hwndItem, text, _countof(text));
    SetBkMode(draw_item->hDC, TRANSPARENT);
    SetTextColor(draw_item->hDC, RGB(240, 245, 255));
    DrawTextW(draw_item->hDC, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (draw_item->itemState & ODS_FOCUS)
    {
        InflateRect(&rect, -4, -4);
        DrawFocusRect(draw_item->hDC, &rect);
    }
}


/* ------------------------------------------------------------------------- */
/* Polling                                                                   */
/* ------------------------------------------------------------------------- */

static void update_data(BOOL lightweight);


static void start_polling(void)
{
    if (!g_window)
        return;

    if (!g_polling_active)
    {
        SetTimer(
            g_window,
            TIMER_ID,
            TIMER_INTERVAL,
            NULL
        );

        g_polling_active = TRUE;
    }

    update_data(FALSE);
}


static void stop_polling(void)
{
    if (!g_window)
        return;

    KillTimer(
        g_window,
        TIMER_ID
    );

    g_polling_active = FALSE;

    invalidate_region(25, 520, 600, 650);
}


/* ------------------------------------------------------------------------- */
/* Extreme Cooling                                                           */
/* ------------------------------------------------------------------------- */

static BOOL set_extreme_cooling(BOOL enable)
{
    if (enable)
    {
        if (g_extreme_cooling_enabled)
            return TRUE;

        if (!GameZoneSetFanCooling(1))
            return FALSE;

        g_extreme_cooling_enabled = TRUE;
        g_command_state = 1;

        return TRUE;
    }
    else
    {
        if (!g_extreme_cooling_enabled)
        {
            /*
             * We have not enabled cooling in this application session.
             * Do not send an unnecessary OFF command.
             */
            g_command_state = 0;
            return TRUE;
        }

        if (!GameZoneSetFanCooling(0))
            return FALSE;

        g_extreme_cooling_enabled = FALSE;
        g_command_state = 0;

        return TRUE;
    }
}


static void disable_cooling_before_exit(void)
{
    BOOL disabled;

    /*
     * Safety shutdown happens before WMI is released.
     *
     * Only disable cooling if this application knows that it
     * enabled it during the current session.
     */
    if (!g_extreme_cooling_enabled)
        return;

    /*
     * Give WMI more than one opportunity to accept the shutdown
     * command because application exit is a safety boundary.
     */
    disabled = GameZoneSetFanCooling(0);

    if (!disabled)
    {
        Sleep(50);
        disabled = GameZoneSetFanCooling(0);
    }

    if (!disabled)
    {
        /*
         * There is no useful action left once WMI is about to be
         * released, but do not falsely record the cooling state
         * as successfully disabled.
         */
        OutputDebugStringW(
            L"Legion Y720 Cooling Controller: "
            L"Extreme Cooling shutdown failed during exit.\n"
        );

        return;
    }

    g_extreme_cooling_enabled = FALSE;
    g_auto_cooling_active = FALSE;
    g_command_state = 0;
}


/* ------------------------------------------------------------------------- */
/* Automatic Cooling                                                         */
/* ------------------------------------------------------------------------- */

static void reset_auto_counters(void)
{
    g_auto_condition_seconds = 0;
}


static void update_auto_button(void)
{
    if (!g_button_auto)
        return;

    if (g_auto_cooling_enabled)
    {
        SetWindowTextW(
            g_button_auto,
            L"DISABLE AUTO COOLING"
        );
    }
    else
    {
        SetWindowTextW(
            g_button_auto,
            L"ENABLE AUTO COOLING"
        );
    }
}


static void update_cooling_button(void)
{
    if (!g_button_cooling)
        return;

    if (g_extreme_cooling_enabled)
    {
        SetWindowTextW(
            g_button_cooling,
            L"DISABLE EXTREME COOLING"
        );
    }
    else
    {
        SetWindowTextW(
            g_button_cooling,
            L"ENABLE EXTREME COOLING"
        );
    }
}


static void update_auto_state_label(void)
{
    wchar_t text[128];

    if (!g_label_auto_state)
        return;

    if (!g_auto_cooling_enabled)
    {
        SetWindowTextW(
            g_label_auto_state,
            L"Automatic Cooling: OFF"
        );

        return;
    }

    if (g_auto_cooling_active)
    {
        /*
         * A negative counter means the stop condition is currently
         * being persisted.
         */
        if (g_auto_condition_seconds < 0)
        {
            swprintf_s(
                text,
                _countof(text),
                L"Automatic Cooling: STOPPING (%d/%d s)",
                -g_auto_condition_seconds,
                AUTO_STOP_SECONDS
            );

            SetWindowTextW(
                g_label_auto_state,
                text
            );

            return;
        }

        SetWindowTextW(
            g_label_auto_state,
            L"Automatic Cooling: ACTIVE"
        );

        return;
    }

    if (g_auto_condition_seconds > 0)
    {
        swprintf_s(
            text,
            _countof(text),
            L"Automatic Cooling: WAITING (%d/%d s)",
            g_auto_condition_seconds,
            AUTO_TRIGGER_SECONDS
        );

        SetWindowTextW(
            g_label_auto_state,
            text
        );

        return;
    }

    SetWindowTextW(
        g_label_auto_state,
        L"Automatic Cooling: MONITORING"
    );
}


static void update_threshold_labels(void)
{
    wchar_t buffer[32];

    if (g_label_trigger_value)
    {
        swprintf_s(
            buffer,
            _countof(buffer),
            L"%d °C",
            g_trigger_temperature
        );

        SetWindowTextW(
            g_label_trigger_value,
            buffer
        );
    }

    if (g_label_stop_value)
    {
        swprintf_s(
            buffer,
            _countof(buffer),
            L"%d °C",
            g_stop_temperature
        );

        SetWindowTextW(
            g_label_stop_value,
            buffer
        );
    }
}


static BOOL validate_temperature_threshold(int temp, int min, int max)
{
    if (temp < min || temp > max)
        return FALSE;
    return TRUE;
}


static void enforce_threshold_relationship(void)
{
    int maximum_stop;

    /*
     * Stop threshold must remain at least 5 °C below the trigger.
     *
     * The Stop trackbar's actual maximum is also changed so that
     * the user cannot select an invalid value.
     */
    maximum_stop = g_trigger_temperature - MAX_STOP_GAP;

    if (maximum_stop < MIN_STOP_TEMP)
        maximum_stop = MIN_STOP_TEMP;

    /*
     * If the current Stop value is now above the new maximum,
     * lower it automatically.
     */
    if (g_stop_temperature > maximum_stop)
        g_stop_temperature = maximum_stop;

    if (g_trackbar_stop)
    {
        /*
         * Dynamically change the actual Stop slider range.
         */
        SendMessageW(
            g_trackbar_stop,
            TBM_SETRANGE,
            TRUE,
            MAKELONG(
                MIN_STOP_TEMP,
                maximum_stop
            )
        );

        /*
         * Keep the slider position synchronized with the
         * validated Stop temperature.
         */
        SendMessageW(
            g_trackbar_stop,
            TBM_SETPOS,
            TRUE,
            g_stop_temperature
        );
    }

    update_threshold_labels();

}


static void automatic_cooling_tick(void)
{
    LONG ir_temp;

    if (!g_auto_cooling_enabled)
        return;

    /*
     * GetIRTemp is the only temperature source known to work reliably
     * on this Y720.
     */
    if (!g_data.ir_temp.success)
    {
        reset_auto_counters();
        update_auto_state_label();
        return;
    }

    ir_temp = g_data.ir_temp.data;

    if (!g_auto_cooling_active)
    {
        /*
         * HIGH-temperature condition:
         * must remain at/above trigger threshold for 10 seconds.
         */
        if (ir_temp >= g_trigger_temperature)
        {
            if (g_auto_condition_seconds < AUTO_TRIGGER_SECONDS)
                g_auto_condition_seconds++;
        }
        else
        {
            g_auto_condition_seconds = 0;
        }

        if (g_auto_condition_seconds >= AUTO_TRIGGER_SECONDS)
        {
            if (set_extreme_cooling(TRUE))
            {
                g_auto_cooling_active = TRUE;
                g_auto_condition_seconds = 0;
            }
        }
    }
    else
    {
        /*
         * LOW-temperature condition:
         * must remain at/below stop threshold for 10 seconds.
         */
        if (ir_temp <= g_stop_temperature)
        {
            if (g_auto_condition_seconds > -AUTO_STOP_SECONDS)
                g_auto_condition_seconds--;
        }
        else
        {
            g_auto_condition_seconds = 0;
        }

        if (g_auto_condition_seconds <= -AUTO_STOP_SECONDS)
        {
            if (set_extreme_cooling(FALSE))
            {
                /*
                 * Restore only the user's Auto preference. Active cooling is
                 * always a fresh session state and must be reacquired by Auto.
                 */
                g_auto_cooling_active = FALSE;
                g_auto_condition_seconds = 0;
            }
        }
    }

    update_auto_state_label();
    update_cooling_button();
}


/*
 * Automatic Cooling is deliberately a session mode.
 *
 * Enabling Auto while manual Extreme Cooling is active first turns
 * Extreme Cooling off. From that point onward the automatic state
 * machine owns the cooling command.
 */
static BOOL enable_automatic_cooling(void)
{
    if (g_auto_cooling_enabled)
        return TRUE;

    if (g_extreme_cooling_enabled)
    {
        if (!set_extreme_cooling(FALSE))
            return FALSE;
    }

    g_auto_cooling_enabled = TRUE;
    g_auto_cooling_active = FALSE;

    reset_auto_counters();

    update_auto_button();
    update_auto_state_label();
	
	save_settings();
	

    /*
     * Automatic mode must continue monitoring regardless of foreground
     * state or minimization, so ensure its timer is running.
     */
    start_polling();

    return TRUE;
}


static void disable_automatic_cooling(BOOL turn_off_cooling)
{
    g_auto_cooling_active = FALSE;

    reset_auto_counters();

    /*
     * When disabling Auto normally, active cooling is also stopped.
     *
     * Manual control can instead use this function as an ownership
     * transfer. In that case, leave the current cooling state untouched
     * so an active automatic session can become manual control without
     * an unnecessary OFF -> ON hardware transition.
     */
    if (turn_off_cooling && g_extreme_cooling_enabled)
        set_extreme_cooling(FALSE);

    g_auto_cooling_enabled = FALSE;
    save_settings();

    update_auto_button();
    update_auto_state_label();
    update_cooling_button();
}


/* ------------------------------------------------------------------------- */
/* Telemetry                                                                 */
/* ------------------------------------------------------------------------- */

static void update_data(BOOL lightweight)
{
    ZeroMemory(
        &g_data,
        sizeof(g_data)
    );

    if (lightweight)
    {
        g_telemetry_available =
            GameZoneReadIRTemperature(&g_data.ir_temp) != 0;
    }
    else
    {
        g_telemetry_available = GameZoneRead(&g_data) != 0;
    }

    if (g_telemetry_available)
    {
        automatic_cooling_tick();
    }

    invalidate_telemetry_regions();
}


/* ------------------------------------------------------------------------- */
/* Painting                                                                  */
/* ------------------------------------------------------------------------- */

static void paint_gui(
    HDC hdc)
{
    RECT client;
    RECT cooling_panel;
    RECT fan_panel;
    RECT thermal_panel;
    RECT auto_panel;
    RECT status_panel;

    wchar_t buffer[64];

    HBRUSH background_brush;

    GetClientRect(
        g_window,
        &client
    );

    /*
     * Background.
     */
    background_brush = gdi_cache_get_brush(COLOR_THEME_BG);

    if (background_brush)
        FillRect(hdc, &client, background_brush);

    /*
     * Header.
     */
    draw_text(
        hdc,
        g_font_title,
        25,
        20,
        L"Legion Y720 Cooling Controller",
        COLOR_TEXT
    );

    draw_text(
        hdc,
        g_font_normal,
        27,
        50,
        L"Native Lenovo GameZone cooling control",
        COLOR_TEXT_DIM
    );

    /*
     * Extreme Cooling panel.
     */
    cooling_panel.left = 25;
    cooling_panel.top = 82;
    cooling_panel.right = 600;
    cooling_panel.bottom = 180;

    draw_panel(
        hdc,
        cooling_panel,
        COLOR_PANEL_BG,
        COLOR_PANEL_BORDER,
        PANEL_CORNER_RADIUS
    );

    draw_panel_accent(
        hdc,
        cooling_panel,
        COLOR_PRIMARY_ACCENT,
        PANEL_ACCENT_WIDTH
    );

    draw_text(
        hdc,
        g_font_section,
        45,
        99,
        L"Extreme Cooling",
        COLOR_TEXT
    );

    if (g_extreme_cooling_enabled)
    {
        draw_text(
            hdc,
            g_font_value,
            45,
            125,
            L"● ACTIVE",
            COLOR_SUCCESS
        );

        draw_text(
            hdc,
            g_font_normal,
            45,
            148,
            g_auto_cooling_active
                ? L"Enabled automatically by temperature control."
                : L"Enabled manually by this application.",
            COLOR_TEXT_DIM
        );
    }
    else
    {
        draw_text(
            hdc,
            g_font_value,
            45,
            125,
            L"● OFF",
            COLOR_TEXT_DIM
        );

        if (g_auto_cooling_enabled)
        {
            draw_text(
                hdc,
                g_font_normal,
                45,
                148,
                L"Automatic Cooling is monitoring temperature.",
                COLOR_TEXT_DIM
            );
        }
        else if (g_command_state == -1)
        {
            draw_text(
                hdc,
                g_font_normal,
                45,
                148,
                L"No cooling command has been sent by this application.",
                COLOR_TEXT_DIM
            );
        }
        else
        {
            draw_text(
                hdc,
                g_font_normal,
                45,
                148,
                L"Extreme Cooling is disabled by this application.",
                COLOR_TEXT_DIM
            );
        }
    }

    /*
     * Manual button.
     */
    /*
     * Fan panel.
     */
    fan_panel.left = 25;
    fan_panel.top = 195;
    fan_panel.right = 300;
    fan_panel.bottom = 325;

    draw_panel(
        hdc,
        fan_panel,
        COLOR_PANEL_BG,
        COLOR_PANEL_BORDER,
        PANEL_CORNER_RADIUS
    );

    draw_text(
        hdc,
        g_font_section,
        45,
        199,
        L"Fan Speed",
        COLOR_TEXT
    );

    format_number(
        buffer,
        _countof(buffer),
        g_data.fan1.data,
        L" RPM"
    );

    draw_label_value(
        hdc,
        g_font_normal,
        g_font_value,
        45,
        238,
        175,
        L"Fan 1",
        g_data.fan1.success ? buffer : L"--",
        COLOR_TEXT
    );

    format_number(
        buffer,
        _countof(buffer),
        g_data.fan2.data,
        L" RPM"
    );

    draw_label_value(
        hdc,
        g_font_normal,
        g_font_value,
        45,
        278,
        175,
        L"Fan 2",
        g_data.fan2.success ? buffer : L"--",
        COLOR_TEXT
    );

    /*
     * Thermal panel.
     */
    thermal_panel.left = 325;
    thermal_panel.top = 195;
    thermal_panel.right = 600;
    thermal_panel.bottom = 325;

    draw_panel(
        hdc,
        thermal_panel,
        COLOR_PANEL_BG,
        COLOR_PANEL_BORDER,
        PANEL_CORNER_RADIUS
    );

    draw_text(
        hdc,
        g_font_section,
        345,
        199,
        L"Thermal",
        COLOR_TEXT
    );

    format_number(
        buffer,
        _countof(buffer),
        g_data.ir_temp.data,
        L" °C"
    );

    draw_label_value(
        hdc,
        g_font_normal,
        g_font_value,
        345,
        250,
        490,
        L"IR Sensor",
        g_data.ir_temp.success ? buffer : L"--",
        COLOR_TEXT
    );

    /*
     * Automatic Cooling panel.
     */
    auto_panel.left = 25;
    auto_panel.top = 340;
    auto_panel.right = 600;
    auto_panel.bottom = 505;

    draw_panel(
        hdc,
        auto_panel,
        COLOR_PANEL_BG,
        COLOR_PANEL_BORDER,
        PANEL_CORNER_RADIUS
    );

    draw_text(
        hdc,
        g_font_section,
        45,
        355,
        L"Automatic Cooling",
        COLOR_TEXT
    );

    /*
     * Trigger temperature.
     */
    draw_text(
        hdc,
        g_font_normal,
        45,
        390,
        L"Trigger",
        COLOR_TEXT
    );

    /*
     * Stop temperature.
     */
    draw_text(
        hdc,
        g_font_normal,
        45,
        430,
        L"Stop",
        COLOR_TEXT
    );

    /*
     * Slider controls are children of the main window.
     */
    /*
     * State text.
     */
    /*
     * Automatic button.
     */
    /*
     * Status panel.
     */
    status_panel.left = 25;
    status_panel.top = 520;
    status_panel.right = 600;
    status_panel.bottom = 650;

    draw_panel(
        hdc,
        status_panel,
        COLOR_PANEL_BG,
        COLOR_PANEL_BORDER,
        PANEL_CORNER_RADIUS
    );

    draw_text(
        hdc,
        g_font_status_bold,
        45,
        530,
        L"STATUS",
        COLOR_TEXT
    );

    if (g_polling_active && g_telemetry_available)
    {
        draw_text(
            hdc,
            g_font_status_bold,
            45,
            558,
            L"● CONNECTED",
            COLOR_SUCCESS
        );

        if (g_auto_cooling_enabled)
        {
            draw_text(
                hdc,
                g_font_status,
                390,
                558,
                L"Auto monitoring: 1 second",
                COLOR_TEXT_DIM
            );
        }
        else
        {
            draw_text(
                hdc,
                g_font_status,
                390,
                558,
                L"Refresh: 1 second",
                COLOR_TEXT_DIM
            );
        }
    }
    else if (g_polling_active)
    {
        draw_text(
            hdc,
            g_font_status_bold,
            45,
            558,
            L"● WMI ERROR",
            COLOR_DANGER
        );

        draw_text(
            hdc,
            g_font_status,
            390,
            558,
            L"Telemetry unavailable",
            COLOR_TEXT_DIM
        );
    }
    else
    {
        draw_text(
            hdc,
            g_font_status_bold,
            45,
            558,
            L"● SUSPENDED",
            COLOR_TEXT_DIM
        );

        draw_text(
            hdc,
            g_font_status,
            390,
            558,
            L"Refresh: SUSPENDED",
            COLOR_TEXT_DIM
        );
    }

    if (g_auto_cooling_enabled)
    {
        draw_text(
            hdc,
            g_font_status,
            45,
            586,
            g_auto_cooling_active
                ? L"Automatic Cooling is currently controlling Extreme Cooling."
                : L"Automatic Cooling is waiting for the trigger condition.",
            COLOR_TEXT_DIM
        );
    }
    else
    {
        draw_text(
            hdc,
            g_font_status,
            45,
            586,
            L"Automatic Cooling is disabled.",
            COLOR_TEXT_DIM
        );
    }

}


/* ------------------------------------------------------------------------- */
/* Control creation                                                          */
/* ------------------------------------------------------------------------- */

static void create_controls(HWND hwnd)
{
    g_button_cooling = CreateWindowExW(
        0,
        L"BUTTON",
        L"ENABLE EXTREME COOLING",
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_PUSHBUTTON |
        BS_OWNERDRAW,
        355,
        102,
        220,
        42,
        hwnd,
        (HMENU)BUTTON_COOLING,
        GetModuleHandleW(NULL),
        NULL
    );

    g_button_auto = CreateWindowExW(
        0,
        L"BUTTON",
        L"ENABLE AUTO COOLING",
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_PUSHBUTTON |
        BS_OWNERDRAW,
        355,
        467,
        220,
        30,
        hwnd,
        (HMENU)BUTTON_AUTO,
        GetModuleHandleW(NULL),
        NULL
    );

    /*
     * Trigger temperature slider.
     */
    g_trackbar_trigger = CreateWindowExW(
        0,
        TRACKBAR_CLASSW,
        L"",
        WS_CHILD |
        WS_VISIBLE |
        TBS_AUTOTICKS |
        TBS_HORZ,
        105,
        390,
        380,
        25,
        hwnd,
        (HMENU)TRACKBAR_TRIGGER,
        GetModuleHandleW(NULL),
        NULL
    );
    SetWindowTheme(g_trackbar_trigger, L"Explorer", NULL);

    SendMessageW(
        g_trackbar_trigger,
        TBM_SETRANGE,
        TRUE,
        MAKELONG(
            MIN_TRIGGER_TEMP,
            MAX_TRIGGER_TEMP
        )
    );

    SendMessageW(
        g_trackbar_trigger,
        TBM_SETTICFREQ,
        5,
        0
    );

    SendMessageW(
        g_trackbar_trigger,
        TBM_SETPOS,
        TRUE,
        g_trigger_temperature
    );

    /*
     * Stop temperature slider.
     */
    g_trackbar_stop = CreateWindowExW(
        0,
        TRACKBAR_CLASSW,
        L"",
        WS_CHILD |
        WS_VISIBLE |
        TBS_AUTOTICKS |
        TBS_HORZ,
        105,
        430,
        380,
        25,
        hwnd,
        (HMENU)TRACKBAR_STOP,
        GetModuleHandleW(NULL),
        NULL
    );
    SetWindowTheme(g_trackbar_stop, L"Explorer", NULL);

    SendMessageW(
		g_trackbar_stop,
		TBM_SETRANGE,
		TRUE,
		MAKELONG(
			MIN_STOP_TEMP,
			g_trigger_temperature - MAX_STOP_GAP
		)
	);

    SendMessageW(
        g_trackbar_stop,
        TBM_SETTICFREQ,
        5,
        0
    );

    SendMessageW(
        g_trackbar_stop,
        TBM_SETPOS,
        TRUE,
        g_stop_temperature
    );

    /*
     * Temperature value labels.
     */
    g_label_trigger_value = CreateWindowExW(
        0,
        L"STATIC",
        L"70 °C",
        WS_CHILD |
        WS_VISIBLE |
        SS_RIGHT,
        500,
        393,
        70,
        22,
        hwnd,
        (HMENU)LABEL_TRIGGER_VALUE,
        GetModuleHandleW(NULL),
        NULL
    );

    g_label_stop_value = CreateWindowExW(
        0,
        L"STATIC",
        L"65 °C",
        WS_CHILD |
        WS_VISIBLE |
        SS_RIGHT,
        500,
        433,
        70,
        22,
        hwnd,
        (HMENU)LABEL_STOP_VALUE,
        GetModuleHandleW(NULL),
        NULL
    );

    g_label_auto_state = CreateWindowExW(
        0,
        L"STATIC",
        L"Automatic Cooling: OFF",
        WS_CHILD |
        WS_VISIBLE |
        SS_LEFT,
        45,
        468,
        300,
        22,
        hwnd,
        (HMENU)LABEL_AUTO_STATE,
        GetModuleHandleW(NULL),
        NULL
    );

    g_check_startup = CreateWindowExW(
        0,
        L"BUTTON",
        L"Start with Windows",
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_AUTOCHECKBOX,
        45,
        615,
        180,
        24,
        hwnd,
        (HMENU)CHECK_STARTUP,
        GetModuleHandleW(NULL),
        NULL
    );

    g_button_uninstall = CreateWindowExW(
        0,
        L"BUTTON",
        L"UNINSTALL",
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_PUSHBUTTON |
        BS_OWNERDRAW,
        465,
        610,
        110,
        30,
        hwnd,
        (HMENU)BUTTON_UNINSTALL,
        GetModuleHandleW(NULL),
        NULL
    );

    g_button_help = CreateWindowExW(
        0,
        L"BUTTON",
        L"HELP",
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_PUSHBUTTON |
        BS_OWNERDRAW,
        335,
        610,
        110,
        30,
        hwnd,
        (HMENU)BUTTON_HELP,
        GetModuleHandleW(NULL),
        NULL
    );

    /*
     * Fonts for child controls.
     */
    SendMessageW(
        g_button_cooling,
        WM_SETFONT,
        (WPARAM)g_font_button,
        TRUE
    );

    SendMessageW(
        g_button_auto,
        WM_SETFONT,
        (WPARAM)g_font_button,
        TRUE
    );

    SendMessageW(
        g_label_trigger_value,
        WM_SETFONT,
        (WPARAM)g_font_slider,
        TRUE
    );

    SendMessageW(
        g_label_stop_value,
        WM_SETFONT,
        (WPARAM)g_font_slider,
        TRUE
    );

    SendMessageW(
        g_label_auto_state,
        WM_SETFONT,
        (WPARAM)g_font_slider,
        TRUE
    );

    SendMessageW(
        g_check_startup,
        WM_SETFONT,
        (WPARAM)g_font_slider,
        TRUE
    );

    SendMessageW(
        g_button_uninstall,
        WM_SETFONT,
        (WPARAM)g_font_slider,
        TRUE
    );
	
	SendMessageW(
		g_button_help,
		WM_SETFONT,
		(WPARAM)g_font_button,
		TRUE
	);

    SendMessageW(
        g_check_startup,
        BM_SETCHECK,
        is_startup_enabled() ? BST_CHECKED : BST_UNCHECKED,
        0
    );
}


/* ------------------------------------------------------------------------- */
/* Window procedure                                                           */
/* ------------------------------------------------------------------------- */

static LRESULT CALLBACK window_proc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    if (g_taskbar_created && message == g_taskbar_created)
    {
        g_tray_icon_added = FALSE;
        add_tray_icon(hwnd);
        return 0;
    }

    switch (message)
    {
        case WM_SHOW_EXISTING:
        {
            show_main_window(hwnd);
            return 0;
        }

        case WM_CREATE:
        {
            NONCLIENTMETRICSW metrics;

            gdi_cache_init();

            ZeroMemory(
                &metrics,
                sizeof(metrics)
            );

            metrics.cbSize = sizeof(metrics);

            SystemParametersInfoW(
                SPI_GETNONCLIENTMETRICS,
                sizeof(metrics),
                &metrics,
                0
            );

            g_font_title = CreateFontW(
                FONT_SIZE_TITLE,
                0,
                0,
                0,
                FONT_WEIGHT_BOLD,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                L"Segoe UI"
            );

            g_font_section = CreateFontW(
                FONT_SIZE_SECTION,
                0,
                0,
                0,
                FONT_WEIGHT_SEMIBOLD,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                L"Segoe UI"
            );

            g_font_normal = CreateFontW(
                FONT_SIZE_NORMAL,
                0,
                0,
                0,
                FONT_WEIGHT_NORMAL,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                L"Segoe UI"
            );

            g_font_value = CreateFontW(
                FONT_SIZE_VALUE,
                0,
                0,
                0,
                FONT_WEIGHT_SEMIBOLD,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                L"Segoe UI"
            );

            g_font_button = CreateFontW(
                FONT_SIZE_BUTTON,
                0,
                0,
                0,
                FONT_WEIGHT_SEMIBOLD,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                L"Segoe UI"
            );

            /*
             * Larger/bolder status fonts requested for the bottom area.
             */
            g_font_status = CreateFontW(
                FONT_SIZE_STATUS,
                0,
                0,
                0,
                FONT_WEIGHT_NORMAL,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                L"Segoe UI"
            );

            g_font_status_bold = CreateFontW(
                FONT_SIZE_STATUS_BOLD,
                0,
                0,
                0,
                FONT_WEIGHT_BOLD,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                L"Segoe UI"
            );

            g_font_slider = CreateFontW(
                FONT_SIZE_SLIDER,
                0,
                0,
                0,
                FONT_WEIGHT_SEMIBOLD,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                L"Segoe UI"
            );

            /*
             * This is deliberately session state.
             * We do not claim the physical hardware is OFF.
             */
            g_command_state = -1;
            g_extreme_cooling_enabled = FALSE;

            g_auto_cooling_active = FALSE;

            reset_auto_counters();

            create_controls(hwnd);
            add_tray_icon(hwnd);
            SetTimer(
                hwnd,
                TRAY_RETRY_TIMER_ID,
                TRAY_RETRY_INTERVAL,
                NULL
            );
            RegisterHotKey(hwnd, HOTKEY_EXTREME, MOD_CONTROL | MOD_SHIFT, '0');

            update_cooling_button();
            update_auto_button();
            update_auto_state_label();
            update_threshold_labels();

            if (g_auto_cooling_enabled)
                start_polling();

            /*
             * Initial telemetry.
             */
            update_data(FALSE);

            return 0;
        }

		case WM_CTLCOLORSTATIC:
		{
			HDC hdc_static = (HDC)wParam;
			HWND control = (HWND)lParam;

			SetBkMode(
				hdc_static,
				TRANSPARENT
			);

			SetTextColor(
				hdc_static,
				COLOR_TEXT
			);

			return (LRESULT)gdi_cache_get_brush(COLOR_PANEL_BG);
		}

        case WM_DRAWITEM:
        {
            draw_action_button((const DRAWITEMSTRUCT *)lParam);
            return TRUE;
        }

        case WM_CTLCOLORBTN:
        {
            HDC hdc_button = (HDC)wParam;
            HWND button = (HWND)lParam;

            if (button == g_check_startup)
            {
                SetTextColor(hdc_button, COLOR_TEXT);
                SetBkColor(hdc_button, COLOR_PANEL_BG);
                return (LRESULT)gdi_cache_get_brush(COLOR_PANEL_BG);
            }

            break;
        }
		
        case WM_COMMAND:
        {
            int control_id = LOWORD(wParam);

            if (control_id == TRAY_TOGGLE_COOLING)
            {
                PostMessageW(
                    hwnd,
                    WM_COMMAND,
                    MAKEWPARAM(BUTTON_COOLING, BN_CLICKED),
                    0
                );
                return 0;
            }

            if (control_id == TRAY_TOGGLE_AUTO)
            {
                PostMessageW(
                    hwnd,
                    WM_COMMAND,
                    MAKEWPARAM(BUTTON_AUTO, BN_CLICKED),
                    0
                );
                return 0;
            }

            if (control_id == TRAY_SHOW)
            {
                show_main_window(hwnd);
                return 0;
            }

            if (control_id == TRAY_EXIT)
            {
                g_exiting = TRUE;
                DestroyWindow(hwnd);
                return 0;
            }

            if (control_id == BUTTON_COOLING &&
				(HIWORD(wParam) == BN_CLICKED || HIWORD(wParam) == 0))
			{
				BOOL enable_cooling;

				/*
				 * Manual control takes ownership away from Auto mode.
				 * The button still toggles the current cooling state, so
				 * disabling Auto must not change the target state.
				 */
				enable_cooling = !g_extreme_cooling_enabled;

				if (g_auto_cooling_enabled)
					disable_automatic_cooling(FALSE);


				if (!set_extreme_cooling(enable_cooling))
				{
					MessageBoxW(
						hwnd,
						enable_cooling
							? L"Windows could not enable Extreme Cooling."
							: L"Windows could not disable Extreme Cooling.",
						L"Cooling Controller",
						MB_OK | MB_ICONERROR
					);
				}

				update_cooling_button();
				update_auto_state_label();

				invalidate_region(25, 82, 600, 180);
				invalidate_region(25, 520, 600, 650);

				return 0;
			}


            if (control_id == BUTTON_AUTO &&
                (HIWORD(wParam) == BN_CLICKED || HIWORD(wParam) == 0))
            {
                if (g_auto_cooling_enabled)
                {
                    disable_automatic_cooling(TRUE);
                }
                else
                {
                    if (!enable_automatic_cooling())
                    {
                        MessageBoxW(
                            hwnd,
                            L"Windows could not start Automatic Cooling.",
                            L"Cooling Controller",
                            MB_OK | MB_ICONERROR
                        );
                    }
                }

                update_cooling_button();
                update_auto_button();
                update_auto_state_label();

                invalidate_region(25, 82, 600, 180);
                invalidate_region(25, 340, 600, 505);
                invalidate_region(25, 520, 600, 650);

                return 0;
            }

            if (control_id == CHECK_STARTUP &&
                HIWORD(wParam) == BN_CLICKED)
            {
                BOOL enabled =
                    SendMessageW(
                        g_check_startup,
                        BM_GETCHECK,
                        0,
                        0
                    ) == BST_CHECKED;

                if (!set_startup_enabled(enabled))
                {
                    SendMessageW(
                        g_check_startup,
                        BM_SETCHECK,
                        enabled ? BST_UNCHECKED : BST_CHECKED,
                        0
                    );

                    MessageBoxW(
                        hwnd,
                        L"Windows could not update the startup setting.",
                        L"Cooling Controller",
                        MB_OK | MB_ICONERROR
                    );
                }

                return 0;
            }

            if (control_id == BUTTON_UNINSTALL &&
                HIWORD(wParam) == BN_CLICKED)
            {
                if (MessageBoxW(
                        hwnd,
                        L"Remove the startup entry and uninstall this application?\n\n"
                        L"The executable will be deleted after the next restart.",
                        L"Uninstall Cooling Controller",
                        MB_OKCANCEL | MB_ICONWARNING
                    ) == IDOK)
                {
                    if (schedule_uninstall())
                    {
						remove_configuration();
						
                        MessageBoxW(
                            hwnd,
                            L"Uninstall scheduled. The application will be removed after the next restart.",
                            L"Cooling Controller",
                            MB_OK | MB_ICONINFORMATION
                        );
                        DestroyWindow(hwnd);
                    }
                    else
                    {
                        MessageBoxW(
                            hwnd,
                            L"Windows could not schedule the uninstall.",
                            L"Cooling Controller",
                            MB_OK | MB_ICONERROR
                        );
                    }
                }

                return 0;
            }
			
			if (control_id == BUTTON_HELP &&
				HIWORD(wParam) == BN_CLICKED)
			{
				int result;

				result = MessageBoxW(
					hwnd,
					L"CONNECTED means this application can communicate with "
					L"the Lenovo GameZone WMI interface used by the Legion Y720 "
					L"for fan and thermal data and Extreme Cooling control.\n\n"
					L"It does not mean Lenovo Nerve Center is running.\n\n"
					L"If WMI is unavailable, the Lenovo GameZone component "
					L"required by this application may be missing. The original "
					L"Lenovo Nerve Center (Sense) software provided the Y720's "
					L"gaming-control functionality, but Lenovo has discontinued "
					L"it. Check Lenovo Support for the Legion Y720 and its "
					L"available software and drivers.\n\n"
					L"Background operation:\n"
					L"The application must remain running in the notification "
					L"area for Automatic Cooling and the global Ctrl+Shift+0 "
					L"shortcut to continue working while the main window is "
					L"hidden.\n\n"
					L"Automatic Cooling preference is saved and restored when "
					L"the application starts. Extreme Cooling itself is never "
					L"automatically enabled at startup; Automatic Cooling begins "
					L"a new temperature-monitoring cycle.\n\n"
					L"Closing the window normally hides the application to the "
					L"notification area. Use Exit from the tray menu to terminate it.\n\n"
					L"Open Lenovo Support now?",
					L"Legion Y720 Cooling Controller",
					MB_YESNO | MB_ICONINFORMATION
				);

				if (result == IDYES)
				{
					ShellExecuteW(
						hwnd,
						L"open",
						L"https://pcsupport.lenovo.com/products/laptops-and-netbooks/legion-series/legion-y720-15ikb/downloads",
						NULL,
						NULL,
						SW_SHOWNORMAL
					);
				}

				return 0;
			}

            break;
        }


        case WM_HSCROLL:
        {
            HWND source = (HWND)lParam;

            if (source == g_trackbar_trigger)
            {
                int new_trigger;

                new_trigger = (int)SendMessageW(
                    g_trackbar_trigger,
                    TBM_GETPOS,
                    0,
                    0
                );

                /*
                 * Validate temperature is within safe absolute bounds.
                 */
                if (!validate_temperature_threshold(new_trigger, ABSOLUTE_MIN_TEMP, ABSOLUTE_MAX_TEMP))
                    new_trigger = MIN_TRIGGER_TEMP;

                g_trigger_temperature = new_trigger;

                enforce_threshold_relationship();
				
				save_settings();
				
                /*
                 * Changing thresholds while Auto is waiting should
                 * restart the persistence timer. This avoids applying
                 * part of an old threshold interval to the new setting.
                 */
                reset_auto_counters();

                update_auto_state_label();

                invalidate_region(25, 340, 600, 505);

                return 0;
            }


            if (source == g_trackbar_stop)
            {
                int new_stop;
                int maximum_stop;

                new_stop = (int)SendMessageW(
                    g_trackbar_stop,
                    TBM_GETPOS,
                    0,
                    0
                );

                /*
                 * Validate temperature is within safe absolute bounds.
                 */
                if (!validate_temperature_threshold(new_stop, ABSOLUTE_MIN_TEMP, ABSOLUTE_MAX_TEMP))
                    new_stop = MIN_STOP_TEMP;

                maximum_stop =
                    g_trigger_temperature - MAX_STOP_GAP;

                if (new_stop > maximum_stop)
                    new_stop = maximum_stop;

                if (new_stop < MIN_STOP_TEMP)
                    new_stop = MIN_STOP_TEMP;

                g_stop_temperature = new_stop;

                SendMessageW(
                    g_trackbar_stop,
                    TBM_SETPOS,
                    TRUE,
                    g_stop_temperature
                );

				save_settings();
				
                update_threshold_labels();

                reset_auto_counters();
                update_auto_state_label();

                invalidate_region(25, 340, 600, 505);

                return 0;
            }

            break;
        }


        case WM_ACTIVATE:
        {
            if (g_auto_cooling_enabled)
            {
                /*
                 * Automatic Cooling must continue even when the window
                 * is not foreground.
                 */
                start_polling();
            }
            else
            {
                if (LOWORD(wParam) != WA_INACTIVE)
                {
                    start_polling();
                }
                else
                {
                    stop_polling();
                }
            }

            return 0;
        }


        case WM_SIZE:
        {
            if (wParam == SIZE_MINIMIZED)
            {
                /*
                 * Auto mode deliberately continues while minimized.
                 */
                if (g_auto_cooling_enabled)
                    start_polling();
                else
                    stop_polling();

                hide_to_tray(hwnd);
            }
            else
            {
                if (g_auto_cooling_enabled)
                {
                    start_polling();
                }
                else if (GetForegroundWindow() == hwnd)
                {
                    start_polling();
                }
            }

            invalidate_region(25, 340, 600, 650);

            return 0;
        }

        case WM_HOTKEY:
        {
            if (wParam == HOTKEY_EXTREME)
                PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(BUTTON_COOLING, BN_CLICKED), 0);
            return 0;
        }

        case WM_TRAYICON:
        {
            UINT tray_event = LOWORD(lParam);

            if (tray_event == WM_LBUTTONUP ||
                tray_event == WM_LBUTTONDBLCLK)
            {
                show_main_window(hwnd);
                return 0;
            }

            if (tray_event == WM_RBUTTONUP ||
                tray_event == WM_CONTEXTMENU)
            {
                show_tray_menu(hwnd);
                return 0;
            }

            break;
        }


        case WM_TIMER:
        {
            if (wParam == TRAY_RETRY_TIMER_ID)
            {
                if (!g_tray_icon_added)
                    add_tray_icon(hwnd);
                else
                    KillTimer(hwnd, TRAY_RETRY_TIMER_ID);

                return 0;
            }

            if (wParam == TIMER_ID)
            {
                if (g_auto_cooling_enabled)
                {
                    /*
                     * Auto mode always owns the timer.
                     */
                    update_data(
                        GetForegroundWindow() != hwnd ||
                        IsIconic(hwnd)
                    );
                }
                else
                {
                    /*
                     * Manual telemetry polling only occurs while the
                     * application is foreground.
                     */
                    if (GetForegroundWindow() == hwnd)
                    {
                        update_data(FALSE);
                    }
                    else
                    {
                        stop_polling();
                    }
                }

                return 0;
            }

            break;
        }


        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc;

            hdc = BeginPaint(
                hwnd,
                &ps
            );

            paint_gui(hdc);

            EndPaint(
                hwnd,
                &ps
            );

            return 0;
        }


        case WM_DESTROY:
        {
            stop_polling();
            KillTimer(hwnd, TRAY_RETRY_TIMER_ID);
            UnregisterHotKey(hwnd, HOTKEY_EXTREME);
            remove_tray_icon();

            /*
             * Safety shutdown happens before WMI is released.
             */
            disable_cooling_before_exit();

            if (g_font_title)
                DeleteObject(g_font_title);

            if (g_font_section)
                DeleteObject(g_font_section);

            if (g_font_normal)
                DeleteObject(g_font_normal);

            if (g_font_value)
                DeleteObject(g_font_value);

            if (g_font_button)
                DeleteObject(g_font_button);

            if (g_font_status)
                DeleteObject(g_font_status);

            if (g_font_status_bold)
                DeleteObject(g_font_status_bold);

            if (g_font_slider)
                DeleteObject(g_font_slider);

            gdi_cache_cleanup();

            GameZoneShutdown();

            PostQuitMessage(0);

            return 0;
        }

        case WM_CLOSE:
        {
            if (!g_exiting)
            {
                hide_to_tray(hwnd);
                return 0;
            }

            DestroyWindow(hwnd);
            return 0;
        }
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam
    );
}


/* ------------------------------------------------------------------------- */
/* Entry point                                                               */
/* ------------------------------------------------------------------------- */

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE previous_instance,
    PWSTR command_line,
    int show_command)
{
    WNDCLASSEXW window_class;
    MSG message;
    HRESULT hr;
    BOOL startup_launch;

    INITCOMMONCONTROLSEX common_controls;

    (void)previous_instance;
    startup_launch =
        command_line && wcsstr(command_line, L"--startup") != NULL;
    initialize_dpi_awareness();
    g_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");

    g_instance_mutex = CreateMutexW(
        NULL,
        TRUE,
        INSTANCE_MUTEX_NAME
    );
    if (!g_instance_mutex)
        return 1;

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        signal_existing_instance();
        CloseHandle(g_instance_mutex);
        g_instance_mutex = NULL;
        return 0;
    }

    /*
     * Initialize common controls because Automatic Cooling uses
     * standard Windows trackbars.
     */
    common_controls.dwSize =
        sizeof(common_controls);

    common_controls.dwICC =
        ICC_BAR_CLASSES;

    InitCommonControlsEx(
        &common_controls
    );
	
    g_auto_cooling_active = FALSE;
    reset_auto_counters();

    /*
     * Initialize Lenovo GameZone WMI before creating the GUI.
     */
    if (!GameZoneInitialize())
    {
        MessageBoxW(
            NULL,
            L"Could not initialize Lenovo GameZone WMI.\n\n"
            L"Please make sure the application is running with "
            L"administrator privileges.",
            L"Legion Y720 Cooling Controller",
            MB_OK | MB_ICONERROR
        );

        CloseHandle(g_instance_mutex);
        g_instance_mutex = NULL;
        return 1;
    }
	
	load_settings();
	
    g_icon_large = LoadIconW(
        instance,
        MAKEINTRESOURCEW(101)
    );

    g_icon_small = LoadIconW(
        instance,
        MAKEINTRESOURCEW(101)
    );

    ZeroMemory(
        &window_class,
        sizeof(window_class)
    );

    window_class.cbSize =
        sizeof(window_class);

    window_class.style =
        CS_HREDRAW | CS_VREDRAW;

    window_class.lpfnWndProc =
        window_proc;

    window_class.hInstance =
        instance;

    window_class.hIcon =
        g_icon_large;

    window_class.hIconSm =
        g_icon_small;

    window_class.hCursor =
	LoadCursorW(
        NULL,
        MAKEINTRESOURCEW(32512)
    );

    window_class.hbrBackground =
        (HBRUSH)(COLOR_WINDOW + 1);

    window_class.lpszClassName =
        WINDOW_CLASS_NAME;

    hr = RegisterClassExW(
        &window_class
    )
        ? S_OK
        : HRESULT_FROM_WIN32(GetLastError());

    if (FAILED(hr))
    {
        CloseHandle(g_instance_mutex);
        g_instance_mutex = NULL;
        GameZoneShutdown();
        return 1;
    }

    {
        RECT window_rect;
        DWORD window_style =
            WS_OVERLAPPED |
            WS_CAPTION |
            WS_SYSMENU |
            WS_MINIMIZEBOX |
            WS_CLIPCHILDREN;

        window_rect.left = 0;
        window_rect.top = 0;
        window_rect.right = WINDOW_WIDTH;
        window_rect.bottom = WINDOW_HEIGHT;
        AdjustWindowRectEx(&window_rect, window_style, FALSE, 0);

        g_window = CreateWindowExW(
        0,
        window_class.lpszClassName,
        L"Legion Y720 Cooling Controller",
        window_style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        NULL,
        NULL,
        instance,
        NULL
        );
    }

    if (!g_window)
    {
        CloseHandle(g_instance_mutex);
        g_instance_mutex = NULL;
        GameZoneShutdown();
        return 1;
    }

    center_window_on_active_monitor(g_window);

    ShowWindow(
        g_window,
        startup_launch ? SW_HIDE : show_command
    );

    UpdateWindow(
        g_window
    );

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

    if (g_instance_mutex)
    {
        CloseHandle(g_instance_mutex);
        g_instance_mutex = NULL;
    }

    /*
     * WM_DESTROY performs the WMI shutdown.
     */
    return (int)message.wParam;
}