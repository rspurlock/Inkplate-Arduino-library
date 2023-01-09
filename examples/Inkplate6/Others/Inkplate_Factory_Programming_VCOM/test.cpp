#include "test.h"

const char sdCardTestStringLength = 100;
const char *testString = {"This is some test string..."};

const char *WSSID = {""};
const char *WPASS = {""};

void testPeripheral(uint8_t _oldInkplate)
{
    // Set display for test report
    display.setTextSize(3);
    display.setTextColor(BLACK);
    display.setCursor(0, 0);
    display.println("INKPLATE CHECKLIST");

    //  Power up epaper PSU
    display.einkOn();

    // Check if epaper PSU (TPS65186 EPD PMIC) is ok.
    Wire.beginTransmission(0x48);                                     // Send address 0x48 on I2C
    if (!(Wire.endTransmission() == 0) || !(display.readPowerGood())) // Check if there was an error in communication
    {
        Serial.println("- TPS Fail!");
        failHandler();
    }
    display.println("- TPS65186: OK");
    display.partialUpdate(0, 1);

    // Only one i/o expander on Inkplate 6
    int _n = 1;
    for (int i = 0; i < _n; i++)
    {
        // Check I/O expander
        display.printf("- I/O Expander %d: ", i + 1);
        display.partialUpdate(0, 1);

        // Try to communicate with I/O expander
        Wire.beginTransmission(0x20 + (i * 2)); // Send address 0x20 or 0x22
        if (Wire.endTransmission() ==
            0) // Check if there was an error in communication and print out the results on display.
        {
            display.println("OK");
            display.partialUpdate(0, 1);
        }
        else
        {
            display.println("FAIL");
            failHandler();
        }
    }

    // Check the micro SD card slot
    display.print("- microSD card slot: ");
    display.partialUpdate(0, 1);
    if (checkMicroSDCard())
    {
        display.println("OK");
        display.partialUpdate(0, 1);
    }
    else
    {
        display.println("FAIL");
        failHandler();
    }

    // Check the WiFi
    display.print("- WiFi: ");
    display.partialUpdate(0, 1);
    if (checkWiFi(WSSID, WPASS, WTIMEOUT))
    {
        display.println("OK");
        display.partialUpdate(0, 1);
    }
    else
    {
        display.println("FAIL");
        failHandler();
    }

    // First version of the Inkplate doesn't have RTC.

    // Check the RTC
    display.print("- PCF85063 RTC: ");
    if (rtcCheck())
    {
        display.println("OK");
        display.partialUpdate(0, 1);
    }
    else
    {
        display.println("FAIL");
        failHandler();
    }


    // Check I2C (easyc)
    // A slave must be connected via easyC address (0x28)
    display.print("- I2C (easyC): ");
    display.partialUpdate(0, 1);
    if (checkI2C(0x28))
    {
        display.println("OK");
        display.partialUpdate(0, 1);
    }
    else
    {
        display.println("FAIL");
        failHandler();
    }

    // Check battery
    // A battery must be connected
    display.print("- Battery and temperature: ");
    display.partialUpdate(0, 1);
    if (checkBatteryAndTemp())
    {
        display.println("OK");
        display.partialUpdate(0, 1);
    }
    else
    {
        display.println("FAIL");
        failHandler();
    }

    // This test only must be run on older Inkplates (e-radionica.com Inkplates with touchpads)
#ifdef ARDUINO_ESP32_DEV
    // Test Touchpads
    if (touchPads(TOUCHPADS_TIMEOUT))
    {
        display.println(" OK");
        display.partialUpdate(0, 1);
    }
    else
    {
        display.println(" FAIL");
        failHandler();
    }
#endif
}

double getVCOMFromSerial(double *_vcom)
{
    double vcom = 1;
    char serialBuffer[50];
    unsigned long serialTimeout;

    // Display a message on Inkplate
    display.print("\r\n- Write VCOM on UART: ");
    display.partialUpdate(0, 1);

    while (true)
    {
        Serial.println(
            "Write VCOM voltage from epaper panel.\r\nDon't forget negative (-) sign!\r\nUse dot as the decimal point. "
            "For example -1.23\nIf it's used automatic VCOM voltage detection, send \"AUTO\"");
        while (!Serial.available())
            ;

        serialTimeout = millis();
        int i = 0;
        while ((Serial.available()) && ((unsigned long)(millis() - serialTimeout) < 500))
        {
            if ((Serial.available()) && (i < 49))
            {
                serialBuffer[i++] = Serial.read();
                serialTimeout = millis();
            }
        }
        serialBuffer[i] = 0;
        if (sscanf(serialBuffer, "%lf", &vcom) == 1)
        {
            *_vcom = vcom;
            return 1;
        }
        else if (strstr(serialBuffer, "AUTO") != NULL)
        {
            *_vcom = 0;
            return 2;
        }
    }

    return 0;
}

