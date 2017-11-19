
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <Wire.h>  // Include Wire if you're using I2C
#include <SFE_MicroOLED.h>  // Include the SFE_MicroOLED library

#define DHTPIN            D4         // Pin which is connected to the DHT sensor.
#define READ_INTERVAL_MS  5000

#define PIN_RESET 9  // Connect RST to pin 9
#define DC_JUMPER 0
#define CHAR_0_W 5
#define CHAR_0_H 7

// Uncomment the type of sensor in use:
// #define DHTTYPE           DHT11     // DHT 11 
#define DHTTYPE           DHT22     // DHT 22 (AM2302)
// #define DHTTYPE           DHT21     // DHT 21 (AM2301)

DHT_Unified dht(DHTPIN, DHTTYPE);

// Update these with values suitable for your network.

const char* ssid = "iPhone de Victor Medeiros";
const char* password = "obainternet";
const char* mqtt_server = "things.ubidots.com";
const int   mqtt_port = 1883;
const char* mqtt_client = "estacao-ufrpe-01";
const char* mqtt_user = "A1E-OCKhQuDfNV6a8PUCBWkd1nMAN84YbW";
const char* mqtt_pass = "";

const char* outTopic = "/v1.6/devices/estacao-ufrpe-01";
const char* tmpTopic = "/v1.6/devices/estacao-ufrpe-01/temperatura";
const char* humTopic = "/v1.6/devices/estacao-ufrpe-01/umidade";
const char* inTopic = "/v1.6/devices/estacao-ufrpe-01/msg";

WiFiClient espClient;
PubSubClient client(espClient);
long lastRead = 0;

MicroOLED oled(PIN_RESET, DC_JUMPER);    // I2C declaration
int SCREEN_WIDTH = oled.getLCDWidth();
int SCREEN_HEIGHT = oled.getLCDHeight();

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);
  oled.begin();
  oled.clear(PAGE);
  setup_dht();
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  oled.setFontType(0);
  oled.setCursor(0, 0);
  oled.print("conectandoa ");
  oled.print(ssid);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  oled.setCursor(0, 41);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    oled.print(".");
    oled.display();
  }
  oled.clear(PAGE);
  oled.setCursor(0, 0);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  oled.println("conectado");
  oled.println("IP:");
  oled.print(WiFi.localIP());
  oled.display();
}

void callback(char* topic, byte* payload, unsigned int length) {
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
    oled.clear(PAGE);
    oled.setCursor(0, 0);
    oled.println("conectando");
    oled.println("ao broker");
    oled.println("MQTT...");
    oled.display();
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_client, mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      oled.clear(PAGE);
      oled.setCursor(0, 0);
      oled.println("conectado");
      oled.display();
      // Once connected, publish an announcement...
      //client.publish(outTopic,"");
      // ... and resubscribe
      client.subscribe(inTopic);
    } else {
      oled.clear(PAGE);
      oled.setCursor(0, 0);
      oled.println("falhou");
      oled.println("tentando novamente em 5s");
      oled.display();
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop() {
  char *json;

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastRead > READ_INTERVAL_MS) {
    lastRead = now;
    readSensors();
  }
}

void readSensors() {
  char json_tmp[100], json_hum[100];
  char temp[10], hum[10];
  // Get temperature event and print its value.
  sensors_event_t event;  
  dht.temperature().getEvent(&event);
  oled.clear(PAGE);
  oled.setCursor(0, 0);
  if (isnan(event.temperature)) {
    Serial.println("Error reading temperature!");
  }
  else {
    Serial.print("Temperature: ");
    Serial.print(event.temperature);
    Serial.println(" *C");
    oled.print("temp: ");
    oled.print((int) event.temperature);
    oled.println("C");
  }
  dtostrf(event.temperature, 6, 2, temp);
  sprintf(json_tmp, "{\"value\":%s}", temp);
  
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println("Error reading humidity!");
  }
  else {
    Serial.print("Humidity: ");
    Serial.print(event.relative_humidity);
    Serial.println("%");
    oled.print("umid: ");
    oled.print((int) event.relative_humidity);
    oled.println("%");
  }
  oled.display();
  dtostrf(event.relative_humidity, 6, 2, hum);
  sprintf(json_hum, "{\"value\":%s}", hum);
  oled.setCursor(0, 38);
  oled.print("enviando..");
  oled.display();
  client.publish(tmpTopic, json_tmp);
  client.publish(humTopic, json_hum);
  oled.setCursor(0, 38);
  oled.print("          ");
  oled.display();
  //sprintf(json, "{\"temperatura\":%s,\"umidade\":%s}", temp, hum);
  //return json;
}

void setup_dht() {
  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  dht.humidity().getSensor(&sensor);
}

