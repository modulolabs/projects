#include "Modulo/Modulo.h"
#include <math.h>

// This is the source code for Aqua, an aquarium controller built using Modulo
// Visit http://www.modulo.co/projects/modulo-aquarium-controller to learn
// more about this project.

// An enum for the various pages that we can show on the display
enum Page {
    PageTime,
    PageTemperature,
    PageHeater,
    PageSumpLight,
    PageReturnPump,
    PageCarbonFilter,
    PageSkimmer,
    PageTopOff,
    NumPages
};

// An enum for tri-state outlets, which can be explicitly on,
// explicitly off, or manually controlled.
enum TriState {
    TriStateOff,
    TriStateOn,
    TriStateAuto
};

void onButtonPressed(DisplayModulo &display, int button);
void onKnobMoved(KnobModulo &knob);
void refreshDisplay();

DisplayModulo display;
TemperatureProbeModulo tempProbe;
KnobModulo knob;
BlankSlateModulo blankSlate;

Page page = PageTime;

TriState topOffMode = TriStateAuto;
TriState heaterMode = TriStateAuto;
uint8_t targetTemperature = 80;
bool sumpLightOn = false;
bool returnPumpOn = true;
bool carbonFilterOn = true;
bool skimmerOn = true;
bool topOffPump = false;
bool topOffSwitch = false;
bool heaterOn = false;

// The time at which to turn on the sump light (in number of minutes since midnight) 
int sumpLightOnTime = 22*60; // 10 PM

// The time at which to turn off the sump light (in number of minutes since midnight)
int sumpLightOffTime = 10*60; // 10 AM

// The timestamp from the last time the loop() function ran.
uint32_t previousTimestamp = 0;

    
void updateTopOff() {  
    // Monitor the top off switch (on the blank slate's I/O pin 4) 
    blankSlate.setPullup(4, true); // Enable pullup on pin 4
    topOffSwitch = blankSlate.getDigitalInput(4);
    
    if (topOffMode == TriStateAuto) {
        // Limit the top off pump so that it can run at most 2 minutes
        // at the beginning of every hour. This prevents flooding if
        // the switch gets stuck.
        if (Time.minute() < 2) {
            topOffPump = !topOffSwitch;
        } else {
            topOffPump = false;
        }
    } else {
        topOffPump = topOffMode == TriStateOn;
    }
    
    refreshDisplay();
}

void updateHeater() {
    // Include hysteresis when updating the heater based on the current temperature.
    // This means that the temperature must be a certain amount over the set point
    // to turn on, and a certain amount under the set point to turn off. This
    // prevents rapid cycling due to noise on the tempearture input.
    float heaterHysteresis = .25;
    if (heaterMode == TriStateAuto) {
        if (tempProbe.getTemperatureF() > targetTemperature+heaterHysteresis) {
            heaterOn = false;
        }
        if (tempProbe.getTemperatureF() < targetTemperature-heaterHysteresis) {
            heaterOn = true;
        }
    
    } else {
        heaterOn = (heaterMode == TriStateOn);
    }
    
    refreshDisplay();
}

void onMinutesChanged(int32_t minutes) {
    if (minutes == sumpLightOnTime) {
        sumpLightOn = true;
    }
    if (minutes == sumpLightOffTime) {
        sumpLightOn = false;
    }
    
    Serial.println(minutes);
    
    refreshDisplay();
}

void updateTimers() {
    // First get the new unix timestamp
    int32_t newTimestamp = Time.now();
    
    // Convert the new and old timestamps to minutes
    int32_t newMinutes = (newTimestamp/60) % (60*24);
    int32_t oldMinutes = (previousTimestamp/60) % (60*24);
    
    if (newMinutes != oldMinutes) {
        onMinutesChanged(Time.hour()*60 + Time.minute());
    }
    
    previousTimestamp = newTimestamp;
}

void updateOutlets() {
    uint8_t outlets =
        (!heaterOn << 0) |
        (!returnPumpOn << 1) |
        (!carbonFilterOn << 2) |
        (!skimmerOn << 3) |
        (1 << 4) | // Unused
        (1 << 5) | // Unused
        (!sumpLightOn << 6) |
        (!topOffPump << 7);
        
    Serial.println(outlets, BIN);
    
    Wire.beginTransmission(0x38);
    Wire.write(outlets);
    Wire.endTransmission();
}

void refreshDisplay() {
    display.clear();
    
    switch(page) {
        case PageTime:
            display.println("  Current Time");
            display.println();
            display.setTextSize(2);
            display.setCursor(6, 20);
            display.print(Time.format(Time.now(), "%I:%M%P"));
            break;
        case PageTemperature:
            refreshTemperatureDisplay(display);
            break;
        case PageHeater:
            refreshHeaterDisplay(display);
            break;
        case PageTopOff:
            display.println("    Top Off");
            display.println();
            display.setTextSize(2);
            switch (topOffMode) {
                case TriStateOff:
                    display.println("Off");
                    break;
                case TriStateOn:
                    display.println("On");
                    break;
                case TriStateAuto:
                    display.println("Auto");
                    break;
            }
            display.setTextSize(1);
            display.println();
            display.print("Switch: ");
            display.println(topOffSwitch ? "High" : "Low");
            display.print("Pump: ");
            display.println(topOffPump ? "On" : "Off");
            break;
        case PageSumpLight:
            display.println("   Sump Light");
            display.println();
            display.setTextSize(2);
            display.println(sumpLightOn ? "On" : "Off");     
            
            display.setTextSize(1);
            display.println();
            display.print(" On at: ");
            display.print((sumpLightOnTime / 60) % 12);
            display.print(":");
            display.print((sumpLightOnTime % 60) / 10, 2);
            display.print(sumpLightOnTime % 10, 2);
            display.println((sumpLightOnTime/60 >= 12) ? "pm" : "am");
         
            display.print("Off at: ");
            display.print((sumpLightOffTime / 60) % 12);
            display.print(":");
            display.print((sumpLightOffTime % 60) / 10, 2);
            display.print(sumpLightOffTime % 10, 2);
            display.println((sumpLightOffTime/60 >= 12) ? "pm" : "am");
            
            break;
        case PageReturnPump:
            display.println("   Return Pump");
            display.println();
            display.setTextSize(2);
            display.print(returnPumpOn ? "On" : "Off");
            break;
        case PageCarbonFilter:
            display.println("  Carbon Filter");
            display.println();
            display.setTextSize(2);
            display.print(carbonFilterOn ? "On" : "Off");
            break;
        case PageSkimmer:
            display.println("    Skimmer");
            display.println();
            display.setTextSize(2);
            display.print(skimmerOn ? "On" : "Off");
            break;
    }
    
    display.refresh();
    
    if (sumpLightOn) {
        knob.setColor(1,1,0);
    } else {
        knob.setColor(0,0,.2);
    }
}


