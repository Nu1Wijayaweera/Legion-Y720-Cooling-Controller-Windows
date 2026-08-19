#ifndef Y720_GAMEZONE_H
#define Y720_GAMEZONE_H

#include <windows.h>


/*
 * Result of a single Lenovo GameZone WMI method.
 */
typedef struct
{
    LONG data;
    LONG return_value;
    int success;
} WMI_RESULT;


/*
 * Telemetry actually used by the v1.0 GUI.
 */
typedef struct
{
    WMI_RESULT fan1;
    WMI_RESULT fan2;
    WMI_RESULT ir_temp;

} GAMEZONE_DATA;


/*
 * Initialize Lenovo GameZone WMI.
 */
int GameZoneInitialize(void);


/*
 * Shut down Lenovo GameZone WMI.
 */
void GameZoneShutdown(void);


/*
 * Read supported telemetry.
 *
 * This function is read-only.
 */
int GameZoneRead(
    GAMEZONE_DATA *data
);


/*
 * Enable or disable Lenovo Extreme Cooling.
 *
 * setting = 1 -> ON
 * setting = 0 -> OFF
 *
 * Returns non-zero when the WMI method call itself
 * succeeded.
 */
BOOL GameZoneSetFanCooling(
    DWORD setting
);

#endif