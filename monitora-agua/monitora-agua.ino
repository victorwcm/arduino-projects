#include <FS.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>

#define MAX_ATTEMPTS 10

// mqtt parameters
char param_mqtt_server[40] = "things.ubidots.com";
char param_mqtt_port[6] = "1883";
char param_mqtt_username[100] = "A1E-w6pTbDKLKGbUdfpeEeZaiXhRmS4a6e";
char param_mqtt_password[2] = "";
const char* configTopic = "config";

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

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// wifi manager configurations
bool shouldSaveConfig = false;
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void readFS() {
  //clean FS, for testing
  //SPIFFS.format();

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
  WiFiManagerParameter custom_mqtt_password("password", "mqtt password", param_mqtt_password, 2);
  WiFiManagerParameter custom_sample_interval("interval", "sample interval", param_sample_interval, 3);
  WiFiManagerParameter custom_lowerest_level("llevel", "lowerest level", param_lowerest_level, 4);
  WiFiManagerParameter custom_highest_level("hlevel", "highest level", param_highest_level, 4);
  // add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_username);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_sample_interval);
  wifiManager.addParameter(&custom_lowerest_level);
  wifiManager.addParameter(&custom_highest_level);
  // reset settings - for testing
  // wifiManager.resetSettings();
  
  // fetches ssid and pass and tries to connect
  // if it does not connect it starts an access point with the specified name
  // and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("JuaLabsPumpSystem", "jualabs")) {
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
  
  client.setServer(param_mqtt_server, atoi(param_mqtt_port));
  client.setCallback(callback);
}

/*
void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(currentSSID);

  WiFi.begin(currentSSID, currentPassword);

  // todo: inserir entrada no modo AP caso nao consiga reconectar
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
*/

void callback(char* topic, byte* payload, unsigned int length) {
  // verify topic
  if(strcmp(topic,configTopic) == 0) { // it is a config message
    
  }
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  uint64_t now = millis();
  if (now - lastMsg > (sampleInterval*1000)) {
    lastMsg = now;
    ++value;
    snprintf (msg, 75, "{\"level\":\"%d\",\"pump\":\"%d\"}", readLevel(), readPump());
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("vwcm/test", msg);
  }
}

int32_t readLevel() {
  return (int32_t) (((readDistance() - lowerestLevel) * 100) / (highestLevel - lowerestLevel));  
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
  //Serial.print("Distance: ");
  //Serial.println(distance);
  // DEBUG end
  return distance;
}

uint8_t readPump() {
  return 0;
}

