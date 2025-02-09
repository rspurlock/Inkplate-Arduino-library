/*
   Inkplate5_Mandelbrot_Set example for Soldered Inkplate 5
   For this example you will need only a USB-C cable and Inkplate 5.
   Select "Soldered Inkplate5" from Tools -> Board menu.
   Don't have "Soldered Inkplate5" option? Follow our tutorial and add it:
   https://soldered.com/learn/add-inkplate-6-board-definition-to-arduino-ide/

   This example renders the mandelbrot set to coordiantes to Inkplate.
   Due to the nature of Mandelbrot set, it is quite slow on low powered MCUs, so please be patient :)

   Want to learn more about Inkplate? Visit www.inkplate.io
   Looking to get support? Write on our forums: https://forum.soldered.com/
   28 March 2023 by Soldered
*/

// Next 3 lines are a precaution, you can ignore those, and the example would also work without them
#ifndef ARDUINO_INKPLATE5
#error "Wrong board selection for this example, please select Soldered Inkplate5 in the boards menu."
#endif

// Include Inkplate library to the sketch
#include "Inkplate.h"

#define MAXITERATIONS 150

// Create object on Inkplate library and set library to work in Black and White mode (1-bit)
Inkplate display(INKPLATE_1BIT);

// Takes a long time to render, cca 3 minutes

// Explore different positions to draw
// Some interesting ones can be found here http://www.cuug.ab.ca/dewara/mandelbrot/Mandelbrowser.html
double xFrom = -0.7423, xTo = -0.8463;
double yFrom = 0.1092, yTo = 0.2102;

void setup()
{
    // Init serial communication to see rendering progress
    Serial.begin(115200);

    // Init library (you should call this function ONLY ONCE)
    display.begin();

    // Clear the display and draw the set
    display.clearDisplay();
    for (int j = 0; j < E_INK_HEIGHT; ++j)
    {
        for (int i = 0; i < E_INK_WIDTH; ++i)
        {
            display.drawPixel(i, j, colorAt(xFrom + (double)i * (xTo - xFrom) / 960.0, yFrom + (double)j * (yTo - yFrom) / 540.0));
            
            // For whole set:
            // display.drawPixel(i, j, colorAt(-2.0 + (3.0 * (double)i / 960.0), -1.0 + 2.0 * (double)j / 540.0));
        }

        // Print rendering progress on the Serial Monitor
        Serial.println(j);
    }

    // When it gets to 539, it's over. Print a message and display the set
    Serial.println("That's it!");
    display.display();
}

void loop()
{
    // Empty...
}

struct complex
{
    double re;
    double im;
};

void addComplex(struct complex *z, struct complex *c)
{
    z->re += c->re;
    z->im += c->im;
}

void squareComplex(struct complex *z)
{
    double re = z->re;
    double im = z->im;
    z->re = re * re - im * im;
    z->im = 2 * re * im;
}

double modulusComplexSqr(struct complex *z)
{
    return z->re * z->re + z->im * z->im;
}

uint8_t colorAt(double x, double y)
{
    struct complex z = {0.0, 0.0};
    struct complex c = {x, y};

    int i;
    for (i = 0; i < MAXITERATIONS && modulusComplexSqr(&z) <= 4.0; ++i)
    {
        squareComplex(&z);
        addComplex(&z, &c);
    }
    return i == MAXITERATIONS;
}
