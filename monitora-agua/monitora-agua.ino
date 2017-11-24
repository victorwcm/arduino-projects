#include <FS.h>
#include <ESP8266WiFi.h>

#define CAYENNE_PRINT Serial
#include <CayenneMQTTESP8266.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>

#define MAX_ATTEMPTS 10
#define CFG_BTN 0
#define CFG_CHN 2
#define LL_CHN 3
#define HL_CHN 4
#define SI_CHN 5

// mqtt parameters
char param_mqtt_server[40] = "mqtt.mydevices.com";
char param_mqtt_port[6] = "1883";
// char param_mqtt_username[100] = "A1E-w6pTbDKLKGbUdfpeEeZaiXhRmS4a6e";
char param_mqtt_username[100] = "20a41dc0-959d-11e7-a491-d751ec027e48";
char param_mqtt_password[100] = "533ad2b2df7fa505ecd01b2792e184702a0c96f0";
char param_mqtt_clientid[100] = "456f6080-cd59-11e7-98e1-8369df76aa6d";
const char* configTopic = "vwcm/config";
const char* SSIDAP = "JuaPumpSystem";
const char* APPass = "JuaPumpSystem";

// application configurable parameters
char param_sample_interval[3] = "5";  // time interval in seconds between samples
char param_lowerest_level[4] = "60";  // reservatory lowerest level in cm
char param_highest_level[4] = "10";  // reservatory highest level in cm
uint32_t sampleInterval = 5;  // time interval in seconds between samples
int32_t lowerestLevel = 60;  // reservatory lowerest level in cm
int32_t highestLevel = 10;  // reservatory highest level in cm

// sensors configurations
const uint8_t triggerPin = D4;  //D4
const uint8_t echoPin = D3;  //D3

long lastMsg = 0;
char msg[50];
int value = 0;

unsigned long lastMillis = 0;

