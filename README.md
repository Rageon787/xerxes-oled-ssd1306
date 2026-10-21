# Xerxes Pi OLED Driver

A small C++ driver for the 128x64 SSD1306 OLED on the Xerxes Pi. It shows
either **live system information** or a **QR code**, and the admin
software picks which one using command-line options.

No external libraries are needed: only the Linux I2C interface and a C++17
compiler. The QR code encoder is built in.

## Code layout

| File | What it does |
| --- | --- |
| `src/main.cpp` | Command-line options, the info and QR screens, the main loop |
| `src/ssd1306.h/.cpp` | Display driver: framebuffer, text, rectangles, I2C, brightness, flip |
| `src/qrcode.h/.cpp` | QR code encoder (versions 1-3, error correction L, up to 53 characters) |
| `src/sysinfo.h/.cpp` | Reads hostname, IP, DNS, CPU temperature, uptime, network speed |

## Build and install

Enable I2C first (`sudo raspi-config` → Interface Options → I2C), then:

```bash
make
sudo make install        # installs /usr/local/bin/xerxes-oled
```

## Usage

```bash
# Live system information (the default)
xerxes-oled

# Choose which lines to show, and add a custom message at the bottom
xerxes-oled --fields hostname,ip,temp --message "Rack 2 / Slot 4"

# QR code pointing at the admin panel. {ip} and {hostname} are filled in.
xerxes-oled --mode qr --qr-text "http://{ip}:8080" --qr-caption "Scan to open admin panel"

# Dimmer, upside down (for boards mounted the other way)
xerxes-oled --brightness 30 --flip
```

Run `xerxes-oled --help` for every option. Ctrl+C (or `systemctl stop`)
clears and turns off the screen.

### Testing without a screen

`--preview` draws the screen in the terminal instead of on the OLED, so
you can work on layouts on any machine (including macOS). `--once` draws a
single frame and exits.

```bash
./oled --preview --mode qr --qr-text "http://192.168.1.10:8080" --once
```

### Running at boot

Create `/etc/systemd/system/xerxes-oled.service`:

```ini
[Unit]
Description=Xerxes Pi OLED display

[Service]
ExecStart=/usr/local/bin/xerxes-oled --mode info
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

Then `sudo systemctl enable --now xerxes-oled`. To change what is shown,
the admin software edits `ExecStart` (or uses a drop-in override) and
restarts the service.

## Notes

- The font only covers ASCII. Four symbols are redrawn as icons:
  `[` up arrow, `]` down arrow, `^` warning, `` ` `` degree sign.
- QR codes are drawn as dark modules on a lit square, because scanners
  expect dark-on-light.
- The QR encoder was checked module by module against the Python
  `qrcode` library for all 8 masks and versions 1-3.
