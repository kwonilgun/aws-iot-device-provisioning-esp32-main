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
#include "util.h"
#include <string>

//2024-06-08 :  wifi manager 추가
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#include "async.h"

//2024-06-16 : Async WebServer 추가
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// 2025-02-05 DNS redirection
#include <DNSServer.h>

DNSServer dnsServer;

// SoftAP의 고정 IP 설정 (IP, Gateway, Subnet Mask)
IPAddress local_DNS(192,168,4,1);
IPAddress gateway_DNS(192,168,4,1);
IPAddress subnet_DNS(255,255,255,0);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

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

// 2024-07-10 : esp32 : software version 
const String ESP32_SW_VERSION = "0.0.9.4";

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


//2024-06-16 : Async server parameters......
// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";

//Variables to save values from HTML form
String ssid;
String pass;
String ip;
String gateway;
// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";
const char* gatewayPath = "/gateway.txt";

IPAddress localIP;
//IPAddress localIP(192, 168, 1, 200); // hardcoded

// Set your Gateway IP address
IPAddress localGateway;
//IPAddress localGateway(192, 168, 1, 1); //hardcoded
IPAddress subnet(255, 255, 0, 0);

// Timer variables
unsigned long previousMillis = 0;
const long interval = 20000;  // interval to wait for Wi-Fi connection (milliseconds)

String loop_start = "NO"; 

// 2024-06-18 : wifi 세팅이 제대로 되었는 지 확인하는 status 
// bool wifiConnectionSuccess = false;

// 2024-07-08 : softAP key 추가
const int switchPin = 4; // 스위치가 연결된 GPIO 핀
const unsigned long debounceDelay = 3000; // 3초 지연 시간 (밀리초)

unsigned long switchPressedTime = 0; // 스위치가 눌린 시간
bool switchState = LOW; // 현재 스위치 상태
bool lastSwitchState = LOW; // 마지막 스위치 상태

// 2025-02-05 : DNS redirection 지원


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
  // Serial.print("messageHandler Received JSON: ");
  // Serial.println(output);

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
    Serial.print("messageHandler: match subscription topic =");
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

  //💇‍♀️🧚‍♀️ 2024-06-04 Sets the keep alive interval used by the client. This value should only be changed when the client is not connected. 이것을 이용해서 keep alive 시간을 조정할 수 있다. 계속해서 c0 00 을 보낸다. 
  // /Users/kwon/.platformio/packages/framework-arduinoespressif32/libraries/WiFiClientSecure/src/ssl_client.cpp 에서  send_ssl_data(sslclient_context *ssl_client, const uint8_t *data, size_t len), 데이터를 계속해서 보낸다.

  // 참조 사이트: PubSubClient API : https://pubsubclient.knolleary.net/api#setKeepAlive


  //2024-06-05 : 15분에 한번씩 PINGREQ를 보낸다.  15분에 한번씩 ping을 한다. 
  client.setKeepAlive(15 * 60);

  
  
  if(client.setBufferSize(4000)){
    Serial.println("buffer resized");
  }
  else{
    Serial.println("buffer could not be resized");
  }
  Serial.print("Connecting to AWS IOT.");

  String clientId = "ESP32_" + WiFi.macAddress(); // 고유한 클라이언트 ID 생성
  unsigned long startAttemptTime = millis();
  const unsigned long timeout = 5000;  // 5초 동안 시도
  
  // 연결 시도
  while (!client.connected() && millis() - startAttemptTime < timeout) {
    Serial.print(".");
    if (client.connect(clientId.c_str())) { // 수정된 부분: client.connect()를 if 문으로 감싸서 연결 성공 여부 확인
      Serial.println("Connected");
      Serial.print("MQTT state: ");
      Serial.println(client.state());  // MQTT 클라이언트 상태 출력
      delay(100);
    }
  }

  if (!client.connected()) {
    Serial.println("Timeout!");
    // 여기에 적절한 오류 처리 코드 추가 (예: 오류 메시지 출력, 재시도, 시스템 재시작 등)
    // ESP.restart();
  }

  // delay(1000);

  // topic 합성 : ozs/client8266/ + mac 주소
  String subscriptionTopic = String(AWS_IOT_SUB_TOPIC) + WiFi.macAddress();
  char topic[100];
  subscriptionTopic.toCharArray(topic, 100);
  Serial.printf("Subscription topic: %s", topic);
  Serial.println();
  client.subscribe(topic);
  

  // delay(2000);

  // 2024-06-04 : send wifi ready
  send_wifi_ready();

  
}

