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

#include <scitems.hxx>
#include <editeng/brushitem.hxx>
#include <svtools/colorcfg.hxx>
#include <svx/rotmodit.hxx>
#include <svx/svdocapt.hxx>
#include <editeng/shaditem.hxx>
#include <editeng/svxfont.hxx>
#include <tools/poly.hxx>
#include <vcl/svapp.hxx>
#include <vcl/pdfextoutdevdata.hxx>
#include <svx/framelinkarray.hxx>
#include <drawinglayer/geometry/viewinformation2d.hxx>
#include <drawinglayer/processor2d/baseprocessor2d.hxx>
#include <drawinglayer/processor2d/processor2dtools.hxx>
#include <officecfg/Office/Common.hxx>
#include <officecfg/Office/Calc.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/gradient.hxx>
#include <vcl/settings.hxx>
#include <vcl/pdf/PDFNote.hxx>
#include <svx/unoapi.hxx>
#include <sal/log.hxx>
#include <comphelper/lok.hxx>
#include <o3tl/unit_conversion.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>

#include <output.hxx>
#include <document.hxx>
#include <drwlayer.hxx>
#include <formulacell.hxx>
#include <attrib.hxx>
#include <patattr.hxx>
#include <progress.hxx>
#include <pagedata.hxx>
#include <chgtrack.hxx>
#include <chgviset.hxx>
#include <viewutil.hxx>
#include <gridmerg.hxx>
#include <fillinfo.hxx>
#include <scmod.hxx>
#include <appoptio.hxx>
#include <postit.hxx>
#include <validat.hxx>
#include <detfunc.hxx>
#include <editutil.hxx>

#include <SparklineRenderer.hxx>
#include <colorscale.hxx>

#include <math.h>
#include <memory>

using namespace com::sun::star;

// Static Data

// color for ChangeTracking "by author" as in the writer (swmodul1.cxx)

#define SC_AUTHORCOLORCOUNT     9

const Color nAuthorColor[ SC_AUTHORCOLORCOUNT ] = {
                    COL_LIGHTRED,       COL_LIGHTBLUE,      COL_LIGHTMAGENTA,
                    COL_GREEN,          COL_RED,            COL_BLUE,
                    COL_BROWN,          COL_MAGENTA,        COL_CYAN };

// Helper class for color assignment to avoid repeated lookups for the same user

ScActionColorChanger::ScActionColorChanger( const ScChangeTrack& rTrack ) :
    rOpt( ScModule::get()->GetAppOptions() ),
    rUsers( rTrack.GetUserCollection() ),
    nLastUserIndex( 0 ),
    nColor( COL_BLACK )
{
}

void ScActionColorChanger::Update( const ScChangeAction& rAction )
{
    Color nSetColor;
    switch (rAction.GetType())
    {
        case SC_CAT_INSERT_COLS:
        case SC_CAT_INSERT_ROWS:
        case SC_CAT_INSERT_TABS:
            nSetColor = rOpt.GetTrackInsertColor();
            break;
        case SC_CAT_DELETE_COLS:
        case SC_CAT_DELETE_ROWS:
        case SC_CAT_DELETE_TABS:
            nSetColor = rOpt.GetTrackDeleteColor();
            break;
        case SC_CAT_MOVE:
            nSetColor = rOpt.GetTrackMoveColor();
            break;
        default:
            nSetColor = rOpt.GetTrackContentColor();
            break;
    }
    if ( nSetColor != COL_TRANSPARENT )     // color assigned
        nColor = nSetColor;
    else                                    // by author
    {
        if (aLastUserName != rAction.GetUser())
        {
            aLastUserName = rAction.GetUser();
            std::set<OUString>::const_iterator it = rUsers.find(aLastUserName);
            if (it == rUsers.end())
            {
                // empty string is possible if a name wasn't found while saving a 5.0 file
                SAL_INFO_IF( aLastUserName.isEmpty(), "sc.ui", "Author not found" );
                nLastUserIndex = 0;
            }
            else
            {
                size_t nPos = std::distance(rUsers.begin(), it);
                nLastUserIndex = nPos % SC_AUTHORCOLORCOUNT;
            }
        }
        nColor = nAuthorColor[nLastUserIndex];
    }
}

ScOutputData::ScOutputData( OutputDevice* pNewDev, ScOutputType eNewType,
                            ScTableInfo& rTabInfo, ScDocument* pNewDoc,
                            SCTAB nNewTab, tools::Long nNewScrX, tools::Long nNewScrY,
                            SCCOL nNewX1, SCROW nNewY1, SCCOL nNewX2, SCROW nNewY2,
                            double nPixelPerTwipsX, double nPixelPerTwipsY,
                            const Fraction* pZoomX, const Fraction* pZoomY ) :
    mpOriginalTargetDevice( pNewDev ),
    mpDev( pNewDev ),
    mpRefDevice( pNewDev ),      // default is output device
    pFmtDevice( pNewDev ),      // default is output device
    mrTabInfo( rTabInfo ),
    mpRowInfo( rTabInfo.mpRowInfo.get() ),
    mnArrCount( rTabInfo.mnArrCount ),
    mpDoc( pNewDoc ),
    mnTab( nNewTab ),
    mnScrX( nNewScrX ),
    mnScrY( nNewScrY ),
    mnX1( nNewX1 ),
    mnY1( nNewY1 ),
    mnX2( nNewX2 ),
    mnY2( nNewY2 ),
    meType( eNewType ),
    mnPPTX( nPixelPerTwipsX ),
    mnPPTY( nPixelPerTwipsY ),
    mpViewShell( nullptr ),
    mpDrawView( nullptr ),
    mbEditMode( false ),
    mnEditCol( 0 ),
    mnEditRow( 0 ),
    mbMetaFile( false ),
    mbPagebreakMode( false ),
    mbSolidBackground( false ),
    mbUseStyleColor( false ),
    mbForceAutoColor( officecfg::Office::Common::Accessibility::IsAutomaticFontColor::get() ),
    mbSyntaxMode( false ),
    maGridColor( COL_BLACK ),
    mbShowNullValues( true ),
    mbShowFormulas( false ),
    mbShowSpellErrors( false ),
    mbMarkClipped( false ), // sal_False for printer/metafile etc.
    mbSnapPixel( false ),
    mbAnyClipped( false ),
    mbVertical(false),
    mpTargetPaintWindow(nullptr), // #i74769# use SdrPaintWindow direct
    mpSpellCheckCxt(nullptr)
{
    if (pZoomX)
        maZoomX = *pZoomX;
    else
        maZoomX = Fraction(1,1);
    if (pZoomY)
        maZoomY = *pZoomY;
    else
        maZoomY = Fraction(1,1);

    mnVisX1 = mnX1;
    mnVisY1 = mnY1;
    mnVisX2 = mnX2;
    mnVisY2 = mnY2;
    mpDoc->StripHidden( mnVisX1, mnVisY1, mnVisX2, mnVisY2, mnTab );

    mnScrW = 0;
    for (SCCOL nX=mnVisX1; nX<=mnVisX2; nX++)
        mnScrW += mpRowInfo[0].basicCellInfo(nX).nWidth;

    mnMirrorW = mnScrW;

    mnScrH = 0;
    for (SCSIZE nArrY=1; nArrY+1<mnArrCount; nArrY++)
        mnScrH += mpRowInfo[nArrY].nHeight;

    mbTabProtected = mpDoc->IsTabProtected( mnTab );
    mbLayoutRTL = mpDoc->IsLayoutRTL( mnTab );

    // always needed, so call at the end of the constructor
    SetCellRotations();
    InitOutputEditEngine();
}

ScOutputData::~ScOutputData()
{
}

void ScOutputData::SetSpellCheckContext( const sc::SpellCheckContext* pCxt )
{
    mpSpellCheckCxt = pCxt;
}

void ScOutputData::SetContentDevice( OutputDevice* pContentDev )
{
    // use pContentDev instead of pDev where used

    if ( mpRefDevice == mpDev )
        mpRefDevice = pContentDev;
    if ( pFmtDevice == mpDev )
        pFmtDevice = pContentDev;
    mpDev = pContentDev;
}

void ScOutputData::SetMirrorWidth( tools::Long nNew )
{
    mnMirrorW = nNew;
}

void ScOutputData::SetGridColor( const Color& rColor )
{
    maGridColor = rColor;
}

void ScOutputData::SetMarkClipped( bool bSet )
{
    mbMarkClipped = bSet;
}

void ScOutputData::SetShowNullValues( bool bSet )
{
    mbShowNullValues = bSet;
}

void ScOutputData::SetShowFormulas( bool bSet )
{
    mbShowFormulas = bSet;
}

void ScOutputData::SetShowSpellErrors( bool bSet )
{
    mbShowSpellErrors = bSet;
    // reset EditEngine because it depends on bShowSpellErrors
    mxOutputEditEngine.reset();
}

void ScOutputData::SetSnapPixel()
{
    mbSnapPixel = true;
}

void ScOutputData::SetEditCell( SCCOL nCol, SCROW nRow )
{
    mnEditCol = nCol;
    mnEditRow = nRow;
    mbEditMode = true;
}

void ScOutputData::SetMetaFileMode( bool bNewMode )
{
    mbMetaFile = bNewMode;
}

void ScOutputData::SetSyntaxMode( bool bNewMode )
{
    mbSyntaxMode = bNewMode;
    if ( bNewMode && !mxValueColor )
    {
        const svtools::ColorConfig& rColorCfg = ScModule::get()->GetColorConfig();
        mxValueColor = rColorCfg.GetColorValue( svtools::CALCVALUE ).nColor;
        mxTextColor = rColorCfg.GetColorValue( svtools::CALCTEXT ).nColor;
        mxFormulaColor = rColorCfg.GetColorValue( svtools::CALCFORMULA ).nColor;
    }
}

bool ScOutputData::ReopenPDFStructureElement(vcl::pdf::StructElement aType, SCROW nRow, SCCOL nCol)
{
    bool bReopenTag = false;
    vcl::PDFExtOutDevData* pPDF = dynamic_cast<vcl::PDFExtOutDevData*>(mpDev->GetExtOutDevData());
    if (pPDF)
    {
        if (aType == vcl::pdf::StructElement::Part) // Worksheet
        {
            if (pPDF->GetScPDFState()->m_WorksheetId != -1)
            {
                sal_Int32 nId = pPDF->GetScPDFState()->m_WorksheetId;
                pPDF->BeginStructureElement(nId);
                bReopenTag = true;
            }
        }
        else if (aType == vcl::pdf::StructElement::TableRow)
        {
            const auto aIter = pPDF->GetScPDFState()->m_TableRowMap.find(nRow);
            if (aIter != pPDF->GetScPDFState()->m_TableRowMap.end() && nRow == aIter->first)
            {
                sal_Int32 nId = (*aIter).second;
                pPDF->BeginStructureElement(nId);
                bReopenTag = true;
            }
        }
        else if (aType == vcl::pdf::StructElement::TableData)
        {
            const std::pair<SCROW, SCCOL> keyToFind = std::make_pair(nRow, nCol);
            const auto aIter = pPDF->GetScPDFState()->m_TableDataMap.find(keyToFind);
            if (aIter != pPDF->GetScPDFState()->m_TableDataMap.end() && keyToFind == aIter->first)
            {
                sal_Int32 nId = (*aIter).second;
                pPDF->BeginStructureElement(nId);
                bReopenTag = true;
            }
        }
    }

    return bReopenTag;
}

