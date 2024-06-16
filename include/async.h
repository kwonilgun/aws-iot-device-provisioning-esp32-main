
#include <Arduino.h>
#include <WiFi.h>
#include "SPIFFS.h"
#include <WiFiClientSecure.h>


void initSPIFFS();

String readFile(fs::FS &fs, const char * path);

void writeFile(fs::FS &fs, const char * path, const char * message);

bool initWiFi(String cont);
void gotoSoftApSetup();