#include <Arduino.h>
#include "serial_communication.h"
#include <ArduinoJson.h>        // Need to add library bblanchon/ArduinoJSON

#define DEBUG

#define SERIAL_DELAY 5


void send_serial_data(const char *data) {
    // Implementation of serial write function

    String newData = "[" + String(data) + "]";

#ifdef DEBUG
    Serial.printf("send_serial_data data = %s\n", newData.c_str());
#endif
    Serial.println("");

    // Your serial write logic goes here
    // Example: serial_write_function(data);
    // For demonstration, I'm just printing the data here
    // Serial write logic for all characters in newData
    for (int i = 0; i < newData.length(); i++) {
        char currentChar = newData.charAt(i);
        
        // Serial.printf("Sending character: %c \n", currentChar);
        
        Serial2.write(currentChar);
        
        //2024-05-29 : 시리얼로 전달할 때 delay가 필요하다. 이것이 없으면 통신이 안되는 경우 발생.
        delay(SERIAL_DELAY);

        // Your serial write logic goes here
        // Example: serial_write_function(currentChar);
        // For demonstration, I'm just printing the character here
    }
}

// wifi ready send
void send_wifi_ready() {
    Serial.println("send wifi ready to ozs");

    send_serial_data("{wifi:on}");

}

void send_power_ozs(String &power) {
    Serial.println("send power cmd");

    // Parse JSON payload
    StaticJsonDocument<200> doc; // Adjust the size as needed
    DeserializationError error = deserializeJson(doc, power);
    
    if (error) {
        Serial.print("send_power_ozs deserializeJson() failed: ");
        Serial.println(error.f_str());
        return;
    }

    // Serialize JSON document to a string
    char jsonBuffer[200];
    serializeJsonPretty(doc, jsonBuffer, sizeof(jsonBuffer));

#ifdef DEBUG  
    // Print JSON string
    Serial.println("send_power_ozs Parsed JSON document:");
    Serial.println(jsonBuffer);
#endif


    // Handle different commands
    if (doc["power"] == "on") {
         Serial.print("send_power_ozs power on");
         Serial.println();
        send_serial_data("{power:on}");

    } else if (doc["power"] == "off") {
        Serial.print("send_power_ozs power off");
         Serial.println();
          send_serial_data("{power:off}");
    } else {
        Serial.println("Key not found in data");
    }

    // start ozs
    send_serial_data("{start:act}"); // Additional data transmission
}

void send_stop_ozs() {
    Serial.println("send stop cmd");

    send_serial_data("{stop:on}");
    // delay(SERIAL_DELAY);
    // send_serial_data("{start:act:}");
}

void send_check_ozs_status() {
    Serial.println("send check ozs status");

    send_serial_data("{status:status}");
    // delay(SERIAL_DELAY);
    // send_serial_data("{start:act:}");
}

void send_check_system_info() {
    Serial.println("check system info");

    send_serial_data("{status:sysinfo}");
    // delay(SERIAL_DELAY);
    // send_serial_data("{start:act:}");
}

void send_start_ozs(int action, int duration, int wind_speed) {

//    Serial.println("send start cmd :");
//     Serial.printf("action = %d\n", action);
//     Serial.printf("time = %d\n", duration);
//     Serial.printf("wind = %d\n", wind_speed);

    // Create a JSON document
    StaticJsonDocument<200> doc;
    doc["action"] = action;
    doc["duration"] = (duration == 5) ? 1 : duration * 30;
    doc["wind_speed"] = wind_speed;

    // Serialize JSON to string
    char jsonBuffer[256];
    serializeJson(doc, jsonBuffer);

    // Create the final message by combining jsonBuffer and {"start"}
    char finalMessage[200];
    snprintf(finalMessage, sizeof(finalMessage), "{\"start\":%s}", jsonBuffer);

    Serial.println("finalMessage =");
    Serial.println(finalMessage);

    // Send the final message
    send_serial_data(finalMessage);
}

void on_message_received(String &topic, DynamicJsonDocument doc, int length) {

    // DynamicJsonDocument doc(length);

#ifdef DEBUG
    Serial.print("on_message_received topic: ");
    Serial.println(topic);

    String output;
    serializeJsonPretty(doc, output);
    Serial.print("on_message_received JSON: ");
    Serial.println(output);
   
#endif
    // Check if "start" key exists
    if (doc.containsKey("start")) {
        // Get the "start" object
        JsonObject startObj = doc["start"];
        // Check if "action", "time", "wind" keys exist within "start" object
        if (startObj.containsKey("action") && startObj.containsKey("time") && startObj.containsKey("wind")) {
            // Get the values of "action", "time", "wind"
            int action = startObj["action"];
            int time = startObj["time"];
            int wind = startObj["wind"];
#ifdef DEBUG
            // Serial.print("Action: ");
            // Serial.println(action);
            // Serial.print("Time: ");
            // Serial.println(time);
            // Serial.print("Wind: ");
            // Serial.println(wind);
#endif
            // Now you can use the extracted values as needed
            send_start_ozs(action, time, wind);

        } else {
            Serial.println("One or more keys (action, time, wind) not found within 'start' object");
        }
    } 
    else if(doc.containsKey("stop")){
        send_stop_ozs();
    }
    else if(doc.containsKey("power")){
        // send_power_ozs(payload);
    }

    else if(doc.containsKey("status")){
       // Print the value of "status"
        const char* statusValue = doc["status"].as<const char*>();
       

        // Additional logic based on the value of "status"
        if (strcmp(statusValue, "status") == 0) {
            #ifdef DEBUG
            Serial.println("Received status check...");
            #endif
            send_check_ozs_status();
        } else if (strcmp(statusValue, "sysinfo") == 0) {
            #ifdef DEBUG
            Serial.println("Received system info inquiry");
            #endif
            send_check_system_info();
        } else {
            Serial.println("Error: unrecognized status value");
        }
        //  publishMessage();

    }
    
    
    else {
        Serial.printf("No 'start' key found in data");
    }

   
}
