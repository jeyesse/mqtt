#include <Arduino.h>
#include "thingPlug.h"

typedef void (*mqttCallback)(char*);
using namespace std;
std::map<string, mqttCallback> callback;
std::map<string, mqttCallback>::iterator iter;
std::pair<string, string> latestData;
std::vector<pair<string, string>> latestDataVector(1000);

/* The pathes will be created dynamically */
char mqttPubTopic[BUF_SIZE_SMALL] = "";
char mqttSubTopic[BUF_SIZE_SMALL] = "";
char mqttPubPath[BUF_SIZE_SMALL]  = "";
char mqttRemoteCSE[BUF_SIZE_SMALL]  = "";
char mqttContainer[BUF_SIZE_SMALL]  = "";
char mqttSubscription[BUF_SIZE_SMALL] = "";
char mqttLatest[BUF_SIZE_SMALL] = "";

const char frameMqttPubTopic[]  = "/oneM2M/req_msg/%s/%s";  /* appEUI, deviceId */
const char frameMqttSubTopic[]  = "/oneM2M/resp/%s/%s";     /* deviceId, appEUI */
const char frameMqttPubPath[] = "/oneM2M/req/%s/%s";        /* deviceId, appEUI */
const char frameMqttRemoteCSE[] = "/%s/v1_0/remoteCSE-%s";    /* appEUI, deviceId */
const char frameMqttContainer[] = "%s/container-%s";        /* remoteCSE, container */
const char frameMqttSubscription[] = "%s/subscription-%s";  /* container, notifyName */
const char frameMqttLatest[] = "%s/latest"; /*container*/

char bufRequest[BUF_SIZE_LARGE] = "";
char strNL[BUF_SIZE_SMALL]    = "";
char strExt[BUF_SIZE_SMALL]   = "";
char strDkey[BUF_SIZE_SMALL]    = "";
char dataName[BUF_SIZE_SMALL]   = "";
char dataValue[BUF_SIZE_SMALL]  = "";
//void (*mqttCallback)(char*) = NULL;


static char address[BUF_SIZE_SMALL]    = "";
static char userName[BUF_SIZE_SMALL]   = "";
static char passWord[BUF_SIZE_SMALL]   = "";
static char deviceId[BUF_SIZE_SMALL]   = "";
static char passCode[BUF_SIZE_SMALL]   = "";
static char container[BUF_SIZE_SMALL]  = "";
static char targetDeviceId[BUF_SIZE_SMALL]  = "";

enum MqttStep step = CREATE_NODE;

/* step 1 - params: appEUI, deviceId, ri, deviceId, deviceId, deviceId */
const char frameCreateNode[] =
"<m2m:req>\
<op>1</op>\
<ty>14</ty>\
<to>/%s/v1_0</to>\
<fr>%s</fr>\
<ri>%s</ri>\
<cty>application/vnd.onem2m-prsp+xml</cty>\
<nm>%s</nm>\
<pc>\
<nod>\
<ni>%s</ni>\
<mga>MQTT|%s</mga>\
</nod>\
</pc>\
</m2m:req>";

/* step 2 - params: AppEUI, deviceId, ri, passCode, deviceId, deviceId, nl */
const char frameCreateRemoteCSE[] = 
"<m2m:req>\
<op>1</op>\
<ty>16</ty>\
<to>/%s/v1_0</to>\
<fr>%s</fr>\
<ri>%s</ri>\
<passCode>%s</passCode>\
<cty>application/vnd.onem2m-prsp+xml</cty>\
<nm>%s</nm>\
<pc>\
<csr>\
<cst>3</cst>\
<csi>%s</csi>\
<rr>true</rr>\
<nl>%s</nl>\
</csr>\
</pc>\
</m2m:req>";

/* step 3-params: cse, deviceId, ri, container, dKey */
const char frameCreateContainer[] = 
"<m2m:req>\
<op>1</op>\
<ty>3</ty>\
<to>%s</to>\
<fr>%s</fr>\
<ri>%s</ri>\
<cty>application/vnd.onem2m-prsp+xml</cty>\
<nm>%s</nm>\
<dKey>%s</dKey>\
<pc>\
<cnt>\
<lbl>con</lbl>\
</cnt>\
</pc>\
</m2m:req>";

