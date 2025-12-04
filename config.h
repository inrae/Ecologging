#ifndef CONFIG_H
#define CONFIG_H

//## nom des fichiers ##
#define DATA_FILENAME "SAMPLE.csv"
#define MOY_FILENAME "AVG.csv"

//## acquisition periodicity ##
const uint8_t SECONDES_CIBLES[] = {0, 20, 40};  //periodicité acquisition donnée
const uint8_t MINUTES_CIBLES[] = {0};           //periodicité calcul moyenne

//## param MQTT ##
#define MOD_SIM7600 1
char mytopic[] = "ecolo/meteo/id_station"; // Topic pour le message MQTT nombre caractère 20

//## config capteurs ##
#define MOD_VEML7700 1    //utilisation d'un luxmettre VEML7700
#define MOD_PYRANO 0      //utilisation d'un pyranometre à la place d'un luxmetre

#define MOD_BME280 1      //capteur T°, Hum, P°
#define MOD_SHT31 0       //capteur T°, Hum
#define MOD_SHT20 0       //capteur T°, Hum

#define MOD_PLUIE 1       //pluviometre

#define MOD_VENT  1       //anemometre Davis

#define MOD_DS18B20 0     //capteur T° eau  
#define MOD_ADS_KIT0139 0 //capteur hauteur eau

//## divers param ##
const uint16_t PROF_SONDE_WL = 1000;  //profondeur installation de la sonde de hauteur d'eau en mm

//_____________##### test config #####_________________
//== Compteur automatique ==
#define THP_CAPTEUR_COUNT (MOD_BME280 + MOD_SHT31 + MOD_SHT20)
#if MOD_BME280 && MOD_SHT31
  #define THP_MERGE 1
#else
  #define THP_MERGE 0
#endif

//== Vérification de cohérence ==
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