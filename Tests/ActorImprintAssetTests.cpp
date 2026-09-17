#include "Engine/ActorImprint/ActorImprintAssetDeserializer.h"
#include "Engine/ActorImprint/ActorImprintAssetSerializer.h"
#include "Engine/ActorImprint/ActorImprintReferenceCodec.h"
#include "Engine/Actor/ActorMetadata.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/Camera.h"
#include "Engine/Core/Reflection/AssetReferenceCodec.h"
#include "Engine/Core/Reflection/ReflectionSerialization.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/SceneActorReferenceContext.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <type_traits>

namespace
{
	using json = nlohmann::json;
	int g_failures = 0;

	void Check(bool condition, const std::string& name)
	{
		if (condition) std::cout << "[PASS] " << name << '\n';
		else { std::cerr << "[FAIL] " << name << '\n'; ++g_failures; }
	}

	json MakeAsset()
	{
		return {
			{ "version", 1 }, { "definitionRevision", "{10AFAAA1-ABCD-4321-ABCD-123456789ABC}" },
			{ "rootActorLocalObjectId", 10 }, { "nextLocalObjectId", 12 },
			{ "actors", json::array({ {
				{ "localObjectId", 10 }, { "parentLocalObjectId", nullptr },
				{ "properties", { { "name", "Root" }, { "tag", "None" }, { "is_active", true } } },
				{ "components", json::array({ {
					{ "localObjectId", 11 }, { "type", "Transform" },
					{ "properties", { { "name", "Transform" }, { "position", { 0.1, 0.0, 0.0 } },
						{ "rotation", { 0.0, 0.0, 0.0, 1.0 } }, { "scale", { 1.0, 1.0, 1.0 } } } }
				} }) }
			} }) }
		};
	}

	void TestPilot()
	{
		static_assert(!std::is_default_constructible_v<ActorImprint>);
		static_assert(std::is_const_v<std::remove_reference_t<decltype(std::declval<const ActorImprint&>().GetActors())>>);

		const json input = MakeAsset();
		auto imprint = ActorImprintAssetDeserializer::Deserialize(input, nullptr);
		Check(imprint != nullptr, "Pilot: real Actor and registered Transform form a validated immutable definition");
		if (!imprint) { std::cerr << "Operation failed\n"; return; }
		const json output = ActorImprintAssetSerializer::Serialize(*imprint);
		Check(output["actors"][0]["components"][0]["properties"]["position"][0].get<double>() ==
			static_cast<double>(0.1f),
			"Pilot: default values reflect C++ float precision");
		auto repeated = ActorImprintAssetDeserializer::Deserialize(output);
		Check(repeated &&
			ActorImprintAssetSerializer::Serialize(*repeated).dump() == output.dump(),
			"Pilot: Serialize-Deserialize-Serialize is byte-stable");
		json malformed = input;
		malformed["actors"][0]["components"][0]["properties"]["scale"] = { 1.0, 1.0 };
		Check(!ActorImprintAssetDeserializer::Deserialize(malformed, nullptr),
			"Pilot: invalid property returns a position and no partial definition");
		Check(ActorImprintAssetSerializer::Serialize(*imprint) == output, "Pilot: failed separate load leaves an existing definition unchanged");
	}

	class ImprintProbe : public Component
	{
	public:
		float weight = 1.0f;
		ActorReference target;
		static inline int attachments = 0;
	private:
		void OnAttachOverride() override { ++attachments; }
		void OnStartOverride() override {}
		void PreUpdateOverride(float) override {}
		void UpdateOverride(float) override {}
		void LateUpdateOverride(float) override {}
		void OnDestroyOverride() override {}
	};
	class NonCanonicalImprintProbe final : public ImprintProbe {};

	void RegisterProbes()
	{
		TypeMetadataBuilder<ImprintProbe> builder("ImprintProbe");
		builder.Property("weight", &ImprintProbe::weight).Validate([](float value) { return value >= 0.0f && value < 10.0f; });
		builder.Property("target", &ImprintProbe::target);
		ComponentRegistry::Get().Register("ImprintProbe", [] { return new ImprintProbe(); }, typeid(ImprintProbe),
			ComponentCardinality::Multiple, ComponentFamily::None, std::make_unique<TypeMetadata>(*builder.Build()));
		TypeMetadataBuilder<NonCanonicalImprintProbe> unstable("NonCanonicalImprintProbe");
		unstable.Property("weight", [](const NonCanonicalImprintProbe& c) { return c.weight; },
			[](NonCanonicalImprintProbe& c, float value) { c.weight = value + 1.0f; });
		ComponentRegistry::Get().RegisterReflected<NonCanonicalImprintProbe>("NonCanonicalImprintProbe",
			std::make_unique<TypeMetadata>(*unstable.Build()));
	}

