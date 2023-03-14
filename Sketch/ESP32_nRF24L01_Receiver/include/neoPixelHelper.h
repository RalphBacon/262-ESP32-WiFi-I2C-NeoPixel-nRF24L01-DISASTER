#pragma once
#include "globals.h"
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <WiFi.h>
//#include "wifiHelper.h"

namespace neo {

// GPIOs the LEDs are connected to
#define CIRCLE1 25
#define CIRCLE2 26
#define CIRCLE3 27

// The number of LEDs per NeoPixel array
#define CIRCLE_LEDS 16

// Initial NeoPixel brightness, 0 (min) to 255 (max)
#define BRIGHTNESS 0

// Declare THREE independent objects to control the NeoPixels. num leds, pin, type
Adafruit_NeoPixel binLED1(CIRCLE_LEDS, CIRCLE1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel binLED2(CIRCLE_LEDS, CIRCLE2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel binLED3(CIRCLE_LEDS, CIRCLE3, NEO_GRB + NEO_KHZ800);

// Move to an array for ease of processing
Adafruit_NeoPixel binLEDs[3] = { binLED1, binLED2, binLED3 };

// Create task handles and copy pointers to them into an array for ease of use in a loop
static TaskHandle_t neoPixelTaskHandle0;
static TaskHandle_t neoPixelTaskHandle1;
static TaskHandle_t neoPixelTaskHandle2;
static TaskHandle_t* neoPixelTaskHandles[3] = { &neoPixelTaskHandle0, &neoPixelTaskHandle1, &neoPixelTaskHandle2 };

// Time RTC handler
static TaskHandle_t rtcTaskHandle;

// enum for fade-up/fade-down/flash
enum class fader : uint8_t {
    DOWN,
    UP,
    FLASH,
    LEFT,
    RIGHT
};

// Forward declarations. Control each circle in a task function.
uint8_t easeInOutCubic(float progressDelay, uint8_t binNo);
void setBinLidStateColour(void* parameter, fader& fadeDirection, uint8_t& currPixel);
void checkStack(uint8_t binNo);
void processNeoPixels(void* binTrackerParm);
void checkQuietHours(void* pvparameters);

void neoTaskSetup()
{
    // Ensure all LED arrays are clear when we start
    for (auto bin : binLEDs) {
        bin.begin();
        bin.clear();
    }

    // Create a separate task for each bin (note: would start immediately)
    // Start at priority 0 so they do not run until boosted to priority 1
    xTaskCreatePinnedToCore(
        neo::processNeoPixels, /* Function to implement the task */
        "Bin-1", /* Friendly name of the task */
        5000, /* Stack size in bytes */
        (void*)&binTracker[0], /* Task input parameter */
        0, /* Priority of the task */
        &neoPixelTaskHandle0, /* Task handle. */
        1); /* Core where the task should run */

    xTaskCreatePinnedToCore(
        neo::processNeoPixels, /* Function to implement the task */
        "Bin-2", /* Friendly name of the task */
        5000, /* Stack size in bytes */
        (void*)&binTracker[1], /* Task input parameter */
        0, /* Priority of the task */
        &neoPixelTaskHandle1, /* Task handle. */
        1); /* Core where the task should run */

    xTaskCreatePinnedToCore(
        neo::processNeoPixels, /* Function to implement the task */
        "Bin-3", /* Friendly name of the task */
        5000, /* Stack size in bytes */
        (void*)&binTracker[2], /* Task input parameter */
        0, /* Priority of the task */
        &neoPixelTaskHandle2, /* Task handle. */
        1); /* Core where the task should run */

    // xTaskCreatePinnedToCore(
    //     neo::checkQuietHours, /* Function to implement the task */
    //     "RTC", /* Friendly name of the task */
    //     5000, /* Stack size in bytes */
    //     NULL, /* Task input parameter */
    //     1, /* Priority of the task */
    //     &rtcTaskHandle, /* Task handle. */
    //     1); /* Core where the task should run */
}

// Soft fade reflecting current status
void processNeoPixels(void* binTrackerParm)
{
    char* binName = pcTaskGetTaskName(NULL);
    printf("processNeoPixels running for task \"%s\"\n", binName);

    // Create a pointer of the correct variable type for the incoming parameter
    binTrackers* thisBinTracker = (binTrackers*)binTrackerParm;

    // This task does everything exclusively for THIS bin
    uint8_t thisBin = thisBinTracker->binData.binID - 1;

    // Debug info
    log_d("%s: Data for bin %d", binName, thisBinTracker->binData.binID);
    log_d("%s: Machine State: %d", binName, thisBinTracker->binMachineState);
    log_d("%s: Bin Lid State: %d", binName, thisBinTracker->binData.binLidState);

    // Slowly increase and decrease the brightness, ideally with ease-in/-out
    const uint8_t startBright = 50;
    const uint8_t endBright = 100;

    // Per bin variables
    uint8_t currBright = startBright;
    fader fadeDirection = fader::RIGHT;

    // Which pixel are we currently lighting up?
    uint8_t currPixel = 0;

    // Set initial colour & brightness
    setBinLidStateColour(thisBinTracker, fadeDirection, currPixel);
    binLEDs[thisBin].setBrightness(startBright);

    // Do forever
    for (;;) {
        taskYIELD();
        switch (fadeDirection) {
        case fader::UP:
            if (currBright > endBright) {
                fadeDirection = fader::DOWN;
                log_v("%s: Switch to decreasing", binName);
                break;
            }

            binLEDs[thisBin].setBrightness(currBright++);
            log_v("%s: Brightness: %d", binName, currBright + 1);
            binLEDs[thisBin].show();

            // Ease in
            vTaskDelay(easeInOutCubic((float)currBright / 200.0, thisBin));
            break;

        case fader::DOWN:
            if (currBright < startBright) {
                fadeDirection = fader::UP;
                log_v("%s: Switch to increasing", binName);
                break;
            }

            binLEDs[thisBin].setBrightness(currBright--);
            log_v("%s: Brightness: %d", binName, currBright - 1);
            binLEDs[thisBin].show();

            // Ease out
            vTaskDelay(easeInOutCubic((float)currBright / 200.0, thisBin));
            break;

        case fader::FLASH:
            static unsigned long flashMillis = 0;
            static uint8_t currFlash = 0;

            if (millis() - flashMillis > 500) {
                currFlash = !currFlash;
                binLEDs[thisBin].setBrightness(currFlash ? endBright : startBright);
                binLEDs[thisBin].show();
                flashMillis = millis();
            }

            break;

        case fader::LEFT:
            binLEDs[thisBin].setPixelColor(currPixel, 0, 0, 0); // OFF

            // Shadow pixel
            if (currPixel < 15 || isQuietTime) {
                binLEDs[thisBin].setPixelColor(currPixel + 1, 0, 0, 0); // OFF
            }

            if (thisBinTracker->isLowVoltage) {
                binLEDs[thisBin].setPixelColor(--currPixel, 255, 255, 0); // yellow
            } else {
                if (isQuietTime) {
                    binLEDs[thisBin].setPixelColor(--currPixel, 0, 0, 0); // off
                } else {
                    binLEDs[thisBin].setPixelColor(--currPixel, 10, 250, 10); // forest green
                }
            }

            // Shadow pixel
            if (thisBinTracker->isLowVoltage) {
                binLEDs[thisBin].setPixelColor(currPixel + 1, 255, 250, 0); // yellow
            } else {
                if (isQuietTime) {
                    binLEDs[thisBin].setPixelColor(currPixel + 1, 0, 0, 0); // off
                } else {
                    binLEDs[thisBin].setPixelColor(currPixel + 1, 10, 250, 10); // forest green
                }
            }

            if (currPixel < 1) {
                fadeDirection = fader::RIGHT;
                binLEDs[thisBin].setBrightness(startBright);
                // log_d("Bin [%d] LEFT fader changing direction at pixel %d", thisBin, currPixel);
            }

            // binLEDs[thisBin].show();
            binLEDs[thisBin].show();
            vTaskDelay(40 + ((thisBin + 1) * 5));
            break;

        case fader::RIGHT:
            binLEDs[thisBin].setPixelColor(currPixel, 0, 0, 0); // OFF
            if (isQuietTime) {
                binLEDs[thisBin].setPixelColor(++currPixel, 0, 0, 0); // off
            } else {
                binLEDs[thisBin].setPixelColor(++currPixel, 10, 250, 10); // forest green
            }

            if (currPixel > 14) {
                fadeDirection = fader::LEFT;
                binLEDs[thisBin].setBrightness(startBright);
                // log_d("Bin [%d] RIGHT fader changing direction at pixel %d", thisBin, currPixel);
            }

            binLEDs[thisBin].show();
            vTaskDelay(40 + ((thisBin + 1) * 5));
            break;

        default:
            log_w("%s: Increase/decrease invalid: %d", binName, fadeDirection);
        }

        // Check colour hasn't changed due to state change.
        // Jan 2023: Changes if the battery is low
        // Mar 2023: Turns off if in "quiet period, overnight"
        setBinLidStateColour(thisBinTracker, fadeDirection, currPixel);

        // Check memory on stack
        checkStack(thisBinTracker->binData.binID - 1);
    }
}

// Adjust NeoPixel colour depending on machine state etc
void setBinLidStateColour(void* parameter, fader& fadeDirection, uint8_t& currPixel)
{

    // Local machine state tracker
    // TODO: incorporate this into main struct
    static machineState prevMachineState[3] = { machineState::IDLE };

    // binTracker de-reference
    binTrackers binTracker = *((binTrackers*)parameter);
    uint8_t thisBin = binTracker.binData.binID - 1;

    // On Change in machine state (for this bin)
    if (prevMachineState[thisBin] != binTracker.binMachineState) {

        // Debug
        log_d("Bin [%d]: Machine state change for \"Bin-%d\"", thisBin, binTracker.binData.binID);
        log_d("Bin [%d]: Machine State: %d", thisBin, binTracker.binMachineState);
        log_d("Bin [%d]: Bin Lid State: %d", thisBin, binTracker.binData.binLidState);
        log_d("Bin [%d]: Previous machine state: %d, new state: %d", thisBin, prevMachineState[thisBin], binTracker.binMachineState);

        // Keep track of local bin state
        prevMachineState[thisBin] = binTracker.binMachineState;

        // Set all pixels to desired colour depending on state
        switch (binTracker.binMachineState) {
        case machineState::RESTING:
            if (binTracker.isLowVoltage) {
                binLEDs[thisBin].setPixelColor(0, 255, 255, 0); // yellow
            } else {
                binLEDs[thisBin].setPixelColor(0, 10, 250, 10);
            }

            log_d("Bin [%d] single pixel %d set", thisBin, 0);
            fadeDirection = fader::RIGHT;
            currPixel = 0;
            break;

        case machineState::IDLE:
            for (int i = 0; i < CIRCLE_LEDS; i++) {
                if (binTracker.isLowVoltage) {
                    binLEDs[thisBin].setPixelColor(i, 255, 255, 0); // yellow
                } else {
                    binLEDs[thisBin].setPixelColor(i, 0, 255, 0);
                }
            }
            fadeDirection = fader::UP;
            break;

        case machineState::ACTIVE:
            for (int i = 0; i < CIRCLE_LEDS; i++) {
                if (binTracker.isLowVoltage) {
                    binLEDs[thisBin].setPixelColor(i, 255, 153, 0); // orange
                } else {
                    binLEDs[thisBin].setPixelColor(i, 255, 0, 0);
                }
            }
            fadeDirection = fader::UP;
            break;

        case machineState::LONGACTIVE:
            for (int i = 0; i < CIRCLE_LEDS; i++) {
                if (binTracker.isLowVoltage) {
                    binLEDs[thisBin].setPixelColor(i, 204, 51, 0); // rust
                } else {
                    binLEDs[thisBin].setPixelColor(i, 0, 0, 255);
                }
            }
            fadeDirection = fader::FLASH;
            break;

        default:
            log_w("Bin [%d]: Unknown machine state: %d", thisBin, binTracker.binMachineState);
        }
    }
} // namespace neo

// Simple pulse function to alter the delay more at low intensity and speed up at high intensity
uint8_t easeInOutCubic(float progressDelay, uint8_t binNo)
{
    static unsigned long counter = 0;
    float result = progressDelay < 0.5 ? 4 * progressDelay * progressDelay * progressDelay : 1 - pow(-2 * progressDelay + 2, 3) / 2;
    uint8_t finalDelay = (20 - (uint8_t)(result * 19.1));

    if (CORE_DEBUG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE) {
        printf("%5.2f @ %d \t", progressDelay, finalDelay);
        if (counter++ % 10 == 0)
            printf("\n");
    }

    // Master delay (adjusted so they deliberately get out of step)
    uint8_t masterDelay = (5 + ((binNo + 1) * 2));

    // All done
    return finalDelay + masterDelay;
}

// Check stack space
void checkStack(uint8_t binNo)
{
    static unsigned long prevMillis[3] = { 0 };
    if (prevMillis[binNo] == 0 || millis() - prevMillis[binNo] > (60000 * 60)) {
        unsigned long remainingStack = uxTaskGetStackHighWaterMark(NULL);
        log_i("Bin [%d]: Free stack:%lu", binNo, remainingStack);
        prevMillis[binNo] = millis();
    }
}

// Task to set a volatile variable whether we are in "quiet time"
/*
void checkQuietHours(void* pvparameters)
{
    // Determine whether we are in "quiet time" in order to mute the LEDs
    static unsigned long prevTimeMillis = 0;

    for (;;) {
        // Only get the time every X (* 1000 mS) seconds
        if (millis() - prevTimeMillis > (10 * 1000)) {
            prevTimeMillis = millis();

            // Read the time from the DS3231 RTC
            rtc.read(&timeinfo);

            // See http://www.cplusplus.com/reference/ctime/strftime/
            char output[80];
            strftime(output, 80, "%H:%M:%S", &timeinfo);
            log_v("Current time: %s", output);

            // If it is now within the "quiet time"
            if (timeinfo.tm_hour >= QUIETTIME_START || timeinfo.tm_hour < QUIETTIME_END) {
                // Turn off all LEDs
                if (!isQuietTime) {
                    log_i("Quiet Time Starts @ %02d:%02d: turning off all LEDs", timeinfo.tm_hour, timeinfo.tm_min);
                    isQuietTime = true;
                }
            } else {
                // Turn the LEDs back on, as it is no longer quiet time
                if (isQuietTime) {
                    log_i("Quiet Time Ends @ %02d:%02d: turning on all LEDs", timeinfo.tm_hour, timeinfo.tm_min);
                    isQuietTime = false;
                }
            }
        } else {
            taskYIELD();
        }
    }
}
*/
} // namespace neo