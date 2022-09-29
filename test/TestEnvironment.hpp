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
#ifndef _TEST_ENVIRONMENT_HPP
#define _TEST_ENVIRONMENT_HPP

#include <stdio.h>
#include <stdlib.h>
#include <gmock/gmock.h>

#include "TestUtils.hpp"

extern std::vector<std::string> g_command_line_arg;
extern long counter;

class TestEnvironment : public testing::Environment {
public:
    virtual ~TestEnvironment() = default;

    virtual void SetUp() {
        std::cout << "Setting up Test Environment....\n";

        // Set the longjmp counter to 0
        counter = 0;

        int status = -1;
        std::string cmdOutput = executeSystemCommand(std::string("modprobe dummy"), &status);
        EXPECT_EQ(WEXITSTATUS(status), EXIT_SUCCESS) << "Failed to insert dummy module. Assuming module exists already....";

        //ASSERT_EQ(0, system("corenetlib-setup.sh"));
    }

    virtual void TearDown() {
        std::cout << "Tearing down Test Environment....\n";

        int status = -1;
        std::string cmdOutput = executeSystemCommand(std::string("rmmod dummy"), &status);
        EXPECT_EQ(WEXITSTATUS(status), EXIT_SUCCESS) << "Failed to remove dummy module";
        //ASSERT_EQ(0, system("corenetlib-teardown.sh"));
    }

    explicit TestEnvironment(int argc, char **argv) {
        g_command_line_arg.clear();
        for (int i=1; i<argc; i++) {
            g_command_line_arg.emplace_back(std::string(argv[i]));
        }
    }
};

#endif

