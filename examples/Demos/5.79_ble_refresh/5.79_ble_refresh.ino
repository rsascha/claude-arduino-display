#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "Ap_29demo.h"
#include <Arduino.h>
#include "EPD.h"
#include "FS.h"           // File system library
#include "SPIFFS.h"       // SPIFFS file system library for file reading and writing

// Image buffer to store black and white image data
uint8_t ImageBW[27200];
#define txt_size 6768
#define pre_size 5888

// UUIDs for BLE service and characteristic
#define SERVICE_UUID "fb1e4001-54ae-4a28-9f74-dfccb248601d"
#define CHARACTERISTIC_UUID "fb1e4002-54ae-4a28-9f74-dfccb248601d"

// BLE characteristic object
BLECharacteristic *pCharacteristicRX;
std::vector<uint8_t> dataBuffer; // Buffer to accumulate received data
size_t totalReceivedBytes = 0;   // Total number of received bytes
bool dataReceived = false;       // Flag indicating whether data has been received completely

unsigned char price_formerly[pre_size]; // Stores uploaded image data
unsigned char txt_formerly[txt_size]; // Stores uploaded image data
// Object for storing uploaded files
File fsUploadFile;
unsigned char test1[26928]; // Array for storing file data
size_t data_size = 0; // Size of file data

String filename; // Array for storing file names

// BLE characteristic callback class
class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      if (value.length() > 0) {
        Serial.printf(".");

        // Assuming the client sends a specific end marker when data transmission is complete
        if (value == "OK") {
          dataReceived = true;
          return;
        }

        size_t len = value.length();
        if (len > 0) {
          // Append received data to the buffer
          dataBuffer.insert(dataBuffer.end(), value.begin(), value.end());
          totalReceivedBytes += len; // Update total received bytes
        }


      }
    }
};

// Setup function to initialize hardware and BLE
void setup() {
  Serial.begin(115200); // Initialize serial communication

  if (SPIFFS.begin()) {  // Start the SPIFFS file system
    Serial.println("SPIFFS Started.");
  } else {
    // If SPIFFS fails to start, try formatting the SPIFFS partition
    if (SPIFFS.format()) {
      // Formatting successful, print message and restart the device
      Serial.println("SPIFFS partition formatted successfully");
      ESP.restart();
    } else {
      // Formatting failed, print message
      Serial.println("SPIFFS partition format failed");
    }
    return;
  }

  // Initialize BLE
  BLEDevice::init("ESP32_S3_BLE");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristicRX = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_WRITE
                      );

  // Set the callback function for the BLE characteristic
  pCharacteristicRX->setCallbacks(new MyCallbacks());
  pCharacteristicRX->addDescriptor(new BLE2902());

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  // Initialize the display
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH); // Turn on the display power
  EPD_GPIOInit(); // Initialize EPD control pins
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE); // Create a new image
  Paint_Clear(WHITE); // Clear the image

  EPD_FastMode1Init(); // Initialize EPD fast mode
  EPD_Display_Clear(); // Clear the display
  EPD_Update(); // Update the display

  UI_price(); // Display the price interface
}



// Main loop function
void loop() {
  // If the flag dataReceived is true, process the data
  main_ui(); // Process main interface update
}