void publish_ozs_status(String &message){

  #ifdef DEBUG
  Serial.println("report ozs board status......." + message);
  #endif

  // ":" delimiter로 message를 분리
  int delimiterIndex = message.indexOf(':');
  String firstToken = message.substring(0, delimiterIndex);
  firstToken.trim();

  // Serial.println("firstToke = " + firstToken);
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
  Serial.println();

//2024-06-05 : client.publish(pubTopic, publishBuffer, 0), the third parameter 0 specifies that the message should be delivered with QoS 0 (at most once).
  if (client.publish(pubTopic, publishBuffer))
  {
    Serial.printf("Publish to topic %s successful\n", pubTopic);
    Serial.println();
  }
  else
  {
    Serial.printf("Publish to topic %s failed\n", pubTopic);
    Serial.println();
  }


  end_time = millis();

  unsigned long elapsedTime = end_time - start_time;
  Serial.print("\n Message handling and publishing took: ");
  Serial.print(elapsedTime);
  Serial.println(" ms");


}

void publish_ozs_system_info(String &message){
  
  // Serial.println("report ozs system info = " + message);

  // ":" delimiter로 message를 분리
  int delimiterIndex = message.indexOf(':');
  String firstToken = message.substring(0, delimiterIndex);

#ifdef DEBUG
  // Serial.println("firstToke = " + firstToken);
#endif
  String info = message.substring(delimiterIndex + 1);


  
  Serial.println("publish_ozs_system_info  info = " + info);

  String clientId = "" + WiFi.macAddress();
  
  StaticJsonDocument<200> doc;

  doc["info"] = info;
  doc["serial"] = clientId;
  // 2024-06-10 : esp32 software version
  doc["esp"] = GetIniString("software", "version", "0");
  
  // Data 합성
  char publishData[512];
  serializeJson(doc, publishData); // print to client

  // topic 합성 : ozs/client8266/ + mac 주소
  String publishTopic = String(AWS_IOT_PUB_TOPIC) + WiFi.macAddress();
  char pubTopic[100];
  publishTopic.toCharArray(pubTopic, 100);
  Serial.printf("publish topic: %s", pubTopic);
  Serial.println();

  // client.publish(pubTopic, publishData, 0);

  //2024-06-05 : client.publish(pubTopic, publishBuffer, 0), the third parameter 0 specifies that the message should be delivered with QoS 0 (at most once).
  if (client.publish(pubTopic, publishData))
  {
    Serial.printf("Publish to topic %s successful\n", pubTopic);
  }
  else
  {
    Serial.printf("Publish to topic %s failed\n", pubTopic);
  }

  end_time = millis();

  unsigned long elapsedTime = end_time - start_time;
  Serial.print("\nMessage handling and publishing took: ");
  Serial.print(elapsedTime);
  Serial.println(" ms");

}

// 2024-06-19 : esp32 연결 체크
void publishMessageConnection(){
  
  Serial.println("publishMessageConnection ....");

  String clientId = "" + WiFi.macAddress();
  
  StaticJsonDocument<200> doc;

  doc["connect"] = "success";
  doc["mac"] = clientId;
  
  // Data 합성
  char publishData[512];
  serializeJson(doc, publishData); // print to client

  // topic 합성 : ozs/client8266/ + mac 주소
  String publishTopic = String(AWS_IOT_PUB_TOPIC) + WiFi.macAddress();
  char pubTopic[100];
  publishTopic.toCharArray(pubTopic, 100);
  Serial.printf("publish connection topic: %s \r\n", pubTopic);
  

  //2024-06-05 : client.publish(pubTopic, publishBuffer, 0), the third parameter 0 specifies that the message should be delivered with QoS 0 (at most once).
  if (client.publish(pubTopic, publishData))
  {
    Serial.printf("Publish to topic %s successful\n", pubTopic);
  }
  else
  {
    Serial.printf("Publish to topic %s failed\n", pubTopic);
  }

  // end_time = millis();

  // unsigned long elapsedTime = end_time - start_time;
  // Serial.print("\nMessage handling and publishing took: ");
  // Serial.print(elapsedTime);
  // Serial.println("ms");
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


void reconnect() {
  // 연결 시도 및 실패 시 재시도
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // 클라이언트 ID 설정
    String clientId = "ESP32_" + WiFi.macAddress(); // 고유한 클라이언트 ID 생성
    // clientId += String(random(0xffff), HEX);
    // // MQTT 브로커에 연결 시도
    if (client.connect(clientId.c_str())) {
      Serial.println("reconnected");
      // topic 합성 : ozs/client8266/ + mac 주소
        String subscriptionTopic = String(AWS_IOT_SUB_TOPIC) + WiFi.macAddress();
        char topic[100];
        subscriptionTopic.toCharArray(topic, 100);
        Serial.printf("reSubscription topic: %s", topic);
        Serial.println();
        client.subscribe(topic);

        // 2024-06-04 : send wifi ready
        send_wifi_ready();

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}


bool initWiFi(String cont, String ssid_str, String password) {
  // NVS 초기화
  preferences.begin("WiFiPrefs", false);

  // 이전 재시도 횟수 불러오기
  int restartCount = preferences.getInt("restartCount", 0);
  Serial.println("restartCount");
  Serial.println(restartCount);

  if (restartCount >= 10) {
    Serial.println("Exceeded maximum restart attempts. Connection failed.");
    preferences.putInt("restartCount", 0);
    preferences.end(); // NVS 닫기
    return false;
  }

  if ((ssid_str == "") || (cont == "init_ap")) {
    Serial.println("Undefined SSID or IP address.");
    preferences.end(); // NVS 닫기
    return false;
  }

  Serial.println("ssid_str =");
  Serial.println(ssid_str);
  Serial.println("password =");
  Serial.println(password);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid_str, password);

  delay(3000); // WiFi 연결 대기
  Serial.println("Checking WiFi status...");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("=======WL_CONNECTED========");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // 연결 성공 시 재시도 횟수 초기화
    preferences.putInt("restartCount", 0);
    preferences.end();
    return true;
  } else if (WiFi.status() == WL_IDLE_STATUS) {
    
    Serial.println("=======WL_IDLE========");
    Serial.print(restartCount);
    
    Serial.println("Restarting ESP...");

    // 재시도 횟수 증가 및 저장
    restartCount++;
    preferences.putInt("restartCount", restartCount);
    preferences.end();
    ESP.restart();
    
  } else {
    Serial.println("WiFi connection failed.");
    preferences.putInt("restartCount", 0);
    preferences.end();
    return false;
  }
}



