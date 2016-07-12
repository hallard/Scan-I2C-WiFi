// **********************************************************************************
// Scan-I2C-WiFi
// **********************************************************************************
// Written by Charles-Henri Hallard (http://hallard.me)
//
// History : V1.00 2014-04-21 - First release
//         : V1.11 2015-09-23 - rewrite for ESP8266 target
//         : V1.20 2016-07-13 - Added new OLED Library and NeoPixelBus
//
// **********************************************************************************

#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <Wire.h>
#include "icons.h"
#include "fonts.h"

// ===========================================
// Setup your board configuration here
// ===========================================

// Wifi Credentials
// Leaving * will try to connect with SDK saved credentials
#define MY_SSID     "*******"
#define MY_PASSWORD "*******"

char ssid[33] ;
char password[65];

// I2C Pins Settings
#define SDA_PIN 4
#define SDC_PIN 5

// Display Settings
// OLED will be checked with this address and this address+1
// so here 0x03c and 0x03d
#define I2C_DISPLAY_ADDRESS 0x3c
// Choose OLED Type (one only)
#define OLED_SSD1306
//#define OLED_SH1106

// RGB Led on GPIO0 comment this line if you have no LED
#define RGB_LED_PIN 0
// 2 LEDs
#define RGB_LED_COUNT 2
// Comment if you have only RGB LED and not RGBW led 
#define RGBW_LED 

// ===========================================
// End of configuration
// ===========================================

#ifdef RGB_LED_PIN 
#include <NeoPixelBus.h>
#endif

#if defined (OLED_SSD1306)
#include <SSD1306Wire.h>
#include <OLEDDisplayUi.h>
#elif defined (OLED_SH1106)
#include <SH1106Wire.h>
#include <OLEDDisplayUi.h>
#endif


// value for HSL color
// see http://www.workwithcolor.com/blue-color-hue-range-01.htm
#define COLOR_RED              0
#define COLOR_ORANGE          30
#define COLOR_ORANGE_YELLOW   45
#define COLOR_YELLOW          60
#define COLOR_YELLOW_GREEN    90
#define COLOR_GREEN          120
#define COLOR_GREEN_CYAN     165
#define COLOR_CYAN           180
#define COLOR_CYAN_BLUE      210
#define COLOR_BLUE           240
#define COLOR_BLUE_MAGENTA   275
#define COLOR_MAGENTA        300
#define COLOR_PINK           350

#ifdef RGB_LED_PIN 
#ifdef RGBW_LED
  NeoPixelBus<NeoGrbwFeature, NeoEsp8266BitBang800KbpsMethod>rgb_led(RGB_LED_COUNT, RGB_LED_PIN);
#else
  NeoPixelBus<NeoRgbFeature, NeoEsp8266BitBang800KbpsMethod>rgb_led(RGB_LED_COUNT, RGB_LED_PIN);
#endif 
#endif

// Number of line to display for devices and Wifi
#define I2C_DISPLAY_DEVICE  4
#define WIFI_DISPLAY_NET    4
#ifdef OLED_SSD1306
SSD1306Wire  display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
#else
SH1106Wire  display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
#endif
OLEDDisplayUi ui( &display );

Ticker ticker;    
bool readyForUpdate = false;  // flag to launch update (I2CScan)

bool has_display          = false;  // if I2C display detected
uint8_t NumberOfI2CDevice = 0;      // number of I2C device detected
int8_t NumberOfNetwork    = 0;      // number of wifi networks detected
uint8_t rgb_luminosity    = 50 ;    // Luminosity from 0 to 100% 

char i2c_dev[I2C_DISPLAY_DEVICE][32]; // Array on string displayed

#ifdef RGB_LED_PIN 
void LedRGBOFF(uint16_t led=0);
void LedRGBON (uint16_t hue, uint16_t led=0);
#else
void LedRGBOFF(uint16_t led=0) {};
void LedRGBON (uint16_t hue, uint16_t led) {};
#endif


#ifdef RGB_LED_PIN 
/* ======================================================================
Function: LedRGBON
Purpose : Set RGB LED strip color, but does not lit it
Input   : Hue of LED (0..360)
          led number (from 1 to ...), if 0 then all leds
Output  : - 
Comments: 
====================================================================== */
void LedRGBON (uint16_t hue, uint16_t led)
{
  uint8_t start = 0;
  uint8_t end   = RGB_LED_COUNT-1; // Start at 0

  // Convert to neoPixel API values
  // H (is color from 0..360) should be between 0.0 and 1.0
  // S is saturation keep it to 1
  // L is brightness should be between 0.0 and 0.5
  // rgb_luminosity is between 0 and 100 (percent)
  RgbColor target = HslColor( hue / 360.0f, 1.0f, 0.005f * rgb_luminosity);

  // just one LED ?
  // Strip start 0 not 1
  if (led) {
    led--;
    start = led ;
    end   = start ;
  } 

  for (uint8_t i=start ; i<=end; i++) {
    rgb_led.SetPixelColor(i, target);
    rgb_led.Show();  
  }
}

