

#include <ArduinoJson.h>

void initializeAwsJson();
void saveCertificateToFS(DynamicJsonDocument doc);
void registerThing(DynamicJsonDocument doc);
void messageHandler(String topic, byte *payload, int length);
void connectToAWS(DynamicJsonDocument cert);
void createCertificate();
void deleteAllFiles();
void deleteFile(fs::FS &fs, const char * path);
void publishMessageConnection();
