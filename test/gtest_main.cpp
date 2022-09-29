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
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "TestEnvironment.hpp"

#include <signal.h>
#include <setjmp.h>

extern "C" {
    #include "safec_lib_common.h"
}

#define GTEST_REPORT_FILEPATH   "/tmp/Gtest_Report/libnet_gtest_report.xml"
#define GTEST_REPORT_FILEPATH_SIZE 128

std::vector<std::string> g_command_line_arg;
long counter = 0;
sigjmp_buf point;

static void signal_handler(int sig, siginfo_t *info, void *args)
{
    longjmp(point, counter + 1);
}

GTEST_API_ int main(int argc, char *argv[])
{
    char filePath[GTEST_REPORT_FILEPATH_SIZE] = {0}; /* Test Results Full File Path */
    errno_t rc = -1;
    struct sigaction sa;

    snprintf(filePath, GTEST_REPORT_FILEPATH_SIZE, "xml:%s", GTEST_REPORT_FILEPATH);
    ::testing::GTEST_FLAG(output) = filePath;
    ::testing::InitGoogleMock(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new TestEnvironment(argc, argv));

    if (g_command_line_arg.size() == 1 && g_command_line_arg[0] == "--help") {
        // When called from the corresponding binary
        std::cout << "Usage: " << argv[0] << " <TEST_INTERFACE_NAME> <TEST_INTERFACE_TYPE> <TEST_VLAN_ID> <TEST_BRIDGE_NAME> <EPH_INTERFACE_NAME> <EPH_INTERFACE_TYPE>" << std::endl;
        return 0;
    }

    rc = memset_s(&sa, sizeof(struct sigaction), 0, sizeof(struct sigaction));
    ERR_CHK(rc);

    sigemptyset(&sa.sa_mask);

    sa.sa_flags = SA_NODEFER;
    sa.sa_sigaction = signal_handler;

    sigaction(SIGSEGV, &sa, NULL);

    return RUN_ALL_TESTS();
}
