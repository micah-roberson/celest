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

#include <com/sun/star/linguistic2/XHyphenator.hpp>

#include <unotools/linguprops.hxx>
#include <unotools/lingucfg.hxx>
#include <fmtinfmt.hxx>
#include <hintids.hxx>
#include <txatbase.hxx>
#include <svl/ctloptions.hxx>
#include <sfx2/infobar.hxx>
#include <sfx2/printer.hxx>
#include <sfx2/StylePreviewRenderer.hxx>
#include <sal/log.hxx>
#include <editeng/hyphenzoneitem.hxx>
#include <editeng/hngpnctitem.hxx>
#include <editeng/scriptspaceitem.hxx>
#include <editeng/splwrap.hxx>
#include <editeng/pgrditem.hxx>
#include <editeng/tstpitem.hxx>
#include <editeng/shaditem.hxx>

#include <SwSmartTagMgr.hxx>
#include <breakit.hxx>
#include <editeng/forbiddenruleitem.hxx>
#include <swmodule.hxx>
#include <vcl/svapp.hxx>
#include <viewsh.hxx>
#include <viewopt.hxx>
#include <frmtool.hxx>
#include <fmturl.hxx>
#include <fmteiro.hxx>
#include <IDocumentSettingAccess.hxx>
#include <IDocumentDeviceAccess.hxx>
#include <IDocumentMarkAccess.hxx>
#include <paratr.hxx>
#include <sectfrm.hxx>
#include <rootfrm.hxx>
#include "inftxt.hxx"
#include <noteurl.hxx>
#include "porfly.hxx"
#include "porftn.hxx"
#include "porrst.hxx"
#include "itratr.hxx"
#include "portab.hxx"
#include <wrong.hxx>
#include <doc.hxx>
#include <pam.hxx>
#include <numrule.hxx>
#include <EnhancedPDFExportHelper.hxx>
#include <docsh.hxx>
#include <strings.hrc>
#include <o3tl/deleter.hxx>
#include <vcl/gdimtf.hxx>
#include <vcl/virdev.hxx>
#include <vcl/gradient.hxx>
#include <i18nlangtag/mslangid.hxx>
#include <formatlinebreak.hxx>

#include <view.hxx>
#include <wrtsh.hxx>
#include <com/sun/star/text/XTextRange.hpp>
#include <unotextrange.hxx>
#include <SwStyleNameMapper.hxx>
#include <unoprnms.hxx>
#include <editeng/unoprnms.hxx>
#include <unomap.hxx>
#include <names.hxx>
#include <com/sun/star/awt/FontSlant.hpp>

using namespace ::com::sun::star;
using namespace ::com::sun::star::linguistic2;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::beans;

#define CHAR_LEFT_ARROW u'\x25C0'
#define CHAR_RIGHT_ARROW u'\x25B6'
#define CHAR_TAB u'\x2192'
#define CHAR_TAB_RTL u'\x2190'
#define CHAR_LINEBREAK u'\x21B5'
#define CHAR_LINEBREAK_RTL u'\x21B3'

#define DRAW_SPECIAL_OPTIONS_CENTER 1
#define DRAW_SPECIAL_OPTIONS_ROTATE 2

SwLineInfo::SwLineInfo()
    : m_pSpace( nullptr ),
      m_nVertAlign( SvxParaVertAlignItem::Align::Automatic ),
      m_nDefTabStop( 0 ),
      m_bListTabStopIncluded( false ),
      m_nListTabStopPosition( 0 )
{
}

SwLineInfo::~SwLineInfo()
{
}

void SwLineInfo::CtorInitLineInfo( const SwAttrSet& rAttrSet,
                                   const SwTextNode& rTextNode )
{
    m_oRuler.emplace( rAttrSet.GetTabStops() );
    if ( rTextNode.GetListTabStopPosition( m_nListTabStopPosition ) )
    {
        m_bListTabStopIncluded = true;

        // insert the list tab stop into SvxTabItem instance <pRuler>
        const SvxTabStop aListTabStop( m_nListTabStopPosition,
                                       SvxTabAdjust::Left );
        m_oRuler->Insert( aListTabStop );

        // remove default tab stops, which are before the inserted list tab stop
        for ( sal_uInt16 i = 0; i < m_oRuler->Count(); i++ )
        {
            if ( (*m_oRuler)[i].GetTabPos() < m_nListTabStopPosition &&
                 (*m_oRuler)[i].GetAdjustment() == SvxTabAdjust::Default )
            {
                m_oRuler->Remove(i);
                continue;
            }
        }
    }

    if ( !rTextNode.getIDocumentSettingAccess()->get(DocumentSettingId::TABS_RELATIVE_TO_INDENT) )
    {
        // remove default tab stop at position 0
        for ( sal_uInt16 i = 0; i < m_oRuler->Count(); i++ )
        {
            if ( (*m_oRuler)[i].GetTabPos() == 0 &&
                 (*m_oRuler)[i].GetAdjustment() == SvxTabAdjust::Default )
            {
                m_oRuler->Remove(i);
                break;
            }
        }
    }

    m_pSpace = &rAttrSet.GetLineSpacing();
    m_nVertAlign = rAttrSet.GetParaVertAlign().GetValue();
    m_nDefTabStop = std::numeric_limits<SwTwips>::max();
}

void SwTextInfo::CtorInitTextInfo( SwTextFrame *pFrame )
{
    m_pPara = pFrame->GetPara();
    m_nTextStart = pFrame->GetOffset();
    if (!m_pPara)
    {
        SAL_WARN("sw.core", "+SwTextInfo::CTOR: missing paragraph information");
        pFrame->Format(pFrame->getRootFrame()->GetCurrShell()->GetOut());
        m_pPara = pFrame->GetPara();
    }
}

SwTextInfo::SwTextInfo( const SwTextInfo &rInf )
    : m_pPara( const_cast<SwTextInfo&>(rInf).GetParaPortion() )
    , m_nTextStart( rInf.GetTextStart() )
{ }

#if OSL_DEBUG_LEVEL > 0

static void ChkOutDev( const SwTextSizeInfo &rInf )
{
    if ( !rInf.GetVsh() )
        return;

    const OutputDevice* pOut = rInf.GetOut();
    const OutputDevice* pRef = rInf.GetRefDev();
    OSL_ENSURE( pOut && pRef, "ChkOutDev: invalid output devices" );
}
#endif

static TextFrameIndex GetMinLen( const SwTextSizeInfo &rInf )
{
    const TextFrameIndex nTextLen(rInf.GetText().getLength());
    if (rInf.GetLen() == TextFrameIndex(COMPLETE_STRING))
        return nTextLen;
    const TextFrameIndex nInfLen = rInf.GetIdx() + rInf.GetLen();
    return std::min(nTextLen, nInfLen);
}

SwTextSizeInfo::SwTextSizeInfo()
: m_pKanaComp(nullptr)
, m_pVsh(nullptr)
, m_pOut(nullptr)
, m_pRef(nullptr)
, m_pFnt(nullptr)
, m_pUnderFnt(nullptr)
, m_pFrame(nullptr)
, m_pOpt(nullptr)
, m_pText(nullptr)
, m_nIdx(0)
, m_nLen(0)
, m_nMeasureLen(COMPLETE_STRING)
, m_nKanaIdx(0)
, m_bOnWin    (false)
, m_bNotEOL   (false)
, m_bURLNotify(false)
, m_bStopUnderflow(false)
, m_bFootnoteInside(false)
, m_bOtherThanFootnoteInside(false)
, m_bMulti(false)
, m_bFirstMulti(false)
, m_bRuby(false)
, m_bHanging(false)
, m_bScriptSpace(false)
, m_bForbiddenChars(false)
, m_bSnapToGrid(false)
, m_nDirection(0)
, m_nExtraSpace(0)
, m_nBreakWidth(0)
{}

SwTextSizeInfo::SwTextSizeInfo( const SwTextSizeInfo &rNew )
    : SwTextInfo( rNew ),
      m_pKanaComp(rNew.GetpKanaComp()),
      m_pVsh(const_cast<SwTextSizeInfo&>(rNew).GetVsh()),
      m_pOut(const_cast<SwTextSizeInfo&>(rNew).GetOut()),
      m_pRef(const_cast<SwTextSizeInfo&>(rNew).GetRefDev()),
      m_pFnt(const_cast<SwTextSizeInfo&>(rNew).GetFont()),
      m_pUnderFnt(rNew.GetUnderFnt()),
      m_pFrame(rNew.m_pFrame),
      m_pOpt(&rNew.GetOpt()),
      m_pText(&rNew.GetText()),
      m_nIdx(rNew.GetIdx()),
      m_nLen(rNew.GetLen()),
      m_nMeasureLen(rNew.GetMeasureLen()),
      m_nKanaIdx( rNew.GetKanaIdx() ),
      m_bOnWin( rNew.OnWin() ),
      m_bNotEOL( rNew.NotEOL() ),
      m_bURLNotify( rNew.URLNotify() ),
      m_bStopUnderflow( rNew.StopUnderflow() ),
      m_bFootnoteInside( rNew.IsFootnoteInside() ),
      m_bOtherThanFootnoteInside( rNew.IsOtherThanFootnoteInside() ),
      m_bMulti( rNew.IsMulti() ),
      m_bFirstMulti( rNew.IsFirstMulti() ),
      m_bRuby( rNew.IsRuby() ),
      m_bHanging( rNew.IsHanging() ),
      m_bScriptSpace( rNew.HasScriptSpace() ),
      m_bForbiddenChars( rNew.HasForbiddenChars() ),
      m_bSnapToGrid( rNew.SnapToGrid() ),
      m_nDirection( rNew.GetDirection() ),
      m_nExtraSpace( rNew.GetExtraSpace() ),
      m_nBreakWidth( rNew.GetBreakWidth() )
{
#if OSL_DEBUG_LEVEL > 0
    ChkOutDev( *this );
#endif
}

void SwTextSizeInfo::CtorInitTextSizeInfo( OutputDevice* pRenderContext, SwTextFrame *pFrame,
           TextFrameIndex const nNewIdx)
{
    m_pKanaComp = nullptr;
    m_nKanaIdx = 0;
    m_nExtraSpace = 0;
    m_nBreakWidth = 0;
    m_pFrame = pFrame;
    CtorInitTextInfo( m_pFrame );
    SwDoc const& rDoc(m_pFrame->GetDoc());
    m_pVsh = m_pFrame->getRootFrame()->GetCurrShell();

    // Get the output and reference device
    if ( m_pVsh )
    {
        m_pOut = pRenderContext;
        m_pRef = &m_pVsh->GetRefDev();
        m_bOnWin = m_pVsh->GetWin() || OUTDEV_WINDOW == m_pOut->GetOutDevType() || m_pVsh->isOutputToWindow();
    }
    else
    {
        // Access via StarONE. We do not need a Shell or an active one.
        if (rDoc.getIDocumentSettingAccess().get(DocumentSettingId::HTML_MODE))
        {
            // We can only pick the AppWin here? (there's nothing better to pick?)
            m_pOut = Application::GetDefaultDevice();
        }
        else
            m_pOut = rDoc.getIDocumentDeviceAccess().getPrinter(false);

        m_pRef = m_pOut;
    }

#if OSL_DEBUG_LEVEL > 0
    ChkOutDev( *this );
#endif

    // Set default layout mode ( LTR or RTL ).
    if ( m_pFrame->IsRightToLeft() )
    {
        m_pOut->SetLayoutMode( vcl::text::ComplexTextLayoutFlags::BiDiStrong | vcl::text::ComplexTextLayoutFlags::BiDiRtl );
        m_pRef->SetLayoutMode( vcl::text::ComplexTextLayoutFlags::BiDiStrong | vcl::text::ComplexTextLayoutFlags::BiDiRtl );
        m_nDirection = DIR_RIGHT2LEFT;
    }
    else
    {
        m_pOut->SetLayoutMode( vcl::text::ComplexTextLayoutFlags::BiDiStrong );
        m_pRef->SetLayoutMode( vcl::text::ComplexTextLayoutFlags::BiDiStrong );
        m_nDirection = DIR_LEFT2RIGHT;
    }

    // The Options

    m_pOpt = m_pVsh ?
           m_pVsh->GetViewOptions() :
           SwModule::get()->GetViewOption(rDoc.getIDocumentSettingAccess().get(DocumentSettingId::HTML_MODE)); // Options from Module, due to StarONE

    // bURLNotify is set if MakeGraphic prepares it
    // TODO: Unwind
    m_bURLNotify = pNoteURL && !m_bOnWin;

    SetSnapToGrid( m_pFrame->GetTextNodeForParaProps()->GetSwAttrSet().GetParaGrid().GetValue() &&
                   m_pFrame->IsInDocBody() );

    m_pFnt = nullptr;
    m_pUnderFnt = nullptr;
    m_pText = &m_pFrame->GetText();

    m_nIdx = nNewIdx;
    m_nLen = m_nMeasureLen = TextFrameIndex(COMPLETE_STRING);
    m_bNotEOL = false;
    m_bStopUnderflow = m_bFootnoteInside = m_bOtherThanFootnoteInside = false;
    m_bMulti = m_bFirstMulti = m_bRuby = m_bHanging = m_bScriptSpace =
        m_bForbiddenChars = false;

    SetLen( GetMinLen( *this ) );
}

