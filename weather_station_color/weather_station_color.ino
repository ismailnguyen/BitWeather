/**The MIT License (MIT)
Copyright (c) 2015 by Daniel Eichhorn
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
See more at http://blog.squix.ch
*/

#include <Arduino.h>

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h> // Hardware-specific library
#include <SPI.h>
#include <Wire.h>  // required even though we do not use I2C 
#include "Adafruit_STMPE610.h"

// Additional UI functions
#include "GfxUi.h"

// Fonts created by http://oleddisplay.squix.ch/
#include "ArialRoundedMTBold_14.h"
#include "ArialRoundedMTBold_36.h"

// Download helper
#include "WebResource.h"

#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiClient.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the &onfiguration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ESP8266mDNS.h>          //Allow custom URL

#include "DHT.h"

#define DHTPIN D3 
#define DHTTYPE DHT11   // DHT 11
// Application settings
#include "settings.h"

#include <JsonListener.h>
#include <WundergroundClient.h>
#include "TimeClient.h"

// Include home html page
#include "html.h"

// HOSTNAME for OTA update
#define HOSTNAME "ESP8266-OTA-"

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
GfxUi ui = GfxUi(&tft);

Adafruit_STMPE610 spitouch = Adafruit_STMPE610(STMPE_CS);

WebResource webResource;
TimeClient timeClient(UTC_OFFSET);

// Set to false, if you prefere imperial/inches, Fahrenheit
WundergroundClient wunderground(IS_METRIC);

//declaring prototypes
void configModeCallback (WiFiManager *myWiFiManager);
void downloadCallback(String filename, int16_t bytesDownloaded, int16_t bytesTotal);
ProgressCallback _downloadCallback = downloadCallback;
void downloadResources();
void updateData();
void drawProgress(uint8_t percentage, String text);
void drawTime();
void drawCurrentWeather();
void drawForecast();
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex);
String getMeteoconIcon(String iconText);
void drawAstronomy();
void drawSeparator(uint16_t y);
void sleepNow(int wakeup);

long lastDownloadUpdate = millis();

DHT dht(DHTPIN, DHTTYPE);
//WiFiManager
//Local intialization. Once its business is done, there is no need to keep it around
WiFiManager wifiManager;

// Web server
// Initialization with custom SSID
ESP8266WebServer server(80);

// Initialisation of default settings from HTML setting screen
String current_WUNDERGRROUND_API_KEY = WUNDERGRROUND_API_KEY;
String current_WUNDERGROUND_CITY = WUNDERGROUND_CITY;
String current_WUNDERGROUND_CITY_CODE = WUNDERGROUND_CITY_CODE;
float current_UTC_OFFSET = UTC_OFFSET;
boolean current_IS_METRIC = IS_METRIC;

String getHTML() {
    String updatedIndexHTML = html_index;
    
    updatedIndexHTML.replace("[HTML_VALUE_API_KEY]", current_WUNDERGRROUND_API_KEY);
    updatedIndexHTML.replace("[HTML_VALUE_CITY]", current_WUNDERGROUND_CITY);
    updatedIndexHTML.replace("[HTML_VALUE_CITY_CODE]", current_WUNDERGROUND_CITY_CODE);

    String current_UTC_OFFSET_string = static_cast<String>(static_cast<int>(current_UTC_OFFSET));
    String current_UTC_OFFSET_HTML = "<option value='" + current_UTC_OFFSET_string + "'>" + current_UTC_OFFSET_string + "</option>";
    String new_UTC_OFFSET_HTML = "<option value='" + current_UTC_OFFSET_string + "' selected>" + current_UTC_OFFSET_string + "</option>";
      
    updatedIndexHTML.replace(current_UTC_OFFSET_HTML, new_UTC_OFFSET_HTML);

    String current_IS_METRIC_HTML = "";
    String new_IS_METRIC_HTML = "";
    if (current_IS_METRIC) {
      current_IS_METRIC_HTML = "<option value='C'>&#8451;</option>";
      new_IS_METRIC_HTML = "<option value='C' selected>&#8451;</option>";
    } 
    else {
      current_IS_METRIC_HTML = "<option value='F'>&#8457;</option>";
      new_IS_METRIC_HTML = "<option value='F' selected>&#8457;</option>";
    }
    updatedIndexHTML.replace(current_IS_METRIC_HTML, new_IS_METRIC_HTML);

    return updatedIndexHTML;
}