void ScOutputData::DrawGrid(vcl::RenderContext& rRenderContext, bool bGrid, bool bPage, bool bMergeCover)
{
    // bMergeCover : Draw lines in sheet bgcolor to cover lok client grid lines in merged cell areas.
    // When scNoGridBackground is set in lok mode, bMergeCover is set to true and bGrid to false.

    SCCOL nX;
    SCROW nY;
    tools::Long nPosX;
    tools::Long nPosY;
    SCSIZE nArrY;
    ScBreakType nBreak    = ScBreakType::NONE;
    ScBreakType nBreakOld = ScBreakType::NONE;

    bool bDashed = false;
    Color aPageColor;
    Color aManualColor;

    if (mbPagebreakMode)
        bPage = false;          // no "normal" breaks over the whole width/height

    // It is a big mess to distinguish when we are using pixels and when logic
    // units for drawing.  Ultimately we want to work only in the logic units,
    // but until that happens, we need to special-case:
    //
    //   * metafile
    //   * drawing to the screen - everything is internally counted in pixels there
    //
    // 'Internally' in the above means the pCellInfo[...].nWidth and
    // mpRowInfo[...]->nHeight:
    //
    //   * when bWorksInPixels is true: these are in pixels
    //   * when bWorksInPixels is false: these are in the logic units
    //
    // This is where all the confusion comes from, ultimately we want them
    // always in the logic units (100th of millimeters), but we need to get
    // there gradually (get rid of setting MapUnit::MapPixel first), otherwise we'd
    // break all the drawing by one change.
    // So until that happens, we need to special case.
    bool bWorksInPixels = mbMetaFile;
    const svtools::ColorConfig& rColorCfg = ScModule::get()->GetColorConfig();
    Color aSheetBGColor = rColorCfg.GetColorValue(::svtools::DOCCOLOR).nColor;

    if ( meType == OUTTYPE_WINDOW )
    {
        bWorksInPixels = true;
        aPageColor = rColorCfg.GetColorValue(svtools::CALCPAGEBREAKAUTOMATIC).nColor;
        aManualColor = rColorCfg.GetColorValue(svtools::CALCPAGEBREAKMANUAL).nColor;
    }
    else
    {
        aPageColor = maGridColor;
        aManualColor = maGridColor;
    }

    tools::Long nOneX = 1;
    tools::Long nOneY = 1;
    if (!bWorksInPixels)
    {
        Size aOnePixel = rRenderContext.PixelToLogic(Size(1,1));
        nOneX = aOnePixel.Width();
        nOneY = aOnePixel.Height();
    }

    tools::Long nLayoutSign = mbLayoutRTL ? -1 : 1;
    tools::Long nSignedOneX = nOneX * nLayoutSign;

    rRenderContext.SetLineColor(bMergeCover ? aSheetBGColor : maGridColor);

    ScGridMerger aGrid(&rRenderContext, nOneX, nOneY);

    // vertical lines

    nPosX = mnScrX;
    if ( mbLayoutRTL )
        nPosX += mnMirrorW - nOneX;

    for (nX=mnX1; nX<=mnX2; nX++)
    {
        sal_uInt16 nWidth = mpRowInfo[0].basicCellInfo(nX).nWidth;
        if (nWidth)
        {
            nPosX += nWidth * nLayoutSign;

            if ( bPage )
            {
                // Search also in hidden part for page breaks
                SCCOL nCol = nX + 1;
                while (nCol <= mpDoc->MaxCol())
                {
                    nBreak = mpDoc->HasColBreak(nCol, mnTab);
                    bool bHidden = mpDoc->ColHidden(nCol, mnTab);

                    if ( nBreak != ScBreakType::NONE || !bHidden )
                        break;
                    ++nCol;
                }

                if (nBreak != nBreakOld)
                {
                    aGrid.Flush();

                    if (static_cast<int>(nBreak))
                    {
                        rRenderContext.SetLineColor( (nBreak & ScBreakType::Manual) ? aManualColor :
                                                        aPageColor );
                        bDashed = true;
                    }
                    else
                    {
                        rRenderContext.SetLineColor(bMergeCover ? aSheetBGColor : maGridColor);
                        bDashed = false;
                    }

                    nBreakOld = nBreak;
                }
            }

            bool bDraw = bGrid || nBreakOld != ScBreakType::NONE || bMergeCover; // simple grid only if set that way

            sal_uInt16 nWidthXplus1 = mpRowInfo[0].basicCellInfo(nX+1).nWidth;
            bool bSingle = false; //! get into Fillinfo !!!!!
            if ( nX<mpDoc->MaxCol() )
            {
                bSingle = ( nWidthXplus1 == 0 );
                for (nArrY=1; nArrY+1<mnArrCount && !bSingle; nArrY++)
                {
                    if (mpRowInfo[nArrY].cellInfo(nX+1).bHOverlapped)
                        bSingle = true;
                    if (mpRowInfo[nArrY].cellInfo(nX).bHideGrid)
                        bSingle = true;
                }
            }

            if (bDraw)
            {
                if ( nX<mpDoc->MaxCol() && bSingle )
                {
                    SCCOL nVisX = nX + 1;
                    while ( nVisX < mpDoc->MaxCol() && !mpDoc->GetColWidth(nVisX,mnTab) )
                        ++nVisX;

                    nPosY = mnScrY;
                    for (nArrY=1; nArrY+1<mnArrCount; nArrY++)
                    {
                        RowInfo* pThisRowInfo = &mpRowInfo[nArrY];
                        const tools::Long nNextY = nPosY + pThisRowInfo->nHeight;

                        bool bHOver = pThisRowInfo->cellInfo(nX).bHideGrid;
                        if (!bHOver)
                        {
                            if (nWidthXplus1)
                                bHOver = pThisRowInfo->cellInfo(nX+1).bHOverlapped;
                            else
                            {
                                if (nVisX <= mnX2)
                                    bHOver = pThisRowInfo->cellInfo(nVisX).bHOverlapped;
                                else
                                    bHOver = mpDoc->GetAttr(
                                                nVisX,pThisRowInfo->nRowNo,mnTab,ATTR_MERGE_FLAG)
                                                ->IsHorOverlapped();
                                if (bHOver)
                                    bHOver = mpDoc->GetAttr(
                                                nX + 1,pThisRowInfo->nRowNo,mnTab,ATTR_MERGE_FLAG)
                                                ->IsHorOverlapped();
                            }
                        }

                        if ((pThisRowInfo->bChanged && !bHOver && !bMergeCover) || (bHOver && bMergeCover))
                        {
                            aGrid.AddVerLine(bWorksInPixels, nPosX-nSignedOneX, nPosY, nNextY-nOneY, bDashed);
                        }
                        nPosY = nNextY;
                    }
                }
                else if (!bMergeCover)
                {
                    aGrid.AddVerLine(bWorksInPixels, nPosX-nSignedOneX, mnScrY, mnScrY+mnScrH-nOneY, bDashed);
                }
            }
        }
    }

    // horizontal lines

    bool bHiddenRow = true;
    SCROW nHiddenEndRow = -1;
    nPosY = mnScrY;
    for (nArrY=1; nArrY+1<mnArrCount; nArrY++)
    {
        SCSIZE nArrYplus1 = nArrY+1;
        nY = mpRowInfo[nArrY].nRowNo;
        SCROW nYplus1 = nY+1;
        nPosY += mpRowInfo[nArrY].nHeight;

        if (mpRowInfo[nArrY].bChanged)
        {
            if ( bPage )
            {
                for (SCROW i = nYplus1; i <= mpDoc->MaxRow(); ++i)
                {
                    if (i > nHiddenEndRow)
                        bHiddenRow = mpDoc->RowHidden(i, mnTab, nullptr, &nHiddenEndRow);
                    /* TODO: optimize the row break thing for large hidden
                     * segments where HasRowBreak() has to be called
                     * nevertheless for each row, as a row break is drawn also
                     * for hidden rows, above them. This needed to be done only
                     * once per hidden segment, maybe giving manual breaks
                     * priority. Something like GetNextRowBreak() and
                     * GetNextManualRowBreak(). */
                    nBreak = mpDoc->HasRowBreak(i, mnTab);
                    if (!bHiddenRow || nBreak != ScBreakType::NONE)
                        break;
                }

                if (nBreakOld != nBreak)
                {
                    aGrid.Flush();

                    if (static_cast<int>(nBreak))
                    {
                        rRenderContext.SetLineColor( (nBreak & ScBreakType::Manual) ? aManualColor :
                                                        aPageColor );
                        bDashed = true;
                    }
                    else
                    {
                        rRenderContext.SetLineColor(bMergeCover ? aSheetBGColor : maGridColor);
                        bDashed = false;
                    }

                    nBreakOld = nBreak;
                }
            }

            bool bDraw = bGrid || nBreakOld != ScBreakType::NONE || bMergeCover;    // simple grid only if set so

            bool bNextYisNextRow = (mpRowInfo[nArrYplus1].nRowNo == nYplus1);
            bool bSingle = !bNextYisNextRow;             // Hidden
            for (SCCOL i=mnX1; i<=mnX2 && !bSingle; i++)
            {
                if (mpRowInfo[nArrYplus1].cellInfo(i).bVOverlapped)
                    bSingle = true;
            }

            if (bDraw)
            {
                if ( bSingle && nY<mpDoc->MaxRow() )
                {
                    SCROW nVisY = mpRowInfo[nArrYplus1].nRowNo;

                    nPosX = mnScrX;
                    if ( mbLayoutRTL )
                        nPosX += mnMirrorW - nOneX;

                    for (SCCOL i=mnX1; i<=mnX2; i++)
                    {
                        const tools::Long nNextX = nPosX + mpRowInfo[0].basicCellInfo(i).nWidth * nLayoutSign;
                        if (nNextX != nPosX)                                // visible
                        {
                            bool bVOver;
                            if ( bNextYisNextRow )
                                bVOver = mpRowInfo[nArrYplus1].cellInfo(i).bVOverlapped;
                            else
                            {
                                bVOver = mpDoc->GetAttr(
                                            i,nYplus1,mnTab,ATTR_MERGE_FLAG)
                                            ->IsVerOverlapped()
                                    &&   mpDoc->GetAttr(
                                            i,nVisY,mnTab,ATTR_MERGE_FLAG)
                                            ->IsVerOverlapped();
                                    //! nVisY from Array ??
                            }

                            if ((!bVOver && !bMergeCover) || (bVOver && bMergeCover))
                            {
                                aGrid.AddHorLine(bWorksInPixels, nPosX, nNextX-nSignedOneX, nPosY-nOneY, bDashed);
                            }
                        }
                        nPosX = nNextX;
                    }
                }
                else if (!bMergeCover)
                {
                    aGrid.AddHorLine(bWorksInPixels, mnScrX, mnScrX+mnScrW-nOneX, nPosY-nOneY, bDashed);
                }
            }
        }
    }
}

void ScOutputData::SetPagebreakMode( ScPageBreakData* pPageData )
{
    mbPagebreakMode = true;
    if (!pPageData)
        return;                     // not yet initialized -> everything "not printed"

    // mark printed range
    // (everything in FillInfo is already initialized to sal_False)

    sal_uInt16 nRangeCount = sal::static_int_cast<sal_uInt16>(pPageData->GetCount());
    for (sal_uInt16 nPos=0; nPos<nRangeCount; nPos++)
    {
        ScRange aRange = pPageData->GetData( nPos ).GetPrintRange();

        SCCOL nStartX = std::max( aRange.aStart.Col(), mnX1 );
        SCCOL nEndX   = std::min( aRange.aEnd.Col(),   mnX2 );
        SCROW nStartY = std::max( aRange.aStart.Row(), mnY1 );
        SCROW nEndY   = std::min( aRange.aEnd.Row(),   mnY2 );

        for (SCSIZE nArrY=1; nArrY+1<mnArrCount; nArrY++)
        {
            RowInfo* pThisRowInfo = &mpRowInfo[nArrY];
            if ( pThisRowInfo->bChanged && pThisRowInfo->nRowNo >= nStartY &&
                                           pThisRowInfo->nRowNo <= nEndY )
            {
                for (SCCOL nX=nStartX; nX<=nEndX; nX++)
                    pThisRowInfo->cellInfo(nX).bPrinted = true;
            }
        }
    }
}

void ScOutputData::SetCellRotations()
{
    //! save nRotMax
    SCCOL nRotMax = mnX2;
    for (SCSIZE nRotY=0; nRotY<mnArrCount; nRotY++)
        if (mpRowInfo[nRotY].nRotMaxCol != SC_ROTMAX_NONE && mpRowInfo[nRotY].nRotMaxCol > nRotMax)
            nRotMax = mpRowInfo[nRotY].nRotMaxCol;

    for (SCSIZE nArrY=1; nArrY<mnArrCount; nArrY++)
    {
        RowInfo* pThisRowInfo = &mpRowInfo[nArrY];
        if ( pThisRowInfo->nRotMaxCol != SC_ROTMAX_NONE &&
             ( pThisRowInfo->bChanged || mpRowInfo[nArrY-1].bChanged ||
               ( nArrY+1<mnArrCount && mpRowInfo[nArrY+1].bChanged ) ) )
        {
            SCROW nY = pThisRowInfo->nRowNo;

            for (SCCOL nX=0; nX<=nRotMax; nX++)
            {
                ScCellInfo* pInfo = &pThisRowInfo->cellInfo(nX);
                const ScPatternAttr* pPattern = pInfo->pPatternAttr;
                const SfxItemSet* pCondSet = pInfo->pConditionSet;

                if ( !pPattern && !mpDoc->ColHidden(nX, mnTab) )
                {
                    pPattern = mpDoc->GetPattern( nX, nY, mnTab );
                    pCondSet = mpDoc->GetCondResult( nX, nY, mnTab );
                }

                if ( pPattern )     // column isn't hidden
                {
                    ScRotateDir nDir = pPattern->GetRotateDir( pCondSet );
                    if (nDir != ScRotateDir::NONE)
                    {
                        // Needed for ScCellInfo internal decisions (bg fill, ...)
                        pInfo->nRotateDir = nDir;

                        // create target coordinates
                        const SCCOL nTargetX(nX - mnVisX1 + 1);
                        const SCROW nTargetY(nY - mnVisY1 + 1);

                        // Check for values - below in SetCellRotation these will
                        // be converted to size_t and thus may not be negative
                        if(nTargetX >= 0 && nTargetY >= 0)
                        {
                            // add rotation info to Array information
                            const Degree100 nAttrRotate(pPattern->GetRotateVal(pCondSet));
                            const SvxRotateMode eRotMode(pPattern->GetItem(ATTR_ROTATE_MODE, pCondSet).GetValue());
                            const double fOrient((mbLayoutRTL ? -1.0 : 1.0) * toRadians(nAttrRotate)); // 1/100th degrees -> [0..2PI]
                            svx::frame::Array& rArray = mrTabInfo.maArray;

                            rArray.SetCellRotation(nTargetX, nTargetY, eRotMode, fOrient);
                        }
                    }
                }
            }
        }
    }
}

static ScRotateDir lcl_GetRotateDir( const ScDocument* pDoc, SCCOL nCol, SCROW nRow, SCTAB mnTab )
{
    const ScPatternAttr* pPattern = pDoc->GetPattern( nCol, nRow, mnTab );
    const SfxItemSet* pCondSet = pDoc->GetCondResult( nCol, nRow, mnTab );

    ScRotateDir nRet = ScRotateDir::NONE;

    Degree100 nAttrRotate = pPattern->GetRotateVal( pCondSet );
    if ( nAttrRotate )
    {
        SvxRotateMode eRotMode =
                    pPattern->GetItem(ATTR_ROTATE_MODE, pCondSet).GetValue();

        if ( eRotMode == SVX_ROTATE_MODE_STANDARD )
            nRet = ScRotateDir::Standard;
        else if ( eRotMode == SVX_ROTATE_MODE_CENTER )
            nRet = ScRotateDir::Center;
        else if ( eRotMode == SVX_ROTATE_MODE_TOP || eRotMode == SVX_ROTATE_MODE_BOTTOM )
        {
            tools::Long nRot180 = nAttrRotate.get() % 18000;     // 1/100 degree
            if ( nRot180 == 9000 )
                nRet = ScRotateDir::Center;
            else if ( ( eRotMode == SVX_ROTATE_MODE_TOP && nRot180 < 9000 ) ||
                      ( eRotMode == SVX_ROTATE_MODE_BOTTOM && nRot180 > 9000 ) )
                nRet = ScRotateDir::Left;
            else
                nRet = ScRotateDir::Right;
        }
    }

    return nRet;
}

static const SvxBrushItem* lcl_FindBackground( const ScDocument* pDoc, SCCOL nCol, SCROW nRow, SCTAB mnTab )
{
    const ScPatternAttr* pPattern = pDoc->GetPattern( nCol, nRow, mnTab );
    const SfxItemSet* pCondSet = pDoc->GetCondResult( nCol, nRow, mnTab );
    const SvxBrushItem* pBackground =
                            &pPattern->GetItem( ATTR_BACKGROUND, pCondSet );

    ScRotateDir nDir = lcl_GetRotateDir( pDoc, nCol, nRow, mnTab );

    // treat CENTER like RIGHT
    if ( nDir == ScRotateDir::Right || nDir == ScRotateDir::Center )
    {
        // text goes to the right -> take background from the left
        while ( nCol > 0 && lcl_GetRotateDir( pDoc, nCol, nRow, mnTab ) == nDir &&
                            pBackground->GetColor().GetAlpha() != 0 )
        {
            --nCol;
            pPattern = pDoc->GetPattern( nCol, nRow, mnTab );
            pCondSet = pDoc->GetCondResult( nCol, nRow, mnTab );
            pBackground = &pPattern->GetItem( ATTR_BACKGROUND, pCondSet );
        }
    }
    else if ( nDir == ScRotateDir::Left )
    {
        // text goes to the left -> take background from the right
        while ( nCol < pDoc->MaxCol() && lcl_GetRotateDir( pDoc, nCol, nRow, mnTab ) == nDir &&
                            pBackground->GetColor().GetAlpha() != 0 )
        {
            ++nCol;
            pPattern = pDoc->GetPattern( nCol, nRow, mnTab );
            pCondSet = pDoc->GetCondResult( nCol, nRow, mnTab );
            pBackground = &pPattern->GetItem( ATTR_BACKGROUND, pCondSet );
        }
    }

    return pBackground;
}

