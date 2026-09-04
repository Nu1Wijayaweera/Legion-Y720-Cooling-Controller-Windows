#include "utils.h"
#include "theme.h"
#include <stdio.h>

void draw_text(
    HDC hdc,
    HFONT font,
    int x,
    int y,
    const wchar_t *text,
    COLORREF color)
{
    HFONT old_font;

    if (!hdc || !text)
        return;

    old_font = (HFONT)SelectObject(hdc, font);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);

    TextOutW(
        hdc,
        x,
        y,
        text,
        lstrlenW(text)
    );

    SelectObject(hdc, old_font);
}


void draw_centered_text(
    HDC hdc,
    HFONT font,
    RECT rect,
    const wchar_t *text,
    COLORREF color)
{
    HFONT old_font;
    RECT r = rect;

    if (!hdc || !text)
        return;

    old_font = (HFONT)SelectObject(hdc, font);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);

    DrawTextW(
        hdc,
        text,
        -1,
        &r,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE
    );

    SelectObject(hdc, old_font);
}


void format_number(
    wchar_t *buffer,
    size_t buffer_count,
    LONG value,
    const wchar_t *suffix)
{
    if (!buffer || buffer_count == 0)
        return;

    if (suffix)
        swprintf_s(buffer, buffer_count, L"%ld%s", value, suffix);
    else
        swprintf_s(buffer, buffer_count, L"%ld", value);
}


void draw_panel(
    HDC hdc,
    RECT rect,
    COLORREF bg_color,
    COLORREF border_color,
    int corner_radius)
{
    HBRUSH brush;
    HPEN pen;
    HPEN old_pen;
    HBRUSH old_brush;

    brush = gdi_cache_get_brush(bg_color);
    pen = gdi_cache_get_pen(border_color, 1);

    if (!brush || !pen)
        return;

    old_pen = (HPEN)SelectObject(hdc, pen);
    old_brush = (HBRUSH)SelectObject(hdc, brush);

    RoundRect(
        hdc,
        rect.left,
        rect.top,
        rect.right,
        rect.bottom,
        corner_radius,
        corner_radius
    );

    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);

}


void draw_panel_accent(
    HDC hdc,
    RECT rect,
    COLORREF accent_color,
    int width)
{
    HBRUSH brush;

    brush = gdi_cache_get_brush(accent_color);

    if (!brush)
        return;

    FillRect(
        hdc,
        &(RECT){
            rect.left,
            rect.top,
            rect.left + width,
            rect.bottom
        },
        brush
    );

}


void draw_label_value(
    HDC hdc,
    HFONT label_font,
    HFONT value_font,
    int x,
    int y,
    int value_x,
    const wchar_t *label,
    const wchar_t *value,
    COLORREF text_color)
{
    draw_text(
        hdc,
        label_font,
        x,
        y,
        label,
        text_color
    );

    draw_text(
        hdc,
        value_font,
        value_x,
        y - 2,
        value,
        text_color
    );
}


COLORREF get_temperature_color(int temperature_celsius)
{
    if (temperature_celsius < 60)
        return COLOR_TEMP_COOL;
    else if (temperature_celsius < 80)
        return COLOR_TEMP_WARM;
    else
        return COLOR_TEMP_HOT;
}

/* ------------------------------------------------------------------------- */
/* GDI Object Cache Implementation                                            */
/* ------------------------------------------------------------------------- */

#define MAX_CACHED_BRUSHES  16
#define MAX_CACHED_PENS     16

typedef struct
{
    COLORREF color;
    HBRUSH brush;
} CACHED_BRUSH;

typedef struct
{
    COLORREF color;
    int width;
    HPEN pen;
} CACHED_PEN;

static CACHED_BRUSH g_brush_cache[MAX_CACHED_BRUSHES];
static CACHED_PEN g_pen_cache[MAX_CACHED_PENS];
static int g_cache_initialized = 0;

void gdi_cache_init(void)
{
    int i;

    if (g_cache_initialized)
        return;

    for (i = 0; i < MAX_CACHED_BRUSHES; i++)
    {
        g_brush_cache[i].brush = NULL;
    }

    for (i = 0; i < MAX_CACHED_PENS; i++)
    {
        g_pen_cache[i].pen = NULL;
    }

    g_cache_initialized = 1;
}

HBRUSH gdi_cache_get_brush(COLORREF color)
{
    int i;
    HBRUSH brush = NULL;

    if (!g_cache_initialized)
        gdi_cache_init();

    /* Look for existing brush with same color */
    for (i = 0; i < MAX_CACHED_BRUSHES; i++)
    {
        if (g_brush_cache[i].brush && g_brush_cache[i].color == color)
        {
            return g_brush_cache[i].brush;
        }
    }

    /* Create new brush and cache it */
    for (i = 0; i < MAX_CACHED_BRUSHES; i++)
    {
        if (!g_brush_cache[i].brush)
        {
            brush = CreateSolidBrush(color);
            if (brush)
            {
                g_brush_cache[i].color = color;
                g_brush_cache[i].brush = brush;
                return brush;
            }
        }
    }

    return NULL;
}

HPEN gdi_cache_get_pen(COLORREF color, int width)
{
    int i;
    HPEN pen = NULL;

    if (!g_cache_initialized)
        gdi_cache_init();

    /* Look for existing pen with same color and width */
    for (i = 0; i < MAX_CACHED_PENS; i++)
    {
        if (g_pen_cache[i].pen && g_pen_cache[i].color == color && g_pen_cache[i].width == width)
        {
            return g_pen_cache[i].pen;
        }
    }

    /* Create new pen and cache it */
    for (i = 0; i < MAX_CACHED_PENS; i++)
    {
        if (!g_pen_cache[i].pen)
        {
            pen = CreatePen(PS_SOLID, width, color);
            if (pen)
            {
                g_pen_cache[i].color = color;
                g_pen_cache[i].width = width;
                g_pen_cache[i].pen = pen;
                return pen;
            }
        }
    }

    return NULL;
}

void gdi_cache_cleanup(void)
{
    int i;

    if (!g_cache_initialized)
        return;

    for (i = 0; i < MAX_CACHED_BRUSHES; i++)
    {
        if (g_brush_cache[i].brush)
        {
            DeleteObject(g_brush_cache[i].brush);
            g_brush_cache[i].brush = NULL;
        }
    }

    for (i = 0; i < MAX_CACHED_PENS; i++)
    {
        if (g_pen_cache[i].pen)
        {
            DeleteObject(g_pen_cache[i].pen);
            g_pen_cache[i].pen = NULL;
        }
    }

    g_cache_initialized = 0;
}