/* step 4-params: appEUI, deviceId, ri, deviceId, dkey, strExt */
const char frameCreateMgmtCmd[] = 
"<m2m:req>\
<op>1</op>\
<ty>12</ty>\
<to>/%s/v1_0</to>\
<fr>%s</fr>\
<ri>%s</ri>\
<cty>application/vnd.onem2m-prsp+xml</cty>\
<nm>%s_turnOn</nm>\
<dKey>%s</dKey>\
<pc>\
<mgc>\
<cmt>turnOn</cmt>\
<exe>false</exe>\
<ext>%s</ext>\
</mgc>\
</pc>\
</m2m:req>";

/* step 4-1 - params: mqttContainer, deviceId, strRi, notificationName, uKey, deviceId */
const char frameSubscribe[] =
"<m2m:req>\
<op>1</op>\
<to>%s</to>\
<fr>%s</fr>\
<ty>23</ty>\
<ri>%s</ri>\
<nm>%s</nm>\
<uKey>%s</uKey>\
<cty>application/vnd.onem2m-prsp+xml</cty>\
<pc>\
<sub>\
<enc>\
<rss>1</rss>\
</enc>\
<nu>MQTT|%s</nu>\
<nct>2</nct>\
</sub>\
</pc>\
</m2m:req>";

/* step 5 - params: container, deviceId, ri, dkey, name, value */
const char frameCreateContentInstance[] = 
"<m2m:req>\
<op>1</op>\
<ty>4</ty>\
<to>%s</to>\
<fr>%s</fr>\
<ri>%s</ri>\
<cty>application/vnd.onem2m-prsp+xml</cty>\
<dKey>%s</dKey>\
<pc>\
<cin>\
<cnf>%s</cnf>\
<con>%s</con>\
</cin>\
</pc>\
</m2m:req>";

/* step 6 - params: container, deviceId, ri, dKey, deviceId */
const char frameDeleteSubscribe[] = 
"<m2m:req>\
<op>4</op>\
<to>%s</to>\
<fr>%s</fr>\
<ri>%s</ri>\
<uKey>%s</uKey>\
<cty>application/vnd.onem2m-prsp+xml</cty>\
<pc>\
<sub>\
<enc>\
<rss>1</rss>\
</enc>\
<nu>MQTT|%s</nu>\
<nct>2</nct>\
</sub>\
</pc>\
</m2m:req>";

/*step 6-2 - params: mqttLatest, deviceId, ri, uKey*/
const char frameLatest[] = 
"<m2m:req>\
<op>2</op>\
<to>%s</to>\
<fr>%s</fr>\
<ri>%s</ri>\
<cty>application/vnd.onem2m-prsp+xml</cty>\
<uKey>%s</uKey>\
</m2m:req>";

int mqttConnect(PubSubClient* client, char* addr, char* id, char* pw, char* devId) 
{
    strcpy(address, addr);
    strcpy(userName, id);
    strcpy(passWord, pw);
    strcpy(deviceId, devId);

    int rc;

    sprintf(mqttPubTopic, frameMqttPubTopic, APP_EUI, deviceId);
    sprintf(mqttSubTopic, frameMqttSubTopic, deviceId, APP_EUI);
    sprintf(mqttPubPath, frameMqttPubPath, deviceId, APP_EUI);
    sprintf(mqttRemoteCSE, frameMqttRemoteCSE, APP_EUI, deviceId);

    Serial.println(mqttPubTopic);
    Serial.println(mqttSubTopic);
    Serial.println(mqttPubPath);
    Serial.println(mqttRemoteCSE);   

    printf("Attempting MQTT connection...\n");
    client->setServer(addr, MQTT_BROKER_PORT);
    boolean result = client->connect(deviceId, id, pw);
    if (result)
    {
        printf("Mqtt connected\n");
      
        // registration of the topics
        Serial.println("MQTT connected");
        step = CREATE_NODE;
        if (client->subscribe(mqttSubTopic)) Serial.println("mqttSubTopic subscribed");
        else Serial.println("mqttSubTopic Not subscribed");
        
        if (client->subscribe(mqttPubTopic)) Serial.println("mqttPubTopic subscribed");
        else Serial.println("mqttPubTopic Not subscribed");

        client->setCallback(callbackArrived);
    }
    else 
    {
        printf("Failed to connect, return code %d\n", rc);
        return FALSE;
    }

    return TRUE;
}

