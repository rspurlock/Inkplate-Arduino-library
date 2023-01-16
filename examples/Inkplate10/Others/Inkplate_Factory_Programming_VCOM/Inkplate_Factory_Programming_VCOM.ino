// Next 3 lines are a precaution, you can ignore those, and the example would also work without them
#if !defined(ARDUINO_INKPLATE10) && !defined(ARDUINO_INKPLATE10V2)
#error "Wrong board selection for this example, please select Inkplate 10 or Inkplate 10 V2 in the boards menu."
#endif

// Do not forget to add WiFi SSID and WiFi Password in test.cpp!

#include "EEPROM.h"
#include "Inkplate.h"
#include "image.h"
#include <Wire.h>
#include "test.h"

Inkplate display(INKPLATE_1BIT);

// Change the VCOM voltage here. There should be a sticker on epaper flex cable with this voltage.
// Usually it's somewhere between -1V and -3V.
double vcomVoltage = -1.30;

// EEPROMOffset must be between 0 and 64
const int EEPROMOffset = 46;
int EEPROMaddress = sizeof(waveformData) + EEPROMOffset;

// Peripheral mode variables and arrays
#define BUFFER_SIZE 1000
char commandBuffer[BUFFER_SIZE + 1];
char strTemp[2001];

// All waveforms for Inkplate 10 boards
uint8_t waveform1[8][9] = {{0, 0, 0, 0, 0, 0, 0, 1, 0}, {0, 0, 0, 2, 2, 2, 1, 1, 0}, {0, 0, 2, 1, 1, 2, 2, 1, 0},
    {0, 1, 2, 2, 1, 2, 2, 1, 0}, {0, 0, 2, 1, 2, 2, 2, 1, 0}, {0, 2, 2, 2, 2, 2, 2, 1, 0},
    {0, 0, 0, 0, 0, 2, 1, 2, 0}, {0, 0, 0, 2, 2, 2, 2, 2, 0}
};
uint8_t waveform2[8][9] = {{0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 2, 1, 2, 1, 1, 0}, {0, 0, 0, 2, 2, 1, 2, 1, 0},
    {0, 0, 2, 2, 1, 2, 2, 1, 0}, {0, 0, 0, 2, 1, 1, 1, 2, 0}, {0, 0, 2, 2, 2, 1, 1, 2, 0},
    {0, 0, 0, 0, 0, 1, 2, 2, 0}, {0, 0, 0, 0, 2, 2, 2, 2, 0}
};
uint8_t waveform3[8][9] = {{0, 3, 3, 3, 3, 3, 3, 3, 0}, {0, 1, 2, 1, 1, 2, 2, 1, 0}, {0, 2, 2, 2, 1, 2, 2, 1, 0},
    {0, 0, 2, 2, 2, 2, 2, 1, 0}, {0, 3, 3, 2, 1, 1, 1, 2, 0}, {0, 3, 3, 2, 2, 1, 1, 2, 0},
    {0, 2, 1, 2, 1, 2, 1, 2, 0}, {0, 3, 3, 3, 2, 2, 2, 2, 0}
};
uint8_t *waveformList[] = {&waveform1[0][0], &waveform2[0][0], &waveform3[0][0]};

// Calculate number of possible waveforms
uint8_t waveformListSize = (sizeof(waveformList) / sizeof(uint8_t *));

// Struct for reading waveform from EEPROM memory of ESP32
struct waveformData waveformEEPROM;

int currentWaveform = 0;