SwTextSizeInfo::SwTextSizeInfo( const SwTextSizeInfo &rNew, const OUString* pText,
              TextFrameIndex const nIndex)
    : SwTextInfo( rNew ),
      m_pKanaComp(rNew.GetpKanaComp()),
      m_pVsh(const_cast<SwTextSizeInfo&>(rNew).GetVsh()),
      m_pOut(const_cast<SwTextSizeInfo&>(rNew).GetOut()),
      m_pRef(const_cast<SwTextSizeInfo&>(rNew).GetRefDev()),
      m_pFnt(const_cast<SwTextSizeInfo&>(rNew).GetFont()),
      m_pUnderFnt(rNew.GetUnderFnt()),
      m_pFrame( rNew.m_pFrame ),
      m_pOpt(&rNew.GetOpt()),
      m_pText(pText),
      m_nIdx(nIndex),
      m_nLen(COMPLETE_STRING),
      m_nMeasureLen(COMPLETE_STRING),
      m_nKanaIdx( rNew.GetKanaIdx() ),
      m_bOnWin( rNew.OnWin() ),
      m_bNotEOL( rNew.NotEOL() ),
      m_bURLNotify( rNew.URLNotify() ),
      m_bStopUnderflow( rNew.StopUnderflow() ),
      m_bFootnoteInside( rNew.IsFootnoteInside() ),
      m_bOtherThanFootnoteInside( rNew.IsOtherThanFootnoteInside() ),
      m_bMulti( rNew.IsMulti() ),
      m_bFirstMulti( rNew.IsFirstMulti() ),
      m_bRuby( rNew.IsRuby() ),
      m_bHanging( rNew.IsHanging() ),
      m_bScriptSpace( rNew.HasScriptSpace() ),
      m_bForbiddenChars( rNew.HasForbiddenChars() ),
      m_bSnapToGrid( rNew.SnapToGrid() ),
      m_nDirection( rNew.GetDirection() ),
      m_nExtraSpace( rNew.GetExtraSpace() ),
      m_nBreakWidth( rNew.GetBreakWidth() )
{
#if OSL_DEBUG_LEVEL > 0
    ChkOutDev( *this );
#endif
    SetLen( GetMinLen( *this ) );
}

SwTextSizeInfo::SwTextSizeInfo(SwTextFrame *const pTextFrame,
            TextFrameIndex const nIndex)
    : m_bOnWin(false)
{
    CtorInitTextSizeInfo( pTextFrame->getRootFrame()->GetCurrShell()->GetOut(), pTextFrame, nIndex );
}

void SwTextSizeInfo::SelectFont()
{
     // The path needs to go via ChgPhysFnt or the FontMetricCache gets confused.
     // In this case pLastMet has it's old value.
     // Wrong: GetOut()->SetFont( GetFont()->GetFnt() );
    GetFont()->Invalidate();
    GetFont()->ChgPhysFnt( m_pVsh, *GetOut() );
}

void SwTextSizeInfo::NoteAnimation() const
{
    if( OnWin() )
        SwRootFrame::FlushVout();

    OSL_ENSURE( m_pOut == m_pVsh->GetOut(),
            "SwTextSizeInfo::NoteAnimation() changed m_pOut" );
}

SwPositiveSize SwTextSizeInfo::GetTextSize( OutputDevice* pOutDev,
                                     const SwScriptInfo* pSI,
                                     const OUString& rText,
                                     const TextFrameIndex nIndex,
                                     const TextFrameIndex nLength) const
{
    SwDrawTextInfo aDrawInf(m_pVsh, *pOutDev, pSI, rText, nIndex, nLength,
                            /*layout context*/ std::nullopt);
    aDrawInf.SetFrame( m_pFrame );
    aDrawInf.SetFont( m_pFnt );
    aDrawInf.SetSnapToGrid( SnapToGrid() );
    aDrawInf.SetKanaComp( 0 );
    return SwPositiveSize(m_pFnt->GetTextSize_( aDrawInf ));
}

SwPositiveSize
SwTextSizeInfo::GetTextSize(std::optional<SwLinePortionLayoutContext> nLayoutContext) const
{
    const SwScriptInfo& rSI =
                     const_cast<SwParaPortion*>(GetParaPortion())->GetScriptInfo();

    // in some cases, compression is not allowed or suppressed for
    // performance reasons
    sal_uInt16 nComp =( SwFontScript::CJK == GetFont()->GetActual() &&
                    rSI.CountCompChg() &&
                    ! IsMulti() ) ?
                    GetKanaComp() :
                                0 ;

    SwDrawTextInfo aDrawInf(m_pVsh, *m_pOut, &rSI, *m_pText, m_nIdx, m_nLen, nLayoutContext);
    aDrawInf.SetMeasureLen( m_nMeasureLen );
    aDrawInf.SetFrame( m_pFrame );
    aDrawInf.SetFont( m_pFnt );
    aDrawInf.SetSnapToGrid( SnapToGrid() );
    aDrawInf.SetKanaComp( nComp );
    return SwPositiveSize(m_pFnt->GetTextSize_( aDrawInf ));
}

void SwTextSizeInfo::GetTextSize(const SwScriptInfo* pSI, const TextFrameIndex nIndex,
                                 const TextFrameIndex nLength,
                                 std::optional<SwLinePortionLayoutContext> nLayoutContext,
                                 const sal_uInt16 nComp, SwTwips& nMinSize,
                                 tools::Long& nMaxSizeDiff, SwTwips& nExtraAscent,
                                 SwTwips& nExtraDescent,
                                 vcl::text::TextLayoutCache const* const pCache) const
{
    SwDrawTextInfo aDrawInf(m_pVsh, *m_pOut, pSI, *m_pText, nIndex, nLength, nLayoutContext, 0,
                            false, pCache);
    aDrawInf.SetFrame( m_pFrame );
    aDrawInf.SetFont( m_pFnt );
    aDrawInf.SetSnapToGrid( SnapToGrid() );
    aDrawInf.SetKanaComp( nComp );
    SwPositiveSize aSize( m_pFnt->GetTextSize_( aDrawInf ) );
    nMaxSizeDiff = aDrawInf.GetKanaDiff();
    nExtraAscent = aDrawInf.GetExtraAscent();
    nExtraDescent = aDrawInf.GetExtraDescent();
    nMinSize = aSize.Width();
}

TextFrameIndex SwTextSizeInfo::GetTextBreak( const tools::Long nLineWidth,
                                       const TextFrameIndex nMaxLen,
                                       const sal_uInt16 nComp,
                                       vcl::text::TextLayoutCache const*const pCache) const
{
    const SwScriptInfo& rScriptInfo =
                     const_cast<SwParaPortion*>(GetParaPortion())->GetScriptInfo();

    OSL_ENSURE( m_pRef == m_pOut, "GetTextBreak is supposed to use the RefDev" );
    SwDrawTextInfo aDrawInf(m_pVsh, *m_pOut, &rScriptInfo, *m_pText, GetIdx(), nMaxLen,
                            /*layout context*/ std::nullopt, 0, false, pCache);
    aDrawInf.SetFrame( m_pFrame );
    aDrawInf.SetFont( m_pFnt );
    aDrawInf.SetSnapToGrid( SnapToGrid() );
    aDrawInf.SetKanaComp( nComp );
    aDrawInf.SetHyphPos( nullptr );

    return m_pFnt->GetTextBreak( aDrawInf, nLineWidth );
}

TextFrameIndex SwTextSizeInfo::GetTextBreak( const tools::Long nLineWidth,
                                       const TextFrameIndex nMaxLen,
                                       const sal_uInt16 nComp,
                                       TextFrameIndex& rExtraCharPos,
                                       vcl::text::TextLayoutCache const*const pCache) const
{
    const SwScriptInfo& rScriptInfo =
                     const_cast<SwParaPortion*>(GetParaPortion())->GetScriptInfo();

    OSL_ENSURE( m_pRef == m_pOut, "GetTextBreak is supposed to use the RefDev" );
    SwDrawTextInfo aDrawInf(m_pVsh, *m_pOut, &rScriptInfo, *m_pText, GetIdx(), nMaxLen,
                            /*layout context*/ std::nullopt, 0, false, pCache);
    aDrawInf.SetFrame( m_pFrame );
    aDrawInf.SetFont( m_pFnt );
    aDrawInf.SetSnapToGrid( SnapToGrid() );
    aDrawInf.SetKanaComp( nComp );
    aDrawInf.SetHyphPos( &rExtraCharPos );

    return m_pFnt->GetTextBreak( aDrawInf, nLineWidth );
}

bool SwTextSizeInfo::HasHint(TextFrameIndex const nPos) const
{
    std::pair<SwTextNode const*, sal_Int32> const pos(m_pFrame->MapViewToModel(nPos));
    return pos.first->GetTextAttrForCharAt(pos.second);
}

void SwTextPaintInfo::CtorInitTextPaintInfo( OutputDevice* pRenderContext, SwTextFrame *pFrame, const SwRect &rPaint )
{
    CtorInitTextSizeInfo( pRenderContext, pFrame, TextFrameIndex(0) );
    m_aTextFly.CtorInitTextFly( pFrame );
    m_aPaintRect = rPaint;
    m_nSpaceIdx = 0;
    m_pSpaceAdd = nullptr;
    m_pWrongList = nullptr;
    m_pGrammarCheckList = nullptr;
    m_pSmartTags = nullptr;
    m_pBrushItem = nullptr;
}

SwTextPaintInfo::SwTextPaintInfo( const SwTextPaintInfo &rInf, const OUString* pText )
    : SwTextSizeInfo( rInf, pText )
    , m_pWrongList( rInf.GetpWrongList() )
    , m_pGrammarCheckList( rInf.GetGrammarCheckList() )
    , m_pSmartTags( rInf.GetSmartTags() )
    , m_pSpaceAdd( rInf.GetpSpaceAdd() ),
      m_pBrushItem( rInf.GetBrushItem() ),
      m_aTextFly( rInf.GetTextFly() ),
      m_aPos( rInf.GetPos() ),
      m_aPaintRect( rInf.GetPaintRect() ),
      m_nSpaceIdx( rInf.GetSpaceIdx() )
{ }

SwTextPaintInfo::SwTextPaintInfo( const SwTextPaintInfo &rInf )
    : SwTextSizeInfo( rInf )
    , m_pWrongList( rInf.GetpWrongList() )
    , m_pGrammarCheckList( rInf.GetGrammarCheckList() )
    , m_pSmartTags( rInf.GetSmartTags() )
    , m_pSpaceAdd( rInf.GetpSpaceAdd() ),
      m_pBrushItem( rInf.GetBrushItem() ),
      m_aTextFly( rInf.GetTextFly() ),
      m_aPos( rInf.GetPos() ),
      m_aPaintRect( rInf.GetPaintRect() ),
      m_nSpaceIdx( rInf.GetSpaceIdx() )
{ }

SwTextPaintInfo::SwTextPaintInfo( SwTextFrame *pFrame, const SwRect &rPaint )
{
    CtorInitTextPaintInfo( pFrame->getRootFrame()->GetCurrShell()->GetOut(), pFrame, rPaint );
}

