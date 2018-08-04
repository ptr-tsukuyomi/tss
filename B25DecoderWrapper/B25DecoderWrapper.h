// B25DecoderWrapper.h

#pragma once
//#pragma comment(lib,"B25Decoder.lib")

#include <msclr/marshal.h>

using namespace System;
using namespace msclr::interop;
using namespace System::Runtime::InteropServices;

#include "IB25Decoder.h"

namespace B25DecoderWrapper {

	public ref class B25Decoder
	{
		IB25Decoder2* d = NULL;
		BYTE* buffer = NULL;
		DWORD remain = 0;
		static System::Object^ sync_obj = gcnew System::Object();

		HMODULE mod = NULL;

	public:
		B25Decoder()
		{
			IB25Decoder2 * (*CreateB25Decoder2)(void) = NULL;

			mod = LoadLibrary(TEXT("B25Decoder.dll"));
			if (mod == NULL) return;

			CreateB25Decoder2 = (IB25Decoder2* (*)(void))GetProcAddress(mod, "CreateB25Decoder2");
			if (CreateB25Decoder2 == NULL) FreeLibrary(mod);
			else d = CreateB25Decoder2();
		}
		const System::Boolean Initialize()
		{
			if (d == NULL) return false;
			return d->Initialize() != FALSE;
		}
		void Release(void)
		{
			if (d != NULL)
			{
				d->Release();
				d = NULL;
			}
			if (mod != NULL)
			{
				FreeLibrary(mod);
				mod = NULL;
			}
			buffer = NULL;
			remain = 0;
		}

		const System::UInt32 Decode(array<System::Byte>^ src, const System::UInt32 srcsize, array<System::Byte>^ dst, System::UInt32 dstsize, [Out] System::UInt32% written)
		{
			if (d == NULL) return 0;
			if (remain == 0)
			{
				BYTE* ptr;
				DWORD ds;
				pin_ptr<System::Byte> p_src = &src[0];
				try
				{
					System::Threading::Monitor::Enter(sync_obj);
					if (d->Decode(p_src, srcsize, &ptr, &ds) == FALSE)
					{
						throw gcnew System::Exception("B25Decoder Decode failed.");
					}
				}
				finally
				{
					System::Threading::Monitor::Exit(sync_obj);
				}

				if (ds == 0)
				{
					written = 0;
					return 0;
				}
				buffer = ptr;
				remain = ds;
			}
			written = min(dstsize, remain);
			Marshal::Copy(IntPtr(buffer), dst, 0, written);
			buffer += written;
			remain -= written;
			return remain;
		}
		const System::Boolean Reset(void)
		{
			if (d == NULL) return false;
			return d->Reset() != FALSE;
		}

		enum class DescrambleState	// GetDescramblerState() リターンコード
		{
			DS_NO_ERROR = 0x00000000UL,		// エラーなし正常
			DS_BCAS_ERROR = 0x00000001UL,		// B-CASカードエラー
			DS_NOT_CONTRACTED = 0x00000002UL		// 視聴未契約
		};

		void DiscardNullPacket(const System::Boolean bEnable)
		{
			if (d == NULL) return;
			d->DiscardNullPacket(bEnable);
		}
		void DiscardNullPacket()
		{
			DiscardNullPacket(true);
		}
		void DiscardScramblePacket(const System::Boolean bEnable)
		{
			if (d == NULL) return;
			d->DiscardScramblePacket(bEnable);
		}
		void DiscardScramblePacket()
		{
			DiscardScramblePacket(true);
		}
		void EnableEmmProcess(const System::Boolean bEnable)
		{
			if (d == NULL) return;
			d->EnableEmmProcess(bEnable);
		}
		void EnableEmmProcess()
		{
			EnableEmmProcess(true);
		}

		const DescrambleState GetDescramblingState(const System::Int16 wProgramID)
		{
			if (d == NULL) return DescrambleState::DS_BCAS_ERROR;
			switch(d->GetDescramblingState(wProgramID))
			{
			case (DWORD)(DescrambleState::DS_BCAS_ERROR) :
				return DescrambleState::DS_BCAS_ERROR;
			case (DWORD)(DescrambleState::DS_NOT_CONTRACTED) :
				return DescrambleState::DS_NOT_CONTRACTED;
			}
			return DescrambleState::DS_NO_ERROR;
		}

		void ResetStatistics(void)
		{
			if (d == NULL) return;
			d->ResetStatistics();
		}

		const System::Int32 GetPacketStride(void)
		{
			if (d == NULL) return -1;
			return d->GetPacketStride();
		}
		const System::Int32 GetInputPacketNum(const System::Int16 wPID)
		{
			if (d == NULL) return -1;
			return d->GetInputPacketNum(wPID);
		}
		const System::Int32 GetInputPacketNum()
		{
			return GetInputPacketNum(TS_INVALID_PID);
		}
		const System::Int32 GetOutputPacketNum(const System::Int16 wPID)
		{
			if (d == NULL) return -1;
			return d->GetOutputPacketNum(wPID);
		}
		const System::Int32 GetOutputPacketNum()
		{
			return GetOutputPacketNum(TS_INVALID_PID);
		}
		const System::Int32 GetSyncErrNum(void)
		{
			if (d == NULL) return -1;
			return d->GetSyncErrNum();
		}
		const System::Int32 GetFormatErrNum(void)
		{
			if (d == NULL) return -1;
			return d->GetFormatErrNum();
		}
		const System::Int32 GetTransportErrNum(void)
		{
			if (d == NULL) return -1;
			return d->GetTransportErrNum();
		}
		const System::Int32 GetContinuityErrNum(const System::Int16 wPID)
		{
			if (d == NULL) return -1;
			return d->GetContinuityErrNum(wPID);
		}
		const System::Int32 GetContinuityErrNum()
		{
			return GetContinuityErrNum(TS_INVALID_PID);
		}
		const System::Int32 GetScramblePacketNum(const System::Int16 wPID)
		{
			if (d == NULL) return -1;
			return d->GetScramblePacketNum(wPID);
		}
		const System::Int32 GetScramblePacketNum()
		{
			return GetScramblePacketNum(TS_INVALID_PID);
		}
		const System::Int32 GetEcmProcessNum(void)
		{
			if (d == NULL) return -1;
			return d->GetEcmProcessNum();
		}
		const System::Int32 GetEmmProcessNum(void)
		{
			if (d == NULL) return -1;
			return d->GetEmmProcessNum();
		}
	};
}
