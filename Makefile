# Build:    make
# Install:  sudo make install
# Remove:   sudo make uninstall

CXX      ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -std=c++17
PREFIX   ?= /usr/local

SOURCES = src/main.cpp src/ssd1306.cpp src/qrcode.cpp src/sysinfo.cpp
HEADERS = src/ssd1306.h src/qrcode.h src/sysinfo.h

oled: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@

install: oled
	install -Dm755 oled $(DESTDIR)$(PREFIX)/bin/xerxes-oled

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/xerxes-oled

clean:
	rm -f oled

.PHONY: install uninstall clean
