//  -------------------------------------------------------------------------
//  Copyright (C) 2017 BMW Car IT GmbH
//  -------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at https://mozilla.org/MPL/2.0/.
//  -------------------------------------------------------------------------

#pragma once

#include "internal/SceneGraph/Scene/ResourceChanges.h"
#include "internal/SceneGraph/SceneAPI/Renderable.h"
#include "internal/SceneGraph/SceneAPI/TextureSampler.h"
#include "internal/SceneGraph/SceneAPI/GeometryDataBuffer.h"
#include "internal/SceneGraph/SceneAPI/TextureBuffer.h"
#include "internal/SceneGraph/Scene/DataLayout.h"

namespace ramses::internal
{
    class IScene;

    // Some functions are templates to avoid using virtual IScene interface (even forbidden where possible) for performance reasons
    namespace ResourceUtils
    {
        template <typename SCENE, typename std::enable_if < !std::is_abstract<SCENE>{}, int > ::type = 0 >
        void GetAllResourcesFromScene(ResourceContentHashVector& resources, const SCENE& scene)
        {
            auto pushBackIfValid = [](ResourceContentHashVector& vec, ResourceContentHash const& hash)
            {
                if (hash.isValid())
                    vec.push_back(hash);
            };

            assert(resources.empty());
            for (const auto& rendIt : scene.getRenderables())
            {
                const auto& renderable = *rendIt.second;
                if (renderable.visibilityMode == EVisibilityMode::Off)
                    continue;

                for (auto type : { ERenderableDataSlotType_Geometry, ERenderableDataSlotType_Uniforms })
                {
                    const auto instanceHandle = renderable.dataInstances[type];
                    if (!instanceHandle.isValid() || !scene.isDataInstanceAllocated(instanceHandle))
                        continue;

                    const auto& layout = scene.getDataLayout(scene.getLayoutOfDataInstance(instanceHandle));
                    pushBackIfValid(resources, layout.getEffectHash());

                    for (DataFieldHandle fieldHandle(0u); fieldHandle < layout.getFieldCount(); ++fieldHandle)
                    {
                        const EDataType fieldType = layout.getField(fieldHandle).dataType;
                        if (IsBufferDataType(fieldType))
                        {
                            pushBackIfValid(resources, scene.getDataResource(instanceHandle, fieldHandle).hash);
                        }
                        else if (IsTextureSamplerType(fieldType))
                        {
                            const auto samplerHandle = scene.getDataTextureSamplerHandle(instanceHandle, fieldHandle);
                            if (samplerHandle.isValid() && scene.isTextureSamplerAllocated(samplerHandle))
                            {
                                pushBackIfValid(resources, scene.getTextureSampler(samplerHandle).textureResource);
                            }
                        }
                    }
                }
            }

            for (const auto& dataSlotIt : scene.getDataSlots())
                pushBackIfValid(resources, dataSlotIt.second->attachedTexture);

            std::sort(resources.begin(), resources.end());
            resources.erase(std::unique(resources.begin(), resources.end()), resources.end());
        }

        template <typename SCENE>
        void GetAllSceneResourcesFromScene(SceneResourceActionVector& actions, const SCENE& scene, size_t& usedDataByteSize)
        {
            assert(actions.empty());

            // collect all scene resources currently in use by scene and mark as pending to be uploaded
            const uint32_t numRenderBuffers = scene.getRenderBufferCount();
            const uint32_t numRenderTargets = scene.getRenderTargetCount();
            const uint32_t numBlitPasses = scene.getBlitPassCount();
            const uint32_t numDataBuffers = scene.getDataBufferCount();
            const uint32_t numUniformBuffers = scene.getUniformBufferCount();
            const uint32_t numTextureBuffers = scene.getTextureBufferCount();
            const uint32_t numSceneResources = numRenderTargets + numRenderBuffers + numBlitPasses + numDataBuffers * 2u + numTextureBuffers * 2u + numUniformBuffers * 2u;

            actions.reserve(numSceneResources);
            usedDataByteSize = 0u;

            for (RenderBufferHandle rbHandle(0u); rbHandle < numRenderBuffers; ++rbHandle)
            {
                if (scene.isRenderBufferAllocated(rbHandle))
                    actions.push_back({ rbHandle.asMemoryHandle(), ESceneResourceAction_CreateRenderBuffer });
            }
            for (RenderTargetHandle rtHandle(0u); rtHandle < numRenderTargets; ++rtHandle)
            {
                if (scene.isRenderTargetAllocated(rtHandle))
                    actions.push_back({ rtHandle.asMemoryHandle(), ESceneResourceAction_CreateRenderTarget });
            }
            for (BlitPassHandle bpHandle(0u); bpHandle < numBlitPasses; ++bpHandle)
            {
                if (scene.isBlitPassAllocated(bpHandle))
                    actions.push_back({ bpHandle.asMemoryHandle(), ESceneResourceAction_CreateBlitPass });
            }

            for (DataBufferHandle dbHandle(0u); dbHandle < numDataBuffers; ++dbHandle)
            {
                if (scene.isDataBufferAllocated(dbHandle))
                {
                    actions.push_back({ dbHandle.asMemoryHandle(), ESceneResourceAction_CreateDataBuffer });
                    actions.push_back({ dbHandle.asMemoryHandle(), ESceneResourceAction_UpdateDataBuffer });
                    usedDataByteSize += scene.getDataBuffer(dbHandle).usedSize;
                }
            }

            for (TextureBufferHandle tbHandle(0u); tbHandle < numTextureBuffers; ++tbHandle)
            {
                if (scene.isTextureBufferAllocated(tbHandle))
                {
                    actions.push_back({ tbHandle.asMemoryHandle(), ESceneResourceAction_CreateTextureBuffer });
                    actions.push_back({ tbHandle.asMemoryHandle(), ESceneResourceAction_UpdateTextureBuffer });
                    usedDataByteSize += TextureBuffer::GetMipMapDataSizeInBytes(scene.getTextureBuffer(tbHandle));
                }
            }

            for (UniformBufferHandle ubHandle(0u); ubHandle < numUniformBuffers; ++ubHandle)
            {
                if (scene.isUniformBufferAllocated(ubHandle))
                {
                    actions.push_back({ ubHandle.asMemoryHandle(), ESceneResourceAction_CreateUniformBuffer });
                    actions.push_back({ ubHandle.asMemoryHandle(), ESceneResourceAction_UpdateUniformBuffer });
                    usedDataByteSize += uint32_t(scene.getUniformBuffer(ubHandle).data.size());
                }
            }
        }

        void DiffResources(ResourceContentHashVector const& old, ResourceContentHashVector const& curr, ResourceChanges& changes);
    }
}
