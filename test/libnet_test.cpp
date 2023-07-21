/*
* Copyright 2020 Comcast Cable Communications Management, LLC
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* SPDX-License-Identifier: Apache-2.0
*/
#include <iostream>
#include <string>

#include <algorithm>
#include <gtest/gtest.h>

#include "TestUtils.hpp"

#include <setjmp.h>

#define TAP_INTERFACE "tap0"

extern "C" {
    #include "libnet.h"
    #include "safec_lib_common.h"
}

extern std::vector<std::string> g_command_line_arg;
extern long counter;
extern sigjmp_buf point;

namespace defaultNS {
    const int testVlanId = 100;
    const std::string testBridgeName = "testBridge0000";
    const std::string testInterfaceName = "dummy0";
    const std::string testInterfaceType = "dummy";
    const std::string testVlanName = getVlanName(testInterfaceName, testVlanId);
    const std::string testInterfaceIpAddr = "192.168.2.2";
    const std::string testInterfaceNetMask = "255.255.255.0";
    const std::string ephimeralInterfaceName = "dummy1";
    const std::string ephimeralInterfaceType = "dummy";
    const std::string ephimeralInterfaceAddr = "192.168.2.2/24";
    const std::string tmpFile = "/tmp/gtest.log";
}

namespace testNS {
    int testVlanId = defaultNS::testVlanId;
    std::string testBridgeName = defaultNS::testBridgeName;
    std::string testInterfaceName = defaultNS::testInterfaceName;
    std::string testInterfaceType = defaultNS::testInterfaceType;
    std::string testVlanName = getVlanName(testInterfaceName, testVlanId);
    std::string testInterfaceIpAddr = defaultNS::testInterfaceIpAddr;
    std::string testInterfaceNetMask = defaultNS::testInterfaceNetMask;
    std::string ephimeralInterfaceName = defaultNS::ephimeralInterfaceName;
    std::string ephimeralInterfaceType = defaultNS::ephimeralInterfaceType;
    std::string ephimeralInterfaceAddr = defaultNS::ephimeralInterfaceAddr;
    std::string tmpFile = defaultNS::tmpFile;
};

void setNamespaceVars()
{
    uint32_t argListSize = g_command_line_arg.size();

    if (argListSize >= 1)
    {
        testNS::testInterfaceName = g_command_line_arg[0];

        if (argListSize == 1)
        {
            std::cerr << "Need to provide interface type along with interface name\n";
            exit(1);
        }

        testNS::testInterfaceType = g_command_line_arg[1];
        testNS::testVlanName = getVlanName(testNS::testInterfaceName, testNS::testVlanId);
    }

    if (argListSize >= 3)
    {
        testNS::testVlanId = std::stoi(g_command_line_arg[2]);
        testNS::testVlanName = getVlanName(testNS::testInterfaceName, testNS::testVlanId);
    }

    if (argListSize >= 4)
    {
        testNS::testBridgeName = g_command_line_arg[3];
    }

    if (argListSize >= 5)
    {
        testNS::ephimeralInterfaceName = g_command_line_arg[4];
    }

    if (argListSize >= 6)
    {
        testNS::ephimeralInterfaceType = g_command_line_arg[5];
    }
}

// ------------------------------ //
// ----- TEST HELPER START ------ //
// ------------------------------ //

/**
 * Creates a test interface of type interfaceType
 */
void createTestInterface(const std::string& interfaceName, const std::string& interfaceType)
{
    ASSERT_EQ(true, createInterfaceSystemCommand(interfaceName, interfaceType)) << "Failed to create interface " + interfaceName + " of type " + interfaceType + "\n";
}

/**
 * Creates a test interface with ipAddress. If ipAddress is an empty string
 * then default to 192.168.2.2/24
 */
void createTestInterface(const std::string& interfaceName, const std::string& interfaceType, const std::string& ipAddress)
{
    if (ipAddress == "")
    {
        // We set it at ip 192.168.2.2 and subnet 255.255.255.0 (defaults)
        ASSERT_EQ(true, createInterfaceSystemCommand(interfaceName, interfaceType, "192.168.2.2/24")) << "Failed to create / set ip address for " + interfaceName + " of type " + interfaceType + "\n";
    }
    else
    {
        ASSERT_EQ(true, createInterfaceSystemCommand(interfaceName, interfaceType, ipAddress)) << "Failed to create / set ip address for " + interfaceName + " of type " + interfaceType + "\n";
    }
}

void destroyTestInterface(const std::string& interfaceName)
{
    int status = -1;
    executeSystemCommand("ip link delete " + interfaceName, &status);
    ASSERT_EQ(WEXITSTATUS(status), EXIT_SUCCESS) << "Failed to delete interface " + interfaceName + "\n";
}

bool interfaceExists(const std::string& interfaceName)
{
    // Verify by checking contents of /sys/class/net dir
    // The interface must be listed
    auto interfaces = listDirectoryFiles("/sys/class/net");
    return (std::find(interfaces.begin(), interfaces.end(), interfaceName) != interfaces.end());
}

class CoreNetLibTestSuite: public ::testing::Test {
public:
    static void SetUpTestSuite()
    {
        setNamespaceVars();

        if (interfaceExists(testNS::testInterfaceName) == false)
        {
            // Doesn't exist. Let's try creating
            std::cout << "Creating interface " << testNS::testInterfaceName << " of type " << testNS::testInterfaceType << std::endl;
            createTestInterface(testNS::testInterfaceName, testNS::testInterfaceType, testNS::testInterfaceIpAddr);
            ASSERT_EQ(interfaceExists(testNS::testInterfaceName), true) << testNS::testInterfaceName << " should be created";
        }
    }

    static void TearDownTestSuite()
    {
        // Any dummy interfaces will be cleaned up by the kernel automatically
    }

    virtual void SetUp()
    {
        errno_t rc = -1;
        // Per test fixture
        counter += 2;
        rc = setjmp(point);
        ERR_CHK(rc);
    }

    virtual void TearDown()
    {
    }
};

// ------------------------------ //
// --------- TEST START --------- //
// ------------------------------ //

TEST_F(CoreNetLibTestSuite, VlanCreate)
{
    std::string interfaceName = testNS::testInterfaceName;
    int vlanId = testNS::testVlanId;

    ASSERT_EQ(0, vlan_create(interfaceName.c_str(), vlanId));
}

TEST_F(CoreNetLibTestSuite, VlanDelete)
{
    std::string vlanName = testNS::testVlanName;

    ASSERT_EQ(0, vlan_delete(vlanName.c_str()));
}

TEST_F(CoreNetLibTestSuite, VlanCreateTwice)
{
    std::string interfaceName = testNS::testInterfaceName;
    int vlanId = testNS::testVlanId;

    EXPECT_EQ(0, vlan_create(interfaceName.c_str(), vlanId));
    EXPECT_NE(0, vlan_create(interfaceName.c_str(), vlanId));

    // TODO: Add coupling with system calls
    // Vlan shouldn't get created again via sys cmd
    ASSERT_EQ(createVlanSystemCommand(interfaceName, vlanId), false);
}

TEST_F(CoreNetLibTestSuite, VlanDeleteTwice)
{
    std::string vlanName = testNS::testVlanName;

    EXPECT_EQ(0, vlan_delete(vlanName.c_str()));
    EXPECT_NE(0, vlan_delete(vlanName.c_str()));

    ASSERT_EQ(deleteVlanSystemCommand(vlanName), false);
}

TEST_F(CoreNetLibTestSuite, VlanCreateAndDeleteTwice)
{
    std::string interfaceName = testNS::testInterfaceName;
    int vlanId = testNS::testVlanId;
    std::string vlanName = testNS::testVlanName;

    // Use EXPECT_* when using create family APIs
    // and ASSERT_* when using delete family APIs
    EXPECT_EQ(0, vlan_create(interfaceName.c_str(), vlanId));
    EXPECT_EQ(0, vlan_delete(vlanName.c_str()));

    EXPECT_EQ(0, vlan_create(interfaceName.c_str(), vlanId));
    EXPECT_EQ(0, vlan_delete(vlanName.c_str()));

    ASSERT_EQ(deleteVlanSystemCommand(vlanName), false);
}

