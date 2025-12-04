#include "WString.h"
/*
Capteurs_meteo
*/
#include "capteurs_meteo.h"

volatile bool CAPTEURS_METEO::IsSampleRequired = false;
volatile unsigned int CAPTEURS_METEO::TimerCount = 0;
volatile unsigned long CAPTEURS_METEO::Rotations;
volatile unsigned long CAPTEURS_METEO::ContactBounceTime;

//+++++++++++ Constructor +++++++++++
CAPTEURS_METEO::CAPTEURS_METEO(uint16_t WLinstall, uint8_t pin_DS18B20)
  : sht31(SHT31_ADDRESS, &Wire),   //Initialisation dans la liste d'initialisation
  oneWire_Teau(pin_DS18B20),
  sensor_Teau(&oneWire_Teau),
  _WLinstall(WLinstall)
{}

//++++++++++ Valeurs courantes ++++++++++
float CAPTEURS_METEO::valTemp(){return TempMesure;}
float CAPTEURS_METEO::valHumid(){return HumidMesure;}
float CAPTEURS_METEO::valPatm(){return PatmMesure;}
float CAPTEURS_METEO::valRay(){return RayMesure;}
float CAPTEURS_METEO::valPyrano(){return PyranoMesure;}
float CAPTEURS_METEO::valTempWater(){return TempWaterMesure;}
float CAPTEURS_METEO::valVitesse(){return VitesseMesure;}
float CAPTEURS_METEO::valDirection(){return DirectionMesure;}
float CAPTEURS_METEO::valWaterVolt(){return WaterVoltMesure;}
float CAPTEURS_METEO::valWaterColonne(){return WaterColonneMesure;}
float CAPTEURS_METEO::valWaterHauteur(){return WaterHauteurMesure;}

//++++++++++ Moyennes ++++++++++
void CAPTEURS_METEO::resetSommes(){
  SommeT = 0.0;
  SommeHR = 0.0;
  SommeL = 0.0;
  SommeLW = 0.0;
  SommeTW = 0.0;
  SommeV = 0.0;
  SommeD = 0.0;
  SommeP = 0.0;
  SommeWV = 0.0;
  SommeWC = 0.0;
  SommeWH = 0.0;

  nbT = 0;
  nbHR = 0;
  nbL = 0;
  nbLW = 0;
  nbTW = 0;
  nbV = 0;
  nbD = 0;
  nbP = 0;
  nbWH = 0;
}

float CAPTEURS_METEO::meanTemp(){if(nbT > 0){return SommeT/nbT;} return 0;}
float CAPTEURS_METEO::meanHumid(){if(nbHR > 0){return SommeHR/nbHR;} return 0;}
float CAPTEURS_METEO::meanPatm(){if(nbP > 0){return SommeP/nbP;} return 0;}
float CAPTEURS_METEO::meanRay(){if(nbL > 0){return SommeL/nbL;} return 0;}
float CAPTEURS_METEO::meanTempWater(){if(nbTW > 0){return SommeTW/nbTW;} return 0;}
float CAPTEURS_METEO::meanPyrano(){if(nbLW > 0){return SommeLW/nbLW;} return 0;}
float CAPTEURS_METEO::meanVitesse(){if(nbV > 0){return SommeV/nbV;} return 0;}
float CAPTEURS_METEO::meanDirection(){if(nbD > 0){return SommeD/nbD;} return 0;}
float CAPTEURS_METEO::meanWaterVolt(){if(nbWH > 0){return SommeWV/nbWH;} return 0;}
float CAPTEURS_METEO::meanWaterColonne(){if(nbWH > 0){return SommeWC/nbWH;} return 0;}
float CAPTEURS_METEO::meanWaterHauteur(){if(nbWH > 0){return SommeWH/nbWH;} return 0;}

//++++++++++ Cumuls ++++++++++
void CAPTEURS_METEO::resetCumuls(){
  dailyRain = 0.0;                                      // clear daily-rain at midnight
  dailyRain_till_LastHour = 0.0;                        // we do not want negative rain at 01:00
}

void CAPTEURS_METEO::setHcumulPluvio(){
  hourlyRain = dailyRain - dailyRain_till_LastHour;      // calculate the last hour's rain
  dailyRain_till_LastHour = dailyRain;// update the rain till last hour for next calculation
}

double CAPTEURS_METEO::cumulHRain(){
  return hourlyRain;
}

double CAPTEURS_METEO::cumulDRain(){
  return dailyRain;
}

//++++++++++ VEML7700 ++++++++++
//Fonction démarrage Rayonnement VEML7700
void CAPTEURS_METEO::initVEML7700(){
  als.begin();//VEML7700
}

//acquisition VEML7700
void CAPTEURS_METEO::acqVEML7700(){
  als.getALSLux(lux);//VEML7700
  RayMesure = lux;
  //sommation
  SommeL += RayMesure;
  nbL++;

  Serial.print(lux);Serial.print(F(" Lux\t"));
}

