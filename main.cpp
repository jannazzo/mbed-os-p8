/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"
#include <cstdio>
#include <iostream>
#include <cmath>

#include "lcd_driver.h"

// PA_0 is CN8/A0
AnalogIn TemperatureSensor(PA_1); //CN8/A1
// PC_1 is CN8/A4
InterruptIn UP_BUTTON(PB_5); //CN5/D4
InterruptIn DOWN_BUTTON(PA_10); //CN5/D2
InterruptIn STATUS_BUTTON(PA_8); //CN5/D7
// DigitalOut OUTPUT(PB_4); //CN9/D5
PwmOut fan(PB_3); //D3 PWM header for fan
DigitalOut status_led(PC_4); // status LED

// LEDs are connected to CN10 pins
DigitalOut temp_red(PC_8);
DigitalOut temp_yellow(PC_6);
DigitalOut temp_green(PC_5);

DigitalOut fan_red(PA_12);
DigitalOut fan_yellow(PA_11);
DigitalOut fan_green(PB_12);

#define Vsupply 3.32f // The microcontroller supplies 3.3 V

int TargetTempLevel = 1; // default the target temp to 1
int FanSpeedLevel = 1; // default to 1

bool status = true;

//variables for temperature sensor
float TemperatureSensorDigiValue; //the A/D converter value read by the controller input pin
float TemperatureSensorVoltValue; //the voltage on the controller input pin (across the 10k resistor) from the temperature sensor voltage divider
float ThermistorResistance; //computed from the voltage drop value across the thermistor
float ThermistorTemperature; //approximate ambient temperature measured by thermistor
#define ThermistorBiasResistor 9850.0f //Bias resistor (lower leg of voltage divider) for thermistor

// Variables to hold control reference values.
float TemperatureLimit = 26; //enter a temperature in Celsius here for temperature deactivation; NOTE: room temperature is 25C

// This function will be attached to the start button interrupt.
void UpPressed(void)
{
    printf("Target Temperature Increased.\n");
    if (TargetTempLevel < 3) {
        TargetTempLevel += 1;
    }
}

// This function will be attached to the stop button interrupt.
void DownPressed(void)
{
    printf("Target Temperature Decreased.\n");
    if (TargetTempLevel > 1) {
        TargetTempLevel -= 1;
    }
}

// This function will be attached to the status button interrupt.
void StatusPressed(void)
{
    status = !status; // flip the status
    printf("Status Toggled: %d\n", status);
}

float cToF(float tempC) {
    return (tempC * 9.0f / 5.0f) + 32.0f;
}

// level variable will always be TargetTempLevel
void setStrip(DigitalOut &red, DigitalOut &yellow, DigitalOut &green, int level)
{
    
    red = 0;
    yellow = 0;
    green = 0;

    if (level == 3) {
        red = 1;
        yellow =1;
        green = 1;
    } else if (level == 2) {
        yellow = 1;
        green = 1;
    } else if (level == 1) {
        green = 1;
    }
    // if none of the above cases are true, the LEDs stay all off (case of fan speed 0)
}


 
// This function converts the voltage value from the thermistor input to an approximate temperature
// in Celsius based on a linear approximation of the thermistor.
float getThermistorTemperature(void)
{
	// 1. Read the TemperatureSensor A/D value and store it in TemperatureSensorDigiValue
	// 2. Calculate TemperatureSensorVoltValue from TemperatureSensorDigiValue and Vsupply
	// 3. Calculate ThermistorResistance using the voltage divider equation

    // debug lines for temperature calibration
    TemperatureSensorDigiValue = TemperatureSensor.read();
    //printf("\nThermistor Digital Value: %f", TemperatureSensorDigiValue);
    TemperatureSensorVoltValue = TemperatureSensorDigiValue*Vsupply;
    //printf("\nThermistor Voltage Value: %f", TemperatureSensorVoltValue);
    //ThermistorResistance = TemperatureSensorVoltValue*ThermistorBiasResistor/(Vsupply - TemperatureSensorVoltValue);
    ThermistorResistance = ThermistorBiasResistor * (Vsupply - TemperatureSensorVoltValue) / TemperatureSensorVoltValue;
    //printf("\nThermistor Resistance: %f", ThermistorResistance);

    //printf("\n");
    // changed order of bias and thermistor resistance to scale correctly
    //ThermistorTemperature = ((ThermistorResistance - 10000.0)/(-320.0)) + 25.0; //temperature of the thermistor computed by a linear approximation of the device response

    // compute Thermistor temperature in Kelvin using the proper function
    float AltThermistorTemperature = (3977 / ( log(ThermistorResistance) - log(10700) + (3977 / 298.15) )) - 273.15;
    //printf("Alt Thermistor Temperature: %f", AltThermistorTemperature);

    return AltThermistorTemperature;
}

