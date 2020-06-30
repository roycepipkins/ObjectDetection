#pragma once
#include <string>

#include <Poco/RegularExpression.h>
#include <Poco/SharedPtr.h>


class StringFilter
{
public:
	StringFilter(const std::string& filter, const bool isRegex);
	StringFilter(const StringFilter& filter);

	bool match(const std::string& candidate) const;
	bool isNegatation() const;

private:
	bool negating;
	std::string detection_filter;
	bool is_regex;
	Poco::RegularExpression regex;
	std::string convertToPattern(const std::string& simplePattern);
	
	

};

