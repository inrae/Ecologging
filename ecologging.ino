//ECOLOGGING project
//
// This program is written for ecologging stations.
// No waranty is given
//
//philippe.chaumeil@inrae.fr _ Univ. Bordeaux, INRAE, BIOGECO, F-33610, Cestas, France
//pierre.bordenave@inrae.fr _INRAE, UEFP, 33610 Cestas, France
#define progversion "20260703"
#define DO_CONFIG_CHECKS

#include <SPI.h>
#include <SD.h>
#include <Wire.h>   //library required for DS18B20
#include "RTClib.h" // RTC library for DS3231

//#include "DFRobot_SHT20.h"//library for SEN0227

#include <math.h>      //required for wind speed
#include "TimerOne.h"  //required for DAVIS sensor

#include "config.h" //configuration file

//## buffer pile ##
#include "buffer_pile.h"
BUFFER_PILE mypile;

unsigned long intervalPubTimer = 0;

//## SAT ##
#include "sat_payload.h"  // encode file
#include "KIM2.h"  //
#if MOD_KIM2
  KIM2 kim2(Serial1, 4, 5);
  char hexString[33];
  const unsigned long kim2PubInterval = 120000;
#endif

//## MQTT ##
#include "SIM7600MQTT.h"
#if MOD_SIM7600
  SIM7600MQTT sim7600mqtt;
#endif
char mypayload[512] = {0};

#include "ecologging.h"

#include "capteurs_meteo.h"
CAPTEURS_METEO Capteurs(PROF_SONDE_WL);

//## main ##
const uint8_t NB_SECONDES_CIBLES = sizeof(SECONDES_CIBLES) / sizeof(SECONDES_CIBLES[0]);
const uint8_t NB_MINUTES_CIBLES = sizeof(MINUTES_CIBLES) / sizeof(MINUTES_CIBLES[0]);
int dernierTriggerSeconde = -1;
int dernierTriggerMinute = -1;

//++++++++++++++++++++++++++++++Setup+++++++++++++++++++++++++++++++++++
void setup() {
  while (!Serial) { delay(10); }

  #if MOD_PYRANO
    Capteurs.initPyrano();
  #endif
  #if MOD_VENT
    Capteurs.initVent1();  //before speed serial definition
  #endif
  
  Serial.begin(115200);    // Serial Monitor
  
  #if MOD_KIM2
    Serial1.begin(9600);     // Serial for KIM2
    kim2.initKim2("3d678af16b5a572078f3dbc95a1104e7");		//rconf key for CLS/Kineis module <min freq>,<max freq>,<modulation>,<rf level>
    kim2.powerOn();
    Serial.println("KIM2 READY");
  #endif
  
  Wire.begin();
  Wire.setClock(100000);

  //initialization of sensors and modules
  initRTC();

  #if MOD_BME280
    Capteurs.initBME280();
  #endif
  #if MOD_SHT31
    Capteurs.initSHT31();
  #elif MOD_SHT20
    Capteurs.initSHT20();
  #endif

  initmicroSD();

  #if MOD_PLUIE
    Capteurs.initPluvio();
  #endif

  #if MOD_VEML7700
    Capteurs.initVEML7700();
  #endif
  
  #if MOD_VENT
    Capteurs.initVent2();
  #endif

  #if MOD_SIM7600
    sim7600mqtt.initSIM7600();
  #endif

  #if MOD_DS18B20
    Capteurs.initDS18B20();
  #endif

  #if MOD_ADS_KIT0139
    Capteurs.initADS();
  #endif

  #if NB_SEN0600_PROBE > 0
    Serial3.begin(9600);
    Capteurs.initSEN0600(&Serial3, NB_SEN0600_PROBE, SEN0600_ADDR_LIST);
  #endif

  //initialization of backup files (header)
  entete_tab_mesures();
  entete_tab_moyennes();
  
  Serial.print(F("#__ version ")); Serial.print(progversion); Serial.println(F("__"));
}

