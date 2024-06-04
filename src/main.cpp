/*
 * File: main.cpp
 * Project: src
 * File Created: Thursday, 28th July 2022 5:15:25 pm
 * Author: Kwonilgun(권일근) (kwonilgun@naver.com)
 * -----
 * Last Modified: Monday, 3rd June 2024 1:52:49 pm
 * Modified By: Kwonilgun(권일근) (kwonilgun@naver.com>)
 * -----
 * Copyright <<projectCreationYear>> - 2024 루트원 AI, 루트원 AI
 */


// 참조 사이트 : https://github.com/toygame/aws-iot-device-provisioning-esp32/blob/main/src/main.cpp

#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include "config.h"
#include "serial_communication.h"
#include "main.h"

#define DEBUG

// EPS32 serial port  참조 사이트 : https://circuits4you.com/2018/12/31/esp32-hardware-serial2-example/
/*
 * There are three serial ports on the ESP known as U0UXD, U1UXD and U2UXD.
 * 
 * U0UXD is used to communicate with the ESP32 for programming and during reset/boot.
 * U1UXD is unused and can be used for your projects. Some boards use this port for SPI Flash access though
 * U2UXD is unused and can be used for your projects.
 * 
*/

#define RXD2 16
#define TXD2 17

WiFiClientSecure net;
PubSubClient client(net);

Preferences preferences;

int init_count = 0;
unsigned long start_time;
unsigned long end_time;

// // port(5,6) ozs serial 통신
// SoftwareSerial SerialPort(D5,D6);

String receivedString = ""; // 수신된 문자열을 저장할 변수
bool insideBrackets = false; // '['와 ']' 사이의 문자열인지 여부를 나타내는 플래그




void initializeAwsJson()
{
  // Write invalid JSON content to the file
  const char* invalidJsonContent = "This is not valid JSON!";
  
  // Open the file for writing
  File file = SPIFFS.open("/aws.json", "w");
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }

  // Write the invalid JSON content to the file
  file.print(invalidJsonContent);
  
  // Close the file
  file.close();
  
  Serial.println("AWS JSON file initialized with invalid JSON content");
}

void saveCertificateToFS(DynamicJsonDocument doc)
{

  // client.publish("$aws/certificates/create/json", "") 에서 certificate 생성이 요청, 
  DynamicJsonDocument pem(4000);
  pem["certificatePem"] = doc["certificatePem"];
  pem["privateKey"] = doc["privateKey"];
  File file = SPIFFS.open("/aws.json", "w");
  if (!file)
  {
    Serial.println("failed to open config file for writing");
  }
  serializeJson(pem, Serial);
  serializeJson(pem, file);
  file.close();
}

void registerThing(DynamicJsonDocument doc)
{
  Serial.println("registerThing....... ");
  const char *certificateOwnershipToken = doc["certificateOwnershipToken"];
  DynamicJsonDocument reqBody(4000);
  reqBody["certificateOwnershipToken"] = certificateOwnershipToken;
  reqBody["parameters"]["SerialNumber"] = WiFi.macAddress();
  char jsonBuffer[4000];
  serializeJson(reqBody, jsonBuffer);
  Serial.println(">>>>>>>>>>>>>Sending certificate<<<<<<<<<<<");
  client.publish("$aws/provisioning-templates/claim_0603/provision/json", jsonBuffer);
}

void messageHandler(String topic, byte *payload, int length)
{

  start_time = millis();

  Serial.print("incoming: ");
  Serial.println(topic);
  // DynamicJsonDocument doc(length);
  DynamicJsonDocument doc(1000);

  //2024-06-03 : payload에 provision에서 발행된 인증서가 실려 온다. 이것을 /aws.json 파일에 저장을 하고 , 저장된 파일을 이용해서 iot 관련 서비스를 진행하면 된다. 먼저 $aws/certificates/create/json/accepted, 받아서 실행을하고, registerThings에서 $aws/provisioning-templates/claim_0603/provision/json를 publish 하면 받아서 restart 하게 된다.  

  String subscriptionTopic = String(AWS_IOT_SUB_TOPIC) + WiFi.macAddress();
  char subTopic[100];
  subscriptionTopic.toCharArray(subTopic, 100);

  deserializeJson(doc, payload);

  String output;
  serializeJsonPretty(doc, output);
  Serial.print("messageHandler Received JSON: ");
  Serial.println(output);

  if (topic == "$aws/certificates/create/json/accepted")
  {
    saveCertificateToFS(doc);
    registerThing(doc);
  }
  else if (topic == "$aws/provisioning-templates/claim_0603/provision/json/accepted")
  {
    Serial.println("Register things successfully.");
    Serial.println("Restart in 5s.");
    sleep(5);
    ESP.restart();
  }
  else if(topic == subTopic){
    Serial.print("match subscription topic =");
    Serial.println(subTopic);
    on_message_received(topic, doc , length);
  }
}

