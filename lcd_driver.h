#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include "mbed.h"

class I2CLCD {
private:
    I2C i2c;
    int addr;

    static const char LCD_BACKLIGHT = 0x08;
    static const char ENABLE_BIT    = 0x04;
    static const char RS_BIT        = 0x01;

    void expanderWrite(char data);
    void pulseEnable(char data);
    void write4Bits(char data);
    void send(char value, char mode);

public:
    I2CLCD(PinName sda, PinName scl, int address);

    void init();
    void command(char value);
    void writeChar(char value);
    void clear();
    void setCursor(int col, int row);
    void print(const char *str);
    void printPadded(const char *str, int width = 20);
};

void initializeLCD(I2CLCD &lcd);
void updateLCDvalues(I2CLCD &lcd, float currentTempF);

#endif // I2C_H