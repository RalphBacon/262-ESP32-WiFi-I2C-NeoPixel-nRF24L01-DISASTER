#pragma once
#include "Arduino.h"
#include "globals.h"

namespace pirH {
/*
    Using PIR detector.

    #define vssPIR 33
    #define gndPIR 32
    #define sigPIR 17
*/
void pirSetup()
{
    pinMode(vssPIR, OUTPUT);
    pinMode(gndPIR, OUTPUT);
    pinMode(sigPIR, INPUT_PULLDOWN);

    // Power the PIR
    digitalWrite(gndPIR, LOW);
    digitalWrite(vssPIR, HIGH);
}

// Read the PIR, goes HIGH on movement, for about 1 second or thereabouts.
// isQuietTime is read by the neoPixel routine(s)
void isMovementDetected()
{
    static unsigned long prevCheckMillis = 0;
    if (millis() - prevCheckMillis >= 1000) {
        prevCheckMillis = millis();

        // Whenever movement detected, (re-)start the timer and return
        if (digitalRead(sigPIR)) {
            pirMillis = millis();
            isQuietTime = false;
            log_v("Movement detected by PIR (timer (re-)started)");
            return;
        }

        // If the delay since the last movement has not yet expired, just return
        if (millis() - pirMillis < MOVEMENT_TIMEOUT_SECONDS) {
            isQuietTime = false;
            log_v("Movement timer still active");
            return;
        }

        // No movement (in the last MOVEMENT_TIMEOUT_SECONDS minutes)
        isQuietTime = true;
        log_v("No movement detected");
    }
}

} // pirH namespace