/*
SIM7600MQTT.h - Library to use SIM7600 module to push MQTT(S) messages
Created by Philippe CHAUMEIL, July 5, 2024.
Modified by Frederic Raspail July 2024
Modified by Philippe CHAUMEIL March 2026
Release into the public domain.
Principle:
This library only push MQTT(S) message. Subscription is not supported.
SIM7600 has a default baudrate set to 115200 but with this speed communication is not clean
At 57600 bps the serial communication is stable. A function will set serial communication to this speed. No modification is required.
SIM7600 has hardware pin connector which can be set to link RX & TW to pins 7 & 8
Those pin can be used for software serial on arduino uno but not for arduino mega which must use hardware serial on pin 18 & 19
the SIM7600MQTTparam.h has a line to comment for use with arduinon Uno.
All the processes to manage SIM7600 must be asynchronous.
This library do not use delays or sequencial commands but states variables to activate or not steps required to send an MQTT message.
First step is to wait for module start, Second is to wait for network connection
The principle is to have a pool of fonction in charge of listening serial response of SIM7600 and parse the response,
A fonction in charge of determining which state is active and which is the next command to send,
and finaly a pool of function in charge of sending AT commands. 
*/

#ifndef SIM7600MQTT_h
#define SIM7600MQTT_h

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <HardwareSerial.h>
#include <avr/pgmspace.h>
#include "SIM7600MQTTparam.h"
#include "RTClib.h"

//parameters
#define NET_TEST_TIMER_DELTA 10000   //Delay between sim7600 request to test network
#define MAX_REQUEST_TIMER 5000       //maximum delay to obtain response to request (Must be lower than NET_TEST_TIMER_DELTA ...?)
#define MAX_SERIAL_TIMER 200         //maximum delay to wait serial buffer
#define ERROR_TIMER_DELAY 120000     //delay before re-launching mqtt sequence after error set to max SIM7600 response delay
#define RETRY_TIMER_DELAY 3000       //delay to retry to launch last mqtt step following specific error
#define TIMEOUT_SESSION 300000       //5min
#define CHECK_NETWORK 960000         //16min # 900000 periodic relaunch to check network
#define GET_NET_TIME_PERIOD 1200000  // 20 min get time from network (NTP method)
#define HARD_RESET_INTERVAL 1800000  // 30 min min interval between successive hard reset

//define settings AT cmd
const char hello[] PROGMEM = "AT";
const char ntphost[] PROGMEM = "AT+CNTP=\"fr.pool.ntp.org\",0";
#if SYNC_NTP_DATETIME == 1
const char ctzu[] PROGMEM = "AT+CTZU=0";
const char ctzr[] PROGMEM = "AT+CTZR=0";
#else
const char ctzu[] PROGMEM = "AT+CTZU=1";
const char ctzr[] PROGMEM = "AT+CTZR=1";
#endif

//define network & strat connection AT cmd
const char netstatus[] PROGMEM = "AT+CREG?";
const char mqttclock[] PROGMEM = "AT+CCLK?";  // #retrieve Real Time Clock of the module
const char ntprequest[] PROGMEM = "AT+CNTP";
const char mqttstart[] PROGMEM = "AT+CMQTTSTART";
const char mqttaccq[] PROGMEM = "AT+CMQTTACCQ=0,\"%s\",%d";                                 //param required
const char mqttconnect[] PROGMEM = "AT+CMQTTCONNECT=0,\"tcp://%s:%s\",90,0,\"%s\",\"%s\"";  //param required

//define msg AT command {NOT USED !!! because interactive input, direct print to serial in function}
//const char mqtttopicT[] PROGMEM ="AT+CMQTTTOPIC=0,%d"; //param required
//const char mqttpayloadT[] PROGMEM ="AT+CMQTTPAYLOAD=0,%d";

//define pub & close AT cmd
const char mqttpub[] PROGMEM = "AT+CMQTTPUB=0,2,60,1";
const char mqttdisc[] PROGMEM = "AT+CMQTTDISC=0,60";
const char mqttrel[] PROGMEM = "AT+CMQTTREL=0";
const char mqttstop[] PROGMEM = "AT+CMQTTSTOP";

//define SSL AT cmd
const char sslver[] PROGMEM = "AT+CSSLCFG=\"sslversion\",0,4";                      // 4 # all protocols
const char sslauth[] PROGMEM = "AT+CSSLCFG=\"authmode\",0,2";                       // 2 # server and client authentication
const char cacfg[] PROGMEM = "AT+CSSLCFG=\"cacert\",0,\"cacert.pem\"";              //uploaded previously using AT FTP commands
const char clientcfg[] PROGMEM = "AT+CSSLCFG=\"clientcert\",0,\"clientcert.pem\"";  //uploaded previously using AT FTP commands
const char keycfg[] PROGMEM = "AT+CSSLCFG=\"clientkey\",0,\"clientkey.pem\"";       //uploaded previously using AT FTP commands

const char sslcfg[] PROGMEM = "AT+CMQTTSSLCFG=0,0";  //set SSL context