/*
 * Set start page of web server
 */
void handleRoot() {
  server.send(200, "text/html", getHTML());
}

void setupServer() {
  // Set root page
  server.on("/", handleRoot);

  // Start web server
  server.begin();

  Serial.println("HTTP server started");
}

void setupMDNS() {
    // Add service to MDNS-SD to access the ESP with the URL http://<ssid>.local
    if (MDNS.begin(ssid)) {
        Serial.print("MDNS responder started as http://");
        Serial.print(ssid);
        Serial.println(".local");
    }
    MDNS.addService("http", "tcp", 8080);
}

void setupWifi() {
  //Manual Wifi
  //WiFi.begin(WIFI_SSID, WIFI_PWD);
  
  //WiFiManager
  WiFiManager wifiManager;

  //reset saved settings -- Flush flash
  //wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect(ssid);

  // might seem redundant but it's not printed the 1st time:
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  if (! spitouch.begin()) {
    Serial.println("STMPE not found?");
  }

  // if using deep sleep, we can use the STMPE's IRQ pin to wake us up
  if (DEEP_SLEEP) {
    // this pin (#2) is connected to the STMPE IRQ pin
    pinMode(STMPE_IRQ, INPUT_PULLUP); 
    // tell the STMPE to use 'low' level as 'touched' indicator
    spitouch.writeRegister8(STMPE_INT_CTRL, STMPE_INT_CTRL_POL_LOW | STMPE_INT_CTRL_ENABLE);
  }
  
  // we'll use STMPE's GPIO 2 for backlight control
  spitouch.writeRegister8(STMPE_GPIO_DIR, _BV(2));
  spitouch.writeRegister8(STMPE_GPIO_ALT_FUNCT, _BV(2));
  // backlight on
  spitouch.writeRegister8(STMPE_GPIO_SET_PIN, _BV(2));
   
  tft.begin();
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(&ArialRoundedMTBold_14);
  ui.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  ui.setTextAlignment(CENTER);
  ui.drawString(120, 160, "Connecting to WiFi");

  setupWifi();

  // OTA Setup
  String hostname(HOSTNAME);
  hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname);
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.begin();
  SPIFFS.begin();

  //Uncomment if you want to update all internet resources
  //SPIFFS.format();

  // download images from the net. If images already exist don't download
  downloadResources();

  // load the weather information
  updateData();

  setupServer();
  setupMDNS();
  
  Serial.println("Setup OK.");
}

long lastDrew = 0;


void loop() {
  
  if (USE_TOUCHSCREEN_WAKE) {     // determine in settings.h
    
    // for AWAKE_TIME seconds we'll hang out and wait for OTA updates
    for (uint16_t i=0; i<AWAKE_TIME; i++  ) {
      // Handle OTA update requests
      ArduinoOTA.handle();

      // Handle web client requests
      server.handleClient();
      
      delay(10000);
      yield();
    }

    // turn off the display and wait for a touch!
    // flush the touch buffer
    while (spitouch.bufferSize() != 0) {
      spitouch.getPoint();
      //Serial.print('.');
      yield();
    }
    
    Serial.println("Zzzz");
    spitouch.writeRegister8(STMPE_GPIO_CLR_PIN, _BV(2)); // backlight off  
    spitouch.writeRegister8(STMPE_INT_STA, 0xFF);

    if (DEEP_SLEEP) {
      sleepNow(STMPE_IRQ);
    } else {
      while (! spitouch.touched()) {
        // twiddle thumbs
        delay(10);
      }
    }
    Serial.println("Touch detected!");
  
    // wipe screen & backlight on
    tft.fillScreen(ILI9341_BLACK);
    spitouch.writeRegister8(STMPE_GPIO_SET_PIN, _BV(2));
    updateData();
  } 
  else // "standard setup"
  {
    // Handle OTA update requests
    ArduinoOTA.handle();

    // Handle web client requests
      server.handleClient();

    // Check if we should update the clock
    if (millis() - lastDrew > 30000) {
      drawTime();
      if (ACTUAL_TEMP) {
        drawTemp();
      }
      lastDrew = millis();
    }

    // Check if we should update weather information
    if (millis() - lastDownloadUpdate > 1000 * UPDATE_INTERVAL_SECS) {
      updateData();
      lastDownloadUpdate = millis();
    }
  }
  
}

