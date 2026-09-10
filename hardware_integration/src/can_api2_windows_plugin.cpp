//================================================================================================
/// @file can_api2_windows_plugin.cpp
///
/// @brief A Windows CANHardwarePlugin for PEAK CAN-API 2 virtual networks.
/// @attention Use of the PEAK driver is governed by PEAK-System's license and requires the
/// corresponding CAN-API 2 driver installation.
/// @author The Open-Agriculture Developers
///
/// @copyright 2026 The Open-Agriculture Developers
//================================================================================================

#include "isobus/hardware_integration/can_api2_windows_plugin.hpp"
#include "isobus/isobus/can_stack_logger.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

namespace
{
	using CANAPI2Status = std::uint32_t;
	using CANAPI2ClientHandle = std::uint8_t;
	using CANAPI2NetHandle = std::uint8_t;

	constexpr CANAPI2Status CAN_API2_ERROR_OK = 0x0000;
	constexpr CANAPI2Status CAN_API2_ERROR_RECEIVE_QUEUE_EMPTY = 0x0020;
	constexpr CANAPI2Status CAN_API2_ERROR_NET_IN_USE = 0x0800;
	constexpr CANAPI2Status CAN_API2_ERROR_INVALID_NET = 0x1800;

	constexpr std::uint8_t CAN_API2_MESSAGE_RTR = 0x01;
	constexpr std::uint8_t CAN_API2_MESSAGE_EXTENDED = 0x02;
	constexpr std::uint8_t CAN_API2_MESSAGE_NON_DATA_MASK = 0xF0;

	constexpr std::size_t CAN_API2_MAX_NAME_LENGTH = 20;
	constexpr std::uint8_t CAN_API2_MIN_NET_HANDLE = 1;
	constexpr std::uint8_t CAN_API2_MAX_NET_HANDLE = 32;
	constexpr std::size_t MAX_NON_DATA_MESSAGES_PER_READ = 32;

#pragma pack(push, 1)
	struct CANAPI2Timestamp
	{
		std::uint32_t millis;
		std::uint16_t millisOverflow;
		std::uint16_t micros;
	};

	struct CANAPI2Message
	{
		std::uint32_t identifier;
		std::uint8_t messageType;
		std::uint8_t length;
		std::uint8_t data[8];
	};
#pragma pack(pop)

	static_assert(sizeof(CANAPI2Timestamp) == 8, "Unexpected CAN-API 2 timestamp layout");
	static_assert(sizeof(CANAPI2Message) == 14, "Unexpected CAN-API 2 message layout");

	bool is_valid_name(const std::string &name)
	{
		return (!name.empty()) &&
		  (name.size() <= CAN_API2_MAX_NAME_LENGTH) &&
		  (std::string::npos == name.find('\0'));
	}

	bool bitrate_to_btr0_btr1(std::uint32_t bitrate, std::uint16_t &result)
	{
		switch (bitrate)
		{
			case 1000000:
				result = 0x0014;
				break;
			case 500000:
				result = 0x001C;
				break;
			case 250000:
				result = 0x011C;
				break;
			case 125000:
				result = 0x031C;
				break;
			case 100000:
				result = 0x432F;
				break;
			case 50000:
				result = 0x472F;
				break;
			case 20000:
				result = 0x532F;
				break;
			case 10000:
				result = 0x672F;
				break;
			case 5000:
				result = 0x7F7F;
				break;
			default:
				return false;
		}
		return true;
	}

	std::string windows_error_text(DWORD error)
	{
		LPSTR rawMessage = nullptr;
		const DWORD length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		                                      FORMAT_MESSAGE_FROM_SYSTEM |
		                                      FORMAT_MESSAGE_IGNORE_INSERTS,
		                                    nullptr,
		                                    error,
		                                    0,
		                                    reinterpret_cast<LPSTR>(&rawMessage),
		                                    0,
		                                    nullptr);
		std::string result;
		if ((0 != length) && (nullptr != rawMessage))
		{
			result.assign(rawMessage, length);
			while ((!result.empty()) &&
			       ((result.back() == '\r') || (result.back() == '\n') || (result.back() == ' ')))
			{
				result.pop_back();
			}
		}
		if (nullptr != rawMessage)
		{
			LocalFree(rawMessage);
		}
		return result;
	}
}