void refreshTemperatureDisplay(DisplayModulo &d) {
    d.println("  Temperature");
    d.setTextSize(2);
    d.setCursor((96-6*6*2)/2, 20);
    if (fabs(tempProbe.getTemperatureF()-targetTemperature) < 2) {
        d.setTextColor(0,1,0);
    } else if (tempProbe.getTemperatureF() < targetTemperature) {
        d.setTextColor(0,0,1);
    } else {
        d.setTextColor(1,0,0);
    }
    
    d.print(tempProbe.getTemperatureF(), 1);
    d.println("\xF7" "F");
    
    d.setTextColor(1,1,1);
    d.setTextSize(1);
    d.println();
    d.print("Target: ");
    d.print(targetTemperature, 1);
    d.println("\xF7" "F");
    d.print("Heater: ");
    d.print(heaterOn ? "On" : "Off"); 
}


void refreshHeaterDisplay(DisplayModulo &d) {
    display.println("     Heater ");
    display.println();
    display.setTextSize(2);
    switch (heaterMode) {
        case TriStateOff:
            display.println("Off");
            break;
        case TriStateOn:
            display.println("On");
            break;
        case TriStateAuto:
            display.println("Auto");
            break;
    }
  
    d.setTextColor(1,1,1);
    d.setTextSize(1);
    d.println();
    d.print("Target: ");
    d.print(targetTemperature, 1);
    d.println("\xF7" "F");
    d.print("Heater: ");
    d.print(heaterOn ? "On" : "Off"); 
}

void onTemperatureChanged(TemperatureProbeModulo &t) {
    updateHeater();
}

void onKnobButtonPressed(KnobModulo &knob) {
    sumpLightOn = !sumpLightOn;
    refreshDisplay();
}

void onButtonPressed(DisplayModulo &display, int button) {
    switch(button) {
        case 0:
            page = Page((int(page)-1+NumPages) % NumPages);
            break;
        case 1:
            switch (page) {
                case PageTemperature:
                    if (targetTemperature >= 84) {
                        targetTemperature = 72;
                    } else {
                        targetTemperature++;
                    }
                    updateHeater();
                    break;
                case PageSumpLight:
                    sumpLightOn = !sumpLightOn;
                    break;
                case PageReturnPump:
                    returnPumpOn = !returnPumpOn;
                    break;
                case PageCarbonFilter:
                    carbonFilterOn = !carbonFilterOn;
                    break;
                case PageSkimmer:
                    skimmerOn = !skimmerOn;
                    break;
                case PageTopOff:
                    topOffMode = TriState((int(topOffMode)+1) % 3);
                    updateTopOff();
                    break;
                case PageHeater:
                    heaterMode = TriState((int(heaterMode)+1) % 3);
                    updateHeater();
                    break;
            }
            break;
        case 2:
            page = Page((int(page)+1) % NumPages);
            break;
    }
    
    refreshDisplay();
}


#define publish_delay 5000
unsigned int lastPublish = 0;

void updatePublish() {
        unsigned long now = millis();

    if ((now - lastPublish) < publish_delay) {
        // it hasn't been 10 seconds yet...
        return;
    }

    Spark.publish("librato_temperature", String(tempProbe.getTemperatureF()), 60, PRIVATE);
    Spark.publish("librato_heater", String(heaterOn), 60, PRIVATE);

    Spark.publish("librato_topOffPump", String(topOffPump), 60, PRIVATE);
    Spark.publish("librato_topOffSwitch", String(topOffSwitch), 60, PRIVATE);

    lastPublish = now;
}

void setup() {
    Serial.begin(9600);
    
    
    Modulo.setup();
    refreshDisplay();
    
    tempProbe.setTemperatureChangeCallback(onTemperatureChanged);
    display.setButtonPressCallback(onButtonPressed);
    knob.setButtonPressCallback(onKnobButtonPressed);

    // Set the timezone to UTC-8 (for Pacific Standard Time)
    // Change this if you live in a different timezone.
    Time.zone(-8);
    
    // Initialize the previousTimestamp
    previousTimestamp = Time.now();
}



void loop() {
    Modulo.loop();
    
    // Update time of day based timers for the sump light.
    updateTimers();

    // Update the auto-topoff
    updateTopOff();

    // Update the outlets based on the current state
    updateOutlets();
    
    // Publish graph data
    updatePublish();
    

}


