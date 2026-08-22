#include <Arduino.h>  // Include Arduino library to provide basic functions.
#include "EPD.h"      // Include electronic paper display library for controlling e-paper displays.
#include <WiFi.h>     // Include WiFi library for WiFi connections.

// Define SSID and password for the WiFi network.
String ssid = "yanfa_software";
String password = "yanfa-123456";

// Define a black and white image array as a buffer for e-paper display.
uint8_t ImageBW[27200];

void setup() {
  // Initialize serial communication with a baud rate of 115200.
  Serial.begin(115200);

  // Set the screen power pin as output mode and set it to high to turn on the power.
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);

  // Start connecting to WiFi.
  WiFi.begin(ssid, password);

  // Wait until the WiFi connection is established.
  while (WiFi.status()!= WL_CONNECTED) {
    delay(500); // Check the connection status every 500 milliseconds.
    Serial.print("."); // Output dots in the serial monitor to show the connection progress.
  }

  // Output information after the WiFi connection is successful.
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP()); // Output the IP address of the device.

  // Create a character array for displaying information.
  char buffer[40];

  // Initialize the e-paper display.
  EPD_GPIOInit(); // Initialize the GPIO pins of the e-paper.
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE); // Create a new image.
  Paint_Clear(WHITE); // Clear the image by filling it with white.

  // Recreate and clear the image.
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);

  // Initialize the e-paper display mode.
  EPD_FastMode1Init();
  EPD_Display_Clear(); // Clear the e-paper display.
  EPD_Update();        // Update the e-paper display.
  EPD_Clear_R26A6H(); // Further clear the display.

  // Set the text content to be displayed.
  strcpy(buffer, "WiFi connected"); // Copy "WiFi connected" to buffer.
  EPD_ShowString(0, 0 + 0 * 20, buffer, 16, BLACK); // Display the text on the e-paper at position (0, 0) with font size 16.

  // Set and display the IP address.
  strcpy(buffer, "IP address: "); // Copy "IP address: " to buffer.
  strcat(buffer, WiFi.localIP().toString().c_str()); // Append the IP address to buffer.
  EPD_ShowString(0, 0 + 1 * 20, buffer, 16, BLACK); // Display the IP address on the e-paper at position (0, 20) with font size 16.

  // Refresh the e-paper display content.
  EPD_Display(ImageBW); // Display the buffer content on the e-paper.
  EPD_PartUpdate();    // Perform partial update of the e-paper display.
  EPD_DeepSleep();    // Enter deep sleep mode to save power consumption.
}

void loop() {
  // No functionality implemented in the main loop.
  delay(10); // Wait for 10 milliseconds.
}