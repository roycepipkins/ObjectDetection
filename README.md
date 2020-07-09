# ObjectDetection
## Summary
ObjectDetection is an [OpenCV](https://opencv.org/) C++ implementation of the [YOLO v4 object detection neural network](https://medium.com/@alexeyab84/yolov4-the-most-accurate-real-time-neural-network-on-ms-coco-dataset-73adfd3602fe) targeting security and automation applications. One or more camera RTSP inputs can be configured. In response to detection events ObjectDetection can be configured to publish MQTT messages and/or fetch a configured URL with an HTTP GET request. The immediate intent is to feed a home automation system as well as replace the built-in motion detection in my [Blue Iris NVR software](https://blueirissoftware.com/).
## Notes
- Currently ObjectDetection is a Windows executable that may be run either as a regular user application or as a Windows Service. 
- Only when run as a user application will ObjectDetection open a display window for each configured camera and visually display object detections. 
- ObjectDetection can be registered as a windows service by running the application with the /registerService command line argument from an adminstrative command prompt. The program will exit immediately but will now show up in the list of Windows services and can now be started and stopped there. Be sure to leave the executable in the location where it was registered. (Use /unregisterService to remove the registration) 
- ObjectDetection will register its executable name as the name of the service. This means can make copies of the executable, give each copy a unique name, and then register each copy as a service seperately from the other copies.
- ObjectDetection will look for a .properties file with a name that matches the name of the executable in order to configure itself. So if you create a copy of ObjectDetection.exe named RearParking.exe be sure to create a RearParking.properties with the configuration for that instance of the exe.
- You do not need to create multiple copies of the executable to handle multiple cameras. A single instance can do it all but the service registration behavior gives you the ability to split it up if you so desire.
- There is no Windows specific code in ObjectDetection. However there is currently no build system apart from the MSVS solution. With more effort, this code could be compiled for Linux or Mac. (Even the service stuff would recompile as daemon stuff)
- Currently neither SSL or CUDA is supported. I just don't need them in my context. My MQTT broker is on my local network and my Blue Iris security computer does not have a graphics card.
- The YOLO weights, config, and COCO classname list files are all expected to be in a sub-directory named yolo-coco beneath the executable.
- *camera_name*, as it appears in the configuration documentation, is meant to represent a user assigned name for a specific camera. No spaces, use alphanumeric or underscore only. The name also serves to organize the various settings that apply to that camera. There is no specific limit in code on the number of cameras but at some point you will encounter a limit on computer resources. 
- *url_name*, as it appears in the configuration documentation, is meant to represent a user assigned name for a specific URL. No spaces, use alphanumeric or underscore only. The name also serves to organize the various settings that apply to that URL. There is no specific limit in code on the number of URLs but at some point you will encounter a limit on computer resources. 
## Configuration
ObjectDetection is configured by a Java properties style file named <executable_name>.properties (Ex. ObjectDetection.properties) located in the same directory as the executable. The basic format of a single configuration entry is *key_name.sub_key_name.sub_sub_key_name = value*. (The number of sub key names can vary.)

|Key Name|Required|Default Value|Description|
|-------------------------|--------|-------------|-----------|
|**Logs**||||
|log.level|N|information| One of: none, fatal, critical, error, warning, notice, information, debug, trace|
|logs.full_path_override  |N| |A fully qualifed path and filename where the log should be written|
|logs.use_utc|N|false|The time stamps in the log can optionally appear in UTC time|
|logs.rotation|N|00:00|The time the log file should be rotated out for a new file. [See here.](https://pocoproject.org/docs/Poco.FileChannel.html)|
|logs.purge_age|N|12 months|Past this age the log files are deleted. [See here.](https://pocoproject.org/docs/Poco.FileChannel.html)|
|**~For Each Camera**||||
|camera.*camera_name*.location|N| |The URL of the camera feed. Used in prefrence to index if specified.|
|camera.*camera_name*.index|N|0|The numeric index of the web camera on the executing machine.|
|camera.*camera_name*.fps|N|0.25|Max FPS pulled and scanned from a feed. Use this to limit CPU usage.|
|camera.*camera_name*.yolo.config|N|yolov4-leaky-416.cfg|Name of the YOLO configuration file.|
|camera.*camera_name*.yolo.weights|N|yolov4-leaky-416.weights|Name of the YOLO weights file.|
|camera.*camera_name*.yolo.coco_names|N|coco.names|Name of the file with the COCO classname list.|
|camera.*camera_name*.yolo.confidence_threshold|N|0.35|(0.00 - 1.00) Minimum confidence required for detection report|
|camera.*camera_name*.yolo.nms_threshold|N|0.48|Used to merge overlapping detections.|
|camera.*camera_name*.yolo.analysis_size|N|416|The square image size previously used to train. Should match configured network.|
|**MQTT**||||
|mqtt.broker_address|N| |The address of the MQTT Broker|
|mqtt.username|N| |The username to be submitted to the broker|
|mqtt.password|N| |The password to be submitted to the broker|
|mqtt.topic_prefix|N| |Prepended to the published topic names|
|mqtt.clientid|N|*executable_name*|The name given to the broker|
|mqtt.qos|N|1|The Quality of Service of the publications|
|mqtt.class_filter|N|\*|Comma seperated list of COCO classnames. \* is a wildcard. ! may be prepended to a specific classname to exclude it.|
|mqtt.source_filter|N|\*|Comma seperated list of camera_name filters. \* is a wildcard. ! may be prepended to a specific camera_name to exclude it.|
|**~For Each URL**||||
|url_fetch.*url_name*.url|N| |The URL to send the HTTP GET request|
|url_fetch.*url_name*.username|N| |The HTTP Basic Authorization user name. (Note: Doen't seem to work for Blue Iris. Embed in URL instead)|
|url_fetch.*url_name*.password|N| |The HTTP Basic Authorization password. (Note: Doen't seem to work for Blue Iris. Embed in URL instead)|
|url_fetch.*url_name*.class_filter|N|\*|Comma seperated list of COCO classnames. \* is a wildcard. ! may be prepended to a specific classname to exclude it.|
|url_fetch.*url_name*.source_filter|N|\*|Comma seperated list of camera_name filters. \* is a wildcard. ! may be prepended to a specific camera_name to exclude it.|




