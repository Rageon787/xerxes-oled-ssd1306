# XerxesPi OLED Driver 

A lightweight **single-file C++ SSD1306 OLED driver** for Raspberry Pi (I²C) with:

- 128×64 framebuffer
- Full ASCII 5×7 font
- Custom icon system (int-based)
- 90° framebuffer rotation support
- Simple UI-friendly drawing API
- QR codes using libqrencode

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
- QR Code mode to access the admin panel of your XerxesP- View system statistics of your Xerxes Pi.
- Startup screen 

# Features to be added next:
- QR Code needs to link to admin panel.
- Customization options for startup and display options.

---

# Requirements

- Raspberry Pi 4 / Pi 5 (or compatible 3rd party)
- Raspberry Pi OS (Linux)
- I²C enabled
- SSD1306 128×64 OLED (I²C address typically `0x3C`)
- g++ compiler
- libqrencode (`sudo apt install libqrencode-dev`)

---

# Instructions

Enable I²C in `raspi-config`

```bash
sudo raspi-config
```

Compile with Makefiles:

```bash
make
./oled                  # system info
./oled qr               # QR code for http://<ip>
./oled qr http://example.com
./oled --preview        # print the screen in the terminal (no OLED needed)
sudo make install       # installs /usr/local/bin/xerxes-oled
```




