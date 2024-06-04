#ifndef SERIAL_COMMUNICATION_H
#define SERIAL_COMMUNICATION_H

#include <Arduino.h>
#include <ArduinoJson.h>        // Need to add library bblanchon/ArduinoJSON
// #include <SoftwareSerial.h>

// extern SoftwareSerial SerialPort;
// void publishMessage();

void send_serial_data(const char *data);
void send_power_ozs(String &power);
void send_stop_ozs();
void send_start_ozs(int action, int duration, int wind_speed);
void on_message_received(String &topic, DynamicJsonDocument doc, int length);

void send_wifi_ready();

#endif // SERIAL_COMMUNICATION_H
