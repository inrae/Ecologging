#ifndef ECOLOGGING_H
#define ECOLOGGING_H

#include "config.h" //configuration file


//++++++++++++++++++++ card recording +++++++++++++++++++++++++++
#define sdCardPinChipSelect   53 //SD pinout MEGA  53 => PIN53

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++ File writing functions ++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//Initialization function for microSD recording
  //WARNING: This requires a call to Wire.begin(); in the program's setup.
void initmicroSD(){
  if (!SD.begin(sdCardPinChipSelect)) { //SD for MEGA 
    Serial.println(F("[SD] Error : No microSD found!"));
    while (1);
  }
  pinMode(sdCardPinChipSelect, OUTPUT);//SD pour MEGA
  Serial.println(F("[SD] MicroSD card initialized."));
}

#endif // ECOLOGGING_H
