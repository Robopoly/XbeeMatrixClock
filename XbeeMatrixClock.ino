/*
 Title:        XbeeMatrixClock
 Description:  Makes a clock out of the XbeeMatrix, the Xbee isn't used.
               Time, date and a Robopoly logo are shown on screen.
               Uses an ATmega164P on a PRisme2 board.
 Author:       Karl Kangur <karl.kangur@gmail.com>
 Date:         2012-11-08
 Version:      1.0
 Website:      http://robopoly.ch
 Comments:     Matrix control based on demo16x24.c by Bill Westfield
*/

#include <avr/pgmspace.h>
#include <util/delay.h>
#include "ht1632.h"
#include "fonts.h"

const uint8_t robopoly[] PROGMEM = {
0b00000000,
0b00000000,
0b01011011,
0b01100100,
0b01000100,
0b01000011,
0b00000000,
0b00000000,
0b01000000,
0b01000011,
0b01110100,
0b11001100,
0b11001011,
0b00110000,
0b00000000,
0b00000000,
0b00000000,
0b00000000,
0b10110011,
0b11001100,
0b01001100,
0b01110011,
0b01000000,
0b01000000,
0b01000000,
0b01000000,
0b01100100,
0b11100100,
0b11011000,
0b01001000,
0b00010000,
0b00000000};

// 24 modules of 8 lines = 192 bytes of data
byte c, addr, x, y, data;
byte screen[192];
// for serial input
char inputStream[7];

