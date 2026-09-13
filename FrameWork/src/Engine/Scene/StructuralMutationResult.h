#pragma once

enum class StructuralMutationReason
{
	Success,
	InvalidActor,
	ForeignScene,
	StaleHandle,
	PendingDestroy,
	TransactionInProgress,
	ImprintMemberImmutable,
	InstanceDestroyRequired,
	InstanceSnapshotRequired,
	InvalidComponent,
	ComponentPolicyViolation,
	HierarchyCycle,
	UIConstraintViolation,
	InvalidInstance,
	SingleRootInvariant,
	ExternalActorReference,
};

struct StructuralMutationResult
{
	StructuralMutationReason reason = StructuralMutationReason::Success;
	constexpr explicit operator bool() const { return reason == StructuralMutationReason::Success; }
	constexpr bool Report(StructuralMutationResult* output) const
	{
		if (output) *output = *this;
		return static_cast<bool>(*this);
	}
	friend bool operator==(const StructuralMutationResult&, const StructuralMutationResult&) = default;
};
