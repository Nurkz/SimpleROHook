// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include "Hook.h"

#include "tinyconsole.h"

#include "ProxyIDirectDraw.h"
#include "ProxyIDirectInput.h"

#include "Core/RoCodeBind.h"

#include <ctime>

BOOL InstallProxyFunction(LPCTSTR dllname,LPCSTR exportname,VOID *ProxyFunction,LPVOID *pOriginalFunction)
{
	BOOL result = FALSE;
	std::stringstream fullpath;

	TCHAR systemdir[MAX_PATH];
	HMODULE hDll;
	::GetSystemDirectory( systemdir, MAX_PATH);

	fullpath << systemdir << "\\" << dllname;
	hDll = ::LoadLibrary(fullpath.str().c_str() );


	DEBUG_LOGGING_DETAIL(("trying to hook %s:%s\n", dllname, exportname));

	if (!hDll)
	{
		DEBUG_LOGGING_DETAIL(("could not get handle"));
		return result;
	}

	BYTE *p = (BYTE*)::GetProcAddress(hDll, exportname);

	DEBUG_LOGGING_DETAIL(("%08X :%s(%08X)\n", hDll, exportname, p));
	if (p)
	{
		if (p[0] == 0x8b && p[1] == 0xff &&
			((p[-5] == 0x90 && p[-4] == 0x90 && p[-3] == 0x90 && p[-2] == 0x90 && p[-1] == 0x90) ||
			 (p[-5] == 0xcc && p[-4] == 0xcc && p[-3] == 0xcc && p[-2] == 0xcc && p[-1] == 0xcc))
			)
		{
			// find hotpatch structure.
			//
			// 9090909090 nop  x 5
			// 8bff       mov  edi,edi
			//       or
			// cccccccccc int 3 x 5
			// 8bff       mov edi,edi
			DWORD flOldProtect, flDontCare;
			if (::VirtualProtect((LPVOID)&p[-5], 7, PAGE_READWRITE, &flOldProtect))
			{
				p[-5] = 0xe9;              // jmp
				p[0] = 0xeb; p[1] = 0xf9;// jmp short [pc-7]
				DEBUG_LOGGING_DETAIL(("hook type a\n"));

				*pOriginalFunction = (void*)&p[2];
				*((DWORD*)&p[-4]) = (DWORD)ProxyFunction - (DWORD)&p[-5] - 5;

				::VirtualProtect((LPVOID)&p[-5], 7, flOldProtect, &flDontCare);
				result = TRUE;
			}
		}
		else
		if (p[-5] == 0xe9 &&
			p[0] == 0xeb && p[1] == 0xf9)
		{
			// find hotpached function.
			// jmp **** 
			// jmp short [pc -7]
			DWORD flOldProtect, flDontCare;
			if (::VirtualProtect((LPVOID)&p[-5], 7, PAGE_READWRITE, &flOldProtect))
			{
				*pOriginalFunction = (LPVOID)(*((DWORD*)&p[-4]) + (DWORD)&p[-5] + 5);
				*((DWORD*)&p[-4]) = (DWORD)ProxyFunction - (DWORD)&p[-5] - 5;
				DEBUG_LOGGING_DETAIL(("hook type b\n"));

				::VirtualProtect((LPVOID)&p[-5], 7, flOldProtect, &flDontCare);
				result = TRUE;
			}
		}
		else
		if (p[0] == 0xe9 &&
			((p[-5] == 0x90 && p[-4] == 0x90 && p[-3] == 0x90 && p[-2] == 0x90 && p[-1] == 0x90) ||
			(p[-5] == 0xcc && p[-4] == 0xcc && p[-3] == 0xcc && p[-2] == 0xcc && p[-1] == 0xcc)))
		{
			// find irregular hook code. case by iro
			//
			// 9090909090 nop  x 5
			// e9******** jmp  im4byte 
			//       or
			// cccccccccc int 3 x 5
			// e9******** jmp  im4byte
			DWORD flOldProtect, flDontCare;
			if (::VirtualProtect((LPVOID)&p[0], 5, PAGE_READWRITE, &flOldProtect))
			{
				*pOriginalFunction = (LPVOID)(*((DWORD*)&p[1]) + (DWORD)&p[0] + 5);

				*((DWORD*)&p[1]) = (DWORD)ProxyFunction - (DWORD)&p[0] - 5;

				DEBUG_LOGGING_DETAIL(("hook type c\n"));

				::VirtualProtect((LPVOID)&p[0], 5, flOldProtect, &flDontCare);
				result = TRUE;
			}
		}
	}

	::FreeLibrary(hDll);

	return result;
}

