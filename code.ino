#include <Wire.h>   // required for the I2C devices communication.
#include <LiquidCrystal_I2C.h>

//pins declarations and definitions.

const int TEMP_SENSOR_PIN = A0;       // LM35 temperature sensor
const int OUTDOOR_LDR_PIN = A1;    // Outdoor light sensor
const int INDOOR_LDR_PIN = A2;        // Indoor light sensor


const int FAN_RELAY_PIN = 4;          // Relay control for fan
const int LIGHT_RELAY_PIN = 5;        // Relay control for lights


const int FAN_BUTTON_PIN = 6;         // Manual fan override button
const int LIGHT_BUTTON_PIN = 7;       // Manual light override button


const int FAN_LED_PIN = 8;            // Blue LED
const int LIGHT_LED_PIN = 9;          // Yellow LED
const int ERROR_LED_PIN = 10;         // Red LED


// temperature thresholds 
const float TEMP_THRESHOLD_ON = 28.0;      // Fan ON above this temp (°C)
const float TEMP_THRESHOLD_OFF = 24.0;     // Fan OFF below this temp (°C)
//here in the temp threshold,the offing temp is 4°C less than the onning temp (hysteresis).

// Light thresholds 
const int LIGHT_THRESHOLD_ON = 400;        // Lights ON below this (darker)
const int LIGHT_THRESHOLD_OFF = 500;       // Lights OFF above this (brighter)
// here also there is hysteresis in lights thresholds.

const int INDOOR_LIGHT_THRESHOLD = 300;     //here we are defining the threshold for the room also,this prevents turning on lights when room already bright

// Sunset simulation steps
const int SUNSET_STEPS = 10;                // 10 brightness levels for gradual change
const unsigned long STEP_DURATION = 30000;  // 30 seconds between steps

//timings
const unsigned long SENSOR_READ_INTERVAL = 500;      // Read sensors every 500ms
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;   // Update LCD every 1 second
const unsigned long TREND_WINDOW = 30000;             // 30 second trend window
const unsigned long MANUAL_OVERRIDE_TIMEOUT = 3600000; // 1 hour in milliseconds
const unsigned long BUTTON_DEBOUNCE_DELAY = 50;       // 50ms debounce


LiquidCrystal_I2C lcd(0x3F, 16, 2);  

// Current sensor readings
float currentTemperature = 0.0;
int outdoorLightLevel = 0;
int indoorLightLevel = 0;

// Control states
bool fanState = false;              // Actual fan state (relay)
bool lightState = false;             // Actual light state (relay)
int currentBrightness = 0;           // 0-100% for sunset simulation

// Manual override states
bool fanManualOverride = false;      
bool lightManualOverride = false;     
unsigned long fanOverrideStartTime = 0;
unsigned long lightOverrideStartTime = 0;
bool fanManualState = false;        
bool lightManualState = false;       

// Temperature trend tracking
float temperatureReadings[10];        // Circular buffer for temperature history or trends
int tempIndex = 0;
unsigned long lastTrendTime = 0;

// Sunset simulation tracking
int targetBrightness = 0;
unsigned long lastStepTime = 0;
int currentStep = 0;

// Timing variables
unsigned long lastSensorRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastErrorBlink = 0;

// Button states with debouncing
int lastFanButtonState = HIGH;
int lastLightButtonState = HIGH;
unsigned long lastFanButtonDebounce = 0;
unsigned long lastLightButtonDebounce = 0;

// Error flags
bool sensorError = false;
bool lastSensorError = false;


