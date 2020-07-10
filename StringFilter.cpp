#include "StringFilter.h"

#include <Poco/Exception.h>
#include <Poco/String.h>

StringFilter::StringFilter(const std::string& filter, const bool isRegex):
	negating(false),
	detection_filter(isRegex ? filter : convertToPattern(filter)),
	is_regex(isRegex),
	regex(isRegex ? filter : convertToPattern(filter))
{
	
}

StringFilter::StringFilter(const StringFilter& filter):
negating(filter.negating),
detection_filter(filter.detection_filter),
is_regex(filter.is_regex),
regex(filter.detection_filter)
{

}

bool StringFilter::match(const std::string& candidate) const
{
	Poco::RegularExpression::Match match;
	return regex.match(candidate, 0, match, Poco::RegularExpression::RE_NOTEMPTY);
}

bool StringFilter::isNegatation() const
{
	return negating;
}

std::string StringFilter::convertToPattern(const std::string& simplePattern)
{
	if (simplePattern.empty())
	{
		throw Poco::RuntimeException("Simple Patterns must not be empty.");
	}



	if (simplePattern[0] == '!') negating = true;

	for (char c : simplePattern.substr(1))
	{
		if (!isalnum(c))
		{
			if (c != '*' && c != '_')
			{
				throw Poco::RuntimeException("Simple Filter Patterns must be alphanumeric or underscore with * wildcards or leading ! only.");
			}
		}
	}

	return Poco::replace(negating ? simplePattern.substr(1) : simplePattern, "*", ".*");
}
