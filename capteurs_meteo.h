#include "Stream.h"
/***************
Library for managing weather sensors
P.Bordenave 2025
P.Chaumeil 2026

//projet ECOLOGGING partie meteo
//UEFP Pierre BORDENAVE 

//V1 : utilisation fonction lowpower pour perte d'énergie avec redémarrage et garder le temps en tête
//V2 : mise en place des fonction et des réductions de PROGMEM pour l'affichage monitoring

//####################### ATTENTION ###########################
//Cette Classe nécessite l'appel à Wire.begin(); Dans le setup du programme.
//This class requires a call to Wire.begin(); in the program setup.

//RTC DS3231 ref Gotronic 34360
//https://learn.adafruit.com/adafruit-ds3231-precision-rtc-breakout/arduino-usage
//https://www.gotronic.fr/art-module-horloge-temps-reel-ada3013-24708.htm
//carte SD mini pour ARDUINO MEGA
//https://www.gotronic.fr/art-module-carte-micro-sd-dfr0229-20602.htm
//https://wiki.dfrobot.com/MicroSD_card_module_for_Arduino__SKU_DFR0229
//https://passionelectronique.fr/carte-sd-arduino/
//VEML0077 ref Gotronic 36530
//https://www.gotronic.fr/art-capteur-de-lumiere-veml7700-ada4162-30811.htm
//https://wiki.dfrobot.com/Gravity__I2C_VEML7700_Ambient_Light_Sensor_SKU__SEN0228

//Capteurs T&H
//SEN0227 ref Gotronic ref 35805
//https://www.gotronic.fr/art-sonde-d-humidite-et-de-temperature-sen0227-27842.htm
//https://wiki.dfrobot.com/SHT20_I2C_Temperature_%26_Humidity_Sensor__Waterproof_Probe__SKU__SEN0227
//BME280 ref Gotronic ref 34076
//https://www.gotronic.fr/art-capteur-de-t-humidite-et-pression-ada2652-23892.htm
//https://learn.adafruit.com/adafruit-bme280-humidity-barometric-pressure-temperature-sensor-breakout/downloads
//SHT31 ou SEN0385 ref Gotronic ref 37092
//https://www.gotronic.fr/art-sonde-de-t-et-d-humidite-sht31-sen0385-32863.htm
//https://wiki.dfrobot.com/SHT31_Temperature_Humidity_Sensor_Weatherproof_SKU_SEN0385
//https://github.com/RobTillaart/SHT31?tab=readme-ov-file
//https://github.com/RobTillaart/SHT31

//Vent Anemometer DAVIS orientation et vitesse
//https://www.meteo-shopping.com/fr/capteurs/109-anemometre-girouette-vantage-pro.html?gclid=EAIaIQobChMI7sba6_LC9AIVxrLVCh1H7gPYEAQYASABEgL-YfD_BwE
//http://cactus.io/hookups/weather/anemometer/davis/hookup-arduino-to-davis-anemometer
//https://github.com/PaulStoffregen/TimerOne
//https://playground.arduino.cc/Code/Timer1/
//Pluviomètre Cumul pluie à l'heure IMPORTANT pensez à faire apparaître cette ref de site car utilisation du code
//http://www.instructables.com/id/Arduino-Weather-Station-Part3-Rain/

//Somme de mesure et Moyenne flottante : insipiration et réflexion à la programmation
//https://forum.arduino.cc/t/afficher-une-valeur-moyenne-tout-les-x-valeurs/538069/6
//https://forum.arduino.cc/t/enregistrer-moyenne-sur-une-carte-sd-sur-les-memes-lignes/933751/5

****************/
#ifndef CAPTEURS_METEO_h
#define CAPTEURS_METEO_h

#include <Wire.h>
#include <HardwareSerial.h>
#include "DFRobot_VEML7700.h"       //VEML7700//measure lux en VEML7700
#include "cactus_io_BME280_I2C.h"   //BME280
#include "SHT31.h"                  //SHT31
#include "DFRobot_SHT20.h"          //SHT20/SEN0227
#include <TimerOne.h>               //Davis
#include <OneWire.h>
#include <DallasTemperature.h> 
#include <Adafruit_ADS1X15.h>       //ADS1X15 ADC_4channel (Kit0139 water level)

//parameters
#define RainPin 6                   //Pluviometer Pin
#define SHT31_ADDRESS   0x44        //SHT31

#define WindSensorPin (9)           //The pin location of the anemometer sensor
#define WindVanePin (A4)
#define VaneOffset 0
constexpr uint8_t INTEGRATION_TIME_SEC = 3; // Integration time in seconds (ex: 2, 3, 5, 10...)

