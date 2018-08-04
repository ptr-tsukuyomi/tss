#include <Windows.h>
#include "IBonDriver.h"
#include "IBonDriver2.h"
#include <msclr/marshal.h>
#include <msclr/marshal_cppstd.h>
#include <string>
#include <memory>

using namespace System;

static const bool enable_logging = false;

namespace TSClient
{
	ref class TSClient
	{
	public:
		static String^ host = nullptr;
		static Int32 cport = -1;
		static Int32 vport = -1;
		static String^ name = nullptr;
		static bool hasPrivilege = false;

		static String^ dllname = nullptr;

		static System::Net::Sockets::TcpClient^ contcon = nullptr;
		static System::Net::Sockets::TcpClient^ vidcon = nullptr;
		static System::IO::StreamReader^ csr = nullptr;
		static System::IO::StreamWriter^ csw = nullptr;
		static System::Net::Sockets::NetworkStream^ vidstrm = nullptr;

		static array<unsigned char>^ buffer = gcnew array<unsigned char>(188 * 1024);

		static System::IO::StreamWriter^ logsw = nullptr;

		static System::Windows::Forms::NotifyIcon^ ni = gcnew System::Windows::Forms::NotifyIcon();

		static bool Connect()
		{
			Log("Connect()");

			contcon = gcnew System::Net::Sockets::TcpClient();
			vidcon = gcnew System::Net::Sockets::TcpClient();

			try
			{
				contcon->Connect(host, cport);
				auto contstrm = contcon->GetStream();
				csr = gcnew System::IO::StreamReader(contstrm);
				csw = gcnew System::IO::StreamWriter(contstrm);
				csw->AutoFlush = true;

				auto GUID = csr->ReadLine();
				auto GUID_byte = System::Text::Encoding::ASCII->GetBytes(GUID);

				vidcon->Connect(host, vport);
				vidstrm = vidcon->GetStream();

				vidstrm->Write(GUID_byte, 0, GUID_byte->Length);
				vidstrm->ReadTimeout = 500;
				
				return true;
			}
			catch(System::Net::Sockets::SocketException^ e)
			{
				if (csr != nullptr)
				{
					contcon->Close();
					csr = nullptr;
					csw = nullptr;
				}
				contcon = nullptr;
				vidcon = nullptr;
				vidstrm = nullptr;
				ShowError(e->ToString(), dllname);
				return false;
			}
		}

		static bool OpenTuner()
		{
			Log("OpenTuner()");

			if (!IsReady()) return false;
			if (!IsConnected()) return false;

			auto process = System::Diagnostics::Process::GetCurrentProcess();
			auto hostname = System::Net::Dns::GetHostName();

			auto opencmd = String::Format("<command><name>OpenTuner</name><args><arg name=\"Name\">{0}</arg><arg name=\"Privilege\">{1}</arg><arg name=\"HostName\">{2}</arg><arg name=\"ProcessName\">{3}</arg><arg name=\"ProcessID\">{4}</arg></args></command>\r\n", name, hasPrivilege ? "True" : "False", hostname, process->ProcessName, process->Id);
			try
			{
				csw->WriteLine(opencmd);
				auto opencmd_result = System::Xml::Linq::XDocument::Parse(ReadToMessageEnd(csr));
				auto opencmd_status = System::UInt32::Parse(opencmd_result->Root->Attribute(System::Xml::Linq::XName::Get("status"))->Value);
				if (opencmd_status == 402)
				{
					ShowError(String::Format("開こうとしたチューナーが存在しませんでした: {0}", name), dllname);
				}
				return opencmd_status == 200;
			}
			catch (System::IO::IOException^ e)
			{
				ShowError(e->ToString(), dllname);
				Finalize_();
				return false;
			}
		}

		static bool SetChannel(UInt32 space, UInt32 channel)
		{
			Log(String::Format("SetChannel({0}, {1})", space, channel));

			if (!IsReady()) return false;
			if (!IsConnected()) return false;
			auto setchcmd = String::Format("<command><name>SetChannel</name><args><arg name=\"Space\">{0}</arg><arg name=\"Channel\">{1}</arg></args></command>\r\n", space, channel);
			csw->WriteLine(setchcmd);
			try
			{
				auto setchcmd_result = System::Xml::Linq::XDocument::Parse(ReadToMessageEnd(csr));
				auto setchcmd_status = System::UInt32::Parse(setchcmd_result->Root->Attribute(System::Xml::Linq::XName::Get("status"))->Value);
				auto setchcmd_return = setchcmd_result->Root->Value == "True";
				if (setchcmd_status == 405)
				{
					ShowError("特権クライアントが存在したため、チャンネルの変更ができませんでした。", dllname);
				}
				return true;
			}
			catch(System::IO::IOException^ e)
			{
				ShowError(e->ToString(), dllname);
				Finalize_();
				return false;
			}
		}

		static float GetSignalLevel()
		{
			Log("GetSignalLevel()");

			if (!IsReady()) throw gcnew System::Exception("IsReady() != True in GetSignalLevel");
			if (!IsConnected()) return -1;
			auto gslcmd = String::Format("<command><name>GetSignalLevel</name></command>\r\n", name);
			try
			{
				csw->WriteLine(gslcmd);
				auto gslcmd_result = System::Xml::Linq::XDocument::Parse(ReadToMessageEnd(csr));
				auto gslcmd_status = System::UInt32::Parse(gslcmd_result->Root->Attribute(System::Xml::Linq::XName::Get("status"))->Value);
				if (gslcmd_status != 200) throw gcnew System::Exception("Response status is NOT 200 in GetSignalLevel");
				auto gslcmd_return = System::Single::Parse(gslcmd_result->Root->Value);
				return gslcmd_return;
			}
			catch (System::IO::IOException^ e)
			{
				ShowError(e->ToString(), dllname);
				Finalize_();
				return -1;
			}
		}

		static String^ EnumTuningSpace(UInt32 space)
		{
			Log(String::Format("EnumTuningSpace({0})", space));

			if (!IsReady()) return nullptr;
			if (!IsConnected()) return nullptr;
			auto etscmd = String::Format("<command><name>EnumTuningSpace</name><args><arg name=\"Space\">{0}</arg></args></command>\r\n", space);
			try
			{
				csw->WriteLine(etscmd);
				auto etscmd_result = System::Xml::Linq::XDocument::Parse(ReadToMessageEnd(csr));
				auto etscmd_status = System::UInt32::Parse(etscmd_result->Root->Attribute(System::Xml::Linq::XName::Get("status"))->Value);
				if (etscmd_status != 200) return nullptr;
				auto etscmd_return = etscmd_result->Root->Value;
				return etscmd_return;
			} catch (System::IO::IOException^ e)
			{
				ShowError(e->ToString(), dllname);
				Finalize_();
				return nullptr;
			}
		}

		static String^ EnumChannelName(UInt32 space, UInt32 channel)
		{
			Log(String::Format("EnumChannelName({0}, {1})", space, channel));

			if (!IsReady()) return nullptr;
			if (!IsConnected()) return nullptr;
			auto ecncmd = String::Format("<command><name>EnumChannelName</name><args><arg name=\"Space\">{0}</arg><arg name=\"Channel\">{1}</arg></args></command>\r\n", space, channel);
			try
			{
				csw->WriteLine(ecncmd);
				auto ecncmd_result = System::Xml::Linq::XDocument::Parse(ReadToMessageEnd(csr));
				auto ecncmd_status = System::UInt32::Parse(ecncmd_result->Root->Attribute(System::Xml::Linq::XName::Get("status"))->Value);
				if (ecncmd_status != 200) return nullptr;
				auto ecncmd_return = ecncmd_result->Root->Value;
				return ecncmd_return;
			} catch (System::IO::IOException^ e)
			{
				ShowError(e->ToString(), dllname);
				Finalize_();
				return nullptr;
			}
		}

		static UInt32 GetCurrentSpace()
		{
			Log(String::Format("GetCurrentSpace()"));

			if (!IsReady()) throw gcnew System::Exception("IsReady() != True in GetSignalLevel");
			if (!IsConnected()) throw gcnew System::Exception("IsConnected() != True in GetSignalLevel");
			auto gslcmd = String::Format("<command><name>GetCurSpace</name></command>\r\n", name);
			try
			{
				csw->WriteLine(gslcmd);
				auto gslcmd_result = System::Xml::Linq::XDocument::Parse(ReadToMessageEnd(csr));
				auto gslcmd_status = System::UInt32::Parse(gslcmd_result->Root->Attribute(System::Xml::Linq::XName::Get("status"))->Value);
				if (gslcmd_status != 200) throw gcnew System::Exception("Response status is NOT 200 in GetCurrentSpace");
				auto gslcmd_return = System::UInt32::Parse(gslcmd_result->Root->Value);
				return gslcmd_return;
			} catch (System::IO::IOException^ e)
			{
				ShowError(e->ToString(), dllname);
				Finalize_();
				return 99;
			}
		}

		static UInt32 GetCurrentChannel()
		{
			Log(String::Format("GetCurrentChannel()"));

			if (!IsReady()) throw gcnew System::Exception("IsReady() != True in GetSignalLevel");
			if (!IsConnected()) throw gcnew System::Exception("IsConnected() != True in GetSignalLevel");
			auto gslcmd = String::Format("<command><name>GetCurChannel</name></command>\r\n", name);
			try
			{
				csw->WriteLine(gslcmd);
				auto gslcmd_result = System::Xml::Linq::XDocument::Parse(ReadToMessageEnd(csr));
				auto gslcmd_status = System::UInt32::Parse(gslcmd_result->Root->Attribute(System::Xml::Linq::XName::Get("status"))->Value);
				if (gslcmd_status != 200) throw gcnew System::Exception("Response status is NOT 200 in GetCurChannel");
				auto gslcmd_return = System::UInt32::Parse(gslcmd_result->Root->Value);
				return gslcmd_return;
			} catch (System::IO::IOException^ e)
			{
				ShowError(e->ToString(), dllname);
				Finalize_();
				return 99;
			}
		}

		static String^ ReadToMessageEnd(System::IO::StreamReader^ sr)
		{
			String^ result = "";

			while (true)
			{
				auto s = sr->ReadLine();
				if (String::IsNullOrEmpty(s)) break;
				result += s + "\r\n";
			}

			return result;
		}

		static void LoadSettings()
		{
			dllname = System::Reflection::Assembly::GetExecutingAssembly()->Location;
			String^ settingfilename = dllname->Substring(0, dllname->Length - 3) + "xml";

			ni->Icon = gcnew System::Drawing::Icon("WindowsForm_817.ico");
			ni->Visible = true;

			if (enable_logging)
			{
				String^ logfilename = dllname->Substring(0, dllname->Length - 3) + DateTime::Now.ToString("yyyy-MM-dd-HH-mm-ss") + ".log";
				logsw = gcnew System::IO::StreamWriter(logfilename);
			}

			auto ssr = gcnew System::IO::StreamReader(settingfilename);
			auto settings = System::Xml::Linq::XDocument::Parse(ssr->ReadToEnd());
			ssr->Close();
			// <settings>
			//   <host>...</host>
			//   <cport>...</cport>
			//   <vport>...</vport>
			//   <name>...</name>
			//   <privilege>True/False</privilege>
			// </settings>
			auto e_host = settings->Root->Element(Xml::Linq::XName::Get("host"));
			auto e_cport = settings->Root->Element(Xml::Linq::XName::Get("cport"));
			auto e_vport = settings->Root->Element(Xml::Linq::XName::Get("vport"));
			auto e_name = settings->Root->Element(Xml::Linq::XName::Get("name"));
			auto e_priv = settings->Root->Element(Xml::Linq::XName::Get("privilege"));

			if (e_host == nullptr || e_cport == nullptr || e_vport == nullptr || e_name == nullptr)
			{
				ShowError("Setting file is invalid.", settingfilename);
				return;
			}

			bool success = true;
			success |= System::Int32::TryParse(e_cport->Value, cport);
			success |= System::Int32::TryParse(e_vport->Value, vport);

			if (!success || !(cport >= 0 && cport < 65536 && vport >= 0 && vport < 65536))
			{
				ShowError("Port number is invalid.", settingfilename);
				cport = -1;
				vport = -1;
				return;
			}

			host = e_host->Value;
			name = e_name->Value;

			if (e_priv != nullptr) hasPrivilege = e_priv->Value == "True";
		}

		static void ShowError(String^ msg, String^ caption)
		{
			ni->BalloonTipIcon = System::Windows::Forms::ToolTipIcon::Error;
			ni->BalloonTipText = msg;
			ni->BalloonTipTitle = caption;
			ni->ShowBalloonTip(50000);
		}

		static bool IsReady()
		{
			return !(host == nullptr || cport == -1 || vport == -1 || name == nullptr);
		}

		static bool IsConnected()
		{
			return vidstrm != nullptr;
		}

		static void Finalize_()
		{
			if (IsConnected())
			{
				vidstrm = nullptr;
				vidcon->Close();
				contcon->Close();
			}

			if (enable_logging)
			{
				logsw->Close();
			}
		}

		static void Release()
		{
			Log(String::Format("Release()"));

			ni->Visible = false;

			Finalize_();
		}

		static void Log(String^ str)
		{
			if (logsw == nullptr) return;

			logsw->WriteLine(String::Format("[{0}] {1}", DateTime::Now.ToString(), str));
			logsw->Flush();
		}
	};
}


