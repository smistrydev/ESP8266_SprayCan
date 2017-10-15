/**
   Sanjay Mistry
   Copyright (C) 2017

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "credentials.h"
#include "Gsender.h"

#define SERIAL_DEBUG

// Include API-Headers
extern "C" {
  // #include "ets_sys.h"
  // #include "os_type.h"
  // #include "osapi.h"
  // #include "mem.h"
#include "user_interface.h"
  // #include "cont.h"
}

ADC_MODE(ADC_VCC); //vcc read-mode

// State related Variables
#define STATE_COLDSTART 0
#define STATE_SLEEP_WAKE 1
#define STATE_HOUSKEEPING 2
#define STATE_CONNECT_WIFI 4
#define RTC_BASE 64
byte rtcStore[3];
uint32_t nextSleep, startTime;
#define TEN_MIN_TIME 10*60*1000000 // 1min
#define ONE_HOU_TIME 60*60*1000000    // 10sec

// Time related Valriables
unsigned int localPort = 2390;      // local port to listen for UDP packets
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP udp;
unsigned int hh = 0;
unsigned int mm = 0;
unsigned int ss = 0;

void setup() {

  startTime = system_get_time();
  nextSleep = TEN_MIN_TIME;

  WiFi.forceSleepBegin();
  yield();

#ifdef SERIAL_DEBUG
  Serial.begin(74880);
  delay(10);
  Serial.println();
#endif

  system_rtc_mem_read(RTC_BASE, rtcStore, 3);

  if (rtcStore[0] != 123) {
    rtcStore[0] = 123;
    rtcStore[1] = STATE_COLDSTART;
    rtcStore[2] = 0;
    system_rtc_mem_write(RTC_BASE, rtcStore, 3);
  } else {
    if (rtcStore[1] == STATE_SLEEP_WAKE) {
      rtcStore[2] += 1;
      //      if (rtcStore[2] > 4) {
      //        nextSleep = SLEEP_TIME;
      //      }
      if (rtcStore[2] > 4) {
        rtcStore[1] = STATE_CONNECT_WIFI;
        rtcStore[2] = 0;
      }
    }
  }


  if (connectToInternet()) {
    getTime();
    if (rtcStore[1] == STATE_COLDSTART) {
      String timeString = "Time: ";
      timeString = timeString + hh + ":" + mm + ":" + ss;
      sendEmail("Device has powered up.", timeString);
    }
    delay(1000);
    disconnectInternet();
  }

}

void loop() {

  delay(10);
  yield();

}

//
//  INTERNET RELATED CODE
//

bool connectToInternet() {
  uint32_t time1 = millis();

  WiFi.forceSleepWake();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("----------------->>>>>");

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
    count += 1;
    if (count > 100) {
      return false;
    }
  }

  uint32_t time2 = millis();

  Serial.println("");
  Serial.print("WiFi connected. took: ");
  Serial.println(time2 - time1);
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  return true;

}

void disconnectInternet() {
  Serial.println("WiFi disconnect.");
  WiFi.disconnect();
  WiFi.forceSleepBegin();  // send wifi to sleep to reduce POWER consumption
  yield();
}

//
//  TIME RELATED CODE
//

unsigned long getTime() {

  Serial.println("Starting UDP");

  hh = 0;
  mm = 0;
  ss = 0;

  udp.begin(localPort);

  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);

  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  // wait to see if a reply is available

  int cb = udp.parsePacket();

  for (int i = 0; i <= 100; i++) {
    if (!cb) {
      cb = udp.parsePacket();
      delay(10);
    } else {
      break;
    }
  }


  if (!cb) {
    Serial.println("no packet yet");
    ESP.deepSleep(10e6, WAKE_RFCAL);
  }
  else {
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears;

    hh = ((((epoch  % 86400L) / 3600) + 11) % 24);
    mm = (epoch  % 3600) / 60;
    ss = epoch % 60;
    Serial.print("time:");
    Serial.print(hh);
    Serial.print(':');
    Serial.print(mm);
    Serial.print(':');
    Serial.print(ss);
    Serial.println("");

    return epoch;

  }

}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
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

void sendEmail(const String &subject, const String &message) {
  Gsender *gsender = Gsender::Instance();
  if (gsender->Subject(subject)->Send(EMAILBASE64_LOGIN, EMAILBASE64_PASSWORD, EMAIL_FROM, EMAIL_TO, message)) {
    Serial.println("Message send.");
  } else {
    Serial.print("Error sending message: ");
    Serial.println(gsender->getError());
  }

}
