#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_STMPE610.h"
#include "GfxUi.h"
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <Adafruit_GFX.h>           //Interface graphique
#include <Adafruit_ILI9341.h>       //Hardware
#include "ArialRoundedMTBold_14.h"  //Font size 14 by http://oleddisplay.squix.ch/
#include "ArialRoundedMTBold_36.h"  //Font size 36 by http://oleddisplay.squix.ch/
#include "WebResource.h"            //For Download
#include <DNSServer.h>              //Local DNS Server used for redirecting all requests to the &onfiguration portal
#include <ESP8266WebServer.h>       //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>            //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ESP8266mDNS.h>            //Url personnalisée BitWeather.local
#include "DHT.h"                    //Temp & Humidity Driver
#include "settings.h"               //Parametrage
#include <JsonListener.h>           //Librarie JSON
#include <WundergroundClient.h>     //Librairie pour récupérer les prévisions
#include "TimeClient.h"
#include "html.h"                   //Page HTML

#define DHTPIN    D3
#define DHTTYPE   DHT11
#define HOSTNAME  "ESP8266-OTA-"

// Initialisation des variables
String              current_WUNDERGRROUND_API_KEY = WUNDERGRROUND_API_KEY;
String              current_WUNDERGROUND_CITY = WUNDERGROUND_CITY;
String              current_WUNDERGROUND_CITY_CODE = WUNDERGROUND_CITY_CODE;
float               current_UTC_OFFSET = UTC_OFFSET;
boolean             current_IS_METRIC = IS_METRIC;
boolean             current_ACTUAL_TEMP = ACTUAL_TEMP;
Adafruit_ILI9341    tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
GfxUi               ui = GfxUi(&tft);
Adafruit_STMPE610   spitouch = Adafruit_STMPE610(STMPE_CS);
WebResource         webResource;
TimeClient          timeClient(UTC_OFFSET);
WundergroundClient  wunderground(current_IS_METRIC);
long                lastDownloadUpdate = millis();
DHT                 dht(DHTPIN, DHTTYPE);
WiFiManager         wifiManager;
ESP8266WebServer    server(80);
long                lastDrew = 0;

//Déclaration des fonctions
void              configModeCallback (WiFiManager *myWiFiManager);
void              downloadCallback(String filename, int16_t bytesDownloaded, int16_t bytesTotal);
ProgressCallback  _downloadCallback = downloadCallback;
void              downloadResources();
void              updateData();
void              drawProgress(uint8_t percentage, String text);
void              drawTime();
void              drawCurrentWeather();
void              drawForecast();
void              drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex);
String            getMeteoconIcon(String iconText);
void              drawAstronomy();

String getHTML() {
  String updatedIndexHTML = html_index;

  updatedIndexHTML.replace("[HTML_VALUE_API_KEY]", current_WUNDERGRROUND_API_KEY);

  Serial.println("SERIAL API");
  Serial.println(current_WUNDERGRROUND_API_KEY);

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

  String current_ACTUAL_TEMP_HTML = "";
  String new_ACTUAL_TEMP_HTML = "";
  if (current_ACTUAL_TEMP) {
    current_ACTUAL_TEMP_HTML = "<option value='Actual_temp'>Temp&eacute;rature locale</option>";
    new_ACTUAL_TEMP_HTML = "<option value='Actual_temp' selected>Temp&eacute;rature locale</option>";
  }
  else {
    current_ACTUAL_TEMP_HTML = "<option value='Moon'>Phase lunaire</option>";
    new_ACTUAL_TEMP_HTML = "<option value='Moon' selected>Phase lunaire</option>";
  }
  updatedIndexHTML.replace(current_ACTUAL_TEMP_HTML, new_ACTUAL_TEMP_HTML);

  return updatedIndexHTML;
}

void handleSettings() {
  // Update settings with form values
  current_WUNDERGRROUND_API_KEY = server.arg("api_key");
  current_WUNDERGROUND_CITY = server.arg("city");
  current_WUNDERGROUND_CITY_CODE = server.arg("city_code");
  current_UTC_OFFSET = server.arg("utc_offset").toFloat();

  if (server.arg("temperature") == "C") {
    current_IS_METRIC = true;
  }
  else {
    current_IS_METRIC = false;
  }

  if (server.arg("info_supp") == "Actual_temp") {
    current_ACTUAL_TEMP = true;
  }
  else {
    current_ACTUAL_TEMP = false;
  }

  // Refresh screen display
  updateData();

  // Return settings page with updated values
  handleRoot();
}

void handleRoot() {
  server.send(200, "text/html", getHTML());
}

