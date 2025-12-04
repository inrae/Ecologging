#include "config.h" //fichier de configuration


//+++++++++++++++++++++++++++++++++enregistrement sur carte+++++++++++++++++++++++++++
Sd2Card card;//creation SD
File fichier20s; //creation SD
File MoyH;
const int chipSelect = 10;//creation SD avt 4
#define sdCardPinChipSelect   53 //SD pour MEGA  53 correspond au PIN53

//+++++++++++++++++++++++++++++++++RTC+++++++++++++++++++++++++++    
RTC_DS3231 RTC;//RTC DS3231

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};//RTC DS3231
char mydatetime[20] = "\0";
#define DS3231RTC_normal_update_interval 960000  //16min
#define DS3231RTC_short_update_interval 60000 //1min
unsigned long DS3231RTC_update_interval = DS3231RTC_short_update_interval; //start with short interval
unsigned long DS3231RTC_update = 0;

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++ Fonctions écriture fichier ++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//Fonction initialisation pour enregistrement microSD
  //ATTENTION necessite l'appel à Wire.begin(); dans le setup du programme
void initmicroSD(){
   if (!SD.begin(sdCardPinChipSelect)) { //SD pour MEGA 
    Serial.println(F("No microSD found!"));
    while (1);
  }
  pinMode(53, OUTPUT);//SD pour MEGA
}

//++++++++++++++++++++++++++++++++++++++++
//++++++++  Fonctions horaire ++++++++++++
//++++++++++++++++++++++++++++++++++++++++

//Fonction initialisation et lowpower RTC
void initRTC(){

  if (! RTC.begin()) { //RTC DS3231
    Serial.flush();
    while (1); delay(10);
  }
  // following line sets the RTC to the date & time this sketch was compiled
  if (RTC.lostPower()) {///fonction ajustement du temps si il y a la perte de l'heure recalage
    RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

//mise à jour de l'heure
bool update_RTC(){
  bool debug = true;
  #if MOD_SIM7600
    DateTime* currentDateTime = sim7600mqtt.get_gsm_datetime();
  #else
    DateTime* currentDateTime = nullptr;
  #endif
  if (currentDateTime == nullptr) {
    Serial.println(F("#__ no gsm DateTime available __"));
  } else {
    if(debug){
      Serial.print(F("__ stored gsm DateTime : "));
      Serial.print(currentDateTime->year());Serial.print('/');
      Serial.print(currentDateTime->month());Serial.print('/');
      Serial.print(currentDateTime->day());Serial.print(' ');
      Serial.print(currentDateTime->hour());Serial.print(':');
      Serial.print(currentDateTime->minute());Serial.print(':');
      Serial.print(currentDateTime->second());Serial.println();
    }
    RTC.adjust(*currentDateTime);
    Serial.println(F("#__ RTC DateTime adjusted with gsm datas __"));
    return true;
  }
  return false;
}

//formatage du datetime
void get_formatted_datetime(const DateTime& now, char* datetime, size_t size){
  snprintf(datetime, size, "%02d/%02d/%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
}

//Impression du DateTime formaté sur le port série
void printFormatedDateTime(const DateTime& now){
  char buffer[25] = "\0";
  snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d %02d:%02d:%02d ", now.day(), now.month(), now.year(), now.hour(), now.minute(), now.second());
  Serial.print(buffer);
}
