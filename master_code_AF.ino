// Bad Idea Barta Board - Control Software - April Fools!
// By Kevin Williams - TexElec.com
// 3/25/2021 - V0.5
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
// The card has two main modes.  The "out of the bus" or "in the bus" mode.  Or as I call the Taunt and Secret mode.
// The card will power on, and by default play one of the "taunt" messages at random.  When uploading wav data to the
// on-board SPI memory, you may mark it for playback in one, or both modes.  If you hold the button in for one second 
// while "out of the bus" it will toggle between random & sequential playback of the taunt messages.  
//
// While in the bus, the card will really only know there is bus activity.  I originally tried to read specific IO calls
// to make specific messages, but the 16MHz on the Arduino won't cut it.  Instead, it basically just detects any bus activity
// and plays messages at random.  Now, it only plays the "secret" messages at random.  The button is still working at this 
// point, but just because I didn't bother to turn it off.  I just don't really expect it to be pushed, and if so, it 
// just works as normal.  Oh yes, I intentionally do not turn the LED on or off during normal mode, to make it a little more
// mysterious.  It flashes only in secret mode.
//
// The other "mode" is the serial input.  The USB port is used as the primary power and also as a serial communications port.
// This card has some canned audio on-board, but it has a lot of free space remaining.  The on-board SPI Flash Memory chip 
// will hold 16MB.  The code, as written, will playback 16KHz 8-Bit Mono unsigned WAV files. This equates to over 17 mins 
// of playback!  There is way less audio shipped on the card, so you can add your own messages, and have them playback as part
// of the card.  MAXTOC controls the number of allowed messages.  Increase if needed, but watch RAM!
//
// To upload, I use audacity and the following steps from any audio source:
// 1. Open the file, if it's stereo, select Tracks->Mix->Mix Stereo down to mono.
// 2. At the bottom, set the project rate to 16000.
// 3. Select Tracks->Resample.  It should default to 16000 now, but if not, set to 16000 and ok.
// 4. Select File->Export->Export as WAV
// 5. On the Encoding drop-down, select Unsigned 8-bit PCM
// 6. You are now ready to send this file to the on-board memory!
//
// In addition to uploading a file, you may also view the table of contents, and playback any entry contained on the 
// chip, including the canned messages.  
//
// The last mode the serial menu presents is serial streaming.  This will allow you to playback audio via the serial port
// at 48Khz!  I had originally tried to get the flash memory to playback @ 48Khz, but the read routine was too slow in 
// single byte mode, and there is not enough RAM to cache enough data to read larger data chunks, so 16KHz.  However, the 
// serial port requires no processing from the card, and it could easily playback even higher-rates, but I capped it @ 48Khz.
// To playback files, just encode them as described above, but instead, resample the file to 48KHz for playback.
//
// Why is uploading not part of this program?  I tried to add it here originally, but I ran into an issue with the 
// SPIMemory library.  Basically, if you call erase or write, it will slowdown each successive read.  The downside
// is that you cannot sustain 16KHz.  Even if you never call the functions, you will have the issue.  I "fixed" it, by
// seperating the code into two, now actually three pieces.  The last program is just to upload the entire original ROM
// image which shipped on the card: spirom.bin.
//
// A lot of this can be fixed with better code, just ran out of time! It is what it is, enjoy!

#include <Arduino.h>
#include <SPIMemory.h>  //Comment out Adesto from manufacturer ID in library to force SFDP or flash will be detected as 512k, not 16MB
//FLASH MEMORY RANGE - 0x000000-0xFFFFFF 16MB
//4K - Sector 0 = Allocation table - ADDR: 0x000000-0x000FFF
//You must write a whole sector at a time, write @ 4k boundaries.
//SPI Flash Playback   - 16KHz mono 8-bit unsigned PCM playback only
//Serial Data Playback - 48KHz mono 8-bit unsigned PCM Playback only

