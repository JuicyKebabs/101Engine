#include "Engine/ActorImprint/Detail/ActorImprintDetachedActors.h"
#include "Engine/ActorImprint/ActorImprintAssetDeserializer.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/Camera.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
	int g_failures = 0;
	void Check(bool condition, const char* name)
	{
		if (condition) std::cout << "[PASS] " << name << '\n';
		else { ++g_failures; std::cerr << "[FAIL] " << name << '\n'; }
	}
	void TestPilot()
	{
		auto definition = ActorImprintAssetDeserializer::Load("Tests/Fixtures/ActorImprint/Minimal.imprint");
		Check(definition != nullptr, "Pilot: immutable definition loads through ET-08");
		if (!definition) return;
		SceneBase destination;

		auto actors = ActorImprintDetail::CreateDetachedActors(*definition, destination, nullptr);
		Check(actors && actors->size() == 1, "Pilot: definition creates the complete detached Actor candidate");
		if (!actors) return;
		const Actor& actor = *actors->front();
		Check(actor.GetGuid().IsValid() &&
			actor.GetOwner() == nullptr &&
			actor.GetHandle().IsNull(),
			"Pilot: candidate gets a new GUID but no Scene ownership or Handle");
		Check(actors->front()->GetComponentByClass<Transform>() &&
			destination.GetAllActors().empty(),
			"Pilot: real Component exists without publishing anything to the destination");
	}

	class CandidateProbe final : public Component
	{
	public:
		static inline int live = 0;
		static inline int constructed = 0;
		static inline int attached = 0;
		static inline bool rejectValue = false;
		static inline bool rejectFactory = false;
		static inline bool throwFactory = false;
		float weight = 1.0f;
		ActorReference target;
		CandidateProbe() { ++live; ++constructed; }
		~CandidateProbe() override { --live; }
	private:
		void OnAttachOverride() override { ++attached; }
		void OnStartOverride() override {}
		void PreUpdateOverride(float) override {}
		void UpdateOverride(float) override {}
		void LateUpdateOverride(float) override {}
		void OnDestroyOverride() override {}
	};

	void RegisterProbe()
	{
		TypeMetadataBuilder<CandidateProbe> builder("CandidateProbe");
		builder.Property("weight", &CandidateProbe::weight).Validate([](float) { return !CandidateProbe::rejectValue; });
		builder.Property("target", &CandidateProbe::target);
		ComponentRegistry::Get().RegisterGameComponent("CandidateProbe",
			[]() -> Component*
			{
				if (CandidateProbe::throwFactory) throw std::runtime_error("Injected factory failure");
				return CandidateProbe::rejectFactory ? nullptr : new CandidateProbe();
			},
			typeid(CandidateProbe), std::make_unique<TypeMetadata>(*builder.Build()));
	}

	nlohmann::json ReadSample()
	{
		std::ifstream file("Tests/Fixtures/ActorImprint/Minimal.imprint");
		return nlohmann::json::parse(file);
	}

	std::unique_ptr<const ActorImprint> MakeHierarchy()
	{
		using json = nlohmann::json;
		auto source = ReadSample();
		auto child = source["actors"][0];
		child["localObjectId"] = 20;
		child["parentLocalObjectId"] = 10;
		child["components"][0]["localObjectId"] = 21;
		source["actors"].push_back(child);
		source["nextLocalObjectId"] = 30;
		for (int id : { 13, 12 })
		{
			source["actors"][0]["components"].push_back({ { "localObjectId", id }, { "type", "CandidateProbe" },
				{ "properties", { { "weight", static_cast<double>(id) },
					{ "target", { { "type", "ActorReference" }, { "scope", "local" }, { "localObjectId", 20 } } } } } });
		}
		std::reverse(source["actors"].begin(), source["actors"].end());
		return ActorImprintAssetDeserializer::Deserialize(source);
	}

	void TestIdentityAndIndependentState()
	{
		auto definition = MakeHierarchy();
		Check(definition != nullptr, "Referenced multi-Component definition is valid");
		if (!definition) return;
		SceneBase destination;

		auto first = ActorImprintDetail::CreateDetachedActors(*definition, destination, nullptr);
		auto second = ActorImprintDetail::CreateDetachedActors(*definition, destination, nullptr);
		Check(first && second && first->size() == 2 && second->size() == 2, "Two complete detached candidate sets can coexist");
		if (!first || !second) return;
		Check((*first)[0]->GetGuid() != (*second)[0]->GetGuid() &&
			(*first)[1]->GetGuid() != (*second)[1]->GetGuid(),
			"Each creation gets distinct Actor GUIDs");
		auto* probe = static_cast<CandidateProbe*>((*first)[0]->GetComponentByExactType(typeid(CandidateProbe), 0));
		auto* otherProbe = static_cast<CandidateProbe*>((*second)[0]->GetComponentByExactType(typeid(CandidateProbe), 0));
		Check(probe &&
			otherProbe &&
			probe != otherProbe &&
			probe->weight == 12.0f,
			"Components use definition identity order and own separate state");
		if (!probe || !otherProbe) return;
		Check(probe->target.GetGuid() == (*first)[1]->GetGuid() &&
			otherProbe->target.GetGuid() == (*second)[1]->GetGuid(),
			"Internal references translate into the corresponding candidate's forward Actor GUID");
		probe->weight = 999.0f;
		(*first)[0]->SetName("Changed candidate");
		Check(otherProbe->weight == 12.0f &&
			(*second)[0]->GetName() == "Root" &&
			definition->GetActors()[0].components[1].properties["weight"] == 12.0,
			"Candidate mutations affect neither another candidate nor immutable defaults");
		ActorImprintReferenceCodec::ActorGuids saved;
		for (const auto& record : definition->GetActors()) saved.emplace(record.id, GuidGenerator::Generate());
		auto restored = ActorImprintDetail::CreateDetachedActors(*definition, destination, &saved);
		Check(restored &&
			(*restored)[0]->GetGuid() == saved.at(10) &&
			(*restored)[1]->GetGuid() == saved.at(20),
			"Restore preserves every supplied Actor GUID");
		Check(destination.GetAllActors().empty() &&
			CandidateProbe::attached == 0 &&
			(*first)[1]->GetParentHandle().IsNull(),
			"Candidate creation performs no Scene publication, hierarchy connection or attach");
	}

	void TestFailures()
	{
		auto definition = MakeHierarchy();
		if (!definition) { Check(false, "Failure-test definition loads"); return; }
		SceneBase destination;
		Actor* existing = destination.AddRootActor(ActorFactory::CreateEmptyActor({}));
		const auto existingHandle = existing->GetHandle();
		const auto existingGuid = existing->GetGuid();
		const auto initialActors = destination.GetAllActors();
		ActorImprintReferenceCodec::ActorGuids valid{ { 10, GuidGenerator::Generate() }, { 20, GuidGenerator::Generate() } };
		std::vector<ActorImprintReferenceCodec::ActorGuids> invalid;
		invalid.push_back({ { 10, valid.at(10) } });
		invalid.push_back({ { 10, valid.at(10) }, { 20, valid.at(20) }, { 99, GuidGenerator::Generate() } });
		invalid.push_back({ { 10, valid.at(10) }, { 21, valid.at(20) } });
		invalid.push_back({ { 10, Guid{} }, { 20, valid.at(20) } });
		invalid.push_back({ { 10, valid.at(10) }, { 20, valid.at(10) } });
		invalid.push_back({ { 10, existingGuid }, { 20, valid.at(20) } });

		CandidateProbe::constructed = 0;
		for (const auto& mapping : invalid)
		{
			Check(!ActorImprintDetail::CreateDetachedActors(*definition, destination, &mapping),
				"Missing/extra/component/zero/duplicate/conflicting GUID mappings fail");
		}
		Check(CandidateProbe::constructed == 0, "Invalid identity input is rejected before any Component factory executes");
		CandidateProbe::rejectValue = true;
		Check(!ActorImprintDetail::CreateDetachedActors(*definition, destination, nullptr), "Invalid property rejects the complete candidate");
		CandidateProbe::rejectValue = false;
		CandidateProbe::rejectFactory = true;
		Check(!ActorImprintDetail::CreateDetachedActors(*definition, destination, nullptr), "Factory failure returns no partial candidate");
		CandidateProbe::rejectFactory = false;
		CandidateProbe::throwFactory = true;
		Check(!ActorImprintDetail::CreateDetachedActors(*definition, destination, nullptr), "Factory exception discards the candidate");
		CandidateProbe::throwFactory = false;
		ComponentRegistry::Get().UnregisterAllGameComponents();
		Check(!ActorImprintDetail::CreateDetachedActors(*definition, destination, nullptr),
			"Missing Component registration cannot be used through an old definition");
		RegisterProbe();
		Check(CandidateProbe::live == 0 && CandidateProbe::attached == 0, "Failed preparation destroys all candidates without attach");
		Check(destination.GetAllActors() == initialActors &&
			destination.ResolveActor(existingGuid) == existing &&
			destination.ResolveActor(existingHandle) == existing &&
			existing->GetParentHandle().IsNull(),
			"Every failure preserves existing Scene actors, GUID mapping, handles and hierarchy");
		destination.Finalize();
	}

	void TestDeepDefinition()
	{
		constexpr std::size_t count = 1024;
		auto source = ReadSample();
		const auto prototype = source["actors"][0];
		source["actors"] = nlohmann::json::array();
		for (std::size_t i = 0; i < count; ++i)
		{
			auto actor = prototype;
			const auto id = 10 + i * 2;
			actor["localObjectId"] = id;
			actor["parentLocalObjectId"] = i == 0 ? nlohmann::json(nullptr) : nlohmann::json(id - 2);
			actor["components"][0]["localObjectId"] = id + 1;
			source["actors"].push_back(std::move(actor));
		}
		source["nextLocalObjectId"] = 10 + count * 2;
		std::reverse(source["actors"].begin(), source["actors"].end());
		auto definition = ActorImprintAssetDeserializer::Deserialize(source);
		Check(definition != nullptr, "Deep reversed input forms a valid definition");
		if (!definition) return;
		SceneBase destination;

		auto actors = ActorImprintDetail::CreateDetachedActors(*definition, destination, nullptr);
		Check(actors &&
			actors->size() == count &&
			destination.GetAllActors().empty(),
			"Deep definition creates detached candidates iteratively without a second runtime hierarchy");
	}
}

int main()
{
	TestPilot();
	RegisterProbe();
	TestIdentityAndIndependentState();
	TestFailures();
	TestDeepDefinition();
	return g_failures ? 1 : 0;
}