namespace isobus
{
	struct CANAPI2WindowsPlugin::Implementation
	{
		using SetDeviceNameFunction = CANAPI2Status(WINAPI *)(char *);
		using VersionInfoFunction = CANAPI2Status(WINAPI *)(char *);
		using GetErrorTextFunction = CANAPI2Status(WINAPI *)(CANAPI2Status, char *);
		using RegisterNetFunction = CANAPI2Status(WINAPI *)(CANAPI2NetHandle, const char *, std::uint8_t, std::uint16_t);
		using RemoveNetFunction = CANAPI2Status(WINAPI *)(CANAPI2NetHandle);
		using RegisterClientFunction = CANAPI2Status(WINAPI *)(const char *, std::uint32_t, CANAPI2ClientHandle *);
		using RemoveClientFunction = CANAPI2Status(WINAPI *)(CANAPI2ClientHandle);
		using ConnectToNetFunction = CANAPI2Status(WINAPI *)(CANAPI2ClientHandle, char *, CANAPI2NetHandle *);
		using DisconnectFromNetFunction = CANAPI2Status(WINAPI *)(CANAPI2ClientHandle, CANAPI2NetHandle);
		using SetClientFilterFunction = CANAPI2Status(WINAPI *)(CANAPI2ClientHandle, CANAPI2NetHandle, std::int32_t, std::uint32_t, std::uint32_t);
		using ReadFunction = CANAPI2Status(WINAPI *)(CANAPI2ClientHandle, CANAPI2Message *, CANAPI2NetHandle *, CANAPI2Timestamp *);
		using WriteFunction = CANAPI2Status(WINAPI *)(CANAPI2ClientHandle, CANAPI2NetHandle, CANAPI2Message *, CANAPI2Timestamp *);
		using GetSystemTimeFunction = CANAPI2Status(WINAPI *)(CANAPI2Timestamp *);

		HMODULE library = nullptr;
		SetDeviceNameFunction setDeviceName = nullptr;
		VersionInfoFunction versionInfo = nullptr;
		GetErrorTextFunction getErrorText = nullptr;
		RegisterNetFunction registerNet = nullptr;
		RemoveNetFunction removeNet = nullptr;
		RegisterClientFunction registerClient = nullptr;
		RemoveClientFunction removeClient = nullptr;
		ConnectToNetFunction connectToNet = nullptr;
		DisconnectFromNetFunction disconnectFromNet = nullptr;
		SetClientFilterFunction setClientFilter = nullptr;
		ReadFunction read = nullptr;
		WriteFunction write = nullptr;
		GetSystemTimeFunction getSystemTime = nullptr;

		CANAPI2ClientHandle clientHandle = 0;
		CANAPI2NetHandle netHandle = 0;
		CANAPI2NetHandle createdNetHandle = 0;
		bool clientRegistered = false;
		bool connected = false;
		std::atomic_bool valid{ false };
		std::atomic<CANAPI2Status> lastReadError{ CAN_API2_ERROR_OK };
		std::atomic<CANAPI2Status> lastWriteError{ CAN_API2_ERROR_OK };

		template<typename FunctionType>
		bool resolve(FunctionType &function, const char *name)
		{
			function = reinterpret_cast<FunctionType>(GetProcAddress(library, name));
			if (nullptr == function)
			{
				LOG_ERROR(std::string("[CAN-API 2]: CanApi2.dll does not export ") + name);
				return false;
			}
			return true;
		}

