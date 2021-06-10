//  -------------------------------------------------------------------------
//  Copyright (C) 2015 BMW Car IT GmbH
//  -------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at https://mozilla.org/MPL/2.0/.
//  -------------------------------------------------------------------------

#include "Utils/LogMacros.h"
#include "framework_common_gmock_header.h"
#include "Resource/ResourceBase.h"
#include "Utils/ThreadBarrier.h"
#include "gtest/gtest.h"
#include <memory>
#include <numeric>
#include <random>
#include <thread>

namespace ramses_internal
{
    namespace
    {
        class TestResource : public ResourceBase
        {
        public:
            explicit TestResource(EResourceType typeID, ResourceCacheFlag cacheFlag, const String& name)
                : ResourceBase(typeID, cacheFlag, name)
            {}

            virtual void serializeResourceMetadataToStream(IOutputStream&) const override {}
        };

        class ResourceCompression : public ::testing::TestWithParam<IResource::CompressionLevel>
        {
        };

        class DummyResource : public ResourceBase
        {
        public:
            explicit DummyResource(uint32_t metadata = 0, const String& name = String())
                : ResourceBase(EResourceType_Invalid, ResourceCacheFlag(15u), name)
                , m_metadata(metadata)
            {}

            virtual void serializeResourceMetadataToStream(IOutputStream& output) const override
            {
                output << m_metadata;
            }

        private:
            uint32_t m_metadata;
        };
    }

    INSTANTIATE_TEST_SUITE_P(AResourceTest,
                            ResourceCompression,
                            ::testing::Values(IResource::CompressionLevel::Realtime,
                                              IResource::CompressionLevel::Offline));

    TEST_P(ResourceCompression, compressUncompressGivesInitialDataForSmallSizes)
    {
        for (UInt32 dataSize = 1001; dataSize < 2002; ++dataSize)
        {
            SCOPED_TRACE(dataSize);

            TestResource res(EResourceType_Invalid, ResourceCacheFlag(0), String());
            ResourceBlob data(dataSize);
            for (UInt32 idx = 0; idx < dataSize; ++idx)
            {
                data.data()[idx] = static_cast<UInt8>(idx+1);
            }
            res.setResourceData(ResourceBlob(data.size(), data.data()));  // copy data
            res.compress(GetParam());

            TestResource resFromCompressed(EResourceType_Invalid, ResourceCacheFlag(0), String());
            resFromCompressed.setCompressedResourceData(CompressedResourceBlob(res.getCompressedResourceData().size(), res.getCompressedResourceData().data()),
                                                        IResource::CompressionLevel::Realtime, res.getDecompressedDataSize(), res.getHash());
            resFromCompressed.decompress();

            EXPECT_EQ(data.span(), resFromCompressed.getResourceData().span());
        }
    }

    TEST_P(ResourceCompression, noCompressionForSmallSizes)
    {
        for (UInt32 dataSize = 1; dataSize < 1001; ++dataSize)
        {
            SCOPED_TRACE(dataSize);
            TestResource res(EResourceType_Invalid, ResourceCacheFlag(0), String());
            res.setResourceData(ResourceBlob(dataSize));
            res.compress(GetParam());
            EXPECT_FALSE(res.isCompressedAvailable());
        }
    }

    class AResource : public ::testing::Test
    {
    public:
        AResource()
            : zeroBlobA(2048)
            , zeroBlobB(2048)
            , compressedBlob(10)
        {
            zeroBlobA.setZero();
            zeroBlobB.setZero();
            compressedBlob.setZero();
        }

        ResourceBlob zeroBlobA;
        ResourceBlob zeroBlobB;
        CompressedResourceBlob compressedBlob;
    };

    TEST_F(AResource, hasZeroSizesByDefault)
    {
        TestResource emptyRes(EResourceType_Invalid, ResourceCacheFlag(0), String());
        EXPECT_EQ(0u, emptyRes.getDecompressedDataSize());
        EXPECT_EQ(0u, emptyRes.getCompressedDataSize());
    }

