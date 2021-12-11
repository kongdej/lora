#include <Wire.h> 
#include <SPI.h>
#include <LoRa.h>

#define BAND    923E6  //915E6

#define NID     0x03
#define BASEID  0x00

bool joined = false;
int interval = 10*1000; // 3 seconds

struct HEADER {
  int from;
  int dest;
  int fn;  
};

void LoRaSend(char * msg)     {
  char buff[strlen(msg)];  
  strcpy(buff, msg);  
  Serial.print("Send Packet:");Serial.println(buff);
  
  while (LoRa.beginPacket() == 0) {
    Serial.print("waiting for radio ... ");
    delay(100);
  }
  
  LoRa.beginPacket();
    LoRa.print(buff); 
  LoRa.endPacket();
}

String LoRaReceived() {
  String tmp_string;
  
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("Received packet: '");
    while (LoRa.available()) {
      tmp_string += (char)LoRa.read();
    }
    Serial.print(tmp_string);
    Serial.print("' with RSSI ");
    Serial.println(LoRa.packetRssi());

    return tmp_string;
  }
  else {
    return "";
  }
}

void joinGW() {
  char buff[7];
  
  unsigned prevTime = millis();
  Serial.println("Joining Gateway..");
  while(!joined) {
    if (millis() - prevTime > interval) {
      prevTime =  millis();
      sprintf(buff,"%02x%02x%02x", NID, BASEID, 0x01); // request to jon
      LoRaSend(buff);      
    } 

    String rmsg = LoRaReceived();
    joined = true;
    if (rmsg.length() > 5 ) {
      Serial.println(rmsg);
      HEADER h = getHeader(rmsg);
      Serial.print(h.from);
      Serial.print(h.dest);
      Serial.print(h.fn);
      if (h.from == 0 && h.dest == NID && h.fn == 2) {
        joined = true;
      }
    }
  
  }
  Serial.println("Joined!");
}

unsigned long hex2int(String str, int start, int s) {
    char buff[9];
    String s_str = str.substring(start,start+s);
    s_str.toCharArray(buff,9);
    return (int) strtol(buff, 0, 16);
}

HEADER getHeader(String msg) {
  HEADER header;
  
  header.from = hex2int(msg,0,2);
  header.dest = hex2int(msg,2,2);
  header.fn = hex2int(msg,4,2); 
  
  return header;
}

void onTxDone() {
  Serial.println("TxDone");
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  //- Start LoRa --------------------------
  Serial.println("LoRa Receiver");
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
  }
  //LoRa.onTxDone(onTxDone);
  //---------------------------------------

  //- Join GateWay 
  //joinGW();
}
int count = 0;
void loop() {
  /*
  String recv_msg = LoRaReceived();
  if (recv_msg.length() > 5 ) {
    HEADER h = getHeader(recv_msg);
    Serial.println();
    Serial.print(h.from);Serial.print("-");
    Serial.print(h.dest);Serial.print("-");
    Serial.println(h.fn);
  }
  */
  String tmp_string;
  
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("Received packet: '");
    while (LoRa.available()) {
      tmp_string += (char)LoRa.read();
    }
    Serial.print(tmp_string);
    Serial.print("' with RSSI ");
    Serial.println(LoRa.packetRssi());
  }
  
  char buff[11];
  unsigned long prevTime;
  if (millis() - prevTime > interval) {
      prevTime =  millis();
      sprintf(buff,"%02x%02x%02x%04x", NID, BASEID, 0x03,count); // request to jon
      Serial.println(buff);
      LoRa.beginPacket();
        LoRa.print(buff); 
      LoRa.endPacket();    
      count++;
   } 
  
}
