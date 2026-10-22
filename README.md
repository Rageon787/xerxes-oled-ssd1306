# XerxesPi OLED Driver

A lightweight **single-file C++ SSD1306 OLED driver** for Raspberry Pi (I²C) with:

- 128×64 framebuffer
- Full ASCII 5×7 font
- Custom icon system (int-based)
- 90° framebuffer rotation support
- Simple UI-friendly drawing API
- No external dependencies (uses Linux I²C only)

---

# Features

- Direct `/dev/i2c-1` communication
- SSD1306 initialization sequence (stable on Pi 4)
- ASCII text rendering (32–127)
- Custom icon system (arrows, warning, UI symbols)
- Integer-based icon API (no ASCII dependency)
- Framebuffer-based rendering (fast + flicker-free)
- Optional framebuffer rotation (portrait mode support)

# Features added by Ragav

- QR Code mode to access the admin panel of your Xerxes Pi.
- View system statistics of your Xerxes Pi (hostname, IP, DNS, CPU temperature).
- Startup screen
- Terminal preview mode (`--preview`) to see the screen without an OLED attached.
- Display is cleared and turned off on exit (Ctrl+C / SIGTERM).

# Features to be added next:

- QR Code needs to link to admin panel.
- Customization options for startup and display options.

---

# Requirements

- Raspberry Pi 4 / Pi 5 (or compatible 3rd party)
- Raspberry Pi OS (Linux)
- I²C enabled
- SSD1306 128×64 OLED (I²C address typically `0x3C`)
- g++ compiler (C++17)
- `make`

---

# Instructions

Enable I²C in `raspi-config`

```bash
sudo raspi-config
```

Then go to **Interface Options → I2C → Enable**, and reboot.

Enable I2C output in your XerxersPi by going to `Interface Options -> I2C -> Enable` under raspi-config and restart the device

Compile with Makefiles:

```bash
make
```

This builds the `oled` binary in the project root.

| Command               | What it does                                        |
| --------------------- | --------------------------------------------------- |
| `make`                | Builds `./oled`                                     |
| `make clean`          | Removes the `oled` binary                           |
| `sudo make install`   | Installs the binary to `/usr/local/bin/xerxes-oled` |
| `sudo make uninstall` | Removes `/usr/local/bin/xerxes-oled`                |

---

# Usage

```bash
./oled                          # system info screen
./oled qr                       # QR code for http://<pi-ip>
./oled qr http://example.com    # QR code for custom text / URL
./oled --preview                # print the screen in the terminal (no OLED needed)
./oled qr --preview             # preview the QR code screen in the terminal
```

There are 3 Modes for the OLED Screen and one debug mode to test the output on a terminal interface.

### System Statistics Mode

System Statistics mode displays real time system information on the OLED Screen. The information is fetched from Linux files.

```bash
<hostname>

IP  <ip address>
DNS <dns server>

CPU <temperature>
```

### QR code Mode

A QR Code to the specified url is displayed on the right side of the screen.

```bash
./oled qr http://example.com
```

---

### Custom Mode

Custom mode allows a user to display anything they wish on the 128 x 64 OLED display. Implementation for this mode is still work in progress and will be available in a future version.

---

### Debug Mode

```bash
./oled --preview              # preview flag prints output on terminal
```

Debug mode allows you to test your OLED Screen output on a terminal interface in your desktop.

# Project Structure

```
src/
├── main.cpp        # Entry point: arguments, startup screen, info + QR screens
├── ssd1306.cpp/.h  # SSD1306 I²C driver, framebuffer, font, icons
├── sysinfo.cpp/.h  # Hostname, IP, DNS and CPU temperature helpers
└── qrcodegen.cpp/.hpp  # Nayuki's QR Code generator
Makefile
```

---

# Credits

- QR codes use [Project Nayuki's QR Code generator](https://github.com/nayuki/QR-Code-generator) (MIT), included in `src/`.

# License

See [LICENSE](LICENSE).