namespace
{
/**
 * Context class that captures the draw operations on rDrawInf's output device for transparency
 * purposes.
 */
class SwTransparentTextGuard
{
    ScopedVclPtrInstance<VirtualDevice> m_aContentVDev { DeviceFormat::WITH_ALPHA };
    GDIMetaFile m_aContentMetafile;
    MapMode m_aNewMapMode;
    SwRect m_aPorRect;
    SwTextPaintInfo& m_rPaintInf;
    SwDrawTextInfo& m_rDrawInf;

public:
    SwTransparentTextGuard(const SwLinePortion& rPor, SwTextPaintInfo& rPaintInf,
                           SwDrawTextInfo& rDrawInf);
    ~SwTransparentTextGuard();
};

SwTransparentTextGuard::SwTransparentTextGuard(const SwLinePortion& rPor,
                                               SwTextPaintInfo& rPaintInf, SwDrawTextInfo& rDrawInf)
    : m_aNewMapMode(rPaintInf.GetOut()->GetMapMode())
    , m_rPaintInf(rPaintInf)
    , m_rDrawInf(rDrawInf)
{
    rPaintInf.CalcRect(rPor, &m_aPorRect);
    rDrawInf.SetOut(*m_aContentVDev);
    m_aContentVDev->SetMapMode(rPaintInf.GetOut()->GetMapMode());
    m_aContentMetafile.Record(m_aContentVDev.get());
    m_aContentVDev->SetLineColor(rPaintInf.GetOut()->GetLineColor());
    m_aContentVDev->SetFillColor(rPaintInf.GetOut()->GetFillColor());
    m_aContentVDev->SetFont(rPaintInf.GetOut()->GetFont());
    m_aContentVDev->SetDrawMode(rPaintInf.GetOut()->GetDrawMode());
    m_aContentVDev->SetSettings(rPaintInf.GetOut()->GetSettings());
    m_aContentVDev->SetRefPoint(rPaintInf.GetOut()->GetRefPoint());
}

SwTransparentTextGuard::~SwTransparentTextGuard()
{
    m_aContentMetafile.Stop();
    m_aContentMetafile.WindStart();
    m_aNewMapMode.SetOrigin(m_aPorRect.TopLeft());
    m_aContentMetafile.SetPrefMapMode(m_aNewMapMode);
    m_aContentMetafile.SetPrefSize(m_aPorRect.SSize());
    m_rDrawInf.SetOut(*m_rPaintInf.GetOut());
    Gradient aVCLGradient;
    sal_uInt8 nTransPercentVcl = 255 - m_rPaintInf.GetFont()->GetColor().GetAlpha();
    const Color aTransColor(nTransPercentVcl, nTransPercentVcl, nTransPercentVcl);
    aVCLGradient.SetStyle(css::awt::GradientStyle_LINEAR);
    aVCLGradient.SetStartColor(aTransColor);
    aVCLGradient.SetEndColor(aTransColor);
    aVCLGradient.SetAngle(0_deg10);
    aVCLGradient.SetBorder(0);
    aVCLGradient.SetOfsX(0);
    aVCLGradient.SetOfsY(0);
    aVCLGradient.SetStartIntensity(100);
    aVCLGradient.SetEndIntensity(100);
    aVCLGradient.SetSteps(2);
    m_rPaintInf.GetOut()->DrawTransparent(m_aContentMetafile, m_aPorRect.TopLeft(),
                                          m_aPorRect.SSize(), aVCLGradient);
}
}

static bool lcl_IsFrameReadonly(SwTextFrame* pFrame)
{
    const SwFlyFrame* pFly;
    const SwSection* pSection;

    if( pFrame && pFrame->IsInFly())
    {
        pFly = pFrame->FindFlyFrame();
        if (pFly->GetFormat()->GetEditInReadonly().GetValue())
        {
            const SwFrame* pLower = pFly->Lower();
            if (pLower && !pLower->IsNoTextFrame())
            {
                return false;
            }
        }
    }
    // edit in readonly sections
    else if ( pFrame && pFrame->IsInSct() &&
        nullptr != ( pSection = pFrame->FindSctFrame()->GetSection() ) &&
        pSection->IsEditInReadonlyFlag() )
    {
        return false;
    }

    return true;
}


void SwTextPaintInfo::DrawText_( const OUString &rText, const SwLinePortion &rPor,
                                TextFrameIndex const nStart, TextFrameIndex const nLength,
                                const bool bKern, const bool bWrong,
                                const bool bSmartTag,
                                const bool bGrammarCheck )
{
    if( !nLength )
        return;

    // The SwScriptInfo is useless if we are inside a field portion
    SwScriptInfo* pSI = nullptr;
    if ( ! rPor.InFieldGrp() )
        pSI = &GetParaPortion()->GetScriptInfo();

    // in some cases, kana compression is not allowed or suppressed for
    // performance reasons
    sal_uInt16 nComp = 0;
    if ( ! IsMulti() )
        nComp = GetKanaComp();

    bool bCfgIsAutoGrammar = false;
    SvtLinguConfig().GetProperty( UPN_IS_GRAMMAR_AUTO ) >>= bCfgIsAutoGrammar;
    const bool bBullet = OnWin() && GetOpt().IsBlank() && IsNoSymbol();
    bool bTmpWrong = bWrong && OnWin() && GetOpt().IsOnlineSpell();
    SfxObjectShell* pObjShell = m_pFrame->GetDoc().GetDocShell();
    if (bTmpWrong && pObjShell)
    {
        if (pObjShell->IsReadOnly() && lcl_IsFrameReadonly(m_pFrame))
            bTmpWrong = false;
    }

    const bool bTmpGrammarCheck = bGrammarCheck && OnWin() && bCfgIsAutoGrammar && GetOpt().IsOnlineSpell();
    const bool bTmpSmart = bSmartTag && OnWin() && !GetOpt().IsPagePreview() && SwSmartTagMgr::Get().IsSmartTagsEnabled();

    OSL_ENSURE( GetParaPortion(), "No paragraph!");
    SwDrawTextInfo aDrawInf(m_pFrame->getRootFrame()->GetCurrShell(), *m_pOut, pSI, rText, nStart,
                            nLength, rPor.GetLayoutContext(), rPor.Width(), bBullet);

    aDrawInf.SetUnderFnt( m_pUnderFnt );

    const tools::Long nSpaceAdd = ( rPor.IsBlankPortion() || rPor.IsDropPortion() ||
                             rPor.InNumberGrp() ) ? 0 : GetSpaceAdd(/*bShrink=*/true);
    if ( nSpaceAdd )
    {
        TextFrameIndex nCharCnt(0);
        // #i41860# Thai justified alignment needs some
        // additional information:
        aDrawInf.SetNumberOfBlanks( rPor.InTextGrp() ?
                                    static_cast<const SwTextPortion&>(rPor).GetSpaceCnt( *this, nCharCnt ) :
                                    TextFrameIndex(0) );
    }

    aDrawInf.SetSpace( nSpaceAdd );
    aDrawInf.SetKanaComp( nComp );

    // the font is used to identify the current script via nActual
    aDrawInf.SetFont( m_pFnt );
    // the frame is used to identify the orientation
    aDrawInf.SetFrame( GetTextFrame() );
    // we have to know if the paragraph should snap to grid
    aDrawInf.SetSnapToGrid( SnapToGrid() );
    // for underlining we must know when not to add extra space behind
    // a character in justified mode
    aDrawInf.SetSpaceStop( ! rPor.GetNextPortion() ||
                             rPor.GetNextPortion()->InFixMargGrp() ||
                             rPor.GetNextPortion()->IsHolePortion() );

    // Draw text next to the left border
    Point aFontPos(m_aPos);
    if( m_pFnt->GetLeftBorder() && rPor.InTextGrp() && !static_cast<const SwTextPortion&>(rPor).GetJoinBorderWithPrev() )
    {
        const sal_uInt16 nLeftBorderSpace = m_pFnt->GetLeftBorderSpace();
        if ( GetTextFrame()->IsRightToLeft() )
        {
            aFontPos.AdjustX( -nLeftBorderSpace );
        }
        else
        {
            switch( m_pFnt->GetOrientation(GetTextFrame()->IsVertical()).get() )
            {
                case 0 :
                    aFontPos.AdjustX(nLeftBorderSpace );
                    break;
                case 900 :
                    aFontPos.AdjustY( -nLeftBorderSpace );
                    break;
                case 1800 :
                    aFontPos.AdjustX( -nLeftBorderSpace );
                    break;
                case 2700 :
                    aFontPos.AdjustY(nLeftBorderSpace );
                    break;
            }
        }
        if( aFontPos.X() < 0 )
            aFontPos.setX( 0 );
        if( aFontPos.Y() < 0 )
            aFontPos.setY( 0 );
    }

    // Handle semi-transparent text if necessary.
    std::unique_ptr<SwTransparentTextGuard, o3tl::default_delete<SwTransparentTextGuard>> pTransparentText;
    if (m_pFnt->GetColor() != COL_AUTO && m_pFnt->GetColor().IsTransparent())
    {
        // if drawing to a backend that supports transparency for text color, then we don't need to use this
        if (!m_bOnWin || !m_pOut->SupportsOperation(OutDevSupportType::TransparentText) || m_pOut->GetConnectMetaFile())
            pTransparentText.reset(new SwTransparentTextGuard(rPor, *this, aDrawInf));
    }

    if( GetTextFly().IsOn() )
    {
        // aPos needs to be the TopLeft, because we cannot calculate the
        // ClipRects otherwise
        const Point aPoint( aFontPos.X(), aFontPos.Y() - rPor.GetAscent() );
        const Size aSize( rPor.Width(), rPor.Height() );
        aDrawInf.SetPos( aPoint );
        aDrawInf.SetSize( aSize );
        aDrawInf.SetAscent( rPor.GetAscent() );
        aDrawInf.SetKern( bKern ? rPor.Width() : 0 );
        aDrawInf.SetWrong( bTmpWrong ? m_pWrongList : nullptr );
        aDrawInf.SetGrammarCheck( bTmpGrammarCheck ? m_pGrammarCheckList : nullptr );
        aDrawInf.SetSmartTags( bTmpSmart ? m_pSmartTags : nullptr );
        GetTextFly().DrawTextOpaque( aDrawInf );
    }
    else
    {
        aDrawInf.SetPos( aFontPos );
        if( bKern )
            m_pFnt->DrawStretchText_( aDrawInf );
        else
        {
            aDrawInf.SetWrong( bTmpWrong ? m_pWrongList : nullptr );
            aDrawInf.SetGrammarCheck( bTmpGrammarCheck ? m_pGrammarCheckList : nullptr );
            aDrawInf.SetSmartTags( bTmpSmart ? m_pSmartTags : nullptr );
            m_pFnt->DrawText_( aDrawInf );
        }
    }
}

