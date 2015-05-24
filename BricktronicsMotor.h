/*
    Bricktronics library for LEGO NXT motors.

    Copyright (C) 2014 Adam Wolf, Matthew Beckler, John Baichtal

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

    Wayne and Layne invests time and resources providing this open-source
    code, please support W&L and open-source hardware by purchasing products
    from https://store.wayneandlayne.com/ - Thanks!

    Wayne and Layne, LLC and our products are not connected to or endorsed by the LEGO Group.
    LEGO, Mindstorms, and NXT are trademarks of the LEGO Group.
*/


#ifndef BRICKTRONICSMOTOR_H
#define BRICKTRONICSMOTOR_H

// Arduino header files
#include <stdint.h>
#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

// Library header files
#include <Encoder.h>
#include <PID_v1.h>
#include "utility/BricktronicsSettings.h"

// These are the default motor PID values for P, I, and D.
// Using Ziegler-Nichols, Ku = 4.4, Tu oscillation period about 41 in 15s = 0.3658536585
#define BRICKTRONICS_MOTOR_PID_KP                        2.64
#define BRICKTRONICS_MOTOR_PID_KI                        14.432
#define BRICKTRONICS_MOTOR_PID_KD                        0.1207317073
// We can have different motor modes, for now, speed and position
#define BRICKTRONICS_MOTOR_PID_MODE_DISABLED             0
#define BRICKTRONICS_MOTOR_PID_MODE_POSITION             1
#define BRICKTRONICS_MOTOR_PID_MODE_SPEED                2
// Sample time - Call update() as often as you can, but it will only update
// as often as this value. Can be updated by the user at runtime if desired.
#define BRICKTRONICS_MOTOR_PID_SAMPLE_TIME_MS            50

#define BRICKTRONICS_MOTOR_ANGLE_MULTIPLIER_DEFAULT      1
// Epsilon is used to evaluate if we are at a desired position (abs(getPosition() - desiredPosition) < epsilon)
#define BRICKTRONICS_MOTOR_EPSILON_DEFAULT               5
// This constant is used to determine if the PID algorithm has settled down enough to stop calling update() and just call stop()
// Used to try and avoid overshoot by stopping PID updates too early.
#define BRICKTRONICS_MOTOR_PID_OUTPUT_SETTLED_THRESHOLD  30

class BricktronicsMotor
{
    public:
        // Constructor - Simple constructor accepts the five motor pins
        BricktronicsMotor(uint8_t enPin,
                          uint8_t dirPin,
                          uint8_t pwmPin,
                          uint8_t encoderPin1,
                          uint8_t encoderPin2):
            _enPin(enPin),
            _dirPin(dirPin),
            _pwmPin(pwmPin),
            _enabled(false),
            _rawSpeed(0),
            _pid(&_pidInput, &_pidOutput, &_pidSetpoint, BRICKTRONICS_MOTOR_PID_KP, BRICKTRONICS_MOTOR_PID_KI, BRICKTRONICS_MOTOR_PID_KD, REVERSE),
            _pidKp(BRICKTRONICS_MOTOR_PID_KP),
            _pidKi(BRICKTRONICS_MOTOR_PID_KI),
            _pidKd(BRICKTRONICS_MOTOR_PID_KD),
            _pidMode(BRICKTRONICS_MOTOR_PID_MODE_DISABLED),
            _encoder(encoderPin1, encoderPin2),
            _angleMultiplier(BRICKTRONICS_MOTOR_ANGLE_MULTIPLIER_DEFAULT),
            _epsilon(BRICKTRONICS_MOTOR_EPSILON_DEFAULT),
            _pinMode(&::pinMode),
            _digitalWrite(&::digitalWrite),
            _digitalRead(&::digitalRead)
        {
            _pid.SetSampleTime(BRICKTRONICS_MOTOR_PID_SAMPLE_TIME_MS);
            _pid.SetOutputLimits(-255, +255);
        }

