#include <Wire.h>
#include <math.h>
#include <AccelStepper.h>
#include "Adafruit_SHT31.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// Pin Definitions for ESP32-S3
#define PH_PIN 1          // ADC1_CH0
#define VPD_PUMP_RELAY 19  
#define ACID_PUMP_RELAY 21 
#define BASE_PUMP_RELAY 20 
#define MIX_PUMP_RELAY 5  
#define TRIG_PIN 13        
#define ECHO_PIN 14        
#define LDR_PIN 8        // Changed to ADC1_CH1 for analog reading
#define STEPPER_STEP_PIN 4 
#define STEPPER_DIR_PIN 5  

// Constants (unchanged)
#define VPD_PUMP_DURATION 5000
#define MIX_PUMP_DURATION 1000
#define PH_CHECK_INTERVAL 30000
#define PH_WAIT_INTERVAL 18000
#define PH_LOWER_LIMIT 5.5
#define PH_UPPER_LIMIT 6.5
#define DOSAGE_RATE 0.00025
#define RESERVOIR_RADIUS 20.0
#define RESERVOIR_HEIGHT 35.0
#define RESERVOIR_CHECK_INTERVAL 3600
#define ROTATION_INTERVAL 5000
#define STEPS_PER_REVOLUTION 200
#define STEPS_90_DEGREES (STEPS_PER_REVOLUTION / 4)

// Global variables
Adafruit_SHT31 sht31 = Adafruit_SHT31();
AccelStepper stepper(AccelStepper::DRIVER, STEPPER_STEP_PIN, STEPPER_DIR_PIN);

unsigned long lastVPDCycleTime = 0;
unsigned long vpdCycleInterval = 120;
unsigned long lastpHCheckTime = 0;
unsigned long lastReservoirCheckTime = 0;
unsigned long lastRotationTime = 0;

bool isVPDPumping = false;
bool isPHAdjusting = false;
bool isPHWaiting = false;

long ph_pump_duration = 0;

float temperature = 0.0;
float humidity = 0.0;
float vpd = 0.0;
float pH = 0.0;
float waterLevel = 0.0;
float reservoirVolume = 0.0;
int lightIntensity = 0;  // Changed from bool to int
float PH_TARGET = 6.0;

// Add WiFi credentials after your existing pin definitions
const char* ssid = "Tbag";
const char* password = "Dbcooper";

// Create WebServer object after your existing global variables
WebServer server(80);

int LIGHT_THRESHOLD = 500;  // Initial value, can be modified at runtime

// Add these at the top with other global variables
unsigned long lastDataUpdate = 0;
const unsigned long DATA_UPDATE_INTERVAL = 1000; // Update data every second

bool sht31_available = false;
const float DEFAULT_TEMP = 25.0;
const float DEFAULT_HUMIDITY = 60.0;

