// Sample RFM69 TX power sender at fixed intervals
// First is number of packet sent from this node (you can check if there is any missing
// packet on gateway)
// Second is RSSI this node got back from gateway and is negative value
// Third is power level of this node (0-31)
// This node also uses OLED 128x64 to display TX level and AckRSSI
//***************************************************************************************************************************
// Copyright Felix Rusu 2018, http://www.LowPowerLab.com/contact
//***************************************************************************************************************************
// License
//***************************************************************************************************************************
// This program is free software; you can redistribute it
// and/or modify it under the terms of the GNU General
// Public License as published by the Free Software
// Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the
// implied warranty of MERCHANTABILITY or FITNESS FOR A
// PARTICULAR PURPOSE. See the GNU General Public
// License for more details.
//
// Licence can be viewed at
// http://www.gnu.org/licenses/gpl-3.0.txt
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code
//***************************************************************************************************************************
#include <RFM69.h>         //get it here: https://github.com/lowpowerlab/rfm69
#include <RFM69_ATC.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_OTA.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <SPIFlash.h>      //get it here: https://github.com/lowpowerlab/spiflash
#include <SPI.h>           //included with Arduino IDE (www.arduino.cc)
#include <Wire.h>
#include <stdio.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define OLED_RESET 5
Adafruit_SSD1306 display(OLED_RESET);

//***************************************************************************************************************************
//**** IMPORTANT RADIO SETTINGS - YOU MUST CHANGE/CONFIGURE TO MATCH YOUR HARDWARE TRANSCEIVER CONFIGURATION! 			 ****
//***************************************************************************************************************************
#define NODEID        98    //unique for each node on same network
#define NETWORKID     100   //the same on all nodes that talk to each other
#define GATEWAYID     1
#define FREQUENCY     RF69_868MHZ
#define IS_RFM69HW_HCW  //uncomment only for RFM69HW/HCW! Leave out if you have RFM69W/CW!
#define ENCRYPTKEY    "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
//***************************************************************************************************************************
#define ENABLE_SERIAL   //comment out if you don't want any serial output
#define ENABLE_FLASH    //comment out if you don't want FLASH
#define ENABLE_OTA      //comment out if you don't want OTA
#define ENABLE_ATC      //comment out to disable AUTO TRANSMISSION CONTROL
#define ATC_RSSI        -70
//***************************************************************************************************************************
#define ACK_TIME      30 // max # of ms to wait for an ack


#ifdef __AVR_ATmega1284P__
#define LED           15 // Moteino MEGAs have LEDs on D15
#define FLASH_SS      23 // and FLASH SS on D23
#else
#define LED           9 // Moteinos have LEDs on D9
#define FLASH_SS      8 // and FLASH SS on D8
#endif

#ifdef ENABLE_SERIAL
  #define SERIAL_BAUD   115200
  #define DEBUG(input)   {Serial.print(input);}
  #define DEBUGln(input) {Serial.println(input);}
#else
  #define DEBUG(input);
  #define DEBUGln(input);
#endif

int TRANSMITPERIOD = 400; //transmit a packet to gateway so often (in ms)
char buff1[50];
byte sendSize1 = 0;

#ifdef ENABLE_ATC
RFM69_ATC radio;
#else
RFM69 radio;
#endif

//***************************************************************************************************************************
// flash(SPI_CS, MANUFACTURER_ID)
// SPI_CS          - CS pin attached to SPI flash chip (8 in case of Moteino)
// MANUFACTURER_ID - OPTIONAL, 0x1F44 for adesto(ex atmel) 4mbit flash
//                             0xEF30 for windbond 4mbit flash
//                             0xEF40 for windbond 16/64mbit flash
//***************************************************************************************************************************
#ifdef ENABLE_FLASH
  SPIFlash flash(FLASH_SS, 0x2020); //EF30 for windbond 4mbit flash
#endif

