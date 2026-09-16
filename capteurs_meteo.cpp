#include "Stream.h"
#include "Arduino.h"
#include "HardwareSerial.h"
#include "WString.h"
/*
Capteurs_meteo

Weather sensors
*/
#include "capteurs_meteo.h"

volatile unsigned int CAPTEURS_METEO::TimerCount = 0;
volatile unsigned long CAPTEURS_METEO::Rotations;
volatile unsigned long CAPTEURS_METEO::ContactBounceTime;
volatile float CAPTEURS_METEO::VitesseMesure = 0.0;
volatile float CAPTEURS_METEO::MaxSpeedMesure = 0.0;

//+++++++++++ Constructor +++++++++++
CAPTEURS_METEO::CAPTEURS_METEO(uint16_t WLinstall, uint8_t pin_DS18B20)
  : sht31(SHT31_ADDRESS, &Wire),   //Initialization in the initialization list
  oneWire_Teau(pin_DS18B20),
  sensor_Teau(&oneWire_Teau),
  _WLinstall(WLinstall)
{}

//++++++++++ Current values ++++++++++
float CAPTEURS_METEO::valTemp(){return TempMesure;}
float CAPTEURS_METEO::valHumid(){return HumidMesure;}
float CAPTEURS_METEO::valPatm(){return PatmMesure;}
float CAPTEURS_METEO::valRay(){return RayMesure;}
float CAPTEURS_METEO::valPyrano(){return PyranoMesure;}
float CAPTEURS_METEO::valTempWater(){return TempWaterMesure;}
float CAPTEURS_METEO::valVitesse(){return VitesseMesure;}
float CAPTEURS_METEO::valMaxVitesse(){return MaxSpeedMesure;}
float CAPTEURS_METEO::valDirection(){return DirectionMesure;}
float CAPTEURS_METEO::valWaterVolt(){return WaterVoltMesure;}
float CAPTEURS_METEO::valWaterColonne(){return WaterColonneMesure;}
float CAPTEURS_METEO::valWaterHauteur(){return WaterHauteurMesure;}
float CAPTEURS_METEO::valTempSEN0600(uint8_t index){return (index < _SEN0600_nbProbes) ? TempSEN0600Mesure[index] : -999.0;}
float CAPTEURS_METEO::valHumidSEN0600(uint8_t index){return (index < _SEN0600_nbProbes) ? HumidSEN0600Mesure[index] : -999.0;}

//++++++++++ Averages ++++++++++
void CAPTEURS_METEO::resetSommes(){
  MaxSpeedMesure = 0.0;

  SommeT = 0.0;
  SommeHR = 0.0;
  SommeL = 0.0;
  SommeLW = 0.0;
  SommeTW = 0.0;
  SommeV = 0.0;
  //SommeD = 0.0;
  SommeSinD = 0.0;
  SommeCosD = 0.0;
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

  for (int i = 0; i < _SEN0600_nbProbes; i++) {
    SommeT_SEN0600[i] = 0.0;
    SommeH_SEN0600[i] = 0.0;
    nbT_SEN0600[i] = 0;
    nbH_SEN0600[i] = 0;
  }
}