void SwTextPaintInfo::CalcRect( const SwLinePortion& rPor,
                               SwRect* pRect, SwRect* pIntersect,
                               const bool bInsideBox ) const
{
    const SwAttrSet& rAttrSet = GetTextFrame()->GetTextNodeForParaProps()->GetSwAttrSet();
    const SvxLineSpacingItem& rSpace = rAttrSet.GetLineSpacing();
    tools::Long nPropLineSpace = rSpace.GetPropLineSpace();

    SwTwips nHeight = rPor.Height();

    // we should take line spacing into account.
    // otherwise, bottom of some letters will be cut because of the "field shading" background layer.
    switch (rSpace.GetInterLineSpaceRule())
    {
        case SvxInterLineSpaceRule::Prop: // proportional
        {
            if (nPropLineSpace < 100)
                nHeight = rPor.Height() * nPropLineSpace / 100;
        }
        break;
        case SvxInterLineSpaceRule::Fix: // fixed
        {
            if (rSpace.GetInterLineSpace() > 0)
                nHeight = std::min<SwTwips>(rSpace.GetInterLineSpace(), rPor.Height());
        }
        break;
        default:
            break;
    }

    Size aSize( rPor.Width(), nHeight);

    if( rPor.IsHangingPortion() )
        aSize.setWidth( static_cast<const SwHangingPortion&>(rPor).GetInnerWidth() );
    if( rPor.InSpaceGrp() && GetSpaceAdd() )
    {
        SwTwips nAdd = rPor.CalcSpacing( GetSpaceAdd(), *this );
        if( rPor.InFieldGrp() && GetSpaceAdd() < 0 && nAdd )
            nAdd += GetSpaceAdd() / SPACING_PRECISION_FACTOR;
        aSize.AdjustWidth(nAdd );
    }

    Point aPoint;

    if( IsRotated() )
    {
        tools::Long nTmp = aSize.Width();
        aSize.setWidth( aSize.Height() );
        aSize.setHeight( nTmp );
        if ( 1 == GetDirection() )
        {
            aPoint.setX( X() - rPor.GetAscent() );
            aPoint.setY( Y() - aSize.Height() );
        }
        else
        {
            aPoint.setX( X() - rPor.Height() + rPor.GetAscent() );
            aPoint.setY( Y() );
        }
    }
    else
    {
        aPoint.setX( X() );
        if (GetTextFrame()->IsVertLR() && !GetTextFrame()->IsVertLRBT())
            aPoint.setY( Y() - rPor.Height() + rPor.GetAscent() );
        else
        {
            SwTwips nAscent = rPor.GetAscent();

            switch (rSpace.GetInterLineSpaceRule())
            {
                case SvxInterLineSpaceRule::Prop: // proportional
                {
                    if (nPropLineSpace < 100)
                        nAscent = (rPor.GetAscent() * nPropLineSpace / 100);
                }
                break;
                case SvxInterLineSpaceRule::Fix: // fixed
                {
                    if (rSpace.GetInterLineSpace() > 0)
                        nAscent = std::min<SwTwips>(rSpace.GetInterLineSpace(), rPor.GetAscent());
                }
                break;
                default:
                    break;
            }

            aPoint.setY( Y() - nAscent);
        }
    }

    // Adjust x coordinate if we are inside a bidi portion
    const bool bFrameDir = GetTextFrame()->IsRightToLeft();
    const bool bCounterDir = ( !bFrameDir && DIR_RIGHT2LEFT == GetDirection() ) ||
                             (  bFrameDir && DIR_LEFT2RIGHT == GetDirection() );

    if ( bCounterDir )
        aPoint.AdjustX( -(aSize.Width()) );

    SwRect aRect( aPoint, aSize );

    if ( GetTextFrame()->IsRightToLeft() )
        GetTextFrame()->SwitchLTRtoRTL( aRect );

    if ( GetTextFrame()->IsVertical() )
        GetTextFrame()->SwitchHorizontalToVertical( aRect );

    if( bInsideBox && rPor.InTextGrp() )
    {
        const bool bJoinWithPrev =
            static_cast<const SwTextPortion&>(rPor).GetJoinBorderWithPrev();
        const bool bJoinWithNext =
            static_cast<const SwTextPortion&>(rPor).GetJoinBorderWithNext();
        const bool bIsVert = GetTextFrame()->IsVertical();
        const bool bIsVertLRBT = GetTextFrame()->IsVertLRBT();
        aRect.AddTop( GetFont()->CalcShadowSpace(SvxShadowItemSide::TOP, bIsVert, bIsVertLRBT,
                                               bJoinWithPrev, bJoinWithNext));
        aRect.AddBottom( - GetFont()->CalcShadowSpace(SvxShadowItemSide::BOTTOM, bIsVert, bIsVertLRBT,
                                                  bJoinWithPrev, bJoinWithNext));
        aRect.AddLeft( GetFont()->CalcShadowSpace(SvxShadowItemSide::LEFT, bIsVert, bIsVertLRBT,
                                                bJoinWithPrev, bJoinWithNext));
        aRect.AddRight( - GetFont()->CalcShadowSpace(SvxShadowItemSide::RIGHT, bIsVert, bIsVertLRBT,
                                                 bJoinWithPrev, bJoinWithNext));
    }

    if ( pRect )
        *pRect = aRect;

    if( aRect.HasArea() && pIntersect )
    {
        ::SwAlignRect( aRect, GetVsh(), GetOut() );

        if ( GetOut()->IsClipRegion() )
        {
            SwRect aClip( GetOut()->GetClipRegion().GetBoundRect() );
            aRect.Intersection( aClip );
        }

        *pIntersect = aRect;
    }
}

/**
 * Draws a special portion
 * E.g.: line break portion, tab portion
 *
 * @param rPor The portion
 * @param rRect The rectangle surrounding the character
 * @param rCol Specify a color for the character
 * @param bCenter Draw the character centered, otherwise left aligned
 * @param bRotate Rotate the character if character rotation is set
 */
static void lcl_DrawSpecial( const SwTextPaintInfo& rTextPaintInfo, const SwLinePortion& rPor,
                      SwRect& rRect, const Color& rCol, sal_Unicode cChar,
                      sal_uInt8 nOptions )
{
    bool bCenter = 0 != ( nOptions & DRAW_SPECIAL_OPTIONS_CENTER );
    bool bRotate = 0 != ( nOptions & DRAW_SPECIAL_OPTIONS_ROTATE );

    // rRect is given in absolute coordinates
    if ( rTextPaintInfo.GetTextFrame()->IsRightToLeft() )
        rTextPaintInfo.GetTextFrame()->SwitchRTLtoLTR( rRect );
    if ( rTextPaintInfo.GetTextFrame()->IsVertical() )
        rTextPaintInfo.GetTextFrame()->SwitchVerticalToHorizontal( rRect );

    const SwFont* pOldFnt = rTextPaintInfo.GetFont();

    // Font is generated only once:
    static SwFont s_aFnt = [&]()
    {
        SwFont tmp( *pOldFnt );
        tmp.SetFamily( FAMILY_DONTKNOW, tmp.GetActual() );
        tmp.SetName( numfunc::GetDefBulletFontname(), tmp.GetActual() );
        tmp.SetStyleName(OUString(), tmp.GetActual());
        tmp.SetCharSet( RTL_TEXTENCODING_SYMBOL, tmp.GetActual() );
        return tmp;
    }();

    // Some of the current values are set at the font:
    if ( ! bRotate )
        s_aFnt.SetVertical( 0_deg10, rTextPaintInfo.GetTextFrame()->IsVertical() );
    else
        s_aFnt.SetVertical( pOldFnt->GetOrientation() );

    s_aFnt.SetColor(rCol);

    Size aFontSize( 0, SPECIAL_FONT_HEIGHT );
    s_aFnt.SetSize( aFontSize, s_aFnt.GetActual() );

    SwTextPaintInfo& rNonConstTextPaintInfo = const_cast<SwTextPaintInfo&>(rTextPaintInfo);

    rNonConstTextPaintInfo.SetFont( &s_aFnt );

    // The maximum width depends on the current orientation
    const Degree10 nDir = s_aFnt.GetOrientation( rTextPaintInfo.GetTextFrame()->IsVertical() );
    SwTwips nMaxWidth;
    if (nDir == 900_deg10 || nDir == 2700_deg10)
        nMaxWidth = rRect.Height();
    else
    {
        assert(nDir == 0_deg10); //Unknown direction set at font
        nMaxWidth = rRect.Width();
    }

    // check if char fits into rectangle
    const OUString aTmp( cChar );
    aFontSize = rTextPaintInfo.GetTextSize( aTmp ).SvLSize();
    while ( aFontSize.Width() > nMaxWidth )
    {
        SwTwips nFactor = ( 100 * aFontSize.Width() ) / nMaxWidth;
        const SwTwips nOldWidth = aFontSize.Width();

        // new height for font
        const SwFontScript nAct = s_aFnt.GetActual();
        aFontSize.setHeight( ( 100 * s_aFnt.GetSize( nAct ).Height() ) / nFactor );
        aFontSize.setWidth( ( 100 * s_aFnt.GetSize( nAct).Width() ) / nFactor );

        if ( !aFontSize.Width() && !aFontSize.Height() )
            break;

        s_aFnt.SetSize( aFontSize, nAct );

        aFontSize = rTextPaintInfo.GetTextSize( aTmp ).SvLSize();

        if ( aFontSize.Width() >= nOldWidth )
            break;
    }

    const Point aOldPos( rTextPaintInfo.GetPos() );

    // adjust values so that tab is vertically and horizontally centered
    SwTwips nX = rRect.Left();
    SwTwips nY = rRect.Top();
    switch ( nDir.get() )
    {
    case 0 :
        if ( bCenter )
            nX += ( rRect.Width() - aFontSize.Width() ) / 2;
        nY += ( rRect.Height() - aFontSize.Height() ) / 2 + rTextPaintInfo.GetAscent();
        break;
    case 900 :
        if ( bCenter )
            nX += ( rRect.Width() - aFontSize.Height() ) / 2 + rTextPaintInfo.GetAscent();
        nY += ( rRect.Height() + aFontSize.Width() ) / 2;
        break;
    case 2700 :
        if ( bCenter )
            nX += ( rRect.Width() + aFontSize.Height() ) / 2 - rTextPaintInfo.GetAscent();
        nY += ( rRect.Height() - aFontSize.Width() ) / 2;
        break;
    }

    Point aTmpPos( nX, nY );
    rNonConstTextPaintInfo.SetPos( aTmpPos );
    SwTwips nOldWidth = rPor.Width();
    const_cast<SwLinePortion&>(rPor).Width(aFontSize.Width());
    rTextPaintInfo.DrawText( aTmp, rPor );
    const_cast<SwLinePortion&>(rPor).Width( nOldWidth );
    rNonConstTextPaintInfo.SetFont( const_cast<SwFont*>(pOldFnt) );
    rNonConstTextPaintInfo.SetPos( aOldPos );
}

void SwTextPaintInfo::DrawRect( const SwRect &rRect, bool bRetouche ) const
{
    if ( OnWin() || !bRetouche )
    {
        if( m_aTextFly.IsOn() )
            const_cast<SwTextPaintInfo*>(this)->GetTextFly().
                DrawFlyRect( m_pOut, rRect );
        else
            m_pOut->DrawRect( rRect.SVRect() );
    }
}

void SwTextPaintInfo::DrawTab( const SwLinePortion &rPor ) const
{
    if( !OnWin() )
        return;

    SwRect aRect;
    CalcRect( rPor, &aRect );

    if ( ! aRect.HasArea() )
        return;

    const sal_Unicode cChar = GetTextFrame()->IsRightToLeft() ? CHAR_TAB_RTL : CHAR_TAB;
    const sal_uInt8 nOptions = DRAW_SPECIAL_OPTIONS_CENTER | DRAW_SPECIAL_OPTIONS_ROTATE;

    lcl_DrawSpecial( *this, rPor, aRect, SwViewOption::GetCurrentViewOptions().GetNonPrintingCharacterColor(), cChar, nOptions );
}

void SwTextPaintInfo::DrawLineBreak( const SwLinePortion &rPor ) const
{
    if( !OnWin() )
        return;

    SwLineBreakClear eClear = SwLineBreakClear::NONE;
    if (rPor.IsBreakPortion())
    {
        const auto& rBreakPortion = static_cast<const SwBreakPortion&>(rPor);
        eClear = rBreakPortion.GetClear();
    }

    SwTwips nOldWidth = rPor.Width();
    const_cast<SwLinePortion&>(rPor).Width( LINE_BREAK_WIDTH );

    SwRect aRect;
    CalcRect( rPor, &aRect );

    if( aRect.HasArea() )
    {
        const sal_Unicode cChar = GetTextFrame()->IsRightToLeft() ?
                                  CHAR_LINEBREAK_RTL : CHAR_LINEBREAK;
        const sal_uInt8 nOptions = 0;

        SwRect aTextRect(aRect);
        if (eClear == SwLineBreakClear::LEFT || eClear == SwLineBreakClear::ALL)
            aTextRect.AddLeft(30);
        if (eClear == SwLineBreakClear::RIGHT || eClear == SwLineBreakClear::ALL)
            aTextRect.AddRight(-30);
        lcl_DrawSpecial( *this, rPor, aTextRect, SwViewOption::GetCurrentViewOptions().GetNonPrintingCharacterColor(), cChar, nOptions );

        if (eClear != SwLineBreakClear::NONE)
        {
            // Paint indicator if this clear is left/right/all.
            m_pOut->Push(vcl::PushFlags::LINECOLOR);
            m_pOut->SetLineColor(SwViewOption::GetCurrentViewOptions().GetNonPrintingCharacterColor());
            if (eClear != SwLineBreakClear::RIGHT)
                m_pOut->DrawLine(aRect.BottomLeft(), aRect.TopLeft());
            if (eClear != SwLineBreakClear::LEFT)
                m_pOut->DrawLine(aRect.BottomRight(), aRect.TopRight());
            m_pOut->Pop();
        }
    }

    const_cast<SwLinePortion&>(rPor).Width( nOldWidth );
}

