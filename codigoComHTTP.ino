#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>

// Pin definitions
#define CONTROL_OUTPUT_PIN 16  // Define GPIO 16 on ESP32 as control output
#define BUZZER_PIN 2           // Define GPIO 2 on ESP32 as the buzzer pin
#define TEMP_SENSOR_PIN 14     // Define GPIO 14 on ESP32 as temperature sensor input
#define BAT_SENSOR_PIN 39      // Define GPIO 39 on ESP32 as battery sensor input

// Interval for feeding queue (in milliseconds)
#define POST_INTERVAL 15000

// OLED display size (in pixels)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define ssid "matheusWiFi"
#define password "teste123"

#define DHTTYPE DHT11

DHT dht(TEMP_SENSOR_PIN, DHTTYPE);

WiFiServer server(80);

struct MonitoringData {
  float temperature;
  float humidity;
};

QueueHandle_t monitoringQueue;
TimerHandle_t queueFeedTimer;

float batteryPercentage, temperature, humidity, pwm;
bool isConnected = false;
unsigned long lastFeedTime = 0, lastBuzzerTime = 0;
;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);
  pinMode(CONTROL_OUTPUT_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TEMP_SENSOR_PIN, INPUT);
  pinMode(BAT_SENSOR_PIN, INPUT);

  dht.begin();

  Wire.begin(5, 4);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  display.setRotation(2);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);

  // Start HTTP server
  server.begin();

  ledcAttachPin(CONTROL_OUTPUT_PIN, 0);  // Anexar o pino ao canal 0 do PWM
  ledcSetup(0, 1000, 8);                 // Canal 0, frequência de 5000 Hz, resolução de 8 bits

  // Create monitoring queue
  monitoringQueue = xQueueCreate(10, sizeof(MonitoringData));  // Alterado para o tamanho da estrutura MonitoringData

  // PID initialization
  temperature = dht.readTemperature();

  isConnected = (WiFi.status() == WL_CONNECTED);
  if (isConnected) {
    getPWMValue();
  }
}

void loop() {
  // Check WiFi connection
  isConnected = (WiFi.status() == WL_CONNECTED);
  if (isConnected) {
    static unsigned long lastPostTime = 0;
    if (millis() - lastPostTime >= POST_INTERVAL) {
      float temperature = dht.readTemperature();
      if (!isnan(temperature)) { 
        processAndSendData();
      }
      lastPostTime = millis();
    }
    getPWMValue();
  }

  // Read temperature
  temperature = dht.readTemperature();

  // Update fan output
  ledcWrite(0, pwm);
  Serial.println("pwm: ");
  Serial.println(pwm);

  batteryPercentage = analogRead(BAT_SENSOR_PIN) / 4095.0 * 100;

  unsigned long currentTime = millis();
  if (batteryPercentage < 15 && (currentTime - lastBuzzerTime >= 15000)) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1000);
    digitalWrite(BUZZER_PIN, LOW);
    lastBuzzerTime = currentTime;
  } else if (batteryPercentage < 55 && (currentTime - lastBuzzerTime >= 5000)) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1000);
    digitalWrite(BUZZER_PIN, LOW);
    lastBuzzerTime = currentTime;
  }

  // Display data on OLED
  displayData();
  delay(500);
}

void displayData() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Temperature: " + String(temperature, 1) + "C");

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println("PWM: " + String(pwm));

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 50);
  display.println("WiFi: " + String(isConnected ? "Connected" : "Not Connected"));

  display.display();
}

void getPWMValue() {
  HTTPClient http;

  float temperatura = dht.readTemperature();

  if (!isnan(temperatura)) {
    String url = "http://192.168.101.29:3000/fuzzy/" + String(temperatura);
    http.begin(url);

    // Fazer a solicitação GET
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      pwm = payload.toInt();
      Serial.print("Valor do PWM recebido: ");
      Serial.print(pwm);
      http.end();
    }
  } else {
    Serial.println("Falha na solicitação HTTP!");
    pwm = 255;
  }
  return;
}

void processAndSendData() {
  // Get temperature and humidity data
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Create a JSON object with monitoring data
  JSONVar createRegistryDto;
  char tempStr[6];
  snprintf(tempStr, sizeof(tempStr), "%.2f", temperature);
  createRegistryDto["temperature"] = atof(tempStr);
  createRegistryDto["humidity"] = humidity;

  // Encapsulate createRegistryDto in the monitoringJSON
  JSONVar monitoringJSON;
  monitoringJSON["createRegistryDto"] = createRegistryDto;

  // Convert JSON object to String to send
  String jsonString = JSON.stringify(monitoringJSON);
  Serial.println(jsonString);

  // Send data via POST
  HTTPClient http;
  http.begin("http://192.168.101.29:3000/registries");
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(jsonString);

  // Check the returning http code
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println(httpCode);
    Serial.println(payload);
  } else {
    Serial.println("Error on HTTP request");
  }

  http.end();
}
