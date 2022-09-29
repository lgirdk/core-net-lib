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
#include "TestUtils.hpp"

#include <iostream>
#include <array>
#include <fstream>
#include <map>
#include <string>
#include <sstream>
#include <vector>
#include <chrono>
#include <thread>

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/ioctl.h>

extern "C" {
    #include "safec_lib_common.h"
}

/**
 * Sleep for the requested number of milliseconds.
 */
void msleep(long msec)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(msec));
}

/**
 * Splits a string via a delimiter
 */
std::vector<std::string> splitString(const std::string& input, const char delim)
{
    std::stringstream ss(input);
    std::string token;
    std::vector<std::string> tokens;

    while(std::getline(ss, token, delim))
    {
        tokens.push_back(token);
    }

    return tokens;
}

/**
 * Returns true only if 'needle' occurs in 'haystack' exactly 'numOccurrences' times.
 *
 * If numOccurrences <= 0 then return true.
 */
bool findStringWithOccurrences(const std::string& haystack, const std::string& needle, int numOccurrences)
{
    if (numOccurrences <= 0)
    {
        return true;
    }

    int occurrences = 0;
    int start = 0;

    while ((start = haystack.find(needle, start)) != haystack.npos) {
        ++occurrences;
        start += needle.length();
    }

    return (occurrences == numOccurrences);
}

/**
 * Checks if the file exists
 *
 * @param fileName Absolute path of file to be checked
 */
bool fileExists(const std::string& filePath)
{
    return access(filePath.c_str(), 0) == 0;
}

/**
 * Returns the list of files in the Linux directory specified by path
 *
 * NOTE: This doesn't list files recursively
 */
std::vector<std::string> listDirectoryFiles(const std::string& path)
{
    std::vector<std::string> files;

    DIR *dir = opendir(path.c_str());
    if (dir == NULL) {
        return files;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR)
        {
            // Skip directories
            continue;
        }
        files.push_back(std::string(entry->d_name));
    }

    closedir(dir);
    return files;
}

/**
 * Parses the netmask from the CIDR Address
 *
 * Eg: 172.17.0.1/16 will have netmask 255.255.0.0
 */
std::string getNetMaskFromCIDRAddress(const std::string& CIDRAddress)
{
    std::string netmask = "";

    auto parts = splitString(CIDRAddress, '/');

    try {
        std::string ipAddr = parts[0];

        std::string prefix;
        if (parts.size() < 2) {
            prefix = "0";
        } else {
            prefix = parts[1];
        }

        int mask = 0xffffffff << (32 - std::stoi(prefix));

        netmask = std::to_string((unsigned int) mask >> 24) + "." +
                  std::to_string(mask >> 16 & 0xff) + "." +
                  std::to_string(mask >> 8 & 0xff) + "." +
                  std::to_string(mask & 0xff);
    } catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        netmask = "";
    }

    return netmask;
}

/**
 * Returns the CIDR Address from the netmask.
 *
 * Eg: netmask 255.255.0.0 will have CIDR 16
 * 
 * @return cidr address, -1 on failure
 */
int getCIDRFromNetMask(const std::string& netMask)
{
    int cidr = 0;

    auto tokens = splitString(netMask, '.');

    if (tokens.size() != 4) {
        return -1;
    }

    for (int i=0; i<4; i++)
    {
        auto token = tokens[i];
        switch(std::stoi(token))
        {
            case 0x80:
                cidr+=1;
                break;
            case 0xC0:
                cidr+=2;
                break;
            case 0xE0:
                cidr+=3;
                break;
            case 0xF0:
                cidr+=4;
                break;
            case 0xF8:
                cidr+=5;
                break;
            case 0xFC:
                cidr+=6;
                break;
            case 0xFE:
                cidr+=7;
                break;
            case 0xFF:
                cidr+=8;
                break;
            default:
                return cidr;
                break;
        }
    }

    return cidr;
}

/**
 * Executes a system command and returns the output as a string
 * @param cmd The command to be executed
 * @param status Pointer to a status variable which will be the status of the command execution
 */