static bool lcl_EqualBack( const RowInfo& rFirst, const RowInfo& rOther,
                    SCCOL mnX1, SCCOL mnX2, bool bShowProt, bool bPagebreakMode )
{
    if ( rFirst.bChanged   != rOther.bChanged ||
         rFirst.bEmptyBack != rOther.bEmptyBack )
        return false;

    SCCOL nX;
    if ( bShowProt )
    {
        for ( nX=mnX1; nX<=mnX2; nX++ )
        {
            const ScPatternAttr* pPat1 = rFirst.cellInfo(nX).pPatternAttr;
            const ScPatternAttr* pPat2 = rOther.cellInfo(nX).pPatternAttr;
            if ( !pPat1 || !pPat2 ||
                    !SfxPoolItem::areSame(pPat1->GetItem(ATTR_PROTECTION), pPat2->GetItem(ATTR_PROTECTION) ) )
                return false;
        }
    }
    else
    {
        for ( nX=mnX1; nX<=mnX2; nX++ )
            if ( !SfxPoolItem::areSame(rFirst.cellInfo(nX).maBackground.getItem(), rOther.cellInfo(nX).maBackground.getItem() ) )
                return false;
    }

    if ( rFirst.nRotMaxCol != SC_ROTMAX_NONE || rOther.nRotMaxCol != SC_ROTMAX_NONE )
        for ( nX=mnX1; nX<=mnX2; nX++ )
            if ( rFirst.cellInfo(nX).nRotateDir != rOther.cellInfo(nX).nRotateDir )
                return false;

    if ( bPagebreakMode )
        for ( nX=mnX1; nX<=mnX2; nX++ )
            if ( rFirst.cellInfo(nX).bPrinted != rOther.cellInfo(nX).bPrinted )
                return false;

    for ( nX=mnX1; nX<=mnX2; nX++ )
    {
        std::optional<Color> const & pCol1 = rFirst.cellInfo(nX).mxColorScale;
        std::optional<Color> const & pCol2 = rOther.cellInfo(nX).mxColorScale;
        if( (pCol1 && !pCol2) || (!pCol1 && pCol2) )
            return false;

        if (pCol1 && (*pCol1 != *pCol2))
            return false;

        const ScDataBarInfo* pInfo1 = rFirst.cellInfo(nX).pDataBar;
        const ScDataBarInfo* pInfo2 = rOther.cellInfo(nX).pDataBar;

        if( (pInfo1 && !pInfo2) || (!pInfo1 && pInfo2) )
            return false;

        if (pInfo1 && (*pInfo1 != *pInfo2))
            return false;

        // each cell with an icon set should be painted the same way
        const ScIconSetInfo* pIconSet1 = rFirst.cellInfo(nX).pIconSet;
        const ScIconSetInfo* pIconSet2 = rOther.cellInfo(nX).pIconSet;

        if(pIconSet1 || pIconSet2)
            return false;
    }

    return true;
}

void ScOutputData::DrawDocumentBackground()
{
    if ( !mbSolidBackground )
        return;

    Color aBgColor(ScModule::get()->GetColorConfig().GetColorValue(svtools::DOCCOLOR).nColor);
    mpDev->SetLineColor(aBgColor);
    mpDev->SetFillColor(aBgColor);

    Point aScreenPos  = mpDev->PixelToLogic(Point(mnScrX, mnScrY));
    Size  aScreenSize = mpDev->PixelToLogic(Size(mnScrW - 1,mnScrH - 1));

    mpDev->DrawRect(tools::Rectangle(aScreenPos, aScreenSize));
}

namespace {

const double lclCornerRectTransparency = 40.0;

void drawDataBars(vcl::RenderContext& rRenderContext, const ScDataBarInfo* pOldDataBarInfo, const tools::Rectangle& rRect, tools::Long nOneX, tools::Long nOneY)
{
    tools::Long nPosZero = 0;
    tools::Rectangle aPaintRect = rRect;
    aPaintRect.AdjustTop(2 * nOneY );
    aPaintRect.AdjustBottom( -(2 * nOneY) );
    aPaintRect.AdjustLeft( 2 * nOneX );
    aPaintRect.AdjustRight( -(2 * nOneX) );
    if(pOldDataBarInfo->mnZero)
    {
        // need to calculate null point in cell
        tools::Long nLength = aPaintRect.Right() - aPaintRect.Left();
        nPosZero = static_cast<tools::Long>(aPaintRect.Left() + nLength*pOldDataBarInfo->mnZero/100.0);
    }
    else
    {
        nPosZero = aPaintRect.Left();
    }

    if(pOldDataBarInfo->mnLength < 0)
    {
        aPaintRect.SetRight( nPosZero );
        tools::Long nLength = nPosZero - aPaintRect.Left();
        aPaintRect.SetLeft( nPosZero + static_cast<tools::Long>(nLength * pOldDataBarInfo->mnLength/100.0) );
    }
    else if(pOldDataBarInfo->mnLength > 0)
    {
        aPaintRect.SetLeft( nPosZero );
        tools::Long nLength = aPaintRect.Right() - nPosZero;
        aPaintRect.SetRight( nPosZero + static_cast<tools::Long>(nLength * pOldDataBarInfo->mnLength/100.0) );
    }
    else
        return;

    if(pOldDataBarInfo->mbGradient)
    {
        rRenderContext.SetLineColor(pOldDataBarInfo->maColor);
        Gradient aGradient(css::awt::GradientStyle_LINEAR, pOldDataBarInfo->maColor, COL_TRANSPARENT);
        aGradient.SetSteps(255);

        if(pOldDataBarInfo->mnLength < 0)
            aGradient.SetAngle(2700_deg10);
        else
            aGradient.SetAngle(900_deg10);

        rRenderContext.DrawGradient(aPaintRect, aGradient);

        rRenderContext.SetLineColor();
    }
    else
    {
        rRenderContext.SetFillColor(pOldDataBarInfo->maColor);
        rRenderContext.DrawRect(aPaintRect);
    }

    //draw axis
    if(!(pOldDataBarInfo->mnZero && pOldDataBarInfo->mnZero != 100))
        return;

    Point aPoint1(nPosZero, rRect.Top());
    Point aPoint2(nPosZero, rRect.Bottom());
    LineInfo aLineInfo(LineStyle::Dash, 1);
    aLineInfo.SetDashCount( 4 );
    aLineInfo.SetDistance( 3 );
    aLineInfo.SetDashLen( 3 );
    rRenderContext.SetFillColor(pOldDataBarInfo->maAxisColor);
    rRenderContext.SetLineColor(pOldDataBarInfo->maAxisColor);
    rRenderContext.DrawLine(aPoint1, aPoint2, aLineInfo);
    rRenderContext.SetLineColor();
    rRenderContext.SetFillColor();
}

const BitmapEx& getIcon(sc::IconSetBitmapMap & rIconSetBitmapMap, ScIconSetType eType, sal_Int32 nIndex)
{
    return ScIconSetFormat::getBitmap(rIconSetBitmapMap, eType, nIndex);
}

void drawIconSets(vcl::RenderContext& rRenderContext, const ScIconSetInfo* pOldIconSetInfo, const tools::Rectangle& rRect, tools::Long nOneX, tools::Long nOneY,
        sc::IconSetBitmapMap & rIconSetBitmapMap)
{
    ScIconSetType eType = pOldIconSetInfo->eIconSetType;
    sal_Int32 nIndex = pOldIconSetInfo->nIconIndex;
    const BitmapEx& rIcon = getIcon(rIconSetBitmapMap, eType, nIndex);

    tools::Long aHeight = o3tl::convert(10, o3tl::Length::pt, o3tl::Length::mm100);

    if (pOldIconSetInfo->mnHeight)
    {
        if (comphelper::LibreOfficeKit::isActive())
        {
            aHeight = rRenderContext.LogicToPixel(Size(0, pOldIconSetInfo->mnHeight), MapMode(MapUnit::MapTwip)).Height();
            aHeight *= comphelper::LibreOfficeKit::getDPIScale();
        }
        else
        {
            aHeight = o3tl::convert(pOldIconSetInfo->mnHeight, o3tl::Length::twip, o3tl::Length::mm100);
        }
    }

    Size aSize = rIcon.GetSizePixel();
    double fRatio = static_cast<double>(aSize.Width()) / aSize.Height();
    tools::Long aWidth = fRatio * aHeight;

    rRenderContext.Push();
    rRenderContext.SetClipRegion(vcl::Region(rRect));
    rRenderContext.DrawBitmapEx(Point(rRect.Left() + 2 * nOneX, rRect.Bottom() - 2 * nOneY - aHeight), Size(aWidth, aHeight), rIcon);
    rRenderContext.Pop();
}

void drawCells(vcl::RenderContext& rRenderContext, std::optional<Color> const & pColor, const SvxBrushItem* pBackground, std::optional<Color>& pOldColor, const SvxBrushItem*& pOldBackground,
        tools::Rectangle& rRect, tools::Long nPosX, tools::Long nLayoutSign, tools::Long nOneX, tools::Long nOneY, const ScDataBarInfo* pDataBarInfo, const ScDataBarInfo*& pOldDataBarInfo,
        const ScIconSetInfo* pIconSetInfo, const ScIconSetInfo*& pOldIconSetInfo,
        sc::IconSetBitmapMap & rIconSetBitmapMap)
{
    tools::Long nSignedOneX = nOneX * nLayoutSign;
    // need to paint if old color scale has been used and now
    // we have a different color or a style based background
    // we can here fall back to pointer comparison
    if (pOldColor && (pBackground || pOldColor != pColor || pOldDataBarInfo || pDataBarInfo || pIconSetInfo || pOldIconSetInfo))
    {
        rRect.SetRight( nPosX-nSignedOneX );
        if( !pOldColor->IsTransparent() )
        {
            rRenderContext.SetFillColor( *pOldColor );
            rRenderContext.DrawRect( rRect );
        }
        if( pOldDataBarInfo )
            drawDataBars(rRenderContext, pOldDataBarInfo, rRect, nOneX, nOneY);
        if( pOldIconSetInfo )
            drawIconSets(rRenderContext, pOldIconSetInfo, rRect, nOneX, nOneY, rIconSetBitmapMap);

        rRect.SetLeft( nPosX - nSignedOneX );
    }

    if ( pOldBackground && (pColor || !SfxPoolItem::areSame(pBackground, pOldBackground) || pOldDataBarInfo || pDataBarInfo || pIconSetInfo || pOldIconSetInfo) )
    {
        rRect.SetRight( nPosX-nSignedOneX );
        if (pOldBackground)             // ==0 if hidden
        {
            Color aBackCol = pOldBackground->GetColor();
            if ( !aBackCol.IsTransparent() )      //! partial transparency?
            {
                rRenderContext.SetFillColor( aBackCol );
                rRenderContext.DrawRect( rRect );
            }
        }
        if( pOldDataBarInfo )
            drawDataBars(rRenderContext, pOldDataBarInfo, rRect, nOneX, nOneY);
        if( pOldIconSetInfo )
            drawIconSets(rRenderContext, pOldIconSetInfo, rRect, nOneX, nOneY, rIconSetBitmapMap);

        rRect.SetLeft( nPosX - nSignedOneX );
    }

    if (!pOldBackground && !pOldColor && (pDataBarInfo || pIconSetInfo))
    {
        rRect.SetRight( nPosX -nSignedOneX );
        rRect.SetLeft( nPosX - nSignedOneX );
    }

    if(pColor)
    {
        // only update pOldColor if the colors changed
        if (!pOldColor || *pOldColor != *pColor)
            pOldColor = pColor;

        pOldBackground = nullptr;
    }
    else if(pBackground)
    {
        pOldBackground = pBackground;
        pOldColor.reset();
    }

    if(pDataBarInfo)
        pOldDataBarInfo = pDataBarInfo;
    else
        pOldDataBarInfo = nullptr;

    if(pIconSetInfo)
        pOldIconSetInfo = pIconSetInfo;
    else
        pOldIconSetInfo = nullptr;
}

}

void ScOutputData::DrawBackground(vcl::RenderContext& rRenderContext)
{
    vcl::PDFExtOutDevData* pPDF = dynamic_cast<vcl::PDFExtOutDevData*>(mpDev->GetExtOutDevData());
    bool bTaggedPDF = pPDF && pPDF->GetIsExportTaggedPDF();

    Size aOnePixel = rRenderContext.PixelToLogic(Size(1,1));
    tools::Long nOneXLogic = aOnePixel.Width();
    tools::Long nOneYLogic = aOnePixel.Height();

    // See more about bWorksInPixels in ScOutputData::DrawGrid
    bool bWorksInPixels = (meType == OUTTYPE_WINDOW);
    const tools::Long nOneX = bWorksInPixels ? 1 : nOneXLogic;
    const tools::Long nLayoutSign = mbLayoutRTL ? -1 : 1;
    const tools::Long nSignedOneX = nOneX * nLayoutSign;

    tools::Rectangle aRect;

    rRenderContext.SetLineColor();

    bool bShowProt = mbSyntaxMode && mpDoc->IsTabProtected(mnTab);
    bool bDoAll = bShowProt || mbPagebreakMode || mbSolidBackground;

    bool bCellContrast = mbUseStyleColor &&
            Application::GetSettings().GetStyleSettings().GetHighContrastMode();

    tools::Long nPosY = mnScrY;

    const svtools::ColorConfig& rColorCfg = ScModule::get()->GetColorConfig();
    Color aProtectedColor( rColorCfg.GetColorValue( svtools::CALCPROTECTEDBACKGROUND ).nColor );
    auto pProtectedBackground = std::make_shared<SvxBrushItem>( aProtectedColor, ATTR_BACKGROUND );

    // iterate through the rows to show
    for (SCSIZE nArrY=1; nArrY+1<mnArrCount; nArrY++)
    {
        RowInfo* pThisRowInfo = &mpRowInfo[nArrY];
        tools::Long nRowHeight = pThisRowInfo->nHeight;

        if ( pThisRowInfo->bChanged )
        {
            if ( ( ( pThisRowInfo->bEmptyBack ) || mbSyntaxMode ) && !bDoAll )
            {
                // nothing
            }
            else
            {
                if (bTaggedPDF)
                    pPDF->WrapBeginStructureElement(vcl::pdf::StructElement::NonStructElement);

                // scan for rows with the same background:
                SCSIZE nSkip = 0;
                while ( nArrY+nSkip+2<mnArrCount &&
                        lcl_EqualBack( *pThisRowInfo, mpRowInfo[nArrY+nSkip+1],
                                        mnX1, mnX2, bShowProt, mbPagebreakMode ) )
                {
                    ++nSkip;
                    nRowHeight += mpRowInfo[nArrY+nSkip].nHeight;    // after incrementing
                }

                tools::Long nPosX = mnScrX;
                if ( mbLayoutRTL )
                    nPosX += mnMirrorW - nOneX;

                // tdf#135891 - adjust the x position to ensure the correct starting point
                if (!bWorksInPixels)
                    nPosX -= nLayoutSign + 1;

                aRect = tools::Rectangle(nPosX, nPosY - 1, nPosX, nPosY - 1 + nRowHeight);
                if (bWorksInPixels)
                    aRect = rRenderContext.PixelToLogic(aRect); // internal data in pixels, but we'll be drawing in logic units

                const SvxBrushItem* pOldBackground = nullptr;
                const SvxBrushItem* pBackground = nullptr;
                std::optional<Color> pOldColor;
                const ScDataBarInfo* pOldDataBarInfo = nullptr;
                const ScIconSetInfo* pOldIconSetInfo = nullptr;
                SCCOL nMergedCols = 1;
                SCCOL nOldMerged = 0;

                for (SCCOL nX=mnX1; nX + nMergedCols <= mnX2 + 1; nX += nOldMerged)
                {
                    ScCellInfo* pInfo = &pThisRowInfo->cellInfo(nX-1+nMergedCols);

                    nOldMerged = nMergedCols;

                    tools::Long nNewPosX = nPosX;
                    // extend for all merged cells
                    nMergedCols = 1;
                    if (pInfo->bMerged && pInfo->pPatternAttr)
                    {
                            const ScMergeAttr* pMerge =
                                    &pInfo->pPatternAttr->GetItem(ATTR_MERGE);
                            nMergedCols = std::max<SCCOL>(1, pMerge->GetColMerge());
                    }

                    for (SCCOL nMerged = 0; nMerged < nMergedCols; ++nMerged)
                    {
                        SCCOL nCol = nX+nOldMerged+nMerged;
                        if (nCol > mnX2+2)
                            break;
                        nNewPosX += mpRowInfo[0].basicCellInfo(nCol-1).nWidth * nLayoutSign;
                    }

                    if (nNewPosX == nPosX)
                        continue; // Zero width, no need to draw.

                    if (bCellContrast)
                    {
                        //  high contrast for cell borders and backgrounds -> empty background
                        pBackground = ScGlobal::GetEmptyBrushItem();
                    }
                    else if (bShowProt)         // show cell protection in syntax mode
                    {
                        const ScPatternAttr* pP = pInfo->pPatternAttr;
                        if (pP)
                        {
                            const ScProtectionAttr& rProt = pP->GetItem(ATTR_PROTECTION);
                            if (rProt.GetProtection() || rProt.GetHideCell())
                                pBackground = pProtectedBackground.get();
                            else
                                pBackground = ScGlobal::GetEmptyBrushItem();
                        }
                        else
                            pBackground = nullptr;
                    }
                    else
                        pBackground = static_cast<const SvxBrushItem*>(pInfo->maBackground.getItem());

                    if ( mbPagebreakMode && !pInfo->bPrinted )
                        pBackground = pProtectedBackground.get();

                    if ( pInfo->nRotateDir > ScRotateDir::Standard &&
                            !pBackground->GetColor().IsFullyTransparent() &&
                            !bCellContrast )
                    {
                        SCROW nY = mpRowInfo[nArrY].nRowNo;
                        pBackground = lcl_FindBackground( mpDoc, nX, nY, mnTab );
                    }

                    std::optional<Color> const & pColor = pInfo->mxColorScale;
                    const ScDataBarInfo* pDataBarInfo = pInfo->pDataBar;
                    const ScIconSetInfo* pIconSetInfo = pInfo->pIconSet;

                    tools::Long nPosXLogic = nPosX;
                    if (bWorksInPixels)
                        nPosXLogic = rRenderContext.PixelToLogic(Point(nPosX, 0)).X();

                    drawCells(rRenderContext, pColor, pBackground, pOldColor, pOldBackground, aRect, nPosXLogic, nLayoutSign, nOneXLogic, nOneYLogic, pDataBarInfo, pOldDataBarInfo, pIconSetInfo, pOldIconSetInfo, mpDoc->GetIconSetBitmapMap());

                    nPosX = nNewPosX;
                    // tdf#135891 - adjust the x position to ensure the correct starting point
                    if (!bWorksInPixels && nX == mnX1)
                        nPosX += nSignedOneX + 1;
                }

                tools::Long nPosXLogic = nPosX;
                if (bWorksInPixels)
                    nPosXLogic = rRenderContext.PixelToLogic(Point(nPosX, 0)).X();

                drawCells(rRenderContext, std::optional<Color>(), nullptr, pOldColor, pOldBackground, aRect, nPosXLogic, nLayoutSign, nOneXLogic, nOneYLogic, nullptr, pOldDataBarInfo, nullptr, pOldIconSetInfo, mpDoc->GetIconSetBitmapMap());

                nArrY += nSkip;

                if (bTaggedPDF)
                    pPDF->EndStructureElement();
            }
        }
        nPosY += nRowHeight;
    }
}

