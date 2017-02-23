/*
   Time_NTP.pde
   Example showing time sync to NTP time source

   This sketch uses the Ethernet library
*/


#include <JTEncode.h>
#include <int.h>
#include <TimeLib.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include <Adafruit_SI5351.h>

Adafruit_SI5351 clockgen = Adafruit_SI5351();

#define TONE_SPACING            1.46           // ~1.46 Hz
#define WSPR_CTC                10672         // CTC value for WSPR
#define SYMBOL_COUNT            WSPR_SYMBOL_COUNT
#define CORRECTION              350            // Freq Correction in HZ





hw_timer_t * timer = NULL;

// WiFi network name and password:
const char * networkName = "xxxxxxxxxxx";
const char * networkPswd = "xxxxxxxxxxx";


JTEncode jtencode;
unsigned long freq =  14097100UL + CORRECTION;              // Change this
char call[7] = "NOCALL";                        // Change this
char loc[5] = "FM05";                           // Change this
uint8_t dbm = 10;
uint8_t tx_buffer[SYMBOL_COUNT];

boolean connected = false;
volatile bool proceed = false;
// NTP Servers:
IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 103); // time-c.timefreq.bldrdoc.gov

WiFiUDP udp;

const int timeZone = -5;     // Central European Time
//const int timeZone = -5;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)

void wspr_spacing()
{
  proceed = true;
}

unsigned int localPort = 8888;  // local port to listen for UDP packets

void setup()
{

  Serial.begin(9600);

  while (!Serial) ; // Needed for Leonardo only
  delay(250);

  connectToWiFi(networkName, networkPswd);

  Serial.println("waiting for sync");
  delay(10000);
  setSyncProvider(getNtpTime);
  setSyncInterval(300000); //Bogus interval, we will reset later
  delay(5000);
  // Initialize the Si5351
  // Change the 2nd parameter in init if using a ref osc other
  // than 25 MHz




  if (clockgen.begin() != ERROR_NONE)
  {
    /* There was a problem detecting the IC ... check your connections */
    Serial.print("Ooops, no Si5351 detected ... Check your wiring or I2C ADDR!");
    while (1);
  }
  Serial.println("OK!");
  delay(5000);
  si5351aSetFrequency(freq);
  clockgen.enableOutputs(false);

  Wire.beginTransmission(0x60);
  Wire.write(16);
  Wire.write(0x03 & 0xFF);
  Wire.endTransmission();



  timer = timerBegin(3, 80, 1);
  timerAttachInterrupt(timer, &wspr_spacing, 1);
  timerAlarmWrite(timer, 682687, true);
  timerAlarmEnable(timer);
  Serial.print(hour());
  Serial.print(":");
  Serial.print(minute());
  Serial.print(":");

  Serial.println(second());
}


void loop()
{

  if (timeStatus() == timeSet && minute() % 4 == 0 && second() == 0)
  {
    setSyncInterval(180); ///reset sync time to prevent interuption of beacon
    delay(1000);
    encode();
    delay(1000);
  }
  delay(1000);
}



/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void connectToWiFi(const char * ssid, const char * pwd) {
  Serial.println("Connecting to WiFi network: " + String(ssid));

  // delete old config
  WiFi.disconnect(true);
  //register event handler
  WiFi.onEvent(WiFiEvent);

  //Initiate connection
  WiFi.begin(ssid, pwd);

  Serial.println("Waiting for WIFI connection...");
}

//wifi event handler
void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      //When connected set
      Serial.print("WiFi connected! IP address: ");
      Serial.println(WiFi.localIP());
      //initializes the UDP state
      //This initializes the transfer buffer
      udp.begin(WiFi.localIP(), localPort);
      connected = true;
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      connected = false;
      break;
  }
}

void encode()
{
  uint8_t i;
  Serial.println("Sending Beacon");
  jtencode.wspr_encode(call, loc, dbm, tx_buffer);

  // Reset the tone to 0 and turn on the output
  Wire.beginTransmission(0x60);
  Wire.write(3);
  Wire.write(0xfe & 0xFF);
  Wire.endTransmission();


  // Now do the rest of the message
  for (i = 0; i < SYMBOL_COUNT; i++)
  {
    si5351aSetFrequency((freq) + (tx_buffer[i] * TONE_SPACING));
    proceed = false;
    while (!proceed);
  }

  // Turn off the output
  Wire.beginTransmission(0x60);
  Wire.write(3);
  Wire.write(0xFF & 0xFF);
  Wire.endTransmission();
  Serial.println();
  Serial.println("Beacon Sent");

}

void si5351aSetFrequency(uint32_t frequency)
{
  uint32_t pllFreq;
  uint32_t xtalFreq = 25000000;
  uint32_t l;
  float f;
  uint8_t mult;
  uint32_t num;
  uint32_t denom;
  uint32_t divider;

  divider = 50;// Force MultiSynth divider to 50 for reduced jitter
  
  pllFreq = divider * frequency;  // Calculate the pllFrequency: the divider * desired output frequency

  mult = pllFreq / xtalFreq;    // Determine the multiplier to get to the required pllFrequency
  l = pllFreq % xtalFreq;     // It has three parts:
  f = l;              // mult is an integer that must be in the range 15..90
  f *= 200000;         // num and denom are the fractional parts, the numerator and denominator
  f /= xtalFreq;          // each is 20 bits (range 0..1048575)
  num = f;            // the actual multiplier is  mult + num / denom
  denom = 200000;       

 
  clockgen.setupPLL(SI5351_PLL_A, mult, num, denom);
  clockgen.setupMultisynth(0, SI5351_PLL_A, divider, 0, 1);
}


