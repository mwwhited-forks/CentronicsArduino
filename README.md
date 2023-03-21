# Centronics (Parallell Interface) Printer Emulator

This is originally based on [CentronicsArduino by boriz](https://github.com/boriz/CentronicsArduino)

It is not uncommon for electronic enthusiast to use an outdated test gears in their labs. Actually it is pretty common even for companies to rely on vintage instruments born in 80s/90s/00s. There is probably a good reason for it, these relics are usually moderately priced and indestructible. They are extremely well built, can withstand hurricanes and small nuclear explosions. These tools are obviously outdated and lacking modern features, but pretty adequate for most cases.
The problem I am typically having (and probably other users of these antique tools) is that I can't easily take a screenshot of the instrument. You can obviously take a picture of an old CRT screen, but it just doesn't look professional. Some instruments have GPIB/HP-IB interfaces and if you are a lucky owner of GPIB adapter you can use [HP 7470A Emulator](http://www.ke5fx.com/gpib/7470.htm) emulate printer over GPIB. But what if you don't have GPIB adapter handy or your instrument don't have a GPIB port?
Fear no more - Centronics printer emulator is to the rescue! It saves whatever it sees on the parallel port to the file on the SD card. When your instrument thinks that it send screensnot to the printer it actually sends it to the SD card. After that you can use your favorite viewer to view/print this file. This emulator can be built in probably less than an hour, using readily available parts. It built around Arduino Mega board with SD card and LCD modules.

## SD card pinout

I did not have SD card breakout board, so I used Ethernet shield, witch happened to have microSD card connector. I don't remember where exactly I got this shield from, but it seems to be pretty standard part. For example this one looks identical to mine: [Ethernet shield](https://www.ebay.com/itm/Ethernet-Shield-Lan-W5100-For-Arduino-Board-UNO-R3-ATMega-328-MEGA-1280-2560/322267901919)
This shield uses the following pins for the SD card communication:

| SD card pin name | Arduino pin |
|------------------|-------------|
| MOSI             | pin 11      |
| MISO             | pin 12      |
| CLK              | pin 13      |
| CS               | pin 4       |
| GND              | GND         |

## LCD pinout

I used the simples LCD module I can find in the random parts pile. It is 2x16 display module. Again, I am not 100% sure, but this one looks identical to mine: [LCD](https://www.ebay.com/itm/HOBBY-COMPONENTS-UK-LCD-1602-16x2-Keypad-Shield-For-Arduino-LA/372201166520) 
Only the following pins are connected to the LCD shield. I left other pins disconnected:

| LCD pin name | Arduino pin number |
|--------------|--------------------|
| RS           | pin 8              |
| EN           | pin 9              |
| D4           | pin 4              |
| D5           | pin 5              |
| D6           | pin 6              |
| D7           | pin 7              |

Here are a few pictures on the shields stackup:

## Parallel interface

Centronics parallel interface is pretty old and it was well documented in nineties, so it was easy to figure out how to connect Arduino to it.

| Name         | DB25  | Arduino | Direction | Notes                                                                     |
|--------------|------=|---------|---------==|---------------------------------------------------------------------------|
| /Strobe      | 1     | pin 18  | Input     | Pullup enabled. Attached to falling edge interrupt.                       |
| D0           | 2     | pin 25  | Input     | Parallel data.                                                            |
| D1           | 3     | pin 27  | Input     | Parallel data.                                                            |
| D2           | 4     | pin 29  | Input     | Parallel data.                                                            |
| D3           | 5     | pin 31  | Input     | Parallel data.                                                            |
| D4           | 6     | pin 33  | Input     | Parallel data.                                                            |
| D5           | 7     | pin 35  | Input     | Parallel data.                                                            |
| D6           | 8     | pin 37  | Input     | Parallel data.                                                            |
| D7           | 9     | pin 39  | Input     | Parallel data.                                                            |
| /Acknowledge | 10    | pin 41  | Output    | Generate falling edge to acknowledge data.                                |
| Busy         | 11    | pin 43  | Output    | Set high on the falling edge of Strobe. Set low after acknowledging data. |
| Paper Out    | 12    | pin 45  | Output    | Not used. Forced low.                                                     |
| Select       | 13    | pin 47  | Output    | Not used. Forced high.                                                    |
| /Auto Feed   | 14    | pin 22  | Input     | Pullup enabled                                                            |
| /Error       | 15    | pin 24  | Output    | Not used. Forced high.                                                    |
| /Initialize  | 16    | pin 26  | Input     | Pullup enabled                                                            |
| /Select In   | 17    | pin 28  | Input     | Pullup enabled                                                            |
| Ground       | 18-25 | pin 28  | Power     |                                                                           |
  
## How to use it

Insert SD card and press reset button on the Arduino. It should dispaly "Ready" message on the LCD.
Configure your instrument to use Centronics (parallel port) for printing. 
Configure printer type. The device saves whatever it sees on the parallel port to the file on the SD card, so we should probably select a printer with standard protocol. 
* HP 54522A oscilloscope. Select "HP 7470A" plotter. The output would be a standard HP-GL format.
* Tektronix TDS2024 scope. Configure "RLE" format. You can open these files with MS Paint
* HP 8594E spectrum analyzer. Select Plotter ("PLT") option. The output would be a standard HP-GL format.
Press "Print" (or "Copy") button, wait for "Done" message on the device LCD.

To view HP_GL files I am using free and open-source [HP-GL Viewer](http://service-hpglview.web.cern.ch/service-hpglview/download_index.html) from CERN. But you should be able to use any other HP-GL viewers, including [HP 7470A Emulator](http://www.ke5fx.com/gpib/7470.htm) I mentioned above.
RLE is a standard bitmap, you should be able to open it with Microsoft Paint or any other graphic editor.

## Timing Diagrams

### Parallel Port

```plantuml
<style>
timingDiagram {
  .output {
    LineColor lightGreen
  }
  .input {
    LineColor lightBlue
    BackgroundColor lightBlue
  }
  .memory {
    LineColor lightCoral
    BackgroundColor lightCoral
  }
}
</style>

binary  "/Strobe"        as Strobe       <<input>>
concise "Data Bits 0..7" as Data         <<input>>
binary  "/Acknowledge"   as Ack          <<output>>
binary  "Busy"           as Busy         <<output>>
binary  "Paper Out"      as PaperOut     <<output>>
binary  "Select"         as Select       <<output>>
binary  "/Auto Feed"     as AutoFeed     <<input>>
binary  "/Error"         as Error        <<output>>
binary  "/Initialize"    as Init         <<input>>
binary  "Select In"      as SelectIn     <<input>>
 
concise "Buffer"         as RingBuffer   <<memory>>
concise "Read Index"     as BufferIndex  <<memory>>

Strobe is high
Data is ""
AutoFeed is low
Init is low

Error is high
Select is high
PaperOut is low
Busy is low
Ack is high

RingBuffer is ""
BufferIndex is 0

@0 as :set_data
Data is 0xAC

@1 as :strobe_pulse
Strobe is low

@2 as :strobe_reset
Strobe is high

@3 as :start_read
Busy is high

@4 as :read_value
RingBuffer is 0xAC
BufferIndex is "+1"

@5 as :read_ack
Ack is low
Busy is low
Data is ""

```

## Action Sequence Diagrams

### Initialize

```plantuml

-> printer: activate
activate printer
printer -> printer: +Error,+Select,-PaperOut,-Busy,+Ack
printer -> terminal: ^Strobe,^Data
note right: Add ^AutoFeed,^Init,^SelectIn
deactivate printer

```

### On Print

```plantuml

loop while data

-> terminal: print
activate terminal
terminal -> terminal: Set Data
terminal -\ printer : -Strobe
terminal -\ printer : +Strobe
activate printer
printer -\ terminal : +Busy
alt if new
  note right: What signals a new job?
  printer -\ buffer : new
  activate buffer
end
printer -> terminal : Get Data
terminal --> printer : <Data>
printer -\ buffer   : Set Value
buffer -\ buffer    : Input Index++
printer -\ terminal : -Ack
printer -\ terminal : -Busy
printer -\ terminal : +Ack
alt if complete
note right: What signals job complete?
  printer -\ buffer : complete
end

end
deactivate terminal
deactivate printer

```

### On New File

```plantuml
--> disk
activate disk
disk -> file: check exists
  loop do while exists
    activate file
    file -> file: increment name
  end
  file --> disk: <new name>
  disk -> file: create <new name>
  deactivate file
  alt success 
    disk --> : <file handle>
  else fail
    disk --> : <error>
  end
```

### Buffer to File

```plantuml
--> buffer : full
activate buffer
alt if new
  disk -\ file : new file
  activate file
end
activate disk
buffer -> file 
loop while full
  file -> file: write bytes
  file -> buffer: Output Index++
end

alt if complete
  note right: What signals a new job?
  disk -> file: close
  deactivate file
end 
deactivate buffer
```



## Notes

* [Arduino SD Library](https://www.arduino.cc/reference/en/libraries/sd/)
* [SD Card with Logic Level hookup](https://learn.sparkfun.com/tutorials/microsd-shield-and-sd-breakout-hookup-guide#sd-card-breakout-boards)
* [Deek Robot - Data Logger v1](https://forum.arduino.cc/t/deek-robot-data-logging-shield-v1-0/621963)
  * https://www.hackster.io/cowboydaniel/deek-robot-datalogging-shield-tutorial-for-mega-1cd2b1
  * https://drive.google.com/file/d/1T5GEPMKtLjfiJvdL_DHEbsTk9sAIzThP/view
* [OSEPP - 16×2 LCD Display & Keypad Shield](https://www.osepp.com/electronic-modules/shields/45-16-2-lcd-display-keypad-shield)

* Remove D10 from LCD shield, could cause long term damage to ATMega2560

## Additional Notes about Shields

### LCD

The OSEPP LCD should have the trace cut for pin 10 that is used to control the background display.  This may cause damage.

If you wish to still control the background you might want to remap the SD Chip Select as well.

### DataLogger

The Deek Robot or Adafruit v1 SD Card Data Logger should have pin 11,12,13 cut and remapped to the SPI/ISCP header.  This will allow hardware control of the SPI bus.

### DB25 Adapter

The Strobe line must be wired to a pin that support hardware interrupt.

## Suggsted Improvements

* Enable RTC (DS1307) on the DataLogger shield.  When in Idle present the time on the screen
* Add support for storing files on a W25Q128 (CS pin 3)
  * When SD Card is removed or used selected the files may be stored on the eeprom
  * look at using the NASA EEFS.
  * https://forum.arduino.cc/t/nasa-eefs/329543/3
  * https://github.com/slviajero/EepromFS
  * https://github.com/feilipu/avr_eefs
  * https://www.instructables.com/Introduction-to-LittleFs-Write-LittleFs-Read-Littl/
* The current read/write process is rather slow
  * look at upgrading to SDFat as well as using a ringbuffer for reading from the parallel poin
* consider adding the ability to directly output the content to the serial connection
* use the inputs on the LCD shield to support ejecting/refreshing the SD card
* consider adding prefix profiles that may be added/modified from the serial connection and selected from the interface
* consider the ability to time stamp files
* add the ability to browse/download/delete content from the serial connection
* add proper support for the control lines in relation to SPP IEEE-1284
* add folder management/selection from USB
  * if tied to quick profiles allow selectig from the LCD panel
* look at using the first few bytes to detect the proper file type so they can ne named correctly
  * otherwise allow changing the file type from the LCD panel
* add proper erroring handling