struct FATLAYOUT{     //entry structure - 16 bytes * MAXTOC 
 byte name[9];        //Always 9 
 uint8_t start00;     //start sector top         0x0000 - 0x1000
 uint8_t start01;     //start sector low
 uint8_t length00;    //start addr top byte
 uint8_t length01;    //start addr middle byte   Len in Bytes 24 bit val
 uint8_t length02;    //start addr low byte
 uint8_t normal;      //normal play 0=off 1=on
 uint8_t secret;      //secret play 0=off 1=on 
};

//Sector 0 memory also contains
//4093 & 4094 = NextFreeSec, unneeded for this code
//4095        = TOCCT IE, record count

const int MAXTOC=60;                       //eats RAM, and controls the max number of allowed entries
const int seedPin=A2;                      //for random function
const int TOPCAN=27;                       //This is the high-water mark for canned playback entries, all other user

typedef struct FATLAYOUT DACFAT;           //keep in memory 1k, write to first sector
DACFAT TOC[MAXTOC];                        //Whole list in RAM, write to sector 0 if changed

const int SPI_CS=7;                        // Arduino Pin  7 - SPImemory CS line.  Pins 8,9 & 10 are SPI MISO/MOSI/SCK 
const int    LED=3;                        // Arduino Pin  3 - Output LED PWM Able PortD0
const int    BUT=11;                       // Arduino Pin 11 - Input Button
const int  AMPSD=2;                        // Arduino Pin  2 - Amplifier enable. High=On Low=Off
const int  ALE=A5;                         // Arduino Pin 19 - ALE ISA BUS  I basically connected what I had pins for
const int  IOR=A4;                         // Arduino Pin 18 - IOR ISA BUS  Address & data change too fast for 16MHz!
const int  IOW=A3;                         // Arduino Pin 17 - IOW ISA BUS
const int MEMR=A2;                         // Arduino Pin 16 - MEMR ISA BUS
const int MEMW=A1;                         // Arduino Pin 15 - MEMW ISA BUS
const int  AEN=A0;                         // Arduino Pin 14 - AEN ISA BUS

// I have several ISA pins hooked up, but basically only checking to see if IOW is alive.  Please note, you 
// could totally honk up your bus if you start doing things you're not supposed to with these pins.  Most are read-only
// so be sure to cosult the ISA specs if you try to use any others.  Pretty pointless though, 16MHz is too slow. :-(
// Probably gonna be switching to a Picos after this project... It's time, it's been fun Atmel, I mean Microchip.

#include <OneButton.h>                     //debounce routine
OneButton BUTTON(BUT, true, true);         //enable button

volatile byte dacdat=0;                    //data for DAC playback

int  MODE=0;                               //Playback Mode - 0=Normal 1=Secret
int  armct=0;
int  secretbegin=0;                        //Used to initiate secret mode
long sectimbegin=0;
long sectimelap=0;
int  ROS=0;                                //Random or Seq Mode, Random=0, Seq=1
int  seqct=0;                              //keeps track of seq mode
int  normalplay[MAXTOC];                   //normal playlist array
int  normalindex=-1;                       //normal index counter
int  secretplay[MAXTOC];                   //secret playlist
int  secretindex=-1;                       //secret index counter
int  showmenu=1;                           //supress menu, if already shown since last command
int  TOCCT=-1;                             //Table of contents index counter
int  firsttime=1;                          //show firsttime message on serial connection
int  action=0;                             //menu action  
long randomnum=0,randomlast=0;             //for random number stuff
long randomnum1=0,randomlast1=0;           //for random number stuff
long randomnum2=0;
int  serialstreaming=0;                    //In serial stream mode?
uint32_t freesec=4096;                     //total free sectors
uint32_t nextfreesec=1;                    //next free sector, 1=default

SPIFlash flash(SPI_CS);                    //SPI Memory instance

