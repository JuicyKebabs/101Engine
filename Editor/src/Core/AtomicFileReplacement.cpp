#include "AtomicFileReplacement.h"

#include "Engine/Core/GUID/GuidGenerator.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <limits>

namespace
{
	class UniqueHandle
	{
	public:
		explicit UniqueHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}
		~UniqueHandle() { if (m_handle != INVALID_HANDLE_VALUE) CloseHandle(m_handle); }
		UniqueHandle(const UniqueHandle&) = delete;
		UniqueHandle& operator=(const UniqueHandle&) = delete;
		HANDLE Get() const { return m_handle; }
	private:
		HANDLE m_handle;
	};

	bool Fail(AtomicFileReplacementError* error,
		AtomicFileReplacementErrorCode code, const std::filesystem::path& path,
		DWORD platformError, std::string message)
	{
		if (error) *error = { code, path, platformError, std::move(message) };
		return false;
	}

	bool Exists(const std::filesystem::path& path)
	{
		return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	bool ValidateDestination(const std::filesystem::path& destination,
		AtomicFileReplacementError* error)
	{
		if (destination.empty() || !destination.is_absolute())
			return Fail(error, AtomicFileReplacementErrorCode::InvalidDestination,
				destination, ERROR_INVALID_NAME, "Destination must be an absolute path.");
		const DWORD attributes = GetFileAttributesW(destination.c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
			return Fail(error, AtomicFileReplacementErrorCode::InvalidDestination,
				destination, attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_DIRECTORY,
				"Destination must be an existing file.");

		std::array<wchar_t, 32768> volumeRoot{};
		if (!GetVolumePathNameW(destination.c_str(), volumeRoot.data(),
			static_cast<DWORD>(volumeRoot.size())))
			return Fail(error, AtomicFileReplacementErrorCode::UnsupportedVolume,
				destination, GetLastError(), "Could not resolve the destination volume.");
		if (GetDriveTypeW(volumeRoot.data()) != DRIVE_FIXED)
			return Fail(error, AtomicFileReplacementErrorCode::UnsupportedVolume,
				destination, ERROR_NOT_SUPPORTED, "Atomic saves require a local fixed volume.");

		std::array<wchar_t, 64> fileSystem{};
		if (!GetVolumeInformationW(volumeRoot.data(), nullptr, 0, nullptr, nullptr, nullptr,
			fileSystem.data(), static_cast<DWORD>(fileSystem.size())))
			return Fail(error, AtomicFileReplacementErrorCode::UnsupportedVolume,
				destination, GetLastError(), "Could not identify the destination filesystem.");
		if (_wcsicmp(fileSystem.data(), L"NTFS") != 0 && _wcsicmp(fileSystem.data(), L"ReFS") != 0)
			return Fail(error, AtomicFileReplacementErrorCode::UnsupportedVolume,
				destination, ERROR_NOT_SUPPORTED, "Atomic saves support local NTFS or ReFS destinations.");
		return true;
	}

	std::filesystem::path UniqueSibling(const std::filesystem::path& destination,
		const wchar_t* suffix)
	{
		return destination.parent_path() /
			(destination.filename().wstring() + L".101-" +
			 std::filesystem::path(GuidGenerator::Generate().ToString()).wstring() + suffix);
	}
}

bool AtomicFileReplacement::WriteTemporary(const std::filesystem::path& destination,
	std::string_view bytes, std::filesystem::path& outTemporary,
	AtomicFileReplacementError* outError)
{
	if (outError) *outError = {};
	outTemporary.clear();
	if (!ValidateDestination(destination, outError)) return false;

	std::filesystem::path temporary;
	HANDLE raw = INVALID_HANDLE_VALUE;
	for (int attempt = 0; attempt < 16; ++attempt)
	{
		temporary = UniqueSibling(destination, L".tmp");
		raw = CreateFileW(temporary.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
			CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_WRITE_THROUGH, nullptr);
		if (raw != INVALID_HANDLE_VALUE) break;
		if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS)
			return Fail(outError, AtomicFileReplacementErrorCode::TemporaryCreateFailed,
				temporary, GetLastError(), "Could not create the same-directory temporary file.");
	}
	if (raw == INVALID_HANDLE_VALUE)
		return Fail(outError, AtomicFileReplacementErrorCode::TemporaryCreateFailed,
			temporary, ERROR_FILE_EXISTS, "Could not allocate a unique temporary filename.");

	bool succeeded = true;
	DWORD failure = ERROR_SUCCESS;
	AtomicFileReplacementErrorCode failureCode = AtomicFileReplacementErrorCode::None;
	{
		UniqueHandle file(raw);
		std::size_t offset = 0;
		while (offset < bytes.size())
		{
			const DWORD requested = static_cast<DWORD>((std::min)(bytes.size() - offset,
				static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
			DWORD written = 0;
			if (!WriteFile(file.Get(), bytes.data() + offset, requested, &written, nullptr) || written != requested)
			{
				succeeded = false;
				failure = GetLastError();
				failureCode = AtomicFileReplacementErrorCode::TemporaryWriteFailed;
				break;
			}
			offset += written;
		}
		if (succeeded && !FlushFileBuffers(file.Get()))
		{
			succeeded = false;
			failure = GetLastError();
			failureCode = AtomicFileReplacementErrorCode::TemporaryFlushFailed;
		}
	}
	if (!succeeded)
	{
		DeleteFileW(temporary.c_str());
		return Fail(outError, failureCode, temporary, failure,
			"Could not persist the complete temporary file.");
	}
	outTemporary = std::move(temporary);
	return true;
}

bool AtomicFileReplacement::Replace(const std::filesystem::path& temporary,
	const std::filesystem::path& destination, AtomicFileReplacementError* outError)
{
	if (outError) *outError = {};
	if (!ValidateDestination(destination, outError)) return false;
	if (temporary.empty() || !temporary.is_absolute() ||
		temporary.parent_path().lexically_normal() != destination.parent_path().lexically_normal() ||
		!Exists(temporary))
		return Fail(outError, AtomicFileReplacementErrorCode::InvalidDestination,
			temporary, ERROR_INVALID_NAME, "Replacement input must be an existing sibling temporary file.");

	std::filesystem::path backup;
	for (int attempt = 0; attempt < 16; ++attempt)
	{
		backup = UniqueSibling(destination, L".bak");
		if (!Exists(backup)) break;
		backup.clear();
	}
	if (backup.empty())
		return Fail(outError, AtomicFileReplacementErrorCode::TemporaryCreateFailed,
			destination, ERROR_FILE_EXISTS, "Could not allocate a unique backup filename.");

	if (ReplaceFileW(destination.c_str(), temporary.c_str(), backup.c_str(), 0, nullptr, nullptr))
	{
		// Replacement already committed. A cleanup failure leaves a recoverable
		// backup but does not turn a successful save into a false failure.
		DeleteFileW(backup.c_str());
		return true;
	}
	const DWORD replaceError = GetLastError();

	if (Exists(backup))
	{
		bool recovered = false;
		if (Exists(destination))
			recovered = ReplaceFileW(destination.c_str(), backup.c_str(), nullptr, 0, nullptr, nullptr) != FALSE;
		else
			recovered = MoveFileExW(backup.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
		if (!recovered)
			return Fail(outError, AtomicFileReplacementErrorCode::RecoveryFailed,
				destination, GetLastError(), "Replacement failed and the original backup could not be restored.");
	}
	DeleteFileW(temporary.c_str());
	return Fail(outError, AtomicFileReplacementErrorCode::ReplaceFailed,
		destination, replaceError, "The destination rejected the atomic replacement; the original was retained.");
}

void AtomicFileReplacement::RemoveTemporary(const std::filesystem::path& temporary) noexcept
{
	if (!temporary.empty()) DeleteFileW(temporary.c_str());
}