std::string executeSystemCommand(char *cmd, int *status)
{
    std::string cmdOutput = "";
    std::array<char, 128> buffer;

    FILE* pipe = popen(cmd, "r");
    if (!pipe)
    {
        std::cerr << "Couldn't run command - Error with popen()" << std::endl;
        return cmdOutput;
    }

    while (fgets(buffer.data(), 128, pipe) != NULL) {
        cmdOutput += buffer.data();
    }

    *status = pclose(pipe);
    if (*status != 0)
    {
        std::cerr << "Warning: error closing pipe - exited with status code " << *status << std::endl;
    }

    if (cmdOutput.length() >= 1 && cmdOutput[cmdOutput.length() - 1] == '\n')
    {
        cmdOutput.pop_back();
    }

    return cmdOutput;
}

/**
 * Executes a system command and returns the output as a string
 * @param cmd The command to be executed
 * @param status Pointer to a status variable which will be the status of the command execution
 */
std::string executeSystemCommand(const std::string& cmd, int *status)
{
    std::string cmdOutput = "";
    std::array<char, 128> buffer;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        std::cerr << "Couldn't run command - Error with popen()" << std::endl;
        return cmdOutput;
    }

    while (fgets(buffer.data(), 128, pipe) != NULL) {
        cmdOutput += buffer.data();
    }

    *status = pclose(pipe);
    if (*status != 0)
    {
        std::cerr << "Warning: error closing pipe - exited with status code " << *status << std::endl;
    }

    if (cmdOutput.length() >= 1 && cmdOutput[cmdOutput.length() - 1] == '\n')
    {
        cmdOutput.pop_back();
    }

    return cmdOutput;
}

/**
 * Executes a system command and returns the output as a string
 * @param cmd The command to be executed
 * @param logFile The output file to redirect STDOUT of `cmd`. Will be ignored if passing empty string ("")
 * @param status Pointer to a status variable which will be the status of the command execution
 */
std::string executeSystemCommand(const std::string& cmd, const std::string& logFile, int *status)
{

    std::string cmdOutput = "";

    std::array<char, 128> buffer;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        std::cerr << "Couldn't run command - Error with popen()" << std::endl;
        return cmdOutput;
    }

    while (fgets(buffer.data(), 128, pipe) != NULL) {
        cmdOutput += buffer.data();
    }

    *status = pclose(pipe);
    if (*status != 0)
    {
        std::cerr << "Warning: error closing pipe - exited with status code " << *status << std::endl;
    }

    if (cmdOutput.length() >= 1 && cmdOutput[cmdOutput.length() - 1] == '\n')
    {
        cmdOutput[cmdOutput.length() - 1] = '\0';
    }

    return cmdOutput;
}

/**
 * Get the VLAN name from the interface and vlanId
 */
std::string getVlanName(const std::string& interfaceName, const int vlanId)
{
    return interfaceName + "." + std::to_string(vlanId);
}

/*
 * Creates a vlan device via the system command:
 * "ip link add link <IF_NAME> name <VLAN_NAME> type vlan id <VLAN_ID>"
 */