TEST_F(CoreNetLibTestSuite, VlanCreateValidVlanID)
{
    std::string interfaceName = testNS::testInterfaceName;
    int vlanId = 4094;
    std::string vlanName = getVlanName(interfaceName, vlanId);

    EXPECT_EQ(0, vlan_create(interfaceName.c_str(), vlanId));
    EXPECT_EQ(0, vlan_delete(vlanName.c_str()));

    ASSERT_EQ(deleteVlanSystemCommand(vlanName), false);
}

TEST_F(CoreNetLibTestSuite, VlanCreateInvalidVlanID)
{
    std::string interfaceName = testNS::testInterfaceName;
    int vlanId = 4095;
    std::string vlanName = getVlanName(interfaceName, vlanId);

    EXPECT_NE(0, vlan_create(interfaceName.c_str(), vlanId)) << vlanId << " VlanID should not be created";
    ASSERT_NE(0, vlan_delete(vlanName.c_str()));

    vlanId = -1;
    EXPECT_NE(0, vlan_create(interfaceName.c_str(), vlanId)) << vlanId << " VlanID should not be created";
    EXPECT_NE(0, vlan_delete(vlanName.c_str()));

    ASSERT_EQ(deleteVlanSystemCommand(vlanName), false);
}

TEST_F(CoreNetLibTestSuite, BridgeCreate)
{
    std::string bridgeName = testNS::testBridgeName;

    EXPECT_EQ(0, bridge_create(bridgeName.c_str()));
    ASSERT_EQ(createBridgeSystemCommand(bridgeName), false);
}

TEST_F(CoreNetLibTestSuite, BridgeDelete)
{
    std::string bridgeName = testNS::testBridgeName;

    EXPECT_EQ(0, bridge_delete(bridgeName.c_str()));
    ASSERT_EQ(deleteBridgeSystemCommand(bridgeName), false);
}

TEST_F(CoreNetLibTestSuite, BridgeCreateTwice)
{
    std::string bridgeName = testNS::testBridgeName;

    EXPECT_EQ(0, bridge_create(bridgeName.c_str()));
    EXPECT_NE(0, bridge_create(bridgeName.c_str()));

    ASSERT_EQ(createBridgeSystemCommand(bridgeName), false);
}

TEST_F(CoreNetLibTestSuite, BridgeDeleteTwice)
{
    std::string bridgeName = testNS::testBridgeName;

    EXPECT_EQ(0, bridge_delete(bridgeName.c_str()));
    EXPECT_NE(0, bridge_delete(bridgeName.c_str()));

    ASSERT_EQ(deleteBridgeSystemCommand(bridgeName), false);
}

TEST_F(CoreNetLibTestSuite, BridgeCreateAndDeleteTwice)
{
    std::string bridgeName = testNS::testBridgeName;

    EXPECT_EQ(0, bridge_create(bridgeName.c_str()));
    EXPECT_EQ(0, bridge_delete(bridgeName.c_str()));

    EXPECT_EQ(0, bridge_create(bridgeName.c_str()));
    EXPECT_EQ(0, bridge_delete(bridgeName.c_str()));

    ASSERT_EQ(deleteBridgeSystemCommand(bridgeName), false);
}

// ------------------------------------ //
// --------- BRIDGE SUITE ------------ //
// ----------------------------------- //

class BridgeTestSuite: public CoreNetLibTestSuite {
public:
    static void SetUpTestSuite()
    {
        CoreNetLibTestSuite::SetUpTestSuite();

        // Delete any existing bridge create before this suite
        deleteBridgeSystemCommand(testNS::testBridgeName);
    }

    static void TearDownTestSuite()
    {
        deleteBridgeSystemCommand(testNS::testBridgeName);

        CoreNetLibTestSuite::TearDownTestSuite();
    }

    virtual void SetUp()
    {
        CoreNetLibTestSuite::SetUp();

        ASSERT_EQ(true, createBridgeSystemCommand(testNS::testBridgeName));
    }

    virtual void TearDown()
    {
        ASSERT_EQ(true, deleteBridgeSystemCommand(testNS::testBridgeName));

        CoreNetLibTestSuite::TearDown();
    }
};

void bridgeGetInfoSubroutine(const std::string& bridgeName, struct bridge_info& info)
{
    // Using const_cast<char *> for the sake of compatibility with the C API
    // i.e assuming the API doesn't modify the str
    ASSERT_EQ(0, bridge_get_info(const_cast<char *>(bridgeName.c_str()), &info));
}

TEST_F(BridgeTestSuite, BridgeGetInfo)
{
    std::string bridgeName = testNS::testBridgeName;
    struct bridge_info info = {0};

    bridgeGetInfoSubroutine(bridgeName, info);
}

void bridgeFreeInfoSubroutine(const std::string& bridgeName, struct bridge_info& info)
{
    ASSERT_NO_FATAL_FAILURE(bridge_free_info(&info));

    EXPECT_EQ(0, info.slave_count);
    EXPECT_TRUE(info.link_cache == NULL) << "link_cache should be NULL";

    for (int i=0; i<MAX_SLAVE_COUNT; i++)
    {
        EXPECT_TRUE(info.slave_name[i] == NULL) << "slave_name[" << i << "] should be NULL";
    }
}

TEST_F(BridgeTestSuite, BridgeFreeInfo)
{
    std::string bridgeName = testNS::testBridgeName;
    struct bridge_info info = {0};

    bridgeFreeInfoSubroutine(bridgeName, info);
}

TEST_F(BridgeTestSuite, BridgeFreeInfoSegFault)
{
    std::string bridgeName = testNS::testBridgeName;
    struct bridge_info info; // Random garbage value should crash the API as it doesn't initialize slave_count to 0

    EXPECT_EQ(0, bridge_get_info(const_cast<char *>(bridgeName.c_str()), &info));

    // It shouldn't segfault
    bridgeFreeInfoSubroutine(bridgeName, info);
    // ASSERT_EXIT((bridge_free_info(&info), exit(0)), ::testing::KilledBySignal(SIGSEGV), ".*");
}

TEST_F(BridgeTestSuite, BridgeSetStp)
{
    std::string bridgeName = testNS::testBridgeName;

    char val[10] = {0};

    strcpy(val, "off");
    EXPECT_EQ(0, bridge_set_stp(bridgeName.c_str(), val));
    EXPECT_EQ(STP_DISABLED, getBridgeSTPStateSystemCommand(bridgeName));

    // bridge set STP should still work in the same way
    EXPECT_EQ(true, setBridgeSTPStateSystemCommand(bridgeName, false));
    EXPECT_EQ(STP_DISABLED, getBridgeSTPStateSystemCommand(bridgeName));

    strcpy(val, "on");
    EXPECT_EQ(0, bridge_set_stp(bridgeName.c_str(), val));
    EXPECT_EQ(STP_LISTENING, getBridgeSTPStateSystemCommand(bridgeName));

    strcpy(val, "invalid");
    EXPECT_LT(bridge_set_stp(bridgeName.c_str(), val), 0);
    EXPECT_EQ(STP_LISTENING, getBridgeSTPStateSystemCommand(bridgeName));

    // bridge set STP should still work in the same way
    EXPECT_EQ(true, setBridgeSTPStateSystemCommand(bridgeName, true));
    EXPECT_EQ(STP_LISTENING, getBridgeSTPStateSystemCommand(bridgeName));
}

TEST_F(BridgeTestSuite, InterfaceAddToBridgeValid)
{
    std::string bridgeName = testNS::testBridgeName; // bridge is valid
    std::string interfaceName = testNS::ephimeralInterfaceName; // interface also valid (we need to create a virtual link to enslave the bridge)

    createInterfaceSystemCommand(interfaceName, testNS::testInterfaceType);

    EXPECT_EQ(0, interface_add_to_bridge(bridgeName.c_str(), interfaceName.c_str()));

    EXPECT_EQ(false, interfaceAddToBridgeSystemCommand(interfaceName, bridgeName));

    destroyTestInterface(interfaceName);
}

TEST_F(BridgeTestSuite, InvalidInterfaceAddToInvalidBridge)
{
    std::string bridgeName = "bridgeInvalid0000"; // bridge invalid
    std::string interfaceName = "invalid0"; // interface also invalid

    ASSERT_NE(0, interface_add_to_bridge(bridgeName.c_str(), interfaceName.c_str()));

    EXPECT_EQ(false, interfaceAddToBridgeSystemCommand(interfaceName, bridgeName));
}

