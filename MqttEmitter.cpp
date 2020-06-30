#include "MqttEmitter.h"
#include "json/json.h"

using namespace std;
using namespace Poco;

MqttEmitter::MqttEmitter(
	const std::string& broker_address, 
	const std::string& username, 
	const std::string& password, 
	const bool use_ssl, 
	const std::string topic_prefix,
    const std::string client_id,
    const int qos)
	:
	broker_addr(broker_address),
	user(username),
	pass(password),
	ssl(use_ssl),
	prefix(topic_prefix),
    id(client_id),
    pub_qos(qos),
	log(Poco::Logger::root().get("MQTT")),
    conn_opts(MQTTClient_connectOptions_initializer)
{
    int rc;

    MQTTClient_create(&client, broker_addr.c_str(), id.c_str(),
        MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        log.error("Failed to connect, return code %d\n", rc);
    }
}

MqttEmitter::~MqttEmitter()
{
    log.information("Disconnecting from MQTT broker");
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    
}

void MqttEmitter::processDetection(std::vector<Detection>& detections)
{
    if (!MQTTClient_isConnected(client))
    {
        int rc;
        log.warning("MQTT client found disconnected. Attempting to reconnect...");
        if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
        {
            log.error("Failed to connect to MQTT broker, return code %d\n", rc);
            return;
        }
        else
        {
            log.information("Reconnected to MQTT");
        }
    }

    //rich detection publication
    if (!detections.empty())
    {
        string full_detection_topic = prefix + "full_detection";
        string payload = getDetectionsAsJson(detections);

        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        MQTTClient_deliveryToken token;

        pubmsg.payload = (void*)payload.c_str();
        pubmsg.payloadlen = (int)payload.size();
        pubmsg.qos = pub_qos;
        pubmsg.retained = 0;
        MQTTClient_publishMessage(client, full_detection_topic.c_str(), &pubmsg, &token);
        if (MQTTCLIENT_SUCCESS != MQTTClient_waitForCompletion(client, token, 1000))
        {
            log.error("Failed to publish MQTT message. Please check server and configured credentials");
        }
    }


    //terse detection publication
    std::unordered_map<std::string, int> last_class_status;
    auto it = last_detection_status.find(detections.at(0).src_name);
    if (it != last_detection_status.end()) last_class_status = it->second;

    for (auto it = last_class_status.begin(); it != last_class_status.end(); ++it)
    {
        it->second = 0;
    }

    for (const auto& detection : detections)
    {
        last_class_status[detection.detection_class] = 1;
    }


    //loop over last_detection_status and publish each.
    for (auto& [classname, present] : last_class_status)

    {
        string topic = prefix + detections.at(0).src_name + "/" + classname;
        string payload = present > 0 ? "1" : "0";

        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        MQTTClient_deliveryToken token;

        pubmsg.payload = (void*)payload.c_str();
        pubmsg.payloadlen = (int)payload.size();
        pubmsg.qos = pub_qos;
        pubmsg.retained = 0;
        MQTTClient_publishMessage(client, topic.c_str(), &pubmsg, &token);
        if (MQTTCLIENT_SUCCESS != MQTTClient_waitForCompletion(client, token, 1000))
        {
            log.error("Failed to publish MQTT message. Please check server and configured credentials");
        }
    }



    //after you publish, delete all the entries with a zero value.
    set<string> non_detected_classes;
    for (auto it = last_class_status.begin(); it != last_class_status.end(); ++it)
    {
        if (it->second == 0) non_detected_classes.insert(it->first);
    }

    for (auto classname : non_detected_classes)
    {
        last_class_status.erase(classname);
    }

    last_detection_status[detections.at(0).src_name] = last_class_status;
    
}




std::string MqttEmitter::getDetectionsAsJson(const std::vector<Detection>& detections)
{
    Json::Value json_detections(Json::arrayValue);

    for (const auto& detection : detections)
    {
        Json::Value json_detection;
        json_detection["classname"] = detection.detection_class;
        json_detection["confidence"] = detection.confidence;
        json_detection["source_name"] = detection.src_name;

        Json::Value bounding_box;
        bounding_box["left"] = detection.bounding_box.x;
        bounding_box["top"] = detection.bounding_box.y;
        bounding_box["width"] = detection.bounding_box.width;
        bounding_box["height"] = detection.bounding_box.height;
        bounding_box["center_x"] = detection.centerX();
        bounding_box["center_y"] = detection.centerX();
        json_detection["bounding_box"] = bounding_box;
        
        json_detections.append(json_detection);
    }

    Json::FastWriter writer;
    return writer.write(json_detections);
}
