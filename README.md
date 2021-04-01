# BadIdeaBarta
Code, ROM and Notes on the Bad Idea Barta Board from TexElec.com

Thanks to:David Murray, aka the 8-Bit Guy 
           Clint Basigner, aka LGR
           Christian Simpson, aka Perifractic
         & Adrian Black of Adrian's Digital Basement

They were kind enough to contribte voice lines for this project, and it really made it a lot cooler than it would
have been.  Thanks guys!              

Note:
Very crude interface which will let you do what you need, and not much more.  Not much in the
way of error checking, so precise input is needed for the serial menu.
 
What is this thing?  It's a novelty card I created which started as an April fools day idea I had to cram as many 
leaky batteries onto one board.  Needless to say, it got pretty feature creepy.

See the youtube video I made on it here for more info:
Or buy one from our website, if there are still any left:

Operation:
The card has two main modes.  The "out of the bus" or "in the bus" mode.  Or as I call the Taunt and Secret mode.
The card will power on, and by default play one of the "taunt" messages at random.  When uploading wav data to the
on-board SPI memory, you may mark it for playback in one, or both modes.  If you hold the button in for one second 
while "out of the bus" it will toggle between random & sequential playback of the taunt messages.  

While in the bus, the card will really only know there is bus activity.  I originally tried to read specific IO calls
to make specific messages, but the 16MHz on the Arduino won't cut it.  Instead, it basically just detects any bus activity
and plays messages at random.  Now, it only plays the "secret" messages at random.  The button is still working at this 
point, but just because I didn't bother to turn it off.  I just don't really expect it to be pushed, and if so, it 
just works as normal.  Oh yes, I intentionally do not turn the LED on or off during normal mode, to make it a little more
mysterious.  It flashes only in secret mode.

The other "mode" is the serial input.  The USB port is used as the primary power and also as a serial communications port.
This card has some canned audio on-board, but it has a lot of free space remaining.  The on-board SPI Flash Memory chip 
will hold 16MB.  The code, as written, will playback 16KHz 8-Bit Mono unsigned WAV files. This equates to over 17 mins 
of playback!  There is way less audio shipped on the card, so you can add your own messages, and have them playback as part
of the card.  MAXTOC controls the number of allowed messages.  Increase if needed, but watch RAM!

The last mode the serial menu presents is serial streaming.  This will allow you to playback audio via the serial port
at 48Khz!  I had originally tried to get the flash memory to playback @ 48Khz, but the read routine was too slow in 
single byte mode, and there is not enough RAM to cache enough data to read larger data chunks, so 16KHz.  However, the 
serial port requires no processing from the card, and it could easily playback even higher-rates, but I capped it @ 48Khz.
To playback files, just encode them as described above, but instead, resample the file to 48KHz for playback.

Why is uploading not part of this program?  I tried to add it here originally, but I ran into an issue with the 
SPIMemory library.  Basically, if you call erase or write, it will slowdown each successive read.  The downside
is that you cannot sustain 16KHz.  Even if you never call the functions, you will have the issue.  I "fixed" it, by
seperating the code into two, now actually three pieces.  The last program is just to upload the entire original ROM
image which shipped on the card: spirom.bin.

To upload, I use audacity and the following steps from any audio source:
1. Open the file, if it's stereo, select Tracks->Mix->Mix Stereo down to mono.
2. At the bottom, set the project rate to 16000.
3. Select Tracks->Resample.  It should default to 16000 now, but if not, set to 16000 and ok.
4. Select File->Export->Export as WAV
5. On the Encoding drop-down, select Unsigned 8-bit PCM
6. You are now ready to send this file to the on-board memory!

ImageWriter is used to recover the original ROM contents which shipped on the Bad Idea Barta Board.  It will erase
and allow you to upload spirom.bin (included here) back to the ROM chip in the event you accidentally delete it, or want 
to "factory reset" the card.

The Master code is what ships on the card, but you can upload the code with Arduino IDE for any programs.  This card is 
basically a 16MHz 5V Arduino Leonardo. 

A lot of this can be fixed with better code, just ran out of time! It is what it is, enjoy!
