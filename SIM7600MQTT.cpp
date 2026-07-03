/*
SIM7600MQTT

This library only allows sending MQTT messages.
We make no guarantees.

philippe.chaumeil@inrae.fr _ Univ. Bordeaux, INRAE, BIOGECO, F-33610, Cestas, France

*/
#include "SIM7600MQTT.h"

SIM7600MQTT::SIM7600MQTT() {
  myserial = &Serial1;
}

void SIM7600MQTT::printSimState() {
  Serial.print(F("processMQTT: "));
  Serial.print(processMQTT);
  Serial.print(F("; sslMQTT: "));
  Serial.print(sslMQTT);
  Serial.print(F("; beginMQTT: "));
  Serial.print(beginMQTT);
  Serial.print(F("; closeMQTT: "));
  Serial.print(closeMQTT);
  Serial.print(F("; msgMQTT: "));
  Serial.print(msgMQTT);
  Serial.print(F("; netready :"));
  Serial.print(networkready);
  Serial.print(F("; executeStep:"));
  Serial.print(executeStep);
  Serial.print(F("; acqRespSIM:"));
  Serial.print(acqRespSIM);
  Serial.print(F("; beginstep:"));
  Serial.print(currentStepBeginMQTT);
  Serial.print(F("; closestep:"));
  Serial.print(currentStepCloseMQTT);
  Serial.print(F("; sslstep:"));
  Serial.print(currentStepSslMQTT);
  Serial.print(F("; msgstep:"));
  Serial.print(currentStepMsgMQTT);
  Serial.print(F("; statusError:"));
  Serial.print(statusError);
  Serial.print(F("; statusOk:"));
  Serial.print(statusOk);
  Serial.print(F("; statusPlus:"));
  Serial.print(statusPlus);
  Serial.print(F("; errorCode:"));
  Serial.println(errorCode);
}

//####################################### SIM7600 FUNCTIONS ###########################################

void SIM7600MQTT::initSIM7600() {
  //____SIM7600____
  //----power relay----
  pinMode(RELAY_SIM_PIN, OUTPUT);
  //----start module----
  myserial->begin(115200);  //unstable default serial speed of SIM7600 module
  Serial.println(F("starting module at 115200..."));
  pinMode(12, OUTPUT);
  digitalWrite(12, HIGH);  //boot sim7600 module

  //--- configure SIM7600 ---
  serialBuffer[SERIAL_BUFFER_SIZE - 1] = '\0';  //secure read
  executeStep = false;

  //--- initialize timers ---
  netTestTimer = millis();
  intervalPubTimer = millis();

  //---initialize software rtc ---
  DateTime now = rtc_sim7600.now();
  if(!now.isValid()){rtc_sim7600.begin(defaultDate);}
  rtc_sim7600.adjust(defaultDate);
}

//function to hard reset module switching off power with relay
void SIM7600MQTT::hard_reset_SIM7600(){
  if(allow_powerOFF()){
    Serial.println(F("--Hard reset Module--"));
    lastPowerOnTimer = 0;
    digitalWrite(RELAY_SIM_PIN, HIGH);
    resetBootState();
  }
}

bool SIM7600MQTT::allow_powerOFF(){
  if (lastPowerOnTimer == 0){
    return false;
  } else if((millis() - lastPowerOnTimer) > HARD_RESET_INTERVAL){
    return true;
  }  
  return false;
}

//return current processMQTT code status
byte SIM7600MQTT::get_status() {
  //#0 not ready to send MQTT #1 ready to send MQTT #2 MQTT msg sent and ready to send new MQTT
  if (!networkready) {
    return 0;
  } else if (processMQTT == 0) {
    return 1;
  } else if (processMQTT == 2) {
    return 2;
  } else {
    return 0;
  }
}

DateTime *SIM7600MQTT::get_gsm_datetime() {
  // get current soft RTC datetime
  DateTime now = rtc_sim7600.now();

  // check if datetime retrieve is default or from gsm provider
  if (now.year() == 2080) {
    if (gsm_datetime != nullptr) {
      delete gsm_datetime;  // release old object if necessary
      gsm_datetime = nullptr;
    }
    return nullptr;
  }

  // update or create DateTime global object
  if (gsm_datetime == nullptr) {
    gsm_datetime = new DateTime(now);  // Allocate only once
  } else {
    *gsm_datetime = now;  // update existing object
  }

  return gsm_datetime;
}

int SIM7600MQTT::get_utc_offset() {
  return UTC_offset;
}

