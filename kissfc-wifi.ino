#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <Arduino.h>
#include <WebSocketsServer.h>
#include "WiFiManager.h"
#include <Ticker.h>

#define SERIAL Serial
//#define DEBUG

Ticker ticker;
typedef enum { NONE, INIT, CONFIG, LOOP, DONE } STATE;
STATE state = NONE;
uint8_t serialBuf[256];
uint8_t packetBuf[256];
uint16_t minBytes = 0;
uint16_t recBytes = 0;
uint8_t packetReceived = 0;
uint8_t packetSize = 0;

uint32_t stime = 0;
uint32_t baudrates[] = { 115200, 250000 };
int baudRateIndex = 1;
WiFiManager wifiManager;
WebSocketsServer webSocket = WebSocketsServer(81);

void tick() {
  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);     // set pin to the opposite state
}

uint8_t nibbleFromChar(char c)
{
  if(c >= '0' && c <= '9') return c - '0';
  if(c >= 'a' && c <= 'f') return c - 'a' + 10;
  if(c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 255;
}


/**
 * Start new packet receiving.
 */
void startNewPacket() {
  recBytes = 0;
  minBytes = 0;
  packetReceived = 0;
}

/**
 * Process serial port incoming data.
 */
void processSerial() {
  if (SERIAL.available() > 0) {
    if (recBytes > 255) recBytes = 0;

    serialBuf[recBytes++] = SERIAL.read();
    if (recBytes == 1 && serialBuf[0] != 5) recBytes = 0; // check for start byte, reset if its wrong
    if (recBytes == 2) {
      minBytes = serialBuf[1] + 3; // got the transmission length
    }
    if (recBytes > 1 && minBytes > 0 && recBytes == minBytes) {
      uint32_t checksum = 0;
      for (int i = 2; i < (minBytes-1); i++) {
        checksum += serialBuf[i];
      }
      uint8_t cs = checksum / (uint16_t)(minBytes - 3);
      if (cs == serialBuf[recBytes - 1]) {
          packetSize = minBytes;
          memcpy(packetBuf, serialBuf, packetSize);
          packetReceived = 1;
      }
      recBytes = 0;
      minBytes = 0;
    }
  }
}

/**
 * Convert byte to hex.
 */
String byteToHex(uint8_t b) {
  String ret="";
  if (b<16) ret+="0";
  ret+=String(b, HEX);
  ret.toUpperCase();
  return ret;
}

/**
 * Main loop.
 */
void loop() {
  switch(state)  {
    case INIT: 
       if (baudRateIndex>=0) {
            SERIAL.begin(baudrates[baudRateIndex--]);
            SERIAL.write(0x30);
            startNewPacket(); 
            state = CONFIG;
            stime = millis();
       } else {
          state = LOOP;
       }
       break;
    case CONFIG: 
       if (((millis()-stime) > 2000) || packetReceived) {
          if (packetReceived) {
            packetReceived = 0;
            uint8_t ver = packetBuf[0x5e];
            //if (ssid="") for (int i=0x56; i<=0x5d; i++)  ssid += byteToHex(packetBuf[i]); // add kiss serial number if not dfined
            state=LOOP;
          } else {
            state=INIT;
          }
       }
       break;
     case LOOP: 
       webSocket.loop();
       break;
     case DONE: 
       break;
  }
  processSerial(); 
  wifiManager.loop();
} 

/**
 * Got web socket event
 */
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  uint32_t beginTime;
  switch (type) {
    case WStype_TEXT:
        for (int i=0; i<(lenght>>1); i++) {
          uint8_t b = (nibbleFromChar((char)payload[i*2])<<4) + nibbleFromChar((char)payload[i*2 + 1]);
          if (lenght==2 && b==0x5f) {
              wifiManager.resetSettings();
              uint8_t ret[] = {5, 1, 6, 6} ; 
              webSocket.sendBIN(num, ret, 4);
              delay(1000);
              ESP.reset();
              delay(2000);
              break;
          } else {
            SERIAL.write(b);
          }
        }
        startNewPacket();

        beginTime = millis();
        while (((millis()-beginTime)<2000) && !packetReceived) {
            processSerial();
            yield();
        }
        if (packetReceived) {
            webSocket.sendBIN(num, packetBuf, packetSize);
            packetReceived = 0;
        }
        
      break;
    case WStype_BIN:
        if (lenght==1 && payload[0]==0x5f) {
              wifiManager.resetSettings();
              uint8_t ret[] = {5, 1, 6, 6} ; 
              webSocket.sendBIN(num, ret, 4);
              delay(1000);
              ESP.reset();
              delay(2000);
              break;
        } else {
          SERIAL.write(payload, lenght);
        }
        startNewPacket();
        beginTime = millis();
        while (((millis()-beginTime)<2000) && !packetReceived) {
            processSerial();
            yield();
        }
        if (packetReceived) {
            webSocket.sendBIN(num, packetBuf, packetSize);
            packetReceived = 0;
        }
      
      break;
  }
}

/**
 * Setup.
 */
void setup (void) {
   #ifdef DEBUG
    SERIAL.begin(115200);
   #endif
   pinMode(BUILTIN_LED, OUTPUT);
   ticker.attach(0.6, tick);

   #ifdef DEBUG
      wifiManager.setDebugOutput(true);
   #else
      wifiManager.setDebugOutput(false);
   #endif

   String ssid = "KISS-WIFI-" + String(ESP.getChipId());
   wifiManager.autoConnect(ssid.c_str(), wifiManager.getAPPassword().c_str());

   ticker.detach();
   digitalWrite(BUILTIN_LED, LOW);
       
   MDNS.begin("kiss");
   webSocket.begin();
   webSocket.onEvent(webSocketEvent);

   MDNS.addService("ws", "tcp", 81);
   state = INIT;
}