    TEST_F(AResource, noCompressionForCompressionLevelNone)
    {
        for (UInt32 dataSize = 1; dataSize < 2000; ++dataSize)
        {
            SCOPED_TRACE(dataSize);
            TestResource res(EResourceType_Invalid, ResourceCacheFlag(0), String());
            res.setResourceData(ResourceBlob(dataSize));
            res.compress(IResource::CompressionLevel::None);
            EXPECT_FALSE(res.isCompressedAvailable());
        }
    }

    TEST_F(AResource, canGetEmptyName)
    {
        TestResource emptyNameRes(EResourceType_Invalid, ResourceCacheFlag(0), String());
        EXPECT_EQ(String(), emptyNameRes.getName());
    }

    TEST_F(AResource, canGetNonEmptyName)
    {
        TestResource nonEmptyNameRes(EResourceType_Invalid, ResourceCacheFlag(0), String("foobar"));
        EXPECT_EQ(String("foobar"), nonEmptyNameRes.getName());
    }

    TEST_F(AResource, givesSameHashForDifferentNames)
    {
        TestResource noNameRes(EResourceType_Invalid, ResourceCacheFlag(0), "");
        TestResource namedRes(EResourceType_Invalid, ResourceCacheFlag(0), "some name");
        TestResource otherNamedRes(EResourceType_Invalid, ResourceCacheFlag(0), "other name");

        EXPECT_EQ(noNameRes.getHash(), namedRes.getHash());
        EXPECT_EQ(noNameRes.getHash(), otherNamedRes.getHash());
    }

    TEST_F(AResource, canGetType)
    {
        TestResource res(EResourceType_Effect, ResourceCacheFlag(0), String());
        EXPECT_EQ(EResourceType_Effect, res.getTypeID());
    }

    TEST_F(AResource, canGetCacheFlag)
    {
        TestResource res(EResourceType_Invalid, ResourceCacheFlag(11), String());
        EXPECT_EQ(ResourceCacheFlag(11), res.getCacheFlag());
    }

    TEST_F(AResource, returnsInvalidHashForEmptyResources)
    {
        DummyResource res;
        EXPECT_EQ(ResourceContentHash::Invalid(), res.getHash());
    }

    TEST_F(AResource, hasGivenHashWhenExplicitlySet)
    {
        DummyResource res;
        ResourceContentHash someHash(1234568, 0);
        res.setResourceData(std::move(zeroBlobA), someHash);
        EXPECT_EQ(someHash, res.getHash());
    }

    TEST_F(AResource, hasGivenHashWhenExplicitlySetForCompressed)
    {
        DummyResource res;
        ResourceContentHash someHash(1234568, 0);
        res.setCompressedResourceData(std::move(compressedBlob), IResource::CompressionLevel::Realtime, 1, someHash);
        EXPECT_EQ(someHash, res.getHash());
    }

    TEST_F(AResource, calculatesValidHashWhenNoneSet)
    {
        DummyResource res;
        res.setResourceData(std::move(zeroBlobA));
        EXPECT_NE(ResourceContentHash::Invalid(), res.getHash());
    }


    TEST_F(AResource, hashChangesWhenContentChanges)
    {
        DummyResource res;
        res.setResourceData(std::move(zeroBlobA));
        const ResourceContentHash hash = res.getHash();
        zeroBlobB.data()[0] = 1;
        res.setResourceData(std::move(zeroBlobB));
        EXPECT_NE(hash, res.getHash());
    }

    TEST_F(AResource, givesSameHashForSameContent)
    {
        DummyResource resA;
        resA.setResourceData(std::move(zeroBlobA));
        DummyResource resB;
        resB.setResourceData(std::move(zeroBlobB));

        EXPECT_EQ(resA.getHash(), resB.getHash());
    }

    TEST_F(AResource, hashIsDifferentForSameContentButDifferentMetadata)
    {
        DummyResource resA(1);
        resA.setResourceData(std::move(zeroBlobA));

        DummyResource resB(2);
        resB.setResourceData(std::move(zeroBlobB));

        EXPECT_NE(resA.getHash(), resB.getHash());
    }