TEST_F(BridgeTestSuite, InterfaceAddToInvalidBridge)
{
    std::string bridgeName = "bridgeInvalid0000"; // bridge is invalid
    std::string interfaceName = testNS::ephimeralInterfaceName; // interface is valid

    createInterfaceSystemCommand(interfaceName, testNS::testInterfaceType);

    EXPECT_NE(0, interface_add_to_bridge(bridgeName.c_str(), interfaceName.c_str()));

    EXPECT_EQ(false, interfaceAddToBridgeSystemCommand(interfaceName, bridgeName));

    destroyTestInterface(interfaceName);
}

TEST_F(BridgeTestSuite, InvalidInterfaceAddToBridge)
{
    std::string bridgeName = testNS::testBridgeName; // bridge will be created and is valid
    std::string interfaceName = "invalid0"; // interface is invalid

    EXPECT_NE(0, interface_add_to_bridge(bridgeName.c_str(), interfaceName.c_str()));

    EXPECT_EQ(false, interfaceAddToBridgeSystemCommand(interfaceName, bridgeName));
}

void interfaceAddToBridgeSubRoutine(const std::string& bridgeName, const std::string& interfaceName)
{
    EXPECT_EQ(0, interface_add_to_bridge(bridgeName.c_str(), interfaceName.c_str())) << "Failed to add interface " << interfaceName << " to bridge";
}

void interfaceRemoveFromBridgeSubRoutine(const std::string& bridgeName, const std::string& interfaceName)
{
    EXPECT_EQ(0, interface_remove_from_bridge(interfaceName.c_str()));
}

TEST_F(BridgeTestSuite, InterfaceRemoveFromBridgeValid)
{
    std::string bridgeName = testNS::testBridgeName;
    std::string interfaceName = testNS::ephimeralInterfaceName;

    createInterfaceSystemCommand(interfaceName, testNS::testInterfaceType);

    interfaceAddToBridgeSubRoutine(bridgeName, interfaceName);

    EXPECT_EQ(0, interface_remove_from_bridge(interfaceName.c_str()));

    EXPECT_EQ(false, interfaceRemoveFromBridgeSystemCommand(interfaceName, bridgeName));

    destroyTestInterface(interfaceName);
}

TEST_F(BridgeTestSuite, InvalidInterfaceRemoveFromInvalidBridge)
{
    std::string bridgeName = "bridgeInValid0000"; // Invalid bridge
    std::string interfaceName = "invalid0"; // invalid interface

    EXPECT_NE(0, interface_remove_from_bridge(interfaceName.c_str()));
    EXPECT_EQ(false, interfaceRemoveFromBridgeSystemCommand(interfaceName, bridgeName));
}

TEST_F(BridgeTestSuite, InvalidInterfaceRemoveFromBridge)
{
    std::string bridgeName = testNS::testBridgeName; // valid bridge
    std::string interfaceName = "invalid0"; // invalid interface

    EXPECT_NE(0, interface_remove_from_bridge(interfaceName.c_str()));

    EXPECT_EQ(false, interfaceRemoveFromBridgeSystemCommand(interfaceName, bridgeName));
}

// ------------------------------------ //
// --------- INTERFACE SUITE --------- //
// ----------------------------------- //

class InterfaceTestSuite: public CoreNetLibTestSuite {
public:
    static void SetUpTestSuite()
    {
        CoreNetLibTestSuite::SetUpTestSuite();

        // Destroy the ephimeral interface if it's created already
        deleteInterfaceSystemCommand(testNS::ephimeralInterfaceName);
    }

    static void TearDownTestSuite()
    {
        deleteInterfaceSystemCommand(testNS::ephimeralInterfaceName);

        CoreNetLibTestSuite::TearDownTestSuite();
    }

    virtual void SetUp()
    {
        CoreNetLibTestSuite::SetUp();

        createInterfaceSystemCommand(testNS::ephimeralInterfaceName, testNS::ephimeralInterfaceType, testNS::ephimeralInterfaceAddr);
    }

    virtual void TearDown()
    {
        CoreNetLibTestSuite::TearDown();

        deleteInterfaceSystemCommand(testNS::ephimeralInterfaceName);
    }
};

TEST_F(InterfaceTestSuite, InterfaceUp)
{
    std::string bridgeName = testNS::testBridgeName;
    std::string interfaceName = testNS::testInterfaceName;

    int prevState = linkStatusUpOrDownSystemCommand(interfaceName);

    EXPECT_EQ(0, interface_up(const_cast<char *> (interfaceName.c_str()))) << "Failed to set " << interfaceName << " as UP";

    // EXPECT_EQ(1, linkStatusUpOrDownSystemCommand(interfaceName));

    auto cmdOutput = getLinkStateSystemCommand(interfaceName);
    std::cout << "InterfaceUp: Interface state = " << cmdOutput << std::endl;

    if (prevState == 0)
    {
        ASSERT_EQ(true, setInterfaceDownSystemCommand(interfaceName));
    }
    else if (prevState == 1)
    {
        ASSERT_EQ(true, setInterfaceUpSystemCommand(interfaceName));
    }
}

TEST_F(InterfaceTestSuite, InterfaceUpFail)
{
    std::string interfaceName = "invalid0"; // invalid interface

    EXPECT_NE(0, interface_up(const_cast<char *> (interfaceName.c_str()))) << "Failed to set " << interfaceName << " as UP";
    ASSERT_EQ(-1, linkStatusUpOrDownSystemCommand(interfaceName));
}

TEST_F(InterfaceTestSuite, InterfaceDown)
{
    std::string interfaceName = testNS::testInterfaceName;

    int prevState = linkStatusUpOrDownSystemCommand(interfaceName);

    EXPECT_EQ(0, interface_down(const_cast<char *> (interfaceName.c_str()))) << "Failed to set " << interfaceName << " as DOWN";
 
    EXPECT_EQ(0, linkStatusUpOrDownSystemCommand(interfaceName));

    if (prevState == 0)
    {
        ASSERT_EQ(true, setInterfaceDownSystemCommand(interfaceName));
    }
    else if (prevState == 1)
    {
        ASSERT_EQ(true, setInterfaceUpSystemCommand(interfaceName));
    }
}

TEST_F(InterfaceTestSuite, InterfaceDownFail)
{
    std::string interfaceName = "invalid0"; // invalid interface

    EXPECT_NE(0, interface_down(const_cast<char *> (interfaceName.c_str()))) << "Failed to set " << interfaceName << " as DOWN";
    ASSERT_EQ(-1, linkStatusUpOrDownSystemCommand(interfaceName));
}

TEST_F(InterfaceTestSuite, InterfaceSetMtu)
{
    std::string interfaceName = testNS::ephimeralInterfaceName;

    createInterfaceSystemCommand(interfaceName, testNS::testInterfaceType);

    char val[10] = {0};

    strcpy(val, "1700");
    EXPECT_EQ(0, interface_set_mtu(interfaceName.c_str(), val));

    EXPECT_EQ(1700, interfaceGetMTUSystemCommand(interfaceName));

    strcpy(val, "65535");
    EXPECT_EQ(0, interface_set_mtu(interfaceName.c_str(), val));

    EXPECT_EQ(65535, interfaceGetMTUSystemCommand(interfaceName));

    destroyTestInterface(interfaceName);
}

TEST_F(InterfaceTestSuite, InterfaceSetMtuFail)
{
    std::string interfaceName = testNS::ephimeralInterfaceName;

    char val[10] = {0};

    strcpy(val, "-2");

    // Default MTU value during creation is 1500 != -2
    EXPECT_EQ(0, interface_set_mtu(interfaceName.c_str(), val));

    EXPECT_NE(-2, interfaceGetMTUSystemCommand(interfaceName));

    strcpy(val, "65537");
    EXPECT_EQ(0, interface_set_mtu(interfaceName.c_str(), val));

    EXPECT_NE(65537, interfaceGetMTUSystemCommand(interfaceName));
}

TEST_F(InterfaceTestSuite, InterfaceGetMac)
{
    std::string interfaceName = testNS::testInterfaceName;
    char mac[65] = {0};
    int sz = sizeof(mac);

    EXPECT_EQ(0, interface_get_mac(interfaceName.c_str(), mac, sz));

    auto macResult = interfaceGetMacSystemCommand(interfaceName);
    ASSERT_STREQ(macResult.c_str(), mac);

    // std::cout << "Interface: " << interfaceName << " has MAC: " << mac << std::endl;
}

