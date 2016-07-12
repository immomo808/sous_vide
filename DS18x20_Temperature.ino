#include <OneWire.h>
#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>


OneWire  ds(3);  // on pin 10 (a 4.7K resistor is necessary)
int PowerSwitch = 2;
double targetTem = 60;
double LowTem = 5;
double HighTem = 2.5;
int16_t raw;
unsigned long OnWindowSize = 2000;
unsigned long OffWindowSize = 2000;
unsigned long Window;
unsigned long StartTime;
bool CoolFlag = false;
bool ChangeFlag = false;
bool OnFlag;
YunServer server;


void setup(void) {
  Serial.begin(9600);
  pinMode(PowerSwitch,OUTPUT);
  Bridge.begin();Bridge.begin();
  server.listenOnLocalhost();
  server.begin();
  StartTime = millis();
}

void getCommand(YunClient client) {
  //Serial.println(client.readString());
  String com = client.readStringUntil('/');
  //Serial.println(com == "tem");
  if ( com == "tem"){
    //client.print(F("Temperature"));
    client.print((float)raw/16.0);
  }else if(com == "time"){
    client.print((millis() -StartTime)/(double)1000);
  }
}
void setCommand(YunClient client) {
  String com = client.readStringUntil('/');
  if ( com == "tem"){
    //client.print(F("Temperature"));
    int value = client.parseInt();
    if (value > 70 or value <40){
      client.print(F("Error"));
    }else{
      targetTem = value;
      client.print(F("Set Temperature to"));
      client.print(targetTem);
    }
  }
}

void process(YunClient client) {
  // read the command
  String command = client.readStringUntil('/');

  if (command == "get") {
    getCommand(client);
  }

  if (command == "set") {
    setCommand(client);
  }

  if (command == "start") {
    //startCommand(client);
  }
  if (command == "stop") {
    //stopCommand(client);
  }
}

void loop(void) {
  YunClient client = server.accept();
  if (client) {
    // Process request
    process(client);

    // Close connection and free resources.
    client.stop();
  }
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;
  
  if ( !ds.search(addr)) {
    //Serial.println("No more addresses.");
    //Serial.println();
    ds.reset_search();
    delay(250);
    return;
  }
  /*
  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }
*/
  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return;
  }
  Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      //Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return;
  } 

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  //Serial.print("  Data = ");
  //Serial.print(present, HEX);
  //Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    //Serial.print(data[i], HEX);
    //Serial.print(" ");
  }
  //Serial.print(" CRC=");
  //Serial.print(OneWire::crc8(data, 8), HEX);
  //Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  //fahrenheit = celsius * 1.8 + 32.0;
  Serial.print("  Temperature = ");
  Serial.print(celsius);
  Serial.print(" Celsius, ");
  
  if (celsius < targetTem - LowTem){
    digitalWrite(PowerSwitch,HIGH);
    Serial.print("on");
    ChangeFlag = false;
    OnFlag = true;
    CoolFlag = false;
  }
  if (celsius > targetTem - LowTem && !ChangeFlag){
    digitalWrite(PowerSwitch,LOW);
    Serial.print("off");
    ChangeFlag = true;
    OnFlag = false;
  }
  if (celsius > targetTem){
    digitalWrite(PowerSwitch,LOW);
    Serial.print("off");
    ChangeFlag = false;
    OnFlag = false;
    CoolFlag = true;
  }
   if (celsius < (targetTem - HighTem) && CoolFlag){
    digitalWrite(PowerSwitch,HIGH);
    Serial.print("on");
    ChangeFlag = true;
    OnFlag = true;
    CoolFlag = false;
  } 
  if (celsius > targetTem){
    digitalWrite(PowerSwitch,LOW);
    Serial.print("off");
    ChangeFlag = false;
    OnFlag = false;
    CoolFlag = true;
  }
  if (ChangeFlag && !CoolFlag){
    unsigned long now = millis();
    if (now > Window){
      if (OnFlag){
        digitalWrite(PowerSwitch,LOW);
        Window = now + OffWindowSize;
        Serial.print("off");
        OnFlag = false;
      }else{
        digitalWrite(PowerSwitch,HIGH);
        Window = now + OnWindowSize;
        Serial.print("on");
        OnFlag = true;
      }
    }
  }

}
