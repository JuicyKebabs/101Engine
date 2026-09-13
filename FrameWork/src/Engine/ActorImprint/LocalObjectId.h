#pragma once
#include <cstdint>

// Persistent identity shared by Actors and Components within one Imprint asset.
// Zero is invalid. Issued IDs must never be reused, including after deletion.
using LocalObjectId = std::uint64_t;
inline constexpr LocalObjectId InvalidLocalObjectId = 0;