TEST_F(InterfaceTestSuite, InterfaceGetMacFail)
{
    std::string interfaceName = "invalid0";
    char mac[65] = {0};
    int sz = sizeof(mac);

    ASSERT_NE(0, interface_get_mac(interfaceName.c_str(), mac, sz));
}

TEST_F(InterfaceTestSuite, InterfaceSetMac)
{
    std::string interfaceName = testNS::ephimeralInterfaceName;
    char mac[65] = {0};
    strcpy(mac, "02:ff:ff:ff:ff:ff"); // dummy MAC address for our virtual interface

    EXPECT_EQ(0, interface_set_mac(interfaceName.c_str(), mac));

    auto macResult = interfaceGetMacSystemCommand(interfaceName);
    EXPECT_STREQ(macResult.c_str(), mac);
}

TEST_F(InterfaceTestSuite, InterfaceSetMacInvalidInterface)
{
    std::string interfaceName = "invalid0";
    char mac[65] = {0};
    strcpy(mac, "02:ff:ff:ff:ff:ff");

    EXPECT_NE(0, interface_set_mac(interfaceName.c_str(), mac));

    auto macResult = interfaceGetMacSystemCommand(interfaceName);
    ASSERT_STRNE(macResult.c_str(), mac);
}

TEST_F(InterfaceTestSuite, InterfaceSetMacInvalidFormat)
{
    std::string interfaceName = "invalid0";

    char mac[65] = {0};

    strcpy(mac, "02:ff:ff:ff:ff:");
    EXPECT_NE(0, interface_set_mac(interfaceName.c_str(), mac));

    strcpy(mac, "02:ff:ff:ff:ff:f");
    EXPECT_NE(0, interface_set_mac(interfaceName.c_str(), mac));

    strcpy(mac, "02:ff:ff:ff:ff:ff:gg");
    EXPECT_NE(0, interface_set_mac(interfaceName.c_str(), mac));

    auto macResult = interfaceGetMacSystemCommand(interfaceName);
    ASSERT_STRNE(macResult.c_str(), mac);
}

TEST_F(InterfaceTestSuite, InterfaceExist)
{
    // testNS::testInterfaceName must be valid
    std::string testIfName = testNS::ephimeralInterfaceName;

    // interface_exist() returns 1 if present
    ASSERT_EQ(0, interface_exist(testIfName.c_str()));

    // Verify via checking the list of interfaces
    ASSERT_TRUE(interfaceExists(testIfName));
}

TEST_F(InterfaceTestSuite, InterfaceDoesNotExist)
{
    // dummy invalid interface
    std::string testIfName = "invalid0";

    // interface_exist() returns 0 if absent
    ASSERT_EQ(-1, interface_exist(testIfName.c_str()));

    // Verify via checking the list of interfaces
    ASSERT_FALSE(interfaceExists(testIfName));
}

TEST_F(InterfaceTestSuite, InterfaceSetNetMaskValid)
{
    std::string interfaceName = testNS::ephimeralInterfaceName;
    std::string ipAddress = "192.0.20.20";
    std::string netmask = "255.255.0.0";

    // Delete the testInterface already created by this fixture in setup
    destroyTestInterface(interfaceName);

    createTestInterface(interfaceName, testNS::ephimeralInterfaceType, ipAddress);

    EXPECT_EQ(0, interface_set_netmask(interfaceName.c_str(), netmask.c_str()));
    EXPECT_STREQ(netmask.c_str(), interfaceGetNetmaskSystemCommand(interfaceName).c_str());

    netmask = "255.255.255.254";

    EXPECT_EQ(0, interface_set_netmask(interfaceName.c_str(), netmask.c_str()));
    EXPECT_STREQ(netmask.c_str(), interfaceGetNetmaskSystemCommand(interfaceName).c_str());

    netmask = "255.255.255.0";

    EXPECT_EQ(0, interface_set_netmask(interfaceName.c_str(), netmask.c_str()));
    EXPECT_STREQ(netmask.c_str(), interfaceGetNetmaskSystemCommand(interfaceName).c_str());
}

TEST_F(InterfaceTestSuite, InterfaceSetInvalidNetMask)
{
    std::string interfaceName = testNS::ephimeralInterfaceName;
    std::string ipAddress = "192.0.20.20";
    std::string netmask = "255.255.255"; // Invalid netmask

    // Delete the testInterface already created by this fixture in setup
    destroyTestInterface(interfaceName);

    createTestInterface(interfaceName, testNS::ephimeralInterfaceType, ipAddress);

    EXPECT_NE(0, interface_set_netmask(interfaceName.c_str(), netmask.c_str()));
    EXPECT_STRNE(netmask.c_str(), interfaceGetNetmaskSystemCommand(interfaceName).c_str());

    netmask = "255.255.255.0.1";

    EXPECT_NE(0, interface_set_netmask(interfaceName.c_str(), netmask.c_str()));
    EXPECT_STRNE(netmask.c_str(), interfaceGetNetmaskSystemCommand(interfaceName).c_str());
}

TEST_F(InterfaceTestSuite, InvalidInterfaceSetNetMask)
{
    std::string interfaceName = "invalid0";
    std::string netmask = "255.255.255.0"; // valid netmask

    EXPECT_NE(0, interface_set_netmask(interfaceName.c_str(), netmask.c_str()));
}

TEST_F(InterfaceTestSuite, InvalidInterfaceSetInvalidNetMask)
{
    std::string interfaceName = "invalid0";
    std::string netmask = "255.255.255.0.1"; // invalid netmask

    EXPECT_NE(0, interface_set_netmask(interfaceName.c_str(), netmask.c_str()));
}

TEST_F(InterfaceTestSuite, InterfaceGetIp)
{
    std::string interfaceName = testNS::ephimeralInterfaceName;

    std::string cmdOutput = interfaceGetIpSystemCommand(interfaceName);

    EXPECT_STREQ(interface_get_ip(interfaceName.c_str()), cmdOutput.c_str());
}

TEST_F(InterfaceTestSuite, InterfaceGetIpNull)
{
    std::string interfaceName = "invalid0";

    EXPECT_EQ(NULL, interface_get_ip(interfaceName.c_str()));
}

TEST_F(InterfaceTestSuite, InterfaceSetIp)
{
    std::string interfaceName = testNS::ephimeralInterfaceName;
    std::string ipAddress = "192.0.20.20";

    // Delete the testInterface already created by this fixture in setup
    destroyTestInterface(interfaceName);

    createTestInterface(interfaceName, testNS::ephimeralInterfaceType);

    EXPECT_EQ(0, interface_set_ip(interfaceName.c_str(), ipAddress.c_str()));

    EXPECT_STREQ(ipAddress.c_str(), interface_get_ip(interfaceName.c_str()));
}

TEST_F(InterfaceTestSuite, InterfaceSetIpWillFailIfAlreadyAssigned)
{
    std::string interfaceName = testNS::ephimeralInterfaceName;

    // Delete the testInterface already created by this fixture in setup
    destroyTestInterface(interfaceName);

    createTestInterface(interfaceName, testNS::ephimeralInterfaceType, "");

    std::string existingIpAddress = interface_get_ip(interfaceName.c_str());
    std::string newIpAddress = "192.0.20.20";

    EXPECT_EQ(0, interface_set_ip(interfaceName.c_str(), newIpAddress.c_str()));

    EXPECT_STRNE(newIpAddress.c_str(), interface_get_ip(interfaceName.c_str()));
    EXPECT_STREQ(existingIpAddress.c_str(), interface_get_ip(interfaceName.c_str()));
}

TEST_F(InterfaceTestSuite, InterfaceSetstats)
{
    std::string interfaceName = testNS::ephimeralInterfaceName;

    // Delete the testInterface already created by this fixture in setup
    destroyTestInterface(interfaceName);

    createTestInterface(interfaceName, testNS::ephimeralInterfaceType, "");

    cnl_iface_stats rxtxIface;

    EXPECT_EQ(0, interface_get_stats(IFSTAT_RXTX_PACKET, interfaceName.c_str(), &rxtxIface));
    EXPECT_EQ(0, interface_get_stats(IFSTAT_RXTX_BYTES, interfaceName.c_str(), &rxtxIface));
    EXPECT_EQ(0, interface_get_stats(IFSTAT_RXTX_ERRORS, interfaceName.c_str(), &rxtxIface));
    EXPECT_EQ(0, interface_get_stats(IFSTAT_RXTX_DROPPED, interfaceName.c_str(), &rxtxIface));
    EXPECT_EQ(0, interface_get_stats(IFSTAT_RXTX_ALL, interfaceName.c_str(), &rxtxIface));
}

