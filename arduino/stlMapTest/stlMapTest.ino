#include <map>
#include <string>

using namespace std;
typedef void (*fp)(void);

std::map<string, fp> callback;
std::map<string, fp>::iterator iter;

void callback1() {
  Serial.print("this is callback1 ");

}
void callback2() {
  Serial.print("this is callback2 ");

}

void setup() {
  Serial.begin(115200);

  callback.insert(pair<string, fp>("noti2", callback2));
  callback["noti1"] = callback1;
  Serial.println(callback.size());
}

void loop() {
  iter = callback.find("noti1");
  if(iter == callback.end()) 
    Serial.println("notification empty");
  else {
    fp mqttCallback = callback["noti1"];
    mqttCallback();
  }
  delay(1000); 
}