/* ======================================================================
Function: LedRGBOFF 
Purpose : light off the RGB LED strip
Input   : Led number starting at 1, if 0=>all leds
Output  : - 
Comments: -
====================================================================== */
void LedRGBOFF(uint16_t led)
{
  uint8_t start = 0;
  uint8_t end   = RGB_LED_COUNT-1; // Start at 0

  // just one LED ?
  if (led) {
    led--;
    start = led ;
    end   = start ;
  }

  // stop animation, reset params
  for (uint8_t i=start ; i<=end; i++) {
    // clear the led strip
    rgb_led.SetPixelColor(i, RgbColor(0));
    rgb_led.Show();  
  }
}
#endif

/* ======================================================================
Function: i2cScan
Purpose : scan I2C bus
Input   : specifc address if looking for just 1 specific device
Output  : number of I2C devices seens
Comments: -
====================================================================== */
uint8_t i2c_scan(uint8_t address=0xff)
{
  uint8_t error;
  int nDevices;
  uint8_t start = 1 ;
  uint8_t end   = 0x7F ;
  uint8_t index = 0;
  char device[16];
  char buffer[32];

  if (address >= start && address <= end) {
    start = address;
    end   = address+1;
    Serial.print(F("Searching for device at address 0x"));
    Serial.printf("%02X ", address);
  } else {
    Serial.println(F("Scanning I2C bus ..."));
  }

  nDevices = 0;
  for(address = start; address < end; address++ ) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("Device ");
      
      if (address == 0x40)
        strcpy(device, "TH02" );
      else if (address==0x29 || address==0x39 || address==0x49)
        strcpy(device, "TSL2561" );
      else if (address==I2C_DISPLAY_ADDRESS || address==I2C_DISPLAY_ADDRESS+1)
        strcpy(device, "OLED SSD1306" );
      else if (address==0x64)
        strcpy(device, "ATSHA204" );
      else 
        strcpy(device, "Unknown" );

      sprintf(buffer, "0x%02X : %s", address, device );
      if (index<I2C_DISPLAY_DEVICE) {
        strcpy(i2c_dev[index++], buffer );
      }
      
      Serial.println(buffer);
      nDevices++;
    }
    else if (error==4) 
    {
      Serial.printf("Unknow error at address 0x%02X", address);
    }    

    yield();
  }
  if (nDevices == 0)
    Serial.println(F("No I2C devices found"));
  else
    Serial.printf("Scan done, %d device found\r\n", nDevices);

  return nDevices;
}


/* ======================================================================
Function: updateData
Purpose : update by rescanning I2C bus
Input   : OLED display pointer
Output  : -
Comments: -
====================================================================== */
void updateData(OLEDDisplay *display) {
  // connected 
  if ( WiFi.status() == WL_CONNECTED  ) {
    LedRGBON(COLOR_GREEN);
  } else {
    LedRGBON(COLOR_ORANGE);
  }

  drawProgress(display, 0, "Scanning I2C...");
  NumberOfI2CDevice = i2c_scan();
  // Simulate slow scan to be able to see on display
  for (uint8_t i=1; i<100; i++) {
    drawProgress(display, i, "Scanning I2C...");
    delay(2);
  }
  drawProgress(display, 100, "Done...");
  readyForUpdate = false;
  LedRGBOFF();
}

/* ======================================================================
Function: drawProgress
Purpose : prograss indication 
Input   : OLED display pointer
          percent of progress (0..100)
          String above progress bar
          String below progress bar
Output  : -
Comments: -
====================================================================== */
void drawProgress(OLEDDisplay *display, int percentage, String labeltop, String labelbot) {
  if (has_display) {
    display->clear();
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(Roboto_Condensed_Bold_Bold_16);
    display->drawString(64, 8, labeltop);
    display->drawProgressBar(10, 28, 108, 12, percentage);
    display->drawString(64, 48, labelbot);
    display->display();
  }
}

/* ======================================================================
Function: drawProgress
Purpose : prograss indication 
Input   : OLED display pointer
          percent of progress (0..100)
          String above progress bar
Output  : -
Comments: -
====================================================================== */
void drawProgress(OLEDDisplay *display, int percentage, String labeltop ) {
  drawProgress(display, percentage, labeltop, String(""));
}

/* ======================================================================
Function: drawFrameWifi
Purpose : WiFi logo and IP address
Input   : OLED display pointer
Output  : -
Comments: -
====================================================================== */
void drawFrameWifi(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(Roboto_Condensed_Bold_Bold_16);
  // see http://blog.squix.org/2015/05/esp8266-nodemcu-how-to-create-xbm.html
  // on how to create xbm files
  display->drawXbm( x + (128-WiFi_width)/2, 0, WiFi_width, WiFi_height, WiFi_bits);
  display->drawString(x+ 64, WiFi_height+4, WiFi.localIP().toString());
  ui.disableIndicator();
}