        // Constructor - Advanced constructor accepts a BricktronicsMotorSettings struct
        // to also override the low-level Arduino functions.
        BricktronicsMotor(const BricktronicsMotorSettings &settings):
            _enPin(settings.enPin),
            _dirPin(settings.dirPin),
            _pwmPin(settings.pwmPin),
            _enabled(false),
            _rawSpeed(0),
            _pid(&_pidInput, &_pidOutput, &_pidSetpoint, BRICKTRONICS_MOTOR_PID_KP, BRICKTRONICS_MOTOR_PID_KI, BRICKTRONICS_MOTOR_PID_KD, REVERSE),
            _pidKp(BRICKTRONICS_MOTOR_PID_KP),
            _pidKi(BRICKTRONICS_MOTOR_PID_KI),
            _pidKd(BRICKTRONICS_MOTOR_PID_KD),
            _pidMode(BRICKTRONICS_MOTOR_PID_MODE_DISABLED),
            _encoder(settings.encoderPin1, settings.encoderPin2),
            _angleMultiplier(BRICKTRONICS_MOTOR_ANGLE_MULTIPLIER_DEFAULT),
            _epsilon(BRICKTRONICS_MOTOR_EPSILON_DEFAULT),
            _pinMode(settings.pinMode),
            _digitalWrite(settings.digitalWrite),
            _digitalRead(settings.digitalRead)
        {
            _pid.SetSampleTime(BRICKTRONICS_MOTOR_PID_SAMPLE_TIME_MS);
            _pid.SetOutputLimits(-255, +255);
        }

        // TODO Reconsider these functions, if they are a good idea, or if they are even needed...
        // Set the dir/pwm/en pins as outputs and stops the motor.
        void begin(void)
        {
            _pid.SetMode(AUTOMATIC);
            _enabled = true;
            // Set timer1 frequency to about 32khz to reduce audible whine
            TCCR1B = (TCCR1B & 0b11111000) | 0x01;
            stop();
            _pinMode(_dirPin, OUTPUT);
            _pinMode(_pwmPin, OUTPUT);
            _pinMode(_enPin, OUTPUT);
        }

        // Another name for begin()
        void enable(void)
        {
            begin();
        }

        // Set the dir/pwm/en pins as inputs
        void disable(void)
        {
            _enabled = false;
            _pinMode(_dirPin, INPUT);
            _pinMode(_pwmPin, INPUT);
            _pinMode(_enPin, INPUT);
        }

        // Set the dir/pwm/en pins to LOW, turning off the motor
        void stop(void)
        {
            _digitalWrite(_enPin, LOW);
            _digitalWrite(_dirPin, LOW);
            _digitalWrite(_pwmPin, LOW);
        }
        // TODO what about braking and coasting? Replace stop() with brake(strength?) and coast()


        // Read the encoder's current position.
        int32_t getPosition(void)
        {
            return _encoder.read();
        }
        // Write the encoder's current position - This will mess up any control in progress!
        //     This only sets the number corresponding to the motor's current position.
        //     Usually you just want to reset the position to zero.
        void setPosition(int32_t pos)
        {
            _encoder.write(pos);
        }
        // Motors have some slop in their encoder output readings, so these two functions
        // can be used to make a "close enough?" check. The epsilon value can be get/set
        // using these functions, and is used in the atPosition check like this:
        // return (abs(getPosition() - position) < _epsilon);
        // This function also checks to ensure that the PID algorithm has settled down enough
        // (that is, _pidOutput < BRICKTRONICS_MOTOR_PID_OUTPUT_SETTLED_THRESHOLD) that we can just stop()
        // without having to worry about coasting through the setpoint.
        // TODO make _epsilon unsigned
        bool settledAtPosition(int32_t position)
        {
            return (    (abs(getPosition() - position) < _epsilon)
                     && (abs(_pidOutput) < BRICKTRONICS_MOTOR_PID_OUTPUT_SETTLED_THRESHOLD) );
        }

        void setEpsilon(int8_t epsilon)
        {
            _epsilon = epsilon;
        }
        int8_t getEpsilon(void)
        {
            return _epsilon;
        }


