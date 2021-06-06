#include <Arduino.h>
#include <ArduinoUnitTests.h>

#define DEBUG 1
#include "DateTime_TC.h"
#include "EEPROM_TC.h"
#include "PHControl.h"
#include "PHProbe.h"
#include "SD_TC.h"
#include "TC_util.h"
#include "TankControllerLib.h"
#include "TempProbe_TC.h"
#include "TemperatureControl.h"

const uint16_t TEMP_PIN = 47;
const uint16_t PH_PIN = 49;

GodmodeState *state = GODMODE();
TankControllerLib *pTC = TankControllerLib::instance();
TempProbe_TC *tempProbe = TempProbe_TC::instance();
TemperatureControl *tempControl = TemperatureControl::instance();
PHProbe *pPHProbe = PHProbe::instance();
PHControl *pPHControl = PHControl::instance();
SD_TC *sd = SD_TC::instance();

unittest_setup() {
  // reset time so offset is consistent
  state->resetClock();

  EEPROM_TC::instance()->setTankID(42);

  // set temperature
  tempProbe->setTemperature(16.75);
  tempProbe->setCorrection(0.0);
  for (size_t i = 0; i < 100; ++i) {
    delay(1000);
    tempProbe->getRunningAverage();
  }

  // set target temperature
  tempControl->setTargetTemperature(16.25);

  // set pH
  state->serialPort[1].dataIn = "7.125\r";  // the queue of data waiting to be read
  pTC->serialEvent1();                      // fake interrupt

  // set target pH
  pPHControl->enablePID(false);  // Stay on continually if needed
  pPHControl->setTargetPh(7.325);

  // set Kp, Ki, and Kd
  EEPROM_TC::instance()->setKP(123456.7);
  EEPROM_TC::instance()->setKI(12345.6);
  EEPROM_TC::instance()->setKD(1234.5);

  // clear SD card
  SD.removeAll();
}

unittest_teardown() {
  SD.removeAll();
}

unittest(basicOperation) {
  // verify startup state, including that solonoids are off
  delay(1000);
  float avgTemp = static_cast<int16_t>((tempProbe->getRunningAverage() * 100.0 + 0.5)) / 100.0;
  assertEqual(16.75, avgTemp);
  assertEqual(7.125, pPHProbe->getPh());
  pTC->loop();
  assertEqual(TURN_SOLENOID_OFF, state->digitalPin[TEMP_PIN]);  // solenoid off
  assertEqual(TURN_SOLENOID_OFF, state->digitalPin[PH_PIN]);    // solenoid off

  // change targets
  tempControl->setTargetTemperature(17.25);
  pPHControl->setTargetPh(6.875);

  // verify that solonoids are on
  delay(1000);
  pTC->loop();
  delay(1000);
  pTC->loop();
  assertEqual(TURN_SOLENOID_ON, state->digitalPin[TEMP_PIN]);  // solenoid on
  assertEqual(TURN_SOLENOID_ON, state->digitalPin[PH_PIN]);    // solenoid on

  // reset targets
  tempControl->setTargetTemperature(16.25);
  pPHControl->setTargetPh(7.375);

  // verify that solonoids are off
  delay(1000);
  pTC->loop();
  delay(1000);
  pTC->loop();
  assertEqual(TURN_SOLENOID_OFF, state->digitalPin[TEMP_PIN]);
  assertEqual(TURN_SOLENOID_OFF, state->digitalPin[PH_PIN]);
}

unittest(storeDataToSD) {
  // set date/time (so we can confirm data)
  DateTime_TC dt(2021, 4, 27, 14, 24, 50);
  dt.setAsCurrent();
  for (size_t i = 0; i < 4; ++i) {
    delay(500);
    pTC->loop();
  }
  /*
  time,tankid,temp,temp setpoint,pH,pH setpoint,onTime,Kp,Ki,Kd
04/27/2021 14:24:50,  42, 16.75, 16.25, 7.125, 6.825,    0, 123456.7,  12345.6,   1234.5
04/27/2021 14:24:51,  42, 16.75, 16.25, 7.125, 6.825,    0, 123456.7,  12345.6,   1234.5
04/27/2021 14:24:52,  42, 16.75, 16.25, 7.125, 6.825,    0, 123456.7,  12345.6,   1234.5
  */
  File file = SD.open("20210427.csv");
  char data[4096];
  file.read(data, file.size());
  data[file.size()] = '\0';
  COUT(data);
  String contents(data), line;
  int16_t i = contents.indexOf('\n');
  line = contents.substring(0, i);
  assertEqual("time,tankid,temp,temp setpoint,pH,pH setpoint,onTime,Kp,Ki,Kd", line.c_str());
  contents = contents.substring(i + 1);
  i = contents.indexOf('\n');
  line = contents.substring(0, i);
  String expected("04/27/2021 14:24:50,  42, 16.75, 16.25, 7.125, 6.825,    0, 123456.7,  12345.6,   1234.5");
  COUT("expectedSize = " << expected.length() << "; actualSize = " << line.length());
  assertEqual(expected, line);
  contents = contents.substring(i + 1);
  i = contents.indexOf('\n');
  line = contents.substring(0, i);
  expected = String("04/27/2021 14:24:51,  42, 16.75, 16.25, 7.125, 6.825,    0, 123456.7,  12345.6,   1234.5");
  COUT("expectedSize = " << expected.length() << "; actualSize = " << line.length());
  assertEqual(expected, line);
  COUT(data);
  file.close();
}

unittest_main()
