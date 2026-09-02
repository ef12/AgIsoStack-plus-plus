//================================================================================================
/// @file wcan_plugin.cpp
///
/// @brief A CANHardwarePlugin for the WCAN shared-memory virtual CAN bus.
///
/// @copyright 2026 The Open-Agriculture Developers
//================================================================================================
#include "isobus/hardware_integration/wcan_plugin.hpp"

#include <cstring>

namespace isobus
{
	WCANPlugin::WCANPlugin(std::string busName, bool receiveOwnMessages, std::uint32_t bitrate) :
	  busName(std::move(busName)),
	  receiveOwnMessages(receiveOwnMessages),
	  bitrate(bitrate)
	{
	}

	WCANPlugin::~WCANPlugin()
	{
		close();
	}

	std::string WCANPlugin::get_name() const
	{
		return "WCAN Virtual Bus (" + busName + ")";
	}

	std::string WCANPlugin::get_bus_name() const
	{
		return busName;
	}

	bool WCANPlugin::get_is_valid() const
	{
		return running.load();
	}

	void WCANPlugin::open()
	{
		if (running.load())
		{
			return;
		}

		wcan_params_t params;
		std::memset(&params, 0, sizeof(params));
		params.bitrate = bitrate; // zero adopts the existing bus rate
		params.flags = receiveOwnMessages ? static_cast<std::uint32_t>(WCAN_OPEN_ECHO)
		                                  : 0u;

		if (WCAN_OK == wcan_open_ex(&socket, busName.c_str(), &params))
		{
			running.store(true);
		}
	}

	void WCANPlugin::close()
	{
		if (running.exchange(false))
		{
			// wcan_close cancels a blocked read_frame and waits for it to
			// return before releasing any state, so this is safe to call from a
			// different thread than the one reading.
			(void)wcan_close(&socket);
		}
	}

	bool WCANPlugin::read_frame(isobus::CANMessageFrame &canFrame)
	{
		if (!running.load())
		{
			return false;
		}

		wcan_frame_t frame;
		std::memset(&frame, 0, sizeof(frame));

		if (WCAN_OK != wcan_recv_timeout(&socket, &frame, readTimeoutMs))
		{
			return false;
		}

		// The stack only handles classical frames here, and WCAN never delivers
		// a classical frame longer than 8 bytes.
		if (frame.dlc > sizeof(canFrame.data))
		{
			return false;
		}

		std::memset(&canFrame, 0, sizeof(canFrame));
		canFrame.identifier = frame.can_id;
		canFrame.isExtendedFrame = (0 != (frame.flags & WCAN_FLAG_EXTENDED));
		canFrame.dataLength = frame.dlc;
		std::memcpy(canFrame.data, frame.data, frame.dlc);
		return true;
	}

	bool WCANPlugin::write_frame(const isobus::CANMessageFrame &canFrame)
	{
		if (!running.load())
		{
			return false;
		}
		if (canFrame.dataLength > sizeof(canFrame.data))
		{
			return false;
		}

		wcan_frame_t frame;
		std::memset(&frame, 0, sizeof(frame));
		frame.can_id = canFrame.identifier;
		frame.flags = canFrame.isExtendedFrame
		                ? static_cast<std::uint8_t>(WCAN_FLAG_EXTENDED)
		                : static_cast<std::uint8_t>(0);
		frame.dlc = canFrame.dataLength;
		std::memcpy(frame.data, canFrame.data, canFrame.dataLength);

		return WCAN_OK == wcan_send(&socket, &frame);
	}

	void WCANPlugin::set_read_timeout(std::uint32_t milliseconds)
	{
		readTimeoutMs = milliseconds;
	}
}
