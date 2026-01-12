//Leo Bodner LBE-1420 Programmer
//
//  ***** Important... the PIO-USB library must run at a multiple of 120MHz. 240Mhz overclock is recommended. 
//
// DP+ and Dp- must be pulled to ground with 15K resistors. 
//

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include "pio_usb.h"
#include "pico/platform.h"
#include <Adafruit_GFX.h>
#include "Adafruit_TinyUSB.h"

//two alternative display chips
#define SH1106
//#define SSD1306

#ifdef SSD1306
#include <Adafruit_SSD1306.h>
#endif

#ifdef SH1106
#include <Adafruit_SH110X.h>
#endif

#define I2CADD 0x3C       //dISPLAY i2c Address
//#define I2CADD 0x78

#define HOST_PIN_DP   2   // Pin used as D+ for host, D- on next sequential pin.

#define I2CSDAPin 6
#define I2CSCLPin 7

//Switches
#define UpPin 13
#define DownPin 10
#define LeftPin 9
#define RightPin 11
#define EnterPin 12

enum buttons {NONE,UP,DOWN,LEFT,RIGHT,ENTER};

uint8_t button;

// TinyUSB Host object
Adafruit_USBH_Host USBHost;

//OLED Display
#ifdef SSD1306
Adafruit_SSD1306 display(128,64,&Wire1,-1);
#endif
#ifdef SH1106
Adafruit_SH1106G display=Adafruit_SH1106G(128,64,&Wire1,-1);
#endif

// =======================
// USB IDs
// =======================
#define LBE_VID        0x1DD2
#define LBE_1420_PID   0x2443
#define LBE_1421_PID   0x2444

#define MINFREQ 1
#define MAXFREQ 1600000000

// =======================
// HID / Protocol
// =======================
#define REPORT_SIZE 60

#define CMD_SET_OUTPUT_EN        0x01     //01 00 sets outputs off   01 FF sets outputs on
#define CMD_BLINK_LEDS           0x02     //02 00 will cause the leds to blink for a few seconds
#define CMD_SET_FREQ_TEMP_1420   0x03     //03 f0 f1 f2 f3      send 4 byte integer representing the frequency in Hz f0 is LSB F3 is MSB. Frequency is temporary and not stored 
#define CMD_SET_FREQ_1420        0x04     //04 f0 f1 f2 f3      send 4 byte integer representing the frequency in Hz f0 is LSB F3 is MSB. Frequency is stored
#define CMD_SET_FREQ1_TEMP_1421  0x05     //05 00 d0 d1 d2 f0 f1 f2 f3     Frequency1 in Hz. d is fractional part MOD 16777216   f is integer part. Temporary
#define CMD_SET_FREQ1_1421       0x06     //06 00 d0 d1 d2 f0 f1 f2 f3     Frequency1 in Hz. d is fractional part MOD 16777216   f is integer part. Stored
#define CMD_SET_SATS             0x07     //07 nn     where nn is bitmap for sats enabled. 0x01=GPS 0x02=SBAS 0x04=Galileo 0x08=Beideo 0x40=GLONASS 
#define CMD_SET_MODE             0x09     //09 nn     where nn is the mode  00=Portable 02=Stationary 08=Airbourne 

#define CMD_SET_FREQ2_TEMP_1421  0x00     //Not Confirmed
#define CMD_SET_FREQ2_1421       0x00     //Not Confirmed

#define CMD_SET_PLL_FLL          0x0B     //0B nn     where nn =00 is Phase Locked Loop    nn = 01 is Frequency Locked Loop
#define CMD_SET_PPS_1421         0x0C     //Unconfirmed
#define CMD_SET_POWER1           0x0D     //0D 00       Set output power  00 = high power 01=low power
#define CMD_SET_POWER2_1421      0x0E     //Unconfirmed

#define CMD_STATUS               0x4B     //used to request a status message


// =======================
// Globals
// =======================
uint8_t lbe_dev_addr = 0;
uint8_t lbe_instance = 0;
bool lbe_connected = false;
bool lbe1421 = false;

long lastStatusTime =0;
#define NUMBEROFMEMS 100
uint32_t memory[NUMBEROFMEMS]= {10000000};

uint8_t status_buf[REPORT_SIZE];
volatile bool status_ready = false;

uint8_t send_buf[REPORT_SIZE];
volatile bool send_busy = false;