//++++++++++ Pyranomètre Davis 6450 +++++++++
//Fonction démarrage pyrano
void CAPTEURS_METEO::initPyrano(){
  int rawValue = analogRead(PyranoPin);
  //filtrage si valeur 1023 i.e. capteur non branché et donc valeur de pull up
  if(rawValue == 1023){Serial.print(F("## WARNING ! : Davis pyrano seems not to be connected!"));}
}

//acquisition pyrano Davis 6450
void CAPTEURS_METEO::acqPyrano(){
  int rawValue = analogRead(PyranoPin); // Signal

  //filtrage si valeur 1023 i.e. capteur non branché et donc valeur de pull up
  if(rawValue == 1023){rawValue = 0;}

  float voltage = rawValue * (pyrReferenceVoltage / 1023);
  
  // Correction offset
  float correctedVoltage = voltage - pyrZeroOffset;
  if (correctedVoltage < 0) correctedVoltage = 0;

  // Conversion et calibration
  float rawRadiation = correctedVoltage / pyrSensitivity;
  PyranoMesure = rawRadiation;
  //sommation
  SommeLW += PyranoMesure;
  nbLW++;

  Serial.print(PyranoMesure);Serial.print("W/m²\t");
}         

//++++++++++ BME280 ++++++++++
//Fonction initialisation BME280
void CAPTEURS_METEO::initBME280(){
  if (!bme.begin()) {//BME280
    while (1);
  }
  bme.setTempCal(-1);
}

//acquisition BME280
void CAPTEURS_METEO::acqBME280(bool Ponly){
  bme.readSensor();

  PatmMesure = bme.getPressure_MB();
  SommeP += PatmMesure;
  nbP++;

  if (!Ponly) {
    TempMesure = bme.getTemperature_C();
    HumidMesure = bme.getHumidity();

    SommeT += TempMesure;
    SommeHR += HumidMesure;

    nbT++;
    nbHR++;

    Serial.print(TempMesure,1);Serial.print(F(" °C T_env bme280\t"));// T en °C
    Serial.print(HumidMesure,1);Serial.print(F(" % H_env bme280\t"));// HR en %
  }

  Serial.print(PatmMesure, 1);Serial.print(" mbar P_env\t");// P en mbar
}

//++++++++++ SHT31 ++++++++++
//Fonction initialisation SHT31
void CAPTEURS_METEO::initSHT31(){
  if(sht31.begin() == false){Serial.println(F("SHT31 device address or reset pb."));}

  uint16_t stat = sht31.readStatus();
  Serial.print(stat, HEX);Serial.println();
}

//acquisition SHT31
void CAPTEURS_METEO::acqSHT31(){
  if(sht31.isConnected()){
    sht31.read();
    TempMesure = sht31.getTemperature();
    HumidMesure = sht31.getHumidity();
    //sommation
    SommeT += TempMesure;
    SommeHR += HumidMesure;
    nbT++;
    nbHR++;

    Serial.print(HumidMesure,1);Serial.print(F(" % H_env sht31\t"));// HR en %
    Serial.print(TempMesure, 1); Serial.print(F(" °C T_env sht31\t")); // T en °C
  }else{
    Serial.println(F("Error : SHT31 Not connected! Try reset sensor."));
    if(sht31.reset() == true){Serial.println(F("sht31 reset done"));}else{Serial.println(F("Failed to reset SHT31"));}
  }
}

//++++++++++ SHT20/SEN0227 ++++++++++
//Fonction initialisation SHT20
void CAPTEURS_METEO::initSHT20(){
  sht20.initSHT20();      //SHT20/SEN0227
  sht20.checkSHT20();     //SHT20/SEN0227
}

//acquisition SHT20
void CAPTEURS_METEO::acqSHT20(){
  TempMesure = sht20.readTemperature();   //SHT20/SEN0227
  HumidMesure = sht20.readHumidity();     //SHT20/SEN0227
  //sommation
  SommeT += TempMesure;
  SommeHR += HumidMesure;
  nbT++;
  nbHR++;
  
  Serial.print(HumidMesure, 1);Serial.print(F("% H_env sht20\t"));// HR en %
  Serial.print(TempMesure, 1);Serial.print(F("*C T_env sht20\t"));// T en °C
}

//++++++++++ Pluviomètre bascule ++++++++++
//Fonction initialisation Pluviomètre
void CAPTEURS_METEO::initPluvio(){
  pinMode(RainPin, INPUT);//pluviomètre lecture PIN2
}

void CAPTEURS_METEO::acqPluvio(){
    //Partie comptage de la bascule
  if ((bucketPositionA==false)&&(digitalRead(RainPin)==HIGH)){
    bucketPositionA=true;
    dailyRain+=bucketAmount;                               // update the daily rain
  }
  
  if ((bucketPositionA==true)&&(digitalRead(RainPin)==LOW)){
    bucketPositionA=false;
  }
}

//++++++++++ ANEMOMETRE DAVIS ++++++++++

//Fonction initialisation Vitesse et orientation du vent
void CAPTEURS_METEO::initVent1(){
  LastValue = 0;//DAVIS
  IsSampleRequired = false;//DAVIS
  TimerCount = 0;//DAVIS
  Rotations = 0;//DAVIS
}