// ------------------------------------ //
// --------- LINK SUITE (Generic) ----- //
// ----------------------------------- //

class LinkSuite: public CoreNetLibTestSuite {
public:
    static void SetUpTestSuite()
    {
        CoreNetLibTestSuite::SetUpTestSuite();
    }

    static void TearDownTestSuite()
    {
        CoreNetLibTestSuite::TearDownTestSuite();
    }

    virtual void SetUp()
    {
        CoreNetLibTestSuite::SetUp();
    }

    virtual void TearDown()
    {
        CoreNetLibTestSuite::TearDown();
    }
};

TEST_F(LinkSuite, SetAllMulticast)
{
    std::string interfaceName = testNS::testInterfaceName;

    int prevState = linkStatusUpOrDownSystemCommand(interfaceName);

    EXPECT_EQ(0, interface_set_allmulticast(const_cast<char *> (interfaceName.c_str())));

    auto cmdOutput = getLinkStateSystemCommand(interfaceName);
    std::cout << "SetAllMulticast: link state = " << cmdOutput << std::endl;

    if (prevState == 0)
    {
        ASSERT_EQ(true, setInterfaceDownSystemCommand(interfaceName));
    }
    else if (prevState == 1)
    {
        ASSERT_EQ(true, setInterfaceUpSystemCommand(interfaceName));
    }
}

// ------------------------------------ //
// --------- ADDR SUITE -------------- //
// ----------------------------------- //

class AddrSuite: public CoreNetLibTestSuite {
public:
    static void SetUpTestSuite()
    {
        CoreNetLibTestSuite::SetUpTestSuite();
    }

    static void TearDownTestSuite()
    {
        CoreNetLibTestSuite::TearDownTestSuite();
    }

    virtual void SetUp()
    {
        CoreNetLibTestSuite::SetUp();
    }

    virtual void TearDown()
    {
        CoreNetLibTestSuite::TearDown();
    }
};

TEST_F(AddrSuite, AddrAdd)
{
    std::string ipAddr = "192.168.11.2";
    std::string netMask = "255.255.255.255";
    std::string interfaceName = testNS::ephimeralInterfaceName;
    std::string interfaceType = testNS::ephimeralInterfaceType;

    int cidr = getCIDRFromNetMask(netMask);

    char bcast[64] = {0};
    EXPECT_EQ(0, addr_derive_broadcast(const_cast<char *>(ipAddr.c_str()),
                                       cidr,
                                       bcast,
                                       sizeof(bcast)));

    deleteInterfaceSystemCommand(interfaceName);

    createTestInterface(interfaceName, interfaceType);

    char args[300] = {0};
    snprintf(args, sizeof(args), "%s/%d broadcast %s dev %s",
                                 ipAddr.c_str(),
                                 cidr,
                                 bcast,
                                 interfaceName.c_str());

    EXPECT_EQ(false, deleteIpAddrSystemCommand(ipAddr,
                                               cidr,
                                               interfaceName));

    EXPECT_EQ(0, addr_add(args));

    EXPECT_EQ(false, addIpAddrSystemCommand(ipAddr,
                                            cidr,
                                            interfaceName));

    EXPECT_EQ(true, deleteIpAddrSystemCommand(ipAddr,
                                              cidr,
                                              interfaceName));

    destroyTestInterface(interfaceName);
}

TEST_F(AddrSuite, AddrDelete)
{
    std::string ipAddr = "192.168.11.2";
    std::string netMask = "255.255.255.255";
    std::string interfaceName = testNS::ephimeralInterfaceName;
    std::string interfaceType = testNS::ephimeralInterfaceType;

    int cidr = getCIDRFromNetMask(netMask);

    char bcast[64] = {0};
    EXPECT_EQ(0, addr_derive_broadcast(const_cast<char *>(ipAddr.c_str()),
                                       cidr,
                                       bcast,
                                       sizeof(bcast)));

    createTestInterface(interfaceName, interfaceType);

    char args[300] = {0};
    snprintf(args, sizeof(args), "%s/%d broadcast %s dev %s",
                                  ipAddr.c_str(),
                                  cidr,
                                  bcast,
                                  interfaceName.c_str());

    EXPECT_NE(0, addr_delete(args));

    EXPECT_EQ(true, addIpAddrSystemCommand(ipAddr,
                                           cidr,
                                           interfaceName));

    EXPECT_EQ(0, addr_delete(args));

    EXPECT_EQ(false, deleteIpAddrSystemCommand(ipAddr,
                                               cidr,
                                               interfaceName));

    destroyTestInterface(interfaceName);
}

// ------------------------------------ //
// --------- RULE SUITE -------------- //
// ----------------------------------- //

class RuleSuite: public CoreNetLibTestSuite {
public:
    static void SetUpTestSuite()
    {
        CoreNetLibTestSuite::SetUpTestSuite();
    }

    static void TearDownTestSuite()
    {
        CoreNetLibTestSuite::TearDownTestSuite();
    }

    virtual void SetUp()
    {
        CoreNetLibTestSuite::SetUp();
    }

    virtual void TearDown()
    {
        CoreNetLibTestSuite::TearDown();
    }
};

TEST_F(RuleSuite, AddRule)
{
    std::string fromPrefix = "192.168.10.9";
    std::string toPrefix = "192.168.10.11";
    std::string tableName = "160"; // Using table ID 160 for test purposes

    char args[300] = {0};
    snprintf(args, sizeof(args), "-4 from %s to %s lookup %s",
                                  fromPrefix.c_str(),
                                  toPrefix.c_str(),
                                  tableName.c_str());

    EXPECT_EQ(false, removeIpRuleSystemCommand(fromPrefix,
                                               toPrefix,
                                               tableName));

    EXPECT_EQ(0, rule_add(args));

    EXPECT_EQ(true, removeIpRuleSystemCommand(fromPrefix,
                                              toPrefix,
                                              tableName));
}

TEST_F(RuleSuite, AddRuleInvalidFromPrefix)
{
    std::string fromPrefix = "xxx.yyy.invalid";
    std::string toPrefix = "192.168.10.11";
    std::string tableName = "160"; // Using table ID 160 for test purposes

    char args[300] = {0};
    snprintf(args, sizeof(args), "-4 from %s to %s lookup %s",
                                  fromPrefix.c_str(),
                                  toPrefix.c_str(),
                                  tableName.c_str());

    EXPECT_NE(0, rule_add(args));

    EXPECT_EQ(false, removeIpRuleSystemCommand(fromPrefix,
                                               toPrefix,
                                               tableName));
}

TEST_F(RuleSuite, AddRuleInvalidToPrefix)
{
    std::string fromPrefix = "192.168.10.9";
    std::string toPrefix = "xxx.yyy.invalid";
    std::string tableName = "160"; // Using table ID 160 for test purposes

    char args[300] = {0};
    snprintf(args, sizeof(args), "-4 from %s to %s lookup %s",
                                  fromPrefix.c_str(),
                                  toPrefix.c_str(),
                                  tableName.c_str());

    EXPECT_NE(0, rule_add(args));

    EXPECT_EQ(false, removeIpRuleSystemCommand(fromPrefix,
                                               toPrefix,
                                               tableName));
}

TEST_F(RuleSuite, AddRuleInvalidTableName)
{
    std::string fromPrefix = "192.168.10.9";
    std::string toPrefix = "192.168.10.11";
    std::string tableName = "nosuchTable0";

    char args[300] = {0};
    snprintf(args, sizeof(args), "-4 from %s to %s lookup %s",
                                  fromPrefix.c_str(),
                                  toPrefix.c_str(),
                                  tableName.c_str());

    EXPECT_NE(0, rule_add(args));

    EXPECT_EQ(false, removeIpRuleSystemCommand(fromPrefix,
                                               toPrefix,
                                               tableName));
}

