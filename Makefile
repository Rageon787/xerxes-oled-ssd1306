# Needs libqrencode: sudo apt install libqrencode-dev

CXX = g++
CXXFLAGS = -O2 -Wall -std=c++17 $(shell pkg-config --cflags libqrencode)
LIBS = $(shell pkg-config --libs libqrencode)

SOURCES = src/main.cpp src/ssd1306.cpp src/sysinfo.cpp

oled: $(SOURCES) src/ssd1306.h src/sysinfo.h
	$(CXX) $(CXXFLAGS) $(SOURCES) -o oled $(LIBS)

clean:
	rm -f oled

install: oled
	install -Dm755 oled /usr/local/bin/xerxes-oled

uninstall:
	rm -f /usr/local/bin/xerxes-oled

.PHONY: clean install uninstall