//++++++++++++++++++++++++++++++ Main infinite loop +++++++++++++++++++++++++++++++++++
void loop() {
  DateTime now = RTC.now();  //read RTC DS3231
  int currentSecond = now.second();
  int currentMinute = now.minute();
  int currentHour = now.hour();
  int currentSecondHour = currentMinute * 60 + currentSecond;
  int currentMinuteDay = currentHour * 60 + currentMinute;

  //### SAT_KIM2 ###
  #if MOD_KIM2
    kim2.update();
    if((millis() - intervalPubTimer) > kim2PubInterval){
      Ecoset dataset;
      intervalPubTimer = millis();
      if(mypile.read_pile(dataset)){
        get_binary_payload(dataset, hexString);
        Serial.print("SAT PAYLOAD = "); Serial.println(hexString);
        kim2.sendPayload(hexString);
      }
    }
  #endif

  //### MQTT ###
  #if MOD_SIM7600
    //get MQTT status code, place first before status modification with current loop
    if(sim7600mqtt.get_status() == 2){
      //mqtt msg send success => next item in buffer pile
      Serial.println(F("#_# next pile"));
      mypile.next_pile();

    }else if(sim7600mqtt.get_status() == 1 && ((millis() - intervalPubTimer) > 60000)){
      // delay to prevent too many frequent publication
      // publish state is possible if data available in buffer_pile and no other process occured
      intervalPubTimer = millis();
      Ecoset dataset;
      if(mypile.read_pile(dataset)){
        json_payload(dataset,mypayload, sizeof(mypayload));
          //Serial.print("#_# formatted payload:");Serial.println(mypayload);
        //publishMQTT
        if(sim7600mqtt.publishMQTT(MY_TOPIC, mypayload) == 0){
          Serial.println(F("#_# publishMQTT can not manage current dataset"));
        }
      }
    }

    //check buffer and hard_reset module
    if(mypile.alert_full_pile()){sim7600mqtt.hard_reset_SIM7600();}

    //launch SIM7600MQTT library
    sim7600mqtt.lib_MQTT();
  
    //### Time management ###
    //try update RTC periodically
    if(millis() - DS3231RTC_update > DS3231RTC_update_interval){
      if(update_RTC()){DS3231RTC_update_interval = DS3231RTC_normal_update_interval;}
      DS3231RTC_update = millis();
    }
  #endif

  //### start acquistion ###
  #if NB_SEN0600_PROBE > 0
  Capteurs.updateSEN0600acq();
  #endif

  for (uint8_t i = 0; i < NB_SECONDES_CIBLES; i++) {
    if (currentSecond == SECONDES_CIBLES[i] && currentSecondHour != dernierTriggerSeconde) {
      dernierTriggerSeconde = currentSecondHour;
    
      //acquisition & monitoring
      printFormatedDateTime(now);
      #if MOD_BME280
        #if THP_MERGE
          Capteurs.acqBME280(1);
        #else
          Capteurs.acqBME280();
        #endif
      #endif
      #if MOD_SHT31
        Capteurs.acqSHT31();
      #elif MOD_SHT20
        Capteur.acqSHT20();
      #endif
      #if MOD_VEML7700
        Capteurs.acqVEML7700();
      #endif
      #if MOD_PYRANO
        Capteurs.acqPyrano();
      #endif
      #if MOD_VENT
        Capteurs.acqVent();
      #endif
      #if MOD_DS18B20
        Capteurs.acqDS18B20Teau();
      #endif
      #if MOD_ADS_KIT0139
        Capteurs.acqADS_kit0139();
      #endif
      #if NB_SEN0600_PROBE > 0
        Capteurs.acqSEN0600launch();
      #endif
      //Writing the ECOLOGING file to the micro SD card every XX seconds
      WriteToFileMeasure(now);

      break;
    }
  }  //end of the acquisition section

  //++++++++++++++++++++++++ Rainfall and mean recording +++++++++++++++++++++++
  #if MOD_PLUIE
    Capteurs.acqPluvio();
  #endif
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  //### triggering average calculation and data recording ###
  for (uint8_t i = 0; i < NB_MINUTES_CIBLES; i++) {
    if (currentMinute == MINUTES_CIBLES[i] && currentMinuteDay != dernierTriggerMinute) {
      dernierTriggerMinute = currentMinuteDay;

      #if MOD_PLUIE
        Capteurs.setHcumulPluvio();
      #endif
      printFormatedDateTime(now);
      MoyenneToSerial();

      WriteToFileMoyenne(now);

      //## Add datas to buffer_pile datasets (Ecoset definition in buffer_pile lib)##
      Ecoset mydata;
      mydata.day = now.day();
      mydata.month = now.month();
      mydata.year = now.year();
      mydata.hour = now.hour();
      mydata.minute = now.minute();
      mydata.second = now.second();
      #if MOD_SIM7600
        mydata.UTCoffset = sim7600mqtt.get_utc_offset();
      #endif
      mydata.MHTemp = Capteurs.meanTemp();
      mydata.MHHum = Capteurs.meanHumid();
      mydata.MHPatm = Capteurs.meanPatm();
      #if MOD_VEML7700
        mydata.MHRay = Capteurs.meanRay();
      #endif
      #if MOD_PYRANO
        mydata.MHPDavis = Capteurs.meanPyrano();
      #endif
      #if MOD_VENT
        mydata.MHVit = Capteurs.meanVitesse();
        mydata.MHDir = Capteurs.meanDirection();
        mydata.MHVmx = Capteurs.valMaxVitesse();
      #endif
      #if MOD_PLUIE
        mydata.MHPluie = Capteurs.cumulHRain();
      #endif
      #if MOD_DS18B20
        mydata.MHTempWater = Capteurs.meanTempWater();
      #endif
      #if MOD_ADS_KIT0139
        mydata.MHWaterHauteur = Capteurs.meanWaterHauteur();
        //mydata.MHWaterVolt = Capteurs.meanWaterVolt();
        //mydata.MHWaterColonne = Capteurs.meanWaterColonne();
      #endif
      #if NB_SEN0600_PROBE > 0
        for (int i = 0; i < NB_SEN0600_PROBE; i++) {
          mydata.MHTempSEN0600[i] = Capteurs.meanTempSEN0600(i);
          mydata.MHHumSEN0600[i] = Capteurs.meanHumidSEN0600(i);
        }
      #endif

      #if MOD_KIM2
        mypile.next_pile();
        mypile.add_pile(mydata);
      #endif

      #if MOD_SIM7600
        mypile.add_pile(mydata);
      #endif

      Capteurs.resetSommes();

      //reset daily rainfall total at midnight
      #if MOD_PLUIE
        if(now.hour()== 0) {
          Capteurs.resetCumuls();
        }
      #endif

      break;
    }
  }

} //end main infinite loop

