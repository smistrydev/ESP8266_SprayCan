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
#define ONE_SEC_TIME       1000000   //  1 sec
#define FIF_MIN_TIME 15*60*1000000    // 15 min
#define ONE_HOU_TIME 60*60*1000000   //  1 hour


// Time related Valriables
unsigned int localPort = 2390;      // local port to listen for UDP packets
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP udp;
unsigned long epoch;
unsigned int hh = 0;
unsigned int mm = 0;
unsigned int ss = 0;

#define RTC_ENDT 70
uint32_t rtcEndTime;

void setup() {

  startTime = system_get_time();
  system_rtc_mem_read(RTC_ENDT, &rtcEndTime, 4);

  nextSleep = FIF_MIN_TIME;
  pinMode(14, OUTPUT);
  digitalWrite(14, LOW);

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
    rtcEndTime = 0;
  }
  else {
    struct rst_info *info;
    info = system_get_rst_info();
    int reason = info->reason;

    if (reason == 0) {
      Serial.println("REASON 0 - REASON_DEFAULT_RST");
    }
    if (reason == 1) {
      Serial.println("REASON 1 - REASON_WDT_RST");
    }
    if (reason == 2) {
      Serial.println("REASON 2 - REASON_EXCEPTION_RST");
    }
    if (reason == 3) {
      Serial.println("REASON 3 - REASON_SOFT_WDT_RST");
    }
    if (reason == 4) {
      Serial.println("REASON 4 - REASON_SOFT_RESTART");
    }
    if (reason == 5) {
      Serial.println("REASON 5 - REASON_DEEP_SLEEP_AWAKE");
    }
    if (reason == 6) {
      Serial.println("REASON 6 - REASON_EXT_SYS_RST ");
    }

    if (reason == 6) {
      rtcStore[0] = 123;
      rtcStore[1] = STATE_HOUSKEEPING;
      rtcStore[2] = 0;
      system_rtc_mem_write(RTC_BASE, rtcStore, 3);
      Serial.println("Reset button press detected.");
    }

  }

  switch (rtcStore[1]) {
    case STATE_COLDSTART:
      if (connectToInternet()) {
        getTime();
        nextSleep = doCalculation();

        String subject =  "Device ";
        subject =  subject + ESP.getChipId();
        subject =  subject + " has Powered up.";

        String message =  "Battery Power: ";
        message = message + ESP.getVcc();
        message = message + "<br />  Power Up Time: " + hh + ":" + mm + ":" + ss;
        message = message + "<br />  Count: ";
        message = message + rtcStore[2];
        message = message + "<br />  sleep time (ms): ";
        message = message + (nextSleep);
        message = message + "<br />  sleep time (min): ";
        message = message + (nextSleep / 60000000);
        message = message + "<br />  startTime: ";
        message = message + startTime;
        message = message + "<br />  rtcEndTime: ";
        message = message + rtcEndTime;
        message = message + "<br />  epoch: ";
        message = message + epoch;

        if (ESP.getVcc() < 2650) {
          subject = "WARNING: Battery LOW - " + subject;
        }

        Serial.println(subject);
        Serial.println(message);

        sendEmail(subject, message);
        disconnectInternet();
        rtcStore[1] = STATE_SLEEP_WAKE;
        system_rtc_mem_write(RTC_BASE, rtcStore, 3);
      }
      break;
    case STATE_SLEEP_WAKE:
      if (rtcStore[2] < 48) {
        doWork();
        nextSleep = nextSleep + 24000000;

      } else {
        if (connectToInternet()) {
          getTime();

          String subject =  "Device ";
          subject = subject + ESP.getChipId();
          subject = subject + " is Just wake up for Count!!!. Count: ";
          subject = subject + rtcStore[2];

          String message =  "Battery Power: ";
          message = message + ESP.getVcc();
          message = message + "<br />  Wake Up Time: " + hh + ":" + mm + ":" + ss;
          message = message + "<br />  Count: ";
          message = message + rtcStore[2];
          message = message + "<br />  sleep time (ms): ";
          message = message + (nextSleep);
          message = message + "<br />  sleep time (min): ";
          message = message + (nextSleep / 60000000);
          message = message + "<br />  startTime: ";
          message = message + startTime;
          message = message + "<br />  rtcEndTime: ";
          message = message + rtcEndTime;
          message = message + "<br />  epoch: ";
          message = message + epoch;

          sendEmail(subject, message);
          disconnectInternet();
        }
        nextSleep = ONE_HOU_TIME + 137000000;
      }
      if (rtcStore[2] >= 60) {
        rtcStore[1] = STATE_HOUSKEEPING;
        system_rtc_mem_write(RTC_BASE, rtcStore, 3);
        nextSleep = 100;

        rtcEndTime = system_get_time();
        system_rtc_mem_write(RTC_ENDT, &rtcEndTime, 4);

        ESP.deepSleep(nextSleep, WAKE_RFCAL);
      }
      break;
    case STATE_HOUSKEEPING:
      if (connectToInternet()) {
        getTime();
        nextSleep = doCalculation();
        String subject =  "Device ";
        subject =  subject + ESP.getChipId();
        subject =  subject + " Syncronizing with time.";

        String message =  "Battery Power: ";
        message = message + ESP.getVcc();
        message = message + "<br />  HouseKeeping Time: " + hh + ":" + mm + ":" + ss;
        message = message + "<br />  Count: ";
        message = message + rtcStore[2];
        message = message + "<br />  sleep time (ms): ";
        message = message + (nextSleep);
        message = message + "<br />  sleep time (min): ";
        message = message + (nextSleep / 60000000);
        message = message + "<br />  startTime: ";
        message = message + startTime;
        message = message + "<br />  rtcEndTime: ";
        message = message + rtcEndTime;
        message = message + "<br />  epoch: ";
        message = message + epoch;

        if (ESP.getVcc() < 2650) {
          subject = "WARNING: Battery LOW - " + subject;
        }

        Serial.println(subject);
        Serial.println(message);
        sendEmail(subject, message);
        disconnectInternet();

        rtcStore[1] = STATE_SLEEP_WAKE;
        if (rtcStore[2] >= 60) {
          rtcStore[2] = 255;
        }

        system_rtc_mem_write(RTC_BASE, rtcStore, 3);
        ESP.deepSleep(nextSleep, WAKE_RFCAL);

      }
      break;
  }

  rtcStore[2]++;
  system_rtc_mem_write(RTC_BASE, rtcStore, 3);

  uint32_t runtime = system_get_time() - startTime;
  nextSleep = nextSleep - runtime;

  Serial.print("Next sleep : ");
  Serial.println(nextSleep);
  //  ESP.deepSleep(nextSleep, WAKE_RF_DISABLED);
  rtcEndTime = system_get_time();
  system_rtc_mem_write(RTC_ENDT, &rtcEndTime, 4);
  ESP.deepSleep(nextSleep, WAKE_RFCAL);

  yield();

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

  byte timeAttempt = 0;

  while (timeAttempt < 10) {

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


    if (cb) {
      udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      unsigned long secsSince1900 = highWord << 16 | lowWord;

      const unsigned long seventyYears = 2208988800UL;
      epoch = secsSince1900 - seventyYears;

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

    Serial.println("no packet yet");
    timeAttempt++;
    delay(10);

  }
  ESP.deepSleep(10e6, WAKE_RFCAL);

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

void doWork() {
  if (connectToInternet()) {
    getTime();

    String subject =  "Device ";
    subject =  subject + ESP.getChipId();
    subject =  subject + " is DO WORK!!!. Count: ";
    subject =  subject + rtcStore[2];

    String message =  "Battery Power: ";
    message = message + ESP.getVcc();
    message = message + "<br />  Work Time: " + hh + ":" + mm + ":" + ss;
    message = message + "<br />  Count: ";
    message = message + rtcStore[2];
    message = message + "<br />  sleep time (ms): ";
    message = message + (nextSleep);
    message = message + "<br />  sleep time (min): ";
    message = message + (nextSleep / 60000000);
    message = message + "<br />  startTime: ";
    message = message + startTime;
    message = message + "<br />  rtcEndTime: ";
    message = message + rtcEndTime;
    message = message + "<br />  epoch: ";
    message = message + epoch;

    sendEmail(subject, message);
    disconnectInternet();
  }

  digitalWrite(14, HIGH);
  delay(1000);
  yield();
  digitalWrite(14, LOW);
}

uint32_t doCalculation() {
  uint32_t nextSleep = FIF_MIN_TIME;

  byte _hh = hh - 7;
  _hh = _hh % 24;

  int timeInSecs = ss;
  timeInSecs = timeInSecs + (mm * 60);
  timeInSecs = timeInSecs + (_hh * 60 * 60);
  Serial.print("time in seconds: ");
  Serial.println(timeInSecs);

  if (_hh < 12) {
    Serial.print("remaining time in seconds to next slot: ");
    nextSleep = (timeInSecs % 900);
    nextSleep = 900 - nextSleep;
    nextSleep = nextSleep * 1000000;
    Serial.println(nextSleep);

    rtcStore[2] = timeInSecs / 900;
    system_rtc_mem_write(RTC_BASE, rtcStore, 3);
    Serial.println("Count : ");
    Serial.println(rtcStore[2]);


  }
  else {
    Serial.print("remaining time in seconds to next slot: ");
    nextSleep = (timeInSecs % 3600);
    nextSleep = 3600 - nextSleep;
    nextSleep = nextSleep * 1000000;
    Serial.println(nextSleep);

    _hh = _hh - 12;
    _hh = _hh + 47;
    rtcStore[2] = _hh;
    system_rtc_mem_write(RTC_BASE, rtcStore, 3);

    Serial.println("Count : ");
    Serial.println(rtcStore[2]);

  }

  return nextSleep;
}

void checkBattery() {
  Serial.print("Battery Voltage: ");
  Serial.println(ESP.getVcc());
}
