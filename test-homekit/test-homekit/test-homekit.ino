#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <Wire.h>

const char* ssid = "rede3";
const char* password = "#r3d33#rur@l";

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);

void setup() {
    Serial.begin(9600);
    setupGPIOs();
    // setupScreen();
    Serial.println("Starting Lighthouse...");
    setupWiFi();
}

void loop()
{
    WiFiClient client = server.available();
    if (!client)
    {
        return;
    }
    
    while (!client.available())
    {
        delay(1);
    }
    handleClient(client);
}

void setupGPIOs() {
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(D0, OUTPUT);
}

void setupWiFi() {
    // Connect to WiFi network
    Serial.print("Connecting to "); Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");

    // Start the server
    server.begin();
    Serial.println(WiFi.localIP().toString());
    delay(1000);
}

void handleClient(WiFiClient client)
{
    String req = client.readStringUntil('\r');
    client.flush();

    if(req.indexOf("on") != -1)//checks for on 
    {
        // Serial.println(req.indexOf("on"));
        digitalWrite(LED_BUILTIN, LOW);    // set pin 5 high
        digitalWrite(D0, LOW);    // set pin 5 high
        Serial.println("Led On");
        client.flush();
        client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nOK");
        client.stop();
    }
    else if(req.indexOf("off") != 1)//checks for off 
    {
        // Serial.println(req.indexOf("off"));
        digitalWrite(LED_BUILTIN, HIGH);    // set pin 5 low
        digitalWrite(D0, HIGH);    // set pin 5 low
        Serial.println("Led Off");
        client.flush();
        client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nOK");
        client.stop();
    }
    else {
        client.stop();
    }
}
