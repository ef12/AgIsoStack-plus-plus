//================================================================================================
/// @file can_api2_windows_plugin.hpp
///
/// @brief A Windows CANHardwarePlugin for PEAK CAN-API 2 virtual networks.
/// @attention Use of the PEAK driver is governed by PEAK-System's license and requires the
/// corresponding CAN-API 2 driver installation.
/// @author The Open-Agriculture Developers
///
/// @copyright 2026 The Open-Agriculture Developers
//================================================================================================
#ifndef CAN_API2_WINDOWS_PLUGIN_HPP
#define CAN_API2_WINDOWS_PLUGIN_HPP

#include "isobus/hardware_integration/can_hardware_plugin.hpp"
#include "isobus/isobus/can_message_frame.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace isobus
{
	//================================================================================================
	/// @class CANAPI2WindowsPlugin
	///
	/// @brief A Windows CAN driver for a named PEAK CAN-API 2 virtual network.
	/// @details The plugin loads the installed, architecture-matched CanApi2.dll at runtime, selects the
	/// `pcan_virtual` device for the current process, and connects to a named CAN-API 2 network.
	/// If requested, a missing network is created as a software-only internal network.
	//================================================================================================
	class CANAPI2WindowsPlugin : public CANHardwarePlugin
	{
	public:
		static constexpr std::uint32_t DEFAULT_BITRATE = 250000; ///< Default ISOBUS bitrate
		static constexpr std::uint8_t DEFAULT_NET_HANDLE = 31; ///< Preferred handle for a new internal net

		/// @brief Constructor for the Windows PEAK CAN-API 2 virtual driver
		/// @param[in] netName Name of the CAN-API 2 network to join (1..20 bytes)
		/// @param[in] clientName Name registered with CAN-API 2 (1..20 bytes)
		/// @param[in] bitrate Bitrate used when creating a missing internal network
		/// @param[in] createMissingNet If true, create a software-only network when it is missing
		/// @param[in] preferredNetHandle Preferred handle (1..32) when creating a network
		explicit CANAPI2WindowsPlugin(std::string netName = "PCANLight_USB",
		                              std::string clientName = "AgIsoStack",
		                              std::uint32_t bitrate = DEFAULT_BITRATE,
		                              bool createMissingNet = true,
		                              std::uint8_t preferredNetHandle = DEFAULT_NET_HANDLE);

		/// @brief Destructor. Disconnects from CAN-API 2 and unloads the runtime library.
		~CANAPI2WindowsPlugin() override;

		CANAPI2WindowsPlugin(const CANAPI2WindowsPlugin &) = delete;
		CANAPI2WindowsPlugin &operator=(const CANAPI2WindowsPlugin &) = delete;

		/// @brief Returns a displayable name containing the configured network
		std::string get_name() const override;

		/// @brief Returns true when the CAN-API 2 client is connected and ready
		bool get_is_valid() const override;

		/// @brief Connects to the configured `pcan_virtual` network
		void open() override;

		/// @brief Disconnects the client and releases all CAN-API 2 resources
		void close() override;

		/// @brief Reads one classical CAN frame, or returns false when no frame is available
		bool read_frame(isobus::CANMessageFrame &canFrame) override;

		/// @brief Writes one classical CAN frame to the configured network
		bool write_frame(const isobus::CANMessageFrame &canFrame) override;

		/// @brief Changes the virtual network settings while the plugin is closed
		/// @returns true if the settings are valid and were applied
		bool configure(const std::string &netName,
		               std::uint32_t bitrate = DEFAULT_BITRATE,
		               bool createMissingNet = true,
		               std::uint8_t preferredNetHandle = DEFAULT_NET_HANDLE);

		std::string get_net_name() const;
		std::string get_client_name() const;
		std::uint32_t get_bitrate() const;
		bool get_create_missing_net() const;
		std::uint8_t get_preferred_net_handle() const;

	private:
		struct Implementation;
		std::unique_ptr<Implementation> implementation;

		std::string netName;
		std::string clientName;
		std::uint32_t bitrate;
		bool createMissingNet;
		std::uint8_t preferredNetHandle;
	};
}

#endif // CAN_API2_WINDOWS_PLUGIN_HPP