void ScOutputData::DrawShadow()
{
    DrawExtraShadow( false, false, false, false );
}

void ScOutputData::DrawExtraShadow(bool bLeft, bool bTop, bool bRight, bool bBottom)
{
    vcl::PDFExtOutDevData* pPDF = dynamic_cast<vcl::PDFExtOutDevData*>(mpDev->GetExtOutDevData());
    bool bTaggedPDF = pPDF && pPDF->GetIsExportTaggedPDF();

    mpDev->SetLineColor();

    const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();
    bool bCellContrast = mbUseStyleColor && rStyleSettings.GetHighContrastMode();
    Color aAutoTextColor;
    if ( bCellContrast )
        aAutoTextColor = ScModule::get()->GetColorConfig().GetColorValue(svtools::FONTCOLOR).nColor;

    tools::Long nInitPosX = mnScrX;
    if ( mbLayoutRTL )
    {
        Size aOnePixel = mpDev->PixelToLogic(Size(1,1));
        tools::Long nOneX = aOnePixel.Width();
        nInitPosX += mnMirrorW - nOneX;
    }
    tools::Long nLayoutSign = mbLayoutRTL ? -1 : 1;

    tools::Long nPosY = mnScrY - mpRowInfo[0].nHeight;
    for (SCSIZE nArrY=0; nArrY<mnArrCount; nArrY++)
    {
        bool bCornerY = ( nArrY == 0 ) || ( nArrY+1 == mnArrCount );
        bool bSkipY = ( nArrY==0 && !bTop ) || ( nArrY+1 == mnArrCount && !bBottom );

        RowInfo* pThisRowInfo = &mpRowInfo[nArrY];
        tools::Long nRowHeight = pThisRowInfo->nHeight;

        if ( pThisRowInfo->bChanged && !bSkipY )
        {
            tools::Long nPosX = nInitPosX - mpRowInfo[0].basicCellInfo(mnX1-1).nWidth * nLayoutSign;
            for (SCCOL nCol=mnX1-1; nCol<=mnX2+1; nCol++)
            {
                bool bCornerX = ( nCol==mnX1-1 || nCol==mnX2+1 );
                bool bSkipX = ( nCol==mnX1-1 && !bLeft ) || ( nCol==mnX2+1 && !bRight );

                for (sal_uInt16 nPass=0; nPass<2; nPass++) // horizontal / vertical
                {
                    const SvxShadowItem* pAttr = nPass ?
                            pThisRowInfo->cellInfo(nCol).pVShadowOrigin :
                            pThisRowInfo->cellInfo(nCol).pHShadowOrigin;
                    if ( pAttr && !bSkipX )
                    {
                        if (bTaggedPDF)
                            pPDF->WrapBeginStructureElement(vcl::pdf::StructElement::NonStructElement);

                        ScShadowPart ePart = nPass ?
                                pThisRowInfo->cellInfo(nCol).eVShadowPart :
                                pThisRowInfo->cellInfo(nCol).eHShadowPart;

                        bool bDo = true;
                        if ( (nPass==0 && bCornerX) || (nPass==1 && bCornerY) )
                            if ( ePart != SC_SHADOW_CORNER )
                                bDo = false;

                        if (bDo)
                        {
                            tools::Long nThisWidth = mpRowInfo[0].basicCellInfo(nCol).nWidth;
                            tools::Long nMaxWidth = nThisWidth;
                            if (!nMaxWidth)
                            {
                                //! direction must depend on shadow location
                                SCCOL nWx = nCol+1;
                                while (nWx<mnX2 && !mpRowInfo[0].basicCellInfo(nWx).nWidth)
                                    ++nWx;
                                nMaxWidth = mpRowInfo[0].basicCellInfo(nWx).nWidth;
                            }

                            // rectangle is in logical orientation
                            tools::Rectangle aRect( nPosX, nPosY,
                                             nPosX + ( nThisWidth - 1 ) * nLayoutSign,
                                             nPosY + mpRowInfo[nArrY].nHeight - 1 );

                            tools::Long nSize = pAttr->GetWidth();
                            tools::Long nSizeX = static_cast<tools::Long>(nSize*mnPPTX);
                            if (nSizeX >= nMaxWidth) nSizeX = nMaxWidth-1;
                            tools::Long nSizeY = static_cast<tools::Long>(nSize*mnPPTY);
                            if (nSizeY >= nRowHeight) nSizeY = nRowHeight-1;

                            nSizeX *= nLayoutSign;      // used only to add to rectangle values

                            SvxShadowLocation eLoc = pAttr->GetLocation();
                            if ( mbLayoutRTL )
                            {
                                //  Shadow location is specified as "visual" (right is always right),
                                //  so the attribute's location value is mirrored here and in FillInfo.
                                switch (eLoc)
                                {
                                    case SvxShadowLocation::BottomRight: eLoc = SvxShadowLocation::BottomLeft;  break;
                                    case SvxShadowLocation::BottomLeft:  eLoc = SvxShadowLocation::BottomRight; break;
                                    case SvxShadowLocation::TopRight:    eLoc = SvxShadowLocation::TopLeft;     break;
                                    case SvxShadowLocation::TopLeft:     eLoc = SvxShadowLocation::TopRight;    break;
                                    default:
                                    {
                                        // added to avoid warnings
                                    }
                                }
                            }

                            if (ePart == SC_SHADOW_HORIZ || ePart == SC_SHADOW_HSTART ||
                                ePart == SC_SHADOW_CORNER)
                            {
                                if (eLoc == SvxShadowLocation::TopLeft || eLoc == SvxShadowLocation::TopRight)
                                    aRect.SetTop( aRect.Bottom() - nSizeY );
                                else
                                    aRect.SetBottom( aRect.Top() + nSizeY );
                            }
                            if (ePart == SC_SHADOW_VERT || ePart == SC_SHADOW_VSTART ||
                                ePart == SC_SHADOW_CORNER)
                            {
                                if (eLoc == SvxShadowLocation::TopLeft || eLoc == SvxShadowLocation::BottomLeft)
                                    aRect.SetLeft( aRect.Right() - nSizeX );
                                else
                                    aRect.SetRight( aRect.Left() + nSizeX );
                            }
                            if (ePart == SC_SHADOW_HSTART)
                            {
                                if (eLoc == SvxShadowLocation::TopLeft || eLoc == SvxShadowLocation::BottomLeft)
                                    aRect.AdjustRight( -nSizeX );
                                else
                                    aRect.AdjustLeft(nSizeX );
                            }
                            if (ePart == SC_SHADOW_VSTART)
                            {
                                if (eLoc == SvxShadowLocation::TopLeft || eLoc == SvxShadowLocation::TopRight)
                                    aRect.AdjustBottom( -nSizeY );
                                else
                                    aRect.AdjustTop(nSizeY );
                            }

                            //! merge rectangles?
                            mpDev->SetFillColor( bCellContrast ? aAutoTextColor : pAttr->GetColor() );
                            mpDev->DrawRect( aRect );

                            if (bTaggedPDF)
                                pPDF->EndStructureElement();
                        }
                    }
                }

                nPosX += mpRowInfo[0].basicCellInfo(nCol).nWidth * nLayoutSign;
            }
        }
        nPosY += nRowHeight;
    }
}

void ScOutputData::DrawClear()
{
    tools::Rectangle aRect;
    Size aOnePixel = mpDev->PixelToLogic(Size(1,1));
    tools::Long nOneX = aOnePixel.Width();
    tools::Long nOneY = aOnePixel.Height();

    // (called only for ScGridWindow)
    Color aBgColor(ScModule::get()->GetColorConfig().GetColorValue(svtools::DOCCOLOR).nColor);

    if (mbMetaFile)
        nOneX = nOneY = 0;

    mpDev->SetLineColor();

    mpDev->SetFillColor( aBgColor );

    tools::Long nPosY = mnScrY;
    for (SCSIZE nArrY=1; nArrY+1<mnArrCount; nArrY++)
    {
        RowInfo* pThisRowInfo = &mpRowInfo[nArrY];
        tools::Long nRowHeight = pThisRowInfo->nHeight;

        if ( pThisRowInfo->bChanged )
        {
            // scan for more rows which must be painted:
            SCSIZE nSkip = 0;
            while ( nArrY+nSkip+2<mnArrCount && mpRowInfo[nArrY+nSkip+1].bChanged )
            {
                ++nSkip;
                nRowHeight += mpRowInfo[nArrY+nSkip].nHeight;    // after incrementing
            }

            aRect = tools::Rectangle( Point( mnScrX, nPosY ),
                    Size( mnScrW+1-nOneX, nRowHeight+1-nOneY) );
            mpDev->DrawRect( aRect );

            nArrY += nSkip;
        }
        nPosY += nRowHeight;
    }
}

// Lines

static tools::Long lclGetSnappedX( const OutputDevice& rDev, tools::Long nPosX, bool mbSnapPixel )
{
    return (mbSnapPixel && nPosX) ? rDev.PixelToLogic( rDev.LogicToPixel( Size( nPosX, 0 ) ) ).Width() : nPosX;
}

static tools::Long lclGetSnappedY( const OutputDevice& rDev, tools::Long nPosY, bool mbSnapPixel )
{
    return (mbSnapPixel && nPosY) ? rDev.PixelToLogic( rDev.LogicToPixel( Size( 0, nPosY ) ) ).Height() : nPosY;
}