//define array of AT command
const char* const AT_periodic_cmd[] PROGMEM = { ntprequest, mqttclock };
const char* const AT_settings_cmd[] PROGMEM = { hello, ntphost, ctzu, ctzr };
const char* const AT_beginMQTT_cmd[] PROGMEM = { netstatus, mqttclock, mqttstart, mqttaccq, mqttconnect };
const char* const AT_closeMQTT_cmd[] PROGMEM = { mqttpub, mqttdisc, mqttrel, mqttstop };
const char* const AT_sslMQTT_cmd[] PROGMEM = { sslver, sslauth, cacfg, clientcfg, keycfg, sslcfg };

// Complete MQTT sequence order
// AT_beginMQTT_cmd[0-1] ; {AT_sslMQTT_cmd[0-4]} ; AT_beginMQTT_cmd[2-3] ; {AT_sslMQTT_cmd[5]} ; AT_beginMQTT_cmd[4] ; AT_closeMQTT_cmd[*]

class SIM7600MQTT {
  //####################### PUBLIC SECTION #############################
public:
  SIM7600MQTT();

  void initSIM7600();
  void hard_reset_SIM7600();
  DateTime* get_gsm_datetime();
  int get_utc_offset();
  byte publishMQTT(const char* topic, char* payload);
  byte get_status();
  void lib_MQTT();

  //######################### PRIVATE SECTION ##########################
private:

  //topic & payload
  const char* mqttTopic;
  char* mqttPayload;

//serial variables
#define SERIAL_BUFFER_SIZE 250
  char serialBuffer[SERIAL_BUFFER_SIZE] = "\0";
  int bufferIndex = 0;

  HardwareSerial* myserial;

//--- SIM7600 variables ---
#define NUM_STEP 20
  bool SIM7600ready = 0;  //module status active
  bool SIMready = 0;      //SIM card active
  bool networkready = 0;  //GSM network ready
  bool beginMQTT = 0;     //init command before message
  bool closeMQTT = 0;     //close command after message
  bool sslMQTT = 0;       //init command SSL
  bool msgMQTT = 0;       //command to publish message
  bool soloMQTT = 0;      //individual command

  byte acqRespSIM = 0;   //status reponse of the module #0 waiting for response #1 aquisition in progress #2 full message received
  bool statusOk = 0;     //status OK for module i.e. module returned OK or similar response
  bool statusError = 0;  //status error for module
  bool statusPlus = 0;   //status of the response receipt +...
  int errorCode = -1;    //error code for module
  byte currentStepSettings = 0;
  byte currentStepBeginMQTT = 0;
  byte currentStepCloseMQTT = 0;
  byte currentStepSslMQTT = 0;
  byte currentStepMsgMQTT = 0;
  byte currentStepSoloMQTT = 0;
  byte executeStep = 0;     //force launch step
  bool onlyGetGsmDate = 0;  //to force retrieve gsm date after network registration and stop after

  byte processMQTT = 0;  //process status of current global MQTT action #0 nothing running #1 running MQTT request #2 finished & success #3 failed to process MQTT request #4 Must relaunch process

  byte waitingTry = 0;  // Try number dialog with module

  //____ timer ____
  unsigned long netTestTimer = 0;                       //timer for network connection test
  unsigned long serialTimer = 0;                        //timer to account for serial buffer delay
  unsigned long requestTimer = 0;                       //request sending timer
  unsigned long requestTimerLimit = MAX_REQUEST_TIMER;  //request time limit
  unsigned long retryTimer = 0;                         //recovery timer on failure
  unsigned long timeoutTimer = 0;                       //Timeout in processMQTT running state
  unsigned long checkNetwork = 0;                       //timer to periodic check network and update datetime
  unsigned long last_nettime_request = 0;               //timer to follow last time request on network (NTP)
  unsigned long lastPowerOnTimer = 0;                   //timer to log last power on
  unsigned long intervalPubTimer = 0;                   //timer for automatic publishing

  //___ define RTC software ___
  RTC_Millis rtc_sim7600;
  DateTime* gsm_datetime = nullptr;
  DateTime defaultDate = DateTime("2080-01-01T00:00:00");
  int UTC_offset = DEFAULT_TZ;

  void listenSerialSIM7600();
  bool dialogCheck();
  bool allow_powerOFF();
  void startupSIM();
  void checkSIM();
  void networkSearch();
  bool launchAtCmdMQTT();
  void printSimState();
  void setSerialSpeed();
  void resetStatus();
  void resetStep();
  void resetAtState();
  void resetTimers();
  void resetCnx();
  void resetBootState();
  void resetSearchNetState();
  void scanResponse(char* buffer);
  bool settingsStep(byte step);
  bool periodicStep(byte step);
  bool beginAtMQTT(byte step);
  bool closeAtMQTT(byte step);
  bool sslAtMQTT(byte step);
  bool sendMsgMQTT(const char* topic, char* payload, byte step);
};

#endif  //SIM7600_h