TEST_F(RuleSuite, DeleteRule)
{
    std::string fromPrefix = "192.168.10.9";
    std::string toPrefix = "192.168.10.11";
    std::string tableName = "160"; // Using table ID 160 for test purposes

    char args[300] = {0};
    snprintf(args, sizeof(args), "-4 from %s to %s lookup %s",
                                  fromPrefix.c_str(),
                                  toPrefix.c_str(),
                                  tableName.c_str());

    EXPECT_EQ(true, addIpRuleSystemCommand(fromPrefix,
                                           toPrefix,
                                           tableName));

    EXPECT_EQ(0, rule_delete(args));

    EXPECT_EQ(false, removeIpRuleSystemCommand(fromPrefix,
                                               toPrefix,
                                               tableName));
}

TEST_F(RuleSuite, DeleteRuleInvalidFromPrefix)
{
    std::string fromPrefix = "xxx.yyy.invalid0";
    std::string toPrefix = "192.168.10.11";
    std::string tableName = "160"; // Using table ID 160 for test purposes

    char args[300] = {0};
    snprintf(args, sizeof(args), "-4 from %s to %s lookup %s",
                                  fromPrefix.c_str(),
                                  toPrefix.c_str(),
                                  tableName.c_str());

    EXPECT_NE(0, rule_delete(args));

    EXPECT_EQ(false, removeIpRuleSystemCommand(fromPrefix,
                                               toPrefix,
                                               tableName));
}

TEST_F(RuleSuite, DeleteRuleInvalidToPrefix)
{
    std::string fromPrefix = "192.168.10.9";
    std::string toPrefix = "xxx.yyy.invalid0";
    std::string tableName = "160"; // Using table ID 160 for test purposes

    char args[300] = {0};
    snprintf(args, sizeof(args), "-4 from %s to %s lookup %s",
                                  fromPrefix.c_str(),
                                  toPrefix.c_str(),
                                  tableName.c_str());

    EXPECT_NE(0, rule_delete(args));

    EXPECT_EQ(false, removeIpRuleSystemCommand(fromPrefix,
                                               toPrefix,
                                               tableName));
}

TEST_F(RuleSuite, DeleteRuleInvalidTableName)
{
    std::string fromPrefix = "192.168.10.9";
    std::string toPrefix = "192.168.10.11";
    std::string tableName = "invalidTable0";

    char args[300] = {0};
    snprintf(args, sizeof(args), "-4 from %s to %s lookup %s",
                                  fromPrefix.c_str(),
                                  toPrefix.c_str(),
                                  tableName.c_str());

    EXPECT_NE(0, rule_delete(args));

    EXPECT_EQ(false, removeIpRuleSystemCommand(fromPrefix,
                                               toPrefix,
                                               tableName));
}

// ------------------------------------ //
// --------- ROUTES SUITE ------------ //
// ----------------------------------- //

class RoutesSuite: public CoreNetLibTestSuite {
public:
    static std::string interfaceName;
    static std::string interfaceType;

    static void SetUpTestSuite()
    {
        CoreNetLibTestSuite::SetUpTestSuite();

        createInterfaceSystemCommand(interfaceName, interfaceType);

        // Set the interface UP for "route add" cmds to work
        setInterfaceUpSystemCommand(interfaceName);

        // Update the backup
        /*
        int status = -1;
        executeSystemCommand("cp /etc/iproute2/rt_tables /etc/iproute2/rt_tables.gtest.bak", &status);
        EXPECT_EQ(WEXITSTATUS(status), EXIT_SUCCESS) << "Failed to copy rt_tables to rt_tables.gtest.bak";
        */
    }

    static void TearDownTestSuite()
    {
        // Restore routing tables from /etc/iproute2/rt_tables.gtest.bak
        /*
        if (fileExists("/etc/iproute2/rt_tables.gtest.bak"))
        {
            std::cout << "Restoring routing tables....\n";

            int status = -1;
            executeSystemCommand("cp /etc/iproute2/rt_tables.gtest.bak /etc/iproute2/rt_tables", &status);
            EXPECT_EQ(WEXITSTATUS(status), EXIT_SUCCESS) << "Failed to restore rt_tables from rt_tables.gtest.bak";
        }
        */

        deleteInterfaceSystemCommand(interfaceName);

        CoreNetLibTestSuite::TearDownTestSuite();
    }

    virtual void SetUp()
    {
        CoreNetLibTestSuite::SetUp();
    }

    virtual void TearDown()
    {
        CoreNetLibTestSuite::TearDown();
    }
};

std::string RoutesSuite::interfaceName = testNS::ephimeralInterfaceName;
std::string RoutesSuite::interfaceType = testNS::ephimeralInterfaceType;

TEST_F(RoutesSuite, AddRoute)
{
    // "ip -6 route add %s dev %s via %s metric %d", prefix, dev, gw, metric
    // route add default via <%gateway> dev %s metric %d
    // ip route add <network_ip>/<cidr> via <gateway_ip> dev <network_card_name>

    std::string dstPrefix = "192.168.8.8";
    std::string nextHopAddr = "0.0.0.0"; // gateway
    std::string deviceName = RoutesSuite::interfaceName;

    char arg[150] = {0};

    snprintf(arg, sizeof(arg), "%s via %s dev %s metric",
                               dstPrefix.c_str(),
                               nextHopAddr.c_str(),
                               deviceName.c_str());

    EXPECT_EQ(0, route_add(arg));

    EXPECT_EQ(false, addIpRouteSystemCommand(dstPrefix,
                                             nextHopAddr,
                                             deviceName));

    EXPECT_EQ(true, removeIpRouteSystemCommand(dstPrefix,
                                               nextHopAddr,
                                               deviceName));
}

TEST_F(RoutesSuite, AddRouteInvalidDst)
{
    std::string dstPrefix = "192.168.8.8.1000"; // invalid dst
    std::string nextHopAddr = "0.0.0.0"; // gateway
    std::string deviceName = RoutesSuite::interfaceName;
    int metric = -1;

    char arg[150] = {0};

    snprintf(arg, sizeof(arg), "%s via %s dev %s metric %d",
                               dstPrefix.c_str(),
                               nextHopAddr.c_str(),
                               deviceName.c_str(),
                               metric);

    EXPECT_NE(0, route_add(arg));

    EXPECT_EQ(false, removeIpRouteSystemCommand(dstPrefix,
                                                nextHopAddr,
                                                deviceName));
}

TEST_F(RoutesSuite, AddRouteInvalidGateway)
{
    std::string dstPrefix = "192.168.8.8"; // valid dst
    std::string nextHopAddr = "0.0.0.0.100"; // invalid gateway
    std::string deviceName = RoutesSuite::interfaceName;
    int metric = -1;

    char arg[150] = {0};

    snprintf(arg, sizeof(arg), "%s via %s dev %s metric %d",
                               dstPrefix.c_str(),
                               nextHopAddr.c_str(),
                               deviceName.c_str(),
                               metric);

    EXPECT_NE(0, route_add(arg));

    EXPECT_EQ(false, removeIpRouteSystemCommand(dstPrefix,
                                                nextHopAddr,
                                                deviceName));
}

TEST_F(RoutesSuite, AddRouteInvalidDevice)
{
    std::string dstPrefix = "192.168.8.8"; // valid dst
    std::string nextHopAddr = "0.0.0.0"; // valid gateway
    std::string deviceName = "invalidDevice0000"; // invalid device
    int metric = -1;

    char arg[150] = {0};

    snprintf(arg, sizeof(arg), "%s via %s dev %s metric %d",
                               dstPrefix.c_str(),
                               nextHopAddr.c_str(),
                               deviceName.c_str(),
                               metric);

    EXPECT_NE(0, route_add(arg));

    EXPECT_EQ(false, removeIpRouteSystemCommand(dstPrefix,
                                                nextHopAddr,
                                                deviceName));
}

TEST_F(RoutesSuite, AddRouteInvalidMetric)
{
    std::string dstPrefix = "192.168.8.8"; // valid dst
    std::string nextHopAddr = "0.0.0.0"; // valid gateway
    std::string deviceName = RoutesSuite::interfaceName; // valid device
    int metric = 100; // Invalid metric

    char arg[150] = {0};

    snprintf(arg, sizeof(arg), "%s via %s dev %s metric %d",
                               dstPrefix.c_str(),
                               nextHopAddr.c_str(),
                               deviceName.c_str(),
                               metric);

    EXPECT_NE(0, route_add(arg));

    EXPECT_EQ(false, removeIpRouteSystemCommand(dstPrefix,
                                                nextHopAddr,
                                                deviceName));
}

