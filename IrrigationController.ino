#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include "constants.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// If this number is not found in EEPROM, then we assume no EEPROM values exist/have been saved
// and all other EEPROM values should be discarded/fallback to default values.
// See loadFromEeprom()
int EEPROM_MAGIC_NUMBER = 1001;
int SOLENOID_1_MINUTES = 5;
int SOLENOID_2_MINUTES = 5;
int SOLENOID_3_MINUTES = 5;

bool solenoid1On = false;
bool solenoid2On = false;
bool solenoid3On = false;
int solenoid1OffMillis = 0;
int solenoid2OffMillis = 0;
int solenoid3OffMillis = 0;
int solendoid1SecondsLeft = 0;
int solendoid2SecondsLeft = 0;
int solendoid3SecondsLeft = 0;

void setup() {
  pinMode(PIN_SOLENOID_1, OUTPUT);
  pinMode(PIN_SOLENOID_2, OUTPUT);
  pinMode(PIN_SOLENOID_3, OUTPUT);

  Serial.begin(115200);
  while (! Serial);

  setupWifi();
  mqttClient.setServer(MQTT_SERVER, 1883);
  mqttClient.setCallback(mqttCallback);
  if (!mqttClient.connected()) {
    mqttReconnect();
  }

  turnOffSolenoid(1);
  turnOffSolenoid(2);
  turnOffSolenoid(3);
  publishSecondsLeft(1, 0);
  publishSecondsLeft(2, 0);
  publishSecondsLeft(3, 0);

  loadFromEeprom();

  mqttClient.subscribe(MQTT_TOPIC_CONTROL_SOLENOID_1_STATE);
  mqttClient.subscribe(MQTT_TOPIC_CONTROL_SOLENOID_2_STATE);
  mqttClient.subscribe(MQTT_TOPIC_CONTROL_SOLENOID_3_STATE);
  mqttClient.subscribe(MQTT_TOPIC_CONTROL_SOLENOID_1_MINUTES);
  mqttClient.subscribe(MQTT_TOPIC_CONTROL_SOLENOID_2_MINUTES);
  mqttClient.subscribe(MQTT_TOPIC_CONTROL_SOLENOID_3_MINUTES);
  mqttClient.subscribe(MQTT_TOPIC_CONTROL_SOLENOID_1_ONFOR);
  mqttClient.subscribe(MQTT_TOPIC_CONTROL_SOLENOID_2_ONFOR);
  mqttClient.subscribe(MQTT_TOPIC_CONTROL_SOLENOID_3_ONFOR);
}


void loop() {
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  int currentMillis = millis();

  if (solenoid1On) {
    if (currentMillis > solenoid1OffMillis) {
      turnOffSolenoid(1);
    }
    int secondsLeft = (solenoid1OffMillis - currentMillis) / 1000;
    if (secondsLeft >= 0 && secondsLeft != solendoid1SecondsLeft) {
      solendoid1SecondsLeft = secondsLeft;
      publishSecondsLeft(1, solendoid1SecondsLeft);
    }
  }

  if (solenoid2On) {
    if (currentMillis > solenoid2OffMillis) {
      turnOffSolenoid(2);
    }    
    int secondsLeft = (solenoid2OffMillis - currentMillis) / 1000;
    if (secondsLeft >= 0 && secondsLeft != solendoid2SecondsLeft) {
      solendoid2SecondsLeft = secondsLeft;
      publishSecondsLeft(2, solendoid2SecondsLeft);
    }
  }

  if (solenoid3On) {
    if (currentMillis > solenoid3OffMillis) {
      turnOffSolenoid(3);
    }
    int secondsLeft = (solenoid3OffMillis - currentMillis) / 1000;
    if (secondsLeft >= 0 && secondsLeft != solendoid3SecondsLeft) {
      solendoid3SecondsLeft = secondsLeft;
      publishSecondsLeft(3, solendoid3SecondsLeft);
    }
  }

  delay(100);
}

