#include "EventFilter.h"

EventFilter::EventFilter(const std::vector<StringFilter> classFilterValues, const std::vector<StringFilter> sourceFilterValues) :
	classFilters(classFilterValues),
	sourceFilters(sourceFilterValues)
{
}

void EventFilter::onDetectionEvent(const void* sender, std::vector<Detection>& detections)
{
	std::vector<Detection> filteredDetections;

	for (auto detection : detections)
	{
		if (detection.is_null)
		{
			filteredDetections.push_back(detection);
		}
		else
		{
			bool class_passed = false;
			bool negated = false;
			for (const auto& filter : classFilters)
			{
				if (filter.isNegatation()) negated |= filter.match(detection.Name());
				else class_passed |= filter.match(detection.Name());
			}

			bool source_passed = false;
			for (const auto& filter : sourceFilters)
			{
				if (filter.isNegatation()) negated |= filter.match(detection.src_name);
				else source_passed |= filter.match(detection.src_name);
			}

			if (class_passed && source_passed && !negated) filteredDetections.push_back(detection);
		}
	}

	if (filteredDetections.empty())
	{
		Detection null_detection;
		null_detection.src_name = detections.at(0).src_name;
		filteredDetections.push_back(null_detection);
	}

	filteredDetectionEvent.notify(this, filteredDetections);
}
