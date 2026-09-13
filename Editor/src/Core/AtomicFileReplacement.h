#pragma once

#include <filesystem>
#include <string>
#include <string_view>

enum class AtomicFileReplacementErrorCode
{
	None,
	InvalidDestination,
	UnsupportedVolume,
	TemporaryCreateFailed,
	TemporaryWriteFailed,
	TemporaryFlushFailed,
	ReplaceFailed,
	RecoveryFailed,
};

struct AtomicFileReplacementError
{
	AtomicFileReplacementErrorCode code = AtomicFileReplacementErrorCode::None;
	std::filesystem::path path;
	unsigned long platformError = 0;
	std::string message;
};

// Windows desktop save primitive. The destination and all transaction files
// stay on one local NTFS/ReFS volume. This protects application-level readers
// from truncate/write gaps; power-loss durability is outside its contract.
class AtomicFileReplacement
{
public:
	static bool WriteTemporary(const std::filesystem::path& destination,
		std::string_view bytes, std::filesystem::path& outTemporary,
		AtomicFileReplacementError* outError = nullptr);
	static bool Replace(const std::filesystem::path& temporary,
		const std::filesystem::path& destination,
		AtomicFileReplacementError* outError = nullptr);
	static void RemoveTemporary(const std::filesystem::path& temporary) noexcept;
};
