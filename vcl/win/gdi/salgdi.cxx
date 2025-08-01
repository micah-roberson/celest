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

#include <string.h>
#include <svsys.h>
#include <rtl/strbuf.hxx>
#include <tools/poly.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <comphelper/windowserrorstring.hxx>
#include <win/wincomp.hxx>
#include <win/saldata.hxx>
#include <win/salgdi.h>
#include <win/salframe.h>
#include <win/salvd.h>
#include <win/winlayout.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>

#include <salgdiimpl.hxx>
#include "gdiimpl.hxx"

#include <config_features.h>
#include <vcl/skia/SkiaHelper.hxx>
#include <skia/win/gdiimpl.hxx>


// we must create pens with 1-pixel width; otherwise the S3-graphics card
// map has many paint problems when drawing polygons/polyLines and a
// complex is set
#define GSL_PEN_WIDTH                   1

void ImplInitSalGDI()
{
    SalData* pSalData = GetSalData();

    pSalData->mbResourcesAlreadyFreed = false;

    // init stock brushes
    pSalData->maStockPenColorAry[0]     = PALETTERGB( 0, 0, 0 );
    pSalData->maStockPenColorAry[1]     = PALETTERGB( 0xFF, 0xFF, 0xFF );
    pSalData->maStockPenColorAry[2]     = PALETTERGB( 0xC0, 0xC0, 0xC0 );
    pSalData->maStockPenColorAry[3]     = PALETTERGB( 0x80, 0x80, 0x80 );
    pSalData->mhStockPenAry[0]          = CreatePen( PS_SOLID, GSL_PEN_WIDTH, pSalData->maStockPenColorAry[0] );
    pSalData->mhStockPenAry[1]          = CreatePen( PS_SOLID, GSL_PEN_WIDTH, pSalData->maStockPenColorAry[1] );
    pSalData->mhStockPenAry[2]          = CreatePen( PS_SOLID, GSL_PEN_WIDTH, pSalData->maStockPenColorAry[2] );
    pSalData->mhStockPenAry[3]          = CreatePen( PS_SOLID, GSL_PEN_WIDTH, pSalData->maStockPenColorAry[3] );
    pSalData->mnStockPenCount = 4;

    pSalData->maStockBrushColorAry[0]   = PALETTERGB( 0, 0, 0 );
    pSalData->maStockBrushColorAry[1]   = PALETTERGB( 0xFF, 0xFF, 0xFF );
    pSalData->maStockBrushColorAry[2]   = PALETTERGB( 0xC0, 0xC0, 0xC0 );
    pSalData->maStockBrushColorAry[3]   = PALETTERGB( 0x80, 0x80, 0x80 );
    pSalData->mhStockBrushAry[0]        = CreateSolidBrush( pSalData->maStockBrushColorAry[0] );
    pSalData->mhStockBrushAry[1]        = CreateSolidBrush( pSalData->maStockBrushColorAry[1] );
    pSalData->mhStockBrushAry[2]        = CreateSolidBrush( pSalData->maStockBrushColorAry[2] );
    pSalData->mhStockBrushAry[3]        = CreateSolidBrush( pSalData->maStockBrushColorAry[3] );
    pSalData->mnStockBrushCount = 4;

    // initialize temporary font lists
    pSalData->mpSharedTempFontItem = nullptr;
    pSalData->mpOtherTempFontItem = nullptr;
}

void ImplFreeSalGDI()
{
    SalData*    pSalData = GetSalData();

    if (pSalData->mbResourcesAlreadyFreed)
        return;

    // destroy stock objects
    int i;
    for ( i = 0; i < pSalData->mnStockPenCount; i++ )
        DeletePen( pSalData->mhStockPenAry[i] );
    for ( i = 0; i < pSalData->mnStockBrushCount; i++ )
        DeleteBrush( pSalData->mhStockBrushAry[i] );

    // delete 50% Brush
    if ( pSalData->mh50Brush )
    {
        DeleteBrush( pSalData->mh50Brush );
        pSalData->mh50Brush = nullptr;
    }

    // delete 50% Bitmap
    if ( pSalData->mh50Bmp )
    {
        DeleteBitmap( pSalData->mh50Bmp );
        pSalData->mh50Bmp = nullptr;
    }

    ImplClearHDCCache( pSalData );

    // delete icon cache
    SalIcon* pIcon = pSalData->mpFirstIcon;
    pSalData->mpFirstIcon = nullptr;
    while( pIcon )
    {
        SalIcon* pTmp = pIcon->pNext;
        DestroyIcon( pIcon->hIcon );
        DestroyIcon( pIcon->hSmallIcon );
        delete pIcon;
        pIcon = pTmp;
    }

    // delete temporary font list
    ImplReleaseTempFonts(*pSalData, true);

    pSalData->mbResourcesAlreadyFreed = true;
}


