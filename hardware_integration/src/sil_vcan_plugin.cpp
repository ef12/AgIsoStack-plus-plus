//================================================================================================
/// @file sil_vcan_plugin.cpp
///
/// @brief An interface for using a SIL virtual CAN interface over UDP.
/// @author Ehud Frank
///
/// @copyright 2024 The Open-Agriculture Developers
//================================================================================================

#include "isobus/hardware_integration/sil_vcan_plugin.hpp"
#include "isobus/isobus/can_stack_logger.hpp"

namespace isobus
{
	SilVcanPlugin::SilVcanPlugin(std::uint16_t localPort,
	                             const std::string &remoteIp,
	                             std::uint16_t remotePort,
	                             std::uint32_t timeoutMs) :
	  remoteIp(remoteIp),
	  localPort(localPort),
	  remotePort(remotePort),
	  timeoutMs(timeoutMs)
	{
	}

	SilVcanPlugin::~SilVcanPlugin()
	{
		close();
	}

	std::string SilVcanPlugin::get_name() const
	{
		return "SIL vCAN (UDP " + std::to_string(localPort) + " <-> " + remoteIp + ":" + std::to_string(remotePort) + ")";
	}

	bool SilVcanPlugin::get_is_valid() const
	{
		return silConfig.initialized;
	}

	void SilVcanPlugin::open()
	{
		if (silConfig.initialized)
		{
			return;
		}

		SilVcanConfigParams params{};
		params.local_port = localPort;
		params.remote_ip = remoteIp.c_str();
		params.remote_port = remotePort;
		params.timeout_ms = timeoutMs;
		params.max_pending_tx = 2000;
		params.max_rx_queue = 2000;

		sil_vcan_config_init(&silConfig, &params);

#ifndef DISABLE_CAN_STACK_LOGGER
		if (!silConfig.initialized)
		{
			LOG_CRITICAL("[SIL vCAN]: Failed to initialize SIL vCAN on port %u", static_cast<unsigned>(localPort));
		}
		else
		{
			LOG_INFO("[SIL vCAN]: Initialized on UDP %u <-> %s:%u", static_cast<unsigned>(localPort), remoteIp.c_str(), static_cast<unsigned>(remotePort));
		}
#endif
	}

	void SilVcanPlugin::close()
	{
		if (silConfig.initialized)
		{
			sil_vcan_config_deinit(&silConfig);
		}
	}

	bool SilVcanPlugin::read_frame(isobus::CANMessageFrame &canFrame)
	{
		if (!silConfig.initialized)
		{
			return false;
		}

		CanFrame frame{};
		CanDriver *driver = sil_vcan_config_get_driver(&silConfig);

		if ((nullptr == driver) || !can_driver_receive(driver, &frame))
		{
			return false;
		}

		canFrame.identifier = frame.id;
		canFrame.dataLength = frame.dlc;
		canFrame.isExtendedFrame = (frame.id > 0x7FFU);
		canFrame.timestamp_us = 0;
		canFrame.channel = 0;

		for (std::uint8_t i = 0; i < frame.dlc && i < 8; i++)
		{
			canFrame.data[i] = frame.data[i];
		}

#ifndef DISABLE_CAN_STACK_LOGGER
		static std::uint32_t rxCount = 0;
		rxCount++;
		std::uint8_t pf = (canFrame.identifier >> 16) & 0xFF;
		// Log all TP/ETP/VT frames (PF 0xC7, 0xC8, 0xE7, 0xEB, 0xEC, 0xEE, 0xFE)
		LOG_DEBUG("[SIL RX #%u] ID=%08X PF=%02X [%02X %02X %02X %02X %02X %02X %02X %02X]",
		          rxCount, canFrame.identifier, pf,
		          canFrame.data[0], canFrame.data[1], canFrame.data[2], canFrame.data[3],
		          canFrame.data[4], canFrame.data[5], canFrame.data[6], canFrame.data[7]);
#endif

		return true;
	}

	bool SilVcanPlugin::write_frame(const isobus::CANMessageFrame &canFrame)
	{
		if (!silConfig.initialized)
		{
			return false;
		}

		CanFrame frame{};
		frame.id = canFrame.identifier;
		frame.dlc = canFrame.dataLength;

		for (std::uint8_t i = 0; i < canFrame.dataLength && i < 8; i++)
		{
			frame.data[i] = canFrame.data[i];
		}

		CanDriver *driver = sil_vcan_config_get_driver(&silConfig);

		if (nullptr == driver)
		{
			return false;
		}

#ifndef DISABLE_CAN_STACK_LOGGER
		static std::uint32_t txCount = 0;
		txCount++;
		std::uint8_t pf_tx = (canFrame.identifier >> 16) & 0xFF;
		LOG_DEBUG("[SIL TX #%u] ID=%08X PF=%02X [%02X %02X %02X %02X %02X %02X %02X %02X]",
		          txCount, canFrame.identifier, pf_tx,
		          canFrame.data[0], canFrame.data[1], canFrame.data[2], canFrame.data[3],
		          canFrame.data[4], canFrame.data[5], canFrame.data[6], canFrame.data[7]);
#endif

		return can_driver_send(driver, &frame);
	}
}