float targetTemp(int TargetTempLevel) {
    if (TargetTempLevel == 1) {
        return 27.0f; // TARGET TEMP FOR LEVEL 1
    } else if (TargetTempLevel == 2) {
        return 29.0f; // TARGET TEMP FOR LEVEL 2
    } else if (TargetTempLevel == 3) {
        return 31.0f; // TARGET TEMP FOR LEVEL 3
    } else {
        return 27.0f; // default value is level 1
    }
}

int setFanSpeed(int TargetTempLevel) {
    // based on the target temp level, adjust the fan speed based on the size of the differential
    float tempC = getThermistorTemperature();

    float target = targetTemp(TargetTempLevel);
    float diff = tempC - target;

    if (diff <= 1.0) {
        return 0; // slowest fan speed
    } else if (diff <= 4.0) {
        return 1;
    } else if (diff <= 7.0) {
        return 2;
    } else if (diff > 7.0) {
        return 3;
    } else {
        return 0; // default
    }
}

int main(void)
{

    // Setup an event queue to handle event requests for the ISR
    // and issue the callback in the event thread.
    EventQueue event_queue;
    // Setup a event thread to update the motor
    Thread event_thread(osPriorityNormal);
    // Start the event on a loop ready to receive event calls from the event queue
    event_thread.start(callback(&event_queue, &EventQueue::dispatch_forever));
    // Attach the functions to the hardware interrupt pins to be inserted into
    // the event queue and exicuted on button press.
    UP_BUTTON.rise(event_queue.event(&UpPressed));
    DOWN_BUTTON.rise(event_queue.event(&DownPressed));
    STATUS_BUTTON.rise(event_queue.event(&StatusPressed));

    // set up the PWM fan
    float dutyCycle = 0.15;
    fan.period(0.00004); // PWM frequency = 25 kHz
    fan.write(dutyCycle); // default PWM to 15%

    // initialize the LCD screen
    I2CLCD lcd(D14, D15, 0x27 << 1);
    initializeLCD(lcd);

    while(true) {

        // set the LEDs for the temperature level
        setStrip(temp_red, temp_yellow, temp_green, TargetTempLevel);

        status_led = status;
        if (!status) {
            FanSpeedLevel = 0;
        } else {
            FanSpeedLevel = setFanSpeed(TargetTempLevel);
        }

        if (FanSpeedLevel == 0) {
            dutyCycle = 0.0f;
        } else {
            dutyCycle = 0.05 + FanSpeedLevel * 0.10;
        }
        

        setStrip(fan_red, fan_yellow, fan_green, FanSpeedLevel); // set the fan strip lights as well

        
        fan.write(dutyCycle);
        
        // output measured temperature for debugging
        printf("\n");
        printf("\nCurrent Temperature Value: %f", getThermistorTemperature());
        printf("\nCurrent Temp Level: %i", TargetTempLevel);
        printf("\nCurrent Fan Level: %i", FanSpeedLevel);
        printf("\nDuty Cycle: %f", dutyCycle);
        printf("\n");

        // print to the screen
        updateLCDvalues(lcd, cToF(getThermistorTemperature()));
        
        wait_us(1000000); // Wait 1 second before repeating the loop.
    }
}