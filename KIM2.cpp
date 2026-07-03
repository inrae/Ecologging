#include "KIM2.h"

// CONSTRUCTOR
KIM2::KIM2(Stream& serial, uint8_t powerPin, uint8_t relayPin) {
  _serial = &serial;
  _powerPin = powerPin;
  _relayPin = relayPin;
}
// INITIALIZATION
void KIM2::initKim2(const char* rconfToken) {

  _rconfToken = rconfToken;

  pinMode(_powerPin, OUTPUT);
  pinMode(_relayPin, OUTPUT);
  digitalWrite(_relayPin, LOW); // active relay (NC configuration)

  _status = KIM2_IDLE;          // job status
  _mode = KIM2_INIT;            // standby mode INIT
  _failCount = 0;               // reset errot counter

  // Buffer initialization
  _bufferIndex = 0;
  _buffer[0] = '\0';

  // reset + boot
  _resetState = RESET_IDLE;     // no reset pending
  
}

// Power on module
void KIM2::powerOn() {
  Serial.println("[KIM2] POWER ON");
  digitalWrite(_powerPin, HIGH);

  _mode = KIM2_BOOTING;
  _startTime = millis();         // set starting timer

}

// run INIT sequence
// (set to 0 and launch message)
void KIM2::start() {
  Serial.println(F("[KIM2] START INIT"));
  _currentIndex = 0;
  _failCount = 0;
  sendCurrentCommand();
}


// send command
void KIM2::sendCurrentCommand() {

 // if all cmd have been sent
  if (_currentIndex >= MAX_CMD) {
    Serial.println(F("[KIM2] INIT FINISHED -> RUN MODE"));

    _mode = KIM2_RUN;
    _status = KIM2_IDLE;          // ready to send datas
    return;
  }

  flushInput();                   // empty serial input buffer

  //_serial->println(_commands[_currentIndex]);
  if (_currentIndex == 0) {
    _serial->println("AT+PING=?");
  } 
  else if (_currentIndex == 1) {
    _serial->print(F("AT+RCONF="));
    _serial->println(_rconfToken); // Dynamic inject Key 
  } 
  else if (_currentIndex == 2) {
    _serial->println(F("AT+KMAC=1"));
  }

  _status = KIM2_BUSY;
  _startTime = millis();
  _timeout = 3000;                // timeout AT response
}

// Send PAYLOAD (MODE RUN)
bool KIM2::sendPayload(const char* payload) {
  // block sending in INIT mode
  if (_mode != KIM2_RUN){
    Serial.println(F("[KIM2] SEND BLOCKED -> NOT RUN MODE"));
    return false;
  }
  // block if sending already pending
  if (_status == KIM2_BUSY){    
    Serial.println(F("[KIM2] SEND BLOCKED -> BUSY"));
    return false;                 // not allowed if already running
  }

  flushInput();

  _serial->print(F("AT+TX="));    // AT prefix
  _serial->print(payload);        // add payload
  _serial->println(F(",00"));     // end cmd     

  // set state & timer
  _status = KIM2_BUSY;
  _startTime = millis();
  _timeout = 5000; 

  return true;
}