class BonDriver_TSClient : public IBonDriver2
{
public:
	BonDriver_TSClient()
	{
		TSClient::TSClient::LoadSettings();
	}
	const BOOL OpenTuner(void)
	{
		if (!TSClient::TSClient::IsReady())
		{
			return FALSE;
		}
		if (!TSClient::TSClient::Connect())
		{
			return FALSE;
		}
		if (!TSClient::TSClient::OpenTuner())
		{
			return FALSE;
		}

		return TRUE;
	}
	void CloseTuner(void)
	{
		if (TSClient::TSClient::IsConnected())
		{
			TSClient::TSClient::contcon->Close();
			TSClient::TSClient::vidcon->Close();
		}
	}

	const BOOL SetChannel(const BYTE bCh)
	{
		return SetChannel(0, bCh);
	}

	const float GetSignalLevel(void)
	{
		return TSClient::TSClient::GetSignalLevel();
	}

	const DWORD WaitTsStream(const DWORD dwTimeOut)
	{
		auto begin = DateTime::Now;

		while(DateTime::Now - begin < TimeSpan(0,0,0,0,(int)dwTimeOut) && GetReadyCount() == 0)
		{
		}
		return GetReadyCount();
	}
	const DWORD GetReadyCount(void)
	{
		TSClient::TSClient::Log(String::Format("GetReadyCount()"));

		if (TSClient::TSClient::vidstrm == nullptr) throw gcnew Exception("bstrm == nullptr in GetReadyCount");
		return TSClient::TSClient::vidcon->Available / 188;
	}