struct LBEStatus 
{
  bool gps_lock;
  bool pll_lock;
  bool antenna_ok;
  bool output_enabled;
  uint32_t frequency;
  uint32_t freqdecimal;
};

LBEStatus lbe_status;



void setup1()
{
  // Initialize PIO USB host
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = HOST_PIN_DP;
  USBHost.configure_pio_usb(1, &pio_cfg);
  // Start TinyUSB host
  USBHost.begin(1);
}

void loop1()
{
  // TinyUSB host task must run all the time so we will dedicate this core to just do that.
  USBHost.task();
}

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(512);
  
  Wire1.setSDA(I2CSDAPin);
  Wire1.setSCL(I2CSCLPin);

#ifdef SSD1306
  display.begin(SSD1306_SWITCHCAPVCC, I2CADD);
#endif
#ifdef SH1106
  display.begin(I2CADD);  
#endif

  pinMode(UpPin,INPUT_PULLUP);
  pinMode(DownPin,INPUT_PULLUP);
  pinMode(LeftPin,INPUT_PULLUP);
  pinMode(RightPin,INPUT_PULLUP);
  pinMode(EnterPin,INPUT_PULLUP);

  readSettings();
  display.setRotation(2);
  display.clearDisplay();   // clears the screen and buffer
  display.setTextSize(0);
  display.setTextColor(1);
  display.setCursor(10,20);
  display.print("LBE-1420 Programmer");
  display.setCursor(35,40);
  display.print("G4EML 2026");
  display.display();
}

void loop()
{
  uint32_t val;
  static uint32_t lastFreq = 0;
  static bool lastGPS = 1;
  static bool noPress = false;

  //request status every 5 seconds while device connected. 
  if((millis() > lastStatusTime + 5000) && (lastFreq !=0))
   {
    lastStatusTime = millis();
    lbeRequestStatus();
   }

  // Handle received status
  if (status_ready)
  {
    status_ready = false;
    if(lbe_status.frequency != lastFreq)
    {
    display.setCursor(0,12);
    display.fillRect(0,12,128,8,0);
    display.print("Freq= ");
    display.print((double) lbe_status.frequency / 1000000, 6);
    display.print(" MHz");
    lastFreq = lbe_status.frequency;
    display.display();
    }

    if(lbe_status.gps_lock != lastGPS)
    {
    display.setCursor(0,24);
    display.fillRect(0,24,128,8,0);
    display.print("GPS ");
    if(lbe_status.gps_lock == 1)
     {
       display.print("Locked    ");
     }
    else
    {
       display.print("Not Locked");
     }
     lastGPS = lbe_status.gps_lock;
     display.display();
    }
  }
  
  //Process any messages from the other core
  if(rp2040.fifo.available())
  {
    uint32_t msg = rp2040.fifo.pop();
    switch(msg)
    {
      case 1:
      display.clearDisplay();   // clears the screen and buffer
      display.setCursor(0,0);
      display.print("LBE-1420 Connected");
      display.display();
      lbeRequestStatus();
      break;

      case 2:
      display.clearDisplay();   // clears the screen and buffer
      display.setCursor(0,0);
      display.print("LBE-1421 Connected");
      display.display();
      lbeRequestStatus();
      break;

      case 3:
      display.clearDisplay();   // clears the screen and buffer
      display.setCursor(0,0);
      display.print("Device Disconnected");
      display.display();
      lastFreq = 0;
      lastGPS = 1;
      break;
    }
  }

// Handle Button Presses
  if(buttonPressed())
   {
    if(noPress)
     {
       noPress = false;
       processButton();
     }
   }
  else
   {
    noPress = true;
    delay(100);            //debounce delay
   }

}

bool buttonPressed(void)
{
  button = NONE;
  if(digitalRead(UpPin) == 0) button = UP;
  if(digitalRead(DownPin) == 0) button = DOWN;
  if(digitalRead(LeftPin) == 0) button = LEFT; 
  if(digitalRead(RightPin) == 0) button = RIGHT;
  if(digitalRead(EnterPin) == 0) button = ENTER;

  return (button != NONE);
}