float CAPTEURS_METEO::meanTemp(){if(nbT > 0){return SommeT/nbT;} return -999.0;}
float CAPTEURS_METEO::meanHumid(){if(nbHR > 0){return SommeHR/nbHR;} return -999.0;}
float CAPTEURS_METEO::meanPatm(){if(nbP > 0){return SommeP/nbP;} return -999.0;}
float CAPTEURS_METEO::meanRay(){if(nbL > 0){return SommeL/nbL;} return -999.0;}
float CAPTEURS_METEO::meanTempWater(){if(nbTW > 0){return SommeTW/nbTW;} return -999.0;}
float CAPTEURS_METEO::meanPyrano(){if(nbLW > 0){return SommeLW/nbLW;} return -999.0;}
float CAPTEURS_METEO::meanVitesse(){if(nbV > 0){return SommeV/nbV;} return -999.0;}
float CAPTEURS_METEO::meanDirection(){
  if(nbD > 0){
    float angleMoyenRad = atan2(SommeSinD, SommeCosD);  // atan2 compute the mean direct angle in radians from the sums
    float angleMoyenDeg = angleMoyenRad * RAD_TO_DEG;   // convert to degree
    if (angleMoyenDeg < 0) {angleMoyenDeg += 360.0;}  	// atan2 range -180° et +180°, transform to range 0° et 360° :
    return angleMoyenDeg;
  }
  return -999.0;
}
float CAPTEURS_METEO::meanWaterVolt(){if(nbWH > 0){return SommeWV/nbWH;} return 0;}
float CAPTEURS_METEO::meanWaterColonne(){if(nbWH > 0){return SommeWC/nbWH;} return 0;}
float CAPTEURS_METEO::meanWaterHauteur(){if(nbWH > 0){return SommeWH/nbWH;} return -999.0;}
float CAPTEURS_METEO::meanTempSEN0600(uint8_t index) {
  if (index < _SEN0600_nbProbes && nbT_SEN0600[index] > 0) return SommeT_SEN0600[index] / nbT_SEN0600[index];
  return -999.0;
}
float CAPTEURS_METEO::meanHumidSEN0600(uint8_t index) {
  if (index < _SEN0600_nbProbes && nbH_SEN0600[index] > 0) return SommeH_SEN0600[index] / nbH_SEN0600[index];
  return -999.0;
}

//++++++++++ Accumulations ++++++++++
void CAPTEURS_METEO::resetCumuls(){
  dailyRain = 0.0;                                      // clear daily-rain at midnight
  dailyRain_till_LastHour = 0.0;                        // we do not want negative rain at 01:00
}

void CAPTEURS_METEO::setHcumulPluvio(){
  hourlyRain = dailyRain - dailyRain_till_LastHour;     // calculate the last hour's rain
  dailyRain_till_LastHour = dailyRain;                  // update the rain till last hour for next calculation
}

double CAPTEURS_METEO::cumulHRain(){
  return hourlyRain;
}

double CAPTEURS_METEO::cumulDRain(){
  return dailyRain;
}

//++++++++++ VEML7700 ++++++++++
//VEML7700 Radiation Start-up Function
void CAPTEURS_METEO::initVEML7700(){
  als.begin();
}

//acquisition VEML7700
void CAPTEURS_METEO::acqVEML7700(){
  als.getALSLux(lux);//VEML7700
  RayMesure = lux;
  //summation
  SommeL += RayMesure;
  nbL++;

  Serial.print(lux);Serial.print(F(" Lux\t"));
}

//++++++++++ Pyranometer Davis 6450 +++++++++
// Pyrano init function
void CAPTEURS_METEO::initPyrano(){
    
  int rawValue = analogRead(PyranoPin);
  //Filtering if value 1023 i.e. sensor not connected and therefore pull-up value
  if(rawValue == 1023){Serial.print(F("## WARNING ! : Davis pyrano seems not to be connected!"));}
}

// Pyrano Davis 6450 acquisition
void CAPTEURS_METEO::acqPyrano(){
  int rawValue = analogRead(PyranoPin); // Signal

  // Filtering if value 1023 i.e. sensor not connected and therefore pull-up value
  if(rawValue == 1023){rawValue = 0;}

  float voltage = rawValue * (pyrReferenceVoltage / 1023);
  
  // Offset correction
  float correctedVoltage = voltage - pyrZeroOffset;
  if (correctedVoltage < 0) correctedVoltage = 0;

  // Conversion and calibration
  float rawRadiation = correctedVoltage / pyrSensitivity;
  PyranoMesure = rawRadiation;
  // summation
  SommeLW += PyranoMesure;
  nbLW++;

  Serial.print(PyranoMesure);Serial.print("W/m²\t");
}         

//++++++++++ BME280 ++++++++++
// Init function BME280
void CAPTEURS_METEO::initBME280(){
  if (!bme.begin()) {//BME280
    while (1);
  }
  bme.setTempCal(-1);
}

