/*
    Inkplate6_Image_Frame_From_Web example for Soldered Inkplate 6
    For this example you will need only USB cable and Inkplate 6.
    Select "e-radionica Inkplate6" or "Soldered Inkplate6" from Tools -> Board menu.
    Don't have "e-radionica Inkplate6" or "Soldered Inkplate6" option? Follow our tutorial and add it:
    https://soldered.com/learn/add-inkplate-6-board-definition-to-arduino-ide/

    This example shows how you can set inkplate to show random pictures from web.

    Want to learn more about Inkplate? Visit www.inkplate.io
    Looking to get support? Write on our forums: https://forum.soldered.com/
    28 July 2020 by Soldered
*/

// Next 3 lines are a precaution, you can ignore those, and the example would also work without them
#if !defined(ARDUINO_ESP32_DEV) && !defined(ARDUINO_INKPLATE6V2)
#error "Wrong board selection for this example, please select e-radionica Inkplate6 or Soldered Inkplate6 in the boards menu."
#endif

#include "Inkplate.h"

Inkplate display(INKPLATE_3BIT);

const char ssid[] = ""; // Your WiFi SSID
const char *password = "";     // Your WiFi password

void setup()
{
    Serial.begin(115200);
    display.begin();

    // Join wifi
    display.joinAP(ssid, password);
    Serial.println("joined wifi");
    char url[256];
    imageUrl(url);

    Serial.println(display.drawImage(url, display.PNG, 0, 0));
    display.display();

    Serial.println("Going to sleep");
    esp_sleep_enable_timer_wakeup(15ll * 60 * 1000 * 1000);
    esp_deep_sleep_start();
}

void loop()
{
    // Never here, as deepsleep restarts esp32
}

void imageUrl(char *a)
{
    String url;
    HTTPClient http;
    if (http.begin("https://source.unsplash.com/random/800x600") && http.GET() > 0)
    {
        url = http.getString();

        int urlStart = url.indexOf("href=\"") + 6;
        int urlEnd = url.indexOf("\">", urlStart);

        url = url.substring(urlStart, urlEnd);
        url = url.substring(0, url.indexOf("?")) + "?crop=entropy&fit=crop&fm=png&h=600&w=800";

        Serial.println(url);
        strcpy(a, url.c_str());
    }
    else
    {
        display.println("HTTP error");
        display.display();
    }
    http.end();
}