		bool load()
		{
			if (nullptr != library)
			{
				return true;
			}

			std::array<wchar_t, MAX_PATH> systemDirectory{};
			const UINT length = GetSystemDirectoryW(systemDirectory.data(), static_cast<UINT>(systemDirectory.size()));
			if ((0 == length) || (length >= systemDirectory.size()))
			{
				LOG_ERROR("[CAN-API 2]: Unable to locate the Windows system directory.");
				return false;
			}

			std::wstring libraryPath(systemDirectory.data(), length);
			libraryPath += L"\\CanApi2.dll";
			library = LoadLibraryW(libraryPath.c_str());
			if (nullptr == library)
			{
				const DWORD error = GetLastError();
				LOG_ERROR("[CAN-API 2]: Unable to load the installed CanApi2.dll matching this process (Windows error " +
				          std::to_string(error) + "): " + windows_error_text(error));
				return false;
			}

			const bool allFunctionsResolved =
			  resolve(setDeviceName, "CAN_SetDeviceName") &&
			  resolve(versionInfo, "CAN_VersionInfo") &&
			  resolve(getErrorText, "CAN_GetErrText") &&
			  resolve(registerNet, "CAN_RegisterNet") &&
			  resolve(removeNet, "CAN_RemoveNet") &&
			  resolve(registerClient, "CAN_RegisterClient") &&
			  resolve(removeClient, "CAN_RemoveClient") &&
			  resolve(connectToNet, "CAN_ConnectToNet") &&
			  resolve(disconnectFromNet, "CAN_DisconnectFromNet") &&
			  resolve(setClientFilter, "CAN_SetClientFilter") &&
			  resolve(read, "CAN_Read") &&
			  resolve(write, "CAN_Write") &&
			  resolve(getSystemTime, "CAN_GetSystemTime");

			if (!allFunctionsResolved)
			{
				unload();
			}
			return allFunctionsResolved;
		}

		void unload()
		{
			setDeviceName = nullptr;
			versionInfo = nullptr;
			getErrorText = nullptr;
			registerNet = nullptr;
			removeNet = nullptr;
			registerClient = nullptr;
			removeClient = nullptr;
			connectToNet = nullptr;
			disconnectFromNet = nullptr;
			setClientFilter = nullptr;
			read = nullptr;
			write = nullptr;
			getSystemTime = nullptr;

			if (nullptr != library)
			{
				FreeLibrary(library);
				library = nullptr;
			}
		}

		std::string describe_status(const std::string &operation, CANAPI2Status status) const
		{
			std::ostringstream stream;
			stream << "[CAN-API 2]: " << operation << " failed with 0x"
			       << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << status;

			if (nullptr != getErrorText)
			{
				std::array<char, 256> message{};
				if (CAN_API2_ERROR_OK == getErrorText(status, message.data()))
				{
					stream << ": " << message.data();
				}
			}
			return stream.str();
		}
	};

	CANAPI2WindowsPlugin::CANAPI2WindowsPlugin(std::string netName,
	                                           std::string clientName,
	                                           std::uint32_t bitrate,
	                                           bool createMissingNet,
	                                           std::uint8_t preferredNetHandle) :
	  implementation(new Implementation()),
	  netName(std::move(netName)),
	  clientName(std::move(clientName)),
	  bitrate(bitrate),
	  createMissingNet(createMissingNet),
	  preferredNetHandle(preferredNetHandle)
	{
	}

	CANAPI2WindowsPlugin::~CANAPI2WindowsPlugin()
	{
		close();
	}

	std::string CANAPI2WindowsPlugin::get_name() const
	{
		return "PEAK PCAN Virtual (" + netName + ")";
	}

	bool CANAPI2WindowsPlugin::get_is_valid() const
	{
		return implementation->valid.load();
	}