void SwTextPaintInfo::DrawRedArrow( const SwLinePortion &rPor ) const
{
    Size aSize( SPECIAL_FONT_HEIGHT, SPECIAL_FONT_HEIGHT );
    SwRect aRect( static_cast<const SwArrowPortion&>(rPor).GetPos(), aSize );
    sal_Unicode cChar;
    if( static_cast<const SwArrowPortion&>(rPor).IsLeft() )
    {
        aRect.Pos().AdjustY(20 - GetAscent() );
        aRect.Pos().AdjustX(20 );
        if( aSize.Height() > rPor.Height() )
            aRect.Height( rPor.Height() );
        cChar = CHAR_LEFT_ARROW;
    }
    else
    {
        if( aSize.Height() > rPor.Height() )
            aRect.Height( rPor.Height() );
        aRect.Pos().AdjustY( -(aRect.Height() + 20) );
        aRect.Pos().AdjustX( -(aRect.Width() + 20) );
        cChar = CHAR_RIGHT_ARROW;
    }

    if ( GetTextFrame()->IsVertical() )
        GetTextFrame()->SwitchHorizontalToVertical( aRect );

    if( aRect.HasArea() )
    {
        const sal_uInt8 nOptions = 0;
        lcl_DrawSpecial( *this, rPor, aRect, COL_LIGHTRED, cChar, nOptions );
    }
}

void SwTextPaintInfo::DrawPostIts( bool bScript ) const
{
    if( !OnWin() || !m_pOpt->IsPostIts() )
        return;

    Size aSize;
    Point aTmp;

    const SwTwips nPostItsWidth = SwViewOption::GetPostItsWidth(GetOut());
    const sal_uInt16 nFontHeight = m_pFnt->GetHeight( m_pVsh, *GetOut() );
    const sal_uInt16 nFontAscent = m_pFnt->GetAscent( m_pVsh, *GetOut() );

    switch ( m_pFnt->GetOrientation( GetTextFrame()->IsVertical() ).get() )
    {
    case 0 :
        aSize.setWidth( nPostItsWidth );
        aSize.setHeight( nFontHeight );
        aTmp.setX( m_aPos.X() );
        aTmp.setY( m_aPos.Y() - nFontAscent );
        break;
    case 900 :
        aSize.setHeight( nPostItsWidth );
        aSize.setWidth( nFontHeight );
        aTmp.setX( m_aPos.X() - nFontAscent );
        aTmp.setY( m_aPos.Y() );
        break;
    case 2700 :
        aSize.setHeight( nPostItsWidth );
        aSize.setWidth( nFontHeight );
        aTmp.setX( m_aPos.X() - nFontHeight +
                              nFontAscent );
        aTmp.setY( m_aPos.Y() );
        break;
    }

    SwRect aTmpRect( aTmp, aSize );

    if ( GetTextFrame()->IsRightToLeft() )
        GetTextFrame()->SwitchLTRtoRTL( aTmpRect );

    if ( GetTextFrame()->IsVertical() )
        GetTextFrame()->SwitchHorizontalToVertical( aTmpRect );

    GetOpt().PaintPostIts( const_cast<OutputDevice*>(GetOut()), aTmpRect, bScript );

}

void SwTextPaintInfo::DrawCheckBox(const SwFieldFormCheckboxPortion &rPor, bool bChecked) const
{
    SwRect aIntersect;
    CalcRect( rPor, &aIntersect );
    if ( !aIntersect.HasArea() )
        return;

    if (OnWin() && GetOpt().IsFieldShadings() &&
            !GetOpt().IsPagePreview())
    {
        OutputDevice* pOut = const_cast<OutputDevice*>(GetOut());
        pOut->Push( vcl::PushFlags::LINECOLOR | vcl::PushFlags::FILLCOLOR );
        if( m_pFnt->GetHighlightColor() != COL_TRANSPARENT )
            pOut->SetFillColor(m_pFnt->GetHighlightColor());
        else
            pOut->SetFillColor(GetOpt().GetFieldShadingsColor());
        pOut->SetLineColor();
        pOut->DrawRect( aIntersect.SVRect() );
        pOut->Pop();
    }
    const int delta = 25;
    tools::Rectangle r(aIntersect.Left()+delta, aIntersect.Top()+delta, aIntersect.Right()-delta, aIntersect.Bottom()-delta);
    m_pOut->Push( vcl::PushFlags::LINECOLOR | vcl::PushFlags::FILLCOLOR );
    m_pOut->SetLineColor( Color(0, 0, 0));
    m_pOut->SetFillColor();
    m_pOut->DrawRect( r );
    if (bChecked)
    {
        m_pOut->DrawLine(r.TopLeft(), r.BottomRight());
        m_pOut->DrawLine(r.TopRight(), r.BottomLeft());
    }
    m_pOut->Pop();
}

void SwTextPaintInfo::DrawBackground( const SwLinePortion &rPor, const Color *pColor ) const
{
    OSL_ENSURE( OnWin(), "SwTextPaintInfo::DrawBackground: printer pollution ?" );

    SwRect aIntersect;
    CalcRect( rPor, nullptr, &aIntersect, true );

    if ( !aIntersect.HasArea() )
        return;

    OutputDevice* pOut = const_cast<OutputDevice*>(GetOut());
    pOut->Push( vcl::PushFlags::LINECOLOR | vcl::PushFlags::FILLCOLOR );

    if ( pColor )
        pOut->SetFillColor( *pColor );
    else
        pOut->SetFillColor( GetOpt().GetFieldShadingsColor() );

    pOut->SetLineColor();

    DrawRect( aIntersect, true );
    pOut->Pop();
}

void SwTextPaintInfo::DrawBackBrush( const SwLinePortion &rPor ) const
{
    {
        SwRect aIntersect;
        CalcRect( rPor, &aIntersect, nullptr, true );
        if(aIntersect.HasArea())
        {
            SwPosition const aPosition(m_pFrame->MapViewToModelPos(GetIdx()));
            const ::sw::mark::MarkBase* pFieldmark =
                m_pFrame->GetDoc().getIDocumentMarkAccess()->getInnerFieldmarkFor(aPosition);
            bool bIsStartMark = (TextFrameIndex(1) == GetLen()
                    && CH_TXT_ATR_FIELDSTART == GetText()[sal_Int32(GetIdx())]);
            if(pFieldmark) {
                SAL_INFO("sw.core", "Found Fieldmark " << pFieldmark->ToString());
            }
            if(bIsStartMark)
                SAL_INFO("sw.core", "Found StartMark");
            if (OnWin() && (pFieldmark!=nullptr || bIsStartMark) &&
                    GetOpt().IsFieldShadings() &&
                    !GetOpt().IsPagePreview())
            {
                OutputDevice* pOutDev = const_cast<OutputDevice*>(GetOut());
                pOutDev->Push( vcl::PushFlags::LINECOLOR | vcl::PushFlags::FILLCOLOR );
                pOutDev->SetFillColor( GetOpt().GetFieldShadingsColor() );
                pOutDev->SetLineColor( );
                pOutDev->DrawRect( aIntersect.SVRect() );
                pOutDev->Pop();
            }
        }
    }

    SwRect aIntersect;
    CalcRect( rPor, nullptr, &aIntersect, true );

    if ( !aIntersect.HasArea() )
        return;

    OutputDevice* pTmpOut = const_cast<OutputDevice*>(GetOut());

    // #i16816# tagged pdf support
    SwTaggedPDFHelper aTaggedPDFHelper( nullptr, nullptr, nullptr, *pTmpOut );

    Color aFillColor;

    if( m_pFnt->GetHighlightColor() != COL_TRANSPARENT )
    {
        aFillColor = m_pFnt->GetHighlightColor();
    }
    else
    {
        if( !m_pFnt->GetBackColor() )
            return;
        aFillColor = *m_pFnt->GetBackColor();
    }

    pTmpOut->Push( vcl::PushFlags::LINECOLOR | vcl::PushFlags::FILLCOLOR );

    if (aFillColor == COL_TRANSPARENT)
        pTmpOut->SetFillColor();
    else
        pTmpOut->SetFillColor(aFillColor);
    pTmpOut->SetLineColor();

    DrawRect( aIntersect, false );

    pTmpOut->Pop();
}

void SwTextPaintInfo::DrawBorder( const SwLinePortion &rPor ) const
{
    SwRect aDrawArea;
    CalcRect( rPor, &aDrawArea );
    if ( aDrawArea.HasArea() )
    {
        PaintCharacterBorder(*m_pFnt, aDrawArea, GetTextFrame()->IsVertical(),
                             GetTextFrame()->IsVertLRBT(), rPor.GetJoinBorderWithPrev(),
                             rPor.GetJoinBorderWithNext());
    }
}

namespace {

bool HasValidPropertyValue(const uno::Any& rAny)
{
    if (bool bValue; rAny >>= bValue)
    {
        return true;
    }
    else if (OUString aValue; (rAny >>= aValue) && !(aValue.isEmpty()))
    {
        return true;
    }
    else if (awt::FontSlant eValue; rAny >>= eValue)
    {
        return true;
    }
    else if (tools::Long nValueLong; rAny >>= nValueLong)
    {
        return true;
    }
    else if (double fValue; rAny >>= fValue)
    {
        return true;
    }
    else if (short nValueShort; rAny >>= nValueShort)
    {
        return true;
    }
    else
        return false;
}
}