void gotoSoftApSetup() {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point

    // SoftAP 시작 (고정 IP 설정 추가)
    WiFi.softAPConfig(local_DNS, gateway_DNS, subnet_DNS);
    WiFi.softAP("ROOTONE-AI-AP", "ROOTONE-AI-PWD");

    // WiFi.softAPsetHostname("ESP32_AP");  // AP 이름 설정
    WiFi.setSleep(false);   
    

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // DNS 서버 시작
    dnsServer.start(53, "*", IP); // 모든 도메인을 AP의 IP로 리다이렉션

    server.serveStatic("/", SPIFFS, "/");

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("http_get /....");
        request->send(200, "text/plain", "Wifi manager connection success");
    });

    server.onNotFound([](AsyncWebServerRequest *request){
        Serial.printf("Not found: %s\n", request->url().c_str());
        request->send(404, "text/plain", "Not found");
    });

    server.on("/mac", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("http_get /mac ....");
        String mac = WiFi.macAddress();
        delay(1000);
        request->send(200, "text/plain", mac);
    });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("http_get /status rx....");
    });

   

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.println("http post rx");
        int params = request->params();
        Serial.printf("http post rx params = %d \n", params);

        for(int i = 0; i < params; i++) {
            AsyncWebParameter* p = request->getParam(i);
            if(p->isPost()) {
                if (p->name() == PARAM_INPUT_1) {
                    ssid = p->value().c_str();
                    Serial.print("SSID set to: ");
                    Serial.println(ssid);
                    writeFile(SPIFFS, ssidPath, ssid.c_str());
                }
                if (p->name() == PARAM_INPUT_2) {
                    pass = p->value().c_str();
                    Serial.print("Password set to: ");
                    Serial.println(pass);
                    writeFile(SPIFFS, passPath, pass.c_str());
                }
                if (p->name() == PARAM_INPUT_3) {
                    ip = p->value().c_str();
                    Serial.print("IP Address set to: ");
                    Serial.println(ip);
                    writeFile(SPIFFS, ipPath, ip.c_str());
                }
                if (p->name() == PARAM_INPUT_4) {
                    gateway = p->value().c_str();
                    Serial.print("Gateway set to: ");
                    Serial.println(gateway);
                    writeFile(SPIFFS, gatewayPath, gateway.c_str());
                }
            }
        }
        request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
        SetIniString("softap", "ssid", "operate");
        delay(2000);
        ESP.restart();
    });

    server.begin();
}


void deleteFile(fs::FS &fs, const char * path) {
  Serial.printf("Deleting file: %s\n", path);

  // 파일 삭제 시도
  if (fs.remove(path)) {
    Serial.println("File deleted successfully");
  } else {
    Serial.println("Failed to delete file");
  }
}

