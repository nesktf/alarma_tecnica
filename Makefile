SKETCH := $(CURDIR)/src/server.cpp
BOARD := nodemcu
ESP_ROOT := $(CURDIR)/lib/esp8266

include $(CURDIR)/lib/makeEspArduino/makeEspArduino.mk
