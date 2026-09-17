#pragma once
#include "ActorImprintInstanceRecord.h"
#include "nlohmann/json_fwd.hpp"

class ActorImprintInstanceRecordReader
{
public:
	// On failure, outRecord is unchanged.
	static bool Read(
		const nlohmann::json& source,
		ActorImprintSerializedInstanceRecord& outRecord);
};

class ActorImprintInstanceRecordWriter
{
public:
	// On failure, outJson is unchanged.
	static bool Write(
		const ActorImprintSerializedInstanceRecord& record,
		nlohmann::json& outJson);
};
