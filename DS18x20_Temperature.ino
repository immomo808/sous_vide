#include <OneWire.h>
#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>

#define StageNum 5

OneWire ds(3);          //DS18B20 sensor on digital 3
int PowerSwitch = 2;    //Relay on digital 2

int Stage = -1;
double TargetTem = 60;
double StageTem[StageNum] = [10, 8, 6, 4, 2]
unsigned long StartTime;
unsigned long OnWindowSize[StageNum] = [10000, 8000, 6000, 4000, 2000];
unsigned long Window;
unsigned long StartTime;
unsigned long Timer = 3600000;
bool OnFlag = false;
bool HeatFlag = false;
float celsius;
YunServer Server;

void setup(void) {
    Serial.begin(9600);
    pinMode(PowerSwitch,OUTPUT);
    Bridge.begin();
    Server.listenOnLocalhost();
    Server.begin();
}

void getCommand(YunClient Client) {
    Client.println("Status: 200");
    Client.println("Content-type: application/json");
    Client.print("{");
    Client.print("\"tem\":");
    Client.print("\"" + celsius + "\",");
    Client.print("\"target\":");
    Client.print("\"" + TargetTem + "\",");
    Client.print("\"timer\":");
    Client.print("\"" + Timer + "\",");
    Client.print("\"status\":");
    Client.print("\"" + OnFlag + "\",");
    if (OnFlag){
        Client.print("\"start\":");
        Client.print("\"" + StartTime + "\",");
        Client.print("\"duration\":");
        Client.print("\"" + millis() - Startime + "\",");
        Client.print("\"stage\":");
        Client.print("\"" + Stage + "\",");
    }
    Client.print("}");
}
void setCommand(YunClient client) {
    String com = client.readStringUntil('/');
    if ( com == "tem"){
        int value = client.parseInt();
        if (value > 70 or value <40){
            client.print(F("Error"));
        }else{
            TargerTem = value;
            client.print(F("Set Temperature to"));
            client.print(TargetTem);
        }
        return
    }else if ( com == "timer" ){
        int value = client.parseInt();
        if (value > 300 or value < 10){
            client.print(F("Error"));
        }else{
            Timer = value * 3600 * 1000;
            client.print(F("Set Timer to"));
            client.print(Timer);
        }
        return
    }
}

int process(YunClient client) {
    String command = client.readStringUntil('/');

    if (command == "get") {
        getCommand(client);
        Serial.println("Get by web command!");
        return 0;
    }

    if (command == "set") {
        if (!OnFlag){
            setCommand(client);
            Serial.println("Set by web command!");
        }else{
            client.print(F("Is heating now, can't chhange the setting."));
            Serial.println("Set by web command, Error!");
        }
        return 0;
    }

    if (command == "start") {
        if (!OnFlag){
            if ( celsius > (TargetTem - StageTem[0])){
                client.print(F("The temperature is too high, can't start heating."));
                Serial.println("Start by web command, Error!");
            }
        }else{
            heatloop(client);
        }
    }else{
        client.print(F("Is already heating now."));
    }

    if (command == "stop") {
        if (OnFlag){
            return 1;
        }else{
            client.print(F("Is not heating now."));
            Serial.println("Stop by web command, Error!");
        }
    }
    return 0;
}

void GetTem(){
    int16_t raw;
    byte i;
    byte present = 0;
    byte type_s = 1;
    byte data[12];
    byte addr[8];
    if ( !ds.search(addr)) {
        Serial.println("No more addresses.");
        Serial.println();
        ds.reset_search();
        delay(250);
        return;
    }
    ds.reset();
    ds.select(addr);
    ds.write(0x44, 1); 

    delay(1000);

    present = ds.reset();
    ds.select(addr);    
    ds.write(0xBE);         // Read Scratchpad

    for ( i = 0; i < 9; i++) {           // we need 9 bytes
        data[i] = ds.read();
    }
    raw = (data[1] << 8) | data[0];
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
        raw = (raw & 0xFFF0) + 12 - data[6];
    }
    celsius = (float)raw / 16.0;
    Serial.print("  Temperature = ");
    Serial.println(celsius);
}
void Fire(bool h){
    digitalWrite(PowerSwitch, h ? HIGH : LOW);
    HeatFlag = h;
    Serial.print(" Switch HeatFlag to");
    Serial.println(h);
}

void heatloop(YunClient Client){
    Client.stop();
    Serial.println("Start by web command!");
    OnFlag = true;
    Stage = 0;
    unsigned long Now = millis();
    StartTime = Now;
    Window = Now;
    while((Now - StartTime) < Timer){
        GetTem();
        Client = Server.accept();
        if (Client) {
            if (process(Client)){
                Fire(false);
                Onflag = false;
                Stage = -1;
                Serial.println("Stop by web command!");
                return;
            }
            Client.stop();
        }
        if (celsius > (TargetTem - StageTem[Stage])){
            Stage += 1;
            Serial.print(" Change to Stage:");
            Serial.println(Stage);
        }
        if (Stage > 0 && celsius < (TargetTem - StageTem[Stage - 1])){
            Stage -= 1;
            Serial.print(" Change to Stage:");
            Serial.println(Stage);
        }
        if (Stage != StageNum){
            if (Now > Window) {
                Fire(!HeatFlag);
                Window = Window + ((HeatFlag) ? OnWindowSize[0] : 10000 - OnWindowSize[0]);
                Serial.print("New Window");
                Serial.println(Window);
            }
        }else{
            Fire(false);
            Window = Now;
            Serial.println("Reach the target, Cooling");
        }
        Now = millis();
    }
    Fire(false);
    Onflag = false;
    Stage = -1;
    Serial.println("Reach the time, Stop!");
    return;
}

void loop(void) {
    GetTem();
    YunClient Client = Server.accept();
    if (Client) {
        process(Client);
        Client.stop();
    }
}