/* ======================================================================
Function: drawFrameI2C
Purpose : I2C info screen (called by OLED ui)
Input   : OLED display pointer
Output  : -
Comments: -
====================================================================== */
void drawFrameI2C(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  char buff[16];
  sprintf(buff, "%d I2C Device%c",NumberOfI2CDevice, NumberOfI2CDevice>1?'s':' ');

  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(Roboto_Condensed_Bold_Bold_16);
  display->drawString(x + 64, y +  0, buff);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  //display->setFont(Roboto_Condensed_Plain_16);
  display->setFont(Roboto_Condensed_12);

  for (uint8_t i=0; i<NumberOfI2CDevice; i++) {
    if (i<I2C_DISPLAY_DEVICE)
      display->drawString(x + 0, y + 16 + 12*i, i2c_dev[i]);
  }
  ui.disableIndicator();
}

/* ======================================================================
Function: drawFrameNet
Purpose : WiFi network info screen (called by OLED ui)
Input   : OLED display pointer
Output  : -
Comments: -
====================================================================== */
void drawFrameNet(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  char buff[64];
  sprintf(buff, "%d Wifi Network",NumberOfNetwork);
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(Roboto_Condensed_Bold_Bold_16);
  display->drawString(x + 64, y + 0 , buff);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(Roboto_Condensed_12);

  for (int i=0; i < NumberOfNetwork; i++) {
    // Print SSID and RSSI for each network found
    if (i<WIFI_DISPLAY_NET) {
      sprintf(buff, "%s %c", WiFi.SSID(i).c_str(), WiFi.encryptionType(i)==ENC_TYPE_NONE?' ':'*' );
      display->drawString(x + 0, y + 16 + 12*i, buff);
    }
  }

  ui.disableIndicator();
 }

/* ======================================================================
Function: drawFrameLogo
Purpose : Company logo info screen (called by OLED ui)
Input   : OLED display pointer
Output  : -
Comments: -
====================================================================== */
void drawFrameLogo(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->clear();
  display->drawXbm(x + (128-ch2i_width)/2, y, ch2i_width, ch2i_height, ch2i_bits);
  ui.disableIndicator();
}

// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = { drawFrameWifi, drawFrameI2C, drawFrameNet, drawFrameLogo};
int numberOfFrames = 4;


/* ======================================================================
Function: setReadyForUpdate
Purpose : Called by ticker to tell main loop we need to update data
Input   : -
Output  : -
Comments: -
====================================================================== */
void setReadyForUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForUpdate = true;
}

