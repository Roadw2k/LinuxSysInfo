# LinuxSysInfo

LinuxSysInfo is a lightweight desktop system information widget for Linux built with Qt6 and C++.

The application provides a clean GNOME-style interface that displays real-time system statistics directly on the desktop.

## Features

* Real-time CPU usage monitoring
* Live CPU history graph
* Memory usage display
* Disk usage statistics
* Network interface and IP information
* System uptime display
* Frameless translucent desktop widget
* Draggable interface
* GNOME-inspired styling

## Requirements

* Ubuntu/Linux desktop environment / Can run in Arch, but not fully tested.
* Qt6
* X11/Xorg support (`QT_QPA_PLATFORM=xcb`)

## Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Run

```bash
QT_QPA_PLATFORM=xcb ./LinuxSysInfo
```

## Packaging

The application can be packaged as a `.deb` package for installation on Ubuntu-based systems.

## Notes

Wayland limits desktop widget positioning and movement. For the best experience, run the application using X11/Xorg with the `xcb` Qt platform plugin.

## License

MIT

## Screenshot

![LinuxSysInfo Screenshot](https://github.com/Roadw2k/LinuxSysInfo/blob/master/LinuxSysInfo.png?raw=true)