#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include "thingPlug.h"

#define PIN_EXTRA_POWER   16
#define PIN_RELAY   15

const char* ssid = "edgeiLAB";
const char* password = "iotiotiot";

char addr[] = "mqtt.sktiot.com";
char id[] = "edgeilab";
char pw[] = "ZEwxMW9DZmNQK3dudWdRcTV4bVhEK1ByK3U2amtxU3NCWjE0OERNREI3QkUwdCtsSmhZWDQ4eGRURkd0NVFIUw==";
char deviceId[] = "edgeilab_MCU_Node_Relay";
char devicePw[] = "123456";
char containerLat[]= "Geolocation_latitude";
char containerLong[] = "Geolocation_longitude";
char targetDeviceId[] = "edgeilab_FireD_02";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void callbackFunctionLong(char * data)
{
  float longitude = 0;
  sscanf(data, "%f", &longitude);
  printf("callbackFunction: Longitude=%f\n", longitude);
} 

void callbackFunctionLat(char * data)
{
  float latitude = 0;
  sscanf(data, "%f", &latitude);
  printf("callbackFunction: Latitude=%f\n", latitude);
} 

void setup()
{
  pinMode(PIN_EXTRA_POWER, OUTPUT);
  digitalWrite(PIN_EXTRA_POWER, LOW); // Node MCU 동작 시작
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, HIGH);
    
  Serial.begin(115200);
  Wire.begin();
  WiFi.begin(ssid, password);
 
  // 연결 완료 까지 대기
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected to ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  if(!mqttConnect(&mqttClient, addr, id, pw, deviceId)) {
    printf("1. mqtt connect failed\n");
    while(1);
  }

  if(!mqttCreateNode(&mqttClient, devicePw)) {
    printf("2. mqtt create node failed\n");
    while(1);
  }

  if(!mqttCreateRemoteCSE(&mqttClient)) {
    printf("3. mqtt create remote cse failed\n");
    while(1);
  }
}

void loop()
{ 
    if(!mqttGetLatest(&mqttClient, targetDeviceId, containerLat, callbackFunctionLat)) {
    printf("6. mqtt get Latest Latitude notification failed\n");
    while(1);
  }  
  
  if(!mqttGetLatest(&mqttClient, targetDeviceId, containerLong, callbackFunctionLong)) {
    printf("6. mqtt get Latest Longitude notification failed\n");
    while(1);
  }
  mqttClient.loop();
  delay(1000);
}