//--------- Payload --------

// format json payload to send on MQTT topic. Data is supplied in a Ecoset structure
//note: ArduinoJson library could be used but more RAM needed
bool json_payload(Ecoset dataset, char* payload, size_t payload_size){
  //Format payload
  const char template_debut[] = 
  "{\"TS\":\"%s\""
  #if THP_CAPTEUR_COUNT > 0
  ",\"MHTemp\":%s,\"MHHum\":%s"
  #endif
  #if MOD_VEML7700
  ",\"MHRay\":%s"
  #endif
  #if MOD_PYRANO
  ",\"MHPDavis\":%s"
  #endif
  #if MOD_VENT
  ",\"MHVit\":%s,\"MHDir\":%s,\"MHVmx\":%s"
  #endif
  #if MOD_PLUIE
  ",\"MHPluie\":%s"
  #endif
  #if MOD_BME280
  ",\"MHPatm\":%s"
  #endif
  #if MOD_DS18B20
  ",\"MHTeau\":%s"
  #endif
  #if MOD_ADS_KIT0139
  ",\"MHWL\":%s"
  #endif
  ;

  const char template_fin[] = ",\"UTC\":\"%s\",\"TZ\":%d,\"AVGI\":%d}";
 
  char formatdatetime[25] = {0};
  char utcformatdatetime[20] = {0};

  //get arguments values
  #if THP_CAPTEUR_COUNT > 0
    char arg1[16] = {0};
    char arg2[16] = {0};
    dtostrf(dataset.MHTemp,1,2,arg1);           //Temp -15°C à +50°C  //4d
    dtostrf(dataset.MHHum,1,2,arg2);            //Relative Humidity 0 à 100%  //5d
  #endif
  #if MOD_VEML7700
    char arg3[10] = {0};
    dtostrf(dataset.MHRay,1,2,arg3);            //Luminosity 0 à 140000Lux //8d 
  #endif
  #if MOD_PYRANO
    char arg4[10] = {0};
    dtostrf(dataset.MHPDavis,1,2,arg4);         //Irridiation  Watt/m²
  #endif
  #if MOD_VENT
    char arg5[10] = {0};
    char arg6[10] = {0};
    char arg11[10] = {0};
    dtostrf(dataset.MHVit,1,2,arg5);            //Wind 0 à 200 ou 250km/h  //5d
    dtostrf(dataset.MHDir,1,2,arg6);            //Direction 0 à 360deg  //5d
    dtostrf(dataset.MHVmx,1,2,arg11);           //Wind 0 à 200 ou 250km/h  //5d
  #endif
  #if MOD_PLUIE
    char arg7[10] = {0};
    dtostrf(dataset.MHPluie,1,2,arg7);          //5d
  #endif
  #if MOD_BME280
    char arg8[10] = {0};
    dtostrf(dataset.MHPatm,1,2,arg8);           //950 à 1100mbar//5d
  #endif
  #if MOD_DS18B20
    char arg9[10] = {0};
    dtostrf(dataset.MHTempWater,1,2,arg9);      //temperature eau//4d
  #endif
  #if MOD_ADS_KIT0139
    char arg10[10] = {0};
    dtostrf(dataset.MHWaterHauteur,1,2,arg10); //water level
  #endif

  //get time and date
  mypile.get_formatted_datetime_bufferpile(dataset, formatdatetime);
  mypile.get_formatted_UTC_bufferpile(dataset, utcformatdatetime);
  
  //cursors
  size_t c_offset = 0; //offset fo cursor
  int result = 0;

  //message generation first part
  result = snprintf(payload + c_offset, payload_size - c_offset, template_debut, formatdatetime
  #if THP_CAPTEUR_COUNT > 0
  , arg1, arg2
  #endif
  #if MOD_VEML7700
  , arg3
  #endif
  #if MOD_PYRANO
  , arg4
  #endif
  #if MOD_VENT
  , arg5, arg6, arg11
  #endif
  #if MOD_PLUIE
  , arg7
  #endif
  #if MOD_BME280
  , arg8
  #endif
  #if MOD_DS18B20
  , arg9
  #endif
  #if MOD_ADS_KIT0139
  , arg10
  #endif
  );

  //check snprintf errors
  if (result < 0 || (size_t)result >= payload_size) {
    Serial.print(F("Error: Payload format failed or truncated. Code: "));Serial.println(result);
    if (payload_size > 0) payload[0] = '\0';
    return false;
  }
  c_offset += result;

  //message generation SEN0600 probes
  #if NB_SEN0600_PROBE > 0
  for (int i = 0; i < NB_SEN0600_PROBE; i++) {
    char str_vwc[16] = {0};
    char str_tsol[16] = {0};

    dtostrf(dataset.MHHumSEN0600[i], 1, 2, str_vwc);
    dtostrf(dataset.MHTempSEN0600[i], 1, 2, str_tsol);

    result = snprintf(payload + c_offset, payload_size - c_offset, ",\"MVWC%d\":%s,\"MTsol%d\":%s", i + 1, str_vwc, i + 1, str_tsol);

    //check snprintf errors
    if (result < 0 || (size_t)result >= payload_size) {
      Serial.print(F("Error: Payload format failed or truncated. Code: "));Serial.println(result);
      if (payload_size > 0) payload[0] = '\0';
      return false;
    }
    c_offset += result;
  }
  #endif

  //Message generation last part 
  result = snprintf(payload + c_offset, payload_size - c_offset, template_fin, utcformatdatetime, dataset.UTCoffset, AVG_INTEGRATION);

  //check snprintf errors
  if (result < 0 || (size_t)result >= payload_size) {
    Serial.print(F("Error: Payload format failed or truncated. Code: "));Serial.println(result);
    if (payload_size > 0) payload[0] = '\0';
    return false;
  }

  return true;
}

