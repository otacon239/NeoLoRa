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
#define BAND 915E6 // 915E6

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
int interval = 5000; // interval between sends

int time_offset = 0; // Used for calculating sync offset
int remote_millis = 0; // Remote device current time
bool transmitter = false; // Device defaults to receive mode - PRG button must be pressed during boot to become the new transmitter
String last_sent = ""; // Keep track of the most recent broadcasted message
String last_received = ""; // Keep track of the most recent received message
float idle_timeout = 30000; // How long, in ms, before the device defaults to transmit mode, assuming it won't get a signal
int last_rx_time = 0;  // How long since the last message

SSD1306 display(0x3c, 4, 15); // Define display
int yvals[128]; // Useful for plotting data - currently unused
int line = 0; // Used for keeping track of which line of text you're on
int lnspace = 10; // Line spacing for text
int txtheight = 10; // Used to separately calculate screen overflow
int txtoffset = 0; // Offsets the text on y axis by this many pixels - can be adjusted per-line as needed

bool led_enabled = false; // Toggle for Neopixels
CRGB leds[LED_COUNT]; // Store LED values

bool booted = false; // Variable to keep track of setup() state

bool txt(String input = "", bool scrupdate = false, int custom_line = -1) { // Draw a string with automatic line arrangement
  int txty = line * lnspace + YOFFSET;

  if (input == "---") {
    txty++;
    display.drawLine(0, txty, SCRWIDTH - 1, txty);
    txtoffset += 2;
    return true;
  }

  if (txty + txtheight > SCRHEIGHT) {
    return false;
  }

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
    for (int j = 0; j < i % 4; j++) {
      dots += ".";
    }

    String lorasync = "LoRa Sync Test" + dots;
    txt(lorasync);

    txt("PRG = Transmit Mode");
    txt("---");

    if (transmitter) {
      txt("Transmit Mode enabled");
    } else {
      txt("Transmit Mode will start");
      txt("automatically in " + String(int(idle_timeout/1000)) + "s");
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
    while (1);
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
  
  last_rx_time = millis() + time_offset - remote_millis; // Calculate how long since the last message
  if (!transmitter) {
    if (last_rx_time > idle_timeout) {
      transmitter = true;
    } else if (last_rx_time > idle_timeout * .5) {
      txt("TX timeout in " + String(int((idle_timeout - last_rx_time) / 1000)));
    }
  }

  int x_prog;

  if (transmitter) {
    x_prog = int(((millis() % 1000) / 1000.0) * SCRWIDTH); // Used for calculating the x position of the moving part on the sync animations
    
    txt("Transmitting...");
    
    if (millis() % 2000 < 1000) { // Animate using millis as reference
      display.drawLine(0, 1, x_prog, 1);
    } else { // Even other second
      display.drawLine(x_prog, 1, SCRWIDTH - 1, 1);
    }

    if (millis() - lastSendTime > interval) { // Broadcast millis every interval
      String message = String(millis());
      sendMessage(message);
      lastSendTime = millis();
    }

  } else if (!time_offset) { // Waiting for signal
    txt("Waiting for signal...");

    // Waiting for signal animation
    int dashes = 10;
    int dash_space = int((SCRWIDTH - 1) / dashes);
    int dash_width = (SCRWIDTH - 1) / dashes / 2;
    int dash_offset = int((-millis() * .02)) % dash_space;

    for (int i = 0; i < dashes; i++) {
      display.drawLine(dash_space * i + dash_offset, 1, // x1, y1
                       dash_space * i + dash_width + dash_offset, 1); // x2, y2
    }

  } else { // Signal received
    x_prog = int((((millis() + time_offset) % 1000) / 1000.0) * SCRWIDTH); // Used for calculating the x position of the moving part on the sync animations
    
    txt("Receiving...");

    if ((millis() + time_offset) % 2000 < 1000) { // Opposite behavior of transmitter
      display.drawLine(SCRWIDTH - 1, 1, SCRWIDTH - 1 - x_prog, 1);
    } else {
      display.drawLine(SCRWIDTH - 1 - x_prog, 1, 0, 1);
    }
  }

  txt("---");

  if (led_enabled) { // Display LED status
    txt("LEDs: ON");
  } else {
    txt("LEDs: OFF");
  }

  txt("PRG = LED Toggle");

  txt("---");

  if (last_sent != "")  {
    String tx = "TX: " + last_sent;
    txt(tx);
  }

  if (last_received != "") {
    String rx = "RX: " + last_received + " (" + String(LoRa.packetRssi(), DEC) + " dB)";
    txt(rx);
  }

  display.display();
  onReceive(LoRa.parsePacket());

  delay(5); // For general loop stability
}