void setup() {
  Serial.begin(115200);
  Wire.begin(41, 42);  // ESP32-S3 default I2C pins: SDA=41, SCL=42
  
  pinMode(VPD_PUMP_RELAY, OUTPUT);
  pinMode(ACID_PUMP_RELAY, OUTPUT);
  pinMode(BASE_PUMP_RELAY, OUTPUT);
  pinMode(MIX_PUMP_RELAY, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  // Remove this line since analog pins don't need pinMode
  // pinMode(LDR_PIN, INPUT);  
  
  digitalWrite(VPD_PUMP_RELAY, LOW);
  digitalWrite(ACID_PUMP_RELAY, LOW);
  digitalWrite(BASE_PUMP_RELAY, LOW);
  digitalWrite(MIX_PUMP_RELAY, HIGH);
  
  if (!sht31.begin(0x44)) {
    Serial.println("Couldn't find SHT31");
    sht31_available = false;
    Serial.println("Using default temperature and humidity values");
  } else {
    sht31_available = true;
    Serial.println("SHT31 sensor initialized successfully");
  }
  
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(500);

  // ESP32 ADC setup
  analogReadResolution(12); // ESP32 has 12-bit ADC

  // Add WiFi and web server setup
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/control", handleControl);
  server.on("/status", handleStatus);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  unsigned long currentTime = millis();
  
  server.handleClient();  // Add this line to handle web requests

  // Read sensor data
  if (sht31_available) {
    temperature = sht31.readTemperature();
    humidity = sht31.readHumidity();
    
    if (isnan(temperature) || isnan(humidity)) {
      temperature = DEFAULT_TEMP;
      humidity = DEFAULT_HUMIDITY;
    }
  } else {
    temperature = DEFAULT_TEMP;
    humidity = DEFAULT_HUMIDITY;
  }
  
  vpd = calculateVPD(temperature, humidity);
  pH = readpH();
  waterLevel = measureWaterLevel();
  reservoirVolume = calculateReservoirVolume(waterLevel);
  lightIntensity = analogRead(LDR_PIN);  // Changed to analogRead

  handleVPDControl(currentTime);
  handlePHControl(currentTime);
  checkReservoirVolume(currentTime);
  checkLightAndRotate(currentTime);
  
  stepper.run();
}

// Modified pH reading for ESP32's 12-bit ADC
float readpH() {
  int sensorValue = analogRead(PH_PIN);
  // ESP32 ADC is 12-bit (0-4095)
  return map(sensorValue, 0, 4095, 0, 14);
}

// The rest of the functions remain the same as they don't need ESP-specific modifications
// Just removing yield() calls as they're not needed for ESP32

void checkLightAndRotate(unsigned long currentTime) {
  if (currentTime - lastRotationTime >= ROTATION_INTERVAL) {
    lastRotationTime = currentTime;
    
    int lightLevel = analogRead(LDR_PIN);
    Serial.printf("Light intensity: %d\n", lightLevel);

    if (lightLevel > LIGHT_THRESHOLD) {
      stepper.moveTo(stepper.currentPosition() + STEPS_90_DEGREES);
      while (stepper.distanceToGo() != 0) {
        stepper.run();
      }
      Serial.println("Rotated 90 degrees");
    } else {
      Serial.println("Insufficient light, not rotating");
    }
  }
}

// Include all other functions here (handleVPDControl, handlePHControl, etc.)
// They remain the same as in your original code, just remove the yield() calls 

void handleVPDControl(unsigned long currentTime) {
  if (currentTime - lastVPDCycleTime >= vpdCycleInterval) {
    lastVPDCycleTime = currentTime;
    
    float humidity, temperature;
    
    if (sht31_available) {
      humidity = sht31.readHumidity();
      temperature = sht31.readTemperature();
      
      if (isnan(humidity) || isnan(temperature)) {
        Serial.println("Failed to read from SHT31 sensor, using default values");
        humidity = DEFAULT_HUMIDITY;
        temperature = DEFAULT_TEMP;
      }
    } else {
      humidity = DEFAULT_HUMIDITY;
      temperature = DEFAULT_TEMP;
    }

    float vpd = calculateVPD(temperature, humidity);
    updateVPDCycleInterval(vpd);
    
    Serial.printf("Humidity: %.1f%%, Temperature: %.1f°C, VPD: %.2f kPa\n", 
                 humidity, temperature, vpd);

    digitalWrite(VPD_PUMP_RELAY, HIGH);
    isVPDPumping = true;
    Serial.println("VPD Pump activated");
  }

  if (isVPDPumping && currentTime - lastVPDCycleTime >= VPD_PUMP_DURATION) {
    digitalWrite(VPD_PUMP_RELAY, LOW);
    isVPDPumping = false;
    Serial.println("VPD Pump deactivated");
  }
}

void handlePHControl(unsigned long currentTime) {
  if (!isPHAdjusting && !isPHWaiting && currentTime - lastpHCheckTime >= PH_CHECK_INTERVAL) {
    checkAndAdjustPH(currentTime);
  }

  if (isPHWaiting && currentTime - lastpHCheckTime >= PH_WAIT_INTERVAL) {
    isPHWaiting = false;
    checkAndAdjustPH(currentTime);
  }

  if (isPHAdjusting && currentTime - lastpHCheckTime >= ph_pump_duration) {
    digitalWrite(ACID_PUMP_RELAY, LOW);
    digitalWrite(BASE_PUMP_RELAY, LOW);
    digitalWrite(MIX_PUMP_RELAY, LOW);
    
    delay(MIX_PUMP_DURATION);
    
    digitalWrite(MIX_PUMP_RELAY, HIGH);
    isPHAdjusting = false;
    isPHWaiting = true;
    Serial.println("pH adjustment cycle completed, waiting before rechecking");
  }
}

void checkReservoirVolume(unsigned long currentTime) {
  if (currentTime - lastReservoirCheckTime >= RESERVOIR_CHECK_INTERVAL) {
    lastReservoirCheckTime = currentTime;
    
    float waterLevel = measureWaterLevel();
    float volume = calculateReservoirVolume(waterLevel);
    
    Serial.printf("Volume: %.1f liters\n", volume);
    ph_pump_duration = volume * DOSAGE_RATE * 1000000; // Convert to ms
  }
}

float calculateVPD(float temperature, float humidity) {
  float svp = 0.6108 * exp(17.27 * temperature / (temperature + 237.3)); 
  float avp = (humidity / 100.0) * svp;
  return svp - avp;
}

void updateVPDCycleInterval(float vpd) {
  vpdCycleInterval = (vpd > 1.5) ? 6000 : (vpd < 0.8) ? 18000 : 12000;
  Serial.printf("New VPD cycle interval: %d seconds\n", vpdCycleInterval / 1000);
}

float measureWaterLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  return RESERVOIR_HEIGHT - (duration * 0.034 / 2);
}

