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

// LED pins
const int ledPin1 = 2;  // First LED pin
const int ledPin2 = 3;  // Second LED pin
const int ledPin3 = 4;  // Third LED pin

// Buzzer pin
const int buzzerPin = 6; // Buzzer pin

// SIM800L configuration
SoftwareSerial sim800l(10, 11);  // TX, RX for Arduino and for the module (inverted)
const int timeout = 5000;         // 5 seconds timeout
unsigned long previousMillis = 0;
unsigned long buzzerStartTime = 0;
unsigned long buzzerDuration = 300000; // Buzzer duration in milliseconds (5 minutes)
unsigned long buzzerRestDuration = 60000; // Rest duration after buzzer in milliseconds (1 minute)
int buzzerCount = 0;
const int maxBuzzerCount = 3;
bool systemOn = true;
bool smsSent = false; // Flag to track if an SMS has been sent

// Phone numbers to send SMS to
const String phoneNumber1 = "+639657937226";
const String phoneNumber2 = "+639358614108";
const String phoneNumber3 = "+639069525799";
const String phoneNumber4 = "+639051758337"; // Add your second phone number here

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

  // Send a message indicating the system is now open to all phone numbers
  SendSMS(phoneNumber1, "System is now open");
  SendSMS(phoneNumber2, "System is now open");
  SendSMS(phoneNumber3, "System is now open");
  SendSMS(phoneNumber4, "System is now open");
}

void loop() {
  unsigned long currentMillis = millis();

  // Read temperature from LM35 sensor
  int temp_adc_val = analogRead(lm35_pin);  // Read temperature
  float temp_val = (temp_adc_val * 0.488);   // Convert ADC value to voltage (10mV per degree Celsius)

  // Display temperature on LCD
  lcd.setCursor(0, 1);
  lcd.print("                "); // Clear previous temperature reading
  lcd.setCursor(0, 1);
  lcd.print(" ");
  lcd.print(temp_val);
  lcd.print(" C ");

  delay(500);

  // Determine which LED to light up based on temperature thresholds
  if (temp_val >= 1 && temp_val <= 50) {
    digitalWrite(ledPin1, HIGH);  // Light up the first LED
    digitalWrite(ledPin2, LOW);   // Turn off the second LED
    digitalWrite(ledPin3, LOW);   // Turn off the third LED
    digitalWrite(buzzerPin, LOW); // Turn off the buzzer
  } else if (temp_val >= 51 && temp_val <= 100) {
    digitalWrite(ledPin1, LOW);    // Turn off the first LED
    digitalWrite(ledPin2, HIGH);   // Light up the second LED
    digitalWrite(ledPin3, LOW);    // Turn off the third LED
    digitalWrite(buzzerPin, LOW);  // Turn off the buzzer
  } else {
    digitalWrite(ledPin1, LOW);    // Turn off the first LED
    digitalWrite(ledPin2, LOW);    // Turn off the second LED
    digitalWrite(ledPin3, HIGH);   // Light up the third LED

    // Activate the buzzer when the third LED lights up
    if (currentMillis - buzzerStartTime < buzzerDuration) {
      digitalWrite(buzzerPin, HIGH); // Turn on the buzzer
    } else {
      if (buzzerCount < maxBuzzerCount) {
        if (currentMillis - buzzerStartTime - buzzerDuration < buzzerRestDuration) {
          digitalWrite(buzzerPin, LOW); // Turn off the buzzer
        } else {
          buzzerCount++;
          buzzerStartTime = currentMillis;
        }
      } else {
        // If buzzer has buzzed 3 times, reset the buzzer count and turn off the buzzer
        buzzerCount = 0;
        digitalWrite(buzzerPin, LOW);
      }
    }
  }

  // Delay for 2 seconds before taking temperature readings
  if (currentMillis - previousMillis >= 2000) {
    previousMillis = currentMillis; // Update previousMillis for the next delay

    // Check if temperature is in the last threshold and send SMS if not already sent
    if (temp_val >= 110.0 && !smsSent && systemOn) {
      // Send the warning message three times with a 2-second delay between each message
      for (int i = 0; i < 3; i++) {
        String message = "Warning! Temperature is critically high: " + String(temp_val) + "°C. Reply with 'off' to turn off the system.";
        SendSMS(phoneNumber1, message);
        SendSMS(phoneNumber2, message);
        SendSMS(phoneNumber3, message);
        SendSMS(phoneNumber4, message);
        delay(2000); // 2-second delay between each message
      }

      smsSent = true; // Set flag to true to indicate SMS has been sent
    }
  }

  // Check for received SMS
  while (sim800l.available()) {
    String sms = getReceivedSMS();
    if (sms.indexOf("off") != -1) {
      systemOn = false;
      SendSMS(phoneNumber1, "The system is now off.");
      SendSMS(phoneNumber2, "The system is now off.");
      SendSMS(phoneNumber3, "The system is now off.");
      SendSMS(phoneNumber4, "The system is now off.");
    }
  }
}

String getReceivedSMS() {
  // Read and return the received SMS content
  String sms = sim800l.readStringUntil('\n');
  sms.trim();
  return sms;
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
