/*
 *  Lora Gateway
 */
#include <SPI.h>
#include <LoRa.h>
#include "SSD1306.h"

#include <WiFi.h>
#include "time.h"
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

const char* ssid       = "ZAB";
const char* password   = "Gearman1";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600;
const int   daylightOffset_sec = 7 * 3600;
 
SSD1306  display(0x3c, 21, 22);

#define SS      18
#define RST     14
#define DI0     26
#define BAND    923E6  //915E6

#define BLYNK_TEMPLATE_ID "TMPLFAzXo6-B"
#define BLYNK_DEVICE_NAME "LoraGateway"
#define BLYNK_AUTH_TOKEN "Zcu3M0ogYJo8QzEexSD9GB7PGY1achb2"

char auth[] = BLYNK_AUTH_TOKEN;

time_t now;
String currentTime;
unsigned long syncClockTime = 60*1000;
unsigned long prevTime=0;

void printLocalTime() {
   char buff[100];
   struct tm timeinfo;
   if (!getLocalTime(&timeinfo)) {
     Serial.println("Failed to obtain time");
     return;
   }
   sprintf(buff, "%02d-%02d-%4d %02d:%02d:%02d",timeinfo.tm_mday,timeinfo.tm_mon,timeinfo.tm_year+1900,timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
   currentTime = String(buff);
   time(&now);
 }

BLYNK_WRITE(V1) {
  char buff[11];
  int pinValue = param.asInt();
  Serial.print("V1  value is: ");
  Serial.println(pinValue);
  sprintf(buff,"%02x%02x%02x%02x%02x",0x00,0x01,0x04, 0x01, pinValue);
  Serial.print("> Send Relay:");Serial.println(buff);
  LoRa.beginPacket();
    LoRa.print(buff); 
  LoRa.endPacket();
}

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
   display.drawString(10, 15, "Wifi Connected.");
   display.display();
    
   //init and get the time
   configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
   printLocalTime();
   delay(1000);
   display.drawString(10, 25, "Waiting packet..");
   display.display();
   
    Blynk.config(auth,"kongdej.trueddns.com",40949);  // in place of Blynk.begin(auth, ssid, pass);
    int mytimeout = millis() / 1000;
    while (Blynk.connect() == false) { // try to connect to server for 10 seconds
      Serial.println(".");
      if((millis() / 1000) > mytimeout + 8){ // try local server if not connected within 9 seconds
         break;
      }
    }
}

void loop() {
  String tmp_string, tmp_rssi;
  char datastream[255];
  
  char timestamp_c[15];
  
  if (millis() - prevTime > syncClockTime) {
    prevTime = millis();
    sprintf(timestamp_c,"%02x%02x%02x%08x",0x00,0xff,0x00, time(&now));
    Serial.print("> Send timestamp:");Serial.println(timestamp_c);
    LoRa.beginPacket();
      LoRa.print(timestamp_c); 
    LoRa.endPacket();
  }
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("< Received packet @ "); Serial.println(now);
    while (LoRa.available()) {
      tmp_string += (char)LoRa.read();  
    }
    printLocalTime();
    Serial.print("= ");
    Serial.print(tmp_string);
    tmp_rssi = LoRa.packetRssi();
    Serial.println(" with RSSI " + tmp_rssi);
    
    int from = hex2int(tmp_string,0,2);
    int dest = hex2int(tmp_string,2,2);
    int fn = hex2int(tmp_string,4,2);

    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(10, 0, "GW:" + currentTime);
    display.drawString(10, 15, "ID: " + String(from) +" -> "+ String(dest) +  ", RSSI = "+tmp_rssi);
    display.drawString(10, 25, tmp_string);
    if (fn == 1) {
      char joinbuff[15];
      sprintf(joinbuff,"%02x%02x%02x%08x", 0x00, from, 0x02, time(&now));
      Serial.print("accept join: "); Serial.println(joinbuff);
      Serial.println();
      LoRa.beginPacket();
        LoRa.print(joinbuff); 
      LoRa.endPacket();
    }
    else if (from == 1 && fn == 3) {
      float temp = hex2int(tmp_string,6,4)/10.0;
      float hum = hex2int(tmp_string,10,4)/10.0;
      int v3 = hex2int(tmp_string,14,4);
      Serial.print("= Staion #1:");Serial.print(from);
      Serial.print("-");Serial.print(dest);
      Serial.print("-");Serial.print(fn);
      Serial.print(",temp=");Serial.print(temp);
      Serial.print(",hum=");Serial.print(hum);
      Serial.print(",v3=");Serial.println(v3);
      display.drawString(10, 37, "Temperature: " + String(temp));
      display.drawString(10, 47, "Humidity: " + String(hum));
      Blynk.virtualWrite(V10,temp);
      Blynk.virtualWrite(V11,hum);
    }
    else if (from == 2 && fn == 3) {
      unsigned long unix_timestamp = hex2int(tmp_string,6,8);
      int counter = hex2int(tmp_string,14,4);
      Serial.print("Counter:");Serial.println(counter);
      Serial.print("Timestamp:");Serial.println(unix_timestamp);
      Blynk.virtualWrite(V0,counter);
      Blynk.virtualWrite(V99,unix_timestamp);
      Blynk.virtualWrite(V100,tmp_rssi);
    }
    display.display();
  }
  tmp_string ="";
  tmp_rssi = ""; 
  Blynk.run();
}

unsigned long hex2int(String str, int start, int s) {
    char buff[9];
    String s_str = str.substring(start,start+s);
    s_str.toCharArray(buff,9);
    return (int) strtol(buff, 0, 16);
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
