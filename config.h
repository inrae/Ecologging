#ifndef CONFIG_H
#define CONFIG_H

//## file names ##
#define DATA_FILENAME "SAMPLE.csv"
#define MOY_FILENAME "AVG.csv"

//## acquisition periodicity ##
const uint8_t SECONDES_CIBLES[] = {0, 20, 40};  //periodicity given acquisition
const uint8_t MINUTES_CIBLES[] = {0};           //average calculation periodicity

//## param MQTT ##
#define MOD_SIM7600 1
char mytopic[] = "ecolo/meteo/id_station"; // Topic for the MQTT message, number of characters: 20

//## config capteurs ##
#define MOD_VEML7700 1    //use of a VEML7700 lux meter
#define MOD_PYRANO 0      //use of a pyranometer instead of a luxmeter

#define MOD_BME280 1      //sensor T°, Hum, P°
#define MOD_SHT31 0       //sensor T°, Hum
#define MOD_SHT20 0       //sensor T°, Hum

#define MOD_PLUIE 1       //pluviometer

#define MOD_VENT  1       //anemometer Davis

#define MOD_DS18B20 0     //sensor water T° 
#define MOD_ADS_KIT0139 0 //sensor water level

//## various param ##
const uint16_t PROF_SONDE_WL = 1000;  //water level probe installation depth in mm

//_____________##### test config #####_________________
//== Automatic counter ==
#define THP_CAPTEUR_COUNT (MOD_BME280 + MOD_SHT31 + MOD_SHT20)
#if MOD_BME280 && MOD_SHT31
  #define THP_MERGE 1
#else
  #define THP_MERGE 0
#endif

//== Consistency check ==
#if THP_CAPTEUR_COUNT == 0
  #warning "ATTENTION : Aucun capteur T°/Hum défini ! cf config.h"
#elif THP_CAPTEUR_COUNT > 1
  #if THP_MERGE
    #warning "ATTENTION : Fusion capteurs T°/Hum/P° ! cf config.h"
  #else
    #error "ERREUR : Plusieurs capteurs T°/Hum définis ! cf config.h"
  #endif
#endif


#endif