void WinSalGraphics::InitGraphics()
{
    if (!getHDC())
        return;

    // calculate the minimal line width for the printer
    if ( isPrinter() )
    {
        int nDPIX = GetDeviceCaps( getHDC(), LOGPIXELSX );
        if ( nDPIX <= 300 )
            mnPenWidth = 0;
        else
            mnPenWidth = nDPIX/300;
    }

    ::SetTextAlign( getHDC(), TA_BASELINE | TA_LEFT | TA_NOUPDATECP );
    ::SetBkMode( getHDC(), TRANSPARENT );
    ::SetROP2( getHDC(), R2_COPYPEN );
}

void WinSalGraphics::DeInitGraphics()
{
    if (!getHDC())
        return;

    // clear clip region
    SelectClipRgn( getHDC(), nullptr );
    // select default objects
    if ( mhDefPen )
    {
        SelectPen( getHDC(), mhDefPen );
        mhDefPen = nullptr;
    }
    if ( mhDefBrush )
    {
        SelectBrush( getHDC(), mhDefBrush );
        mhDefBrush = nullptr;
    }
    if ( mhDefFont )
    {
        SelectFont( getHDC(), mhDefFont );
        mhDefFont = nullptr;
    }
    if (mhDefPal)
    {
        SelectPalette(getHDC(), mhDefPal, /*bForceBkgd*/TRUE);
        mhDefPal = nullptr;
    }

    mpImpl->DeInit();
}

void WinSalGraphics::setHDC(HDC aNew)
{
    DeInitGraphics();
    mhLocalDC = aNew;
    InitGraphics();
}

HDC ImplGetCachedDC( sal_uLong nID, HBITMAP hBmp )
{
    SalData*    pSalData = GetSalData();
    HDCCache*   pC = &pSalData->maHDCCache[ nID ];

    if( !pC->mhDC )
    {
        HDC hDC = GetDC( nullptr );

        // create new DC with DefaultBitmap
        pC->mhDC = CreateCompatibleDC( hDC );

        pC->mhSelBmp = CreateCompatibleBitmap( hDC, CACHED_HDC_DEFEXT, CACHED_HDC_DEFEXT );
        pC->mhDefBmp = static_cast<HBITMAP>(SelectObject( pC->mhDC, pC->mhSelBmp ));

        ReleaseDC( nullptr, hDC );
    }

    if ( hBmp )
        SelectObject( pC->mhDC, pC->mhActBmp = hBmp );
    else
        pC->mhActBmp = nullptr;

    return pC->mhDC;
}

void ImplReleaseCachedDC( sal_uLong nID )
{
    SalData*    pSalData = GetSalData();
    HDCCache*   pC = &pSalData->maHDCCache[ nID ];

    if ( pC->mhActBmp )
        SelectObject( pC->mhDC, pC->mhSelBmp );
}

void ImplClearHDCCache( SalData* pData )
{
    for( sal_uLong i = 0; i < CACHESIZE_HDC; i++ )
    {
        HDCCache* pC = &pData->maHDCCache[ i ];

        if( pC->mhDC )
        {
            SelectObject( pC->mhDC, pC->mhDefBmp );

            if( pC->mhDefPal )
                SelectPalette( pC->mhDC, pC->mhDefPal, TRUE );

            DeleteDC( pC->mhDC );
            DeleteObject( pC->mhSelBmp );
        }
    }
}

