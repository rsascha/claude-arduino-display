#include <Arduino.h>         // Include the core Arduino library to provide basic Arduino functionality
#include "EPD.h"             // Include the EPD library for controlling the electronic ink screen (E-Paper Display)
#include "pic_scenario.h"    // Include the header file containing image data

uint8_t ImageBW[27200];      // Declare an array of 27200 bytes to store black and white image data

void setup() {
  // Initialization settings, executed once when the program starts

  // Configure the screen power pin
  pinMode(7, OUTPUT);        // Set pin 7 to output mode
  digitalWrite(7, HIGH);     // Set pin 7 to high level to activate the screen power

  EPD_GPIOInit();            // Initialize the GPIO pin configuration for the EPD electronic ink screen
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE); // Create a new image buffer with dimensions EPD_W x EPD_H and a white background
  Paint_Clear(WHITE);        // Clear the image buffer and fill it with white

  /************************ Fast refresh screen operation in partial refresh mode ************************/
  EPD_FastMode1Init();       // Initialize the EPD screen's fast mode 1
  EPD_Display_Clear();       // Clear the screen content
  EPD_Update();              // Update the screen display

  EPD_HW_RESET();            // Perform a hardware reset operation to ensure the screen starts up properly
  EPD_ShowPicture(0, 0, 312, 152, gImage_1, WHITE); // Display the image gImage_1 starting at coordinates (0, 0), width 312, height 152, with a white background
  EPD_Display(ImageBW);      // Display the image stored in the ImageBW array
  EPD_PartUpdate();          // Update part of the screen to show the new content
  EPD_DeepSleep();           // Set the screen to deep sleep mode to save power

  delay(5000);               // Wait for 5000 milliseconds (5 seconds)

  clear_all();               // Call the clear_all function to clear the screen content
}

void loop() {
  // Main loop function, currently does not perform any actions
  // Code that needs to be executed repeatedly can be added here
}

void clear_all()
{
  // Function to clear the screen content
  EPD_FastMode1Init();       // Initialize the EPD screen's fast mode 1
  EPD_Display_Clear();       // Clear the screen content
  EPD_Update();              // Update the screen display
}