	const BOOL GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain)
	{
		TSClient::TSClient::Log(String::Format("GetTsStream() with *pDst"));

		if (TSClient::TSClient::vidstrm == nullptr) return FALSE;

		static size_t remain = 0;
		static int offset = 0;
		if (remain == 0)
		{
			try
			{
				remain = TSClient::TSClient::vidstrm->Read(TSClient::TSClient::buffer, remain, TSClient::TSClient::buffer->Length - remain);
			}
			catch (IO::IOException^ e)
			{
				remain = 0;
				return FALSE;
			}
			
			offset = 0;
		}

		size_t copy_len = min(*pdwSize, remain);

		System::Runtime::InteropServices::Marshal::Copy(TSClient::TSClient::buffer, offset, IntPtr(pDst), copy_len);
		*pdwSize = copy_len;
		remain = *pdwRemain = remain - copy_len;
		offset = offset + copy_len;
		return TRUE;
	}
	const BOOL GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain)
	{
		TSClient::TSClient::Log(String::Format("GetTsStream() with **ppDst"));

		static unsigned char buffer[188 * 1024];
		DWORD size = sizeof(buffer);
		BOOL result = GetTsStream(buffer, &size, pdwRemain);
		*pdwSize = size;
		*ppDst = buffer;
		return result;
	}

	void PurgeTsStream(void)
	{
		TSClient::TSClient::Log(String::Format("PurgeTsStream()"));
		TSClient::TSClient::vidstrm->Flush();
	}

	void Release(void)
	{
		TSClient::TSClient::Release();
	}

	LPCTSTR GetTunerName(void)
	{
		TSClient::TSClient::Log(String::Format("GetTunerName()"));
		return TEXT("TSClient");
	}

	const BOOL IsTunerOpening(void)
	{
		return TSClient::TSClient::IsConnected() && TSClient::TSClient::IsReady();
	}

