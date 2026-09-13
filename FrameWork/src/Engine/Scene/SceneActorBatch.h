#pragma once
#include "SceneBase.h"

// Internal construction transaction. The candidate uses the ordinary Scene's
// pool, hierarchy and reference lookup. No candidate callback may retain its
// Scene address; registration belongs to OnAttach after transfer.
class SceneActorBatch
{
public:
	SceneActorBatch(const SceneActorBatch&) = delete;
	SceneActorBatch& operator=(const SceneActorBatch&) = delete;
	~SceneActorBatch();

private:
	friend class ActorImprintSystem;
	friend class ActorImprintDefinitionExpander;
	friend class SceneLoader;
	explicit SceneActorBatch(SceneBase& destination);
	void Stage(std::vector<std::unique_ptr<Actor>> actors);
	SceneBase& Candidate() { return m_candidate; }
	const std::vector<ActorHandle>& Handles() const { return m_handles; }
	bool SetParent(Actor* actor, Actor* parent);
	bool ResolveAndValidate(Actor* root, Actor* externalParent, std::string& error);
	void PrepareCommit(Actor* root, Actor* externalParent);
	void Commit() noexcept;
	void Attach() noexcept;

	SceneBase& m_destination;
	SceneBase m_candidate;
	std::vector<ActorHandle> m_handles;
	std::unordered_map<Guid, ActorHandle> m_guidMap;
	std::unordered_map<ActorHandle, Guid> m_handleMap;
	Actor* m_root = nullptr;
	Actor* m_externalParent = nullptr;
	bool m_prepared = false;
	bool m_committed = false;
};
