//projet ECOLOGGING partie meteo
//UEFP Pierre BORDENAVE
#define progversion "20251204"

#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include "RTClib.h" //bibliotheque RTC pour DS3231

//#include "DFRobot_SHT20.h"//bibliotheque SEN0227

#include <math.h> //vent vitesse
#include "TimerOne.h"//DAVIS

#include "config.h" //fichier de configuration

//## buffer pile ##
#include "buffer_pile.h"
BUFFER_PILE mypile;


//## MQTT ##
unsigned long intervalPubTimer = 0;
#include "SIM7600MQTT.h"
#if MOD_SIM7600
  SIM7600MQTT sim7600mqtt;
#endif
char mypayload[256] = "\0";

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
  //Serial.println(F("#__ version " progversion "__"));

  #if MOD_PYRANO
    Capteurs.initPyrano();
  #endif
  #if MOD_VENT
    Capteurs.initVent1();  //before speed serial definition
  #endif
  
  Serial.begin(115200);   // Moniteur série

  Wire.begin();
  Wire.setClock(100000);

  //initialisation des capteurs & modules
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

  //initialisation des fichiers de sauvegardes (entête)
  entete_tab_mesures();
  entete_tab_moyennes();
  
  Serial.print(F("#__ version ")); Serial.print(progversion); Serial.println(F("__"));
}

//++++++++++++++++++++++++++++++Le loop boucle infinie +++++++++++++++++++++++++++++++++++
void loop() {
  DateTime now = RTC.now(); //lecture RTC DS3231
  int currentSecond = now.second();
  int currentMinute = now.minute();
  int currentHour = now.hour();
  int currentSecondHour = currentMinute * 60 + currentSecond;
  int currentMinuteDay = currentHour * 60 + currentMinute;

  //### MQTT ###
  #if MOD_SIM7600
    //get MQTT status code, place first before status modification with current loop
    if(sim7600mqtt.get_status() == 2){
      //mqtt msg send success => next item in buffer pile
      Serial.println(F("#_# next pile"));
      mypile.next_pile();

    }else if(sim7600mqtt.get_status() == 1 && ((millis() - intervalPubTimer) > 60000)){
      //delay to prevent too many frequent publication
      // publish state is possible if data available in buffer_pile and no other process occured
      intervalPubTimer = millis();
      Ecoset dataset;
      if(mypile.read_pile(dataset)){
        formatted_payload(dataset,mypayload);
          //Serial.print("#_# formatted payload:");Serial.println(mypayload);
        //publishMQTT
        if(sim7600mqtt.publishMQTT(mytopic, mypayload) == 0){
          Serial.println(F("#_# publishMQTT can not manage current dataset"));
        }
      }
    }

    //check buffer and hard_reset module
    if(mypile.alert_full_pile()){sim7600mqtt.hard_reset_SIM7600();}

    //lancement librairie SIM7600MQTT
    sim7600mqtt.lib_MQTT();
  
    //### gestion temps ###
    //try update RTC periodically
    if(millis() - DS3231RTC_update > DS3231RTC_update_interval){
      if(update_RTC()){DS3231RTC_update_interval = DS3231RTC_normal_update_interval;}
      DS3231RTC_update = millis();
    }
  #endif

  //### déclenchement acquistion ###
  for (uint8_t i = 0; i < NB_SECONDES_CIBLES; i++) {
    if (currentSecond == SECONDES_CIBLES[i] && currentSecondHour != dernierTriggerSeconde) {
      dernierTriggerSeconde = currentSecondHour;
    
      //acquisition et monitoring
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
      //ecriture sur la carte micro SD du fichier ECOLOGING acquisition toutes les 20secondes
      WriteToFileMeasure(now);

      break;
    }
  }  //fin acquisition 20sec

  //++++++++++++++++++++++++Pluviométrie+avec enregistrement 60min+++++++++++++++++++++++
  #if MOD_PLUIE
    Capteurs.acqPluvio();
  #endif
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  //### déclenchement calcul moyenne et enregistrement des données ###
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
      mydata.MHRay = Capteurs.meanRay();
      mydata.MHPDavis = Capteurs.meanPyrano();
      mydata.MHVit = Capteurs.meanVitesse();
      mydata.MHDir = Capteurs.meanDirection();
      mydata.MHPluie = Capteurs.cumulHRain();
      mydata.MHTempWater = Capteurs.meanTempWater();
      mydata.MHWaterHauteur = Capteurs.meanWaterHauteur();
      //mydata.MHWaterVolt = Capteurs.meanWaterVolt();
      //mydata.MHWaterColonne = Capteurs.meanWaterColonne();

      #if MOD_SIM7600
        mypile.add_pile(mydata);
      #endif

      Capteurs.resetSommes();

      //remise a zero cumul pluie a minuit
      #if MOD_PLUIE
        if(now.hour()== 0) {
          Capteurs.resetCumuls();
        }
      #endif

      break;
    }
  }

}//fermeture boucle du void loop infini


