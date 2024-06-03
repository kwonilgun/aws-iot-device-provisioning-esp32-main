// WiFi Configuration
#define WIFI_SSID "TIQUAD"       // <= Edit WiFi SSID.
#define WIFI_PASSWORD "na1048ho" // <= Edit WiFi password.

// const char WIFI_SSID[] = "U+NetC610";
// const char WIFI_PASSWORD[] = "191B1J5D@6";

// Amazon Certificate
#define THING_GROUP "OZS_2024_0613"
// #define THING_GROUP "auth_user_0602"
#define AWS_IOT_SUB_TOPIC "cmd/esp32dkc"
#define AWS_IOT_PUB_TOPIC "tele/esp32dkc"
#define AWS_IOT_ENDPOINT "a9gxi0conitbh-ats.iot.ap-northeast-2.amazonaws.com" // <= Edit AWS IoT Endpoint.
extern const char AWS_CERT_CA[] asm("_binary_certs_ca_pem_start");
extern const char AWS_CERT_CRT[] asm("_binary_certs_cert_pem_start");
extern const char AWS_CERT_PRIVATE[] asm("_binary_certs_private_pem_start");