void ScOutputData::DrawFrame(vcl::RenderContext& rRenderContext)
{
    vcl::PDFExtOutDevData* pPDF = dynamic_cast<vcl::PDFExtOutDevData*>(mpDev->GetExtOutDevData());
    bool bTaggedPDF = pPDF && pPDF->GetIsExportTaggedPDF();
    if (bTaggedPDF)
        pPDF->WrapBeginStructureElement(vcl::pdf::StructElement::NonStructElement);

    DrawModeFlags nOldDrawMode = rRenderContext.GetDrawMode();

    Color aSingleColor;
    bool bUseSingleColor = false;
    const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();
    bool bCellContrast = mbUseStyleColor && rStyleSettings.GetHighContrastMode();

    //  if a Calc OLE object is embedded in Draw/Impress, the VCL DrawMode is used
    //  for display mode / B&W printing. The VCL DrawMode handling doesn't work for lines
    //  that are drawn with DrawRect, so if the line/background bits are set, the DrawMode
    //  must be reset and the border colors handled here.

    if ( ( nOldDrawMode & DrawModeFlags::WhiteFill ) && ( nOldDrawMode & DrawModeFlags::BlackLine ) )
    {
        rRenderContext.SetDrawMode( nOldDrawMode & (~DrawModeFlags::WhiteFill) );
        aSingleColor = COL_BLACK;
        bUseSingleColor = true;
    }
    else if ( ( nOldDrawMode & DrawModeFlags::SettingsFill ) && ( nOldDrawMode & DrawModeFlags::SettingsLine ) )
    {
        rRenderContext.SetDrawMode( nOldDrawMode & (~DrawModeFlags::SettingsFill) );
        aSingleColor = rStyleSettings.GetWindowTextColor();     // same as used in VCL for DrawModeFlags::SettingsLine
        bUseSingleColor = true;
    }
    else if ( bCellContrast )
    {
        aSingleColor = ScModule::get()->GetColorConfig().GetColorValue(svtools::FONTCOLOR).nColor;
        bUseSingleColor = true;
    }

    const Color* pForceColor = bUseSingleColor ? &aSingleColor : nullptr;

    if (mrTabInfo.maArray.HasCellRotation())
    {
        DrawRotatedFrame(rRenderContext);        // removes the lines that must not be painted here
    }

    tools::Long nInitPosX = mnScrX;
    if ( mbLayoutRTL )
    {
        Size aOnePixel = rRenderContext.PixelToLogic(Size(1,1));
        tools::Long nOneX = aOnePixel.Width();
        nInitPosX += mnMirrorW - nOneX;
    }
    tools::Long nLayoutSign = mbLayoutRTL ? -1 : 1;

    // *** set column and row sizes of the frame border array ***

    svx::frame::Array& rArray = mrTabInfo.maArray;
    size_t nColCount = rArray.GetColCount();
    size_t nRowCount = rArray.GetRowCount();

    // row heights

    // row 0 is not visible (dummy for borders from top) - subtract its height from initial position
    // subtract 1 unit more, because position 0 is first *in* cell, grid line is one unit before
    tools::Long nOldPosY = mnScrY - 1 - mpRowInfo[ 0 ].nHeight;
    tools::Long nOldSnapY = lclGetSnappedY( rRenderContext, nOldPosY, mbSnapPixel );
    rArray.SetYOffset( nOldSnapY );
    for( size_t nRow = 0; nRow < nRowCount; ++nRow )
    {
        tools::Long nNewPosY = nOldPosY + mpRowInfo[ nRow ].nHeight;
        tools::Long nNewSnapY = lclGetSnappedY( rRenderContext, nNewPosY, mbSnapPixel );
        rArray.SetRowHeight( nRow, nNewSnapY - nOldSnapY );
        nOldPosY = nNewPosY;
        nOldSnapY = nNewSnapY;
    }

    // column widths

    // column mnX1-1 is not visible (dummy for borders from left) - subtract its width from initial position
    // subtract 1 unit more, because position 0 is first *in* cell, grid line is one unit above
    tools::Long nOldPosX = nInitPosX - nLayoutSign * (1 + mpRowInfo[ 0 ].basicCellInfo( mnX1 - 1 ).nWidth);
    tools::Long nOldSnapX = lclGetSnappedX( rRenderContext, nOldPosX, mbSnapPixel );
    // set X offset for left-to-right sheets; for right-to-left sheets this is done after for() loop
    if( !mbLayoutRTL )
        rArray.SetXOffset( nOldSnapX );
    for( SCCOL nCol = mnX1 - 1; nCol <= mnX2 + 1; ++nCol )
    {
        size_t nArrCol = mbLayoutRTL ? mnX2 + 1 - nCol : nCol - (mnX1 - 1);
        tools::Long nNewPosX = nOldPosX + mpRowInfo[ 0 ].basicCellInfo( nCol ).nWidth * nLayoutSign;
        tools::Long nNewSnapX = lclGetSnappedX( rRenderContext, nNewPosX, mbSnapPixel );
        rArray.SetColWidth( nArrCol, std::abs( nNewSnapX - nOldSnapX ) );
        nOldPosX = nNewPosX;
        nOldSnapX = nNewSnapX;
    }
    if( mbLayoutRTL )
        rArray.SetXOffset( nOldSnapX );

    // *** draw the array ***

    size_t nFirstCol = 1;
    size_t nFirstRow = 1;
    size_t nLastCol = nColCount - 2;
    size_t nLastRow = nRowCount - 2;

    if( mrTabInfo.mbPageMode )
        rArray.SetClipRange( nFirstCol, nFirstRow, nLastCol, nLastRow );

    // draw only rows with set RowInfo::bChanged flag
    size_t nRow1 = nFirstRow;
    std::unique_ptr<drawinglayer::processor2d::BaseProcessor2D> pProcessor(CreateProcessor2D());
    if (!pProcessor)
        return;
    while( nRow1 <= nLastRow )
    {
        while( (nRow1 <= nLastRow) && !mpRowInfo[ nRow1 ].bChanged ) ++nRow1;
        if( nRow1 <= nLastRow )
        {
            size_t nRow2 = nRow1;
            while( (nRow2 + 1 <= nLastRow) && mpRowInfo[ nRow2 + 1 ].bChanged ) ++nRow2;
            auto xPrimitive = rArray.CreateB2DPrimitiveRange(
                    nFirstCol, nRow1, nLastCol, nRow2, pForceColor );
            pProcessor->process(xPrimitive);
            nRow1 = nRow2 + 1;
        }
    }
    pProcessor.reset();

    rRenderContext.SetDrawMode(nOldDrawMode);

    if (bTaggedPDF)
        pPDF->EndStructureElement();
}

void ScOutputData::DrawRotatedFrame(vcl::RenderContext& rRenderContext)
{
    //! save nRotMax
    SCCOL nRotMax = mnX2;
    for (SCSIZE nRotY=0; nRotY<mnArrCount; nRotY++)
        if (mpRowInfo[nRotY].nRotMaxCol != SC_ROTMAX_NONE && mpRowInfo[nRotY].nRotMaxCol > nRotMax)
            nRotMax = mpRowInfo[nRotY].nRotMaxCol;

    const ScPatternAttr* pPattern;
    const SfxItemSet*    pCondSet;

    const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();
    bool bCellContrast = mbUseStyleColor && rStyleSettings.GetHighContrastMode();

    tools::Long nInitPosX = mnScrX;
    if ( mbLayoutRTL )
    {
        Size aOnePixel = rRenderContext.PixelToLogic(Size(1,1));
        tools::Long nOneX = aOnePixel.Width();
        nInitPosX += mnMirrorW - nOneX;
    }
    tools::Long nLayoutSign = mbLayoutRTL ? -1 : 1;

    tools::Rectangle aClipRect( Point(mnScrX, mnScrY), Size(mnScrW, mnScrH) );
    if (mbMetaFile)
    {
        rRenderContext.Push();
        rRenderContext.IntersectClipRegion( aClipRect );
    }
    else
        rRenderContext.SetClipRegion( vcl::Region( aClipRect ) );

    std::unique_ptr<drawinglayer::processor2d::BaseProcessor2D> pProcessor(CreateProcessor2D( ));
    tools::Long nPosY = mnScrY;
    for (SCSIZE nArrY=1; nArrY<mnArrCount; nArrY++)
    {
        // Rotated is also drawn one line above/below Changed if parts extend into the cell

        RowInfo& rPrevRowInfo = mpRowInfo[nArrY-1];
        RowInfo& rThisRowInfo = mpRowInfo[nArrY];
        RowInfo& rNextRowInfo = mpRowInfo[nArrY+1];

        tools::Long nRowHeight = rThisRowInfo.nHeight;
        if ( rThisRowInfo.nRotMaxCol != SC_ROTMAX_NONE &&
             ( rThisRowInfo.bChanged || rPrevRowInfo.bChanged ||
               ( nArrY+1<mnArrCount && rNextRowInfo.bChanged ) ) )
        {
            SCROW nY = rThisRowInfo.nRowNo;
            tools::Long nPosX = 0;
            SCCOL nX;
            for (nX=0; nX<=nRotMax; nX++)
            {
                if (nX==mnX1) nPosX = nInitPosX;     // calculated individually for preceding positions

                ScCellInfo* pInfo = &rThisRowInfo.cellInfo(nX);
                tools::Long nColWidth = mpRowInfo[0].basicCellInfo(nX).nWidth;
                if ( pInfo->nRotateDir > ScRotateDir::Standard &&
                        !pInfo->bHOverlapped && !pInfo->bVOverlapped )
                {
                    pPattern = pInfo->pPatternAttr;
                    pCondSet = pInfo->pConditionSet;
                    if (!pPattern)
                    {
                        pPattern = mpDoc->GetPattern( nX, nY, mnTab );
                        pInfo->pPatternAttr = pPattern;
                        pCondSet = mpDoc->GetCondResult( nX, nY, mnTab );
                        pInfo->pConditionSet = pCondSet;
                    }

                    //! LastPattern etc.

                    Degree100 nAttrRotate = pPattern->GetRotateVal( pCondSet );
                    SvxRotateMode eRotMode =
                                    pPattern->GetItem(ATTR_ROTATE_MODE, pCondSet).GetValue();

                    if (nAttrRotate)
                    {
                        if (nX < mnX1)         // compute negative position
                        {
                            nPosX = nInitPosX;
                            SCCOL nCol = mnX1;
                            while (nCol > nX)
                            {
                                --nCol;
                                nPosX -= nLayoutSign * static_cast<tools::Long>(mpRowInfo[0].basicCellInfo(nCol).nWidth);
                            }
                        }

                        // start position minus 1 so rotated backgrounds suit the border
                        // (border is on the grid)

                        tools::Long nTop = nPosY - 1;
                        tools::Long nBottom = nPosY + nRowHeight - 1;
                        tools::Long nTopLeft = nPosX - nLayoutSign;
                        tools::Long nTopRight = nPosX + (nColWidth - 1) * nLayoutSign;
                        tools::Long nBotLeft = nTopLeft;
                        tools::Long nBotRight = nTopRight;

                        // inclusion of the sign here hasn't been decided yet
                        // (if not, the extension of the non-rotated background must also be changed)
                        double nRealOrient = nLayoutSign * toRadians(nAttrRotate);     // 1/100th degrees
                        double nCos = cos(nRealOrient);
                        double nSin = sin(nRealOrient);
                        //! restrict !!!
                        tools::Long nSkew = static_cast<tools::Long>(nRowHeight * nCos / nSin);

                        switch (eRotMode)
                        {
                        case SVX_ROTATE_MODE_BOTTOM:
                            nTopLeft += nSkew;
                            nTopRight += nSkew;
                            break;
                        case SVX_ROTATE_MODE_CENTER:
                            nSkew /= 2;
                            nTopLeft += nSkew;
                            nTopRight += nSkew;
                            nBotLeft -= nSkew;
                            nBotRight -= nSkew;
                            break;
                        case SVX_ROTATE_MODE_TOP:
                            nBotLeft -= nSkew;
                            nBotRight -= nSkew;
                            break;
                        default:
                        {
                            // added to avoid warnings
                        }
                        }

                        Point aPoints[4];
                        aPoints[0] = Point(nTopLeft, nTop);
                        aPoints[1] = Point(nTopRight, nTop);
                        aPoints[2] = Point(nBotRight, nBottom);
                        aPoints[3] = Point(nBotLeft, nBottom);

                        const SvxBrushItem* pBackground(static_cast<const SvxBrushItem*>(pInfo->maBackground.getItem()));
                        if (!pBackground)
                            pBackground = &pPattern->GetItem(ATTR_BACKGROUND, pCondSet);
                        if (bCellContrast)
                        {
                            //  high contrast for cell borders and backgrounds -> empty background
                            pBackground = ScGlobal::GetEmptyBrushItem();
                        }
                        if (!pInfo->mxColorScale)
                        {
                            const Color& rColor = pBackground->GetColor();
                            if (rColor.GetAlpha() != 0)
                            {
                                //  draw background only for the changed row itself
                                //  (background doesn't extend into other cells).
                                //  For the borders (rotated and normal), clipping should be
                                //  set if the row isn't changed, but at least the borders
                                //  don't cover the cell contents.
                                if (rThisRowInfo.bChanged)
                                {
                                    tools::Polygon aPoly(4, aPoints);

                                    // for DrawPolygon, without Pen one pixel is left out
                                    // to the right and below...
                                    if (!rColor.IsTransparent())
                                        rRenderContext.SetLineColor(rColor);
                                    else
                                        rRenderContext.SetLineColor();
                                    rRenderContext.SetFillColor(rColor);
                                    rRenderContext.DrawPolygon(aPoly);
                                }
                            }
                        }
                        else
                        {
                            tools::Polygon aPoly(4, aPoints);
                            std::optional<Color> const & pColor = pInfo->mxColorScale;

                            // for DrawPolygon, without Pen one pixel is left out
                            // to the right and below...
                            if (!pColor->IsTransparent())
                                rRenderContext.SetLineColor(*pColor);
                            else
                                rRenderContext.SetLineColor();
                            rRenderContext.SetFillColor(*pColor);
                            rRenderContext.DrawPolygon(aPoly);

                        }
                    }
                }
                nPosX += nColWidth * nLayoutSign;
            }
        }
        nPosY += nRowHeight;
    }

    pProcessor.reset();

    if (mbMetaFile)
        rRenderContext.Pop();
    else
        rRenderContext.SetClipRegion();
}

std::unique_ptr<drawinglayer::processor2d::BaseProcessor2D> ScOutputData::CreateProcessor2D( )
{
    mpDoc->InitDrawLayer(mpDoc->GetDocumentShell());
    ScDrawLayer* pDrawLayer = mpDoc->GetDrawLayer();
    if (!pDrawLayer)
        return nullptr;

    basegfx::B2DRange aViewRange;
    SdrPage *pDrawPage = pDrawLayer->GetPage( static_cast< sal_uInt16 >( mnTab ) );
    drawinglayer::geometry::ViewInformation2D aNewViewInfos;
    aNewViewInfos.setViewTransformation(mpDev->GetViewTransformation());
    aNewViewInfos.setViewport(aViewRange);
    aNewViewInfos.setVisualizedPage(GetXDrawPageForSdrPage( pDrawPage ));

    return drawinglayer::processor2d::createProcessor2DFromOutputDevice(
                    *mpDev, aNewViewInfos );
}

// Printer

vcl::Region ScOutputData::GetChangedAreaRegion()
{
    vcl::Region aRegion;
    tools::Rectangle aDrawingRect;
    bool bHad(false);
    tools::Long nPosY = mnScrY;
    SCSIZE nArrY;

    aDrawingRect.SetLeft( mnScrX );
    aDrawingRect.SetRight( mnScrX+mnScrW-1 );

    for(nArrY=1; nArrY+1<mnArrCount; nArrY++)
    {
        RowInfo* pThisRowInfo = &mpRowInfo[nArrY];

        if(pThisRowInfo->bChanged)
        {
            if(!bHad)
            {
                aDrawingRect.SetTop( nPosY );
                bHad = true;
            }

            aDrawingRect.SetBottom( nPosY + mpRowInfo[nArrY].nHeight - 1 );
        }
        else if(bHad)
        {
            aRegion.Union(mpDev->PixelToLogic(aDrawingRect));
            bHad = false;
        }

        nPosY += mpRowInfo[nArrY].nHeight;
    }

    if(bHad)
    {
        aRegion.Union(mpDev->PixelToLogic(aDrawingRect));
    }

    return aRegion;
}

bool ScOutputData::SetChangedClip()
{
    tools::PolyPolygon aPoly;

    tools::Rectangle aDrawingRect;
    aDrawingRect.SetLeft( mnScrX );
    aDrawingRect.SetRight( mnScrX+mnScrW-1 );

    bool    bHad    = false;
    tools::Long    nPosY   = mnScrY;
    SCSIZE  nArrY;
    for (nArrY=1; nArrY+1<mnArrCount; nArrY++)
    {
        RowInfo* pThisRowInfo = &mpRowInfo[nArrY];

        if ( pThisRowInfo->bChanged )
        {
            if (!bHad)
            {
                aDrawingRect.SetTop( nPosY );
                bHad = true;
            }
            aDrawingRect.SetBottom( nPosY + mpRowInfo[nArrY].nHeight - 1 );
        }
        else if (bHad)
        {
            aPoly.Insert( tools::Polygon( mpDev->PixelToLogic(aDrawingRect) ) );
            bHad = false;
        }
        nPosY += mpRowInfo[nArrY].nHeight;
    }

    if (bHad)
        aPoly.Insert( tools::Polygon( mpDev->PixelToLogic(aDrawingRect) ) );

    bool bRet = (aPoly.Count() != 0);
    if (bRet)
        mpDev->SetClipRegion(vcl::Region(aPoly));
    return bRet;
}