void setup()
{
    Serial.begin(115200);
    display.begin();
    EEPROM.begin(512);

    // Check if VCOM and waveform programming is not already done
    if (EEPROM.read(EEPROMaddress) != 170)
    {
        // Test all peripherals of the Inkplate (I/O expander, RTC, uSD card hodler, etc).
        // If for some reason second I/O expander needs to be skipped, use testPeripheral(1);
        testPeripheral();

        // Wait until valid VCOM has been recieved
        uint8_t flag = getVCOMFromSerial(&vcomVoltage);

        // If the flag is 1, use manual inserted VCOM voltage from UART
        if (flag == 1)
        {
            display.printf("MANUAL VCOM: %.2lf ", vcomVoltage);
            display.partialUpdate();
            if (!writeVCOMToEEPROM(vcomVoltage))
            {
                display.println("VCOM PROG. FAIL");
                failHandler();
            }
        }
        else
        {
            display.println("VCOM ERROR");
            failHandler();
        }
        
        display.selectDisplayMode(INKPLATE_3BIT);

        // Display all shades of gray on epaper with first waveform
        showGradient(currentWaveform);

        // Until "Load" key is not pressed, user can select one of the waveforms
        while (!display.readTouchpad(PAD2))
        {
            // Select and show next waveform
            if (display.readTouchpad(PAD3))
            {
                currentWaveform++;
                if (currentWaveform > waveformListSize - 1)
                    currentWaveform = 0;
                showGradient(currentWaveform);
            }

            // Select and show prev. waveform
            if (display.readTouchpad(PAD1))
            {
                currentWaveform--;
                if (currentWaveform < 0)
                    currentWaveform = waveformListSize - 1;
                showGradient(currentWaveform);
            }
        }

        // Load waveform in EEPROM memory of ESP32
        waveformEEPROM.waveformId = INKPLATE10_WAVEFORM1 + currentWaveform;
        memcpy(&waveformEEPROM.waveform, waveformList[currentWaveform], sizeof(waveformEEPROM.waveform));
        waveformEEPROM.checksum = display.calculateChecksum(waveformEEPROM);
        display.burnWaveformToEEPROM(waveformEEPROM);

        EEPROM.write(EEPROMaddress, 170);
        EEPROM.commit();
    }
    else
    {
        Serial.println("Vcom and waveform already set!");
        display.einkOn();
        vcomVoltage = (double)(readReg(0x03) | ((uint16_t)((readReg(0x04) & 1) << 8))) / (-100);
        display.getWaveformFromEEPROM(&waveformEEPROM) ? waveformEEPROM.waveformId : -1;
    }
    memset(commandBuffer, 0, BUFFER_SIZE);

    showSplashScreen(waveformEEPROM);
}