// Acquisition BME280
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
// Init function SHT31
void CAPTEURS_METEO::initSHT31(){
  if(sht31.begin() == false){Serial.println(F("SHT31 device address or reset pb."));}

  uint16_t stat = sht31.readStatus();
  Serial.print(stat, HEX);Serial.println();
}

// Acquisition SHT31
void CAPTEURS_METEO::acqSHT31(){
  if(sht31.isConnected()){
    sht31.read();
    TempMesure = sht31.getTemperature();
    HumidMesure = sht31.getHumidity();
    //summation
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
// Init function SHT20
void CAPTEURS_METEO::initSHT20(){
  sht20.initSHT20();      //SHT20/SEN0227
  sht20.checkSHT20();     //SHT20/SEN0227
}

// Acquisition SHT20
void CAPTEURS_METEO::acqSHT20(){
  TempMesure = sht20.readTemperature();   //SHT20/SEN0227
  HumidMesure = sht20.readHumidity();     //SHT20/SEN0227
  //summation
  SommeT += TempMesure;
  SommeHR += HumidMesure;
  nbT++;
  nbHR++;
  
  Serial.print(HumidMesure, 1);Serial.print(F("% H_env sht20\t"));// HR en %
  Serial.print(TempMesure, 1);Serial.print(F("*C T_env sht20\t"));// T en °C
}

//++++++++++ Tilting Pluviometer ++++++++++
// Init function pluviometer
void CAPTEURS_METEO::initPluvio(){
  pinMode(RainPin, INPUT);//pluviometer read
}

void CAPTEURS_METEO::acqPluvio(){
    //counting section
  if ((bucketPositionA==false)&&(digitalRead(RainPin)==HIGH)){
    bucketPositionA=true;
    dailyRain+=bucketAmount;                               // update the daily rain
  }
  
  if ((bucketPositionA==true)&&(digitalRead(RainPin)==LOW)){
    bucketPositionA=false;
  }
}

//++++++++++ ANEMOMETER DAVIS ++++++++++

// Init function Wind speed and direction
void CAPTEURS_METEO::initVent1(){
  LastValue = 0;
  TimerCount = 0;
  Rotations = 0;
}

void CAPTEURS_METEO::initVent2(){
  pinMode(WindSensorPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(WindSensorPin), isr_rotation, FALLING);
  Timer1.initialize(1000000);                 //Timer irq set to 1 sec
  Timer1.attachInterrupt(isr_timer);

  Serial.print(F("Davis anemometer integration time : "));
  Serial.print(INTEGRATION_TIME_SEC);
  Serial.println(F(" sec."));
}

// isr routine fr timer interrupt
void CAPTEURS_METEO::isr_timer() {
  
  TimerCount++;
  
  if(TimerCount >= INTEGRATION_TIME_SEC) {
    // convert to mp/h using the formula V=P(2.25/T)
    // WindSpeedMPH = Rotations * (2.25/(float)INTEGRATION_TIME_SEC);
    // WindSpeedKMH = WindSpeedMPH * 1.60934;

    VitesseMesure = Rotations * WIND_FACTOR;
    
    //store max speed
    if (VitesseMesure > MaxSpeedMesure) {
      MaxSpeedMesure = VitesseMesure;
    }
    
    // Reset count for next sample
    Rotations = 0;
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
    if(direction < 22.5)        Serial.print(" N");
    else if (direction < 45)    Serial.print(" NNE");
    else if (direction < 67.5)  Serial.print(" NE");
    else if (direction < 90)    Serial.print(" ENE");
    else if (direction < 112.5) Serial.print(" E");
    else if (direction < 135)   Serial.print(" ESE");
    else if (direction < 157.5) Serial.print(" SE");
    else if (direction < 180)   Serial.print(" SSE");
    else if (direction < 202.5) Serial.print(" S");
    else if (direction < 225)   Serial.print(" SS0");
    else if (direction < 247.5) Serial.print(" S0");
    else if (direction < 270)   Serial.print(" OSO");
    else if (direction < 292.5) Serial.print(" O");
    else if (direction < 315)   Serial.print(" ONO");
    else if (direction < 337.5) Serial.print(" NO");
    else if (direction < 360)   Serial.print(" NNO");
    else                        Serial.print(" N");  
}

// Speed & Wind direction acquisition
void CAPTEURS_METEO::acqVent(){
  CAPTEURS_METEO::acqWindDirection();
  if(abs(CalDirection - LastValue) > 5){LastValue = CalDirection;}  // Only update the display if change greater than 5 degrees.
  DirectionMesure = CalDirection;

  // --- Compute TRIGONOMETRIC ---
  float angleRad = (float)DirectionMesure * DEG_TO_RAD; 
  // sum horizontal and vertical component of wind vector
  SommeSinD += sin(angleRad);
  SommeCosD += cos(angleRad);
  nbD++;

  //summation speed
  SommeV += VitesseMesure;
  nbV++;

  Serial.print(VitesseMesure);Serial.print(F(" km/h\t"));
  getHeading(CalDirection);Serial.println(F("\t"));
}

//++++++++++ DS18B20 Soil temperature ++++++++++

// Init function DS18B20
void CAPTEURS_METEO::initDS18B20(){
   sensor_Teau.begin();
}

// Acquisition DS18B20
void CAPTEURS_METEO::acqDS18B20Teau(){
   sensor_Teau.requestTemperatures();
   TempWaterMesure = sensor_Teau.getTempCByIndex(0);
   //summation
   SommeTW += TempWaterMesure;
   nbTW++;
   Serial.print(TempWaterMesure, 2);Serial.print(F("°C TW_env\t"));
 }

//++++++++++ ADS1X15 + water level probe KIT0139 Franck Perret ++++++++++
void CAPTEURS_METEO::initADS(){
  ads.begin();
}

// acquisition ADS with Kit0139
void CAPTEURS_METEO::acqADS_kit0139(){
  float rawADC = ads.readADC_SingleEnded(0);
  WaterVoltMesure = rawADC*0.0001875;
  WaterColonneMesure = ((1.25*WaterVoltMesure)-1.25)*1000;    //With a 250 ohm precision resistor, 5VDC power supply, be careful to connect the same ground
  WaterHauteurMesure = WaterColonneMesure - _WLinstall;            //3450 for piezo C5
  
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

//++++++++++ RS485 SEN0600 ++++++++++
// Initialisation RS485
void CAPTEURS_METEO::initSEN0600(Stream* mySerialPort, uint8_t nbProbes, const uint8_t* addrList) {
  _serialRS485 = mySerialPort;
  _SEN0600_addrList = addrList;

  //check memory allocation
  if (nbProbes > MAX_SEN0600_PROBES ) {
    _SEN0600_nbProbes = MAX_SEN0600_PROBES;
    Serial.println(F("WARNING: To many probes requested. Limit to MAX_SEN0600_PROBES."));
  } else {
    _SEN0600_nbProbes = nbProbes;
  }

  pinMode(RS485_DE_RE, OUTPUT);
  digitalWrite(RS485_DE_RE, LOW);
  _serialRS485->setTimeout(100); // Timeout pour readBytes
  //Serial.println(F("SEN0600 init done"));
  
  // Initialisation des tableaux à zéro
  for (int i = 0; i < _SEN0600_nbProbes; i++) {
    TempSEN0600Mesure[i] = 0.0;
    HumidSEN0600Mesure[i] = 0.0;
    SommeT_SEN0600[i] = 0.0;
    SommeH_SEN0600[i] = 0.0;
    nbT_SEN0600[i] = 0;
    nbH_SEN0600[i] = 0;
  }
  //Serial.println(F("SEN0600 variables to zero"));
}

// Compute CRC 16 modbus
uint16_t CAPTEURS_METEO::calculateCRC(uint8_t *buf, int len) {
  uint16_t crc = 0xFFFF;
  for (int pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)buf[pos];
    for (int i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}


//Acquisition session launcher
void CAPTEURS_METEO::acqSEN0600launch(){
  if(_SEN0600_nbProbes == 0) return;
  if(!_SEN0600_session_running){
    _SEN0600_session_running = true;
    _SEN0600_ActiveProbeIndex = 0;
    _t_SEN0600_LastAction = millis();
  }
}

//Assync acquisition datas
void CAPTEURS_METEO::updateSEN0600acq(){
  if(!_SEN0600_session_running || _SEN0600_nbProbes == 0) return;
  if(millis() - _t_SEN0600_LastAction >= _SEN0600_PROBE_INTERVAL){
    _t_SEN0600_LastAction = millis();
    acqSEN0600(_SEN0600_ActiveProbeIndex);
    _SEN0600_ActiveProbeIndex++;
    if(_SEN0600_ActiveProbeIndex >= _SEN0600_nbProbes){
      _SEN0600_session_running = false;
    }
  }
}


// Acquisition RS485 SEN0600 probe
void CAPTEURS_METEO::acqSEN0600(uint8_t index) {
  if (_SEN0600_nbProbes == 0 || _SEN0600_addrList == nullptr || index >= _SEN0600_nbProbes) return;

  Serial.print(F("Start SEN0600["));Serial.print(index);Serial.println(F("] acquisition"));
  uint8_t addr = _SEN0600_addrList[index];
  uint8_t msg[8] = {addr, 0x03, 0x00, 0x00, 0x00, 0x02, 0, 0};
  uint16_t crc = calculateCRC(msg, 6);
  msg[6] = lowByte(crc);
  msg[7] = highByte(crc);

  // free buffer
  while (_serialRS485->available()) _serialRS485->read();

  // Send request
  //Serial.println(F("Request bus"));
  digitalWrite(RS485_DE_RE, HIGH);
  delayMicroseconds(10);
  //Serial.println(F("Bus write"));
  _serialRS485->write(msg, 8);
  _serialRS485->flush();
  digitalWrite(RS485_DE_RE, LOW);
  //Serial.println(F("Request end"));

  //Wait for probe response
  unsigned long startWait = millis();
  while (_serialRS485->available() == 0) {
    if (millis() - startWait > 10) break; // Timeout de sécurité de 10ms
  }

  // Receive message (9 bytes)
  uint8_t buffer[9];
  size_t received = _serialRS485->readBytes(buffer, 9);
  //Serial.println(F("Read bus"));


  if (received < 9){Serial.print(F("Not enough bus data : only ")); Serial.println(received); return;} // Not enough bytes received
  if (buffer[0] != addr){Serial.println(F("Incorrect address")); return;} // bad address
  if (buffer[1] != 0x03){Serial.println(F("Incorrect function")); return;} // bad function code

  if (received == 9 && buffer[0] == addr && buffer[1] == 0x03) {
    uint16_t checkCRC = calculateCRC(buffer, 7);
    if (lowByte(checkCRC) == buffer[7] && highByte(checkCRC) == buffer[8]) {
      //Serial.println(F("store datas"));
      HumidSEN0600Mesure[index] = ((buffer[3] << 8) | buffer[4]) / 10.0;
      TempSEN0600Mesure[index] = ((buffer[5] << 8) | buffer[6]) / 10.0;

      // Average datas
      SommeH_SEN0600[index] += HumidSEN0600Mesure[index];
      SommeT_SEN0600[index] += TempSEN0600Mesure[index];
      nbH_SEN0600[index]++;
      nbT_SEN0600[index]++;

      Serial.print(F("SEN0600[")); Serial.print(index); Serial.print(F("] Addr:")); Serial.print(addr);
      Serial.print(F(" H:")); Serial.print(HumidSEN0600Mesure[index], 1);
      Serial.print(F("% T:")); Serial.print(TempSEN0600Mesure[index], 1); Serial.println(F("°C"));
      return;
    }
  }
  Serial.print(F("Error RS485 Index ")); Serial.println(index);
}