bool createVlanSystemCommand(const std::string& interfaceName, const int vlanId)
{
    int status = -1;
    auto vlanName = interfaceName + "." + std::to_string(vlanId);
    auto cmdOutput = executeSystemCommand(
        "ip link add link " + interfaceName +
        " name " + vlanName + " type vlan id " + std::to_string(vlanId), &status);

    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/*
 * Deletes a vlan device via the system command:
 * "ip link delete <VLAN_NAME>"
 */
bool deleteVlanSystemCommand(const std::string& vlanName)
{
    int status = -1;
    auto cmdOutput = executeSystemCommand("ip link delete " + vlanName, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/*
 * Creates a bridge device via the system command:
 * "brctl addbr <BRIDGE_NAME>"
 */
bool createBridgeSystemCommand(const std::string& bridgeName)
{
    int status = -1;
    auto cmdOutput = executeSystemCommand("brctl addbr " + bridgeName, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/*
 * Deletes a bridge device via the system command:
 * "brctl delbr <BRIDGE_NAME>"
 */
bool deleteBridgeSystemCommand(const std::string& bridgeName)
{
    int status = -1;
    auto cmdOutput = executeSystemCommand("brctl delbr " + bridgeName, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Creates an interface of type interfaceType
 */
bool createInterfaceSystemCommand(const std::string& interfaceName, const std::string& interfaceType)
{
    int status = -1;
    executeSystemCommand("ip link add " + interfaceName + " type " + interfaceType, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Creates an interface with ipAddress via:
 * "ip link add <IF_NAME> type <IF_TYPE>" and "ip address add <IP_ADDR> dev <IF_NAME>"
 */
bool createInterfaceSystemCommand(const std::string& interfaceName, const std::string& interfaceType, const std::string& ipAddress)
{
    int status = -1;
    executeSystemCommand("ip link add " + interfaceName + " type " + interfaceType, &status);
    if (WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        return false;
    }

    executeSystemCommand("ip address add " + ipAddress + " dev " + interfaceName, &status);

    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Deletes an interface via:
 * "ip link delete <IF_NAME>"
 */
bool deleteInterfaceSystemCommand(const std::string& interfaceName)
{
    int status = -1;
    executeSystemCommand("ip link delete " + interfaceName, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Get link state via:
 * "cat /sys/class/net/eth0/operstate"
 *
 * Returns state string, empty string ("") on failure
 */
std::string getLinkStateSystemCommand(const std::string& interfaceName)
{
    int status = -1;
    auto cmdOutput = executeSystemCommand("cat /sys/class/net/" + interfaceName + "/operstate", &status);
    if (WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        return "";
    }
    else
    {
        return cmdOutput;
    }
}

/**
 * Check link state via:
 * "cat /sys/class/net/eth0/operstate"
 *
 * Returns -1 on failure, 0 if DOWN and 1 if UP
 */
int linkStatusUpOrDownSystemCommand(const std::string& interfaceName)
{
    int status = -1;
    auto cmdOutput = executeSystemCommand("cat /sys/class/net/" + interfaceName + "/operstate", &status);
    if (WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        return -1;
    }
    else
    {
        if (cmdOutput.substr(0, 2) == "up")
        {
            return 1;
        }
        else if (cmdOutput.substr(0, 4) == "down")
        {
            return 0;
        }
        else
        {
            std::cerr << "State of interface: " << interfaceName << " is " << cmdOutput << std::endl;
        }
        return -1;
    }
}

/**
 * Sets an interface as UP via:
 * "ip link set <IF_NAME> up"
 */
bool setInterfaceUpSystemCommand(const std::string& interfaceName)
{
    int status = -1;
    executeSystemCommand("ip link set " + interfaceName + " up", &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Sets an interface as DOWN via:
 * "ip link set <IF_NAME> down"
 */
bool setInterfaceDownSystemCommand(const std::string& interfaceName)
{
    int status = -1;
    executeSystemCommand("ip link set " + interfaceName + " down", &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Checks if an interface exists via:
 * "ip link show <IF_NAME>"
 */
bool interfaceExistsSystemCommand(const std::string& interfaceName)
{
    int status = -1;
    executeSystemCommand("ip link show " + interfaceName, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Gets the MAC address of an interface via:
 * "ip link show <IF_NAME> | grep link/ether | awk '{print $2}'"
 *
 * Returns empty string ("") if the interface doesn't exist or if the command fails
 */
std::string interfaceGetMacSystemCommand(const std::string& interfaceName)
{
    int status = -1;
    char cmd[96] = {0};
    errno_t rc = -1;
    rc = sprintf_s(cmd, sizeof(cmd), "ip link show %s | grep link/ether | awk '{print $2}'", interfaceName.c_str());
    ERR_CHK(rc);
 
    auto cmdOutput = executeSystemCommand(cmd, &status);
    if (WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        std::cerr << "Error in " << __func__ << ": interface " << interfaceName << "returned with status " << status << std::endl;
        return "";
    }
    else
    {
        return cmdOutput;
    }
}

/**
 * Gets the bridge STP state (>=0) via:
 * "cat /sys/class/net/<BRIDGE_NAME>/bridge/stp_state"
 *
 * Returns -1 on failure
 */
int getBridgeSTPStateSystemCommand(const std::string& bridgeName)
{
    int status = -1;
    auto cmdOutput = executeSystemCommand("cat /sys/class/net/" + bridgeName + "/bridge/stp_state", &status);
    if (WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        return -1;
    }
    else
    {
        try
        {
            int val = std::stoi(cmdOutput);
            if (val < 0 || val > 4)
            {
                return -1;
            }
            return val;
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            return -1;
        }
    }
}

/**
 * Sets the bridge STP state on/off via:
 * "brctl stp <BRIDGE_NAME> on/off"
 *
 * Returns -1 on failure
 */
bool setBridgeSTPStateSystemCommand(const std::string& bridgeName, bool onOrOff)
{
    int status = -1;
    std::string value;
    if (onOrOff == true)
    {
        value.assign("on");
    }
    else
    {
        value.assign("off");
    }
    executeSystemCommand("brctl stp " + bridgeName + " " + value, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Sets the MAC address of the interface via:
 * "ip link set %s address %s"
 *
 * Returns false on failure
 */
bool interfaceSetMacSystemCommand(const std::string& interfaceName, const std::string& macAddress)
{
    int status = -1;
    char cmd[96] = {0};
    errno_t rc = -1;
    rc = sprintf_s(cmd, sizeof(cmd), "ip link set %s address %s", interfaceName.c_str(), macAddress.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Gets the IP addr of the interface via:
 * "ip -4 -o addr show %s | awk '{print $4}' | cut -d "/" -f 1"
 *
 * Returns empty string ("") on failure
 */
std::string interfaceGetIpSystemCommand(const std::string& interfaceName)
{
    int status = -1;
    char cmd[128] = {0};
    errno_t rc = -1;
    rc = sprintf_s(cmd, sizeof(cmd), "ip -4 -o addr show %s | awk '{print $4}' | cut -d \"/\" -f 1", interfaceName.c_str());
    ERR_CHK(rc);

    auto cmdOutput = executeSystemCommand(cmd, &status);
    if (WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        std::cerr << "Error in " << __func__ << ": interface " << interfaceName << "returned with status " << status << std::endl;
        return "";
    }
    else
    {
        return cmdOutput;
    }
}

/**
 * Gets the netmask of an interface via:
 * "ip addr show %s | grep inet | awk '{print $2}'"
 *
 * Returns empty string ("") on failure
 */
std::string interfaceGetNetmaskSystemCommand(const std::string& interfaceName)
{
    int status = -1;
    char cmd[200] = {0};
    errno_t rc = -1;

    rc = sprintf_s(cmd, sizeof(cmd), "ip addr show %s | grep inet | awk '{print $2}'", interfaceName.c_str());
    ERR_CHK(rc);

    auto cmdOutput = executeSystemCommand(cmd, &status);
    if (WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        std::cerr << "Error in " << __func__ << ": interface " << interfaceName << "returned with status " << status << std::endl;
        return "";
    }
    else
    {
        return getNetMaskFromCIDRAddress(cmdOutput);
    }
}

/**
 * Sets the netmask of an interface via:
 * "ip a add %s/%s dev %s"
 *
 * Returns false on failure (eg. you can't set netmask w/o an IP)
 */
bool interfaceSetNetmaskSystemCommand(const std::string& interfaceName, const std::string& interfaceType, const std::string& netmask)
{
    int status = -1;
    char cmd[96] = {0};
    errno_t rc = -1;

    auto ipAddress = interfaceGetIpSystemCommand(interfaceName);
    if (ipAddress == "")
    {
        return false;
    }

    rc = sprintf_s(cmd, sizeof(cmd), "ip a add %s/%s dev %s", ipAddress.c_str(), netmask.c_str(), interfaceType.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Gets the interface MTU via:
 * "cat /sys/class/net/%s/mtu"
 *
 * Returns -1 on failure
 */
int interfaceGetMTUSystemCommand(const std::string& interfaceName)
{
    int status = -1;
    char cmd[96] = {0};
    errno_t rc = -1;
    rc = sprintf_s(cmd, sizeof(cmd), "cat /sys/class/net/%s/mtu", interfaceName.c_str());
    ERR_CHK(rc);

    auto cmdOutput = executeSystemCommand(cmd, &status);

    if (WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        return -1;
    }
    else
    {
        try
        {
            int val = std::stoi(cmdOutput);
            if (val < 0 || val > 65536)
            {
                return -1;
            }
            return val;
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            return -1;
        }
    }
}

/**
 * Adds an interface to a bridge via:
 * "brctl addif <BRIDGE_NAME> <IF_NAME>"
 *
 * Example: brctl addif brlan0 veth0
 */
bool interfaceAddToBridgeSystemCommand(const std::string& interfaceName, const std::string& bridgeName)
{
    int status = -1;
    char cmd[96] = {0};
    errno_t rc = -1;
    rc = sprintf_s(cmd, sizeof(cmd), "brctl addif %s %s", bridgeName.c_str(), interfaceName.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Deletes an interface from a bridge via:
 * "brctl delif <BRIDGE_NAME> <IF_NAME>"
 *
 * Example: brctl delif brlan0 veth0
 */
bool interfaceRemoveFromBridgeSystemCommand(const std::string& interfaceName, const std::string& bridgeName)
{
    int status = -1;
    char cmd[96] = {0};
    errno_t rc = -1;
    rc = sprintf_s(cmd, sizeof(cmd), "brctl delif %s %s", bridgeName.c_str(), interfaceName.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Adds an address to a device via:
 *
 * ip addr add <IP_ADDR>/<CIDR> broadcast + dev <DEVICE_NAME>
 *
 * Example: ip addr add 192.168.100.17/24 broadcast + dev <DEVICE_NAME>
 */
bool addIpAddrSystemCommand(const std::string& ipAddr,
                            int cidr,
                            const std::string& deviceName)
{
    int status = -1;
    char cmd[200] = {0};
    errno_t rc = -1;

    rc = sprintf_s(cmd, sizeof(cmd), "ip addr add %s/%d broadcast + dev %s",
                                ipAddr.c_str(),
                                cidr,
                                deviceName.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Deletes an address from a device via:
 *
 * ip addr del <IP_ADDR>/<CIDR> broadcast + dev <DEVICE_NAME>
 *
 * Example: ip addr del 192.168.100.17/24 broadcast + dev <DEVICE_NAME>
 */
bool deleteIpAddrSystemCommand(const std::string& ipAddr,
                               int cidr,
                               const std::string& deviceName)
{
    int status = -1;
    char cmd[200] = {0};
    errno_t rc = -1;

    rc = sprintf_s(cmd, sizeof(cmd), "ip addr del %s/%d broadcast + dev %s",
                                ipAddr.c_str(),
                                cidr,
                                deviceName.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Adds a route to the IP routing table via:
 *
 * ip route add %s via %s dev %s table %s
 *
 * Example: ip route add default via 192.168.30.1 dev eth0.30
 */
bool addIpRouteSystemCommand(const std::string& dstPrefix,
                             const std::string& nextHopAddr,
                             const std::string& deviceName)
{
    int status = -1;
    char cmd[150] = {0};
    errno_t rc = -1;

    rc = sprintf_s(cmd, sizeof(cmd), "ip route add %s via %s dev %s",
                                dstPrefix.c_str(),
                                nextHopAddr.c_str(),
                                deviceName.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Adds a route to the IP routing table via:
 *
 * ip route add %s via %s dev %s metric %s
 *
 * Example: ip route add default via 192.168.30.1 dev eth0.30 metric 100
 */
bool addIpRouteSystemCommand(const std::string& dstPrefix,
                             const std::string& nextHopAddr,
                             const std::string& deviceName,
                             const int metric)
{
    int status = -1;
    char cmd[164] = {0};
    errno_t rc = -1;
    rc = sprintf_s(cmd, sizeof(cmd), "ip route add %s via %s dev %s metric %d",
                                dstPrefix.c_str(),
                                nextHopAddr.c_str(),
                                deviceName.c_str(),
                                metric);
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Removes a route from the IP routing table via:
 *
 * ip route delete %s via %s dev %s
 *
 * Example: ip route delete 192.168.8.8 via 0.0.0.0 dev dummy50
 */
bool removeIpRouteSystemCommand(const std::string& dstPrefix,
                                const std::string& nextHopAddr,
                                const std::string& deviceName)
{
    int status = -1;
    char cmd[164] = {0};
    errno_t rc = -1;
    rc = sprintf_s(cmd, sizeof(cmd), "ip route delete %s via %s dev %s",
                                dstPrefix.c_str(),
                                nextHopAddr.c_str(),
                                deviceName.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Adds a policy routing rule via:
 *
 * ip rule add from %s to %s table %s
 *
 * Example: ip rule add from 192.168.100.17 tos 0x08 fwmark 4 table 7
 */
bool addIpRuleSystemCommand(const std::string& fromPrefix,
                            const std::string& toPrefix,
                            const std::string& tableName)
{
    int status = -1;
    char cmd[200] = {0};
    errno_t rc = -1;
    rc = sprintf_s(cmd, sizeof(cmd), "ip rule add from %s to %s table %s",
                                fromPrefix.c_str(),
                                toPrefix.c_str(),
                                tableName.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Removes a policy routing rule via:
 *
 * ip rule del from %s to %s table %s
 *
 * Example: ip rule del from 192.168.100.17 tos 0x08 fwmark 4 table 7
 */
bool removeIpRuleSystemCommand(const std::string& fromPrefix,
                               const std::string& toPrefix,
                               const std::string& tableName)
{
    int status = -1;
    char cmd[256] = {0};
    errno_t rc = -1;
    rc = sprintf_s(cmd, sizeof(cmd), "ip rule del from %s to %s table %s",
                                fromPrefix.c_str(),
                                toPrefix.c_str(),
                                tableName.c_str());
    ERR_CHK(rc);

    auto cmdOutput = executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Create tunnel via:
 *
 * ip tunnel add <NAME> mode <MODE> [ local <S> ] [ remote <D> ]
 */
bool addIpTunnelSystemCommand(const std::string& tunnelName,
                              const std::string& tunnelMode,
                              const std::string& localV6Addr,
                              const std::string& remoteV6Addr,
                              const int encapLimit)
{
    int status = -1;
    char cmd[150] = {0};
    errno_t rc = -1;
    rc = sprintf_s(cmd, sizeof(cmd), "ip tunnel add %s mode %s local %s remote %s",
                                tunnelName.c_str(), tunnelMode.c_str(),
                                localV6Addr.c_str(), remoteV6Addr.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Create tunnel via:
 *
 * ip tunnel add <NAME> mode <MODE> [ local <S> ] [ remote <D> ] dev <DEV_NAME>
 */
bool addIpTunnelSystemCommand(const std::string& tunnelName,
                              const std::string& tunnelMode,
                              const std::string& deviceName,
                              const std::string& localV6Addr,
                              const std::string& remoteV6Addr,
                              const int encapLimit)
{
    int status = -1;
    char cmd[150] = {0};
    errno_t rc = -1;

    rc = sprintf_s(cmd, sizeof(cmd), "ip tunnel add %s mode %s local %s remote %s dev %s",
                                tunnelName.c_str(), tunnelMode.c_str(),
                                localV6Addr.c_str(), remoteV6Addr.c_str(),
                                deviceName.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Deletes a tunnel via:
 *
 * ifconfig %s down && ip tunnel del %s
 */
bool deleteIpTunnelSystemCommand(const std::string& tunnelName)
{
    int status = -1;
    char cmd[96] = {0};
    errno_t rc = -1;
    rc = sprintf_s(cmd, sizeof(cmd), "ifconfig %s down && ip tunnel del %s", tunnelName.c_str(), tunnelName.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Populates the neighbours in the neighbour table via:
 * "ip neighbour show"
 *
 * @return true if the function succeeded and false otherwise
 */
bool getNeighboursSystemCommand(std::vector<TestNeighbourInfo *>& neighbours)
{
    int status = -1;
    auto cmdOutput = executeSystemCommand(std::string("ip neighbour show"), &status);

    static std::map<std::string, int> neighbourStateMap = {
        {"INCOMPLETE", NUD_INCOMPLETE},
        {"REACHABLE", NUD_REACHABLE},
        {"STALE", NUD_STALE},
        {"DELAY", NUD_DELAY},
        {"PROBE", NUD_PROBE},
        {"FAILED", NUD_FAILED},
        {"NOARP", NUD_NOARP},
        {"PERMANENT", NUD_PERMANENT},
    };

    if (WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        std::cerr << "ERROR " << __func__ << " when fetching neighbours\n";
        return false;
    }

    auto lines = splitString(cmdOutput, '\n');
    for (auto &line : lines)
    {
        if (line == "") continue;
        auto tokens = splitString(line, ' ');

        TestNeighbourInfo *info = new TestNeighbourInfo();

        for (int i=0, idx=0; i<tokens.size(); i++)
        {
            if (tokens[i] == "") continue;
            if (idx == 0)
            {
                info->local = tokens[i];
            }
            else if (idx == 3)
            {
                if (tokens[i] == "lladdr")
                {
                    info->mac = tokens[++i];
                }
            }

            if (i + 1 == tokens.size())
            {
                auto it = neighbourStateMap.find(tokens[i]);
                if (it != neighbourStateMap.end())
                {
                    info->state = it->second;
                }
            }

            idx++;
        }

        neighbours.push_back(info);
    }

    return true;
}

/**
 * Frees up memory allocated by getNeighboursSystemCommand()
 */
void cleanupNeighboursList(std::vector<TestNeighbourInfo *>& neighbours)
{
    for (auto it = neighbours.begin(); it != neighbours.end(); ++it)
    {
        delete *it;
    }
}

/**
 * Adds a neighbour to the neighbour table via:
 * "ip neighbour add <IPv4/6 address> dev <device>"
 *
 * @return true if the function succeeded and false otherwise
 */
bool addNeighbourSystemCommand(const std::string& address,
                               const std::string& deviceName)
{
    int status = -1;
    char cmd[256] = {0};
    errno_t rc = -1;

    rc = sprintf_s(cmd, sizeof(cmd), "ip neighbour add %s dev %s",
                                address.c_str(),
                                deviceName.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Adds a neighbour to the neighbour table via:
 * "ip neighbour add <IPv4/6 address> lladdr <link-layer address> dev <device>"
 *
 * @return true if the function succeeded and false otherwise
 */
bool addNeighbourSystemCommand(const std::string& address,
                               const std::string& llAddr,
                               const std::string& deviceName)
{
    int status = -1;
    char cmd[256] = {0};
    errno_t rc = -1;

    rc = sprintf_s(cmd, sizeof(cmd), "ip neighbour add %s lladdr %s dev %s",
                                address.c_str(),
                                llAddr.c_str(),
                                deviceName.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Removes a neighbour from the neighbour table via:
 * "ip neighbour delete <IPv4/6 address> dev <device>"
 *
 * @return true if the function succeeded and false otherwise
 */
bool deleteNeighbourSystemCommand(const std::string& address,
                                  const std::string& deviceName)
{
    int status = -1;
    char cmd[256] = {0};
    errno_t rc = -1;

    rc = sprintf_s(cmd, sizeof(cmd), "ip neighbour delete %s dev %s",
                                address.c_str(),
                                deviceName.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

/**
 * Removes a neighbour from the neighbour table via:
 * "ip neighbour delete <IPv4/6 address> lladdr <link-layer address> dev <device>"
 *
 * @return true if the function succeeded and false otherwise
 */
bool deleteNeighbourSystemCommand(const std::string& address,
                                  const std::string& llAddr,
                                  const std::string& deviceName)
{
    int status = -1;
    char cmd[256] = {0};
    errno_t rc = -1;

    rc = sprintf_s(cmd, sizeof(cmd), "ip neighbour delete %s lladdr %s dev %s",
                                address.c_str(),
                                llAddr.c_str(),
                                deviceName.c_str());
    ERR_CHK(rc);

    executeSystemCommand(cmd, &status);
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}
