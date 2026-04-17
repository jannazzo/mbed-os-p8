/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"
#include <cstdio>
#include <iostream>
 
// PA_0 is CN8/A0
AnalogIn TemperatureSensor(PA_1); //CN8/A1
// PC_1 is CN8/A4
InterruptIn UP_BUTTON(PB_5); //CN5/D4
InterruptIn DOWN_BUTTON(PA_10); //CN5/D2
// DigitalOut OUTPUT(PB_4); //CN9/D5
PwmOut fan(PB_3); //D3 PWM header for fan
DigitalOut led(LED1);

// LEDs are connected to CN10 pins
DigitalOut temp_red(PC_8);
DigitalOut temp_yellow(PC_6);
DigitalOut temp_green(PC_5);

DigitalOut fan_red(PA_12);
DigitalOut fan_yellow(PA_11);
DigitalOut fan_green(PB_12);


#define Vsupply 3.3f // The microcontroller supplies 3.3 V

int TargetTempLevel = 1; // default the target temp to 1

//variables for temperature sensor
float TemperatureSensorDigiValue; //the A/D converter value read by the controller input pin
float TemperatureSensorVoltValue; //the voltage on the controller input pin (across the 10k resistor) from the temperature sensor voltage divider
float ThermistorResistance; //computed from the voltage drop value across the thermistor
float ThermistorTemperature; //approximate ambient temperature measured by thermistor
#define ThermistorBiasResistor 10000.0f //Bias resistor (lower leg of voltage divider) for thermistor

// Variables to hold control reference values.
float TemperatureLimit = 26; //enter a temperature in Celsius here for temperature deactivation; NOTE: room temperature is 25C

// This function will be attached to the start button interrupt.
void UpPressed(void)
{
    cout << "Target Temperature Increased." << endl;
    if (TargetTempLevel < 3) {
        TargetTempLevel += 1;
    }
}

// This function will be attached to the stop button interrupt.
void DownPressed(void)
{
    cout << "Target Temperature Decreased." << endl;
    if (TargetTempLevel > 1) {
        TargetTempLevel -= 1;
    }
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
    } else {
        green = 1;
    }
}
 
// This function converts the voltage value from the thermistor input to an approximate temperature
// in Celsius based on a linear approximation of the thermistor.
float getThermistorTemperature(void)
{
	// 1. Read the TemperatureSensor A/D value and store it in TemperatureSensorDigiValue
	// 2. Calculate TemperatureSensorVoltValue from TemperatureSensorDigiValue and Vsupply
	// 3. Calculate ThermistorResistance using the voltage divider equation

    TemperatureSensorDigiValue = TemperatureSensor.read();
    TemperatureSensorVoltValue = TemperatureSensorDigiValue*Vsupply;
    ThermistorResistance = TemperatureSensorVoltValue*ThermistorBiasResistor/(Vsupply - TemperatureSensorVoltValue);

    // changed order of bias and thermistor resistance to scale correctly
    ThermistorTemperature = ((ThermistorResistance - 10000.0)/(1000.0)) + 25.0; //temperature of the thermistor computed by a linear approximation of the device response

    return ThermistorTemperature;
}
 
// This function will check for a temperature triggered deactivation of the motor
void CheckTemperatureSensor(void)
{
    // Use the getThermistorTemperature() function defined above to obtain a temperature value to use for comparison and decision making with your TemperatureLimit
    if (getThermistorTemperature() >= TemperatureLimit) {
        cout << "Temperature Stop!" << endl;
        led = 0;
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

    fan.period(0.00004); // PWM frequency = 25 kHz
    fan.write(0.15); // default PWM to 15%

    while(true) {
        // Check the analog inputs.
        CheckTemperatureSensor();

        // set the LEDs for the temperature level
        setStrip(temp_red, temp_yellow, temp_green, TargetTempLevel);

        fan.write(0.05 + TargetTempLevel * 0.10);
       
        // output measured temperature for debugging
        //cout << "\n" << endl; // newline to separate cycles
        printf("\n");
        printf("\nCurrent Temperature Value: %f", getThermistorTemperature());
        printf("\nCurrent Temp Level: %i", TargetTempLevel);
        printf("\nUp Button: %i", UP_BUTTON.read());
        printf("\nDown Button: %i", DOWN_BUTTON.read());
        printf("\n");
        
        wait_us(1000000); // Wait 1 second before repeating the loop.
    }
}