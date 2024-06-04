/*
 * File: config.h
 * Project: include
 * File Created: Thursday, 28th July 2022 5:15:25 pm
 * Author: Kwonilgun(권일근) (kwonilgun@naver.com)
 * -----
 * Last Modified: Monday, 3rd June 2024 8:13:35 pm
 * Modified By: Kwonilgun(권일근) (kwonilgun@naver.com>)
 * -----
 * Copyright <<projectCreationYear>> - 2024 루트원 AI, 루트원 AI
 * 
 * 2024-06-03 : 통합 버전으로 수정, aws server와 local server의 토픽을 구별함.
 */



// 2024-05-29 define asw iot server, 로컬용일 때는 block 하면된다. 
// #define AWS_IOT_SERVER


// WiFi Configuration
#define WIFI_SSID "TIQUAD"       // <= Edit WiFi SSID.
#define WIFI_PASSWORD "na1048ho" // <= Edit WiFi password.

// const char WIFI_SSID[] = "U+NetC610";
// const char WIFI_PASSWORD[] = "191B1J5D@6";

// Amazon Certificate

// 2024-06-03 : sub, pub topic 을 변경
#define AWS_IOT_SUB_TOPIC "ozs/client8266/"

#ifdef AWS_IOT_SERVER
     #define AWS_IOT_PUB_TOPIC "ozs/awsServerIot/"
#else
     #define AWS_IOT_PUB_TOPIC "ozs/awsServerLocalIot/"
#endif

#define AWS_IOT_ENDPOINT "a9gxi0conitbh-ats.iot.ap-northeast-2.amazonaws.com" // <= Edit AWS IoT Endpoint.
extern const char AWS_CERT_CA[] asm("_binary_certs_ca_pem_start");
extern const char AWS_CERT_CRT[] asm("_binary_certs_cert_pem_start");
extern const char AWS_CERT_PRIVATE[] asm("_binary_certs_private_pem_start");
