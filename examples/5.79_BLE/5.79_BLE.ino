#include <Arduino.h>                 // Include the Arduino core library, providing basic Arduino functionality
#include "EPD.h"                     // Include the EPD (Electronic Paper Display) library header file

#include "BLEDevice.h"               // Include the library for BLE device-related functions
#include "BLEServer.h"               // Include the library for BLE server-related functions
#include "BLEUtils.h"                // Include the library for BLE utility functions
#include "BLE2902.h"                 // Include the library for the BLE2902 descriptor, used for characteristic descriptors

BLECharacteristic *pCharacteristic;  // Pointer to the BLE characteristic, used for managing and operating BLE characteristics
BLEServer *pServer;                  // Pointer to the BLE server, used for creating and managing the BLE server
BLEService *pService;                // Pointer to the BLE service, used for creating and managing the BLE service
bool deviceConnected = false;        // Device connection status flag
char BLEbuf[32] = {0};               // Buffer to store received BLE data

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // Define the UUID for the service
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // Define the UUID for the receive characteristic
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // Define the UUID for the transmit characteristic

// Custom BLE server callback class to handle connection and disconnection events
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;        // Set the device connection status to true
        Serial.println("------> BLE connect ."); // Print the device connection message
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;       // Set the device connection status to false
        Serial.println("------> BLE disconnect ."); // Print the device disconnection message
        pServer->startAdvertising();    // Restart BLE advertising
        Serial.println("start advertising"); // Print the start advertising message
    }
};

// Custom BLE characteristic callback class to handle characteristic write events
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue(); // Get the value of the characteristic

        if (rxValue.length() > 0) { // If the length of the received data is greater than 0
            Serial.print("------>Received Value: "); // Print the received value
            for (int i = 0; i < rxValue.length(); i++) { // Iterate through the received data
                Serial.print(rxValue[i]); // Print each character
            }
            Serial.println(); // New line

            // Perform different actions based on the received data content
            if (rxValue.find("A") != -1) {
                Serial.print("Rx A!"); // If the received value contains "A", print "Rx A!"
            }
            else if (rxValue.find("B") != -1) {
                Serial.print("Rx B!"); // If the received value contains "B", print "Rx B!"
            }
            Serial.println(); // New line
        }
    }
};

uint8_t ImageBW[27200]; // Declare an array of size 27200 bytes to store image data

void setup() {
    // Initialization settings, executed only once
    pinMode(7, OUTPUT); // Set GPIO pin 7 to output mode to control the screen power
    digitalWrite(7, HIGH); // Set GPIO pin 7 to high level to turn on the screen power

    BLEDevice::init("CrowPanel5-79"); // Initialize the BLE device and set the device name to "CrowPanel5-79"
    pServer = BLEDevice::createServer(); // Create a BLE server
    pServer->setCallbacks(new MyServerCallbacks()); // Set the server callback class to handle connection and disconnection events

    pService = pServer->createService(SERVICE_UUID); // Create a BLE service and specify the service UUID
    // Create a transmit characteristic with notify property
    pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
    pCharacteristic->addDescriptor(new BLE2902()); // Add a BLE2902 descriptor for characteristic notification

    // Create a receive characteristic with write property and set the callback class to handle write events
    BLECharacteristic *pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
    pCharacteristic->setCallbacks(new MyCallbacks());

    pService->start(); // Start the BLE service
    pServer->getAdvertising()->start(); // Start BLE advertising to make the device discoverable and connectable
}
int flag = 0;  // Define and initialize a flag variable to track device connection status and display content

void loop() {
  // Main loop, code is executed repeatedly

  if (deviceConnected) {  // Check if the device is connected
    memset(BLEbuf, 0, 32);  // Zero out the first 32 bytes of the BLEbuf buffer
    memcpy(BLEbuf, (char*)"Hello BLE APP!", 32);  // Copy the string "Hello BLE APP!" to BLEbuf

    pCharacteristic->setValue(BLEbuf);  // Set the characteristic value to the content of BLEbuf

    pCharacteristic->notify(); // Send a notification to the application with the value of BLEbuf
    Serial.print("*** Sent Value: ");  // Print the sent value to the serial monitor
    Serial.print(BLEbuf);
    Serial.println(" ***");
    if (flag != 2)  // If the flag is not 2 (i.e., the connection status has not been processed before), set the flag to 1
      flag = 1;
  } else {
    if (flag != 4)  // If the device is not connected and the flag is not 4 (i.e., the disconnection status has not been processed before), set the flag to 3
      flag = 3;
  }

  if (flag == 1) {  // Device is connected and the flag is 1
    char buffer[30];  // Buffer for display
    EPD_GPIOInit();  // Initialize EPD GPIO
    Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);  // Create a new image with a white background
    Paint_Clear(WHITE);  // Clear the canvas with a white background

    Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);  // Create a new image again (possibly to ensure clarity)
    Paint_Clear(WHITE);  // Clear the canvas with a white background
    EPD_FastMode1Init();  // Initialize the fast mode of the EPD
    EPD_Display_Clear();  // Clear the EPD display content
    EPD_Update();  // Update the EPD display
    EPD_Clear_R26A6H();  // Clear the EPD cache

    strcpy(buffer, "Bluetooth connected");  // Set the string to display
    strcpy(BLEbuf, "Sent Value:");  // Set the prefix for the value to display
    strcat(BLEbuf, "Hello BLE APP!");  // Append the message content to the value prefix

    EPD_ShowString(0, 0 + 0 * 20, buffer, 16, BLACK);  // Display "Bluetooth connected" on the EPD
    EPD_ShowString(0, 0 + 1 * 20, BLEbuf, 16, BLACK);  // Display "Sent Value: Hello BLE APP!" on the EPD
    EPD_Display(ImageBW);  // Display the image on the EPD
    EPD_PartUpdate();  // Perform a partial update (refresh the display)
    EPD_DeepSleep();  // Set the EPD to deep sleep mode to save power

    flag = 2;  // Update the flag to 2, indicating that the device connection display has been processed
  } else if (flag == 3) {  // Device is not connected and the flag is 3
    char buffer[30];  // Buffer for display
    EPD_GPIOInit();  // Initialize EPD GPIO
    Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);  // Create a new image with a white background
    Paint_Clear(WHITE);  // Clear the canvas with a white background

    Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);  // Create a new image again (possibly to ensure clarity)
    Paint_Clear(WHITE);  // Clear the canvas with a white background
    EPD_FastMode1Init();  // Initialize the fast mode of the EPD
    EPD_Display_Clear();  // Clear the EPD display content
    EPD_Update();  // Update the EPD display
    EPD_Clear_R26A6H();  // Clear the EPD cache

    strcpy(buffer, "Bluetooth not connected!");  // Set the string to display

    EPD_ShowString(0, 0 + 0 * 20, buffer, 16, BLACK);  // Display "Bluetooth not connected!" on the EPD
    EPD_Display(ImageBW);  // Display the image on the EPD
    EPD_PartUpdate();  // Perform a partial update (refresh the display)
    EPD_DeepSleep();  // Set the EPD to deep sleep mode to save power

    flag = 4;  // Update the flag to 4, indicating that the device disconnection display has been processed
  }
  
  delay(1000);  // Delay for 1000 milliseconds (1 second) before re-entering the loop
}