int checkWiFi(const char *_ssid, const char *_pass, uint8_t _wifiTimeout)
{
    unsigned long _timeout = millis();

    // Try to connect to WiFi network
    WiFi.begin(WSSID, WPASS);

    // Wait until WiFi connection is established or timeout has occured.
    while ((WiFi.status() != WL_CONNECTED) && ((unsigned long)(millis() - _timeout) < (_wifiTimeout * 1000UL)))
        ;

    // Check if board is connected to WiFi
    if (WiFi.status() == WL_CONNECTED)
    {
        return 1;
    }
    else
    {
        return 0;
    }

    // Something is wrong if you got there.
    return 0;
}

int checkMicroSDCard()
{
    int sdInitOk = 0;
    sdInitOk = display.sdCardInit();

    if (sdInitOk)
    {
        File file;

        if (file.open("/testFile.txt", O_CREAT | O_RDWR))
        {
            file.print(testString);
            file.close();
        }
        else
        {
            return 0;
        }

        delay(250);

        if (file.open("/testFile.txt", O_RDWR))
        {
            char sdCardString[sdCardTestStringLength];
            file.read(sdCardString, sizeof(sdCardString));
            sdCardString[file.fileSize()] = 0;
            int stringCompare = strcmp(testString, sdCardString);
            file.remove();
            file.close();
            if (stringCompare != 0)
                return 0;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }
    return 1;
}

int checkI2C(int address)
{
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int checkBatteryAndTemp()
{
    int temperature;
    float voltage;
    int result = 1;

    temperature = display.readTemperature();
    voltage = display.readBattery();
    Serial.println(temperature);
    Serial.println(voltage);

    //ToDo check if these kinds of checks are OK?

    if(temperature < -10 || temperature > 85)
    {
        result = 0;
    }
    if(voltage <=0 || voltage > 100)
    {
        result = 0;
    }
    return result;

}

int rtcCheck()
{
    // First "ping" RTC on the I2C protocol and reset the RTC
    Wire.beginTransmission(0x51);
    int _res = Wire.endTransmission();

    // If result is from I2C is anything else than success (_res = 0), return 0 (error).
    if (_res != 0)
        return 0;

    // Reset and re-init RTC.
    display.rtcReset();

    // Set some time in epoch in RTC.
    uint32_t _epoch = 1640995200ULL;
    display.rtcSetEpoch(_epoch);

    // Wait at least one and a half second
    delay(1500);

    // Read the epoch (if everything is ok, epoch from RTC should not be the same)
    if (display.rtcGetEpoch() != _epoch)
    {
        return 1;
    }
    else
    {
        return 0;
    }

    return 0;
}

int touchPads(uint8_t _timeoutTouchpads)
{
    // This test only must be run on older Inkplates (e-radionica.com Inkplates with touchpads)
#ifdef ARDUINO_ESP32_DEV
    // Variable for storing detected touch
    int _flags = 0;

    // Show message on display
    display.print("- Touchpads [Press them!]: ");
    display.partialUpdate(0, 1);

    // Wait until all touchpads are pressed at least once in test.

    unsigned long _timeout = millis();
    while ((_flags != 0b00000111) && ((unsigned long)(millis() - _timeout) < (_timeoutTouchpads * 1000UL)))
    {
        for (int i = 0; i < 3; i++)
        {
            if (display.readTouchpad(PAD1 + i) && (!(_flags & (1 << i))))
            {
                _flags |= (1 << i);
                display.print(i + 1, DEC);
                display.partialUpdate(0, 1);
            }
        }
    }

    if ((millis() - _timeout) >= (_timeoutTouchpads * 1000UL))
    {
        return 0;
    }
    else
    {
        return 1;
    }

    return 0;
#endif
    return 1;
}

// Show a message and stop the code from executing.
void failHandler()
{
    display.print(" -> Test stopped!");
    display.partialUpdate(0, 1);

    // Inf. loop... halt the program!
    while (true)
        ;
}
