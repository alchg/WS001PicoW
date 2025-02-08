#include "Station.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <GxEPD2_3C.h>
#include "GxEPD2_display_selection_new_style.h"
#include <Fonts/FreeMonoBold9pt7b.h>
#include "Bitmap.h"

Station::Station(){
}

void Station::begin(){
  setenv("TZ", "JST-9", 1);
  tzset();
  display.init(115200, true, 2, false); // 115200 to 0 false serial
  display.setRotation(3);
  display.setFont(&FreeMonoBold9pt7b);
  front_color = GxEPD_BLACK;
  back_color = GxEPD_WHITE;
  display.setTextColor(front_color);

}

void Station::setConfig(const char* _ssid, const char* _password, const char* _area_code, const char* _area_code_sub, const char* _spot_code, const char* _area_code_week, bool _dark_mode){
  strncpy(ssid, _ssid,99);
  strncpy(password, _password,99);
  strncpy(area_code, _area_code,99);
  strncpy(area_code_sub, _area_code_sub,99);
  strncpy(spot_code, _spot_code,99);
  strncpy(area_code_week, _area_code_week,99);
  dark_mode = _dark_mode;
}

void Station::setColorMode(){
  if(dark_mode){
    front_color = GxEPD_WHITE;
    back_color = GxEPD_BLACK;
  }else{
    front_color = GxEPD_BLACK;
    back_color = GxEPD_WHITE;
  }
  display.setTextColor(front_color);
}

bool Station::tryWLAN(){
  bool ret = false;

  WiFi.begin(ssid, password);
  for(int i=0;i<10;i++){
    delay(1000);
    if(WiFi.status() == WL_CONNECTED){
      ret = true;
      break;
    }
  }

  return ret;
}

bool Station::tryNTP(){
  bool ret = false;

  configTime(60 * 60 * 9, 0, "ntp.jst.mfeed.ad.jp", "ntp.nict.jp");
  time_t nowSecs = time(nullptr);
  for(int i=0;i < 30;i++){
    if(nowSecs > 60 * 60 * 24 * 365 * 10){
      ret = true;
      break;
    }
    delay(1000);
    nowSecs = time(nullptr);
  }

  return ret;
}

bool Station::getWeather(){
  bool ret = false;

  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client -> setCACert(rootCACertificate);
    {
      HTTPClient https;
      String url = "https://www.jma.go.jp/bosai/forecast/data/forecast/";
      url += area_code;
      url += ".json";
      if(https.begin(*client, url)){
        int httpCode = https.GET();
        if (httpCode == HTTP_CODE_OK){
          payload = https.getString();
          ret = true;
        }else{
          Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }
        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }
    }
    delete client;
  }
  
  return ret;
}


