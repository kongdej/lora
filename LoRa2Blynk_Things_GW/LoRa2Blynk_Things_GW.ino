/*
 *  Lora to Blynk Gateway
 */
#include <WiFi.h>
#include "time.h"
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <SPI.h>
#include <LoRa.h>
#include "SSD1306.h"
#include <ThingsBoard.h>

#define TOKEN "xFIDsmf35NN29CGZtJJ7"
#define THINGSBOARD_SERVER  "mqtt.egat.co.th"

WiFiClient espClient;
ThingsBoard tb(espClient);

bool debug = true;

// Blynk
#define BLYNK_TEMPLATE_ID "TMPLFAzXo6-B"
#define BLYNK_DEVICE_NAME "LoraGateway"
#define BLYNK_AUTH_TOKEN "Zcu3M0ogYJo8QzEexSD9GB7PGY1achb2"
char auth[] = BLYNK_AUTH_TOKEN;

// Wifi 
const char* ssid       = "ZAB";
const char* password   = "Gearman1";
const char* ntpServer = "th.pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600;
const int   daylightOffset_sec = 7 * 3600;
 
// OLED
SSD1306  display(0x3c, 21, 22);
#define SS      18
#define RST     14
#define DI0     26
#define BAND    923E6  //915E6

time_t now_t;
String currentTime;
unsigned long syncClockTime = 60*1000;  // broadcast time sync
unsigned long prevTime=0;
unsigned long prevTimestamp[255];
unsigned long currentTimestamp[255];

int nlive = 3*60;
bool subscribed = false;
int led_delay = 1000;

// Helper macro to calculate array size
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))


void getNTP() {
   char buff[100];   
   struct tm timeinfo;
   while (1) {
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");   
    }
    else {
      sprintf(buff, "%02d-%02d-%4d %02d:%02d:%02d",timeinfo.tm_mday,timeinfo.tm_mon,timeinfo.tm_year+1900,timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
      currentTime = String(buff);
      Serial.println(currentTime);
      break;
    }
   }
 }
 
 void sendTime() {
   char timestamp_c[15];
   sprintf(timestamp_c,"%02x%02x%02x%08x",0x00,0xff,0x00, time(&now_t)); // fn=0, ff=all node
   Serial.print("> Sync timestamp:");Serial.print(time(&now_t));Serial.print("->");Serial.println(timestamp_c);
   LoRa.beginPacket();
      LoRa.print(timestamp_c); 
   LoRa.endPacket(); 
}

void joinNode(int id) {
  char joinbuff[15];
  sprintf(joinbuff,"%02x%02x%02x%08x", 0x00, id, 0x02, time(&now_t));  // fn=2, accept node
  Serial.print("accept join: "); Serial.println(joinbuff);Serial.println();
  LoRa.beginPacket();
    LoRa.print(joinbuff); 
  LoRa.endPacket();
}
 
unsigned long hex2int(String str, int start, int s) {
    char buff[9];
    String s_str = str.substring(start,start+s);
    s_str.toCharArray(buff,9);
    return (int) strtol(buff, 0, 16);
}
 #define YELLOW 0xFFE0
// Blynk Virtual Pin ---------
BLYNK_WRITE(V1) {
  char buff[11];
  int pinValue = param.asInt();
  Serial.print("V1  value is: ");Serial.println(pinValue);
  sprintf(buff,"%02x%02x%02x%02x%02x",0x00,0x01,0x04, 0x01, pinValue);  // fn=0x04, write , relay 0x01 
  Serial.print("> Send Write:");Serial.println(buff);
  LoRa.beginPacket();
    LoRa.print(buff); 
  LoRa.endPacket();
}
// ----------------------------

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

void setup() {
  Serial.begin(9600);
  
  while (!Serial);
  Serial.println("LoRa Receiver");
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("LoRa Initial OK!");
  
  // Initialising the UI will init the display too.
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50);
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
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
   
   //init and get the time
   configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
   getNTP();
   sendTime();
}


void loop() {
  String tmp_string, tmp_rssi;
  char datastream[255];
 
  // broadcast time sync
  if (millis() - prevTime > syncClockTime) {
    prevTime = millis();
    getNTP();
    sendTime();
  }
  
  // Waiting for packets
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    if (debug) {Serial.print("< Received packet @ "); Serial.println(now_t);}
    while (LoRa.available()) {
      tmp_string += (char)LoRa.read();  
    }

    
    
    //getNTP();
    tmp_rssi = LoRa.packetRssi();
    if (debug) { Serial.print("= "); Serial.print(tmp_string); Serial.println(" with RSSI " + tmp_rssi);}
    // get header 
    int from = hex2int(tmp_string,0,2);
    int dest = hex2int(tmp_string,2,2);
    int fn   = hex2int(tmp_string,4,2);

    display.clear();
    display.drawString(0, 50, tmp_string);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    
    display.drawString(10, 0, currentTime);
    if (debug) Serial.println(currentTime);
    if (debug) Serial.println(tmp_string);
     // request to join
    if (fn == 1) {
      joinNode(from);
    }
    
    // node 1
    else if (from == 1 && fn == 3) {
      currentTimestamp[from] = hex2int(tmp_string,6,8);
      float temp = hex2int(tmp_string,14,4)/10.0;
      float hum = hex2int(tmp_string,18,4)/10.0;
      if (debug) {
        Serial.print("= Staion #1:");Serial.print(from);Serial.print("-");Serial.print(dest);Serial.print("-");Serial.print(fn);
        Serial.print(",temp=");Serial.print(temp);Serial.print(",hum=");Serial.print(hum);
      }
      display.drawString(0, 15, String(from) +  ": RSSI = "+tmp_rssi);
      //display.drawString(10, 37, "Temperature: " + String(temp));
      //display.drawString(10, 47, "Humidity: " + String(hum));
      Blynk.virtualWrite(V10,temp);
      Blynk.virtualWrite(V11,hum);
      tb.sendTelemetryFloat("temperature", temp);
      tb.sendTelemetryFloat("humidity", hum);
    }
    
    //node 2
    else if (from == 2 && fn == 3) {
      display.drawString(0, 25, String(from) +  ": RSSI = "+tmp_rssi+"          ");
      currentTimestamp[from] = hex2int(tmp_string,6,8); 
      int counter = hex2int(tmp_string,14,4);
      if (debug) {Serial.print("Counter:");Serial.println(counter);Serial.print("Timestamp:");Serial.println(currentTimestamp[from]);}
      Blynk.virtualWrite(V0,counter);
      Blynk.virtualWrite(V99,currentTimestamp[from]);
      Blynk.virtualWrite(V100,tmp_rssi);
    }
  }
  
  if ((time(&now_t) - currentTimestamp[1]) > nlive && currentTimestamp[1] != 0) {
    //display.clear();
    display.drawString(10, 0, currentTime);
    display.drawString(0, 15, +"1: **********");
  }
  if ((time(&now_t) - currentTimestamp[2]) > nlive && currentTimestamp[2] != 0) {
    //display.clear();
    display.drawString(10, 0, currentTime);
    display.drawString(0, 25, +"2: **********");
  }
  
  display.display();
  tmp_string ="";
  tmp_rssi = ""; 

  if (!tb.connected()) {
    // Connect to the ThingsBoard
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
  
  if (WiFi.status() != WL_CONNECTED) {
    reconnect();
    return;
  }
  tb.loop();
  
  Blynk.run();
}


/* 

|From|To|Function|timestamp|payload|
| 1  |1 | 1      |4        |  
function (1)
-------------------
0 = sync time
1 = request to join
2 = accept join
3 = send data
4 = receive data
*/
