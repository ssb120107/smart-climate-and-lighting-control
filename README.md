# smart-climate-and-lighting-control
This is an intelligent embedded system that automatically controls a fan based on room temperature and lights based on outdoor sunlight. The system uses an Arduino Uno as the main controller, with LM35 temperature sensor and dual LDR (Light Dependent Resistor) sensors for input, and relays for controlling the fan and lights. A 16x2 I2C LCD display shows real-time system status, and push buttons provide manual override capability.

# Key Features:
For Temperature control-
The system provides automatic temperature control where the fan turns on when the room temperature exceeds 28°C and turns off when it drops below 24°C. This 4°C difference, called hysteresis, prevents rapid on-off cycling that could damage the relay. The system also includes temperature trend prediction - if the temperature is rising faster than 1.5°C per 30 seconds and reaches 26°C, the fan turns on early to pre-cool the room before it gets too hot.

For lighting control-
the system uses two LDR sensors. One LDR is placed outside to detect sunlight levels, while another LDR inside the room measures ambient brightness. The lights turn on only when both outdoor light is low (below 400) and indoor light is low (below 300), preventing false triggers. When turning on, the lights don't just switch abruptly - they dim gradually over 5 minutes in 10 steps, simulating a sunrise. Similarly, when turning off, they dim down gradually like a sunset.

The system includes manual override buttons for both fan and light control. When a button is pressed, the system enters manual mode and the user can toggle the device on or off. After one hour, the system automatically reverts to automatic mode, preventing energy waste if the user forgets to switch back.LEDs provide visual feedback - a blue LED indicates fan status, yellow indicates light status, and a red LED blinks if any sensor error is detected.
All system information is displayed on a 16x2 I2C LCD screen. The display shows current temperature with a trend arrow indicating whether temperature is rising, falling, or stable. It also shows outdoor and indoor light levels, fan status, light status with brightness level indicators, and manual override indicators.
