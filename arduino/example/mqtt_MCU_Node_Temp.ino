#include <SparkFunHTU21D.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include "thingplug.h"

#define PIN_EXTRA_POWER   16
 
const char* ssid = "edgeiLAB";
const char* password = "iotiotiot";

char addr[] = "mqtt.sktiot.com";
char id[] = "sjs1210";
char pw[] = "WEZsQytadnBaOGNWandsMzEvK0ZYVlpMbm5QVXNsYmhGb0ZvU20vbVVGTFlCUEVZNGdFZDdhaHRoTDNTeHpYWA==";
char deviceId[] = "sjs1210_MCU_Node_Temp";
char devicePw[] = "123456";
char containerTemp[]= "temperature";
char containerHumi[] = "humidity";
char targetDeviceId[] = "sjs1210_iot_em_python";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
HTU21D sht20;

void mqttPublish(){
  float temp = sht20.readTemperature();
  float humi = sht20.readHumidity();
  char strTemp[BUF_SIZE_SMALL];
  char strHumi[BUF_SIZE_SMALL];
  sprintf(strTemp, "%f", temp);
  sprintf(strHumi, "%f", humi);
  mqttCreateContentInstance(&mqttClient, containerTemp, strTemp);
  mqttCreateContentInstance(&mqttClient, containerHumi, strHumi);
}

void callbackFunctionTemp(char * data)
{
  float temp = 0;
  sscanf(data, "%f", &temp);
  printf("callbackFunction: temp=%f\n", temp);
} 

void callbackFunctionHumi(char * data)
{
  float humi = 0;
  sscanf(data, "%f", &humi);
  printf("callbackFunction: humi=%f\n", humi);
} 

void setup()
{
  pinMode(PIN_EXTRA_POWER, OUTPUT);
  digitalWrite(PIN_EXTRA_POWER, LOW); // Node MCU 동작 시작
  
  sht20.begin();
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

  if(!mqttCreateContainer(&mqttClient, containerTemp)) {
    printf("4. mqtt create container failed\n");
    while(1);
  }
  
  if(!mqttCreateContainer(&mqttClient, containerHumi)) {
    printf("4. mqtt create container failed\n");
    while(1);
  }
  
  if(!mqttCreateMgmtCmd(&mqttClient)) {
    printf("5. mqtt create mgmt cmd failed\n");
    while(1);
  }

  if(!mqttSubscribe(&mqttClient, targetDeviceId, containerTemp, callbackFunctionTemp)) {
    printf("6. mqtt subscribe notification failed\n");
    while(1);
  }

  if(!mqttSubscribe(&mqttClient, targetDeviceId, containerHumi, callbackFunctionHumi)) {
    printf("6. mqtt subscribe notification failed\n");
    while(1);
  }

//  if(!mqttDeleteSubscribe(&mqttClient, targetDeviceId, containerTemp)) {
//    printf("7. mqtt Delete subscribe Temp Failed\n");
//    while(1);
//  }

}

void loop()
{ 
  static unsigned long curruntTime = millis();
  while(curruntTime + 10000 > millis()){
    mqttClient.loop();
  }
  mqttPublish();
  curruntTime += 10000;
}