void connectToAWS(DynamicJsonDocument cert)
{
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(cert["certificatePem"]);
  net.setPrivateKey(cert["privateKey"]);
  client.setServer(AWS_IOT_ENDPOINT, 8883);
  // Create a message handler
  client.setCallback(messageHandler);
  client.setBufferSize(10000);
  Serial.print("Connecting to AWS IOT.");

  String clientId = "ESP32_" + WiFi.macAddress(); // 고유한 클라이언트 ID 생성
  unsigned long startAttemptTime = millis();
  const unsigned long timeout = 5000;  // 5초 동안 시도
  
  // // 연결 시도
  while (!client.connected() && millis() - startAttemptTime < timeout) {
    Serial.print(".");
    client.connect(clientId.c_str());
    
    
  }

  if (client.connected()) {
    Serial.println("Connected");
    Serial.print("MQTT state: ");
    Serial.println(client.state());  // MQTT 클라이언트 상태 출력
    delay(100);
  } else {
    Serial.println("Timeout!");
    // 여기에 적절한 오류 처리 코드 추가 (예: 오류 메시지 출력, 재시도, 시스템 재시작 등)
    // ESP.restart();
  }

  delay(1000);

  // topic 합성 : ozs/client8266/ + mac 주소
  String subscriptionTopic = String(AWS_IOT_SUB_TOPIC) + WiFi.macAddress();
  char topic[100];
  subscriptionTopic.toCharArray(topic, 100);
  Serial.printf("Subscription topic: %s", topic);
  Serial.println();
  client.subscribe(topic);

  delay(2000);

  // 2024-06-04 : send wifi ready
  send_wifi_ready();

  // // 테스트 메시지 발행
  // String testTopic = String(AWS_IOT_PUB_TOPIC) + "_" + WiFi.macAddress();
  // char pubTopic[50];
  // testTopic.toCharArray(pubTopic, 50);
  // String payload = "{\"message\": \"Hello from ESP32 안녕하세요...\"}";
  // if (client.publish(pubTopic, payload.c_str()))
  // {
  //   Serial.printf("Publish to topic %s successful\n", pubTopic);
  // }
  // else
  // {
  //   Serial.printf("Publish to topic %s failed\n", pubTopic);
  // }
}


void publish_ozs_status(String &message){

  #ifdef DEBUG
  Serial.println("report ozs board status......." + message);
  #endif

  // ":" delimiter로 message를 분리
  int delimiterIndex = message.indexOf(':');
  String firstToken = message.substring(0, delimiterIndex);
  firstToken.trim();

  Serial.println("firstToke = " + firstToken);
  String secondToken = message.substring(delimiterIndex + 1);

  delimiterIndex = secondToken.indexOf(':');
  String status = secondToken.substring(0,delimiterIndex);
  String time = secondToken.substring(delimiterIndex+1);
  
  #ifdef DEBUG
  Serial.println("status = " + status);
  Serial.println("time = " + time);
  #endif


  StaticJsonDocument<200> doc;

  doc["status"] = status;
  doc["time"] = time;

  char publishBuffer[512];
  serializeJson(doc, publishBuffer); // print to client


// topic 합성 : ozs/client8266/ + mac 주소
  String publishTopic = String(AWS_IOT_PUB_TOPIC) + WiFi.macAddress();
  char pubTopic[100];
  publishTopic.toCharArray(pubTopic, 100);
  Serial.printf("publish topic: %s", pubTopic);


  if (client.publish(pubTopic, publishBuffer))
  {
    Serial.printf("Publish to topic %s successful\n", pubTopic);
  }
  else
  {
    Serial.printf("Publish to topic %s failed\n", pubTopic);
  }


  end_time = millis();

  unsigned long elapsedTime = end_time - start_time;
  Serial.print("Message handling and publishing took: ");
  Serial.print(elapsedTime);
  Serial.println(" ms");


}

