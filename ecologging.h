#include "config.h" //configuration file


//++++++++++++++++++++ card recording +++++++++++++++++++++++++++
Sd2Card card; //instantiates SD
File fichier20s; //instantiates file
File MoyH;
const int chipSelect = 10;  //pinout before 4
#define sdCardPinChipSelect   53 //SD pinout MEGA  53 => PIN53

//++++++++++++++++++++++++++++++++ RTC ++++++++++++++++++++++++++    
RTC_DS3231 RTC;//RTC DS3231

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};//RTC DS3231
char mydatetime[20] = "\0";
#define DS3231RTC_normal_update_interval 960000  //16min
#define DS3231RTC_short_update_interval 60000 //1min
unsigned long DS3231RTC_update_interval = DS3231RTC_short_update_interval; //start with short interval
unsigned long DS3231RTC_update = 0;

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++ File writing functions ++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//Initialization function for microSD recording
  //WARNING: This requires a call to Wire.begin(); in the program's setup.
void initmicroSD(){
   if (!SD.begin(sdCardPinChipSelect)) { //SD for MEGA 
    Serial.println(F("No microSD found!"));
    while (1);
  }
  pinMode(sdCardPinChipSelect, OUTPUT);//SD pour MEGA
}

//+++++++++++++++++++++++++++++++++++++
//++++++++  Time functions ++++++++++++
//+++++++++++++++++++++++++++++++++++++

//Initialization function and low-power RTC
void initRTC(){

  if (! RTC.begin()) { //RTC DS3231
    Serial.flush();
    while (1); delay(10);
  }
  // following line sets the RTC to the date & time this sketch was compiled
  if (RTC.lostPower()) {//time adjustment function if there is a loss of time; recalibration.
    RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

//time update
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

//datetime formatting
void get_formatted_datetime(const DateTime& now, char* datetime, size_t size){
  snprintf(datetime, size, "%02d/%02d/%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
}

//Printing the formatted DateTime on the serial port
void printFormatedDateTime(const DateTime& now){
  char buffer[25] = "\0";
  snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d %02d:%02d:%02d ", now.day(), now.month(), now.year(), now.hour(), now.minute(), now.second());
  Serial.print(buffer);
}
