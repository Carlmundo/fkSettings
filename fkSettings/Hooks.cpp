//https://github.com/nizikawa-worms/wkJellyWorm/blob/master/src/Hooks.cpp
//Modified to support Windows XP

#include "Hooks.h"

std::map<std::string, DWORD> Hooks::hookNameToAddr;
std::map<DWORD, std::string> Hooks::hookAddrToName;

std::map<std::string, DWORD> Hooks::scanNameToAddr;
std::map<DWORD, std::string> Hooks::scanAddrToName;

bool Hooks::scanFoundNew = false;

#include <stdexcept>
#include <sstream>
#include <fstream>
#include "include/MinHook.h"
#include <vector>

static int HexNibble(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';

	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;

	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return -1;
}

static bool ParsePattern(const char* pattern, std::vector<int>& bytes)
{
	bytes.clear();

	if (!pattern)
		return false;

	const char* p = pattern;

	while (*p)
	{
		while (*p == ' ')
			++p;

		if (!*p)
			break;

		if (*p == '?')
		{
			bytes.push_back(-1);
			++p;
			if (*p == '?')
				++p;
			continue;
		}

		int high = HexNibble(p[0]);
		if (high < 0 || !p[1])
			return false;

		int low = HexNibble(p[1]);
		if (low < 0)
			return false;

		bytes.push_back((high << 4) | low);
		p += 2;
	}

	return !bytes.empty();
}

static DWORD FindPattern(const char* pattern, HMODULE module)
{
	std::vector<int> patternBytes;
	if (!ParsePattern(pattern, patternBytes))
		return 0;

	BYTE* image = reinterpret_cast<BYTE*>(module ? module : GetModuleHandleA(NULL));
	if (!image)
		return 0;

	IMAGE_DOS_HEADER* dos =	reinterpret_cast<IMAGE_DOS_HEADER*>(image);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
		return 0;

	IMAGE_NT_HEADERS* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE)
		return 0;

	IMAGE_SECTION_HEADER* section =	IMAGE_FIRST_SECTION(nt);

	for (WORD s = 0; s < nt->FileHeader.NumberOfSections; ++s, ++section)
	{
		// We only need executable code sections.
		if (!(section->Characteristics & IMAGE_SCN_MEM_EXECUTE)) {
			continue;
		}

		BYTE* start = image + section->VirtualAddress;
		size_t sectionSize = section->Misc.VirtualSize;

		if (sectionSize < patternBytes.size())
			continue;

		for (size_t offset = 0;	offset <= sectionSize - patternBytes.size(); ++offset)
		{
			bool match = true;

			for (size_t i = 0; i < patternBytes.size();	++i)
			{
				if (patternBytes[i] != -1 && start[offset + i] != static_cast<BYTE>(patternBytes[i]))
				{
					match = false;
					break;
				}
			}

			if (match)
			{
				return reinterpret_cast<DWORD>(start + offset);
			}
		}
	}

	return 0;
}

void Hooks::minhook(std::string name, DWORD pTarget, DWORD* pDetour, DWORD* ppOriginal) {
	if (!pTarget)
		throw std::runtime_error("Hook adress is null: " + name);
	if (hookNameToAddr.find(name) != hookNameToAddr.end())
		throw std::runtime_error("Hook name reused: " + name);
	if (hookAddrToName.find(pTarget) != hookAddrToName.end()) {
		std::stringstream ss;
		ss << "The specified address is already hooked: " << name << "(0x" << std::hex << pTarget << "), " << hookAddrToName[pTarget];
		throw std::runtime_error(ss.str());
	}
	auto wut = MH_CreateHook((LPVOID)pTarget, (LPVOID)pDetour, (LPVOID*)ppOriginal);
	if (wut != MH_OK)
		throw std::runtime_error("Failed to create hook: " + name);
	if (MH_EnableHook((LPVOID)pTarget) != MH_OK)
		throw std::runtime_error("Failed to enable hook: " + name);

	hookAddrToName[pTarget] = name;
	hookNameToAddr[name] = pTarget;
	printf("minhook: %s 0x%X -> 0x%X\n", name.c_str(), pTarget, pDetour);
}

//Worms development tools by StepS
BOOL Hooks::PatchMemData(PVOID pAddr, size_t buf_len, PVOID pNewData, size_t data_len) {
	if (!buf_len || !data_len || !pNewData || !pAddr || buf_len < data_len) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}
	DWORD dwLastProtection;
	if (!VirtualProtect((void*)pAddr, data_len, PAGE_EXECUTE_READWRITE, &dwLastProtection))
		return 0;
	memcpy_s(pAddr, buf_len, pNewData, data_len);
	return VirtualProtect((void*)pAddr, data_len, dwLastProtection, &dwLastProtection);
}

