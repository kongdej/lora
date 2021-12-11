#include <SPI.h>
#include <LoRa.h>
#include "ModbusMaster.h"
#include <BlynkSimpleEsp32.h>
#include <ESP32Time.h>

ESP32Time rtc;
BlynkTimer timer;

#define MAX485_RE_NEG  4
#define Slave_ID    9
#define RX_PIN      16 //RX2 
#define TX_PIN      17  //TX2 


#define SS      18
#define RST     14
#define DI0     26
#define BAND    923E6  //915E6

uint8_t id = 0x01;
uint8_t base = 0x00;

uint8_t result;
int t[2] = {};
int counter = 0;
bool joined = false;
unsigned long prvtime;
unsigned long interval = 10*1000;

ModbusMaster modbus;

int relay_01 = 2;
 
void preTransmission() {
  digitalWrite(MAX485_RE_NEG, HIGH); //Switch to transmit data
}

void postTransmission() {
  digitalWrite(MAX485_RE_NEG, LOW); //Switch to receive data
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

void sendSensors() {
  /*
  modbus.readInputRegisters(1, 2);
  if (result == modbus.ku8MBSuccess) {
    t[0] = modbus.getResponseBuffer(0);
    t[1] = modbus.getResponseBuffer(1);
    //Serial.print("Temp: ");Serial.println(t[0] /10.0);Serial.print("Humi: ");Serial.println(t[1] /10.0);
  }
  */
  char buff[23];
  sprintf(buff,"%02x%02x%02x%08x%04x%04x", id, base, 0x03, rtc.getEpoch(), t[0], t[1]);
  Serial.print("Sending_"); Serial.print(counter); Serial.print(": ");Serial.println(buff);
  Serial.println();
    
  // send packet
  LoRa.beginPacket();
    LoRa.print(buff); 
  LoRa.endPacket();
  
  counter++;
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
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  pinMode(MAX485_RE_NEG, OUTPUT);
  digitalWrite(MAX485_RE_NEG, LOW);
  
  modbus.begin(Slave_ID, Serial2);
  modbus.preTransmission(preTransmission);
  modbus.postTransmission(postTransmission);
  
  pinMode(relay_01, OUTPUT); 
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
    
    Serial.println(tmp_string);
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
    }
    else if (from == 0 && dest == 1 && fn == 4) {
        int n   = hex2int(tmp_string,6,2);
        int cmd = hex2int(tmp_string,8,2);
        Serial.print("command relay ");Serial.print(n); Serial.print('-');Serial.print(cmd);
        digitalWrite(relay_01,cmd);
    }
    
    tmp_string ="";
    tmp_rssi = ""; 
  }
  
  timer.run();
}


/*
 *  Help data structure
 *  id_from,id_to, fn,data
 *  00,,01,01,01,0000,0
 *  01,00,03,
 *  fn 
 *  0x00 = timestamp
 *  0x01 = write [nodeid, relayid, delay] ; all = 0xff
 *  0x03 = read [data,data,data]
 *  
 *  id =  gateway
 *  
 */