// Called if WiFi has not been configured yet
void configModeCallback (WiFiManager *myWiFiManager) {
  ui.setTextAlignment(CENTER);
  tft.setFont(&ArialRoundedMTBold_14);
  tft.setTextColor(ILI9341_CYAN);
  ui.drawString(120, 28, "Wifi Manager");
  ui.drawString(120, 42, "Please connect to AP");
  tft.setTextColor(ILI9341_WHITE);
  ui.drawString(120, 56, myWiFiManager->getConfigPortalSSID());
  tft.setTextColor(ILI9341_CYAN);
  ui.drawString(120, 70, "To setup Wifi Configuration");
}

// callback called during download of files. Updates progress bar
void downloadCallback(String filename, int16_t bytesDownloaded, int16_t bytesTotal) {
  Serial.println(String(bytesDownloaded) + " / " + String(bytesTotal));

  int percentage = 100 * bytesDownloaded / bytesTotal;
  if (percentage == 0) {
    ui.drawString(120, 160, filename);
  }
  if (percentage % 5 == 0) {
    ui.setTextAlignment(CENTER);
    ui.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
    //ui.drawString(120, 160, String(percentage) + "%");
    ui.drawProgressBar(10, 165, 240 - 20, 15, percentage, ILI9341_WHITE, ILI9341_BLUE);
  }

}

// Download the bitmaps
void downloadResources() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(&ArialRoundedMTBold_14);
  char id[5];
  for (int i = 0; i < 21; i++) {
    sprintf(id, "%02d", i);
    tft.fillRect(0, 120, 240, 40, ILI9341_BLACK);
    webResource.downloadFile("http://www.squix.org/blog/wunderground/" + wundergroundIcons[i] + ".bmp", wundergroundIcons[i] + ".bmp", _downloadCallback);
  }
  for (int i = 0; i < 21; i++) {
    sprintf(id, "%02d", i);
    tft.fillRect(0, 120, 240, 40, ILI9341_BLACK);
    webResource.downloadFile("http://www.squix.org/blog/wunderground/mini/" + wundergroundIcons[i] + ".bmp", "/mini/" + wundergroundIcons[i] + ".bmp", _downloadCallback);
  }
  for (int i = 0; i < 23; i++) {
    tft.fillRect(0, 120, 240, 40, ILI9341_BLACK);
    webResource.downloadFile("http://www.squix.org/blog/moonphase_L" + String(i) + ".bmp", "/moon" + String(i) + ".bmp", _downloadCallback);
  }
}

// Update the internet based information and update screen
void updateData() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(&ArialRoundedMTBold_14);
  drawProgress(20, "Updating time...");
  timeClient.updateTime();
  drawProgress(50, "Updating conditions...");
  wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(70, "Updating forecasts...");
  wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(90, "Updating astronomy...");
  wunderground.updateAstronomy(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  //lastUpdate = timeClient.getFormattedTime();
  //readyForWeatherUpdate = false;
  drawProgress(100, "Done...");
  delay(1000);
  tft.fillScreen(ILI9341_BLACK);
  drawTime();
  drawCurrentWeather();
  drawForecast();
  if (!ACTUAL_TEMP) {
    drawAstronomy();
  }
}

// Progress bar helper
void drawProgress(uint8_t percentage, String text) {
  ui.setTextAlignment(CENTER);
  ui.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.fillRect(0, 140, 240, 45, ILI9341_BLACK);
  ui.drawString(120, 160, text);
  ui.drawProgressBar(10, 165, 240 - 20, 15, percentage, ILI9341_WHITE, ILI9341_BLUE);
}

// draws the clock
void drawTime() {
  ui.setTextAlignment(CENTER);
  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&ArialRoundedMTBold_14);
  String date = wunderground.getDate();
  ui.drawString(120, 20, date);
  
  tft.setFont(&ArialRoundedMTBold_36);
  String time = timeClient.getHours() + ":" + timeClient.getMinutes();
  ui.drawString(120, 56, time);
  drawSeparator(65);
}

// draws current weather information
void drawCurrentWeather() {
  // Weather Icon
  String weatherIcon = getMeteoconIcon(wunderground.getTodayIcon());
  ui.drawBmp(weatherIcon + ".bmp", 0, 55);
  
  // Weather Text
  tft.setFont(&ArialRoundedMTBold_14);
  ui.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  ui.setTextAlignment(RIGHT);
  ui.drawString(220, 90, wunderground.getWeatherText());

  tft.setFont(&ArialRoundedMTBold_36);
  ui.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  ui.setTextAlignment(RIGHT);
  String degreeSign = "F";
  if (IS_METRIC) {
    degreeSign = ".C";
  }
  String temp = wunderground.getCurrentTemp() + degreeSign;
  ui.drawString(220, 125, temp);
  drawSeparator(135);

}