#define IJ_JUMP 0 //Insert a jump (0xE9) with InsertJump
#define IJ_CALL 1 //Insert a call (0xE8) with InsertJump
#define IJ_FARJUMP 2 //Insert a farjump (0xEA) with InsertJump
#define IJ_FARCALL 3 //Insert a farcall (0x9A) with InsertJump
#define IJ_PUSHRET 4 //Insert a pushret with InsertJump
BOOL  Hooks::InsertJump(PVOID pDest, size_t dwPatchSize, PVOID pCallee, DWORD dwJumpType) {
	if (dwPatchSize >= 5 && pDest) {
		DWORD OpSize = 5, OpCode = 0xE9;
		PBYTE dest = (PBYTE)pDest;
		switch (dwJumpType) {
		case IJ_PUSHRET:
			OpSize = 6;
			OpCode = 0x68;
			break;
		case IJ_FARJUMP:
			OpSize = 7;
			OpCode = 0xEA;
			break;
		case IJ_FARCALL:
			OpSize = 7;
			OpCode = 0x9A;
			break;
		case IJ_CALL:
			OpSize = 5;
			OpCode = 0xE8;
			break;
		default:
			OpSize = 5;
			OpCode = 0xE9;
			break;
		}
		if (dwPatchSize < OpSize)
			return 0;
		PatchMemVal(dest, (BYTE)OpCode);
		switch (OpSize) {
		case 7:
			PatchMemVal(dest + 1, pCallee);
			WORD w_cseg;
			__asm mov[w_cseg], cs;
			PatchMemVal(dest + 5, w_cseg);
			break;
		case 6:
			PatchMemVal(dest + 1, pCallee);
			PatchMemVal<BYTE>(dest + 5, 0xC3);
			break;
		default:
			PatchMemVal(dest + 1, (ULONG_PTR)pCallee - (ULONG_PTR)pDest - 5);
			break;
		}
		for (size_t i = OpSize; i < dwPatchSize; i++)
			PatchMemVal<BYTE>(dest + i, 0x90);
	}
	return 0;
}

void Hooks::hookAsm(DWORD startAddr, DWORD hookAddr) {
	printf("hookAsm: 0x%X -> 0x%X\n", startAddr, hookAddr);
	InsertJump((PVOID)startAddr, 6, (PVOID)hookAddr, IJ_PUSHRET);
}

void Hooks::patchAsm(DWORD addr, unsigned char* op, size_t opsize) {
	printf("patchAsm: 0x%X : ", addr);
	for (size_t i = 0; i < opsize; i++) {
		printf("%02X ", *(unsigned char*)(addr + i));
	}
	printf(" -> ");
	for (size_t i = 0; i < opsize; i++) {
		printf("%02X ", op[i]);
	}
	printf("\n");
	PatchMemData((PVOID)addr, opsize, (PVOID)op, opsize);
}

void Hooks::hookVtable(const char* classname, int offset, DWORD addr, DWORD hookAddr, DWORD* original) {
	printf("hookVtable: %s::0x%X 0x%X -> 0x%X\n", classname, offset, *(DWORD*)addr, addr);
	*original = *(DWORD*)addr;
	int dest = hookAddr;
	PatchMemData((PVOID)addr, sizeof(dest), &dest, sizeof(dest));
}
/*
DWORD Hooks::scanPattern(const char* name, const char* pattern, const char* mask, DWORD expected) {
	uintptr_t ret = 0;
	//rebase offsets in cached file
	if (!hl::PatternScanner::WAregion.base) {
		hl::FindPatternMask(pattern, mask);
		for (auto& it : scanNameToAddr) {
			it.second = it.second + (hl::PatternScanner::WAregion.base - 0x401000);
			scanAddrToName[it.second] = it.first;
		}
	}
	auto it = scanNameToAddr.find(name);
	if (it == scanNameToAddr.end()) {
		ret = hl::FindPatternMask(pattern, mask);
		printf("scanPattern: %s = 0x%X\n", name, ret);
		if (!ret) {
			std::string msg = "scanPattern: failed to find memory pattern: ";
			msg += name;
			throw std::runtime_error(msg);
		}
		scanNameToAddr[name] = ret;
		scanAddrToName[ret] = name;
		scanFoundNew = true;
	}
	else {
		ret = it->second;//scanNameToAddr[name];
		printf("scanCache: %s = 0x%X\n", name, ret);
	}

	return ret;
}
*/

DWORD Hooks::scanPattern2(const char* name, const char* pattern, DWORD expected, HMODULE module) {
	DWORD address = FindPattern(pattern, module);
	if (!address) {
		char buffer[512];
		sprintf_s(buffer, "fkSettings: pattern not found: %s\n", name ? name : "(unnamed)");
		OutputDebugStringA(buffer);
		return 0;
	}
	scanNameToAddr[name ? name : ""] = address;
	scanAddrToName[address] = name ? name : "";
	return address;
}
