#include "Engine/ActorImprint/ActorImprintInstanceRecordCodec.h"
#include "Engine/ActorImprint/ActorImprintPropertyOverrides.h"
#include "Engine/ActorImprint/ActorImprintReferenceCodec.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Core/Reflection/AssetReferenceCodec.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Core/Reflection/ReflectionSerialization.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

enum class ActorImprintRecordTestMode { Idle, Active };

template<>
struct EnumReflection<ActorImprintRecordTestMode>
{
	static std::optional<EnumMetadata> Get()
	{
		return EnumMetadataBuilder<ActorImprintRecordTestMode>()
			.Add("Idle", ActorImprintRecordTestMode::Idle)
			.Add("Active", ActorImprintRecordTestMode::Active)
			.Build();
	}
};

namespace
{
	using json = nlohmann::json;
	int g_failures = 0;

	void Check(bool condition, const std::string& name)
	{
		if (condition) std::cout << "[PASS] " << name << '\n';
		else { std::cerr << "[FAIL] " << name << '\n'; ++g_failures; }
	}

	struct OverrideProbe
	{
		bool enabled = true;
		float weight = 1.0f;
		Vector3 position{};
		std::string label = "base";
		std::string escaped = "same";
		ActorReference target;
		AssetReference<TextureAsset> texture;
		ActorImprintRecordTestMode mode = ActorImprintRecordTestMode::Idle;
	};

	const TypeMetadata& ProbeMetadata()
	{
		static const TypeMetadata metadata = []
		{
			TypeMetadataBuilder<OverrideProbe> builder("ActorImprintRecordOverrideProbe");
			builder.Property("enabled", &OverrideProbe::enabled);
			builder.Property("weight", &OverrideProbe::weight).Validate([](float value) { return value >= 0.0f; });
			builder.Property("position", &OverrideProbe::position);
			builder.Object("nested", [](auto& object) { object.Property("label", &OverrideProbe::label); });
			builder.Property("a/b~c", &OverrideProbe::escaped);
			builder.Property("target", &OverrideProbe::target);
			builder.Property("texture", &OverrideProbe::texture);
			builder.Property("mode", &OverrideProbe::mode);
			return *builder.Build();
		}();
		return metadata;
	}

	json DefaultProperties()
	{
		return {
			{ "enabled", true },
			{ "weight", 1.0 },
			{ "position", json::array({ 0.0, 0.0, 0.0 }) },
			{ "nested", { { "label", "base" } } },
			{ "a/b~c", "same" },
			{ "target", nullptr },
			{ "texture", nullptr },
			{ "mode", "Idle" },
		};
	}

	PropertyPath Path(const std::string& text)
	{
		const auto path = PropertyPath::FromString(text);
		if (!path)
		{
			Check(false, "Test fixture PropertyPath is valid: " + text);
			return *PropertyPath::FromString("/invalid-test-fixture");
		}
		return *path;
	}

	ActorImprintPropertyOverrideTarget Target(LocalObjectId id,
		std::initializer_list<std::pair<std::string, json>> properties)
	{
		ActorImprintPropertyOverrideTarget target;
		target.targetLocalObjectId = id;
		for (const auto& [path, value] : properties) target.properties.emplace_back(Path(path), value);
		return target;
	}

	json RecordJson()
	{
		const Guid asset = GuidGenerator::Generate();
		const Guid root = GuidGenerator::Generate();
		const Guid child = GuidGenerator::Generate();
		const DefinitionRevision revision = DefinitionRevision::Generate();
		return {
			{ "assetGuid", asset.ToString() },
			{ "sourceDefinitionRevision", revision.ToString() },
			{ "rootActorGuid", root.ToString() },
			{ "externalParentActorGuid", nullptr },
			{ "actorGuids", json::array({
				{ { "localObjectId", 20 }, { "actorGuid", child.ToString() } },
				{ { "localObjectId", 10 }, { "actorGuid", root.ToString() } },
			}) },
			{ "propertyOverrides", json::array({
				{ { "targetLocalObjectId", 21 }, { "properties", {
					{ "/weight", 2.0 }, { "/a~1b~0c", "changed" }
				} } },
				{ { "targetLocalObjectId", 10 }, { "properties", { { "/target", nullptr } } } },
			}) },
		};
	}