//--------- debug/log data functions ------------

// print Mean values to serial
void MoyenneToSerial(){
  #if MOD_PLUIE
    Serial.print(Capteurs.cumulHRain(),2);Serial.print(F(" cumul en mm\t"));// Pluie en mm
  #endif
  #if THP_CAPTEUR_COUNT > 0
    Serial.print(Capteurs.meanHumid(), 2);Serial.print(F(" MoyHR %\t"));// HR en %
    Serial.print(Capteurs.meanTemp(), 2);Serial.print(F(" MoyT °C\t"));// T en °C
  #endif
  #if MOD_VEML7700
    Serial.print(Capteurs.meanRay(), 2);Serial.print(F(" MoyL Lux\t"));// L en Lux
  #endif
  #if MOD_PYRANO
    Serial.print(Capteurs.meanPyrano(), 2);Serial.print(F(" MoyL Watt_m2\t"));//
  #endif
  #if MOD_VENT
    Serial.print(Capteurs.meanVitesse(), 2);Serial.print(F(" MoyV Kmh\t"));// V en km/h
    Serial.print(Capteurs.meanDirection(), 2);Serial.print(F(" MoyD °\t"));// D en °
    Serial.print(Capteurs.valMaxVitesse(), 2);Serial.print(F(" Vmax Kmh\t"));// V en km/h
  #endif
  #if MOD_BME280
    Serial.print(Capteurs.meanPatm(), 2);Serial.print(F(" MoyP mbar\t"));
  #endif
  #if MOD_DS18B20
    Serial.print(Capteurs.meanTempWater(), 2);Serial.print(F(" MoyTW °C\t"));
  #endif
  #if MOD_ADS_KIT0139
    Serial.print(Capteurs.meanWaterColonne(), 2);Serial.print(F(" MoyHC mm\t"));
    Serial.print(Capteurs.meanWaterHauteur(), 2);Serial.print(F(" MoyHW mm\t"));
  #endif
  #if NB_SEN0600_PROBE > 0
    for (int i = 0; i < NB_SEN0600_PROBE; i++) {
      Serial.print(Capteurs.meanTempSEN0600(i), 2);Serial.print(F(" ["));Serial.print(i);Serial.print(F("] MoyTss °C\t"));
      Serial.print(Capteurs.meanHumidSEN0600(i), 2);Serial.print(F(" ["));Serial.print(i);Serial.print(F("] MoyHRss %\t"));
    }
  #endif
  Serial.println();
}

