// Bad Idea Barta Board - Control Software - April Fools!
// By Kevin Williams - TexElec.com
// 3/27/2021 - V0.5
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
// This program is mostly meant to allow you to upload messages to the SPIROM.  The SPIMemory routine has an issue where 
// even having eraseSector or WriteByte in your code will cause subsequent reads of SPIMemory to go too slow for 16KHz
// playback.  I seperated them into two apps, so one could be installed for uploading, and the other for the main functionality.
//
// You cannnot delete previous entries, so be careful.  It would be pretty simple to delete the last entry, but any others
// would cause fragmentation.  The chip must be deleted at at least the sector level before written to.  There are 4096-4K
// sectors.  Sector 0 is for the table of contents, and the rest for data.  The code only starts writes on the next new 
// sector, so up to 4K-1 byte per entry could be lost in size on uploads.
//
// You can delete the whole chip and start over however.  If you lose the original image, and want to get it back, use 
// the imagewrite utility.

#include <SPIMemory.h>  //Comment out Adesto from manufacturer ID in library to force SFDP or flash will be detected as 512k, not 16MB

volatile byte dacdat=0;
uint32_t freesec=4096;
uint32_t nextfreesec=1;
int choice=0;

struct FATLAYOUT{     //entry structure - 16 bytes * 64 max entries
 char name[9];        //Always 9
 uint8_t start00;     //start sector top         0x0000 - 0x1000
 uint8_t start01;     //start sector low
 uint8_t length00;    //start addr top byte
 uint8_t length01;    //start addr middle byte   Len in Bytes 24 bit val
 uint8_t length02;    //start addr low byte
 uint8_t normal;      //normal play 0=off 1=on
 uint8_t secret;      //secret play 0=off 1=on 
};

const int MAXTOC=60;
typedef struct FATLAYOUT DACFAT;       //keep in memory 1k, write to first sector
DACFAT TOC[MAXTOC];                        //Whole list in RAM, write to sector 0 if changed
int TOCCT=-1;
int showmenu=1;
uint8_t array0[128];


const int  SPI_CS=7; // Arduino Pin  7 - SPImemory CS line.  Pins 8,9 & 10 are SPI MISO/MOSI/SCK 
SPIFlash flash(SPI_CS);  

void setup() {
 dacdat=0x7F;
 playdat();
 DDRC |= B11000000;  //Set DAC output pins
 DDRB |= B01110000;
 DDRD |= B11010000;
 // put your setup code here, to run once:
 Serial.begin(115200);
 
 flash.begin(MB(16)); //open flash lib

 while (!Serial) {}  //Wait for serial connection
 Serial.println(F("Bad Idea Barta Board - TexElec.com 2021"));
 Serial.println(F("Uploader / SPI Chip Eraser Tool v0.1"));
 Serial.println();
 Serial.println(F("The playback routine is slow in this code."));
 Serial.println(F("Use this for uploads, and the main code for playback."));
 Serial.println();
 readtoc();
   
}

void loop() {
  while(Serial.available()) {Serial.read();}
  delay(100);
  sermenu();
}

void sermenu() {
 if (showmenu) {
  showmenu=0;
  Serial.println(F("1. Show Table of Contents"));
  Serial.println(F("2. Upload Entry"));
  Serial.println(F("3. Play Entry"));
  Serial.println(F("4. Erase Chip"));
  Serial.println();
  Serial.print(F("> "));
 }
 if (Serial.available()>0) {
  showmenu=1;
  choice=Serial.read();
  Serial.write(choice);
  Serial.println();
  switch (choice) {
    case 0x31:showtoc();
              break;
    case 0x32:upload();
              break;
    case 0x33:playsong();
              break;
    case 0x34:erase();
              break;
    default:  break;
  }
 }
}