//function to call in main loop to listenSerial
void SIM7600MQTT::listenSerialSIM7600() {
  while (myserial->available()) {
    int inByte = myserial->read();
    serialTimer = millis();
    acqRespSIM = 1;
    if (bufferIndex >= SERIAL_BUFFER_SIZE) {
      bufferIndex = 0;
      Serial.println(F("Buffer overflow!"));
    }  //overflow buffer
    if (inByte != -1) {
      char inChar = (char)inByte;
      if ((inChar == '\n') || (inChar == '\r')) {
        serialBuffer[bufferIndex] = '\0';                              //adding string termination
        if (strlen(serialBuffer) > 0) { scanResponse(serialBuffer); }  //response analysis
        bufferIndex = 0;
      } else {
        serialBuffer[bufferIndex] = inChar;
        bufferIndex++;
      }
      //check if wait for entry
      if (serialBuffer[0] == '>') {
        serialBuffer[bufferIndex] = '\0';
        scanResponse(serialBuffer);
        bufferIndex = 0;
      }
    }
  }
}

//-- parse response to search patterns --
void SIM7600MQTT::scanResponse(char *buffer) {
  bool debug = 1;
  char *result;
  if (debug) { Serial.print(F("buffer:")); Serial.println(buffer); }
  //search "CME" message
  result = strstr(buffer, "CME");
  if (result != NULL) {
    Serial.print(F("buffer:")); Serial.println(buffer);
  }
  //search ICCID
  result = strstr(buffer, "ICCID");
  if (result != NULL) {
    if (debug) { Serial.print(F("buffer:")); Serial.println(buffer); }
    Serial.println(F("SIM card ID response"));
    statusError = 0;
    errorCode = 0;
  }
  // search "ERROR"
  result = strstr(buffer, "ERROR");
  if (result != NULL) {
    statusError = 1;
    if (debug) { Serial.println(F("found ERROR")); }
  }
  // search "OK"
  result = strstr(buffer, "OK");
  if (result != NULL) {
    statusOk = 1;
    if (debug) { Serial.println(F("found OK")); }
  }
  //search ">"
  result = strstr(buffer, ">");
  if (result != NULL) {
    statusOk = 1;
    if (debug) { Serial.println(F("found >")); }
  }
  // search response "+XXX"
  result = strstr(buffer, "+C");
  if (result != NULL) {
    if (debug) { Serial.print(F("found motif +C: ")); Serial.println(result); }
    //search error code
    if (strstr(buffer, "START:") != NULL) {
      errorCode = atoi(&result[13]);
      statusPlus = 1;  //Note: no before because the command is echoed on the serial port
      if (errorCode != 0) {
        statusError = 1;
        Serial.print(F("--scan--")); Serial.print(F("buffer:")); Serial.println(buffer);
        Serial.print(F("found start error code: ")); Serial.println(errorCode);
        printSimState(); Serial.println(F("--end--"));
      }
    } else if (strstr(buffer, "STOP:") != NULL) {
      errorCode = atoi(&result[12]);
      statusPlus = 1;
      if (errorCode != 0) {
        statusError = 1;
        Serial.print(F("--scan--")); Serial.print(F("buffer:")); Serial.println(buffer);
        Serial.print(F("found stop error code: ")); Serial.println(errorCode);
        printSimState(); Serial.println(F("--end--"));
      }
    } else if (strstr(buffer, "REG:") != NULL) {
      errorCode = atoi(&result[9]);  //not an errorcode but a stat index
      statusPlus = 1;
      checkNetwork = millis();
      switch (errorCode) {
        case 0:
          Serial.print(F("--scan--")); Serial.print(F("buffer:")); Serial.println(buffer);
          Serial.println(F("Not registered, not searching operator!"));
          printSimState();
          Serial.println(F("--end--"));
          statusError = 1;
          errorCode = 99;
          break;
        case 1:
          statusError = 0;
          errorCode = 0;
          break;
        case 2:
          Serial.println(F("Not yet registered, searching operator..."));
          statusError = 1;
          errorCode = 99;
          break;
        case 5:
          Serial.print(F("--scan--")); Serial.print(F("buffer:")); Serial.println(buffer);
          Serial.print(F("Registered, roaming!")); printSimState();
          Serial.println(F("--end--"));
          statusError = 0;
          errorCode = 0;
          break;
        default:
          Serial.print(F("--scan--")); Serial.print(F("buffer:")); Serial.println(buffer);
          Serial.print(F("Registration pb!")); printSimState();
          Serial.println(F("--end--"));
          statusError = 1;
          errorCode = 99;
          break;
      }
    } else if (strstr(buffer, "CCLK:") != NULL) {
      //parse DateTime returned by SIM7600
      int Year, Month, Day, Hour, Minute, Second, Timezone;
      sscanf(buffer, "+CCLK: \"%d/%d/%d,%d:%d:%d%d\"", &Year, &Month, &Day, &Hour, &Minute, &Second, &Timezone);
      //default date if no date supplied by gsm operator is 80/xxx
      DateTime sim7600_date = DateTime(Year, Month, Day, Hour, Minute, Second);
      rtc_sim7600.adjust(sim7600_date);
      if (Year != 80) {
        UTC_offset = Timezone / 4;  //sim7600 return timezone in quarter of hour
      }
      Serial.print(F("-- SIM7600 soft RTC updated with -- ")); Serial.println(buffer);
    } else if (strstr(buffer, "NTP:") != NULL) {
      errorCode = atoi(&result[7]);
      statusPlus = 1;
      if (errorCode != 0) {
        statusError = 0;  //force not blocking ntp status code
        Serial.print(F("--scan--")); Serial.print(F("buffer:")); Serial.println(buffer);
        Serial.print(F("NTP failed code: ")); Serial.println(errorCode); Serial.println(F("--end--"));
      } else {
        Serial.println(F("-- NTP request passed --"));
        resetAtState();
        soloMQTT = true;
        currentStepSoloMQTT = 1;
        periodicStep(currentStepSoloMQTT);
        last_nettime_request = millis();
      }
    } else if (strstr(buffer, ": 0,") != NULL) {
      result = strstr(buffer, ": 0,");
      errorCode = atoi(&result[4]);
      statusPlus = 1;
      if (errorCode != 0) {
        statusError = 1;
        Serial.print(F("--scan--")); Serial.print(F("buffer:")); Serial.println(buffer);
        Serial.print(F("found error code: ")); Serial.println(errorCode);
        printSimState(); Serial.println(F("--end--"));
      }
    }
  }
}

