#include <Arduino.h>     // Includes Arduino core library files
#include "EPD.h"         // Includes library files for electronic paper screens
#include "Ap_29demo.h"   // Includes custom function libraries
#include "FS.h"          // File system library for file operations
#include "SPIFFS.h"      // SPIFFS file system library for file reading and writing
#include <WiFi.h>        // WiFi library for creating and managing WiFi connections
#include <Ticker.h>      // Ticker library for timing operations
#include <WebServer.h>   // WebServer library for creating HTTP servers

// Define an array to store image data, with a size of 27200 bytes
uint8_t ImageBW[27200];

#define txt_size 6768 // Define the size of the description picture file
#define pre_size 5888 // Define the size of the price tag picture file

// Define the pins for the buttons
#define HOME_KEY 2    // Pin for home key
#define EXIT_KEY 1    // Pin for exit key
#define PRV_KEY 6     // Pin for previous page key
#define NEXT_KEY 4    // Pin for next page key
#define OK_KEY 5      // Pin for confirm key

int NUM = 0; // Current page number

// Clear the display content on the electronic paper.
void clear_all()
{
  EPD_FastMode1Init(); // Initialize the fast mode of the electronic paper.
  EPD_Display_Clear(); // Clear the display content on the electronic paper.
  EPD_Update();        // Update the electronic paper display.
}

// Create a WebServer instance and listen on port 80.
WebServer server(80);
const char *AP_SSID = "ESP32_Config"; // WiFi hotspot name

// HTML form for the upload page.
String HTML_UPLOAD = "<form method=\"post\" action=\"ok\" enctype=\"multipart/form-data\"><input type=\"file\" name=\"msg\"><input class=\"btn\" type=\"submit\" name=\"submit\"value=\"Submit\"></form>";

// Handle requests to the root path and return the upload page.
void handle_root()
{
  server.send(200, "text/html", HTML_UPLOAD); // Send the HTML upload page.
}

// HTML page after a successful upload.
String HTML_OK = "<!DOCTYPE html>\
<html>\
<body>\
<h1>OK</h1>\
</body>\
</html>";

// File object for uploading files.
File fsUploadFile;          // For operating files in the SPIFFS file system.
unsigned char test1[26928]; // Array for storing file data.
size_t data_size = 0;      // Size of the file data.
String filename;           // Variable for storing the file name.
int i = 0;                // Index for the file name array.
unsigned char price_formerly[pre_size]; // Array for storing the uploaded preview image data.
unsigned char txt_formerly[txt_size];   // Array for storing the uploaded text image data.

int flag_txt = 0; // Flag indicating whether a txt file has been uploaded.
int flag_pre = 0; // Flag indicating whether a preview file has been uploaded.

// Handle requests for uploading files.
void okPage()
{
  server.send(200, "text/html", HTML_OK); // Send the success page.
  HTTPUpload &upload = server.upload();    // Get the uploaded file object.

  // Note: The initial size of upload.buf is only 1436 bytes. It needs to be adjusted to support large file uploads.
  // Modify the HTTP_UPLOAD_BUFLEN definition in the WebServer.h file to increase the initial size to 14360 bytes.
  if (upload.status == UPLOAD_FILE_END) // File upload ends.
  {
    Serial.println("draw file");
    Serial.println(upload.filename);  // Print the uploaded file name.
    Serial.println(upload.totalSize); // Print the total size of the file.

    // Determine the file type based on the file size.
    if (upload.totalSize == 6768) // If the file size is 6768 bytes, it is a txt file.
      filename = "txt.bin";
    else
      filename = "pre.bin"; // Otherwise, it is a preview file.

    // Save the received file.
    if (!filename.startsWith("/")) filename = "/" + filename;
    fsUploadFile = SPIFFS.open(filename, FILE_WRITE); // Open the file in write mode.
    fsUploadFile.write(upload.buf, upload.totalSize); // Write the file data.
    fsUploadFile.close(); // Close the file.
    Serial.println("Save successful");
    Serial.printf("Saved:");
    Serial.println(filename);

    // Store the data in the corresponding array based on the file size.
    if (upload.totalSize == txt_size)
    {
      for (int i = 0; i < 6768; i++) {
        txt_formerly[i] = upload.buf[i];
      }
      Serial.println("txt_formerly OK");
      flag_txt = 1; // Set the flag indicating that a txt file has been uploaded.
    } 
    else
    {
      for (int i = 0; i < pre_size; i++) {
        price_formerly[i] = upload.buf[i];
      }
      Serial.println("price_formerly OK");
      flag_pre = 1; // Set the flag indicating that a preview file has been uploaded.
    }

    EPD_GPIOInit();            // Initialize the GPIO pins of the EPD electronic ink screen.
    EPD_FastMode1Init();       // Initialize the fast mode 1 of the EPD screen.

    EPD_ShowPicture(0, 0, 792, 40, background_top, WHITE); // Display the background picture on the screen with a background color of white.

    // Display the corresponding image based on the uploaded file type.
    if (upload.totalSize!= 6768)
    {
        EPD_ShowPicture(500, 80, 256, 184, price_formerly, WHITE); // Display the preview image.
    }
    else
    {
        EPD_ShowPicture(30, 80, 376, 144, txt_formerly, WHITE); // Display the text image.
    }
    
    EPD_Display(ImageBW);      // Display the image stored in the ImageBW array.
    EPD_FastUpdate();          // Perform a fast update to refresh the screen.
    EPD_DeepSleep();          // Set the screen to deep sleep mode to save power.
  }
}