void *pResumeAIL_open_digital_driverFunction;
void __declspec(naked) ProxyAIL_open_digital_driver(void)
{
	__asm mov   eax,[esp+0x04] // eax = soundrate
	__asm add   [esp+0x04],eax // soundrate + soundrate ( soundrate x 2 )
	__asm sub   esp,0x010
	__asm jmp   pResumeAIL_open_digital_driverFunction
}

BOOL RagexeSoundRateFixer(void)
{
	DEBUG_LOGGING_NORMAL(("Performing sound rate fix"));
	BOOL result = FALSE;
	std::stringstream fullpath;

	TCHAR currentdir[MAX_PATH];
	HINSTANCE hDll;
	::GetCurrentDirectoryA( MAX_PATH,currentdir );

	fullpath << currentdir << "\\Mss32.dll";
	hDll = ::LoadLibrary(fullpath.str().c_str() );

	if( !hDll ){
		DEBUG_LOGGING_NORMAL(("sound rate fix: could not open mss32.dll"));
		return result;
	}

	BYTE *p = (BYTE*)::GetProcAddress( hDll, "_AIL_open_digital_driver@16");

	if( p )
	{
		if( p[ 0] == 0x83 && p[ 1] == 0xec && p[ 2] == 0x10 )
		{
			// find hotpatch structure.
			DWORD flOldProtect, flDontCare;
			if ( ::VirtualProtect(   (LPVOID)&p[-5], 7 , PAGE_READWRITE, &flOldProtect) )
			{
				p[-5] = 0xe9;              // jmp
				p[ 0] = 0xeb; p[ 1] = 0xf9;// jmp short [pc-7]
				p[ 2] = 0x90; // nop
				pResumeAIL_open_digital_driverFunction = &p[3];
				*((DWORD*)&p[-4])  = (DWORD)ProxyAIL_open_digital_driver - (DWORD)&p[-5] -5;

				::VirtualProtect( (LPVOID)&p[-5], 7 , flOldProtect, &flDontCare);
				result = TRUE;
			}
			else {
				DEBUG_LOGGING_NORMAL(("sound rate fix: could not change page permissions"));
			}
		}
	}
	else {
		DEBUG_LOGGING_NORMAL(("sound rate fix: no symbol _AIL_open_digital_driver@16"));
	}
	::FreeLibrary(hDll);

	if (!result) {
		DEBUG_LOGGING_NORMAL(("sound rate fix: failed to apply"));
	}

	return result;
}


typedef HRESULT (WINAPI *tDirectDrawCreateEx)( GUID FAR *lpGUID, LPVOID *lplpDD, REFIID iid, IUnknown FAR *pUnkOuter );
typedef HRESULT (WINAPI *tDirectInputCreateA)( HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTA *ppDI, LPUNKNOWN punkOuter);

typedef int (WSAAPI *tWS2_32_recv)( SOCKET s, char *buf,int len,int flags );


tDirectDrawCreateEx OrigDirectDrawCreateEx = NULL;
tDirectInputCreateA OrigDirectInputCreateA = NULL;

HMODULE g_ws2_32_dll = NULL;
tWS2_32_recv OrigWS2_32_recv = NULL;

int WSAAPI ProxyWS2_32_recv( SOCKET s, char *buf,int len,int flags )
{
	int result;

	result = OrigWS2_32_recv(s,buf,len,flags);
	if( g_pRoCodeBind )
		g_pRoCodeBind->PacketQueueProc( buf,result );

	return result;
}


HRESULT WINAPI ProxyDirectDrawCreateEx(
	GUID FAR     *lpGuid,
	LPVOID       *lplpDD,
	REFIID        iid,
	IUnknown FAR *pUnkOuter )
{
	DEBUG_LOGGING_MORE_DETAIL(("DirectDrawCreateEx hookfunc\n"));

	HRESULT Result = OrigDirectDrawCreateEx( lpGuid, lplpDD, iid, pUnkOuter );
	if(FAILED(Result))
		return Result;

	CProxyIDirectDraw7 *lpcDD;
	*lplpDD = lpcDD = new CProxyIDirectDraw7((IDirectDraw7*)*lplpDD);
	lpcDD->setThis(lpcDD);

	DEBUG_LOGGING_MORE_DETAIL(("DirectDrawCreateEx Hook hookfunc"));

    return Result;
}

