#include "Controls.h"
#include <math.h>
#include "rev/CANSparkMax.h"

// Xbox button maps
#define A_BUTTON      1 // GetRawButton() give bool
#define B_BUTTON      2 // GetRawButton() give bool
#define X_BUTTON      3 // GetRawButton() give bool
#define Y_BUTTON      4 // GetRawButton() give bool
#define LEFT_BUMPER   5 // GetRawButton() give bool
#define RIGHT_BUMPER  6 // GetRawButton() give bool
#define BACK_BUTTON   7 // GetRawButton() give bool
#define START_BUTTON  8 // GetRawButton() give bool
#define LEFT_TRIGGER  2 // GetRawAxis() give double
#define RIGHT_TRIGGER 3 // GetRawAxis() give double
#define LEFT_X_AXIS   0 // GetRawAxis() give double
#define LEFT_Y_AXIS   1 // GetRawAxis() give double
#define RIGHT_X_AXIS  4 // GetRawAxis() give double
#define RIGHT_Y_AXIS  5 // GetRawAxis() give double
#define DPAD          0 // GetPOV() give int

#define AXIS_DEADZONE .05

Controls::Controls(Drive* drive) : drive(drive) {
#ifdef ENABLE_MUSIC
    for(unsigned i = 0; i < drive->swerveModules.size(); ++i) {
        orchestra.AddInstrument(drive->swerveModules[i]->driveMotor);
    }
#endif
}

void Controls::process() {
    bool zeroRotation            = controllerDriver.GetRawButton(Y_BUTTON);
    bool toggleDriveMode         = controllerDriver.GetRawButton(X_BUTTON);
    double xAxisVelocity         = controllerDriver.GetRawAxis(LEFT_X_AXIS);
    double yAxisVelocity         = controllerDriver.GetRawAxis(LEFT_Y_AXIS);
    double leftRotationVelocity  = controllerDriver.GetRawAxis(LEFT_TRIGGER);
    double rightRotationVelocity = controllerDriver.GetRawAxis(RIGHT_TRIGGER);
    int slowDriveDirection       = controllerDriver.GetPOV(DPAD);
    bool slowLeftVelocity        = controllerDriver.GetRawButton(LEFT_BUMPER);
    bool slowRightVelocity       = controllerDriver.GetRawButton(RIGHT_BUMPER);
    bool toggleSlowMode          = controllerDriver.GetRawButton(B_BUTTON);
#ifdef ENABLE_MUSIC
    bool toggleMusic             = controllerDriver.GetRawButton(A_BUTTON);
    if(toggleMusic) {
        if(!wasMusicToggled) {
            playMusic = !playMusic;
        }
    }
    wasMusicToggled = toggleMusic;
    
    bool isMusicPlaying = orchestra.IsPlaying();
    if(playMusic) {
        if(!isMusicPlaying) {
            orchestra.LoadMusic("home_depo_long.chrp");
            orchestra.Play();
        }
        return;
    } else {
        if(isMusicPlaying) {
            orchestra.Stop();
        }
    }
#endif

    if(zeroRotation) {
        drive->resetIMU();
    }

    if(toggleDriveMode) {
        if(!wasDriveModeToggled) {
            isFieldCentric = !isFieldCentric;
        }
    }
    wasDriveModeToggled = toggleDriveMode;

    if(toggleSlowMode) {
        if(!wasSlowModeToggled) {
            slowModeEnabled = !slowModeEnabled;
        }
    }
    wasSlowModeToggled = toggleSlowMode;

    double finalXAxis = 0.0;
    double finalYAxis = 0.0;
    double finalRotation = 0.0;

    // DPad not pressed, so normal mode.
    if(slowDriveDirection == -1) {
        if(slowModeEnabled){
            if(abs(xAxisVelocity) > AXIS_DEADZONE) {
                finalXAxis = (.2 * xAxisVelocity);
            }
            if(abs(yAxisVelocity) > AXIS_DEADZONE) {
                finalYAxis = (.2 * yAxisVelocity);
            }
        }
        else{
            if(abs(xAxisVelocity) > AXIS_DEADZONE) {
                finalXAxis = xAxisVelocity;
            }
            if(abs(yAxisVelocity) > AXIS_DEADZONE) {
                finalYAxis = yAxisVelocity;
            }
        }
    }
    // DPad is pressed, so slow mode.
    else {
        // DPad Up.
        if((slowDriveDirection >= 375) || (slowDriveDirection <= 45)) {
            finalYAxis = -.1;
        }
        // DPad Down.
        if((slowDriveDirection >= 135) && (slowDriveDirection <= 225)) {
            finalYAxis = .1;
        }
        // DPad Right.
        if((slowDriveDirection >= 45) && (slowDriveDirection <= 135)) {
            finalXAxis = .1;
        }
        // DPad Left.
        if((slowDriveDirection >= 225) && (slowDriveDirection <= 315)) {
            finalXAxis = -.1;
        }
    }
    // Bumper pressed, so slow mode.
    if(slowLeftVelocity || slowRightVelocity) {
        if(slowLeftVelocity && !slowRightVelocity) {
            finalRotation = -.25;
        }
        if(slowRightVelocity && !slowLeftVelocity) {
            finalRotation = .25;
        }
    }
    // Bumper not pressed, so normal mode.
    else {
        if(slowModeEnabled){
            if((leftRotationVelocity > 0) && (rightRotationVelocity == 0)) {
                finalRotation = -(.3 * leftRotationVelocity);
            }
            if((rightRotationVelocity > 0) && (leftRotationVelocity == 0)) {
                finalRotation = (.3 * rightRotationVelocity);
            }
        }
        else{
            if((leftRotationVelocity > 0) && (rightRotationVelocity == 0)) {
                finalRotation = -leftRotationVelocity;
            }
            if((rightRotationVelocity > 0) && (leftRotationVelocity == 0)) {
                finalRotation = rightRotationVelocity;
            }
        }
    }

    drive->setDrive(units::velocity::meters_per_second_t(-finalXAxis), units::velocity::meters_per_second_t(finalYAxis), units::radians_per_second_t(finalRotation), isFieldCentric);
}