// wifi manager configurations
bool shouldSaveConfig = false;
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void readFS() {
  //clean FS, for testing
  // SPIFFS.format();

  // read configuration from FS json
  Serial.println("mounting FS...");

  if(SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(param_mqtt_server, json["mqtt_server"]);
          strcpy(param_mqtt_port, json["mqtt_port"]);
          strcpy(param_mqtt_username, json["mqtt_username"]);
          strcpy(param_mqtt_password, json["mqtt_password"]);
          strcpy(param_sample_interval, json["sample_interval"]);
          strcpy(param_lowerest_level, json["lowerest_level"]);
          strcpy(param_highest_level, json["highest_level"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } 
  else {
    Serial.println("failed to mount FS");
  }
  //end read
}

void configWM() {
  // declare a WiFiManager
  // local initialization cause once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  // configure parameters
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", param_mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", param_mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_username("username", "mqtt username", param_mqtt_username, 100);
  WiFiManagerParameter custom_mqtt_password("password", "mqtt password", param_mqtt_password, 100);
  WiFiManagerParameter custom_mqtt_clientid("clientid", "client id", param_mqtt_clientid, 100);
  WiFiManagerParameter custom_sample_interval("interval", "sample interval", param_sample_interval, 3);
  WiFiManagerParameter custom_lowerest_level("llevel", "lowerest level", param_lowerest_level, 4);
  WiFiManagerParameter custom_highest_level("hlevel", "highest level", param_highest_level, 4);
  // add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_username);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_mqtt_clientid);
  wifiManager.addParameter(&custom_sample_interval);
  wifiManager.addParameter(&custom_lowerest_level);
  wifiManager.addParameter(&custom_highest_level);
  // reset settings - for testing
  // wifiManager.resetSettings();
  
  // fetches ssid and pass and tries to connect
  // if it does not connect it starts an access point with the specified name
  // and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(SSIDAP, APPass)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  // if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  // read updated parameters
  strcpy(param_mqtt_server, custom_mqtt_server.getValue());
  strcpy(param_mqtt_port, custom_mqtt_port.getValue());
  strcpy(param_mqtt_username, custom_mqtt_username.getValue());
  strcpy(param_mqtt_password, custom_mqtt_password.getValue());
  strcpy(param_mqtt_clientid, custom_mqtt_clientid.getValue());
  strcpy(param_sample_interval, custom_sample_interval.getValue());
  strcpy(param_lowerest_level, custom_lowerest_level.getValue());
  strcpy(param_highest_level, custom_highest_level.getValue());
  // convert the integer values
  sampleInterval = atoi(param_sample_interval);  // time interval in seconds between samples
  lowerestLevel = atoi(param_lowerest_level);  // reservatory lowerest level in cm
  highestLevel = atoi(param_highest_level);  // reservatory highest level in cm

  // save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = param_mqtt_server;
    json["mqtt_port"] = param_mqtt_port;
    json["mqtt_username"] = param_mqtt_username;
    json["mqtt_password"] = param_mqtt_password;
    json["mqtt_clientid"] = param_mqtt_clientid;
    json["sample_interval"] = param_sample_interval;
    json["lowerest_level"] = param_lowerest_level;
    json["highest_level"] = param_highest_level;


    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
}

void startConfigPortal() {
    // WiFiManager declaration
    WiFiManager wifiManager;
    
    if (!wifiManager.startConfigPortal(SSIDAP, APPass)) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
}

void checkConfigBtnPress() {
  if ( digitalRead(CFG_BTN) == LOW ) {
    startConfigPortal();
  }
}

void setup() {
  // pins configuration
  pinMode(BUILTIN_LED, OUTPUT);     // initialize the BUILTIN_LED pin as an output
  pinMode(triggerPin, OUTPUT);      // sets the triggerPin as an output
  pinMode(echoPin, INPUT);          // sets the echoPin as an input
  Serial.begin(115200);                 // starts the serial communication
  
  readFS();
  configWM();
  
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  
  Cayenne.begin(param_mqtt_username, param_mqtt_password, param_mqtt_clientid);

}

void loop() {
  Cayenne.loop();

  // publish data every sampleInterval seconds
  if (millis() - lastMillis > (sampleInterval*1000)) {
    lastMillis = millis();
    // write the tank level data to Cayenne
    Cayenne.virtualWrite(0, readLevel(), "tl", "null");
    // write the pump state to Cayenne
    Cayenne.virtualWrite(1, readPump(), "digital_sensor", "d");
  }
}

int32_t readLevel() {
  int32_t returnValue = (int32_t) (((readDistance() - lowerestLevel) * 100) / (highestLevel - lowerestLevel));
  CAYENNE_LOG("NIVEL: %d - %d - %d", returnValue, lowerestLevel, highestLevel);
  return returnValue;  
}

int32_t readDistance() {
  int64_t duration;
  int32_t distance;
  
  // clears the trigPin
  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);

  // sets the triggerPin on HIGH state for 10 microseconds
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);

  // reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  // calculating the distance
  distance = (int32_t) (duration*0.034)/2;
  // DEBUG
  Serial.print("Distance: ");
  Serial.println(distance);
  // DEBUG end
  return distance;
}

uint8_t readPump() {
  return 0;
}

// Default function for processing actuator commands from the Cayenne Dashboard.
// You can also use functions for specific channels, e.g CAYENNE_IN(1) for channel 1 commands.

CAYENNE_IN_DEFAULT() {
  CAYENNE_LOG("CAYENNE_IN_DEFAULT(%u) - %s, %s", request.channel, getValue.getId(), getValue.asString());
  switch(request.channel) {
    case CFG_CHN:
      startConfigPortal();
      break;
    case LL_CHN:
      lowerestLevel = atoi(getValue.asString());
      break;
    case HL_CHN:
      highestLevel = atoi(getValue.asString());
      break;
    case SI_CHN:
      sampleInterval = atoi(getValue.asString());
      break;
    default:
      break;
  }
}