void callbackArrived(char * topicName, uint8_t * payload, unsigned int len)
{
  Serial.print("MQTT Message arrived: Topic=");
  Serial.println(topicName);

  if(len>0) payload[len]='\0';
  printf("Payload(%d): ", len);
  Serial.println((char*)payload);

  // error check
  char strRsc[128]  = "";
  int rc = parseValue(strRsc, (char*)payload, len, "rsc");
  if(rc==MQTTCLIENT_SUCCESS )
  {
    printResultCode(strRsc);
    int resultCode = atoi(strRsc);
    if(resultCode == 4004) return;
    if(resultCode == 4000) return;
  
    char strRi[128] = "";
    generateRi(strRi);

    switch(step) {
    case CREATE_NODE_REQUESTED:
          // parse response message
          rc = parseValue(strNL, strstr((char*)payload, "pc"), len, "ri");
          if(rc==MQTTCLIENT_SUCCESS) {
        printf("ri:%s\n", strNL);
        strcpy(strExt, strNL);
        step = CREATE_REMOTE_CSE;
      }
      break;
  
    case  CREATE_REMOTE_CSE_REQUESTED : 
      rc = parseValue(strDkey, (char*)payload, len, "dKey");
      printf("dKey=%s\n", strDkey);
      if(rc==MQTTCLIENT_SUCCESS) {
        step = CREATE_CONTAINER;
            }
            break;
  
    case CREATE_CONTAINER_REQUESTED:
      step = CREATE_MGMT_CMD;
      break;
  
    case CREATE_MGMT_CMD_REQUESTED:
      step = SUBSCRIBE;
      break;
  
    case SUBSCRIBE_REQUESTED:
      step = CREATE_CONTENT_INSTANCE;
      break;
  
    case GET_LATEST_DATA_REQUESTED:
    {
    char strRi[128] = "";
    parseValue(strRi, (char*)payload, len, "ri");
    char strCon[128] = "";
    parseValue(strCon, (char*)payload, len, "con");
    string notificationName = "";
      
      for(int i = 0; i < latestDataVector.size(); i++) {
        latestData = latestDataVector.at(i);
        if(latestData.first.compare(strRi) == 0) {
          notificationName = latestData.second;
        }else continue;
      }
      printf("latestData callback : %s \t %s \t %s \n", strRi, notificationName.c_str(), strCon);
      mqttCallback callbackFunction;
      iter = callback.find(notificationName);
      if(iter != callback.end()) {
        callbackFunction = callback[notificationName];
      }else {
        callbackFunction = NULL;
      }
      if(strCon != NULL && callbackFunction != NULL) callbackFunction(strCon);
      step = FINISH;
      break;
    }
    default:
      step = FINISH;
      break;
    }
  }
  else
  {
    // Notification from ThingPlug Server
    char strCon[128] = "";
    rc = parseValue(strCon, (char*)payload, len, "con");

    char strRoute[128] = "";
    char notifySubName[128] = "";
    parseValue(strRoute, (char*)payload, len, "sr");
    char *pStrRoute = strtok(strRoute, "-");
    while (pStrRoute) {
      strncpy(notifySubName, pStrRoute, 128);
      pStrRoute = strtok(NULL, "-");
    }
    //iter를 이용한 반환 필요
    mqttCallback callbackFunction;
    iter = callback.find(notifySubName);
    if(iter != callback.end()) {
      callbackFunction = callback[notifySubName];
    }
    else 
      callbackFunction = NULL;
    if(rc==MQTTCLIENT_SUCCESS && callbackFunction!=NULL) callbackFunction(strCon);
  } 

  return; // Do Not Need to be recalled.
}

// generates a unique resource ID
void generateRi(char * buf)
{
  if(buf==NULL) return;
  sprintf(buf, "%s_%lu", deviceId, millis());
}

int parseValue(char* buf, const char * payload, const int len, const char * param)
{
  if(payload==NULL)
  {
    printf("parseValue error: Payload is NULL\n");
    return MQTTCLIENT_FAILURE;
  } 

  int result = MQTTCLIENT_FAILURE;
  int lenParam = strlen(param);
  
  char tagBegin[128]="";
  sprintf(tagBegin, "<%s>", param);
  
  char tagEnd[128];
  sprintf(tagEnd, "</%s>", param);

  char * pBegin = strstr(payload, tagBegin);
  if(pBegin==NULL) return result;
  int indexBegin = pBegin - payload;
  if(indexBegin>0){
     char* pEnd = strstr(payload, tagEnd);
     int indexEnd = pEnd - payload;
     int indexValue = indexBegin+lenParam+2;
     int lenValue = indexEnd-indexValue;
     strncpy(buf, &payload[indexValue], lenValue);
     buf[lenValue]='\0';
     result = MQTTCLIENT_SUCCESS;
  }

  return result;
}

