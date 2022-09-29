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
#ifndef _TEST_UTILS_HPP
#define _TEST_UTILS_HPP

#include <string>
#include <vector>

#ifndef __LINUX_NEIGHBOUR_H
#define NUD_INCOMPLETE   0x01
#define NUD_REACHABLE    0x02
#define NUD_STALE        0x04
#define NUD_DELAY        0x08
#define NUD_PROBE        0x10
#define NUD_FAILED       0x20
#define NUD_NOARP        0x40
#define NUD_PERMANENT    0x80
#define NUD_NONE         0x00
#endif

typedef struct {
    int state;
    std::string local;
    std::string mac;
} TestNeighbourInfo;

void msleep(long msec);
std::vector<std::string> splitString(const std::string& input, const char delim);
bool findStringWithOccurrences(const std::string& haystack, const std::string& needle, int numOccurrences);
bool fileExists(const std::string& filePath);
std::vector<std::string> listDirectoryFiles(const std::string& path);

std::string getNetMaskFromCIDRAddress(const std::string& CIDRAddress);
int getCIDRFromNetMask(const std::string& netMask);

std::string executeSystemCommand(char *cmd, int *status);
std::string executeSystemCommand(const std::string& cmd, int *status);
std::string executeSystemCommand(const std::string& cmd, const std::string& logFile, int *status);

std::string getVlanName(const std::string& interfaceName, const int vlanId);

bool createVlanSystemCommand(const std::string& interfaceName, const int vlanId);
bool deleteVlanSystemCommand(const std::string& vlanName);

bool createBridgeSystemCommand(const std::string& bridgeName);
bool deleteBridgeSystemCommand(const std::string& bridgeName);

bool createInterfaceSystemCommand(const std::string& interfaceName, const std::string& interfaceType);
bool createInterfaceSystemCommand(const std::string& interfaceName, const std::string& interfaceType, const std::string& ipAddress);
bool deleteInterfaceSystemCommand(const std::string& interfaceName);

std::string getLinkStateSystemCommand(const std::string& interfaceName);
int linkStatusUpOrDownSystemCommand(const std::string& interfaceName);

bool setInterfaceUpSystemCommand(const std::string& interfaceName);
bool setInterfaceDownSystemCommand(const std::string& interfaceName);

bool interfaceExistsSystemCommand(const std::string& interfaceName);

std::string interfaceGetMacSystemCommand(const std::string& interfaceName);
bool interfaceSetMacSystemCommand(const std::string& interfaceName, const std::string& macAddress);

std::string interfaceGetIpSystemCommand(const std::string& interfaceName);

int getBridgeSTPStateSystemCommand(const std::string& bridgeName);
bool setBridgeSTPStateSystemCommand(const std::string& bridgeName, bool onOrOff);

std::string interfaceGetNetmaskSystemCommand(const std::string& interfaceName);
bool interfaceSetNetmaskSystemCommand(const std::string& interfaceName, const std::string& netmask);

int interfaceGetMTUSystemCommand(const std::string& interfaceName);

bool interfaceAddToBridgeSystemCommand(const std::string& interfaceName, const std::string& bridgeName);
bool interfaceRemoveFromBridgeSystemCommand(const std::string& interfaceName, const std::string& bridgeName);

bool addIpAddrSystemCommand(const std::string& ipAddr,
                            int cidr,
                            const std::string& deviceName);
bool deleteIpAddrSystemCommand(const std::string& ipAddr,
                               int cidr,
                               const std::string& deviceName);

bool addIpRouteSystemCommand(const std::string& dstPrefix,
                             const std::string& nextHopAddr,
                             const std::string& deviceName);
bool addIpRouteSystemCommand(const std::string& dstPrefix,
                             const std::string& nextHopAddr,
                             const std::string& deviceName,
                             const int metric);

bool removeIpRouteSystemCommand(const std::string& dstPrefix,
                                const std::string& nextHopAddr,
                                const std::string& deviceName);

bool addIpRuleSystemCommand(const std::string& fromPrefix,
                            const std::string& toPrefix,
                            const std::string& tableName);

bool removeIpRuleSystemCommand(const std::string& fromPrefix,
                               const std::string& toPrefix,
                               const std::string& tableName);

bool addIpTunnelSystemCommand(const std::string& tunnelName,
                              const std::string& tunnelMode,
                              const std::string& localV6Addr,
                              const std::string& remoteV6Addr,
                              const int encapLimit);
bool addIpTunnelSystemCommand(const std::string& tunnelName,
                              const std::string& tunnelMode,
                              const std::string& deviceName,
                              const std::string& localV6Addr,
                              const std::string& remoteV6Addr,
                              const int encapLimit);
bool deleteIpTunnelSystemCommand(const std::string& tunnelName);

bool addNeighbourSystemCommand(const std::string& address,
                               const std::string& llAddr,
                               const std::string& deviceName);
bool addNeighbourSystemCommand(const std::string& address,
                               const std::string& deviceName);

bool deleteNeighbourSystemCommand(const std::string& address,
                                  const std::string& deviceName);
bool deleteNeighbourSystemCommand(const std::string& address,
                                  const std::string& llAddr,
                                  const std::string& deviceName);

bool getNeighboursSystemCommand(std::vector<TestNeighbourInfo *>& neighbours);
void cleanupNeighboursList(std::vector<TestNeighbourInfo *>& neighbours);

#endif