void SwTextPaintInfo::DrawCSDFHighlighting(const SwLinePortion &rPor) const
{
    // Don't use GetActiveView() as it does not work as expected when there are multiple open
    // documents.
    SwView* pView = SwTextFrame::GetView();
    if (!pView)
        return;

    if (!pView->IsSpotlightCharStyles() && !pView->IsHighlightCharDF())
        return;

    SwRect aRect;
    CalcRect(rPor, &aRect, nullptr, true);
    if(!aRect.HasArea())
        return;

    SwTextFrame* pFrame = const_cast<SwTextFrame*>(GetTextFrame());
    if (!pFrame)
        return;

    SwPosition aPosition(pFrame->MapViewToModelPos(GetIdx()));
    SwPosition aMarkPosition(pFrame->MapViewToModelPos(GetIdx() + GetLen()));

    rtl::Reference<SwXTextRange> xRange(
                SwXTextRange::CreateXTextRange(pFrame->GetDoc(), aPosition, &aMarkPosition));

    OUString sCurrentCharStyle;
    xRange->getPropertyValue(u"CharStyleName"_ustr) >>= sCurrentCharStyle;

    std::optional<OUString> sCSNumberOrDF; // CS number or "df" or not used
    std::optional<Color> aFillColor;

    // check for CS formatting, if not CS formatted check for direct character formatting
    if (!sCurrentCharStyle.isEmpty())
    {
        UIName sCharStyleDisplayName = SwStyleNameMapper::GetUIName(ProgName(sCurrentCharStyle),
                                                             SwGetPoolIdFromName::ChrFmt);
        if (comphelper::LibreOfficeKit::isActive())
        {
            // For simplicity in kit mode, we render in the document "all styles" that exist
            if (const SwCharFormat* pCharFormat = pFrame->GetDoc().FindCharFormatByName(sCharStyleDisplayName))
            {
                // Do this so these are stable across views regardless of an individual
                // user's selection mode in the style panel.
                sCSNumberOrDF = OUString::number(pFrame->GetDoc().GetCharFormats()->GetPos(pCharFormat));
                aFillColor = ColorHash(sCharStyleDisplayName.toString());
            }
        }
        else
        {
            if (!sCharStyleDisplayName.isEmpty())
            {
                StylesSpotlightColorMap& rCharStylesColorMap = pView->GetStylesSpotlightCharColorMap();
                auto it = rCharStylesColorMap.find(sCharStyleDisplayName.toString());
                if (it != rCharStylesColorMap.end())
                {
                    sCSNumberOrDF = OUString::number(it->second.second);
                    aFillColor = it->second.first;
                }
            }
        }
    }
    // not character style formatted
    else if (pView->IsHighlightCharDF())
    {
        const std::vector<OUString> aHiddenProperties{ UNO_NAME_RSID,
                    UNO_NAME_PARA_IS_NUMBERING_RESTART,
                    UNO_NAME_PARA_STYLE_NAME,
                    UNO_NAME_PARA_CONDITIONAL_STYLE_NAME,
                    UNO_NAME_PAGE_STYLE_NAME,
                    UNO_NAME_NUMBERING_START_VALUE,
                    UNO_NAME_NUMBERING_IS_NUMBER,
                    UNO_NAME_PARA_CONTINUEING_PREVIOUS_SUB_TREE,
                    UNO_NAME_CHAR_STYLE_NAME,
                    UNO_NAME_NUMBERING_LEVEL,
                    UNO_NAME_SORTED_TEXT_ID,
                    UNO_NAME_PARRSID,
                    UNO_NAME_CHAR_COLOR_THEME,
                    UNO_NAME_CHAR_COLOR_TINT_OR_SHADE };

        SfxItemPropertySet const& rPropSet(
                    *aSwMapProvider.GetPropertySet(PROPERTY_MAP_CHAR_AUTO_STYLE));
        SfxItemPropertyMap const& rMap(rPropSet.getPropertyMap());


        const uno::Sequence<beans::Property> aProperties
                = xRange->getPropertySetInfo()->getProperties();

        for (const beans::Property& rProperty : aProperties)
        {
            const OUString& rPropName = rProperty.Name;

            if (!rMap.hasPropertyByName(rPropName))
                continue;

            if (std::find(aHiddenProperties.begin(), aHiddenProperties.end(), rPropName)
                    != aHiddenProperties.end())
                continue;

            if (xRange->getPropertyState(rPropName) == beans::PropertyState_DIRECT_VALUE)
            {
                const uno::Any aAny = xRange->getPropertyValue(rPropName);
                if (HasValidPropertyValue(aAny))
                {
                    sCSNumberOrDF = SwResId(STR_CHARACTER_DIRECT_FORMATTING_TAG);
                    aFillColor = COL_LIGHTGRAY;
                    break;
                }
            }
        }
    }
    if (sCSNumberOrDF)
    {
        OutputDevice* pTmpOut = const_cast<OutputDevice*>(GetOut());
        pTmpOut->Push(vcl::PushFlags::LINECOLOR | vcl::PushFlags::FILLCOLOR
                      | vcl::PushFlags::TEXTLAYOUTMODE | vcl::PushFlags::FONT);

        // draw a filled rectangle at the formatted CS or DF text
        pTmpOut->SetFillColor(aFillColor.value());
        pTmpOut->SetLineColor(aFillColor.value());
        tools::Rectangle aSVRect(aRect.SVRect());
        pTmpOut->DrawRect(aSVRect);

        // calculate size and position for the CS number or "df" text and rectangle
        tools::Long nWidth = pTmpOut->GetTextWidth(sCSNumberOrDF.value());
        tools::Long nHeight = pTmpOut->GetTextHeight();
        aSVRect.SetSize(Size(nWidth, nHeight));
        aSVRect.Move(-(nWidth / 1.5), -(nHeight / 1.5));

        vcl::Font aFont(pTmpOut->GetFont());
        aFont.SetOrientation(Degree10(0));
        pTmpOut->SetFont(aFont);

        pTmpOut->SetLayoutMode(vcl::text::ComplexTextLayoutFlags::TextOriginLeft);
        //pTmpOut->SetLayoutMode(vcl::text::ComplexTextLayoutFlags::BiDiStrong);

        pTmpOut->SetTextFillColor(aFillColor.value());
        pTmpOut->DrawText(aSVRect, sCSNumberOrDF.value(), DrawTextFlags::NONE);

        pTmpOut->Pop();
    }
}

void SwTextPaintInfo::DrawViewOpt( const SwLinePortion &rPor,
                                   PortionType nWhich, const Color *pColor ) const
{
    if( !OnWin() || IsMulti() )
        return;

    bool bDraw = false;
    if ( !GetOpt().IsPagePreview()
         && !GetOpt().IsReadonly() )
    {
        switch( nWhich )
        {
        case PortionType::Tab:
            if ( GetOpt().IsViewMetaChars() )
                bDraw = GetOpt().IsTab();
            break;
        case PortionType::SoftHyphen:
            if ( GetOpt().IsViewMetaChars() )
                bDraw = GetOpt().IsSoftHyph();
            break;
        case PortionType::Blank:
            if ( GetOpt().IsViewMetaChars() )
                bDraw = GetOpt().IsHardBlank();
            break;
        case PortionType::ControlChar:
            if ( GetOpt().IsViewMetaChars() )
                bDraw = true;
            break;
        case PortionType::Bookmark:
            // no shading
            break;
        case PortionType::Footnote:
        case PortionType::QuoVadis:
        case PortionType::Number:
        case PortionType::Hidden:
        case PortionType::Tox:
        case PortionType::Ref:
        case PortionType::Meta:
        case PortionType::ContentControl:
        case PortionType::Field:
        case PortionType::InputField:
            // input field shading also in read-only mode
            if (GetOpt().IsFieldShadings()
                && ( PortionType::Number != nWhich
                    || m_pFrame->GetTextNodeForParaProps()->HasMarkedLabel())) // #i27615#
            {
                bDraw = PortionType::Footnote != nWhich || m_pFrame->IsFootnoteAllowed();
            }
            break;
        default:
            {
                OSL_ENSURE( false, "SwTextPaintInfo::DrawViewOpt: don't know how to draw this" );
                break;
            }
        }
    }

    if ( bDraw )
        DrawBackground( rPor, pColor );
}

void SwTextPaintInfo::NotifyURL_(const SwLinePortion& rPor) const
{
    assert(pNoteURL);

    SwRect aIntersect;
    CalcRect(rPor, nullptr, &aIntersect);

    if (aIntersect.HasArea())
    {
        SwTextNode* pNd = const_cast<SwTextNode*>(GetTextFrame()->GetTextNodeFirst());
        SwTextAttr* const pAttr = pNd->GetTextAttrAt(sal_Int32(GetIdx()), RES_TXTATR_INETFMT);
        if (pAttr)
        {
            const SwFormatINetFormat& rFormat = pAttr->GetINetFormat();
            pNoteURL->InsertURLNote(rFormat.GetValue(), rFormat.GetTargetFrame(), aIntersect);
        }
        else if (rPor.IsFlyCntPortion())
        {
            if (auto* pFlyContentPortion = dynamic_cast<const sw::FlyContentPortion*>(&rPor))
            {
                if (auto* pFlyFtame = pFlyContentPortion->GetFlyFrame())
                {
                    if (auto* pFormat = pFlyFtame->GetFormat())
                    {
                        auto& url = pFormat->GetURL(); // TODO: url.GetMap() ?
                        pNoteURL->InsertURLNote(url.GetURL(), url.GetTargetFrameName(), aIntersect);
                    }
                }
            }
        }
    }
}

static void lcl_InitHyphValues( PropertyValues &rVals,
            sal_Int16 nMinLeading, sal_Int16 nMinTrailing,
            bool bNoCapsHyphenation, bool bNoLastWordHyphenation,
            sal_Int16 nMinWordLength, sal_Int16 nTextHyphZone, bool bKeep, sal_Int16 nKeepType,
            bool bKeepLine, sal_Int16 nCompoundMinLeading, sal_Int16 nTextHyphZoneAlways )
{
    sal_Int32 nLen = rVals.getLength();

    if (0 == nLen)  // yet to be initialized?
    {
        rVals.realloc( 11 );
        PropertyValue *pVal = rVals.getArray();

        pVal[0].Name    = UPN_HYPH_MIN_LEADING;
        pVal[0].Handle  = UPH_HYPH_MIN_LEADING;
        pVal[0].Value   <<= nMinLeading;

        pVal[1].Name    = UPN_HYPH_MIN_TRAILING;
        pVal[1].Handle  = UPH_HYPH_MIN_TRAILING;
        pVal[1].Value   <<= nMinTrailing;

        pVal[2].Name    = UPN_HYPH_NO_CAPS;
        pVal[2].Handle  = UPH_HYPH_NO_CAPS;
        pVal[2].Value   <<= bNoCapsHyphenation;

        pVal[3].Name    = UPN_HYPH_NO_LAST_WORD;
        pVal[3].Handle  = UPH_HYPH_NO_LAST_WORD;
        pVal[3].Value   <<= bNoLastWordHyphenation;

        pVal[4].Name    = UPN_HYPH_MIN_WORD_LENGTH;
        pVal[4].Handle  = UPH_HYPH_MIN_WORD_LENGTH;
        pVal[4].Value   <<= nMinWordLength;

        pVal[5].Name    = UPN_HYPH_ZONE;
        pVal[5].Handle  = UPH_HYPH_ZONE;
        pVal[5].Value   <<= nTextHyphZone;

        pVal[6].Name    = UPN_HYPH_KEEP_TYPE;
        pVal[6].Handle  = UPH_HYPH_KEEP_TYPE;
        pVal[6].Value   <<= nKeepType;

        pVal[7].Name    = UPN_HYPH_COMPOUND_MIN_LEADING;
        pVal[7].Handle  = UPH_HYPH_COMPOUND_MIN_LEADING;
        pVal[7].Value   <<= nCompoundMinLeading;

        pVal[8].Name    = UPN_HYPH_KEEP;
        pVal[8].Handle  = UPH_HYPH_KEEP;
        pVal[8].Value   <<= bKeep;

        pVal[9].Name    = UPN_HYPH_KEEP_LINE;
        pVal[9].Handle  = UPH_HYPH_KEEP_LINE;
        pVal[9].Value   <<= bKeepLine;

        pVal[10].Name    = UPN_HYPH_ZONE_ALWAYS;
        pVal[10].Handle  = UPH_HYPH_ZONE_ALWAYS;
        pVal[10].Value   <<= nTextHyphZoneAlways;
    }
    else if (11 == nLen) // already initialized once?
    {
        PropertyValue *pVal = rVals.getArray();
        pVal[0].Value <<= nMinLeading;
        pVal[1].Value <<= nMinTrailing;
        pVal[2].Value <<= bNoCapsHyphenation;
        pVal[3].Value <<= bNoLastWordHyphenation;
        pVal[4].Value <<= nMinWordLength;
        pVal[5].Value <<= nTextHyphZone;
        pVal[6].Value <<= nKeepType;
        pVal[7].Value <<= nCompoundMinLeading;
        pVal[8].Value <<= bKeep;
        pVal[9].Value <<= bKeepLine;
        pVal[10].Value <<= nTextHyphZoneAlways;
    }
    else {
        OSL_FAIL( "unexpected size of sequence" );
    }
}

const PropertyValues & SwTextFormatInfo::GetHyphValues() const
{
    OSL_ENSURE( 11 == m_aHyphVals.getLength(),
            "hyphenation values not yet initialized" );
    return m_aHyphVals;
}

bool SwTextFormatInfo::InitHyph( const bool bAutoHyphen )
{
    const SwAttrSet& rAttrSet = GetTextFrame()->GetTextNodeForParaProps()->GetSwAttrSet();
    SetHanging( rAttrSet.GetHangingPunctuation().GetValue() );
    SetScriptSpace( rAttrSet.GetScriptSpace().GetValue() );
    SetForbiddenChars( rAttrSet.GetForbiddenRule().GetValue() );
    const SvxHyphenZoneItem &rAttr = rAttrSet.GetHyphenZone();
    MaxHyph() = rAttr.GetMaxHyphens();
    const bool bAuto = bAutoHyphen || rAttr.IsHyphen();
    if( bAuto || m_bInterHyph )
    {
        const sal_Int16 nMinimalLeading  = std::max(rAttr.GetMinLead(), sal_uInt8(2));
        const sal_Int16 nMinimalTrailing = rAttr.GetMinTrail();
        const sal_Int16 nMinimalWordLength = rAttr.GetMinWordLength();
        const bool bNoCapsHyphenation = rAttr.IsNoCapsHyphenation();
        const bool bNoLastWordHyphenation = rAttr.IsNoLastWordHyphenation();
        const sal_Int16 nTextHyphZone = rAttr.GetTextHyphenZone();
        const sal_Int16 nTextHyphZoneAlways = rAttr.GetTextHyphenZoneAlways();
        const bool bKeep = rAttr.IsKeep();
        const sal_Int16 nKeepType = rAttr.GetKeepType();
        const bool bKeepLine = rAttr.IsKeepLine();
        const sal_Int16 nCompoundMinimalLeading  = std::max(rAttr.GetCompoundMinLead(), sal_uInt8(2));
        lcl_InitHyphValues( m_aHyphVals, nMinimalLeading, nMinimalTrailing,
                 bNoCapsHyphenation, bNoLastWordHyphenation,
                 nMinimalWordLength, nTextHyphZone, bKeep, nKeepType,
                 bKeepLine, nCompoundMinimalLeading, nTextHyphZoneAlways );
    }
    return bAuto;
}