//set module to max stable speed with arduino uno
void SIM7600MQTT::setSerialSpeed() {
  Serial.println(F("--setSerialSpeed 57600--"));
  myserial->println(F("AT+IPR=57600"));
  delay(100);
  myserial->end();
  myserial->begin(57600);
  delay(250);
}

//check timeout and completeness responses return false if problem
bool SIM7600MQTT::dialogCheck() {
  //Serial.println("--dialogCheck--");

  // response times are too long
  if (requestTimer != 0 && SIM7600ready) {
    if ((millis() - requestTimer) > requestTimerLimit) {
      Serial.println(F("No resp to request"));
      resetBootState();
      return false;
    }
  }
  // Serial port response considered complete if acqRespSIM #2. acqRespSIM #1 means that data has already been detected on the serial port.
  if (serialTimer != 0 && acqRespSIM == 1) {
    // Serial.print("--check serialTimer--");
    // Serial.println(millis() - serialTimer);

    //step at+connect must receive both responses
    if (beginMQTT && currentStepBeginMQTT == 4) {
      if ((statusOk || statusError) && statusPlus) {
        acqRespSIM = 2;
        requestTimer = 0;
        serialTimer = 0;
        // Serial.println(F("--serial complete #--"));
      }
    } else if (closeMQTT && (currentStepCloseMQTT <= 1)) {
      if ((statusOk || statusError) && statusPlus) {
        acqRespSIM = 2;
        requestTimer = 0;
        serialTimer = 0;
        // Serial.println(F("--serial complete #--"));
      }
    } else if (closeMQTT && (currentStepCloseMQTT == 2)) {
      if (statusOk || statusError) {
        acqRespSIM = 2;
        requestTimer = 0;
        serialTimer = 0;
        // Serial.println(F("--serial complete #--"));
      }
    } else if (soloMQTT && (currentStepSoloMQTT == 0)) {
      if (statusOk && statusPlus) {
        acqRespSIM = 2;
        requestTimer = 0;
        serialTimer = 0;
      }
    //other command based on duration
    } else if ((millis() - serialTimer) > MAX_SERIAL_TIMER) {
      acqRespSIM = 2;
      requestTimer = 0;
      serialTimer = 0;
      serialBuffer[bufferIndex] = '\0';  //adding string termination
      // Serial.println(F("--serial complete--"));
    }
  }
  return true;
}

void SIM7600MQTT::resetStatus() {
  acqRespSIM = 0;
  statusError = 0;
  statusOk = 0;
  statusPlus = 0;
  errorCode = -1;
  bufferIndex = 0;
}

void SIM7600MQTT::resetStep() {
  currentStepSettings = 0;
  currentStepBeginMQTT = 0;
  currentStepCloseMQTT = 0;
  currentStepSslMQTT = 0;
  currentStepMsgMQTT = 0;
  currentStepSoloMQTT = 0;
  executeStep = 0;
  onlyGetGsmDate = 0;
}

void SIM7600MQTT::resetAtState() {
  sslMQTT = false;
  beginMQTT = false;
  closeMQTT = false;
  msgMQTT = false;
  soloMQTT = false;
}

void SIM7600MQTT::resetTimers() {
  netTestTimer = 0;
  serialTimer = 0;
  requestTimer = 0;
  timeoutTimer = 0;
  retryTimer = 0;
  checkNetwork = 0;
  last_nettime_request = 0;
}

// close current connexion
void SIM7600MQTT::resetCnx() {
  resetStatus();
  resetStep();
  resetAtState();
  resetTimers();
  bufferIndex = 0;
  processMQTT = 1;  //TODO to check
  closeMQTT = true;
  currentStepCloseMQTT = 1;
  executeStep = true;
}

