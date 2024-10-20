/*********************************************************************************************************
  MatrixClock with Web Interface                                                                                 
  ********************************************************************************************************
  Author: HACK Labs
  Version 3.0 
  Modified by: Adrian Davis 
  Description: Added countdown timer by irisdown integration, web interface for adjusting settings, and Wi-Fi configuration. 
  *********************************************************************************************************/

#include <SPI.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>  // Added for web server functionality
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <time.h>
#include <EEPROM.h>             // Added for storing settings persistently

#define SDA_PIN        5      // Pin for SDA (I2C)
#define SCL_PIN        4      // Pin for SCL (I2C)
#define CS_PIN         15     // Pin for CS  (SPI)
#define MATRIX_COUNT   4      // Number of matrix modules
//#define REVERSE_HORIZONTAL
//#define REVERSE_VERTICAL

// Define the size of EEPROM to store settings
#define EEPROM_SIZE 512

// Wi-Fi Credentials
char ssid[32] = "";                    // your network SSID (name)
char pass[32] = "";                    // your network password

unsigned short maxPosX = MATRIX_COUNT * 8 - 1;            
unsigned short LEDMatrix[MATRIX_COUNT][8];                   
unsigned short helperArrayMAX[MATRIX_COUNT * 8];              
unsigned short helperArrayPos[MATRIX_COUNT * 8];              
unsigned int z_PosX = 0;                            
unsigned int d_PosX = 0;                            
bool flagTicker1s = false;
bool flagTicker50ms = false;
bool reverseDisplay;
unsigned long epoch = 0;
unsigned int localPort = 61003;                      // local port to listen for UDP packets
const char* ntpServerName = "pool.ntp.org";
const int NTP_PACKET_SIZE = 48;                      // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE];                  // buffer to hold incoming and outgoing packets
IPAddress timeServerIP;                            
tm *tt, ttm;

// Variables to handle the countdown timer
long countdownTime = 0;             // Time remaining in seconds
unsigned long countdownReceivedMillis = 0;  // Timestamp when the last countdown was received
bool countdownAvailable = false;    // Flag to indicate if countdown is available
bool countdownIsNegative = false;   // Flag to indicate if countdown time is negative

const unsigned char DS3231_ADDRESS = 0x68;
const unsigned char secondREG = 0x00;
const unsigned char minuteREG = 0x01;
const unsigned char hourREG = 0x02;
const unsigned char dayOfWeekREG = 0x03;                   // Weekday
const unsigned char dateREG = 0x04;
const unsigned char monthREG = 0x05;
const unsigned char yearREG = 0x06;
const unsigned char controlREG = 0x0E;
const unsigned char statusREG = 0x0F;
const unsigned char tempMSBREG = 0x11;
const unsigned char tempLSBREG = 0x12;

struct DateTime {
    unsigned short sec1, sec2, sec12, min1, min2, min12, hour1, hour2, hour12;
    unsigned short day1, day2, day12, month1, month2, month12, year1, year2, year12, dayOfWeek;
} currentTime;

// The object for the Ticker
Ticker ticker;
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
// Web server instance
ESP8266WebServer server(80);

// Months
char monthArray[12][5] = { { ' ', 'J', 'A', 'N', ' ' }, { ' ', 'F', 'E', 'B', ' ' },
        { ' ', 'M', 'A', 'R', ' ' }, { ' ', 'A', 'P', 'R', ' ' }, { ' ', 'M', 'A',
                'Y', ' ' }, { ' ', 'J', 'U', 'N', ' ' }, { ' ', 'J', 'U', 'L', ' ' }, {
                ' ', 'A', 'U', 'G', ' ' }, { ' ', 'S', 'E', 'P', ' ' }, { ' ', 'O', 'C',
                'T', ' ' }, { ' ', 'N', 'O', 'V', ' ' }, { ' ', 'D', 'E', 'C', ' ' } };
// Days
char dayOfWeekArray[7][4] = { { 'S', 'U', 'N', ' ' }, { 'M', 'O', 'N', ' ' }, { 'T', 'U', 'E', ' ' }, {
        'W', 'E', 'D', ' ' }, { 'T', 'H', 'U', ' ' }, { 'F', 'R', 'I', ' ' }, { 'S', 'A', 'T', ' ' } };

