#include <Arduino.h>
#include "EPD.h"  // include header files for EPD display library, providing functionality for operating electronic paper display screens

// Define an array for storing display images, with a size of 27200 bytes
uint8_t ImageBW[27200];

// Determine the status of GPIO pins and display status information on EPD
void judgement_function(int* pin) {
  char buffer[30];  // Store the string to be displayed

  EPD_GPIOInit();  // Initialize GPIO settings for EPD display screen

  // Create a new image with a white background color
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);  // Clear the canvas with a white background color

  // Create a new image again (possibly to ensure clarity) with a white background color
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);  // Clear the canvas with a white background color

  // Initialize the quick mode of EPD display
  EPD_FastMode1Init();
  EPD_Display_Clear();  // Clear EPD display content
  EPD_Update();  // Update EPD display
  EPD_Clear_R26A6H();  // Clear EPD cache

  // Traverse all GPIO pins, read the status of each pin and display it on EPD
  for (int i = 0; i < 12; i++) {
    int state = digitalRead(pin[i]);  // Read the status of the specified pin (HIGH or LOW)

    if (state == HIGH) {
      // If the pin status is HIGH, format the string and display it
      int length = sprintf(buffer, "GPIO%d : on", pin[i]);
      buffer[length] = '\0';  // Ensure that the string ends with '\ 0'
      EPD_ShowString(0, 0 + i * 20, buffer, 16, BLACK);  // Display pin status on EPD
    } else {
      // If the pin status is LOW, format the string and display it
      int length = sprintf(buffer, "GPIO%d : off", pin[i]);
      buffer[length] = '\0';  // Ensure that the string ends with '\ 0'
      EPD_ShowString(0, 0 + i * 20, buffer, 16, BLACK);  // Display pin status on EPD
    }
  }
  EPD_Display(ImageBW);  // Display the image to EPD
  EPD_PartUpdate();  // Perform partial update (refresh display)
  EPD_DeepSleep();  // Set EPD to deep sleep mode to save power
}

// Define GPIO pin array
int pin_Num[12] = {8, 3, 14, 9, 16, 15, 18, 17, 20, 19, 38, 21};

void setup() {
  Serial.begin(115200);  // Initialize serial communication, baud rate 115200

  // Configure pin 7 to output mode and set it to HIGH (possibly for power control)
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);

  // Configure GPIO pins to output mode
  pinMode(8, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(16, OUTPUT);
  pinMode(15, OUTPUT);
  pinMode(18, OUTPUT);
  pinMode(17, OUTPUT);
  pinMode(20, OUTPUT);
  pinMode(19, OUTPUT);
  pinMode(38, OUTPUT);
  pinMode(21, OUTPUT);

  // Set all GPIO pins to HIGH
  digitalWrite(8, HIGH);
  digitalWrite(3, HIGH);
  digitalWrite(14, HIGH);
  digitalWrite(9, HIGH);
  digitalWrite(16, HIGH);
  digitalWrite(15, HIGH);
  digitalWrite(18, HIGH);
  digitalWrite(17, HIGH);
  digitalWrite(20, HIGH);
  digitalWrite(19, HIGH);
  digitalWrite(38, HIGH);
  digitalWrite(21, HIGH);

  // Call a function to determine the status of GPIO pins and display the result
  judgement_function(pin_Num);
}

void loop() {
  // Main loop, currently not performing any operations, only delaying by 1 second
  delay(1000);  // Delay of 1000 milliseconds (1 second)
}