//reset to Boot conditions
void SIM7600MQTT::resetBootState() {
  Serial.println(F("--- Reset to Boot State ---"));
  resetStatus();
  resetStep();
  resetAtState();
  resetTimers();
  bufferIndex = 0;
  SIM7600ready = false;
  SIMready = false;
  networkready = false;
  processMQTT = 0;
  myserial->println(F("AT+CRESET"));
  myserial->end();
  initSIM7600();
  netTestTimer = millis();
}

//reset to search network conditions
void SIM7600MQTT::resetSearchNetState() {
  resetStatus();
  resetStep();
  resetAtState();
  resetTimers();
  bufferIndex = 0;
  SIM7600ready = true;
  networkready = false;  // SIMready = false;
  processMQTT = 0;
  beginMQTT = true;
  executeStep = true;
  netTestTimer = millis();
}

//-- management / startup verification SIM7600 --
void SIM7600MQTT::startupSIM() {
  bool debug = 0;
  static unsigned long tempo_HW_start = 0;
  static bool attente = false;
  if (!SIM7600ready) {
    //relay state to power on
    if (lastPowerOnTimer == 0){
      if(!attente) {
        tempo_HW_start = millis();
        attente = true;
      } else {
        if (millis() - tempo_HW_start >= 3000) {
          digitalWrite(RELAY_SIM_PIN, LOW);
          lastPowerOnTimer = millis();
          attente = false;
        }
      }
    }
  
    if (acqRespSIM == 2 && statusOk) {  // response management
      //Serial.println(F("--acqRespSIM=2 & statusOk--"));
      if (currentStepSettings == 0) {  //first item
        setSerialSpeed();              //switch to 57600bps
        waitingTry = 0;
        currentStepSettings++;
        settingsStep(currentStepSettings);
      } else if (currentStepSettings == 3) {  //last item
        SIM7600ready = 1;
        Serial.println(F("--- Last settingStep ---"));
      } else {
        currentStepSettings++;
        settingsStep(currentStepSettings);
      }
      acqRespSIM = 0;
      if (debug) { Serial.print("--startupSIM--"); printSimState();}
    } else if (currentStepSettings == 0 && millis() - netTestTimer > NET_TEST_TIMER_DELTA) {  // test module every x sec
      waitingTry++;
      if (waitingTry == 24) {  //If there is no response, it's possible that the Arduino was rebooted, but not the module which is listening for 57600.
        Serial.print(F("## Try 57600bps ##"));
        setSerialSpeed();
      }
      if (waitingTry > 48) {
        initSIM7600();
        waitingTry = 0;
      }
      Serial.print(F("##waitingTry:"));
      Serial.println(waitingTry);
      resetStatus();
      processMQTT = 0;
      Serial.println(F("waiting SIM7600 ready..."));
      settingsStep(currentStepSettings);
      netTestTimer = millis();
      if (debug) {  Serial.print(F("--startupSIM--")); printSimState(); }
    }
  }
}

void SIM7600MQTT::checkSIM() {
  bool debug = 1;
  if (SIM7600ready && !SIMready) {
    if (acqRespSIM == 2 && statusOk && errorCode == 0) {  // reponse management
      SIMready = 1;
      resetStatus();
      netTestTimer = 0;
      Serial.println(F("SIM card detected"));
      if (debug) { Serial.print("--checkSIM--"); printSimState(); }
    } else if (millis() - netTestTimer > NET_TEST_TIMER_DELTA) {  // test module every x sec
      resetStatus();
      processMQTT = 0;
      Serial.println(F("checking SIM..."));
      myserial->println("AT+CICCID");
      netTestTimer = millis();
      if (debug) { Serial.print(F("--checking SIM--")); printSimState(); }
    }
  }
}

//-- network acquisition --
void SIM7600MQTT::networkSearch() {
  if (SIM7600ready && SIMready) {
    if (!networkready) {
      processMQTT = 0;
      // Serial.print(F("--networkSearch-- ")); printSimState();
      // -- réponse netstatus --
      if (acqRespSIM == 2 && statusOk && errorCode == 0) {
        resetStatus();
        netTestTimer = 0;
        networkready = 1;
        Serial.println(F("Network connected..."));
        checkNetwork = millis();
        //launch get gsm date
        onlyGetGsmDate = true;
        processMQTT = 1;
        beginMQTT = true;
        currentStepBeginMQTT++;
        executeStep = true;
      } else if (millis() - netTestTimer > NET_TEST_TIMER_DELTA) {  //test network every x sec
        Serial.println(F("--networkSearch-- "));
        resetSearchNetState();
      }
    } else if (onlyGetGsmDate && acqRespSIM == 2) {
      resetStatus();
      onlyGetGsmDate = false;
      processMQTT = 0;
    } else if (processMQTT == 0 && (millis() - checkNetwork > CHECK_NETWORK)) {
      resetSearchNetState();
      checkNetwork = millis();
    }
  }
}

