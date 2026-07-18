#include "pawnio_ec.h"
#include "config.h"

#include <windows.h>

#include <chrono>
#include <fstream>
#include <sstream>

static std::string HrStr(long hr) {
	std::ostringstream oss;
	oss << "0x" << std::hex << (unsigned long)hr;
	return oss.str();
}

static std::wstring DirOfExe() {
	wchar_t buf[MAX_PATH]{};
	DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
	if (n == 0 || n >= MAX_PATH) return L"";
	std::wstring p(buf, buf + n);
	size_t slash = p.find_last_of(L"\\\\/");
	if (slash == std::wstring::npos) return L"";
	return p.substr(0, slash);
}

static std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
	if (a.empty()) return b;
	if (a.back() == L'\\' || a.back() == L'/') return a + b;
	return a + L"\\" + b;
}

static bool FileExists(const std::wstring& p) {
	DWORD attr = GetFileAttributesW(p.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

PawnIoEc::~PawnIoEc() { Shutdown(); }

std::wstring PawnIoEc::FindPawnIOLibPath() {
	wchar_t sys[MAX_PATH]{};
	DWORD n = SearchPathW(nullptr, L"PawnIOLib.dll", nullptr, MAX_PATH, sys, nullptr);
	if (n > 0 && n < MAX_PATH) return std::wstring(sys, sys + n);

	const std::wstring fixed = L"C:\\Program Files\\PawnIO\\PawnIOLib.dll";
	if (FileExists(fixed)) return fixed;
	return L"";
}

// Look for LpcACPIEC.bin near the executable or current working dir.
std::wstring PawnIoEc::FindModulePath(std::string& err) {
	const std::wstring modRel = L"LpcACPIEC.bin";

	wchar_t cwd[MAX_PATH]{};
	DWORD n = GetCurrentDirectoryW(MAX_PATH, cwd);
	std::wstring cwdDir = (n > 0 && n < MAX_PATH) ? std::wstring(cwd, cwd + n) : L"";

	std::wstring exeDir = DirOfExe();
	std::vector tries = {
		JoinPath(exeDir, modRel),
		JoinPath(JoinPath(exeDir, L".."), modRel),
		JoinPath(cwdDir, modRel),
		JoinPath(JoinPath(cwdDir, L".."), modRel),
	};

	for (const auto& t : tries) {
		if (FileExists(t)) return t;
	}

	err = "Could not find PawnIO module LpcACPIEC.bin.";
	return L"";
}

bool PawnIoEc::ReadFileBytes(const std::wstring& path, std::vector<uint8_t>& out, std::string& err) {
	std::ifstream f(path.c_str(), std::ios::binary);
	if (!f) {
		std::ostringstream oss;
		oss << "Failed to open module file.";
		err = oss.str();
		return false;
	}
	f.seekg(0, std::ios::end);
	std::streamoff sz = f.tellg();
	if (sz <= 0) {
		err = "Module file is empty.";
		return false;
	}
	f.seekg(0, std::ios::beg);
	out.resize((size_t)sz);
	f.read(reinterpret_cast<char*>(out.data()), sz);
	if (!f) {
		err = "Failed to read module file.";
		return false;
	}
	return true;
}

bool PawnIoEc::Initialize(std::string& err) {
	if (handle_) return true;

	const std::wstring dllPath = FindPawnIOLibPath();
	if (dllPath.empty()) {
		err = "PawnIOLib.dll not found (install PawnIO, or add PawnIOLib.dll to PATH).";
		return false;
	}

	HMODULE h = LoadLibraryW(dllPath.c_str());
	if (!h) {
		err = "LoadLibrary(PawnIOLib.dll) failed.";
		return false;
	}
	dll_ = (void*)h;

	pawnio_open_ = (pawnio_open_fn)GetProcAddress(h, "pawnio_open");
	pawnio_load_ = (pawnio_load_fn)GetProcAddress(h, "pawnio_load");
	pawnio_execute_ = (pawnio_execute_fn)GetProcAddress(h, "pawnio_execute");
	pawnio_close_ = (pawnio_close_fn)GetProcAddress(h, "pawnio_close");

	if (!pawnio_open_ || !pawnio_load_ || !pawnio_execute_ || !pawnio_close_) {
		err = "PawnIOLib.dll is missing required exports (pawnio_open/load/execute/close).";
		Shutdown();
		return false;
	}

	void* execHandle = nullptr;
	long hr = pawnio_open_(&execHandle);
	if (hr < 0 || !execHandle) {
		if ((unsigned long)hr == 0x80070005UL) {
			err = "pawnio_open failed hr=0x80070005 (E_ACCESSDENIED). Run this app from an elevated (Administrator) terminal.";
		} else {
			err = "pawnio_open failed hr=" + HrStr(hr);
		}
		Shutdown();
		return false;
	}
	handle_ = execHandle;

	std::wstring modPath = FindModulePath(err);
	if (modPath.empty()) {
		Shutdown();
		return false;
	}

	std::vector<uint8_t> blob;
	if (!ReadFileBytes(modPath, blob, err)) {
		Shutdown();
		return false;
	}

	hr = pawnio_load_(handle_, (const unsigned char*)blob.data(), blob.size());
	if (hr < 0) {
		err = "pawnio_load(LpcACPIEC.bin) failed hr=" + HrStr(hr);
		Shutdown();
		return false;
	}

	return true;
}

void PawnIoEc::Shutdown() {
	if (handle_ && pawnio_close_) {
		pawnio_close_(handle_);
	}
	handle_ = nullptr;
	pawnio_open_ = nullptr;
	pawnio_load_ = nullptr;
	pawnio_execute_ = nullptr;
	pawnio_close_ = nullptr;

	if (dll_) {
		FreeLibrary((HMODULE)dll_);
	}
	dll_ = nullptr;
}

bool PawnIoEc::PioInb(uint16_t port, uint8_t& outVal, std::string& err) {
	if (!handle_ || !pawnio_execute_) {
		err = "PawnIO not initialized.";
		return false;
	}

	const unsigned long long in[1] = { (unsigned long long)port };
	unsigned long long out[1] = { 0 };
	size_t ret = 0;
	long hr = pawnio_execute_(handle_, "ioctl_pio_read", in, 1, out, 1, &ret);
	if (hr < 0) {
		err = "pawnio_execute(ioctl_pio_read) failed hr=" + HrStr(hr);
		return false;
	}
	if (ret < 1) {
		err = "pawnio_execute(ioctl_pio_read) returned no data.";
		return false;
	}
	outVal = (uint8_t)(out[0] & 0xFF);
	return true;
}

bool PawnIoEc::PioOutb(uint16_t port, uint8_t val, std::string& err) {
	if (!handle_ || !pawnio_execute_) {
		err = "PawnIO not initialized.";
		return false;
	}

	const unsigned long long in[2] = { (unsigned long long)port, (unsigned long long)val };
	size_t ret = 0;
	long hr = pawnio_execute_(handle_, "ioctl_pio_write", in, 2, nullptr, 0, &ret);
	if (hr < 0) {
		err = "pawnio_execute(ioctl_pio_write) failed hr=" + HrStr(hr);
		return false;
	}
	return true;
}

bool PawnIoEc::WaitForEcStatus(uint8_t mask, bool wantSet, uint32_t timeoutMs, std::string& err) {
	const auto start = std::chrono::steady_clock::now();
	while (true) {
		uint8_t st = 0;
		if (!PioInb(cfg::EC_PORT_CMD, st, err)) return false;
		const bool isSet = (st & mask) == mask;
		if (isSet == wantSet) return true;

		if (std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - start)
				.count() >= timeoutMs) {
			std::ostringstream oss;
			oss << "EC status wait timeout (mask=0x" << std::hex << (int)mask << ").";
			err = oss.str();
			return false;
		}
		Sleep(0);
	}
}

bool PawnIoEc::WaitRead(std::string& err) { return WaitForEcStatus(cfg::EC_STATUS_OBF, true, 10, err); }
bool PawnIoEc::WaitWrite(std::string& err) { return WaitForEcStatus(cfg::EC_STATUS_IBF, false, 10, err); }

bool PawnIoEc::ReadEcByte(uint8_t reg, uint8_t& outVal, std::string& err) {
	if (!WaitWrite(err)) return false;
	if (!PioOutb(cfg::EC_PORT_CMD, cfg::EC_CMD_READ, err)) return false;
	if (!WaitWrite(err)) return false;
	if (!PioOutb(cfg::EC_PORT_DATA, reg, err)) return false;
	if (!WaitRead(err)) return false;
	return PioInb(cfg::EC_PORT_DATA, outVal, err);
}

bool PawnIoEc::WriteEcByte(uint8_t reg, uint8_t val, std::string& err) {
	if (!WaitWrite(err)) return false;
	if (!PioOutb(cfg::EC_PORT_CMD, cfg::EC_CMD_WRITE, err)) return false;
	if (!WaitWrite(err)) return false;
	if (!PioOutb(cfg::EC_PORT_DATA, reg, err)) return false;
	if (!WaitWrite(err)) return false;
	return PioOutb(cfg::EC_PORT_DATA, val, err);
}

bool PawnIoEc::GetFullBlast(bool& enabled, std::string& err) {
	uint8_t v = 0;
	if (!ReadEcByte(cfg::FULLBLAST_REG, v, err)) return false;
	enabled = (v & cfg::FULLBLAST_BIT) == cfg::FULLBLAST_BIT;
	return true;
}

bool PawnIoEc::ToggleFullBlast(bool& nowEnabled, std::string& err) {
	uint8_t v = 0;
	if (!ReadEcByte(cfg::FULLBLAST_REG, v, err)) return false;
	const bool wasEnabled = (v & cfg::FULLBLAST_BIT) == cfg::FULLBLAST_BIT;
	uint8_t nv = wasEnabled ? (uint8_t)(v & ~cfg::FULLBLAST_BIT) : (uint8_t)(v | cfg::FULLBLAST_BIT);
	if (!WriteEcByte(cfg::FULLBLAST_REG, nv, err)) return false;
	nowEnabled = !wasEnabled;
	return true;
}

bool PawnIoEc::ReadPort66(uint8_t& outVal, std::string& err) {
	return PioInb(cfg::EC_PORT_CMD, outVal, err);
}