	bool RejectRecord(const json& source)
	{
		ActorImprintSerializedInstanceRecord sentinel;
		sentinel.assetGuid = GuidGenerator::Generate();
		const Guid before = sentinel.assetGuid;

		return !ActorImprintInstanceRecordReader::Read(source, sentinel) && sentinel.assetGuid == before;
	}

	void TestRecordCodec()
	{
		const json source = RecordJson();
		ActorImprintSerializedInstanceRecord record;

		Check(ActorImprintInstanceRecordReader::Read(source, record), "Instance record reader accepts the complete schema");
		Check(record.actorGuids.size() == 2 &&
			record.actorGuids[0].localObjectId == 10 &&
			record.propertyOverrides.size() == 2 &&
			record.propertyOverrides[0].targetLocalObjectId == 10,
			"Reader normalizes GUID and target arrays by LocalObjectID");
		Check(record.propertyOverrides[1].properties[0].path.ToString() == "/a~1b~0c" &&
			record.propertyOverrides[1].properties[0].path.GetMembers()[0] == "a/b~c",
			"JSON Pointer escaping round-trips through decoded members");

		json encoded = { { "sentinel", true } };
		Check(ActorImprintInstanceRecordWriter::Write(record, encoded), "Validated Instance record writes successfully");
		Check(encoded["actorGuids"][0]["localObjectId"] == 10 &&
			encoded["propertyOverrides"][0]["targetLocalObjectId"] == 10 &&
			encoded["propertyOverrides"][1]["properties"].begin().key() == "/a~1b~0c",
			"Writer output order is deterministic");
		ActorImprintSerializedInstanceRecord repeated;
		json repeatedJson;
		Check(ActorImprintInstanceRecordReader::Read(encoded, repeated) &&
			ActorImprintInstanceRecordWriter::Write(repeated, repeatedJson) &&
			repeatedJson.dump() == encoded.dump(),
			"Instance record read-write-read-write is byte-stable");
		Check(encoded["propertyOverrides"][0]["properties"]["/target"].is_null(), "Explicit null remains an Override value");

		json noOverrides = source;
		noOverrides.erase("propertyOverrides");
		ActorImprintSerializedInstanceRecord empty;
		json normalized;
		Check(ActorImprintInstanceRecordReader::Read(noOverrides, empty) &&
			empty.propertyOverrides.empty() &&
			ActorImprintInstanceRecordWriter::Write(empty, normalized) &&
			!normalized.contains("propertyOverrides"),
			"Omitted propertyOverrides means empty and stays omitted");
		noOverrides["propertyOverrides"] = json::array({ {
			{ "targetLocalObjectId", 10 }, { "properties", json::object() }
		} });
		Check(ActorImprintInstanceRecordReader::Read(noOverrides, empty) &&
			empty.propertyOverrides.empty() &&
			ActorImprintInstanceRecordWriter::Write(empty, normalized) &&
			!normalized.contains("propertyOverrides"),
			"Empty target records normalize away");

		for (const std::string field : { "assetGuid", "sourceDefinitionRevision", "rootActorGuid",
			"externalParentActorGuid", "actorGuids" })
		{
			json bad = source;
			bad.erase(field);
			Check(RejectRecord(bad), "Required Instance field is rejected: " + field);
		}
		json bad = source;
		bad["unknown"] = true;
		Check(RejectRecord(bad), "Unknown Instance field is rejected");
		bad = source; bad["assetGuid"] = nullptr;
		Check(RejectRecord(bad), "Null Asset GUID is rejected");
		bad = source; bad["assetGuid"] = source["assetGuid"].get<std::string>() + std::string("\0suffix", 7);
		Check(RejectRecord(bad), "Asset GUID with embedded NUL is rejected");
		bad = source; bad["sourceDefinitionRevision"] = "bad";
		Check(RejectRecord(bad), "Malformed revision is rejected");
		bad = source; bad["rootActorGuid"] = source["rootActorGuid"].get<std::string>() + std::string("\0suffix", 7);
		Check(RejectRecord(bad), "Root GUID with embedded NUL is rejected");
		bad = source; bad["externalParentActorGuid"] = 10;
		Check(RejectRecord(bad), "External parent accepts only null or GUID");
		bad = source; bad["externalParentActorGuid"] = source["rootActorGuid"].get<std::string>() + std::string("\0suffix", 7);
		Check(RejectRecord(bad), "External parent GUID with embedded NUL is rejected");
		bad = source; bad["actorGuids"] = json::object();
		Check(RejectRecord(bad), "Actor mapping must be an array");
		bad = source; bad["actorGuids"][0]["localObjectId"] = 0;
		Check(RejectRecord(bad), "Zero Actor LocalObjectID is rejected");
		bad = source; bad["actorGuids"][0]["localObjectId"] = -1;
		Check(RejectRecord(bad), "Negative Actor LocalObjectID is rejected");
		bad = source; bad["actorGuids"][0]["localObjectId"] = 10;
		Check(RejectRecord(bad), "Duplicate Actor LocalObjectID is rejected");
		bad = source; bad["actorGuids"][0]["actorGuid"] = bad["actorGuids"][1]["actorGuid"];
		Check(RejectRecord(bad), "Duplicate Actor GUID is rejected");
		bad = source; bad["actorGuids"][0]["actorGuid"] =
			source["actorGuids"][0]["actorGuid"].get<std::string>() + std::string("\0suffix", 7);
		Check(RejectRecord(bad), "Actor GUID with embedded NUL is rejected");
		bad = source; bad["rootActorGuid"] = GuidGenerator::Generate().ToString();
		Check(RejectRecord(bad), "Root GUID must occur in the mapping");

		bad = source; bad["propertyOverrides"] = json::object();
		Check(RejectRecord(bad), "Property Overrides must be an array");
		bad = source; bad["propertyOverrides"][0]["targetLocalObjectId"] = 0;
		Check(RejectRecord(bad), "Zero Override target is rejected");
		bad = source; bad["propertyOverrides"].push_back(bad["propertyOverrides"][0]);
		Check(RejectRecord(bad), "Duplicate Override target is rejected");
		bad = source; bad["propertyOverrides"][0]["properties"] = json::array();
		Check(RejectRecord(bad), "Override properties must be an object");
		for (const std::string invalid : { "", "name", "/~2", "/a//b" })
		{
			bad = source;
			bad["propertyOverrides"][0]["properties"] = json::object({ { invalid, true } });
			Check(RejectRecord(bad), "Malformed JSON Pointer is rejected: " + invalid);
		}
		bad = source;
		bad["propertyOverrides"][0]["properties"] = { { "/nested", json::object() }, { "/nested/label", "x" } };
		Check(RejectRecord(bad), "Ancestor and descendant paths are rejected independently of order");
		bad = source;
		bad["propertyOverrides"][0]["properties"]["/weight"] = (std::numeric_limits<double>::quiet_NaN)();
		Check(RejectRecord(bad), "Non-finite Override values are rejected at the persistence boundary");
		bad = source;
		bad["propertyOverrides"][0]["properties"]["/payload"] = json::binary({ 1, 2, 3 });
		Check(RejectRecord(bad), "Binary Override values are rejected at the JSON persistence boundary");
		bad = source;
		bad["propertyOverrides"][0]["properties"]["/label"] = std::string("\xff", 1);
		Check(RejectRecord(bad), "Invalid UTF-8 Override strings are rejected at the JSON persistence boundary");

		ActorImprintSerializedInstanceRecord invalidWriter = record;
		invalidWriter.actorGuids[0].localObjectId = InvalidLocalObjectId;
		json writerSentinel = { { "unchanged", 1 } };
		const json beforeWriter = writerSentinel;
		Check(!ActorImprintInstanceRecordWriter::Write(invalidWriter, writerSentinel) &&
			writerSentinel == beforeWriter,
			"Writer failure leaves its output JSON unchanged");
		invalidWriter = record;
		invalidWriter.propertyOverrides[0].properties[0].value = (std::numeric_limits<double>::infinity)();
		Check(!ActorImprintInstanceRecordWriter::Write(invalidWriter, writerSentinel) &&
			writerSentinel == beforeWriter,
			"Writer rejects non-finite JSON without changing its output");
	}