/* ======================================================================
Function: setup
Purpose : you should know ;-)
====================================================================== */
void setup() 
{
  uint16_t led_color ;
  char thishost[33];
  uint8_t pbar = 0;

  Serial.begin(115200);
  Serial.print(F("\r\nBooting on "));
  Serial.println(ARDUINO_BOARD);

  LedRGBOFF();

  //Wire.pins(SDA, SCL); 
  Wire.begin(SDA_PIN, SDC_PIN);
  Wire.setClock(100000);

  if (i2c_scan(I2C_DISPLAY_ADDRESS)) {
    has_display = true;
  } else {
    if (i2c_scan(I2C_DISPLAY_ADDRESS+1)) {
      has_display = true;
    }
  }

  if (has_display) {
    Serial.println(F("Display found"));
    // initialize dispaly
    display.init();
    display.flipScreenVertically();
    display.clear();
    display.drawXbm((128-ch2i_width)/2, 0, ch2i_width, ch2i_height, ch2i_bits);
    display.display();

    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setContrast(255);
    delay(500);
  }

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  delay(100);

  strcpy(ssid, MY_SSID);
  strcpy(password, MY_PASSWORD);

  // empty sketch SSID, try with SDK ones
  if ( *ssid=='*' && *password=='*' ) {
    // empty sketch SSID, try autoconnect with SDK saved credentials
    Serial.println(F("No SSID/PSK defined in sketch\r\nConnecting with SDK ones if any"));
    struct station_config conf;
    wifi_station_get_config(&conf);
    strcpy(ssid, reinterpret_cast<char*>(conf.ssid));
    strcpy(password, reinterpret_cast<char*>(conf.password));
  } 

  Serial.println(F("WiFi scan start"));
  drawProgress(&display, pbar, F("Scanning WiFi"));

  // WiFi.scanNetworks will return the number of networks found
  led_color = 0;
  NumberOfNetwork = 0;
  WiFi.scanNetworks(true);
  // Async, wait start
  while (WiFi.scanNetworks()!=WIFI_SCAN_RUNNING );

  do {
    LedRGBON(led_color);
    delay(5);
   
    // Rainbow loop
    if (++led_color>360)
      led_color=360;

    // WiFi scan max 50% of progress bar 
    pbar = led_color * 100 / 360 / 2;
    drawProgress(&display, pbar, F("Scanning WiFi"));

    NumberOfNetwork = WiFi.scanComplete();
    //Serial.printf("NumberOfNetwork=%d\n", NumberOfNetwork);
  } while (NumberOfNetwork==WIFI_SCAN_RUNNING || NumberOfNetwork==WIFI_SCAN_FAILED);

  Serial.println(F("scan done"));

  LedRGBOFF();

  Serial.println(F("I2C scan start"));
  pbar = 50;
  drawProgress(&display, pbar, F("Scanning I2C"));
  delay(200);
  NumberOfI2CDevice = i2c_scan();
  Serial.println(F("scan done"));

  // Set Hostname for OTA and network (add only 2 last bytes of last MAC Address)
  sprintf_P(thishost, PSTR("ScanI2CWiFi-%04X"), ESP.getChipId() & 0xFFFF);

  Serial.printf("connecting to %s with psk %s\r\n", ssid, password );
  WiFi.begin(ssid, password);

  // Loop until connected or 20 sec time out
  #define WIFI_TIME_OUT 20
  unsigned long this_start = millis();
  led_color = 360;
  while ( WiFi.status() !=WL_CONNECTED && millis()-this_start < (WIFI_TIME_OUT * 1000) ) {
    // 125 ms wait 
    for (uint8_t j = 0; j<125; j++) {
      // Rainbow loop
      LedRGBON(led_color);
      if (led_color==0) {
        led_color=360;
      } else {
        led_color--;
      }
      delay(1); 
    }
    if (pbar++>99) {
      pbar = 99;
    }
    drawProgress(&display, pbar, F("Connecting WiFi"), ssid);
  }

  if (  WiFi.status() == WL_CONNECTED  ) {
    Serial.printf("OK from %s@", thishost);
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("Error unable to connect to WiFi"));
  }

  LedRGBOFF();

  Serial.println(F("Setup done"));
  drawProgress(&display, 100, F("Setup Done"));

  ArduinoOTA.setHostname(thishost);
  ArduinoOTA.begin();

  // OTA callbacks
  ArduinoOTA.onStart([]() { 
    // Light of the LED, stop animation
    LedRGBOFF();
    Serial.println(F("\r\nOTA Starting")); 
    drawProgress(&display, 0, "Starting OTA");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) { 
    char buff[8];
    uint8_t percent=progress/(total/100); 
    sprintf(buff, "%d%%", percent);
    drawProgress(&display, percent, "Uploading", String(buff));
    Serial.printf("%03d %%\r", percent); 

    // hue from 0.0 to 1.0 (rainbow) with 33% (of 0.5f) luminosity
    // With blink
    if (percent % 4 >= 2) {
      LedRGBON( percent * 360 / 100);
    } else {
      LedRGBOFF();
    }

  });

  ArduinoOTA.onEnd([]() { 
    LedRGBON(COLOR_ORANGE);
    if (has_display) {
      display.clear();
      //display.setTextAlignment(TEXT_ALIGN_CENTER);
      //display.setFont(Roboto_Condensed_Bold_Bold_16);
      display.drawString(64,  8, F("Writing Flash"));
      display.drawString(64, 22, F("..."));
      display.drawString(64, 40, F("Please wait"));
      display.display();
    }

    Serial.println(F("\r\nDone Rebooting")); 
  });

  ArduinoOTA.onError([](ota_error_t error) { 
    LedRGBON(COLOR_RED);
    drawProgress(&display, 0, "Error");
    Serial.println(F("\r\nError")); 
  });

  if (has_display) {
    ui.setTargetFPS(30);
    ui.setFrameAnimation(SLIDE_LEFT);
    ui.setFrames(frames, numberOfFrames);
    ui.init();
    display.flipScreenVertically();
  }

  //updateData(&display);

  // Rescan I2C every 10 seconds
  ticker.attach(10, setReadyForUpdate);

}

/* ======================================================================
Function: loop
Purpose : you should know ;-)
====================================================================== */
void loop() 
{
  if (has_display) {
    if (readyForUpdate && ui.getUiState()->frameState == FIXED) {
      updateData(&display);
    }

    int remainingTimeBudget = ui.update();

    if (remainingTimeBudget > 0) {
      // You can do some work here
      // Don't do stuff if you are below your
      // time budget.
      delay(remainingTimeBudget);
    }
  } else {
    if (readyForUpdate ) {
      updateData(&display);
    }
  }

  // Handle OTA
  ArduinoOTA.handle();
}