void setup() {
 //AT PIN  DAC  PHY  ARDUINO
 //PD4 - D0 - 25 -  4
 //PD6 - D1 - 26 - 12
 //PD7 - D2 - 27 -  6
 //PB4 - D3 - 28 -  8
 //PB5 - D4 - 29 -  9
 //PB6 - D5 - 30 - 10
 //PC6 - D6 - 31 -  5
 //PC7 - D7 - 32 - 13

 DDRC |= B11000000;  //Set DAC output pins
 DDRB |= B01110000;
 DDRD |= B11010000;

 DDRF  = B00000000;  //Port F all Inputs - Bus pins
  
 BUTTON.attachClick(but_short);                 //Play msg
 BUTTON.attachDuringLongPress(but_long);        //change mode

 pinMode(LED,OUTPUT); 
 digitalWrite(LED,LOW);
 pinMode(IOW,INPUT);                            //only pin used for secret mode
 digitalWrite(IOW, HIGH);                       //set pullup high
 pinMode(AMPSD,OUTPUT);                         //Amplifier enable pin
 digitalWrite(AMPSD, LOW);                      //low=disable high=active
 
 Serial.begin(115200);                          //pretty sure the speed doesn't do much on USB Arduinos
 flash.begin(MB(16));                           //open flash lib
 readtoc();                                     //build table of contents in RAM from SPI memory
 buildplay();                                   //populate normal & secret arrays from TOC in memory
}

void loop() { 
 delay(100);                                    //onebutton needs some delay
 if (Serial) {
  if (firsttime) {                              //display 1st time msg
   firstmsg();                                  
   firsttime=0;
  }              
  sermenu();                                    //enter serial menu if available
 }
 if (digitalRead(IOW)==0) {MODE=1;}             //check for ISA bus activity, go to secret mode
 if (MODE) {secretmode();}                      //enter ISA mode, and never exit until power-off                                    
 BUTTON.tick();                                 //check button press
}

void secretmode(){
 if (!secretbegin) {
  secretbegin=1;                              
  sectimbegin=millis();                    
  for (int i=0;i<8;i++) {                       //flash led on startup - was for debugging, but I left it
   delay(60);
   fastled(1);  
   delay(60);
   fastled(0);  
  }
  delay(5000);
  playsong(13,1);                               //play trumpet
  delay(2000);
 }
 else {
  sectimelap=millis()-sectimbegin;              //check to see how much time has passed since start
  randomSeed(generateRandomSeed());
  randomnum2=random(7,15);                      //play random msg between 7-15 secs
  if (sectimelap>(1000*int(randomnum2))) {
   while (randomlast1==randomnum1) {
    randomSeed(generateRandomSeed());
    randomnum1=random(secretindex);
  }
   randomlast1=randomnum1;
   playsong(secretplay[randomnum1],0);
   sectimbegin=millis();
   armct++;
  }
  if (armct==14) {delay(5000);armageddon();armct=0;}    //after 15 random messages, does the "armageddon" routine
 }
}

void armageddon() {
 playsong(14,1);              
 playsong(2,1);
 sectimbegin=millis();
}

void but_short(){
 if (!serialstreaming) {
  if (ROS) {seqplay();} else {randomplay();}
 }                                              //checks to see which mode to run, then does it
 else {
  serialstreaming=0;                            //breaks out of stream mode
  Serial.println(F("Exiting streaming mode."));
 }
}

void but_long(){
 seqct=0;                                           //reset seq playlist to 0
 if (ROS) {
  playsong(0,0);                                    //set Random mode
  ROS=0;
 } else if (!ROS) {
  playsong(1,0);                                    //set seq mode
  ROS=1;
 }
}

void sermenu() {
 if (showmenu){
 showmenu=0;
 Serial.println(F("Options:")); 
 Serial.println();
 Serial.println(F("1. Show Table of Contents."));
 Serial.println(F("2. Play Entry."));
 Serial.println(F("3. Stream Serial -> 48KHz Mono WAV."));
 Serial.println();
 Serial.print(F("> "));
}
if (Serial.available() > 0) {
 showmenu=1;
 action=Serial.read();
 Serial.write(action);
 Serial.println();
 Serial.println();
 switch (action) {
  case 0x31: showtoc();
             break;
  case 0x32: songmenu();
             break;
  case 0x33: serialstream();
             break;
  default:   break;   
  }
 }
}