void SwTextFormatInfo::CtorInitTextFormatInfo( OutputDevice* pRenderContext, SwTextFrame *pNewFrame, const bool bNewInterHyph,
                                const bool bNewQuick, const bool bTst )
{
    CtorInitTextPaintInfo( pRenderContext, pNewFrame, SwRect() );

    m_bQuick = bNewQuick;
    m_bInterHyph = bNewInterHyph;

    //! needs to be done in this order
    m_bAutoHyph = InitHyph();

    m_bIgnoreFly = false;
    m_bFakeLineStart = false;
    m_bShift = false;
    m_bDropInit = false;
    m_bTestFormat = bTst;
    m_nLeft = 0;
    m_nRight = 0;
    m_nFirst = 0;
    m_nRealWidth = 0;
    m_nForcedLeftMargin = 0;
    m_nExtraAscent = 0;
    m_nExtraDescent = 0;
    m_pRest = nullptr;
    m_nLineHeight = 0;
    m_nLineNetHeight = 0;
    SetLineStart(TextFrameIndex(0));

    SvtCTLOptions::TextNumerals const nTextNumerals(SwModule::get()->GetCTLTextNumerals());
    // cannot cache for NUMERALS_CONTEXT because we need to know the string
    // for the whole paragraph now
    if (nTextNumerals != SvtCTLOptions::NUMERALS_CONTEXT)
    {
        // set digit mode to what will be used later to get same results
        SwDigitModeModifier const m(*m_pRef, LANGUAGE_NONE /*dummy*/, nTextNumerals);
        assert(m_pRef->GetDigitLanguage() != LANGUAGE_NONE);
        SetCachedVclData(OutputDevice::CreateTextLayoutCache(*m_pText));
    }

    Init();
}

/**
 * If the Hyphenator returns ERROR or the language is set to NOLANGUAGE
 * we do not hyphenate.
 * Else, we always hyphenate if we do interactive hyphenation.
 * If we do not do interactive hyphenation, we only hyphenate if ParaFormat is
 * set to automatic hyphenation.
 */
bool SwTextFormatInfo::IsHyphenate() const
{
    if( !m_bInterHyph && !m_bAutoHyph )
        return false;

    LanguageType eTmp = GetFont()->GetLanguage();
    // TODO: check for more ideographic langs w/o hyphenation as a concept
    if ( LANGUAGE_DONTKNOW == eTmp || LANGUAGE_NONE == eTmp
            || !MsLangId::usesHyphenation(eTmp) )
        return false;

    uno::Reference< XHyphenator > xHyph = ::GetHyphenator();
    if (!xHyph.is())
        return false;

    if (m_bInterHyph)
        SvxSpellWrapper::CheckHyphLang( xHyph, eTmp );

    if (!xHyph->hasLocale(g_pBreakIt->GetLocale(eTmp)))
    {
        SfxObjectShell* pShell = m_pFrame->GetDoc().GetDocShell();
        if (pShell)
        {
            pShell->AppendInfoBarWhenReady(
                u"hyphenationmissing"_ustr, SwResId(STR_HYPH_MISSING),
                SwResId(STR_HYPH_MISSING_DETAIL)
                    .replaceFirst("%1", LanguageTag::convertToBcp47( g_pBreakIt->GetLocale(eTmp))),
                InfobarType::WARNING);
        }
    }

    return xHyph->hasLocale( g_pBreakIt->GetLocale(eTmp) );
}

const SwFormatDrop *SwTextFormatInfo::GetDropFormat() const
{
    const SwFormatDrop *pDrop = &GetTextFrame()->GetTextNodeForParaProps()->GetSwAttrSet().GetDrop();
    if( 1 >= pDrop->GetLines() ||
        ( !pDrop->GetChars() && !pDrop->GetWholeWord() ) )
        pDrop = nullptr;
    return pDrop;
}

void SwTextFormatInfo::Init()
{
    // Not initialized: pRest, nLeft, nRight, nFirst, nRealWidth
    X(0);
    m_bArrowDone = m_bFull = m_bFootnoteDone = m_bErgoDone = m_bNumDone = m_bNoEndHyph =
        m_bNoMidHyph = m_bStop = m_bNewLine = m_bUnderflow = m_bTabOverflow = false;

    // generally we do not allow number portions in follows, except...
    if ( GetTextFrame()->IsFollow() )
    {
        const SwTextFrame* pMaster = GetTextFrame()->FindMaster();
        OSL_ENSURE(pMaster, "pTextFrame without Master");
        const SwLinePortion* pTmpPara = pMaster ? pMaster->GetPara() : nullptr;

        // there is a master for this follow and the master does not have
        // any contents (especially it does not have a number portion)
        m_bNumDone = ! pTmpPara ||
                   ! static_cast<const SwParaPortion*>(pTmpPara)->GetFirstPortion()->IsFlyPortion();
    }

    m_pRoot = nullptr;
    m_pLast = nullptr;
    m_pFly = nullptr;
    m_pLastTab = nullptr;
    m_pUnderflow = nullptr;
    m_cTabDecimal = 0;
    m_nWidth = m_nRealWidth;
    m_nForcedLeftMargin = 0;
    m_nExtraAscent = 0;
    m_nExtraDescent = 0;
    m_nSoftHyphPos = TextFrameIndex(0);
    m_nLastBookmarkPos = TextFrameIndex(-1);
    m_cHookChar = 0;
    SetIdx(TextFrameIndex(0));
    SetLen(TextFrameIndex(GetText().getLength()));
    SetPaintOfst(0);
}

SwTextFormatInfo::SwTextFormatInfo(OutputDevice* pRenderContext, SwTextFrame *pFrame, const bool bInterHyphL,
                                   const bool bQuickL, const bool bTst)
{
    CtorInitTextFormatInfo(pRenderContext, pFrame, bInterHyphL, bQuickL, bTst);
}

/**
 * There are a few differences between a copy constructor
 * and the following constructor for multi-line formatting.
 * The root is the first line inside the multi-portion,
 * the line start is the actual position in the text,
 * the line width is the rest width from the surrounding line
 * and the bMulti and bFirstMulti-flag has to be set correctly.
 */
SwTextFormatInfo::SwTextFormatInfo( const SwTextFormatInfo& rInf,
    SwLineLayout& rLay, SwTwips nActWidth ) :
    SwTextPaintInfo( rInf ),
    m_pRoot(&rLay),
    m_pLast(&rLay),
    m_pFly(nullptr),
    m_pUnderflow(nullptr),
    m_pRest(nullptr),
    m_pLastTab(nullptr),
    m_nSoftHyphPos(TextFrameIndex(0)),
    m_nLineStart(rInf.GetIdx()),
    m_nLeft(rInf.m_nLeft),
    m_nRight(rInf.m_nRight),
    m_nFirst(rInf.m_nLeft),
    m_nRealWidth(nActWidth),
    m_nWidth(m_nRealWidth),
    m_nLineHeight(0),
    m_nLineNetHeight(0),
    m_nForcedLeftMargin(0),
    m_nExtraAscent(0),
    m_nExtraDescent(0),
    m_bFull(false),
    m_bFootnoteDone(true),
    m_bErgoDone(true),
    m_bNumDone(true),
    m_bArrowDone(true),
    m_bStop(false),
    m_bNewLine(true),
    m_bShift(false),
    m_bUnderflow(false),
    m_bInterHyph(false),
    m_bAutoHyph(false),
    m_bDropInit(false),
    m_bQuick(rInf.m_bQuick),
    m_bNoEndHyph(false),
    m_bNoMidHyph(false),
    m_bIgnoreFly(false),
    m_bFakeLineStart(false),
    m_bTabOverflow( false ),
    m_bTestFormat(rInf.m_bTestFormat),
    m_cTabDecimal(0),
    m_cHookChar(0),
    m_nMaxHyph(0)
{
    SetMulti( true );
    SetFirstMulti( rInf.IsFirstMulti() );
}

void SwTextFormatInfo::UpdateTabSeen(PortionType type)
{
    switch (type)
    {
        case PortionType::TabLeft:
            m_eLastTabsSeen = TabSeen::Left;
            break;
        case PortionType::TabRight:
            m_eLastTabsSeen = TabSeen::Right;
            break;
        case PortionType::TabCenter:
            m_eLastTabsSeen = TabSeen::Center;
            break;
        case PortionType::TabDecimal:
            m_eLastTabsSeen = TabSeen::Decimal;
            break;
        case PortionType::Break:
            m_eLastTabsSeen = TabSeen::None;
            break;
        default:
            break;
    }
}

void SwTextFormatInfo::SetLast(SwLinePortion* pNewLast)
{
    m_pLast = pNewLast;
    assert(pNewLast); // We never pass nullptr here. If we start, then a check is needed below.
    UpdateTabSeen(pNewLast->GetWhichPor());
}

bool SwTextFormatInfo::CheckFootnotePortion_( SwLineLayout const * pCurr )
{
    const SwTwips nHeight = pCurr->GetRealHeight();
    for( SwLinePortion *pPor = pCurr->GetNextPortion(); pPor; pPor = pPor->GetNextPortion() )
    {
        if( pPor->IsFootnotePortion() && nHeight > static_cast<SwFootnotePortion*>(pPor)->Orig() )
        {
            SetLineHeight( nHeight );
            SetLineNetHeight( pCurr->Height() );
            return true;
        }
    }
    return false;
}

TextFrameIndex SwTextFormatInfo::ScanPortionEnd(TextFrameIndex const nStart,
                                                TextFrameIndex const nEnd)
{
    m_cHookChar = 0;
    TextFrameIndex i = nStart;

    // Used for decimal tab handling:
    const sal_Unicode cTabDec = GetLastTab() ? GetTabDecimal() : 0;
    const sal_Unicode cThousandSep  = ',' == cTabDec ? '.' : ',';

    // #i45951# German (Switzerland) uses ' as thousand separator
    const sal_Unicode cThousandSep2 = ',' == cTabDec ? '.' : '\'';

    bool bNumFound = false;
    const bool bTabCompat = GetTextFrame()->GetDoc().getIDocumentSettingAccess().get(DocumentSettingId::TAB_COMPAT);

    for( ; i < nEnd; ++i )
    {
        const sal_Unicode cPos = GetChar( i );
        switch( cPos )
        {
        case CH_TXTATR_BREAKWORD:
        case CH_TXTATR_INWORD:
            if( !HasHint( i ))
                break;
            [[fallthrough]];

        case CHAR_SOFTHYPHEN:
        case CHAR_HARDHYPHEN:
        case CHAR_HARDBLANK:
        case CH_TAB:
        case CH_BREAK:
        case CHAR_ZWSP :
        case CHAR_WJ :
            m_cHookChar = cPos;
            return i;

        default:
            if ( cTabDec )
            {
                if( cTabDec == cPos )
                {
                    OSL_ENSURE( cPos, "Unexpected end of string" );
                    if( cPos ) // robust
                    {
                        m_cHookChar = cPos;
                        return i;
                    }
                }

                // Compatibility: First non-digit character behind a
                // a digit character becomes the hook character
                if ( bTabCompat )
                {
                    if ( ( 0x2F < cPos && cPos < 0x3A ) ||
                         ( bNumFound && ( cPos == cThousandSep || cPos == cThousandSep2 ) ) )
                    {
                        bNumFound = true;
                    }
                    else
                    {
                        if ( bNumFound )
                        {
                            m_cHookChar = cPos;
                            SetTabDecimal( cPos );
                            return i;
                        }
                    }
                }
            }
        }
    }

    // Check if character *behind* the portion has
    // to become the hook:
    if (i == nEnd && i < TextFrameIndex(GetText().getLength()) && bNumFound)
    {
        const sal_Unicode cPos = GetChar( i );
        if ( cPos != cTabDec && cPos != cThousandSep && cPos !=cThousandSep2 && ( 0x2F >= cPos || cPos >= 0x3A ) )
        {
            m_cHookChar = GetChar( i );
            SetTabDecimal( m_cHookChar );
        }
    }

    return i;
}

