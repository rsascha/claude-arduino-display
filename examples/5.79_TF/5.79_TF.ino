#include <Arduino.h>  // Include the Arduino library
#include "EPD.h"      // Include the e-paper display library
#include "SD.h"       // Include the SD card library

// Define the SPI pins for the SD card
#define SD_MOSI 40    // SD card MOSI pin
#define SD_MISO 13    // SD card MISO pin
#define SD_SCK 39     // SD card SCK pin
#define SD_CS 10      // SD card CS pin

// Create an instance of SPIClass for SD card SPI communication
SPIClass SD_SPI = SPIClass(HSPI);

// Define a black and white image array as the buffer for the e-paper display
uint8_t ImageBW[27200];

void setup() {
  // Initialize serial communication with a baud rate of 115200
  Serial.begin(115200);

  // Set the screen power pin as an output and set it high to turn on the power
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);

  // Set another power pin as an output and set it high to turn on the power
  pinMode(42, OUTPUT);
  digitalWrite(42, HIGH);

  // Brief delay to ensure power stability
  delay(10);

  // Initialize the SD card
  SD_SPI.begin(SD_SCK, SD_MISO, SD_MOSI); // Initialize SPI with specified pins

  // Try to mount the SD card file system and set the SPI clock speed to 80 MHz
  if (!SD.begin(SD_CS, SD_SPI, 80000000)) {
    // If SD card initialization fails, print error message to the serial monitor
    Serial.println(F("ERROR: File system mount failed!"));
  } else {
    // If SD card initialization succeeds, print SD card size information to the serial monitor
    Serial.printf("SD Size: %lluMB \n", SD.cardSize() / (1024 * 1024));

    // Create a buffer to store the display content
    char buffer[30]; // Assume the display content will not exceed 30 characters
    // Use sprintf to format the string and store the SD card size information in buffer
    int length = sprintf(buffer, "SD Size:%lluMB", SD.cardSize() / (1024 * 1024));
    buffer[length] = '\0'; // Ensure the string ends with a null character

    // Initialize the e-paper display
    EPD_GPIOInit(); // Initialize the GPIO pins for the e-paper
    Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE); // Create a new image
    Paint_Clear(WHITE); // Clear the image, fill with white

    // Recreate and clear the image
    Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
    Paint_Clear(WHITE);

    // Initialize the e-paper display mode
    EPD_FastMode1Init();
    EPD_Display_Clear(); // Clear the e-paper display
    EPD_Update();        // Update the e-paper display
    EPD_Clear_R26A6H(); // Further clear the display

    // Print the display content to the serial monitor
    Serial.println(buffer);

    // Display the content on the e-paper
    EPD_ShowString(0, 0, buffer, 16, BLACK); // Draw the string on the e-paper

    // Refresh the e-paper display content
    EPD_Display(ImageBW);
    EPD_PartUpdate();
    EPD_DeepSleep(); // Enter deep sleep mode to save power
  }
}

void loop() {
  // Main loop code
  delay(10); // Wait for 10 milliseconds
}