	json MakeComponent(const std::string& type, LocalObjectId id)
	{
		std::unique_ptr<Component> component(ComponentRegistry::Get().Create(type));
		json properties;
		Check(component && component->Serialize(properties), "Construct fixture from real Reflection schema: " + type);
		return { { "localObjectId", id }, { "type", type }, { "properties", std::move(properties) } };
	}

	bool Rejects(const json& input)
	{
		const json before = input;

		return !ActorImprintAssetDeserializer::Deserialize(input, nullptr) && input == before;
	}

	void TestHierarchyComponentsAndReferences()
	{
		json input = MakeAsset();
		json child = input["actors"][0];
		child["localObjectId"] = 20;
		child["parentLocalObjectId"] = 10;
		child["components"][0]["localObjectId"] = 21;
		input["actors"].push_back(child);
		input["nextLocalObjectId"] = 50;
		json camera = MakeComponent("Camera", 12);
		camera["properties"]["targetActorId"] = { { "type", "ActorReference" }, { "scope", "local" }, { "localObjectId", 20 } };
		input["actors"][0]["components"].push_back(camera);
		input["actors"][0]["components"].push_back(MakeComponent("ImprintProbe", 14));
		input["actors"][0]["components"].push_back(MakeComponent("ImprintProbe", 13));
		input["actors"][0]["components"][2]["properties"]["weight"] = 3.25;

		auto imprint = ActorImprintAssetDeserializer::Deserialize(input, nullptr);
		Check(imprint != nullptr, "Multiple same-type components and internal forward ActorReference load together");
		if (!imprint) { std::cerr << "Operation failed\n"; return; }
		json output = ActorImprintAssetSerializer::Serialize(*imprint);
		Check(output["actors"][0]["components"][1]["properties"]["targetActorId"] == camera["properties"]["targetActorId"],
			"Internal reference remains a typed LocalObjectID after normalization");
		Check(output["actors"][0]["components"][2]["localObjectId"] == 13 &&
			output["actors"][0]["components"][3]["properties"]["weight"] == 3.25,
			"Component output order follows identity without mixing same-type values");
		std::reverse(input["actors"].begin(), input["actors"].end());
		std::reverse(input["actors"][1]["components"].begin(), input["actors"][1]["components"].end());
		auto reordered = ActorImprintAssetDeserializer::Deserialize(input);
		Check(reordered &&
			ActorImprintAssetSerializer::Serialize(*reordered).dump() == output.dump(),
			"Full asset read is independent of Actor/Component input order and temporary GUIDs");
		auto roundTrip = ActorImprintAssetDeserializer::Deserialize(output);
		Check(roundTrip &&
			ActorImprintAssetSerializer::Serialize(*roundTrip).dump() == output.dump(),
			"Referenced hierarchy round-trip is deterministic");

		const std::string referencePath = "/actors/0/components/1/properties/targetActorId";
		for (const json& invalid : std::vector<json>{
			{ { "type", "ActorReference" }, { "scope", "local" }, { "localObjectId", 11 } },
			{ { "type", "ActorReference" }, { "scope", "local" }, { "localObjectId", 99 } },
			{ { "type", "ActorReference" }, { "scope", "local" }, { "localObjectId", 20.0 } },
			{ { "type", "ActorReference" }, { "scope", "scene" }, { "actorGuid", GuidGenerator::Generate().ToString() } },
			{ { "type", "ComponentReference" }, { "scope", "local" }, { "localObjectId", 11 } },
			{ { "type", "ActorReference" }, { "scope", "local" }, { "localObjectId", 20 }, { "extra", true } },
			GuidGenerator::Generate().ToString() })
		{
			json bad = output;
			bad[json::json_pointer(referencePath)] = invalid;
			Check(Rejects(bad), "Invalid/external reference is rejected: " + invalid.dump());
		}
		Check(ImprintProbe::attachments == 0, "Asset validation never attaches candidate Components to a Scene");
	}