//--------- fonctions ecriture/envoi datas ------------


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
  Serial.println();
}

// format payload to send on MQTT topic. Data is supplied in a Ecoset structure
bool formatted_payload(Ecoset dataset, char* payload){
  //formatage du message
  const char template_payload[] = 
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
  ",\"MHVit\":%s,\"MHDir\":%s"
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
  ",\"UTC\":\"%s\",\"TZ\":%d}";

  char arg1[10] = "\0";char arg2[10] = "\0";char arg3[10] = "\0";char arg4[10] = "\0";char arg5[10] = "\0";
  char arg6[10] = "\0";char arg7[10] = "\0";char arg8[10] = "\0";char arg9[10] = "\0";char arg10[10] = "\0";
  
  char message[250] = "\0";
  char formatdatetime[25] = "\0";
  char utcformatdatetime[20] = "\0";

  //récupération des valeurs des arguments
  mypile.get_formatted_datetime_bufferpile(dataset, formatdatetime);
  dtostrf(dataset.MHTemp,1,2,arg1);           //Temp -15°C à +50°C  //4d
  dtostrf(dataset.MHHum,1,2,arg2);            //Hum Relative 0 à 100%  //5d
  dtostrf(dataset.MHRay,1,2,arg3);            //Luminosité 0 à 140000Lux //8d 
  dtostrf(dataset.MHPDavis,1,2,arg4);         //Irridiation  Watt/m²
  dtostrf(dataset.MHVit,1,2,arg5);            //Vent 0 à 200 ou 250km/h  //5d
  dtostrf(dataset.MHDir,1,2,arg6);            //Direction 0 à 360deg  //5d
  dtostrf(dataset.MHPluie,1,2,arg7);          //5d
  dtostrf(dataset.MHPatm,1,2,arg8);           //950 à 1100mbar//5d
  dtostrf(dataset.MHTempWater,1,2,arg9);      //temperature eau//4d
  dtostrf(dataset.MHWaterHauteur,1,2,arg10); //hauteur nappe

  mypile.get_formatted_UTC_bufferpile(dataset, utcformatdatetime);
  
  //generation du message
  int result = snprintf(message, sizeof(message), template_payload, formatdatetime
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
  , arg5, arg6
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
  , utcformatdatetime, dataset.UTCoffset);

  if(abs(result) >= sizeof(message) || result < 0){
    Serial.print(F("format payload failed error "));Serial.println(result);
    return false;
  }
  strncpy(payload, message, sizeof(message));
  return true;
}

//Fonction création entete.csv mesures
void entete_tab_mesures(){
  fichier20s = SD.open(DATA_FILENAME, FILE_WRITE);//ici limite au nombre de lettre pour ecrire nom fichier de plus si on veut un fichier txt il faut changer en .txt
  fichier20s.print(F("Date et heure"));fichier20s.print(";");
  #if THP_CAPTEUR_COUNT > 0
    fichier20s.print(F("HR_%"));fichier20s.print(";");
    fichier20s.print(F("T_DegC"));fichier20s.print(";");
  #endif
  #if MOD_VEML7700
    fichier20s.print(F("Ray_veml7700_lux"));fichier20s.print(";");
  #endif
  #if MOD_PYRANO
    fichier20s.print(F("Ray_davis6450_W/m2"));fichier20s.print(";");
  #endif
  #if MOD_VENT
    fichier20s.print(F("V_km/h"));fichier20s.print(";");
    fichier20s.print(F("Dir_deg"));fichier20s.print(";");
  #endif
  #if MOD_BME280
    fichier20s.print(F("Patm_mbar"));fichier20s.print(";");
  #endif
  #if MOD_ADS_KIT0139
    fichier20s.print(F("WLV mm"));fichier20s.print(";");
    fichier20s.print(F("WLC mm"));fichier20s.print(";");
    fichier20s.print(F("WL mm"));fichier20s.print(";");
  #endif
  #if MOD_DS18B20
    fichier20s.print(F("T_W °C"));fichier20s.print(";");
  #endif
  fichier20s.println("");
  fichier20s.close();//fermeture fichier
}

