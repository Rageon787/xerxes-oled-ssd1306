CXX = g++
CXXFLAGS = -O2 -Wall -std=c++17

SOURCES = src/main.cpp src/ssd1306.cpp src/sysinfo.cpp src/qrcodegen.cpp

oled: $(SOURCES) src/ssd1306.h src/sysinfo.h src/qrcodegen.hpp
	$(CXX) $(CXXFLAGS) $(SOURCES) -o oled

clean:
	rm -f oled

install: oled
	install -Dm755 oled /usr/local/bin/xerxes-oled

uninstall:
	rm -f /usr/local/bin/xerxes-oled

.PHONY: clean install uninstall