// UPDATE PRINCIPAL 
void KIM2::update() {

  // 1. RESET management
  switch (_resetState) {
    // switch off module
    case RESET_OFF:
      Serial.println(F("[KIM2] RESET -> POWER OFF"));
      digitalWrite(_relayPin, HIGH); 
      _startTime = millis();        
      _resetState = RESET_WAIT_OFF; 
      return;

    case RESET_WAIT_OFF:
      // wait 3 seconds
      if (millis() - _startTime >= 3000) {
        _resetState = RESET_ON;       
      }
      return;

    case RESET_ON:
      // switch on module
      digitalWrite(_relayPin, LOW); 
      _startTime = millis();         
      _resetState = RESET_WAIT_ON;   
      return;

    case RESET_WAIT_ON:
      // Wait 8 seconds before reboot
      if (millis() - _startTime >= 8000) { 
        Serial.println(F("[KIM2] RESET COMPLETE"));
        flushInput();            
        _resetState = RESET_IDLE; 
        _mode = KIM2_INIT;       
        start();                 
      }
      return;

    default:
      break;
  }


  // 2. Mode management
  switch (_mode) {
    case KIM2_BOOTING:
      if (millis() - _startTime >= 8000) {
        Serial.println(F("[KIM2] BOOT FINISHED"));
        flushInput();             // clean serial datas (AT+FW)
        _mode = KIM2_INIT;        // switch state
        start();                  // launch init session commands
      }
      break;
    case KIM2_INIT:
      readSerial();
     // TIMEOUT IN INIT : pause between request
      if (_status == KIM2_BUSY && (millis() - _startTime > _timeout)) {
        Serial.println(F("[KIM2] TIMEOUT IN INIT"));
        _failCount++;
        
        if (_failCount >= 3) {
          Serial.println(F("[KIM2] TOO MANY FAILS -> RESET"));
          _failCount = 0;
          hardwareReset(); 
        } else {
          _status = KIM2_TIMEOUT;
          _startTime = millis();
          _timeout = 1000;
        }
      }
      //pause & automatic retry
      if (_status == KIM2_ERROR || _status == KIM2_TIMEOUT) {
        if (millis() - _startTime > _timeout) {
          Serial.println(F("[KIM2] -> RETRYING INIT COMMAND"));
          sendCurrentCommand();
        }
      }
      break;
    case KIM2_RUN:
      readSerial();               // read received datas
      // TIMEOUT
      if (_status == KIM2_BUSY && millis() - _startTime > _timeout) {
        Serial.println(F("[KIM2] TIMEOUT"));
        _failCount++;
        Serial.print(F("[KIM2] FAIL COUNT = "));Serial.println(_failCount);
        
        if (_failCount >= 3) {
          Serial.println(F("[KIM2] TOO MANY FAILS -> RESET"));
          _failCount = 0;
          hardwareReset();        // reset with relay
        } else {
           _status = KIM2_TIMEOUT;
        }
      }
      break;
  }
}

// read module
void KIM2::readSerial() {
  while (_serial->available()) {
    char c = _serial->read();
    // check EOF character
    if (c == '\n') {
      _buffer[_bufferIndex] = '\0';
      parseLine(_buffer);
      _bufferIndex = 0;         // reinit buffer for next line
    }
    // ignore \r
    else if (c != '\r') {
      //add character
      if (_bufferIndex < sizeof(_buffer) - 1) {
        _buffer[_bufferIndex++] = c;
      }
    }
  }
}

// Parse response
void KIM2::parseLine(const char* line) {
  if (strlen(line) == 0) return;

  Serial.print(F("[KIM2 RX] "));
  Serial.println(line);

  // Ignore boot message
  if (strncmp(line, "AT+FW", 5) == 0) return;

  // MODE INITphase : AT+PING / AT+RCONF / AT+KMAC
  if (_mode == KIM2_INIT) {
    // if line contain "+OK", command succeeded
    if (strstr(line, "+OK") != NULL) {
      _failCount = 0;
      // next command
      _currentIndex++;
      sendCurrentCommand();
      return;
    }
  }

  // MODE RUN
  if (_mode == KIM2_RUN) {
    // if line starts with "+TX=", send payload succeeded
    if (strncmp(line, "+TX=", 4) == 0) {
      Serial.println(F("[KIM2] TX OK"));
      // update job status
      _status = KIM2_OK;
      _failCount = 0;
      return;
    }
  }

  // Line contain "ERROR"
  if (strstr(line, "ERROR") != NULL) {
    Serial.println(F("[KIM2] ERROR"));
    // Update job status
    _failCount++;

    if (_failCount >= 3) {
      Serial.println(F("[KIM2] TOO MANY ERRORS -> RESET"));
      _failCount = 0;
      hardwareReset();
    } else {
      _status = KIM2_ERROR;
      if (_mode == KIM2_INIT) {
        _startTime = millis();
        _timeout = 1000;
      }
    }
  }
}

// Empty input serial buffer
void KIM2::flushInput() {
  while (_serial->available()) {
    _serial->read();
  }
}

// RESET HARDWARE with relay
void KIM2::hardwareReset() {
  Serial.println(F("[KIM2] HARD RESET"));
  _status = KIM2_IDLE;
  // switch to reset state
  _resetState = RESET_OFF;
}

// GET Jpb status
// return current state (IDLE, BUSY, OK, ERROR, TIMEOUT)
KIM2Status KIM2::getStatus() const {
  return _status;
}
