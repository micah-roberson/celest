/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#pragma once

#include <vcl/dllapi.h>
#include <com/sun/star/task/InteractionHandler.hpp>

namespace vcl::pdf
{
/** retrieve password from user
     */
bool VCL_DLLPUBLIC getPassword(const css::uno::Reference<css::task::XInteractionHandler>& xHandler,
                               OUString& rOutPwd, bool bFirstTry, const OUString& rDocName);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