void printResultCode(char * buf)
{
  if(buf==NULL) return;
  Serial.print("[result code]: ");
  
  int resultCode = atoi(buf);
  switch(resultCode) {
    case 2000: Serial.println("OK"); break;
    case 2001: Serial.println("CREATED"); break;
    case 2002: Serial.println("DELETED"); break;
    case 2004: Serial.println("UPDATED"); break;
    case 2100: Serial.println("CONTENT_EMPTY"); break;
    case 4105: Serial.println("EXIST"); break;
    case 4004: Serial.println("NOT FOUND"); break;
    default: Serial.println("UNKNOWN ERROR"); break;
  }
}

void checkMqttPacketSize() 
{
  if(MQTT_MAX_PACKET_SIZE<BUF_SIZE_LARGE)
  {
    sprintf(bufRequest, "\nERROR: MQTT_MAX_PACKET_SIZE(%d) is smaller than BUF_SIZE_LARGE(%d).", MQTT_MAX_PACKET_SIZE, BUF_SIZE_LARGE);
    Serial.println(bufRequest);
    sprintf(bufRequest, "Please modify MQTT_MAX_PACKET_SIZE for %d on Arduino\\libraries\\pubsubclient\\src\\PubSubClient.h\n\n", BUF_SIZE_LARGE);
    Serial.println(bufRequest);
    while(1){}
  }
  else
  {
    sprintf(bufRequest, "\nPacket Size OK: MQTT_MAX_PACKET_SIZE(%d), BUF_SIZE_LARGE(%d).", MQTT_MAX_PACKET_SIZE, BUF_SIZE_LARGE);
    Serial.println(bufRequest);
  }
}

int mqttCreateNode(PubSubClient* client, char * devPw)
{
    checkMqttPacketSize();

    strcpy(passCode, devPw);

    int rc = 0;
    char ri[100]="";
    generateRi(ri);

    sprintf(bufRequest,frameCreateNode, APP_EUI, deviceId, ri, deviceId, deviceId, deviceId);    
    printf("1. Create Node :\n");
    printf("payload=");
    Serial.println(bufRequest);

    // publish bufRequest
    step = CREATE_NODE_REQUESTED;
    if(client->publish(mqttPubPath, bufRequest) ) Serial.println("Publish success");
    else Serial.println("Publish failed");
    
    while(step == CREATE_NODE_REQUESTED) { client->loop(); }
    
    printf("Create Node Success\n\n");

    return TRUE;
}

int mqttCreateRemoteCSE(PubSubClient* client)
{
    int rc = 0;

    char strRi[100]="";
    generateRi(strRi);

    sprintf(bufRequest, frameCreateRemoteCSE, APP_EUI,
            deviceId, strRi, passCode, deviceId, deviceId, strNL);

    printf("2. Create RemoteCSE :\n payload=%s\n", bufRequest);
    step = CREATE_REMOTE_CSE_REQUESTED;
    client->publish(mqttPubPath, bufRequest);
    while(step == CREATE_REMOTE_CSE_REQUESTED) client->loop();
    printf("Publish Success\n\n");

    return TRUE;
}


int mqttCreateContainer(PubSubClient* client, char* con)
{
    strcpy(container, con);

    int rc = 0;

    char strRi[100]="";
    generateRi(strRi);

    sprintf(mqttContainer, frameMqttContainer, mqttRemoteCSE, container);
    sprintf( bufRequest, frameCreateContainer,
             mqttRemoteCSE, deviceId, strRi, container, strDkey);

    printf("3. Create Container :\n payload=%s\n", bufRequest);
    step = CREATE_CONTAINER_REQUESTED;
    client->publish(mqttPubPath, bufRequest);
    while(step == CREATE_CONTAINER_REQUESTED) client->loop();
    printf("Publish Success\n\n");

    return TRUE;
}

int mqttCreateMgmtCmd(PubSubClient* client)
{
    int rc = 0;

    char strRi[100]="";
    generateRi(strRi);

    sprintf( bufRequest, frameCreateMgmtCmd,
        APP_EUI, deviceId, strRi, deviceId, strDkey, strExt);

    printf("4. Create Mgmt Cmd :\n payload=%s\n", bufRequest);
    step = CREATE_MGMT_CMD_REQUESTED;
    client->publish(mqttPubPath, bufRequest);
    while(step == CREATE_MGMT_CMD_REQUESTED) client->loop();
    printf("Publish Success\n\n");

    return TRUE;
}