void turnOnSolenoid(int solenoidNum, int minutes) {
  int targetMillis = millis() + (minutes * 60 * 1000);
  int targetPin = PIN_SOLENOID_1;
  if (solenoidNum == 1) {
    solenoid1OffMillis = targetMillis;
    solenoid1On = true;
  } else if (solenoidNum == 2) {
    targetPin = PIN_SOLENOID_2;
    solenoid2OffMillis = targetMillis;
    solenoid2On = true;
  } else if (solenoidNum == 3) {
    targetPin = PIN_SOLENOID_3;
    solenoid3OffMillis = targetMillis;
    solenoid3On = true;
  }

  digitalWrite(targetPin, HIGH);
  publishSolendoidState(solenoidNum, "1");
}

void turnOffSolenoid(int solenoidNum) {
  int targetPin = PIN_SOLENOID_1;
  if (solenoidNum == 1) {
    solenoid1On = false;
  } else if (solenoidNum == 2) {
    targetPin = PIN_SOLENOID_2;
    solenoid2On = false;
  } else if (solenoidNum == 3) {
    targetPin = PIN_SOLENOID_3;
    solenoid3On = false;
  }

  digitalWrite(targetPin, LOW);
  publishSolendoidState(solenoidNum, "0");
}

void publishSolendoidState(int solenoidNum, char* state) {
  if (solenoidNum == 1) {
    mqttPublish(MQTT_TOPIC_SOLENOID_1_STATE, state);
  } else if (solenoidNum == 2) {
    mqttPublish(MQTT_TOPIC_SOLENOID_2_STATE, state);
  } else if (solenoidNum == 3) {
    mqttPublish(MQTT_TOPIC_SOLENOID_3_STATE, state);
  }
}

void publishSecondsLeft(int solenoidNum, int secondsLeft) {
  char time[16];

  int minutes = 0;
  int seconds = 0;

  if (secondsLeft > 0) {
      minutes = (secondsLeft / 60) % 60;
      seconds = secondsLeft % 60;
  }

  sprintf(time,"%02u:%02u ", minutes, seconds);  

  if (solenoidNum == 1) {
    mqttPublish(MQTT_TOPIC_SOLENOID_1_COUNTDOWN, time);
  } else if (solenoidNum == 2) {
    mqttPublish(MQTT_TOPIC_SOLENOID_2_COUNTDOWN, time);
  } else if (solenoidNum == 3) {
    mqttPublish(MQTT_TOPIC_SOLENOID_3_COUNTDOWN, time);
  }
}


/**
 * WIFI / MQTT HELPERS
 */
void setupWifi() {
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = topic;
  String value = "";
  for (int i = 0; i < length; i++) {
      value += (char)payload[i];
  }

  Serial.println("mqttCallback !");
  Serial.println(topicStr);
  Serial.println(value);

  if (topicStr == MQTT_TOPIC_CONTROL_SOLENOID_1_STATE) {
    if (value.equals("1")) {
      turnOnSolenoid(1, SOLENOID_1_MINUTES);
    } else if (value.equals("0")) {
      turnOffSolenoid(1);
    }
  }

  if (topicStr == MQTT_TOPIC_CONTROL_SOLENOID_2_STATE) {
    if (value.equals("1")) {
      turnOnSolenoid(2, SOLENOID_2_MINUTES);
    } else if (value.equals("0")) {
      turnOffSolenoid(2);
    }
  }

  if (topicStr == MQTT_TOPIC_CONTROL_SOLENOID_3_STATE) {
    if (value.equals("1")) {
      turnOnSolenoid(3, SOLENOID_3_MINUTES);
    } else if (value.equals("0")) {
      turnOffSolenoid(3);
    }
  }

  if (topicStr == MQTT_TOPIC_CONTROL_SOLENOID_1_ONFOR) {
    int minutes = value.toInt();
    if (minutes > 0 && minutes <= 60) {
      turnOnSolenoid(1, minutes);
    }
  }

  if (topicStr == MQTT_TOPIC_CONTROL_SOLENOID_2_ONFOR) {
    int minutes = value.toInt();
    if (minutes > 0 && minutes <= 60) {
      turnOnSolenoid(2, minutes);
    }
  }

  if (topicStr == MQTT_TOPIC_CONTROL_SOLENOID_3_ONFOR) {
    int minutes = value.toInt();
    if (minutes > 0 && minutes <= 60) {
      turnOnSolenoid(3, minutes);
    }
  }

  if (topicStr == MQTT_TOPIC_CONTROL_SOLENOID_1_MINUTES) {
    int minutes = value.toInt();
    if (minutes >= 0 && minutes <= 60) {
      SOLENOID_1_MINUTES = minutes;
      saveToEeprom();
      mqttPublish(MQTT_TOPIC_SOLENOID_1_MINUTES, SOLENOID_1_MINUTES);
    }
  }

  if (topicStr == MQTT_TOPIC_CONTROL_SOLENOID_2_MINUTES) {
    int minutes = value.toInt();
    if (minutes >= 0 && minutes <= 60) {
      SOLENOID_2_MINUTES = minutes;
      saveToEeprom();
      mqttPublish(MQTT_TOPIC_SOLENOID_2_MINUTES, SOLENOID_2_MINUTES);
    }
  }

  if (topicStr == MQTT_TOPIC_CONTROL_SOLENOID_3_MINUTES) {
    int minutes = value.toInt();
    if (minutes >= 0 && minutes <= 60) {
      SOLENOID_3_MINUTES = minutes;
      saveToEeprom();
      mqttPublish(MQTT_TOPIC_SOLENOID_3_MINUTES, SOLENOID_3_MINUTES);
    }
  }
}

void mqttReconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");

    // Attempt to connect
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS, MQTT_TOPIC_STATUS, 1, true, "disconnected", false)) {
      Serial.println("connected");

      // Once connected, publish an announcement...
      mqttClient.publish(MQTT_TOPIC_STATUS, "connected", true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void mqttPublish(char *topic, int payload) {
  // Serial.print(topic);
  // Serial.print(": ");
  // Serial.println(payload);

  mqttClient.publish(topic, String(payload).c_str(), true);
}

void mqttPublish(char *topic, String payload) {
  // Serial.print(topic);
  // Serial.print(": ");
  // Serial.println(payload);

  mqttClient.publish(topic, payload.c_str(), true);
}



/**
 * EEPROM HELPERS
 */

void loadFromEeprom() {
  EEPROM.begin(24);

  //Read data from eeprom
  int magicNumber;
  int solenoid1Minutes;
  int solenoid2Minutes;
  int solenoid3Minutes;

  EEPROM.get(0, solenoid1Minutes);
  EEPROM.get(4, solenoid2Minutes);
  EEPROM.get(8, solenoid3Minutes);
  EEPROM.get(12, magicNumber);
  EEPROM.end();

  if (magicNumber == EEPROM_MAGIC_NUMBER) {
    Serial.print("Successfully loaded config from eeprom");
    SOLENOID_1_MINUTES = solenoid1Minutes;
    SOLENOID_2_MINUTES = solenoid2Minutes;
    SOLENOID_3_MINUTES = solenoid3Minutes;
  } else {
    Serial.print("EEPROM MAGIC NUMBER INCORRECT: ");
    Serial.print(magicNumber);
    Serial.print(" != ");
    Serial.println(EEPROM_MAGIC_NUMBER);
    Serial.print("Keeping default step config!");
  }

  mqttPublish(MQTT_TOPIC_SOLENOID_1_MINUTES, SOLENOID_1_MINUTES);
  mqttPublish(MQTT_TOPIC_SOLENOID_2_MINUTES, SOLENOID_2_MINUTES);
  mqttPublish(MQTT_TOPIC_SOLENOID_3_MINUTES, SOLENOID_3_MINUTES);
}

void saveToEeprom() {
  EEPROM.begin(16);

  EEPROM.put(0, SOLENOID_1_MINUTES);
  EEPROM.put(4, SOLENOID_2_MINUTES);
  EEPROM.put(8, SOLENOID_3_MINUTES);
  EEPROM.put(12, EEPROM_MAGIC_NUMBER);
  EEPROM.commit();
  EEPROM.end();
}