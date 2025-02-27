#include "Drive.h"

#include "frc/kinematics/SwerveModuleState.h"
#include <cmath>

#define ENCODER_OFFSETS_FILE_NAME "/home/lvuser/magnetic_encoder_offsets.txt"

Drive::Drive() {
    resetIMU();
    // Configure the calibration time to 4 seconds.
    imu.ConfigCalTime(frc::ADIS16470CalibrationTime::_4s);
    // Set axis for the gryo to take (Z is up and down).
    imu.SetYawAxis(frc::ADIS16470_IMU::IMUAxis::kZ);
    // Calibrate the IMU.
    calibrateIMU();

    // Read encoder offsets from the file and apply them to the swerve modules.
    if (readOffsetsFile()) {
        applyOffsets();
    }
}

Drive::~Drive() {
    for (SwerveModule* module : swerveModules) {
        delete module;
    }
}

void Drive::reset() {
    cmdCancel();
    resetOdometry();
}

void Drive::process() {
    // hi nevin
    // hi jeff :D
    // Update the position on the field.
    updateOdometry();

    // Execute the current command.
    executeCommand();
}

frc::Pose2d Drive::getPose() {
    return odometry.GetPose();
}

void Drive::zeroRotation() {
    resetIMU();
}

void Drive::calibrateIMU() {
    imu.Calibrate();
}

void Drive::configMagneticEncoders() {
    // Apply the current rotation of the swerve modules to the offsets.
    for (unsigned i = 0; i < swerveModules.size(); i++) {
        units::radian_t angle = swerveModules.at(i)->getState().angle.Radians() - units::radian_t(wpi::math::pi / 2);
        offsets.at(i) += angle;
    }
    // Write the new offsets to the offsets file.
    writeOffsetsFile();

    // Apply the new offsets to the swerve modules.
    applyOffsets();
}

void Drive::manualDrive(double xPct, double yPct, double rotPct) {
    // Take control if drive command is running.
    cmdCancel();

    units::meters_per_second_t xVel    = xPct   * DRIVE_MAX_SPEED;
    units::meters_per_second_t yVel    = yPct   * DRIVE_MAX_SPEED;
    units::radians_per_second_t rotVel = rotPct * DRIVE_MAX_ANGULAR_SPEED;
    
    frc::ChassisSpeeds chassisSpeeds;

    // Generate chassis speeds depending on the control mode.
    switch (controlMode) {
        case FIELD_CENTRIC:
            chassisSpeeds = frc::ChassisSpeeds::FromFieldRelativeSpeeds(xVel, yVel, rotVel, getRotation());
            break;
        case ROBOT_CENTRIC:
            chassisSpeeds = { xVel, yVel, rotVel };
            break;
    }
    
    // Set the modules to turn based on the speeds of the entire chassis.
    setModuleStates(chassisSpeeds);
}

void Drive::setControlMode(ControlMode mode) {
    controlMode = mode;
}

void Drive::makeBrick() {
    for (unsigned i = 0; i < swerveModules.size(); i++) {
        units::degree_t angle;
        // If even index.
        if (i % 2 == 0) {
            angle = -45_deg;
        }
        // If odd index.
        else {
            angle = 45_deg;
        }
        // Stop the drive motor.
        swerveModules.at(i)->setDriveMotor(0_mps);
        // Turn the swerve module to point towards the center of the robot.
        swerveModules.at(i)->setTurningMotor(angle);
    }
}

void Drive::cmdRotate(frc::Rotation2d angle) {
    // Create a trajectory to rotate the specified angle.
    frc::Trajectory trajectory = frc::TrajectoryGenerator::GenerateTrajectory(
        getPose(), {}, { 0_m, 0_m, angle }, AUTO_TRAJECTORY_CONFIG);

    // Start a follow trajectory command.
    cmdFollowTrajectory(trajectory);
}

void Drive::cmdDrive(units::meter_t x, units::meter_t y, frc::Rotation2d angle) {
    // Create a trajectory that drives the specified distance / turns the specified angle.

    frc::TrajectoryConfig config = AUTO_TRAJECTORY_CONFIG;
    // config.SetReversed(true);
    config.AddConstraint(frc::SwerveDriveKinematicsConstraint<4>(kinematics, AUTO_MAX_SPEED));

    frc::Trajectory trajectory = frc::TrajectoryGenerator::GenerateTrajectory(
        getPose(), {}, { x, y, angle }, config); 

    // Start a follow trajectory command.
    cmdFollowTrajectory(trajectory);
}

void Drive::cmdFollowPathweaverTrajectory(std::string path) {
    // std::string directory = "";
    // frc::filesystem::GetDeployDirectory(directory);
    // Load trajectory from Pathweaver json file.
    cmdFollowTrajectory(frc::TrajectoryUtil::FromPathweaverJson(path));
}

