// This code was made for the TTGO LoRa32, but should work with the rights PINs set on any Arduino with the necessary LoRa hardware

#include "FastLED.h"
#include "SSD1306.h"
#include "SPI.h"
#include "LoRa.h"

#define SCK 5 // GPIO5 - SX1278's SCK
#define MISO 19 // GPIO19 - SX1278's MISO
#define MOSI 27 // GPIO27 - SX1278's MOSI
#define SS 18 // GPIO18 - SX1278's CS
#define RST 14 // GPIO14 - SX1278's RESET
#define DI0 26 // GPIO26 - SX1278's IRQ (interrupt request)
#define BAND 868E6 // 915E6

#define LED_PIN 21 // Neopixel Data Out Pin
#define LED_TYPE WS2812
#define COLOR_ORDER GRB
#define LED_COUNT 5 // Number of LEDs
#define BRIGHTNESS 255

#define PRG 0 // Onboard button pin
#define LED 2 // Onboard LED
#define SCRWIDTH 128 // Width of onboard OLED
#define SCRHEIGHT 64
#define YOFFSET 1 // Used to give room for the animations

String outgoing; // outgoing message

byte msgCount = 0; // count of outgoing messages
byte localAddress = 0xBB; // address of this device
byte destination = 0xFF; // destination to send to
long lastSendTime = 0; // last send time
int interval = 10000; // interval between sends

int time_offset = 0; // Used for calculating sync offset
int remote_millis = 0; // Remote device current time
bool transmitter = true; // Device defaults to transmitter mode - automatically switches to receiver upon first received message
String last_sent; // Keep track of the most recent broadcasted message
String last_received; // Keep track of the most recent received message

SSD1306 display(0x3c, 4, 15); // Define display
int yvals[128]; // Useful for plotting data
int line = 0; // Used for keeping track of which line of text you're on
int lnspace = 10; // Line spacing for text
int txtheight = 10; // Used to separately calculate screen overflow

bool led_enabled = false; // Toggle for Neopixels
CRGB leds[LED_COUNT]; // Store LED values

bool booted = false; // Variable to keep track of setup() state

bool txt(String input = "----", bool scrupdate = false, int custom_line = -1) { // Draw a string with automatic line arrangement
  int txty = line*lnspace+YOFFSET;

  if (txty+txtheight > SCRHEIGHT) { return false; }
  
  display.drawString(0, txty, input);
  if (scrupdate) {
    display.display();
  }
  
  if (custom_line == -1) {
    line++; // Increment line
  }
  
  return true;
}

void cldsp() { // Clear display and reset line
  display.clear();
  line = 0;
}

void setup() {
  // put your setup code here, to run once:
  xTaskCreate( // Scheduled loop for handling LED updates https://techtutorialsx.com/2017/05/06/esp32-arduino-creating-a-task/
    LEDBlink, // Function
    "LEDBlink", // Task name
    10000, // Stack size
    NULL, // Parameter passed
    1, // Priority
    NULL // Handle
  );
  
  xTaskCreate( // Scheduled loop for handling button input
    ButtonPoll,
    "ButtonPoll",
    10000,
    NULL,
    1,
    NULL
  );

  pinMode(16, OUTPUT); // OLED enable
  pinMode(LED, OUTPUT); // Onboard LED
  pinMode(PRG, INPUT); // Onboard button

  digitalWrite(16, LOW); // set GPIO16 low to reset OLED
  delay(50);
  digitalWrite(16, HIGH); // while OLED is running, GPIO16 must go high

  // Initialize display
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  // Loading animation
  int dot_delay = 50;
  for (int i = 0; i < 60; i++) {
    String dots = "";
    for (int j = 0; j < i%4; j++) {
      dots += ".";
    }
    
    String lorasync = "LoRa Sync Test" + dots;
    txt(lorasync);
    
    txt("PRG = Receive Mode");

    if (!transmitter) {
      txt("Receive Mode enabled");
    }

    display.display();
    
    delay(dot_delay);
    cldsp();
  }

  // Enable LoRa
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(868)) {
    txt("Starting LoRa failed!", true);
    while(1);
  }

  // Initialize FastLED
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, LED_COUNT)
  .setCorrection(TypicalLEDStrip)
  .setDither(BRIGHTNESS < 255);

  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  if (transmitter) { // Send initial sync message
    String message = String(millis());
    sendMessage(message);
    lastSendTime = millis();
  }
  
  booted = true;
}

void loop() {
  cldsp();
  int x_prog = int(((millis() + time_offset) % 1000 / 1000.0) * 128); // Used for calculating the x position of the moving part on the sync animations
  
  if (transmitter) {
    txt("Transmitting...");

    if (millis() % 2000 < 1000) { // Animate using millis as reference
      display.drawLine(0, 1, x_prog, 1);
    } else { // Even second
      display.drawLine(x_prog, 1, SCRWIDTH-1, 1);
    }

    if (millis() - lastSendTime > interval) { // Broadcast millis every interval
      String message = String(millis());
      sendMessage(message);
      lastSendTime = millis();
    }
    
  } else if (!time_offset) { // Waiting for signal
    txt("Receive Mode");
    txt("Waiting for signal");

    // Waiting for signal animation
    int dashes = 10;
    int dash_space = int((SCRWIDTH-1) / dashes);
    int dash_width = (SCRWIDTH-1) / dashes / 2;
    int dash_offset = int((-millis() * .02)) % dash_space;

    for (int i = 0; i < dashes; i++) {
      display.drawLine(dash_space * i + dash_offset, 1, // x1, y1
      dash_space * i + dash_width + dash_offset, 1); // x2, y2
    }
    
  } else { // Signal received
    txt("Receiving...");

    if ((millis() + time_offset) % 2000 < 1000) { // Opposite behavior of transmitter
      display.drawLine(SCRWIDTH-1, 1, SCRWIDTH-1 - x_prog, 1);
    } else {
      display.drawLine(SCRWIDTH-1 - x_prog, 1, 0, 1);
    }
  }

  if (led_enabled) { // Display LED status
    txt("LEDs: ON");
  } else {
    txt("LEDs: OFF");
  }

  txt("PRG = LED Toggle");

  String tx = "TX: " + last_sent;
  String rx = "RX: " + last_received;
  txt(tx);
  txt(rx);

  display.display();
  onReceive(LoRa.parsePacket());

  delay(5); // For general loop stability
}