void deleteAllFiles(){
  // 파일 삭제 함수 호출
  deleteFile(SPIFFS, ssidPath);
  deleteFile(SPIFFS, passPath);
  deleteFile(SPIFFS, ipPath);
  deleteFile(SPIFFS, gatewayPath);
}

boolean checkSoftApKey3Sec() {

  int currentSwitchState = digitalRead(switchPin);

  // 스위치 상태가 변경된 경우 처리
  if (currentSwitchState != lastSwitchState) {
    // 스위치가 눌린 상태로 변경된 경우
    if (currentSwitchState == HIGH) {
      switchPressedTime = millis();
    }
    // 스위치가 해제된 상태로 변경된 경우
    else if (currentSwitchState == LOW) {
      switchState = LOW;
    }
    // 마지막 스위치 상태 업데이트
    lastSwitchState = currentSwitchState;
  }

  // 스위치가 눌린 상태를 3초 동안 유지하는지 확인
  if (currentSwitchState == HIGH && (millis() - switchPressedTime >= debounceDelay)) {
    switchState = HIGH;
  } else {
    // 스위치가 눌린 상태가 아니거나 debounceDelay를 충족하지 못한 경우
    return false;
  }

  // 현재 스위치 상태에 따라 시리얼 모니터에 출력
  if (switchState == HIGH) {
    Serial.println("Switch is ON");
    return true;
  } else {
    // 이 부분은 실행되지 않지만 안전을 위해 남겨둠
    return false;
  }
}

void readSwitchPort() {
  // 스위치 상태 읽기
  int switchState = digitalRead(switchPin);

  // 스위치 상태에 따라 시리얼 모니터에 출력
  if (switchState == HIGH) {
    Serial.println("Switch is ON");
  } else {
    Serial.println("Switch is OFF");
  }

  // 500ms 대기
  delay(500);
}

void setup()
{

  
  Serial.begin(115200);
  delay(500);

  // 스위치 핀을 입력 모드로 설정
  pinMode(switchPin, INPUT);

  // Note the format for setting a serial port is as follows: Serial2.begin(baud-rate, protocol, RX pin, TX pin);
 
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

  initSPIFFS();

  // WiFi.mode(WIFI_STA);
  // WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi..");

  Serial.println("start setup..");

  String check_ssid =  GetIniString("softap", "ssid", "init_ap");
  Serial.println(check_ssid);

   // Load values saved in SPIFFS
  ssid = readFile(SPIFFS, ssidPath);
  pass = readFile(SPIFFS, passPath);
  ip = readFile(SPIFFS, ipPath);
  gateway = readFile (SPIFFS, gatewayPath);
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);
  Serial.println(gateway);

  // 2024-07-09 : version 초기화, 초기화하지 않고 읽으면 에러가 발생한다. 
  SetIniString("software", "version", ESP32_SW_VERSION.c_str());

  // 2025-02-05 : wifi 세팅을 
  preferences.putInt("restartCount", 0);

  if (check_ssid == "init_ap") {
    // No stored SSID, start WiFiManager
    Serial.println("check_ssid is init_ap ....");
    loop_start = "NO";
    send_setup_voice_stm();     //음성으로 softAp 시작을 알려준다. 
    Serial.println("Soft AP mode begins .....");
    gotoSoftApSetup();

  } 
  else if(ssid == ""){
    Serial.println(" ssid is empty  ....");
    loop_start = "NO";
    gotoSoftApSetup();
  }
  else {
    if(initWiFi("operate", ssid, pass)){
          // Route for root / web page
              Serial.println("initWiFi OK");
              loop_start = "YES_OP";
              
              Serial.println("Connected.");

              // Read AWS config file.
              File file = SPIFFS.open("/aws.json", "r");
              if (!file)
              {
                Serial.println("Failed to open file for reading");
                return;
              }

              Serial.println("\nsucceed to open file for reading..");

              // delay(1000);

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
                  Serial.println("start createCertificate....");
                  createCertificate();
              }
              file.close();

      }
      else{

          Serial.println("***********");
          Serial.println(" !!!!!!!connection fail and restart ESP!!!!!!");
          Serial.println("***********");

          // 2024-06-18 : 💇‍♀️ !!!!!!! 10초 동안 와이파이를 연결했지만 안됨. 다시 처음 부터 시작으 해야한다. 
          deleteAllFiles();

          SetIniString("softap", "ssid", "init_ap");


          // wifiConnectionSuccess = false; // Set flag to false on failure
          delay(2000); // Add a delay to ensure the HTTP response can be sent before restart

        
          ESP.restart();
      
      }
  }

}