void publish_ozs_system_info(String &message){
  
  Serial.println("report ozs system info = " + message);

  // ":" delimiter로 message를 분리
  int delimiterIndex = message.indexOf(':');
  String firstToken = message.substring(0, delimiterIndex);

#ifdef DEBUG
  Serial.println("firstToke = " + firstToken);
#endif
  String info = message.substring(delimiterIndex + 1);


  
  Serial.println("publish_ozs_system_info  info = " + info);

  String clientId = "" + WiFi.macAddress();
  
  StaticJsonDocument<200> doc;

  doc["info"] = info;
  doc["serial"] = clientId;
  
  // Data 합성
  char publishData[512];
  serializeJson(doc, publishData); // print to client

  // topic 합성 : ozs/client8266/ + mac 주소
  String publishTopic = String(AWS_IOT_PUB_TOPIC) + WiFi.macAddress();
  char pubTopic[100];
  publishTopic.toCharArray(pubTopic, 100);
  Serial.printf("publish topic: %s", pubTopic);


  client.publish(pubTopic, publishData);

 
  end_time = millis();

  unsigned long elapsedTime = end_time - start_time;
  Serial.print("Message handling and publishing took: ");
  Serial.print(elapsedTime);
  Serial.println(" ms");


}
void createCertificate()
{
  Serial.println("No file content.");

  //2024-06-03 : Provisioning devices with claim certificates, --> Connect / connect many devices에 있는 Provisioned devices with claim certificates를 선택하고 iam, policy, 인증서를 선택한다. 인증서는 Security/Certificates 에서 생성을 하고, device certificate, private key를 여기서 다운로드 받는다. 

  // Choose this option if your IoT devices are delivered with claim certificates that are shared with other devices. The devices use their claim certificates to connect to AWS IoT for the first time. The claim certificate is replaced with a unique device certificate after provisioning. This option is also known as fleet provisioning with certificate.


  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
  client.setServer(AWS_IOT_ENDPOINT, 8883);
  // Create a message handler
  client.setCallback(messageHandler);
  // Set buffer size for receive a certificate.
  client.setBufferSize(4000);
  Serial.println("Connecting to AWS IOT.");
  String clientId = "ESP32_" + WiFi.macAddress(); // 고유한 클라이언트 ID 생성
  Serial.println("clientId ");
  Serial.println(clientId);
  while (!client.connect(clientId.c_str())) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Connected");
  client.subscribe("$aws/certificates/create/json/accepted");
  client.subscribe("$aws/certificates/create/json/rejected");

  //2024-06-03 : 💇‍♀️💇‍♀️, 핵심은 claim_0603 이다. 이것이 Provisioning devices with claim certificates 에서 생성한 template 이름이다. 이 이름을 지정해야만 제대로 작동이 된다. 아니면 reject 이 된다. 
  client.subscribe("$aws/provisioning-templates/claim_0603/provision/json/accepted");
  client.subscribe("$aws/provisioning-templates/claim_0603/provision/json/rejected");
  Serial.println("Create certificate..");

  /*
  client.publish("$aws/certificates/create/json", "")의 역할
목적: client.publish("$aws/certificates/create/json", "")의 목적은 AWS IoT Core에 장치의 새로운 인증서를 생성하라는 요청을 보내는 것입니다. $aws/certificates/create/json 토픽은 이 토픽에 메시지가 발행되면 AWS IoT Core가 새로운 인증서를 생성하는 특별한 토픽입니다.

메커니즘:

AWS IoT Core가 이 토픽에서 발행 요청을 수신하면, 요청을 처리하고 장치에 대한 새로운 인증서를 생성합니다.
그런 다음 AWS IoT Core는 $aws/certificates/create/json/accepted (성공 시) 또는 $aws/certificates/create/json/rejected (오류 발생 시) 토픽에 메시지로 응답합니다.
응답 처리:

messageHandler 함수가 AWS IoT Core의 응답을 처리합니다.
응답 토픽이 $aws/certificates/create/json/accepted인 경우, saveCertificateToFS(doc)를 호출하여 수신된 인증서와 개인 키를 SPIFFS 파일 시스템에 저장하고 registerThing(doc)을 호출하여 장치를 등록합니다.
응답 토픽이 $aws/certificates/create/json/rejected인 경우, 필요한 오류 처리를 구현할 수 있습니다.
이 토픽에 발행함으로써, 함수는 AWS IoT로부터 새로운 인증서를 얻는 과정을 시작하며, 이는 IoT 환경에서 안전한 통신과 인증을 위해 매우 중요합니다.
  */
  client.publish("$aws/certificates/create/json", "");
}

