//  -------------------------------------------------------------------------
//  Copyright (C) 2014 BMW Car IT GmbH
//  -------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at https://mozilla.org/MPL/2.0/.
//  -------------------------------------------------------------------------

#include "internal/Components/ResourceComponent.h"
#include "internal/Components/ResourceTableOfContents.h"
#include "internal/Components/ResourceFilesRegistry.h"
#include "internal/Components/SceneFileHandle.h"
#include "internal/Core/Utils/LogMacros.h"

namespace ramses::internal
{
    ResourceComponent::ResourceComponent(StatisticCollectionFramework& statistics, PlatformLock& frameworkLock, EFeatureLevel featureLevel)
        : m_resourceStorage{ frameworkLock, statistics }
        , m_statistics{ statistics }
        , m_featureLevel{ featureLevel }
    {
    }

    ResourceComponent::~ResourceComponent() = default;

    ManagedResource ResourceComponent::getResource(ResourceContentHash hash)
    {
        return m_resourceStorage.getResource(hash);
    }

    ResourceHashUsage ResourceComponent::getResourceHashUsage(const ResourceContentHash& hash)
    {
        return m_resourceStorage.getResourceHashUsage(hash);
    }

    ManagedResourceVector ResourceComponent::getResources()
    {
        return m_resourceStorage.getResources();
    }

    bool ResourceComponent::knowsResource(const ResourceContentHash& hash) const
    {
        return m_resourceStorage.knowsResource(hash);
    }

    ManagedResource ResourceComponent::manageResource(const IResource& resource)
    {
        return m_resourceStorage.manageResource(resource, false);
    }

    ManagedResource ResourceComponent::manageResourceDeletionAllowed(const IResource& resource)
    {
        return m_resourceStorage.manageResource(resource, true);
    }

    SceneFileHandle ResourceComponent::addResourceFile(InputStreamContainerSPtr resourceFileInputStream, const ResourceTableOfContents& toc)
    {
        for (const auto& item : toc.getFileContents())
        {
            m_resourceStorage.storeResourceInfo(item.key, item.value.resourceInfo);
        }
        return m_resourceFiles.registerResourceFile(resourceFileInputStream, toc, m_resourceStorage);
    }

    void ResourceComponent::loadResourceFromFile(SceneFileHandle handle)
    {
        // If resources of a file are loaded, check if they are in use by any scene object (=hashusage) or as a resource
        // a) If they are in use, we need to load them from file, also remove the deletion allowed flag from
        // them, because they is not supposed to be loadable anymore.
        // b) If a resource is unused, nothing is to be done since there wouldn't be any entry in the resource storage for it
        const FileContentsMap* content = m_resourceFiles.getContentsOfResourceFile(handle);
        if (!content)
        {
            LOG_WARN(CONTEXT_FRAMEWORK, "ResourceComponent::loadResourceFromFile: handle {} unknown, can't force load", handle);
            return;
        }

        for (auto const& entry : *content)
        {
            auto const& id = entry.key;
            if (m_resourceStorage.isFileResourceInUseAnywhereElse(id))
            {
                ManagedResource res;
                if (!m_resourceStorage.getResource(id))
                    res = loadResource(id);

                m_resourceStorage.markDeletionDisallowed(id);
            }
        }
    }

    void ResourceComponent::removeResourceFile(SceneFileHandle handle)
    {
        m_resourceFiles.unregisterResourceFile(handle);
    }

    bool ResourceComponent::hasResourceFile(SceneFileHandle handle) const
    {
        return m_resourceFiles.getContentsOfResourceFile(handle) != nullptr;
    }

    ManagedResource ResourceComponent::loadResource(const ResourceContentHash& hash)
    {
        IInputStream* resourceStream = nullptr;
        ResourceFileEntry entry;
        SceneFileHandle fileHandle;
        std::unique_ptr<IResource> lowLevelResource;

        if (EStatus::Ok != m_resourceFiles.getEntry(hash, resourceStream, entry, fileHandle))
            return {};

        try
        {
            lowLevelResource = ResourcePersistation::RetrieveResourceFromStream(*resourceStream, entry, m_featureLevel);
        }
        catch(std::exception const& e)
        {
            size_t currentPos = 0;
            resourceStream->getPos(currentPos);
            LOG_ERROR(CONTEXT_FRAMEWORK, "ResourceComponent::loadResource: RetrieveResourceFromStream CRITICALLY failed with a std::exception ('{}')"
                " for type {}, hash {}, fileHandle {}, offset {}, size {}, streamState {}, current streamPos {}. No resource created, expect further errors.",
                e.what(), entry.resourceInfo.type, entry.resourceInfo.hash, fileHandle, entry.offsetInBytes, entry.sizeInBytes, resourceStream->getState(), currentPos);
            return {};
        }
        if (!lowLevelResource)
        {
            size_t currentPos = 0;
            resourceStream->getPos(currentPos);
            LOG_ERROR(CONTEXT_FRAMEWORK, "ResourceComponent::loadResource: RetrieveResourceFromStream CRITICALLY failed and did not return a resource"
                " for type {}, hash {}, fileHandle {}, offset {}, size {}, streamState {}, current streamPos {}. Expect further errors.",
                entry.resourceInfo.type, entry.resourceInfo.hash, fileHandle, entry.offsetInBytes, entry.sizeInBytes, resourceStream->getState(), currentPos);
            return {};
        }

        m_statistics.statResourcesLoadedFromFileNumber.incCounter(1);
        m_statistics.statResourcesLoadedFromFileSize.incCounter(entry.sizeInBytes);

        return m_resourceStorage.manageResource(*lowLevelResource.release(), true);
    }

    void ResourceComponent::reserveResourceCount(uint32_t totalCount)
    {
        m_resourceStorage.reserveResourceCount(totalCount);
    }

    ManagedResourceVector ResourceComponent::resolveResources(ResourceContentHashVector& hashes)
    {
        ManagedResourceVector result;
        result.reserve(hashes.size());
        ResourceContentHashVector failed;
        for (auto& hash : hashes)
        {
            ManagedResource mr = getResource(hash);
            if (!mr)
                mr = loadResource(hash);

            if (mr)
            {
                result.push_back(mr);
            }
            else
            {
                failed.push_back(hash);
            }
        }

        if (!failed.empty())
            LOG_ERROR(CONTEXT_FRAMEWORK, "ResourceComponent::resolveResources: failed to load resources: {}", failed);

        return result;
    }

    ResourceInfo const& ResourceComponent::getResourceInfo(ResourceContentHash const& hash)
    {
        return m_resourceStorage.getResourceInfo(hash);
    }
}