WinSalGraphics::WinSalGraphics(WinSalGraphics::Type eType, bool bScreen, HWND hWnd, [[maybe_unused]] SalGeometryProvider *pProvider):
    mhLocalDC(nullptr),
    mbPrinter(eType == WinSalGraphics::PRINTER),
    mbVirDev(eType == WinSalGraphics::VIRTUAL_DEVICE),
    mbWindow(eType == WinSalGraphics::WINDOW),
    mbScreen(bScreen),
    mhWnd(hWnd),
    mhRegion(nullptr),
    mhDefPen(nullptr),
    mhDefBrush(nullptr),
    mhDefFont(nullptr),
    mhDefPal(nullptr),
    mpStdClipRgnData(nullptr),
    mnPenWidth(GSL_PEN_WIDTH)
{
    assert(SkiaHelper::isVCLSkiaEnabled() && "Windows requires skia");
    if (!mbPrinter)
    {
        auto const impl = new WinSkiaSalGraphicsImpl(*this, pProvider);
        mpImpl.reset(impl);
        mWinSalGraphicsImplBase = impl;
    }
    else
    {
        auto const impl = new WinSalGraphicsImpl(*this);
        mpImpl.reset(impl);
        mWinSalGraphicsImplBase = impl;
    }
}

WinSalGraphics::~WinSalGraphics()
{
    // free obsolete GDI objects
    ReleaseFonts();

    if ( mhRegion )
    {
        DeleteRegion( mhRegion );
        mhRegion = nullptr;
    }

    // delete cache data
    delete [] reinterpret_cast<BYTE*>(mpStdClipRgnData);

    setHDC(nullptr);
}

SalGraphicsImpl* WinSalGraphics::GetImpl() const
{
    return mpImpl.get();
}

bool WinSalGraphics::isPrinter() const
{
    return mbPrinter;
}

bool WinSalGraphics::isVirtualDevice() const
{
    return mbVirDev;
}

bool WinSalGraphics::isWindow() const
{
    return mbWindow;
}

bool WinSalGraphics::isScreen() const
{
    return mbScreen;
}

HWND WinSalGraphics::gethWnd()
{
    return mhWnd;
}

void WinSalGraphics::setHWND(HWND hWnd)
{
    mhWnd = hWnd;
}

HPALETTE WinSalGraphics::getDefPal() const
{
    assert(getHDC() || !mhDefPal);
    return mhDefPal;
}

HRGN WinSalGraphics::getRegion() const
{
    return mhRegion;
}

void WinSalGraphics::GetResolution( sal_Int32& rDPIX, sal_Int32& rDPIY )
{
    rDPIX = GetDeviceCaps( getHDC(), LOGPIXELSX );
    rDPIY = GetDeviceCaps( getHDC(), LOGPIXELSY );

    // #111139# this fixes the symptom of div by zero on startup
    // however, printing will fail most likely as communication with
    // the printer seems not to work in this case
    if( !rDPIX || !rDPIY )
        rDPIX = rDPIY = 600;
}

// static
IDWriteFactory* WinSalGraphics::getDWriteFactory()
{
    static sal::systools::COMReference<IDWriteFactory> pDWriteFactory(
        []()
        {
            sal::systools::COMReference<IDWriteFactory> pResult;
            HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                             reinterpret_cast<IUnknown**>(&pResult));
            if (FAILED(hr))
            {
                SAL_WARN("vcl.fonts", "HRESULT 0x" << OUString::number(hr, 16) << ": "
                                                   << comphelper::WindowsErrorStringFromHRESULT(hr));
                abort();
            }
            return pResult;
        }());
    return pDWriteFactory.get();
}

// static
IDWriteGdiInterop* WinSalGraphics::getDWriteGdiInterop()
{
    static sal::systools::COMReference<IDWriteGdiInterop> pDWriteGdiInterop(
        []()
        {
            sal::systools::COMReference<IDWriteGdiInterop> pResult;
            HRESULT hr = getDWriteFactory()->GetGdiInterop(&pResult);
            if (FAILED(hr))
            {
                SAL_WARN("vcl.fonts", "HRESULT 0x" << OUString::number(hr, 16) << ": "
                                                   << comphelper::WindowsErrorStringFromHRESULT(hr));
                abort();
            }
            return pResult;
        }());
    return pDWriteGdiInterop.get();
}

sal_uInt16 WinSalGraphics::GetBitCount() const
{
    return mpImpl->GetBitCount();
}

tools::Long WinSalGraphics::GetGraphicsWidth() const
{
    return mpImpl->GetGraphicsWidth();
}

void WinSalGraphics::Flush()
{
    mWinSalGraphicsImplBase->Flush();
}

