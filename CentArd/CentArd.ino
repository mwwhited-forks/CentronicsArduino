// Arduino Centronics printer capture tool 
// Using Ethernet shield with built-in SD card
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
// 1	  /STROBE	-->	Strobe	      18
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
// 14	  /AUTOFD	-->	Autofeed	    22
// 15  	/ERROR	<--	Error	        24
// 16	  /INIT	  -->	Initialize    26
// 17  	/SELIN	-->	Select In	    28
// 18	  GND	    ---	Signal Ground	GND
// 19	  GND	    ---	Signal Ground	GND
// 20	  GND	    ---	Signal Ground	GND
// 21  	GND	    ---	Signal Ground	GND
// 22  	GND	    ---	Signal Ground	GND
// 23	  GND	    ---	Signal Ground	GND
// 24	  GND	    ---	Signal Ground	GND
// 25	  GND     ---	Signal Ground	GND

#include <LiquidCrystal.h>
#include <SPI.h>
#include <SD.h>

// 10s timeout before considering the print completed
#define TIMEOUT_MS 10000
//const long serialPortSpeed = 115200;
const long serialPortSpeed = 2000000;

// Global variables/flags
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);  // RS, EN, D4, D5, D6, D7
bool init_complete = false;
bool print_in_progress = false;
bool data_ready = false;
byte data = 0;
byte buff[512];
int buff_index = 0;
long last_update;
long waiting_update;
File current_file;
long file_size = 0;

const int sdcard_cs = 10;   

const int parallelPortStrobePin = 18;                             // on receive
const int parallelPortDataPins[8] = { 25,27,29,31,33,35,37,39 };  // data in
const int parallelPortAckPin = 41;                                // toggle confirm
const int parallelPortBusyPin = 43;                               // toggle status
const int parallelPortPaperOutPin = 45;                           // fixed low
const int parallelPortSelectPin = 47;                             // fixed high
const int parallelPortAutoFeedPin = 22;                           // input
const int parallelPortErrorPin = 24;                              // fixed high
const int parallelPortInitializePin = 26;                         // input
const int parallelPortSelectInPin = 28;                           // input

void setup() 
{
  Serial.begin(serialPortSpeed);
  delay(10);
  Serial.println();
  Serial.println();
  Serial.println();
  
  lcd.begin(16, 2);  
  delay(10);
  lcd.clear();  
  
  // Initialize SD card
  Serial.println("Init SD");
  pinMode(sdcard_cs, OUTPUT);
  if ( !SD.begin(sdcard_cs) ) 
  {
    Serial.println("SD Init Failed");
    lcd.print("! No SD card !");
  }
  else
  {
    Serial.println("SD Init Ok");
    lcd.print("--== Ready ==--");
  }
  
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

  pinMode(parallelPortAutoFeedPin,   INPUT_PULLUP);
  pinMode(parallelPortInitializePin, INPUT_PULLUP);
  pinMode(parallelPortSelectInPin,   INPUT_PULLUP);
  
  // pinMode(parallelPortAutoFeedPin,   INPUT);
  // pinMode(parallelPortInitializePin, INPUT);
  // pinMode(parallelPortSelectInPin,   INPUT);
    
  // Update timeout
  waiting_update = last_update = millis();
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
    digitalWrite(parallelPortAckPin, true);  // ACK

    // Reset timeout
    last_update = millis();

    // Actively printing?
    if (!print_in_progress)
    {
      // Just started printing. Create new file
      CreateNewFile();
      Serial.print(F("Receiving from printer. "));
      Serial.print(F(" "));
      file_size = 0;

      // Update LCD
      lcd.clear();
      lcd.print(F("Prn to:"));
      lcd.print(current_file.name());      
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

  if (print_in_progress && waiting_update + 200 < millis())
  {
    waiting_update = millis();
    Serial.print("[");
    Serial.print(getControlPins(), HEX);
    Serial.print("]");    
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

    // Update LCD
    lcd.clear();
    lcd.print("Done: ");
    lcd.print(current_file.name());
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


// Strobe pin on falling edge interrupt
void StrobeFallingEdge()
{
  // Be sure that init sequence is completed
    if (!init_complete)
    {
      return;
    }

    if (!digitalRead(parallelPortStrobePin))
    {
      return; //Note: glitch
    }
    
  // Set busy signal
  digitalWrite(parallelPortBusyPin, true);
  delay(5);

  // Read data from port
  data = 
    (digitalRead(parallelPortDataPins[0]) << 0) | 
    (digitalRead(parallelPortDataPins[1]) << 1) |
    (digitalRead(parallelPortDataPins[2]) << 2) | 
    (digitalRead(parallelPortDataPins[3]) << 3) | 
    (digitalRead(parallelPortDataPins[4]) << 4) | 
    (digitalRead(parallelPortDataPins[5]) << 5) | 
    (digitalRead(parallelPortDataPins[6]) << 6) |
    (digitalRead(parallelPortDataPins[7]) << 7) ;

  // Set ready bit
  data_ready = true;    
}

byte getControlPins(){
  byte val =   
    (digitalRead(parallelPortStrobePin) << 0)    | 
    (digitalRead(parallelPortAutoFeedPin) << 1)  |
    (digitalRead(parallelPortInitializePin) << 2)| 
    (digitalRead(parallelPortSelectInPin) << 3);
  return val;
}
