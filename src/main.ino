#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Pin Tanımlamaları (ESP32-C6 için)
#define LED_PIN 8          // GPIO8 - NeoPixel data pin
#define BUTTON1_PIN 9      // GPIO9
#define BUTTON2_PIN 10     // GPIO10
#define BUTTON3_PIN 11     // GPIO11
#define RELAY_PIN 12       // GPIO12 - Röle kontrol
#define TRIG_PIN 13        // GPIO13 - HC-SR04 Trig
#define ECHO_PIN 14        // GPIO14 - HC-SR04 Echo

// NeoPixel ayarları
#define LED_COUNT 60       // LED sayısı
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// WiFi ayarları
const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASS";
WebServer server(80);

// Zaman ayarları
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800); // GMT+3 (Türkiye)

// Hava durumu API
const String weatherAPIKey = "API_KEY";
const String city = "Ankara,TR";

// Sistem durumu
struct SystemState {
  uint32_t color = 0xFF0000; // Varsayılan kırmızı
  uint8_t brightness = 100;
  bool relayState = false;
  bool weatherMode = false;
  String weatherStatus = "Clear";
  float temperature = 20.0;
} state;

// Buton yönetimi
unsigned long lastButtonPress[3] = {0, 0, 0};

void setup() {
  Serial.begin(115200);
  
  // Pin ayarları
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // LED şerit başlatma
  strip.begin();
  strip.show();
  strip.setBrightness(state.brightness);
  
  // WiFi bağlantısı
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Zaman istemcisi
  timeClient.begin();
  timeClient.update();
  
  // Web sunucusu
  setupWebServer();
  
  // EEPROM
  EEPROM.begin(512);
  loadSettings();
}

void loop() {
  // Buton kontrolü
  handleButtons();
  
  // Oda varlık kontrolü
  checkRoomPresence();
  
  // LED güncelleme
  updateLEDs();
  
  // Web sunucusu
  server.handleClient();
  
  // Zaman güncelleme (her dakika)
  static unsigned long lastTimeUpdate = 0;
  if (millis() - lastTimeUpdate > 60000) {
    timeClient.update();
    lastTimeUpdate = millis();
  }
  
  // Hava durumu güncelleme (her 10 dakika)
  static unsigned long lastWeatherUpdate = 0;
  if (millis() - lastWeatherUpdate > 600000 && WiFi.status() == WL_CONNECTED) {
    updateWeather();
    lastWeatherUpdate = millis();
  }
  
  delay(10);
}

// Web sunucusu ayarları
void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><title>RGB LED Control</title>";
    html += "<style>body{font-family:Arial,sans-serif;max-width:800px;margin:0 auto;padding:20px;}";
    html += ".color-picker{margin:20px 0;}button{padding:10px 15px;margin:5px;}</style>";
    html += "<script>function updateColor(){var r=document.getElementById('r').value;";
    html += "var g=document.getElementById('g').value;var b=document.getElementById('b').value;";
    html += "var xhr=new XMLHttpRequest();xhr.open('POST','/api/color',true);";
    html += "xhr.setRequestHeader('Content-Type','application/json');";
    html += "xhr.send(JSON.stringify({r:r,g:g,b:b}));}</script></head><body>";
    html += "<h1>RGB LED Kontrol Paneli</h1>";
    html += "<div class='color-picker'>";
    html += "R: <input type='range' id='r' min='0' max='255' value='255' oninput='updateColor()'><br>";
    html += "G: <input type='range' id='g' min='0' max='255' value='0' oninput='updateColor()'><br>";
    html += "B: <input type='range' id='b' min='0' max='255' value='0' oninput='updateColor()'></div>";
    html += "<button onclick=\"fetch('/api/relay/on')\">Röle Aç</button>";
    html += "<button onclick=\"fetch('/api/relay/off')\">Röle Kapat</button>";
    html += "<p>Durum: Röle " + String(state.relayState ? "AÇIK" : "KAPALI") + "</p>";
    html += "<p>Hava Durumu: " + state.weatherStatus + " (" + String(state.temperature) + "°C)</p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/api/color", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(256);
      deserializeJson(doc, server.arg("plain"));
      
      uint8_t r = doc["r"];
      uint8_t g = doc["g"];
      uint8_t b = doc["b"];
      
      state.color = strip.Color(r, g, b);
      state.weatherMode = false;
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.on("/api/relay/on", HTTP_GET, []() {
    state.relayState = true;
    digitalWrite(RELAY_PIN, HIGH);
    server.send(200, "text/plain", "Röle AÇIK");
  });

  server.on("/api/relay/off", HTTP_GET, []() {
    state.relayState = false;
    digitalWrite(RELAY_PIN, LOW);
    server.send(200, "text/plain", "Röle KAPALI");
  });

  server.on("/api/weather", HTTP_GET, []() {
    updateWeather();
    String response = "{\"weather\":\"" + state.weatherStatus + "\",\"temp\":" + String(state.temperature) + "}";
    server.send(200, "application/json", response);
  });

  server.begin();
}

