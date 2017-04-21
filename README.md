# WeatherStation
Weather station using Arduino
The goal of this project is to allow a termometer accuracy. It is composed of a screen, a wemos D1 and a web interface to check for the accuracy and set the BPM.<BR>

## Hardware requirements <BR>
- Weemos D1 (ESP8266, Arduino-compatible layout, wifi, 80/160Mhz, 4Mb flash)<BR>
[![Weemos D1 - Face](http://i.imgur.com/Wp4gmGz.jpg)](http://i.imgur.com/Wp4gmGz.jpg)
[![Weemos D1 - Back](http://i.imgur.com/7sasqUQ.jpg)](http://i.imgur.com/7sasqUQ.jpg)<BR>

- 2.4" TFT LCD Shield Touch Panel Module Micro SD (For Arduino UNO compatible)<BR>
[![Screen - Face](http://i.imgur.com/AzSjkEK.jpg)](http://i.imgur.com/AzSjkEK.jpg)
[![Screen - Back](http://i.imgur.com/h1CRPeX.jpg)](http://i.imgur.com/h1CRPeX.jpg)<BR>

- All in place <BR>
[![All assemblate](http://i.imgur.com/ysy0L4u.jpg)](http://i.imgur.com/ysy0L4u.jpg)<BR>

## Software requirements

[Arduino IDE with ESP8266 platform installed](https://www.arduino.cc/en/main/software)

If you use windows / OSx you will probably need drivers: [Wemos Driver](https://www.wemos.cc/downloads)


## Install

### Clone project

```sh
git clone https://github.com/ismailnguyen/WeatherStation.git  
```

### Setup environment

1. Start Arduino IDE

#### Setup the board

1. Open the Preferences window
2. In the Additional Board Manager URLs field, enter: http://arduino.esp8266.com/versions/2.3.0/package_esp8266com_index.json
3. Open `Tools` -> `Board` -> `Boards Manager...`
4. Type `esp` and the `esp8266` platform will appear, install it
5. Select your board: `Tools` -> `Board` -> `Wemos D1 R2 & mini`

#### Add external libraries

1. Open `Sketch` -> `Include Library` -> `Manage Libraries...`
2. Install `WiFiManager`
3. Install `Adafruit NeoPixel`
4. Install `elapsedMillis`
5. Install `Weather Station Library`
6. Install `Adafruit ILI9341`
7. Install `Adafruit GFX`
8. Install `WifiManager`

### Upload `WeatherStation` project to your Arduino

1. Open the BitWeather project
2. Open monitor: `Tools` -> `Serial Monitor`
3. Maybe you will have to select the port: `Tools` -> `Port`

### Set the network connection

1. Upload the program, you should now see the access point `BitWeather` on your computer !
2. Connect your computer to it
3. Open your webbrowser
4. Configure the network by the web interface (SSID + password)

## Steps followed
We made the project by following these steps :

- Creating the weather Station :
  - Code and hardware :
    - Fixe the screen on the Weemos
    - swith the thread
      - IHM
        - screen on weemos


## Wiring - Pin's Correspondence (ARDUINO / Screen)

See code for pin configurations

Weemos D1 Mini | ILI9341 Screen
--- | --- 
3V3 | VCC
GND | GND
D2 | CS
RST | RESET
D4 | D/C
D7 | SDI (MOSI)
D5 | SCK
3V3 | LED


Weemos D1 Mini | DHT (Temperature and Humidity)
--- | --- 
3V3 | 3V3
GND | GND
D3  | D4




## Sources
- [Original Projet Inspiration](https://github.com/squix78/esp8266-weather-station-color)
- [ESP8266 Weather Library](https://github.com/squix78/esp8266-weather-station)
- [Adafruit Unified Sensor](https://github.com/adafruit/Adafruit_Sensor)
- [Arduino core for ESP8266](https://github.com/esp8266/Arduino)
- [Font for arduino screen](http://oleddisplay.squix.ch)
- [Wifi Manager](https://github.com/tzapu/WiFiManager)
