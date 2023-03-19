
// Arduino Centronics printer capture tool 

// SD card pinout:
// MOSI: pin 11
// MISO: pin 12
// CLK:  pin 13
// CS:   pin 4

// LCD pinout:
//  Reset  8
//  Enable 9
//  Data0  4
//  Data1  5
//  Data2  6
//  Data3  7

// Parallel Printer Pinout:
//  
// // const int parallelPortErrorPin = 22; //fixed high
// // const int parallelPortSelectPin = 24; // fixed high
// // const int parallelPortPaperOutPin = 26; // fixed low
// const int parallelPortDataPins[8] = {0,1,2,3,14,15,16,17}; //data in
// const int parallelPortStrobePin = 18; //on receive
// const int parallelPortBusyPin = 19; //toggle status
// const int parallelPortAckPin = 10; //toggle confirm

#include <LiquidCrystal.h>
#include "SdFat.h"

#if SPI_DRIVER_SELECT != 2 
#error SPI_DRIVER_SELECT must be two in SdFat/SdFatConfig.h
#endif

// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 0

const uint8_t SD_CS_PIN = 4;
//
// Pin numbers in templates must be constants.
const uint8_t SOFT_MISO_PIN = 12;
const uint8_t SOFT_MOSI_PIN = 11;
const uint8_t SOFT_SCK_PIN  = 13;

// SdFat software SPI template
SoftSpiDriver<SOFT_MISO_PIN, SOFT_MOSI_PIN, SOFT_SCK_PIN> softSpi;
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(0), &softSpi)

SdFat sd;
File current_file;

// 10s timeout before considering the print completed
#define TIMEOUT_MS 10000
const int serialPortSpeed = 115200;

bool init_complete = false;
long last_update;

bool print_in_progress = false;
bool data_ready = false;
byte data = 0;
byte buff[512];
int buff_index = 0;
long file_size = 0;

// Global variables/flags
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);  // RS, EN, D4, D5, D6, D7

const int parallelPortDataPins[8] = {39,41,43,45,47,49,46,48}; //data in
const int parallelPortStrobePin = 18; //on receive
const int parallelPortErrorPin = 22; //fixed high
const int parallelPortSelectPin = 24; // fixed high
const int parallelPortPaperOutPin = 26; // fixed low
const int parallelPortBusyPin = 28; //toggle status
const int parallelPortAckPin = 30; //toggle confirm

void initSdCard() {
  // Initialize SD card
  Serial.println("Init SD");
    lcd.clear();
    lcd.print("--= Loading =--");

  pinMode(SD_CS_PIN, OUTPUT);
  if (!sd.begin(SD_CONFIG)) 
  {
    Serial.println("SD Init Failed");
    lcd.clear();
    lcd.print("! No SD card !");
    sd.initErrorHalt();
  }
  else
  {
    Serial.println("SD Init Ok");
    lcd.clear();
    lcd.print("--== Ready ==--");
  }
}

void initSerialPort() {

  // // Wait for USB Serial
  // while (!Serial) {
  //   yield();
  // }
  // Serial.println("Type any character to start");
  // while (!Serial.available()) {
  //   yield();
  // }

  Serial.begin(serialPortSpeed);
  delay(10);
  Serial.println("serial init");
  Serial.println();
  Serial.println();
  Serial.println();
}

void initLcdDisplay() {
  lcd.begin(16, 2);  
  delay(10);
  lcd.clear();  
}
void initParallelPort() {
  // Configure pins
  pinMode(parallelPortStrobePin, INPUT_PULLUP); // Strobe - normally high
  attachInterrupt(digitalPinToInterrupt(parallelPortStrobePin), StrobeFallingEdge, FALLING); // Attach to pin interrupt

  pinMode(parallelPortErrorPin, OUTPUT);  // Error - normally high
  digitalWrite(parallelPortErrorPin, true);

  pinMode(parallelPortSelectPin, OUTPUT);  // Select - normally high
  digitalWrite(parallelPortSelectPin, true);

  pinMode(parallelPortPaperOutPin, OUTPUT);  // Paper out - normally low
  digitalWrite(parallelPortPaperOutPin, false);

  pinMode(parallelPortBusyPin, OUTPUT);  // Busy - normally low
  digitalWrite(parallelPortBusyPin, false);
  
  pinMode(parallelPortAckPin, OUTPUT);  // Ack - normally high
  digitalWrite(parallelPortAckPin, true);

  pinMode(parallelPortDataPins[0], INPUT_PULLUP);  // D0
  pinMode(parallelPortDataPins[1], INPUT_PULLUP);  // D1
  pinMode(parallelPortDataPins[2], INPUT_PULLUP);  // D2
  pinMode(parallelPortDataPins[3], INPUT_PULLUP);  // D3
  pinMode(parallelPortDataPins[4], INPUT_PULLUP);  // D4
  pinMode(parallelPortDataPins[5], INPUT_PULLUP);  // D5
  pinMode(parallelPortDataPins[6], INPUT_PULLUP);  // D6
  pinMode(parallelPortDataPins[7], INPUT_PULLUP);  // D7
}

