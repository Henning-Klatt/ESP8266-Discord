#include <Arduino.h>
#include <HardwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include "config.h"
#include "WebSocketClient.h"

void setup_wifi();

WebSocketClient ws(true);
DynamicJsonDocument doc(4096);

const char *host = "discord.com";
const int httpsPort = 443; // HTTPS= 443 and HTTP = 80

unsigned long heartbeatInterval = 0;
unsigned long lastHeartbeatAck = 0;
unsigned long lastHeartbeatSend = 0;

bool hasWsSession = false;
String websocketSessionId;
bool hasReceivedWSSequence = false;
unsigned long lastWebsocketSequence = 0;

const char *debugSendPrefix = "Send:: ";

const char *op = "op";

const int relayPin = D1;

void setup() {
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW);
    Serial.begin(115200);
    while (!Serial){
    }

    setup_wifi();
}

void loop() {
  if (!ws.isConnected()){
        Serial.println(F("connecting"));
        ws.setSecureFingerprint(certificateFingerprint);
        // It technically should fetch url from discord.com/api/gateway
        ws.connect(F("gateway.discord.gg"), F("/?v=8&encoding=json"), 443);
    }
    else{
        unsigned long now = millis();
        if (heartbeatInterval > 0){
            if (now > lastHeartbeatSend + heartbeatInterval){
                if (hasReceivedWSSequence){
                    String msg = F("{\"op\":1,\"d\":") + String(lastWebsocketSequence, 10) + "}";
                    Serial.println(debugSendPrefix + msg);
                    ws.send(msg);
                }
                else{
                    String msg = F("{\"op\":1,\"d\":null}");
                    Serial.println(debugSendPrefix + msg);
                    ws.send(msg);
                }
                lastHeartbeatSend = now;
            }
            if (lastHeartbeatAck > lastHeartbeatSend + (heartbeatInterval / 2)){
                Serial.println(F("Heartbeat ack timeout"));
                ws.disconnect();
                heartbeatInterval = 0;
            }
        }

        String msg;
        if (ws.getMessage(msg)){
            Serial.println(msg);
            DeserializationError err = deserializeJson(doc, msg);
            if (err){
                Serial.print(F("deserializeJson() failed with code "));
                Serial.println(err.f_str());
                if (err == DeserializationError::NoMemory)
                {
                    Serial.println(F("Try increasing DynamicJsonDocument size"));
                }
            }
            else{
                // TODO Should maintain heartbeat
                if (doc[op] == 0) { // Message
                    if (doc.containsKey(F("s"))) {
                        lastWebsocketSequence = doc[F("s")];
                        hasReceivedWSSequence = true;
                        String user = doc["d"]["author"]["username"].as<String>();
                        String message = doc["d"]["content"].as<String>().substring(23);
                        String channel = doc["d"]["channel_id"].as<String>();
                        Serial.println(channel + " > " + user + " > " + message);
                        String codeword = "auf";
                        String codechannel = "1040284663144525915";
                        if(message.equals(codeword) && channel.equals(codechannel)){
                            Serial.println("Öffne Türe!");
                            digitalWrite(relayPin, HIGH);
                            delay(1500);
                            digitalWrite(relayPin, LOW);
                        }
                    }

                    if (doc[F("t")] == "READY") {
                        websocketSessionId = doc[F("d")][F("session_id")].as<String>();
                        hasWsSession = true;
                    }
                }
                else if (doc[op] == 9) // Connection invalid
                {
                    ws.disconnect();
                    hasWsSession = false;
                    heartbeatInterval = 0;
                }
                else if (doc[op] == 11) // Heartbeat ACK
                {
                    lastHeartbeatAck = now;
                }
                else if (doc[op] == 10) // Start
                {
                    heartbeatInterval = doc[F("d")][F("heartbeat_interval")];

                    if (hasWsSession){
                        String msg = F("{\"op\":6,\"d\":{\"token\":\"") + String(bot_token) + F("\",\"session_id\":\"") + websocketSessionId + F("\",\"seq\":\"") + String(lastWebsocketSequence, 10) + F("\"}}");
                        Serial.println(debugSendPrefix + msg);
                        ws.send(msg);
                    }
                    else{
                        String msg = F("{\"op\":2,\"d\":{\"token\":\"") + String(bot_token) + F("\",\"intents\":") + gateway_intents + F(",\"properties\":{\"$os\":\"linux\",\"$browser\":\"ESP8266\",\"$device\":\"ESP8266\"},\"compress\":false,\"large_threshold\":250}}");
                        Serial.println(debugSendPrefix + msg);
                        ws.send(msg);
                    }

                    lastHeartbeatSend = now;
                    lastHeartbeatAck = now;
                }
            }
        }
    }
}


void setup_wifi(){
    Serial.println();
    Serial.println(F("Connecting to welcome"));

    WiFi.begin("welcome");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(F("."));
    }

    Serial.println(F(""));
    Serial.println(F("WiFi connected"));
    Serial.println(F("IP address: "));
    Serial.println(WiFi.localIP());
}