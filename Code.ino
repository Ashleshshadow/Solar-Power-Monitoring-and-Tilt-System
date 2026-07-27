#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>

Servo solarServo;

// WiFi
const char* ssid = "IQOO Z7 5G";
const char* password = "Ashleshs24";

// ThingSpeak
String apiKey = "2UQT54FYC6ZVKJJP";
String server = "http://api.thingspeak.com/update";

// Pins
int LDR_LEFT = 34;
int LDR_RIGHT = 35;
int servoPin = 18;
int voltagePin = 32;
int currentPin = 33;

// Servo control
int pos = 90;
int threshold = 120;
float gain = 0.02;

// Voltage sensor
float voltageScale = 5.0;

// Current sensor (ACS712)
float sensitivity = 0.185;      
float vRef = 3.3;
int adcResolution = 4095;
float offsetVoltage = 1.65;
const float CURRENT_THRESHOLD = 0.05;

// Timing
unsigned long lastUpload = 0;
const long interval = 15000;

// -------------------- LDR --------------------
int readLDR(int pin)
{
    int sum = 0;
    for (int i = 0; i < 8; i++)
    {
        sum += analogRead(pin);
        delay(2);
    }

    return sum / 8;
}

// -------------------- Voltage --------------------
float readVoltage()
{
    int adc = analogRead(voltagePin);

    float vOut = (adc * vRef) / adcResolution;

    return vOut * voltageScale;
}

// -------------------- Current Calibration --------------------
void calibrateCurrentOffset()
{
    Serial.println("Calibrating current sensor... ensure NO load is connected.");
    delay(2000);

    long sum = 0;
    int samples = 200;

    for (int i = 0; i < samples; i++)
    {
        sum += analogRead(currentPin);
        delayMicroseconds(500);
    }

    float avgADC = sum / (float)samples;

    offsetVoltage = (avgADC * vRef) / adcResolution;

    Serial.print("Calibrated Offset Voltage: ");
    Serial.print(offsetVoltage, 4);
    Serial.println(" V");
}

// -------------------- Current --------------------
float readCurrent()
{
    long sum = 0;
    int samples = 200;

    for (int i = 0; i < samples; i++)
    {
        sum += analogRead(currentPin);
        delayMicroseconds(200);
    }

    float avgADC = sum / (float)samples;
    float vOut = (avgADC * vRef) / adcResolution;

    float current = (vOut - offsetVoltage) / sensitivity;

    if (abs(current) < CURRENT_THRESHOLD)
    {
        current = 0.0;
    }

    return abs(current);
}

void setup()
{
    Serial.begin(9600);

    analogReadResolution(12);
    analogSetPinAttenuation(voltagePin, ADC_11db);
    analogSetPinAttenuation(currentPin, ADC_11db);

    solarServo.setPeriodHertz(50);
    solarServo.attach(servoPin, 500, 2400);
    solarServo.write(pos);

    calibrateCurrentOffset();

    WiFi.begin(ssid, password);

    Serial.print("Connecting");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nConnected");
}

void loop()
{
    // Servo + LDR
    int leftVal = readLDR(LDR_LEFT);
    int rightVal = readLDR(LDR_RIGHT);

    int diff = leftVal - rightVal;

    if (abs(diff) > threshold)
    {
        int change = diff * gain;
        pos -= change;
    }

    pos = constrain(pos, 20, 160);
    solarServo.write(pos);

    // Sensor readings
    float voltage = readVoltage();
    float current = readCurrent();
    float power = voltage * current;

    // Serial Monitor
    Serial.print("Voltage: ");
    Serial.print(voltage, 3);
    Serial.print(" V | Current: ");
    Serial.print(current, 3);
    Serial.print(" A | Power: ");
    Serial.print(power, 3);
    Serial.println(" W");

    // Upload to ThingSpeak
    if (millis() - lastUpload > interval)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            HTTPClient http;

            String url = server +
                         "?api_key=" + apiKey +
                         "&field1=" + String(voltage, 3) +
                         "&field2=" + String(current, 3) +
                         "&field3=" + String(power, 3);

            http.begin(url);

            int code = http.GET();

            Serial.print("Upload Response: ");
            Serial.println(code);

            http.end();
        }

        lastUpload = millis();
    }

    delay(200);
}