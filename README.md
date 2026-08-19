# Legion Y720 Cooling Monitor

A lightweight native Windows application for monitoring and controlling cooling features on the **Lenovo Legion Y720**.

The application communicates with the Lenovo GameZone WMI interface and provides a simple graphical interface for monitoring fan speeds and thermal data, as well as enabling or disabling Extreme Cooling.

## Features

- Monitor **Fan 1** speed in RPM
- Monitor **Fan 2** speed in RPM
- Monitor the Lenovo GameZone thermal / IR sensor
- Enable **Extreme Cooling**
- Disable **Extreme Cooling**
- Automatic telemetry refresh every second
- Native Windows GUI written in C
- Custom application icon
- Runs with the administrator privileges required by the Lenovo GameZone WMI interface

## Requirements

- Windows
- Lenovo Legion Y720
- Lenovo GameZone WMI interface available on the system
- Administrator privileges

This application was developed specifically for the **Lenovo Legion Y720**. It is not intended to be a universal Lenovo Legion cooling controller.

## Usage

Run:

```text
Y720CoolingMonitor.exe
```

Windows will request administrator privileges because the application requires access to the Lenovo GameZone WMI interface.

The main window displays:

- **Extreme Cooling** controls
- Fan 1 RPM
- Fan 2 RPM
- Thermal / IR sensor temperature
- WMI connection status
- Telemetry refresh interval

### Extreme Cooling

Use:

**EXTREME COOLING ON**

to request Extreme Cooling mode.

Use:

**EXTREME COOLING OFF**

to disable it.

The application displays the last command sent by the GUI.

> Note: the Lenovo `GetFanCoolingStatus` WMI method was found to be unreliable on the Y720 during development. Therefore, the application does not claim that the displayed command state represents a verified physical cooling state. The status indicates the last command successfully accepted by the WMI interface.

## Building from Source

The project uses the **MinGW-w64 UCRT64** toolchain provided by MSYS2.

The current build script expects:

```text
C:\msys64\ucrt64\bin\gcc.exe
C:\msys64\ucrt64\bin\windres.exe
```

### Build

Run:

```text
build.bat
```

The script:

1. Cleans the `build` directory
2. Compiles the cooling backend
3. Compiles the GUI
4. Compiles the Windows resource file
5. Links the final executable

The resulting executable is:

```text
build\Y720CoolingMonitor.exe
```

## Project Structure

```text
Legion-Y720-Cooling-Controller-Windows/
│
├── src/
│   ├── Y720CoolingMonitorGUI.c
│   ├── Y720CoolingMonitor.c
│   ├── Y720CoolingMonitor.h
│   ├── Y720CoolingMonitor.rc
│   └── Y720CoolingMonitor.manifest
│
├── resources/
│   └── Y720CoolingMonitor.ico
│
├── build.bat
├── README.md
└── LICENSE
```

The `build` directory is generated during compilation and is not part of the source distribution.

## Technical Notes

The application uses the Lenovo GameZone WMI interface exposed by the Y720.

The GUI is implemented using the native Win32 API and is written in C. No external GUI framework is required.

Telemetry is refreshed once per second.

The application requests administrator privileges through its Windows application manifest.

## Compatibility

This project was developed and tested on a Lenovo Legion Y720.

Because Lenovo's GameZone WMI interface and firmware behavior can vary between systems, compatibility with other Lenovo models is not guaranteed.

## Release

### v1.0.0

Initial public release.

Included:

- Fan 1 monitoring
- Fan 2 monitoring
- Thermal sensor monitoring
- Extreme Cooling ON/OFF control
- Native Windows GUI
- Application icon
- Lenovo GameZone WMI integration
- Administrator manifest

## Disclaimer

This software is provided for use with the Lenovo Legion Y720.

Use it at your own risk. The author is not responsible for hardware damage, data loss, system instability, or other consequences resulting from the use of this software.

The application relies on Lenovo's GameZone WMI interface and does not modify or replace Lenovo firmware.

## License

This project is licensed under the **MIT License**.

See [LICENSE](LICENSE) for the full license text.