void setup() {
  Serial.begin(115200);  // Start serial communication with a baud rate of 115200.

  if (SPIFFS.begin()) {  // Start the SPIFFS file system.
    Serial.println("SPIFFS Started.");  // SPIFFS started successfully. Output a prompt message.
  } else {
    // If SPIFFS fails to start, try formatting the SPIFFS partition.
    if (SPIFFS.format()) {
      // Formatting successful. Output a prompt message and restart the device.
      Serial.println("SPIFFS partition formatted successfully");
      ESP.restart();  // Restart the device.
    } else {
      // Formatting failed. Output a prompt message.
      Serial.println("SPIFFS partition format failed");
    }
    return;  // Exit the setup function.
  }

  Serial.println("Try Connecting to ");

  WiFi.mode(WIFI_AP);  // Set the WiFi mode to AP (access point).
  boolean result = WiFi.softAP(AP_SSID, "");  // Start the WiFi hotspot with SSID as AP_SSID and no password.
  if (result) {
    IPAddress myIP = WiFi.softAPIP();  // Get the IP address of the hotspot.
    // Print hotspot information.
    Serial.println("");
    Serial.print("Soft-AP IP address = ");
    Serial.println(myIP);
    Serial.println(String("MAC address = ") + WiFi.softAPmacAddress().c_str());
    Serial.println("waiting...");
  } else {
    // If starting the hotspot fails, output a prompt message and delay for 3 seconds.
    Serial.println("WiFiAP Failed");
    delay(3000);
  }

  // Configure HTTP server routes.
  server.on("/", handle_root);  // Root directory route. Call handle_root function when accessing the root directory.
  server.on("/ok", okPage);  // /ok route. Call okPage function when accessing /ok.
  server.begin();  // Start the HTTP server.
  Serial.println("HTTP server started");  // Output HTTP server start success message.
  delay(100);  // Delay for 100 milliseconds.

  // Set the screen power control pin.
  pinMode(7, OUTPUT);  // Set pin 7 as output mode.
  digitalWrite(7, HIGH);  // Set pin 7 to high level to turn on the screen power.

  // Initialize the EPD (electronic paper display) screen.
  EPD_GPIOInit();  // Initialize the GPIO pins of the EPD.
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);  // Create a new canvas with size EPD_W x EPD_H and background color white.
  Paint_Clear(WHITE);  // Clear the canvas with background color white.

  EPD_FastMode1Init();  // Initialize the fast mode 1 of the EPD.
  EPD_Display_Clear();  // Clear the EPD display content.
  EPD_Update();  // Update the display.
  UI_price();  // Display price information.
}

void loop() {
  server.handleClient();  // Handle client requests.
}

void UI_price()
{
  EPD_GPIOInit();  // Reinitialize the GPIO pins of the EPD electronic ink screen.
  EPD_FastMode1Init();  // Reinitialize the fast mode 1 of the EPD screen.

  // Display the background image on the screen with background color white.
  EPD_ShowPicture(0, 0, 792, 40, background_top, WHITE); 

  EPD_Display(ImageBW);  // Display the image stored in the ImageBW array.
  EPD_FastUpdate();  // Perform a fast update to refresh the screen.
  EPD_DeepSleep();  // Set the screen to deep sleep mode to save power.

  if (SPIFFS.exists("/txt.bin")) {  // Check if the file /txt.bin exists.
    // File exists. Read the file content.
    File file = SPIFFS.open("/txt.bin", FILE_READ);  // Open the file for reading.
    if (!file) {
      Serial.println("Unable to open file for reading");  // If the file cannot be opened, output a prompt message.
      return;
    }
    // Read data from the file to the array.
    size_t bytesRead = file.read(txt_formerly, txt_size);

    Serial.println("File content:");
    while (file.available()) {
      Serial.write(file.read());  // Print the file content.
    }
    file.close();  // Close the file.

    flag_txt = 1;  // Set the flag indicating that a text file exists.

    EPD_GPIOInit();  // Reinitialize the GPIO pins of the EPD electronic ink screen.
    EPD_FastMode1Init();  // Reinitialize the fast mode 1 of the EPD screen.

    // Display the read picture on the screen at position (30, 80) with size 376x144 and background color white.
    EPD_ShowPicture(30, 80, 376, 144, txt_formerly, WHITE);

    EPD_Display(ImageBW);  // Display the image stored in the ImageBW array.
    EPD_FastUpdate();  // Perform a fast update to refresh the screen.
    EPD_DeepSleep();  // Set the screen to deep sleep mode to save power.
  }

  if (SPIFFS.exists("/pre.bin")) {  // Check if the file /pre.bin exists.
    // File exists. Read the file content.
    File file = SPIFFS.open("/pre.bin", FILE_READ);  // Open the file for reading.
    if (!file) {
      Serial.println("Unable to open file for reading");  // If the file cannot be opened, output a prompt message.
      return;
    }
    // Read data from the file to the array.
    size_t bytesRead = file.read(price_formerly, pre_size);

    Serial.println("File content:");
    while (file.available()) {
      Serial.write(file.read());  // Print the file content.
    }
    file.close();  // Close the file.
    flag_pre = 1;  // Set the flag indicating that a price file exists.

    EPD_GPIOInit();  // Reinitialize the GPIO pins of the EPD electronic ink screen.
    EPD_FastMode1Init();  // Reinitialize the fast mode 1 of the EPD screen.

    // Display the read price picture on the screen at position (500, 80) with size 256x184 and background color white.
    EPD_ShowPicture(500, 80, 256, 184, price_formerly, WHITE);

    EPD_Display(ImageBW);  // Display the image stored in the ImageBW array.
    EPD_FastUpdate();  // Perform a fast update to refresh the screen.
    EPD_DeepSleep();  // Set the screen to deep sleep mode to save power.
  }
}