#ifdef UNICODE
	std::wstring ts;
#else 
	std::string ts;
#endif

	LPCTSTR EnumTuningSpace(const DWORD dwSpace)
	{
		auto result = TSClient::TSClient::EnumTuningSpace(dwSpace);
		if (result == nullptr) return nullptr;
#ifdef UNICODE
		ts = msclr::interop::marshal_as<std::wstring>(result);
#else
		ts = msclr::interop::marshal_as<std::string>(result);
#endif
		return ts.c_str();
	}

#ifdef UNICODE
	std::wstring cn;
#else 
	std::string cn;
#endif

	LPCTSTR EnumChannelName(const DWORD dwSpace, const DWORD dwChannel)
	{
		auto result = TSClient::TSClient::EnumChannelName(dwSpace, dwChannel);
		if (result == nullptr) return nullptr;
#ifdef UNICODE
		cn = msclr::interop::marshal_as<std::wstring>(result);
#else
		cn = msclr::interop::marshal_as<std::string>(result);
#endif
		return cn.c_str();
	}

	const BOOL SetChannel(const DWORD dwSpace, const DWORD dwChannel)
	{
		return TSClient::TSClient::SetChannel(dwSpace,dwChannel);
	}

	const DWORD GetCurSpace(void)
	{
		return TSClient::TSClient::GetCurrentSpace();
	}
	const DWORD GetCurChannel(void)
	{
		return TSClient::TSClient::GetCurrentChannel();
	}
};

std::unique_ptr<BonDriver_TSClient> driver;

#pragma unmanaged
extern "C"
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch(fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		return TRUE;
	case DLL_PROCESS_DETACH:
		return TRUE;
	default:
		return FALSE;
	}
}


extern "C" __declspec(dllexport) IBonDriver* CreateBonDriver()
{
	driver = std::unique_ptr<BonDriver_TSClient>(new BonDriver_TSClient());
	return driver.get();
}