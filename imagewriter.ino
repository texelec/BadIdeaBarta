// Bad Idea Barta Board - Control Software - April Fools!
// By Kevin Williams - TexElec.com
// 3/30/2021 - V0.5
//
// Thanks to:David Murray, aka the 8-Bit Guy 
//           Clint Basigner, aka LGR
//           Christian Simpson, aka Perifractic
//         & Adrian Black of Adrian's Digital Basement
//
// They were kind enough to contribte voice lines for this project, and it really made it a lot cooler than it would
// have been.  Thanks guys!              
//
// Note:
// Very crude interface which will let you do what you need, and not much more.  Not much in the
// way of error checking, so precise input is needed for the serial menu.
//
// Operation:
// This program is mostly meant to allow you to upload the original rom contents to the card.  Included in the github 
// repository is a file called: spirom.bin.  If you accidentally erased the images with the uploader too, you can use this
// utility to recover. 
//
// I used the uploader tool to build the original image, then dumped that IC with a utility called flashrom on an RPi4.
// I couldn't find a simple way to send data back to the PC with the serial port.  I was messing around with an XMODEM 
// library, but I ran short on time and just dumped the ROM manually.  I'm sure there's a better way to do this, but 
// short of writing a PC-app to handshake with it, I wasn't totally sure what to do.  So long story short, you can
// send data to the card, but not get it back with this code.

#include <SPIMemory.h>  
int choice=0;
bool showmenu=1;
uint8_t array0[1024];  // Not any faster than 128, but we have plenty of space for it.
SPIFlash flash(7);     // Arduino Pin  7 - SPImemory CS line.  Pins 8,9 & 10 are SPI MISO/MOSI/SCK 

const uint32_t SECTORS=9000;  //Total chip size would be 16384 * 1024.  My data needs 8852.
                              //Going over a bit to be safe, but change this if you plan to 
                              //write your own image file. Takes about 3 mins.

void setup() {
 Serial.begin(115200);
 flash.begin(MB(16)); //open flash lib
 while (!Serial) {}   //Wait for serial connection
 Serial.println(F("Bad Idea Barta Board - TexElec.com 2021"));
 Serial.println(F("SPI Flash ROM Image Writer"));
 Serial.println();
}

void loop() {
  while(Serial.available()) {Serial.read();}
  delay(100);
  sermenu();
}

void sermenu() {
 if (showmenu) {
  showmenu=0;
  Serial.println(F("1. Program Chip. (Send: spirom.bin)"));
  Serial.println(F("2. Erase Chip."));
  Serial.println();
  Serial.print(F("> "));
 }
 if (Serial.available()>0) {
  showmenu=1;
  choice=Serial.read();
  Serial.write(choice);
  Serial.println();
  switch (choice) {
    case 0x31:uploadall();
              break;
    case 0x32:erase();
              break;
    default:  break;
  }
 }
}

void uploadall() {
 erase();
 while(Serial.available()) {Serial.read();}
 delay(100);
 Serial.println(F("Start File Upload Now..."));
 while(Serial.available()<1) {}
 
 for (uint32_t i=0;i<SECTORS;i++) {   //total chip size would be 16384 * 1024.  My data needs 8852.
  for (uint32_t j=0;j<1024;j++) {  
   while(Serial.available()<1) {}  
   array0[j]=Serial.read();
  }
  if (i<9) Serial.print(F("0"));
  if (i<99) Serial.print(F("0"));
  if (i<999) Serial.print(F("0"));
  if (i<9999) Serial.print(F("0"));
  Serial.print(i,DEC);
  Serial.print(F("/"));
  Serial.print(SECTORS,DEC);
  for (int k=0;k<11;k++) Serial.write(8);
  flash.writeByteArray(i*1024, array0, 1024, true); 
 }
  while(Serial.available()) {Serial.read();}
 Serial.println();
}

void erase() {
  Serial.println(F("Erasing Chip."));
  Serial.println();
  if (flash.eraseChip()) {
  Serial.println(F("Chip erased."));
  Serial.println();
  }
  else {
    Serial.println(F("Erase failed!"));
  }
}