// ------------------------------------ //
// --------- TUNNEL SUITE ------------ //
// ----------------------------------- //

class IpTunnelTestSuite: public CoreNetLibTestSuite {
public:
    static bool ipIpModulePrevPresent;

    static void SetUpTestSuite()
    {
        CoreNetLibTestSuite::SetUpTestSuite();

        int status = -1;
        auto cmdOutput = executeSystemCommand(std::string("lsmod | grep ipip"), &status);
        if (WEXITSTATUS(status) != EXIT_SUCCESS && cmdOutput.length() == 0)
        {
            // Empty grep -> error. Need to add the module
            ipIpModulePrevPresent = false;
            executeSystemCommand(std::string("modprobe ipip"), &status);
            EXPECT_EQ(EXIT_SUCCESS, WEXITSTATUS(status));
        }
        else
        {
            std::cout << "ipip module already present. Will not remove it during teardown...\n";
            ipIpModulePrevPresent = true;
        }
    }

    static void TearDownTestSuite()
    {
        if (ipIpModulePrevPresent == false)
        {
            int status = -1;
            executeSystemCommand(std::string("rmmod ipip"), &status);
            EXPECT_EQ(EXIT_SUCCESS, WEXITSTATUS(status));
        }

        CoreNetLibTestSuite::TearDownTestSuite();
    }

    virtual void SetUp()
    {
        CoreNetLibTestSuite::SetUp();
    }

    virtual void TearDown()
    {
        CoreNetLibTestSuite::TearDown();
    }
};

bool IpTunnelTestSuite::ipIpModulePrevPresent = false;

TEST_F(IpTunnelTestSuite, TunnelAddIp4Ip6Valid)
{
    std::string tunnelName = "testTunnel";

    std::string interfaceName = "testInterface";
    std::string interfaceType = "dummy";

    // Using dummy IPV6 addresses inside (0100::/64)
    std::string localV6Addr = "0100:0000:0000:0000:0100:0000:0000:0011";
    std::string remoteV6Addr = "0100:0000:0000:0000:0100:0000:0000:0022";

    createTestInterface(interfaceName, interfaceType);

    int mtu = interfaceGetMTUSystemCommand(interfaceName);
    EXPECT_TRUE(mtu >= 0 && mtu <= 65536) << "Got MTU value of " << interfaceName << " as " << mtu;

    EXPECT_TRUE(mtu - 20 > 0); // For encapsulation limit for tunnel

    int encapLimit = mtu - 20;

    char encapString[20] = {0};
    snprintf(encapString, sizeof(encapString), "%d", encapLimit);

    // No tunnel exists so can't delete it initially
    EXPECT_EQ(false, deleteIpTunnelSystemCommand(tunnelName));

    EXPECT_EQ(0, tunnel_add_ip4ip6(tunnelName.c_str(),
                                   interfaceName.c_str(),
                                   localV6Addr.c_str(),
                                   remoteV6Addr.c_str(),
                                   encapString));

    EXPECT_EQ(true, deleteIpTunnelSystemCommand(tunnelName));

    // Tunnel must be created since we deleted it (to verify if everything worked properly)
    EXPECT_EQ(true, addIpTunnelSystemCommand(tunnelName,
                                             "ip4ip6",
                                             interfaceName,
                                             localV6Addr,
                                             remoteV6Addr,
                                             encapLimit));

    EXPECT_EQ(true, deleteIpTunnelSystemCommand(tunnelName));

    destroyTestInterface(interfaceName);
}

TEST_F(IpTunnelTestSuite, TunnelAddIp4Ip6InvalidLocalAddress)
{
    std::string tunnelName = "testTunnel";

    std::string interfaceName = "testInterface";
    std::string interfaceType = "dummy";

    std::string localV4Addr = "240.101.83.2"; // local has to be v6
    std::string remoteV6Addr = "0100:0000:0000:0000:0100:0000:0000:0022";

    createTestInterface(interfaceName, interfaceType);

    int mtu = interfaceGetMTUSystemCommand(interfaceName);
    EXPECT_TRUE(mtu >= 0 && mtu <= 65536) << "Got MTU value of " << interfaceName << " as " << mtu;

    EXPECT_TRUE(mtu - 20 > 0); // For encapsulation limit for tunnel

    int encapLimit = mtu - 20;

    char encapString[20] = {0};
    snprintf(encapString, sizeof(encapString), "%d", encapLimit);

    // No tunnel exists so can't delete it initially
    EXPECT_EQ(false, deleteIpTunnelSystemCommand(tunnelName));

    // Tunnel can't be created
    EXPECT_NE(0, tunnel_add_ip4ip6(tunnelName.c_str(),
                                   interfaceName.c_str(),
                                   localV4Addr.c_str(),
                                   remoteV6Addr.c_str(),
                                   encapString));

    // Tunnel must *NOT* be created so can't delete
    EXPECT_EQ(false, deleteIpTunnelSystemCommand(tunnelName));

    // Tunnel must *NOT* be created
    EXPECT_EQ(false, addIpTunnelSystemCommand(tunnelName,
                                             "ip4ip6",
                                             interfaceName,
                                             localV4Addr,
                                             remoteV6Addr,
                                             encapLimit));

    // Tunnel must *NOT* be created so can't delete
    EXPECT_EQ(false, deleteIpTunnelSystemCommand(tunnelName));

    destroyTestInterface(interfaceName);
}

TEST_F(IpTunnelTestSuite, TunnelAddIp4Ip6InvalidRemoteAddress)
{
    std::string tunnelName = "testTunnel";

    std::string interfaceName = "testInterface";
    std::string interfaceType = "dummy";

    std::string localV6Addr = "0100:0000:0000:0000:0100:0000:0000:0011";
    std::string remoteV4Addr = "251.4.92.217"; // remote has to be v6

    createTestInterface(interfaceName, interfaceType);

    int mtu = interfaceGetMTUSystemCommand(interfaceName);
    EXPECT_TRUE(mtu >= 0 && mtu <= 65536) << "Got MTU value of " << interfaceName << " as " << mtu;

    EXPECT_TRUE(mtu - 20 > 0); // For encapsulation limit for tunnel

    int encapLimit = mtu - 20;

    char encapString[20] = {0};
    snprintf(encapString, sizeof(encapString), "%d", encapLimit);

    // No tunnel exists so can't delete it initially
    EXPECT_EQ(false, deleteIpTunnelSystemCommand(tunnelName));

    // Tunnel can't be created
    EXPECT_NE(0, tunnel_add_ip4ip6(tunnelName.c_str(),
                                   interfaceName.c_str(),
                                   localV6Addr.c_str(),
                                   remoteV4Addr.c_str(),
                                   encapString));

    // Tunnel must *NOT* be created so can't delete
    EXPECT_EQ(false, deleteIpTunnelSystemCommand(tunnelName));

    // Tunnel must *NOT* be created
    EXPECT_EQ(false, addIpTunnelSystemCommand(tunnelName,
                                             "ip4ip6",
                                             interfaceName,
                                             localV6Addr,
                                             remoteV4Addr,
                                             encapLimit));

    // Tunnel must *NOT* be created so can't delete
    EXPECT_EQ(false, deleteIpTunnelSystemCommand(tunnelName));

    destroyTestInterface(interfaceName);
}

TEST_F(IpTunnelTestSuite, TunnelAddIp4Ip6InValidDevice)
{
    std::string tunnelName = "testTunnel";

    std::string interfaceName = "invalid0";
    std::string interfaceType = "dummy";

    std::string localV6Addr = "0100:0000:0000:0000:0100:0000:0000:0011";
    std::string remoteV6Addr = "0100:0000:0000:0000:0100:0000:0000:0022";

    int encapLimit = 500; // Dummy value
    char encapString[20] = {0};
    snprintf(encapString, sizeof(encapString), "%d", encapLimit);

    // No tunnel exists so can't delete it initially
    EXPECT_EQ(false, deleteIpTunnelSystemCommand(tunnelName));

    // Should fail because interface is invalid
    EXPECT_NE(0, tunnel_add_ip4ip6(tunnelName.c_str(),
                                   interfaceName.c_str(),
                                   localV6Addr.c_str(),
                                   remoteV6Addr.c_str(),
                                   encapString));

    EXPECT_EQ(false, addIpTunnelSystemCommand(tunnelName,
                                              "ip4ip6",
                                              interfaceName,
                                              localV6Addr,
                                              remoteV6Addr,
                                              encapLimit));

    EXPECT_EQ(false, deleteIpTunnelSystemCommand(tunnelName));
}