	void CANAPI2WindowsPlugin::open()
	{
		if (implementation->valid.load())
		{
			return;
		}

		std::uint16_t encodedBitrate = 0;
		if (!is_valid_name(netName))
		{
			LOG_ERROR("[CAN-API 2]: Net name must contain 1..20 bytes and no embedded nulls.");
			return;
		}
		if (!is_valid_name(clientName))
		{
			LOG_ERROR("[CAN-API 2]: Client name must contain 1..20 bytes and no embedded nulls.");
			return;
		}
		if (!bitrate_to_btr0_btr1(bitrate, encodedBitrate))
		{
			LOG_ERROR("[CAN-API 2]: Unsupported bitrate: " + std::to_string(bitrate));
			return;
		}
		if ((preferredNetHandle < CAN_API2_MIN_NET_HANDLE) ||
		    (preferredNetHandle > CAN_API2_MAX_NET_HANDLE))
		{
			LOG_ERROR("[CAN-API 2]: Preferred net handle must be in the range 1..32.");
			return;
		}

		close();
		if (!implementation->load())
		{
			return;
		}

		char virtualDeviceName[] = "pcan_virtual";
		CANAPI2Status status = implementation->setDeviceName(virtualDeviceName);
		if (CAN_API2_ERROR_OK != status)
		{
			LOG_ERROR(implementation->describe_status("selecting pcan_virtual", status));
			close();
			return;
		}

		std::array<char, 256> version{};
		status = implementation->versionInfo(version.data());
		if (CAN_API2_ERROR_OK == status)
		{
			std::string versionText(version.data());
			std::replace(versionText.begin(), versionText.end(), '\r', ' ');
			std::replace(versionText.begin(), versionText.end(), '\n', ' ');
			LOG_INFO("[CAN-API 2]: Loaded " + versionText);
		}

		status = implementation->registerClient(clientName.c_str(), 0, &implementation->clientHandle);
		if (CAN_API2_ERROR_OK != status)
		{
			LOG_ERROR(implementation->describe_status("registering client '" + clientName + "'", status));
			close();
			return;
		}
		implementation->clientRegistered = true;

		status = implementation->connectToNet(implementation->clientHandle,
		                                      const_cast<char *>(netName.c_str()),
		                                      &implementation->netHandle);
		if ((CAN_API2_ERROR_INVALID_NET == status) && createMissingNet)
		{
			std::vector<CANAPI2NetHandle> handles;
			handles.push_back(preferredNetHandle);
			for (std::uint8_t handle = CAN_API2_MAX_NET_HANDLE; handle >= CAN_API2_MIN_NET_HANDLE; --handle)
			{
				if (handle != preferredNetHandle)
				{
					handles.push_back(handle);
				}
				if (CAN_API2_MIN_NET_HANDLE == handle)
				{
					break;
				}
			}

			CANAPI2Status registrationStatus = CAN_API2_ERROR_INVALID_NET;
			for (const CANAPI2NetHandle handle : handles)
			{
				registrationStatus = implementation->registerNet(handle,
				                                                 netName.c_str(),
				                                                 0,
				                                                 encodedBitrate);
				if (CAN_API2_ERROR_OK == registrationStatus)
				{
					implementation->createdNetHandle = handle;
					break;
				}
			}

			// Retry even if registration failed. Another process may have created the
			// named network after our first connection attempt.
			status = implementation->connectToNet(implementation->clientHandle,
			                                      const_cast<char *>(netName.c_str()),
			                                      &implementation->netHandle);
			if ((CAN_API2_ERROR_OK != status) && (0 == implementation->createdNetHandle))
			{
				LOG_ERROR(implementation->describe_status("creating internal net '" + netName + "'", registrationStatus));
			}
		}

		if (CAN_API2_ERROR_OK != status)
		{
			LOG_ERROR(implementation->describe_status("connecting to net '" + netName + "'", status));
			close();
			return;
		}
		implementation->connected = true;

		status = implementation->setClientFilter(implementation->clientHandle,
		                                         implementation->netHandle,
		                                         0,
		                                         0,
		                                         0x7FF);
		if (CAN_API2_ERROR_OK != status)
		{
			LOG_ERROR(implementation->describe_status("opening the standard identifier filter", status));
			close();
			return;
		}

		status = implementation->setClientFilter(implementation->clientHandle,
		                                         implementation->netHandle,
		                                         1,
		                                         0,
		                                         0x1FFFFFFF);
		if (CAN_API2_ERROR_OK != status)
		{
			LOG_ERROR(implementation->describe_status("opening the extended identifier filter", status));
			close();
			return;
		}

		implementation->lastReadError.store(CAN_API2_ERROR_OK);
		implementation->lastWriteError.store(CAN_API2_ERROR_OK);
		implementation->valid.store(true);
		LOG_INFO("[CAN-API 2]: Connected to pcan_virtual net '" + netName + "'.");
	}