//Fonction création entete.csv moyenne
void entete_tab_moyennes(){
  MoyH = SD.open(MOY_FILENAME, FILE_WRITE);//ici limite au nombre de lettre pour ecrire nom fichier de plus si on veut un fichier txt il faut changer en .txt
  MoyH.print(F("Date et heure"));MoyH.print(";");
  #if MOD_PLUIE
    MoyH.print(F("CumulPluie en mm"));MoyH.print(";");
  #endif
  #if THP_CAPTEUR_COUNT > 0
    MoyH.print(F("HR_AVG en %"));MoyH.print(";");
    MoyH.print(F("T_AVG en DegC"));MoyH.print(";");
  #endif
  #if MOD_VEML7700
    MoyH.print(F("RAY_VEML770_AVG en Lux"));MoyH.print(";");
  #endif
  #if MOD_PYRANO
    MoyH.print(F("RAY_DAVIS6450_AVG en W/m2"));MoyH.print(";");
  #endif
  #if MOD_VENT
    MoyH.print(F("V_AVG en km/h"));MoyH.print(";");
    MoyH.print(F("DIR_AVG en Deg"));MoyH.print(";");
  #endif
  #if MOD_PLUIE
    MoyH.print(F("CumulPluieday en mm"));MoyH.print(";");
  #endif
  #if MOD_BME280
    MoyH.print(F("Patm en mbar"));MoyH.print(";");
  #endif
  #if MOD_ADS_KIT0139
    MoyH.print(F("WVolt_AVG en mm"));MoyH.print(";");
    MoyH.print(F("WCol_AVG en mm"));MoyH.print(";");
    MoyH.print(F("WLevel_AVG en mm"));MoyH.print(";");
  #endif
  #if MOD_DS18B20
    MoyH.print(F("T_AVG_W en DegC"));MoyH.print(";");
  #endif
  MoyH.println("");
  MoyH.close();//fermeture fichier
}

void WriteToFileMeasure(DateTime now){
  //ecriture sur la carte micro SD du fichier ECOLOGING acquisition toutes les 20secondes
  File fichier20s = SD.open(DATA_FILENAME,FILE_WRITE);
  /// ecriture des donnees dans la carte SD
  char DT_template[] = "DD/MM/YYYY hh:mm:ss ; ";
  now.toString(DT_template);
  fichier20s.print(DT_template);
  #if THP_CAPTEUR_COUNT > 0
    fichier20s.print(Capteurs.valHumid(), 1); fichier20s.print(";");
    fichier20s.print(Capteurs.valTemp(), 1); fichier20s.print(";");
  #endif
  #if MOD_VEML7700
    fichier20s.print(Capteurs.valRay(), 1); fichier20s.print(";");
  #endif
  #if MOD_PYRANO
    fichier20s.print(Capteurs.valPyrano(), 1); fichier20s.print(";");
  #endif
  #if MOD_VENT
    fichier20s.print(Capteurs.valVitesse()); fichier20s.print(";");
    fichier20s.print(Capteurs.valDirection()); fichier20s.print(";");
  #endif
  #if MOD_BME280
    fichier20s.print(Capteurs.valPatm()); fichier20s.print(";");
  #endif
  #if MOD_ADS_KIT0139
    fichier20s.print(Capteurs.valWaterVolt());fichier20s.print(";");
    fichier20s.print(Capteurs.valWaterColonne());fichier20s.print(";");
    fichier20s.print(Capteurs.valWaterHauteur());fichier20s.print(";");
  #endif
  #if MOD_DS18B20
    fichier20s.print(Capteurs.valTempWater());fichier20s.print(";");
  #endif
  fichier20s.println("");
  fichier20s.close();//fermeture fichier
  //fin enregistrement
}

void WriteToFileMoyenne(DateTime now){
  File MoyH = SD.open(MOY_FILENAME,FILE_WRITE);/// ecriture des donnees dans la carte SD
  char DT_template[] = "DD/MM/YYYY hh:mm:ss";
  now.toString(DT_template);
  MoyH.print(DT_template); MoyH.print(" ; ");
  #if MOD_PLUIE
    MoyH.print(Capteurs.cumulHRain(),2); MoyH.print(" ; ");
  #endif
  #if THP_CAPTEUR_COUNT > 0
    MoyH.print(Capteurs.meanHumid(), 1); MoyH.print(";");
    MoyH.print(Capteurs.meanTemp(), 1); MoyH.print(";");
  #endif
  #if MOD_VEML7700
    MoyH.print(Capteurs.meanRay(), 1); MoyH.print(";");
  #endif
  #if MOD_PYRANO
    MoyH.print(Capteurs.meanPyrano(), 1); MoyH.print(";");
  #endif
  #if MOD_VENT
    MoyH.print(Capteurs.meanVitesse(), 1); MoyH.print(";");
    MoyH.print(Capteurs.meanDirection(), 1); MoyH.print(" ; ");
  #endif
  #if MOD_PLUIE
    MoyH.print(Capteurs.cumulDRain(),2); MoyH.print(";");
  #endif
  #if MOD_BME280
    MoyH.print(Capteurs.meanPatm()); MoyH.print(";");
  #endif
  #if MOD_ADS_KIT0139
    MoyH.print(Capteurs.meanWaterVolt());MoyH.print(";");
    MoyH.print(Capteurs.meanWaterColonne());MoyH.print(";");
    MoyH.print(Capteurs.meanWaterHauteur());MoyH.print(";");
  #endif
  #if MOD_DS18B20
    MoyH.print(Capteurs.meanTempWater());MoyH.print(";");
  #endif
  MoyH.println("");
  MoyH.close();//fermeture fichier
}