	void TestDiff()
	{
		const json defaults = DefaultProperties();
		ActorImprintPropertyOverrideTarget target;

		Check(ActorImprintPropertyOverrides::Diff(ProbeMetadata(), 10, defaults, defaults, target) &&
			target.targetLocalObjectId == 10 &&
			target.properties.empty(),
			"Unedited normalized values generate no Override");

		json current = defaults;
		current["position"] = json::array({ 4.0, 0.0, 0.0 });
		current["a/b~c"] = "changed";
		current["weight"] = static_cast<double>(std::nextafter(1.0f, 2.0f));
		Check(ActorImprintPropertyOverrides::Diff(ProbeMetadata(), 10, defaults, current, target) &&
			target.properties.size() == 3 &&
			target.properties[0].path.ToString() == "/a~1b~0c" &&
			target.properties[1].path.ToString() == "/position" &&
			target.properties[1].value.is_array() &&
			target.properties[2].path.ToString() == "/weight",
			"Diff uses exact float equality, escaped pointers and whole-array leaves");

		ActorImprintPropertyOverrideTarget sentinel = Target(99, { { "/enabled", false } });
		current = defaults;
		current["nested"].erase("label");
		Check(!ActorImprintPropertyOverrides::Diff(ProbeMetadata(), 10, defaults, current, sentinel) &&
			sentinel.targetLocalObjectId == 99 &&
			sentinel.properties.size() == 1,
			"Current schema mismatch fails without changing the output target");
		current = defaults;
		current["unknown"] = true;
		Check(!ActorImprintPropertyOverrides::Diff(ProbeMetadata(), 10, defaults, current, sentinel),
			"Unknown current property is a schema mismatch");
		json invalidDefault = defaults;
		invalidDefault.erase("weight");
		Check(!ActorImprintPropertyOverrides::Diff(ProbeMetadata(), 10, invalidDefault, defaults, sentinel),
			"Invalid default schema is distinguished from current mismatch");
	}

