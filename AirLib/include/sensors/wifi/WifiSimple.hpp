// Developed by Cosys-Lab, University of Antwerp

#ifndef msr_airlib_Wifi_hpp
#define msr_airlib_Wifi_hpp

#include <random>
#include "common/Common.hpp"
#include "WifiSimpleParams.hpp"
#include "WifiBase.hpp"
#include "common/DelayLine.hpp"
#include "common/FrequencyLimiter.hpp"

namespace msr { namespace airlib {

class WifiSimple : public WifiBase {
public:
	/*WifiSimple(const AirSimSettings::WifiSetting& setting = AirSimSettings::WifiSetting())
        : WifiBase(setting.sensor_name, setting.attach_link)
    {*/
	WifiSimple(const AirSimSettings::WifiSetting& setting = AirSimSettings::WifiSetting())
		: WifiBase(setting.sensor_name)
	{
        // initialize params
        params_.initializeFromSettings(setting);

        //initialize frequency limiter
		freq_limiter_.initialize(params_.update_frequency, params_.startup_delay, false);
    }

    //*** Start: UpdatableState implementation ***//
    virtual void reset() override
    {
		WifiBase::reset();

		freq_limiter_.reset();
		last_time_ = clock()->nowNanos();

		updateOutput();        

		WifiSensorData emptyInput;
		setInput(emptyInput);
    }

	virtual void update(float delta = 0) override
	{
		WifiBase::update(delta);
		freq_limiter_.update(delta);

		if (freq_limiter_.isWaitComplete())
		{
			last_time_ = freq_limiter_.getLastTime();
			if(params_.pause_after_measurement)pause(true);
			updateOutput();
			updateInput();
		}
    }

    virtual void reportState(StateReporter& reporter) override
    {
        //call base
		WifiBase::reportState(reporter);

		reporter.writeValue("Wifi-MeasurementFreq", params_.measurement_frequency);
    }
    //*** End: UpdatableState implementation ***//

    virtual ~WifiSimple() = default;

    const WifiSimpleParams& getParams() const
    {
        return params_;
    }

protected:
    //virtual void getPointCloud(const Pose& sensor_pose, const Pose& vehicle_pose, vector<real_T>& point_cloud) = 0;

	virtual void updatePose(const Pose& sensor_pose, const Pose& vehicle_pose) = 0;

	virtual void getLocalPose(Pose& sensor_pose) = 0;

	virtual void pause(const bool is_paused) = 0;

	//virtual void setPointCloud(const Pose& sensor_pose, vector<real_T>& point_cloud, TTimePoint time_stamp) = 0;

	virtual void updateWifiRays() = 0;

private:
	void updateOutput()
	{
		//point_cloud_.clear();

		const GroundTruth& ground_truth = getGroundTruth();
		Pose const pose_offset = params_.external ? Pose() : ground_truth.kinematics->pose;
		// calculate the pose before obtaining the point-cloud. Before/after is a bit arbitrary
		// decision here. If the pose can change while obtaining the point-cloud (could happen for drones)
		// then the pose won't be very accurate either way.
		//
		// TODO: Seems like pose is in vehicle inertial-frame (NOT in Global NED frame).
		//    That could be a bit unintuitive but seems consistent with the position/orientation returned as part of 
		//    ImageResponse for cameras and pose returned by getCameraInfo API.
		//    Do we need to convert pose to Global NED frame before returning to clients?

		double start = FPlatformTime::Seconds();
		/*getPointCloud(params_.relative_pose, // relative sensor pose
			ground_truth.kinematics->pose,   // relative vehicle pose			
			point_cloud_);*/
		updatePose(params_.relative_pose, pose_offset);
		updateWifiRays();
		double end = FPlatformTime::Seconds();
		UAirBlueprintLib::LogMessageString("Wifi: ", "Sensor data generation took " + std::to_string(end - start), LogDebugLevel::Informational);

		WifiSensorData output;

		//output.point_cloud = point_cloud_;
		output.time_stamp = last_time_;
		if (params_.external && params_.external_ned) {
			getLocalPose(output.pose);
		}
		else {
			output.pose = params_.relative_pose;
		}

		std::vector<float> beaconsActiveRSSI;
		std::vector<std::string> beaconsActiveID;
		std::vector<float> beaconsActivePosX;
		std::vector<float> beaconsActivePosY;
		std::vector<float> beaconsActivePosZ;
		for (int i = 0; i < beaconsActive_.Num(); i++) {
			for (int ii = 0; ii < beaconsActive_[i].Num(); ii++) {
				beaconsActiveRSSI.push_back(beaconsActive_[i][ii].rssi);
				//std::string tmpName, tmpId;
				//beaconsActive_[i][ii].beaconID.Split(TEXT("_"), &tmpName, &tmpId);
				//beaconsActiveID.push_back(FCString::Atoi(*tmpId));
				beaconsActiveID.push_back(beaconsActive_[i][ii].beaconID);
				beaconsActivePosX.push_back(beaconsActive_[i][ii].beaconPosX);
				beaconsActivePosY.push_back(beaconsActive_[i][ii].beaconPosY);
				beaconsActivePosZ.push_back(beaconsActive_[i][ii].beaconPosZ);
			}
		}
		output.beaconsActiveRssi = beaconsActiveRSSI;
		output.beaconsActiveID = beaconsActiveID;
		output.beaconsActivePosX = beaconsActivePosX;
		output.beaconsActivePosY = beaconsActivePosY;
		output.beaconsActivePosZ = beaconsActivePosZ;
		
		//output.beaconsActiveRssi;
		setOutput(output);
	}
	void updateInput() {
		WifiSensorData input = getInput();
		//setPointCloud(input.pose, input.point_cloud, input.time_stamp);
	}
private:
    //vector<real_T> point_cloud_;

    FrequencyLimiter freq_limiter_;
    TTimePoint last_time_;
	bool last_tick_measurement_ = false;
protected:
	WifiSimpleParams params_;
	TArray<TArray<WifiHit>> beaconsActive_;
};

}} //namespace
#endif
