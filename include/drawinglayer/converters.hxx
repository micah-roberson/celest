/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#pragma once

#include <vcl/bitmapex.hxx>
#include <drawinglayer/primitive2d/Primitive2DContainer.hxx>

namespace drawinglayer
{
// Helper that just creates the AlphaMask for a given Seq of Primitives.
// If only the mask is needed this can be significantly faster then
// creating content & mask in a BitmapEx (since the creation uses
// e.g. a unified color for gradients instead of having to fully paint
// these)
// New mode: bUseLuminance allows simple creation of alpha channels
//           for any content (e.g. gradients)
AlphaMask DRAWINGLAYER_DLLPUBLIC createAlphaMask(
    drawinglayer::primitive2d::Primitive2DContainer&& rSeq,
    const geometry::ViewInformation2D& rViewInformation2D, sal_uInt32 nDiscreteWidth,
    sal_uInt32 nDiscreteHeight, sal_uInt32 nMaxSquarePixels, bool bUseLuminance = false);

// Helper for convertPrimitive2DContainerToBitmapEx below, but can be also used
// directly
BitmapEx DRAWINGLAYER_DLLPUBLIC convertToBitmapEx(
    drawinglayer::primitive2d::Primitive2DContainer&& rSeq,
    const geometry::ViewInformation2D& rViewInformation2D, sal_uInt32 nDiscreteWidth,
    sal_uInt32 nDiscreteHeight, sal_uInt32 nMaxSquarePixels, bool bForceAlphaMaskCreation = false);

// helper to convert any Primitive2DSequence to a good quality BitmapEx,
// using default parameters
BitmapEx DRAWINGLAYER_DLLPUBLIC convertPrimitive2DContainerToBitmapEx(
    drawinglayer::primitive2d::Primitive2DContainer&& rSeq, const basegfx::B2DRange& rTargetRange,
    const sal_uInt32 nMaximumQuadraticPixels = 500000,
    const o3tl::Length eTargetUnit = o3tl::Length::mm100,
    const std::optional<Size>& rTargetDPI = std::nullopt);
Bitmap DRAWINGLAYER_DLLPUBLIC convertPrimitive2DContainerToBitmap(
    drawinglayer::primitive2d::Primitive2DContainer&& rSeq, const basegfx::B2DRange& rTargetRange,
    const sal_uInt32 nMaximumQuadraticPixels = 500000,
    const o3tl::Length eTargetUnit = o3tl::Length::mm100,
    const std::optional<Size>& rTargetDPI = std::nullopt);

} // end of namespace drawinglayer

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