	bool FullyDeserialize(const json& properties)
	{
		OverrideProbe probe;
		ActorImprintReferenceCodec actorCodec({ { 10, GuidGenerator::Generate() } });
		AssetReferenceCodec assetCodec;
		ReflectionRestoreContext context{ .actorReferenceCodec = &actorCodec, .assetReferenceCodec = &assetCodec };
		return ReflectionDeserializer::Deserialize(ProbeMetadata(), typeid(OverrideProbe), properties, &probe, context);
	}

	void TestMergeAndFullValidation()
	{
		const json defaults = DefaultProperties();

		json completed = { { "sentinel", true } };
		std::vector<ActorImprintPropertyOverrideEntry> applied{ { Path("/sentinel"), true } };
		ActorImprintPropertyOverrideMigration migration{ 7, 8, 9 };
		const json beforeCompleted = completed;

		auto missing = Target(10, { { "/removed", 1.0 } });
		Check(!ActorImprintPropertyOverrides::Merge(ProbeMetadata(), defaults, missing,
			ActorImprintOverrideRevisionRelation::Same, completed, &applied, &migration) &&
			completed == beforeCompleted &&
			applied.size() == 1 &&
			migration.applied == 7,
			"Same-revision missing path is fatal and all outputs remain unchanged");

		auto migrated = Target(10, { { "/weight", 2.0 }, { "/removed", true }, { "/enabled", "wrong" } });
		Check(ActorImprintPropertyOverrides::Merge(ProbeMetadata(), defaults, migrated,
			ActorImprintOverrideRevisionRelation::Different, completed, &applied, &migration) &&
			completed["weight"] == 2.0 &&
			completed["enabled"] == true &&
			!completed.contains("removed") &&
			applied.size() == 1 &&
			applied[0].path.ToString() == "/weight" &&
			migration.applied == 1 &&
			migration.staleMissingPath == 1 &&
			migration.staleIncompatible == 1,
			"Different revision drops only missing and representation-incompatible entries");

		auto arrayElement = Target(10, { { "/position/0", 4.0 } });
		Check(!ActorImprintPropertyOverrides::Merge(ProbeMetadata(), defaults, arrayElement,
			ActorImprintOverrideRevisionRelation::Different, completed, nullptr, nullptr),
			"Array element paths remain fatal across revisions");
		auto wrongFloat = Target(10, { { "/weight", 2 } });
		Check(!ActorImprintPropertyOverrides::Merge(ProbeMetadata(), defaults, wrongFloat,
			ActorImprintOverrideRevisionRelation::Same, completed, nullptr, nullptr),
			"Same-revision JSON representation mismatch is fatal");
		auto nullPrimitive = Target(10, { { "/weight", nullptr } });
		Check(ActorImprintPropertyOverrides::Merge(ProbeMetadata(), defaults, nullPrimitive,
			ActorImprintOverrideRevisionRelation::Different, completed, nullptr, &migration) &&
			migration.staleIncompatible == 1 &&
			completed["weight"] == defaults["weight"],
			"Primitive null is stale only when revisions differ");

		auto nullReferences = Target(10, { { "/target", nullptr }, { "/texture", nullptr } });
		Check(ActorImprintPropertyOverrides::Merge(ProbeMetadata(), defaults, nullReferences,
			ActorImprintOverrideRevisionRelation::Same, completed, &applied, &migration) &&
			applied.size() == 2 &&
			completed["target"].is_null() &&
			completed["texture"].is_null(),
			"Null remains a compatible explicit Actor/Asset reference value");

		auto malformedReference = Target(10, { { "/target", { { "type", "ActorReference" }, { "scope", "local" } } } });
		Check(ActorImprintPropertyOverrides::Merge(ProbeMetadata(), defaults, malformedReference,
			ActorImprintOverrideRevisionRelation::Different, completed, nullptr, nullptr) &&
			!FullyDeserialize(completed),
			"Malformed typed reference is not misclassified as a stale JSON type");
		auto invalidAssetGuid = Target(10, { { "/texture", "not-a-guid" } });
		Check(ActorImprintPropertyOverrides::Merge(ProbeMetadata(), defaults, invalidAssetGuid,
			ActorImprintOverrideRevisionRelation::Different, completed, nullptr, nullptr) &&
			!FullyDeserialize(completed),
			"Invalid Asset GUID remains a fatal full-deserialization failure");
		auto invalidArray = Target(10, { { "/position", json::array({ 1.0, 2.0 }) } });
		Check(ActorImprintPropertyOverrides::Merge(ProbeMetadata(), defaults, invalidArray,
			ActorImprintOverrideRevisionRelation::Different, completed, nullptr, nullptr) &&
			!FullyDeserialize(completed),
			"Invalid whole-array shape remains a fatal full-deserialization failure");
		auto invalidDomain = Target(10, { { "/weight", -1.0 } });
		Check(ActorImprintPropertyOverrides::Merge(ProbeMetadata(), defaults, invalidDomain,
			ActorImprintOverrideRevisionRelation::Different, completed, nullptr, nullptr) &&
			!FullyDeserialize(completed),
			"Property domain failure remains fatal after representation preflight");
		auto invalidEnum = Target(10, { { "/mode", "RemovedEnumerator" } });
		Check(ActorImprintPropertyOverrides::Merge(ProbeMetadata(), defaults, invalidEnum,
			ActorImprintOverrideRevisionRelation::Different, completed, nullptr, nullptr) &&
			!FullyDeserialize(completed),
			"Unknown enum value remains a fatal full-deserialization failure");

		auto conflicting = Target(10, { { "/nested", json::object() }, { "/nested/label", "x" } });
		Check(!ActorImprintPropertyOverrides::Merge(ProbeMetadata(), defaults, conflicting,
			ActorImprintOverrideRevisionRelation::Different, completed, nullptr, nullptr),
			"Ancestor conflict remains fatal across revisions");
	}
}

int main()
{
	TestRecordCodec();
	TestDiff();
	TestMergeAndFullValidation();
	return g_failures == 0 ? 0 : 1;
}
