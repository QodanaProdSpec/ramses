//  -------------------------------------------------------------------------
//  Copyright (C) 2019 BMW AG
//  -------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at https://mozilla.org/MPL/2.0/.
//  -------------------------------------------------------------------------

#ifndef RAMSES_COMPONENTS_IDCSMPROVIDEREVENTHANDLER_H
#define RAMSES_COMPONENTS_IDCSMPROVIDEREVENTHANDLER_H

#include "ramses-framework-api/DcsmApiTypes.h"
#include "ramses-framework-api/CategoryInfoUpdate.h"
#include "ramses-framework-api/DcsmStatusMessage.h"
#include "DcsmTypes.h"

namespace ramses_internal
{
    class Guid;

    class IDcsmProviderEventHandler
    {
    public:
        virtual ~IDcsmProviderEventHandler() = default;

        virtual void contentSizeChange(ramses::ContentID, const ramses::CategoryInfoUpdate&, ramses::AnimationInformation) = 0;
        virtual void contentStateChange(ramses::ContentID, ramses_internal::EDcsmState, const ramses::CategoryInfoUpdate&, ramses::AnimationInformation) = 0;
        virtual void contentStatus(ramses::ContentID, std::unique_ptr<ramses::DcsmStatusMessageImpl>&&) = 0;
    };
}

#endif
