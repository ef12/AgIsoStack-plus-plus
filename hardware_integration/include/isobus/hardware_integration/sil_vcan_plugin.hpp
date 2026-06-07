//================================================================================================
/// @file sil_vcan_plugin.hpp
///
/// @brief An interface for using a SIL virtual CAN interface over UDP.
/// @author Ehud Frank
///
/// @copyright 2024 The Open-Agriculture Developers
//================================================================================================
#ifndef SIL_VCAN_PLUGIN_HPP
#define SIL_VCAN_PLUGIN_HPP

#include "isobus/hardware_integration/can_hardware_plugin.hpp"

#include <cstdint>
#include <string>

extern "C"
{
#include "sil_vcan_config.h"
}

namespace isobus
{
	//================================================================================================
	/// @class SilVcanPlugin
	///
	/// @brief A CAN hardware plugin for the SIL virtual CAN interface (UDP transport).
	//================================================================================================
	class SilVcanPlugin : public CANHardwarePlugin
	{
	public:
		/// @brief Constructor
		/// @param[in] localPort The UDP port to bind locally
		/// @param[in] remoteIp The remote IPv4 address (e.g. "127.0.0.1")
		/// @param[in] remotePort The remote UDP port
		/// @param[in] timeoutMs The socket receive timeout in milliseconds (default 1 for non-blocking poll)
		SilVcanPlugin(std::uint16_t localPort,
		              const std::string &remoteIp,
		              std::uint16_t remotePort,
		              std::uint32_t timeoutMs = 1);

		/// @brief Destructor, closes the connection if open
		~SilVcanPlugin() override;

		/// @brief Returns the name of the plugin
		/// @returns The name of the plugin
		std::string get_name() const override;

		/// @brief Returns if the driver is ready and connected
		/// @returns `true` if the driver is initialized and valid
		bool get_is_valid() const override;

		/// @brief Opens the SIL vCAN connection
		void open() override;

		/// @brief Closes the SIL vCAN connection
		void close() override;

		/// @brief Reads one CAN frame from the virtual bus
		/// @param[in, out] canFrame The CAN frame that was read
		/// @returns `true` if a CAN frame was read, otherwise `false`
		bool read_frame(isobus::CANMessageFrame &canFrame) override;

		/// @brief Writes one CAN frame to the virtual bus
		/// @param[in] canFrame The frame to write to the bus
		/// @returns `true` if the frame was written, otherwise `false`
		bool write_frame(const isobus::CANMessageFrame &canFrame) override;

	private:
		SilVcanConfig silConfig{}; ///< The SIL vCAN configuration state
		std::string remoteIp; ///< The remote IP address
		std::uint16_t localPort; ///< The local UDP port
		std::uint16_t remotePort; ///< The remote UDP port
		std::uint32_t timeoutMs; ///< The socket receive timeout
	};
}

#endif // SIL_VCAN_PLUGIN_HPP