void setup() {
#ifdef ENABLE_SERIAL
  Serial.begin(SERIAL_BAUD);
#endif

  radio.initialize(FREQUENCY, NODEID, NETWORKID);
  radio.encrypt(ENCRYPTKEY);
  
#ifdef IS_RFM69HW_HCW
  radio.setHighPower(); //must include this only for RFM69HW/HCW!
#endif

//Auto Transmission Control - dials down transmit power to save battery (-100 is the noise floor, -90 is still pretty good)
//For indoor nodes that are pretty static and at pretty stable temperatures (like a MotionMote) -90dBm is quite safe
//For more variable nodes that can expect to move or experience larger temp drifts a lower margin like -70 to -80 would
//probably be better
//Always test your ATC mote in the edge cases in your own environment to ensure ATC will perform as you expect
#ifdef ENABLE_ATC
  radio.enableAutoPower(ATC_RSSI);
#endif

#ifdef ENABLE_FLASH
  if (flash.initialize()){
    DEBUGln("SPI Flash Init OK!");}
  else{
    DEBUGln("SPI Flash Init FAIL!");}
#endif
  //radio.writeReg(0x58, 0x2D);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x64)
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  sprintf(buff1, "\nTransmitting at %d Mhz...", FREQUENCY == RF69_433MHZ ? 433 : FREQUENCY == RF69_868MHZ ? 868 : 915);
  DEBUGln(buff1);
}

long lastPeriod = 0;
long Number1 = 0;
int Number2 = 0;
int Number3 = 0;
void loop() {
  if (Serial.available() > 0) {
    char input = Serial.read();
    if (input == 'R')
    {
      DEBUG("Rebooting");
      resetUsingWatchdog(true);
    }
    else if (input == 'r')
    {
      DEBUG("RFM69 registers:");
      radio.readAllRegs();
    }
#ifdef ENABLE_FLASH 
     else if (input == 'd') //d=dump first page
    {
      DEBUGln("Flash content:");
      int counter = 0;

      while (counter <= 256) {
        Serial.print(flash.readByte(counter++), HEX);
        DEBUG('.');
      }
      DEBUGln();
    }
    else if (input == 'e')
    {
      DEBUG("Erasing Flash chip ... ");
      flash.chipErase();
      while (flash.busy());
      DEBUGln("DONE");
    }
    else if (input == 'i')
    {
      DEBUG("DeviceID: ");
      Serial.println(flash.readDeviceId(), HEX);
    }
#endif    
  }
  if (radio.receiveDone())
  {
    DEBUG('['); Serial.print(radio.SENDERID, DEC); DEBUG("] ");
    for (byte i = 0; i < radio.DATALEN; i++)
      DEBUG((char)radio.DATA[i]);
    DEBUG("   [RX_RSSI:"); DEBUG(radio.RSSI); DEBUG("]");

#if defined (ENABLE_OTA) && defined (ENABLE_FLASH)
    CheckForWirelessHEX(radio, flash, true);
#endif

    if (radio.ACKRequested())
    {
      radio.sendACK();
      //DEBUG(" - ACK sent");
    }
    DEBUGln();
  }

  int currPeriod = millis() / TRANSMITPERIOD;
  if (currPeriod != lastPeriod)
  {
    lastPeriod = currPeriod;
    Number1 = Number1 + 1;
    Number2 = radio.getAckRSSI(), DEC;
    Number3 = radio._transmitLevel, DEC;
    sprintf(buff1,"%ld;%i;%i",Number1,Number2,Number3); // combine 3 numbers and add ; between them (semicolon separated)
    sendSize1 = strlen(buff1);
    if (radio.sendWithRetry(GATEWAYID, buff1, sendSize1)) {
      DEBUG("My TX level:"); Serial.print(radio._transmitLevel, DEC);
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println(radio.getAckRSSI(), DEC);   // RSSI level that came back to this node
      display.println(radio._transmitLevel, DEC); // transmit level of this node
      display.display();

      DEBUG(" ok!");
    }
    else DEBUG(" nothing...");

    DEBUGln();
  }
}
