//================================================================================================
/// @file vt_server_tests.cpp
///
/// @brief Unit tests for the VirtualTerminalServer class.
/// @author Ehud Frank
///
/// @copyright 2026 The Open-Agriculture Developers
//================================================================================================
#include <gtest/gtest.h>

#include "isobus/isobus/can_general_parameter_group_numbers.hpp"
#include "isobus/isobus/isobus_virtual_terminal_server.hpp"

#include "helpers/control_function_helpers.hpp"
#include "helpers/messaging_helpers.hpp"
#include "helpers/test_fixture.hpp"

using namespace isobus;

class DerivedTestVTServer : public VirtualTerminalServer
{
public:
	explicit DerivedTestVTServer(std::shared_ptr<InternalControlFunction> controlFunctionToUse) :
	  VirtualTerminalServer(controlFunctionToUse)
	{
	}

	bool get_is_enough_memory(std::uint32_t) const override
	{
		return true;
	}

	VTVersion get_version() const override
	{
		return VTVersion::Version4;
	}

	std::uint8_t get_number_of_navigation_soft_keys() const override
	{
		return 0;
	}

	std::uint8_t get_soft_key_descriptor_x_pixel_width() const override
	{
		return 60;
	}

	std::uint8_t get_soft_key_descriptor_y_pixel_height() const override
	{
		return 60;
	}

	std::uint8_t get_number_of_possible_virtual_soft_keys_in_soft_key_mask() const override
	{
		return 64;
	}

	std::uint8_t get_number_of_physical_soft_keys() const override
	{
		return 6;
	}

	std::uint16_t get_data_mask_area_size_x_pixels() const override
	{
		return 480;
	}

	std::uint16_t get_data_mask_area_size_y_pixels() const override
	{
		return 480;
	}

	void suspend_working_set(std::shared_ptr<VirtualTerminalServerManagedWorkingSet>) override
	{
	}

	SupportedWideCharsErrorCode get_supported_wide_chars(std::uint8_t,
	                                                     std::uint16_t,
	                                                     std::uint16_t,
	                                                     std::uint8_t &,
	                                                     std::vector<std::uint8_t> &) override
	{
		return SupportedWideCharsErrorCode::AnyOtherError;
	}

	std::vector<std::array<std::uint8_t, 7>> get_versions(NAME) override
	{
		return {};
	}

	std::vector<std::uint8_t> get_supported_objects() const override
	{
		return {};
	}

	std::vector<std::uint8_t> load_version(const std::vector<std::uint8_t> &, NAME) override
	{
		return {};
	}

	bool save_version(const std::vector<std::uint8_t> &, const std::vector<std::uint8_t> &, NAME) override
	{
		return false;
	}

	bool delete_version(const std::vector<std::uint8_t> &, NAME) override
	{
		return false;
	}

	bool delete_all_versions(NAME) override
	{
		return false;
	}

	bool delete_object_pool(NAME) override
	{
		return false;
	}

	void receive(const CANMessage &message)
	{
		process_rx_message(message, this);
	}

	std::shared_ptr<VirtualTerminalServerManagedWorkingSet> get_managed_working_set() const
	{
		return managedWorkingSetList.empty() ? nullptr : managedWorkingSetList.back();
	}

	void disconnect_working_sets()
	{
		managedWorkingSetList.clear();
	}
};

class VirtualTerminalServerTest : public AgIsoStackTestFixture
{
protected:
	void SetUp() override
	{
		AgIsoStackTestFixture::SetUp();
		serverECU = test_helpers::create_mock_internal_control_function(0x26);
		clientECU = test_helpers::create_mock_control_function(0x81);
	}

	CANMessage create_client_message(std::initializer_list<std::uint8_t> data) const
	{
		return test_helpers::create_message(7, static_cast<std::uint32_t>(CANLibParameterGroupNumber::ECUtoVirtualTerminal), serverECU, clientECU, data);
	}