void Drive::cmdFollowTrajectory(frc::Trajectory trajectory) {
    cmd.trajectory = trajectory;
    cmd.timer.Reset();
    cmd.timer.Start();
    cmd.running = true;
    units::second_t totalTime = trajectory.TotalTime();

    auto start = trajectory.Sample(totalTime).pose;
    std::cout << "initial pose: " << start.X() << ", " << start.Y() << ", " << start.Rotation().Degrees() << '\n';
    auto low = trajectory.Sample(totalTime/4).pose;
    std::cout << "a little below mid pose: " << low.X() << ", " << low.Y() << ", " << low.Rotation().Degrees() << '\n';
    auto mid = trajectory.Sample(totalTime/2).pose;

    std::cout << "mid pose: " << mid.X() << ", " << mid.Y() << ", " << mid.Rotation().Degrees() << '\n';
    auto high = trajectory.Sample((totalTime/4) * 3).pose;
    std::cout << "a little higher than mid: " << high.X() << ", " << high.Y() << ", " << high.Rotation().Degrees() << '\n';
    auto last = trajectory.Sample(totalTime).pose;
    std::cout << "final pose: " << last.X() << ", " << last.Y() << ", " << last.Rotation().Degrees() << '\n';
    
    std::cout << "FOLLOW TRAJECTORY STARTED\n";
}

bool Drive::cmdIsFinished() {
    // If a command is currently running, but the total time of the trajectory has elapsed.
    if (cmd.running && cmd.timer.Get() >= cmd.trajectory.TotalTime().value()) {
        // Stop the command.
        cmdCancel();
        // Set the wheel speeds to zero.
        setModuleStates({0_mps, 0_mps, 0_rad_per_s});
        std::cout << "FOLLOW TRAJECTORY ENDED\n";
    }
    return !cmd.running;
}

void Drive::cmdCancel() {
    cmd.timer.Stop();
    cmd.running = false;
}

void Drive::setModuleStates(frc::ChassisSpeeds chassisSpeeds) {
    // Generate module states using chassis velocities. The magic function of swerve drive.
    wpi::array<frc::SwerveModuleState, 4> moduleStates = kinematics.ToSwerveModuleStates(chassisSpeeds);
    
    // Recalculate wheel speeds relative to the max speed.
    kinematics.NormalizeWheelSpeeds(&moduleStates, DRIVE_MAX_SPEED);
    
    // Set the module states.
    for(unsigned i = 0; i < swerveModules.size(); i++) {
        swerveModules.at(i)->setState(moduleStates.at(i));
    }
}

void Drive::updateOdometry() {
    // Update the position and rotation on the field.
    odometry.Update(getRotation(),
        swerveModules.at(0)->getState(),
        swerveModules.at(1)->getState(),
        swerveModules.at(2)->getState(),
        swerveModules.at(3)->getState());
}

void Drive::resetOdometry(frc::Pose2d pose) {
    // Reset the position on the field.
    odometry.ResetPosition(pose, getRotation());
}

void Drive::resetIMU() {
    imu.Reset();
}

frc::Rotation2d Drive::getRotation() {
    units::radian_t rotation = units::degree_t(fmod(imu.GetAngle(), 360));

    // Convert -2π to 2π value into -π to π value.
    if(units::math::abs(rotation).value() > wpi::math::pi) {
        rotation = units::radian_t(rotation.value() - (2 * wpi::math::pi) * (std::signbit(rotation.value()) ? -1 : 1));
    }
    
    return frc::Rotation2d(units::degree_t(rotation));
}

void Drive::executeCommand() {
    // Only execute a command if the command is running.
    if (!cmdIsFinished()) {
        // Get the current time of the trajectory.
        units::second_t currentTime = units::second_t(cmd.timer.Get());
        // Sample the desired state of the trajectory at this point in time.
        frc::Trajectory::State desiredState = cmd.trajectory.Sample(currentTime);
        // Calculate chassis speeds required in order to reach the desired state.
        frc::ChassisSpeeds targetChassisSpeeds = cmdController.Calculate(
            getPose(), desiredState, cmd.trajectory.States().back().pose.Rotation());
            
        // std::cout << "target x: " << targetChassisSpeeds.vx << " target y: " << targetChassisSpeeds.vy << " target rot: " << targetChassisSpeeds.omega << '\n';
        // Finally drive!
        setModuleStates(targetChassisSpeeds);
    }

    //hi ishan
    //hi jeff
    //hi trevor
}

bool Drive::readOffsetsFile() {
    // Open the file.
    std::ifstream file(ENCODER_OFFSETS_FILE_NAME);
    // Make sure the file exists.
    if (!file) {
        return false;
    }

    unsigned i = 0;
    std::string line;
    // Loop through each line of the file.
    while (getline(file, line) && i <= 3) {
        // Convert the line to a number.
        double num = std::atof(line.c_str());
        
        // If it can read a value from the file.
        if (num) {
            // Set the offset in the array.
            offsets.at(i) = units::radian_t(num);
        }
        // Increment the index.
        i++;
    }

    return true;
    //hi nadia
}

void Drive::writeOffsetsFile() {
    // Open the file (will create a new file if already exists).
    std::ofstream file(ENCODER_OFFSETS_FILE_NAME);
    // Clear the file's contents.
    file.clear();

    // Write each offset to the file.
    for (units::radian_t offset : offsets) {
        file << offset.value() << '\n';
    }
}

void Drive::applyOffsets() {
    for (unsigned i = 0; i < swerveModules.size(); i++) {
        swerveModules.at(i)->setOffset(offsets.at(i));
    }
}