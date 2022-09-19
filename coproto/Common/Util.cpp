#include "Util.h"
#ifdef _MSC_VER
#include <windows.h>
#include <processthreadsapi.h>

#include "DbgHelp.h"
#include <WinBase.h>
#pragma comment(lib, "Dbghelp.lib")
#endif
#include <sstream>

namespace coproto
{

	void setThreadName(std::string s)
	{
#ifdef _MSC_VER
		std::wstring ss(s.begin(), s.end());
		HRESULT r;
		r = SetThreadDescription(
			GetCurrentThread(),
			ss.c_str()
		);
#endif

	}

	auto gPrintStart = std::chrono::system_clock::now();
	std::mutex gPrntMtx;
	std::stringstream gLog;
	std::string getLog()
	{
		std::lock_guard<std::mutex> lock(gPrntMtx);
		return gLog.str();
	}


	void clearLog()
	{
		std::lock_guard<std::mutex> lock(gPrntMtx);
		gLog = std::stringstream{}; 
		gPrintStart = std::chrono::system_clock::now(); 
	}

	void log(const std::string& s)
	{
		auto now = std::chrono::system_clock::now();
		std::lock_guard<std::mutex> lock(gPrntMtx);
		gLog << s << " " << std::chrono::duration_cast<std::chrono::milliseconds>(now - gPrintStart).count() << " ms\n";
	}



	std::string stackTrace()
	{
#ifdef _MSC_VER

		typedef USHORT(WINAPI* CaptureStackBackTraceType)(__in ULONG, __in ULONG, __out PVOID*, __out_opt PULONG);
		CaptureStackBackTraceType func = (CaptureStackBackTraceType)(GetProcAddress(LoadLibrary("kernel32.dll"), "RtlCaptureStackBackTrace"));

		if (func == NULL)
			return "stack trace unavailable"; // WOE 29.SEP.2010

		// Quote from Microsoft Documentation:
		// ## Windows Server 2003 and Windows XP:  
		// ## The sum of the FramesToSkip and FramesToCapture parameters must be less than 63.
		const int kMaxCallers = 62;

		void* callers_stack[kMaxCallers];
		unsigned short frames;
		SYMBOL_INFO* symbol;
		HANDLE         process;
		process = GetCurrentProcess();
		SymInitialize(process, NULL, TRUE);
		frames = (func)(0, kMaxCallers, callers_stack, NULL);
		symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
		symbol->MaxNameLen = 255;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

		//const unsigned short  MAX_CALLERS_SHOWN = 6;
		//frames = frames < MAX_CALLERS_SHOWN ? frames : MAX_CALLERS_SHOWN;
		std::stringstream out;

		for (unsigned int i = 0; i < frames; i++)
		{
			SymFromAddr(process, (DWORD64)(callers_stack[i]), 0, symbol);
			out << "*** " << i << ": " << callers_stack[i] << " " << symbol->Name << " - 0x" << symbol->Address << std::endl;
		}

		free(symbol);

		return out.str();
#else
		return "stack trace not implimented on unix";
#endif
	}
}