//--------- write/send to file data functions ------------

//Function to create header.csv (measurements)
void entete_tab_mesures(){
  fichier20s = SD.open(DATA_FILENAME, FILE_WRITE); //warning limitation in filename length.
  fichier20s.print(F("Date et heure"));fichier20s.print(F(";"));
  #if THP_CAPTEUR_COUNT > 0
    fichier20s.print(F("HR_%"));fichier20s.print(F(";"));
    fichier20s.print(F("T_DegC"));fichier20s.print(F(";"));
  #endif
  #if MOD_VEML7700
    fichier20s.print(F("Ray_veml7700_lux"));fichier20s.print(F(";"));
  #endif
  #if MOD_PYRANO
    fichier20s.print(F("Ray_davis6450_W/m2"));fichier20s.print(F(";"));
  #endif
  #if MOD_VENT
    fichier20s.print(F("V_km/h"));fichier20s.print(F(";"));
    fichier20s.print(F("Dir_deg"));fichier20s.print(F(";"));
  #endif
  #if MOD_BME280
    fichier20s.print(F("Patm_mbar"));fichier20s.print(F(";"));
  #endif
  #if MOD_ADS_KIT0139
    fichier20s.print(F("WLV mm"));fichier20s.print(F(";"));
    fichier20s.print(F("WLC mm"));fichier20s.print(F(";"));
    fichier20s.print(F("WL mm"));fichier20s.print(F(";"));
  #endif
  #if MOD_DS18B20
    fichier20s.print(F("T_W °C"));fichier20s.print(F(";"));
  #endif
  #if NB_SEN0600_PROBE > 0
    for (int i = 0; i < NB_SEN0600_PROBE; i++) {
      fichier20s.print(F("MTsol"));fichier20s.print(i+1);fichier20s.print(F(";"));
      fichier20s.print(F("MVWC"));fichier20s.print(i+1);fichier20s.print(F(";"));
    }
  #endif
  fichier20s.println("");
  fichier20s.close();
}

