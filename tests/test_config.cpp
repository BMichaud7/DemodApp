/*
========================================================================
Project: OpenRFStack
Author:  Brendan Michaud
Year:    2026
Part of OpenRFStack (https://github.com/OpenRFStack)

Licensed under the Personal Use License.
Do not use for commercial, organizational, or military purposes.
Contact author for permission: https://github.com/OpenRFStack
========================================================================
*/
#include <gtest/gtest.h>
#include "AppConfig.hpp"
#include <cstdio>
#include <fstream>

using namespace demod;

TEST(AppConfig, ParsesXmlDefaults) {
    // Write a temp XML config file
    const char* path = "/tmp/test_demod_config.xml";
    {
        std::ofstream f(path);
        f << R"(<?xml version="1.0"?><demod_config>
    <broker><url>amqp://test:5672</url></broker>
</demod_config>)";
    }

    AppConfig cfg = AppConfig::fromXml(path);
    EXPECT_EQ(cfg.broker.url, "amqp://test:5672");
    EXPECT_FLOAT_EQ(cfg.engine.min_confidence, 0.70f);

    std::remove(path);
}

/*
========================================================================
End of file — OpenRFStack
Subject to Personal Use License
https://github.com/OpenRFStack
========================================================================
*/
