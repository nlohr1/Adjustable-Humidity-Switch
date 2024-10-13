//********************************************************************************
//Humidity-Relais with LCD-Display (20x4)
//********************************************************************************
//... (alle Kommentare und die ursprünglichen Komponentenbeschreibung) ...

//Libraries used:
#include "DHT.h"                 //https://github.com/adafruit/DHT-sensor-library
#include <SPI.h>                 //	(Arduino-internal) ../Arduino/hardware/arduino/avr/libraries/SPI
#include <Wire.h>                //(Arduino-internal) ../Arduino/libraries/Wire
#include <LiquidCrystal_I2C.h>   //https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library

//Pin-Connections:
//Pin-Name          Pin //Explanation
#define potiPin     A0  //Input for the 10k-Poti
#define switchPin   A1  //Input for the 3-state Switch (0V | bandpass | 5V)
#define MenuPin     2   //Input for the Menu-Button (Att.: With internal PullUp-Resistor activated, see Setup)
#define pwmPinForBL 3   //PWM Output to turn-On the Backlight-Transistor (dimming with 2s transition)
#define RelaisPin   4   //Output Pin for the Relay Transistor
#define DHTPin_a    5   //Sensor_a Input (default configuration!)
#define DHTPin_b    6   //Sensor_b Input (only if 2nd. Dew Point Sensor present for automatic difference-calculation)

#define DHTType DHT22
DHT dht(DHTPin_a, DHTType);

//-------------------------- Variables ---------------------------
bool relais = false;            //Relay status
int potiValue = 0;              //Potentiometer value
int potiValueOld = 0;           //Previous potentiometer value
volatile bool BUTTON_2 = true;  //Button status for D2
float temperature;                     //Measured temperature
float humidity;                 //Measured humidity
unsigned long previous;         //Timestamp for display refresh
int hysterese = 6;              //Hysteresis for relay control
char tempStr[6];                //String buffer for temperature
char humidityStr[6];            //String buffer for humidity

bool backlightOn = false;       //Backlight status (if dimmed or full brightness)
unsigned long backlightPrevious = 0;   //Timestamp for backlight control
unsigned int backlightTimeout = 60000; //60 seconds timeout for backlight, then 1s dimming to 10% intensity
unsigned int displayRefreshInterval = 1500; //1.5 seconds interval = display refresh time
bool isDimming = false;         //Define a flag to prevent double dimming while dimming

LiquidCrystal_I2C lcd(0x27, 20, 4); //LCD: Address 0x27, 20 characters, 4 lines


//~~~~~~~~~~~~~~~~~~~~~~~~~~ Setup ~~~~~~~~~~~~~~~~~~~~~~~~~~
void setup() {
  Serial.begin(9600);
  dht.begin();  //initialize Humidity-Sensor

//Att.: On begin all Pins are Input-Pins, so no need to declare Input-Pins here twice - if used as Input...
  pinMode(RelaisPin, OUTPUT);
  pinMode(MenuPin, INPUT_PULLUP);  //Pullup activated for MenuPin (D2)

  attachInterrupt(digitalPinToInterrupt(MenuPin), ButtonD2_LOW, FALLING); 

  lcd.begin();        //initialize LCD-Display
  lcd.backlight();    //Backlight "On"
  dimBacklight(true); //Start with full backlight (100%), see dim-function @ the end

  byte Grad[8] = {B00111, B00101, B00111, B0000, B00000, B00000, B00000, B00000}; //Create custom character "°" as array
  lcd.createChar(0, Grad); //Create special character °, to display temperature in Centigrade on the LCD

}