//Function to create entete.csv (averages)
void entete_tab_moyennes(){
  MoyH = SD.open(MOY_FILENAME, FILE_WRITE); //warning limitation in filename length.
  MoyH.print(F("Date et heure"));MoyH.print(F(";"));
  #if MOD_PLUIE
    MoyH.print(F("CumulPluie en mm"));MoyH.print(F(";"));
  #endif
  #if THP_CAPTEUR_COUNT > 0
    MoyH.print(F("HR_AVG en %"));MoyH.print(F(";"));
    MoyH.print(F("T_AVG en DegC"));MoyH.print(F(";"));
  #endif
  #if MOD_VEML7700
    MoyH.print(F("RAY_VEML770_AVG en Lux"));MoyH.print(F(";"));
  #endif
  #if MOD_PYRANO
    MoyH.print(F("RAY_DAVIS6450_AVG en W/m2"));MoyH.print(F(";"));
  #endif
  #if MOD_VENT
    MoyH.print(F("V_AVG en km/h"));MoyH.print(F(";"));
    MoyH.print(F("DIR_AVG en Deg"));MoyH.print(F(";"));
    MoyH.print(F("V_MAX en km/h"));MoyH.print(F(";"));
  #endif
  #if MOD_PLUIE
    MoyH.print(F("CumulPluieday en mm"));MoyH.print(F(";"));
  #endif
  #if MOD_BME280
    MoyH.print(F("Patm en mbar"));MoyH.print(F(";"));
  #endif
  #if MOD_ADS_KIT0139
    MoyH.print(F("WVolt_AVG en mm"));MoyH.print(F(";"));
    MoyH.print(F("WCol_AVG en mm"));MoyH.print(F(";"));
    MoyH.print(F("WLevel_AVG en mm"));MoyH.print(F(";"));
  #endif
  #if MOD_DS18B20
    MoyH.print(F("T_AVG_W en DegC"));MoyH.print(F(";"));
  #endif
  #if NB_SEN0600_PROBE > 0
    for (int i = 0; i < NB_SEN0600_PROBE; i++) {
      MoyH.print(F("MTsol_AVG"));MoyH.print(i+1);MoyH.print(F(";"));
      MoyH.print(F("MVWC_AVG"));MoyH.print(i+1);MoyH.print(F(";"));
    }
  #endif
  MoyH.println("");
  MoyH.close();
}