    TEST_F(AResource, contentSameAfterCompressDecompress)
    {
        DummyResource resA;
        ResourceBlob blob(4096);
        std::iota(blob.data(), blob.data() + blob.size(), static_cast<uint8_t>(10));
        resA.setResourceData(std::move(blob));
        resA.compress(IResource::CompressionLevel::Realtime);
        ASSERT_TRUE(resA.isCompressedAvailable());
        ASSERT_TRUE(resA.isDeCompressedAvailable());

        const CompressedResourceBlob& compBlobA = resA.getCompressedResourceData();
        DummyResource resB;
        resB.setCompressedResourceData(CompressedResourceBlob(compBlobA.size(), compBlobA.data()), IResource::CompressionLevel::Realtime, resA.getDecompressedDataSize(), resA.getHash());
        EXPECT_FALSE(resB.isDeCompressedAvailable());
        resB.decompress();
        ASSERT_TRUE(resB.isDeCompressedAvailable());
        EXPECT_TRUE(resB.isCompressedAvailable());

        ASSERT_EQ(resA.getDecompressedDataSize(), resB.getDecompressedDataSize());
        EXPECT_EQ(0, std::memcmp(resA.getResourceData().data(), resB.getResourceData().data(), resA.getDecompressedDataSize()));
    }

    TEST_F(AResource, canCompressDecompressSameResource)
    {
        DummyResource resA(1);
        resA.setResourceData(std::move(zeroBlobA));
        resA.compress(IResource::CompressionLevel::Realtime);
        resA.decompress();
        EXPECT_TRUE(resA.isCompressedAvailable());
        EXPECT_TRUE(resA.isDeCompressedAvailable());
    }

    TEST_F(AResource, canOverwriteRealtimeCompressionWithOfflineCompressionButNotViceVersa)
    {
        // the following parameters will generate a blob which compresses differently with Offline and Realtime compression
        std::mt19937 gen(123456); // NOLINT(cert-msc32-c,cert-msc51-cpp) we do want deterministically created values
        std::uniform_int_distribution<unsigned short> dis(0, 32);
        std::vector<Byte> nonTrivialData(4096);
        std::generate(nonTrivialData.begin(), nonTrivialData.end(), [&](){ return static_cast<Byte>(dis(gen)); });

        DummyResource resA;
        resA.setResourceData(ResourceBlob{ nonTrivialData.size(), nonTrivialData.data() });

        DummyResource resB;
        resB.setResourceData(ResourceBlob{ nonTrivialData.size(), nonTrivialData.data() });

        resA.compress(IResource::CompressionLevel::Realtime);
        resB.compress(IResource::CompressionLevel::Offline);
        EXPECT_NE(resA.getCompressedResourceData().span(), resB.getCompressedResourceData().span());
        resA.compress(IResource::CompressionLevel::Offline);
        EXPECT_EQ(resA.getCompressedResourceData().span(), resB.getCompressedResourceData().span());
        resA.compress(IResource::CompressionLevel::Realtime);
        EXPECT_EQ(resA.getCompressedResourceData().span(), resB.getCompressedResourceData().span());
    }

    TEST_F(AResource, canBeCompressedAgainAfterSettingNewResourceData)
    {
        DummyResource resA(1);
        resA.setResourceData(std::move(zeroBlobA));
        resA.compress(IResource::CompressionLevel::Offline);
        EXPECT_TRUE(resA.isCompressedAvailable());
        resA.setResourceData(std::move(zeroBlobB));
        EXPECT_FALSE(resA.isCompressedAvailable());
        resA.compress(IResource::CompressionLevel::Realtime);
        EXPECT_TRUE(resA.isCompressedAvailable());
    }

    TEST_F(AResource, canBeCompressedAgainAfterSettingNewResourceDataWithHash)
    {
        DummyResource resA(1);
        resA.setResourceData(std::move(zeroBlobA));
        resA.compress(IResource::CompressionLevel::Offline);
        EXPECT_TRUE(resA.isCompressedAvailable());
        resA.setResourceData(std::move(zeroBlobB), ResourceContentHash{ 1, 1 });
        EXPECT_FALSE(resA.isCompressedAvailable());
        resA.compress(IResource::CompressionLevel::Realtime);
        EXPECT_TRUE(resA.isCompressedAvailable());
    }

