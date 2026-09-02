//================================================================================================
/// @file wcan_plugin.hpp
///
/// @brief A CANHardwarePlugin for the WCAN shared-memory virtual CAN bus.
///
/// WCAN is developed in the SIL repository under sil_lib/wcan. This plugin is the
/// adapter between it and the stack: WCAN itself knows nothing about ISOBUS or
/// AgIsoStack, so the dependency runs in one direction only.
///
/// Unlike VirtualCANPlugin, which connects instances within a single process, a
/// WCAN bus is shared across processes. A Virtual Terminal, an implement
/// simulation and a test injector can therefore all sit on the same bus at once,
/// including across process bitness.
///
/// @copyright 2026 The Open-Agriculture Developers
//================================================================================================
#ifndef WCAN_PLUGIN_HPP
#define WCAN_PLUGIN_HPP

#include <atomic>
#include <cstdint>
#include <string>

#include "isobus/hardware_integration/can_hardware_plugin.hpp"
#include "isobus/isobus/can_message_frame.hpp"

#include "wcan.h"

namespace isobus
{
	/// @brief A CAN driver for the WCAN shared-memory virtual bus.
	class WCANPlugin : public CANHardwarePlugin
	{
	public:
		/// @brief Constructor
		/// @param[in] busName The bus to join. Any name is valid; nodes sharing a
		/// name share a bus.
		/// @param[in] receiveOwnMessages If true, this instance also receives the
		/// frames it transmits. Defaults to false, matching a real CAN controller.
		/// @param[in] bitrate Bus bitrate in bit/s. Zero adopts the rate of an
		/// existing bus, and is the safest choice for a client joining a bus the
		/// simulation created.
		explicit WCANPlugin(std::string busName = "wcan0",
		                    bool receiveOwnMessages = false,
		                    std::uint32_t bitrate = 0);

		/// @brief Destructor. Closes the bus if it is still open.
		virtual ~WCANPlugin();

		WCANPlugin(const WCANPlugin &) = delete;
		WCANPlugin &operator=(const WCANPlugin &) = delete;

		/// @brief Returns the displayable name of the plugin
		std::string get_name() const override;

		/// @brief Returns the bus name this instance is attached to
		std::string get_bus_name() const;

		/// @brief Returns whether the driver is connected
		bool get_is_valid() const override;

		/// @brief Joins the bus
		void open() override;

		/// @brief Leaves the bus. Safe to call while read_frame is blocked.
		void close() override;

		/// @brief Reads one frame, blocking up to the read timeout
		/// @returns true if a frame was read, false on timeout or error
		bool read_frame(isobus::CANMessageFrame &canFrame) override;

		/// @brief Writes one frame to the bus
		/// @returns true if the frame was accepted
		bool write_frame(const isobus::CANMessageFrame &canFrame) override;

		/// @brief Sets how long read_frame blocks before reporting no data
		void set_read_timeout(std::uint32_t milliseconds);

	private:
		static constexpr std::uint32_t DEFAULT_READ_TIMEOUT_MS = 1000;

		const std::string busName;
		const bool receiveOwnMessages;
		const std::uint32_t bitrate;
		std::uint32_t readTimeoutMs = DEFAULT_READ_TIMEOUT_MS;
		wcan_socket_t socket = WCAN_SOCKET_INITIALIZER;
		std::atomic<bool> running{ false };
	};
}

#endif // WCAN_PLUGIN_HPP