void ScOutputData::FindChanged()
{
    SCCOL   nX;
    SCSIZE  nArrY;

    bool bWasIdleEnabled = mpDoc->IsIdleEnabled();
    mpDoc->EnableIdle(false);
    for (nArrY=0; nArrY<mnArrCount; nArrY++)
        mpRowInfo[nArrY].bChanged = false;

    SCCOL nCol1 = mpDoc->MaxCol(), nCol2 = 0;
    SCROW nRow1 = mpDoc->MaxRow(), nRow2 = 0;
    bool bAnyDirty = false;
    bool bAnyChanged = false;

    for (nArrY=0; nArrY<mnArrCount; nArrY++)
    {
        RowInfo* pThisRowInfo = &mpRowInfo[nArrY];
        for (nX=mnX1; nX<=mnX2; nX++)
        {
            const ScRefCellValue& rCell = pThisRowInfo->cellInfo(nX).maCell;

            if (rCell.getType() != CELLTYPE_FORMULA)
                continue;

            ScFormulaCell* pFCell = rCell.getFormula();
            if (pFCell->IsRunning())
                // still being interpreted. Skip it.
                continue;

            bool bDirty = pFCell->GetDirty();
            bAnyChanged = bAnyChanged || pFCell->IsChanged();

            if (bDirty)
            {
                if (!bAnyDirty)
                {
                    ScProgress::CreateInterpretProgress(mpDoc);
                    bAnyDirty = true;
                }

                ScAddress& rPos(pFCell->aPos);
                nCol1 = std::min(rPos.Col(), nCol1);
                nCol2 = std::max(rPos.Col(), nCol2);
                nRow1 = std::min(rPos.Row(), nRow1);
                nRow2 = std::max(rPos.Row(), nRow2);

                const SfxUInt32Item* pItem = mpDoc->GetAttr(rPos, ATTR_VALIDDATA);
                const ScValidationData* pData = mpDoc->GetValidationEntry(pItem->GetValue());
                if (pData)
                {
                    ScRefCellValue aCell(*mpDoc, rPos);
                    if (pData->IsDataValid(aCell, rPos))
                        ScDetectiveFunc(*mpDoc, rPos.Tab()).DeleteCirclesAt(rPos.Col(), rPos.Row());
                }
            }
        }
    }

    if (bAnyDirty || bAnyChanged)
    {
        if (bAnyDirty)
            mpDoc->EnsureFormulaCellResults(ScRange(nCol1, nRow1, mnTab, nCol2, nRow2, mnTab), true);

        for (nArrY=0; nArrY<mnArrCount; nArrY++)
        {
            RowInfo* pThisRowInfo = &mpRowInfo[nArrY];
            for (nX=mnX1; nX<=mnX2; nX++)
            {
                const ScRefCellValue& rCell = pThisRowInfo->cellInfo(nX).maCell;

                if (rCell.getType() != CELLTYPE_FORMULA)
                    continue;

                ScFormulaCell* pFCell = rCell.getFormula();
                if (pFCell->IsRunning())
                    // still being interpreted. Skip it.
                    continue;

                if (!pFCell->IsChanged())
                    // the result hasn't changed. Skip it.
                    continue;

                pThisRowInfo->bChanged = true;
                if ( pThisRowInfo->cellInfo(nX).bMerged )
                {
                    SCSIZE nOverY = nArrY + 1;
                    while ( nOverY<mnArrCount &&
                            mpRowInfo[nOverY].cellInfo(nX).bVOverlapped )
                    {
                        mpRowInfo[nOverY].bChanged = true;
                        ++nOverY;
                    }
                }
            }
        }

        if (bAnyDirty)
            ScProgress::DeleteInterpretProgress();
    }

    mpDoc->EnableIdle(bWasIdleEnabled);
}

ReferenceMark ScOutputData::FillReferenceMark( SCCOL nRefStartX, SCROW nRefStartY,
                                SCCOL nRefEndX, SCROW nRefEndY, const Color& rColor)
{
    ReferenceMark aResult;

    PutInOrder( nRefStartX, nRefEndX );
    PutInOrder( nRefStartY, nRefEndY );

    if ( nRefStartX == nRefEndX && nRefStartY == nRefEndY )
        mpDoc->ExtendMerge( nRefStartX, nRefStartY, nRefEndX, nRefEndY, mnTab );

    if ( nRefStartX <= mnVisX2 && nRefEndX >= mnVisX1 &&
         nRefStartY <= mnVisY2 && nRefEndY >= mnVisY1 )
    {
        tools::Long nMinX = mnScrX;
        tools::Long nMinY = mnScrY;
        tools::Long nMaxX = mnScrX + mnScrW - 1;
        tools::Long nMaxY = mnScrY + mnScrH - 1;
        if ( mbLayoutRTL )
            std::swap( nMinX, nMaxX );
        tools::Long nLayoutSign = mbLayoutRTL ? -1 : 1;

        bool bTop    = false;
        bool bBottom = false;
        bool bLeft   = false;
        bool bRight  = false;

        tools::Long nPosY = mnScrY;
        bool bNoStartY = ( mnY1 < nRefStartY );
        bool bNoEndY   = false;
        for (SCSIZE nArrY=1; nArrY<mnArrCount; nArrY++)      // loop to end for bNoEndY check
        {
            SCROW nY = mpRowInfo[nArrY].nRowNo;

            if ( nY==nRefStartY || (nY>nRefStartY && bNoStartY) )
            {
                nMinY = nPosY;
                bTop = true;
            }
            if ( nY==nRefEndY )
            {
                nMaxY = nPosY + mpRowInfo[nArrY].nHeight - 2;
                bBottom = true;
            }
            if ( nY>nRefEndY && bNoEndY )
            {
                nMaxY = nPosY-2;
                bBottom = true;
            }
            bNoStartY = ( nY < nRefStartY );
            bNoEndY   = ( nY < nRefEndY );
            nPosY += mpRowInfo[nArrY].nHeight;
        }

        tools::Long nPosX = mnScrX;
        if ( mbLayoutRTL )
            nPosX += mnMirrorW - 1;      // always in pixels

        for (SCCOL nX=mnX1; nX<=mnX2; nX++)
        {
            if ( nX==nRefStartX )
            {
                nMinX = nPosX;
                bLeft = true;
            }
            if ( nX==nRefEndX )
            {
                nMaxX = nPosX + ( mpRowInfo[0].basicCellInfo(nX).nWidth - 2 ) * nLayoutSign;
                bRight = true;
            }
            nPosX += mpRowInfo[0].basicCellInfo(nX).nWidth * nLayoutSign;
        }

        if (bTop && bBottom && bLeft && bRight)
        {
            // mnPPT[XY] already has the factor aZoom[XY] in it.
            aResult = ReferenceMark( nMinX / mnPPTX,
                                     nMinY / mnPPTY,
                                     ( nMaxX - nMinX ) / mnPPTX,
                                     ( nMaxY - nMinY ) / mnPPTY,
                                     mnTab,
                                     rColor );
        }
    }

    return aResult;
}

void ScOutputData::DrawRefMark( SCCOL nRefStartX, SCROW nRefStartY,
                                SCCOL nRefEndX, SCROW nRefEndY,
                                const Color& rColor, bool bHandle )
{
    PutInOrder( nRefStartX, nRefEndX );
    PutInOrder( nRefStartY, nRefEndY );

    if ( nRefStartX == nRefEndX && nRefStartY == nRefEndY )
        mpDoc->ExtendMerge( nRefStartX, nRefStartY, nRefEndX, nRefEndY, mnTab );
    else if (mpDoc->ValidCol(nRefEndX) && mpDoc->ValidRow(nRefEndY) &&
             mpDoc->HasAttrib(nRefEndX, nRefEndY, mnTab, HasAttrFlags::Merged))
        mpDoc->ExtendMerge(nRefEndX, nRefEndY, nRefEndX, nRefEndY, mnTab);

    if ( !(nRefStartX <= mnVisX2 && nRefEndX >= mnVisX1 &&
         nRefStartY <= mnVisY2 && nRefEndY >= mnVisY1) )
        return;

    tools::Long nMinX = mnScrX;
    tools::Long nMinY = mnScrY;
    tools::Long nMaxX = mnScrX + mnScrW - 1;
    tools::Long nMaxY = mnScrY + mnScrH - 1;
    if ( mbLayoutRTL )
        std::swap( nMinX, nMaxX );
    tools::Long nLayoutSign = mbLayoutRTL ? -1 : 1;

    bool bTop    = false;
    bool bBottom = false;
    bool bLeft   = false;
    bool bRight  = false;

    tools::Long nPosY = mnScrY;
    bool bNoStartY = ( mnY1 < nRefStartY );
    bool bNoEndY   = false;
    for (SCSIZE nArrY=1; nArrY<mnArrCount; nArrY++)      // loop to end for bNoEndY check
    {
        SCROW nY = mpRowInfo[nArrY].nRowNo;

        if ( nY==nRefStartY || (nY>nRefStartY && bNoStartY) )
        {
            nMinY = nPosY;
            bTop = true;
        }
        if ( nY==nRefEndY )
        {
            nMaxY = nPosY + mpRowInfo[nArrY].nHeight - 2;
            bBottom = true;
        }
        if ( nY>nRefEndY && bNoEndY )
        {
            nMaxY = nPosY-2;
            bBottom = true;
        }
        bNoStartY = ( nY < nRefStartY );
        bNoEndY   = ( nY < nRefEndY );
        nPosY += mpRowInfo[nArrY].nHeight;
    }

    tools::Long nPosX = mnScrX;
    if ( mbLayoutRTL )
        nPosX += mnMirrorW - 1;      // always in pixels

    for (SCCOL nX=mnX1; nX<=mnX2; nX++)
    {
        if ( nX==nRefStartX )
        {
            nMinX = nPosX;
            bLeft = true;
        }
        if ( nX==nRefEndX )
        {
            nMaxX = nPosX + ( mpRowInfo[0].basicCellInfo(nX).nWidth - 2 ) * nLayoutSign;
            bRight = true;
        }
        nPosX += mpRowInfo[0].basicCellInfo(nX).nWidth * nLayoutSign;
    }

    if ( nMaxX * nLayoutSign < nMinX * nLayoutSign || nMaxY < nMinY )
        return;

    mpDev->SetLineColor( rColor );
    if (bTop && bBottom && bLeft && bRight && !comphelper::LibreOfficeKit::isActive() )
    {
            mpDev->SetFillColor();
            mpDev->DrawRect( tools::Rectangle( nMinX, nMinY, nMaxX, nMaxY ) );
    }
    else if ( !comphelper::LibreOfficeKit::isActive() )
    {
        if (bTop)
            mpDev->DrawLine( Point( nMinX, nMinY ), Point( nMaxX, nMinY ) );
        if (bBottom)
            mpDev->DrawLine( Point( nMinX, nMaxY ), Point( nMaxX, nMaxY ) );
        if (bLeft)
            mpDev->DrawLine( Point( nMinX, nMinY ), Point( nMinX, nMaxY ) );
        if (bRight)
            mpDev->DrawLine( Point( nMaxX, nMinY ), Point( nMaxX, nMaxY ) );
    }
    if ( !bHandle || !bRight || !bBottom || comphelper::LibreOfficeKit::isActive() )
        return;

    mpDev->SetLineColor( rColor );
    mpDev->SetFillColor( rColor );

    const sal_Int32 aRadius = 4;

    sal_Int32 aRectMaxX1 = nMaxX - nLayoutSign * aRadius;
    sal_Int32 aRectMaxX2 = nMaxX + nLayoutSign;
    sal_Int32 aRectMinX1 = nMinX - nLayoutSign;
    sal_Int32 aRectMinX2 = nMinX + nLayoutSign * aRadius;

    sal_Int32 aRectMaxY1 = nMaxY - aRadius;
    sal_Int32 aRectMaxY2 = nMaxY + 1;
    sal_Int32 aRectMinY1 = nMinY - 1;
    sal_Int32 aRectMinY2 = nMinY + aRadius;

    // Draw corner rectangles
    tools::Rectangle aLowerRight( aRectMaxX1, aRectMaxY1, aRectMaxX2, aRectMaxY2 );
    tools::Rectangle aUpperLeft ( aRectMinX1, aRectMinY1, aRectMinX2, aRectMinY2 );
    tools::Rectangle aLowerLeft ( aRectMinX1, aRectMaxY1, aRectMinX2, aRectMaxY2 );
    tools::Rectangle aUpperRight( aRectMaxX1, aRectMinY1, aRectMaxX2, aRectMinY2 );

    mpDev->DrawTransparent( tools::PolyPolygon( tools::Polygon( aLowerRight ) ), lclCornerRectTransparency );
    mpDev->DrawTransparent( tools::PolyPolygon( tools::Polygon( aUpperLeft  ) ), lclCornerRectTransparency );
    mpDev->DrawTransparent( tools::PolyPolygon( tools::Polygon( aLowerLeft  ) ), lclCornerRectTransparency );
    mpDev->DrawTransparent( tools::PolyPolygon( tools::Polygon( aUpperRight ) ), lclCornerRectTransparency );
}