    TEST_F(AResource, ordersCompressionLevelsCorrectly)
    {
        EXPECT_GT(IResource::CompressionLevel::Realtime, IResource::CompressionLevel::None);
        EXPECT_GT(IResource::CompressionLevel::Offline, IResource::CompressionLevel::Realtime);
    }

    class AResourceThreaded : public ::testing::Test
    {
    public:
        AResourceThreaded()
        {
            constexpr size_t numResources = 100;
            constexpr size_t resourceSize = 2000;
            for (size_t i = 0; i < numResources; ++i)
            {
                auto res = std::make_unique<TestResource>(EResourceType_Effect, ResourceCacheFlag(0), "");
                ResourceBlob blob(resourceSize);
                std::iota(blob.data(), blob.data() + blob.size(), static_cast<uint8_t>(10));
                res->setResourceData(std::move(blob));
                resources.push_back(std::move(res));
            }
        }

        void run()
        {
            std::vector<std::thread> threads(funcs.size());
            ThreadBarrier startBarrier(static_cast<uint32_t>(threads.size()));
            for (size_t i = 0; i < threads.size(); ++i)
                threads[i] = std::thread([func = std::move(funcs[i]), &startBarrier]() {
                    startBarrier.wait();
                    func();
                });
            for (auto& t : threads)
                t.join();
        }

        void makeCompressedOnly()
        {
            for (size_t i = 0; i < resources.size(); ++i)
            {
                auto& res = resources[i];
                res->compress(IResource::CompressionLevel::Realtime);
                auto compressedRes = std::make_unique<TestResource>(res->getTypeID(), res->getCacheFlag(), res->getName());
                CompressedResourceBlob compressedData(res->getCompressedResourceData().size(), res->getCompressedResourceData().data());
                compressedRes->setCompressedResourceData(std::move(compressedData), IResource::CompressionLevel::Realtime,
                                                         res->getDecompressedDataSize(), res->getHash());
                resources[i] = std::move(compressedRes);
            }
        }

        std::vector<std::unique_ptr<TestResource>> resources;
        std::vector<std::function<void()>> funcs;
    };

    TEST_F(AResourceThreaded, simultaneousCompression)
    {
        for (size_t i = 0; i < 2; ++i)
            funcs.push_back([&]() {
                size_t value = 0;
                for (auto& res : resources)
                {
                    res->compress(IResource::CompressionLevel::Realtime);
                    value += res->getCompressedResourceData().data()[0];
                }
                LOG_DEBUG(CONTEXT_FRAMEWORK, "Test result " << value);
            });
        run();
    }

    TEST_F(AResourceThreaded, simultaneousDecompression)
    {
        makeCompressedOnly();
        for (size_t i = 0; i < 2; ++i)
            funcs.push_back([&]() {
                size_t value = 0;
                for (auto& res : resources)
                {
                    res->decompress();
                    value += res->getResourceData().data()[0];
                    value += res->getDecompressedDataSize();
                }
                LOG_DEBUG(CONTEXT_FRAMEWORK, "Test result " << value);
            });
        run();
    }

    TEST_F(AResourceThreaded, simultaneousCompressAndRead)
    {
        for (size_t i = 0; i < 2; ++i)
            funcs.push_back([&]() {
                size_t value = 0;
                for (auto& res : resources)
                {
                    value += res->getCompressedDataSize();
                    value += res->isCompressedAvailable() ? 1: 0;
                    if (res->isCompressedAvailable())
                        value += res->getCompressedResourceData().size();
                    else
                        res->compress(IResource::CompressionLevel::Realtime);
                }
                LOG_DEBUG(CONTEXT_FRAMEWORK, "Test result " << value);
            });
        run();
    }

    TEST_F(AResourceThreaded, simultaneousRecompressAndDecompress)
    {
        makeCompressedOnly();
        funcs.push_back([&]() {
            for (auto& res : resources)
                res->compress(IResource::CompressionLevel::Offline);
        });
        funcs.push_back([&]() {
            size_t value = 0;
            for (auto& res : resources)
            {
                res->decompress();
                value += res->getResourceData().data()[0];
                value += res->getDecompressedDataSize();
            }
            LOG_DEBUG(CONTEXT_FRAMEWORK, "Test result " << value);
        });
        run();
    }
}
