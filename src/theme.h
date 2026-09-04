#ifndef THEME_H
#define THEME_H

#include <windows.h>

/*
 * Lenovo Legion Nerve Center Inspired Color Theme
 *
 * Modern bluish theme matching Lenovo Legion aesthetic
 */

/* Primary Colors */
#define COLOR_PRIMARY_ACCENT        RGB(0, 120, 215)      /* Lenovo blue */
#define COLOR_SECONDARY_ACCENT     RGB(0, 180, 255)      /* Lighter blue */
#define COLOR_THEME_BG             RGB(10, 15, 25)       /* Deep dark blue */
#define COLOR_PANEL_BG             RGB(20, 30, 45)       /* Lighter dark blue */
#define COLOR_PANEL_BORDER         RGB(54, 59, 68)       /* Panel border */
#define COLOR_TEXT                 RGB(200, 210, 230)    /* Soft white-blue */
#define COLOR_TEXT_DIM             RGB(150, 160, 180)    /* Dimmed text */

/* Status Colors */
#define COLOR_SUCCESS              RGB(0, 200, 150)      /* Green-blue */
#define COLOR_WARNING              RGB(255, 180, 0)      /* Amber */
#define COLOR_DANGER               RGB(255, 80, 80)      /* Red */
#define COLOR_ACTIVE               RGB(45, 140, 230)     /* Active accent */
#define COLOR_BUTTON_BG            RGB(30, 55, 80)       /* Default action button */
#define COLOR_BUTTON_ACTIVE        RGB(0, 120, 90)      /* Cooling enabled */
#define COLOR_BUTTON_AUTO          RGB(0, 90, 160)      /* Auto mode enabled */

/* Temperature Zone Colors */
#define COLOR_TEMP_COOL            RGB(0, 200, 150)      /* Cool zone */
#define COLOR_TEMP_WARM            RGB(255, 180, 0)      /* Warm zone */
#define COLOR_TEMP_HOT             RGB(255, 80, 80)      /* Hot zone */

/* Layout Constants */
#define WINDOW_WIDTH               625
#define WINDOW_HEIGHT              665
#define PANEL_CORNER_RADIUS        15
#define PANEL_ACCENT_WIDTH         4

/* Font Sizes (negative values for specific height in pixels) */
#define FONT_SIZE_TITLE            -24
#define FONT_SIZE_SECTION          -17
#define FONT_SIZE_NORMAL           -14
#define FONT_SIZE_VALUE            -16
#define FONT_SIZE_BUTTON           -13
#define FONT_SIZE_STATUS           -14
#define FONT_SIZE_STATUS_BOLD      -15
#define FONT_SIZE_SLIDER           -14

/* Font Weights */
#define FONT_WEIGHT_NORMAL         FW_NORMAL
#define FONT_WEIGHT_SEMIBOLD       FW_SEMIBOLD
#define FONT_WEIGHT_BOLD           FW_BOLD

/* Panel Layout Constants */
#define MARGIN                     25
#define PANEL_SPACING              20
#define HEADER_HEIGHT              70

/* Control Constants */
#define BUTTON_WIDTH               220
#define BUTTON_HEIGHT              42
#define TRACKBAR_HEIGHT            25
#define TRACKBAR_WIDTH             380

#endif /* THEME_H */