// Buton kontrol fonksiyonları
void handleButtons() {
  // Buton 1 - Renk değiştirme
  if (digitalRead(BUTTON1_PIN) == LOW) {
    if (millis() - lastButtonPress[0] < 300) { // Çift tık
      // Röle durumunu göster
      Serial.println("Röle durumu: " + String(state.relayState ? "AÇIK" : "KAPALI"));
    } else { // Tek tık
      // Renk değiştirme
      state.color = wheel(random(255));
      state.weatherMode = false;
    }
    lastButtonPress[0] = millis();
    delay(200); // Debounce
  }

  // Buton 2 - Parlaklık ayarı
  if (digitalRead(BUTTON2_PIN) == LOW) {
    if (millis() - lastButtonPress[1] < 300) { // Çift tık
      // PWM durumunu göster
      Serial.println("PWM Değeri: " + String(state.brightness));
    } else { // Tek tık
      // Parlaklık ayarı
      state.brightness = (state.brightness + 25) % 125;
      strip.setBrightness(state.brightness);
    }
    lastButtonPress[1] = millis();
    delay(200); // Debounce
  }

  // Buton 3 - Hava durumu modu
  if (digitalRead(BUTTON3_PIN) == LOW) {
    state.weatherMode = !state.weatherMode;
    if (state.weatherMode) {
      updateWeather();
      Serial.println("Hava durumu modu aktif");
    } else {
      Serial.println("Hava durumu modu pasif");
    }
    lastButtonPress[2] = millis();
    delay(200); // Debounce
  }
}

// Oda varlık kontrolü
void checkRoomPresence() {
  // HC-SR04 ile mesafe ölçümü
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  int distance = duration * 0.034 / 2;
  
  // Hareket algılama (1m içinde)
  if (distance > 0 && distance < 100) {
    if (!state.relayState && shouldLightBeOn()) {
      digitalWrite(RELAY_PIN, HIGH);
      state.relayState = true;
    }
  } else if (distance > 100 && state.relayState) {
    digitalWrite(RELAY_PIN, LOW);
    state.relayState = false;
  }
}

bool shouldLightBeOn() {
  int currentHour = timeClient.getHours();
  return (currentHour >= 19 || currentHour < 7); // 19:00-07:00 arası
}

// LED güncelleme
void updateLEDs() {
  if (state.weatherMode) {
    weatherAnimation();
  } else {
    for (int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, state.color);
    }
  }
  strip.show();
}

// Hava durumu animasyonu
void weatherAnimation() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 100) {
    if (state.weatherStatus == "Rain") {
      // Yağmur efekti
      for (int i = 0; i < strip.numPixels(); i++) {
        if (random(10) < 3) {
          strip.setPixelColor(i, strip.Color(0, 0, 255)); // Mavi
        } else {
          strip.setPixelColor(i, strip.Color(0, 0, 0)); // Kapalı
        }
      }
    } else if (state.weatherStatus == "Clear") {
      // Açık hava efekti
      static int pos = 0;
      for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, wheel((i + pos) % 255));
      }
      pos++;
    }
    strip.show();
    lastUpdate = millis();
  }
}

// Hava durumu güncelleme
void updateWeather() {
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + 
               "&appid=" + weatherAPIKey + "&units=metric&lang=tr";
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());
    
    state.weatherStatus = doc["weather"][0]["main"].as<String>();
    state.temperature = doc["main"]["temp"];
  }
  http.end();
}

// Yardımcı fonksiyonlar
uint32_t wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void loadSettings() {
  // EEPROM'dan ayarları yükle
  EEPROM.get(0, state);
}

void saveSettings() {
  // EEPROM'a ayarları kaydet
  EEPROM.put(0, state);
  EEPROM.commit();
}