//Writing the ECOLOGING measurements file to the micro SD card every XX seconds
void WriteToFileMeasure(DateTime now){
  File fichier20s = SD.open(DATA_FILENAME,FILE_WRITE);
  char DT_template[] = "DD/MM/YYYY hh:mm:ss ; ";
  now.toString(DT_template);
  fichier20s.print(DT_template);
  #if THP_CAPTEUR_COUNT > 0
    fichier20s.print(Capteurs.valHumid(), 1); fichier20s.print(F(";"));
    fichier20s.print(Capteurs.valTemp(), 1); fichier20s.print(F(";"));
  #endif
  #if MOD_VEML7700
    fichier20s.print(Capteurs.valRay(), 1); fichier20s.print(F(";"));
  #endif
  #if MOD_PYRANO
    fichier20s.print(Capteurs.valPyrano(), 1); fichier20s.print(F(";"));
  #endif
  #if MOD_VENT
    fichier20s.print(Capteurs.valVitesse()); fichier20s.print(F(";"));
    fichier20s.print(Capteurs.valDirection()); fichier20s.print(F(";"));
  #endif
  #if MOD_BME280
    fichier20s.print(Capteurs.valPatm()); fichier20s.print(F(";"));
  #endif
  #if MOD_ADS_KIT0139
    fichier20s.print(Capteurs.valWaterVolt());fichier20s.print(F(";"));
    fichier20s.print(Capteurs.valWaterColonne());fichier20s.print(F(";"));
    fichier20s.print(Capteurs.valWaterHauteur());fichier20s.print(F(";"));
  #endif
  #if MOD_DS18B20
    fichier20s.print(Capteurs.valTempWater());fichier20s.print(F(";"));
  #endif
  #if NB_SEN0600_PROBE > 0
    for (int i = 0; i < NB_SEN0600_PROBE; i++) {
      fichier20s.print(Capteurs.meanTempSEN0600(i), 1);fichier20s.print(F(";"));
      fichier20s.print(Capteurs.meanHumidSEN0600(i), 1);fichier20s.print(F(";"));
    }
  #endif
  fichier20s.println("");
  fichier20s.close();
}

//Writing the ECOLOGING averages file to the micro SD card
void WriteToFileMoyenne(DateTime now){
  File MoyH = SD.open(MOY_FILENAME,FILE_WRITE);
  char DT_template[] = "DD/MM/YYYY hh:mm:ss";
  now.toString(DT_template);
  MoyH.print(DT_template); MoyH.print(" ; ");
  #if MOD_PLUIE
    MoyH.print(Capteurs.cumulHRain(),2); MoyH.print(" ; ");
  #endif
  #if THP_CAPTEUR_COUNT > 0
    MoyH.print(Capteurs.meanHumid(), 1); MoyH.print(F(";"));
    MoyH.print(Capteurs.meanTemp(), 1); MoyH.print(F(";"));
  #endif
  #if MOD_VEML7700
    MoyH.print(Capteurs.meanRay(), 1); MoyH.print(F(";"));
  #endif
  #if MOD_PYRANO
    MoyH.print(Capteurs.meanPyrano(), 1); MoyH.print(F(";"));
  #endif
  #if MOD_VENT
    MoyH.print(Capteurs.meanVitesse(), 1); MoyH.print(F(";"));
    MoyH.print(Capteurs.meanDirection(), 1); MoyH.print(" ; ");
    MoyH.print(Capteurs.valMaxVitesse(), 1); MoyH.print(F(";"));
  #endif
  #if MOD_PLUIE
    MoyH.print(Capteurs.cumulDRain(),2); MoyH.print(F(";"));
  #endif
  #if MOD_BME280
    MoyH.print(Capteurs.meanPatm()); MoyH.print(F(";"));
  #endif
  #if MOD_ADS_KIT0139
    MoyH.print(Capteurs.meanWaterVolt());MoyH.print(F(";"));
    MoyH.print(Capteurs.meanWaterColonne());MoyH.print(F(";"));
    MoyH.print(Capteurs.meanWaterHauteur());MoyH.print(F(";"));
  #endif
  #if MOD_DS18B20
    MoyH.print(Capteurs.meanTempWater());MoyH.print(F(";"));
  #endif
  #if NB_SEN0600_PROBE > 0
    for (int i = 0; i < NB_SEN0600_PROBE; i++) {
      MoyH.print(Capteurs.meanTempSEN0600(i), 1);MoyH.print(F(";"));
      MoyH.print(Capteurs.meanHumidSEN0600(i), 1);MoyH.print(F(";"));
    }
  #endif
  MoyH.println("");
  MoyH.close();
}
