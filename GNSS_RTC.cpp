#include "Arduino.h"
#include "GNSS_RTC.h"

GNSS_RTC::GNSS_RTC(uint8_t relayPin, GsmDateTimeCallback gsmCallback, unsigned long syncIntervalMs, unsigned long retryIntervalMs)
    : _relayPin(relayPin),
      _getGsmDateTime(gsmCallback),
      _state(IDLE),
      _lastSyncTime(0),
      _syncInterval(syncIntervalMs),
      _retryInterval(retryIntervalMs),
      _nextInterval(syncIntervalMs),
      _stateTimer(0),
      _lastFixCheckTimer(0)
#if MOD_GPS
      , _gnss(&Wire, 0x20)
#endif
{}

bool GNSS_RTC::begin() {
    if (MOD_GPS) {
        pinMode(_relayPin, OUTPUT);
        Serial.print(F("[GNSS_RTC] Relay pin is : "));
        Serial.println(_relayPin);
        digitalWrite(_relayPin, LOW); // Relay off default
        Serial.println(F("[GNSS_RTC] Relay Low."));
    }

    if (!_rtc.begin()) {
        Serial.println(F("[GNSS_RTC] Error : DS3231 not found !"));
        return false;
    }

    if (_rtc.lostPower()) {
        Serial.println(F("[GNSS_RTC] RTC power loss; set to compilation date."));
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // Starting the initial synchronization based on the hardware configuration
    triggerSync();
    return true;
}

void GNSS_RTC::triggerSync() {
    if (MOD_GPS) {
        if (_state == IDLE) startGNSS();
    }
    else if (MOD_SIM7600){
        if (adjustWithGSM()) {
            _nextInterval = _syncInterval;
        } else {
            _nextInterval = _retryInterval;
        }
        _lastSyncTime = millis();
    }
    else {
        Serial.println(F("[GNSS_RTC] No external synchronization source enabled (MOD_GPS=0, MOD_SIM7600=0)."));
    }
}

void GNSS_RTC::update() {
#if MOD_GPS

    // Verification of frequency (e.g., once a week)
    if (_state == IDLE){
        if(millis() - _lastSyncTime >= _nextInterval){
            triggerSync();
            return;
        }
    }

    // Non-blocking state machine, active only if GPS support is compiled in.
    switch (_state) {
        case IDLE:
            break;

        case POWERING_ON:
            if (millis() - _stateTimer < WARMUP_DELAY_MS) {
                break;
            }

            if (millis() - _lastAttemptTimer >= 200) {
                _lastAttemptTimer = millis();
                if (_attemptCount == 0) {
                    Wire.begin();
                }
                Serial.println(F("[GNSS_RTC] GNSS begin attempt ..."));
                if (_gnss.begin()) {
                    _gnss.enablePower();
                    _gnss.setGnss(eGPS_BeiDou_GLONASS);
                    _gnss.setRgbOn();
                    
                    Serial.println(F("[GNSS_RTC] GNSS module detected on I2C and powered up. Searching for a fix..."));
                    _stateTimer = millis();
                    _lastFixCheckTimer = millis();
                    _attemptCount = 0;
                    _state = WAITING_FIX;
                    break;
                }
                
                _attemptCount++;
                if (_attemptCount >= 3) {
                    Serial.println(F("[GNSS_RTC] I2C initialization failure for the TEL0157."));
                    _nextInterval = _retryInterval;
                    _lastSyncTime = millis();
                    _attemptCount = 0;
                    stopGNSS();
                }
            }
            break;

        case WAITING_FIX:
            if (millis() - _lastFixCheckTimer >= FIX_CHECK_INTERVAL_MS) {
                _lastFixCheckTimer = millis();
                
                if (trySyncFromGNSS()) {
                    Serial.println(F("[GNSS_RTC] RTC synchronization successful via GNSS!"));
                    _nextInterval = _syncInterval;
                    _lastSyncTime = millis();
                    stopGNSS();
                    break;
                }
            }
            
            if (millis() - _stateTimer >= GNSS_TIMEOUT_MS) {
                Serial.print(F("[GNSS_RTC] GNSS FIX timeout reached. Satellites seen:"));
                Serial.println(_gnss.getNumSatUsed());

                bool gsmOk = false;
                // Fallback optionnel sur le GSM si le GPS échoue en timeout

                if (MOD_SIM7600) {
                    Serial.println(F("[GNSS_RTC] GPS timeout, attempting GSM fallback..."));
                    gsmOk = adjustWithGSM();
                }

                _lastSyncTime = millis();
                
                if (gsmOk) {
                    Serial.println(F("[GNSS_RTC] Fallback GSM success. Next sync in nominal interval."));
                    _nextInterval = _syncInterval;
                } else {
                    Serial.println(F("[GNSS_RTC] Full sync failure. Retrying in retry interval."));
                    _nextInterval = _retryInterval; // Échec total : reprogramme à 48h
                }
                
                stopGNSS();
            }
            break;

        case POWERING_OFF:
            stopGNSS();
            break;
    }
#endif
}

void GNSS_RTC::startGNSS() {
    Serial.println(F("[GNSS_RTC] Activating GNSS relay..."));
    digitalWrite(_relayPin, HIGH);
    Serial.println(F("[GNSS_RTC] Relay High."));
    _stateTimer = millis();
    _lastAttemptTimer = millis();
    _attemptCount = 0;
    _state = POWERING_ON;
}

void GNSS_RTC::stopGNSS() {
    Serial.println(F("[GNSS_RTC] Putting the GNSS relay into standby mode and powering it off."));
#if MOD_GPS
    _gnss.disablePower(); 
#endif
    digitalWrite(_relayPin, LOW);
    _state = IDLE;
    Serial.print(F("[GNSS_RTC] Relay pin "));
    Serial.println(_relayPin);
    Serial.println(F("[GNSS_RTC] Relay Low."));
}

bool GNSS_RTC::trySyncFromGNSS() {
#if MOD_GPS
    uint8_t satCount = _gnss.getNumSatUsed();

    // At least 4 satellites required for a fix.
    if (satCount >= 4) {
        sTim_t date = _gnss.getDate();
        sTim_t time = _gnss.getUTC();

        // Convert to a 4-digit year if the module returns 2 digits (e.g., 26 -> 2026)
        uint16_t fullYear = date.year;
        if (fullYear < 100) {
            fullYear += 2000;
        }

        // Strict verification of UTC date AND time consistency
        bool isDateValid = (fullYear >= 2024 && fullYear <= 2050) &&
                           (date.month >= 1 && date.month <= 12) &&
                           (date.date >= 1  && date.date <= 31);

        bool isTimeValid = (time.hour <= 23) &&
                           (time.minute <= 59) &&
                           (time.second <= 59);

        if (isDateValid && isTimeValid) {
            DateTime gnssTime(fullYear, date.month, date.date, time.hour, time.minute, time.second);
            _rtc.adjust(gnssTime);
            
            Serial.print(F("[GNSS_RTC] RTC successfully adjusted to UTC (Satellites used: "));
            Serial.print(satCount);
            Serial.println(F(")"));
            
            return true; // Synchronisation OK
        } else {
            Serial.print(F("[GNSS_RTC] Positional fix OK, but UTC time/date still incomplete: "));
            Serial.print(date.date); Serial.print(F("/"));
            Serial.print(date.month); Serial.print(F("/"));
            Serial.print(fullYear); Serial.print(F(" "));
            Serial.print(time.hour); Serial.print(F(":"));
            Serial.print(time.minute); Serial.print(F(":"));
            Serial.println(time.second);
        }
    }
#endif
    return false; // Not enough satellites yet or invalid data
}


bool GNSS_RTC::adjustWithGSM() {
#if MOD_SIM7600
    if (_getGsmDateTime != nullptr) {
        DateTime* gsmDateTime = _getGsmDateTime();
        if (gsmDateTime != nullptr) {
            _rtc.adjust(*gsmDateTime);
            Serial.println(F("[GNSS_RTC] GSM synchronization successful."));
            return true;
        }
    }
    Serial.println(F("[GNSS_RTC] GSM synchronization failed (null DateTime or callback not provided)."));
#endif
    return false;
}

DateTime GNSS_RTC::getNow() {
    return _rtc.now();
}

void GNSS_RTC::getFormattedDateTime(char* buffer, size_t size) {
    DateTime now = _rtc.now();
    snprintf(buffer, size, "%02d/%02d/%04d %02d:%02d:%02d ", 
             now.day(), now.month(), now.year(), 
             now.hour(), now.minute(), now.second());
}

void GNSS_RTC::printFormattedDateTime() {
    char buffer[25];
    getFormattedDateTime(buffer, sizeof(buffer));
    Serial.print(buffer);
}

