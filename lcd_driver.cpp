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
// CIRCUIT INPUTS
// ==========================================================

DigitalOut OUTPUT(PB_4);

std::atomic_bool startFlag(false);
std::atomic_bool stopFlag(false);

// ==========================================================
// SYSTEM VARIABLES
// ==========================================================

float currentTempC = 25.0f;
float currentTempF = 77.0f;
float targetTempF  = 68.0f;
int fanLevel       = 0;
bool overrideMode  = false;
bool coolingActive = false;

// Timer variables
Timer coolingTimer;
bool timerRunning = false;
bool targetReached = false;
float elapsedTimeSec = 0.0f;

// ==========================================================
// INTERRUPTS
// ==========================================================

void isrStart() { startFlag.store(true); }
void isrStop()  { stopFlag.store(true); }

// ==========================================================
// SENSOR FUNCTION
// ==========================================================


// ==========================================================
// DISPLAY HELPERS
// ==========================================================

const char* fanText(int level) {
    switch (level) {
        case 0: return "OFF";
        case 1: return "LOW";
        case 2: return "MED";
        case 3: return "HIGH";
        default: return "ERR";
    }
}

const char* modeText() {
    if (overrideMode) return "OVERRIDE";
    if (targetReached) return "AT TEMP";
    if (coolingActive) return "COOLING";
    return "IDLE";
}

void updateFanLevel(float tempF) {
    if (!coolingActive) {
        fanLevel = 0;
    } else if (tempF < targetTempF + 2.0f) {
        fanLevel = 1;
    } else if (tempF < targetTempF + 5.0f) {
        fanLevel = 2;
    } else {
        fanLevel = 3;
    }
}

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

void updateLCD(I2CLCD &lcd) {
    char line[21];
    char tempStr[10];
    char setStr[10];
    char timeStr[10];

    formatTemp(tempStr, sizeof(tempStr), currentTempF);
    formatTemp(setStr, sizeof(setStr), targetTempF);
    formatElapsedTime(timeStr, sizeof(timeStr), elapsedTimeSec);

    lcd.setCursor(0, 0);
    snprintf(line, sizeof(line), "Temp:%sF Set:%s", tempStr, setStr);
    lcd.printPadded(line);

    lcd.setCursor(0, 1);
    snprintf(line, sizeof(line), "Fan:%-4s Out:%-3s", fanText(fanLevel), OUTPUT.read() ? "ON" : "OFF");
    lcd.printPadded(line);

    lcd.setCursor(0, 2);
    snprintf(line, sizeof(line), "Time to set:%s", timeStr);
    lcd.printPadded(line);

    lcd.setCursor(0, 3);
    snprintf(line, sizeof(line), "Mode:%-8s", modeText());
    lcd.printPadded(line);
}

// ==========================================================
// TIMER LOGIC
// ==========================================================

void updateCoolingTimer() {
    // Start timer only when cooling is active and temp is above target
    if (coolingActive && currentTempF > targetTempF) {
        if (!timerRunning && !targetReached) {
            coolingTimer.reset();
            coolingTimer.start();
            timerRunning = true;
        }
    }

    // If timer is running, keep track of elapsed time
    if (timerRunning) {
        elapsedTimeSec = std::chrono::duration<float>(coolingTimer.elapsed_time()).count();
    }

    // Stop timer once target temperature is reached
    if (timerRunning && currentTempF <= targetTempF) {
        coolingTimer.stop();
        elapsedTimeSec = std::chrono::duration<float>(coolingTimer.elapsed_time()).count();
        timerRunning = false;
        targetReached = true;
    }

    // If cooling is stopped manually, stop timer
    if (!coolingActive && timerRunning) {
        coolingTimer.stop();
        elapsedTimeSec = std::chrono::duration<float>(coolingTimer.elapsed_time()).count();
        timerRunning = false;
    }
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

void updateLCDvalues(I2CLCD &lcd, float currentTempF) {
    updateFanLevel(currentTempF);
    updateCoolingTimer();
    updateLCD(lcd);
}