bool Station::parseWeather(){
  JsonDocument doc;
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  String localDate = String(timeinfo.tm_year + 1900) + "-" + (timeinfo.tm_mon + 1 < 10 ? "0" : "") + String(timeinfo.tm_mon + 1) + "-" + (timeinfo.tm_mday < 10 ? "0" : "") + String(timeinfo.tm_mday);
  Serial.println("localDate      :" + localDate);
  
  DeserializationError error = deserializeJson(doc, payload);
  if(error){
    Serial.println("Deserialization error.");
    return false;
  }else{
    String tmpTimeDefine = "";
    int idxTodayDefWeatherCode;
    for(int i=0;i < doc[0]["timeSeries"][0]["timeDefines"].size();i++){
      tmpTimeDefine = doc[0]["timeSeries"][0]["timeDefines"][i].as<String>().substring(0,10);
      if(tmpTimeDefine == localDate){
        idxTodayDefWeatherCode = i;
        Serial.println("idxTodayDefWeatherCode:" + String(idxTodayDefWeatherCode));
        break;
      }
    }

    bool noWeatherCodes = true;
    for (const auto& area : doc[0]["timeSeries"][0]["areas"].as<JsonArray>()) {
      if (area["area"]["code"] == area_code_sub) {
        JsonArray weatherCodes = area["weatherCodes"];
        weather_info.code = weatherCodes[idxTodayDefWeatherCode];
        noWeatherCodes = false;
        Serial.println("code:" + String(weather_info.code));
        break;
      }
    }
    if(noWeatherCodes){
      return false;
    }

    int idx_area_sub = NO_DATA;
    for(int i=0;i < doc[0]["timeSeries"][1]["areas"].size();i++){
      if(doc[0]["timeSeries"][1]["areas"][i]["area"]["code"].as<String>() == area_code_sub){
        idx_area_sub = i;
        break;
      }
    }
    Serial.println("idx_area_sub:" + String(idx_area_sub));
    if(idx_area_sub == NO_DATA){
      return false;
    }

    for(int i=0;i < 4;i++){
      weather_info.pops[i] = NO_DATA;
    }
    for(int i=0;i < doc[0]["timeSeries"][1]["timeDefines"].size();i++){
      String tmpPopDate = doc[0]["timeSeries"][1]["timeDefines"][i].as<String>().substring(0,13);
      //Serial.println(tmpPopDate);
      if(idx_area_sub != NO_DATA){
        int tmpPop = doc[0]["timeSeries"][1]["areas"][idx_area_sub]["pops"][i].as<int>();
        if(tmpPopDate == localDate + "T00"){
          weather_info.pops[0] = tmpPop;
          //Serial.println("T00:" + String(weather_info.pops[0]));
        }
        if(tmpPopDate == localDate + "T06"){
          weather_info.pops[1] = tmpPop;
          //Serial.println("T06:" + String(weather_info.pops[1]));
        }
        if(tmpPopDate == localDate + "T12"){
          weather_info.pops[2] = tmpPop;
          //Serial.println("T12:" + String(weather_info.pops[2]));
        }
        if(tmpPopDate == localDate + "T18"){
          weather_info.pops[3] = tmpPop;
          //Serial.println("T18:" + String(weather_info.pops[3]));
        }
      }
    }

    weather_info.max_temp = NO_DATA;
    weather_info.min_temp = NO_DATA;
    int idxTodayDefTemp[2];
    bool enableTemp = false;
    for (int i=0;i < doc[0]["timeSeries"][2]["timeDefines"].size();i++){
      String tmpTempDate = doc[0]["timeSeries"][2]["timeDefines"][i].as<String>().substring(0, 10);
      //Serial.println(tmpTempDate);
      if(localDate == tmpTempDate){
        idxTodayDefTemp[i] = i;
        enableTemp = true;
        Serial.println("idxTodayDefTemp[" + String(i) + "]:" + String(idxTodayDefTemp[i]));
      }
    }

    if(enableTemp){
      Serial.println("spot_code:" + String(spot_code));
      bool noSpotCode = true;
      for(int i=0;i < doc[0]["timeSeries"][2]["areas"].size();i++){
        if(doc[0]["timeSeries"][2]["areas"][i]["area"]["code"].as<String>() == spot_code){
          noSpotCode = false;
          break;
        }
      }
      if(noSpotCode){
        return false;
      }
  
      for(const auto& area : doc[0]["timeSeries"][2]["areas"].as<JsonArray>()){
        if(area["area"]["code"] == spot_code){
          for(int i=0;i < 2;i++){
            if(i == 0){
              weather_info.min_temp = area["temps"][idxTodayDefTemp[i]];
              Serial.println("min_temp:" + String(weather_info.min_temp));
            }
            if(i == 1){
              weather_info.max_temp = area["temps"][idxTodayDefTemp[i]];
              Serial.println("max_temp:" + String(weather_info.max_temp));
            }
          }
        }
      }
      if(weather_info.min_temp == weather_info.max_temp){
        weather_info.min_temp = NO_DATA;
        Serial.println("min_temp:" + String(weather_info.min_temp));
      }
    }

    tmpTimeDefine = "";
    for(int i=0;i < doc[1]["timeSeries"][0]["timeDefines"].size();i++){
      tmpTimeDefine = doc[1]["timeSeries"][0]["timeDefines"][i].as<String>().substring(0,10);
      //Serial.println("tmpTimeDefine:" + String(tmpTimeDefine));
      weather_info.codes_date[i] = tmpTimeDefine;
      //Serial.println("codes_date[" + String(i) + "]:" + String(weather_info.codes_date[i]));
    }

    noWeatherCodes = true;
    for (const auto& area : doc[1]["timeSeries"][0]["areas"].as<JsonArray>()) {
      if (area["area"]["code"] == area_code_week) {
        JsonArray weatherCodes = area["weatherCodes"];
        for(int i=0;i < weatherCodes.size();i++){
          weather_info.codes[i] = weatherCodes[i];
          //Serial.println("codes[" + String(i) + "]:" + String(weather_info.codes[i]));
        }
        noWeatherCodes = false;
        break;
      }
    }
    if(noWeatherCodes){
      return false;
    }
  }

  return true;
}