	void CANAPI2WindowsPlugin::close()
	{
		implementation->valid.store(false);

		if (implementation->connected && (nullptr != implementation->disconnectFromNet))
		{
			const CANAPI2Status status = implementation->disconnectFromNet(implementation->clientHandle,
			                                                               implementation->netHandle);
			if (CAN_API2_ERROR_OK != status)
			{
				LOG_WARNING(implementation->describe_status("disconnecting from net '" + netName + "'", status));
			}
		}
		implementation->connected = false;
		implementation->netHandle = 0;

		if (implementation->clientRegistered && (nullptr != implementation->removeClient))
		{
			const CANAPI2Status status = implementation->removeClient(implementation->clientHandle);
			if (CAN_API2_ERROR_OK != status)
			{
				LOG_WARNING(implementation->describe_status("removing client '" + clientName + "'", status));
			}
		}
		implementation->clientRegistered = false;
		implementation->clientHandle = 0;

		if ((0 != implementation->createdNetHandle) && (nullptr != implementation->removeNet))
		{
			const CANAPI2Status status = implementation->removeNet(implementation->createdNetHandle);
			if ((CAN_API2_ERROR_OK != status) && (CAN_API2_ERROR_NET_IN_USE != status))
			{
				LOG_WARNING(implementation->describe_status("removing internal net '" + netName + "'", status));
			}
		}
		implementation->createdNetHandle = 0;
		implementation->unload();
	}

