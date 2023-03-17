// Arduino Centronics printer capture tool 
// Using Ethernet shield with built-in SD card
// SD card pinout:
// MOSI: pin 11
// MISO: pin 12
// CLK:  pin 13
// CS:   pin 4

// LCD pinout:
// EN: pin A7

#define __BOARD_UNO
//#define __BOARD_MEGA

#define __ENABLE_LCD
#define __ENABLE_SDCARD
//#define __ENABLE_PARALLEL

#if defined(__ENABLE_LCD)
#include <LiquidCrystal.h>
#endif

#if defined(__ENABLE_SDCARD)
#include <SPI.h>
#include <SD.h>
#endif

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
#if defined(__ENABLE_LCD)
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);  // RS, EN, D4, D5, D6, D7
#endif

#if defined(__ENABLE_SDCARD)
//SD card must only be used when LCD is disabled
const int sdcardPin = 4;  //SD Card uses SPI pins with CS below 
#endif

#if defined(__ENABLE_PARALLEL)
#if defined(__BOARD_UNO)  
  // const int parallelPortErrorPin = 22; //fixed high
  // const int parallelPortSelectPin = 24; // fixed high
  // const int parallelPortPaperOutPin = 26; // fixed low
  const int parallelPortDataPins[8] = {0,1,2,3,14,15,16,17}; //data in
  const int parallelPortStrobePin = 18; //on receive
  const int parallelPortBusyPin = 19; //toggle status
  const int parallelPortAckPin = 10; //toggle confirm
#elif defined(__BOARD_MEGA)
  const int parallelPortDataPins[8] = {39,41,43,45,47,49,46,48}; //data in
  const int parallelPortStrobePin = 18; //on receive
  const int parallelPortErrorPin = 22; //fixed high
  const int parallelPortSelectPin = 24; // fixed high
  const int parallelPortPaperOutPin = 26; // fixed low
  const int parallelPortBusyPin = 28; //toggle status
  const int parallelPortAckPin = 30; //toggle confirm
#endif
#endif

#if defined(__ENABLE_SDCARD)  
File current_file;
void initSdCard() {
  // Initialize SD card
  Serial.println("Init SD");

  pinMode(sdcardPin, OUTPUT);
  //pinMode(53, OUTPUT);  // HW CS pin, init it just in case
  if ( !SD.begin(sdcardPin) ) 
  {
    Serial.println("SD Init Failed");

#if defined(__ENABLE_LCD)    
    lcd.print("! No SD card !");
#endif
  }
  else
  {
    Serial.println("SD Init Ok");

#if defined(__ENABLE_LCD)
    lcd.print("--== Ready ==--");
#endif
  }
}
#endif

void initSerialPort() {
  Serial.begin(serialPortSpeed);
  delay(10);
  Serial.println();
  Serial.println();
  Serial.println();
}

#if defined(__ENABLE_LCD)
void initLcdDisplay() {
  lcd.begin(16, 2);  
  delay(10);
  lcd.clear();  
}
#endif

#if defined(__ENABLE_PARALLEL)
void initParallelPort() {
  // Configure pins
  pinMode(parallelPortStrobePin, INPUT_PULLUP); // Strobe - normally high
  attachInterrupt(digitalPinToInterrupt(parallelPortStrobePin), StrobeFallingEdge, FALLING); // Attach to pin interrupt
    
#if !defined(__BOARD_UNO) 
  pinMode(parallelPortErrorPin, OUTPUT);  // Error - normally high
  digitalWrite(parallelPortErrorPin, true);

  pinMode(parallelPortSelectPin, OUTPUT);  // Select - normally high
  digitalWrite(parallelPortSelectPin, true);

  pinMode(parallelPortPaperOutPin, OUTPUT);  // Paper out - normally low
  digitalWrite(parallelPortPaperOutPin, false);
#endif

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
#endif

void setup() 
{
  initSerialPort();
#if defined(__ENABLE_LCD)
  initLcdDisplay();  
#endif
#if defined(__ENABLE_SDCARD) 
  initSdCard();
#endif
#if defined(__ENABLE_PARALLEL)
  initParallelPort();
#endif
      
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
    
#if defined(__ENABLE_PARALLEL)
    // Ack byte, reset busy
    digitalWrite(parallelPortAckPin, false);  // ACK
    delayMicroseconds(7);
    digitalWrite(parallelPortBusyPin, false);  // BUSY
    delayMicroseconds(5);
    digitalWrite(parallelPortAckPin, true);   // ACK
  #endif

    // Reset timeout
    last_update = millis();

    // Actively printing?
    if (!print_in_progress)
    {
      // Just started printing. Create new file
#if defined(__ENABLE_SDCARD) 
      CreateNewFile();
#endif
      Serial.print("Receiving from printer.");
      file_size = 0;

#if defined(__ENABLE_LCD)
      // Update LCD
      lcd.clear();
      lcd.print("Prn to:");
#if defined(__ENABLE_SDCARD) 
      lcd.print(current_file.name());      
#endif  
#endif
    }
    print_in_progress = true;
  }

  // Check buffer size
  if(buff_index >= 512)
  {
    // Flush buffer to file
    Serial.print(".");
#if defined(__ENABLE_SDCARD) 
    WriteToFile(buff, sizeof(buff));   
#endif
    file_size += buff_index - 1;
    buff_index = 0;  

#if defined(__ENABLE_LCD)
    // Update LCD
    lcd.setCursor(0, 1);
    lcd.print("Size:");
    lcd.print(file_size);
    lcd.print("B");   
#endif
  }  

  // Timeout
  if ( print_in_progress && (millis() > (last_update + TIMEOUT_MS)) )
  {
    // Timeout. Flush the buffer to file
    if (buff_index > 0)
    {
#if defined(__ENABLE_SDCARD) 
      WriteToFile(buff, buff_index - 1);
#endif
      file_size += buff_index - 1;
      buff_index = 0;
    }
    Serial.println(".Done");
    Serial.print("Closing file..");

#if defined(__ENABLE_SDCARD) 
    current_file.close();
#endif
    
    Serial.println("..Ok");
    print_in_progress = false;

#if defined(__ENABLE_LCD)
    // Update LCD
    lcd.clear();
    lcd.print("Done: ");
#if defined(__ENABLE_SDCARD) 
    lcd.print(current_file.name());
#endif
    //TODO: should we look at header and rename file?
#endif
  } 
}

#if defined(__ENABLE_SDCARD) 
void CreateNewFile()
{  
  // Find unique file 
  char fname[30];  
  int i = 1;  
  do
  {
    sprintf (fname, "sa%03d.prn", i);
    i++;
  } while(SD.exists(fname));

  // Found new file
  Serial.println();
  current_file = SD.open(fname, FILE_WRITE);
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
#endif

#if defined(__ENABLE_PARALLEL)
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
#endif
