#include <Arduino.h>         // Include the core Arduino library to provide basic Arduino functionality
#include "EPD.h"             // Include the EPD library for controlling the electronic ink screen (E-Paper Display)
#include "pic_home.h"        // Include the header file containing image data

uint8_t ImageBW[27200];      // Declare an array of 27200 bytes to store black and white image data

void setup() {
  // Initialization settings, executed once when the program starts
  pinMode(7, OUTPUT);        // Set pin 7 to output mode
  digitalWrite(7, HIGH);     // Set pin 7 to high level to activate the screen power

  EPD_GPIOInit();            // Initialize the GPIO pin configuration for the EPD electronic ink screen
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE); // Create a new image buffer with dimensions EPD_W x EPD_H and a white background
  Paint_Clear(WHITE);        // Clear the image buffer and fill it with white

  /************************ Fast refresh screen operation in partial refresh mode ************************/
  EPD_FastMode1Init();       // Initialize the EPD screen's fast mode 1
  EPD_Display_Clear();       // Clear the screen content
  EPD_Update();              // Update the screen display

  EPD_GPIOInit();            // Reinitialize the GPIO pin configuration for the EPD electronic ink screen
  EPD_FastMode1Init();       // Reinitialize the EPD screen's fast mode 1
  EPD_ShowPicture(0, 0, 792, 272, gImage_home, WHITE); // Display the image gImage_home starting at coordinates (0, 0), width 792, height 272, with a white background
  EPD_Display(ImageBW);      // Display the image stored in the ImageBW array
  //  EPD_WhiteScreen_ALL_Fast(gImage_boot_setup); // Commented-out code: Display the boot setup image using fast mode
  //  EPD_PartUpdate();       // Commented-out code: Update part of the screen
  EPD_FastUpdate();          // Perform a fast update to refresh the screen
  EPD_DeepSleep();           // Set the screen to deep sleep mode to save power
  delay(5000);               // Wait for 5000 milliseconds (5 seconds)

  clear_all();               // Call the clear_all function to clear the screen content
}

void loop() {
  // Main loop function, currently does not perform any actions
  // Code that needs to be executed repeatedly can be added here
}

void clear_all() {
  // Function to clear the screen content
  EPD_FastMode1Init();       // Initialize the EPD screen's fast mode 1
  EPD_Display_Clear();       // Clear the screen content
  EPD_Update();              // Update the screen display
}