void playsong() {
 byte t1;
 byte value[2];
 int sel;
 uint32_t saddr,leng;
 showtoc();

 if (TOCCT){
  Serial.print(F("Enter Song number to play (2 chars total): "));
  for (int i=0;i<2;i++) {
   while (Serial.available()<1) {}
   t1=Serial.read() - 48;
   Serial.print(t1);
   value[i]=t1;
  }
  sel=(int)value[0]*10;
  sel+=(int)value[1];
  
  saddr=TOC[sel].start00;
  saddr=(saddr<<8) + TOC[sel].start01;

  Serial.println();

  leng=TOC[sel].length00;
  leng=(leng<<8) + TOC[sel].length01;
  leng=(leng<<8) + TOC[sel].length02; 
    
  Serial.println();
  Serial.print(F("Playing Selection "));
  Serial.print(sel,DEC);
  Serial.print(". 0x");
  Serial.print(leng,HEX);
  Serial.println(F(" bytes long. "));
  Serial.print(F("Start Address: 0x"));
  Serial.print(saddr,HEX);
  Serial.print(F(". "));
  Serial.println();
  Serial.println();

  for (uint32_t i=0;i<leng;i++) {
   dacdat=flash.readByte((saddr*4096)+i);
   playdat();
   delayMicroseconds(1);
  }
 }
 
}

void writetoc() {
 Serial.println(F("Writing TOC..."));
 flash.eraseSector(0);                   //this call, and flast.write forever slowdown reads from SPI.
 for (int i=0;i<MAXTOC;i++) {
  for (int j=0;j<9;j++) {
   flash.writeByte((i*16)+j,TOC[i].name[j]);
  }
  flash.writeByte((i*16)+ 9,TOC[i].start00);
  flash.writeByte((i*16)+10,TOC[i].start01);
  flash.writeByte((i*16)+11,TOC[i].length00);
  flash.writeByte((i*16)+12,TOC[i].length01);
  flash.writeByte((i*16)+13,TOC[i].length02);
  flash.writeByte((i*16)+14,TOC[i].normal);
  flash.writeByte((i*16)+15,TOC[i].secret);
 }

 flash.writeByte(4093,((nextfreesec/256)-(nextfreesec%256)/256));
 flash.writeByte(4094,(nextfreesec%256));

 
 flash.writeByte(4095,TOCCT);   //number of recs
 Serial.println();
 Serial.println(F("Done..."));
 Serial.println();
}