HRESULT WINAPI ProxyDirectInputCreateA(
	HINSTANCE hinst,
	DWORD dwVersion,
	LPDIRECTINPUTA *ppDI,
	LPUNKNOWN punkOuter )
{
	DEBUG_LOGGING_MORE_DETAIL(("DirectInputCreateA hookfunc instance = %08X", hinst));

	HRESULT Result = OrigDirectInputCreateA( hinst, dwVersion, ppDI, punkOuter );

	if(FAILED(Result))
		return Result;
	if(dwVersion == 0x0700){
		*ppDI = new CProxyIDirectInput7((IDirectInput7*)*ppDI);
	}
	DEBUG_LOGGING_MORE_DETAIL(("DirectInputCreateA Hook hookfunc"));

    return Result;
}


BOOL IsRagnarokApp(void)
{
	TCHAR filename[MAX_PATH];

	::GetModuleFileName( ::GetModuleHandle( NULL ), filename, sizeof(filename)/sizeof(TCHAR) );
	::PathStripPath( filename );
	// check the module filename
	if( _tcsicmp( filename, _T("Ragexe.exe") ) == 0
	 || _tcsicmp( filename, _T("Sakexe.exe") ) == 0
	 || _tcsicmp( filename, _T("clragexe.exe") ) == 0
	 || _tcsicmp( filename, _T("RagFree.exe") ) == 0
	 || _tcsicmp( filename, _T("HighPriest.exe") ) == 0
		)
	{
		return TRUE;
	}
	return FALSE;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_ATTACH:
		g_hDLL = hModule;
		if( IsRagnarokApp() )
		{
			TCHAR temppath[MAX_PATH];
			::DisableThreadLibraryCalls( hModule );
			CreateTinyConsole();
			DEBUG_LOGGING_NORMAL(("Tiny Console created."));
			OpenSharedMemory();
			DEBUG_LOGGING_NORMAL(("Shared memory opened."));

			char currentdir[MAX_PATH];
			::GetCurrentDirectoryA(MAX_PATH, currentdir);

			DEBUG_LOGGING_NORMAL(("Working directory: %s", currentdir));

			std::stringstream datetime;
			auto t = std::time(nullptr);
			auto tm = *std::localtime(&t);
			datetime << std::put_time(&tm, "%d-%m-%Y %H:%M:%S") << std::endl;
			DEBUG_LOGGING_NORMAL((datetime.str().c_str()));

#ifdef USE_WS2_32DLLINJECTION
			InstallProxyFunction(
				_T("ws2_32.dll"), "recv",
				ProxyWS2_32_recv, (LPVOID*)&OrigWS2_32_recv);
#endif
			if (g_pSharedData) {
				DEBUG_LOGGING_NORMAL(("Shared data found."));
				::GetCurrentDirectory(MAX_PATH, temppath);
				strcat_s(temppath, "\\BGM\\");

				::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED,
					temppath, strlen(temppath) + 1,
					g_pSharedData->musicfilename, MAX_PATH);
				g_FreeMouseSw = g_pSharedData->freemouse;
				if (g_pSharedData->_44khz_audiomode)
					RagexeSoundRateFixer();

				if (g_pSharedData->chainload)
				{
					DEBUG_LOGGING_NORMAL(("Attempting to chainload"));

					// LoadLibrary fails gracefully, so we just try to load both files
					// if one doesn't exist, ignore it
					if (!GetModuleHandle("dinput.dll"))
					{
						std::stringstream dinput_dll_path;
						dinput_dll_path << currentdir << "\\dinput.dll";
						::LoadLibraryA(dinput_dll_path.str().c_str());
					}
					std::stringstream dinput_asi_path;
					dinput_asi_path << currentdir << "\\dinput.asi";
					::LoadLibraryA(dinput_asi_path.str().c_str());
				}
			}

			InstallProxyFunction(
				_T("ddraw.dll"),  "DirectDrawCreateEx", 
				ProxyDirectDrawCreateEx, (LPVOID*)&OrigDirectDrawCreateEx);
			InstallProxyFunction(
				_T("dinput.dll"), "DirectInputCreateA",
				ProxyDirectInputCreateA, (LPVOID*)&OrigDirectInputCreateA);

		}
		break;
	case DLL_PROCESS_DETACH:
		if( IsRagnarokApp() )
		{
			ReleaseTinyConsole();
			ReleaseSharedMemory();
		}
		break;
	}
	return TRUE;
}

