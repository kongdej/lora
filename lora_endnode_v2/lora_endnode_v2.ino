#include <SPI.h>
#include <LoRa.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Time.h>

ESP32Time rtc;
BlynkTimer timer;

#define SS      18
#define RST     14
#define DI0     26
#define BAND    923E6  //915E6

uint8_t id = 0x02;
uint8_t base = 0x00;

int counter = 0;
bool joined = false;

unsigned long interval = 3*1000;
unsigned long prvtime;

void sendSensors() {  
  char buff[19];
  counter++; 
  unsigned long t_stamp = rtc.getEpoch();
  sprintf(buff,"%02x%02x%02x%08x%04x", id, base, 0x03, t_stamp, counter);
  Serial.print("Sending: [");Serial.print(t_stamp);Serial.print("] ");Serial.println(buff);
  LoRa.beginPacket();
    LoRa.print(buff); 
  LoRa.endPacket();
}

void joinGW() {
  char buff[7];
  if (millis() - prvtime > interval) {
    prvtime = millis();
    sprintf(buff,"%02x%02x%02x", id, base, 0x01); // request to jon
    Serial.print("..");Serial.println(buff);
    Serial.println();
    LoRa.beginPacket();
      LoRa.print(buff); 
    LoRa.endPacket();
  }
  
  String tmp_string;
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    while (LoRa.available()) {
      tmp_string += (char)LoRa.read();  
    }
    int from = hex2int(tmp_string,0,2);
    int dest = hex2int(tmp_string,2,2);
    int fn = hex2int(tmp_string,4,2);
    if (from == 0 && dest == id && fn == 2) {
      joined = true;
      unsigned long unix_timestamp = hex2int(tmp_string,6,8);
      rtc.setTime(unix_timestamp);
    }
  }
}

unsigned long hex2int(String str, int start, int s) {
    char buff[9];
    String s_str = str.substring(start,start+s);
    s_str.toCharArray(buff,9);
    return (int) strtol(buff, 0, 16);
}

void setup() {
  //Serial.begin(115200);
  Serial.begin(9600, SERIAL_8N1);
  while (!Serial);
  
  Serial.println("LoRa Sender");
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("LoRa Initial OK!");
  Serial.println("Request to join!");
  while(!joined) {
    joinGW();
  }
  Serial.println("join done");

  timer.setInterval(60*1000L, sendSensors);
  
}

void loop() {

  // listen packets
  String tmp_string,tmp_rssi;
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    while (LoRa.available()) {
      tmp_string += (char)LoRa.read();  
    }
    
    int from = hex2int(tmp_string,0,2);
    int dest = hex2int(tmp_string,2,2);
    int fn = hex2int(tmp_string,4,2);
    if (from == 0 && dest == 255 && fn == 0) {
      Serial.print("timesync: [");
      Serial.print(tmp_string);
      tmp_rssi = LoRa.packetRssi();
      unsigned long unix_timestamp = hex2int(tmp_string,6,8);
      Serial.println("]="+ String(unix_timestamp) +" with RSSI " + tmp_rssi);
      Serial.println();
      rtc.setTime(unix_timestamp);
    }
    tmp_string ="";
    tmp_rssi = ""; 
  }
  
  timer.run();
}
