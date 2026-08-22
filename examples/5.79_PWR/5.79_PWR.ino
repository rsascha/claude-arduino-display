#include <Arduino.h>
#include "EPD.h"

// Define a black and white image array as the buffer for the e-paper display
uint8_t ImageBW[27200];

// Define the pin for the home button
#define HOME_KEY 2

// Variable to count the state of the home button
int HOME_NUM = 0;

void setup() {
  // Initialize serial communication with a baud rate of 115200
  Serial.begin(115200);

  // Set the screen power pin as an output and set it high to turn on the power
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);

  // Set the POWER LED pin as an output
  pinMode(41, OUTPUT);

  // Set the home button pin as an input
  pinMode(HOME_KEY, INPUT);
}

void loop() {
  // Flag variable to indicate if any button has been pressed
  int flag = 0;

  // Check if the home button is pressed
  if (digitalRead(HOME_KEY) == 0)
  {
    delay(100); // Debounce delay to ensure the button press is stable

    // Re-check the home button state to prevent false triggering due to bouncing
    if (digitalRead(HOME_KEY) == 1)
    {
      Serial.println("HOME_KEY"); // Print the home button press information to the serial monitor
      HOME_NUM = !HOME_NUM; // Toggle the state of the home button (on/off)

      flag = 1; // Set the flag to indicate that the display content needs to be updated
    }
  }

  // If a button has been pressed, update the display content
  if (flag == 1)
  {
    char buffer[30]; // Buffer to store the display content

    // Initialize the e-paper display
    EPD_GPIOInit();
    Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
    Paint_Clear(WHITE);

    // Initialize the e-paper display and clear the previous display content
    EPD_FastMode1Init();
    EPD_Display_Clear();
    EPD_Update();
    EPD_Clear_R26A6H();

    // Set the state of the POWER LED and display content based on the home button state
    if (HOME_NUM == 1)
    {
      digitalWrite(41, HIGH); // Turn on the POWER LED
      strcpy(buffer, "PWR:on"); // Set the display content to "PWR:on"
    } else
    {
      digitalWrite(41, LOW); // Turn off the POWER LED
      strcpy(buffer, "PWR:off"); // Set the display content to "PWR:off"
    }

    // Display the content on the e-paper
    EPD_ShowString(0, 0 + 0 * 20, buffer, 16, BLACK);

    // Refresh the e-paper display content
    EPD_Display(ImageBW);
    EPD_PartUpdate();
    EPD_DeepSleep(); // Enter deep sleep mode to save power
  }
}