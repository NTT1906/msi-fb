#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Minimal PawnIO-based Embedded Controller (EC) access.
// We use a PawnIO module that supports port I/O (inb/outb) and then implement
// the ACPI EC protocol (0x66/0x62) in userland, mirroring YAMDCC's approach.
class PawnIoEc {
public:
	PawnIoEc() = default;
	~PawnIoEc();

	PawnIoEc(const PawnIoEc&) = delete;
	PawnIoEc& operator=(const PawnIoEc&) = delete;

	bool Initialize(std::string& err);
	void Shutdown();

	bool ReadEcByte(uint8_t reg, uint8_t& outVal, std::string& err);
	bool WriteEcByte(uint8_t reg, uint8_t val, std::string& err);

	bool GetFullBlast(bool& enabled, std::string& err);
	bool ToggleFullBlast(bool& nowEnabled, std::string& err);

	// Debug helpers
	bool ReadPort66(uint8_t& outVal, std::string& err); // EC command/status port

private:
	bool PioInb(uint16_t port, uint8_t& outVal, std::string& err);
	bool PioOutb(uint16_t port, uint8_t val, std::string& err);

	bool WaitForEcStatus(uint8_t mask, bool wantSet, uint32_t timeoutMs, std::string& err);
	bool WaitRead(std::string& err);
	bool WaitWrite(std::string& err);

	static bool ReadFileBytes(const std::wstring& path, std::vector<uint8_t>& out, std::string& err);
	static std::wstring FindModulePath(std::string& err);
	static std::wstring FindPawnIOLibPath();

	void* dll_ = nullptr;
	void* handle_ = nullptr;

	// Function pointers (loaded dynamically from PawnIOLib.dll).
	using pawnio_open_fn = long(__stdcall*)(void**);
	using pawnio_load_fn = long(__stdcall*)(void*, const unsigned char*, size_t);
	using pawnio_execute_fn = long(__stdcall*)(void*, const char*, const unsigned long long*, size_t,
											  unsigned long long*, size_t, size_t*);
	using pawnio_close_fn = long(__stdcall*)(void*);

	pawnio_open_fn pawnio_open_ = nullptr;
	pawnio_load_fn pawnio_load_ = nullptr;
	pawnio_execute_fn pawnio_execute_ = nullptr;
	pawnio_close_fn pawnio_close_ = nullptr;
};