void loop()
{
    if (Serial.available())
    {
        while (Serial.available())
        {
            for (int i = 0; i < (BUFFER_SIZE - 1); i++)
            {
                commandBuffer[i] = commandBuffer[i + 1];
            }
            commandBuffer[BUFFER_SIZE - 1] = Serial.read();
        }
    }
    char *s = NULL;
    char *e = NULL;
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        if (commandBuffer[i] == '#' && s == NULL)
            s = &commandBuffer[i];
        if (commandBuffer[i] == '*' && e == NULL)
            e = &commandBuffer[i];
    }
    if (s != NULL && e != NULL)
    {
        if ((e - s) > 0)
        {
            int x, x1, x2, y, y1, y2, x3, y3, l, c, w, h, r, n;
            char b;
            switch (*(s + 1))
            {
                case '?':
                    Serial.print("OK");
                    break;

                case '0':
                    sscanf(s + 3, "%d,%d,%d", &x, &y, &c);
                    display.drawPixel(x, y, c);
                    break;

                case '1':
                    sscanf(s + 3, "%d,%d,%d,%d,%d", &x1, &y1, &x2, &y2, &c);
                    display.drawLine(x1, y1, x2, y2, c);
                    break;

                case '2':
                    sscanf(s + 3, "%d,%d,%d,%d", &x, &y, &l, &c);
                    display.drawFastVLine(x, y, l, c);
                    break;

                case '3':
                    sscanf(s + 3, "%d,%d,%d,%d", &x, &y, &l, &c);
                    display.drawFastHLine(x, y, l, c);
                    break;

                case '4':
                    sscanf(s + 3, "%d,%d,%d,%d,%d", &x, &y, &w, &h, &c);
                    display.drawRect(x, y, w, h, c);
                    break;

                case '5':
                    sscanf(s + 3, "%d,%d,%d,%d", &x, &y, &r, &c);
                    display.drawCircle(x, y, r, c);
                    break;

                case '6':
                    sscanf(s + 3, "%d,%d,%d,%d,%d,%d,%d", &x1, &y1, &x2, &y2, &x3, &y3, &c);
                    display.drawTriangle(x1, y1, x2, y2, x3, y3, c);
                    break;

                case '7':
                    sscanf(s + 3, "%d,%d,%d,%d,%d,%d", &x, &y, &w, &h, &r, &c);
                    display.drawRoundRect(x, y, w, h, r, c);
                    break;

                case '8':
                    sscanf(s + 3, "%d,%d,%d,%d,%d", &x, &y, &w, &h, &c);
                    display.fillRect(x, y, w, h, c);
                    break;

                case '9':
                    sscanf(s + 3, "%d,%d,%d,%d", &x, &y, &r, &c);
                    display.fillCircle(x, y, r, c);
                    break;

                case 'A':
                    sscanf(s + 3, "%d,%d,%d,%d,%d,%d,%d", &x1, &y1, &x2, &y2, &x3, &y3, &c);
                    display.fillTriangle(x1, y1, x2, y2, x3, y3, c);
                    break;

                case 'B':
                    sscanf(s + 3, "%d,%d,%d,%d,%d,%d", &x, &y, &w, &h, &r, &c);
                    display.fillRoundRect(x, y, w, h, r, c);
                    break;

                case 'C':
                    sscanf(s + 3, "\"%2000[^\"]\"", strTemp);
                    n = strlen(strTemp);
                    for (int i = 0; i < n; i++)
                    {
                        strTemp[i] = toupper(strTemp[i]);
                    }
                    for (int i = 0; i < n; i += 2)
                    {
                        strTemp[i / 2] = (hexToChar(strTemp[i]) << 4) | (hexToChar(strTemp[i + 1]) & 0x0F);
                    }
                    strTemp[n / 2] = 0;
                    display.print(strTemp);
                    break;

                case 'D':
                    sscanf(s + 3, "%d", &c);
                    display.setTextSize(c);
                    break;

                case 'E':
                    sscanf(s + 3, "%d,%d", &x, &y);
                    display.setCursor(x, y);
                    break;

                case 'F':
                    sscanf(s + 3, "%c", &b);
                    if (b == 'T')
                        display.setTextWrap(true);
                    if (b == 'F')
                        display.setTextWrap(false);
                    break;

                case 'G':
                    sscanf(s + 3, "%d", &c);
                    c &= 3;
                    display.setRotation(c);
                    break;

                case 'H':
                    sscanf(s + 3, "%d,%d,\"%149[^\"]\"", &x, &y, strTemp);
                    n = strlen(strTemp);
                    for (int i = 0; i < n; i++)
                    {
                        strTemp[i] = toupper(strTemp[i]);
                    }
                    for (int i = 0; i < n; i += 2)
                    {
                        strTemp[i / 2] = (hexToChar(strTemp[i]) << 4) | (hexToChar(strTemp[i + 1]) & 0x0F);
                    }
                    strTemp[n / 2] = 0;
                    r = display.sdCardInit();
                    if (r)
                    {
                        r = display.drawBitmapFromSd(strTemp, x, y);
                        Serial.print("#H(");
                        Serial.print(r, DEC);
                        Serial.println(")*");
                        Serial.flush();
                    }
                    else
                    {
                        Serial.println("#H(-1)*");
                        Serial.flush();
                    }
                    break;

                case 'I':
                    sscanf(s + 3, "%d", &c);
                    if (c == 1)
                        display.selectDisplayMode(INKPLATE_1BIT);
                    if (c == 3)
                        display.selectDisplayMode(INKPLATE_3BIT);
                    break;

                case 'J':
                    sscanf(s + 3, "%c", &b);
                    if (b == '?')
                    {
                        if (display.getDisplayMode() == INKPLATE_1BIT)
                        {
                            Serial.println("#J(0)*");
                            Serial.flush();
                        }
                        if (display.getDisplayMode() == INKPLATE_3BIT)
                        {
                            Serial.println("#J(1)*");
                            Serial.flush();
                        }
                    }
                    break;

                case 'K':
                    sscanf(s + 3, "%c", &b);
                    if (b == '1')
                    {
                        display.clearDisplay();
                    }
                    break;

                case 'L':
                    sscanf(s + 3, "%c", &b);
                    if (b == '1')
                    {
                        display.display();
                    }
                    break;

                case 'M':
                    sscanf(s + 3, "%d,%d,%d", &y1, &x2, &y2);
                    display.partialUpdate();
                    break;

                case 'N':
                    sscanf(s + 3, "%c", &b);
                    if (b == '?')
                    {
                        Serial.print("#N(");
                        Serial.print(display.readTemperature(), DEC);
                        Serial.println(")*");
                        Serial.flush();
                    }
                    break;

                case 'O':
                    sscanf(s + 3, "%d", &c);
                    if (c >= 0 && c <= 2)
                    {
                        Serial.print("#O(");
                        Serial.print(display.readTouchpad(c), DEC);
                        Serial.println(")*");
                        Serial.flush();
                    }
                    break;

                case 'P':
                    sscanf(s + 3, "%c", &b);
                    if (b == '?')
                    {
                        Serial.print("#P(");
                        Serial.print(display.readBattery(), 2);
                        Serial.println(")*");
                        Serial.flush();
                    }
                    break;

                case 'Q':
                    sscanf(s + 3, "%d", &c);
                    c &= 1;
                    if (c == 0)
                        display.einkOff();
                    if (c == 1)
                        display.einkOn();
                    break;

                case 'R':
                    sscanf(s + 3, "%c", &b);
                    if (b == '?')
                    {
                        Serial.print("#R(");
                        Serial.print(display.getPanelState(), DEC);
                        Serial.println(")*");
                        Serial.flush();
                    }
                    break;
            }
            *s = 0;
            *e = 0;
        }
    }
}

