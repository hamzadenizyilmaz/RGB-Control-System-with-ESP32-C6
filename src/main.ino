#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define LED_PIN 8
#define BUTTON1_PIN 9
#define BUTTON2_PIN 10
#define BUTTON3_PIN 11
#define RELAY_PIN 12
#define TRIG_PIN 13
#define ECHO_PIN 14

#define LED_COUNT 60
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASS";
WebServer server(80);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800);

const String weatherAPIKey = "API_KEY";
const String city = "Ankara,TR";

struct SystemState {
  uint32_t color = 0xFF0000;
  uint8_t brightness = 100;
  bool relayState = false;
  bool weatherMode = false;
  String weatherStatus = "Clear";
  float temperature = 20.0;
} state;

unsigned long lastButtonPress[3] = {0, 0, 0};

void setup() {
  Serial.begin(115200);
  
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  strip.begin();
  strip.show();
  strip.setBrightness(state.brightness);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  timeClient.begin();
  timeClient.update();
  
  setupWebServer();
  
  EEPROM.begin(512);
  loadSettings();
}

void loop() {
  handleButtons();
  checkRoomPresence();
  updateLEDs();
  server.handleClient();
  
  static unsigned long lastTimeUpdate = 0;
  if (millis() - lastTimeUpdate > 60000) {
    timeClient.update();
    lastTimeUpdate = millis();
  }
  
  static unsigned long lastWeatherUpdate = 0;
  if (millis() - lastWeatherUpdate > 600000 && WiFi.status() == WL_CONNECTED) {
    updateWeather();
    lastWeatherUpdate = millis();
  }
  
  delay(10);
}

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

void handleButtons() {
  if (digitalRead(BUTTON1_PIN) == LOW) {
    if (millis() - lastButtonPress[0] < 300) {
      Serial.println("Röle durumu: " + String(state.relayState ? "AÇIK" : "KAPALI"));
    } else {
      state.color = wheel(random(255));
      state.weatherMode = false;
    }
    lastButtonPress[0] = millis();
    delay(200);
  }

  if (digitalRead(BUTTON2_PIN) == LOW) {
    if (millis() - lastButtonPress[1] < 300) {
      Serial.println("PWM Değeri: " + String(state.brightness));
    } else {
      state.brightness = (state.brightness + 25) % 125;
      strip.setBrightness(state.brightness);
    }
    lastButtonPress[1] = millis();
    delay(200);
  }

  if (digitalRead(BUTTON3_PIN) == LOW) {
    state.weatherMode = !state.weatherMode;
    if (state.weatherMode) {
      updateWeather();
      Serial.println("Hava durumu modu aktif");
    } else {
      Serial.println("Hava durumu modu pasif");
    }
    lastButtonPress[2] = millis();
    delay(200);
  }
}

void checkRoomPresence() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  int distance = duration * 0.034 / 2;
  
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
  return (currentHour >= 19 || currentHour < 7);
}

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

void weatherAnimation() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 100) {
    if (state.weatherStatus == "Rain") {
      for (int i = 0; i < strip.numPixels(); i++) {
        if (random(10) < 3) {
          strip.setPixelColor(i, strip.Color(0, 0, 255));
        } else {
          strip.setPixelColor(i, strip.Color(0, 0, 0));
        }
      }
    } else if (state.weatherStatus == "Clear") {
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
  EEPROM.get(0, state);
}

void saveSettings() {
  EEPROM.put(0, state);
  EEPROM.commit();
}
