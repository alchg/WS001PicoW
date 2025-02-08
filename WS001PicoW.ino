#include "Station.h"

#define RETRY 5

Station station;
void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("Start");

  station.begin();

/*        CONFIG        */
  station.setConfig(
    "ssid", // ssid
    "password", // password
    "130000", // area_code
    "130010", // area_code_sub
    "44132", // spot_code
    "130010", // area_code_week
    false // dark_mode
  );
/*        CONFIG        */

  station.setColorMode();

  bool enableWLAN = false;
  while(enableWLAN == false){
    for(int i=0;i < RETRY;i++){
      if(station.tryWLAN()){
        enableWLAN = true;
        Serial.println("WLAN OK");
        break;
      }else{
        Serial.println("WLAN ERR");
        delay(1000);
        if(i == RETRY - 1){
          station.dispError("WLAN ERROR");
        }
      }
    }
  }

  bool enableNTP = false;
  while(enableNTP == false){
    for(int i=0;i < RETRY;i++){
      if(station.tryNTP()){
        enableNTP = true;
        Serial.println("NTP OK");
        break;
      }else{
        Serial.println("NTP ERR");
        delay(1000);
        if(i == RETRY - 1){
          station.dispError("NTP ERROR");
        }
      }
    }
  }

}

void loop() {

  bool enableWLAN = false;
  for(int i=0;i < RETRY;i++){
    if(station.tryWLAN()){
      enableWLAN = true;
      Serial.println("WLAN OK");
      break;
    }else{
      Serial.println("WLAN ERR");
      delay(1000);
      if(i == RETRY - 1){
        station.dispError("WLAN ERROR");
      }
    }
  }

  bool enableJSON = false;
  if(enableWLAN){
    for(int i=0;i < RETRY;i++){
      if(station.getWeather()){
        enableJSON = true;
        Serial.println("GET OK");
        break;
      }else{
        Serial.println("GET ERR");
        delay(1000);
        if(i == RETRY - 1){
          station.dispError("HTTP ERROR");
        }
      }
    }
  }

  bool enableParse = false;
  if(enableJSON){
    if(station.parseWeather()){
      enableParse = true;
      Serial.println("PARSE OK");
    }else{
      station.dispError("DATA ERROR");
      Serial.println("PARSE ERR");
    }
  }

  if(enableParse){
    station.dispWeather();
  }

  station.sleep(enableParse);

}
