#include "AnimationClip.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace
{

using json = nlohmann::json;
constexpr int kFormatVersion = 1;

const json& ReadArray(const json& object, const char* name)
{
	const auto& value = object.at(name);

	if (!value.is_array())
	{
		throw std::runtime_error(std::string(name) + " must be an array.");
	}

	return value;
}

double ReadNumber(const json& value)
{
	if (!value.is_number())
	{
		throw std::runtime_error("Expected a number.");
	}

	const double number = value.get<double>();

	if (!std::isfinite(number))
	{
		throw std::runtime_error("Expected a finite number.");
	}

	return number;
}

template<class T>
json WriteKeys(const std::vector<AnimationKey<T>>& keys)
{
	auto result = json::array();

	for (const auto& key : keys)
	{
		const auto& v = key.value;
		auto components = json::array({ v.x, v.y, v.z });

		if constexpr (std::is_same_v<T, Quaternion>)
		{
			components.push_back(v.w);
		}

		result.push_back({
			{ "timeSeconds", key.timeSeconds },
			{ "value", std::move(components) }
			});
	}

	return result;
}

float ReadComponent(const json& value)
{
	const double number = ReadNumber(value);

	if (std::abs(number) > std::numeric_limits<float>::max())
	{
		throw std::runtime_error("Key component is outside the float range.");
	}

	return static_cast<float>(number);
}

template<class T>
std::vector<AnimationKey<T>> ReadKeys(const json& input)
{
	constexpr bool isRotation = std::is_same_v<T, Quaternion>;

	std::vector<AnimationKey<T>> keys;
	keys.reserve(input.size());

	for (const auto& entry : input)
	{
		const double time = ReadNumber(entry.at("timeSeconds"));

		if (!keys.empty() && time <= keys.back().timeSeconds)
		{
			throw std::runtime_error("Key times must be strictly increasing.");
		}

		const auto& v = ReadArray(entry, "value");

		if (v.size() != (isRotation ? 4 : 3))
		{
			throw std::runtime_error("Unexpected key component count.");
		}

		T value{};
		value.x = ReadComponent(v[0]);
		value.y = ReadComponent(v[1]);
		value.z = ReadComponent(v[2]);

		if constexpr (isRotation)
		{
			value.w = ReadComponent(v[3]);
		}

		keys.push_back({ time, value });
	}

	return keys;
}

}

json SerializeAnimationClip(const AnimationClip& clip)
{
	auto nodes = json::array();

	for (const auto& node : clip.nodes)
	{
		nodes.push_back({ { "name", node.name },{ "parentIndex", node.parentIndex } });

	}

	auto tracks = json::array();

	for (const auto& track : clip.tracks)
	{
		tracks.push_back({
			{ "nodeIndex", track.nodeIndex },
			{ "positions", WriteKeys(track.positions) },
			{ "rotations", WriteKeys(track.rotations) },
			{ "scales", WriteKeys(track.scales) }
			});
	}

	return {
		{ "formatVersion", kFormatVersion },
		{ "sourceModelGuid", clip.sourceModelGuid.ToString() },
		{ "durationSeconds", clip.durationSeconds },
		{ "nodes", std::move(nodes) },
		{ "tracks", std::move(tracks) }
	};
}

AnimationClip DeserializeAnimationClip(const json& json)
{
	const auto& version = json.at("formatVersion");

	if (!version.is_number_integer() || version != kFormatVersion)
	{
		throw std::runtime_error("Unsupported AnimationClip format version.");
	}

	AnimationClip clip;

	if (!Guid::TryParse(json.at("sourceModelGuid").get<std::string>(), clip.sourceModelGuid)
		|| !clip.sourceModelGuid.IsValid())
	{
		throw std::runtime_error("Invalid source model GUID.");
	}

	clip.durationSeconds = ReadNumber(json.at("durationSeconds"));

	if (clip.durationSeconds < 0.0)
	{
		throw std::runtime_error("Clip duration must be nonnegative.");
	}

	const auto& nodes = ReadArray(json, "nodes");
	clip.nodes.reserve(nodes.size());

	for (const auto& entry : nodes)
	{
		const auto& parent = entry.at("parentIndex");

		if (!parent.is_number_integer())
		{
			throw std::runtime_error("Parent index must be an integer.");
		}

		const double index = parent.get<double>();
		const bool validParent =
			clip.nodes.empty() ? index == -1
			: index >= 0 &&
			  index < static_cast<double>(clip.nodes.size()) && 
			  index <= std::numeric_limits<std::int32_t>::max();

		if (!validParent)
		{
			throw std::runtime_error("Invalid parent node index.");
		}

		clip.nodes.push_back({
			entry.at("name").get<std::string>(),
			static_cast<std::int32_t>(index)
			});
	}

	const auto& tracks = ReadArray(json, "tracks");
	clip.tracks.reserve(tracks.size());

	for (const auto& entry : tracks)
	{
		const auto& node = entry.at("nodeIndex");

		if (!node.is_number_integer())
		{
			throw std::runtime_error("Track node index must be an integer.");
		}

		const double index = node.get<double>();

		if (index < 0 ||
			index >= static_cast<double>(clip.nodes.size()) ||
			index > std::numeric_limits<std::uint32_t>::max())
		{
			throw std::runtime_error("Track node index is outside the node array.");
		}

		AnimationTrack track;
		track.nodeIndex = static_cast<std::uint32_t>(index);
		track.positions = ReadKeys<Vector3>(ReadArray(entry, "positions"));
		track.rotations = ReadKeys<Quaternion>(ReadArray(entry, "rotations"));
		track.scales = ReadKeys<Vector3>(ReadArray(entry, "scales"));

		clip.tracks.push_back(std::move(track));
	}

	return clip;
}