void seqplay() {
 playsong(normalplay[seqct],0);
 seqct++;
 if (seqct>normalindex) seqct=0;
}

void randomplay(){
 randomSeed(generateRandomSeed());               
 if (normalindex) {
  while (int(randomnum)==int(randomlast)) {           //don't play the same random twice in a row
   randomnum=random(normalindex); 
  }
 } else randomnum=0;
 randomlast=randomnum;
 if (normalindex>-1) {playsong(normalplay[int(randomnum)],0);}
}

void buildplay(){                            
 uint8_t compare;
 normalindex=-1; 
 secretindex=-1;
 for (int i=0;i<TOCCT;i++){
  compare=TOC[i].normal;                                     //this routine builds the 'normal' & 'secret' playlists.
  if (compare) {normalindex++;normalplay[normalindex]=i;}    //These are used by each mode to know which songs are valid.      
  compare=TOC[i].secret;
  if (compare) {secretindex++;secretplay[secretindex]=i;}
 }
}

void readtoc() {
 TOCCT=int(flash.readByte(4095));                            //reads the TOC into memory from SPIROM
 if (TOCCT>=MAXTOC) {TOCCT=-1;}
 if (TOCCT>-1) {
  nextfreesec= flash.readByte(4093)*256;
  nextfreesec+=flash.readByte(4094);
  for (int i=0;i<TOCCT;i++) {
   for (int j=0;j<9;j++) {
    TOC[i].name[j]=flash.readByte((i*16)+j);
   }
   TOC[i].start00=flash.readByte((i*16)+ 9);
   TOC[i].start01=flash.readByte((i*16)+10);
   TOC[i].length00=flash.readByte((i*16)+11);
   TOC[i].length01=flash.readByte((i*16)+12);
   TOC[i].length02=flash.readByte((i*16)+13);
   TOC[i].normal=flash.readByte((i*16)+14);
   TOC[i].secret=flash.readByte((i*16)+15);
  }
 } 
}

void serialstream() {
 uint32_t counter=0; 
 Serial.println();
 Serial.println(F("Audio Streaming via Serial:"));
 Serial.println();
 Serial.println(F("Transmit binary data stream with an application like Tera Term."));
 Serial.println(F("(Send file, check binary.)"));
 Serial.println(F("Convert audio to 48Khz 8-bit WAV Mono, and send away!"));
 Serial.println();
 Serial.println(F("Stop Stream then press \"Pushy Thingy\" to exit..."));
 serialstreaming=1;
 digitalWrite(AMPSD, HIGH);           //turn on the amp
 while (serialstreaming) {
 while (serialstreaming && (Serial.available()<1)) {counter++;if (counter>10000) {BUTTON.tick();counter=0;}}  //check for exit
  dacdat=Serial.read();
  playdat();
  counter++;
  if (counter>10000) {BUTTON.tick();counter=0;} 
  delayMicroseconds(9);
  }
 eatserial();                  
 Serial.println();
 digitalWrite(AMPSD, LOW);           //shut amp off on exit
}

void songmenu() {
 byte t1;
 byte value[2];
 showtoc();
 int sel;
 if (TOCCT>-1){
 Serial.print(F("Enter Song number to play (Use leading zero IE. 01, etc): "));
 for (int i=0;i<2;i++) {
  while (Serial.available()<1) {}
  t1=Serial.read() - 48;
  Serial.print(t1);
  value[i]=t1;
 }
 sel=(int)value[0]*10;
 sel+=(int)value[1];
 Serial.println();
 Serial.println();
 playsong(sel,0);
 }
}