//prepare and format AT command for SSL steps
bool SIM7600MQTT::settingsStep(byte step) {
  bool debug = 1;
  requestTimer = 0;
  requestTimerLimit = MAX_REQUEST_TIMER;
  size_t buffer_size = 128;
  if (step > 3) { return false; Serial.println(F("Err#overTAB")); }
  char buffer[buffer_size];
  processMQTT = 1;
  strcpy_P(buffer, (char *)pgm_read_ptr(&(AT_settings_cmd[step])));
  if(debug){Serial.print(F("--- launch settingStep : ")); Serial.println(step); }
  myserial->println(buffer);
  requestTimer = millis();
  return true;
}

bool SIM7600MQTT::periodicStep(byte step){
  bool debug = 1;
  requestTimer = 0;
  requestTimerLimit = MAX_REQUEST_TIMER;
  size_t buffer_size = 128;
  if (step > 1) { return false; Serial.println(F("Err#overTAB")); }
  if (step == 0) { requestTimerLimit = 10000; } //to obtain response frome NTP server
  char buffer[buffer_size];
  processMQTT = 1;
  strcpy_P(buffer, (char *)pgm_read_ptr(&(AT_periodic_cmd[step])));
  if(debug){Serial.print(F("--- launch periodicStep : ")); Serial.println(step); }
  myserial->println(buffer);
  requestTimer = millis();
  return true;
}

//prepare and format AT command for begin steps
bool SIM7600MQTT::beginAtMQTT(byte step) {
  bool debug = 1;
  requestTimer = 0;
  size_t buffer_size = 128;
  if (step > 4) { return false; Serial.println(F("Err#overTAB")); }
  char buffer[buffer_size];
  char bufout[buffer_size];
  if (debug) { Serial.print("#stepBegin:"); Serial.println(step); }
  processMQTT = 1;
  switch (step) {
    case 3:
      requestTimerLimit = MAX_REQUEST_TIMER;
      strcpy_P(buffer, (char *)pgm_read_ptr(&(AT_beginMQTT_cmd[step])));
      snprintf(bufout, buffer_size, buffer, CLIENT_ID, SSLMODE);
      myserial->println(bufout);
      requestTimer = millis();
      if (debug) { Serial.print("launching:"); Serial.println(bufout); }
      break;
    case 4:
      requestTimerLimit = 120000;
      strcpy_P(buffer, (char *)pgm_read_ptr(&(AT_beginMQTT_cmd[step])));
      snprintf(bufout, buffer_size, buffer, SERVER_URL, PORT, LOGIN, PASSWD);
      myserial->println(bufout);
      requestTimer = millis();
      if (debug) { Serial.print("launching:"); Serial.println(bufout); }
      break;
    default:
      requestTimerLimit = MAX_REQUEST_TIMER;
      strcpy_P(buffer, (char *)pgm_read_ptr(&(AT_beginMQTT_cmd[step])));
      myserial->println(buffer);
      requestTimer = millis();
      if (debug) { Serial.print("launching:"); Serial.println(buffer); }
      break;
  }
  return true;
}

//prepare and format AT command for close steps
bool SIM7600MQTT::closeAtMQTT(byte step) {
  size_t buffer_size = 128;
  bool debug = 1;
  requestTimer = 0;

  if (step > 3) { return false; Serial.println(F("Err#overTAB")); }
  if (step < 3) {
    requestTimerLimit = 120000;
  } else {
    requestTimerLimit = MAX_REQUEST_TIMER;
  }

  char buffer[buffer_size];
  processMQTT = 1;
  if (debug) { Serial.print("StepClose:"); Serial.println(step); }
  strcpy_P(buffer, (char *)pgm_read_ptr(&(AT_closeMQTT_cmd[step])));
  myserial->println(buffer);
  requestTimer = millis();
  return true;
}

//prepare and format AT command for SSL steps
bool SIM7600MQTT::sslAtMQTT(byte step) {
  bool debug = 1;
  requestTimer = 0;
  requestTimerLimit = MAX_REQUEST_TIMER;
  size_t buffer_size = 128;
  if (step > 5) { return false; Serial.println(F("Err#overTAB")); }
  char buffer[buffer_size];
  processMQTT = 1;
  if (debug) { Serial.print("#stepSSL:"); Serial.println(step); }
  strcpy_P(buffer, (char *)pgm_read_ptr(&(AT_sslMQTT_cmd[step])));
  myserial->println(buffer);
  requestTimer = millis();
  return true;
}