void Station::dispWeather(){
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  String localDate = String(timeinfo.tm_year + 1900) + "-" + (timeinfo.tm_mon + 1 < 10 ? "0" : "") + String(timeinfo.tm_mon + 1) + "-" + (timeinfo.tm_mday < 10 ? "0" : "") + String(timeinfo.tm_mday);
  String localTime = (timeinfo.tm_hour < 10 ? "0" : "") + String(timeinfo.tm_hour) + ":" + (timeinfo.tm_min < 10 ? "0" : "") + String(timeinfo.tm_min);

  display.firstPage();
  do{
    display.fillScreen(back_color);
    display.drawRect(0, 0, display.width(), display.height(), front_color);
    display.drawLine(0, 50, display.width(), 50, front_color);
    dispIcon(1,1,weather_info.code);
    display.drawLine(50, 23, 85, 23, front_color);
    dispText(50, 0, 35, 23, cnvNum(weather_info.max_temp));
    dispText(50, 25, 35, 23, cnvNum(weather_info.min_temp));
    display.drawLine(90, 0, 90, 50, front_color);
    dispText(90, 0, 40, 50, cnvNum(weather_info.pops[0]));
    display.drawLine(130, 0, 130, 50, front_color);
    dispText(130, 0, 40, 50, cnvNum(weather_info.pops[1]));
    display.drawLine(170, 0, 170, 50, front_color);
    dispText(170, 0, 40, 50, cnvNum(weather_info.pops[2]));
    display.drawLine(210, 0, 210, 50, front_color);
    dispText(210, 0, 40, 50, cnvNum(weather_info.pops[3]));
    int offsetDate = 0;
    if(localDate == weather_info.codes_date[0]){
      offsetDate = 1;
    }
    Serial.println("offsetDate:" + String(offsetDate));
    display.drawLine(50, 50, 50, display.height(), front_color);
    dispIcon(1, 51, weather_info.codes[0 + offsetDate]);
    dispText(1, 101, 50, 22, weather_info.codes_date[0 + offsetDate].substring(8,10));
    display.drawLine(100, 50, 100, display.height(), front_color);
    dispIcon(51, 51, weather_info.codes[1 + offsetDate]);
    dispText(51, 101, 50, 22, weather_info.codes_date[1 + offsetDate].substring(8,10));
    display.drawLine(150, 50, 150, display.height(), front_color);
    dispIcon(101, 51, weather_info.codes[2 + offsetDate]);
    dispText(101, 101, 50, 22, weather_info.codes_date[2 + offsetDate].substring(8,10));
    dispText(151, 51, 100, 24, localDate.substring(5,10));
    dispText(151, 75, 100, 24, localTime);
    dispText(151, 99, 100, 24, "Update");
  }while (display.nextPage());
  display.hibernate();
}

