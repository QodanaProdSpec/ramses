//  -------------------------------------------------------------------------
//  Copyright (C) 2017 BMW Car IT GmbH
//  -------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at https://mozilla.org/MPL/2.0/.
//  -------------------------------------------------------------------------

#include "ClientCommands/SceneCommandBuffer.h"
#include "Utils/ThreadBarrier.h"
#include "gmock/gmock.h"
#include <thread>

namespace ramses_internal
{
    using namespace testing;

    namespace
    {
        class MockSceneCommandVisitor
        {
        public:
            void operator()(const SceneCommandForceFallback& cmd)
            {
                handleSceneCommandForceFallback(cmd);
            }
            void operator()(const SceneCommandFlushSceneVersion& cmd)
            {
                handleSceneCommandFlushSceneVersion(cmd);
            }
            void operator()(const SceneCommandValidationRequest& cmd)
            {
                handleSceneCommandValidationRequest(cmd);
            }
            void operator()(const SceneCommandDumpSceneToFile& cmd)
            {
                handleSceneCommandDumpSceneToFile(cmd);
            }
            void operator()(const SceneCommandLogResourceMemoryUsage& cmd)
            {
                handleSceneCommandLogResourceMemoryUsage(cmd);
            }

            MOCK_METHOD(void, handleSceneCommandForceFallback, (const SceneCommandForceFallback&));
            MOCK_METHOD(void, handleSceneCommandFlushSceneVersion, (const SceneCommandFlushSceneVersion&));
            MOCK_METHOD(void, handleSceneCommandValidationRequest, (const SceneCommandValidationRequest&));
            MOCK_METHOD(void, handleSceneCommandDumpSceneToFile, (const SceneCommandDumpSceneToFile&), (const));
            MOCK_METHOD(void, handleSceneCommandLogResourceMemoryUsage, (const SceneCommandLogResourceMemoryUsage&), (const));
        };

    }

    // local comparison operators
    static bool operator==(const SceneCommandForceFallback& a, const SceneCommandForceFallback& b)
    {
        return a.streamTextureName == b.streamTextureName && a.forceFallback == b.forceFallback;
    }

    static bool operator==(const SceneCommandFlushSceneVersion& a, const SceneCommandFlushSceneVersion& b)
    {
        return a.sceneVersion == b.sceneVersion;
    }

    static bool operator==(const SceneCommandValidationRequest& a, const SceneCommandValidationRequest& b)
    {
        return a.severity == b.severity && a.optionalObjectName == b.optionalObjectName;
    }

    static bool operator==(const SceneCommandDumpSceneToFile& a, const SceneCommandDumpSceneToFile& b)
    {
        return a.fileName == b.fileName && a.sendViaDLT == b.sendViaDLT;
    }

    static bool operator==(const SceneCommandLogResourceMemoryUsage&, const SceneCommandLogResourceMemoryUsage&)
    {
        return true;
    }


    class ASceneCommandBuffer : public Test
    {
    public:
        StrictMock<MockSceneCommandVisitor> visitor;
        SceneCommandBuffer buffer;
    };

    TEST_F(ASceneCommandBuffer, canUseAllCommands)
    {
        InSequence seq;
        {
            SceneCommandForceFallback cmd{"foo", true};
            buffer.enqueueCommand(cmd);
            EXPECT_CALL(visitor, handleSceneCommandForceFallback(cmd));
        }
        {
            SceneCommandFlushSceneVersion cmd{12345u};
            buffer.enqueueCommand(cmd);
            EXPECT_CALL(visitor, handleSceneCommandFlushSceneVersion(cmd));
        }
        {
            SceneCommandValidationRequest cmd{ramses::EValidationSeverity_Error, "bar"};
            buffer.enqueueCommand(cmd);
            EXPECT_CALL(visitor, handleSceneCommandValidationRequest(cmd));
        }
        {
            SceneCommandDumpSceneToFile cmd{"somename", false};
            buffer.enqueueCommand(cmd);
            EXPECT_CALL(visitor, handleSceneCommandDumpSceneToFile(cmd));
        }
        {
            SceneCommandLogResourceMemoryUsage cmd{};
            buffer.enqueueCommand(cmd);
            EXPECT_CALL(visitor, handleSceneCommandLogResourceMemoryUsage(cmd));
        }
        buffer.execute(std::ref(visitor));
    }

    TEST_F(ASceneCommandBuffer, canUseSameCommandMultipleTimes)
    {
        buffer.enqueueCommand(SceneCommandFlushSceneVersion{1});
        buffer.enqueueCommand(SceneCommandFlushSceneVersion{2});
        buffer.enqueueCommand(SceneCommandFlushSceneVersion{3});
        buffer.enqueueCommand(SceneCommandFlushSceneVersion{4});

        InSequence seq;
        EXPECT_CALL(visitor, handleSceneCommandFlushSceneVersion(SceneCommandFlushSceneVersion{1}));
        EXPECT_CALL(visitor, handleSceneCommandFlushSceneVersion(SceneCommandFlushSceneVersion{2}));
        EXPECT_CALL(visitor, handleSceneCommandFlushSceneVersion(SceneCommandFlushSceneVersion{3}));
        EXPECT_CALL(visitor, handleSceneCommandFlushSceneVersion(SceneCommandFlushSceneVersion{4}));
        buffer.execute(std::ref(visitor));
    }

    TEST_F(ASceneCommandBuffer, canBeUsedFromMultipleThreads)
    {
        ThreadBarrier setupDoneBarrier(3);
        ThreadBarrier writersDoneBarrier(3);
        ThreadBarrier allDone(3);

        std::thread t1([&]() {
                           setupDoneBarrier.wait();
                           buffer.enqueueCommand(SceneCommandForceFallback{"foo", true});
                           buffer.enqueueCommand(SceneCommandFlushSceneVersion{12345u});
                           writersDoneBarrier.wait();
                           allDone.wait();
                       });
        std::thread t2([&]() {
                           setupDoneBarrier.wait();
                           buffer.enqueueCommand(SceneCommandValidationRequest{ramses::EValidationSeverity_Error, "bar"});
                           buffer.enqueueCommand(SceneCommandDumpSceneToFile{"somename", false});
                           buffer.enqueueCommand(SceneCommandLogResourceMemoryUsage{});
                           writersDoneBarrier.wait();
                           allDone.wait();
                       });
        std::thread t3([&]() {
                           Sequence seqT1;
                           Sequence seqT2;
                           EXPECT_CALL(visitor, handleSceneCommandForceFallback(SceneCommandForceFallback{"foo", true})).InSequence(seqT1);
                           EXPECT_CALL(visitor, handleSceneCommandFlushSceneVersion(SceneCommandFlushSceneVersion{12345u})).InSequence(seqT1);
                           EXPECT_CALL(visitor, handleSceneCommandValidationRequest(SceneCommandValidationRequest{ramses::EValidationSeverity_Error, "bar"})).InSequence(seqT2);
                           EXPECT_CALL(visitor, handleSceneCommandDumpSceneToFile(SceneCommandDumpSceneToFile{"somename", false})).InSequence(seqT2);
                           EXPECT_CALL(visitor, handleSceneCommandLogResourceMemoryUsage(SceneCommandLogResourceMemoryUsage{})).InSequence(seqT2);
                           setupDoneBarrier.wait();
                           writersDoneBarrier.wait();
                           buffer.execute(std::ref(visitor));
                           allDone.wait();
                       });
        t1.join();
        t2.join();
        t3.join();
    }
}
