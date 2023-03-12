/*
GS: Ground Station
by Bjarke Gotfredsen
License: MIT

Receive data like this from the flight station:                    
$GPVTG,224.50,T,,M,0.00,N,0.00,K,A*3C                                           
$GPGGA,075330.000,3411.8889,S,01822.7103,E,1,12,0.85,96.6,M,32.4,M,,*4B         
$PDIWB,24.79,1005.24,66.89*45                                                   
$PQVEL,-0.088864,-0.013409,0.052223*6D                                          

The GPCGA is UTC Time, Lat, Lon, Quality, No of Sats, HDOP, and Altitude in meters.
The PDIWB is my own sentence, with Temperature in ºC, Pressure in hPa (or mb), and QNE Altitude in meters, coming from a pressure sensor (IWB) not related to the GPS.
The PQVEL is North/South velocity, East/West velocity, and Down/Up Velocity in +/- m/s
The GPVTG is True Course over Ground (aka Wind direction), Magnetic Course over Ground (N/A), Speed over Ground in kNots (aka Wind Speed) and Speed over Ground in Kph
*/


#include <SPI.h>
#include <RadioLib.h>  // https://github.com/jgromes/RadioLib
#include <Adafruit_GPS.h>  // https://github.com/adafruit/Adafruit_GPS - for checkdigit calculations
#define SPI_MISO 12
#define SPI_MOSI 13
#define SPI_SCK 14
#define LORA_CS 15  // NSS
#define LORA_DIO0 33
#define LORA_DIO1 -1
#define LORA_RESET -1

SPIClass mySpi(HSPI);
SPISettings spiSettings(2000000, MSBFIRST, SPI_MODE0);
SX1278 radio = new Module(LORA_CS, LORA_DIO0, LORA_RESET, LORA_DIO1, mySpi, spiSettings);

Adafruit_GPS GPS(&Wire);

volatile bool dataReceived = false;
void setFlag(void) {
  dataReceived = true;
}

String addCheckSumCRLF(String s) {
  char buf[100];
  s.toCharArray(buf, s.length() + 1);
  GPS.addChecksum(buf);
  return String(buf);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  mySpi.begin();
  ESP_LOGI("GS", "Atmospheric Sounding");

#define SX127X_SYNC_WORD 0x12
  // Using https://loratools.nl/, we calculate an airtime of around a 1 second
  int state = radio.begin(434.0, 250, 10, 7, SX127X_SYNC_WORD, 20, 8, 0);
  if (state == RADIOLIB_ERR_NONE) {
    ESP_LOGI("GS", "LoRa connected successfully!");
  } else {
    ESP_LOGE("GS", "LoRa Failed. Code=%i", state);
    vTaskDelete(NULL);  // instead of while (true);
  }

  radio.setDio0Action(setFlag);
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    ESP_LOGI("GS", "LoRa listening successfully!");
  } else {
    ESP_LOGE("GS", "LoRa Failed. Code=%i", state);
    vTaskDelete(NULL);  // instead of while (true);
  }
}

void loop() {
  if (dataReceived) {
    dataReceived = false;
    String str;
    int state = radio.readData(str);
    if (state == RADIOLIB_ERR_NONE) {
      str.trim();
      // str = str + "," + String(radio.getRSSI()) + "," + String(radio.getSNR());
      Serial.println(str);
      str = addCheckSumCRLF("$PDOMR," + String(radio.getRSSI()) + "," + String(radio.getSNR()));
      Serial.println(str);
    }
    state = radio.startReceive();
  }
}
