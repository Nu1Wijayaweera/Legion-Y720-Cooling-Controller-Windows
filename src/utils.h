#ifndef UTILS_H
#define UTILS_H

#include <windows.h>

/*
 * Utility functions for GUI operations
 */

/*
 * Draw text at specified position with transparency
 */
void draw_text(
    HDC hdc,
    HFONT font,
    int x,
    int y,
    const wchar_t *text,
    COLORREF color
);

/*
 * Draw centered text within a rectangle
 */
void draw_centered_text(
    HDC hdc,
    HFONT font,
    RECT rect,
    const wchar_t *text,
    COLORREF color
);

/*
 * Format a number with optional suffix
 */
void format_number(
    wchar_t *buffer,
    size_t buffer_count,
    LONG value,
    const wchar_t *suffix
);

/*
 * Draw a rounded panel with specified background and border colors
 */
void draw_panel(
    HDC hdc,
    RECT rect,
    COLORREF bg_color,
    COLORREF border_color,
    int corner_radius
);

/*
 * Draw an accent bar on the left side of a panel
 */
void draw_panel_accent(
    HDC hdc,
    RECT rect,
    COLORREF accent_color,
    int width
);

/*
 * Draw a label/value pair at specified positions
 */
void draw_label_value(
    HDC hdc,
    HFONT label_font,
    HFONT value_font,
    int x,
    int y,
    int value_x,
    const wchar_t *label,
    const wchar_t *value,
    COLORREF text_color
);

/*
 * Get temperature color based on value (cool -> warm -> hot)
 */
COLORREF get_temperature_color(int temperature_celsius);

/*
 * GDI Object Cache Management
 * These functions cache frequently used GDI objects to improve performance
 */

/*
 * Initialize the GDI object cache
 */
void gdi_cache_init(void);

/*
 * Get a cached brush for the specified color
 */
HBRUSH gdi_cache_get_brush(COLORREF color);

/*
 * Get a cached pen for the specified color and width
 */
HPEN gdi_cache_get_pen(COLORREF color, int width);

/*
 * Cleanup the GDI object cache
 */
void gdi_cache_cleanup(void);

#endif /* UTILS_H */