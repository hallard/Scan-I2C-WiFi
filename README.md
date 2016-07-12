ESP8266 I2C Scanner with oled display, WS2812 RGB Led and OTA
=============================================================

This is a sample sketch do demonstrate how to use ESP8266 with OLED, I2C Scanner, OTA and OLED Display.

I used it on WeMos shield, you can find more information on WeMos on their [site][1], it's really well documented.
I now use WeMos boards instead of NodeMCU's one because they're just smaller, features remains the same, but I also suspect WeMos regulator far better quality than the one used on NodeMCU that are just fake of originals AMS117 3V3.

I'm using it on lot of my boards to test them since I'm often using WS2812B RGB LED, OLED and I2C devices. On this sample. For example in 

- WeMos [RN2483 Shield][8]
- WeMos [Lora Shield][7]
- WeMos [RFM69 Gateway Shield][9]
- NodeMCU [Gateway Shield][10]

This project is mainly based on different samples sketches that I merged and adapted.

Documentation
-------------
just change in the Scan-I2C-WiFi.ino the definition according to the board you're using and the wiring.

```arduino
/ ===========================================
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

````

Dependencies
------------
- Arduino [ESP8266][6] environement
- @squix78 for excellent OLED [library][4] 
- @Makuna for no less excellent RGB Led library called [NeoPixelBus][5] library 

Running sketch Video
--------------------

Here a little video (sorry for the quality, just took with my iPhone 5) of rendering

[![ESP8266 I2C WiFi scanner](http://img.youtube.com/vi/57EoCybljsQ/0.jpg)](http://www.youtube.com/watch?v=57EoCybljsQ)


Misc
----
See news and other projects on my [blog][2] 
 
[1]: http://www.wemos.cc/
[2]: https://hallard.me
[4]: https://github.com/squix78/esp8266-oled-ssd1306
[5]: https://github.com/Makuna/NeoPixelBus
[6]: https://github.com/esp8266/Arduino/blob/master/README.md
[7]: https://github.com/hallard/WeMos-Lora
[8]: https://github.com/hallard/WeMos-RN2483
[9]: https://github.com/hallard/WeMos-RFM69
[10]: https://github.com/hallard/NodeMCU-Gateway