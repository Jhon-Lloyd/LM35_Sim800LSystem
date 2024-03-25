#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Define the I2C address of the LCD
const int lcdColumns = 16;
const int lcdRows = 2;
const int lcdAddress = 0x27;  // Adjust this address according to your LCD module

// Initialize the LCD with the I2C address
LiquidCrystal_I2C lcd(lcdAddress, lcdColumns, lcdRows);

const int lm35_pin = A1;      /* LM35 O/P pin */

// Define LED pins
const int ledPin1 = 2;        // First LED pin
const int ledPin2 = 3;        // Second LED pin
const int ledPin3 = 4;        // Third LED pin
const int buzzerPin = 5;      // Piezo buzzer pin

// Variables to control buzzer toggling
int buzzerCount = 0;
bool buzzerState = false;
unsigned long buzzerStartTime = 0;

// Variables to track LED state
int prevLedState = -1;

// SIM800L configuration
SoftwareSerial sim800l(10, 11); // TX, RX for Arduino and for the module (inverted)
const int timeout = 5000; // 5 seconds timeout
unsigned long previousMillis = 0;
const unsigned long interval = 1000; // Check every second
bool systemOn = true;

void setup() {
  Serial.begin(9600);
  sim800l.begin(9600);  // Module baud rate
  lcd.init();                      // Initialize the LCD
  lcd.backlight();                 // Turn on backlight
  lcd.setCursor(0, 0);             // Set cursor to the first column of the first row
  lcd.print("Temperature:");       // Print initial message

  // Set LED pins as outputs
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(ledPin3, OUTPUT);

  // Set buzzer pin as output
  pinMode(buzzerPin, OUTPUT);

  // Set the module to SMS mode
  sim800l.println("AT+CMGF=1\r");
  delay(1000);

  // Set the module to show SMS text mode directly in the serial output
  sim800l.println("AT+CNMI=2,2,0,0,0\r");
  delay(1000);
}

void loop() {
  unsigned long currentMillis = millis();
  static unsigned long previousTempMillis = 0;
  static float previousTemp = 0.0;
  
  // Check for received SMS
  while (sim800l.available()) {
    String sms = getReceivedSMS();
    if (sms.indexOf("off") != -1) {
      systemOn = false;
      SendSMS("+639051758337", "The system is now off.");
    }
    int temperature = extractTemperature(sms);
    if (temperature != -1 && systemOn) {
      handleTemperature(temperature);
    }
  }

  // Read temperature from LM35 sensor
  if (currentMillis - previousTempMillis >= interval) {
    int temp_adc_val = analogRead(lm35_pin);  // Read temperature
    float temp_val = (temp_adc_val * 0.488);    // Convert ADC value to voltage (10mV per degree Celsius)
    
    // Display temperature on LCD
    lcd.setCursor(0, 1);
    lcd.print(temp_val);
    lcd.print(" C ");

    // Check if temperature change is significant and send SMS if temperature is 38 or above
    if (abs(temp_val - previousTemp) >= 1.0 && temp_val >= 38.0) {
      String message = "Warning! Temperature is critically high: " + String(temp_val) + "°C. Reply with 'off' to turn off the system.";
      SendSMS("+639051758337", message);
      delay(5000); // Wait for 5 seconds before sending the next SMS
    }

    // Update previous temperature and time
    previousTemp = temp_val;
    previousTempMillis = currentMillis;
  }

  // Check if it's time to send a command to the SIM800 module
  if (currentMillis - previousMillis >= interval) {
    // Update the previousMillis variable with the current time
    previousMillis = currentMillis;
    
    // Read temperature from LM35 sensor and send SMS
    int temp_adc_val = analogRead(lm35_pin);  // Read temperature
    float temp_val = (temp_adc_val * 0.488);    // Convert ADC value to voltage (10mV per degree Celsius)
    String message = "Current temperature: " + String(temp_val) + "°C";
    SendSMS("+639051758337", message);
  }
}

String getReceivedSMS() {
  // Read and return the received SMS content
  String sms = sim800l.readStringUntil('\n');
  sms.trim();
  return sms;
}

int extractTemperature(const String& sms) {
  int index = sms.indexOf("Temp:");
  if (index != -1) {
    String tempStr = sms.substring(index + 5); // Skip "Temp:"
    tempStr.trim();
    return tempStr.toInt();
  } else {
    // Temperature not found in SMS
    return -1; // Return a special value to indicate error
  }
}

void handleTemperature(int temperature) {
  if (temperature != -1) {
    String message = getSMSMessageBasedOnTemperature(temperature);
    SendSMS("+639051758337", "Received: " + String(temperature) + "\n" + message);
  }
}

String getSMSMessageBasedOnTemperature(int temperature) {
  if (temperature < 10) {  // Temperature is fine
    return "Temperature is fine.";
  } else if (temperature < 25) {  // Temperature is getting higher
    return "Temperature is getting higher. Please monitor.";
  } else if (temperature < 37) {  // Warning: Moderately high temperature
    return "Warning! Temperature is moderately high.";
  } else {  // Critical: Very high temperature
    return "Warning! Temperature is critically high! Reply with 'off' to turn off the system.";
  }
}

void SendSMS(String number, String message) {
  Serial.println("Sending SMS to: " + number);
  sim800l.print("AT+CMGS=\"" + number + "\"\r");
  delay(500);

  sim800l.print(message);
  delay(500);

  sim800l.print((char)26); // Ctrl+Z to end message
  delay(500);

  // Optional: Wait for OK response from SIM800
  sim800l.find("OK");

  Serial.println("Text Sent.");
}