// draws the three forecast columns
void drawForecast() {
  drawForecastDetail(10, 165, 0);
  drawForecastDetail(95, 165, 2);
  drawForecastDetail(180, 165, 4);
  drawSeparator(165 + 65 + 10);
}

// helper for the forecast columns
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex) {
  ui.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setFont(&ArialRoundedMTBold_14);
  ui.setTextAlignment(CENTER);
  String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
  day.toUpperCase();
  ui.drawString(x + 25, y, day);

  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  ui.drawString(x + 25, y + 14, wunderground.getForecastLowTemp(dayIndex) + "|" + wunderground.getForecastHighTemp(dayIndex));
  
  String weatherIcon = getMeteoconIcon(wunderground.getForecastIcon(dayIndex));
  ui.drawBmp("/mini/" + weatherIcon + ".bmp", x, y + 15);
    
}

// draw moonphase and sunrise/set and moonrise/set
void drawAstronomy() {
  int moonAgeImage = 24 * wunderground.getMoonAge().toInt() / 30.0;
  ui.drawBmp("/moon" + String(moonAgeImage) + ".bmp", 120 - 30, 255);
  
  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&ArialRoundedMTBold_14);  
  ui.setTextAlignment(LEFT);
  ui.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  ui.drawString(20, 270, SUN);
  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  ui.drawString(20, 285, wunderground.getSunriseTime());
  ui.drawString(20, 300, wunderground.getSunsetTime());

  ui.setTextAlignment(RIGHT);
  ui.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  ui.drawString(220, 270, LUNAR);
  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  ui.drawString(220, 285, wunderground.getMoonriseTime());
  ui.drawString(220, 300, wunderground.getMoonsetTime());
  
}

// draw moonphase and sunrise/set and moonrise/set
void drawTemp() {
  
  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&ArialRoundedMTBold_14);  
  ui.setTextAlignment(LEFT);
  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  ui.drawString(20, 270, "Temp.");
  ui.setTextAlignment(RIGHT);
  ui.drawString(220, 270, "Humi.");
  ui.setTextColor(ILI9341_CYAN, ILI9341_BLACK);

  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  
  Serial.println("TEMP");
  Serial.println(t);
  Serial.println("HUMI");
  Serial.println(h);

  String result_t = "0.C";
  String result_h = "0 %";

  if (h > 0.0 && h < 110.0 && t > -1 && t < 150) {
    result_t.replace("0", static_cast<String>(static_cast<int>(t)));
    result_h.replace("0", static_cast<String>(static_cast<int>(h)));
  }
  
  ui.setTextAlignment(LEFT);
  tft.setFont(&ArialRoundedMTBold_36);  
  ui.drawString(20, 300, result_t);
  ui.setTextAlignment(RIGHT);
  ui.drawString(220, 300, result_h);
  
}


// Helper function, should be part of the weather station library and should disappear soon
String getMeteoconIcon(String iconText) {
  if (iconText == "F") return "chanceflurries";
  if (iconText == "Q") return "chancerain";
  if (iconText == "W") return "chancesleet";
  if (iconText == "V") return "chancesnow";
  if (iconText == "S") return "chancetstorms";
  if (iconText == "B") return "clear";
  if (iconText == "Y") return "cloudy";
  if (iconText == "F") return "flurries";
  if (iconText == "M") return "fog";
  if (iconText == "E") return "hazy";
  if (iconText == "Y") return "mostlycloudy";
  if (iconText == "H") return "mostlysunny";
  if (iconText == "H") return "partlycloudy";
  if (iconText == "J") return "partlysunny";
  if (iconText == "W") return "sleet";
  if (iconText == "R") return "rain";
  if (iconText == "W") return "snow";
  if (iconText == "B") return "sunny";
  if (iconText == "0") return "tstorms";
  

  return "unknown";
}

// if you want separators, uncomment the tft-line
void drawSeparator(uint16_t y) {
   //tft.drawFastHLine(10, y, 240 - 2 * 10, 0x4228);
}