void playsong(int sel, int ledstate) {
 digitalWrite(AMPSD, HIGH);                     //turn amp on
 uint32_t saddr,leng,ctr=0;
 int itson=0;
 saddr=TOC[sel].start00;
 saddr=(saddr<<8) + TOC[sel].start01;
 leng=TOC[sel].length00;
 leng=(leng<<8) + TOC[sel].length01;
 leng=(leng<<8) + TOC[sel].length02; 
 for (uint32_t i=0;i<leng;i++) {
  dacdat=flash.readByte((saddr*4096)+i);
  if (ledstate) ctr++;
  if (ctr>900) if (itson) {fastled(0);ctr=0;itson=0;} else {fastled(1);ctr=0;itson=1;}  //toggle led during playback
  playdat();
  delayMicroseconds(1);                        //seems roughly correct for 16KHz
  if (i==(leng-300)) digitalWrite(AMPSD, LOW); //DAC POP FIX, turn it off a hair early, no pop.            
 }                                             //16KHz = 62.5microsec period. readByte takes 52-55microsecs in testing
 if (ledstate) fastled(0);                     //but the rest of the code seems to eat the rest minus 1.  Pretty much the fastest
}                                              //you could pull this off without more RAM, DMA or 3-4x more CPU power.
                                          

void showtoc() {
 Serial.println(F("Entry Name       Start Sec  Length    Normal Secret"));
 Serial.println(F("---------------------------------------------------"));
 for (int i=0;i<TOCCT;i++) {
  Serial.print(" ");
  if (i<10) {Serial.print("0");}
  Serial.print(i,DEC);
  Serial.print("   ");
  for (int j=0;j<9;j++) {
   Serial.write(TOC[i].name[j]);  
  }
  Serial.print("  0x");
  if (TOC[i].start00 < 0x10) {Serial.print("0");};
  Serial.print(TOC[i].start00,HEX);
  if (TOC[i].start01 < 0x10) {Serial.print("0");};
  Serial.print(TOC[i].start01,HEX);
  Serial.print("     0x");
  if (TOC[i].length00 < 0x10) {Serial.print("0");};
  Serial.print(TOC[i].length00,HEX);
  if (TOC[i].length01 < 0x10) {Serial.print("0");};
  Serial.print(TOC[i].length01,HEX);
  if (TOC[i].length02 < 0x10) {Serial.print("0");};
  Serial.print(TOC[i].length02,HEX);
  Serial.print("    ");
  if  (TOC[i].normal) {Serial.print(F("YES"));}
  if (!TOC[i].normal) {Serial.print(F(" NO"));}
  Serial.print("   ");
  if  (TOC[i].secret) {Serial.println(F("YES"));}
  if (!TOC[i].secret) {Serial.println(F(" NO"));}
 }
 Serial.println();
 if (TOCCT<0) {
  Serial.println(F("No entries found."));
  Serial.println();
 }
 freespace();
}

void freespace() {
 Serial.print(F("Free Sectors: "));
 Serial.println(freesec-nextfreesec,DEC);
 Serial.print(F("Free Bytes: "));
 Serial.println((freesec-nextfreesec)*4096,DEC);
 Serial.println();
}

void eatserial() {
 while (Serial.available()) {Serial.read();}
}

void firstmsg() {
 Serial.println();
 Serial.println(F("Welcome to the Bad Idea Barta Board - By TexElec.com"));
 Serial.println();
 Serial.println(F("Special Thanks to: Clint Basinger aka LGR,"));
 Serial.println(F("                   Adrian Black of Adrian's Digital Basement,"));
 Serial.println(F("                   David Murray aka The 8-Bit Guy,"));
 Serial.println(F("               and Christian Simpson aka Perifractic"));
 Serial.println(); 
}

void fastled(bool state) {
  if (state) PORTD |=(1<<PD0) ; else PORTD &=~(1<<PD0);   //toggle LED asap
}