void WinSalGraphics::ResetClipRegion()
{
    mpImpl->ResetClipRegion();
}

void WinSalGraphics::setClipRegion( const vcl::Region& i_rClip )
{
    mpImpl->setClipRegion( i_rClip );
}

void WinSalGraphics::SetLineColor()
{
    mpImpl->SetLineColor();
}

void WinSalGraphics::SetLineColor( Color nColor )
{
    mpImpl->SetLineColor( nColor );
}

void WinSalGraphics::SetFillColor()
{
    mpImpl->SetFillColor();
}

void WinSalGraphics::SetFillColor( Color nColor )
{
    mpImpl->SetFillColor( nColor );
}

void WinSalGraphics::SetXORMode( bool bSet, bool bInvertOnly )
{
    mpImpl->SetXORMode( bSet, bInvertOnly );
}

void WinSalGraphics::SetROPLineColor( SalROPColor nROPColor )
{
    mpImpl->SetROPLineColor( nROPColor );
}

void WinSalGraphics::SetROPFillColor( SalROPColor nROPColor )
{
    mpImpl->SetROPFillColor( nROPColor );
}

void WinSalGraphics::drawPixel( tools::Long nX, tools::Long nY )
{
    mpImpl->drawPixel( nX, nY );
}

void WinSalGraphics::drawPixel( tools::Long nX, tools::Long nY, Color nColor )
{
    mpImpl->drawPixel( nX, nY, nColor );
}

void WinSalGraphics::drawLine( tools::Long nX1, tools::Long nY1, tools::Long nX2, tools::Long nY2 )
{
    mpImpl->drawLine( nX1, nY1, nX2, nY2 );
}

void WinSalGraphics::drawRect( tools::Long nX, tools::Long nY, tools::Long nWidth, tools::Long nHeight )
{
    mpImpl->drawRect( nX, nY, nWidth, nHeight );
}

void WinSalGraphics::drawPolyLine( sal_uInt32 nPoints, const Point* pPtAry )
{
    mpImpl->drawPolyLine( nPoints, pPtAry );
}

void WinSalGraphics::drawPolygon( sal_uInt32 nPoints, const Point* pPtAry )
{
    mpImpl->drawPolygon( nPoints, pPtAry );
}

void WinSalGraphics::drawPolyPolygon( sal_uInt32 nPoly, const sal_uInt32* pPoints,
                                   const Point** pPtAry )
{
    mpImpl->drawPolyPolygon( nPoly, pPoints, pPtAry );
}

bool WinSalGraphics::drawPolyLineBezier( sal_uInt32 nPoints, const Point* pPtAry, const PolyFlags* pFlgAry )
{
    return mpImpl->drawPolyLineBezier( nPoints, pPtAry, pFlgAry );
}

bool WinSalGraphics::drawPolygonBezier( sal_uInt32 nPoints, const Point* pPtAry, const PolyFlags* pFlgAry )
{
    return mpImpl->drawPolygonBezier( nPoints, pPtAry, pFlgAry );
}

bool WinSalGraphics::drawPolyPolygonBezier( sal_uInt32 nPoly, const sal_uInt32* pPoints,
                                             const Point* const* pPtAry, const PolyFlags* const* pFlgAry )
{
    return mpImpl->drawPolyPolygonBezier( nPoly, pPoints, pPtAry, pFlgAry );
}

bool WinSalGraphics::drawGradient(const tools::PolyPolygon& rPoly, const Gradient& rGradient)
{
    return mpImpl->drawGradient(rPoly, rGradient);
}

bool WinSalGraphics::implDrawGradient(basegfx::B2DPolyPolygon const & rPolyPolygon, SalGradient const & rGradient)
{
    return mpImpl->implDrawGradient(rPolyPolygon, rGradient);
}

static BYTE* ImplSearchEntry( BYTE* pSource, BYTE const * pDest, sal_uLong nComp, sal_uLong nSize )
{
    while ( nComp-- >= nSize )
    {
        sal_uLong i;
        for ( i = 0; i < nSize; i++ )
        {
            if ( ( pSource[i]&~0x20 ) != ( pDest[i]&~0x20 ) )
                break;
        }
        if ( i == nSize )
            return pSource;
        pSource++;
    }
    return nullptr;
}