float calculateReservoirVolume(float waterLevel) {
  return PI * RESERVOIR_RADIUS * RESERVOIR_RADIUS * waterLevel / 1000.0;
}

void checkAndAdjustPH(unsigned long currentTime) {
  lastpHCheckTime = currentTime;
  float pH = readpH();
  Serial.printf("Current pH: %.2f\n", pH);

  if (pH < PH_LOWER_LIMIT || pH > PH_UPPER_LIMIT) {
    if (pH < PH_TARGET) {
      Serial.println("pH too low, activating base pump");
      digitalWrite(BASE_PUMP_RELAY, HIGH);
    } else {
      Serial.println("pH too high, activating acid pump");
      digitalWrite(ACID_PUMP_RELAY, HIGH);
    }
    isPHAdjusting = true;
    
    Serial.printf("Dosing for %ld ms based on current reservoir volume\n", ph_pump_duration);
  } else {
    Serial.println("pH within acceptable range");
  }
}

// Replace it with a simple redirect to your Netlify site
void handleRoot() {
  server.sendHeader("Location", "https://aeroponics-dashboard.netlify.app", true);
  server.send(302, "text/plain", "");
}

// Optimize handleData() to reduce JSON processing
void handleData() {
  unsigned long currentTime = millis();
  if (currentTime - lastDataUpdate < DATA_UPDATE_INTERVAL) {
    server.send(304); // Not Modified
    return;
  }
  
  lastDataUpdate = currentTime;
  
  String jsonString = "{";
  jsonString += "\"Temperature\":\"" + String(temperature, 1) + " °C\",";
  jsonString += "\"Humidity\":\"" + String(humidity, 1) + " %\",";
  jsonString += "\"VPD\":\"" + String(vpd, 2) + " kPa\",";
  jsonString += "\"pH\":\"" + String(pH, 2) + "\",";
  jsonString += "\"WaterLevel\":\"" + String(waterLevel, 1) + " cm\",";
  jsonString += "\"ReservoirVolume\":\"" + String(reservoirVolume, 1) + " L\",";
  jsonString += "\"LightIntensity\":\"" + String(lightIntensity) + "\"";
  jsonString += "}";
  
  server.sendHeader("Cache-Control", "max-age=1");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Access-Control-Max-Age", "86400");
  server.sendHeader("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  
  server.send(200, "application/json", jsonString);
}

// Optimize handleControl() to prevent rapid-fire requests
void handleControl() {
  // Add OPTIONS handling for CORS preflight requests
  if (server.method() == HTTP_OPTIONS) {
    server.sendHeader("Access-Control-Allow-Origin", "*");  // In production, replace * with your Netlify domain
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.sendHeader("Access-Control-Max-Age", "86400");
    server.send(204);
    return;
  }

  static unsigned long lastControlUpdate = 0;
  if (millis() - lastControlUpdate < 100) {
    server.send(429, "text/plain", "Too Many Requests");
    return;
  }
  
  if (server.hasArg("plain")) {
    lastControlUpdate = millis();
    String message;
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
      server.send(400, "text/plain", "Invalid JSON");
      return;
    }
    
    if (doc.containsKey("lightThreshold")) {
      LIGHT_THRESHOLD = doc["lightThreshold"];
      message = "Light threshold set to: " + String(LIGHT_THRESHOLD);
    } else if (doc.containsKey("pHTarget")) {
      PH_TARGET = doc["pHTarget"];
      message = "pH target set to: " + String(PH_TARGET);
    }
    
    server.send(200, "text/plain", message);
  }
}

// Add a new function to handle connection status
void handleStatus() {
  String status = "{\"status\":\"connected\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", status);
}