#define PyranoPin (A1)              //Pin location for Pyranometer
#define RS485_DE_RE 27               //Pin for DE/RE RS485
constexpr uint8_t DS18B20Pin = 2;            //Pin location for DS18B20 soil temperature
constexpr uint16_t WaterLevelInstall = 1000;  //Installation depth of kit0139 Water level in mm

class CAPTEURS_METEO {

  //####################### PUBLIC SECTION #############################
public:
  CAPTEURS_METEO(uint16_t WLinstall = WaterLevelInstall, uint8_t pin_DS18B20 = DS18B20Pin);

  //++++++++++ Current values ++++++++++
  float valTemp();                        //returns current value
  float valHumid();                       //returns current value
  float valPatm();                        //returns current value
  float valRay();                         //returns current value
  float valPyrano();                      //returns current value
  float valTempWater();                   //returns current value
  float valVitesse();                     //returns current value
  float valMaxVitesse();                  //returns current value
  float valDirection();                   //returns current value
  float valWaterVolt();                   //returns current value
  float valWaterColonne();                //returns current value
  float valWaterHauteur();                //returns current value
  float valTempSEN0600(uint8_t index);    //returns current value
  float valHumidSEN0600(uint8_t index);   //returns current value

  //++++++++++ Averages ++++++++++
  void resetSommes();
  float meanTemp();                        //returns the current average
  float meanHumid();                       //returns the current average
  float meanPatm();                        //returns the current average
  float meanRay();                         //returns the current average
  float meanPyrano();                      //returns the current average
  float meanTempWater();                   //returns the current average
  float meanVitesse();                     //returns the current average
  float meanDirection();                   //returns the current average
  float meanWaterVolt();                   //returns the current average
  float meanWaterColonne();                //returns the current average
  float meanWaterHauteur();                //returns the current average
  float meanTempSEN0600(uint8_t index);    //returns the current average
  float meanHumidSEN0600(uint8_t index);   //returns the current average
  
  //++++++++++ Accumulations ++++++++++
  void resetCumuls();                      //reset daily totals
  void setHcumulPluvio();                  //records the status of the accumulated hours
  double cumulHRain();                     //returns the last hourly total
  double cumulDRain();                     //returns the latest daily total

  //++++++++++ VEML7700 ++++++++++
  void initVEML7700();                      //VEML7700 Radiation Start-up Function
  void acqVEML7700();                       //Acquisition VEML7700

  //++++++++++ Pyranomètre Davis 6450 +++++++++
  void initPyrano();                        //initialisation : check AREF connected to 3.3V (measure 3.21V multimeter)
  void acqPyrano();                         //Acquisition pyrano Davis 6450

  //++++++++++ BME280 ++++++++++
  void initBME280();                        //Initialisation function BME280
  void acqBME280(bool Ponly = 0);           //Acquisition BME280 (T°, Hum, Pressure) (Ponly = 1 if read only Pressure)

  //++++++++++ SHT31 ++++++++++
  void initSHT31();                         //Initialisation function SHT31
  void acqSHT31();                          //Acquisition SHT31 (T°, Hum)

  //++++++++++ SHT20/SEN0227 ++++++++++
  void initSHT20();                         //Initialisation function SHT20
  void acqSHT20();                          //Acquisition SHT20 (T°, Hum)

  //++++++++++ Pluviometer ++++++++++
  void initPluvio();                        //Initialisation function Pluviometer
  void acqPluvio();                         //check status pluvio (run each loop)

  //++++++++++ Anémometer : wind DAVIS ++++++++++
  void initVent1();                         //Initialization function Wind speed and direction
  void initVent2();
  void acqVent();                           //Acquisition Wind speed and direction
  
  //++++++++++ DS18B20 Soil temperature ++++++++++
  void initDS18B20();                       //Initialisation function DS18B20
  void acqDS18B20Teau();                    //Acquisition DS18B20 (T°)

  //++++++++++ ADS1X15 + water level probe KIT0139 Franck Perret ++++++++++
  void initADS();
  void acqADS_kit0139();

  //++++++++++ RS485 SEN0600 ++++++++++
  void initSEN0600(Stream* mySerialPort, uint8_t nbProbes, const uint8_t* addrList);     // Initialize port & pin DE/RE SEN0600
  void acqSEN0600launch();                    // Launch acq session
  void updateSEN0600acq();                    // assync SEN0600 acquisition
  
  //######################### PRIVATE SECTION ##########################
private:
  static const uint8_t MAX_SEN0600_PROBES = 10;       //for memory allocation

