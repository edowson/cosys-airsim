
#ifndef msr_air_copter_sim_SimpleImu_hpp
#define msr_air_copter_sim_SimpleImu_hpp

#include "common/Common.hpp"
#include "ImuSimpleParams.hpp"
#include "ImuBase.hpp"

namespace msr { namespace airlib {

class ImuSimple : public ImuBase {
public:
    //constructors
    ImuSimple()
    {
        ImuSimple::reset();
    }
    ImuSimple(GroundTruth* ground_truth)
    {
        initialize(ground_truth);
    }
    void initialize(GroundTruth* ground_truth)
    {
        ImuBase::initialize(ground_truth);

        gyro_bias_stability_norm = params_.gyro.bias_stability / sqrt(params_.gyro.tau);
        accel_bias_stability_norm = params_.accel.bias_stability / sqrt(params_.accel.tau);

        ImuSimple::reset();
    }

    //*** Start: UpdatableState implementation ***//
    virtual void reset() override
    {
        state_.gyroscope_bias = params_.gyro.turn_on_bias;
        state_.accelerometer_bias = params_.accel.turn_on_bias;
        gauss_dist.reset();
        updateOutput(0);
    }

    virtual void update(real_T dt) override
    {
        updateOutput(dt);
    }
    //*** End: UpdatableState implementation ***//

    virtual ~ImuSimple() = default;

private: //methods
    void updateOutput(real_T dt)
    {
        Output output;
        const GroundTruth& ground_truth = getGroundTruth();

        output.angular_velocity = ground_truth.kinematics->twist.angular;
        output.linear_acceleration = ground_truth.kinematics->accelerations.linear - ground_truth.environment->getState().gravity;
        output.orientation = ground_truth.kinematics->pose.orientation;

        //acceleration is in world frame so transform to body frame
        output.linear_acceleration = VectorMath::transformToBodyFrame(output.linear_acceleration, 
            ground_truth.kinematics->pose.orientation, true);

        //add noise
        addNoise(output.linear_acceleration, output.angular_velocity, dt);
        // TODO: Add noise in orientation?

        setOutput(output);
    }

    void addNoise(Vector3r& linear_acceleration, Vector3r& angular_velocity, float dt)
    {
        //ref: An introduction to inertial navigation, Oliver J. Woodman, Sec 3.2, pp 10-12
        //https://www.cl.cam.ac.uk/techreports/UCAM-CL-TR-696.pdf

        real_T sqrt_dt = sqrt(std::max(dt, params_.min_sample_time));

        // Gyrosocpe
        //convert arw to stddev
        real_T gyro_sigma_arw = params_.gyro.arw / sqrt_dt;
        angular_velocity += gauss_dist.next() * gyro_sigma_arw + state_.gyroscope_bias;
        //update bias random walk
        real_T gyro_sigma_bias = gyro_bias_stability_norm * sqrt_dt;
        state_.gyroscope_bias += gauss_dist.next() * gyro_sigma_bias;

        //accelerometer
        //convert vrw to stddev
        real_T accel_sigma_vrw = params_.accel.vrw / sqrt_dt;
        linear_acceleration += gauss_dist.next() * accel_sigma_vrw + state_.accelerometer_bias;
        //update bias random walk
        real_T accel_sigma_bias = accel_bias_stability_norm * sqrt_dt;
        state_.accelerometer_bias += gauss_dist.next() * accel_sigma_bias;
    }


private: //fields
    ImuSimpleParams params_;
    RandomVectorGaussianR gauss_dist = RandomVectorGaussianR(0, 1);

    //cached calculated values
    real_T gyro_bias_stability_norm, accel_bias_stability_norm;

    struct State {
        Vector3r gyroscope_bias;
        Vector3r accelerometer_bias;
    } state_;
};


}} //namespace
#endif