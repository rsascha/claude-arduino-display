#include <WiFi.h>
#include <Arduino.h>  // Include the Arduino library for basic functionality
#include "EPD.h"      // Include the e-paper display library for controlling the e-paper display
#include <ArduinoJson.h>  // Include the ArduinoJson library for handling JSON data
#include <HTTPClient.h>   // Include the HTTPClient library for making HTTP requests
#include "pic.h"          // Include custom image files, typically used for display

// WiFi configuration
const char * ID = "yanfa_software";  // WiFi SSID
const char * PASSWORD = "yanfa-123456";  // WiFi password

// Weather-related parameters
String API = "SBy5V5m7FkAn1e_vi";  // Weather API key
String WeatherURL = "";  // Weather API request URL
String CITY = "深圳";  // City to query
String url_xinzhi = "";  // Used to build the complete weather API request URL
String Weather = "0";  // Store the retrieved weather information

long sum = 0;  // For other calculations or recording

// Define a black and white image array as the buffer for the e-paper display
uint8_t ImageBW[27200];  // Define the size based on the resolution of the e-paper display

// Create an HTTPClient instance
HTTPClient http;

// Build the weather API request URL
String GitURL(String api, String city)
{
  url_xinzhi =  "https://api.seniverse.com/v3/weather/now.json?key=";  // Base URL
  url_xinzhi += api;  // Add API key
  url_xinzhi += "&location=";  // Add city parameter
  url_xinzhi += city;  // Add city name
  url_xinzhi += "&language=zh-Hans&unit=c";  // Set language and temperature unit
  return url_xinzhi;  // Return the complete request URL
}

// Parse weather data
void ParseWeather(String url)
{
  DynamicJsonDocument doc(1024);  // Dynamically allocate 1024 bytes of memory for parsing JSON data
  http.begin(url);  // Start the HTTP request

  int httpGet = http.GET();  // Send GET request
  if (httpGet > 0)
  {
    Serial.printf("HTTPGET is %d\n", httpGet);  // Print the request status

    if (httpGet == HTTP_CODE_OK)
    {
      String json = http.getString();  // Get the response content
      Serial.println(json);  // Print the response content

      deserializeJson(doc, json);  // Parse the JSON data

      // Extract weather information
      Weather = doc["results"][0]["now"]["text"].as<String>();
    }
    else
    {
      Serial.printf("ERROR1!!\n");  // Print error message
    }
  }
  else
  {
    Serial.printf("ERROR2!!\n");  // Print error message
  }
  http.end();  // End the HTTP request
}

// Create a character array for displaying information
char buffer[40];

void setup()
{
  Serial.begin(115200);  // Initialize serial communication with a baud rate of 115200

  //================== WiFi Connection ==================
  Serial.println("WiFi:");
  Serial.println(ID);  // Print WiFi SSID
  Serial.println("PASSWORD:");
  Serial.println(PASSWORD);  // Print WiFi password

  WiFi.begin(ID, PASSWORD);  // Connect to WiFi

  // Wait for a successful connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);  // Try every 500 milliseconds
    Serial.println("Connecting...");
  }

  Serial.println("Connection successful!");  // Print connection success message
  //================== WiFi Connection ==================

  WeatherURL = GitURL(API, CITY);  // Generate the weather API request URL

  // Set the screen power pin to output mode and set it to high level to turn on the power
  pinMode(7, OUTPUT);  // Set GPIO 7 to output mode
  digitalWrite(7, HIGH);  // Set GPIO 7 to high level

  // Initialize the e-paper display
  EPD_GPIOInit();  // Initialize the GPIO pins of the e-paper

  Serial.println("Connection successful!");  // Print connection success message
}

void loop()
{
  ParseWeather(WeatherURL);  // Parse weather data

  PW(WeatherURL);  // Assume PW is a custom function for processing weather data display or other operations
  delay(1000 * 60 * 60); // Main loop delay for 1 hour
}