int hexToChar(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

void writeReg(uint8_t _reg, uint8_t _data)
{
    Wire.beginTransmission(0x48);
    Wire.write(_reg);
    Wire.write(_data);
    Wire.endTransmission();
}

uint8_t readReg(uint8_t _reg)
{
    Wire.beginTransmission(0x48);
    Wire.write(_reg);
    Wire.endTransmission(false);
    Wire.requestFrom(0x48, 1);
    return Wire.read();
}

void showSplashScreen(struct waveformData _w)
{
    display.clearDisplay();
    display.display();
    display.selectDisplayMode(INKPLATE_3BIT);
    display.drawBitmap3Bit(0, 0, demo_image, demo_image_w, demo_image_h);
    display.setTextColor(0, 7);
    display.setTextSize(1);
    display.setCursor(10, 10);
    display.print(vcomVoltage, 2);
    display.print("V");
    display.setCursor(10, 20);
    display.print("Waveform");
    display.print(_w.waveformId - 20 + 1, DEC);
    display.display();
}

// Need to be changed to inkplate 10 Null waveform
void writeToScreen()
{
    display.clean(1, 8);
    display.clean(0, 2);
    display.clean(2, 10);
}

// Do not use until until null waveform is set correctly
double readVCOM()
{
    double vcomVolts;
    writeReg(0x01, B00111111); // enable all rails
    writeReg(0x04, (readReg(0x04) | B00100000));
    writeToScreen();
    writeReg(0x04, (readReg(0x04) | B10000000));
    delay(10);
    while (display.digitalReadMCP(6))
    {
        delay(1);
    };
    readReg(0x07);
    uint16_t vcom = ((readReg(0x04) & 1) << 8) | readReg(0x03);
    vcomVolts = vcom * 10 / 1000.0;
    display.einkOff();
    return -vcomVolts;
}

uint8_t writeVCOMToEEPROM(double v)
{
    int vcom = int(abs(v) * 100);
    int vcomH = (vcom >> 8) & 1;
    int vcomL = vcom & 0xFF;
    
    // Set MCP23017 pin where TPS65186 INT pin is connectet to input pull up
    display.pinModeInternal(MCP23017_INT_ADDR, display.mcpRegsInt, 6, INPUT_PULLUP);

    // First power up TPS65186 so we can communicate with it
    display.einkOn();

    // Wait a little bit
    delay(250);
    
    // Send to TPS65186 first 8 bits of VCOM
    writeReg(0x03, vcomL);

    // Send new value of register to TPS
    writeReg(0x04, vcomH);
    delay(1);

    // Program VCOM value to EEPROM
    writeReg(0x04, vcomH | (1 << 6));

    // Wait until EEPROM has been programmed
    delay(100);
    do
    {
        delay(1);
    } while (display.digitalReadInternal(MCP23017_INT_ADDR, display.mcpRegsInt, 6));

    // Clear Interrupt flag by reading INT1 register
    readReg(0x07);

    // Now, power off whole TPS
    display.einkOff();

    // Wait a little bit...
    delay(1000);

    // Power up TPS again
    display.einkOn();

    delay(10);

    // Read VCOM valuse from registers
    vcomL = readReg(0x03);
    vcomH = readReg(0x04);
    Serial.print("Vcom: ");
    Serial.println(vcom);
    Serial.print("Vcom register: ");
    Serial.println(vcomL | (vcomH << 8));

    // Trun off the TPS65186 and wait a little bit
    display.einkOff();
    delay(100);

    if (vcom != (vcomL | (vcomH << 8)))
    {
        Serial.println("\nVCOM EEPROM PROGRAMMING FAILED!\n");
        return 0;
    }
    else
    {
        Serial.println("\nVCOM EEPROM PROGRAMMING OK\n");
        return 1;
    }
    return 0;
}

void showGradient(int _selected)
{
    int w = display.width() / 8;
    int h = display.height() - 100;

    display.changeWaveform(waveformList[currentWaveform]);

    display.fillRect(0, 725, 1200, 100, 7);

    display.setTextSize(4);
    display.setTextColor(0);
    display.setCursor(420, 743);
    display.print("Waveform select");
    display.setCursor(432, 792);
    display.print("Prev Load Next");

    display.setCursor(800, 743);
    display.print("1 2 3");
    display.drawRect((_selected * 6 * 4 * 2) + 800 - 3, 740, (6 * 4) + 2, (8 * 4) + 2, 0);

    for (int i = 0; i < 8; i++)
    {
        display.fillRect(i * w, 0, w, h, i);
    }
    display.display();
}
