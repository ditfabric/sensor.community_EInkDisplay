// PINNING E-INK to ModeMCU: // BUSY -> D2, RST -> D2 (!), DC -> D3, CS -> D8, CLK -> D5, DIN -> D7, GND -> GND, 3.3V -> 3.3V
// connect NodeMCU RST pin to D1 to provide wake up from deep sleep

#include <GxEPD.h>
#include <GxGDEM029T94/GxGDEM029T94.h>      // 2.9" b/w
#include <Fonts/Picopixel.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

#if defined(ESP8266)
GxIO_Class io(SPI, /*CS=D8*/ SS, /*DC=D3*/ 0, /*RST=D1*/ 5); // RST Set tp 5!
GxEPD_Class display(io, /*RST=D1*/ 5, /*BUSY=D2*/ 4); // default selection of D4(=2), D2(=4)
#else
GxIO_Class io(SPI, /*CS=*/ SS, /*DC=*/ 8, /*RST=*/ 9); // arbitrary selection of 8, 9 selected for default of GxEPD_Class
GxEPD_Class display(io, /*RST=*/ 9, /*BUSY=*/ 7); // default selection of (9), 7
#endif


#include <ESP8266WiFi.h>
#include <StreamUtils.h>
#include <time.h>                   // time() ctime()
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#include "arduino_secrets.h"
#include "font.h"

const char* ssid     = SECRET_SSID;
const char* password = SECRET_PASS;
const char* sensorIP = SECRET_SENSOR_DATA_URL;

/* Configuration of NTP */
#define MY_NTP_SERVER "de.pool.ntp.org"           
#define MY_TZ "CET-1CEST,M3.5.0,M10.5.0/3"  

time_t now; 
tm tm;

ESP8266WiFiMulti WiFiMulti;

float currentTemperature = 0;
float currentHumidity = 0;
float currentPressure  = 0;
float currentPM25  = 0;
float currentPM10  = 0;
int retryCount = 0;

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(2000);
  
  // Wait for serial to initialize.
  while (!Serial) { }
  Serial.println();
  Serial.println("starting setup");

  display.init(115200); // enable diagnostic output on Serial for display

  // We start connecting to a WiFi network
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, password);
  Serial.println();
  Serial.println();
  Serial.print("Wait for WiFi... ");
  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Setting up NTP...");
  configTime(MY_TZ, MY_NTP_SERVER); 
  Serial.println("setup done");
}

void loop()
{
  
  time(&now);                       // read the current time    
  localtime_r(&now, &tm);           // update the structure tm 
  getData();
  delay(2000);
  Serial.println("Dispay updated, waiting for 20s, then going to deep sleep for 10 mins");
  delay(20000);
  ESP.deepSleep(600e6);
  delay(20000);
}


void getData() {

  Serial.println("Attempting to fetch data from sensor...");
  WiFiClient client;
  HTTPClient http;
  DynamicJsonDocument doc(2048);
  http.setTimeout(10000);
  http.useHTTP10(true);

  // Send request for PM sensor
  if (http.begin(client, sensorIP)) {

    int httpCode = http.GET();
    if (httpCode > 0) {
      ReadLoggingStream loggingStream(http.getStream(), Serial);
      DeserializationError err = deserializeJson(doc, loggingStream);
      if (err) {
        Serial.print(F("deserializeJson() returned "));
        Serial.println(err.f_str());
        handleReadError();
      }
      // Extract values
      currentPM10 = doc["sensordatavalues"][0]["value"].as<float>();
      currentPM25 = doc["sensordatavalues"][1]["value"].as<float>();
      currentTemperature = doc["sensordatavalues"][2]["value"].as<float>();
      currentPressure = doc["sensordatavalues"][3]["value"].as<float>();
      currentHumidity = doc["sensordatavalues"][4]["value"].as<float>();
      // calculate timestamp from last measurement
      time(&now);                       // read the current time
      now = now - doc["age"].as<int>(); // subtract reading age
      localtime_r(&now, &tm);           // update the structure tm 
      drawDisplay();
    } else {
      handleReadError();
      Serial.print("HTTP Begin code not >0");
    }
    // Disconnect
    http.end();
  } else {
    Serial.print("HTTP Begin returned false");
    handleReadError();
  }
}

void drawDisplayError() {
  Serial.println("Error reading data from sensor.");
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);
  display.fillScreen(GxEPD_WHITE); // set the background to white (fill the buffer with value for white)
  display.setFont(&Lato_Semibold_32);
  display.setCursor(10, 74);
  display.print("KEINE DATEN VOM SENSOR :(");
  display.update();
}

void handleReadError() {
  retryCount = retryCount + 1;
  Serial.print("Fetching failed ");
  Serial.print(retryCount);
  Serial.println(" times.");
  if (retryCount < 3) {
    Serial.println("Retrying in 5 secs...");
    delay(5000);
    getData();
  } else {
    drawDisplayError();
  }
  
}

void drawDisplay() {

  display.setRotation(1);
  display.setTextWrap(false);
  display.setTextColor(GxEPD_BLACK);
  display.fillScreen(GxEPD_WHITE); // set the background to white (fill the buffer with value for white)
  display.setFont(&Lato_Hairline_12);
  
  drawLeftAlignedString("Feuchtigkeit", 0,40);
  drawRightAlignedString("Luftdruck", 296, 40);
  drawLeftAlignedString("PM2.5 (ppm)", 0, 90);
  drawRightAlignedString("PM 10 (ppm)", 296, 90);

  display.setFont(&Lato_Semibold_48);
  display.setCursor(85, 76);
  display.print(currentTemperature, 1);
  display.drawCircle(display.getCursorX() + 4, display.getCursorY() - 27, 4, GxEPD_BLACK);
  display.drawCircle(display.getCursorX() + 4, display.getCursorY() - 27, 3, GxEPD_BLACK);
  display.print(" C");

  display.setFont(&Lato_Semibold_32);
  display.setCursor(0, 24);
  display.print(round(currentHumidity), 0);
  display.print(" %");
  display.setCursor(165, 24);
  display.print(round(currentPressure / 100), 0);
  display.print(" hPA");
  display.setCursor(0, 120);
  display.print(currentPM25);
  display.setCursor(215, 120);
  display.print(currentPM10);

  display.setFont(&Lato_Hairline_12);
  display.setCursor(105, 120);
  display.print("Stand: ");
  display.print(tm.tm_hour);
  display.print(":");
  display.print(tm.tm_min);
  display.print(":");
  display.print(tm.tm_sec);

  display.update();
}


//helpers
void drawLeftAlignedString(const char *buf, int x, int y)
{
    display.setCursor(x, y);
    display.print(buf);
}
void drawRightAlignedString(const char *buf, int x, int y)
{
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
    display.setCursor(x - w - 1, y);
    display.print(buf);
}

void drawCenteredString(const char *buf, int x, int y)
{
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
    display.setCursor(x - w / 2, y);
    display.print(buf);
}
