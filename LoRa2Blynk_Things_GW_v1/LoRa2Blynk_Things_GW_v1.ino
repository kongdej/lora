#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <TimeLib.h>
#include <WidgetRTC.h>
#include "SSD1306.h"
#include <ThingsBoard.h>

// Thing.egat.co.th -------------------------
#define TOKEN "xFIDsmf35NN29CGZtJJ7"
#define THINGSBOARD_SERVER  "mqtt.egat.co.th"

WiFiClient espClient;
ThingsBoard tb(espClient);
//---------------------------------------------

// Blynk ------------------------------------
#define BLYNK_TEMPLATE_ID "TMPLFAzXo6-B"
#define BLYNK_DEVICE_NAME "LoraGateway"
#define BLYNK_AUTH_TOKEN "Zcu3M0ogYJo8QzEexSD9GB7PGY1achb2"
char auth[] = BLYNK_AUTH_TOKEN;

BlynkTimer timer;
WidgetRTC rtc;
//---------------------------------------------

// Wifi -------------------------------------
const char* ssid       = "ZAB";
const char* password   = "Gearman1";
//-------------------------------------------

#define SS      18
#define RST     14
#define DI0     26
#define BAND    923E6  // 915E6
#define DCT     5000   // duty cycle time 2 sec. 
#define SF      7      // SpreadingFactor: ranges from 6-12,default 7 
#define SW      0x88   // SyncWord: ranges from 0-0xFF, default 0x34

String outgoing;              // outgoing message
byte msgCount = 0;            // count of outgoing messages
byte localAddress = 0x00;     // address of this device
byte destination = 0xFF;      // destination to send to
long lastSendTime = 0;        // last send time
int interval = DCT;          // interval between sends

bool subscribed = false;
float t[10],h[10];
int  n[10],r[10];
int  led_delay = 1000;

String incoming;
int  recipient;          // recipient address
byte sender;            // sender address
byte incomingMsgId;     // incoming msg ID
byte incomingLength;    // incoming msg length
String incomingClock;
int incomingRSSI;

// Helper macro to calculate array size
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

SSD1306  display(0x3c, 21, 22);

// Blynk -----------------------------------
BLYNK_CONNECTED() {
  rtc.begin();
}

BLYNK_WRITE(V1) {
  char buff[11];
  int pinValue = param.asInt();
  Serial.print("V1  value is: ");Serial.println(pinValue);
  sprintf(buff,"%02x%02x", 0x01, pinValue); 
  sendMessage(String(buff));
}

void clockDisplay() {
  String currentTime = String(hour()) + ":" + minute() + ":" + second();
  String currentDate = String(day()) + " " + month() + " " + year();
  Serial.print("Current time: ");
  Serial.print(currentTime);
  Serial.print(" ");
  Serial.print(currentDate);
  Serial.println();
  Blynk.virtualWrite(V3, currentTime);
  Blynk.virtualWrite(V4, currentDate);
}

void TimerSend() {
  Blynk.virtualWrite(V5, millis() / 1000);
  
  Blynk.virtualWrite(V10, t[1]);
  Blynk.virtualWrite(V11, h[1]);
  Blynk.virtualWrite(V20, t[2]);
  Blynk.virtualWrite(V21, h[2]);
  Blynk.virtualWrite(V30, t[3]);
  Blynk.virtualWrite(V31, h[3]);
  
  tb.sendTelemetryFloat("T001-01", t[1]);
  tb.sendTelemetryFloat("H001-01", h[1]);
  tb.sendTelemetryFloat("T002-01", t[2]);
  tb.sendTelemetryFloat("H002-01", h[2]);
  tb.sendTelemetryFloat("T003-01", t[3]);
  tb.sendTelemetryFloat("H003-01", h[3]);
}
//------------------------------------------

//- Things.egat ----------------------------------------
RPC_Response processDelayChange(const RPC_Data &data) {
  Serial.println("Received the set delay RPC method");
  led_delay = data;
  Serial.print("Set new delay: ");
  Serial.println(led_delay);

  return RPC_Response(NULL, led_delay);
}

RPC_Response processGetDelay(const RPC_Data &data) {
  Serial.println("Received the get value method");

  return RPC_Response(NULL, led_delay);
}

int st = 0;
RPC_Response processCheckStatus(const RPC_Data &data) {
  Serial.println("Received the get checkstatus");
  return RPC_Response(NULL, st);
}

RPC_Response processSet(const RPC_Data &data) {
  char buff[11];
  Serial.print("Set relay ");
  st = data;
  Serial.println(st);
  sprintf(buff,"%02x%02x%02x%02x%02x",0x00,0x01,0x04, 0x01, st);  // fn=0x04, write , relay 0x01 
  Serial.print("> Send Write:");Serial.println(buff);
  LoRa.beginPacket();
    LoRa.print(buff); 
  LoRa.endPacket();
  return RPC_Response(NULL, st);
}

RPC_Callback callbacks[] = {
  { "setValue", processDelayChange },
  { "getValue", processGetDelay },
  { "checkStatus", processCheckStatus },
  { "setValue1", processSet },
  { "getValue1", processCheckStatus },
};
//---------------------------------------------