int mqttCreateContentInstance(PubSubClient* client, char* con, char * dataValue)
{
    int rc = 0;

    char strRi[128] = "";
    generateRi(strRi);

    strcpy(container, con);
    strcpy(dataName, "text"); // data type
    sprintf(mqttRemoteCSE, frameMqttRemoteCSE, APP_EUI, deviceId);
    sprintf(mqttContainer, frameMqttContainer, mqttRemoteCSE, container);
    sprintf(bufRequest, frameCreateContentInstance, 
        mqttContainer, deviceId, strRi, strDkey, dataName, dataValue);

    printf("5. Create Content Instance :\n payload=%s\n", bufRequest);
    step = CREATE_CONTENT_INSTANCE_REQUESTED;
    client->publish(mqttPubPath, bufRequest);
    while(step == CREATE_CONTENT_INSTANCE_REQUESTED) client->loop();
    printf("Publish Success\n\n");
    return TRUE;
}


int mqttSubscribe(PubSubClient* client, char* targetDevId, char* con, void (*fp)(char *))
{
    //mqttCallback = fp;

    int rc = 0;

    char strRi[128] = "";
    generateRi(strRi);
    char notifySubName[BUF_SIZE_SMALL] = "";
    strcpy(targetDeviceId, targetDevId);
    strcpy(container, con);
    strcat(notifySubName, container);
    strcat(notifySubName, targetDeviceId);
    
    callback.insert(pair<string, mqttCallback>(notifySubName, fp));

    sprintf(mqttRemoteCSE, frameMqttRemoteCSE, APP_EUI, targetDeviceId);
    sprintf(mqttContainer, frameMqttContainer, mqttRemoteCSE, container);

    sprintf( bufRequest, frameSubscribe,
             mqttContainer, deviceId, strRi, notifySubName, passWord, deviceId);

    printf("4-1. Subscribe :\n payload=%s\n", bufRequest);
    step = SUBSCRIBE_REQUESTED;
    client->publish(mqttPubPath, bufRequest);
    while(step == SUBSCRIBE_REQUESTED) client->loop();
    printf("Publish Success\n\n");

    return TRUE;
}

int mqttDeleteSubscribe(PubSubClient* client, char* targetDevId, char* con)
{
    char strRi[128] = "";
    generateRi(strRi);
    char notifySubName[BUF_SIZE_SMALL] = "";
    strcpy(container, con);
    strcpy(targetDeviceId, targetDevId);
    strcat(notifySubName, container);
    strcat(notifySubName, targetDeviceId);

    sprintf(mqttRemoteCSE, frameMqttRemoteCSE, APP_EUI, targetDeviceId);
    sprintf(mqttContainer, frameMqttContainer, mqttRemoteCSE, container);
    sprintf(mqttSubscription, frameMqttSubscription, mqttContainer, notifySubName);
    sprintf(bufRequest, frameDeleteSubscribe, mqttSubscription, deviceId, strRi, passWord, deviceId);

    printf("4-2. Delete Subscribe : \n payload=%s\n", bufRequest);
    step = DELETE_SUBSCRIBE_REQUESTED;
    client -> publish(mqttPubPath, bufRequest);
    while(step == DELETE_SUBSCRIBE_REQUESTED) client -> loop();
    printf("Publish Success\n\n");

    return TRUE;
}

int mqttGetLatest(PubSubClient* client, char* targetDevId, char* con, void(*fp)(char*)) {
    char strRi[128] = "";
    generateRi(strRi);
    strcpy(container, con);
    strcpy(targetDeviceId, targetDevId);

    char notificationName[BUF_SIZE_SMALL] = "";
    strcat(notificationName, container);
    strcat(notificationName, targetDeviceId);

    latestData = make_pair(strRi, notificationName);
    if(latestDataVector.size() >= 1000) {
      for(int i = 0; i < 500; i++) {
        latestDataVector.erase(latestDataVector.begin());
      }
    }
    latestDataVector.push_back(latestData);

    printf("latestData : %s \t %s\n", strRi, notificationName);
    callback.insert(pair<string, mqttCallback>(notificationName, fp));

    sprintf(mqttRemoteCSE, frameMqttRemoteCSE, APP_EUI, targetDeviceId);
    sprintf(mqttContainer, frameMqttContainer, mqttRemoteCSE, container);
    sprintf(mqttLatest, frameMqttLatest, mqttContainer);
    sprintf(bufRequest, frameLatest, mqttLatest, deviceId, strRi, passWord);

    printf("5. get Latest Data : \n payload = %s \n", bufRequest);
    step = GET_LATEST_DATA_REQUESTED;
    client -> publish(mqttPubPath, bufRequest);
    while(step == GET_LATEST_DATA_REQUESTED) client -> loop();
    printf("Publish success \n\n");

    return TRUE;

}