// Font1
unsigned short const font1[96][9] = { { 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00 },   // 0x20, Space
        { 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x00 },   // 0x21, !
        { 0x07, 0x09, 0x09, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x22, "
        { 0x07, 0x0a, 0x0a, 0x1f, 0x0a, 0x1f, 0x0a, 0x0a, 0x00 },   // 0x23, #
        { 0x07, 0x04, 0x0f, 0x14, 0x0e, 0x05, 0x1e, 0x04, 0x00 },   // 0x24, $
        { 0x07, 0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13, 0x00 },   // 0x25, %
        { 0x07, 0x04, 0x0a, 0x0a, 0x0a, 0x15, 0x12, 0x0d, 0x00 },   // 0x26, &
        { 0x07, 0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x27, '
        { 0x07, 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02, 0x00 },   // 0x28, (
        { 0x07, 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08, 0x00 },   // 0x29, )
        { 0x07, 0x04, 0x15, 0x0e, 0x1f, 0x0e, 0x15, 0x04, 0x00 },   // 0x2a, *
        { 0x07, 0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00, 0x00 },   // 0x2b, +
        { 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02 },   // 0x2c, ,
        { 0x07, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00 },   // 0x2d, -
        { 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00 },   // 0x2e, .
        { 0x07, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10, 0x00 },   // 0x2f, /
        { 0x07, 0x0F, 0x09, 0x09, 0x09, 0x09, 0x09, 0x0F, 0x00 },   // 0x30, 0
        { 0x07, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00 },   // 0x31, 1
        { 0x07, 0x0F, 0x01, 0x01, 0x0F, 0x08, 0x08, 0x0F, 0x00 },   // 0x32, 2
        { 0x07, 0x0F, 0x01, 0x01, 0x0F, 0x01, 0x01, 0x0F, 0x00 },   // 0x33, 3
        { 0x07, 0x09, 0x09, 0x09, 0x0F, 0x01, 0x01, 0x01, 0x00 },   // 0x34, 4
        { 0x07, 0x0F, 0x08, 0x08, 0x0F, 0x01, 0x01, 0x0F, 0x00 },   // 0x35, 5
        { 0x07, 0x0F, 0x08, 0x08, 0x0F, 0x09, 0x09, 0x0F, 0x00 },   // 0x36, 6
        { 0x07, 0x0F, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00 },   // 0x37, 7
        { 0x07, 0x0F, 0x09, 0x09, 0x0F, 0x09, 0x09, 0x0F, 0x00 },   // 0x38, 8
        { 0x07, 0x0F, 0x09, 0x09, 0x0F, 0x01, 0x01, 0x0F, 0x00 },   // 0x39, 9
        { 0x04, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00 },   // 0x3a, :
        { 0x07, 0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x04, 0x08, 0x00 },   // 0x3b, ;
        { 0x07, 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02, 0x00 },   // 0x3c, <
        { 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x3d, =
        { 0x07, 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08, 0x00 },   // 0x3e, >
        { 0x07, 0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04, 0x00 },   // 0x3f, ?
        { 0x07, 0x0e, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0f, 0x00 },   // 0x40, @
        { 0x07, 0x04, 0x0a, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x00 },   // 0x41, A
        { 0x07, 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e, 0x00 },   // 0x42, B
        { 0x07, 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e, 0x00 },   // 0x43, C
        { 0x07, 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E, 0x00 },   // 0x44, D
        { 0x07, 0x1f, 0x10, 0x10, 0x1c, 0x10, 0x10, 0x1f, 0x00 },   // 0x45, E
        { 0x07, 0x1f, 0x10, 0x10, 0x1f, 0x10, 0x10, 0x10, 0x00 },   // 0x46, F
        { 0x07, 0x0e, 0x11, 0x10, 0x10, 0x13, 0x11, 0x0f, 0x00 },   // 0x37, G
        { 0x07, 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11, 0x00 },   // 0x48, H
        { 0x07, 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e, 0x00 },   // 0x49, I
        { 0x07, 0x1f, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0c, 0x00 },   // 0x4a, J
        { 0x07, 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11, 0x00 },   // 0x4b, K
        { 0x07, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f, 0x00 },   // 0x4c, L
        { 0x07, 0x11, 0x1b, 0x15, 0x11, 0x11, 0x11, 0x11, 0x00 },   // 0x4d, M
        { 0x07, 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x00 },   // 0x4e, N
        { 0x07, 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e, 0x00 },   // 0x4f, O
        { 0x07, 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10, 0x00 },   // 0x50, P
        { 0x07, 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d, 0x00 },   // 0x51, Q
        { 0x07, 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11, 0x00 },   // 0x52, R
        { 0x07, 0x0e, 0x11, 0x10, 0x0e, 0x01, 0x11, 0x0e, 0x00 },   // 0x53, S
        { 0x07, 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00 },   // 0x54, T
        { 0x07, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e, 0x00 },   // 0x55, U
        { 0x07, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04, 0x00 },   // 0x56, V
        { 0x07, 0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11, 0x00 },   // 0x57, W
        { 0x07, 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11, 0x00 },   // 0x58, X
        { 0x07, 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04, 0x00 },   // 0x59, Y
        { 0x07, 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f, 0x00 },   // 0x5a, Z
        { 0x07, 0x0e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0e, 0x00 },   // 0x5b, [
        { 0x07, 0x10, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01, 0x00 },   // 0x5c, '\'
        { 0x07, 0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e, 0x00 },   // 0x5d, ]
        { 0x07, 0x04, 0x0a, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x5e, ^
        { 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x00 },   // 0x5f, _
        { 0x07, 0x04, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x60, 
        { 0x07, 0x00, 0x0e, 0x01, 0x0d, 0x13, 0x13, 0x0d, 0x00 },   // 0x61, a
        { 0x07, 0x10, 0x10, 0x10, 0x1c, 0x12, 0x12, 0x1c, 0x00 },   // 0x62, b
        { 0x07, 0x00, 0x00, 0x0E, 0x10, 0x10, 0x10, 0x0E, 0x00 },   // 0x63, c
        { 0x07, 0x01, 0x01, 0x01, 0x07, 0x09, 0x09, 0x07, 0x00 },   // 0x64, d
        { 0x07, 0x00, 0x00, 0x0e, 0x11, 0x1f, 0x10, 0x0f, 0x00 },   // 0x65, e
        { 0x07, 0x06, 0x09, 0x08, 0x1c, 0x08, 0x08, 0x08, 0x00 },   // 0x66, f
        { 0x07, 0x00, 0x0e, 0x11, 0x13, 0x0d, 0x01, 0x01, 0x0e },   // 0x67, g
        { 0x07, 0x10, 0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x00 },   // 0x68, h
        { 0x05, 0x00, 0x02, 0x00, 0x06, 0x02, 0x02, 0x07, 0x00 },   // 0x69, i
        { 0x07, 0x00, 0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0c },   // 0x6a, j
        { 0x07, 0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12, 0x00 },   // 0x6b, k
        { 0x05, 0x06, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00 },   // 0x6c, l
        { 0x07, 0x00, 0x00, 0x0a, 0x15, 0x15, 0x11, 0x11, 0x00 },   // 0x6d, m
        { 0x07, 0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11, 0x00 },   // 0x6e, n
        { 0x07, 0x00, 0x00, 0x0e, 0x11, 0x11, 0x11, 0x0e, 0x00 },   // 0x6f, o
        { 0x07, 0x00, 0x00, 0x1c, 0x12, 0x12, 0x1c, 0x10, 0x10 },   // 0x70, p
        { 0x07, 0x00, 0x00, 0x07, 0x09, 0x09, 0x07, 0x01, 0x01 },   // 0x71, q
        { 0x07, 0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10, 0x00 },   // 0x72, r
        { 0x07, 0x00, 0x00, 0x0f, 0x10, 0x0e, 0x01, 0x1e, 0x00 },   // 0x73, s
        { 0x07, 0x08, 0x08, 0x1c, 0x08, 0x08, 0x09, 0x06, 0x00 },   // 0x74, t
        { 0x07, 0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0d, 0x00 },   // 0x75, u
        { 0x07, 0x00, 0x00, 0x11, 0x11, 0x11, 0x0a, 0x04, 0x00 },   // 0x76, v
        { 0x07, 0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0a, 0x00 },   // 0x77, w
        { 0x07, 0x00, 0x00, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x00 },   // 0x78, x
        { 0x07, 0x00, 0x00, 0x11, 0x11, 0x0f, 0x01, 0x11, 0x0e },   // 0x79, y
        { 0x07, 0x00, 0x00, 0x1f, 0x02, 0x04, 0x08, 0x1f, 0x00 },   // 0x7a, z
        { 0x07, 0x06, 0x08, 0x08, 0x10, 0x08, 0x08, 0x06, 0x00 },   // 0x7b, {
        { 0x07, 0x04, 0x04, 0x04, 0x00, 0x04, 0x04, 0x04, 0x00 },   // 0x7c, |
        { 0x07, 0x0c, 0x02, 0x02, 0x01, 0x02, 0x02, 0x0c, 0x00 },   // 0x7d, }
        { 0x07, 0x08, 0x15, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x7e, ~
        { 0x07, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x00 }    // 0x7f, DEL
};

// Font2
unsigned short const font2[96][9] = { { 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00 },   // 0x20, Space
        { 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x00 },   // 0x21, !
        { 0x07, 0x09, 0x09, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x22, "
        { 0x07, 0x0a, 0x0a, 0x1f, 0x0a, 0x1f, 0x0a, 0x0a, 0x00 },   // 0x23, #
        { 0x07, 0x04, 0x0f, 0x14, 0x0e, 0x05, 0x1e, 0x04, 0x00 },   // 0x24, $
        { 0x07, 0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13, 0x00 },   // 0x25, %
        { 0x07, 0x04, 0x0a, 0x0a, 0x0a, 0x15, 0x12, 0x0d, 0x00 },   // 0x26, &
        { 0x07, 0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x27, '
        { 0x07, 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02, 0x00 },   // 0x28, (
        { 0x07, 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08, 0x00 },   // 0x29, )
        { 0x07, 0x04, 0x15, 0x0e, 0x1f, 0x0e, 0x15, 0x04, 0x00 },   // 0x2a, *
        { 0x07, 0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00, 0x00 },   // 0x2b, +
        { 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02 },   // 0x2c, ,
        { 0x07, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00 },   // 0x2d, -
        { 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00 },   // 0x2e, .
        { 0x07, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10, 0x00 },   // 0x2f, /
        { 0x07, 0x00, 0x00, 0x07, 0x05, 0x05, 0x05, 0x07, 0x00 },   // 0x30, 0
        { 0x07, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00 },   // 0x31, 1
        { 0x07, 0x00, 0x00, 0x07, 0x01, 0x07, 0x04, 0x07, 0x00 },   // 0x32, 2
        { 0x07, 0x00, 0x00, 0x07, 0x01, 0x07, 0x01, 0x07, 0x00 },   // 0x33, 3
        { 0x07, 0x00, 0x00, 0x05, 0x05, 0x07, 0x01, 0x01, 0x00 },   // 0x34, 4
        { 0x07, 0x00, 0x00, 0x07, 0x04, 0x07, 0x01, 0x07, 0x00 },   // 0x35, 5
        { 0x07, 0x00, 0x00, 0x07, 0x04, 0x07, 0x05, 0x07, 0x00 },   // 0x36, 6
        { 0x07, 0x00, 0x00, 0x07, 0x01, 0x01, 0x01, 0x01, 0x00 },   // 0x37, 7
        { 0x07, 0x00, 0x00, 0x07, 0x05, 0x07, 0x05, 0x07, 0x00 },   // 0x38, 8
        { 0x07, 0x00, 0x00, 0x07, 0x05, 0x07, 0x01, 0x07, 0x00 },   // 0x39, 9
        { 0x04, 0x00, 0x03, 0x03, 0x00, 0x03, 0x03, 0x00, 0x00 },   // 0x3a, :
        { 0x07, 0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x04, 0x08, 0x00 },   // 0x3b, ;
        { 0x07, 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02, 0x00 },   // 0x3c, <
        { 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x3d, =
        { 0x07, 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08, 0x00 },   // 0x3e, >
        { 0x07, 0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04, 0x00 },   // 0x3f, ?
        { 0x07, 0x0e, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0f, 0x00 },   // 0x40, @
        { 0x07, 0x04, 0x0a, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x00 },   // 0x41, A
        { 0x07, 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e, 0x00 },   // 0x42, B
        { 0x07, 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e, 0x00 },   // 0x43, C
        { 0x07, 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E, 0x00 },   // 0x44, D
        { 0x07, 0x1f, 0x10, 0x10, 0x1c, 0x10, 0x10, 0x1f, 0x00 },   // 0x45, E
        { 0x07, 0x1f, 0x10, 0x10, 0x1f, 0x10, 0x10, 0x10, 0x00 },   // 0x46, F
        { 0x07, 0x0e, 0x11, 0x10, 0x10, 0x13, 0x11, 0x0f, 0x00 },   // 0x37, G
        { 0x07, 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11, 0x00 },   // 0x48, H
        { 0x07, 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e, 0x00 },   // 0x49, I
        { 0x07, 0x1f, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0c, 0x00 },   // 0x4a, J
        { 0x07, 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11, 0x00 },   // 0x4b, K
        { 0x07, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f, 0x00 },   // 0x4c, L
        { 0x07, 0x11, 0x1b, 0x15, 0x11, 0x11, 0x11, 0x11, 0x00 },   // 0x4d, M
        { 0x07, 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x00 },   // 0x4e, N
        { 0x07, 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e, 0x00 },   // 0x4f, O
        { 0x07, 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10, 0x00 },   // 0x50, P
        { 0x07, 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d, 0x00 },   // 0x51, Q
        { 0x07, 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11, 0x00 },   // 0x52, R
        { 0x07, 0x0e, 0x11, 0x10, 0x0e, 0x01, 0x11, 0x0e, 0x00 },   // 0x53, S
        { 0x07, 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00 },   // 0x54, T
        { 0x07, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e, 0x00 },   // 0x55, U
        { 0x07, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04, 0x00 },   // 0x56, V
        { 0x07, 0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11, 0x00 },   // 0x57, W
        { 0x07, 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11, 0x00 },   // 0x58, X
        { 0x07, 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04, 0x00 },   // 0x59, Y
        { 0x07, 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f, 0x00 },   // 0x5a, Z
        { 0x07, 0x0e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0e, 0x00 },   // 0x5b, [
        { 0x07, 0x10, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01, 0x00 },   // 0x5c, '\'
        { 0x07, 0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e, 0x00 },   // 0x5d, ]
        { 0x07, 0x04, 0x0a, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x5e, ^
        { 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x00 },   // 0x5f, _
        { 0x07, 0x04, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x60, 
        { 0x07, 0x00, 0x0e, 0x01, 0x0d, 0x13, 0x13, 0x0d, 0x00 },   // 0x61, a
        { 0x07, 0x10, 0x10, 0x10, 0x1c, 0x12, 0x12, 0x1c, 0x00 },   // 0x62, b
        { 0x07, 0x00, 0x00, 0x0E, 0x10, 0x10, 0x10, 0x0E, 0x00 },   // 0x63, c
        { 0x07, 0x01, 0x01, 0x01, 0x07, 0x09, 0x09, 0x07, 0x00 },   // 0x64, d
        { 0x07, 0x00, 0x00, 0x0e, 0x11, 0x1f, 0x10, 0x0f, 0x00 },   // 0x65, e
        { 0x07, 0x06, 0x09, 0x08, 0x1c, 0x08, 0x08, 0x08, 0x00 },   // 0x66, f
        { 0x07, 0x00, 0x0e, 0x11, 0x13, 0x0d, 0x01, 0x01, 0x0e },   // 0x67, g
        { 0x07, 0x10, 0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x00 },   // 0x68, h
        { 0x05, 0x00, 0x02, 0x00, 0x06, 0x02, 0x02, 0x07, 0x00 },   // 0x69, i
        { 0x07, 0x00, 0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0c },   // 0x6a, j
        { 0x07, 0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12, 0x00 },   // 0x6b, k
        { 0x05, 0x06, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00 },   // 0x6c, l
        { 0x07, 0x00, 0x00, 0x0a, 0x15, 0x15, 0x11, 0x11, 0x00 },   // 0x6d, m
        { 0x07, 0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11, 0x00 },   // 0x6e, n
        { 0x07, 0x00, 0x00, 0x0e, 0x11, 0x11, 0x11, 0x0e, 0x00 },   // 0x6f, o
        { 0x07, 0x00, 0x00, 0x1c, 0x12, 0x12, 0x1c, 0x10, 0x10 },   // 0x70, p
        { 0x07, 0x00, 0x00, 0x07, 0x09, 0x09, 0x07, 0x01, 0x01 },   // 0x71, q
        { 0x07, 0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10, 0x00 },   // 0x72, r
        { 0x07, 0x00, 0x00, 0x0f, 0x10, 0x0e, 0x01, 0x1e, 0x00 },   // 0x73, s
        { 0x07, 0x08, 0x08, 0x1c, 0x08, 0x08, 0x09, 0x06, 0x00 },   // 0x74, t
        { 0x07, 0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0d, 0x00 },   // 0x75, u
        { 0x07, 0x00, 0x00, 0x11, 0x11, 0x11, 0x0a, 0x04, 0x00 },   // 0x76, v
        { 0x07, 0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0a, 0x00 },   // 0x77, w
        { 0x07, 0x00, 0x00, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x00 },   // 0x78, x
        { 0x07, 0x00, 0x00, 0x11, 0x11, 0x0f, 0x01, 0x11, 0x0e },   // 0x79, y
        { 0x07, 0x00, 0x00, 0x1f, 0x02, 0x04, 0x08, 0x1f, 0x00 },   // 0x7a, z
        { 0x07, 0x06, 0x08, 0x08, 0x10, 0x08, 0x08, 0x06, 0x00 },   // 0x7b, {
        { 0x07, 0x04, 0x04, 0x04, 0x00, 0x04, 0x04, 0x04, 0x00 },   // 0x7c, |
        { 0x07, 0x0c, 0x02, 0x02, 0x01, 0x02, 0x02, 0x0c, 0x00 },   // 0x7d, }
        { 0x07, 0x08, 0x15, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x7e, ~
        { 0x07, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x00 }    // 0x7f, DEL
};

// Settings variables
struct Settings {
    int timeOffset;                // Time offset in seconds
    bool dateScrollEnabled;        // Enable or disable date scroll
    int brightness;                // Brightness level (0-15)
    char countdownID[32];          // Countdown Timer ID
    bool displayModeHHMMSS;        // True for HH:MM:SS, False for MM:SS
} settings;

// Function prototypes
void saveSettings();
void loadSettings();
void handleRoot();
void handleNotFound();
void handleSaveSettings();
void setupWiFi();
void startWebServer();
void enterAPMode();
void displayClock();

// End of Part 1
//************************ Wi-Fi Setup Functions **************************************************************************
void setupWiFi() {
    Serial.println("Connecting to Wi-Fi...");
    WiFi.mode(WIFI_AP_STA);  // Enable both AP and Station modes

    if (strlen(ssid) == 0) {
        Serial.println("No Wi-Fi credentials stored.");
        enterAPMode();  // If no credentials, enter AP mode
        return;
    }

    WiFi.begin(ssid, pass);

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
        delay(500);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to Wi-Fi.");
        IPAddress localIP = WiFi.localIP();
        Serial.println(localIP);
        
        // Convert IP address to a string and scroll it on the matrix
        char ipStr[16];  // Buffer to hold the IP address as a string
        sprintf(ipStr, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
        
        // Scroll the IP address
        scrollText(ipStr);

        udp.begin(localPort);
        Serial.print("Local port: ");
        Serial.println(udp.localPort());

        // *** Start the web server here ***
        startWebServer();  // Ensure the web server starts in station mode
    } else {
        Serial.println("\nFailed to connect to Wi-Fi.");
        enterAPMode();  // Enter AP mode if Wi-Fi connection fails
    }
}


// Enter Access Point mode for Wi-Fi configuration
void enterAPMode() {
    Serial.println("Starting AP mode for configuration.");
    WiFi.softAP("MatrixClock_Config");
    Serial.println("AP SSID: MatrixClock_Config");
    
    // Call startWebServer to ensure it's running in both modes
    startWebServer();
}


void startWebServer() {
    server.on("/", handleRoot);
    server.on("/save", handleSaveSettings);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("Web server started.");
}

void handleRoot() {
    String html = "<html><head><title>MatrixClock (for Countdown Timer) - Setup</title></head><body>";
    html += "<h1>MatrixClock (for Countdown Timer) Configuration</h1>";
    html += "<form action=\"/save\" method=\"POST\">";

    html += "Wi-Fi SSID: <input type=\"text\" name=\"ssid\" value=\"" + String(ssid) + "\"><br>";
    html += "Wi-Fi Password: <input type=\"password\" name=\"pass\" placeholder=\"Leave blank to keep current password\"><br>";

    // Separate inputs for hours, minutes, and seconds
    html += "Time Offset Hours: <input type=\"number\" name=\"timeOffsetHours\" value=\"" + String(settings.timeOffset / 3600) + "\"><br>";
    html += "Time Offset Minutes: <input type=\"number\" name=\"timeOffsetMinutes\" value=\"" + String((settings.timeOffset % 3600) / 60) + "\"><br>";
    html += "Time Offset Seconds: <input type=\"number\" name=\"timeOffsetSeconds\" value=\"" + String(settings.timeOffset % 60) + "\"><br>";

    html += "Date Scroll: <input type=\"checkbox\" name=\"dateScroll\"" + String(settings.dateScrollEnabled ? " checked" : "") + "><br>";
    html += "Brightness (0-15): <input type=\"number\" name=\"brightness\" min=\"0\" max=\"15\" value=\"" + String(settings.brightness) + "\"><br>";
    html += "Countdown Timer ID: <input type=\"text\" name=\"countdownID\" value=\"" + String(settings.countdownID) + "\"><br>";
    html += "Display Mode: <select name=\"displayMode\">";
    html += "<option value=\"HHMMSS\"" + String(settings.displayModeHHMMSS ? " selected" : "") + ">HH:MM:SS</option>";
    html += "<option value=\"MMSS\"" + String(!settings.displayModeHHMMSS ? " selected" : "") + ">MM:SS</option>";
    html += "</select><br>";
    html += "<input type=\"submit\" value=\"Save Settings\">";
    html += "</form>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}


void handleSaveSettings() {
    if (server.hasArg("ssid")) {
        String newSSID = server.arg("ssid");
        newSSID.toCharArray(ssid, 32);
    }

    if (server.hasArg("pass") && server.arg("pass").length() > 0) {
        String newPass = server.arg("pass");
        newPass.toCharArray(pass, 32);
    }

    if (server.hasArg("timeOffsetHours") && server.hasArg("timeOffsetMinutes") && server.hasArg("timeOffsetSeconds")) {
        int hours = server.arg("timeOffsetHours").toInt();
        int minutes = server.arg("timeOffsetMinutes").toInt();
        int seconds = server.arg("timeOffsetSeconds").toInt();
        settings.timeOffset = (hours * 3600) + (minutes * 60) + seconds;
    }

    settings.dateScrollEnabled = server.hasArg("dateScroll");

    if (server.hasArg("brightness")) {
        settings.brightness = server.arg("brightness").toInt();
        if (settings.brightness < 0) settings.brightness = 0;
        if (settings.brightness > 15) settings.brightness = 15;
    }

    if (server.hasArg("countdownID")) {
        String newID = server.arg("countdownID");
        newID.toCharArray(settings.countdownID, 32);
    }

    if (server.hasArg("displayMode")) {
        String mode = server.arg("displayMode");
        settings.displayModeHHMMSS = (mode == "HHMMSS");
    }

    saveSettings();

    String html = "<html><head><title>Settings Saved</title></head><body>";
    html += "<h1>Settings Saved!</h1>";
    html += "<p>The device will restart with the new settings.</p>";
    html += "</body></html>";
    server.send(200, "text/html", html);

    delay(2000);
    ESP.restart();  // Restart the ESP8266 to apply the new settings
}



void handleNotFound() {
    server.send(404, "text/plain", "404: Not Found");
}

void saveSettings() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(0, settings);
    EEPROM.put(sizeof(settings), ssid);
    EEPROM.put(sizeof(settings) + sizeof(ssid), pass);
    EEPROM.commit();
}

void loadSettings() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(0, settings);
    EEPROM.get(sizeof(settings), ssid);
    EEPROM.get(sizeof(settings) + sizeof(ssid), pass);
    EEPROM.end();
}

// End of Part 2
//************************ MAX7219 Functions *****************************************************************
const unsigned short InitArray[7][2] = { { 0x0C, 0x00 },    // display off
        { 0x00, 0xFF },    // no LED test
        { 0x09, 0x00 },    // BCD off
        { 0x0F, 0x00 },    // normal operation
        { 0x0B, 0x07 },    // scan limit
        { 0x0A, 0x04 },    // brightness
        { 0x0C, 0x01 }     // display on
};

void max7219_init()  // Initialize all MAX7219
{
    unsigned short i, j;
    for (i = 0; i < 7; i++) {
        digitalWrite(CS_PIN, LOW);
        delayMicroseconds(1);
        for (j = 0; j < MATRIX_COUNT; j++) {
            SPI.write(InitArray[i][0]);  // Register
            SPI.write(InitArray[i][1]);  // Value
        }
        digitalWrite(CS_PIN, HIGH);
    }
}

void max7219_set_brightness(unsigned short br)  // Set brightness of MAX7219
{
    unsigned short j;
    if (br < 16) {
        digitalWrite(CS_PIN, LOW);
        delayMicroseconds(1);
        for (j = 0; j < MATRIX_COUNT; j++) {
            SPI.write(0x0A);  // Register
            SPI.write(br);    // Value
        }
        digitalWrite(CS_PIN, HIGH);
    }
}

//**************************************************************************************************
void helperArray_init(void)  // Helper array initialization
{
    unsigned short i, j, k;
    j = 0;
    k = 0;
    for (i = 0; i < MATRIX_COUNT * 8; i++) {
        helperArrayPos[i] = (1 << j);   // Bitmask
        helperArrayMAX[i] = k;
        j++;
        if (j > 7) {
            j = 0;
            k++;
        }
    }
}

//*********************************** Clear Display ***************************************************************
void clear_Display()   // Clear all matrices
{
    unsigned short i, j;
    for (i = 0; i < 8; i++)     // 8 rows
    {
        digitalWrite(CS_PIN, LOW);
        delayMicroseconds(1);
        for (j = MATRIX_COUNT; j > 0; j--) {
            LEDMatrix[j - 1][i] = 0;       // Clear LEDMatrix
            SPI.write(i + 1);              // Current row
            SPI.write(LEDMatrix[j - 1][i]);
        }
        digitalWrite(CS_PIN, HIGH);
    }
}

//************************************* Rotate 90 Degrees ********************************************************************
void rotate_90() // For generic displays
{
    for (uint8_t k = MATRIX_COUNT; k > 0; k--) {

        uint8_t i, j, m, imask, jmask;
        uint8_t tmp[8]={0,0,0,0,0,0,0,0};
        for (  i = 0, imask = 0x01; i < 8; i++, imask <<= 1) {
          for (j = 0, jmask = 0x01; j < 8; j++, jmask <<= 1) {
            if (LEDMatrix[k-1][i] & jmask) {
              tmp[j] |= imask;
            }
          }
        }
        for(m=0; m<8; m++){
            LEDMatrix[k-1][m]=tmp[m];
        }
    }
}

//***************************** Refresh Display *********************************************************************
void refresh_display() // Update LEDMatrix
{
    unsigned short i, j;

#ifdef ROTATE_90
    rotate_90();
#endif

    for (i = 0; i < 8; i++)     // 8 rows
    {
        digitalWrite(CS_PIN, LOW);
        delayMicroseconds(1);
        for (j = 1; j <= MATRIX_COUNT; j++) 
        {
            SPI.write(i + 1);  // Current row
            if(reverseDisplay){
              SPI.setBitOrder(LSBFIRST);      // Bit order for reverse columns
              SPI.write(LEDMatrix[4-j][7-i]);
              SPI.setBitOrder(MSBFIRST);      // Reset bit order
            }
            else {
#ifdef REVERSE_HORIZONTAL    // Reverse horizontally
            SPI.setBitOrder(LSBFIRST);      // Bit order for reverse columns
#endif

#ifdef REVERSE_VERTICAL     // Reverse vertically
            SPI.write(LEDMatrix[4-j][7-i]);
#else
            SPI.write(LEDMatrix[j - 1][i]);
#endif

#ifdef REVERSE_HORIZONTAL
            SPI.setBitOrder(MSBFIRST);      // Reset bit order
#endif
        }}
        digitalWrite(CS_PIN, HIGH);
    }
}

//**************************************************************************************************
void charToMatrix(unsigned short ch, int PosX, short PosY) { // Characters into LEDMatrix
    int i, j, k, l, m, o1, o2, o3, o4;  // In LEDMatrix
    PosX++;
    k = ch - 32;                        // ASCII position in font
    if ((k >= 0) && (k < 96))           // Character found in font?
    {
        o4 = font1[k][0];                 // Character width
        o3 = 1 << (o4 - 2);
        for (i = 0; i < o4; i++) {
            if (((PosX - i <= maxPosX) && (PosX - i >= 0))
                    && ((PosY > -8) && (PosY < 8))) // Within matrix?
            {
                o1 = helperArrayPos[PosX - i];
                o2 = helperArrayMAX[PosX - i];
                for (j = 0; j < 8; j++) {
                    if (((PosY >= 0) && (PosY <= j)) || ((PosY < 0) && (j < PosY + 8))) // Scroll vertical
                    {
                        l = font1[k][j + 1];
                        m = (l & (o3 >> i));  // E.g., o4=7  0zzzzz0, o4=4  0zz0
                        if (m > 0)
                            LEDMatrix[o2][j - PosY] = LEDMatrix[o2][j - PosY] | (o1);  // Set point
                        else
                            LEDMatrix[o2][j - PosY] = LEDMatrix[o2][j - PosY] & (~o1); // Clear point
                    }
                }
            }
        }
    }
}

void charToMatrix2(unsigned short ch, int PosX, short PosY) { // Characters into LEDMatrix
    int i, j, k, l, m, o1, o2, o3, o4;  // In LEDMatrix
    PosX++;
    k = ch - 32;                        // ASCII position in font
    if ((k >= 0) && (k < 96))           // Character found in font?
    {
        o4 = font2[k][0];                 // Character width
        o3 = 1 << (o4 - 2);
        for (i = 0; i < o4; i++) {
            if (((PosX - i <= maxPosX) && (PosX - i >= 0))
                    && ((PosY > -8) && (PosY < 8))) // Within matrix?
            {
                o1 = helperArrayPos[PosX - i];
                o2 = helperArrayMAX[PosX - i];
                for (j = 0; j < 8; j++) {
                    if (((PosY >= 0) && (PosY <= j)) || ((PosY < 0) && (j < PosY + 8))) // Scroll vertical
                    {
                        l = font2[k][j + 1];
                        m = (l & (o3 >> i));  // E.g., o4=7  0zzzzz0, o4=4  0zz0
                        if (m > 0)
                            LEDMatrix[o2][j - PosY] = LEDMatrix[o2][j - PosY] | (o1);  // Set point
                        else
                            LEDMatrix[o2][j - PosY] = LEDMatrix[o2][j - PosY] & (~o1); // Clear point
                    }
                }
            }
        }
    }
}

// End of Part 3

//************************** Scroll Text ******************************
void scrollText(const char* text) {
    int len = strlen(text);
    for (int i = 0; i < len * 6 + MATRIX_COUNT * 8; i++) {
        for (int j = 0; j < len; j++) {
            int charPos = i - j * 6;
            if (charPos >= 0 && charPos < MATRIX_COUNT * 8) {
                charToMatrix(text[j], charPos, 0);  // Adjust character position
            }
        }
        refresh_display();
        delay(100);  // Adjust scrolling speed
        clear_Display();
    }
}

//************************** RTC Functions *****************************************************************

// BCD Code
unsigned char dec2bcd(unsigned char x) { // Value 0...99
    unsigned char z, e, r;
    e = x % 10;
    z = x / 10;
    z = z << 4;
    r = e | z;
    return (r);
}

unsigned char bcd2dec(unsigned char x) { // Value 0...99
    int z, e;
    e = x & 0x0F;
    z = x & 0xF0;
    z = z >> 4;
    z = z * 10;
    return (z + e);
}

// RTC I2C Code
unsigned char rtcRead(unsigned char regaddress) {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(regaddress);
    Wire.endTransmission();
    Wire.requestFrom((unsigned char) DS3231_ADDRESS, (unsigned char) 1);
    return (Wire.read());
}

void rtcWrite(unsigned char regaddress, unsigned char value) {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(regaddress);
    Wire.write(value);
    Wire.endTransmission();
}

// RTC Read/Write Functions
unsigned char rtcSecond() {
    return (bcd2dec(rtcRead(secondREG)));
}

unsigned char rtcMinute() {
    return (bcd2dec(rtcRead(minuteREG)));
}

unsigned char rtcHour() {
    return (bcd2dec(rtcRead(hourREG)));
}

unsigned char rtcDayOfWeek() {
    return (bcd2dec(rtcRead(dayOfWeekREG)));
}

unsigned char rtcDay() {
    return (bcd2dec(rtcRead(dateREG)));
}

unsigned char rtcMonth() {
    return (bcd2dec(rtcRead(monthREG)));
}

unsigned char rtcYear() {
    return (bcd2dec(rtcRead(yearREG)));
}

void rtcSetSecond(unsigned char sec) {
    rtcWrite(secondREG, (dec2bcd(sec)));
}

void rtcSetMinute(unsigned char min) {
    rtcWrite(minuteREG, (dec2bcd(min)));
}

void rtcSetHour(unsigned char hour) {
    rtcWrite(hourREG, (dec2bcd(hour)));
}

void rtcSetDayOfWeek(unsigned char dow) {
    rtcWrite(dayOfWeekREG, (dec2bcd(dow)));
}

void rtcSetDay(unsigned char day) {
    rtcWrite(dateREG, (dec2bcd(day)));
}

void rtcSetMonth(unsigned char month) {
    rtcWrite(monthREG, (dec2bcd(month)));
}

void rtcSetYear(unsigned char year) {
    rtcWrite(yearREG, (dec2bcd(year)));
}

// Set RTC with tm structure
void rtcSet(tm* tt) {
    rtcSetSecond((unsigned char) tt->tm_sec);
    rtcSetMinute((unsigned char) tt->tm_min);
    rtcSetHour((unsigned char) tt->tm_hour);
    rtcSetDay((unsigned char) tt->tm_mday);
    rtcSetMonth((unsigned char) tt->tm_mon + 1);
    rtcSetYear((unsigned char) tt->tm_year - 100);
    if (tt->tm_wday == 0)
    {
      rtcSetDayOfWeek(7);
    }
    else
      rtcSetDayOfWeek((unsigned char) tt->tm_wday);
}

// Read RTC and update currentTime structure
void rtcToTime() {
    unsigned short Year, Day, Month, DayOfWeek, Hour, Minute, Second;

    Year = rtcYear();
    if (Year > 99)
        Year = 0;
    Month = rtcMonth();
    if (Month > 12)
        Month = 0;
    Day = rtcDay();
    if (Day > 31)
        Day = 0;
    DayOfWeek = rtcDayOfWeek() - 1;
    if (DayOfWeek == 7)
        DayOfWeek = 0;
    Hour = rtcHour();
    if (Hour > 23)
        Hour = 0;
    Minute = rtcMinute();
    if (Minute > 59)
        Minute = 0;
    Second = rtcSecond();
    if (Second > 59)
        Second = 0;
    
    currentTime.dayOfWeek = DayOfWeek; // Sunday=0, Monday=1, etc.
    currentTime.sec1 = Second % 10;
    currentTime.sec2 = Second / 10;
    currentTime.sec12 = Second;
    currentTime.min1 = Minute % 10;
    currentTime.min2 = Minute / 10;
    currentTime.min12 = Minute;
    currentTime.hour1 = Hour % 10;
    currentTime.hour2 = Hour / 10;
    currentTime.hour12 = Hour;
    currentTime.day12 = Day;
    currentTime.day1 = Day % 10;
    currentTime.day2 = Day / 10;
    currentTime.month12 = Month;
    currentTime.month1 = Month % 10;
    currentTime.month2 = Month / 10;
    currentTime.year12 = Year;
    currentTime.year1 = Year % 10;
    currentTime.year2 = Year / 10;
}

// NTP Functions
tm* connectNTP() { // Return *tm if successful, else return NULL
    WiFi.hostByName(ntpServerName, timeServerIP);
    Serial.println(timeServerIP);
    Serial.println("Sending NTP packet...");
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;
    udp.beginPacket(timeServerIP, 123); // NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
    delay(1000);                 // Wait for a reply
    int cb = udp.parsePacket();
    if (cb == 0) {
        Serial.println("No NTP response.");
        return NULL;
    }
    udp.read(packetBuffer, NTP_PACKET_SIZE); // Read the packet into the buffer
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    const unsigned long seventyYears = 2208988800UL;
    epoch = secsSince1900 - seventyYears + settings.timeOffset; // Apply time offset from settings
    time_t t;
    t = epoch;
    tm* tt;
    tt = localtime(&t);
    Serial.println(epoch);
    Serial.println(asctime(tt)); 
    return (tt);
}

// End of Part 4
//**************************************************************************************************
// The setup function is called once at startup of the sketch
void setup() {
    // Add your initialization code here
    pinMode(16, INPUT);
    pinMode(CS_PIN, OUTPUT);
    digitalWrite(CS_PIN, HIGH);
    Serial.begin(115200);
    SPI.begin();
    helperArray_init();
    max7219_init();
    loadSettings();
    // Set brightness from settings
    max7219_set_brightness(settings.brightness);
    // Initialize RTC
    Wire.begin(SDA_PIN, SCL_PIN);
    rtcWrite(controlREG, 0x00);
    clear_Display();
    refresh_display(); // Update LEDMatrix
    ticker.attach(0.05, timer50ms);    // Every 50 milliseconds
    setupWiFi();
    // Get network time
    tt = connectNTP();
    if (tt != NULL)   // If network time is obtained, write to RTC
        rtcSet(tt);
    else
        Serial.println("No NTP time received");
}

// Timer function for 50ms intervals
void timer50ms() {
    static unsigned int cnt50ms = 0;
    flagTicker50ms = true;
    cnt50ms++;
    if (cnt50ms == 20) {
        flagTicker1s = true; // 1 second
        cnt50ms = 0;
    }
}

// End of Part 5
//**************************************************************************************************
// The loop function is called in an endless loop
void loop() {
    // Add your repeated code here
    unsigned int sec1 = 0, sec2 = 0, min1 = 0, min2 = 0, hour1 = 0, hour2 = 0;
    unsigned int sec11 = 0, sec12 = 0, sec21 = 0, sec22 = 0;
    unsigned int min11 = 0, min12 = 0, min21 = 0, min22 = 0;
    unsigned int hour11 = 0, hour12 = 0, hour21 = 0, hour22 = 0;
    signed int x = 0;
    signed int y = 0, y1 = 0, y2 = 0, y3=0;
    bool updown = false;
    unsigned int sc1 = 0, sc2 = 0, sc3 = 0, sc4 = 0, sc5 = 0, sc6 = 0;
    bool flagScrollEndY = false;
    unsigned int flagScrollX = false;

    z_PosX = maxPosX;
    d_PosX = -8;

    refresh_display();
    updown = true;
    if (updown == false) {
        y2 = -9;
        y1 = 8;
    }
    if (updown == true) { // Scroll up to down
        y2 = 8;
        y1 = -8;
    }
    while (true) {
        yield();  // In ESP8266, call yield occasionally to prevent the watchdog timer from resetting the ESP8266.

        // Process incoming UDP packets
        int packetSize = udp.parsePacket();
        if (packetSize) {
            // We have received a packet.
            char packetBuffer[255];
            int len = udp.read(packetBuffer, 255);
            if (len > 0) {
                packetBuffer[len] = 0;  // Null-terminate the string.
            }
            // Check if the packet is at least 15 characters long
            if (len >= 15) {
                if (strncmp(packetBuffer, "IDCT:", 5) == 0) {
                    // Parse the countdown time
                    char sign = packetBuffer[5];
                    char timeStr[7]; // 6 chars + null terminator.
                    memcpy(timeStr, &packetBuffer[6], 6);
                    timeStr[6] = 0; // Null-terminate.
                    long timeRemaining = atol(timeStr);
                    if (sign == '-') {
                        timeRemaining = -timeRemaining;
                    }
                    // Store the timeRemaining and set a flag.
                    countdownTime = timeRemaining;
                    countdownReceivedMillis = millis();
                    countdownAvailable = true;
                }
            }
        }

        // Check if countdownAvailable is true and if the last packet was received within 2 seconds
        if (countdownAvailable && (millis() - countdownReceivedMillis > 2000)) {
            countdownAvailable = false;
        }

        if (currentTime.hour12 == 0 && currentTime.min12 == 20 && currentTime.sec12 == 0 ) // Synchronize RTC every day at 00:20:00
        { 
            clear_Display();
            delay(500);
            ESP.restart();
        }
        if (flagTicker1s == true)        // Flag for 1 second
        {
            if(!digitalRead(16)){reverseDisplay=1;}else reverseDisplay=0;

            if (countdownAvailable) {
                // We have a countdown time to display
                if (countdownTime > 0) {
                    countdownTime--;
                } else if (countdownTime < 0) {
                    countdownTime++;
                }
                // Convert countdownTime to hours, minutes, seconds
                long timeRemaining = countdownTime;
                if (timeRemaining < 0) {
                    countdownIsNegative = true;
                    timeRemaining = -timeRemaining;
                } else {
                    countdownIsNegative = false;
                }
                int hours = timeRemaining / 3600;
                int minutes = (timeRemaining % 3600) / 60;
                int seconds = timeRemaining % 60;

                // Adjust display based on settings.displayModeHHMMSS
                if (settings.displayModeHHMMSS) {
                    // Display HH:MM:SS
                    sec1 = seconds % 10;
                    sec2 = seconds / 10;
                    min1 = minutes % 10;
                    min2 = minutes / 10;
                    hour1 = hours % 10;
                    hour2 = hours / 10;
                } else {
                    // Display MM:SS
                    sec1 = seconds % 10;
                    sec2 = seconds / 10;
                    min1 = minutes % 10;
                    min2 = minutes / 10;
                    hour1 = 0;
                    hour2 = 0;
                }

                // Update other variables as in the existing code
                sec11 = sec12;
                sec12 = sec1;
                sec21 = sec22;
                sec22 = sec2;
                min11 = min12;
                min12 = min1;
                min21 = min22;
                min22 = min2;
                hour11 = hour12;
                hour12 = hour1;
                hour21 = hour22;
                hour22 = hour2;

                flagTicker1s = false;
                if (countdownTime == 45)   // Scroll when time is 45
                    flagScrollX = true;    // Scroll switch
            } else {
                // No countdown available, use RTC time
                rtcToTime();
                sec1 = currentTime.sec1;
                sec2 = currentTime.sec2;
                min1 = currentTime.min1;
                min2 = currentTime.min2;
                hour1 = currentTime.hour1;
                hour2 = currentTime.hour2;

                sec11 = sec12;
                sec12 = sec1;
                sec21 = sec22;
                sec22 = sec2;
                min11 = min12;
                min12 = min1;
                min21 = min22;
                min22 = min2;
                hour11 = hour12;
                hour12 = hour1;
                hour21 = hour22;
                hour22 = hour2;
                flagTicker1s = false;
                if (currentTime.sec12 == 45)   // Scroll when second is 45
                    flagScrollX = true;        // Scroll switch
            }
        }

        if (flagTicker50ms == true) { // This part executes every 50ms to control scroll speed
            flagTicker50ms = false;
            if (flagScrollX == true) {
                z_PosX++;
                d_PosX++;
                if (d_PosX == 101)
                    z_PosX = 0;
                if (z_PosX == maxPosX) {
                    flagScrollX = false;
                    d_PosX = -8;
                }
            }

            // Determine positions based on display mode
            int posSec1, posSec2, posMin1, posMin2, posHour1, posHour2, posColon;

            if (settings.displayModeHHMMSS) {
                // HH:MM:SS mode
                posSec1 = z_PosX - 27;
                posSec2 = z_PosX - 23;
                posMin1 = z_PosX - 18;
                posMin2 = z_PosX - 13;
                posHour1 = z_PosX - 4;
                posHour2 = z_PosX + 1;
                posColon = z_PosX - 10 + x;
            } else {
                // MM:SS mode
                // Adjust positions to center MM:SS on the display
                posSec1 = z_PosX - 24;
                posSec2 = z_PosX - 19;
                posMin1 = z_PosX - 8;
                posMin2 = z_PosX - 3;
                posColon = z_PosX - 15 + x; // Adjust as needed
            }

            // Display seconds digits
            if (settings.displayModeHHMMSS) {
                // Use font2 for seconds in HH:MM:SS mode
                charToMatrix2(48 + sec1, posSec1, 0);
                charToMatrix2(48 + sec2, posSec2, 0);
            } else {
                // Use font1 for seconds in MM:SS mode
                charToMatrix(48 + sec1, posSec1, 0);
                charToMatrix(48 + sec2, posSec2, 0);
            }

            // Display minutes digits using font1
            charToMatrix(48 + min1, posMin1, 0);
            charToMatrix(48 + min2, posMin2, 0);

            // Display colon
            charToMatrix(':', posColon, 0);

            // Display hours digits if in HH:MM:SS mode
            if (settings.displayModeHHMMSS) {
                charToMatrix(48 + hour1, posHour1, 0);
                if (countdownIsNegative) {
                    // Display '-' if time is negative
                    charToMatrix('-', posHour2, 0);
                } else {
                    charToMatrix(48 + hour2, posHour2, 0);
                }
            }

            // Display scrolling date and info only when countdown is not available
            if (settings.dateScrollEnabled && !countdownAvailable) {
                charToMatrix(' ', d_PosX+5, 0);        // Day of the week
                charToMatrix(dayOfWeekArray[currentTime.dayOfWeek][0], d_PosX - 1, 0);
                charToMatrix(dayOfWeekArray[currentTime.dayOfWeek][1], d_PosX - 7, 0);
                charToMatrix(dayOfWeekArray[currentTime.dayOfWeek][2], d_PosX - 13, 0);
                charToMatrix(dayOfWeekArray[currentTime.dayOfWeek][3], d_PosX - 19, 0);
                charToMatrix(48 + currentTime.day2, d_PosX - 24, 0);           // Day
                charToMatrix(48 + currentTime.day1, d_PosX - 30, 0);
                charToMatrix(monthArray[currentTime.month12 - 1][0], d_PosX - 39, 0); // Month
                charToMatrix(monthArray[currentTime.month12 - 1][1], d_PosX - 43, 0);
                charToMatrix(monthArray[currentTime.month12 - 1][2], d_PosX - 49, 0);
                charToMatrix(monthArray[currentTime.month12 - 1][3], d_PosX - 55, 0);
                charToMatrix(monthArray[currentTime.month12 - 1][4], d_PosX - 61, 0);
                charToMatrix('2', d_PosX - 68, 0);                     // Year
                charToMatrix('0', d_PosX - 74, 0);
                charToMatrix(48 + currentTime.year2, d_PosX - 80, 0);
                charToMatrix(48 + currentTime.year1, d_PosX - 86, 0);
            }

            refresh_display(); // Every 50ms
            if (flagScrollEndY == true) {
                flagScrollEndY = false;
            }
        }
    // Handle the web server clients, making sure it's accessible in both AP and STA modes
    server.handleClient();

    }  // End of while(true)
}

// End of Part 6