void readtoc() {
 Serial.println(F("Reading TOC..."));
 TOCCT=flash.readByte(4095);
 if (TOCCT!=255) {
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
 else {Serial.println();Serial.println(F("No entries found..."));}
 Serial.println();
 Serial.println(F("Done..."));
 Serial.println();  
}

void upload() {
 freespace();
 byte t1;
 uint32_t t2,leng=0;
 byte a,b,c;
 Serial.print(F("Enter Song name (9 chars total - add trailing spaces to end): "));
 for (int i=0;i<9;i++) {
  while (Serial.available()<1) {}
  t1=Serial.read();
  Serial.write(t1);
  TOC[TOCCT].name[i]=t1;
 }
 Serial.println();
 Serial.println();
 Serial.print(F("Is this part of the normal (out of the bus) playlist? 0=NO 1=YES: "));
 while (Serial.available()<1) {}
 t1=Serial.read();
 if (t1==0x31) {TOC[TOCCT].normal=1;}
 if (t1!=0x31) {TOC[TOCCT].normal=0;}  //If entry is bad, set to zero
 Serial.write(t1);
 Serial.println();

 Serial.println();
 Serial.print(F("Is this part of the secret (plugged in the bus) playlist? 0=NO 1=YES: "));
 while (Serial.available()<1) {}
 t1=Serial.read();
 if (t1==0x31) {TOC[TOCCT].secret=1;}
 if (t1!=0x31) {TOC[TOCCT].secret=0;}  //If entry is bad, set to zero
 Serial.write(t1);
 Serial.println();

 while(Serial.available()) {Serial.read();}
 delay(100);
 Serial.println(F("Start File Upload Now..."));
 while(Serial.available()<1) {}
 for (int i=0;i<40;i++) {int k=Serial.read();}   //eat wav header

 while(Serial.available()<1) {}                  //get payload size from WAV header
 a=Serial.read();
 TOC[TOCCT].length02=a;
 
 while(Serial.available()<1) {}                  //get payload size from WAV header
 b=Serial.read();
 TOC[TOCCT].length01=b;

 while(Serial.available()<1) {}                  //get payload size from WAV header
 c=Serial.read();
 TOC[TOCCT].length00=c;

 leng=c;                                         //use 24 bit value.  SPIROM is max 0xFFFFFF anyway.
 leng=(leng<<8) + b;
 leng=(leng<<8) + c; 

 while(Serial.available()<1) {}  
 Serial.read();                                  //eat 32bit length - should be zero
 
 for (uint32_t i=0;i<(leng/128);i++) {
  for (uint32_t j=0;j<128;j++) {
   while(Serial.available()<1) {}
   array0[j]=Serial.read();                      //use array for speed, but greater than 128 doesn't seem to help much.
  }                                              //and eats RAM
  flash.writeByteArray((nextfreesec*4096)+(i*128), array0, 128, true); 
 }
 
 t2=((leng/4096)-((leng%4096)/4096));            //get whole value only

 TOC[TOCCT].start00=((nextfreesec/256)-((nextfreesec%256)/256));  //get high byte of start sector
 TOC[TOCCT].start01=(nextfreesec%256);                            //dumb bitshifting, bit it works
 nextfreesec+=(t2+1);                                             //mark next free sector
 TOCCT++;                                                         //bump TOC counter

 Serial.print(leng);
 Serial.print(F(" bytes written."));
 Serial.println();
 writetoc();
 freespace();
 while(Serial.available()) {Serial.read();}
 Serial.println();
}

void showtoc() {
 Serial.println();
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
  if (TOC[i].start00 < 0x10) {Serial.print(F("0"));};
  Serial.print(TOC[i].start00,HEX);
  if (TOC[i].start01 < 0x10) {Serial.print(F("0"));};
  Serial.print(TOC[i].start01,HEX);
  Serial.print("     0x");
  if (TOC[i].length00 < 0x10) {Serial.print(F("0"));};
  Serial.print(TOC[i].length00,HEX);
  if (TOC[i].length01 < 0x10) {Serial.print(F("0"));};
  Serial.print(TOC[i].length01,HEX);
  if (TOC[i].length02 < 0x10) {Serial.print(F("0"));};
  Serial.print(TOC[i].length02,HEX);
  Serial.print("    ");
  if  (TOC[i].normal) {Serial.print(F("YES"));}
  if (!TOC[i].normal) {Serial.print(F(" NO"));}
  Serial.print("   ");
  if  (TOC[i].secret) {Serial.println(F("YES"));}
  if (!TOC[i].secret) {Serial.println(F(" NO"));}
 }
 Serial.println();
 if (TOCCT==0) {
  Serial.println(F("No entries found."));
  Serial.println();
 }
 freespace();
}

void erase() {
  if (flash.eraseChip()) {
  Serial.println();
  Serial.println(F("Chip erased."));                         //taken from the diag routine originally
  TOCCT=0;
  nextfreesec=1;
  freesec=4096;
  Serial.println();
  writetoc();
  }
  else {
    Serial.println(F("Erase failed!"));
  }
}

void freespace() {
 Serial.print(F("Free Sectors: "));
 Serial.println(freesec-nextfreesec,DEC);
 Serial.print(F("Free Bytes: "));
 Serial.println((freesec-nextfreesec)*4096,DEC);
 Serial.println();
}

void playdat() {
/*if (dacdat & 0x80) PORTC |=(1<<PC7) ; else PORTC &=~(1<<PC7);  //original method, a little slower
  if (dacdat & 0x40) PORTC |=(1<<PC6) ; else PORTC &=~(1<<PC6);
  if (dacdat & 0x20) PORTB |=(1<<PB6) ; else PORTB &=~(1<<PB6);
  if (dacdat & 0x10) PORTB |=(1<<PB5) ; else PORTB &=~(1<<PB5);
  if (dacdat & 0x08) PORTB |=(1<<PB4) ; else PORTB &=~(1<<PB4);
  if (dacdat & 0x04) PORTD |=(1<<PD7) ; else PORTD &=~(1<<PD7);
  if (dacdat & 0x02) PORTD |=(1<<PD6) ; else PORTD &=~(1<<PD6);
  if (dacdat & 0x01) PORTD |=(1<<PD4) ; else PORTD &=~(1<<PD4);*/
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