        // Some of the functions below need to periodically check on the motor's operation
        // and update data. Use this update() call to do that.
        // Call this function as often as you can, since it will only actually update as
        // often as the frequency setpoint (defaults to 50ms), which can be updated below.
        void update(void)
        {
            switch (_pidMode)
            {
                case BRICKTRONICS_MOTOR_PID_MODE_POSITION:
                    _pidInput = _encoder.read();
                    if( abs(_pidInput - _pidSetpoint) < 5 )
                    {
                        pidUpdateTuningsConservative(8);
                    }
                    else
                    {
                        pidUpdateTunings();
                    }
                    _pid.Compute();
                    rawSetSpeed(_pidOutput);
                    break;

                case BRICKTRONICS_MOTOR_PID_MODE_SPEED:
                    // TODO how can we determine the current speed if this is being called frequently
                    break;

                default: // includes BRICKTRONICS_MOTOR_PID_MODE_DISABLED
                    break;
            }
        }

        // This function periodically calls the motor's update() function until
        // delayMS milliseconds have elapsed. Useful if you have nothing else to do.
        void delayUpdateMS(int delayMS)
        {
            unsigned long endTime = millis() + delayMS;
            while (millis() < endTime)
            {
                update();
                // We could put a delay(5) here, but the PID library already has a 
                // "sample time" parameter to only run so frequent, you know?
            }
        }


        // PID related functions
        // Update the maximum frequency at which the PID algorithm will actually update.
        void pidSetUpdateFrequencyMS(int timeMS)
        {
            _pid.SetSampleTime(timeMS);
        }

        // Print out the PID values
        void pidPrintValues(void)
        {
            Serial.print("SET:");
            Serial.println(_pidSetpoint);
            Serial.print("INP:");
            Serial.println(_pidInput);
            Serial.print("OUT:");
            Serial.println(_pidOutput);
        }

        // Functions for getting and setting the PID tuning parameters
        double pidGetKp(void)
        {
            return _pid.GetKp();
        }

        double pidGetKi(void)
        {
            return _pid.GetKi();
        }

        double pidGetKd(void)
        {
            return _pid.GetKd();
        }

        void pidSetTunings(double Kp, double Ki, double Kd)
        {
            _pidKp = Kp;
            _pidKi = Ki;
            _pidKd = Kd;
            pidUpdateTunings();
        }

        void pidUpdateTuningsConservative(double divisor)
        {
            _pid.SetTunings(_pidKp / divisor, _pidKi / divisor, _pidKd / divisor);
        }
        void pidUpdateTunings(void)
        {
            _pid.SetTunings(_pidKp, _pidKi, _pidKd);
        }


        void pidSetKp(double Kp)
        {
            _pidKp = Kp;
            pidUpdateTunings();
        }

        void pidSetKi(double Ki)
        {
            _pidKi = Ki;
            pidUpdateTunings();
        }

        void pidSetKd(double Kd)
        {
            _pidKd = Kd;
            pidUpdateTunings();
        }


        // Raw, uncontroleld speed settings
        // There is no control of the speed here,
        // just set a value between -255 and +255 (0 = stop).
        void rawSetSpeed(int16_t s)
        {
            _rawSpeed = s;
            if (s == 0)
            {
                stop();
            }
            else if (s < 0)
            {
                _digitalWrite(_dirPin, HIGH);
                analogWrite(_pwmPin, 255 + s);
                _digitalWrite(_enPin, HIGH);
            }
            else
            {
                _digitalWrite(_dirPin, LOW);
                analogWrite(_pwmPin, s);
                _digitalWrite(_enPin, HIGH);
            }
        }

        // Retrieves the previously-set raw speed.
        int16_t rawGetSpeed(void)
        {
            return _rawSpeed;
        }


        // Position control functions
        void goToPosition(int32_t position)
        {
            // Swith our internal PID into position mode
            _pidMode = BRICKTRONICS_MOTOR_PID_MODE_POSITION;
            _pidSetpoint = position;
        }

        // Go to the specified position using PID, but wait until the motor arrives
        void goToPositionWait(int32_t position)
        {
            goToPosition(position);
            while (!settledAtPosition(position))
            {
                update();
            }
            stop();
        }

        // Same as above, but return after timeoutMS milliseconds in case it gets stuck
        // Returns true if we made it to position, false if we had a timeout
        bool goToPositionWaitTimeout(int32_t position, uint32_t timeoutMS)
        {
            goToPosition(position);
            timeoutMS += millis(); // future time when we timeout
            while ( !settledAtPosition(position) && (millis() < timeoutMS) )
            {
                update();
            }
            stop();
            if (millis() >= timeoutMS)
            {
                return false;
            }
            return true;
        }