static bool ImplGetBoundingBox( double* nNumb, BYTE* pSource, sal_uLong nSize )
{
    BYTE* pDest = ImplSearchEntry( pSource, reinterpret_cast<BYTE const *>("%%BoundingBox:"), nSize, 14 );
    if ( !pDest )
        return false;

    bool    bRetValue = false;

    nNumb[0] = nNumb[1] = nNumb[2] = nNumb[3] = 0;
    pDest += 14;

    int nSizeLeft = nSize - ( pDest - pSource );
    if ( nSizeLeft > 100 )
        nSizeLeft = 100;    // only 100 bytes following the bounding box will be checked

    int i;
    for ( i = 0; ( i < 4 ) && nSizeLeft; i++ )
    {
        int     nDivision = 1;
        bool    bDivision = false;
        bool    bNegative = false;
        bool    bValid = true;

        while ( ( --nSizeLeft ) && ( ( *pDest == ' ' ) || ( *pDest == 0x9 ) ) ) pDest++;
        BYTE nByte = *pDest;
        while ( nSizeLeft && ( nByte != ' ' ) && ( nByte != 0x9 ) && ( nByte != 0xd ) && ( nByte != 0xa ) )
        {
            switch ( nByte )
            {
                case '.' :
                    if ( bDivision )
                        bValid = false;
                    else
                        bDivision = true;
                    break;
                case '-' :
                    bNegative = true;
                    break;
                default :
                    if ( ( nByte < '0' ) || ( nByte > '9' ) )
                        nSizeLeft = 1;  // error parsing the bounding box values
                    else if ( bValid )
                    {
                        if ( bDivision )
                            nDivision*=10;
                        nNumb[i] *= 10;
                        nNumb[i] += nByte - '0';
                    }
                    break;
            }
            nSizeLeft--;
            nByte = *(++pDest);
        }
        if ( bNegative )
            nNumb[i] = -nNumb[i];
        if ( bDivision && ( nDivision != 1 ) )
            nNumb[i] /= nDivision;
    }
    if ( i == 4 )
        bRetValue = true;
    return bRetValue;
}

#define POSTSCRIPT_BUFSIZE 0x4000           // MAXIMUM BUFSIZE EQ 0xFFFF