uint8_t month_length[2][12] = {
  {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
  {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

// store hours, minutes and seconds
struct time
{
  unsigned char hours, minutes, seconds;
  unsigned char day, month, year;
};

// time instance
time myTime;

/*
 * Set these constants to the values of the pins connected to the SureElectronics Module
 */
static const byte ht1632_data = PA(1);  // Data pin (pin 7)
static const byte ht1632_wrclk = PA(3); // Write clock pin (pin 5)
/*
 * ht1632_writebits
 * Write bits (up to 8) to h1632 on pins ht1632_data, ht1632_wrclk
 * Chip is assumed to already be chip-selected
 * Bits are shifted out from MSB to LSB, with the first bit sent
 * being (bits & firstbit), shifted till firsbit is zero.
 */
void ht1632_chipselect(byte chipno)
{
  switch(chipno)
  {
    case 0:
      digitalWrite(PA(7), 0);
      break;
    case 1:
      digitalWrite(PA(6), 0);
      break;
    case 2:
      digitalWrite(PA(5), 0);
      break;
    case 3:
      digitalWrite(PA(4), 0);
      break;
  }
}

void ht1632_chipfree(byte chipno)
{
  switch(chipno)
  {
    case 0:
      digitalWrite(PA(7), 1);
      break;
    case 1:
      digitalWrite(PA(6), 1);
      break;
    case 2:
      digitalWrite(PA(5), 1);
      break;
    case 3:
      digitalWrite(PA(4), 1);
      break;
  }
}

void ht1632_writebits(byte bits, byte firstbit)
{
  while (firstbit) {
    digitalWrite(ht1632_wrclk, LOW);
    if (bits & firstbit) {
      digitalWrite(ht1632_data, HIGH);
    } 
    else {
      digitalWrite(ht1632_data, LOW);
    }
    digitalWrite(ht1632_wrclk, HIGH);
    firstbit >>= 1;
  }
}

static void ht1632_sendcmd(byte chip, byte command)
{
  ht1632_chipselect(chip);  // Select chip
  ht1632_writebits(HT1632_ID_CMD, 1<<2);  // send 3 bits of id: COMMMAND
  ht1632_writebits(command, 1<<7);  // send the actual command
  ht1632_writebits(0, 1);       /* one extra dont-care bit in commands. */
  ht1632_chipfree(chip); //done
}

static void ht1632_senddata(byte chip, byte address, byte data)
{
  ht1632_chipselect(chip);  // Select chip
  ht1632_writebits(HT1632_ID_WR, 1<<2);  // send ID: WRITE to RAM
  ht1632_writebits(address, 1<<6); // Send address
  ht1632_writebits(data, 1<<3); // send 4 bits of data
  ht1632_chipfree(chip); // done
}

void setup ()  // flow chart from page 17 of datasheet
{
  // chip select lines
  pinMode(PA(7), OUTPUT);
  pinMode(PA(6), OUTPUT);
  pinMode(PA(5), OUTPUT);
  pinMode(PA(4), OUTPUT);
  digitalWrite(PA(7), HIGH);
  digitalWrite(PA(6), HIGH);
  digitalWrite(PA(5), HIGH);
  digitalWrite(PA(4), HIGH);
  
  // led pin
  pinMode(PC(2), OUTPUT);
  
  // clock and data lines
  pinMode(ht1632_wrclk, OUTPUT);
  pinMode(ht1632_data, OUTPUT);
  
  // system startup routine
  for(byte c = 0; c < 4; c++)
  {
    ht1632_sendcmd(c, HT1632_CMD_SYSDIS);  // Disable system
    ht1632_sendcmd(c, HT1632_CMD_COMS11);  // 16*32, PMOS drivers
    ht1632_sendcmd(c, HT1632_CMD_MSTMD);   // Master Mode
    ht1632_sendcmd(c, HT1632_CMD_SYSON);   // System on
    ht1632_sendcmd(c, HT1632_CMD_LEDON);   // LEDs on
    for (byte i=0; i<128; i++)
      ht1632_senddata(c, i, 0);  // clear the display!
  }
  
  // enable serial to set date/time
  Serial.begin(9600);
  
  // initialize time to 00h00m00s
  myTime.hours = 23;
  myTime.minutes = 59;
  myTime.seconds = 55;
  
  // initialize date to 01 january 1970
  myTime.day = 28;
  myTime.month = 2;
  myTime.year = 12;
  
  // clock the timer from external ocillator connected to PC6 and PC7 pins
  ASSR |= (1 << AS2);
  // initialize counter
  TCNT2 = 0;
  // set prescaler to 128 (32768/128 = 256)
  TCCR2A = 0;
  TCCR2B = (1 << CS22) + (1 << CS20);
  // wait for the busy flags to go away
  while(ASSR & 0x07);
  // enable overflow interrupt (interrput when TCNT2 == 255)
  TIMSK2 |= (1 << TOIE2);
  asm("SEI");
  
  updateScreen();
}

// increment time every second based on the interrupt
ISR(TIMER2_OVF_vect)
{
  if(myTime.seconds < 59)
  {
    myTime.seconds++;
    updateSeparator();
  }
  else if(myTime.minutes < 59)
  {
    myTime.seconds = 0;
    myTime.minutes++;
    updateScreen();
  }
  else if(myTime.hours < 23)
  {
    myTime.seconds = 0;
    myTime.minutes = 0;
    myTime.hours++;
    updateScreen();
  }
  else
  {
    myTime.seconds = 0;
    myTime.minutes = 0;
    myTime.hours = 0;
    
    // see if leap year
    byte leap = 0;
    if((myTime.year % 4) == 0)
    {
      leap = 1;
    }
    
    // increase day, month, year
    myTime.day++;
    if(myTime.day > month_length[leap][myTime.month - 1])
    {
      myTime.day = 1;
      myTime.month++;
      if(myTime.month > 12)
      {
        myTime.month = 1;
        myTime.year++;
      }
    }
    
    updateScreen();
  }
}

void loop ()
{
  if(Serial.available() > 0)
  {
    // read bytes until a new line is detected
    Serial.readBytesUntil('\n', inputStream, 7);
    unsigned char hours, minutes, seconds, day, month, year;
    switch(inputStream[0])
    {
      case 't':
        sendTime();
        break;
      case 's':
        Serial.write("Enter time in HHMMSS format\n");
        // allow 10 seconds to write new time
        Serial.setTimeout(10000);
        Serial.readBytesUntil('\n', inputStream, 7);
        // set timeout back to default value
        Serial.setTimeout(1000);
        // set the time
        hours = 10*(inputStream[0]-48)+inputStream[1]-48;
        minutes = 10*(inputStream[2]-48)+inputStream[3]-48;
        seconds = 10*(inputStream[4]-48)+inputStream[5]-48;
        
        // test for valid input
        if(hours >= 0 && hours <= 23 && minutes >= 0 && minutes <= 59 && seconds >= 0 && seconds <= 59)
        {
          myTime.hours = hours;
          myTime.minutes = minutes;
          myTime.seconds = seconds;
          // announce new time
          Serial.write("New time set\n");
          updateScreen();
        }
        else
        {
          // report error
          Serial.write("Error setting time!\n");
        }
        break;
      case 'd':
        Serial.write("Enter date in DDMMYY format\n");
        // allow 10 seconds to write new date
        Serial.setTimeout(10000);
        Serial.readBytesUntil('\n', inputStream, 7);
        // set timeout back to default value
        Serial.setTimeout(1000);
        // set the time
        day = 10*(inputStream[0]-48)+inputStream[1]-48;
        month = 10*(inputStream[2]-48)+inputStream[3]-48;
        year = 10*(inputStream[4]-48)+inputStream[5]-48;
        
        // test for valid input
        if(day >= 0 && day <= 31 && month >= 0 && month <= 12 && year >= 0 && year <= 99)
        {
          myTime.day = day;
          myTime.month = month;
          myTime.year = year;
          // announce new time
          Serial.write("New date set\n");
          updateScreen();
        }
        else
        {
          // report error
          Serial.write("Error setting date!\n");
        }
        break;
      default:
        // inform about available commands
        Serial.write("Available commands:\nt: return date and time\ns: set time\nd: set date\n");
    }
  }
}

// send time to serial in HHhMMmSSs format
void sendTime()
{
  if(myTime.hours < 10)
  {
    Serial.write("0");
  }
  Serial.print(myTime.hours);
  Serial.write("h");
  if(myTime.minutes < 10)
  {
    Serial.write("0");
  }
  Serial.print(myTime.minutes);
  Serial.write("m");
  if(myTime.seconds < 10)
  {
    Serial.write("0");
  }
  Serial.print(myTime.seconds);
  Serial.write("s ");
  if(myTime.day < 10)
  {
    Serial.write("0");
  }
  Serial.print(myTime.day);
  Serial.write(".");
  if(myTime.month < 10)
  {
    Serial.write("0");
  }
  Serial.print(myTime.month);
  Serial.write(".");
  if(myTime.year < 10)
  {
    Serial.write("0");
  }
  Serial.print(myTime.year);
  Serial.write("\n");
}

void updateSeparator()
{
  if((myTime.seconds % 2) == 0)
  {
    // set separator
    ht1632_senddata(0, 38*2+1, 0b0110);
    ht1632_senddata(0, 38*2+2, 0b0110);
    ht1632_senddata(0, 40*2+1, 0b0110);
    ht1632_senddata(0, 40*2+2, 0b0110);
  }
  else
  {
    // clear separator
    ht1632_senddata(0, 38*2+1, 0);
    ht1632_senddata(0, 38*2+2, 0);
    ht1632_senddata(0, 40*2+1, 0);
    ht1632_senddata(0, 40*2+2, 0);
  }
  
  PORTC ^= 0x4;
}

void updateScreen()
{
  // erase screen
  for(addr = 0; addr < 192; addr++)
    screen[addr] = 0;
  
  addCharacter(b[myTime.hours / 10], 0, 0, 1, 2);
  addCharacter(b[myTime.hours - (myTime.hours / 10) * 10], 8, 0, 1, 2);
  addCharacter(b[myTime.minutes / 10], 24, 0, 1, 2);
  addCharacter(b[myTime.minutes - (myTime.minutes / 10) * 10], 32, 0, 1, 2);
  
  addCharacter(s[myTime.day / 10], 48, 0, 1, 1);
  addCharacter(s[myTime.day - (myTime.day / 10) * 10], 56, 0, 1, 1);
  addCharacter(s[myTime.month / 10], 64, 0, 1, 1);
  addCharacter(s[myTime.month - (myTime.month / 10) * 10], 72, 0, 1, 1);
  addCharacter(s[myTime.year / 10], 80, 0, 1, 1);
  addCharacter(s[myTime.year - (myTime.year / 10) * 10], 88, 0, 1, 1);
  
  addCharacter(robopoly, 56, 1, 4, 1);
  
  // module counter
  for(c = 0; c < 4; c++)
  {
    // data written in nibbles (8*6*2 = 96)
    for(addr = 0; addr < 48; addr++)
    {
      // set 8 pixels
      data = screen[c*48 + addr];
      ht1632_senddata(c, addr*2+1, data & 0xf);
      ht1632_senddata(c, addr*2, data >> 4);
    }
  }
}

// width and height is the size in bytes
void addCharacter(const uint8_t* element, byte posx, byte posy, byte width, byte height)
{
  for(x = 0; x < width*8; x++) // max 96
  {
    for(y = 0; y < height*8; y++) // max 16
    {
      // I'm quite proud of this one, don't try to understand this or you'll break your brain!
      byte pixel = ((pgm_read_word(&(element[y + (x / 8) * 8])) >> (7 - (x % 8))) & 1);
      byte pin = (7 - (y % 8));
      screen[(posx + x) * 2 + ((y % 16) / 8) + posy] = (screen[(posx + x) * 2 + ((y % 16) / 8) + posy] & ~(pixel << pin)) | (pixel << pin);
    }
  }
}