void PW(String url)
{
  // Create a dynamic JSON document, allocating 1024 bytes of memory
  DynamicJsonDocument doc(1024); 
  
  // Start the HTTP request
  http.begin(url);

  // Initiate the GET request
  int httpGet = http.GET();
  
  if (httpGet > 0)
  {
    // Print the HTTP GET request response code
    Serial.printf("HTTPGET is %d\n", httpGet);

    // Check if the HTTP response is successful
    if (httpGet == HTTP_CODE_OK)
    {
      // Get the HTTP response content (JSON string)
      String json = http.getString();
      Serial.println(json);

      // Parse the JSON string
      deserializeJson(doc, json);

      // Extract information from the parsed JSON
      String location = doc["results"][0]["location"]["name"].as<String>(); // City name
      String weatherText = doc["results"][0]["now"]["text"].as<String>();   // Current weather description
      String temperature = doc["results"][0]["now"]["temperature"].as<String>(); // Current temperature
      String humidity = doc["results"][0]["now"]["humidity"].as<String>();  // Current humidity
      String windSpeed = doc["results"][0]["now"]["wind"]["speed"].as<String>(); // Current wind speed

      String Country = doc["results"][0]["location"]["country"].as<String>(); // Country
      String Timezone = doc["results"][0]["location"]["timezone"].as<String>(); // Timezone
      String last_update = doc["results"][0]["last_update"].as<String>(); // Last update time

      // Create a character array for storing information
      char buffer[40];

      // Clear the image and initialize the e-ink screen
      Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE); // Create a new image
      Paint_Clear(WHITE); // Clear the image content
      EPD_FastMode1Init(); // Initialize the e-ink screen
      EPD_Display_Clear(); // Clear the screen display
      EPD_Update(); // Update the screen
      EPD_Clear_R26A6H(); // Clear the e-ink screen cache

      // Display the home icon
      EPD_ShowPicture(0, 0, 32, 32, home_2, WHITE);

      // Display the weather update time UI
      EPD_ShowPicture(0, 180, 32, 32, weather, WHITE);
      EPD_ShowString(50, 180, "Last Time :", 24, BLACK); // Display text "Last Time :"

      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s ", last_update.c_str()); // Format the update time as a string
      EPD_ShowString(10, 210, buffer, 24, BLACK); // Display the update time

      // Update the city name UI
      EPD_ShowString(55, 80, "City :", 24, BLACK); // Display text "City :"
      EPD_ShowPicture(0, 80, 48, 48, city, WHITE); // Display the city icon

      memset(buffer, 0, sizeof(buffer));
      if (strcmp(location.c_str(), "深圳") == 0)
      {
        snprintf(buffer, sizeof(buffer), " %s", "Sheng Zhen"); // If the city is Shenzhen, display "Sheng Zhen"
      }
      else
      {
        snprintf(buffer, sizeof(buffer), "%s", "Null"); // Otherwise, display "Null"
      }
      EPD_ShowString(10, 110, buffer, 24, BLACK); // Display the city name

      // Update the timezone UI
      EPD_ShowString(610, 180, "time zone :", 24, BLACK); // Display text "time zone :"
      EPD_ShowPicture(550, 170, 48, 48, time_zone, WHITE); // Display the timezone icon

      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s ", Timezone.c_str()); // Format the timezone as a string
      EPD_ShowString(600, 210, buffer, 24, BLACK); // Display the timezone

      // Update the temperature UI
      EPD_ShowString(610, 90, "Temp :", 24, BLACK); // Display text "Temp :"
      EPD_ShowPicture(550, 90, 32, 32, temp, WHITE); // Display the temperature icon

      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s C", temperature.c_str()); // Format the temperature as a string
      EPD_ShowString(600, 120, buffer, 48, BLACK); // Display the temperature

      // Update the weather condition UI
      memset(buffer, 0, sizeof(buffer));
      if (strcmp(weatherText.c_str(), "大雨") == 0)
      {
        snprintf(buffer, sizeof(buffer), "Weather:%s", "Heavy rain"); // If the weather is "大雨", display "Heavy rain"
        EPD_ShowPicture(370, 60, 80, 80, heavy_rain, WHITE); // Display the "大雨" icon
      }
      else if (strcmp(weatherText.c_str(), "多云") == 0)
      {
        snprintf(buffer, sizeof(buffer), "Weather:%s", "Cloudy"); // If the weather is "多云", display "Cloudy"
        EPD_ShowPicture(370, 60, 80, 80, cloudy, WHITE); // Display the "多云" icon
      }
      else if (strcmp(weatherText.c_str(), "小雨") == 0)
      {
        snprintf(buffer, sizeof(buffer), "Weather:%s", "Small rain"); // If the weather is "小雨", display "Small rain"
        EPD_ShowPicture(370, 60, 80, 80, small_rain, WHITE); // Display the "小雨" icon
      }
      else if (strcmp(weatherText.c_str(), "晴") == 0)
      {
        snprintf(buffer, sizeof(buffer), "Weather:%s", "Clear day"); // If the weather is "晴", display "Clear day"
        EPD_ShowPicture(370, 60, 80, 80, clear_day, WHITE); // Display the "晴" icon
      }
      EPD_ShowString(330, 160, buffer, 24, BLACK); // Display the weather description

      // Draw partition lines
      EPD_DrawLine(0, 40, 792, 40, BLACK); // Draw a horizontal line
      EPD_DrawLine(320, 40, 320, 270, BLACK); // Draw a vertical line
      EPD_DrawLine(540, 40, 540, 270, BLACK); // Draw a vertical line

      // Update the e-ink screen display content
      EPD_Display(ImageBW); // Display the image
      EPD_PartUpdate(); // Partially update the screen
      EPD_DeepSleep(); // Enter deep sleep mode

      // Update the Weather variable
      Weather = weatherText;
    }
    else
    {
      // If the HTTP response code is not 200, print an error message
      Serial.println("ERROR1!!");
    }
  }
  else
  {
    // If the HTTP GET request fails, print an error message
    Serial.println("ERROR2!!");
  }
  // End the HTTP request
  http.end();
}