void setup() {
  Serial.begin(9600);
  
  while (!Serial);
  Serial.println("LoRa Gateway Start..");
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (true);
  }
  
  //LoRa.setSpreadingFactor(SF); // ranges from 6-12,default 7 
  //LoRa.setSyncWord(SW);  // ranges from 0-0xFF, default 0x34
  
  // Initialising the UI will init the display too.
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50);
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);  
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.clear();
  display.drawString(10, 0, "LoRa Initial OK!");
  display.display();
  delay(1000);
  
  //connect to WiFi
   Serial.printf("Connecting to %s ", ssid);
   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.print(".");
   }
   Serial.println(" CONNECTED");
   display.drawString(10, 15, "Wifi Connected!");
   display.display();
   delay(1000);
       
   //Blynk connect 
    Blynk.config(auth,"kongdej.trueddns.com",40949);  // in place of Blynk.begin(auth, ssid, pass);
    int mytimeout = millis() / 1000;
    while (Blynk.connect() == false) { // try to connect to server for 10 seconds
      Serial.println(".");
      if((millis() / 1000) > mytimeout + 8){ // try local server if not connected within 9 seconds
         break;
      }
    }
    display.drawString(10, 25, "Blynk connected!");
    display.drawString(10, 45, "Waiting a packet..");
    display.display();

    
    LoRa.onReceive(onReceive);
    LoRa.receive();
    Serial.println("LoRa init succeeded.");
    timer.setInterval(10*1000L, TimerSend);
    
    setSyncInterval(10 * 60); // Sync interval in seconds (10 minutes)
  
    // Display digital clock every 10 seconds
    timer.setInterval(10000L, clockDisplay);
    timer.setInterval(1000L, showDisplay);
}


void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    reconnect();
    return;
  }
  
  // Connect to the ThingsBoard
  if (!tb.connected()) {
    Serial.print("Connecting to: ");Serial.print(THINGSBOARD_SERVER);Serial.print(" with token ");Serial.println(TOKEN);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN)) {
      Serial.println("Failed to connect");
      return;
    }
  }
  
  // Subscribe for RPC, if needed
  if (!subscribed) {
    Serial.println("Subscribing for RPC...");
    if (!tb.RPC_Subscribe(callbacks, COUNT_OF(callbacks))) {
      Serial.println("Failed to subscribe for RPC");
      return;
    }
    Serial.println("Subscribe done");
    subscribed = true;
  }
  
  if (tb.connected()) {
    tb.loop();
  }
  
  if (Blynk.connected()) {
    Blynk.run();
    timer.run();
  }
  
}
void showDisplay() {
  display.clear();
  display.drawString(10, 0, incomingClock);
  display.drawString(0, 14, "From: "+ String(sender) + ", RSSI: "+ incomingRSSI);
  display.drawString(0, 26, "MsgId: "+ String(incomingMsgId));
  display.drawString(0, 38, "Len: "+ String(incomingLength));
  display.drawString(0, 50, "Data: "+ incoming);
  display.display();
}

void sendMessage(String outgoing) {
  LoRa.beginPacket();                   // start packet
  LoRa.write(destination);              // add destination address
  LoRa.write(localAddress);             // add sender address
  LoRa.write(msgCount);                 // add message ID
  LoRa.write(outgoing.length());        // add payload length
  LoRa.print(outgoing);                 // add payload
  LoRa.endPacket();                     // finish packet and send it
  msgCount++;                           // increment message ID
  LoRa.receive();
}

void onReceive(int packetSize) {
  char clockb[20];
  
  if (packetSize == 0) return;          // if there's no packet, return

  // read packet header bytes:
  recipient = LoRa.read();          // recipient address
  sender = LoRa.read();            // sender address
  incomingMsgId = LoRa.read();     // incoming msg ID
  incomingLength = LoRa.read();    // incoming msg length
  
  incoming = "";                 // payload of packet

  while (LoRa.available()) {            // can't use readString() in callback, so
    incoming += (char)LoRa.read();      // add bytes one by one
  }

  if (incomingLength != incoming.length()) {   // check length for error
    Serial.println("error: message length does not match length");
    return;                             
  }

  // if the recipient isn't this device or broadcast,
  if (recipient != localAddress && recipient != 0xFF) {
    Serial.println("This message is not for me.");
    return;                             // skip rest of function
  }

  
 
  // if message is for this device, or broadcast, print details:
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message ID: " + String(incomingMsgId));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + incoming);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println();
  
  sprintf(clockb,"%02d-%02d-%02d %02d:%02d:%02d",day(),month(),year(),hour(),minute(),second());
  incomingClock = String(clockb);
  incomingRSSI = LoRa.packetRssi();
  
  switch (sender) {
    case 0x01:
          t[1] = (int) strtol( &incoming.substring(0,4)[0], NULL, 16)/10.;
          h[1] = (int) strtol( &incoming.substring(4,8)[0], NULL, 16)/10.;
          r[1] = (int) strtol( &incoming.substring(8,10)[0], NULL, 16);
          break;
    case 0x02:
          n[2] = (int) strtol( &incoming.substring(0,2)[0], NULL, 16);
          t[2] = (int) strtol( &incoming.substring(2,6)[0], NULL, 16)/10.;
          h[2] = (int) strtol( &incoming.substring(6,10)[0], NULL, 16)/10.;
          break;
    case 0x03:
          n[3] = (int) strtol( &incoming.substring(0,2)[0], NULL, 16);
          t[3] = (int) strtol( &incoming.substring(2,6)[0], NULL, 16)/10.;
          h[3] = (int) strtol( &incoming.substring(6,10)[0], NULL, 16)/10.;
          break;
  }

}

void reconnect() {
  int status = WiFi.status();
  if ( status != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    display.clear();
    display.drawString(10, 0, "Reconnect Wifi..");
    display.display();
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("Connected to AP");
    display.clear();
    display.drawString(10, 0, "Connected to AP!");
    display.display();
    delay(3000);
  }
}