        // Angle control functions - 0 - 359, handles discontinuity nicely.
        // Can specify any angle, positive or negative. If you say 
        // "go to angle 721" it will be the same as "go to angle 1".
        // Similarly, "go to angle -60" will be "go to angle 300".
        // If you want "go 45 degrees clockwise from here", try using
        // m.goToAngle(m.getAngle() + 45);
        // The fancy angle math is in _getDestPositionFromAngle, these functions just
        // use the result of that function with their corresponding goToPosition*.
        void goToAngle(int32_t angle)
        {
            goToPosition(_getDestPositionFromAngle(angle));
        }

        // Go to the specified angle using PID, but wait until the motor arrives
        void goToAngleWait(int32_t angle)
        {
            goToPositionWait(_getDestPositionFromAngle(angle));
        }

        // Same as above, but return after timeoutMS milliseconds in case it gets stuck
        // Returns true if we made it to the desired position, false if we had a timeout
        bool goToAngleWaitTimeout(int32_t angle, uint32_t timeoutMS)
        {
            return goToPositionWaitTimeout(_getDestPositionFromAngle(angle), timeoutMS);
        }

        uint16_t getAngle(void) // Returns the current angle (0-359)
        {
            return ( (getPosition() / _angleMultiplier) % 360 );
        }

        void setAngle(int32_t angle) // Updates the encoder position to be the specified angle
        {
            setPosition((angle % 360) * _angleMultiplier);
        }

        // For the angle control, the user can specify a different multiplier
        // between motor encoder ticks and "output rotations", defaults to 1.
        // Use this setting if your motor is connected to a gear train that
        // makes a different number of motor rotations per output rotation.
        // For example, if you have a 5:1 gear train between your motor and
        // the final output, then you can specify this value as 5.
        // Negative numbers should work just fine.
        // TODO Using integer angles means we can't do sub-degree positioning,
        // which only really becomes noticable if we scale-up the output by a lot.
        void setAngleOutputMultiplier(int8_t multiplier)
        {
            // Since the LEGO NXT motor encoders have 720 ticks per 360 degrees,
            // we have to double the user's specified multiplier.
            _angleMultiplier = multiplier << 1;
        }



    //private:
        // We really don't like to hide things inside private,
        // but if we did, these would be the private items.
        uint8_t _enPin;
        uint8_t _dirPin;
        uint8_t _pwmPin;

        bool _enabled;
        uint16_t _rawSpeed;

        // PID variables
        PID _pid;
        uint8_t _pidMode;
        double _pidSetpoint, _pidInput, _pidOutput;
        double _pidKp, _pidKi, _pidKd;

        // Tracks the position of the motor
        Encoder _encoder;

        // Check out the comments above for setAngleOutputMultiplier()
        int8_t _angleMultiplier;

        // Uses the current angle to determine the desired destination
        // position of the motor. Used in the goToAngle* functions.
        int32_t _getDestPositionFromAngle(int32_t angle)
        {
            int16_t delta = (angle % 360) - getAngle();

            while (delta > 180)
            {
                delta -= 360;
            }
            while (delta < -180)
            {
                delta += 360;
            }

            // Now, delta is between -180 and +180
            return getPosition() + (delta * _angleMultiplier);
        }

        // When checking if the motor has reached a certain position,
        // there is likely to be a small amount of "slop", and it would be
        // unreasonable to stall forever trying to get to position 180 when
        // we are "only" at position 179. This value can be read/set using
        // the functions above, and is used in position check like this:
        // abs(getPosition() - checkPosition) > _epsilon
        uint8_t _epsilon;

        // For the Bricktronics Shield, which has an I2C I/O expander chip, we need a way to
        // override some common Arduino functions. We use function pointers here to handle this.
        // For the non-Bricktronics Shield cases, the simple constructor above provides the built-in functions.
        void (*_pinMode)(uint8_t, uint8_t);
        void (*_digitalWrite)(uint8_t, uint8_t);
        int (*_digitalRead)(uint8_t);
};

#endif // #ifdef BRICKTRONICSMOTOR_H