// Process BLE data and save it to a file
void ble_pic()
{
  // Check if data has been received
  if (dataReceived) {
    // Ensure the data buffer is not empty
    if (!dataBuffer.empty()) {
      size_t bufferSize = dataBuffer.size();
      Serial.println(bufferSize);
      if (dataBuffer.size() == txt_size)//txt size
        filename = "txt.bin";
      else
        filename = "pre.bin";
      if (!filename.startsWith("/")) filename = "/" + filename;
      fsUploadFile = SPIFFS.open(filename, FILE_WRITE);
      fsUploadFile.write(dataBuffer.data(), dataBuffer.size());
      fsUploadFile.close();
      Serial.println("Saved successfully");
      Serial.printf("Saved:");
      Serial.println(filename);
      if (bufferSize == txt_size )
      {
        for (int i = 0; i < txt_size; i++) {
          txt_formerly[i] = dataBuffer[i];
        }
        //      memcpy(txt_formerly, upload.buf, sizeof(upload.totalSize));
        Serial.println("txt_formerly OK");

      } else
      {
        for (int i = 0; i < pre_size; i++) {
          price_formerly[i] = dataBuffer[i];
        }
        //      memcpy(price_formerly, upload.buf, sizeof(upload.totalSize));
        Serial.println(" price_formerlyOK");

      }

      EPD_GPIOInit();            // Reinitialize the GPIO pin configuration of the EPD e-ink screen
      EPD_FastMode1Init();       // Reinitialize EPD screen's fast mode 1



      EPD_ShowPicture(0, 0, 792, 40, background_top, WHITE); // Display the gImage_scenario_home picture on the screen with a background color of white

      if (bufferSize!= txt_size)
      {

        EPD_ShowPicture(500, 80, 256, 184, price_formerly, WHITE); // Display the gImage_scenario_home picture on the screen with a background color of white
        //         EPD_ShowPicture(400, 60, 256, 184, price_formerly, WHITE); // Display the gImage_scenario_home picture on the screen with a background color of white

      } else
      {
        //          EPD_ShowPicture(400, 60, 256, 184, price_formerly, WHITE); // Display the gImage_scenario_home picture on the screen with a background color of white

        EPD_ShowPicture(30, 80, 376, 144, txt_formerly, WHITE); // Display the gImage_scenario_home picture on the screen with a background color of white

      }

      EPD_Display(ImageBW);      // Display the image stored in the ImageBW array
      EPD_FastUpdate();          // Perform a fast update to refresh the screen
      EPD_DeepSleep();          // Set the screen to deep sleep mode to save power


      // Clear the data buffer after writing
      dataBuffer.clear();
      totalReceivedBytes = 0;


      // Clear the data buffer
      dataBuffer.clear(); // Clear the buffer after writing
      totalReceivedBytes = 0;


    }

    // Reset the data received flag after processing the data
    dataReceived = false;
  }
}

// Clear the content of the display
void clear_all()
{
  // Initialize the fast mode of the e-ink screen
  EPD_FastMode1Init();
  // Clear the e-ink screen
  EPD_Display_Clear();
  // Update the display
  EPD_Update();
}


// Main user interface processing function
void main_ui()
{
  // Process BLE data
  ble_pic();


}

// User interface function: Display price information
void UI_price()
{
  EPD_GPIOInit();            // Reinitialize the GPIO pin configuration of the EPD e-ink screen
  EPD_FastMode1Init();       // Reinitialize EPD screen's fast mode 1
  EPD_ShowPicture(0, 0, 792, 40, background_top, WHITE); // Display the gImage_scenario_home picture on the screen with a background color of white


  if (SPIFFS.exists("/txt.bin")) {
    // File exists, read the file content
    File file = SPIFFS.open("/txt.bin", FILE_READ);
    if (!file) {
      Serial.println("Unable to open file for reading");
      return;
    }
    // Read data from the file into an array
    size_t bytesRead = file.read(txt_formerly, txt_size);

    Serial.println("File content:");
    while (file.available()) {
      Serial.write(file.read());
    }
    file.close();


    EPD_ShowPicture(30, 80, 376, 144, txt_formerly, WHITE);


  }

  if (SPIFFS.exists("/pre.bin")) {
    // File exists, read the file content
    File file = SPIFFS.open("/pre.bin", FILE_READ);
    if (!file) {
      Serial.println("Unable to open file for reading");
      return;
    }
    // Read data from the file into an array
    size_t bytesRead = file.read(price_formerly, pre_size);

    Serial.println("File content:");
    while (file.available()) {
      Serial.write(file.read());
    }
    file.close();



    EPD_ShowPicture(500, 80, 256, 184, price_formerly, WHITE);


  }
  EPD_Display(ImageBW);      // Display the image stored in the ImageBW array
  EPD_FastUpdate();          // Perform a fast update to refresh the screen
  EPD_DeepSleep();          // Set the screen to deep sleep mode to save power
}