	void TestSchemaAndPolicyFailures()
	{
		const json input = MakeAsset();
		for (const std::string& path : { "/version", "/definitionRevision", "/rootActorLocalObjectId", "/nextLocalObjectId", "/actors",
			"/actors/0/localObjectId", "/actors/0/parentLocalObjectId", "/actors/0/properties", "/actors/0/components",
			"/actors/0/components/0/localObjectId", "/actors/0/components/0/type", "/actors/0/components/0/properties",
			"/actors/0/properties/name", "/actors/0/properties/tag", "/actors/0/properties/is_active",
			"/actors/0/components/0/properties/position" })
		{
			json bad = input;
			const json::json_pointer pointer(path);
			bad[pointer.parent_pointer()].erase(pointer.back());
			Check(Rejects(bad), "Missing field is located at " + path);
			if (path == "/actors/0/parentLocalObjectId") continue;
			bad = input;
			bad[pointer] = nullptr;
			Check(Rejects(bad), "Unexpected null is located at " + path);
		}
		for (const std::string& path : { "", "/actors/0", "/actors/0/components/0", "/actors/0/properties", "/actors/0/components/0/properties" })
		{
			json bad = input;
			bad[json::json_pointer(path)]["unknown/name~"] = false;
			Check(Rejects(bad), "Unknown field has an escaped JSON Pointer: " + path);
		}
		for (const json& version : std::vector<json>{ 0, 2, 1.0, true, "1" })
		{
			json bad = input; bad["version"] = version;
			Check(Rejects(bad), "Invalid asset version is rejected");
		}
		json bad = input;
		bad["actors"][0]["components"][0]["type"] = "TransformComponent";
		Check(Rejects(bad), "Unregistered illustrative type name is rejected");
		bad = input;
		bad["actors"][0]["components"] = json::array();
		Check(Rejects(bad), "Every Actor requires a Transform-family component");
		bad = input; bad["nextLocalObjectId"] = 50;
		bad["actors"][0]["components"].push_back(MakeComponent("RectTransform", 12));
		Check(Rejects(bad), "Mixed Transform-family duplicates use existing Actor policy");
		bad = input; bad["nextLocalObjectId"] = 50;
		bad["actors"][0]["components"].push_back(MakeComponent("Camera", 12));
		bad["actors"][0]["components"].push_back(MakeComponent("Camera", 13));
		Check(Rejects(bad), "Unique Component cardinality is enforced");
		bad = input;
		bad["actors"][0]["components"][0]["properties"]["position"][0] = (std::numeric_limits<double>::infinity)();

		Check(!ActorImprintAssetDeserializer::Deserialize(bad, nullptr), "Nonfinite property values are rejected");
		bad = input; bad["nextLocalObjectId"] = 50;
		bad["actors"][0]["components"].push_back(MakeComponent("NonCanonicalImprintProbe", 12));
		Check(Rejects(bad), "Non-idempotent Component normalization cannot become a default");
		bad = input; bad["nextLocalObjectId"] = 50;
		bad["actors"][0]["components"].push_back(MakeComponent("Camera", 12));
		bad["actors"][0]["components"][1]["properties"]["lens"]["nearZ"] = 100.0;
		bad["actors"][0]["components"][1]["properties"]["lens"]["farZ"] = 10.0;
		Check(Rejects(bad), "Whole-Component invariant is enforced through Reflection");
	}

	void TestGraphThroughAssetReader()
	{
		const json input = MakeAsset();
		json bad = input;
		bad["actors"][0]["components"][0]["localObjectId"] = 10;
		Check(Rejects(bad), "Asset reader rejects cross-kind duplicate identity");
		for (const json& id : std::vector<json>{ 0, -1, 10.0, true, "10", 1.8446744073709552e19 })
		{
			bad = input; bad["actors"][0]["localObjectId"] = id;
			Check(Rejects(bad), "Asset reader rejects malformed or overflowing object ID");
		}
		bad = input; bad["nextLocalObjectId"] = 11;
		Check(Rejects(bad), "Asset reader enforces the next-ID high watermark");
		bad = input; bad["rootActorLocalObjectId"] = 11;
		Check(Rejects(bad), "Component cannot be declared the root Actor");
		json child = input["actors"][0];
		child["localObjectId"] = 20;
		child["components"][0]["localObjectId"] = 21;
		bad = input; bad["nextLocalObjectId"] = 50;
		bad["actors"].push_back(child);
		Check(Rejects(bad), "Asset reader rejects a second root");
		for (const int parent : { 11, 99, 20 })
		{
			bad["actors"][1]["parentLocalObjectId"] = parent;
			Check(Rejects(bad), "Asset reader rejects Component/external parent and disconnected cycle");
		}
	}

	class MeshCatalog final : public AssetReferenceSaveContext
	{
	public:
		Guid id = GuidGenerator::Generate();
		bool Validate(const Guid& guid, AssetType type) const override
		{
			if (guid != id) return false;
			return type == AssetType::Mesh;
		}
	};

