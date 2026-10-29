// Regression checks for the lifetime boundary between Scene-owned Colliders
// and the CollisionSystem's borrowed registration/contact/history pointers.
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/Collider.h"
#include "Engine/Physics/CollisionSystem.h"
#include "Engine/Scene/SceneBase.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

// Inspect the real private containers without adding a production debug API or
// guessing object offsets. Explicit instantiation supplies typed member access.
struct RegistrationAccess
{
    using Type = std::vector<Collider*> CollisionSystem::*;
    friend Type Member(RegistrationAccess);
};
struct NarrowPhaseAccess
{
    using Type = std::vector<CollisionPair> CollisionSystem::*;
    friend Type Member(NarrowPhaseAccess);
};
struct CurrentPairsAccess
{
    using Type = std::vector<CollisionPair> CollisionSystem::*;
    friend Type Member(CurrentPairsAccess);
};
struct PreviousPairsAccess
{
    using Type = std::vector<CollisionPair> CollisionSystem::*;
    friend Type Member(PreviousPairsAccess);
};
template<class Tag, typename Tag::Type Pointer>
struct BindAccess
{
    friend typename Tag::Type Member(Tag)
    {
        return Pointer;
    }
};
template struct BindAccess<RegistrationAccess, &CollisionSystem::m_pCollidersList>;
template struct BindAccess<NarrowPhaseAccess, &CollisionSystem::m_pNarrowPhaseColliders>;
template struct BindAccess<CurrentPairsAccess, &CollisionSystem::m_currentCollisionPairs>;
template struct BindAccess<PreviousPairsAccess, &CollisionSystem::m_previousCollisionPairs>;

template<class Tag>
auto& State(CollisionSystem& system)
{
    Tag access;
    return system.*Member(access);
}

void Require(bool condition, const char* purpose)
{
    if (!condition)
    {
        throw std::runtime_error(purpose);
    }
}

struct ColliderActor
{
    Actor* actor = nullptr;
    Collider* collider = nullptr;
};

Collider* AddCollider(Actor& actor)
{
    Collider::ParamDesc description;
    description.type = ColliderType::SPHERE;
    Vector3 scale;
    scale.x = 2;
    scale.y = 2;
    scale.z = 2;
    description.localScale = scale;
    auto component = std::make_unique<Collider>();
    component->SetParams(description);
    Collider* collider = component.get();
    Require(actor.AddComponent(std::move(component)) == collider, "Add Collider");
    return collider;
}

ColliderActor AddBody(SceneBase& scene, const char* name, float positionX = 0)
{
    Actor::InitDesc description;
    description.name = name;
    auto actor = ActorFactory::CreateEmptyActor(description);
    Vector3 position;
    position.x = positionX;
    actor->GetComponentByClass<Transform>()->SetLocalPosition(position);
    ColliderActor body;
    body.collider = AddCollider(*actor);
    body.actor = scene.AddRootActor(std::move(actor));
    Require(body.actor != nullptr, "Add Collider Actor");
    return body;
}

bool Registered(CollisionSystem& system, Collider* collider)
{
    const auto& registrations = State<RegistrationAccess>(system);
    return std::find(registrations.begin(), registrations.end(), collider) != registrations.end();
}

void Tick(SceneBase& scene)
{
    scene.PreUpdate(0);
    scene.Update(0);
    scene.LateUpdate(0);
}

