#include "URLEmitter.h"

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPCredentials.h>

#include <Poco/Exception.h>
#include <iostream>
#include <sstream>


using namespace std;
using namespace Poco;
using namespace Poco::Net;

URLEmitter::URLEmitter(const std::string url):
	theUrl(url),
	uri(url),
	log(Poco::Logger::get("URL"))
{

}

void URLEmitter::processDetection(std::vector<Detection>& detections)
{
	HTTPClientSession session(uri.getHost(), uri.getPort());
	HTTPRequest request(HTTPRequest::HTTP_GET, uri.getPathAndQuery(), HTTPMessage::HTTP_1_1);
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
}
