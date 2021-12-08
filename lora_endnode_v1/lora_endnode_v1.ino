#include <SPI.h>
#include <LoRa.h>
#include "ModbusMaster.h"
#include <BlynkSimpleEsp32.h>

BlynkTimer timer;

#define MAX485_RE_NEG  4
#define Slave_ID    9
#define RX_PIN      16 //RX2 
#define TX_PIN      17  //TX2 


#define SS      18
#define RST     14
#define DI0     26
#define BAND    923E6  //915E6


uint8_t result;
int t[2] = {};
int counter = 0;
ModbusMaster modbus;

uint8_t id = 0x01;
uint8_t base = 0x00;

void preTransmission() {
  digitalWrite(MAX485_RE_NEG, HIGH); //Switch to transmit data
}

void postTransmission() {
  digitalWrite(MAX485_RE_NEG, LOW); //Switch to receive data
}

void sendSensors() {
  modbus.readInputRegisters(1, 2);
  if (result == modbus.ku8MBSuccess) {
    t[0] = modbus.getResponseBuffer(0);
    t[1] = modbus.getResponseBuffer(1);
    /*
    Serial.print("Temp: ");Serial.println(t[0] /10.0);
    Serial.print("Humi: ");Serial.println(t[1] /10.0);
    */
  }
  
  char buff[19];
  int v3 = 291; //random(15,50);
  sprintf(buff,"%02x%02x%02x%04x%04x%04x", id, base, 0x03, t[0], t[1], v3);
  Serial.print("Sending_"); Serial.print(counter); Serial.print(": ");Serial.println(buff);
  Serial.println();
    
  // send packet
  LoRa.beginPacket();
    LoRa.print(buff); 
  LoRa.endPacket();
  
  counter++;
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
  
  pinMode(25, OUTPUT); //Send success, LED will bright 1 second
  while (!Serial);
  
  Serial.println("LoRa Sender");
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("LoRa Initial OK!");
  timer.setInterval(3000L, sendSensors);
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
    }
    else if (from == 0 && dest == 1 && fn == 4) {
        int n   = hex2int(tmp_string,6,2);
        int cmd = hex2int(tmp_string,8,2);
        Serial.print("command relay ");Serial.print(n); Serial.print('-');
        Serial.print(cmd);
        digitalWrite(25,HIGH);
    }
    
    tmp_string ="";
    tmp_rssi = ""; 
  }
  
  timer.run();
}

int hex2int(String str, int start, int s) {
    char buff[9];
    String s_str = str.substring(start,start+s);
    s_str.toCharArray(buff,9);
    return (int) strtol(buff, 0, 16);
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
