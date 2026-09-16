#ifndef CONFIG_H
  #define CONFIG_H

  //## file names ##
  #define DATA_FILENAME "SAMPLE.csv"
  #define MOY_FILENAME "AVG.csv"

  //## acquisition periodicity ##
  const uint8_t SECONDES_CIBLES[] = {0, 20, 40};  //periodicity given acquisition
  const uint8_t MINUTES_CIBLES[] = {0, 15, 30, 45};       //average calculation periodicity (Mean_integration should be change if MINUTES_CIBLES modified)
  #define AVG_INTEGRATION 30                      //periodicity in min published in mqtt payload

  //## param transmission ##
  #define MOD_KIM2 0

  #define MOD_SIM7600 0
  #define MY_TOPIC "meteo/testvent"      // Topic for the MQTT message (4G only), number of characters: 20

  //## config Time sync
  #define MOD_GPS 1         //use of a TEL0157 GNSS module I2C setting

  //## config sensors ##
  #define MOD_VEML7700 1    //use of a VEML7700 lux meter
  #define MOD_PYRANO 0      //use of a pyranometer instead of a luxmeter

  #define MOD_BME280 1      //sensor T°, Hum, P°
  #define MOD_SHT31 1       //sensor T°, Hum
  #define MOD_SHT20 0       //sensor T°, Hum

  #define MOD_PLUIE 1       //pluviometer

  #define MOD_VENT  1       //anemometer Davis

  #define MOD_DS18B20 0     //sensor water T° 
  #define MOD_ADS_KIT0139 0 //sensor water level

  #define NB_SEN0600_PROBE 0 //nb soil probe SEN0600
  #if NB_SEN0600_PROBE
  const uint8_t SEN0600_ADDR_LIST[NB_SEN0600_PROBE] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}; //address list on bus (to complete and adapt)(ex: {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, ...} )
  #endif

  //## various param ##
  const uint16_t PROF_SONDE_WL = 1000;  //water level probe installation depth in mm

  #define RELAY_PIN_GNSS 3                // Relay Pin for TEL0157 module
  #define KIM2_POWER_PIN 4                // Power Pin for KIM2 module (non-editable)
  #define KIM2_RELAY_PIN 5                // Relay Pin for KIM2 module (same as SIM7600 relay pin)

  //== Automatic counter ==
  #define THP_CAPTEUR_COUNT (MOD_BME280 + MOD_SHT31 + MOD_SHT20)
  #if MOD_BME280 && MOD_SHT31
    #define THP_MERGE 1
  #else
    #define THP_MERGE 0
  #endif

#endif

  //_____________##### test config #####_________________
#ifdef DO_CONFIG_CHECKS
  #ifndef CONFIG_CHECKS_DONE
    #define CONFIG_CHECKS_DONE
    //== Check data transmisson config ==
    #if MOD_KIM2 && MOD_SIM7600
      #error "ERROR : KIM2 et SIM7600 both actives"
    #endif
    #if !MOD_KIM2 && !MOD_SIM7600
      #warning "WARNING : No data transmission selected"
      #if !MOD_GPS
        #warning "no source of time selected. GPS module should be installed !"
      #endif
    #endif
    #if MOD_KIM2
      #if (MOD_VEML7700 > 0) || (MOD_DS18B20 > 0) || (MOD_ADS_KIT0139 > 0) || (NB_SEN0600_PROBE > 0)
        #error "SORRY : some selected sensors are not yet supported with KIM2 transmission"
      #endif
      #if !MOD_GPS
        #warning "no source of time selected. GPS module should be installed !"
      #endif
    #endif

    //== Consistency check ==
    #if THP_CAPTEUR_COUNT == 0
      #warning "WARNING : No T°/Hum sensor defined ! cf config.h"
    #elif THP_CAPTEUR_COUNT > 1
      #if THP_MERGE
        #warning "WARNING : 2 T&H Sensors selected, T°/Hum/P° fusion! cf config.h"
      #else
        #error "ERROR : More than 1 T°/Hum sensor defined ! cf config.h"
      #endif
    #endif
  #endif
#endif
