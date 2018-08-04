// BonDriverWrapper.h

#pragma once
#include "IBonDriver2.h"
#include <msclr/marshal.h>

using namespace System;
using namespace msclr::interop;
using namespace System::Runtime::InteropServices;

namespace BonDriverWrapper {

	public ref class IBonDriver2IsNotImplementedException : System::Exception
	{
	};

	public ref class CannotLoadDLLException : System::Exception
	{
	};

	public ref class CannotFindProcAddress : System::Exception
	{
	};

	public ref class BonDriver
	{
		HMODULE mod = NULL;
		IBonDriver2* d = NULL;
		IBonDriver* d1 = NULL;
		BYTE* buffer = NULL;
		DWORD remain = 0;
		Object^ syncobj = gcnew Object();
	public:
		BonDriver(System::String^ path)
		{
			marshal_context mc;

			mod = LoadLibraryW(mc.marshal_as<const wchar_t*>(path));
			if (mod == NULL) throw gcnew CannotLoadDLLException;

			IBonDriver* (*func)() = (IBonDriver* (*)())GetProcAddress(mod, "CreateBonDriver");
			if (!func)
			{
				FreeLibrary(mod);
				throw gcnew CannotFindProcAddress;
			}

			d1 = func();
			try
			{
				d = dynamic_cast<IBonDriver2*>(d1);
			} catch (std::bad_cast e) {
				FreeLibrary(mod);
				throw gcnew IBonDriver2IsNotImplementedException;
			}
		}

		const System::Boolean OpenTuner(void)
		{
			return d->OpenTuner() != FALSE;
		}

		void CloseTuner(void)
		{
			d->CloseTuner();
		}

		const System::Single GetSignalLevel(void)
		{
			try
			{
				System::Threading::Monitor::Enter(syncobj);
				return d->GetSignalLevel();
			} 
			finally
			{
				System::Threading::Monitor::Exit(syncobj);
			}
		}

		const System::UInt32 WaitTsStream([Optional] [DefaultParameterValue(0)] const System::UInt32 dwTimeOut)
		{
			return d->WaitTsStream(dwTimeOut);
		}
		const System::UInt32 GetReadyCount(void)
		{
			return d->GetReadyCount();
		}

		const System::UInt32 GetTsStream(array<System::Byte>^ pDst, System::Int32 offset, System::UInt32 size)
		{
			try
			{
				System::Threading::Monitor::Enter(syncobj);
				if (remain == 0)
				{
					DWORD ps, pd;
					BYTE* ptr;
					System::Boolean result = d->GetTsStream(&ptr, &ps, &pd) != FALSE;
					if (!result)
					{
						//throw gcnew System::Exception("GetTsStream failed.");
						buffer = nullptr;
						return 0;
					} else {
						buffer = ptr;
						remain = ps;
					}
				}
				DWORD written = min(size, remain);
				Marshal::Copy(IntPtr(buffer), pDst, offset, written);
				buffer += written;
				remain -= written;
				return written;
			}
			finally
			{
				System::Threading::Monitor::Exit(syncobj);
			}
		}

		void PurgeTsStream(void)
		{
			try
			{
				System::Threading::Monitor::Enter(syncobj);
				return d->PurgeTsStream();
			} finally
			{
				System::Threading::Monitor::Exit(syncobj);
			}
		}
			
		System::String^ GetTunerName(void)
		{
			return marshal_as<System::String^>(d->GetTunerName());
		}

		const System::Boolean IsTunerOpening(void)
		{
			return d->IsTunerOpening() != FALSE;
		}

		System::String^ EnumTuningSpace(System::UInt32 dwSpace)
		{
			const wchar_t* str = d->EnumTuningSpace(dwSpace);
			if (str != NULL) return marshal_as<System::String^>(str);
			else return nullptr;
		}

		System::String^ EnumChannelName(const System::UInt32 dwSpace, const System::UInt32 dwChannel)
		{
			const wchar_t* str = d->EnumChannelName(dwSpace, dwChannel);
			if (str != NULL) return marshal_as<System::String^>(str);
			else return nullptr;
		}

		const System::Boolean SetChannel(const System::UInt32 dwSpace, const System::UInt32 dwChannel)
		{
			try
			{
				System::Threading::Monitor::Enter(syncobj);
				return d->SetChannel(dwSpace, dwChannel) != FALSE;
			} finally
			{
				System::Threading::Monitor::Exit(syncobj);
			}			
		}

		const System::UInt32 GetCurSpace(void)
		{
			return d->GetCurSpace();
		}
		const System::UInt32 GetCurChannel(void)
		{
			return d->GetCurChannel();
		}

		void Release(void)
		{
			d->Release();
			FreeLibrary(mod);
			mod = NULL;
			d = NULL;
		}
	};
}
