// i2c code "library"

#include "mbed.h"
#include <cstdio>
#include <atomic>

#include "lcd_driver.h"

I2CLCD::I2CLCD(PinName sda, PinName scl, int address)
    : i2c(sda, scl), addr(address) {
    i2c.frequency(100000);
}

void I2CLCD::expanderWrite(char data) {
    char temp = data | LCD_BACKLIGHT;
    i2c.write(addr, &temp, 1);
}

void I2CLCD::pulseEnable(char data) {
    expanderWrite(data | ENABLE_BIT);
    wait_us(1000);
    expanderWrite(data & ~ENABLE_BIT);
    wait_us(1000);
}

void I2CLCD::write4Bits(char data) {
    expanderWrite(data);
    pulseEnable(data);
}

void I2CLCD::send(char value, char mode) {
    char highNibble = value & 0xF0;
    char lowNibble  = (value << 4) & 0xF0;

    write4Bits(highNibble | mode);
    write4Bits(lowNibble  | mode);
}

void I2CLCD::init() {
    wait_us(50000);

    write4Bits(0x30);
    wait_us(5000);
    write4Bits(0x30);
    wait_us(5000);
    write4Bits(0x30);
    wait_us(2000);
    write4Bits(0x20);
    wait_us(2000);

    command(0x28);
    command(0x0C);
    command(0x06);
    clear();
}

void I2CLCD::command(char value) {
    send(value, 0);
}

void I2CLCD::writeChar(char value) {
    send(value, RS_BIT);
}

void I2CLCD::clear() {
    command(0x01);
    wait_us(2000);
}

void I2CLCD::setCursor(int col, int row) {
    static const int rowOffsets[] = {0x00, 0x40, 0x14, 0x54};
    command(0x80 | (col + rowOffsets[row]));
}

void I2CLCD::print(const char *str) {
    while (*str) writeChar(*str++);
}

void I2CLCD::printPadded(const char *str, int width) {
    int count = 0;
    while (*str && count < width) {
        writeChar(*str++);
        count++;
    }
    while (count < width) {
        writeChar(' ');
        count++;
    }
}

// ==========================================================
// DISPLAY HELPERS
// ==========================================================

void formatTemp(char *buffer, size_t size, float value) {
    int whole = (int)value;
    int tenth = (int)((value - whole) * 10.0f);
    if (tenth < 0) tenth = -tenth;
    snprintf(buffer, size, "%d.%d", whole, tenth);
}

void formatElapsedTime(char *buffer, size_t size, float seconds) {
    int total = (int)seconds;
    int mins = total / 60;
    int secs = total % 60;
    snprintf(buffer, size, "%02d:%02d", mins, secs);
}

// ==========================================================
// MAIN
// ==========================================================

void initializeLCD(I2CLCD &lcd) {
    lcd.init();
    lcd.setCursor(0, 0);
    lcd.printPadded("Smart AC System");
    lcd.setCursor(0, 1);
    lcd.printPadded("LCD + Thermistor");
    lcd.setCursor(0, 2);
    lcd.printPadded("Timer Enabled");
    wait_us(2000000);
    lcd.clear();
}

void updateLCDvalues(I2CLCD &lcd, float currentTempC, float targetTempC, float dutyCycle) {
    char line[21];
    char curStr[10];
    char tgtStr[10];

    formatTemp(curStr, sizeof(curStr), currentTempC);
    formatTemp(tgtStr, sizeof(tgtStr), targetTempC);
    int pwmPct = (int)(dutyCycle * 100.0f + 0.5f);

    lcd.setCursor(0, 0);
    snprintf(line, sizeof(line), "Current: %sC", curStr);
    lcd.printPadded(line);

    lcd.setCursor(0, 1);
    snprintf(line, sizeof(line), "Target:  %sC", tgtStr);
    lcd.printPadded(line);

    lcd.setCursor(0, 2);
    snprintf(line, sizeof(line), "Fan PWM: %d%%", pwmPct);
    lcd.printPadded(line);

    lcd.setCursor(0, 3);
    lcd.printPadded("");
}