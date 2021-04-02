#include "Devices/PHProbe.h"

#include "Devices/Serial_TC.h"
#include "TankControllerLib.h"

//  class instance variables
/**
 * static variable for singleton
 */
PHProbe *PHProbe::_instance = nullptr;

//  class methods
/**
 * static member function to return singleton
 */
PHProbe *PHProbe::instance() {
  if (!_instance) {
    _instance = new PHProbe();
  }
  return _instance;
}

//  instance methods
/**
 * constructor (private so clients use the singleton)
 */
PHProbe::PHProbe() {
  Serial1.print(F("*OK,0\r"));  // Turn off the returning of OK after command to EZO pH
  Serial1.print(F("C,1\r"));    // Reset pH stamp to continuous measurement: once per second
}

void PHProbe::clearCalibration() {
  Serial1.print("Cal,clear\r");  // send that string to the Atlas Scientific product
}

void PHProbe::sendSlopeRequest() {
  Serial1.print(F("Slope,?\r"));  // Sending request for Calibration Slope
}

String PHProbe::getSlope() {
  // for example "?Slope,99.7,100.3, -0.89\r"
  if(slopeResponse.length() < 10) {
    return String("");
  }
  String slope = slopeResponse.substring(7);
  slope.remove((slope.length() - 1));
  // output to log
  Serial_TC *serial = Serial_TC::instance();
  serial->print(F("Calibration Slope: "), false);
  serial->print(slope);
  serial->println();
  return slope;
}

void PHProbe::onePointCalibration(double midpoint) {
  String fullCommand;
  fullCommand = "Cal,mid," + String(midpoint, 3) + "\r";
  Serial1.print(fullCommand);  // send that string to the Atlas Scientific product
}

/**
 * interrupt handler for data arriving from probe
 */
void PHProbe::serialEvent1() {
  while (Serial1.available() > 0) {               // if we see that the Atlas Scientific product has sent a character
    Serial_TC *serial = Serial_TC::instance();
    String string = Serial1.readStringUntil(13);  // read the string until we see a <CR>
    serial->print(F("Serial1 = "), false);
    serial->print(string, true);
    if (string.length() > 0) {
      if (isdigit(string[0])) {  // if the first character in the string is a digit
        // convert the string to a floating point number so it can be evaluated by the Arduino
        value = string.toFloat();
      } else if (string[0] == '?') {  // answer to a previous query
        if (string.length() > 7 && string.substring(0, 7) == "?Slope,") {
          // for example "?Slope,99.7,100.3, -0.89\r"
          slopeResponse = string;
        }
      }
    }
  }
}

// "pH decreases with increase in temperature. But this does not mean that
//  water becomes more acidic at higher temperatures."
// https://www.westlab.com/blog/2017/11/15/how-does-temperature-affect-ph
void PHProbe::setTemperatureCompensation(double temperature) {
  const String PARTIAL_COMMAND = "T,";
  String fullCommand;
  if (temperature > 0 && temperature < 100) {
    fullCommand = PARTIAL_COMMAND + String(temperature, 2) + "\r";
  } else {
    fullCommand = PARTIAL_COMMAND + "20\r";
  }
  Serial.println(fullCommand);
  Serial1.print(fullCommand);  // send that string to the Atlas Scientific product
}

void PHProbe::twoPointCalibration(double lowpoint, double midpoint) {
  String fullCommand;
  // do mid first because it clears low
  fullCommand = "Cal,mid," + String(midpoint, 3) + "\r";
  Serial1.print(fullCommand);  // send that string to the Atlas Scientific product
  fullCommand = "Cal,low," + String(lowpoint, 3) + "\r";
  Serial1.print(fullCommand);  // send that string to the Atlas Scientific product
}
