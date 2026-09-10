#include <gtest/gtest.h>

#include "isobus/hardware_integration/can_api2_windows_plugin.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <chrono>
#include <cstring>
#include <string>
#include <thread>

using namespace isobus;

namespace
{
	bool receive_with_timeout(CANAPI2WindowsPlugin &receiver,
	                          CANMessageFrame &frame,
	                          std::chrono::milliseconds timeout)
	{
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		while (std::chrono::steady_clock::now() < deadline)
		{
			if (receiver.read_frame(frame))
			{
				return true;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return false;
	}
}

TEST(CAN_API2_WINDOWS_PLUGIN_TESTS, ValidatesConfiguration)
{
	CANAPI2WindowsPlugin plugin;

	EXPECT_EQ("PCANLight_USB", plugin.get_net_name());
	EXPECT_EQ("AgIsoStack", plugin.get_client_name());
	EXPECT_EQ(250000, plugin.get_bitrate());
	EXPECT_TRUE(plugin.get_create_missing_net());
	EXPECT_EQ(31, plugin.get_preferred_net_handle());
	EXPECT_EQ(CANAPI2WindowsPlugin::DeviceType::Virtual, plugin.get_device_type());
	EXPECT_EQ("pcan_virtual", plugin.get_device_name());
	EXPECT_EQ("PEAK PCAN Virtual (PCANLight_USB)", plugin.get_name());

	EXPECT_FALSE(plugin.configure("", 250000, true, 31));
	EXPECT_FALSE(plugin.configure("A_CAN_API2_NET_NAME_THAT_IS_TOO_LONG", 250000, true, 31));
	EXPECT_FALSE(plugin.configure(std::string("Bad\0Name", 8), 250000, true, 31));
	EXPECT_FALSE(plugin.configure("ValidNet", 123456, true, 31));
	EXPECT_FALSE(plugin.configure("ValidNet", 250000, true, 0));
	EXPECT_FALSE(plugin.configure("ValidNet", 250000, true, 33));

	EXPECT_TRUE(plugin.configure("ValidNet", 500000, false, 12));
	EXPECT_EQ("ValidNet", plugin.get_net_name());
	EXPECT_EQ(500000, plugin.get_bitrate());
	EXPECT_FALSE(plugin.get_create_missing_net());
	EXPECT_EQ(12, plugin.get_preferred_net_handle());

	CANAPI2WindowsPlugin usbPlugin("PCANLight_USB",
	                               "AgIsoUSBTest",
	                               CANAPI2WindowsPlugin::DEFAULT_BITRATE,
	                               true,
	                               CANAPI2WindowsPlugin::DEFAULT_NET_HANDLE,
	                               CANAPI2WindowsPlugin::DeviceType::USB);
	EXPECT_EQ(CANAPI2WindowsPlugin::DeviceType::USB, usbPlugin.get_device_type());
	EXPECT_EQ("pcan_usb", usbPlugin.get_device_name());
	EXPECT_EQ("PEAK PCAN USB (PCANLight_USB)", usbPlugin.get_name());
	EXPECT_FALSE(usbPlugin.get_create_missing_net());

	EXPECT_TRUE(usbPlugin.configure("USBNet", 250000, true, 12));
	EXPECT_EQ("USBNet", usbPlugin.get_net_name());
	EXPECT_FALSE(usbPlugin.get_create_missing_net());
}

TEST(CAN_API2_WINDOWS_PLUGIN_TESTS, ExchangesStandardAndExtendedFramesOnVirtualNet)
{
	const std::string netName = "AgIso" + std::to_string(GetCurrentProcessId());
	CANAPI2WindowsPlugin first(netName, "AgIsoTestFirst");
	CANAPI2WindowsPlugin second(netName, "AgIsoTestSecond");

	first.open();
	if (!first.get_is_valid())
	{
		GTEST_SKIP() << "The installed x64 PEAK CAN-API 2 virtual driver is unavailable.";
	}

	second.open();
	ASSERT_TRUE(second.get_is_valid());
	EXPECT_FALSE(first.configure("AnotherNet"));

	CANMessageFrame standardFrame{};
	standardFrame.identifier = 0x613;
	standardFrame.dataLength = 3;
	standardFrame.data[0] = 0x10;
	standardFrame.data[1] = 0x20;
	standardFrame.data[2] = 0x30;
	standardFrame.isExtendedFrame = false;
	ASSERT_TRUE(first.write_frame(standardFrame));

	CANMessageFrame receivedStandardFrame{};
	ASSERT_TRUE(receive_with_timeout(second,
	                                 receivedStandardFrame,
	                                 std::chrono::milliseconds(1000)));
	EXPECT_EQ(standardFrame.identifier, receivedStandardFrame.identifier);
	EXPECT_EQ(standardFrame.dataLength, receivedStandardFrame.dataLength);
	EXPECT_EQ(0, std::memcmp(standardFrame.data, receivedStandardFrame.data, standardFrame.dataLength));
	EXPECT_FALSE(receivedStandardFrame.isExtendedFrame);
	EXPECT_GT(receivedStandardFrame.timestamp_us, 0);

	CANMessageFrame extendedFrame{};
	extendedFrame.identifier = 0x18FF50E5;
	extendedFrame.dataLength = 8;
	extendedFrame.isExtendedFrame = true;
	for (std::uint8_t i = 0; i < extendedFrame.dataLength; ++i)
	{
		extendedFrame.data[i] = static_cast<std::uint8_t>(i + 1);
	}
	ASSERT_TRUE(second.write_frame(extendedFrame));

	CANMessageFrame receivedExtendedFrame{};
	ASSERT_TRUE(receive_with_timeout(first,
	                                 receivedExtendedFrame,
	                                 std::chrono::milliseconds(1000)));
	EXPECT_EQ(extendedFrame.identifier, receivedExtendedFrame.identifier);
	EXPECT_EQ(extendedFrame.dataLength, receivedExtendedFrame.dataLength);
	EXPECT_EQ(0, std::memcmp(extendedFrame.data, receivedExtendedFrame.data, extendedFrame.dataLength));
	EXPECT_TRUE(receivedExtendedFrame.isExtendedFrame);
	EXPECT_GT(receivedExtendedFrame.timestamp_us, 0);

	CANMessageFrame invalidFrame{};
	invalidFrame.identifier = 0x800;
	invalidFrame.isExtendedFrame = false;
	EXPECT_FALSE(first.write_frame(invalidFrame));
	invalidFrame.identifier = 0x123;
	invalidFrame.dataLength = 9;
	EXPECT_FALSE(first.write_frame(invalidFrame));

	second.close();
	first.close();
	EXPECT_FALSE(second.get_is_valid());
	EXPECT_FALSE(first.get_is_valid());
}