bool WinSalGraphics::drawEPS( tools::Long nX, tools::Long nY, tools::Long nWidth, tools::Long nHeight, void* pPtr, sal_uInt32 nSize )
{

    if ( !mbPrinter )
        return false;

    int nEscape = POSTSCRIPT_PASSTHROUGH;
    if ( !Escape( getHDC(), QUERYESCSUPPORT, sizeof( int ), reinterpret_cast<LPSTR>(&nEscape), nullptr ) )
        return false;

    double  nBoundingBox[4];
    if ( !ImplGetBoundingBox( nBoundingBox, static_cast<BYTE*>(pPtr), nSize ) )
        return false;

    OStringBuffer aBuf( POSTSCRIPT_BUFSIZE );

    // reserve place for a sal_uInt16
    aBuf.append( "aa" );

    // #107797# Write out EPS encapsulation header

    // directly taken from the PLRM 3.0, p. 726. Note:
    // this will definitely cause problems when
    // recursively creating and embedding PostScript files
    // in OOo, since we use statically-named variables
    // here (namely, b4_Inc_state_salWin, dict_count_salWin and
    // op_count_salWin). Currently, I have no idea on how to
    // work around that, except from scanning and
    // interpreting the EPS for unused identifiers.

    // append the real text
    aBuf.append( "\n\n/b4_Inc_state_salWin save def\n"
                 "/dict_count_salWin countdictstack def\n"
                 "/op_count_salWin count 1 sub def\n"
                 "userdict begin\n"
                 "/showpage {} def\n"
                 "0 setgray 0 setlinecap\n"
                 "1 setlinewidth 0 setlinejoin\n"
                 "10 setmiterlimit [] 0 setdash newpath\n"
                 "/languagelevel where\n"
                 "{\n"
                 "  pop languagelevel\n"
                 "  1 ne\n"
                 "  {\n"
                 "    false setstrokeadjust false setoverprint\n"
                 "  } if\n"
                 "} if\n\n" );

    // #i10737# Apply clipping manually

    // Windows seems to ignore any clipping at the HDC,
    // when followed by a POSTSCRIPT_PASSTHROUGH

    // Check whether we've got a clipping, consisting of
    // exactly one rect (other cases should be, but aren't
    // handled currently)

    // TODO: Handle more than one rectangle here (take
    // care, the buffer can handle only POSTSCRIPT_BUFSIZE
    // characters!)
    if ( mhRegion != nullptr &&
         mpStdClipRgnData != nullptr &&
         mpClipRgnData == mpStdClipRgnData &&
         mpClipRgnData->rdh.nCount == 1 )
    {
        RECT* pRect = &(mpClipRgnData->rdh.rcBound);

        aBuf.append( "\nnewpath\n"
                     + OString::number(pRect->left) + " " + OString::number(pRect->top)
                     + " moveto\n"
                     + OString::number(pRect->right) + " " + OString::number(pRect->top)
                     + " lineto\n"
                     + OString::number(pRect->right) + " "
                     + OString::number(pRect->bottom) + " lineto\n"
                     + OString::number(pRect->left) + " "
                     + OString::number(pRect->bottom) + " lineto\n"
                     "closepath\n"
                     "clip\n"
                     "newpath\n" );
    }

    // #107797# Write out buffer

    *reinterpret_cast<sal_uInt16*>(const_cast<char *>(aBuf.getStr())) = static_cast<sal_uInt16>( aBuf.getLength() - 2 );
    Escape ( getHDC(), nEscape, aBuf.getLength(), aBuf.getStr(), nullptr );

    // #107797# Write out EPS transformation code

    double  dM11 = nWidth / ( nBoundingBox[2] - nBoundingBox[0] );
    double  dM22 = nHeight / (nBoundingBox[1] - nBoundingBox[3] );
    // reserve a sal_uInt16 again
    aBuf.setLength( 2 );
    aBuf.append( "\n\n[" + OString::number(dM11) + " 0 0 " + OString::number(dM22) + " "
                 + OString::number(nX - ( dM11 * nBoundingBox[0] )) + " "
                 + OString::number(nY - ( dM22 * nBoundingBox[3] )) + "] concat\n"
                 "%%BeginDocument:\n" );
    *reinterpret_cast<sal_uInt16*>(const_cast<char *>(aBuf.getStr())) = static_cast<sal_uInt16>( aBuf.getLength() - 2 );
    Escape ( getHDC(), nEscape, aBuf.getLength(), aBuf.getStr(), nullptr );

    // #107797# Write out actual EPS content

    sal_uLong   nToDo = nSize;
    sal_uLong   nDoNow;
    while ( nToDo )
    {
        nDoNow = nToDo;
        if ( nToDo > POSTSCRIPT_BUFSIZE - 2 )
            nDoNow = POSTSCRIPT_BUFSIZE - 2;
        // the following is based on the string buffer allocation
        // of size POSTSCRIPT_BUFSIZE at construction time of aBuf
        *reinterpret_cast<sal_uInt16*>(const_cast<char *>(aBuf.getStr())) = static_cast<sal_uInt16>(nDoNow);
        memcpy( const_cast<char *>(aBuf.getStr() + 2), static_cast<BYTE*>(pPtr) + nSize - nToDo, nDoNow );
        sal_uLong nResult = Escape ( getHDC(), nEscape, nDoNow + 2, aBuf.getStr(), nullptr );
        if (!nResult )
            break;
        nToDo -= nResult;
    }

    // #107797# Write out EPS encapsulation footer

    // reserve a sal_uInt16 again
    aBuf.setLength( 2 );
    aBuf.append( "%%EndDocument\n"
                 "count op_count_salWin sub {pop} repeat\n"
                 "countdictstack dict_count_salWin sub {end} repeat\n"
                 "b4_Inc_state_salWin restore\n\n" );
    *reinterpret_cast<sal_uInt16*>(const_cast<char *>(aBuf.getStr())) = static_cast<sal_uInt16>( aBuf.getLength() - 2 );
    Escape ( getHDC(), nEscape, aBuf.getLength(), aBuf.getStr(), nullptr );

    return true;
}

SystemGraphicsData WinSalGraphics::GetGraphicsData() const
{
    SystemGraphicsData aRes;
    aRes.nSize = sizeof(aRes);
    aRes.hDC = getHDC();
    return aRes;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