	void TestAssetReferencesAndOtherBuiltins()
	{
		MeshCatalog catalog;
		json input = MakeAsset(); input["nextLocalObjectId"] = 50;
		input["actors"][0]["components"].push_back(MakeComponent("MeshRenderer", 12));
		input["actors"][0]["components"][1]["properties"]["meshAssetId"] = catalog.id.ToString();
		Check(!ActorImprintAssetDeserializer::Deserialize(input), "Non-null AssetReference requires a validating context");
		auto imprint = ActorImprintAssetDeserializer::Deserialize(input, &catalog);
		Check(imprint &&
			ActorImprintAssetSerializer::Serialize(*imprint)["actors"][0]["components"][1]["properties"]["meshAssetId"] == catalog.id.ToString(),
			"AssetReference reuses the existing typed GUID codec and catalog validation");
		input["actors"][0]["components"][1]["properties"]["meshAssetId"] = GuidGenerator::Generate().ToString();
		Check(!ActorImprintAssetDeserializer::Deserialize(input, &catalog), "Missing referenced asset is rejected");
		input["actors"][0]["components"][1] = MakeComponent("SpriteRenderer", 12);
		input["actors"][0]["components"][1]["properties"]["textureAssetId"] = catalog.id.ToString();
		Check(!ActorImprintAssetDeserializer::Deserialize(input, &catalog), "Wrong referenced asset type is rejected");
		for (const char* type : { "MeshRenderer", "SpriteRenderer", "UIRenderer", "UIImage", "Canvas", "Camera", "Collider" })
		{
			input = MakeAsset(); input["nextLocalObjectId"] = 50;
			input["actors"][0]["components"].push_back(MakeComponent(type, 12));
			auto builtin = ActorImprintAssetDeserializer::Deserialize(input);
			Check(builtin != nullptr, std::string("Builtin properties normalize through existing Reflection: ") + type);
		}
	}

	void TestFileAndRevisionContract()
	{

		auto sample = ActorImprintAssetDeserializer::Load("Tests/Fixtures/ActorImprint/Minimal.imprint", nullptr);
		Check(sample != nullptr, "Sample .imprint file loads through the production reader");
		std::ifstream input("Tests/Fixtures/ActorImprint/Minimal.imprint");
		const json sampleJson = json::parse(input);
		Check(sample &&
			ActorImprintAssetSerializer::Serialize(*sample).dump() == sampleJson.dump(),
			"Sample asset is a canonical round-trip golden fixture");
		const DefinitionRevision revision = DefinitionRevision::Generate();
		DefinitionRevision parsed;
		Check(revision.IsValid() &&
			DefinitionRevision::TryParse(revision.ToString(), parsed) &&
			parsed == revision,
			"DefinitionRevision uses nonzero GUID identity and equality");
		Check(!DefinitionRevision::TryParse("bad-revision", parsed) && parsed == revision, "Invalid revision parse leaves output unchanged");
		json bad = MakeAsset(); bad["definitionRevision"] = "{00000000-0000-0000-0000-000000000000}";
		Check(Rejects(bad), "Zero revision is rejected");
		const auto path = std::filesystem::temp_directory_path() / ("101Imprint-" + revision.ToString() + ".imprint");
		std::string duplicate = MakeAsset().dump();
		duplicate.replace(duplicate.find("\"version\":1"), 11, "\"version\":1,\"version\":1");
		{ std::ofstream file(path); file << duplicate; }
		Check(!ActorImprintAssetDeserializer::Load(path.string(), nullptr),
			"File parser rejects duplicate fields before DOM canonicalization loses them");
		{ std::ofstream file(path); file << "{\"version\":"; }
		Check(!ActorImprintAssetDeserializer::Load(path.string(), nullptr), "Malformed JSON has a file diagnostic and no partial definition");
		std::filesystem::remove(path);
		Check(!ActorImprintAssetDeserializer::Load(path.string(), nullptr), "Missing file reports an I/O failure");
	}
}

int main()
{
	TestPilot();
	RegisterProbes();
	TestHierarchyComponentsAndReferences();
	TestSchemaAndPolicyFailures();
	TestGraphThroughAssetReader();
	TestAssetReferencesAndOtherBuiltins();
	TestFileAndRevisionContract();
	if (g_failures == 0) { std::cout << "All ActorImprintAsset tests passed.\n"; return 0; }
	std::cerr << g_failures << " ActorImprintAsset test(s) failed.\n";
	return 1;
}