void processButton(void)
{
  static uint8_t menuMode = 0;
  enum MM {MOFF,MMEM,MDIG};
  static uint8_t memNumber = 0;
  static uint8_t digitNumber = 0;
  char ml[32];
  static uint32_t increment;
  uint8_t mult;

    switch(menuMode)
    {
      case MOFF:
       if(button == ENTER)
        {
          menuMode = MMEM;
          sprintf(ml,"M%02d= %11.6f MHz",memNumber , (double) memory[memNumber] / 1000000.0);
          menuLine(ml);
          digitNumber =1;
          setUnderLine(digitNumber);
        }

      break;

      case MMEM:
       if(button == ENTER)
        {
          lbeSetFrequency(1, memory[memNumber], true);
          menuMode = MOFF;
          digitNumber =0;
          menuLine("");
          setUnderLine(digitNumber);
          lastStatusTime = millis() - 4000;    //update the display in 1 second
          break;
        }
       if(button == UP)
        {
          memNumber = (memNumber + 1) % NUMBEROFMEMS;
        }
        if(button == DOWN)
        {
          memNumber = (memNumber -1);
          if(memNumber > NUMBEROFMEMS) memNumber = NUMBEROFMEMS - 1;
        }   
        if(button == RIGHT)
        {
          digitNumber = 2;
          menuMode = MDIG;
        }    
       sprintf(ml,"M%02d= %11.6f MHz",memNumber , (double) memory[memNumber] / 1000000.0);
       menuLine(ml);
       setUnderLine(digitNumber);
       break;

       case MDIG:
       if(button == ENTER)
        {
          if((millis() - lastStatusTime) > 500);
          {
           lbeSetFrequency(1, memory[memNumber], true);
           writeSettings();
           lastStatusTime = millis() - 4500;    //update the display in 0.5 second           
          }
          break;
        }
       if(button == RIGHT)
       {
        digitNumber=digitNumber +1;
        if(digitNumber > 11) digitNumber = 11;
        setUnderLine(digitNumber);
       }
       if(button == LEFT)
       {
        digitNumber = digitNumber - 1;
        if(digitNumber == 1)
         {
          menuMode = MMEM;
         }
        setUnderLine(digitNumber);
       }
       if(button == UP)
       {
        increment = 1;
        mult = 11-digitNumber;
        while(mult > 0)
         {
          increment = increment *10;
          mult--;
         }
        memory[memNumber] = memory[memNumber] +  increment;
        if(memory[memNumber] >= MAXFREQ) memory[memNumber] = MAXFREQ;
        sprintf(ml,"M%02d= %11.6f MHz",memNumber , (double) memory[memNumber] / 1000000.0);
        menuLine(ml);
       }
       if(button == DOWN)
       {
        increment = 1;
        mult = 11-digitNumber;
        while(mult > 0)
         {
          increment = increment *10;
          mult--;
         }
         if(memory[memNumber] > increment)
          {
            memory[memNumber] = memory[memNumber] -  increment;
          } 
          else
          {
            memory[memNumber] = MINFREQ;
          }
        sprintf(ml,"M%02d= %11.6f MHz",memNumber , (double) memory[memNumber] / 1000000.0);
        menuLine(ml);
       }
    }

}

void menuLine(char * txt)
{
    display.setCursor(0,50);
    display.fillRect(0,50,128,8,0);
    display.print(txt);
    display.display();
}

void setUnderLine(uint8_t ul)
{
    display.fillRect(0,60,128,4,0);
    switch(ul)
    {
      case 0:
      break;
      case 1:
        display.fillRect(0 , 60, 18, 4, 1);
      break;
      case 2 ... 5:
        display.fillRect(ul*6 + 18, 60, 6, 4, 1);
      break;
      case 6 ... 11:
        display.fillRect(ul*6 + 24, 60, 6, 4, 1);
      break;
    }
    display.display();
}

void readSettings(void)
{
  if(EEPROM.read(0) == 0x73)              //valid EEPROM
   {
    EEPROM.get(1,memory);
   }
  else
   {
     for(int i = 0;i<NUMBEROFMEMS; i++)
      {
        memory[i] = 10000000;
      }
      writeSettings();
   } 
}

void writeSettings(void)
{
  EEPROM.write(0,0x73);
  EEPROM.put(1,memory);
  EEPROM.commit();
}

// =======================
// TinyUSB callbacks
// =======================

// Device mounted
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  uint16_t vid, pid;

  tuh_vid_pid_get(dev_addr, &vid, &pid);

  if (vid == LBE_VID && (pid == LBE_1420_PID || pid == LBE_1421_PID))
  {
    lbe_dev_addr = dev_addr;
    lbe_instance = instance;

    if(pid == LBE_1420_PID)
    {
      rp2040.fifo.push(1);             //signal this event to the other core
      lbe1421=false;
      lbe_connected = true;
    }

    if(pid == LBE_1421_PID)
    {
      rp2040.fifo.push(2);           //signal this event to the other core
      lbe1421=true;
      lbe_connected = true;
    }
  }
}

