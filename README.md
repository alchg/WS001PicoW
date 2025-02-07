# WS001PicoW  
[WS001](https://github.com/alchg/WS001) for RaspberryPiPicoW  

# 必要なもの  
RaspberryPiPicoW  
2.13 epaper module [MH-ET LIVE 2.13 RED]  
![epaper](epaper.png)  

# 配線  
|GPIO|ePaper|note|
|:----|:----|:----|
|17|CS|CSn|
|18|SCLK|SCK|
|19|SDI|TX|
|20|D/C||
|21|Reset||
|22|Busy||
|3V3|VCC|OUT|
|GND|GND||

# 利用手順  
ArduinoIDEを使用する。  
WS001PicoW.inoのCONFIG部分を修正する。  
[エリアコード一覧](https://github.com/alchg/WS001/blob/main/%E3%83%89%E3%82%AD%E3%83%A5%E3%83%A1%E3%83%B3%E3%83%88/101%E3%82%A8%E3%83%AA%E3%82%A2%E3%82%B3%E3%83%BC%E3%83%89.xlsx)  
アップロードする。  