void setupServer() {
  // Set root page
  server.on("/", handleRoot);

  // Settings update endpoint
  server.on("/update", handleSettings);

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
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.autoConnect(ssid);
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  if (! spitouch.begin()) {
    Serial.println("STMPE not found?");
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
  ui.drawString(120, 160, "Lancement du WiFi");

  setupWifi();

  // OTA Setup
  String hostname(HOSTNAME);
  hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname);
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.begin();
  SPIFFS.begin();

  //On recupère les images
  downloadResources();

  //On met a jour les données
  updateData();

  setupServer();
  setupMDNS();

  Serial.println("Setup OK.");
}



void loop() {
  ArduinoOTA.handle();

  // On vérifie si il y a un appel client pour la page de paramètrage
  server.handleClient();

  // Toutes les 30s
  if (millis() - lastDrew > 30000) {
    drawTime();
    if (current_ACTUAL_TEMP) {
      drawTemp();
    }
    lastDrew = millis();
  }

  // Toutes les 10 mn (UPDATE_INTERVAL_SECS ==> 10 * 60 = 600)
  if (millis() - lastDownloadUpdate > 1000 * UPDATE_INTERVAL_SECS) {
    updateData();
    lastDownloadUpdate = millis();
  }
}

// Called if WiFi has not been configured yet
void configModeCallback (WiFiManager *myWiFiManager) {
  ui.setTextAlignment(CENTER);
  tft.setFont(&ArialRoundedMTBold_14);
  tft.setTextColor(ILI9341_CYAN);
  ui.drawString(120, 28, "Wifi Manager");
  ui.drawString(120, 42, "Connectez vous sur ce point d acces");
  tft.setTextColor(ILI9341_WHITE);
  ui.drawString(120, 56, myWiFiManager->getConfigPortalSSID());
  tft.setTextColor(ILI9341_CYAN);
  ui.drawString(120, 70, "Pour configurer le wifi");
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
  drawProgress(50, "MAJ du temps...");
  //wunderground.updateConditions(current_WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, current_WUNDERGROUND_CITY); ancienne méthode, on utilise le code propre a la ville maintenant
  wunderground.updateConditions(current_WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, current_WUNDERGROUND_CITY_CODE);
  drawProgress(70, "MAJ Previsions...");
  wunderground.updateForecast(current_WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, current_WUNDERGROUND_CITY);
  drawProgress(90, "MAJ des astres...");
  wunderground.updateAstronomy(current_WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, current_WUNDERGROUND_CITY);
  drawProgress(100, "J ai enfin fini...");
  delay(1000);
  tft.fillScreen(ILI9341_BLACK);
  drawTime();
  drawCurrentWeather();
  drawForecast();
  if (!current_ACTUAL_TEMP) {
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
  String degreeSign = ".F";
  if (current_IS_METRIC) {
    degreeSign = ".C";
  }
  String temp = wunderground.getCurrentTemp() + degreeSign;
  ui.drawString(220, 125, temp);

}

// draws the three forecast columns
void drawForecast() {
  drawForecastDetail(10, 165, 0);
  drawForecastDetail(95, 165, 2);
  drawForecastDetail(180, 165, 4);
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

  if (iconText == "0") return "tstorms";
  if (iconText == "B") return "sunny";
  if (iconText == "E") return "hazy";
  if (iconText == "F") return "flurries";
  if (iconText == "H") return "mostlysunny";
  if (iconText == "J") return "partlysunny";
  if (iconText == "M") return "fog";
  if (iconText == "Q") return "chancerain";
  if (iconText == "R") return "rain";
  if (iconText == "S") return "chancetstorms";
  if (iconText == "V") return "chancesnow";
  if (iconText == "W") return "snow";
  if (iconText == "Y") return "cloudy";

  /*if (iconText == "0") return "tstorms";
    if (iconText == "B") return "clear";
    if (iconText == "B") return "sunny";
    if (iconText == "E") return "hazy";
    if (iconText == "F") return "flurries";
    if (iconText == "F") return "chanceflurries";
    if (iconText == "H") return "mostlysunny";
    if (iconText == "H") return "partlycloudy";
    if (iconText == "J") return "partlysunny";
    if (iconText == "M") return "fog";
    if (iconText == "Q") return "chancerain";
    if (iconText == "R") return "rain";
    if (iconText == "S") return "chancetstorms";
    if (iconText == "V") return "chancesnow";
    if (iconText == "W") return "chancesleet";
    if (iconText == "W") return "sleet";
    if (iconText == "W") return "snow";
    if (iconText == "Y") return "cloudy";
    if (iconText == "Y") return "mostlycloudy";*/

  return "unknown";
}