void setup() 
{
  initSerialPort();
  initLcdDisplay();  
  initSdCard();
  initParallelPort();
      
  // Update timeout
  last_update = millis();
  Serial.println("Init Complete");
  init_complete = true;
}


void loop() 
{
  if (data_ready)
  {
    // Receive byte
    buff[buff_index] = data;
    buff_index++;

    // Reset data ready flag
    data_ready = false;
    
    // Ack byte, reset busy
    digitalWrite(parallelPortAckPin, false);  // ACK
    delayMicroseconds(7);
    digitalWrite(parallelPortBusyPin, false);  // BUSY
    delayMicroseconds(5);
    digitalWrite(parallelPortAckPin, true);   // ACK

    // Reset timeout
    last_update = millis();

    // Actively printing?
    if (!print_in_progress)
    {
      // Just started printing. Create new file
      CreateNewFile();
      Serial.print("Receiving from printer.");
      file_size = 0;

      char nameBuffer[20];
      current_file.getName(nameBuffer, 20);

      // Update LCD
      lcd.clear();
      lcd.print("Prn to:");
      lcd.print(nameBuffer);      
    }
    print_in_progress = true;
  }

  // Check buffer size
  if(buff_index >= 512)
  {
    // Flush buffer to file
    Serial.print(".");
    WriteToFile(buff, sizeof(buff));   
    file_size += buff_index - 1;
    buff_index = 0;  

    // Update LCD
    lcd.setCursor(0, 1);
    lcd.print("Size:");
    lcd.print(file_size);
    lcd.print("B");   
  }  

  // Timeout
  if ( print_in_progress && (millis() > (last_update + TIMEOUT_MS)) )
  {
    // Timeout. Flush the buffer to file
    if (buff_index > 0)
    {
      WriteToFile(buff, buff_index - 1);
      file_size += buff_index - 1;
      buff_index = 0;
    }
    Serial.println(".Done");
    Serial.print("Closing file..");

    current_file.close();
    
    Serial.println("..Ok");
    print_in_progress = false;


      char nameBuffer[20];
      current_file.getName(nameBuffer, 20);

      // Update LCD

    // Update LCD
    lcd.clear();
    lcd.print("Done: ");
    lcd.print(nameBuffer);

    //TODO: should we look at header and rename file?
  } 
}

void CreateNewFile()
{  
  // Find unique file 
  char fname[30];  
  int i = 1;  
  do
  {
    sprintf (fname, "sa%03d.prn", i);
    i++;
  } while(sd.exists(fname));

  // Found new file
  Serial.println();
  current_file = sd.open(fname, FILE_WRITE);
  Serial.print("New file created: ");
  Serial.println(fname);
}

void WriteToFile(byte* b, int b_size)
{
  // Verify that the file is open
  if (current_file) 
  {
    current_file.write(b, b_size);
  }
  else 
  {
    Serial.println();
    Serial.println("Can't write to file");
  }
}

// Strobe pin on falling edge interrupt
void StrobeFallingEdge()
{
  // Be sure that init sequence is completed
    if (!init_complete)
    {
      return;
    }
    
  // Set busy signal
  digitalWrite(parallelPortBusyPin, true);
  
  // Read data from port
  data = (digitalRead(parallelPortDataPins[0]) << 0) | 
         (digitalRead(parallelPortDataPins[1]) << 1) | 
         (digitalRead(parallelPortDataPins[2]) << 2) | 
         (digitalRead(parallelPortDataPins[3]) << 3) |
         (digitalRead(parallelPortDataPins[4]) << 4) |
         (digitalRead(parallelPortDataPins[5]) << 5) |
         (digitalRead(parallelPortDataPins[6]) << 6) |
         (digitalRead(parallelPortDataPins[7]) << 7);
  
  // Set ready bit
  data_ready = true;    
}