void Station::dispIcon(int x, int y,uint16_t code){

  switch(code){
    case 200:
    case 209:
      // cloud
      display.drawBitmap(x, y, cloud_bitmap, 48, 48, front_color);
      break;
    case 204:
    case 205:
    case 215:
    case 216:
    case 217:
    case 228:
    case 229:
    case 230:
    case 250:
    case 400:
    case 402:
    case 405:
    case 406:
    case 407:
    case 413:
    case 421:
    case 425:
    case 450:
      // snow
      display.drawBitmap(x, y, snow_bitmap, 48, 48, front_color);
      break;
    case 101:
    case 110:
    case 111:
    case 130:
    case 131:
    case 132:
    case 201:
    case 210:
    case 211:
    case 223:
      // sun cloud
      display.drawBitmap(x, y, small_sun_bitmap, 48, 48, GxEPD_RED);
      display.drawBitmap(x, y, small_cloud_bitmap, 48, 48, front_color);
      break;
    case 104:
    case 105:
    case 115:
    case 116:
    case 117:
    case 124:
    case 401:
    case 411:
    case 420:
      // sun snow
      display.drawBitmap(x, y, small_sun_bitmap, 48, 48, GxEPD_RED);
      display.drawBitmap(x, y, snow_bitmap, 48, 48, front_color);
      break;
    case 106:
    case 107:
    case 118:
    case 160:
    case 170:
    case 181:
    case 316:
    case 361:
      // sun rain snow
      display.drawBitmap(x, y, small_sun_bitmap, 48, 48, GxEPD_RED);
      display.drawBitmap(x, y, rain_snow_bitmap, 48, 48, front_color);
      break;
    case 102:
    case 103:
    case 108:
    case 112:
    case 113:
    case 114:
    case 119:
    case 120:
    case 121:
    case 122:
    case 123:
    case 125:
    case 126:
    case 127:
    case 128:
    case 140:
    case 301:
    case 311:
    case 320:
    case 323:
    case 324:
    case 325:
      // sun rain
      display.drawBitmap(x, y, small_sun_bitmap, 48, 48, GxEPD_RED);
      display.drawBitmap(x, y, rain_bitmap, 48, 48, front_color);
      break;
    case 100:
      // sun
      display.drawBitmap(x, y, sun_bitmap, 48, 48, GxEPD_RED);
      break;
    case 206:
    case 207:
    case 218:
    case 260:
    case 270:
    case 281:
    case 303:
    case 304:
    case 309:
    case 314:
    case 315:
    case 317:
    case 322:
    case 326:
    case 327:
    case 340:
    case 371:
    case 403:
    case 409:
    case 414:
    case 422:
    case 423:
    case 426:
    case 427:
      // rain snow
      display.drawBitmap(x, y, rain_snow_bitmap, 48, 48, front_color);
      break;
    case 202:
    case 203:
    case 208:
    case 212:
    case 213:
    case 214:
    case 219:
    case 220:
    case 221:
    case 222:
    case 224:
    case 225:
    case 226:
    case 231:
    case 240:
    case 300:
    case 302:
    case 306:
    case 308:
    case 313:
    case 321:
    case 328:
    case 329:
    case 350:
      // rain
      display.drawBitmap(x, y, rain_bitmap, 48, 48, front_color);
      break;
    default:
      dispText(x, y, 50, 22, String(code));
      break;
  }
}

void Station::dispText(int x, int y, uint16_t width, uint16_t height, String text){
  int16_t tbx, tby; uint16_t tbw, tbh;
  
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
  if(width > tbw && height > tbh){
    uint16_t tx = (width - tbw) / 2 - tbx + x;
    uint16_t ty = (height - tbh) / 2 - tby + y;
    display.setCursor(tx, ty);
    display.println(text);
  }else{
    display.setCursor(x - tbx, y - tby);
    display.println("ERR");
  } 
}

String Station::cnvNum(int num){
  String ret = "";

  if(num == NO_DATA){
    ret = "-";
  }else{
    ret = String(num);
  }

  return ret;
}

void Station::dispError(String text){
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  uint16_t tx, ty;

  display.firstPage();
  do{
    display.fillScreen(back_color);
    display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
    tx = -(tbx);
    ty = -(tby);
    display.setCursor(tx, ty);
    display.println(text);

    display.getTextBounds("ssid:" + String(ssid), 0, 0, &tbx, &tby, &tbw, &tbh);
    tx = -(tbx);
    ty = 35 -(tby);
    display.setCursor(tx, ty);
    display.println("ssid:" + String(ssid));
    display.println("area code:" + String(area_code));
    display.println("area code sub:" + String(area_code_sub));
    display.println("spot code:" + String(spot_code));
    display.println("area code week:" + String(area_code_week));

  }while(display.nextPage());
}

void Station::sleep(bool enableNTP){
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  uint64_t wait_sec;
  if(enableNTP){
    if(timeinfo.tm_hour == 0){
      if(tryWLAN()){
        Serial.println("WLAN OK");
      }
      if(tryNTP()){
        Serial.println("NTP OK");
      }
    }
    wait_sec = 3600;
  }else{
    wait_sec = 900;
  }

  uint64_t wait_ms= 1000ULL * wait_sec;
  Serial.println("Go to sleep");
  delay(wait_ms);
}