void TestActorCollectionAndComponentRemoval()
{
    EngineContext context;
    SceneBase scene;
    scene.Initialize(context);
    ColliderActor first = AddBody(scene, "Collected");
    ColliderActor survivor = AddBody(scene, "Survivor", 10);
    CollisionSystem& system = *scene.GetCollisionSystem();
    Require(State<RegistrationAccess>(system).empty(), "Edit-only attach must not start physics");
    scene.PreUpdate(0);
    Require(Registered(system, first.collider), "OnStart registers Collider");

    // EditorUpdate collects destroyed Actors without a collision sweep.
    // Lifecycle teardown itself must remove references; the next frame is too late.
    const ActorHandle oldHandle = first.actor->GetHandle();
    Require(first.actor->Destroy(), "Mark Actor destroyed");
    scene.EditorUpdate(0);
    Require(!Registered(system, first.collider), "Actor collection removes its Collider registration");
    Require(scene.ResolveActor(oldHandle) == nullptr, "Actor storage was actually collected");
    Require(Registered(system, survivor.collider), "Unrelated Collider remains registered");
    Tick(scene);

    Collider* removed = survivor.collider;
    Require(scene.RemoveActorComponentImmediate(survivor.actor, removed), "Remove only Collider Component");
    Require(!Registered(system, removed), "Component removal unregisters before storage release");
    Collider* replacement = AddCollider(*survivor.actor);
    scene.PreUpdate(0);
    Require(Registered(system, replacement), "Replacement Component starts a fresh registration");
    Require(State<RegistrationAccess>(system).size() == 1, "Replacement has no duplicate registration");
    scene.Finalize();
    Require(State<RegistrationAccess>(system).empty(), "Scene finalization empties registration list");
    std::cout << "[PASS] Edit attach / Actor collection / Component removal / replacement / Scene finalization\n";
}

CollisionPair MakePair(Collider* first, Collider* second)
{
    CollisionPair pair;
    pair.colliderA = first;
    pair.colliderB = second;
    return pair;
}

void SeedHistory(CollisionSystem& system, Collider* removed, Collider* first, Collider* second)
{
    CollisionPair affected = MakePair(removed, first);
    CollisionPair unrelated = MakePair(first, second);
    State<NarrowPhaseAccess>(system) = {affected, unrelated};
    State<CurrentPairsAccess>(system) = {affected, unrelated};
    State<PreviousPairsAccess>(system) = {affected, unrelated};
    CollisionInfo affectedInfo;
    affectedInfo.opponent = removed;
    affectedInfo.state = COLLISION_STAY;
    CollisionInfo unrelatedInfo;
    unrelatedInfo.opponent = second;
    unrelatedInfo.state = COLLISION_ENTER;
    first->AddCollisionInfo(affectedInfo);
    first->AddCollisionInfo(unrelatedInfo);
    first->SetDetected(true);
    removed->AddCollisionInfo(unrelatedInfo);
}

void TestUnregisterPurgesHistoryAndPreservesOtherContacts()
{
    EngineContext context;
    SceneBase scene;
    scene.Initialize(context);
    ColliderActor removed = AddBody(scene, "Removed");
    ColliderActor first = AddBody(scene, "First");
    ColliderActor second = AddBody(scene, "Second");
    scene.PreUpdate(0);
    CollisionSystem& system = *scene.GetCollisionSystem();
    SeedHistory(system, removed.collider, first.collider, second.collider);

    system.Unregister(removed.collider);
    system.Unregister(removed.collider);
    Require(!Registered(system, removed.collider), "Repeated Unregister is harmless");
    Require(State<NarrowPhaseAccess>(system).size() == 1, "Pending narrow-phase pair was purged");
    Require(State<CurrentPairsAccess>(system).size() == 1, "Current pair was purged");
    Require(State<PreviousPairsAccess>(system).size() == 1, "Previous pair was purged");
    const auto& contact = first.collider->GetCollisionInfos();
    Require(contact.size() == 1 && contact.front().opponent == second.collider, "Only the removed opponent is erased");
    Require(contact.front().state == COLLISION_ENTER, "Unrelated contact state stays intact");
    Require(first.collider->isDetected(), "Another valid contact keeps detected state");
    Require(removed.collider->GetCollisionInfos().empty(), "Removed Collider releases its own contact pointers");

    // Exercise the destruction override's fallback after ordinary registration
    // was already ended; later Scene teardown will also call OnDetach.
    removed.collider->OnDestroyOverride();
    Require(State<RegistrationAccess>(system).size() == 2, "Destroy after Unregister does not remove other Colliders");
    scene.Finalize();
    Require(State<PreviousPairsAccess>(system).empty(), "Scene teardown purges remaining pair history");
    std::cout << "[PASS] All pair caches / opponent infos / idempotent unregister / unrelated contacts\n";
}