// Device unmounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  if (dev_addr == lbe_dev_addr && instance == lbe_instance)
  {
    lbe_connected = false;
    rp2040.fifo.push(3);              //signal this event to the other core
  }
}

// GET_REPORT completed

void tuh_hid_get_report_complete_cb(uint8_t dev_addr, uint8_t idx, uint8_t report_id, uint8_t report_type, uint16_t len)
{
  if (!lbe_connected) return;
  if (dev_addr != lbe_dev_addr || idx != lbe_instance) return;
  if (report_type != HID_REPORT_TYPE_FEATURE) return;
  if (report_id != CMD_STATUS) return;
  if (len < REPORT_SIZE) return;

  // status_buf was filled by TinyUSB
  uint8_t raw = status_buf[1];

  lbe_status.gps_lock       = raw & (1 << 0);
  lbe_status.pll_lock       = raw & (1 << 1);
  lbe_status.antenna_ok     = raw & (1 << 2);
  lbe_status.output_enabled = raw & (1 << 4);

  lbe_status.frequency =status_buf[6] | (status_buf[7] << 8) | (status_buf[8] << 16) | (status_buf[9] << 24);

  lbe_status.freqdecimal = status_buf[3] | (status_buf[4] << 8) | (status_buf[5] << 16);

  status_ready = true;
}

//=======================
//Low-level helpers
//=======================
bool lbeSendFeature(void)
{
  if (!lbe_connected) return false;  
  send_busy = true;
  bool ok = tuh_hid_set_report(lbe_dev_addr, lbe_instance, 0x00, HID_REPORT_TYPE_FEATURE, send_buf, REPORT_SIZE);
  return ok;
}

void tuh_hid_set_report_complete_cb(uint8_t dev_addr, uint8_t idx, uint8_t report_id, uint8_t report_type, uint16_t len)
{
  send_busy = false;                    //finished with the send buffer
}


bool lbeRequestStatus(void)
{
  if (!lbe_connected) return false;

  memset(status_buf, 0, REPORT_SIZE);
  status_ready = false;

  return tuh_hid_get_report(lbe_dev_addr, lbe_instance, CMD_STATUS, HID_REPORT_TYPE_FEATURE, status_buf, REPORT_SIZE);
}

// =======================
// High-level API
// =======================

void lbeSetFrequency(uint8_t chan, uint32_t freq_hz, bool permanent)
{
  while(send_busy)            //wait until the send buffer is available
  {
    delay(1);
  }

  for(int i=0;i<REPORT_SIZE;i++)
   {
    send_buf[i] = 0;
   }

  if(lbe1421)
  {
    if(chan == 2)
    {
      send_buf[0] = CMD_SET_FREQ2_TEMP_1421;
    }
    else
    {
      send_buf[0] = CMD_SET_FREQ1_TEMP_1421;
    }

  }
  else
  {
    send_buf[0] = CMD_SET_FREQ_TEMP_1420;  
  }

  if(permanent)
  {
   send_buf[0] = send_buf[0] +1;
  }

  send_buf[1] = freq_hz & 0xFF;
  send_buf[2] = (freq_hz >> 8) & 0xFF;
  send_buf[3] = (freq_hz >> 16) & 0xFF;
  send_buf[4] = (freq_hz >> 24) & 0xFF;
  lbeSendFeature();
}

void lbeEnableOutput(bool enable)
{
  while(send_busy)            //wait until the send buffer is available
  {
    delay(1);
  }

  for(int i=0;i<REPORT_SIZE;i++)
   {
    send_buf[i] = 0;
   }
  send_buf[0] = CMD_SET_OUTPUT_EN;
  send_buf[1] = enable ? 0xFF : 0x00;
  lbeSendFeature();
}

void lbeSetPower(uint8_t chan, bool low)
{
  while(send_busy)            //wait until the send buffer is available
  {
    delay(1);
  }

  for(int i=0;i<REPORT_SIZE;i++)
   {
    send_buf[i] = 0;
   }

if((lbe1421) && (chan == 2))
  {
    send_buf[0] = CMD_SET_POWER2_1421;
  }
  else
  {
    send_buf[0] = CMD_SET_POWER1;
  }

  send_buf[1] = low;
  lbeSendFeature();
}