void ScOutputData::DrawOneChange( SCCOL nRefStartX, SCROW nRefStartY,
                                SCCOL nRefEndX, SCROW nRefEndY,
                                const Color& rColor, sal_uInt16 nType )
{
    PutInOrder( nRefStartX, nRefEndX );
    PutInOrder( nRefStartY, nRefEndY );

    if ( nRefStartX == nRefEndX && nRefStartY == nRefEndY )
        mpDoc->ExtendMerge( nRefStartX, nRefStartY, nRefEndX, nRefEndY, mnTab );

    if ( !(nRefStartX <= mnVisX2 + 1 && nRefEndX >= mnVisX1 &&
         nRefStartY <= mnVisY2 + 1 && nRefEndY >= mnVisY1) )       // +1 because it touches next cells left/top
        return;

    tools::Long nMinX = mnScrX;
    tools::Long nMinY = mnScrY;
    tools::Long nMaxX = mnScrX+mnScrW-1;
    tools::Long nMaxY = mnScrY+mnScrH-1;
    if ( mbLayoutRTL )
        std::swap( nMinX, nMaxX );
    tools::Long nLayoutSign = mbLayoutRTL ? -1 : 1;

    bool bTop    = false;
    bool bBottom = false;
    bool bLeft   = false;
    bool bRight  = false;

    tools::Long nPosY = mnScrY;
    bool bNoStartY = ( mnY1 < nRefStartY );
    bool bNoEndY   = false;
    for (SCSIZE nArrY=1; nArrY<mnArrCount; nArrY++)      // loop to end for bNoEndY check
    {
        SCROW nY = mpRowInfo[nArrY].nRowNo;

        if ( nY==nRefStartY || (nY>nRefStartY && bNoStartY) )
        {
            nMinY = nPosY - 1;
            bTop = true;
        }
        if ( nY==nRefEndY )
        {
            nMaxY = nPosY + mpRowInfo[nArrY].nHeight - 1;
            bBottom = true;
        }
        if ( nY>nRefEndY && bNoEndY )
        {
            nMaxY = nPosY - 1;
            bBottom = true;
        }
        bNoStartY = ( nY < nRefStartY );
        bNoEndY   = ( nY < nRefEndY );
        nPosY += mpRowInfo[nArrY].nHeight;
    }

    tools::Long nPosX = mnScrX;
    if ( mbLayoutRTL )
        nPosX += mnMirrorW - 1;      // always in pixels

    for (SCCOL nX=mnX1; nX<=mnX2+1; nX++)
    {
        if ( nX==nRefStartX )
        {
            nMinX = nPosX - nLayoutSign;
            bLeft = true;
        }
        if ( nX==nRefEndX )
        {
            nMaxX = nPosX + ( mpRowInfo[0].basicCellInfo(nX).nWidth - 1 ) * nLayoutSign;
            bRight = true;
        }
        nPosX += mpRowInfo[0].basicCellInfo(nX).nWidth * nLayoutSign;
    }

    if ( nMaxX * nLayoutSign < nMinX * nLayoutSign || nMaxY < nMinY )
        return;

    if ( nType == SC_CAT_DELETE_ROWS )
        bLeft = bRight = bBottom = false;       //! thick lines???
    else if ( nType == SC_CAT_DELETE_COLS )
        bTop = bBottom = bRight = false;        //! thick lines???

    mpDev->SetLineColor( rColor );
    if (bTop && bBottom && bLeft && bRight)
    {
        mpDev->SetFillColor();
        mpDev->DrawRect( tools::Rectangle( nMinX, nMinY, nMaxX, nMaxY ) );
    }
    else
    {
        if (bTop)
        {
            mpDev->DrawLine( Point( nMinX,nMinY ), Point( nMaxX,nMinY ) );
            if ( nType == SC_CAT_DELETE_ROWS )
                mpDev->DrawLine( Point( nMinX,nMinY+1 ), Point( nMaxX,nMinY+1 ) );
        }
        if (bBottom)
            mpDev->DrawLine( Point( nMinX,nMaxY ), Point( nMaxX,nMaxY ) );
        if (bLeft)
        {
            mpDev->DrawLine( Point( nMinX,nMinY ), Point( nMinX,nMaxY ) );
            if ( nType == SC_CAT_DELETE_COLS )
                mpDev->DrawLine( Point( nMinX+nLayoutSign,nMinY ), Point( nMinX+nLayoutSign,nMaxY ) );
        }
        if (bRight)
            mpDev->DrawLine( Point( nMaxX,nMinY ), Point( nMaxX,nMaxY ) );
    }
    if ( bLeft && bTop )
    {
        mpDev->SetLineColor();
        mpDev->SetFillColor( rColor );
        mpDev->DrawRect( tools::Rectangle( nMinX+nLayoutSign, nMinY+1, nMinX+3*nLayoutSign, nMinY+3 ) );
    }
}

void ScOutputData::DrawChangeTrack()
{
    ScChangeTrack* pTrack = mpDoc->GetChangeTrack();
    ScChangeViewSettings* pSettings = mpDoc->GetChangeViewSettings();
    if ( !pTrack || !pTrack->GetFirst() || !pSettings || !pSettings->ShowChanges() )
        return;         // nothing there or hidden

    ScActionColorChanger aColorChanger(*pTrack);

    //  clipping happens from the outside
    //! without clipping, only paint affected cells ??!??!?

    SCCOL nEndX = mnX2;
    SCROW nEndY = mnY2;
    if ( nEndX < mpDoc->MaxCol() ) ++nEndX;      // also from the next cell since the mark
    if ( nEndY < mpDoc->MaxRow() ) ++nEndY;      // protrudes from the preceding cell
    ScRange aViewRange( mnX1, mnY1, mnTab, nEndX, nEndY, mnTab );
    const ScChangeAction* pAction = pTrack->GetFirst();
    while (pAction)
    {
        if ( pAction->IsVisible() )
        {
            ScChangeActionType eActionType = pAction->GetType();
            const ScBigRange& rBig = pAction->GetBigRange();
            if ( rBig.aStart.Tab() == mnTab )
            {
                ScRange aRange = rBig.MakeRange( *mpDoc );

                if ( eActionType == SC_CAT_DELETE_ROWS )
                    aRange.aEnd.SetRow( aRange.aStart.Row() );
                else if ( eActionType == SC_CAT_DELETE_COLS )
                    aRange.aEnd.SetCol( aRange.aStart.Col() );

                if ( aRange.Intersects( aViewRange ) &&
                     ScViewUtil::IsActionShown( *pAction, *pSettings, *mpDoc ) )
                {
                    aColorChanger.Update( *pAction );
                    Color aColor( aColorChanger.GetColor() );
                    DrawOneChange( aRange.aStart.Col(), aRange.aStart.Row(),
                                    aRange.aEnd.Col(), aRange.aEnd.Row(), aColor, sal::static_int_cast<sal_uInt16>(eActionType) );

                }
            }
            if ( eActionType == SC_CAT_MOVE &&
                    static_cast<const ScChangeActionMove*>(pAction)->
                        GetFromRange().aStart.Tab() == mnTab )
            {
                ScRange aRange = static_cast<const ScChangeActionMove*>(pAction)->
                        GetFromRange().MakeRange( *mpDoc );
                if ( aRange.Intersects( aViewRange ) &&
                     ScViewUtil::IsActionShown( *pAction, *pSettings, *mpDoc ) )
                {
                    aColorChanger.Update( *pAction );
                    Color aColor( aColorChanger.GetColor() );
                    DrawOneChange( aRange.aStart.Col(), aRange.aStart.Row(),
                                    aRange.aEnd.Col(), aRange.aEnd.Row(), aColor, sal::static_int_cast<sal_uInt16>(eActionType) );
                }
            }
        }

        pAction = pAction->GetNext();
    }
}

void ScOutputData::DrawSparklines(vcl::RenderContext& rRenderContext)
{
    tools::Long nInitPosX = mnScrX;
    if ( mbLayoutRTL )
        nInitPosX += mnMirrorW - 1;              // always in pixels
    tools::Long nLayoutSign = mbLayoutRTL ? -1 : 1;

    tools::Long nPosY = mnScrY;
    for (SCSIZE nArrY=1; nArrY+1<mnArrCount; nArrY++)
    {
        RowInfo* pThisRowInfo = &mpRowInfo[nArrY];
        if ( pThisRowInfo->bChanged )
        {
            tools::Long nPosX = nInitPosX;
            for (SCCOL nX=mnX1; nX<=mnX2; nX++)
            {
                ScCellInfo* pInfo = &pThisRowInfo->cellInfo(nX);
                bool bIsMerged = false;

                SCCOL nOverX = nX;
                SCROW nOverY = pThisRowInfo->nRowNo;
                tools::Long nStartPosX = nPosX;
                tools::Long nStartPosY = nPosY;

                ScAddress aCurrentAddress(nX, mpRowInfo[nArrY].nRowNo, mnTab);
                std::shared_ptr<sc::Sparkline> pSparkline = mpDoc->GetSparkline(aCurrentAddress);

                if (pInfo->bHOverlapped || pInfo->bVOverlapped)
                {
                    while (nOverX > 0 && (mpDoc->GetAttr(
                           nOverX, nOverY, mnTab, ATTR_MERGE_FLAG)->GetValue() & ScMF::Hor))
                    {
                        --nOverX;
                        nStartPosX -= nLayoutSign
                                      * static_cast<tools::Long>(mpDoc->GetColWidth(nOverX, mnTab)
                                                                 * mnPPTX);
                    }

                    while (nOverY > 0 && (mpDoc->GetAttr(
                           nOverX, nOverY, mnTab, ATTR_MERGE_FLAG)->GetValue() & ScMF::Ver))
                    {
                        --nOverY;
                        nStartPosY -= nLayoutSign
                                      * static_cast<tools::Long>(mpDoc->GetRowHeight(nOverY, mnTab)
                                                                 * mnPPTY);
                    }

                    pSparkline = mpDoc->GetSparkline(ScAddress(nOverX, nOverY, mnTab));
                    bIsMerged = pSparkline ? true : false;
                }

                if (!mpDoc->ColHidden(nX, mnTab) && pSparkline
                    && (bIsMerged || (!pInfo->bHOverlapped && !pInfo->bVOverlapped)))
                {
                    tools::Long nWidth = mpRowInfo[0].basicCellInfo(nX).nWidth;
                    tools::Long nHeight = pThisRowInfo->nHeight;

                    if (bIsMerged || pInfo->bMerged)
                    {
                        const ScMergeAttr* pMerge = mpDoc->GetAttr(nOverX, nOverY, mnTab, ATTR_MERGE);
                        SCROW nCountX = pMerge->GetColMerge();
                        if (nCountX > 0)
                        {
                            sal_Int32 nIndex = 1;
                            while (nCountX > nIndex && (mpDoc->GetAttr(
                                   nOverX + nIndex, nOverY, mnTab, ATTR_MERGE_FLAG)->GetValue() & ScMF::Hor))
                            {
                                nWidth += nLayoutSign
                                          * static_cast<tools::Long>(
                                              mpDoc->GetColWidth(nOverX + nIndex, mnTab) * mnPPTX);
                                nIndex++;
                            }
                        }

                        SCROW nCountY = pMerge->GetRowMerge();
                        if (nCountY > 0)
                        {
                            sal_Int32 nIndex = 1;
                            while (nCountY > nIndex && (mpDoc->GetAttr(
                                   nOverX, nOverY + nIndex, mnTab, ATTR_MERGE_FLAG)->GetValue() & ScMF::Ver))
                            {
                                nHeight += nLayoutSign
                                           * static_cast<tools::Long>(
                                               mpDoc->GetRowHeight(nOverY + nIndex, mnTab) * mnPPTY);
                                nIndex++;
                            }
                        }
                    }

                    Point aPoint(nStartPosX, nStartPosY);
                    Size aSize(nWidth, nHeight);

                    sc::SparklineRenderer renderer(*mpDoc);
                    renderer.render(pSparkline, rRenderContext, tools::Rectangle(aPoint, aSize), 1, 1, double(maZoomX), double(maZoomY));
                }

                nPosX += mpRowInfo[0].basicCellInfo(nX).nWidth * nLayoutSign;
            }
        }
        nPosY += pThisRowInfo->nHeight;
    }

}

//TODO: moggi Need to check if this can't be written simpler
void ScOutputData::DrawNoteMarks(vcl::RenderContext& rRenderContext)
{
    // cool#6911 draw the note indicator browser-side instead
    if (comphelper::LibreOfficeKit::isActive())
        return;

    tools::Long nInitPosX = mnScrX;
    if ( mbLayoutRTL )
        nInitPosX += mnMirrorW - 1;              // always in pixels
    tools::Long nLayoutSign = mbLayoutRTL ? -1 : 1;

    tools::Long nPosY = mnScrY - 1;
    for (SCSIZE nArrY=1; nArrY+1<mnArrCount; nArrY++)
    {
        RowInfo* pThisRowInfo = &mpRowInfo[nArrY];
        if ( pThisRowInfo->bChanged )
        {
            tools::Long nPosX = nInitPosX;
            for (SCCOL nX=mnX1; nX<=mnX2; nX++)
            {
                ScCellInfo* pInfo = &pThisRowInfo->cellInfo(nX);
                bool bIsMerged = false;

                if ( nX==mnX1 && pInfo->bHOverlapped && !pInfo->bVOverlapped )
                {
                    // find start of merged cell
                    bIsMerged = true;
                    SCROW nY = mpRowInfo[nArrY].nRowNo;
                    SCCOL nMergeX = nX;
                    SCROW nMergeY = nY;
                    mpDoc->ExtendOverlapped( nMergeX, nMergeY, nX, nY, mnTab );
                }

                if (!mpDoc->ColHidden(nX, mnTab) && mpDoc->GetNote(nX, mpRowInfo[nArrY].nRowNo, mnTab)
                    && (bIsMerged || (!pInfo->bHOverlapped && !pInfo->bVOverlapped)))
                {
                    ScModule* mod = ScModule::get();
                    rRenderContext.SetLineColor(mod->GetColorConfig().GetColorValue(svtools::CALCGRID).nColor);

                    const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();
                    if ( mbUseStyleColor && rStyleSettings.GetHighContrastMode() )
                        rRenderContext.SetFillColor( mod->GetColorConfig().GetColorValue(svtools::FONTCOLOR).nColor );
                    else
                        rRenderContext.SetFillColor( mod->GetColorConfig().GetColorValue(svtools::CALCCOMMENTS).nColor );

                    tools::Long nMarkX = nPosX + ( mpRowInfo[0].basicCellInfo(nX).nWidth - 1) * nLayoutSign;
                    if ( bIsMerged || pInfo->bMerged )
                    {
                        //  if merged, add widths of all cells
                        SCCOL nNextX = nX + 1;
                        while ( nNextX <= mnX2 + 1 && pThisRowInfo->cellInfo(nNextX).bHOverlapped )
                        {
                            nMarkX += mpRowInfo[0].basicCellInfo(nNextX).nWidth * nLayoutSign;
                            ++nNextX;
                        }
                    }
                    // DPI/ZOOM 100/100 => 6, 100/50 => 4.5, 100/150 => 7.5
                    // DPI/ZOOM 150/100 => 7.5, 150/50 => 6, 150/150 => 9
                    sal_Int16 nSize = officecfg::Office::Calc::Content::Display::NoteIndicator::get();
                    if (nSize < 1)
                    {
                       const double fSize(rRenderContext.GetDPIScaleFactor() * maZoomX * 3 + 3);
                       nSize = static_cast<sal_Int16>(fSize);
                    }
                    Point aPoints[3];
                    aPoints[0] = Point(nMarkX, nPosY);
                    aPoints[0].setX( mbLayoutRTL ? aPoints[0].X() + nSize : aPoints[0].X() - nSize );
                    aPoints[1] = Point(nMarkX, nPosY);
                    aPoints[2] = Point(nMarkX, nPosY + nSize);
                    tools::Polygon aPoly(3, aPoints);

                    if ( mbLayoutRTL ? ( nMarkX >= 0 ) : ( nMarkX < mnScrX+mnScrW ) )
                        rRenderContext.DrawPolygon(aPoly);
                }

                nPosX += mpRowInfo[0].basicCellInfo(nX).nWidth * nLayoutSign;
            }
        }
        nPosY += pThisRowInfo->nHeight;
    }
}