void setup() {
  
  Serial.begin(9600);     //9600 bits per second.
  Serial.println(F("Smart Climate & Lighting Control System"));
  
  // Initialize all pins
  pinMode(FAN_RELAY_PIN, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  pinMode(FAN_BUTTON_PIN, INPUT_PULLUP);  // Using internal pull-up
  pinMode(LIGHT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(FAN_LED_PIN, OUTPUT);
  pinMode(LIGHT_LED_PIN, OUTPUT);
  pinMode(ERROR_LED_PIN, OUTPUT);
  
  // Set initial states to OFF
  digitalWrite(FAN_RELAY_PIN, LOW);
  digitalWrite(LIGHT_RELAY_PIN, LOW);
  digitalWrite(FAN_LED_PIN, LOW);
  digitalWrite(LIGHT_LED_PIN, LOW);
  digitalWrite(ERROR_LED_PIN, LOW);
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  
  // Display welcome message
  lcd.setCursor(0, 0);
  lcd.print("Smart Climate");
  lcd.setCursor(0, 1);
  Serial.println(F("LCD Initialized - Welcome Screen"));
  delay(2000);
  lcd.clear();
  
  // Initialize temperature readings array
  for (int i = 0; i < 10; i++) {
    temperatureReadings[i] = 0.0;
  }
}

void loop() {
  unsigned long currentMillis = millis();
  
  //reading sensors in equispaced timings.
  if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
    readSensors();
    updateTemperatureTrend();
    lastSensorRead = currentMillis;
  }
  
  checkManualOverrides();
  if (!fanManualOverride) {
    updateFanControl();
  } else {
    //Check if manual override timeout has expired i.e. if 1 hour is passed.
    if (currentMillis - fanOverrideStartTime > MANUAL_OVERRIDE_TIMEOUT) {
      fanManualOverride = false;
      Serial.println(F("Fan manual override timeout,reverting to auto"));
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Fan: Auto Mode");
      delay(1000);
    }
  
  if (!lightManualOverride) {
    updateLightControl();
  } else {
    if (currentMillis - lightOverrideStartTime > MANUAL_OVERRIDE_TIMEOUT) {
      lightManualOverride = false;
      Serial.println(F("Light manual override timeout - reverting to auto"));
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Light: Auto Mode");
      delay(1000);
    }
  }
}
  // gradual dimming of light
  updateSunsetSimulation(currentMillis);
  
  //updating the lcd display.
  if (currentMillis - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = currentMillis;
  }
  
  updateStatusLEDs();
  delay(50);
}

void readSensors() {
  int tempRaw = analogRead(TEMP_SENSOR_PIN);
  int outdoorRaw = analogRead(OUTDOOR_LDR_PIN);
  int indoorRaw = analogRead(INDOOR_LDR_PIN);
  
  //temp convertion,converts raw ADC to temp in °C
  float calculatedTemp = (tempRaw * 5.0 / 1024.0) * 100.0;
  
  // Validation
  sensorError = false;
  
  // temp check--LM35 should read between -10°C and 100°C for valid range)
  if (calculatedTemp < -10.0 || calculatedTemp > 100.0) {
    sensorError = true;
    Serial.print(F("WARNING: Invalid temperature reading: "));
    Serial.println(calculatedTemp);
  } else {
    currentTemperature = calculatedTemp;
  }

  // Check LDR readings (should be between 0-1023)
  if (outdoorRaw < 0 || outdoorRaw > 1023 || indoorRaw < 0 || indoorRaw > 1023) {
    sensorError = true;
    Serial.println(F("WARNING: Invalid LDR reading"));
  } else {
    outdoorLightLevel = outdoorRaw;
    indoorLightLevel = indoorRaw;
  }
  
  // Log readings for debugging
  Serial.println("current Temp: " + String(currentTemperature, 1) + "°C");
  Serial.println("Outdoor light level: "+ String(outdoorLightLevel));
  Serial.println("Indoor light level: "+ String(indoorLightLevel));
  
  // Handle error LED
  if (sensorError && !lastSensorError) {
    // New error detected
    Serial.println(F("ERROR: Sensor failure detected!"));
    lastSensorError = true;
  } else if (!sensorError && lastSensorError) {
    // Error cleared
    lastSensorError = false;
  }
}

void updateTemperatureTrend() {
  temperatureReadings[tempIndex] = currentTemperature;
  tempIndex = (tempIndex + 1) % 10;
  lastTrendTime = millis();
}

float calculateTemperatureTrend() {
  float sumOld = 0.0;
  float sumNew = 0.0;
  int countOld = 0;
  int countNew = 0;
  
  // Average of old 3 readings
  for (int i = 0; i < 3; i++) {
    int index = (tempIndex - 1 - i + 10) % 10;
    if (temperatureReadings[index] > 0.1) {  
      sumOld += temperatureReadings[index];
      countOld++;
    }
  }
  
  // Average of new 3 readings (current)
  for (int i = 0; i < 3; i++) {
    int index = (tempIndex - 4 - i + 10) % 10;
    if (temperatureReadings[index] > 0.1) {  
      sumNew += temperatureReadings[index];
      countNew++;
    }
  }
  
  if (countOld > 0 && countNew > 0) {
    float avgOld = sumOld / countOld;
    float avgNew = sumNew / countNew;
    float trend = avgNew - avgOld;
    
    // Log trend for debugging
    if (abs(trend) > 0.5) {
      Serial.print(F("Temperature trend: "));
      Serial.print(trend, 2);
      Serial.println(trend > 0 ? F("°C/30s (Rising)") : F("°C/30s (Falling)"));
    }
    
    return trend;
  }
  
  return 0.0;  // Not enough data
}


void checkManualOverrides() {
  unsigned long currentMillis = millis();
  
  // Read button states
  int fanButtonReading = digitalRead(FAN_BUTTON_PIN);
  int lightButtonReading = digitalRead(LIGHT_BUTTON_PIN);
  
  // Fan button debouncing
  if (fanButtonReading != lastFanButtonState) {
    lastFanButtonDebounce = currentMillis;
  }
  
  if ((currentMillis - lastFanButtonDebounce) > BUTTON_DEBOUNCE_DELAY) {
    // Stable reading
    if (fanButtonReading == LOW) {  // Button pressed (LOW due to pull-up)
      fanManualOverride = !fanManualOverride;
      fanOverrideStartTime = currentMillis;
      
      if (fanManualOverride) {
        // Entering manual mode - toggle current state
        fanManualState = !fanState;
        digitalWrite(FAN_RELAY_PIN, fanManualState ? HIGH : LOW);
        fanState = fanManualState;
        Serial.println(F("BUTTON: Fan manual mode activated"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Fan: MANUAL MODE");
        lcd.setCursor(0, 1);
        lcd.print(fanManualState ? "ON" : "OFF");
        delay(1000);
      } else {
        // Exiting manual mode
        Serial.println(F("BUTTON: Fan auto mode restored"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Fan: Auto Mode");
        delay(1000);
      }
      
      // Debounce: wait for button release
      while (digitalRead(FAN_BUTTON_PIN) == LOW) {
        delay(10);
      }
    }
  }
  
  lastFanButtonState = fanButtonReading;
  
  // Light button debouncing
  if (lightButtonReading != lastLightButtonState) {
    lastLightButtonDebounce = currentMillis;
  }
  
  if ((currentMillis - lastLightButtonDebounce) > BUTTON_DEBOUNCE_DELAY) {
    if (lightButtonReading == LOW) {
      lightManualOverride = !lightManualOverride;
      lightOverrideStartTime = currentMillis;
      
      if (lightManualOverride) {
        lightManualState = !lightState;
        digitalWrite(LIGHT_RELAY_PIN, lightManualState ? HIGH : LOW);
        lightState = lightManualState;
        currentBrightness = lightManualState ? 100 : 0;
        Serial.println(F("BUTTON: Light manual mode activated"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Light: MANUAL");
        lcd.setCursor(0, 1);
        lcd.print(lightManualState ? "ON" : "OFF");
        delay(1000);
      } else {
        Serial.println(F("BUTTON: Light auto mode restored"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Light: Auto Mode");
        delay(1000);
      }
      
      while (digitalRead(LIGHT_BUTTON_PIN) == LOW) {
        delay(10);
      }
    }
  }
  
  lastLightButtonState = lightButtonReading;
}


void updateFanControl() {
  // Skip if sensor error (safe mode)
  if (sensorError) {
    if (fanState) {
      // Turn off fan in safe mode
      digitalWrite(FAN_RELAY_PIN, LOW);
      fanState = false;
      Serial.println(F("SAFE MODE: Fan turned OFF due to sensor error"));
    }
    return;
  }
  
  float trend = calculateTemperatureTrend();
  bool shouldFanBeOn = fanState;  
  
  // Check if we should turn fan ON
  if (!fanState) {
    // Fan is currently OFF
    
    // Case 1: Temperature above threshold
    if (currentTemperature > TEMP_THRESHOLD_ON) {
      shouldFanBeOn = true;
      Serial.println(F("FAN: Temperature above threshold---turning ON"));
    }
    // Case 2: Temperature rising fast (predictive pre-cooling)
    else if (currentTemperature > (TEMP_THRESHOLD_ON - 2.0) && trend > 1.5) {
      shouldFanBeOn = true;
      Serial.println(F("FAN: Rapid temperature rise detected--- pre-cooling ON"));
    }
  } 
  else {
    // Fan is currently ON
    
    // Turn OFF only when temperature drops below lower threshold (hysteresis)
    if (currentTemperature < TEMP_THRESHOLD_OFF) {
      shouldFanBeOn = false;
      Serial.println(F("FAN: Temperature below OFF threshold---turning OFF"));
    }
  }
  
  // Apply new state if changed
  if (shouldFanBeOn != fanState) {
    digitalWrite(FAN_RELAY_PIN, shouldFanBeOn ? HIGH : LOW);
    fanState = shouldFanBeOn;
    
    // Log state change
    Serial.print(F("FAN State Change: "));
    Serial.println(fanState ? F("ON") : F("OFF"));
  }
}

void updateLightControl() {
  // Skip if sensor error (safe mode)
  if (sensorError) {
    if (lightState) {
      // Turn off lights in safe mode
      digitalWrite(LIGHT_RELAY_PIN, LOW);
      lightState = false;
      currentBrightness = 0;
      Serial.println(F("SAFE MODE: Lights turned OFF due to sensor error"));
    }
    return;
  }
  
  bool shouldLightBeOn = lightState;
  
  // Determine if lights should be ON based on outdoor light
  bool outdoorDark = (outdoorLightLevel < LIGHT_THRESHOLD_ON);
  bool indoorDark = (indoorLightLevel < INDOOR_LIGHT_THRESHOLD);
  
  bool needLight = outdoorDark && indoorDark;
  
  // Check if outdoor is bright enough to turn lights off
  bool outdoorBright = (outdoorLightLevel > LIGHT_THRESHOLD_OFF);
  
  if (!lightState) {
    // Lights are currently OFF
    
    // Turn ON if needed (outdoor dark and indoor dark)
    if (needLight) {
      shouldLightBeOn = true;
      // Start sunset simulation (gradual turn on)
      targetBrightness = 100;
      currentStep = 0;
      lastStepTime = millis();
      Serial.println(F("LIGHT: Outdoor dark & indoor dark--- starting sunset simulation ON"));
    }
  } 
  else {
    // Lights are currently ON
    
    // Turn OFF if outdoor is bright enough (using higher threshold for hysteresis)
    if (outdoorBright) {
      shouldLightBeOn = false;
      // Start sunrise simulation (gradual turn off)
      targetBrightness = 0;
      currentStep = 0;
      lastStepTime = millis();
      Serial.println(F("LIGHT: Outdoor bright---starting sunrise simulation OFF"));
    }
  }
}

void updateSunsetSimulation(unsigned long currentMillis) {
  static int lastRelayState = LOW;
  
  if (lightManualOverride) {
    return;
  }
  
  // Check if we need to change brightness
  if (currentBrightness != targetBrightness) {
 
    if (currentMillis - lastStepTime >= STEP_DURATION) {
      currentStep++;
      
      if (targetBrightness > currentBrightness) {
        // Turning ON gradually
        currentBrightness = min(targetBrightness, (currentStep * 100) / SUNSET_STEPS);
        Serial.print(F("SUNSET: Brightness increasing: "));
        Serial.print(currentBrightness);
        Serial.println(F("%"));
      } else {
        // Turning OFF gradually
        currentBrightness = max(targetBrightness, 100 - (currentStep * 100) / SUNSET_STEPS);
        Serial.print(F("SUNRISE: Brightness decreasing: "));
        Serial.print(currentBrightness);
        Serial.println(F("%"));
      }
      
      lastStepTime = currentMillis;
      
      int newRelayState = (currentBrightness > 50) ? HIGH : LOW;
      
      if (newRelayState != lastRelayState) {
        digitalWrite(LIGHT_RELAY_PIN, newRelayState);
        lastRelayState = newRelayState;
        lightState = (newRelayState == HIGH);
        
        Serial.print(F("LIGHT Relay: "));
        Serial.println(lightState ? F("ON") : F("OFF"));
      }
      
      // Stop stepping when reached target
      if (currentStep >= SUNSET_STEPS) {
        currentBrightness = targetBrightness;
        Serial.println(F("Sunset/Sunrise simulation complete"));
      }
    }
  }
}
//display functions
void updateDisplay() {
  lcd.clear();
  
  // Line 1: Temperature and Fan Status
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(currentTemperature, 1);
  lcd.print((char)223);  // Degree symbol
  lcd.print("C");
  
  // Show trend arrow
  float trend = calculateTemperatureTrend();
  lcd.setCursor(6, 0);
  if (trend > 0.5) {
    lcd.print("↑");  // Rising
  } else if (trend < -0.5) {
    lcd.print("↓");  // Falling
  } else {
    lcd.print("→");  // Stable
  }
  
  // Fan status with fixed width
  lcd.setCursor(9, 0);
  lcd.print("FN:");
  if (fanState) {
    lcd.print("ON ");
  } else {
    lcd.print("OFF");
  }
  
  // Manual override indicator for fan
  if (fanManualOverride) {
    lcd.setCursor(14, 0);
    lcd.print("*");
  } else {
    lcd.setCursor(14, 0);
    lcd.print(" ");
  }
  
  // Line 2: Light Info
  lcd.setCursor(0, 1);
  lcd.print("OL:");
  lcd.print(outdoorLightLevel);
  lcd.print(" ");
  
  lcd.setCursor(7, 1);
  lcd.print("IL:");
  lcd.print(indoorLightLevel);
  lcd.print(" ");
  
  // Light status with brightness (MAX/MID/LOW/DIM/OFF)
  lcd.setCursor(12, 1);
  lcd.print("LT:");
  
  if (lightState) {
    if (currentBrightness > 80) {
      lcd.print("MAX");
    } else if (currentBrightness > 50) {
      lcd.print("MID");
    } else if (currentBrightness > 20) {
      lcd.print("LOW");
    } else {
      lcd.print("DIM");
    }
  } else {
    lcd.print("OFF");
  }
  
  // Manual override indicator for lights
  if (lightManualOverride) {
    lcd.setCursor(15, 1);
    lcd.print("*");
  }
  
  // ============ SERIAL DEBUGGING OUTPUT ============
  // Print current readings to Serial Monitor
  
  Serial.println("Current Sensor Readings:");

  // Temperature reading
  Serial.println("current Temp: " + String(currentTemperature, 1) + "°C");
  
  // Light readings
  Serial.println("Outdoor light level: " + String(outdoorLightLevel));
  Serial.println("Indoor light level: " + String(indoorLightLevel));
  
  // Fan status
  Serial.println("Fan state: " + String(fanState ? "ON" : "OFF") + 
                 (fanManualOverride ? " (MANUAL)" : " (AUTO)"));
  
  // Light status with brightness
  String lightStatus = "Light state: " + String(lightState ? "ON" : "OFF");
  if (lightState) {
    lightStatus += " (Brightness: " + String(currentBrightness) + "%)";
    
    // Add the text indicator
    if (currentBrightness > 80) lightStatus += " [MAX]";
    else if (currentBrightness > 50) lightStatus += " [MID]";
    else if (currentBrightness > 20) lightStatus += " [LOW]";
    else lightStatus += " [DIM]";
  }
  lightStatus += (lightManualOverride ? " (MANUAL)" : " (AUTO)");
  Serial.println(lightStatus);
  
  // Temperature trend if significant
  if (abs(trend) > 0.5) {
    String trendDir = (trend > 0) ? "RISING" : "FALLING";
    Serial.println("Temperature trend: " + String(trend, 2) + "°C/30s (" + trendDir + ")");
  }
  
  // Error status if any
  if (sensorError) {
    Serial.println("Sensor error detected!!!");
  }
}

//LED functions
void updateStatusLEDs() {
  // Fan LED (Blue) - ON when fan is on
  digitalWrite(FAN_LED_PIN, fanState ? HIGH : LOW);
  
  // Light LED (Yellow) - ON when lights are on
  digitalWrite(LIGHT_LED_PIN, lightState ? HIGH : LOW);
  
  // Error LED (Red) - Blink if sensor error
  if (sensorError) {
    // Blink error LED every 500ms
    unsigned long currentMillis = millis();
    if (currentMillis - lastErrorBlink > 500) {
      digitalWrite(ERROR_LED_PIN, !digitalRead(ERROR_LED_PIN));
      lastErrorBlink = currentMillis;
    }
  } else {
    digitalWrite(ERROR_LED_PIN, LOW);
  }
}