bool SwTextFormatInfo::LastKernPortion()
{
    if( GetLast() )
    {
        if( GetLast()->IsKernPortion() )
            return true;
        if( GetLast()->Width() || ( GetLast()->GetLen() &&
            !GetLast()->IsHolePortion() ) )
            return false;
    }
    SwLinePortion* pPor = GetRoot();
    SwLinePortion *pKern = nullptr;
    while( pPor )
    {
        if( pPor->IsKernPortion() )
            pKern = pPor;
        else if( pPor->Width() || ( pPor->GetLen() && !pPor->IsHolePortion() ) )
            pKern = nullptr;
        pPor = pPor->GetNextPortion();
    }
    if( pKern )
    {
        SetLast( pKern );
        return true;
    }
    return false;
}

SwTwips SwTextFormatInfo::GetLineWidth()
{
    SwTwips nLineWidth = Width() - X();

    const bool bTabOverMargin = GetTextFrame()->GetDoc().getIDocumentSettingAccess().get(
        DocumentSettingId::TAB_OVER_MARGIN);
    const bool bTabOverSpacing = GetTextFrame()->GetDoc().getIDocumentSettingAccess().get(
        DocumentSettingId::TAB_OVER_SPACING);
    if (!bTabOverMargin && !bTabOverSpacing)
        return nLineWidth;

    SwTabPortion* pLastTab = GetLastTab();
    if (!pLastTab)
        return nLineWidth;

    // Consider tab portions over the printing bounds of the text frame.
    if (pLastTab->GetTabPos() <= Width())
        return nLineWidth;

    // Calculate the width that starts at the left (or in case of first line:
    // first) margin, but ends after the right paragraph margin:
    //
    // +--------------------+
    // |LL|              |RR|
    // +--------------------+
    // ^ m_nLeftMargin (absolute)
    //    ^ nLeftMarginWidth (relative to m_nLeftMargin), X() is relative to this
    //                   ^ right margin
    //                      ^ paragraph right
    // <--------------------> is GetTextFrame()->getFrameArea().Width()
    //    <-------------->    is Width()
    //    <-----------------> is what we need to be able to compare to X() (nTextFrameWidth)
    SwTwips nLeftMarginWidth = m_nLeftMargin - GetTextFrame()->getFrameArea().Left();
    SwTwips nTextFrameWidth = GetTextFrame()->getFrameArea().Width() - nLeftMarginWidth;

    // If there is one such tab portion, then text is allowed to use the full
    // text frame area to the right (RR above, but not LL).
    nLineWidth = nTextFrameWidth - X();

    if (!bTabOverMargin) // thus bTabOverSpacing only
    {
        // right, center, decimal can back-fill all the available space - same as TabOverMargin
        if (pLastTab->GetWhichPor() == PortionType::TabLeft)
            nLineWidth = nTextFrameWidth - pLastTab->GetTabPos();
    }
    else
    {   // tdf#158658 Put content after tab into margin like Word.
        // Try to limit the paragraph to 55.87cm, it's max tab pos in Word UI.
        nLineWidth = o3tl::toTwips(558, o3tl::Length::mm) - X();
    }
    return nLineWidth;
}

SwTextSlot::SwTextSlot(
    const SwTextSizeInfo *pNew,
    const SwLinePortion *pPor,
    bool bTextLen,
    bool bExgLists,
    OUString const & rCh )
    : pOldText(nullptr)
    , m_pOldSmartTagList(nullptr)
    , m_pOldGrammarCheckList(nullptr)
    , nIdx(0)
    , nLen(0)
    , nMeasureLen(0)
    , pInf(nullptr)
{
    if( rCh.isEmpty() )
    {
        bOn = pPor->GetExpText( *pNew, aText );
    }
    else
    {
        aText = rCh;
        bOn = true;
    }

    // The text is replaced ...
    if( !bOn )
        return;

    pInf = const_cast<SwTextSizeInfo*>(pNew);
    nIdx = pInf->GetIdx();
    nLen = pInf->GetLen();
    nMeasureLen = pInf->GetMeasureLen();
    pOldText = &(pInf->GetText());
    m_pOldCachedVclData = pInf->GetCachedVclData();
    pInf->SetText( aText );
    pInf->SetIdx(TextFrameIndex(0));
    pInf->SetLen(bTextLen ? TextFrameIndex(pInf->GetText().getLength()) : pPor->GetLen());
    if (nMeasureLen != TextFrameIndex(COMPLETE_STRING))
        pInf->SetMeasureLen(TextFrameIndex(COMPLETE_STRING));

    pInf->SetCachedVclData(nullptr);

    // ST2
    if ( !bExgLists )
        return;

    m_pOldSmartTagList = static_cast<SwTextPaintInfo*>(pInf)->GetSmartTags();
    if (m_pOldSmartTagList)
    {
        std::pair<SwTextNode const*, sal_Int32> pos(pNew->GetTextFrame()->MapViewToModel(nIdx));
        SwWrongList const*const pSmartTags(pos.first->GetSmartTags());
        if (pSmartTags)
        {
            const sal_uInt16 nPos = pSmartTags->GetWrongPos(pos.second);
            const sal_Int32 nListPos = pSmartTags->Pos(nPos);
            if (nListPos == pos.second && pSmartTags->SubList(nPos) != nullptr)
            {
                m_pTempIter.reset(new sw::WrongListIterator(*pSmartTags->SubList(nPos)));
                static_cast<SwTextPaintInfo*>(pInf)->SetSmartTags(m_pTempIter.get());
            }
            else if (!m_pTempList && nPos < pSmartTags->Count()
                        && nListPos < pos.second && !aText.isEmpty())
            {
                m_pTempList.reset(new SwWrongList( WRONGLIST_SMARTTAG ));
                m_pTempList->Insert( OUString(), nullptr, 0, aText.getLength(), 0 );
                m_pTempIter.reset(new sw::WrongListIterator(*m_pTempList));
                static_cast<SwTextPaintInfo*>(pInf)->SetSmartTags(m_pTempIter.get());
            }
            else
                static_cast<SwTextPaintInfo*>(pInf)->SetSmartTags(nullptr);
        }
        else
            static_cast<SwTextPaintInfo*>(pInf)->SetSmartTags(nullptr);
    }
    m_pOldGrammarCheckList = static_cast<SwTextPaintInfo*>(pInf)->GetGrammarCheckList();
    if (!m_pOldGrammarCheckList)
        return;

    std::pair<SwTextNode const*, sal_Int32> pos(pNew->GetTextFrame()->MapViewToModel(nIdx));
    SwWrongList const*const pGrammar(pos.first->GetGrammarCheck());
    if (pGrammar)
    {
        const sal_uInt16 nPos = pGrammar->GetWrongPos(pos.second);
        const sal_Int32 nListPos = pGrammar->Pos(nPos);
        if (nListPos == pos.second && pGrammar->SubList(nPos) != nullptr)
        {
            m_pTempIter.reset(new sw::WrongListIterator(*pGrammar->SubList(nPos)));
            static_cast<SwTextPaintInfo*>(pInf)->SetGrammarCheckList(m_pTempIter.get());
        }
        else if (!m_pTempList && nPos < pGrammar->Count()
                    && nListPos < pos.second && !aText.isEmpty())
        {
            m_pTempList.reset(new SwWrongList( WRONGLIST_GRAMMAR ));
            m_pTempList->Insert( OUString(), nullptr, 0, aText.getLength(), 0 );
            m_pTempIter.reset(new sw::WrongListIterator(*m_pTempList));
            static_cast<SwTextPaintInfo*>(pInf)->SetGrammarCheckList(m_pTempIter.get());
        }
        else
            static_cast<SwTextPaintInfo*>(pInf)->SetGrammarCheckList(nullptr);
    }
    else
        static_cast<SwTextPaintInfo*>(pInf)->SetGrammarCheckList(nullptr);
}

SwTextSlot::~SwTextSlot()
{
    if( !bOn )
        return;

    pInf->SetCachedVclData(m_pOldCachedVclData);
    pInf->SetText( *pOldText );
    pInf->SetIdx( nIdx );
    pInf->SetLen( nLen );
    pInf->SetMeasureLen( nMeasureLen );

    // ST2
    // Restore old smart tag list
    if (m_pOldSmartTagList)
        static_cast<SwTextPaintInfo*>(pInf)->SetSmartTags(m_pOldSmartTagList);
    if (m_pOldGrammarCheckList)
        static_cast<SwTextPaintInfo*>(pInf)->SetGrammarCheckList(m_pOldGrammarCheckList);
}

SwFontSave::SwFontSave(const SwTextSizeInfo &rInf, SwFont *pNew,
        SwAttrIter* pItr)
    : pInf(nullptr)
    , pFnt(pNew ? const_cast<SwTextSizeInfo&>(rInf).GetFont() : nullptr)
    , pIter(nullptr)
{
    if( !pFnt )
        return;

    pInf = &const_cast<SwTextSizeInfo&>(rInf);
    // In these cases we temporarily switch to the new font:
    // 1. the fonts have a different magic number
    // 2. they have different script types
    // 3. their background colors differ (this is not covered by 1.)
    if( pFnt->DifferentFontCacheId( pNew, pFnt->GetActual() ) ||
        pNew->GetActual() != pFnt->GetActual() ||
        ( ! pNew->GetBackColor() && pFnt->GetBackColor() ) ||
        ( pNew->GetBackColor() && ! pFnt->GetBackColor() ) ||
        ( pNew->GetBackColor() && pFnt->GetBackColor() &&
          ( *pNew->GetBackColor() != *pFnt->GetBackColor() ) )
        || !pNew->GetActualFont().SvxFontSubsetEquals(pFnt->GetActualFont()))
    {
        pNew->SetTransparent( true );
        pNew->SetAlign( ALIGN_BASELINE );
        pInf->SetFont( pNew );
    }
    else
        pFnt = nullptr;
    pNew->Invalidate();
    pNew->ChgPhysFnt( pInf->GetVsh(), *pInf->GetOut() );
    if( pItr && pItr->GetFnt() == pFnt )
    {
        pIter = pItr;
        pIter->SetFnt( pNew );
    }
}

SwFontSave::~SwFontSave()
{
    if( pFnt )
    {
        // Reset SwFont
        pFnt->Invalidate();
        pInf->SetFont( pFnt );
        if( pIter )
        {
            pIter->SetFnt( pFnt );
            pIter->m_nPosition = COMPLETE_STRING;
        }
    }
}

bool SwTextFormatInfo::ChgHyph( const bool bNew )
{
    const bool bOld = m_bAutoHyph;
    if( m_bAutoHyph != bNew )
    {
        m_bAutoHyph = bNew;
        InitHyph( bNew );
        // Set language in the Hyphenator
        if( m_pFnt )
            m_pFnt->ChgPhysFnt( m_pVsh, *m_pOut );
    }
    return bOld;
}


bool SwTextFormatInfo::CheckCurrentPosBookmark()
{
    if (m_nLastBookmarkPos != GetIdx())
    {
        m_nLastBookmarkPos = GetIdx();
        return true;
    }
    else
    {
        return false;
    }
}

sal_Int32 SwTextFormatInfo::GetLineSpaceCount(TextFrameIndex nBreakPos)
{
    if ( sal_Int32(nBreakPos) >= GetText().getLength() )
        return 0;

    sal_Int32 nSpaces = 0;
    sal_Int32 nInlineSpaces = -1;
    for (sal_Int32 i = sal_Int32(GetLineStart()); i < sal_Int32(nBreakPos); ++i)
    {
        sal_Unicode cChar = GetText()[i];
        if ( cChar == CH_BLANK )
            ++nSpaces;
        else
        {
            if ( nInlineSpaces == -1 )
            {
                nInlineSpaces = 0;
                nSpaces = 0;
            }
            else
                nInlineSpaces = nSpaces;
        }
    }
    return nInlineSpaces == -1 ? 0: nInlineSpaces;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