void playdat() {                                          //in ASM to make it as fast as possible for DAC
 asm  ("lds r26, dacdat \n"  
                 
       "rol r26 \n"          //read top bit into carry, cmp rinse & repeat
       "brcs SD0 \n"
       "cbi 0x08, 0x7 \n"    //PORTC7 - set zero
       "jmp O0 \n"
       "SD0: \n"
       "sbi 0x08, 0x7 \n"    //PORTC7 - set one
       "O0: \n"

       "rol r26 \n"          
       "brcs SD1 \n"
       "cbi 0x08, 0x6 \n"    //PORTC6 - set zero
       "jmp O1 \n"
       "SD1: \n"
       "sbi 0x08, 0x6 \n"    //PORTC6 - set one
       "O1: \n"

       "rol r26 \n"         
       "brcs SD2 \n"
       "cbi 0x05, 0x6 \n"    //PORTB6 - set zero
       "jmp O2 \n"
       "SD2: \n"
       "sbi 0x05, 0x6 \n"    //PORTB6 - set one
       "O2: \n"

       "rol r26 \n"          
       "brcs SD3 \n"
       "cbi 0x05, 0x5 \n"    //PORTB5 - set zero
       "jmp O3 \n"
       "SD3: \n"
       "sbi 0x05, 0x5 \n"    //PORTB5 - set one
       "O3: \n"

       "rol r26 \n"          
       "brcs SD4 \n"
       "cbi 0x05, 0x4 \n"    //PORTB4 - set zero
       "jmp O4 \n"
       "SD4: \n"
       "sbi 0x05, 0x4 \n"    //PORTB4 - set one
       "O4: \n"

       "rol r26 \n"          
       "brcs SD5 \n"
       "cbi 0x0B, 0x7 \n"    //PORTD7 - set zero
       "jmp O5 \n"
       "SD5: \n"
       "sbi 0x0B, 0x7 \n"    //PORTD7 - set one
       "O5: \n"

       "rol r26 \n"         
       "brcs SD6 \n"
       "cbi 0x0B, 0x6 \n"    //PORTD6 - set zero
       "jmp O6 \n"
       "SD6: \n"
       "sbi 0x0B, 0x6 \n"    //PORTD6 - set one
       "O6: \n"

       "rol r26 \n"         
       "brcs SD7 \n"
       "cbi 0x0B, 0x4 \n"    //PORTD4 - set zero
       "jmp O7 \n"
       "SD7: \n"
       "sbi 0x0B, 0x4 \n"    //PORTD4 - set one
       "O7: \n" 

       ::: "r26");
}

//Not my code, not even 100% sure if it's helping, but, the placebo effect, you know? 
//From the following site, thanks!:
//https://rheingoldheavy.com/better-arduino-random-values/
uint32_t generateRandomSeed()
{
  uint8_t  seedBitValue  = 0;
  uint8_t  seedByteValue = 0;
  uint32_t seedWordValue = 0;
  
  for (uint8_t wordShift = 0; wordShift < 4; wordShift++)     // 4 bytes in a 32 bit word
  {
    for (uint8_t byteShift = 0; byteShift < 8; byteShift++)   // 8 bits in a byte
    {
      for (uint8_t bitSum = 0; bitSum <= 8; bitSum++)         // 8 samples of analog pin
      {
        seedBitValue = seedBitValue + (analogRead(seedPin) & 0x01);                // Flip the coin eight times, adding the results together
      }
      delay(1);                                                                    // Delay a single millisecond to allow the pin to fluctuate
      seedByteValue = seedByteValue | ((seedBitValue & 0x01) << byteShift);        // Build a stack of eight flipped coins
      seedBitValue = 0;                                                            // Clear out the previous coin value
    }
    seedWordValue = seedWordValue | (uint32_t)seedByteValue << (8 * wordShift);    // Build a stack of four sets of 8 coins (shifting right creates a larger number so cast to 32bit)
    seedByteValue = 0;                                                             // Clear out the previous stack value
  }
  return (seedWordValue);
}

//Song 56 - Michael Oneil, Mark Poot and me on drums.  I used to hang out at Mike's house on Sunday nights, drink
//beer and play in his shed until the neighbors had enough.  I put this song on here mostly because it sounds ok, and
//is one of the few non-copyrighted songs we played.  Well, it belongs to Mike or Mark, but they wouldn't mind me
//putting it here.  The one and only take, and I had never heard it before. I just made the drums up on the fly, so 
//it kinda sucks, but hey, I had like 12mb free of memory, I had to put something else on there! :-)  Still 7MB free!
