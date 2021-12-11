#include <SPI.h>
#include <LoRa.h>
#include "ModbusMaster.h"

#define MAX485_RE_NEG  4
#define Slave_ID    0x08
#define RX_PIN      16 //RX2 
#define TX_PIN      17  //TX2 

#define SS      18
#define RST     14
#define DI0     26
#define BAND    923E6  //915E6
#define DCT     60000   // duty cycle time 2 sec.
#define SF      7      // SpreadingFactor: ranges from 6-12,default 7 
#define SW      0xab   // SyncWord: ranges from 0-0xFF, default 0x34

String outgoing;              // outgoing message
byte msgCount = 0;            // count of outgoing messages
byte localAddress = 0x01;     // address of this device
byte destination = 0x00;      // destination to send to
long lastSendTime = 0;        // last send time
int interval = DCT;          // interval between sends

uint8_t result;
int t[2] = {};

ModbusMaster modbus;

int relay[] = {2,2,2,2};
 
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
    Serial.print("Temp: ");Serial.println(t[0] /10.0);Serial.print("Humi: ");Serial.println(t[1] /10.0);
  }
  
  char buff[11];
  sprintf(buff,"%04x%04x%02x", t[0], t[1], digitalRead(2));
  Serial.print("Sending ");Serial.println(buff);
  Serial.println();
  String outgoing = String(buff);
  LoRa.beginPacket();                   // start packet
  LoRa.write(destination);              // add destination address
  LoRa.write(localAddress);             // add sender address
  LoRa.write(msgCount);                 // add message ID
  LoRa.write(outgoing.length());        // add payload length
  LoRa.print(outgoing);                 // add payload
  LoRa.endPacket();                     // finish packet and send it
  msgCount++;         
}

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  pinMode(MAX485_RE_NEG, OUTPUT);
  digitalWrite(MAX485_RE_NEG, LOW);
  
  modbus.begin(Slave_ID, Serial2);
  modbus.preTransmission(preTransmission);
  modbus.postTransmission(postTransmission);

  for (int i=0; i < 4; i++)
    pinMode(relay[i], OUTPUT); 
    
  while (!Serial);
  
  Serial.println("LoRa Node 1 Starting..");
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("LoRa Initial OK!");
  Serial.println("Request to join!");
  
  Serial.println("join done");
  
  LoRa.setSpreadingFactor(SF); // ranges from 6-12,default 7 
  LoRa.setSyncWord(SW);  // ranges from 0-0xFF, default 0x34
  
  LoRa.onReceive(onReceive);
  LoRa.receive();
  
  Serial.println("LoRa init succeeded.");
}

void loop() {
  if (millis() - lastSendTime > interval) {
    lastSendTime = millis();            // timestamp the message
    interval = DCT + random(2000);     // 5-7 seconds
    sendSensors();
    LoRa.receive();                    // go back into receive mode
  }
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();          // recipient address
  byte sender = LoRa.read();            // sender address
  byte incomingMsgId = LoRa.read();     // incoming msg ID
  byte incomingLength = LoRa.read();    // incoming msg length

  String incoming = "";                 // payload of packet

  while (LoRa.available()) {            // can't use readString() in callback, so
    incoming += (char)LoRa.read();      // add bytes one by one
  }

  if (incomingLength != incoming.length()) {   // check length for error
    Serial.println("error: message length does not match length");
    return;                             // skip rest of function
  }

  // if the recipient isn't this device or broadcast,
  if (recipient != localAddress && recipient != 0xFF) {
    Serial.println("This message from " + String(sender,HEX) + " is not for me.");
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
  int rno = (int) strtol( &incoming.substring(0,2)[0], NULL, 16);
  int cmd = (int) strtol( &incoming.substring(2,4)[0], NULL, 16);
  if (sender == 0) {
    digitalWrite(relay[rno-1], cmd);
  }
}