//MQTT sending sequence trigger management
byte SIM7600MQTT::publishMQTT(const char *topic, char *payload) {
  bool debug = 0;
  //process status of current MQTT action #0 nothing running #1 running MQTT request #2 finished & success  #3 failed to process MQTT request  #4 Must relaunch process
  mqttTopic = topic;
  mqttPayload = payload;
  if (debug) {
    Serial.println(F("--launch publishMQTT--"));
    Serial.print(F("#topic: "));
    Serial.println(mqttTopic);
    Serial.print(F("#payload: "));
    Serial.println(mqttPayload);
  }

  if (strlen(payload) == 0) {
    Serial.println(F("--empty payload!--"));
  } else if (networkready && (processMQTT == 0 || processMQTT == 2) && strlen(payload) != 0) {
    //launching an MQTT sending process
    if (debug) { Serial.println(F("--init publish state--")); }
    resetStatus();
    resetStep();
    resetAtState();
    resetTimers();
    bufferIndex = 0;
    processMQTT = 1;  //#processMQTT running
    beginMQTT = true;
    executeStep = true;
    timeoutTimer = millis();
  } else if (networkready && processMQTT == 1 && timeoutTimer != 0 && (millis() - timeoutTimer) > TIMEOUT_SESSION) {
    //Stuck in an incomplete sending state (may be due to insufficient time between two publishes)
    Serial.println(F("--#publishMQTT error process running--"));
    if (debug) { Serial.print("##"); printSimState(); }
    processMQTT = 3;
  }
  if (debug) { Serial.print("###"); printSimState(); }
  return processMQTT;
}

//send msg payload to topic
bool SIM7600MQTT::sendMsgMQTT(const char *topic, char *payload, byte step) {
  if (msgMQTT) {
    int topicLength = strlen(topic);
    int payloadLength = strlen(payload);
    // Serial.print("stepMSG:");Serial.println(step);
    processMQTT = 1;
    requestTimer = 0;
    switch (step) {
      case 1:
        requestTimerLimit = MAX_REQUEST_TIMER;
        myserial->print(F("AT+CMQTTTOPIC=0,"));
        myserial->println(topicLength);
        requestTimer = millis();
        break;
      case 2:
        requestTimerLimit = MAX_REQUEST_TIMER;
        myserial->println(topic);
        requestTimer = millis();
        break;
      case 3:
        requestTimerLimit = MAX_REQUEST_TIMER;
        myserial->print(F("AT+CMQTTPAYLOAD=0,"));
        myserial->println(payloadLength);
        requestTimer = millis();
        break;
      case 4:
        requestTimerLimit = MAX_REQUEST_TIMER;
        myserial->println(payload);
        requestTimer = millis();
        break;
      default:
        //in case no valid step
        requestTimer = millis();
        break;
    }
  }
  return true;
}

