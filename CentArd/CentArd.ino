
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
// Pin	Name	  Dir	Description	  Mega
// 1	  /STROBE	-->	Strobe	      23
// 2	  D0	    -->	Data Bit 0	  25
// 3	  D1	    -->	Data Bit 1	  27
// 4	  D2	    -->	Data Bit 2	  29
// 5	  D3	    -->	Data Bit 3	  31
// 6  	D4	    -->	Data Bit 4	  33
// 7  	D5	    -->	Data Bit 5	  35
// 8  	D6	    -->	Data Bit 6	  37
// 9  	D7	    -->	Data Bit 7	  39
// 10  	/ACK    <--	Acknowledge	  41
// 11  	BUSY    <--	Busy	        43
// 12  	PE	    <--	Paper End	    45
// 13	  SEL    	<--	Select	      47
// 14	  /AUTOFD	-->	Autofeed	    N/C
// 15  	/ERROR	<--	Error	        24
// 16	  /INIT	  -->	Initialize    N/C
// 17  	/SELIN	-->	Select In	    N/C
// 18	  GND	    ---	Signal Ground	GND
// 19	  GND	    ---	Signal Ground	GND
// 20	  GND	    ---	Signal Ground	GND
// 21  	GND	    ---	Signal Ground	GND
// 22  	GND	    ---	Signal Ground	GND
// 23	  GND	    ---	Signal Ground	GND
// 24	  GND	    ---	Signal Ground	GND
// 25	  GND     ---	Signal Ground	GND

#include <LiquidCrystal.h>
#include "SdFat.h"

#if SPI_DRIVER_SELECT != 2
#error SPI_DRIVER_SELECT must be two in SdFat/SdFatConfig.h
#endif

// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 0

const uint8_t SD_CS_PIN = 4; //4;
//
// Pin numbers in templates must be constants.
const uint8_t SOFT_MISO_PIN = 12;
const uint8_t SOFT_MOSI_PIN = 11;
const uint8_t SOFT_SCK_PIN = 13;

// SdFat software SPI template
SoftSpiDriver<SOFT_MISO_PIN, SOFT_MOSI_PIN, SOFT_SCK_PIN> softSpi;
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(0), &softSpi)

SdFat sd;
File file;

// 10s timeout before considering the print completed
#define TIMEOUT_MS 10000
//const long serialPortSpeed = 115200;
const long serialPortSpeed = 2000000;

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

const int parallelPortStrobePin = 23;                             // on receive
const int parallelPortDataPins[8] = { 25,27,29,31,33,35,37,39 };  // data in
const int parallelPortAckPin = 41;                                // toggle confirm
const int parallelPortBusyPin = 43;                               // toggle status
const int parallelPortPaperOutPin = 45;                           // fixed low
const int parallelPortSelectPin = 47;                             // fixed high
const int parallelPortErrorPin = 24;                              // fixed high


// const int parallelPortStrobePin = 18;                             // on receive
// const int parallelPortDataPins[8] = { 39,41,43,45,47,49,46,46 };  // data in
// const int parallelPortAckPin = 30;                                // toggle confirm
// const int parallelPortBusyPin = 28;                               // toggle status
// const int parallelPortPaperOutPin = 26;                           // fixed low
// const int parallelPortSelectPin = 24;                             // fixed high
// const int parallelPortErrorPin = 22;                              // fixed high

void initSdCard() {
  // Initialize SD card
  Serial.println("Init SD");
  lcd.clear();
  lcd.print("--= Loading =--");

  pinMode(SD_CS_PIN, OUTPUT);
  if (!sd.begin(SD_CONFIG)) {
    Serial.println("SD Init Failed");
    lcd.clear();
    lcd.print("! No SD card !");
    sd.initErrorHalt();
  } else {
    Serial.println("SD Init Ok");
    lcd.clear();
    lcd.print("--== Ready ==--");
  }
}