void ScOutputData::DrawFormulaMarks(vcl::RenderContext& rRenderContext)
{
    bool bFirst = true;

    tools::Long nInitPosX = mnScrX;
    if ( mbLayoutRTL )
        nInitPosX += mnMirrorW - 1;              // always in pixels
    tools::Long nLayoutSign = mbLayoutRTL ? -1 : 1;

    tools::Long nPosY = mnScrY;
    for (SCSIZE nArrY=1; nArrY+1<mnArrCount; nArrY++)
    {
        RowInfo* pThisRowInfo = &mpRowInfo[nArrY];
        if ( pThisRowInfo->bChanged )
        {
            tools::Long nPosX = nInitPosX;
            for (SCCOL nX=mnX1; nX<=mnX2; nX++)
            {
                ScCellInfo* pInfo = &pThisRowInfo->cellInfo(nX);
                if (!mpDoc->ColHidden(nX, mnTab) && !mpDoc->GetFormula(nX, mpRowInfo[nArrY].nRowNo, mnTab).isEmpty()
                    && (!pInfo->bHOverlapped && !pInfo->bVOverlapped))
                {
                    if (bFirst)
                    {
                        rRenderContext.SetLineColor(COL_WHITE);

                        const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();
                        if ( mbUseStyleColor && rStyleSettings.GetHighContrastMode() )
                            rRenderContext.SetFillColor( ScModule::get()->GetColorConfig().GetColorValue(svtools::FONTCOLOR).nColor );
                        else
                            rRenderContext.SetFillColor(COL_LIGHTBLUE);

                        bFirst = false;
                    }

                    tools::Long nMarkX = nPosX;
                    tools::Long nMarkY = nPosY + pThisRowInfo->nHeight - 2;
                    if ( pInfo->bMerged )
                    {
                        for (SCSIZE nNextY=nArrY+1; nNextY+1<mnArrCount; nNextY++)
                        {
                            bool bVOver;
                            if (mpRowInfo[nNextY + 1].nRowNo == (mpRowInfo[nNextY].nRowNo + 1)) {
                                bVOver = mpRowInfo[nNextY].cellInfo(nX).bVOverlapped;
                            } else {
                                bVOver = mpDoc->GetAttr(nX,nNextY,mnTab,ATTR_MERGE_FLAG)->IsVerOverlapped();
                            }
                            if (!bVOver) break;
                            nMarkY += mpRowInfo[nNextY].nHeight;
                        }
                    }
                    // DPI/ZOOM 100/100 => 10, 100/50 => 7, 100/150 => 13
                    // DPI/ZOOM 150/100 => 13, 150/50 => 8.5, 150/150 => 17.5
                    const double nSize( rRenderContext.GetDPIScaleFactor() * maZoomX * 6 + 4);
                    Point aPoints[3];
                    aPoints[0] = Point(nMarkX, nMarkY);
                    aPoints[0].setX( mbLayoutRTL ? aPoints[0].X() - nSize : aPoints[0].X() + nSize );
                    aPoints[1] = Point(nMarkX, nMarkY);
                    aPoints[2] = Point(nMarkX, nMarkY - nSize);
                    tools::Polygon aPoly(3, aPoints);

                    if ( mbLayoutRTL ? ( nMarkX >= 0 ) : ( nMarkX < mnScrX+mnScrW ) )
                        rRenderContext.DrawPolygon(aPoly);
                }

                nPosX += mpRowInfo[0].basicCellInfo(nX).nWidth * nLayoutSign;
            }
        }
        nPosY += pThisRowInfo->nHeight;
    }
}

void ScOutputData::AddPDFNotes()
{
    vcl::PDFExtOutDevData* pPDFData = dynamic_cast< vcl::PDFExtOutDevData* >( mpDev->GetExtOutDevData() );
    if ( !pPDFData || !pPDFData->GetIsExportNotes() )
        return;

    tools::Long nInitPosX = mnScrX;
    if ( mbLayoutRTL )
    {
        Size aOnePixel = mpDev->PixelToLogic(Size(1,1));
        tools::Long nOneX = aOnePixel.Width();
        nInitPosX += mnMirrorW - nOneX;
    }
    tools::Long nLayoutSign = mbLayoutRTL ? -1 : 1;

    tools::Long nPosY = mnScrY;
    for (SCSIZE nArrY=1; nArrY+1<mnArrCount; nArrY++)
    {
        RowInfo* pThisRowInfo = &mpRowInfo[nArrY];
        if ( pThisRowInfo->bChanged )
        {
            tools::Long nPosX = nInitPosX;
            for (SCCOL nX=mnX1; nX<=mnX2; nX++)
            {
                ScCellInfo* pInfo = &pThisRowInfo->cellInfo(nX);
                bool bIsMerged = false;
                SCROW nY = mpRowInfo[nArrY].nRowNo;
                SCCOL nMergeX = nX;
                SCROW nMergeY = nY;

                if ( nX==mnX1 && pInfo->bHOverlapped && !pInfo->bVOverlapped )
                {
                    // find start of merged cell
                    bIsMerged = true;
                    mpDoc->ExtendOverlapped( nMergeX, nMergeY, nX, nY, mnTab );
                    // use origin's pCell for NotePtr test below
                }

                ScPostIt* pNote = mpDoc->GetNote(nMergeX, nMergeY, mnTab);

                if ( pNote && ( bIsMerged || ( !pInfo->bHOverlapped && !pInfo->bVOverlapped ) ) )
                {
                    tools::Long nNoteWidth = static_cast<tools::Long>( SC_CLIPMARK_SIZE * mnPPTX );
                    tools::Long nNoteHeight = static_cast<tools::Long>( SC_CLIPMARK_SIZE * mnPPTY );

                    tools::Long nMarkX = nPosX + ( mpRowInfo[0].basicCellInfo(nX).nWidth - nNoteWidth ) * nLayoutSign;
                    if ( bIsMerged || pInfo->bMerged )
                    {
                        //  if merged, add widths of all cells
                        SCCOL nNextX = nX + 1;
                        while ( nNextX <= mnX2 + 1 && pThisRowInfo->cellInfo(nNextX).bHOverlapped )
                        {
                            nMarkX += mpRowInfo[0].basicCellInfo(nNextX).nWidth * nLayoutSign;
                            ++nNextX;
                        }
                    }
                    if ( mbLayoutRTL ? ( nMarkX >= 0 ) : ( nMarkX < mnScrX+mnScrW ) )
                    {
                        tools::Rectangle aNoteRect( nMarkX, nPosY, nMarkX+nNoteWidth*nLayoutSign, nPosY+nNoteHeight );

                        vcl::pdf::PDFNote aNote;

                        // Note title is the cell address (as on printed note pages)
                        ScAddress aAddress( nMergeX, nMergeY, mnTab );
                        aNote.maTitle = aAddress.Format(ScRefFlags::VALID, mpDoc, mpDoc->GetAddressConvention());

                        // Content has to be a simple string without line breaks
                        OUString aContent = pNote->GetText();
                        aNote.maContents = aContent.replaceAll("\n", " ");

                        // If the caption is hidden, we need to show it to get its rectangle,
                        // then hide it again because it is also hidden in the file.
                        bool bShowCaption = pNote->IsCaptionShown();
                        if (!bShowCaption)
                            pNote->ShowCaption(aAddress, true);

                        SdrCaptionObj* pCaption = pNote->GetCaption();
                        tools::Rectangle aCaptionRect(pCaption->GetLogicRect());
                        Point aPoint(aCaptionRect.getX() + mnScrX, aCaptionRect.getY() + mnScrY);
                        Size aSize(aCaptionRect.GetWidth(), aCaptionRect.GetHeight());
                        tools::Rectangle aPopupRect(aPoint, aSize);

                        if (!bShowCaption)
                            pNote->ShowCaption(aAddress, false);

                        pPDFData->CreateNote(aNoteRect, aNote, aPopupRect);
                    }
                }

                nPosX += mpRowInfo[0].basicCellInfo(nX).nWidth * nLayoutSign;
            }
        }
        nPosY += pThisRowInfo->nHeight;
    }
}

void ScOutputData::DrawClipMarks()
{
    if (!mbAnyClipped)
        return;

    ScModule* mod = ScModule::get();
    Color aArrowFillCol(mod->GetColorConfig().GetColorValue(svtools::CALCTEXTOVERFLOW).nColor);
    const bool bIsDarkBackground = mod->GetColorConfig().GetColorValue(svtools::DOCCOLOR).nColor.IsDark();

    DrawModeFlags nOldDrawMode = mpDev->GetDrawMode();

    tools::Long nInitPosX = mnScrX;
    if ( mbLayoutRTL )
        nInitPosX += mnMirrorW - 1;              // always in pixels
    tools::Long nLayoutSign = mbLayoutRTL ? -1 : 1;

    tools::Rectangle aCellRect;
    tools::Long nPosY = mnScrY;
    for (SCSIZE nArrY=1; nArrY+1<mnArrCount; nArrY++)
    {
        RowInfo* pThisRowInfo = &mpRowInfo[nArrY];
        if ( pThisRowInfo->bChanged )
        {
            SCROW nY = pThisRowInfo->nRowNo;
            tools::Long nPosX = nInitPosX;
            for (SCCOL nX=mnX1; nX<=mnX2; nX++)
            {
                ScCellInfo* pInfo = &pThisRowInfo->cellInfo(nX);
                if (pInfo->nClipMark != ScClipMark::NONE)
                {
                    if (pInfo->bHOverlapped || pInfo->bVOverlapped)
                    {
                        //  merge origin may be outside of visible area - use document functions

                        SCCOL nOverX = nX;
                        SCROW nOverY = nY;
                        tools::Long nStartPosX = nPosX;
                        tools::Long nStartPosY = nPosY;

                        while ( nOverX > 0 && ( mpDoc->GetAttr(
                                nOverX, nOverY, mnTab, ATTR_MERGE_FLAG )->GetValue() & ScMF::Hor ) )
                        {
                            --nOverX;
                            nStartPosX -= nLayoutSign * static_cast<tools::Long>( mpDoc->GetColWidth(nOverX,mnTab) * mnPPTX );
                        }

                        while ( nOverY > 0 && ( mpDoc->GetAttr(
                                nOverX, nOverY, mnTab, ATTR_MERGE_FLAG )->GetValue() & ScMF::Ver ) )
                        {
                            --nOverY;
                            nStartPosY -= nLayoutSign * static_cast<tools::Long>( mpDoc->GetRowHeight(nOverY,mnTab) * mnPPTY );
                        }

                        tools::Long nOutWidth = static_cast<tools::Long>( mpDoc->GetColWidth(nOverX,mnTab) * mnPPTX );
                        tools::Long nOutHeight = static_cast<tools::Long>( mpDoc->GetRowHeight(nOverY,mnTab) * mnPPTY );

                        const ScMergeAttr* pMerge = mpDoc->GetAttr( nOverX, nOverY, mnTab, ATTR_MERGE );
                        SCCOL nCountX = pMerge->GetColMerge();
                        for (SCCOL i=1; i<nCountX; i++)
                            nOutWidth += mpDoc->GetColWidth(nOverX+i,mnTab) * mnPPTX;
                        SCROW nCountY = pMerge->GetRowMerge();
                        nOutHeight += mpDoc->GetScaledRowHeight( nOverY+1, nOverY+nCountY-1, mnTab, mnPPTY);

                        if ( mbLayoutRTL )
                            nStartPosX -= nOutWidth - 1;
                        aCellRect = tools::Rectangle( Point( nStartPosX, nStartPosY ), Size( nOutWidth, nOutHeight ) );
                    }
                    else
                    {
                        tools::Long nOutWidth = mpRowInfo[0].basicCellInfo(nX).nWidth;
                        tools::Long nOutHeight = pThisRowInfo->nHeight;

                        if ( pInfo->bMerged && pInfo->pPatternAttr )
                        {
                            SCCOL nOverX = nX;
                            SCROW nOverY = nY;
                            const ScMergeAttr* pMerge =
                                    &pInfo->pPatternAttr->GetItem(ATTR_MERGE);
                            SCCOL nCountX = pMerge->GetColMerge();
                            for (SCCOL i=1; i<nCountX; i++)
                                nOutWidth += mpDoc->GetColWidth(nOverX+i,mnTab) * mnPPTX;
                            SCROW nCountY = pMerge->GetRowMerge();
                            nOutHeight += mpDoc->GetScaledRowHeight( nOverY+1, nOverY+nCountY-1, mnTab, mnPPTY);
                        }

                        tools::Long nStartPosX = nPosX;
                        if ( mbLayoutRTL )
                            nStartPosX -= nOutWidth - 1;
                        // #i80447# create aCellRect from two points in case nOutWidth is 0
                        aCellRect = tools::Rectangle( Point( nStartPosX, nPosY ),
                                               Point( nStartPosX+nOutWidth-1, nPosY+nOutHeight-1 ) );
                    }

                    aCellRect.AdjustBottom( -1 );    // don't paint over the cell grid
                    if ( mbLayoutRTL )
                        aCellRect.AdjustLeft(1 );
                    else
                        aCellRect.AdjustRight( -1 );

                    tools::Long nMarkPixel = static_cast<tools::Long>( SC_CLIPMARK_SIZE * mnPPTX );
                    Size aMarkSize( nMarkPixel, (nMarkPixel-1)*2 );

                    const Color aColor = pInfo->maBackground ?
                        static_cast<const SvxBrushItem*>(pInfo->maBackground.getItem())->GetColor() :
                        COL_AUTO;
                    if ( aColor == COL_AUTO ? bIsDarkBackground : aColor.IsDark() )
                        mpDev->SetDrawMode( nOldDrawMode | DrawModeFlags::WhiteLine );
                    else
                        mpDev->SetDrawMode( nOldDrawMode | DrawModeFlags::BlackLine );

                    if (mbVertical)
                    {
                        if (pInfo->nClipMark & (mbLayoutRTL ? ScClipMark::Bottom : ScClipMark::Top))
                        {
                            //  visually top
                            tools::Rectangle aMarkRect = aCellRect;
                            aMarkRect.SetBottom(aCellRect.Top() + nMarkPixel - 1);
                            SvxFont::DrawArrow(*mpDev, aMarkRect, aMarkSize, aArrowFillCol, true, true);
                        }
                        if (pInfo->nClipMark & (mbLayoutRTL ? ScClipMark::Top : ScClipMark::Bottom))
                        {
                            //  visually bottom
                            tools::Rectangle aMarkRect = aCellRect;
                            aMarkRect.SetTop(aCellRect.Bottom() + nMarkPixel + 1);
                            SvxFont::DrawArrow(*mpDev, aMarkRect, aMarkSize, aArrowFillCol, false,
                                true);
                        }
                    }
                    else
                    {
                        if (pInfo->nClipMark & (mbLayoutRTL ? ScClipMark::Right : ScClipMark::Left))
                        {
                            //  visually left
                            tools::Rectangle aMarkRect = aCellRect;
                            aMarkRect.SetRight(aCellRect.Left() + nMarkPixel - 1);
                            SvxFont::DrawArrow(*mpDev, aMarkRect, aMarkSize, aArrowFillCol, true,
                                false);
                        }
                        if (pInfo->nClipMark & (mbLayoutRTL ? ScClipMark::Left : ScClipMark::Right))
                        {
                            //  visually right
                            tools::Rectangle aMarkRect = aCellRect;
                            aMarkRect.SetLeft(aCellRect.Right() - nMarkPixel + 1);
                            SvxFont::DrawArrow(*mpDev, aMarkRect, aMarkSize, aArrowFillCol, false,
                                false);
                        }
                    }
                }
                nPosX += mpRowInfo[0].basicCellInfo(nX).nWidth * nLayoutSign;
            }
        }
        nPosY += pThisRowInfo->nHeight;
    }

    mpDev->SetDrawMode(nOldDrawMode);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
