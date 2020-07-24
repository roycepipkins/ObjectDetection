#pragma once
#include <vector>
#include <queue>
#include <unordered_map>

#include <Poco/Logger.h>
#include <Poco/Mutex.h>
#include <Poco/Event.h>
#include <Poco/Runnable.h>
#include <Poco/Thread.h>

#include <MQTTClient.h>

#include "Detection.h"
#include "ThreadedDetectionProcessor.h"

class MqttEmitter : public ThreadedDetectionProcessor
{
public:
	MqttEmitter(
		const std::string& broker_address, 
		const std::string& username, 
		const std::string& password, 
		const bool use_ssl, 
		const std::string topic_prefix,
		const std::string client_id,
		const int qos);
	virtual ~MqttEmitter();

	void processDetection(std::vector<Detection>& detections);


private:
	const std::string broker_addr;
	const std::string user;
	const std::string pass;
	const bool ssl; 
	const std::string prefix;
	const std::string id;
	const int pub_qos;

	Poco::Logger& log;


	std::string getDetectionsAsJson(const std::vector<Detection>& detections);
	std::unordered_map<std::string, std::unordered_map<std::string, int>> last_detection_status;

	MQTTClient client;
	MQTTClient_connectOptions conn_opts;

	bool MqttConnect();
};