  //+++++++++ measured variables +++++++++
  float TempMesure = 0.0; //stores the last measured value
  float HumidMesure = 0.0;
  float PatmMesure = 0.0;
  float RayMesure = 0.0;
  float PyranoMesure = 0.0;
  float TempWaterMesure = 0.0;
  static volatile float VitesseMesure;
  float DirectionMesure = 0.0;
  static volatile float MaxSpeedMesure;
  float WaterVoltMesure = 0.0;
  float WaterColonneMesure = 0.0;
  float WaterHauteurMesure = 0.0;
  float TempSEN0600Mesure[MAX_SEN0600_PROBES];
  float HumidSEN0600Mesure[MAX_SEN0600_PROBES];

  float SommeT = 0.0;
  float SommeHR = 0.0;
  float SommeP = 0.0;
  float SommeL = 0.0;
  float SommeLW = 0.0;
  float SommeTW = 0.0;
  float SommeV = 0.0;
  float SommeSinD = 0.0;
  float SommeCosD = 0.0;
  //float SommeD = 0.0;
  float SommeWV = 0.0;
  float SommeWC = 0.0;
  float SommeWH = 0.0;
  float SommeT_SEN0600[MAX_SEN0600_PROBES];
  float SommeH_SEN0600[MAX_SEN0600_PROBES];

  int nbT = 0;  //number of summed values
  int nbHR = 0;
  int nbP = 0;
  int nbL = 0;
  int nbLW = 0;
  int nbTW = 0;
  int nbV = 0;
  int nbD = 0;
  int nbWH = 0;
  int nbT_SEN0600[MAX_SEN0600_PROBES];
  int nbH_SEN0600[MAX_SEN0600_PROBES];

  //++++++++++ VEML7700 ++++++++++
  float lux;
  DFRobot_VEML7700 als;

  //++++++++++ Pyranometer Davis 6450 +++++++++
  const float pyrReferenceVoltage = 5.0;  // Measured with a multimeter
  const float pyrSensitivity = 0.00167;   // 1,67 mV/W/m²
  const float pyrZeroOffset = 0;

  //++++++++++ BME280 ++++++++++
  BME280_I2C bme;

  //++++++++++ SHT31 ++++++++++
  SHT31 sht31;

  //++++++++++ SHT20/SEN0227 ++++++++++
  DFRobot_SHT20    sht20;

  //++++++++++ Pluviometer ++++++++++
  bool bucketPositionA = false;             // one of the two positions of tipping-bucket               
  const double bucketAmount = 0.11;         // The value of 0.2794mm was given by the manufacturer LEXTRONIC without the DIY cup
  double dailyRain = 0.0;                   // rain accumulated for the day
  double hourlyRain = 0.0;                  // rain accumulated for one hour
  double dailyRain_till_LastHour = 0.0;     // rain accumulated for the day till the last hour          

  //++++++++++ Anemometer : wind DAVIS ++++++++++
  int VaneValue;
  int Direction; 
  int CalDirection;
  int LastValue;
  static volatile unsigned int TimerCount;  //Integration time in seconds
  static volatile unsigned long Rotations;  //Integration time in seconds
  static volatile unsigned long ContactBounceTime;  //Integration time in seconds
  static constexpr float WIND_FACTOR = 3.621015 / (float)INTEGRATION_TIME_SEC; //cf isr_timer() in .cpp

  static void isr_timer();                  // isr routine for timer interrupt
  static void isr_rotation();               // This is the function that the interrupt calls to increment the rotation count
  void acqWindDirection();                  // Get Wind Direction
  void getHeading(int direction);           // Converts compass direction to heading
  
  //++++++++++ DS18B20 Soil temperature ++++++++++
  OneWire oneWire_Teau;
  DallasTemperature sensor_Teau;

  //++++++++++ ADS1X15 + water level probe KIT0139 Franck Perret ++++++++++
  Adafruit_ADS1115 ads;   /* Use this for the 16-bit version */
  uint16_t _WLinstall;     //probe installation depth in mm

  //++++++++++ RS485 SEN0600 ++++++++++
  Stream* _serialRS485;
  uint8_t _SEN0600_nbProbes;
  const uint8_t* _SEN0600_addrList;           //pointer to address list on bus (ex: {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, ...} 
  bool _SEN0600_session_running = false;
  uint8_t _SEN0600_ActiveProbeIndex = 0;
  unsigned long _t_SEN0600_LastAction = 0;
  const unsigned long _SEN0600_PROBE_INTERVAL = 50; // ms
  uint16_t calculateCRC(uint8_t *buf, int len); //compute modbus CRC bytes
  void acqSEN0600(uint8_t index); 		// Acquisition selected probe. index & addr can be identical (usefull if addresses are spread over 0-255 values to avoid reserve large array in memory)


};
#endif //CAPTEURS_METEO_H
