// SPDX-License-Identifier: GPL-2.0
#include <Arduino.h>
#include <MCP23S17.h>
#include <RingBuf.h>
#include <SPI.h>

#ifndef ARDUINO_AVR_NANO
#error "This sketch is only for Arduion NANO"
#endif

#define PIN_MCP23S17_nSS  10

#define PORT_VIDEO        0     // of MCP23S17
#define PIN_VIDEO_D0      0     // of MCP23S17
#define PIN_VIDEO_D1      1     // of MCP23S17
#define PIN_VIDEO_D2      2     // of MCP23S17
#define PIN_VIDEO_D3      3     // of MCP23S17
#define PIN_VIDEO_D4      4     // of MCP23S17
#define PIN_VIDEO_D5      5     // of MCP23S17
#define PIN_VIDEO_D6      6     // of MCP23S17
#define PIN_VIDEO_nRDA    5     // of Arduino
#define PIN_VIDEO_DA      3     // of Arduino

#define PORT_KBD          1     // of MCP23S17
#define PIN_KBD_D0        8     // of MCP23S17
#define PIN_KBD_D1        9     // of MCP23S17
#define PIN_KBD_D2        10    // of MCP23S17
#define PIN_KBD_D3        11    // of MCP23S17
#define PIN_KBD_D4        12    // of MCP23S17
#define PIN_KBD_D5        13    // of MCP23S17
#define PIN_KBD_D6        14    // of MCP23S17
#define PIN_KBD_DA        15    // of MCP23S17
#define PIN_KBD_CLR       2     // of Arduino
#define PIN_KBD_STR       4     // of Arduino

static MCP23S17 mcp23s17(&SPI, PIN_MCP23S17_nSS, 0);

static RingBuf<int, 512> serial_buf;

static void (*kbd_state_machine)(void);
static bool kbd_clr_flag;

static void __kbd_clr_flag_isr(void) {
  kbd_clr_flag = true;
}

static void __kbd_state_idle(void) {
  if (serial_buf.isEmpty())
    return;

  kbd_state_machine = __kbd_state_write_data;
}

static void __kbd_state_write_data(void) {
  int c;

  serial_buf.pop(c);
  mcp23s17.writePort(PORT_KBD, c | 0x80);
  digitalWrite(PIN_KBD_STR, HIGH);

  kbd_state_machine = __kbd_state_wait_clr_interrupt;
}

static void __kbd_state_wait_clr_interrupt(void) {
  if (!kbd_clr_flag)
    return;

  digitalWrite(PIN_KBD_STR, LOW);

  kbd_clr_flag = false;
  kbd_state_machine = __kbd_state_wait_until_clr_low;
}

static void __kbd_state_wait_until_clr_low(void) {
  if (digitalRead(PIN_KBD_CLR) != LOW)
    return;

  kbd_state_machine = __kbd_state_idle;
}

static void __setup_mcp23s17(void) {
  mcp23s17.begin();

  for (int pin = 0; pin < 16; pin++)
    mcp23s17.pinMode(pin, INPUT_PULLUP);
}

static void __setup_pin_kbd(void) {
  pinMode(PIN_KBD_CLR, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_KBD_CLR), __kbd_clr_flag_isr, HIGH);

  pinMode(PIN_KBD_STR, OUTPUT);
  digitalWrite(PIN_KBD_STR, LOW);

  for (int pin = PIN_KBD_D0; pin <= PIN_KBD_DA; pin++)
    mcp23s17.pinMode(pin, OUTPUT);

  mcp23s17.writePort(PORT_KBD, 0x0);

  kbd_state_machine = __kbd_state_idle;
}

static void __setup_pin_video(void) {
  pinMode(PIN_VIDEO_DA, INPUT);

  pinMode(PIN_VIDEO_nRDA, OUTPUT);
  digitalWrite(PIN_VIDEO_nRDA, HIGH);

  for (int pin = PIN_VIDEO_D0; pin <= PIN_VIDEO_D6; pin++)
    mcp23s17.pinMode(pin, INPUT);
}

static inline void input_from_kbd(void) {
  while (Serial.available() && !serial_buf.isFull()) {
    int c = toupper(Serial.read());

    if (c >= 0x80)  // ignore ASCII Extended Characters
      continue;

    serial_buf.push(c);
  }
}

static inline void output_to_mc6820(void) {
  kbd_state_machine();
}

static inline void ____tty_println(void) {
  while (!Serial.println()) ;
}

static inline void ____tty_putchar(int c) {
  switch (c) {
    case 0x08:  /* 'BS' */
    case 0x09:  /* 'TAB' */
    case 0x12:  /* 'LF' */
    case 0x13:  /* 'VT' */
    case 0x20 ... 0x7E: /* 'space' to '~' */
      break;
    default:
      return;
  }

  while (!Serial.print((char)c)) ;
}

static inline int __tty_putchar(int c) {
  if (c == '\r')
    ____tty_println();
  else
    ____tty_putchar(c);

  return c;
}

static inline int __mcp23s17_getchar(void) {
  return mcp23s17.readPort(PORT_VIDEO) & 0x7F;
}

static inline void output_to_video(void) {
  digitalWrite(PIN_VIDEO_nRDA, HIGH);

  if (digitalRead(PIN_VIDEO_DA) == HIGH) {
    int c = __mcp23s17_getchar();

    __tty_putchar(c);

    digitalWrite(PIN_VIDEO_nRDA, LOW);
  }
}

static void __print_banner(void) {
  Serial.print(F("\033[2J"));   // clear screen
  Serial.println(F("RC6502 Apple 1 Replica"));
  Serial.println();
  Serial.println(F("  - E000 R - INTEGER BASIC"));
  Serial.println(F("  - F000 R - KRUSADER 1.3"));
  Serial.println();
}

void setup(void) {
  // put your setup code here, to run once:
  Serial.begin(115200);

  __setup_mcp23s17();
  __setup_pin_video();
  __setup_pin_kbd();

  __print_banner();
}

void loop(void) {
  // put your main code here, to run repeatedly:
  input_from_kbd();
  output_to_mc6820();
  output_to_video();
}
