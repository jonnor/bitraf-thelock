#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266WiFi.h>

#include <PubSubClient.h>
#include <Msgflo.h>

#include "./Config.h"

struct Config {
  const String role = "public/toollock/1";

  const int ledPin = D4;
  const int sensorPin = D5;
  const int lockPin = D0;

  const String wifiSsid = WIFI_SSID;
  const String wifiPassword = WIFI_PASSWORD;

  const char *mqttHost = "mqtt.bitraf.no";
  //  const char *mqttHost = "10.13.37.127";
  const int mqttPort = 1883;

  const char *mqttUsername = "myuser";
  const char *mqttPassword = "mypassword";
} cfg;

enum lockState {
  locked = 0, takeTool, toolTaken, lockInit, n_states
};

const char *stateNames[n_states] = {
  "locked",
  "takeTool",
  "toolTaken",
  "lockInit"
};

long keyCheck = 0;
lockState state = lockState::lockInit;
int takeToolCheck = 0;
int takeToolTimeout = 0;

WiFiClient wifiClient;
PubSubClient mqttClient;
msgflo::Engine *engine;
msgflo::InPort *lockPort;
auto participant = msgflo::Participant("iot/TheLock", cfg.role);

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println();
  Serial.println();

  Serial.printf("Configuring wifi: %s\r\n", cfg.wifiSsid.c_str());
  WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPassword.c_str());
  participant.icon = "lock";

  mqttClient.setServer(cfg.mqttHost, cfg.mqttPort);
  mqttClient.setClient(wifiClient);

  String clientId = cfg.role;
  clientId += WiFi.macAddress();

  engine = msgflo::pubsub::createPubSubClientEngine(participant, &mqttClient, clientId.c_str());

  //  lockPort = engine->addOutPort("button-event", "any", cfg.role + "/event");

  digitalWrite(cfg.lockPin, LOW);

  lockPort = engine->addInPort("lock", "boolean", cfg.role + "/lock",
  [](byte * data, int length) -> void {
    const std::string in((char *)data, length);
    const boolean on = (in == "1" || in == "true");
    Serial.printf("data: %s\n", in.c_str());
    if (!on && state == lockState::locked) {
//      digitalWrite(cfg.lockPin, LOW);
      state = lockState::takeTool;
      takeToolTimeout = millis() + 5000;
    }
  });

  Serial.printf("lock pin: %d\r\n", cfg.lockPin);
  pinMode(cfg.lockPin, OUTPUT);
  pinMode(cfg.sensorPin, INPUT);
}

void loop() {
  static bool connected = false;

  if (WiFi.status() == WL_CONNECTED) {
    if (!connected) {
      Serial.printf("Wifi connected: ip=%s\r\n", WiFi.localIP().toString().c_str());
    }
    connected = true;
    engine->loop();
  } else {
    if (connected) {
      connected = false;
      Serial.println("Lost wifi connection.");
    }
  }
      const bool keyPresent = digitalRead(cfg.sensorPin);
      if (state == lockState::lockInit) {
        if (keyPresent) state = lockState::locked;
        else state = lockState::toolTaken;
      }
      if (!keyPresent && state == lockState::takeTool) {
         state = lockState::toolTaken;
      }
      
      if (keyPresent && state == lockState::toolTaken){
        state = lockState::locked;
      }
      
      if (millis() > keyCheck) {
        Serial.printf("%s, %d\n", stateNames[state], keyPresent);
        keyCheck += 500;
      }
      
  // TODO: check for statechange. If changed, send right away. Else only send every 3 seconds or so
  if (millis() > takeToolTimeout && state == lockState::takeTool) {
    state = lockState::locked;
  }
  
  if (state == lockState::locked) {
    digitalWrite(cfg.lockPin, HIGH);
  } else {
    digitalWrite(cfg.lockPin, LOW); 
  }
}