//~~~~~~~~~~~~~~~~~~~~~~~~~~ Main-Loop ~~~~~~~~~~~~~~~~~~~~~~~~~~
void loop() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  //Detection, if the Sensor sends a valid Signal:
  if (isnan(temperature) || isnan(humidity)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error!");
    lcd.setCursor(0, 1);
    lcd.print("No values.");
    delay(2000);
    return;
  }

  //ReadOut of Poti-position:
  potiValue = analogRead(potiPin); //readout actual Poti-position
  potiValue = map(potiValue, 0, 1023, 20, 100); //calculate the spread (target is from 0-255)
  if (potiValue <= 30) { potiValue = 30; } //don't decrease below 30%
  if (potiValue >= 90) { potiValue = 90; } //don't increase above 90%

  //Activate the Relay if RH is just above potiValue - and deactivate if 8% below
  if (humidity >= potiValue) {
    digitalWrite(RelaisPin, HIGH);
    relais = true; //turn on the Relay
  } else if (humidity < potiValue - hysterese) {
    digitalWrite(RelaisPin, LOW);
    relais = false; //turn off the Relay
  }

  //if either the Poti or the MenuButton was touched...
  if ((BUTTON_2 == LOW || potiValue != potiValueOld) && !isDimming) { //"isDimming" = ensure only one dimming event
    dimBacklight(true); //...then set the backlight to 100% brightness
    backlightOn = true;
    backlightPrevious = millis(); //set value to actual millis()
    BUTTON_2 = HIGH;              //reset the flag
    potiValueOld = potiValue;
    isDimming = true;             // Set dimming flag to true
  }

  //TimeOut after 60 seconds:
  if (backlightOn && millis() - backlightPrevious >= backlightTimeout) {
    dimBacklight(false); //dim backlight to 10% brightness
    backlightOn = false;
    isDimming = false;            // Reset dimming flag after timeout
  }

  //Dimming-Timeout:
  if (millis() - previous >= displayRefreshInterval) { //1500ms
    previous = millis();

    dtostrf(temperature, 2, 0, tempStr);
    dtostrf(humidity, 2, 0, humidityStr);

    lcd.clear();              
    lcd.setCursor(0, 0);
    lcd.print("Temp.:   ");
    lcd.print(tempStr);
    lcd.write((uint8_t)0);  //° symbol
    lcd.print("C");

    lcd.setCursor(0, 1);      
    lcd.print("Feuchte: ");
    lcd.print(humidityStr);
    lcd.print(" %");

    lcd.setCursor(0, 2);      
    lcd.print("Poti:    ");
    lcd.print(potiValue);
    lcd.print(" %");

    lcd.setCursor(0, 3);      
    lcd.print("Trockner ");
    lcd.print(relais ? "EIN" : "AUS");
  }

  //Read analog value from Pin-A1 Input for switch status and display the result on LCD-Display (right side of the last line):
  int analogValue = analogRead(switchPin); //read-out the Switch-Position on Pin-A1 (input)
  lcd.setCursor(17, 3); //Move to the right side of the last line, position 17 (of 20)
    
  if (analogValue > 900) {
    lcd.print("__1"); //Permanent-On (Switch on +5V)
  }
  else if (analogValue > 60 && analogValue <= 900) {
    lcd.print("_S_"); //Sensor mode (Switch on "S" position ⇒ detect if within "bandpass" between 2.9V and 0.3V through analog-Pin A1)
  }
  else {
    lcd.print("0__"); //Permanent-Off (Switch on 0V)
  }

} //~~~~~~~~~~~~~~~~~~~~~~~~~~ End of Main-Loop ~~~~~~~~~~~~~~~~~~~~~~~~~~


//-------------------------- Functions --------------------------
void ButtonD2_LOW() {
  BUTTON_2 = LOW; //Set button flag to LOW if Button pressed (to GND)
}

//Function to dim the backlight from 10% to 100% and viceversa
void dimBacklight(bool turnOn) { //"turnOn" flag is set either "1" or "0"
  int delayTime = 10;  //milliSeconds (for PWM-pause ¯¯\_/¯¯ )
  int stepCount = 100; //counts (100 steps of 10ms = 1s total transition-time)
  int start = turnOn ? 25 : 255; //dim-UP  from 10% to 100% - if "turnOn" is TRUE = "1"
  int end = turnOn ? 255 : 25;   //dim-DOWN from 100% to 10% if "turnOn" is FALSE = "0"
  int step = (end - start) / stepCount; //step-"width" calculation

  for (int brightness = start; (turnOn ? brightness <= end : brightness >= end); brightness += step) {
    analogWrite(pwmPinForBL, brightness); //"brightness" is the changing value (from 25 to 255)
    delay(delayTime);
  }
}

/* Display Example:
-------------------
Temp:    25°C
Feuchte: 75 %
Poti:    60 %        Relay: directly "On" above 60% and "Off" below 52% RH (52 = 60%-8% hysteresis)
Trockner EIN        (EIN | AUS)
-------------------------------

Explanation:
************
Button and Potentiometer:
Button and Potentiometer activate the LCD-Display for 60 seconds.

Display Timeout:
The display turns off after 60 seconds of inactivity.

Hysteresis:
Turns on the device when the humidity is directly above the target value,
and off when it is 8% below.
*/
  