// ------------------------------------ //
// --------- NEIGHBOUR SUITE ---------- //
// ----------------------------------- //

class NeighbourTestSuite: public CoreNetLibTestSuite {
public:
    static void SetUpTestSuite()
    {
        CoreNetLibTestSuite::SetUpTestSuite();
    }

    static void TearDownTestSuite()
    {
        CoreNetLibTestSuite::TearDownTestSuite();
    }

    virtual void SetUp()
    {
        CoreNetLibTestSuite::SetUp();
    }

    virtual void TearDown()
    {
        CoreNetLibTestSuite::TearDown();
    }
};

void printNeighboursTables(struct neighbour_info *table1, std::vector<TestNeighbourInfo *>& table2)
{
    std::cout << "\nPrinting Neighbour Tables:\n";
    std::cout << "---------------------------\n";
    std::cout << "CoreNetLib API Table:\n";

    if (table1 == NULL)
    {
        std::cout << "|NULL|\n";
        std::cout << std::endl;
    }
    else
    {
        fprintf(stdout, "Neighbour Count = %d\n", table1->neigh_count);

        for (int i=0; i<table1->neigh_count; i++)
        {
            fprintf(stdout, "------------------------\n");
            fprintf(stdout, "#%d\t", i+1);
            auto info = table1->neigh_arr[i];
            fprintf(stdout, "|State = %d|\t", info.state);
            fprintf(stdout, "|Local = %s|\t", info.local);
            fprintf(stdout, "|Mac = %s|\n", info.mac);
        }

        fprintf(stdout, "------------------------\n");
    }

    std::cout << "---------------------------\n";

    std::cout << "'ip neighbour show' Table:\n";

    fprintf(stdout, "Neighbour Count = %zu\n", table2.size());

    for (int i=0; i<table2.size(); i++)
    {
        fprintf(stdout, "------------------------\n");
        fprintf(stdout, "#%d\t", i+1);

        auto info = table2[i];
        if (info == NULL)
        {
            fprintf(stdout, "|NULL|\n");
            continue;
        }

        fprintf(stdout, "|State = %d|\t", info->state);
        fprintf(stdout, "|Local = %s|\t", info->local.c_str());
        fprintf(stdout, "|Mac = %s|\n", info->mac.c_str());
    }

    fprintf(stdout, "------------------------\n");

    std::cout << "---------------------------\n";
}

TEST_F(NeighbourTestSuite, NeighbourGetList)
{
    std::vector<TestNeighbourInfo *> neighbours;
    EXPECT_EQ(true, getNeighboursSystemCommand(neighbours));

    struct neighbour_info arr = {
        .neigh_count = 0,
    };
    memset(arr.neigh_arr, 0, MAX_NEIGH_COUNT * sizeof(struct _neighbour_info));
    EXPECT_EQ(0, neighbour_get_list(&arr));

    EXPECT_LE(neighbours.size(), arr.neigh_count);

    if (neighbours.size() > arr.neigh_count)
    {
        printNeighboursTables(&arr, neighbours);
        cleanupNeighboursList(neighbours);
        return;
    }

    for (auto neighbour : neighbours)
    {
        bool isPresent = false;
        for (int j=0; j<arr.neigh_count; j++)
        {
            auto info = arr.neigh_arr[j];
            if (std::string(info.local) == neighbour->local)
            {
                isPresent = true;
                if (strcmp(info.mac, "none") == 0)
                {
                    EXPECT_TRUE(neighbour->mac == "");
                }
                else
                {
                    EXPECT_STREQ(info.mac, neighbour->mac.c_str());
                }
                EXPECT_EQ(neighbour->state, info.state);
                break;
            }
        }

        EXPECT_EQ(true, isPresent);
    }

    if (HasFailure())
    {
        printNeighboursTables(&arr, neighbours);
    }

    cleanupNeighboursList(neighbours);
}

TEST_F(NeighbourTestSuite, NeighbourRemove)
{
    createTestInterface(testNS::ephimeralInterfaceName, testNS::ephimeralInterfaceType);

    // First we'll add a neighbour (V4)
    std::string address, llAddress, deviceName;
    address.assign("192.168.9.9");
    llAddress.assign("0E:01:23:45:67:89"); // "dummy" ll address of the form: xE-xx-xx-xx-xx-xx
    deviceName.assign(testNS::ephimeralInterfaceName);

    // Delete needs to return false as no such address should exist
    EXPECT_EQ(false, deleteNeighbourSystemCommand(address, deviceName));

    EXPECT_EQ(true, addNeighbourSystemCommand(address, deviceName));

    EXPECT_EQ(0, neighbour_delete(const_cast<char *> (deviceName.c_str()),
                                  const_cast<char *> (address.c_str())));

    // Delete needs to return false since our API did it already
    EXPECT_EQ(false, deleteNeighbourSystemCommand(address, deviceName));

    destroyTestInterface(testNS::ephimeralInterfaceName);
}

TEST_F(NeighbourTestSuite, NeighbourRemoveInvalidAddress)
{
    createTestInterface(testNS::ephimeralInterfaceName, testNS::ephimeralInterfaceType);

    // First we'll add a neighbour (V4)
    std::string address, llAddress, deviceName;
    address.assign("invalidAddr");
    llAddress.assign("0E:01:23:45:67:89"); // "dummy" ll address of the form: xE-xx-xx-xx-xx-xx
    deviceName.assign(testNS::ephimeralInterfaceName);

    EXPECT_NE(0, neighbour_delete(const_cast<char *> (deviceName.c_str()),
                                  const_cast<char *> (address.c_str())));

    // Delete needs to return false too
    EXPECT_EQ(false, deleteNeighbourSystemCommand(address, deviceName));

    destroyTestInterface(testNS::ephimeralInterfaceName);
}

TEST_F(NeighbourTestSuite, NeighbourRemoveInvalidDevice)
{
    // First we'll add a neighbour (V4)
    std::string address, llAddress, deviceName;
    address.assign("192.168.9.9");
    llAddress.assign("0E:01:23:45:67:89"); // "dummy" ll address of the form: xE-xx-xx-xx-xx-xx
    deviceName.assign("invalidDevice0");

    EXPECT_NE(0, neighbour_delete(const_cast<char *> (deviceName.c_str()),
                                  const_cast<char *> (address.c_str())));

    // Delete needs to return false too
    EXPECT_EQ(false, deleteNeighbourSystemCommand(address, deviceName));
}

// ------------------------------------ //
// --------- CRASH SUITE ------------- //
// ----------------------------------- //

class CrashTestSuite: public CoreNetLibTestSuite {
public:
    static void SetUpTestSuite()
    {
        CoreNetLibTestSuite::SetUpTestSuite();
    }

    static void TearDownTestSuite()
    {
        CoreNetLibTestSuite::TearDownTestSuite();
    }

    virtual void SetUp()
    {
        CoreNetLibTestSuite::SetUp();
    }

    virtual void TearDown()
    {
        CoreNetLibTestSuite::TearDown();
    }
};

TEST_F(CrashTestSuite, SetIpWillFail)
{
    std::string interfaceName = testNS::ephimeralInterfaceName;
    std::string ipAddress = "192.0.20.2000"; // Bogus address

    EXPECT_NE(0, interface_set_ip(interfaceName.c_str(), ipAddress.c_str()));

    EXPECT_STREQ(NULL, interface_get_ip(interfaceName.c_str()));
}

TEST_F(CrashTestSuite, SetIpWillFailNull)
{
    std::string interfaceName = testNS::ephimeralInterfaceName;
    std::string ipAddress = ""; // Bogus address

    EXPECT_NE(0, interface_set_ip(interfaceName.c_str(), ipAddress.c_str()));

    EXPECT_STREQ(NULL, interface_get_ip(interfaceName.c_str()));
}