void setup()
{
  Serial.begin(115200);

  // Note the format for setting a serial port is as follows: Serial2.begin(baud-rate, protocol, RX pin, TX pin);
 
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected.");

  // Init SPIFFS.
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Init NVS.
  preferences.begin("my-app", false);
  init_count = preferences.getInt("init_count", 0);
  
  Serial.print("init_count before initialization: ");
  Serial.println(init_count);

  if (init_count == 0) {
    // 2024-06-03 : 💇‍♀️ /aws.json 이 파일을 초기화 한다. 이것은 최초는 json 포맷 에러가 발생하도록 한다. 
    Serial.println("initialize awsJson file");
    initializeAwsJson();
  }

  // init_count++;
  // preferences.putInt("init_count", init_count);
  
  Serial.print("init_count : ");
  Serial.println(init_count);

  // Read AWS config file.
  File file = SPIFFS.open("/aws.json", "r");
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.println("succeed to open file for reading..");

  // 파일 내용을 Serial.println을 사용하여 터미널에 출력
  // while (file.available())
  // {
  //   String line = file.readStringUntil('\n');
  //   Serial.println(line);
  // }

  delay(1000);
  DynamicJsonDocument cert(4000);
  auto deserializeError = deserializeJson(cert, file);

  if (!deserializeError)
  {
      Serial.println("deserializeError false");
      if (cert["certificatePem"])
      {
        connectToAWS(cert);
      }
  }
  else 
  {
    //2024-06-03 : 최초는 여기로 온다. 에러를 일부러 발생한다. 
    Serial.println("deserializeError true");
    if(init_count == 0) {
      Serial.println("start createCertificate....");
      createCertificate();
      init_count++;
      preferences.putInt("init_count", init_count);
    }
    else{  
          Serial.print("init_count > 0 ");
          Serial.println(init_count);
          
    }
    
  }
  file.close();
}


void reconnectMQTT() {
  if (!client.connected()) {
    Serial.print("Reconnecting to MQTT...");
    while (!client.connected()) {
      Serial.print(".");
      String clientId = "ESP32_" + WiFi.macAddress();
      if (client.connect(clientId.c_str())) {
        Serial.println("Connected");
        // Resubscribe or perform other setup tasks
      } else {
        Serial.print("Failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        delay(5000);
      }
    }
  }
}

void ensureWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting...");
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    Serial.println("Reconnected to WiFi.");
  }
}

void loop()
{

  // ensureWiFiConnection();
  // reconnectMQTT();

  client.loop();


  // 2024-06-04 computer terminal로 들어온 Serial 명령 처리
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();  // 공백 제거
    if (command.equals("RESET_INIT_COUNT_0")) {
      init_count = 0;
      preferences.putInt("init_count", init_count);
      Serial.println("init_count has been reset to 0");
    }
    else if (command.equals("RESET_INIT_COUNT_1")) {
      init_count = 1;
      preferences.putInt("init_count", init_count);
      Serial.println("init_count has been reset to 1");
    }
  }
  


  // 2024-05-06 :SerialPort로부터 데이터를 읽어옴
  // OZS 보드에서 MQTT가 연결이 되었는지 확인 메세지를 처리한다. 
  // 확인 메세지 "hello 8226"
  // 2024-06-04 : ESP32로 upgrade함.

  while (Serial2.available() > 0) {
     // 시리얼 데이터 읽기
    char incomingChar = Serial2.read();

    // '['가 들어오면 내부 문자열 시작
    if (incomingChar == '[') {
      insideBrackets = true;
      receivedString = ""; // 새로운 문자열 시작
    }
    // ']'가 들어오면 내부 문자열 끝
    else if (incomingChar == ']') {
      insideBrackets = false;
      
      // 내부 문자열을 처리 (예: 시리얼 모니터로 출력)
      Serial.println(" Rx=" + receivedString);

      // 2024-05-06 : "8266" 문자열이 포함되어 있는지 확인하여 처리
      if (receivedString.indexOf("8266") != -1) {
        // "8266"이 포함되어 있으면 send_wifi_ready() 함수 호출
        send_wifi_ready();
      }
      else if(receivedString.indexOf("OZS") != -1){
        publish_ozs_status(receivedString);
      }
      else if(receivedString.indexOf("SYSINFO") != -1){
         publish_ozs_system_info(receivedString);
      }
      else{
        Serial.println("loop: critical error:");
      }


    }
    // '['와 ']' 사이의 문자열인 경우 receivedString에 문자 추가
    else if (insideBrackets) {
      receivedString += incomingChar;
    }
  }

  //2024-05-29 : 10ms 마다 한번씩 체크한다. 시간을 획기적으로 줄였다.
  delay(100);


}