	bool CANAPI2WindowsPlugin::read_frame(isobus::CANMessageFrame &canFrame)
	{
		if ((!implementation->valid.load()) || (nullptr == implementation->read))
		{
			return false;
		}

		for (std::size_t i = 0; i < MAX_NON_DATA_MESSAGES_PER_READ; ++i)
		{
			CANAPI2Message message{};
			CANAPI2NetHandle sourceNet = 0;
			CANAPI2Timestamp timestamp{};
			const CANAPI2Status status = implementation->read(implementation->clientHandle,
			                                                  &message,
			                                                  &sourceNet,
			                                                  &timestamp);
			if (CAN_API2_ERROR_OK != status)
			{
				if (0 != (status & ~CAN_API2_ERROR_RECEIVE_QUEUE_EMPTY))
				{
					const CANAPI2Status previousStatus = implementation->lastReadError.exchange(status);
					if (previousStatus != status)
					{
						LOG_WARNING(implementation->describe_status("reading a frame", status));
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				return false;
			}

			implementation->lastReadError.store(CAN_API2_ERROR_OK);
			if ((sourceNet != implementation->netHandle) ||
			    (0 != (message.messageType & CAN_API2_MESSAGE_NON_DATA_MASK)) ||
			    (0 != (message.messageType & CAN_API2_MESSAGE_RTR)))
			{
				continue;
			}
			if (message.length > sizeof(canFrame.data))
			{
				LOG_WARNING("[CAN-API 2]: Ignoring a frame with an invalid data length.");
				continue;
			}

			canFrame.identifier = message.identifier;
			canFrame.dataLength = message.length;
			canFrame.isExtendedFrame = (0 != (message.messageType & CAN_API2_MESSAGE_EXTENDED));
			canFrame.timestamp_us =
			  (((static_cast<std::uint64_t>(timestamp.millisOverflow) << 32) | timestamp.millis) * 1000ULL) +
			  timestamp.micros;
			canFrame.channel = 0;
			std::memset(canFrame.data, 0, sizeof(canFrame.data));
			std::memcpy(canFrame.data, message.data, message.length);
			return true;
		}

		return false;
	}

	bool CANAPI2WindowsPlugin::write_frame(const isobus::CANMessageFrame &canFrame)
	{
		if ((!implementation->valid.load()) ||
		    (nullptr == implementation->write) ||
		    (nullptr == implementation->getSystemTime))
		{
			return false;
		}
		if (canFrame.dataLength > sizeof(canFrame.data))
		{
			LOG_WARNING("[CAN-API 2]: Cannot write a frame with a data length greater than 8.");
			return false;
		}
		if ((canFrame.isExtendedFrame && (canFrame.identifier > 0x1FFFFFFF)) ||
		    ((!canFrame.isExtendedFrame) && (canFrame.identifier > 0x7FF)))
		{
			LOG_WARNING("[CAN-API 2]: Cannot write a frame with an identifier outside its valid range.");
			return false;
		}

		CANAPI2Message message{};
		message.identifier = canFrame.identifier;
		message.messageType = canFrame.isExtendedFrame ? CAN_API2_MESSAGE_EXTENDED : 0;
		message.length = canFrame.dataLength;
		std::memcpy(message.data, canFrame.data, canFrame.dataLength);

		CANAPI2Timestamp sendTime{};
		CANAPI2Status status = implementation->getSystemTime(&sendTime);
		if (CAN_API2_ERROR_OK == status)
		{
			status = implementation->write(implementation->clientHandle,
			                               implementation->netHandle,
			                               &message,
			                               &sendTime);
		}

		if (CAN_API2_ERROR_OK != status)
		{
			const CANAPI2Status previousStatus = implementation->lastWriteError.exchange(status);
			if (previousStatus != status)
			{
				LOG_WARNING(implementation->describe_status("writing a frame", status));
			}
			return false;
		}

		implementation->lastWriteError.store(CAN_API2_ERROR_OK);
		return true;
	}

	bool CANAPI2WindowsPlugin::configure(const std::string &newNetName,
	                                     std::uint32_t newBitrate,
	                                     bool newCreateMissingNet,
	                                     std::uint8_t newPreferredNetHandle)
	{
		std::uint16_t unusedEncodedBitrate = 0;
		if (implementation->valid.load() || implementation->connected || implementation->clientRegistered)
		{
			LOG_ERROR("[CAN-API 2]: Cannot change configuration while the plugin is open.");
			return false;
		}
		if (!is_valid_name(newNetName))
		{
			LOG_ERROR("[CAN-API 2]: Net name must contain 1..20 bytes and no embedded nulls.");
			return false;
		}
		if (!bitrate_to_btr0_btr1(newBitrate, unusedEncodedBitrate))
		{
			LOG_ERROR("[CAN-API 2]: Unsupported bitrate: " + std::to_string(newBitrate));
			return false;
		}
		if ((newPreferredNetHandle < CAN_API2_MIN_NET_HANDLE) ||
		    (newPreferredNetHandle > CAN_API2_MAX_NET_HANDLE))
		{
			LOG_ERROR("[CAN-API 2]: Preferred net handle must be in the range 1..32.");
			return false;
		}

		netName = newNetName;
		bitrate = newBitrate;
		createMissingNet = newCreateMissingNet;
		preferredNetHandle = newPreferredNetHandle;
		return true;
	}

	std::string CANAPI2WindowsPlugin::get_net_name() const
	{
		return netName;
	}

	std::string CANAPI2WindowsPlugin::get_client_name() const
	{
		return clientName;
	}

	std::uint32_t CANAPI2WindowsPlugin::get_bitrate() const
	{
		return bitrate;
	}

	bool CANAPI2WindowsPlugin::get_create_missing_net() const
	{
		return createMissingNet;
	}

	std::uint8_t CANAPI2WindowsPlugin::get_preferred_net_handle() const
	{
		return preferredNetHandle;
	}
}