void initSerialPort() {
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
  lcd.print("--= LCD INIT =--");
  Serial.println("lcd init");
  Serial.println();
}
void initParallelPort() {
  // Configure pins
  pinMode(parallelPortStrobePin, INPUT_PULLUP);                                               // Strobe - normally high
  attachInterrupt(digitalPinToInterrupt(parallelPortStrobePin), StrobeFallingEdge, FALLING);  // Attach to pin interrupt

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

void setup() {
  initSerialPort();
  initLcdDisplay();
  initSdCard();
  initParallelPort();

  // Update timeout
  last_update = millis();
  Serial.println("Init Complete");
  init_complete = true;
}

int t = 0;

void loop() {

  if (data_ready) {
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
    digitalWrite(parallelPortAckPin, true);  // ACK

    // Reset timeout
    last_update = millis();

    // Actively printing?
    if (!print_in_progress) {
      // Just started printing. Create new file
      if (CreateNewFile()) {
        Serial.print("Receiving from printer.");
        file_size = 0;

        char nameBuffer[20];
        file.getName(nameBuffer, 20);

        // Update LCD
        lcd.clear();
        lcd.print("Prn to:");
        lcd.print(nameBuffer);
      } else {
        Serial.println("Error Creating File.");        
        digitalWrite(parallelPortErrorPin, false);
        lcd.clear();
        lcd.print("--== ERROR ==--");
        delay(5 * 1000);        
        digitalWrite(parallelPortBusyPin, false);
        digitalWrite(parallelPortErrorPin, true);
        
        sd.errorHalt(F("open failed"));

        return;
      }
    }
    print_in_progress = true;
  }

  // Check buffer size
  if (buff_index >= 512) {
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
  if (print_in_progress && (millis() > (last_update + TIMEOUT_MS))) {
    Serial.print("-> TIMEOUT <-");
    digitalWrite(parallelPortErrorPin, true);
    // Timeout. Flush the buffer to file
    if (buff_index > 0) {
      WriteToFile(buff, buff_index - 1);
      file_size += buff_index - 1;
      buff_index = 0;
    }
    Serial.println(".Done");
    Serial.print("Closing file..");

    file.close();

    Serial.println("..Ok");
    print_in_progress = false;

    char nameBuffer[20];
    file.getName(nameBuffer, 20);

    // Update LCD

    // Update LCD
    lcd.clear();
    lcd.print("Done: ");
    lcd.print(nameBuffer);

    //TODO: should we look at header and rename file?
  }
}

int CreateNewFile() {
  // Find unique file
  char fname[30];
  int i = 1;
  do {
    sprintf(fname, "sa%03d.prn", i);
    i++;
  } while (sd.exists(fname));

  // Found new file
  Serial.println();
  // fname
  
  if (!sd.begin(SD_CONFIG)) {
    Serial.print("init error :( ");
    sd.initErrorHalt();
  }

  file = sd.open(fname, O_RDWR | O_CREAT);
  if (file) {
    Serial.print("New file created: ");
    Serial.println(fname);
    return -1;
  } else {
    Serial.print("error creating file: ");
    Serial.println(fname);
    return 0;
  }
}

void WriteToFile(byte* b, int b_size) {
  // Verify that the file is open
  if (file) {
    file.write(b, b_size);
    //file.sync();
  } else {
    Serial.println();
    Serial.println("Can't write to file");
  }
}

// Strobe pin on falling edge interrupt
void StrobeFallingEdge() {
  // Be sure that init sequence is completed
  if (!init_complete) {
    return;
  }

  // Set busy signal
  digitalWrite(parallelPortBusyPin, true);

  // Read data from port
  data = (digitalRead(parallelPortDataPins[0]) << 0) | (digitalRead(parallelPortDataPins[1]) << 1) | (digitalRead(parallelPortDataPins[2]) << 2) | (digitalRead(parallelPortDataPins[3]) << 3) | (digitalRead(parallelPortDataPins[4]) << 4) | (digitalRead(parallelPortDataPins[5]) << 5) | (digitalRead(parallelPortDataPins[6]) << 6) | (digitalRead(parallelPortDataPins[7]) << 7);

  // Set ready bit
  data_ready = true;
}