void loop()
{

    
  if(loop_start == "YES_OP"){

        if (!client.connected()) {
            Serial.println("\n\n>>>>>>>>>>client connected error occurs..Restart.<<<<<<<<<<<<<\n\n");
            ESP.restart();
            return;
        }
        client.loop();

        // server.handleClient(); // 클라이언트 요청 처리

        // readSwitchPort();

      // 2024-07-09 : softAP 키를 3초 동안 누르면 softAP 모드로 진입한다. 
      if (checkSoftApKey3Sec() ) {

        loop_start = "NO";
        Serial.println("soft ap key pressed above 3 sec and YES_OP");

        SetIniString("softap", "ssid", "init_ap");
        deleteAllFiles();
        ESP.restart();

      };

     
  }
  else{
    // DNS 요청 처리
    dnsServer.processNextRequest();

  }
  

// 2024-06-04 computer terminal로 들어온 Serial 명령 처리
  if (Serial.available()) {

    // // 2024-07-25 : '\r', '\n' 이 없는 경우 무한 루프에 빠지지 않도록 한다.  
    Serial.setTimeout(3000);  // Set a timeout of 1000 milliseconds (1 second)

    String command = Serial.readStringUntil('\n');
    command.trim();  // 공백 제거


    // Check if the command starts with "write serial#"
    if (command.startsWith("write:")) {
      // ":" delimiter로 message를 분리
      int delimiterIndex = command.indexOf(':');
      String version = command.substring(delimiterIndex + 1);
      version.trim();
      Serial.printf("\nwrite version %s to flash\n", version.c_str());
      SetIniString("software", "version", version);
    }
   
    else if(command.equals("read")){
      // Serial.println("read software version : ");
      // Serial.println(GetIniString("software", "version", "0"));
      Serial.printf("\nread version %s from flash\n", GetIniString("software", "version", "0").c_str());
      delay(100);
     
    }

    // 2024-06-09 : 증명서를 다시 발행하는 명령, 증명서가 한번 세팅이 되면 다시는 할 필요가 없다. 
    else if (command.equals("reset cert")) {
    
      initializeAwsJson();

      Serial.println("reset certification");
    }

    // 2024-06-09 : stm 보드에서 추가 구현, softAP reset이 오면 이렇게 처리를 하면 된다. 추가 구현
    else if(command.equals("reset")){
        Serial.println("\n\n reset softAp  \n\n");
        
        SetIniString("softap", "ssid", "init_ap");
        deleteAllFiles();

        ESP.restart();
        
    }
    
    //2024-06-09 : mac address를 얻어낸다. 
    else if(command.equals("mac")){
        String macAddress = WiFi.macAddress();
        
        if (macAddress.length() == 0) {
        Serial.println("\n\nmac address : 없음");
        } else {
            char macAdd[100];
            macAddress.toCharArray(macAdd, 100);
            Serial.printf("\n\nmac address : %s \n", macAdd);
        }
        
        // char macAdd[100];
        // macAddress.toCharArray(macAdd, 100);
        // Serial.printf("\n\nmac address : %s", macAdd);
    }

    //2024-06-09 : mac address를 얻어낸다. 
    else if(command.equals("read mode")){
        // String macAddress = WiFi.macAddress();
        // char macAdd[100];
        // macAddress.toCharArray(macAdd, 100);
        if(loop_start == "YES_OP"){
              Serial.printf("\n\n **** normal mode state *** \n");
        } else {
              Serial.printf("\n\n **** Soft AP mode state *** \n");
        }
        
    }

    else if (command.equals("disconnect mqtt")){
      Serial.println("\nmqtt disconnect test.. \n");
      client.disconnect();
      delay(200);
    }
    
    
    else if(command.equals("set softap")){
        Serial.println("\n\n set softAp  \n\n");
        SetIniString("softap", "ssid", "operate");
        
    }
    else if(command.equals("get softap")){

      // String getId = GetIni("softap", "ssid", "none");
      Serial.println("get softap : \n");
      Serial.println(GetIniString("softap", "ssid", "none"));

    }
    else{
      Serial.println("input command error...\n");
      delay(2000);
    }

    //  // Clear the input buffer after processing the command
    while (Serial.available()) {
        Serial.read();
    }
    delay(100);
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
        Serial.println("Serial2: critical error:!!!!!!");
      }


    }
    // '['와 ']' 사이의 문자열인 경우 receivedString에 문자 추가
    else if (insideBrackets) {
      receivedString += incomingChar;
    }
  }

  //2024-05-29 : 10ms 마다 한번씩 체크한다. 시간을 획기적으로 줄였다.
  delay(10);
}