void CAPTEURS_METEO::initVent2(){
  pinMode(WindSensorPin, INPUT);//DAVIS
  attachInterrupt(digitalPinToInterrupt(WindSensorPin), isr_rotation, FALLING);//DAVIS
  Timer1.initialize(500000);//DAVIS
  Timer1.attachInterrupt(isr_timer);//DAVIS
}

// isr routine fr timer interrupt
void CAPTEURS_METEO::isr_timer() {
  
  TimerCount++;
  
  if(TimerCount == 5)
  {
    IsSampleRequired = true;
    TimerCount = 0;
  }
}

// This is the function that the interrupt calls to increment the rotation count
void CAPTEURS_METEO::isr_rotation() {

  if ((millis() - ContactBounceTime) > 15 ) {  // debounce the switch contact.
    Rotations++;
    ContactBounceTime = millis();
  }

}

// Get Wind Direction
void CAPTEURS_METEO::acqWindDirection() {
 
   VaneValue = analogRead(WindVanePin);
   Direction = map(VaneValue, 0, 1023, 0, 360);
   CalDirection = Direction + VaneOffset;
   
   if(CalDirection > 360)
     CalDirection = CalDirection - 360;
     
   if(CalDirection < 0)
     CalDirection = CalDirection + 360;
   
}

// Converts compass direction to heading
void CAPTEURS_METEO::getHeading(int direction) {
    if(direction < 22.5)
      Serial.print(" N");
    else if (direction < 45)
      Serial.print(" NNE");
    else if (direction < 67.5)
      Serial.print(" NE");
    else if (direction < 90)
      Serial.print(" ENE");
    else if (direction < 112.5)
      Serial.print(" E");
    else if (direction < 135)
      Serial.print(" ESE");
    else if (direction < 157.5)
      Serial.print(" SE");
    else if (direction < 180)
      Serial.print(" SSE");
    else if (direction < 180)
      Serial.print(" SSE");
    else if (direction < 202.5)
      Serial.print(" S");
    else if (direction < 225)
      Serial.print(" SS0");
    else if (direction < 247.5)
      Serial.print(" S0");
    else if (direction < 270)
      Serial.print(" OSO");
    else if (direction < 292.5)
      Serial.print(" O");
    else if (direction < 315)
      Serial.print(" ONO");
    else if (direction < 337.5)
      Serial.print(" NO");
    else if (direction < 360)
      Serial.print(" NNO");
    else
      Serial.print(" N");  
}

//acquisition Vitesse et orientation du vent
void CAPTEURS_METEO::acqVent(){
  CAPTEURS_METEO::acqWindDirection();
  if(abs(CalDirection - LastValue) > 5){LastValue = CalDirection;}
  
  DirectionMesure = CalDirection;
  //sommation
  SommeD += DirectionMesure;
  nbD++;

  if(IsSampleRequired){
    WindSpeed = ((Rotations * 0.1125)*1.60934);//multiplication par 1.60934 pour passer en km/h formule pour calculer la vitesse windspeed : V = P(2.25/2.5) = P * 0.9 
    Rotations = 0;
    IsSampleRequired = false;

    VitesseMesure = WindSpeed;
    //sommation
    SommeV += VitesseMesure;
    nbV++;

    Serial.print(WindSpeed);Serial.print(F(" km/h\t"));
    getHeading(CalDirection);Serial.println(F("\t"));
  }
}

//++++++++++ DS18B20 Soil temperature ++++++++++

//Fonction initialisation DS18B20
void CAPTEURS_METEO::initDS18B20(){
   sensor_Teau.begin();
}

//acquisition DS18B20
void CAPTEURS_METEO::acqDS18B20Teau(){
   sensor_Teau.requestTemperatures();
   TempWaterMesure = sensor_Teau.getTempCByIndex(0);
   //sommation
   SommeTW += TempWaterMesure;
   nbTW++;
   Serial.print(TempWaterMesure, 2);Serial.print(F("°C TW_env\t"));
 }

//++++++++++ ADS1X15 + sonde water level KIT0139 Franck Perret ++++++++++
void CAPTEURS_METEO::initADS(){
  ads.begin();
}

//acquisition ADS avec Kit0139
void CAPTEURS_METEO::acqADS_kit0139(){
  float rawADC = ads.readADC_SingleEnded(0);
  WaterVoltMesure = rawADC*0.0001875;
  WaterColonneMesure = ((1.25*WaterVoltMesure)-1.25)*1000;    //Avec une résistance de précision de 250 ohms alimentation en 5VDC attention même masse
  WaterHauteurMesure = WaterColonneMesure - _WLinstall;            //3450 pour piezo C5
  
  //sommation
  SommeWV += WaterVoltMesure;
  SommeWC += WaterColonneMesure;
  SommeWH += WaterHauteurMesure;
  nbWH++;

  //Serial.print(_WLinstall, 1);Serial.print(F(" WLinstall \t"));
  Serial.print(WaterVoltMesure, 3);Serial.print(F(" Volt \t"));
  Serial.print(WaterColonneMesure, 3);Serial.print(F(" WCol mm \t"));
  Serial.print(WaterHauteurMesure, 1);Serial.println(F(" WL mm \t"));
}