#include "URLEmitter.h"

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPCredentials.h>

#include <Poco/Util/Application.h>

#include <Poco/Exception.h>
#include <Poco/StreamCopier.h>
#include <Poco/Base64Encoder.h>
#include <Poco/Timestamp.h>
#include <Poco/DateTime.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/Path.h>
#include <Poco/File.h>

#include <iostream>
#include <sstream>

#include <opencv2/opencv.hpp>


using namespace std;
using namespace Poco;
using namespace Poco::Net;

URLEmitter::URLEmitter(const std::string& emitter_name, const std::string& url, const std::string& username, const std::string& password, const bool log_detections):
	name(emitter_name),
	theUrl(url),
	user(username),
	pw(password),
	uri(url),
	log_detects(log_detections),
	log(Poco::Logger::get("URL"))
{

}

void URLEmitter::processDetection(std::vector<Detection>& detections)
{
	assert(!detections.empty());
	
	try
	{
		if (detections.size() == 1 && detections.at(0).is_null) return;

		HTTPClientSession session(uri.getHost(), uri.getPort());

		HTTPRequest request(HTTPRequest::HTTP_GET, uri.getPathAndQuery(), HTTPMessage::HTTP_1_1);

		if (!user.empty())
		{
			stringstream b64_creds;
			Base64Encoder b64(b64_creds);
			stringstream header_value;
			b64 << user << ":" << pw;
			b64.flush();
			b64.close();
			header_value << "Basic " << b64_creds.str();
			request.set("Authorization", header_value.str());
		}


		HTTPResponse response;
		session.sendRequest(request);
		session.receiveResponse(response);

		stringstream msg;
		msg << uri.getPathAndQuery() << " " << response.getStatus() << " " << response.getReason();


		if (response.getStatus() != HTTPResponse::HTTP_OK)
		{
			log.warning(msg.str());
		}
		else
		{
			log.information(msg.str());
		}

		if (log_detects)
		{
			LogDetection(detections);
		}

	}
	catch (Poco::Exception& e)
	{
		log.error(theUrl + " -> " + e.displayText());
	}
	catch (std::exception& e)
	{
		log.error(theUrl + " -> " + e.what());
	}
	
}

//TODO Move this to its own threaded detector. It really should not be part of URLEmitter.
void URLEmitter::LogDetection(std::vector<Detection>& detections)
{
	Poco::LocalDateTime now;
	Path log_path(Poco::Util::Application::instance().config().getString("application.dir"));
	log_path.append("logs");
	log_path.append(name);

	File(log_path).createDirectories();

	std::stringstream fname;
	fname << name << "_" << Poco::DateTimeFormatter::format(now, "%Y%m%d-%H%M%S.%i") << ".jpg";
	log_path.append(fname.str());

	cv::imwrite(log_path.toString(), detections.at(0).frame);

}