	CANMessage create_get_memory_message(std::uint32_t requiredMemory) const
	{
		return create_client_message({ 0xC0,
		                               0xFF,
		                               static_cast<std::uint8_t>(requiredMemory & 0xFF),
		                               static_cast<std::uint8_t>((requiredMemory >> 8) & 0xFF),
		                               static_cast<std::uint8_t>((requiredMemory >> 16) & 0xFF),
		                               static_cast<std::uint8_t>((requiredMemory >> 24) & 0xFF),
		                               0xFF,
		                               0xFF });
	}

	CANMessage create_working_set_maintenance_message(bool initiating) const
	{
		return create_client_message({ 0xFF, static_cast<std::uint8_t>(initiating ? 0x01 : 0x00), 0x04, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF });
	}

	CANMessage create_object_pool_transfer_message(std::uint32_t poolBytes) const
	{
		std::vector<std::uint8_t> data(poolBytes + 1, 0x00);
		data.at(0) = 0x11;
		return test_helpers::create_message(7, static_cast<std::uint32_t>(CANLibParameterGroupNumber::ECUtoVirtualTerminal), serverECU, clientECU, data.data(), static_cast<std::uint32_t>(data.size()));
	}

	std::shared_ptr<InternalControlFunction> serverECU;
	std::shared_ptr<ControlFunction> clientECU;
};

TEST_F(VirtualTerminalServerTest, ObjectPoolUploadProgressUsesMemoryRequestedWhileConnected)
{
	DerivedTestVTServer server(serverECU);

	server.receive(create_working_set_maintenance_message(true));
	server.receive(create_get_memory_message(1000));
	auto workingSet = server.get_managed_working_set();
	ASSERT_NE(nullptr, workingSet);
	EXPECT_FLOAT_EQ(0.0f, workingSet->iop_load_percentage());

	server.receive(create_object_pool_transfer_message(250));
	EXPECT_FLOAT_EQ(25.0f, workingSet->iop_load_percentage());
}

TEST_F(VirtualTerminalServerTest, ObjectPoolUploadProgressUsesMemoryRequestedBeforeConnecting)
{
	DerivedTestVTServer server(serverECU);

	// The client sizes its pool before it initiates working set maintenance
	server.receive(create_get_memory_message(1000));
	ASSERT_EQ(nullptr, server.get_managed_working_set());

	server.receive(create_working_set_maintenance_message(true));
	auto workingSet = server.get_managed_working_set();
	ASSERT_NE(nullptr, workingSet);

	server.receive(create_object_pool_transfer_message(100));
	EXPECT_FLOAT_EQ(10.0f, workingSet->iop_load_percentage());
}

TEST_F(VirtualTerminalServerTest, ObjectPoolUploadProgressSurvivesReconnectAfterMemoryRequest)
{
	DerivedTestVTServer server(serverECU);

	// Matches the sequence seen from an implement: probe the VT version, drop out,
	// request memory while disconnected, then reconnect and upload
	server.receive(create_working_set_maintenance_message(true));
	server.receive(create_get_memory_message(0));
	server.disconnect_working_sets();
	server.receive(create_get_memory_message(1000));
	server.receive(create_working_set_maintenance_message(true));
	auto workingSet = server.get_managed_working_set();
	ASSERT_NE(nullptr, workingSet);

	server.receive(create_object_pool_transfer_message(100));
	EXPECT_FLOAT_EQ(10.0f, workingSet->iop_load_percentage());
}

TEST_F(VirtualTerminalServerTest, VersionQueryDoesNotClearRequestedMemory)
{
	DerivedTestVTServer server(serverECU);

	server.receive(create_working_set_maintenance_message(true));
	server.receive(create_get_memory_message(1000));
	server.receive(create_get_memory_message(0)); // Only asks for the VT version
	auto workingSet = server.get_managed_working_set();
	ASSERT_NE(nullptr, workingSet);

	server.receive(create_object_pool_transfer_message(500));
	EXPECT_FLOAT_EQ(50.0f, workingSet->iop_load_percentage());
}
