#include <Arduino.h>
#include "EPD.h"

// Pre allocated black and white image array
uint8_t ImageBW[27200];

// key definition
#define HOME_KEY 2      // Connect the home button to digital pin 2
#define EXIT_KEY 1      // Exit key connected to digital pin 1
#define PRV_KEY 6       // Previous page key connected to digital pin 6
#define NEXT_KEY 4      // Next page key connected to digital pin 4
#define OK_KEY 5        // Confirm key connected to digital pin 5

// Initialize each key counter
int HOME_NUM = 0;
int EXIT_NUM = 0;
int PRV_NUM = 0;
int NEXT_NUM = 0;
int OK_NUM = 0;

// Storage key count array
int NUM_btn[5] = {0};

// Key counting and display function
void count_btn(int NUM[5])
{
  char buffer[30];  // Buffer for storing strings

  // EPD (Electronic Paper Display) initialization
  EPD_GPIOInit();
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);

  // Clear the display screen and initialize it
  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_Update();
  EPD_Clear_R26A6H();

  // Display the count of each button
  int length = sprintf(buffer, "HOME_KEY_NUM:%d", NUM[0]);
  buffer[length] = '\0';
  EPD_ShowString(0, 0 + 0 * 20, buffer, 16, BLACK);
  
  length = sprintf(buffer, "EXIT_KEY_NUM:%d", NUM[1]);
  buffer[length] = '\0';
  EPD_ShowString(0, 0 + 1 * 20, buffer, 16, BLACK);

  length = sprintf(buffer, "PRV_KEY_NUM:%d", NUM[2]);
  buffer[length] = '\0';
  EPD_ShowString(0, 0 + 2 * 20, buffer, 16, BLACK);

  length = sprintf(buffer, "NEXT__NUM:%d", NUM[3]);
  buffer[length] = '\0';
  EPD_ShowString(0, 0 + 3 * 20, buffer, 16, BLACK);

  length = sprintf(buffer, "OK_NUM:%d", NUM[4]);
  buffer[length] = '\0';
  EPD_ShowString(0, 0 + 4 * 20, buffer, 16, BLACK);

  // Update display content and enter deep sleep mode
  EPD_Display(ImageBW);
  EPD_PartUpdate();
  EPD_DeepSleep();
}


void setup() {
  Serial.begin(115200);  // Initialize serial communication, baud rate 115200
  
  // Set button pins
  pinMode(7, OUTPUT);    // Set pin 7 as output for power control
  digitalWrite(7, HIGH); // Set the power to high level and turn on the screen power
  
  pinMode(HOME_KEY, INPUT);  // Set the home button pin as input
  pinMode(EXIT_KEY, INPUT);  // 
  pinMode(PRV_KEY, INPUT);   // 
  pinMode(NEXT_KEY, INPUT);  // 
  pinMode(OK_KEY, INPUT);    // 
}

void loop() {
      int flag = 0; // Mark whether any buttons have been pressed
  
  // Check if the home button is pressed
  if (digitalRead(HOME_KEY) == 0)
  {
    delay(100); // Anti shake delay
    if (digitalRead(HOME_KEY) == 1)
    {
      Serial.println("HOME_KEY"); 
      HOME_NUM++; // Increase homepage key count
      flag = 1; // set mark
    }
  }
  // Check if the exit button has been pressed
  else if (digitalRead(EXIT_KEY) == 0)
  {
    delay(100); // delay
    if (digitalRead(EXIT_KEY) == 1)
    {
      Serial.println("EXIT_KEY"); 
      EXIT_NUM++; // Increase exit key count
      flag = 1; 
    }
  }
  
  else if (digitalRead(PRV_KEY) == 0)
  {
    delay(100); 
    if (digitalRead(PRV_KEY) == 1)
    {
      Serial.println("PRV_KEY"); 
      PRV_NUM++; 
      flag = 1; 
    }
  }
  
  else if (digitalRead(NEXT_KEY) == 0)
  {
    delay(100); 
    if (digitalRead(NEXT_KEY) == 1)
    {
      Serial.println("NEXT_KEY"); 
      NEXT_NUM++; 
      flag = 1; 
    }
  }
  
  else if (digitalRead(OK_KEY) == 0)
  {
    delay(100); 
    if (digitalRead(OK_KEY) == 1)
    {
      Serial.println("OK_KEY"); 
      OK_NUM++; 
      flag = 1; 
    }
  }Reset Tag
  
  // If a button is pressed, update the display content
  if (flag == 1)
  {
    NUM_btn[0] = HOME_NUM;
    NUM_btn[1] = EXIT_NUM;
    NUM_btn[2] = PRV_NUM;
    NUM_btn[3] = NEXT_NUM;
    NUM_btn[4] = OK_NUM;

    count_btn(NUM_btn); // Call function to update display
    flag = 0; // Reset flag
  }
}