//Launching AT commands for MQTT connection: parsing the previous command message and launching the next command
bool SIM7600MQTT::launchAtCmdMQTT() {
  bool debug = 0;
  // inconsistency check
  int incCpt = 0;
  if (sslMQTT) { incCpt++; }
  if (beginMQTT) { incCpt++; }
  if (closeMQTT) { incCpt++; }
  if (msgMQTT) { incCpt++; }
  if (soloMQTT) { incCpt++; }
  if (incCpt > 1) {
    Serial.println(F("incoherence!"));
    processMQTT = 3;
    return false;
  }

  //periodic command to launch when not busy
  if (networkready && processMQTT == 0){
    #if SYNC_NTP_DATETIME == 1
      unsigned long mylimit = GET_NET_TIME_PERIOD;
      DateTime* currentDateTime = get_gsm_datetime();
      if (currentDateTime == nullptr) {mylimit = 300000;} //shortens the delay if there is no time synchronization
      if(millis() - last_nettime_request > mylimit){
        resetAtState();
        soloMQTT = true;
        currentStepSoloMQTT = 0;
        periodicStep(currentStepSoloMQTT);
        last_nettime_request = millis();
        return true;
      }
    #endif
  }

  if (processMQTT == 2) {
    Serial.println(F("--message envoyé--"));
    //Note: This function works with buffer stack because loop processes the MQTT process between two calls to this function
    processMQTT = 0;
    return true;
  }

  //verification status process in progress
  if (processMQTT == 3) {
    if (retryTimer == 0) {
      Serial.println(F("--MQTT seq failed to process!--"));
      Serial.print(F("--Wait for : "));
      Serial.print(ERROR_TIMER_DELAY);
      Serial.println("ms --");
      retryTimer = millis();
      processMQTT = 3;  //to block executestep
      return false;
    } else if ((millis() - retryTimer) > ERROR_TIMER_DELAY) {
      Serial.println(F("--Try re-launch MQTT seq.--"));
      //TODO finir modif !
      resetBootState();
      //publishMQTT(mqttTopic, mqttPayload);
      return true;
    }
    return false;
  }

  if (processMQTT == 4) {  //Logical process and network management available...TODO
    if (retryTimer == 0) {
      Serial.println(F("--MQTT cmd failed to process!--"));
      retryTimer = millis();
    } else if ((millis() - retryTimer) > RETRY_TIMER_DELAY) {
      Serial.println(F("--Try re-launch MQTT cmd.--"));
      resetStatus();
      processMQTT = 1;
      executeStep = true;
      retryTimer = 0;
    }
    return true;
  }

  //parse common error code requiring relaunch
  if (errorCode == 14) {
    //client is busy
    processMQTT = 4;
    Serial.println(F("--client is busy, relaunch!--"));
    return true;
  }

  //received response SIM7600
  if (networkready && acqRespSIM == 2 && processMQTT == 1) {
    if (debug) { Serial.print("--launchAtCmdMQTT--"); printSimState(); }
    // ---- commands SSL ----
    if (sslMQTT) {
      if (errorCode > 0) { statusError = true; }
      //switch step beginMQTT
      if (statusOk && !statusError && currentStepSslMQTT == 4) {
        resetStatus();
        bufferIndex = 0;
        sslMQTT = false;
        beginMQTT = true;
        currentStepBeginMQTT++;
        executeStep = true;
      } else if (statusOk && !statusError && currentStepSslMQTT == 5) {
        resetStatus();
        bufferIndex = 0;
        sslMQTT = false;
        beginMQTT = true;
        currentStepBeginMQTT++;
        executeStep = true;
      }
      //launch AT cmd
      else if (statusOk && !statusError && currentStepSslMQTT < 6) {
        if (debug) { Serial.println("--lanch ssl step--"); }
        currentStepSslMQTT++;
        if (sslAtMQTT(currentStepSslMQTT) == false) {
          sslMQTT = false;
          beginMQTT = false;
          currentStepSslMQTT = 0;
          processMQTT = 3;
          return false;
        }
        resetStatus();
        bufferIndex = 0;
      } else {
        //error
        Serial.println(F("--#sslMQTT step error!--"));
        printSimState();
        statusOk = 0;
        statusError = 1;
        statusPlus = 0;
        processMQTT = 3;
        return false;
      }
      //TODO manage statusError
    } else if (beginMQTT) {
      //manage error cmd AT MQTT
      switch (errorCode) {
        case -1:  //no error code detected
          break;
        case 0:  //all is OK
          statusOk = 1;
          statusError = 0;
          break;
        case 1:  //failed
          statusOk = 0;
          statusError = 1;
          processMQTT = 3;
          return false;
          Serial.println(F("--E: Failed!--"));
          break;
        case 7:  //network open failed
          Serial.println(F("--E: Network open failed!--"));
          statusOk = 0;
          statusError = 1;
          processMQTT = 3;
          return false;
          break;
        case 9:  //network not opened
          Serial.println(F("--E: Network not opened!--"));
          statusOk = 0;
          statusError = 1;
          processMQTT = 3;
          return false;
          break;
        case 11:
          Serial.println(F("--E: no connection!--"));
          statusOk = 0;
          statusError = 1;
          processMQTT = 3;
          return false;
          break;
        case 23:  //network is opened => non-blocking next step but signs of a problem in the sequence
          Serial.println(F("--E: network is opened!--"));
          statusOk = 0;
          statusError = 1;
          processMQTT = 3;
          return false;
          break;
        case 19:  //client is used => non-blocking next step but signs of a problem in the sequence
          Serial.println(F("--E: client is used!--"));
          statusOk = 0;
          statusError = 1;
          processMQTT = 3;
          return false;
          break;
        case 26:  //socket closed by server => echec connection
          Serial.println(F("--E: Server refused cnx!--"));
          statusOk = 0;
          statusError = 1;
          processMQTT = 3;
          return false;
          break;
        case 32:  //handshake error
          Serial.println(F("--E: Hanshake fail!--"));
          //close and finish
          statusOk = 0;
          statusError = 1;
          processMQTT = 3;
          return false;
          break;
        case 99:  //personal error code no relaunch mqtt seq
          Serial.println(F("--E: Perso MQTT 99!--"));
          statusOk = 0;
          statusError = 1;
          processMQTT = 3;
          return false;
          break;
        default:
          Serial.println(F("--E: ErrorMQTT!--"));
          statusOk = 0;
          statusError = 1;
          processMQTT = 3;
          return false;
          break;
      }
      //switch step ssl if required
      if (statusOk && !statusError && currentStepBeginMQTT == 1 && SSLMODE) {
        resetStatus();
        sslMQTT = true;
        beginMQTT = false;
        closeMQTT = false;
        currentStepSslMQTT = 0;
        executeStep = true;
        if (debug) { Serial.println(F("Preparing SSL")); }
        return true;
      }
      //switch step ssl if required
      if (statusOk && !statusError && currentStepBeginMQTT == 3 && SSLMODE) {
        beginMQTT = false;
        sslMQTT = true;
        currentStepSslMQTT++;
        executeStep = true;
        resetStatus();
        bufferIndex = 0;
        if (debug) { Serial.print("--switch-- "); printSimState(); }
      }
      //switch step topic & payload
      if (statusOk && !statusError && currentStepBeginMQTT == 4) {
        beginMQTT = false;
        msgMQTT = true;
        currentStepMsgMQTT++;
        executeStep = true;
        resetStatus();
        bufferIndex = 0;
        if (debug) { Serial.print(F("--init payload-- ")); printSimState(); }
      }
      //launch AT cmd
      if (statusOk == true && !statusError && beginMQTT) {
        // Serial.println("--lanch begin step--");
        currentStepBeginMQTT++;
        if (currentStepBeginMQTT <= 4) {
          if (beginAtMQTT(currentStepBeginMQTT) == false) {
            beginMQTT = false;
            closeMQTT = false;
            currentStepBeginMQTT = 0;
            Serial.println(F("--Failed #BeginAtMQTT--"));
            //TODO management error
            statusOk = 0;
            statusError = 1;
            processMQTT = 3;
            return false;
          }
          resetStatus();
          bufferIndex = 0;
        } else {
          resetStatus();
          statusOk = 0;
          statusError = 1;
          processMQTT = 3;
          return false;
          Serial.println(F("--#BeginAtMQTT step error!--"));
        }
      }
    } else if (msgMQTT) {
      if (debug) { Serial.print("--access msgMQTT--"); printSimState(); }
      // launch topic and payload command
      if (statusOk && !statusError && currentStepMsgMQTT < 4) {
        currentStepMsgMQTT++;
        sendMsgMQTT(mqttTopic, mqttPayload, currentStepMsgMQTT);
        resetStatus();
        bufferIndex = 0;
      } else if (statusOk && !statusError && currentStepMsgMQTT == 4) {
        //switch to pub & close steps
        if (debug) { Serial.println(F("--Switch to publish step--")); }
        msgMQTT = false;
        beginMQTT = false;
        closeMQTT = true;
        executeStep = true;
        resetStatus();
        bufferIndex = 0;
      } else {
        //erreur
        Serial.println(F("--#msgMQTT step error!--"));
        statusOk = 0;
        statusError = 1;
        processMQTT = 3;
      }
    } else if (closeMQTT) {
      //TODO end of sequence
      if (statusOk && !statusError && currentStepCloseMQTT == 3) {
        resetStatus();
        resetAtState();
        resetStep();
        resetTimers();
        bufferIndex = 0;
        processMQTT = 2;
        if (debug) { Serial.println(F("--MQTT seq done--")); }
        return true;
      }
      //launch At cmd
      else if (statusOk && !statusError && currentStepCloseMQTT < 3) {
        currentStepCloseMQTT++;
        if (closeAtMQTT(currentStepCloseMQTT) == false) {
          resetAtState();
          resetStep();
          processMQTT = 3;
          return false;
        }
        resetStatus();
        bufferIndex = 0;
      } else {
        //error
        Serial.println(F("--#closeMQTT step error!--"));
        statusOk = 0;
        statusError = 1;
        processMQTT = 3;
        return false;
      }
    } else if (soloMQTT) {
      if(currentStepSoloMQTT == 0){processMQTT = 0; resetStatus(); soloMQTT = false;}
      if(currentStepSoloMQTT == 1){processMQTT = 0; resetStatus(); soloMQTT = false;}
    }
    //force relaunch step without previous response
  } else if (SIM7600ready && SIMready && executeStep) {
    resetStatus();
    bufferIndex = 0;
    if (debug) { Serial.print("--Force launchAtCmdMQTT-- "); printSimState(); }
    if (sslMQTT) {
      if (sslAtMQTT(currentStepSslMQTT) == false) {
        resetAtState();
        resetStep();
        Serial.println(F("-failed sslAtMQTT-"));
        processMQTT = 3;
      }
      if (debug) { Serial.println("--executestep ssl--"); }
      executeStep = false;
    }
    if (beginMQTT) {
      if (beginAtMQTT(currentStepBeginMQTT) == false) {
        resetAtState();
        resetStep();
        Serial.println(F("--failed exec #beginMQTT--"));
        processMQTT = 3;
      }
      executeStep = false;
    }
    if (msgMQTT) {
      // Serial.println("--executestep msg--");
      if (sendMsgMQTT(mqttTopic, mqttPayload, currentStepMsgMQTT) == false) {
        resetAtState();
        resetStep();
        closeMQTT = true;
        Serial.println(F("--failed exec msgMQTT--"));
        processMQTT = 3;
      }
      executeStep = false;
    }
    if (closeMQTT) {
      if (debug) { Serial.println("--executestep close--"); }
      if (closeAtMQTT(currentStepCloseMQTT) == false) {
        resetAtState();
        resetStep();
        Serial.println(F("--failed exec #closeAtMQTT--"));
        processMQTT = 3;
      }
      executeStep = false;
    }
  }
  return true;
}

//Launch of AT orders for global process
//warning : order of following function is essential
void SIM7600MQTT::lib_MQTT() {
  //-- listen messages SIM7600
  listenSerialSIM7600();
  dialogCheck();
  //-- state management --
  startupSIM();
  checkSIM();
  networkSearch();
  //sim7600mqtt.publishAutoMQTT(interpublish);  //For test
  launchAtCmdMQTT();
}
