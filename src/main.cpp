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

WiFiClientSecure net;
PubSubClient client(net);

Preferences preferences;
int init_count = 0;

void initializeAwsJson();
void saveCertificateToFS(DynamicJsonDocument doc);
void registerThing(DynamicJsonDocument doc);
void messageHandler(String topic, byte *payload, int length);
void connectToAWS(DynamicJsonDocument cert);
void createCertificate();

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
  Serial.print("incoming: ");
  Serial.println(topic);
  DynamicJsonDocument doc(length);

  //2024-06-03 : payload에 provision에서 발행된 인증서가 실려 온다. 이것을 /aws.json 파일에 저장을 하고 , 저장된 파일을 이용해서 iot 관련 서비스를 진행하면 된다. 먼저 $aws/certificates/create/json/accepted, 받아서 실행을하고, registerThings에서 $aws/provisioning-templates/claim_0603/provision/json를 publish 하면 받아서 restart 하게 된다.  

  deserializeJson(doc, payload);
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
}

void connectToAWS(DynamicJsonDocument cert)
{
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(cert["certificatePem"]);
  net.setPrivateKey(cert["privateKey"]);
  client.setServer(AWS_IOT_ENDPOINT, 8883);
  // Create a message handler
  client.setCallback(messageHandler);
  client.setBufferSize(4000);
  Serial.print("Connecting to AWS IOT.");

  String clientId = "ESP32_" + WiFi.macAddress(); // 고유한 클라이언트 ID 생성
  client.connect(clientId.c_str());
  if (!client.connected())
  {
    Serial.println("Timeout!");
    return;
    // ESP.restart();
  }
  Serial.println("Connected");
  String subscriptionTopic = String(AWS_IOT_SUB_TOPIC) + "_" + WiFi.macAddress();
  char topic[50];
  subscriptionTopic.toCharArray(topic, 50);
  Serial.printf("Subscription topic: %s", topic);
  Serial.println();
  client.subscribe(topic);

  // 테스트 메시지 발행
  String testTopic = String(AWS_IOT_PUB_TOPIC) + "_" + WiFi.macAddress();
  char pubTopic[50];
  testTopic.toCharArray(pubTopic, 50);
  String payload = "{\"message\": \"Hello from ESP32 안녕하세요...\"}";
  if (client.publish(pubTopic, payload.c_str()))
  {
    Serial.printf("Publish to topic %s successful\n", pubTopic);
  }
  else
  {
    Serial.printf("Publish to topic %s failed\n", pubTopic);
  }
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
          // if (cert["certificatePem"])
          // {
          //   Serial.println("cert data exits");
          //   connectToAWS(cert);
          // }
          // else{
          //   Serial.println("cert data doesn't exits");
          // }
    }
    
  }
  file.close();
}

void loop()
{
  client.loop();

  // Serial 명령 처리
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
}
