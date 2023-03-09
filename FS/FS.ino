/*
FS: Flight Station
by Bjarke Gotfredsen
License: MIT

Send data like this to a ground station:                    
$GPVTG,224.50,T,,M,0.00,N,0.00,K,A*3C                                           
$GPGGA,075330.000,3411.8889,S,01822.7103,E,1,12,0.85,96.6,M,32.4,M,,*4B         
$MAXIQ,24.79,1005.24,66.89*45                                                   
$PQVEL,-0.088864,-0.013409,0.052223*6D                                          

The GPCGA is UTC Time, Lat, Lon, Quality, No of Sats, HDOP, and Altitude in meters.
The MAXIQ is my own sentence, with Temperature in ºC, Pressure in hPa (or mb), and QNE Altitude in meters, coming from a pressure sensor (IWB) not related to the GPS.
The PQVEL is North/South velocity, East/West velocity, and Down/Up Velocity in +/- m/s
The GPVTG is True Course over Ground (aka Wind direction), Magnetic Course over Ground (N/A), Speed over Ground in kNots (aka Wind Speed) and Speed over Ground in Kph
*/

#include <SPI.h>           // Builtin
#include <Adafruit_GPS.h>  // https://github.com/adafruit/Adafruit_GPS
#include <SPL06-007.h>     // https://github.com/rv701/SPL06-007
#include <RadioLib.h>      // https://github.com/jgromes/RadioLib

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

String s = "";
char savec = 0;
double standard_pressure = 1013.25;
char buf[100];

String addCheckSumCRLF(String s) {
  s.toCharArray(buf, s.length() + 1);
  GPS.addChecksum(buf);
  return String(buf) + "\r\n";
}

void setup() {
  Wire.begin(26, 27);
  delay(1000);
  ESP_LOGI("FS", "Atmospheric Sounding");
  delay(1000);

  GPS.begin(0x10);

  // Create a command telling the GPS that we only want GGA and VTG of the standard NMEA 0183 sentences
  s = addCheckSumCRLF("$PMTK314,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
  s.toCharArray(buf, s.length() + 1);
  ESP_LOGI("FS", "%.1f: Setting: %s", millis(), buf);
  GPS.sendCommand(buf);
  delay(1000);

  // Send data every second
  ESP_LOGI("FS", "%.1f: Setting: %s", millis(), PMTK_SET_NMEA_UPDATE_1HZ);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  delay(1000);

  // Tell the GPS to go into balloon mode, it will then operate until 80km
  ESP_LOGI("FS", "%.1f: Setting: %s", millis(), "$PMTK886,3*2B");
  GPS.sendCommand("$PMTK886,3*2B");
  delay(1000);

  // Tell the GPS to send velocities, special for this GPS
  ESP_LOGI("FS", "%.1f: Setting: %s", millis(), "$PQVEL,W,1,1*25");
  GPS.sendCommand("$PQVEL,W,1,1*25");
  delay(1000);

  mySpi.begin();
#define SX127X_SYNC_WORD 0x12
  // Using https://loratools.nl/, we calculate an airtime of around a 1 second
  int state = radio.begin(434.0, 250, 10, 7, SX127X_SYNC_WORD, 20, 8, 0);
  if (state == RADIOLIB_ERR_NONE) {
    ESP_LOGI("FS", "LoRa connected successfully!");
  } else {
    ESP_LOGE("FS", "LoRa Failed. Code=%i", state);
    vTaskDelete(NULL);  // instead of while (true);
  }

  SPL_init(0x77);
}

void downlink(String s) {
  ESP_LOGI("FS", "%.1f: %s", millis(), s);
  radio.transmit(s);
}

void loop() {
  char c = GPS.read();
  if (c) {
    if (c == '$') {
      downlink(s);
      s = "";
    }
    s += c;

    if (savec == '$' && c == 'P') {  // If we are about to build the $PQVEL, then just push this out first
      // Create our own sentence with temperature, pressure and QNE Altitude (Flight level,but in meters)
      downlink(addCheckSumCRLF("$MAXIQ," + String(get_temp_c()) + "," + String(get_pressure()) + "," + String(get_altitude(get_pressure(), standard_pressure))));
      s = "$P";
    }

    savec = c;
  }
}