void TestAdjacentRemovalAndBulkClear()
{
    EngineContext context;
    SceneBase scene;
    scene.Initialize(context);
    ColliderActor first = AddBody(scene, "First");
    ColliderActor second = AddBody(scene, "Second");
    ColliderActor survivor = AddBody(scene, "Survivor");
    scene.PreUpdate(0);
    CollisionSystem& system = *scene.GetCollisionSystem();
    first.collider->SetDeleteFlag(true);
    second.collider->SetDeleteFlag(true);
    system.CheckColliders();
    Require(State<RegistrationAccess>(system).size() == 1, "Adjacent erasures do not skip or invalidate traversal");
    Require(Registered(system, survivor.collider), "Valid trailing registration survives");

    CollisionPair pair = MakePair(survivor.collider, survivor.collider);
    State<NarrowPhaseAccess>(system).push_back(pair);
    State<CurrentPairsAccess>(system).push_back(pair);
    State<PreviousPairsAccess>(system).push_back(pair);
    system.ClearColliders();
    system.ClearColliders();
    Require(State<RegistrationAccess>(system).empty(), "Bulk clear removes membership");
    Require(State<NarrowPhaseAccess>(system).empty(), "Bulk clear removes pending narrow pairs");
    Require(State<CurrentPairsAccess>(system).empty(), "Bulk clear removes current pairs");
    Require(State<PreviousPairsAccess>(system).empty(), "Bulk clear removes previous pairs");

    CollisionSystem anotherSystem;
    anotherSystem.Register(survivor.collider);
    Require(Registered(anotherSystem, survivor.collider), "Bulk clear also releases the old registration backlink");
    anotherSystem.Unregister(survivor.collider);
    std::cout << "[PASS] Adjacent removal / repeated bulk clear / registration backlink\n";
}

void TestOrdinaryEnterStayExitAndDestroyedOpponent()
{
    EngineContext context;
    SceneBase scene;
    scene.Initialize(context);
    ColliderActor first = AddBody(scene, "First", 0);
    ColliderActor second = AddBody(scene, "Second", 0.5f);
    scene.EditorUpdate(0);
    for (Collider* collider : {first.collider, second.collider})
    {
        collider->SetPreviousState();
        collider->UpdateAABB();
    }
    Tick(scene);
    Require(!first.collider->GetCollisionInfos().empty(), "Real sphere collision produces a contact");
    Require(first.collider->GetCollisionInfos().front().state == COLLISION_ENTER, "First contact is ENTER");
    Tick(scene);
    Require(!first.collider->GetCollisionInfos().empty(), "Ongoing contact remains available");
    Require(first.collider->GetCollisionInfos().front().state == COLLISION_STAY, "Ongoing contact is STAY");

    Vector3 separatedPosition;
    separatedPosition.x = 10;
    second.actor->GetComponentByClass<Transform>()->SetLocalPosition(separatedPosition);
    Tick(scene);
    Require(!first.collider->GetCollisionInfos().empty(), "Separation produces an exit contact");
    Require(first.collider->GetCollisionInfos().front().state == COLLISION_EXIT, "Live separation still produces EXIT");

    Require(second.actor->Destroy(), "Destroy opponent after EXIT");
    scene.EditorUpdate(0);
    Require(first.collider->GetCollisionInfos().empty(), "Destruction discards opponent info instead of retaining a dead EXIT pointer");
    Tick(scene);
    std::cout << "[PASS] Real ENTER / STAY / EXIT preserved; destroyed opponent leaves no borrowed pointer\n";
}

}

int main()
{
    try
    {
        TestActorCollectionAndComponentRemoval();
        TestUnregisterPurgesHistoryAndPreservesOtherContacts();
        TestAdjacentRemovalAndBulkClear();
        TestOrdinaryEnterStayExitAndDestroyedOpponent();
    }
    catch (const std::exception& error)
    {
        std::cerr << "[FAIL] " << error.what() << '\n';
        return 1;
    }
    return 0;
}
