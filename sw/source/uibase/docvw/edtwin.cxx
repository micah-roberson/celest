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

#include <config_wasm_strip.h>

#include <swtypes.hxx>
#include <hintids.hxx>

#include <com/sun/star/accessibility/XAccessible.hpp>
#include <com/sun/star/awt/PopupMenuDirection.hpp>
#include <com/sun/star/awt/XPopupMenu.hpp>
#include <com/sun/star/i18n/XBreakIterator.hpp>
#include <com/sun/star/i18n/ScriptType.hpp>
#include <com/sun/star/i18n/InputSequenceCheckMode.hpp>
#include <com/sun/star/i18n/UnicodeScript.hpp>
#include <com/sun/star/i18n/XExtendedInputSequenceChecker.hpp>
#include <com/sun/star/ui/ContextMenuExecuteEvent.hpp>

#include <comphelper/scopeguard.hxx>
#include <comphelper/string.hxx>

#include <vcl/dialoghelper.hxx>
#include <vcl/inputctx.hxx>
#include <vcl/help.hxx>
#include <vcl/weld.hxx>
#include <vcl/ptrstyle.hxx>
#include <svl/macitem.hxx>
#include <unotools/securityoptions.hxx>
#include <basic/sbxvar.hxx>
#include <svl/ctloptions.hxx>
#include <basic/sbx.hxx>
#include <svl/eitem.hxx>
#include <svl/stritem.hxx>
#include <sfx2/ipclient.hxx>
#include <sfx2/viewfrm.hxx>
#include <sfx2/request.hxx>
#include <sfx2/bindings.hxx>
#include <sfx2/dispatch.hxx>
#include <svl/ptitem.hxx>
#include <editeng/sizeitem.hxx>
#include <editeng/langitem.hxx>
#include <svx/statusitem.hxx>
#include <svx/svdview.hxx>
#include <svx/svdhdl.hxx>
#include <svx/svdoutl.hxx>
#include <editeng/editeng.hxx>
#include <editeng/editview.hxx>
#include <editeng/svxacorr.hxx>
#include <editeng/flditem.hxx>
#include <editeng/colritem.hxx>
#include <unotools/charclass.hxx>
#include <unotools/datetime.hxx>

#include <comphelper/lok.hxx>
#include <sfx2/lokhelper.hxx>

#include <editeng/acorrcfg.hxx>
#include <SwSmartTagMgr.hxx>
#include <edtdd.hxx>
#include <edtwin.hxx>
#include <view.hxx>
#include <wrtsh.hxx>
#include <IDocumentDrawModelAccess.hxx>
#include <IDocumentUndoRedo.hxx>
#include <textboxhelper.hxx>
#include <dcontact.hxx>
#include <fldbas.hxx>
#include <swmodule.hxx>
#include <docsh.hxx>
#include <viewopt.hxx>
#include <drawbase.hxx>
#include <dselect.hxx>
#include <textsh.hxx>
#include <shdwcrsr.hxx>
#include <txatbase.hxx>
#include <fmtanchr.hxx>
#include <fmtornt.hxx>
#include <fmthdft.hxx>
#include <frmfmt.hxx>
#include <modcfg.hxx>
#include <fmtcol.hxx>
#include <wview.hxx>
#include <gloslst.hxx>
#include <inputwin.hxx>
#include <gloshdl.hxx>
#include <swundo.hxx>
#include <drwtxtsh.hxx>
#include <fchrfmt.hxx>
#include "romenu.hxx"
#include <initui.hxx>
#include <frmatr.hxx>
#include <extinput.hxx>
#include <acmplwrd.hxx>
#include <swcalwrp.hxx>
#include <swdtflvr.hxx>
#include <breakit.hxx>
#include <checkit.hxx>
#include <pagefrm.hxx>
#include <usrpref.hxx>

#include <helpids.h>
#include <cmdid.h>
#include <uitool.hxx>
#include <fmtfollowtextflow.hxx>
#include <toolkit/helper/vclunohelper.hxx>
#include <charfmt.hxx>
#include <numrule.hxx>
#include <pagedesc.hxx>
#include <svtools/ruler.hxx>
#include <formatclipboard.hxx>
#include <vcl/svapp.hxx>
#include <wordcountdialog.hxx>
#include <fmtfld.hxx>

#include <IMark.hxx>
#include <doc.hxx>
#include <xmloff/odffields.hxx>

#include <PostItMgr.hxx>
#include <FrameControlsManager.hxx>
#include <AnnotationWin.hxx>

#include <algorithm>
#include <vector>

#include <rootfrm.hxx>

#include <unotools/syslocaleoptions.hxx>
#include <i18nlangtag/mslangid.hxx>
#include <salhelper/singletonref.hxx>
#include <sfx2/event.hxx>
#include <memory>

#include "../../core/crsr/callnk.hxx"
#include <IDocumentOutlineNodes.hxx>
#include <ndtxt.hxx>
#include <cntfrm.hxx>
#include <txtfrm.hxx>
#include <strings.hrc>
#include <textcontentcontrol.hxx>
#include <contentcontrolbutton.hxx>

using namespace sw::mark;
using namespace ::com::sun::star;

#define SCROLL_TIMER_RETARD_LIMIT 5

/**
 * Globals
 */
static bool g_bInputLanguageSwitched = false;

// Used to draw the guide line while resizing the comment sidebar width
static tools::Rectangle aLastCommentSidebarPos;

// Usually in MouseButtonUp a selection is revoked when the selection is
// not currently being pulled open. Unfortunately in MouseButtonDown there
// is being selected at double/triple click. That selection is completely
// finished in the Handler and thus can't be distinguished in the Up.
// To resolve this g_bHoldSelection is set in Down and evaluated in Up.
static bool g_bHoldSelection      = false;

bool g_bFrameDrag                   = false;
static bool g_bValidCursorPos       = false;
bool g_bModePushed = false;
bool g_bDDTimerStarted            = false;
bool g_bDDINetAttr                = false;
static SdrHdlKind g_eSdrMoveHdl   = SdrHdlKind::User;

QuickHelpData* SwEditWin::s_pQuickHlpData = nullptr;

tools::Long    SwEditWin::s_nDDStartPosY = 0;
tools::Long    SwEditWin::s_nDDStartPosX = 0;

static SfxShell* lcl_GetTextShellFromDispatcher( SwView const & rView );

/// Check if the selected shape has a TextBox: if so, go into that instead.
static bool lcl_goIntoTextBox(SwEditWin& rEditWin, SwWrtShell& rSh)
{
    SdrMark* pMark = rSh.GetDrawView()->GetMarkedObjectList().GetMark(0);
    if (!pMark)
        return false;

    SdrObject* pSdrObject = pMark->GetMarkedSdrObj();
    SwFrameFormat* pObjectFormat = ::FindFrameFormat(pSdrObject);
    if (SwFrameFormat* pTextBoxFormat = SwTextBoxHelper::getOtherTextBoxFormat(pObjectFormat, RES_DRAWFRMFMT))
    {
        SdrObject* pTextBox = pTextBoxFormat->FindRealSdrObject();
        SdrView* pSdrView = rSh.GetDrawView();
        // Unmark the shape.
        pSdrView->UnmarkAllObj();
        // Mark the textbox.
        rSh.SelectObj(Point(), SW_ALLOW_TEXTBOX, pTextBox);
        // Clear the DrawFuncPtr.
        rEditWin.StopInsFrame();
        return true;
    }
    return false;
}

class SwAnchorMarker
{
    SdrHdl* m_pHdl;
    Point m_aHdlPos;
    Point m_aLastPos;
    bool m_bTopRightHandle;
public:
    explicit SwAnchorMarker( SdrHdl* pH )
        : m_pHdl( pH )
        , m_aHdlPos( pH->GetPos() )
        , m_aLastPos( pH->GetPos() )
        , m_bTopRightHandle( pH->GetKind() == SdrHdlKind::Anchor_TR )
    {}
    const Point& GetLastPos() const { return m_aLastPos; }
    void SetLastPos( const Point& rNew ) { m_aLastPos = rNew; }
    void SetPos( const Point& rNew ) { m_pHdl->SetPos( rNew ); }
    const Point& GetHdlPos() const { return m_aHdlPos; }
    SdrHdl* GetHdl() const { return m_pHdl; }
    void ChgHdl( SdrHdl* pNew )
    {
        m_pHdl = pNew;
        if ( m_pHdl )
        {
            m_bTopRightHandle = (m_pHdl->GetKind() == SdrHdlKind::Anchor_TR);
        }
    }
    Point GetPosForHitTest( const OutputDevice& rOut )
    {
        Point aHitTestPos( m_pHdl->GetPos() );
        aHitTestPos = rOut.LogicToPixel( aHitTestPos );
        if ( m_bTopRightHandle )
        {
            aHitTestPos += Point( -1, 1 );
        }
        else
        {
            aHitTestPos += Point( 1, 1 );
        }
        aHitTestPos = rOut.PixelToLogic( aHitTestPos );

        return aHitTestPos;
    }
};

/// Assists with auto-completion of AutoComplete words and AutoText names.
struct QuickHelpData
{
    /// Strings that at least partially match an input word, and match length.
    std::vector<std::pair<OUString, sal_uInt16>> m_aHelpStrings;
    /// Index of the current help string.
    sal_uInt16 nCurArrPos;
    static constexpr sal_uInt16 nNoPos = std::numeric_limits<sal_uInt16>::max();

    /// Help data stores AutoText names rather than AutoComplete words.
    bool m_bIsAutoText;
    /// Display help string as a tip rather than inline.
    bool m_bIsTip;
    /// Tip ID when a help string is displayed as a tip.
    void* nTipId;
    /// Append a space character to the displayed help string (if appropriate).
    bool m_bAppendSpace;

    /// Help string is currently displayed.
    bool m_bIsDisplayed;

    QuickHelpData() { ClearContent(); }

    void Move( QuickHelpData& rCpy );
    void ClearContent();
    void Start(SwWrtShell& rSh, bool bRestart);
    void Stop( SwWrtShell& rSh );

    bool HasContent() const { return !m_aHelpStrings.empty() && nCurArrPos != nNoPos; }
    const OUString& CurStr() const { return m_aHelpStrings[nCurArrPos].first; }
    sal_uInt16 CurLen() const { return m_aHelpStrings[nCurArrPos].second; }

    /// Next help string.
    void Next( bool bEndLess )
    {
        if( ++nCurArrPos >= m_aHelpStrings.size() )
            nCurArrPos = (bEndLess && !m_bIsAutoText ) ? 0 : nCurArrPos-1;
    }
    /// Previous help string.
    void Previous( bool bEndLess )
    {
        if( 0 == nCurArrPos-- )
            nCurArrPos = (bEndLess && !m_bIsAutoText ) ? m_aHelpStrings.size()-1 : 0;
    }

    // Fills internal structures with hopefully helpful information.
    void FillStrArr( SwWrtShell const & rSh, const OUString& rWord );
    void SortAndFilter(const OUString &rOrigWord);
};


// Minimise jitter

constexpr auto HIT_PIX = 2;  // Hit tolerance in pixels
constexpr auto MIN_MOVE = 4;

static bool IsMinMove(const Point &rStartPos, const Point &rLPt)
{
    return std::abs(rStartPos.X() - rLPt.X()) > MIN_MOVE ||
           std::abs(rStartPos.Y() - rLPt.Y()) > MIN_MOVE;
}

/**
 * For MouseButtonDown - determine whether a DrawObject
 * a NO SwgFrame was hit! Shift/Ctrl should only result
 * in selecting, with DrawObjects; at SwgFlys to trigger
 * hyperlinks if applicable (Download/NewWindow!)
 */
static bool IsDrawObjSelectable( const SwWrtShell& rSh, const Point& rPt )
{
    bool bRet = true;
    SdrObject* pObj;
    switch( rSh.GetObjCntType( rPt, pObj ))
    {
    case OBJCNT_NONE:
    case OBJCNT_FLY:
    case OBJCNT_GRF:
    case OBJCNT_OLE:
        bRet = false;
        break;
    default:; //prevent warning
    }
    return bRet;
}

/*
 * Switch pointer
 */
void SwEditWin::UpdatePointer(const Point &rLPt, sal_uInt16 nModifier )
{
    SetQuickHelpText(OUString());
    SwWrtShell &rSh = m_rView.GetWrtShell();
    if( m_pApplyTempl )
    {
        PointerStyle eStyle = PointerStyle::Fill;
        if ( rSh.IsOverReadOnlyPos( rLPt ) )
        {
            m_pUserMarker.reset();

            eStyle = PointerStyle::NotAllowed;
        }
        else
        {
            SwRect aRect;
            SwRect* pRect = &aRect;
            const SwFrameFormat* pFormat = nullptr;

            bool bFrameIsValidTarget = false;
            if( m_pApplyTempl->m_pFormatClipboard )
                bFrameIsValidTarget = m_pApplyTempl->m_pFormatClipboard->HasContentForThisType( SelectionType::Frame );
            else if( !m_pApplyTempl->nColor )
                bFrameIsValidTarget = ( m_pApplyTempl->eType == SfxStyleFamily::Frame );

            if( bFrameIsValidTarget &&
                        nullptr !=(pFormat = rSh.GetFormatFromObj( rLPt, &pRect )) &&
                        dynamic_cast<const SwFlyFrameFormat*>( pFormat) )
            {
                //turn on highlight for frame
                tools::Rectangle aTmp( pRect->SVRect() );

                if ( !m_pUserMarker )
                {
                    m_pUserMarker.reset(new SdrDropMarkerOverlay( *rSh.GetDrawView(), aTmp ));
                }
            }
            else
            {
                m_pUserMarker.reset();
            }

            rSh.SwCursorShell::SetVisibleCursor( rLPt );
        }
        SetPointer( eStyle );
        return;
    }

    if( !rSh.VisArea().Width() )
        return;

    CurrShell aCurr(&rSh);

    if ( IsChainMode() )
    {
        SwRect aRect;
        SwChainRet nChainable = rSh.Chainable( aRect, *rSh.GetFlyFrameFormat(), rLPt );
        PointerStyle eStyle = nChainable != SwChainRet::OK
                ? PointerStyle::ChainNotAllowed : PointerStyle::Chain;
        if ( nChainable == SwChainRet::OK )
        {
            tools::Rectangle aTmp( aRect.SVRect() );

            if ( !m_pUserMarker )
            {
                m_pUserMarker.reset(new SdrDropMarkerOverlay( *rSh.GetDrawView(), aTmp ));
            }
        }
        else
        {
            m_pUserMarker.reset();
        }

        SetPointer( eStyle );
        return;
    }

    bool bExecHyperlinks = m_rView.GetDocShell()->IsReadOnly();
    if ( !bExecHyperlinks )
    {
        const bool bSecureOption = SvtSecurityOptions::IsOptionSet( SvtSecurityOptions::EOption::CtrlClickHyperlink );
        if ( (  bSecureOption && nModifier == KEY_MOD1 ) ||
             ( !bSecureOption && nModifier != KEY_MOD1 ) )
            bExecHyperlinks = true;
    }

    const bool bExecSmarttags  = nModifier == KEY_MOD1;

    SdrView *pSdrView = rSh.GetDrawView();
    bool bPrefSdrPointer = false;
    bool bHitHandle = false;
    bool bCntAtPos = false;
    bool bIsViewReadOnly = IsViewReadonly();

    m_aActHitType = SdrHitKind::NONE;
    PointerStyle eStyle = PointerStyle::Text;
    if ( !pSdrView )
        bCntAtPos = true;
    else if ( (bHitHandle = (pSdrView->PickHandle(rLPt) != nullptr)) )
    {
        m_aActHitType = SdrHitKind::Object;
        bPrefSdrPointer = true;
    }
    else
    {
        const bool bNotInSelObj = !rSh.IsInsideSelectedObj( rLPt );
        if ( m_rView.GetDrawFuncPtr() && !m_bInsDraw && bNotInSelObj )
        {
            m_aActHitType = SdrHitKind::Object;
            if (IsObjectSelect())
                eStyle = PointerStyle::Arrow;
            else
                bPrefSdrPointer = true;
        }
        else
        {
            SdrPageView* pPV = nullptr;
            pSdrView->SetHitTolerancePixel( HIT_PIX );
            SdrObject* pObj  = (bNotInSelObj && bExecHyperlinks) ?
                 pSdrView->PickObj(rLPt, pSdrView->getHitTolLog(), pPV, SdrSearchOptions::PICKMACRO) :
                 nullptr;
            if (pObj)
            {
                SdrObjMacroHitRec aTmp;
                aTmp.aPos = rLPt;
                aTmp.pPageView = pPV;
                SetPointer( pObj->GetMacroPointer( aTmp ) );
                return;
            }
            else
            {
                // dvo: IsObjSelectable() eventually calls SdrView::PickObj, so
                // apparently this is used to determine whether this is a
                // drawling layer object or not.
                if ( rSh.IsObjSelectable( rLPt ) )
                {
                    if (pSdrView->IsTextEdit())
                    {
                        m_aActHitType = SdrHitKind::NONE;
                        bPrefSdrPointer = true;
                    }
                    else
                    {
                        SdrViewEvent aVEvt;
                        SdrHitKind eHit = pSdrView->PickAnything(rLPt, aVEvt);

                        if (eHit == SdrHitKind::UrlField && bExecHyperlinks)
                        {
                            m_aActHitType = SdrHitKind::Object;
                            bPrefSdrPointer = true;
                        }
                        else
                        {
                            // if we're over a selected object, we show an
                            // ARROW by default. We only show a MOVE if 1) the
                            // object is selected, and 2) it may be moved
                            // (i.e., position is not protected).
                            bool bMovable =
                                (!bNotInSelObj) &&
                                (rSh.GetSelectedObjCount() || rSh.IsFrameSelected()) &&
                                (rSh.IsSelObjProtected(FlyProtectFlags::Pos) == FlyProtectFlags::NONE);

                            SdrObject* pSelectableObj = rSh.GetObjAt(rLPt);
                            // Don't update pointer if this is a background image only.
                            if (pSelectableObj->GetLayer() != rSh.GetDoc()->getIDocumentDrawModelAccess().GetHellId())
                                eStyle = bMovable ? PointerStyle::Move : PointerStyle::Arrow;
                            m_aActHitType = SdrHitKind::Object;
                        }
                    }
                }
                else
                {
                    if ( rSh.IsFrameSelected() && !bNotInSelObj )
                    {
                        // dvo: this branch appears to be dead and should be
                        // removed in a future version. Reason: The condition
                        // !bNotInSelObj means that this branch will only be
                        // executed in the cursor points inside a selected
                        // object. However, if this is the case, the previous
                        // if( rSh.IsObjSelectable(rLPt) ) must always be true:
                        // rLPt is inside a selected object, then obviously
                        // rLPt is over a selectable object.
                        if (rSh.IsSelObjProtected(FlyProtectFlags::Size) != FlyProtectFlags::NONE)
                            eStyle = PointerStyle::NotAllowed;
                        else
                            eStyle = PointerStyle::Move;
                        m_aActHitType = SdrHitKind::Object;
                    }
                    else
                    {
                        if ( m_rView.GetDrawFuncPtr() )
                            bPrefSdrPointer = true;
                        else
                            bCntAtPos = true;
                    }
                }
            }
        }
    }
    if ( bPrefSdrPointer )
    {
        if (bIsViewReadOnly || (rSh.GetSelectedObjCount() && rSh.IsSelObjProtected(FlyProtectFlags::Content) != FlyProtectFlags::NONE))
            SetPointer( PointerStyle::NotAllowed );
        else
        {
            if (m_rView.GetDrawFuncPtr() && m_rView.GetDrawFuncPtr()->IsInsertForm() && !bHitHandle)
                SetPointer( PointerStyle::DrawRect );
            else
                SetPointer( pSdrView->GetPreferredPointer( rLPt, rSh.GetOut() ) );
        }
    }
    else
    {
        if( !rSh.IsPageAtPos( rLPt ) || m_pAnchorMarker )
            eStyle = PointerStyle::Arrow;
        else
        {
            // Even if we already have something, prefer URLs if possible.
            SwContentAtPos aUrlPos(IsAttrAtPos::InetAttr);
            if (bCntAtPos || rSh.GetContentAtPos(rLPt, aUrlPos))
            {
                SwContentAtPos aSwContentAtPos(
                    IsAttrAtPos::Field |
                    IsAttrAtPos::ClickField |
                    IsAttrAtPos::InetAttr |
                    IsAttrAtPos::Footnote |
                    IsAttrAtPos::SmartTag);
                if( rSh.GetContentAtPos( rLPt, aSwContentAtPos) )
                {
                    // Is edit inline input field
                    if (IsAttrAtPos::Field == aSwContentAtPos.eContentAtPos
                             && aSwContentAtPos.pFndTextAttr != nullptr
                             && aSwContentAtPos.pFndTextAttr->Which() == RES_TXTATR_INPUTFIELD)
                    {
                        const SwField *pCursorField = rSh.CursorInsideInputField() ? rSh.GetCurField( true ) : nullptr;
                        if (!(pCursorField && pCursorField == aSwContentAtPos.pFndTextAttr->GetFormatField().GetField()))
                            eStyle = PointerStyle::RefHand;
                    }
                    else
                    {
                        const bool bClickToFollow = IsAttrAtPos::InetAttr == aSwContentAtPos.eContentAtPos ||
                                                    IsAttrAtPos::SmartTag == aSwContentAtPos.eContentAtPos;
                        if( !bClickToFollow ||
                            (IsAttrAtPos::InetAttr == aSwContentAtPos.eContentAtPos && bExecHyperlinks) ||
                            (IsAttrAtPos::SmartTag == aSwContentAtPos.eContentAtPos && bExecSmarttags) )
                            eStyle = PointerStyle::RefHand;
                    }
                }
                else if (GetView().GetWrtShell().GetViewOptions()->IsShowOutlineContentVisibilityButton())
                {
                    aSwContentAtPos.eContentAtPos = IsAttrAtPos::Outline;
                    if (rSh.GetContentAtPos(rLPt, aSwContentAtPos))
                    {
                        if (IsAttrAtPos::Outline == aSwContentAtPos.eContentAtPos)
                        {
                            if (nModifier == KEY_MOD1)
                            {
                                eStyle = PointerStyle::RefHand;
                                // set quick help
                                if(aSwContentAtPos.aFnd.pNode && aSwContentAtPos.aFnd.pNode->IsTextNode())
                                {
                                    const SwNodes& rNds = GetView().GetWrtShell().GetDoc()->GetNodes();
                                    SwOutlineNodes::size_type nPos;
                                    rNds.GetOutLineNds().Seek_Entry(aSwContentAtPos.aFnd.pNode->GetTextNode(), &nPos);
                                    SwOutlineNodes::size_type nOutlineNodesCount
                                            = rSh.getIDocumentOutlineNodesAccess()->getOutlineNodesCount();
                                    int nLevel = rSh.getIDocumentOutlineNodesAccess()->getOutlineLevel(nPos);
                                    OUString sQuickHelp(SwResId(STR_CLICK_OUTLINE_CONTENT_TOGGLE_VISIBILITY));
                                    if (!rSh.GetViewOptions()->IsTreatSubOutlineLevelsAsContent()
                                            && nPos + 1 < nOutlineNodesCount
                                            && rSh.getIDocumentOutlineNodesAccess()->getOutlineLevel(nPos + 1) > nLevel)
                                        sQuickHelp += " (" + SwResId(STR_CLICK_OUTLINE_CONTENT_TOGGLE_VISIBILITY_EXT) + ")";
                                    SetQuickHelpText(sQuickHelp);
                                }
                            }
                        }
                    }
                }
            }
        }

        // which kind of text pointer have we to show - horz / vert - ?
        if( PointerStyle::Text == eStyle && rSh.IsInVerticalText( &rLPt ))
            eStyle = PointerStyle::TextVertical;
        else if (rSh.GetViewOptions()->CanHideWhitespace() &&
                 rSh.GetLayout()->IsBetweenPages(rLPt))
        {
            if (rSh.GetViewOptions()->IsHideWhitespaceMode())
                eStyle = PointerStyle::ShowWhitespace;
            else
                eStyle = PointerStyle::HideWhitespace;
        }

        if( m_pShadCursor )
        {
            if( text::HoriOrientation::LEFT == m_eOrient )    // Arrow to the right
                eStyle = PointerStyle::AutoScrollE;
            else    // Arrow to the left
                eStyle = PointerStyle::AutoScrollW;
        }

        SetPointer( eStyle );
    }
}

/**
 * Increase timer for selection
 */
IMPL_LINK_NOARG(SwEditWin, TimerHandler, Timer *, void)
{
    ++m_nTimerCalls;
    SwWrtShell &rSh = m_rView.GetWrtShell();
    Point aModPt( m_aMovePos );
    const SwRect aOldVis( rSh.VisArea() );
    bool bDone = false;

    if ( !rSh.VisArea().Contains( aModPt ) )
    {
        if ( m_bInsDraw )
        {
            const int nMaxScroll = 40;
            m_rView.Scroll( tools::Rectangle(aModPt,Size(1,1)), nMaxScroll, nMaxScroll);
            bDone = true;
        }
        else if ( g_bFrameDrag )
        {
            rSh.Drag(&aModPt, false);
            bDone = true;
        }
        if (!bDone)
        {
            bool bForward = aModPt.Y() > rSh.VisArea().Bottom();
            if (m_xRowColumnSelectionStart)
                aModPt = rSh.GetContentPos( aModPt, bForward );
            else
            {
                sal_Int32 nMove = (aOldVis.Bottom() - aOldVis.Top()) / 20;
                if (bForward)
                    aModPt.setY(aOldVis.Bottom() + nMove);
                else
                    aModPt.setY(aOldVis.Top() - nMove);
            }
        }
    }
    if ( !bDone && !(g_bFrameDrag || m_bInsDraw) )
    {
        if ( m_xRowColumnSelectionStart )
        {
            Point aPos( aModPt );
            rSh.SelectTableRowCol( *m_xRowColumnSelectionStart, &aPos, m_bIsRowDrag );
        }
        else
            rSh.CallSetCursor( &aModPt, true, m_eScrollSizeMode );

        // It can be that a "jump" over a table cannot be accomplished like
        // that. So we jump over the table by Up/Down here.
        const SwRect& rVisArea = rSh.VisArea();
        if( aOldVis == rVisArea && !rSh.IsStartOfDoc() && !rSh.IsEndOfDoc() )
        {
            // take the center point of VisArea to
            // decide in which direction the user want.
            if( aModPt.Y() < ( rVisArea.Top() + rVisArea.Height() / 2 ) )
                rSh.Up( true );
            else
                rSh.Down( true );
        }
    }

    m_aMovePos += rSh.VisArea().Pos() - aOldVis.Pos();
    JustifyAreaTimer();
}

void SwEditWin::JustifyAreaTimer(bool bStart)
{
    if (bStart)
        m_nTimerCalls = 0;
    const tools::Rectangle &rVisArea = GetView().GetVisArea();
#ifdef UNX
    const tools::Long coMinLen = 40;
#else
    const tools::Long coMinLen = 20;
#endif
    tools::Long const nTimeout = 800,
         nDiff = std::max(
         std::max( m_aMovePos.Y() - rVisArea.Bottom(), rVisArea.Top() - m_aMovePos.Y() ),
         std::max( m_aMovePos.X() - rVisArea.Right(),  rVisArea.Left() - m_aMovePos.X()));
    if (m_nTimerCalls < SCROLL_TIMER_RETARD_LIMIT)
    {
        m_aTimer.SetTimeout(nTimeout);
        m_eScrollSizeMode = ScrollSizeMode::ScrollSizeMouseSelection;
    }
    else
    {
        m_aTimer.SetTimeout( std::max( coMinLen, nTimeout - nDiff) );
        m_eScrollSizeMode = m_aTimer.GetTimeout() < 100 ?
            ScrollSizeMode::ScrollSizeTimer2 :
            m_aTimer.GetTimeout() < 400 ?
                ScrollSizeMode::ScrollSizeTimer :
                ScrollSizeMode::ScrollSizeMouseSelection;
    }
}

void SwEditWin::LeaveArea(const Point &rPos)
{
    m_aMovePos = rPos;
    JustifyAreaTimer(true);
    if( !m_aTimer.IsActive() )
        m_aTimer.Start();
    m_pShadCursor.reset();
}

inline void SwEditWin::EnterArea()
{
    m_aTimer.Stop();
}

/**
 * Insert mode for frames
 */
void SwEditWin::InsFrame(sal_uInt16 nCols)
{
    StdDrawMode(SdrObjKind::NewFrame, false);
    m_bInsFrame = true;
    m_nInsFrameColCount = nCols;
}

void SwEditWin::StdDrawMode( SdrObjKind eSdrObjectKind, bool bObjSelect )
{
    SetSdrDrawMode( eSdrObjectKind );

    if (bObjSelect)
        m_rView.SetDrawFuncPtr(std::make_unique<DrawSelection>( &m_rView.GetWrtShell(), this, m_rView ));
    else
        m_rView.SetDrawFuncPtr(std::make_unique<SwDrawBase>( &m_rView.GetWrtShell(), this, m_rView ));

    m_rView.SetSelDrawSlot();
    SetSdrDrawMode( eSdrObjectKind );
    if (bObjSelect)
        m_rView.GetDrawFuncPtr()->Activate( SID_OBJECT_SELECT );
    else
        m_rView.GetDrawFuncPtr()->Activate( sal::static_int_cast< sal_uInt16 >(eSdrObjectKind) );
    m_bInsFrame = false;
    m_nInsFrameColCount = 1;
}

void SwEditWin::StopInsFrame()
{
    if (m_rView.GetDrawFuncPtr())
    {
        m_rView.GetDrawFuncPtr()->Deactivate();
        m_rView.SetDrawFuncPtr(nullptr);
    }
    m_rView.LeaveDrawCreate();    // leave construction mode
    m_bInsFrame = false;
    m_nInsFrameColCount = 1;
}

bool SwEditWin::IsInputSequenceCheckingRequired( const OUString &rText, const SwPaM& rCursor )
{
    if ( !SvtCTLOptions::IsCTLFontEnabled() ||
         !SvtCTLOptions::IsCTLSequenceChecking() )
         return false;

    if ( 0 == rCursor.Start()->GetContentIndex() ) /* first char needs not to be checked */
        return false;

    SwBreakIt *pBreakIter = SwBreakIt::Get();
    uno::Reference < i18n::XBreakIterator > xBI = pBreakIter->GetBreakIter();
    assert(xBI.is());
    tools::Long nCTLScriptPos = -1;

    if (xBI->getScriptType( rText, 0 ) == i18n::ScriptType::COMPLEX)
        nCTLScriptPos = 0;
    else
        nCTLScriptPos = xBI->nextScript( rText, 0, i18n::ScriptType::COMPLEX );

    return (0 <= nCTLScriptPos && nCTLScriptPos <= rText.getLength());
}

//return INVALID_HINT if language should not be explicitly overridden, the correct
//HintId to use for the eBufferLanguage otherwise
static sal_uInt16 lcl_isNonDefaultLanguage(LanguageType eBufferLanguage, SwView const & rView,
    const OUString &rInBuffer)
{
    sal_uInt16 nWhich = INVALID_HINT;

    //If the option to IgnoreLanguageChange is set, short-circuit this method
    //which results in the document/paragraph language remaining the same
    //despite a change to the keyboard/input language
    SvtSysLocaleOptions aSysLocaleOptions;
    if(aSysLocaleOptions.IsIgnoreLanguageChange())
    {
        return INVALID_HINT;
    }

    bool bLang = true;
    if(eBufferLanguage != LANGUAGE_DONTKNOW)
    {
        switch( SvtLanguageOptions::GetI18NScriptTypeOfLanguage( eBufferLanguage ))
        {
            case  i18n::ScriptType::ASIAN:     nWhich = RES_CHRATR_CJK_LANGUAGE; break;
            case  i18n::ScriptType::COMPLEX:   nWhich = RES_CHRATR_CTL_LANGUAGE; break;
            case  i18n::ScriptType::LATIN:     nWhich = RES_CHRATR_LANGUAGE; break;
            default: bLang = false;
        }
        if(bLang)
        {
            SfxItemSet aLangSet(rView.GetPool(), nWhich, nWhich);
            SwWrtShell& rSh = rView.GetWrtShell();
            rSh.GetCurAttr(aLangSet);
            if(SfxItemState::DEFAULT <= aLangSet.GetItemState(nWhich))
            {
                LanguageType eLang = static_cast<const SvxLanguageItem&>(aLangSet.Get(nWhich)).GetLanguage();
                if ( eLang == eBufferLanguage )
                {
                    // current language attribute equal to language reported from system
                    bLang = false;
                }
                else if ( !g_bInputLanguageSwitched && RES_CHRATR_LANGUAGE == nWhich )
                {
                    // special case: switching between two "LATIN" languages
                    // In case the current keyboard setting might be suitable
                    // for both languages we can't safely assume that the user
                    // wants to use the language reported from the system,
                    // except if we knew that it was explicitly switched (thus
                    // the check for "bInputLangeSwitched").

                    // The language reported by the system could be just the
                    // system default language that the user is not even aware
                    // of, because no language selection tool is installed at
                    // all. In this case the OOo language should get preference
                    // as it might have been selected by the user explicitly.

                    // Usually this case happens if the OOo language is
                    // different to the system language but the system keyboard
                    // is still suitable for the OOo language (e.g. writing
                    // English texts with a German keyboard).

                    // For non-latin keyboards overwriting the attribute is
                    // still valid. We do this for cyrillic and greek ATM.  In
                    // future versions of OOo this should be replaced by a
                    // configuration switch that allows to give the preference
                    // to the OOo setting or the system setting explicitly
                    // and/or a better handling of the script type.
                    i18n::UnicodeScript eType = !rInBuffer.isEmpty() ?
                        GetAppCharClass().getScript( rInBuffer, 0 ) :
                        i18n::UnicodeScript_kScriptCount;

                    bool bSystemIsNonLatin = false;
                    switch ( eType )
                    {
                        case i18n::UnicodeScript_kGreek:
                        case i18n::UnicodeScript_kCyrillic:
                            // in case other UnicodeScripts require special
                            // keyboards they can be added here
                            bSystemIsNonLatin = true;
                            break;
                        default:
                            break;
                    }

                    bool bOOoLangIsNonLatin = MsLangId::isNonLatinWestern( eLang);

                    bLang = (bSystemIsNonLatin != bOOoLangIsNonLatin);
                }
            }
        }
    }
    return bLang ? nWhich : INVALID_HINT;
}

/**
 * Character buffer is inserted into the document
 */
void SwEditWin::FlushInBuffer()
{
    if ( m_aKeyInputFlushTimer.IsActive())
        m_aKeyInputFlushTimer.Stop();

    if ( m_aInBuffer.isEmpty() )
        return;

    SwWrtShell& rSh = m_rView.GetWrtShell();
    uno::Reference<frame::XDispatchRecorder> xRecorder
        = m_rView.GetViewFrame().GetBindings().GetRecorder();

    comphelper::ScopeGuard showTooltipGuard(
        [this, &rSh]
        {
            SvxAutoCorrCfg& rACfg = SvxAutoCorrCfg::Get();
            const bool bAutoTextShown
                = rACfg.IsAutoTextTip() && ShowAutoText(rSh.GetChunkForAutoText());
            if (!bAutoTextShown)
            {
                SvxAutoCorrect* pACorr = rACfg.GetAutoCorrect();
                if (pACorr && pACorr->GetSwFlags().bAutoCompleteWords)
                    ShowAutoCorrectQuickHelp(rSh.GetPrevAutoCorrWord(*pACorr), *pACorr);
            }
        });
    if (!m_bMaybeShowTooltipAfterBufferFlush || xRecorder)
        showTooltipGuard.dismiss();
    m_bMaybeShowTooltipAfterBufferFlush = false;

    // generate new sequence input checker if not already done
    if ( !pCheckIt )
        pCheckIt = new SwCheckIt;

    uno::Reference < i18n::XExtendedInputSequenceChecker > xISC = pCheckIt->xCheck;
    if ( xISC.is() && IsInputSequenceCheckingRequired( m_aInBuffer, *rSh.GetCursor() ) )
    {

        // apply (Thai) input sequence checking/correction

        rSh.Push(); // push current cursor to stack

        // get text from the beginning (i.e left side) of current selection
        // to the start of the paragraph
        rSh.NormalizePam();     // make point be the first (left) one
        if (!rSh.GetCursor()->HasMark())
            rSh.GetCursor()->SetMark();
        rSh.GetCursor()->GetMark()->SetContent(0);

        const OUString aOldText( rSh.GetCursor()->GetText() );
        const sal_Int32 nOldLen = aOldText.getLength();

        sal_Int32 nExpandSelection = 0;
        if (nOldLen > 0)
        {
            sal_Int32 nTmpPos = nOldLen;
            sal_Int16 nCheckMode = SvtCTLOptions::IsCTLSequenceCheckingRestricted() ?
                    i18n::InputSequenceCheckMode::STRICT : i18n::InputSequenceCheckMode::BASIC;

            OUString aNewText( aOldText );
            if (SvtCTLOptions::IsCTLSequenceCheckingTypeAndReplace())
            {
                for( sal_Int32 k = 0;  k < m_aInBuffer.getLength();  ++k)
                {
                    const sal_Unicode cChar = m_aInBuffer[k];
                    const sal_Int32 nPrevPos =xISC->correctInputSequence( aNewText, nTmpPos - 1, cChar, nCheckMode );

                    // valid sequence or sequence could be corrected:
                    if (nPrevPos != aNewText.getLength())
                        nTmpPos = nPrevPos + 1;
                }

                // find position of first character that has changed
                sal_Int32 nNewLen = aNewText.getLength();
                const sal_Unicode *pOldText = aOldText.getStr();
                const sal_Unicode *pNewText = aNewText.getStr();
                sal_Int32 nChgPos = 0;
                while ( nChgPos < nOldLen && nChgPos < nNewLen &&
                        pOldText[nChgPos] == pNewText[nChgPos] )
                    ++nChgPos;

                const sal_Int32 nChgLen = nNewLen - nChgPos;
                if (nChgLen)
                {
                    m_aInBuffer = aNewText.copy( nChgPos, nChgLen );
                    nExpandSelection = nOldLen - nChgPos;
                }
                else
                    m_aInBuffer.clear();
            }
            else
            {
                for( sal_Int32 k = 0;  k < m_aInBuffer.getLength(); ++k )
                {
                    const sal_Unicode cChar = m_aInBuffer[k];
                    if (xISC->checkInputSequence( aNewText, nTmpPos - 1, cChar, nCheckMode ))
                    {
                        // character can be inserted:
                        aNewText += OUStringChar( cChar );
                        ++nTmpPos;
                    }
                }
                m_aInBuffer = aNewText.copy( aOldText.getLength() );  // copy new text to be inserted to buffer
            }
        }

        // at this point now we will insert the buffer text 'normally' some lines below...

        rSh.Pop(SwCursorShell::PopMode::DeleteCurrent);

        if (m_aInBuffer.isEmpty())
            return;

        // if text prior to the original selection needs to be changed
        // as well, we now expand the selection accordingly.
        SwPaM &rCursor = *rSh.GetCursor();
        const sal_Int32 nCursorStartPos = rCursor.Start()->GetContentIndex();
        OSL_ENSURE( nCursorStartPos >= nExpandSelection, "cannot expand selection as specified!!" );
        if (nExpandSelection && nCursorStartPos >= nExpandSelection)
        {
            if (!rCursor.HasMark())
                rCursor.SetMark();
            rCursor.Start()->AdjustContent( -nExpandSelection );
        }
    }

    if ( xRecorder.is() )
    {
        // determine shell
        SfxShell *pSfxShell = lcl_GetTextShellFromDispatcher( m_rView );
        // generate request and record
        if (pSfxShell)
        {
            SfxRequest aReq(m_rView.GetViewFrame(), FN_INSERT_STRING);
            aReq.AppendItem( SfxStringItem( FN_INSERT_STRING, m_aInBuffer ) );
            aReq.Done();
        }
    }

    sal_uInt16 nWhich = lcl_isNonDefaultLanguage(m_eBufferLanguage, m_rView, m_aInBuffer);
    if (nWhich != INVALID_HINT )
    {
        SvxLanguageItem aLangItem( m_eBufferLanguage, nWhich );
        rSh.SetAttrItem( aLangItem );
    }

    rSh.Insert( m_aInBuffer );
    m_eBufferLanguage = LANGUAGE_DONTKNOW;
    m_aInBuffer.clear();
}

#define MOVE_LEFT_SMALL     0
#define MOVE_UP_SMALL       1
#define MOVE_RIGHT_BIG      2
#define MOVE_DOWN_BIG       3
#define MOVE_LEFT_BIG       4
#define MOVE_UP_BIG         5
#define MOVE_RIGHT_SMALL    6
#define MOVE_DOWN_SMALL     7

// #i121236# Support for shift key in writer
#define MOVE_LEFT_HUGE      8
#define MOVE_UP_HUGE        9
#define MOVE_RIGHT_HUGE     10
#define MOVE_DOWN_HUGE      11

void SwEditWin::ChangeFly( sal_uInt8 nDir, bool bWeb )
{
    SwWrtShell &rSh = m_rView.GetWrtShell();
    SwRect aTmp = rSh.GetFlyRect();
    if( !aTmp.HasArea() ||
        rSh.IsSelObjProtected( FlyProtectFlags::Pos ) != FlyProtectFlags::NONE )
        return;

    SfxItemSetFixed<
            RES_FRM_SIZE, RES_FRM_SIZE,
            RES_PROTECT, RES_PROTECT,
            RES_VERT_ORIENT, RES_ANCHOR,
            RES_COL, RES_COL,
            RES_FOLLOW_TEXT_FLOW, RES_FOLLOW_TEXT_FLOW>
        aSet( rSh.GetAttrPool() );
    rSh.GetFlyFrameAttr( aSet );
    RndStdIds eAnchorId = aSet.Get(RES_ANCHOR).GetAnchorId();
    Size aSnap;
    bool bHuge(MOVE_LEFT_HUGE == nDir ||
        MOVE_UP_HUGE == nDir ||
        MOVE_RIGHT_HUGE == nDir ||
        MOVE_DOWN_HUGE == nDir);

    if(MOVE_LEFT_SMALL == nDir ||
        MOVE_UP_SMALL == nDir ||
        MOVE_RIGHT_SMALL == nDir ||
        MOVE_DOWN_SMALL == nDir )
    {
        aSnap = PixelToLogic(Size(1,1));
    }
    else
    {
        aSnap = rSh.GetViewOptions()->GetSnapSize();
        short nDiv = rSh.GetViewOptions()->GetDivisionX();
        if ( nDiv > 0 )
            aSnap.setWidth( std::max( sal_uLong(1), static_cast<sal_uLong>(aSnap.Width()) / nDiv ) );
        nDiv = rSh.GetViewOptions()->GetDivisionY();
        if ( nDiv > 0 )
            aSnap.setHeight( std::max( sal_uLong(1), static_cast<sal_uLong>(aSnap.Height()) / nDiv ) );
    }

    if(bHuge)
    {
        // #i121236# 567twips == 1cm, but just take three times the normal snap
        aSnap = Size(aSnap.Width() * 3, aSnap.Height() * 3);
    }

    SwRect aBoundRect;
    Point aRefPoint;
    // adjustment for allowing vertical position
    // aligned to page for fly frame anchored to paragraph or to character.
    {
        const SwFormatVertOrient& aVert( aSet.Get(RES_VERT_ORIENT) );
        const bool bFollowTextFlow =
                aSet.Get(RES_FOLLOW_TEXT_FLOW).GetValue();
        const SwFormatAnchor& rFormatAnchor = aSet.Get(RES_ANCHOR);
        rSh.CalcBoundRect( aBoundRect, eAnchorId,
                           text::RelOrientation::FRAME, aVert.GetRelationOrient(),
                           &rFormatAnchor, bFollowTextFlow,
                           false, &aRefPoint );
    }
    tools::Long nLeft = std::min( aTmp.Left() - aBoundRect.Left(), aSnap.Width() );
    tools::Long nRight = std::min( aBoundRect.Right() - aTmp.Right(), aSnap.Width() );
    tools::Long nUp = std::min( aTmp.Top() - aBoundRect.Top(), aSnap.Height() );
    tools::Long nDown = std::min( aBoundRect.Bottom() - aTmp.Bottom(), aSnap.Height() );

    switch ( nDir )
    {
        case MOVE_LEFT_BIG:
        case MOVE_LEFT_HUGE:
        case MOVE_LEFT_SMALL: aTmp.Left( aTmp.Left() - nLeft );
            break;

        case MOVE_UP_BIG:
        case MOVE_UP_HUGE:
        case MOVE_UP_SMALL: aTmp.Top( aTmp.Top() - nUp );
            break;

        case MOVE_RIGHT_SMALL:
            if( aTmp.Width() < aSnap.Width() + MINFLY )
                break;
            nRight = aSnap.Width();
            [[fallthrough]];
        case MOVE_RIGHT_HUGE:
        case MOVE_RIGHT_BIG: aTmp.Left( aTmp.Left() + nRight );
            break;

        case MOVE_DOWN_SMALL:
            if( aTmp.Height() < aSnap.Height() + MINFLY )
                break;
            nDown = aSnap.Height();
            [[fallthrough]];
        case MOVE_DOWN_HUGE:
        case MOVE_DOWN_BIG: aTmp.Top( aTmp.Top() + nDown );
            break;

        default: OSL_ENSURE(true, "ChangeFly: Unknown direction." );
    }
    bool bSet = false;
    if ((RndStdIds::FLY_AS_CHAR == eAnchorId) && ( nDir % 2 ))
    {
        tools::Long aDiff = aTmp.Top() - aRefPoint.Y();
        if( aDiff > 0 )
            aDiff = 0;
        else if ( aDiff < -aTmp.Height() )
            aDiff = -aTmp.Height();
        SwFormatVertOrient aVert( aSet.Get(RES_VERT_ORIENT) );
        sal_Int16 eNew;
        if( bWeb )
        {
            eNew = aVert.GetVertOrient();
            bool bDown = 0 != ( nDir & 0x02 );
            switch( eNew )
            {
                case text::VertOrientation::CHAR_TOP:
                    if( bDown ) eNew = text::VertOrientation::CENTER;
                break;
                case text::VertOrientation::CENTER:
                    eNew = bDown ? text::VertOrientation::TOP : text::VertOrientation::CHAR_TOP;
                break;
                case text::VertOrientation::TOP:
                    if( !bDown ) eNew = text::VertOrientation::CENTER;
                break;
                case text::VertOrientation::LINE_TOP:
                    if( bDown ) eNew = text::VertOrientation::LINE_CENTER;
                break;
                case text::VertOrientation::LINE_CENTER:
                    eNew = bDown ? text::VertOrientation::LINE_BOTTOM : text::VertOrientation::LINE_TOP;
                break;
                case text::VertOrientation::LINE_BOTTOM:
                    if( !bDown ) eNew = text::VertOrientation::LINE_CENTER;
                break;
                default:; //prevent warning
            }
        }
        else
        {
            aVert.SetPos( aDiff );
            eNew = text::VertOrientation::NONE;
        }
        aVert.SetVertOrient( eNew );
        aSet.Put( aVert );
        bSet = true;
    }
    if (bWeb && (RndStdIds::FLY_AT_PARA == eAnchorId)
        && ( nDir==MOVE_LEFT_SMALL || nDir==MOVE_RIGHT_BIG ))
    {
        SwFormatHoriOrient aHori( aSet.Get(RES_HORI_ORIENT) );
        sal_Int16 eNew;
        eNew = aHori.GetHoriOrient();
        switch( eNew )
        {
            case text::HoriOrientation::RIGHT:
                if( nDir==MOVE_LEFT_SMALL )
                    eNew = text::HoriOrientation::LEFT;
            break;
            case text::HoriOrientation::LEFT:
                if( nDir==MOVE_RIGHT_BIG )
                    eNew = text::HoriOrientation::RIGHT;
            break;
            default:; //prevent warning
        }
        if( eNew != aHori.GetHoriOrient() )
        {
            aHori.SetHoriOrient( eNew );
            aSet.Put( aHori );
            bSet = true;
        }
    }
    rSh.StartAllAction();
    if( bSet )
        rSh.SetFlyFrameAttr( aSet );
    bool bSetPos = (RndStdIds::FLY_AS_CHAR != eAnchorId);
    if(bSetPos && bWeb)
    {
        bSetPos = RndStdIds::FLY_AT_PAGE == eAnchorId;
    }
    if( bSetPos )
        rSh.SetFlyPos( aTmp.Pos() );
    rSh.EndAllAction();

}

void SwEditWin::ChangeDrawing( sal_uInt8 nDir )
{
    // start undo action in order to get only one
    // undo action for this change.
    SwWrtShell &rSh = m_rView.GetWrtShell();
    rSh.StartUndo();

    tools::Long nX = 0;
    tools::Long nY = 0;
    const bool bOnePixel(
        MOVE_LEFT_SMALL == nDir ||
        MOVE_UP_SMALL == nDir ||
        MOVE_RIGHT_SMALL == nDir ||
        MOVE_DOWN_SMALL == nDir);
    const bool bHuge(
        MOVE_LEFT_HUGE == nDir ||
        MOVE_UP_HUGE == nDir ||
        MOVE_RIGHT_HUGE == nDir ||
        MOVE_DOWN_HUGE == nDir);
    SwMove nAnchorDir = SwMove::UP;
    switch(nDir)
    {
        case MOVE_LEFT_SMALL:
        case MOVE_LEFT_HUGE:
        case MOVE_LEFT_BIG:
            nX = -1;
            nAnchorDir = SwMove::LEFT;
        break;
        case MOVE_UP_SMALL:
        case MOVE_UP_HUGE:
        case MOVE_UP_BIG:
            nY = -1;
        break;
        case MOVE_RIGHT_SMALL:
        case MOVE_RIGHT_HUGE:
        case MOVE_RIGHT_BIG:
            nX = +1;
            nAnchorDir = SwMove::RIGHT;
        break;
        case MOVE_DOWN_SMALL:
        case MOVE_DOWN_HUGE:
        case MOVE_DOWN_BIG:
            nY = +1;
            nAnchorDir = SwMove::DOWN;
        break;
    }

    if(0 != nX || 0 != nY)
    {
        FlyProtectFlags nProtect = rSh.IsSelObjProtected( FlyProtectFlags::Pos|FlyProtectFlags::Size );
        Size aSnap( rSh.GetViewOptions()->GetSnapSize() );
        short nDiv = rSh.GetViewOptions()->GetDivisionX();
        if ( nDiv > 0 )
            aSnap.setWidth( std::max( sal_uLong(1), static_cast<sal_uLong>(aSnap.Width()) / nDiv ) );
        nDiv = rSh.GetViewOptions()->GetDivisionY();
        if ( nDiv > 0 )
            aSnap.setHeight( std::max( sal_uLong(1), static_cast<sal_uLong>(aSnap.Height()) / nDiv ) );

        if(bOnePixel)
        {
            aSnap = PixelToLogic(Size(1,1));
        }
        else if(bHuge)
        {
            // #i121236# 567twips == 1cm, but just take three times the normal snap
            aSnap = Size(aSnap.Width() * 3, aSnap.Height() * 3);
        }

        nX *= aSnap.Width();
        nY *= aSnap.Height();

        SdrView *pSdrView = rSh.GetDrawView();
        const SdrHdlList& rHdlList = pSdrView->GetHdlList();
        SdrHdl* pHdl = rHdlList.GetFocusHdl();
        rSh.StartAllAction();
        if(nullptr == pHdl)
        {
            // now move the selected draw objects
            // if the object's position is not protected
            if(!(nProtect&FlyProtectFlags::Pos))
            {
                // Check if object is anchored as character and move direction
                bool bDummy1, bDummy2;
                const bool bVertAnchor = rSh.IsFrameVertical( true, bDummy1, bDummy2 );
                bool bHoriMove = !bVertAnchor == !( nDir % 2 );
                bool bMoveAllowed =
                    !bHoriMove || (rSh.GetAnchorId() != RndStdIds::FLY_AS_CHAR);
                if ( bMoveAllowed )
                {
                    pSdrView->MoveAllMarked(Size(nX, nY));
                    rSh.SetModified();
                }
            }
        }
        else
        {
            // move handle with index nHandleIndex
            if (nX || nY)
            {
                if( SdrHdlKind::Anchor == pHdl->GetKind() ||
                    SdrHdlKind::Anchor_TR == pHdl->GetKind() )
                {
                    // anchor move cannot be allowed when position is protected
                    if(!(nProtect&FlyProtectFlags::Pos))
                        rSh.MoveAnchor( nAnchorDir );
                }
                //now resize if size is protected
                else if(!(nProtect&FlyProtectFlags::Size))
                {
                    // now move the Handle (nX, nY)
                    Point aStartPoint(pHdl->GetPos());
                    Point aEndPoint(pHdl->GetPos() + Point(nX, nY));
                    const SdrDragStat& rDragStat = pSdrView->GetDragStat();

                    // start dragging
                    pSdrView->BegDragObj(aStartPoint, nullptr, pHdl, 0);

                    if(pSdrView->IsDragObj())
                    {
                        bool bWasNoSnap = rDragStat.IsNoSnap();
                        bool bWasSnapEnabled = pSdrView->IsSnapEnabled();

                        // switch snapping off
                        if(!bWasNoSnap)
                            const_cast<SdrDragStat&>(rDragStat).SetNoSnap();
                        if(bWasSnapEnabled)
                            pSdrView->SetSnapEnabled(false);

                        pSdrView->MovAction(aEndPoint);
                        pSdrView->EndDragObj();
                        rSh.SetModified();

                        // restore snap
                        if(!bWasNoSnap)
                            const_cast<SdrDragStat&>(rDragStat).SetNoSnap(bWasNoSnap);
                        if(bWasSnapEnabled)
                            pSdrView->SetSnapEnabled(bWasSnapEnabled);
                    }
                }
            }
        }
        rSh.EndAllAction();
    }

    rSh.EndUndo();
}

/**
 * KeyEvents
 */
void SwEditWin::KeyInput(const KeyEvent &rKEvt)
{
    SwWrtShell &rSh = m_rView.GetWrtShell();

    if (comphelper::LibreOfficeKit::isActive() && m_rView.GetPostItMgr())
    {
        if (vcl::Window* pWindow = m_rView.GetPostItMgr()->GetActiveSidebarWin())
        {
            pWindow->KeyInput(rKEvt);
            return;
        }
    }

    // Do not show autotext / word completion tooltips in intermediate flushes
    m_bMaybeShowTooltipAfterBufferFlush = false;

    sal_uInt16 nKey = rKEvt.GetKeyCode().GetCode();

    if (nKey == KEY_ESCAPE)
    {
        if (m_pApplyTempl && m_pApplyTempl->m_pFormatClipboard)
        {
            m_pApplyTempl->m_pFormatClipboard->Erase();
            SetApplyTemplate(SwApplyTemplate());
            m_rView.GetViewFrame().GetBindings().Invalidate(SID_FORMATPAINTBRUSH);
        }
        else if (rSh.IsHeaderFooterEdit())
        {
            bool bHeader = bool(FrameTypeFlags::HEADER & rSh.GetFrameType(nullptr, false));
            if (bHeader)
                rSh.SttPg();
            else
                rSh.EndPg();
            rSh.ToggleHeaderFooterEdit();
        }
    }

    SfxObjectShell *pObjSh = m_rView.GetViewFrame().GetObjectShell();
    if ( m_bLockInput || (pObjSh && pObjSh->GetProgress()) )
        // When the progress bar is active or a progress is
        // running on a document, no order is being taken
        return;

    m_pShadCursor.reset();
    // Do not reset the timer here, otherwise when flooded with events it would never time out
    // if every key event stopped and started it again.
    comphelper::ScopeGuard keyInputFlushTimerStop([this]() { m_aKeyInputFlushTimer.Stop(); });

    bool bIsViewReadOnly = IsViewReadonly();

    //if the language changes the buffer must be flushed
    LanguageType eNewLanguage = GetInputLanguage();
    if(!bIsViewReadOnly && m_eBufferLanguage != eNewLanguage && !m_aInBuffer.isEmpty())
    {
        FlushInBuffer();
    }
    m_eBufferLanguage = eNewLanguage;

    QuickHelpData aTmpQHD;
    if( s_pQuickHlpData->m_bIsDisplayed )
    {
        aTmpQHD.Move( *s_pQuickHlpData );
        s_pQuickHlpData->Stop( rSh );
    }

    // OS:the DrawView also needs a readonly-Flag as well
    if ( !bIsViewReadOnly && rSh.GetDrawView() && rSh.GetDrawView()->KeyInput( rKEvt, this ) )
    {
        rSh.GetView().GetViewFrame().GetBindings().InvalidateAll( false );
        rSh.SetModified();
        return; // Event evaluated by SdrView
    }

    if ( m_rView.GetDrawFuncPtr() && m_bInsFrame )
    {
        StopInsFrame();
        rSh.Edit();
    }

    bool bFlushBuffer = false;
    bool bNormalChar = false;
    bool bAppendSpace = s_pQuickHlpData->m_bAppendSpace;
    s_pQuickHlpData->m_bAppendSpace = false;

    if (nKey == KEY_F12 && getenv("SW_DEBUG"))
    {
        if( rKEvt.GetKeyCode().IsShift())
        {
            GetView().GetDocShell()->GetDoc()->dumpAsXml();
        }
        else
        {
            SwRootFrame* pLayout = GetView().GetDocShell()->GetWrtShell()->GetLayout();
            pLayout->dumpAsXml( );
        }
        return;
    }

    KeyEvent aKeyEvent( rKEvt );
    // look for vertical mappings
    if( !bIsViewReadOnly && !rSh.IsSelFrameMode() && !rSh.GetSelectedObjCount() )
    {
        if( KEY_UP == nKey || KEY_DOWN == nKey ||
            KEY_LEFT == nKey || KEY_RIGHT == nKey )
        {
            // In general, we want to map the direction keys if we are inside
            // some vertical formatted text.
            // 1. Exception: For a table cursor in a horizontal table, the
            //               directions should never be mapped.
            // 2. Exception: For a table cursor in a vertical table, the
            //               directions should always be mapped.
            const bool bVertText = rSh.IsInVerticalText();
            const bool bTableCursor = rSh.GetTableCursor();
            const bool bVertTable = rSh.IsTableVertical();
            if( ( bVertText && ( !bTableCursor || bVertTable ) ) ||
                ( bTableCursor && bVertTable ) )
            {
                SvxFrameDirection eDirection = rSh.GetTextDirection();
                if (eDirection == SvxFrameDirection::Vertical_LR_BT)
                {
                    // Map from physical to logical, so rotate clockwise.
                    if (KEY_UP == nKey)
                        nKey = KEY_RIGHT;
                    else if (KEY_DOWN == nKey)
                        nKey = KEY_LEFT;
                    else if (KEY_LEFT == nKey)
                        nKey = KEY_UP;
                    else /* KEY_RIGHT == nKey */
                        nKey = KEY_DOWN;
                }
                else
                {
                    // Attempt to integrate cursor travelling for mongolian layout does not work.
                    // Thus, back to previous mapping of cursor keys to direction keys.
                    if( KEY_UP == nKey ) nKey = KEY_LEFT;
                    else if( KEY_DOWN == nKey ) nKey = KEY_RIGHT;
                    else if( KEY_LEFT == nKey ) nKey = KEY_DOWN;
                    else /* KEY_RIGHT == nKey */ nKey = KEY_UP;
                }
            }

            if ( rSh.IsInRightToLeftText() )
            {
                if( KEY_LEFT == nKey ) nKey = KEY_RIGHT;
                else if( KEY_RIGHT == nKey ) nKey = KEY_LEFT;
            }

            aKeyEvent = KeyEvent( rKEvt.GetCharCode(),
                                  vcl::KeyCode( nKey, rKEvt.GetKeyCode().GetModifier() ),
                                  rKEvt.GetRepeat() );
        }
    }

    const vcl::KeyCode& rKeyCode = aKeyEvent.GetKeyCode();
    sal_Unicode aCh = aKeyEvent.GetCharCode();

    // enable switching to notes anchor with Ctrl - Alt - Page Up/Down
    // pressing this inside a note will switch to next/previous note
    if ((rKeyCode.IsMod1() && rKeyCode.IsMod2()) && ((rKeyCode.GetCode() == KEY_PAGEUP) || (rKeyCode.GetCode() == KEY_PAGEDOWN)))
    {
        const bool bNext = rKeyCode.GetCode()==KEY_PAGEDOWN;
        const SwFieldType* pFieldType = rSh.GetFieldType( 0, SwFieldIds::Postit );
        rSh.MoveFieldType( pFieldType, bNext );
        return;
    }

    if (SwTextContentControl* pTextContentControl = rSh.CursorInsideContentControl())
    {
        // Check if this combination of rKeyCode and pTextContentControl should open a popup.
        const SwFormatContentControl& rFormatContentControl = pTextContentControl->GetContentControl();
        std::shared_ptr<SwContentControl> pContentControl = rFormatContentControl.GetContentControl();
        if (pContentControl->ShouldOpenPopup(rKeyCode))
        {
            SwShellCursor* pCursor = rSh.GetCursor_();
            if (pCursor)
            {
                VclPtr<SwContentControlButton> pContentControlButton = pCursor->GetContentControlButton();
                if (pContentControlButton)
                {
                    pContentControlButton->StartPopup();
                    return;
                }
            }
        }
    }

    const SwFrameFormat* pFlyFormat = rSh.GetFlyFrameFormat();

    if (pFlyFormat)
    {
        // See if the fly frame's anchor is in a content control. If so,
        // try to interact with it.
        const SwFormatAnchor& rFormatAnchor = pFlyFormat->GetAnchor();
        SwNode* pAnchorNode = rFormatAnchor.GetAnchorNode();
        if (pAnchorNode)
        {
            SwTextNode* pTextNode = pAnchorNode->GetTextNode();
            if (pTextNode)
            {
                sal_Int32 nContentIdx = rFormatAnchor.GetAnchorContentOffset();
                SwTextAttr* pAttr = pTextNode->GetTextAttrAt(
                    nContentIdx, RES_TXTATR_CONTENTCONTROL, ::sw::GetTextAttrMode::Parent);
                if (pAttr)
                {
                    SwTextContentControl* pTextContentControl
                        = static_txtattr_cast<SwTextContentControl*>(pAttr);
                    const SwFormatContentControl& rFormatContentControl
                        = pTextContentControl->GetContentControl();
                    std::shared_ptr<SwContentControl> pContentControl
                        = rFormatContentControl.GetContentControl();
                    if (pContentControl->IsInteractingCharacter(aCh))
                    {
                        rSh.GotoContentControl(rFormatContentControl);
                        return;
                    }
                }
            }
        }
    }

    if( pFlyFormat )
    {
        SvMacroItemId nEvent;

        if( 32 <= aCh &&
            0 == (( KEY_MOD1 | KEY_MOD2 ) & rKeyCode.GetModifier() ))
            nEvent = SvMacroItemId::SwFrmKeyInputAlpha;
        else
            nEvent = SvMacroItemId::SwFrmKeyInputNoAlpha;

        const SvxMacro* pMacro = pFlyFormat->GetMacro().GetMacroTable().Get( nEvent );
        if( pMacro )
        {
            SbxArrayRef xArgs = new SbxArray;
            SbxVariableRef xVar = new SbxVariable;
            xVar->PutString( pFlyFormat->GetName().toString() );
            xArgs->Put(xVar.get(), 1);

            xVar = new SbxVariable;
            if( SvMacroItemId::SwFrmKeyInputAlpha == nEvent )
                xVar->PutChar( aCh );
            else
                xVar->PutUShort( rKeyCode.GetModifier() | rKeyCode.GetCode() );
            xArgs->Put(xVar.get(), 2);

            OUString sRet;
            rSh.ExecMacro( *pMacro, &sRet, xArgs.get() );
            if( !sRet.isEmpty() && sRet.toInt32()!=0 )
                return ;
        }
    }
    SelectionType nLclSelectionType;
    //A is converted to 1
    if( rKeyCode.GetFullCode() == (KEY_A | KEY_MOD1 |KEY_SHIFT)
        && rSh.HasDrawView() &&
        (bool(nLclSelectionType = rSh.GetSelectionType()) &&
        ((nLclSelectionType & (SelectionType::Frame|SelectionType::Graphic)) ||
        ((nLclSelectionType & (SelectionType::DrawObject|SelectionType::DbForm)) &&
                rSh.GetDrawView()->GetMarkedObjectList().GetMarkCount() == 1))))
    {
        SdrHdlList& rHdlList = const_cast<SdrHdlList&>(rSh.GetDrawView()->GetHdlList());
        SdrHdl* pAnchor = rHdlList.GetHdl(SdrHdlKind::Anchor);
        if ( ! pAnchor )
            pAnchor = rHdlList.GetHdl(SdrHdlKind::Anchor_TR);
        if(pAnchor)
            rHdlList.SetFocusHdl(pAnchor);
        return;
    }

    SvxAutoCorrCfg* pACfg = nullptr;
    SvxAutoCorrect* pACorr = nullptr;

    uno::Reference< frame::XDispatchRecorder > xRecorder =
            m_rView.GetViewFrame().GetBindings().GetRecorder();
    if ( !xRecorder.is() )
    {
        pACfg = &SvxAutoCorrCfg::Get();
        pACorr = pACfg->GetAutoCorrect();
    }

    SwModuleOptions* pModOpt = SwModule::get()->GetModuleConfig();

    OUString sFormulaEntry;

    enum class SwKeyState { CheckKey, InsChar, InsTab,
                       NoNum, NumOff, NumOrNoNum, NumDown, NumUp,
                       NumIndentInc, NumIndentDec,

                       OutlineLvOff,
                       NextCell, PrevCell, OutlineUp, OutlineDown,
                       GlossaryExpand, NextPrevGlossary,
                       AutoFormatByInput,
                       NextObject, PrevObject,
                       KeyToView,
                       LaunchOLEObject, GoIntoFly, GoIntoDrawing,
                       EnterDrawHandleMode,
                       CheckDocReadOnlyKeys,
                       CheckAutoCorrect, EditFormula,
                       ColLeftBig, ColRightBig,
                       ColLeftSmall, ColRightSmall,
                       ColBottomBig,
                       ColBottomSmall,
                       CellLeftBig, CellRightBig,
                       CellLeftSmall, CellRightSmall,
                       CellTopBig, CellBottomBig,
                       CellTopSmall, CellBottomSmall,

                       Fly_Change, Draw_Change,
                       SpecialInsert,
                       EnterCharCell,
                       GotoNextFieldMark,
                       GotoPrevFieldMark,
                       End };

    SwKeyState eKeyState = bIsViewReadOnly ? SwKeyState::CheckDocReadOnlyKeys : SwKeyState::CheckKey;

    // tdf#112932 Pressing enter in read-only Table of Content doesn't jump to heading
    if (!bIsViewReadOnly
        && ((rKeyCode.GetModifier() | rKeyCode.GetCode()) == KEY_RETURN
            || (rKeyCode.GetModifier() | rKeyCode.GetCode()) == (KEY_MOD1 | KEY_RETURN)))
    {
        const SwTOXBase* pTOXBase = rSh.GetCurTOX();
        if (pTOXBase && SwEditShell::IsTOXBaseReadonly(*pTOXBase))
            eKeyState = SwKeyState::CheckDocReadOnlyKeys;
    }

    SwKeyState eNextKeyState = SwKeyState::End;
    sal_uInt8 nDir = 0;

    if (m_nKS_NUMDOWN_Count > 0)
        m_nKS_NUMDOWN_Count--;

    if (m_nKS_NUMINDENTINC_Count > 0)
        m_nKS_NUMINDENTINC_Count--;

    while( SwKeyState::End != eKeyState )
    {
        SwKeyState eFlyState = SwKeyState::KeyToView;

        switch( eKeyState )
        {
        case SwKeyState::CheckKey:
            eKeyState = SwKeyState::KeyToView;       // default forward to View

            if (!comphelper::LibreOfficeKit::isActive() &&
                !rKeyCode.IsMod2() && '=' == aCh &&
                !rSh.IsTableMode() && rSh.GetTableFormat() &&
                rSh.IsSttPara() &&
                !rSh.HasReadonlySel())
            {
                // at the beginning of the table's cell a '=' ->
                // call EditRow (F2-functionality)
                // [Avoid this for LibreOfficeKit, as the separate input window
                // steals the focus & things go wrong - the user never gets
                // the focus back.]
                rSh.Push();
                if( !rSh.MoveSection( GoCurrSection, fnSectionStart) &&
                    !rSh.IsTableBoxTextFormat() )
                {
                    // is at the beginning of the box
                    eKeyState = SwKeyState::EditFormula;
                    if( rSh.HasMark() )
                        rSh.SwapPam();
                    else
                        rSh.SttSelect();
                    rSh.MoveSection( GoCurrSection, fnSectionEnd );
                    rSh.Pop();
                    rSh.EndSelect();
                    sFormulaEntry = "=";
                }
                else
                    rSh.Pop(SwCursorShell::PopMode::DeleteCurrent);
            }
            else
            {
                if( pACorr && aTmpQHD.HasContent() && !rSh.HasSelection() &&
                    !rSh.HasReadonlySel() && !aTmpQHD.m_bIsAutoText &&
                    pACorr->GetSwFlags().nAutoCmpltExpandKey ==
                    (rKeyCode.GetModifier() | rKeyCode.GetCode()) )
                {
                    eKeyState = SwKeyState::GlossaryExpand;
                    break;
                }

                switch( rKeyCode.GetModifier() | rKeyCode.GetCode() )
                {
                case KEY_RIGHT | KEY_MOD2:
                    eKeyState = SwKeyState::ColRightBig;
                    eFlyState = SwKeyState::Fly_Change;
                    nDir = MOVE_RIGHT_SMALL;
                    goto KEYINPUT_CHECKTABLE;

                case KEY_LEFT | KEY_MOD2:
                    eKeyState = SwKeyState::ColRightSmall;
                    eFlyState = SwKeyState::Fly_Change;
                    nDir = MOVE_LEFT_SMALL;
                    goto KEYINPUT_CHECKTABLE;

                case KEY_RIGHT | KEY_MOD2 | KEY_SHIFT:
                    eKeyState = SwKeyState::ColLeftSmall;
                    goto KEYINPUT_CHECKTABLE;

                case KEY_LEFT | KEY_MOD2 | KEY_SHIFT:
                    eKeyState = SwKeyState::ColLeftBig;
                    goto KEYINPUT_CHECKTABLE;

                case KEY_RIGHT | KEY_MOD2 | KEY_MOD1:
                    eKeyState = SwKeyState::CellRightBig;
                    goto KEYINPUT_CHECKTABLE;

                case KEY_LEFT | KEY_MOD2 | KEY_MOD1:
                    eKeyState = SwKeyState::CellRightSmall;
                    goto KEYINPUT_CHECKTABLE;

                case KEY_RIGHT | KEY_MOD2 | KEY_SHIFT | KEY_MOD1:
                    eKeyState = SwKeyState::CellLeftSmall;
                    goto KEYINPUT_CHECKTABLE;

                case KEY_LEFT | KEY_MOD2 | KEY_SHIFT | KEY_MOD1:
                    eKeyState = SwKeyState::CellLeftBig;
                    goto KEYINPUT_CHECKTABLE;

                case KEY_UP | KEY_MOD2:
                    eKeyState = SwKeyState::ColBottomSmall;
                    eFlyState = SwKeyState::Fly_Change;
                    nDir = MOVE_UP_SMALL;
                    goto KEYINPUT_CHECKTABLE;

                case KEY_DOWN | KEY_MOD2:
                    eKeyState = SwKeyState::ColBottomBig;
                    eFlyState = SwKeyState::Fly_Change;
                    nDir = MOVE_DOWN_SMALL;
                    goto KEYINPUT_CHECKTABLE;

                case KEY_UP | KEY_MOD2 | KEY_MOD1:
                    eKeyState = SwKeyState::CellBottomSmall;
                    goto KEYINPUT_CHECKTABLE;

                case KEY_DOWN | KEY_MOD2 | KEY_MOD1:
                    eKeyState = SwKeyState::CellBottomBig;
                    goto KEYINPUT_CHECKTABLE;

                case KEY_UP | KEY_MOD2 | KEY_SHIFT | KEY_MOD1:
                    eKeyState = SwKeyState::CellTopBig;
                    goto KEYINPUT_CHECKTABLE;

                case KEY_DOWN | KEY_MOD2 | KEY_SHIFT | KEY_MOD1:
                    eKeyState = SwKeyState::CellTopSmall;
                    goto KEYINPUT_CHECKTABLE;

KEYINPUT_CHECKTABLE:
                    // Resolve bugs 49091, 53190, 93402 and
                    // https://bz.apache.org/ooo/show_bug.cgi?id=113502
                    // but provide an option for restoring interactive
                    // table sizing functionality when needed.
                    if (
                      ! (Window::GetIndicatorState() & KeyIndicatorState::CAPSLOCK)
                      && m_rView.KeyInput( aKeyEvent ) // Keystroke is customized
                    )
                    {
                        bFlushBuffer = true;
                        bNormalChar = false;
                        eKeyState = SwKeyState::End;
                        break ;
                    }

                    if( rSh.IsTableMode() || !rSh.GetTableFormat() )
                    {
                        if(!pFlyFormat && SwKeyState::KeyToView != eFlyState &&
                            (rSh.GetSelectionType() & (SelectionType::DrawObject|SelectionType::DbForm))  &&
                                rSh.GetDrawView()->GetMarkedObjectList().GetMarkCount() != 0)
                            eKeyState = SwKeyState::Draw_Change;

                        if( pFlyFormat )
                            eKeyState = eFlyState;
                        else if( SwKeyState::Draw_Change != eKeyState)
                            eKeyState = SwKeyState::EnterCharCell;
                    }
                    break;

                // huge object move
                case KEY_RIGHT | KEY_SHIFT:
                case KEY_LEFT | KEY_SHIFT:
                case KEY_UP | KEY_SHIFT:
                case KEY_DOWN | KEY_SHIFT:
                {
                    const SelectionType nSelectionType = rSh.GetSelectionType();
                    if ( ( pFlyFormat
                           && ( nSelectionType & (SelectionType::Frame|SelectionType::Ole|SelectionType::Graphic) ) )
                         || ( ( nSelectionType & (SelectionType::DrawObject|SelectionType::DbForm) )
                              && rSh.GetDrawView()->GetMarkedObjectList().GetMarkCount() != 0 ) )
                    {
                        eKeyState = pFlyFormat ? SwKeyState::Fly_Change : SwKeyState::Draw_Change;
                        if (nSelectionType & SelectionType::DrawObject)
                        {
                            // tdf#137964: always move the DrawObject if one is selected
                            eKeyState = SwKeyState::Draw_Change;
                        }
                        switch ( rKeyCode.GetCode() )
                        {
                            case KEY_RIGHT: nDir = MOVE_RIGHT_HUGE; break;
                            case KEY_LEFT: nDir = MOVE_LEFT_HUGE; break;
                            case KEY_UP: nDir = MOVE_UP_HUGE; break;
                            case KEY_DOWN: nDir = MOVE_DOWN_HUGE; break;
                        }
                    }
                    break;
                }

                case KEY_LEFT:
                case KEY_LEFT | KEY_MOD1:
                {
                    bool bMod1 = 0 != (rKeyCode.GetModifier() & KEY_MOD1);
                    if(!bMod1)
                    {
                        eFlyState = SwKeyState::Fly_Change;
                        nDir = MOVE_LEFT_BIG;
                    }
                    goto KEYINPUT_CHECKTABLE_INSDEL;
                }
                case KEY_RIGHT | KEY_MOD1:
                {
                    goto KEYINPUT_CHECKTABLE_INSDEL;
                }
                case KEY_UP:
                case KEY_UP | KEY_MOD1:
                {
                    bool bMod1 = 0 != (rKeyCode.GetModifier() & KEY_MOD1);
                    if(!bMod1)
                    {
                        eFlyState = SwKeyState::Fly_Change;
                        nDir = MOVE_UP_BIG;
                    }
                    goto KEYINPUT_CHECKTABLE_INSDEL;
                }
                case KEY_DOWN:
                case KEY_DOWN | KEY_MOD1:
                {
                    bool bMod1 = 0 != (rKeyCode.GetModifier() & KEY_MOD1);
                    if(!bMod1)
                    {
                        ::sw::mark::Fieldmark* pMark = rSh.GetCurrentFieldmark();
                        if (auto pDropDown = dynamic_cast<FieldmarkWithDropDownButton*>(pMark))
                        {
                            pDropDown->LaunchPopup();
                            eKeyState = SwKeyState::End;
                            break;
                        }
                        eFlyState = SwKeyState::Fly_Change;
                        nDir = MOVE_DOWN_BIG;
                    }
                    goto KEYINPUT_CHECKTABLE_INSDEL;
                }

KEYINPUT_CHECKTABLE_INSDEL:
                if( rSh.IsTableMode() || !rSh.GetTableFormat() )
                {
                    const SelectionType nSelectionType = rSh.GetSelectionType();

                    eKeyState = SwKeyState::KeyToView;
                    if(SwKeyState::KeyToView != eFlyState)
                    {
                        if((nSelectionType & (SelectionType::DrawObject|SelectionType::DbForm))  &&
                                rSh.GetDrawView()->GetMarkedObjectList().GetMarkCount() != 0)
                            eKeyState = SwKeyState::Draw_Change;
                        else if(nSelectionType & (SelectionType::Frame|SelectionType::Ole|SelectionType::Graphic))
                            eKeyState = SwKeyState::Fly_Change;
                    }
                }
                break;


                case KEY_DELETE:
                    if ( !rSh.HasReadonlySel() || rSh.CursorInsideInputField())
                    {
                        if (rSh.IsInFrontOfLabel() && rSh.NumOrNoNum())
                            eKeyState = SwKeyState::NumOrNoNum;
                    }
                    else if (!rSh.IsCursorInParagraphMetadataField())
                    {
                        rSh.InfoReadOnlyDialog(/*bAsync=*/true);
                        eKeyState = SwKeyState::End;
                    }
                    break;

                case KEY_RETURN:
                {
                    if ( !rSh.HasReadonlySel()
                         && !rSh.CursorInsideInputField()
                         && !rSh.CursorInsideContentControl() )
                    {
                        const SelectionType nSelectionType = rSh.GetSelectionType();
                        if(nSelectionType & SelectionType::Ole)
                            eKeyState = SwKeyState::LaunchOLEObject;
                        else if(nSelectionType & SelectionType::Frame)
                            eKeyState = SwKeyState::GoIntoFly;
                        else if((nSelectionType & SelectionType::DrawObject) &&
                                !(nSelectionType & SelectionType::DrawObjectEditMode) &&
                                rSh.GetDrawView()->GetMarkedObjectList().GetMarkCount() == 1)
                        {
                            eKeyState = SwKeyState::GoIntoDrawing;
                            if (lcl_goIntoTextBox(*this, rSh))
                                eKeyState = SwKeyState::GoIntoFly;
                        }
                        else if( aTmpQHD.HasContent() && !rSh.HasSelection() &&
                            aTmpQHD.m_bIsAutoText )
                            eKeyState = SwKeyState::GlossaryExpand;

                        //RETURN and empty paragraph in numbering -> end numbering
                        else if( m_aInBuffer.isEmpty() &&
                                 rSh.GetNumRuleAtCurrCursorPos() &&
                                 !rSh.GetNumRuleAtCurrCursorPos()->IsOutlineRule() &&
                                 !rSh.HasSelection() &&
                                rSh.IsSttPara() && rSh.IsEndPara() )
                        {
                            eKeyState = SwKeyState::NumOff;
                            eNextKeyState = SwKeyState::OutlineLvOff;
                        }
                        //RETURN for new paragraph with AutoFormatting
                        else if( pACfg && pACfg->IsAutoFormatByInput() &&
                                !(nSelectionType & (SelectionType::Graphic |
                                    SelectionType::Ole | SelectionType::Frame |
                                    SelectionType::TableCell | SelectionType::DrawObject |
                                    SelectionType::DrawObjectEditMode)) )
                        {
                            eKeyState = SwKeyState::AutoFormatByInput;
                        }
                        else
                        {
                            eNextKeyState = eKeyState;
                            eKeyState = SwKeyState::CheckAutoCorrect;
                        }
                    }
                }
                break;
                case KEY_RETURN | KEY_MOD2:
                {
                    if ( !rSh.HasReadonlySel()
                         && !rSh.IsSttPara()
                         && rSh.GetNumRuleAtCurrCursorPos()
                         && !rSh.CursorInsideInputField() )
                    {
                        eKeyState = SwKeyState::NoNum;
                    }
                    else if( rSh.CanSpecialInsert() )
                        eKeyState = SwKeyState::SpecialInsert;
                }
                break;
                case KEY_BACKSPACE:
                case KEY_BACKSPACE | KEY_SHIFT:
                    if ( !rSh.HasReadonlySel() || rSh.CursorInsideInputField())
                    {
                        bool bDone = false;
                        // try to add comment for code snip:
                        // Remove the paragraph indent, if the cursor is at the
                        // beginning of a paragraph, there is no selection
                        // and no numbering rule found at the current paragraph
                        // Also try to remove indent, if current paragraph
                        // has numbering rule, but isn't counted and only
                        // key <backspace> is hit.
                        const bool bOnlyBackspaceKey( KEY_BACKSPACE == rKeyCode.GetFullCode() );
                        if ( rSh.IsSttPara()
                             && !rSh.HasSelection()
                             && ( rSh.GetNumRuleAtCurrCursorPos() == nullptr
                                  || ( rSh.IsNoNum() && bOnlyBackspaceKey ) ) )
                        {
                            bDone = rSh.TryRemoveIndent();
                        }

                        if (bDone)
                            eKeyState = SwKeyState::End;
                        else
                        {
                            if ( rSh.IsSttPara() && !rSh.IsNoNum() )
                            {
                                if (m_nKS_NUMDOWN_Count > 0 &&
                                    0 < rSh.GetNumLevel())
                                {
                                    eKeyState = SwKeyState::NumUp;
                                    m_nKS_NUMDOWN_Count = 2;
                                    bDone = true;
                                }
                                else if (m_nKS_NUMINDENTINC_Count > 0)
                                {
                                    eKeyState = SwKeyState::NumIndentDec;
                                    m_nKS_NUMINDENTINC_Count = 2;
                                    bDone = true;
                                }
                            }

                            // If the cursor is in an empty paragraph, which has
                            // a numbering, but not the outline numbering, and
                            // there is no selection, the numbering has to be
                            // deleted on key <Backspace>.
                            // Otherwise method <SwEditShell::NumOrNoNum(..)>
                            // should only change the <IsCounted()> state of
                            // the current paragraph depending of the key.
                            // On <backspace> it is set to <false>,
                            // on <shift-backspace> it is set to <true>.
                            // Thus, assure that method <SwEditShell::NumOrNum(..)>
                            // is only called for the intended purpose.
                            if ( !bDone && rSh.IsSttPara() )
                            {
                                bool bCallNumOrNoNum( false );
                                if ( bOnlyBackspaceKey && !rSh.IsNoNum() )
                                {
                                    bCallNumOrNoNum = true;
                                }
                                else if ( !bOnlyBackspaceKey && rSh.IsNoNum() )
                                {
                                    bCallNumOrNoNum = true;
                                }
                                else if ( bOnlyBackspaceKey
                                          && rSh.IsSttPara()
                                          && rSh.IsEndPara()
                                          && !rSh.HasSelection() )
                                {
                                    const SwNumRule* pCurrNumRule( rSh.GetNumRuleAtCurrCursorPos() );
                                    if ( pCurrNumRule != nullptr
                                         && pCurrNumRule != rSh.GetOutlineNumRule() )
                                    {
                                        bCallNumOrNoNum = true;
                                    }
                                }
                                if ( bCallNumOrNoNum
                                     && rSh.NumOrNoNum( !bOnlyBackspaceKey ) )
                                {
                                    eKeyState = SwKeyState::NumOrNoNum;
                                }
                            }
                        }
                    }
                    else if (!rSh.IsCursorInParagraphMetadataField())
                    {
                        rSh.InfoReadOnlyDialog(false);
                        eKeyState = SwKeyState::End;
                    }
                    break;

                case KEY_RIGHT:
                    {
                        eFlyState = SwKeyState::Fly_Change;
                        nDir = MOVE_RIGHT_BIG;
                        goto KEYINPUT_CHECKTABLE_INSDEL;
                    }
                case KEY_TAB:
                {
                    // Rich text contentControls accept tabs and fieldmarks and other rich text,
                    // so first act on cases that are not a content control
                    SwTextContentControl* pTextContentControl = rSh.CursorInsideContentControl();
                    if ((rSh.IsFormProtected() && !pTextContentControl) ||
                        rSh.GetCurrentFieldmark() || rSh.GetChar(false)==CH_TXT_ATR_FORMELEMENT)
                    {
                        eKeyState = SwKeyState::GotoNextFieldMark;
                    }
                    else if ( !rSh.IsMultiSelection() && rSh.CursorInsideInputField() )
                    {
                        GetView().GetViewFrame().GetDispatcher()->Execute( FN_GOTO_NEXT_INPUTFLD );
                        eKeyState = SwKeyState::End;
                    }
                    else if( rSh.GetNumRuleAtCurrCursorPos()
                             && rSh.IsSttOfPara()
                             && !rSh.HasReadonlySel() )
                    {
                        if (numfunc::NumDownChangesIndent(rSh))
                        {
                            eKeyState = SwKeyState::NumDown;
                        }
                        else
                        {
                            eKeyState = SwKeyState::InsTab;
                        }
                    }
                    else if (rSh.GetSelectionType() &
                             (SelectionType::Graphic |
                              SelectionType::Frame |
                              SelectionType::Ole |
                              SelectionType::DrawObject |
                              SelectionType::DbForm))
                    {
                        eKeyState = SwKeyState::NextObject;
                    }
                    else if ( rSh.GetTableFormat() )
                    {
                        if( rSh.HasSelection() || rSh.HasReadonlySel() )
                            eKeyState = SwKeyState::NextCell;
                        else
                        {
                            eKeyState = SwKeyState::CheckAutoCorrect;
                            eNextKeyState = SwKeyState::NextCell;
                        }
                    }
                    else if (pTextContentControl)
                    {
                        auto pCC = pTextContentControl->GetContentControl().GetContentControl();
                        if (pCC)
                        {
                            switch (pCC->GetType())
                            {
                                case SwContentControlType::RICH_TEXT:
                                    eKeyState = SwKeyState::InsTab;
                                    break;
                                default:
                                    eKeyState = SwKeyState::GotoNextFieldMark;
                            }
                        }
                    }
                    else
                    {
                        eKeyState = SwKeyState::InsTab;
                        if( rSh.IsSttOfPara() && !rSh.HasReadonlySel() )
                        {
                            SwTextFormatColl* pColl = rSh.GetCurTextFormatColl();
                            if( pColl &&

                                pColl->IsAssignedToListLevelOfOutlineStyle()
                                && MAXLEVEL-1 > pColl->GetAssignedOutlineStyleLevel() )
                                eKeyState = SwKeyState::OutlineDown;
                        }
                    }
                }
                break;
                case KEY_TAB | KEY_SHIFT:
                {
                    SwTextContentControl* pTextContentControl = rSh.CursorInsideContentControl();
                    if ((rSh.IsFormProtected() && !pTextContentControl) ||
                        rSh.GetCurrentFieldmark()|| rSh.GetChar(false)==CH_TXT_ATR_FORMELEMENT)
                    {
                        eKeyState = SwKeyState::GotoPrevFieldMark;
                    }
                    else if ( !rSh.IsMultiSelection() && rSh.CursorInsideInputField() )
                    {
                        GetView().GetViewFrame().GetDispatcher()->Execute( FN_GOTO_PREV_INPUTFLD );
                        eKeyState = SwKeyState::End;
                    }
                    else if( rSh.GetNumRuleAtCurrCursorPos()
                             && rSh.IsSttOfPara()
                             && !rSh.HasReadonlySel() )
                    {
                        eKeyState = SwKeyState::NumUp;
                    }
                    else if (rSh.GetSelectionType() &
                             (SelectionType::Graphic |
                              SelectionType::Frame |
                              SelectionType::Ole |
                              SelectionType::DrawObject |
                              SelectionType::DbForm))
                    {
                        eKeyState = SwKeyState::PrevObject;
                    }
                    else if ( rSh.GetTableFormat() )
                    {
                        if( rSh.HasSelection() || rSh.HasReadonlySel() )
                            eKeyState = SwKeyState::PrevCell;
                        else
                        {
                            eKeyState = SwKeyState::CheckAutoCorrect;
                            eNextKeyState = SwKeyState::PrevCell;
                        }
                    }
                    else if (pTextContentControl)
                    {
                        eKeyState = SwKeyState::GotoPrevFieldMark;
                    }
                    else
                    {
                        eKeyState = SwKeyState::End;
                        if( rSh.IsSttOfPara() && !rSh.HasReadonlySel() )
                        {
                            SwTextFormatColl* pColl = rSh.GetCurTextFormatColl();
                            if( pColl &&
                                pColl->IsAssignedToListLevelOfOutlineStyle() &&
                                0 < pColl->GetAssignedOutlineStyleLevel())
                                eKeyState = SwKeyState::OutlineUp;
                        }
                    }
                }
                break;
                case KEY_TAB | KEY_MOD1:
                case KEY_TAB | KEY_MOD2:
                    if( !rSh.HasReadonlySel() )
                    {
                        if( aTmpQHD.HasContent() && !rSh.HasSelection() )
                        {
                            // Next auto-complete suggestion
                            aTmpQHD.Next( pACorr &&
                                          pACorr->GetSwFlags().bAutoCmpltEndless );
                            eKeyState = SwKeyState::NextPrevGlossary;
                        }
                        else if( rSh.GetTableFormat() )
                            eKeyState = SwKeyState::InsTab;
                        else if((rSh.GetSelectionType() &
                                    (SelectionType::DrawObject|SelectionType::DbForm|
                                        SelectionType::Frame|SelectionType::Ole|SelectionType::Graphic))  &&
                                rSh.GetDrawView()->GetMarkedObjectList().GetMarkCount() != 0)
                            eKeyState = SwKeyState::EnterDrawHandleMode;
                        else
                        {
                            if ( !rSh.IsMultiSelection()
                                && numfunc::ChangeIndentOnTabAtFirstPosOfFirstListItem() )
                                eKeyState = SwKeyState::NumIndentInc;
                        }
                    }
                    break;

                    case KEY_TAB | KEY_MOD1 | KEY_SHIFT:
                    {
                        if( aTmpQHD.HasContent() && !rSh.HasSelection() &&
                            !rSh.HasReadonlySel() )
                        {
                            // Previous auto-complete suggestion.
                            aTmpQHD.Previous( pACorr &&
                                              pACorr->GetSwFlags().bAutoCmpltEndless );
                            eKeyState = SwKeyState::NextPrevGlossary;
                        }
                        else if((rSh.GetSelectionType() & (SelectionType::DrawObject|SelectionType::DbForm|
                                        SelectionType::Frame|SelectionType::Ole|SelectionType::Graphic)) &&
                                rSh.GetDrawView()->GetMarkedObjectList().GetMarkCount() != 0)
                        {
                            eKeyState = SwKeyState::EnterDrawHandleMode;
                        }
                        else
                        {
                            if ( !rSh.IsMultiSelection()
                                && numfunc::ChangeIndentOnTabAtFirstPosOfFirstListItem() )
                                eKeyState = SwKeyState::NumIndentDec;
                        }
                    }
                    break;
                    case KEY_F2 :
                    if( !rSh.HasReadonlySel() )
                    {
                        const SelectionType nSelectionType = rSh.GetSelectionType();
                        if(nSelectionType & SelectionType::Frame)
                            eKeyState = SwKeyState::GoIntoFly;
                        else if(nSelectionType & SelectionType::DrawObject)
                        {
                            eKeyState = SwKeyState::GoIntoDrawing;
                            if (lcl_goIntoTextBox(*this, rSh))
                                eKeyState = SwKeyState::GoIntoFly;
                        }
                    }
                    break;
                }
            }
            break;
        case SwKeyState::CheckDocReadOnlyKeys:
            {
                eKeyState = SwKeyState::KeyToView;
                switch( rKeyCode.GetModifier() | rKeyCode.GetCode() )
                {
                    case KEY_TAB:
                    case KEY_TAB | KEY_SHIFT:
                        bNormalChar = false;
                        eKeyState = SwKeyState::End;
                        if ( rSh.GetSelectionType() &
                                (SelectionType::Graphic |
                                    SelectionType::Frame |
                                    SelectionType::Ole |
                                    SelectionType::DrawObject |
                                    SelectionType::DbForm))

                        {
                            eKeyState = (rKeyCode.GetModifier() & KEY_SHIFT) ?
                                                SwKeyState::PrevObject : SwKeyState::NextObject;
                        }
                        else if ( !rSh.IsMultiSelection() && rSh.CursorInsideInputField() )
                        {
                            GetView().GetViewFrame().GetDispatcher()->Execute(
                                KEY_SHIFT != rKeyCode.GetModifier() ? FN_GOTO_NEXT_INPUTFLD : FN_GOTO_PREV_INPUTFLD );
                        }
                        else
                        {
                            rSh.SelectNextPrevHyperlink( KEY_SHIFT != rKeyCode.GetModifier() );
                        }
                    break;
                    case KEY_RETURN:
                    case KEY_RETURN | KEY_MOD1:
                    {
                        const SelectionType nSelectionType = rSh.GetSelectionType();
                        if(nSelectionType & SelectionType::Frame)
                            eKeyState = SwKeyState::GoIntoFly;
                        else
                        {
                            SfxItemSetFixed<RES_TXTATR_INETFMT, RES_TXTATR_INETFMT> aSet(rSh.GetAttrPool());
                            rSh.GetCurAttr(aSet);
                            if(SfxItemState::SET == aSet.GetItemState(RES_TXTATR_INETFMT, false))
                            {
                                const SfxPoolItem& rItem = aSet.Get(RES_TXTATR_INETFMT);
                                bNormalChar = false;
                                eKeyState = SwKeyState::End;
                                rSh.ClickToINetAttr(static_cast<const SwFormatINetFormat&>(rItem));
                            }
                        }
                    }
                    break;
                }
            }
            break;

        case SwKeyState::EnterCharCell:
            {
                eKeyState = SwKeyState::KeyToView;
                switch ( rKeyCode.GetModifier() | rKeyCode.GetCode() )
                {
                    case KEY_RIGHT | KEY_MOD2:
                        rSh.Right( SwCursorSkipMode::Chars, false, 1, false );
                        eKeyState = SwKeyState::End;
                        FlushInBuffer();
                        break;
                    case KEY_LEFT | KEY_MOD2:
                        rSh.Left( SwCursorSkipMode::Chars, false, 1, false );
                        eKeyState = SwKeyState::End;
                        FlushInBuffer();
                        break;
                }
            }
            break;

        case SwKeyState::KeyToView:
            {
                eKeyState = SwKeyState::End;
                bNormalChar =
                    !rKeyCode.IsMod2() &&
                    rKeyCode.GetModifier() != KEY_MOD1 &&
                    rKeyCode.GetModifier() != (KEY_MOD1|KEY_SHIFT) &&
                    SW_ISPRINTABLE( aCh );

                if( bNormalChar && rSh.IsInFrontOfLabel() )
                {
                    rSh.NumOrNoNum();
                }

                if( !m_aInBuffer.isEmpty() && ( !bNormalChar || bIsViewReadOnly ))
                    FlushInBuffer();

                if (rSh.HasReadonlySel()
                    && (   rKeyCode.GetFunction() == KeyFuncType::PASTE
                        || rKeyCode.GetFunction() == KeyFuncType::CUT))
                {
                    rSh.InfoReadOnlyDialog(true);
                    eKeyState = SwKeyState::End;
                }
                else if( m_rView.KeyInput( aKeyEvent ) )
                {
                    bFlushBuffer = true;
                    bNormalChar = false;
                }
                else
                {
                    // Because Sfx accelerators are only called when they were
                    // enabled at the last status update, copy has to called
                    // 'forcefully' by us if necessary.
                    if( rKeyCode.GetFunction() == KeyFuncType::COPY )
                        GetView().GetViewFrame().GetBindings().Execute(SID_COPY);

                    if( !bIsViewReadOnly && bNormalChar )
                    {
                        const SelectionType nSelectionType = rSh.GetSelectionType();
                        const bool bDrawObject = (nSelectionType & SelectionType::DrawObject) &&
                            !(nSelectionType & SelectionType::DrawObjectEditMode) &&
                            rSh.GetDrawView()->GetMarkedObjectList().GetMarkCount() == 1;

                        bool bTextBox = false;
                        if (bDrawObject && lcl_goIntoTextBox(*this, rSh))
                            // A draw shape was selected, but it has a TextBox,
                            // start editing that instead when the normal
                            // character is pressed.
                            bTextBox = true;

                        if (bDrawObject && !bTextBox)
                        {
                            SdrObject* pObj = rSh.GetDrawView()->GetMarkedObjectList().GetMark(0)->GetMarkedSdrObj();
                            if(pObj)
                            {
                                EnterDrawTextMode(pObj->GetLogicRect().Center());
                                if ( auto pSwDrawTextShell = dynamic_cast< SwDrawTextShell *>(  m_rView.GetCurShell() )  )
                                    pSwDrawTextShell->Init();
                                rSh.GetDrawView()->KeyInput( rKEvt, this );
                            }
                        }
                        else if (nSelectionType & SelectionType::Frame || bTextBox)
                        {
                            rSh.UnSelectFrame();
                            rSh.LeaveSelFrameMode();
                            m_rView.AttrChangedNotify(nullptr);
                            rSh.MoveSection( GoCurrSection, fnSectionEnd );
                        }
                        eKeyState = SwKeyState::InsChar;
                    }
                    else
                    {
                        bNormalChar = false;
                        Window::KeyInput( aKeyEvent );
                    }
                }
            }
            break;
        case SwKeyState::LaunchOLEObject:
        {
            rSh.LaunchOLEObj();
            eKeyState = SwKeyState::End;
        }
        break;
        case SwKeyState::GoIntoFly:
        {
            rSh.UnSelectFrame();
            rSh.LeaveSelFrameMode();
            m_rView.AttrChangedNotify(nullptr);
            rSh.MoveSection( GoCurrSection, fnSectionEnd );
            eKeyState = SwKeyState::End;
        }
        break;
        case SwKeyState::GoIntoDrawing:
        {
            if (SdrMark* pMark = rSh.GetDrawView()->GetMarkedObjectList().GetMark(0))
            {
                SdrObject* pObj = pMark->GetMarkedSdrObj();
                if(pObj)
                {
                    EnterDrawTextMode(pObj->GetLogicRect().Center());
                    if (auto pSwDrawTextShell = dynamic_cast< SwDrawTextShell *>(  m_rView.GetCurShell() )  )
                        pSwDrawTextShell->Init();
                }
            }
            eKeyState = SwKeyState::End;
        }
        break;
        case SwKeyState::EnterDrawHandleMode:
        {
            const SdrHdlList& rHdlList = rSh.GetDrawView()->GetHdlList();
            bool bForward(!aKeyEvent.GetKeyCode().IsShift());

            const_cast<SdrHdlList&>(rHdlList).TravelFocusHdl(bForward);
            eKeyState = SwKeyState::End;
        }
        break;
        case SwKeyState::InsTab:
            if( dynamic_cast<const SwWebView*>( &m_rView) !=  nullptr)     // no Tab for WebView
            {
                // then it should be passed along
                Window::KeyInput( aKeyEvent );
                eKeyState = SwKeyState::End;
                break;
            }
            aCh = '\t';
            [[fallthrough]];
        case SwKeyState::InsChar:
        {
            if (rSh.CursorInsideContentControl())
            {
                const SwPosition* pStart = rSh.GetCursor()->Start();
                SwTextNode* pTextNode = pStart->GetNode().GetTextNode();
                if (pTextNode)
                {
                    sal_Int32 nIndex = pStart->GetContentIndex();
                    SwTextAttr* pAttr = pTextNode->GetTextAttrAt(nIndex, RES_TXTATR_CONTENTCONTROL, ::sw::GetTextAttrMode::Parent);
                    if (pAttr)
                    {
                        auto pTextContentControl = static_txtattr_cast<SwTextContentControl*>(pAttr);
                        const SwFormatContentControl& rFormatContentControl = pTextContentControl->GetContentControl();
                        std::shared_ptr<SwContentControl> pContentControl = rFormatContentControl.GetContentControl();
                        if (pContentControl->IsInteractingCharacter(aCh))
                        {
                            rSh.GotoContentControl(rFormatContentControl);
                            eKeyState = SwKeyState::End;
                            break;
                        }
                    }
                }
            }

            if (rSh.GetChar(false)==CH_TXT_ATR_FORMELEMENT)
            {
                ::sw::mark::CheckboxFieldmark* pFieldmark =
                    dynamic_cast< ::sw::mark::CheckboxFieldmark* >
                        (rSh.GetCurrentFieldmark());
                OSL_ENSURE(pFieldmark,
                    "Where is my FieldMark??");
                if(pFieldmark)
                {
                    pFieldmark->SetChecked(!pFieldmark->IsChecked());
                    OSL_ENSURE(pFieldmark->IsExpanded(),
                        "where is the otherpos?");
                    if (pFieldmark->IsExpanded())
                    {
                        rSh.CalcLayout();
                    }
                }
                eKeyState = SwKeyState::End;
            }
            else if ( !rSh.HasReadonlySel()
                      || rSh.CursorInsideInputField() )
            {
                const bool bIsNormalChar =
                    GetAppCharClass().isLetterNumeric( OUString( aCh ), 0 );
                if( bAppendSpace && bIsNormalChar &&
                    (!m_aInBuffer.isEmpty() || !rSh.IsSttPara() || !rSh.IsEndPara() ))
                {
                    // insert a blank ahead of the character. this ends up
                    // between the expanded text and the new "non-word-separator".
                    m_aInBuffer += " ";
                }

                const SwViewOption& rVwOpt = SwViewOption::GetCurrentViewOptions();
                const bool bIsAutoCorrectChar = SvxAutoCorrect::IsAutoCorrectChar(aCh);
                if (!aKeyEvent.GetRepeat() && rSh.HasSelection()
                    && rVwOpt.IsEncloseWithCharactersOn()
                    && SwViewOption::IsEncloseWithCharactersTrigger(aCh))
                {
                    FlushInBuffer();
                    switch (aCh)
                    {
                        case '(':
                            rSh.InsertEnclosingChars(u"("_ustr, u")"_ustr);
                            break;
                        case '[':
                            rSh.InsertEnclosingChars(u"["_ustr, u"]"_ustr);
                            break;
                        case '{':
                            rSh.InsertEnclosingChars(u"{"_ustr, u"}"_ustr);
                            break;
                        case '\"':
                        {
                            LanguageType eLang
                                = Application::GetSettings().GetLanguageTag().getLanguageType();
                            OUString sStartQuote{ pACorr->GetQuote('\"', true, eLang) };
                            OUString sEndQuote{ pACorr->GetQuote('\"', false, eLang) };
                            rSh.InsertEnclosingChars(sStartQuote, sEndQuote);
                            break;
                        }
                        case '\'':
                        {
                            LanguageType eLang
                                = Application::GetSettings().GetLanguageTag().getLanguageType();
                            OUString sStartQuote{ pACorr->GetQuote('\'', true, eLang) };
                            OUString sEndQuote{ pACorr->GetQuote('\'', false, eLang) };
                            rSh.InsertEnclosingChars(sStartQuote, sEndQuote);
                            break;
                        }
                    }
                }
                else if( !aKeyEvent.GetRepeat() && pACorr && ( bIsAutoCorrectChar || rSh.IsNbspRunNext() ) &&
                        pACfg->IsAutoFormatByInput() &&
                    (( pACorr->IsAutoCorrFlag( ACFlags::ChgWeightUnderl ) &&
                        ( '*' == aCh || '_' == aCh ) ) ||
                     ( pACorr->IsAutoCorrFlag( ACFlags::ChgQuotes ) && ('\"' == aCh ))||
                     ( pACorr->IsAutoCorrFlag( ACFlags::ChgSglQuotes ) && ( '\'' == aCh))))
                {
                    FlushInBuffer();
                    rSh.AutoCorrect( *pACorr, aCh );
                    if( '\"' != aCh && '\'' != aCh )        // only call when "*_"!
                        rSh.UpdateAttr();
                }
                else if( !aKeyEvent.GetRepeat() && pACorr && ( bIsAutoCorrectChar || rSh.IsNbspRunNext() ) &&
                        pACfg->IsAutoFormatByInput() &&
                    pACorr->IsAutoCorrFlag( ACFlags::CapitalStartSentence | ACFlags::CapitalStartWord |
                                            ACFlags::ChgOrdinalNumber | ACFlags::AddNonBrkSpace |
                                            ACFlags::ChgToEnEmDash | ACFlags::SetINetAttr |
                                            ACFlags::Autocorrect | ACFlags::TransliterateRTL |
                                            ACFlags::SetDOIAttr ) &&
                    '\"' != aCh && '\'' != aCh && '*' != aCh && '_' != aCh
                    )
                {
                    FlushInBuffer();
                    rSh.AutoCorrect( *pACorr, aCh );
                }
                else
                {
                    OUStringBuffer aBuf(m_aInBuffer);
                    comphelper::string::padToLength(aBuf,
                        m_aInBuffer.getLength() + aKeyEvent.GetRepeat() + 1, aCh);
                    m_aInBuffer = aBuf.makeStringAndClear();
                    bool delayFlush = Application::AnyInput( VclInputFlags::KEYBOARD );
                    bFlushBuffer = !delayFlush;
                    if( delayFlush )
                    {
                        // Start the timer, make sure to not restart it.
                        keyInputFlushTimerStop.dismiss();
                        if( !m_aKeyInputFlushTimer.IsActive())
                            m_aKeyInputFlushTimer.Start();
                    }
                }
                eKeyState = SwKeyState::End;
            }
            else
            {
                rSh.InfoReadOnlyDialog(true);
                eKeyState = SwKeyState::End;
            }

            bool bIsSpace = (aCh == ' ');
            if (bIsSpace && pACorr && pACfg)
            {
                // do the formatting only for few starting characters (for "* " or "- " conversion)
                SwPosition aPos(*rSh.GetCursor()->GetPoint());
                if (aPos.nContent < 3)
                {
                    SvxSwAutoFormatFlags& rFlags = pACorr->GetSwFlags();
                    if(pACfg->IsAutoFormatByInput() && rFlags.bSetNumRule && rFlags.bSetNumRuleAfterSpace)
                        rSh.AutoFormat(&rFlags, true);
                }
            }
        }
        break;

        case SwKeyState::CheckAutoCorrect:
        {
            if( pACorr && pACfg->IsAutoFormatByInput() &&
                pACorr->IsAutoCorrFlag( ACFlags::CapitalStartSentence | ACFlags::CapitalStartWord |
                                        ACFlags::ChgOrdinalNumber | ACFlags::TransliterateRTL |
                                        ACFlags::ChgToEnEmDash | ACFlags::SetINetAttr |
                                        ACFlags::Autocorrect | ACFlags::SetDOIAttr ) &&
                !rSh.HasReadonlySel() )
            {
                FlushInBuffer();
                rSh.AutoCorrect( *pACorr, u'\0' );
            }
            eKeyState = eNextKeyState;
        }
        break;

        default:
        {
            sal_uInt16 nSlotId = 0;
            FlushInBuffer();
            switch( eKeyState )
            {
            case SwKeyState::SpecialInsert:
                rSh.DoSpecialInsert();
                break;

            case SwKeyState::NoNum:
                rSh.NoNum();
                break;

            case SwKeyState::NumOff:
                // shell change - so record in advance
                rSh.DelNumRules();
                break;
            case SwKeyState::OutlineLvOff: // delete autofmt outlinelevel later
                break;

            case SwKeyState::NumDown:
                rSh.NumUpDown();
                m_nKS_NUMDOWN_Count = 2;
                break;
            case SwKeyState::NumUp:
                rSh.NumUpDown( false );
                break;

            case SwKeyState::NumIndentInc:
                rSh.ChangeIndentOfAllListLevels(360);
                m_nKS_NUMINDENTINC_Count = 2;
                break;

            case SwKeyState::GotoNextFieldMark:
                {
                    rSh.GotoFormControl(/*bNext=*/true);
                }
                break;

            case SwKeyState::GotoPrevFieldMark:
                {
                    rSh.GotoFormControl(/*bNext=*/false);
                }
                break;

            case SwKeyState::NumIndentDec:
                rSh.ChangeIndentOfAllListLevels(-360);
                break;

            case SwKeyState::OutlineDown:
                rSh.OutlineUpDown();
                break;
            case SwKeyState::OutlineUp:
                rSh.OutlineUpDown( -1 );
                break;

            case SwKeyState::NextCell:
                // always 'flush' in tables
                rSh.GoNextCell(!rSh.HasReadonlySel());
                nSlotId = FN_GOTO_NEXT_CELL;
                break;
            case SwKeyState::PrevCell:
                rSh.GoPrevCell();
                nSlotId = FN_GOTO_PREV_CELL;
                break;
            case SwKeyState::AutoFormatByInput:
                rSh.SplitNode( true );
                break;

            case SwKeyState::NextObject:
            case SwKeyState::PrevObject:
                if(rSh.GotoObj( SwKeyState::NextObject == eKeyState, GotoObjFlags::Any))
                {
                    if( rSh.IsFrameSelected() &&
                        m_rView.GetDrawFuncPtr() )
                    {
                        m_rView.GetDrawFuncPtr()->Deactivate();
                        m_rView.SetDrawFuncPtr(nullptr);
                        m_rView.LeaveDrawCreate();
                        m_rView.AttrChangedNotify(nullptr);
                    }
                    rSh.HideCursor();
                    rSh.EnterSelFrameMode();
                }
            break;
            case SwKeyState::GlossaryExpand:
            {
                // replace the word or abbreviation with the auto text
                rSh.StartUndo( SwUndoId::START );

                OUString sFnd(aTmpQHD.CurStr());
                if( aTmpQHD.m_bIsAutoText )
                {
                    SwGlossaryList* pList = ::GetGlossaryList();
                    OUString sShrtNm;
                    OUString sGroup;
                    if(pList->GetShortName( sFnd, sShrtNm, sGroup))
                    {
                        rSh.SttSelect();
                        rSh.ExtendSelection(false, aTmpQHD.CurLen());
                        SwGlossaryHdl* pGlosHdl = GetView().GetGlosHdl();
                        pGlosHdl->SetCurGroup(sGroup, true);
                        pGlosHdl->InsertGlossary( sShrtNm);
                        s_pQuickHlpData->m_bAppendSpace = true;
                    }
                }
                else
                {
                    sFnd = sFnd.copy(aTmpQHD.CurLen());
                    rSh.Insert( sFnd );
                    s_pQuickHlpData->m_bAppendSpace = !pACorr ||
                            pACorr->GetSwFlags().bAutoCmpltAppendBlank;
                }
                rSh.EndUndo( SwUndoId::END );
            }
            break;

            case SwKeyState::NextPrevGlossary:
                s_pQuickHlpData->Move( aTmpQHD );
                s_pQuickHlpData->Start(rSh, false);
                break;

            case SwKeyState::EditFormula:
            {
                const sal_uInt16 nId = SwInputChild::GetChildWindowId();

                SfxViewFrame& rVFrame = GetView().GetViewFrame();
                rVFrame.ToggleChildWindow( nId );
                SwInputChild* pChildWin = static_cast<SwInputChild*>(rVFrame.
                                                    GetChildWindow( nId ));
                if( pChildWin )
                    pChildWin->SetFormula( sFormulaEntry );
            }
            break;

            case SwKeyState::ColLeftBig:         rSh.SetColRowWidthHeight( TableChgWidthHeightType::ColLeft|TableChgWidthHeightType::BiggerMode, pModOpt->GetTableHMove() );   break;
            case SwKeyState::ColRightBig:        rSh.SetColRowWidthHeight( TableChgWidthHeightType::ColRight|TableChgWidthHeightType::BiggerMode, pModOpt->GetTableHMove() );  break;
            case SwKeyState::ColLeftSmall:       rSh.SetColRowWidthHeight( TableChgWidthHeightType::ColLeft, pModOpt->GetTableHMove() );   break;
            case SwKeyState::ColRightSmall:      rSh.SetColRowWidthHeight( TableChgWidthHeightType::ColRight, pModOpt->GetTableHMove() );  break;
            case SwKeyState::ColBottomBig:       rSh.SetColRowWidthHeight( TableChgWidthHeightType::RowBottom|TableChgWidthHeightType::BiggerMode, pModOpt->GetTableVMove() ); break;
            case SwKeyState::ColBottomSmall:     rSh.SetColRowWidthHeight( TableChgWidthHeightType::RowBottom, pModOpt->GetTableVMove() ); break;
            case SwKeyState::CellLeftBig:        rSh.SetColRowWidthHeight( TableChgWidthHeightType::CellLeft|TableChgWidthHeightType::BiggerMode, pModOpt->GetTableHMove() );  break;
            case SwKeyState::CellRightBig:       rSh.SetColRowWidthHeight( TableChgWidthHeightType::CellRight|TableChgWidthHeightType::BiggerMode, pModOpt->GetTableHMove() ); break;
            case SwKeyState::CellLeftSmall:      rSh.SetColRowWidthHeight( TableChgWidthHeightType::CellLeft, pModOpt->GetTableHMove() );  break;
            case SwKeyState::CellRightSmall:     rSh.SetColRowWidthHeight( TableChgWidthHeightType::CellRight, pModOpt->GetTableHMove() ); break;
            case SwKeyState::CellTopBig:         rSh.SetColRowWidthHeight( TableChgWidthHeightType::CellTop|TableChgWidthHeightType::BiggerMode, pModOpt->GetTableVMove() );   break;
            case SwKeyState::CellBottomBig:      rSh.SetColRowWidthHeight( TableChgWidthHeightType::CellBottom|TableChgWidthHeightType::BiggerMode, pModOpt->GetTableVMove() );    break;
            case SwKeyState::CellTopSmall:       rSh.SetColRowWidthHeight( TableChgWidthHeightType::CellTop, pModOpt->GetTableVMove() );   break;
            case SwKeyState::CellBottomSmall:    rSh.SetColRowWidthHeight( TableChgWidthHeightType::CellBottom, pModOpt->GetTableVMove() );    break;

            case SwKeyState::Fly_Change:
            {
                SdrView *pSdrView = rSh.GetDrawView();
                const SdrHdlList& rHdlList = pSdrView->GetHdlList();
                if(rHdlList.GetFocusHdl())
                    ChangeDrawing( nDir );
                else
                    ChangeFly( nDir, dynamic_cast<const SwWebView*>( &m_rView) !=  nullptr );
            }
            break;
            case SwKeyState::Draw_Change :
                ChangeDrawing( nDir );
                break;
            default:
                break;
            }
            if( nSlotId && m_rView.GetViewFrame().GetBindings().GetRecorder().is() )
            {
                SfxRequest aReq(m_rView.GetViewFrame(), nSlotId);
                aReq.Done();
            }
            eKeyState = SwKeyState::End;
        }
        }
    }

    // update the page number in the statusbar
    nKey = rKEvt.GetKeyCode().GetCode();
    if( KEY_UP == nKey || KEY_DOWN == nKey || KEY_PAGEUP == nKey || KEY_PAGEDOWN == nKey )
        GetView().GetViewFrame().GetBindings().Update( FN_STAT_PAGE );

    m_bMaybeShowTooltipAfterBufferFlush = bNormalChar;

    // in case the buffered characters are inserted
    if( bFlushBuffer && !m_aInBuffer.isEmpty() )
    {
        FlushInBuffer();
    }

    // get the word count dialog to update itself
    SwWordCountWrapper *pWrdCnt = static_cast<SwWordCountWrapper*>(GetView().GetViewFrame().GetChildWindow(SwWordCountWrapper::GetChildWindowId()));
    if( pWrdCnt )
        pWrdCnt->UpdateCounts();

}

/**
 * MouseEvents
 */
void SwEditWin::ResetMouseButtonDownFlags()
{
    // Not on all systems a MouseButtonUp is used ahead
    // of the modal dialog (like on WINDOWS).
    // So reset the statuses here and release the mouse
    // for the dialog.
    m_bMBPressed = false;
    g_bNoInterrupt = false;
    EnterArea();
    ReleaseMouse();
}

/**
 * Determines if the current position has a clickable url over a background
 * frame. In that case, ctrl-click should select the url, not the frame.
 */
static bool lcl_urlOverBackground(SwWrtShell& rSh, const Point& rDocPos)
{
    SwContentAtPos aSwContentAtPos(IsAttrAtPos::InetAttr);
    SdrObject* pSelectableObj = rSh.GetObjAt(rDocPos);

    return rSh.GetContentAtPos(rDocPos, aSwContentAtPos) && pSelectableObj->GetLayer() == rSh.GetDoc()->getIDocumentDrawModelAccess().GetHellId();
}

void SwEditWin::MoveCursor( SwWrtShell &rSh, const Point& rDocPos,
                            const bool bOnlyText, bool bLockView )
{
    const bool bTmpNoInterrupt = g_bNoInterrupt;
    g_bNoInterrupt = false;

    int nTmpSetCursor = 0;

    if( !rSh.IsViewLocked() && bLockView )
        rSh.LockView( true );
    else
        bLockView = false;

    {
        // only temporary generate move context because otherwise
        // the query to the content form doesn't work!!!
        SwMvContext aMvContext( &rSh );
        nTmpSetCursor = rSh.CallSetCursor(&rDocPos, bOnlyText);
        g_bValidCursorPos = !(CRSR_POSCHG & nTmpSetCursor);
    }

    // notify the edit window that from now on we do not use the input language
    if ( !(CRSR_POSOLD & nTmpSetCursor) )
        SetUseInputLanguage( false );

    if( bLockView )
        rSh.LockView( false );

    g_bNoInterrupt = bTmpNoInterrupt;
}

void SwEditWin::MouseButtonDown(const MouseEvent& _rMEvt)
{
    SwWrtShell &rSh = m_rView.GetWrtShell();
    const SwField *pCursorField = rSh.CursorInsideInputField() ? rSh.GetCurField( true ) : nullptr;

    // We have to check if a context menu is shown and we have an UI
    // active inplace client. In that case we have to ignore the mouse
    // button down event. Otherwise we would crash (context menu has been
    // opened by inplace client and we would deactivate the inplace client,
    // the context menu is closed by VCL asynchronously which in the end
    // would work on deleted objects or the context menu has no parent anymore)
    SfxInPlaceClient* pIPClient = rSh.GetSfxViewShell()->GetIPClient();
    bool bIsOleActive = ( pIPClient && pIPClient->IsObjectInPlaceActive() );

    if (bIsOleActive && vcl::IsInPopupMenuExecute())
        return;

    MouseEvent aMEvt(_rMEvt);

    if (m_rView.GetPostItMgr()->IsHit(aMEvt.GetPosPixel()))
        return;

    if (comphelper::LibreOfficeKit::isActive())
    {
        if (vcl::Window* pWindow = m_rView.GetPostItMgr()->IsHitSidebarWindow(aMEvt.GetPosPixel()))
        {
            pWindow->MouseButtonDown(aMEvt);
            return;
        }
    }

    if (aMEvt.GetButtons() == MOUSE_LEFT && m_rView.GetPostItMgr()->IsHitSidebarDragArea(aMEvt.GetPosPixel()))
    {
        mbIsDragSidebar = true;
        // Capture mouse to keep tracking even if the mouse leaves the document window
        CaptureMouse();
        return;
    }

    m_rView.GetPostItMgr()->SetActiveSidebarWin(nullptr);

    GrabFocus();
    rSh.addCurrentPosition();

    //ignore key modifiers for format paintbrush
    {
        bool bExecFormatPaintbrush = m_pApplyTempl && m_pApplyTempl->m_pFormatClipboard
                                &&  m_pApplyTempl->m_pFormatClipboard->HasContent();
        if( bExecFormatPaintbrush )
            aMEvt = MouseEvent(_rMEvt.GetPosPixel(), _rMEvt.GetClicks(), _rMEvt.GetMode(),
                               _rMEvt.GetButtons());
    }

    m_bWasShdwCursor = nullptr != m_pShadCursor;
    m_pShadCursor.reset();

    const Point aDocPos(PixelToLogic(aMEvt.GetPosPixel()));

    FrameControlType eControl;
    bool bOverFly = false;
    bool bPageAnchored = false;
    bool bOverHeaderFooterFly = IsOverHeaderFooterFly( aDocPos, eControl, bOverFly, bPageAnchored );

    bool bIsViewReadOnly = m_rView.GetDocShell()->IsReadOnly() || (rSh.GetSfxViewShell() && rSh.GetSfxViewShell()->IsLokReadOnlyView());
    if (bOverHeaderFooterFly && (!bIsViewReadOnly && rSh.GetCurField()))
        // We have a field here, that should have priority over header/footer fly.
        bOverHeaderFooterFly = false;

    // Are we clicking on a blank header/footer area?
    if ( IsInHeaderFooter( aDocPos, eControl ) || bOverHeaderFooterFly )
    {
        const SwPageFrame* pPageFrame = rSh.GetLayout()->GetPageAtPos( aDocPos );

        if ( pPageFrame )
        {
            // Is it active?
            bool bActive = true;
            const SwPageDesc* pDesc = pPageFrame->GetPageDesc();

            const SwFrameFormat* pFormat = pDesc->GetLeftFormat();
            if ( pPageFrame->OnRightPage() )
                 pFormat = pDesc->GetRightFormat();

            if ( pFormat )
            {
                if ( eControl == FrameControlType::Header )
                    bActive = pFormat->GetHeader().IsActive();
                else
                    bActive = pFormat->GetFooter().IsActive();
            }

            if ( !bActive )
            {
                // HeaderFooter menu implies header/footer controls, so only do this with IsUseHeaderFooterMenu enabled.
                // But, additionally, when in Hide-Whitespace mode, we don't want those controls.
                if (rSh.GetViewOptions()->IsUseHeaderFooterMenu() && !rSh.GetViewOptions()->IsHideWhitespaceMode())
                {
                    SwPaM aPam(*rSh.GetCurrentShellCursor().GetPoint());
                    const bool bWasInHeader = aPam.GetPoint()->GetNode().FindHeaderStartNode() != nullptr;
                    const bool bWasInFooter = aPam.GetPoint()->GetNode().FindFooterStartNode() != nullptr;

                    // Is the cursor in a part like similar to the one we clicked on? For example,
                    // if the cursor is in a header and we click on an empty header... don't change anything to
                    // keep consistent behaviour due to header edit mode (and the same for the footer as well).

                    // Otherwise, we hide the header/footer control if a separator is shown, and vice versa.
                    if (!(bWasInHeader && eControl == FrameControlType::Header) &&
                        !(bWasInFooter && eControl == FrameControlType::Footer))
                    {
                        const bool bSeparatorWasVisible = rSh.IsShowHeaderFooterSeparator(eControl);
                        rSh.SetShowHeaderFooterSeparator(eControl, !bSeparatorWasVisible);

                        // Repaint everything
                        Invalidate();

                        // tdf#84929. If the footer control had not been showing, do not change the cursor position,
                        // because the user may have scrolled to turn on the separator control and
                        // if the cursor cannot be positioned on-screen, then the user would need to scroll back again to use the control.
                        // This should only be done for the footer. The cursor can always be re-positioned near the header. tdf#134023.
                        if ( eControl == FrameControlType::Footer && !bSeparatorWasVisible
                             && !Application::IsHeadlessModeEnabled() )
                            return;
                    }
                }
            }
            else
            {
                // Make sure we have the proper Header/Footer separators shown
                // as these may be changed if clicking on an empty Header/Footer
                rSh.SetShowHeaderFooterSeparator( FrameControlType::Header, eControl == FrameControlType::Header );
                rSh.SetShowHeaderFooterSeparator( FrameControlType::Footer, eControl == FrameControlType::Footer );

                if ( !rSh.IsHeaderFooterEdit() )
                    rSh.ToggleHeaderFooterEdit();
            }
        }
    }
    else
    {
        if ( rSh.IsHeaderFooterEdit( ) )
            rSh.ToggleHeaderFooterEdit( );
        else
        {
            // Make sure that the separators are hidden
            rSh.SetShowHeaderFooterSeparator( FrameControlType::Header, false );
            rSh.SetShowHeaderFooterSeparator( FrameControlType::Footer, false );
        }

        // Toggle Hide-Whitespace if between pages.
        if (rSh.GetViewOptions()->CanHideWhitespace() &&
            rSh.GetLayout()->IsBetweenPages(aDocPos))
        {
            if (_rMEvt.GetClicks() >= 2)
            {
                SwViewOption aOpt(*rSh.GetViewOptions());
                aOpt.SetHideWhitespaceMode(!aOpt.IsHideWhitespaceMode());
                rSh.ApplyViewOptions(aOpt);
            }

            return;
        }
    }

    if ( IsChainMode() )
    {
        SetChainMode( false );
        SwRect aDummy;
        SwFlyFrameFormat *pFormat = static_cast<SwFlyFrameFormat*>(rSh.GetFlyFrameFormat());
        if ( rSh.Chainable( aDummy, *pFormat, aDocPos ) == SwChainRet::OK )
            rSh.Chain( *pFormat, aDocPos );
        UpdatePointer(aDocPos, aMEvt.GetModifier());
        return;
    }

    // After GrabFocus a shell should be pushed. That should actually
    // work but in practice ...
    m_rView.SelectShellForDrop();

    bool bCallBase = true;

    if( s_pQuickHlpData->m_bIsDisplayed )
        s_pQuickHlpData->Stop( rSh );
    s_pQuickHlpData->m_bAppendSpace = false;

    if( rSh.FinishOLEObj() )
        return; // end InPlace and the click doesn't count anymore

    CurrShell aCurr( &rSh );

    SdrView *pSdrView = rSh.GetDrawView();
    if ( pSdrView )
    {
        if (pSdrView->MouseButtonDown(aMEvt, GetOutDev()))
        {
            rSh.GetView().GetViewFrame().GetBindings().InvalidateAll(false);
            return; // SdrView's event evaluated
        }
    }

    m_bIsInMove = false;
    m_aStartPos = aMEvt.GetPosPixel();
    m_aRszMvHdlPt.setX( 0 );
    m_aRszMvHdlPt.setY( 0 );

    SwTab nMouseTabCol = SwTab::COL_NONE;
    const bool bTmp = !rSh.IsDrawCreate() && !m_pApplyTempl && !rSh.IsInSelect()
                      && aMEvt.GetClicks() == 1 && MOUSE_LEFT == aMEvt.GetButtons();

    if (  bTmp &&
         SwTab::COL_NONE != (nMouseTabCol = rSh.WhichMouseTabCol( aDocPos ) ) &&
         ( !rSh.IsObjSelectable( aDocPos ) ||
             // allow resizing row height, if the image is anchored as character in the cell
             !( SwTab::COL_VERT == nMouseTabCol || SwTab::COL_HORI == nMouseTabCol ) ) )
    {
        // Enhanced table selection
        if ( SwTab::SEL_HORI <= nMouseTabCol && SwTab::COLSEL_VERT >= nMouseTabCol )
        {
            rSh.EnterStdMode();
            rSh.SelectTableRowCol( aDocPos );
            if( SwTab::SEL_HORI  != nMouseTabCol && SwTab::SEL_HORI_RTL  != nMouseTabCol)
            {
                m_xRowColumnSelectionStart = aDocPos;
                m_bIsRowDrag = SwTab::ROWSEL_HORI == nMouseTabCol||
                            SwTab::ROWSEL_HORI_RTL == nMouseTabCol ||
                            SwTab::COLSEL_VERT == nMouseTabCol;
                m_bMBPressed = true;
                CaptureMouse();
            }
            return;
        }

        if ( !rSh.IsTableMode() )
        {
            // comes from table columns out of the document.
            if(SwTab::COL_VERT == nMouseTabCol || SwTab::COL_HORI == nMouseTabCol)
                m_rView.SetTabColFromDoc( true );
            else
                m_rView.SetTabRowFromDoc( true );

            m_rView.SetTabColFromDocPos( aDocPos );
            m_rView.InvalidateRulerPos();
            SfxBindings& rBind = m_rView.GetViewFrame().GetBindings();
            rBind.Update();
            if (RulerColumnDrag(
                    aMEvt, (SwTab::COL_VERT == nMouseTabCol || SwTab::ROW_HORI == nMouseTabCol)))
            {
                m_rView.SetTabColFromDoc( false );
                m_rView.SetTabRowFromDoc( false );
                m_rView.InvalidateRulerPos();
                rBind.Update();
                bCallBase = false;
            }
            else
            {
                return;
            }
        }
    }
    else if (bTmp &&
             rSh.IsNumLabel(aDocPos))
    {
        SwTextNode* pNodeAtPos = rSh.GetNumRuleNodeAtPos( aDocPos );
        m_rView.SetNumRuleNodeFromDoc( pNodeAtPos );
        m_rView.InvalidateRulerPos();
        SfxBindings& rBind = m_rView.GetViewFrame().GetBindings();
        rBind.Update();

        if (RulerMarginDrag(aMEvt, SwFEShell::IsVerticalModeAtNdAndPos(*pNodeAtPos, aDocPos)))
        {
            m_rView.SetNumRuleNodeFromDoc( nullptr );
            m_rView.InvalidateRulerPos();
            rBind.Update();
            bCallBase = false;
        }
        else
        {
            // Make sure the pointer is set to 0, otherwise it may point to
            // nowhere after deleting the corresponding text node.
            m_rView.SetNumRuleNodeFromDoc( nullptr );
            return;
        }
    }

    if ( rSh.IsInSelect() )
        rSh.EndSelect();

    // query against LEFT because otherwise for example also a right
    // click releases the selection.
    if (MOUSE_LEFT == aMEvt.GetButtons())
    {
        bool bOnlyText = false;
        m_bMBPressed = true;
        g_bNoInterrupt = true;
        m_nKS_NUMDOWN_Count = 0;

        CaptureMouse();

        // reset cursor position if applicable
        rSh.ResetCursorStack();

        switch (aMEvt.GetModifier() + aMEvt.GetButtons())
        {
            case MOUSE_LEFT:
            case MOUSE_LEFT + KEY_SHIFT:
            case MOUSE_LEFT + KEY_MOD2:
                if( rSh.GetSelectedObjCount() )
                {
                    SdrHdl* pHdl;
                    if( !bIsViewReadOnly &&
                        !m_pAnchorMarker &&
                        pSdrView &&
                        nullptr != ( pHdl = pSdrView->PickHandle(aDocPos) ) &&
                            ( pHdl->GetKind() == SdrHdlKind::Anchor ||
                              pHdl->GetKind() == SdrHdlKind::Anchor_TR ) )
                    {
                        // #i121463# Set selected during drag
                        pHdl->SetSelected();
                        m_pAnchorMarker.reset( new SwAnchorMarker( pHdl ) );
                        UpdatePointer(aDocPos, aMEvt.GetModifier());
                        return;
                    }
                }
                if (EnterDrawMode(aMEvt, aDocPos))
                {
                    g_bNoInterrupt = false;
                    return;
                }
                else  if ( m_rView.GetDrawFuncPtr() && m_bInsFrame )
                {
                    StopInsFrame();
                    rSh.Edit();
                }

                // Without SHIFT because otherwise Toggle doesn't work at selection
                if (aMEvt.GetClicks() == 1)
                {
                    if ( rSh.IsSelFrameMode())
                    {
                        SdrHdl* pHdl = rSh.GetDrawView()->PickHandle(aDocPos);
                        bool bHitHandle = pHdl && pHdl->GetKind() != SdrHdlKind::Anchor &&
                                                  pHdl->GetKind() != SdrHdlKind::Anchor_TR;

                        if ((rSh.IsInsideSelectedObj(aDocPos) || bHitHandle)
                            && (aMEvt.GetModifier() != KEY_SHIFT || bHitHandle))
                        {
                            rSh.EnterSelFrameMode( &aDocPos );
                            if ( !m_pApplyTempl )
                            {
                                // only if no position to size was hit.
                                if (!bHitHandle)
                                {
                                    StartDDTimer();
                                    SwEditWin::s_nDDStartPosY = aDocPos.Y();
                                    SwEditWin::s_nDDStartPosX = aDocPos.X();
                                }
                                g_bFrameDrag = true;
                            }
                            g_bNoInterrupt = false;
                            return;
                        }
                    }
                }
        }

        bool bExecHyperlinks = m_rView.GetDocShell()->IsReadOnly();
        if ( !bExecHyperlinks )
        {
            const bool bSecureOption = SvtSecurityOptions::IsOptionSet( SvtSecurityOptions::EOption::CtrlClickHyperlink );
            if ((bSecureOption && aMEvt.GetModifier() == KEY_MOD1)
                || (!bSecureOption && aMEvt.GetModifier() != KEY_MOD1))
                bExecHyperlinks = true;
        }

        // Enhanced selection
        sal_uInt8 nNumberOfClicks = static_cast<sal_uInt8>(aMEvt.GetClicks() % 4);
        if (0 == nNumberOfClicks && 0 < aMEvt.GetClicks())
            nNumberOfClicks = 4;

        bool bExecDrawTextLink = false;

        switch (aMEvt.GetModifier() + aMEvt.GetButtons())
        {
            case MOUSE_LEFT:
            case MOUSE_LEFT + KEY_MOD1:
            case MOUSE_LEFT + KEY_MOD2:
            {

                // fdo#79604: first, check if a link has been clicked - do not
                // select fly in this case!
                if (1 == nNumberOfClicks)
                {
                    UpdatePointer(aDocPos, aMEvt.GetModifier());
                    SwEditWin::s_nDDStartPosY = aDocPos.Y();
                    SwEditWin::s_nDDStartPosX = aDocPos.X();

                    // hit a URL in DrawText object?
                    if (bExecHyperlinks && pSdrView)
                    {
                        SdrViewEvent aVEvt;
                        pSdrView->PickAnything(aMEvt, SdrMouseEventKind::BUTTONDOWN, aVEvt);

                        if (aVEvt.meEvent == SdrEventKind::ExecuteUrl)
                            bExecDrawTextLink = true;
                    }
                }

                if (1 == nNumberOfClicks && !bExecDrawTextLink)
                {
                    // only try to select frame, if pointer already was
                    // switched accordingly
                    if ( m_aActHitType != SdrHitKind::NONE && !rSh.IsSelFrameMode() &&
                        !GetView().GetViewFrame().GetDispatcher()->IsLocked())
                    {
                        // Test if there is a draw object at that position and if it should be selected.
                        bool bSelectFrameInsteadOfCroppedImage = false;
                        bool bShould = rSh.ShouldObjectBeSelected(aDocPos, &bSelectFrameInsteadOfCroppedImage);

                        if(bShould)
                        {
                            m_rView.NoRotate();
                            rSh.HideCursor();

                            bool bUnLockView = !rSh.IsViewLocked();
                            rSh.LockView( true );
                            bool bSelObj
                                = rSh.SelectObj(aDocPos, aMEvt.IsMod1() ? SW_ENTER_GROUP : 0);
                            if ( bSelObj && bSelectFrameInsteadOfCroppedImage && pSdrView )
                            {
                                bool bWrapped(false);
                                const SdrObject* pFly = rSh.GetBestObject(false, GotoObjFlags::FlyAny, true, nullptr, &bWrapped);
                                pSdrView->UnmarkAllObj();
                                bSelObj =
                                    rSh.SelectObj(aDocPos, aMEvt.IsMod1() ? SW_ENTER_GROUP : 0, const_cast<SdrObject*>(pFly));
                            }
                            if( bUnLockView )
                                rSh.LockView( false );

                            if( bSelObj )
                            {
                                // if the frame was deselected in the macro
                                // the cursor just has to be displayed again
                                if( FrameTypeFlags::NONE == rSh.GetSelFrameType() )
                                    rSh.ShowCursor();
                                else
                                {
                                    if (rSh.IsFrameSelected() && m_rView.GetDrawFuncPtr())
                                    {
                                        m_rView.GetDrawFuncPtr()->Deactivate();
                                        m_rView.SetDrawFuncPtr(nullptr);
                                        m_rView.LeaveDrawCreate();
                                        m_rView.AttrChangedNotify(nullptr);
                                    }

                                    rSh.EnterSelFrameMode( &aDocPos );
                                    g_bFrameDrag = true;
                                    UpdatePointer(aDocPos, aMEvt.GetModifier());
                                }
                                return;
                            }
                            else
                                bOnlyText = rSh.IsObjSelectable( aDocPos );

                            if (!m_rView.GetDrawFuncPtr())
                                rSh.ShowCursor();
                        }
                        else
                            bOnlyText = KEY_MOD1 != aMEvt.GetModifier();
                    }
                    else if ( rSh.IsSelFrameMode() &&
                              (m_aActHitType == SdrHitKind::NONE ||
                               !rSh.IsInsideSelectedObj( aDocPos )))
                    {
                        m_rView.NoRotate();
                        SdrHdl *pHdl;
                        if( !bIsViewReadOnly && !m_pAnchorMarker && nullptr !=
                            ( pHdl = pSdrView->PickHandle(aDocPos) ) &&
                                ( pHdl->GetKind() == SdrHdlKind::Anchor ||
                                  pHdl->GetKind() == SdrHdlKind::Anchor_TR ) )
                        {
                            m_pAnchorMarker.reset( new SwAnchorMarker( pHdl ) );
                            UpdatePointer(aDocPos, aMEvt.GetModifier());
                            return;
                        }
                        else
                        {
                            bool bUnLockView = !rSh.IsViewLocked();
                            rSh.LockView( true );
                            sal_uInt8 nFlag = aMEvt.IsShift() ? SW_ADD_SELECT : 0;
                            if (aMEvt.IsMod1())
                                nFlag = nFlag | SW_ENTER_GROUP;

                            if ( rSh.IsSelFrameMode() )
                            {
                                rSh.UnSelectFrame();
                                rSh.LeaveSelFrameMode();
                                m_rView.AttrChangedNotify(nullptr);
                            }

                            bool bSelObj = rSh.SelectObj( aDocPos, nFlag );
                            if( bUnLockView )
                                rSh.LockView( false );

                            if( !bSelObj )
                            {
                                // move cursor here so that it is not drawn in the
                                // frame first; ShowCursor() happens in LeaveSelFrameMode()
                                g_bValidCursorPos = !(CRSR_POSCHG & rSh.CallSetCursor(&aDocPos, false));
                                rSh.LeaveSelFrameMode();
                                m_rView.AttrChangedNotify(nullptr);
                                bCallBase = false;
                            }
                            else
                            {
                                rSh.HideCursor();
                                rSh.EnterSelFrameMode( &aDocPos );
                                rSh.SelFlyGrabCursor();
                                rSh.MakeSelVisible();
                                g_bFrameDrag = true;
                                if( rSh.IsFrameSelected() &&
                                    m_rView.GetDrawFuncPtr() )
                                {
                                    m_rView.GetDrawFuncPtr()->Deactivate();
                                    m_rView.SetDrawFuncPtr(nullptr);
                                    m_rView.LeaveDrawCreate();
                                    m_rView.AttrChangedNotify(nullptr);
                                }
                                UpdatePointer(aDocPos, aMEvt.GetModifier());
                                return;
                            }
                        }
                    }
                }

                switch ( nNumberOfClicks )
                {
                    case 1:
                        break;
                    case 2:
                    {
                        g_bFrameDrag = false;
                        if (!bIsViewReadOnly && rSh.IsInsideSelectedObj(aDocPos)
                            && (FlyProtectFlags::NONE
                                    == rSh.IsSelObjProtected(FlyProtectFlags::Content
                                                             | FlyProtectFlags::Parent)
                                || rSh.GetSelectionType() == SelectionType::Ole))
                        {
                        /* This is no good: on the one hand GetSelectionType is used as flag field
                         * (take a look into the GetSelectionType method) and on the other hand the
                         * return value is used in a switch without proper masking (very nice), this must lead to trouble
                         */
                            switch ( rSh.GetSelectionType() & ~SelectionType( SelectionType::FontWork | SelectionType::ExtrudedCustomShape ) )
                            {
                            case SelectionType::Graphic:
                                ResetMouseButtonDownFlags();
                                if (!comphelper::LibreOfficeKit::isActive())
                                {
                                    GetView().GetViewFrame().GetBindings().Execute(
                                        FN_FORMAT_GRAFIC_DLG, nullptr,
                                        SfxCallMode::RECORD|SfxCallMode::SLOT);
                                }
                                return;

                            // double click on OLE object --> OLE-InPlace
                            case SelectionType::Ole:
                                ResetMouseButtonDownFlags();
                                rSh.LaunchOLEObj();
                                return;

                            case SelectionType::Frame:
                                ResetMouseButtonDownFlags();
                                if (!comphelper::LibreOfficeKit::isActive())
                                {
                                    GetView().GetViewFrame().GetBindings().Execute(
                                        FN_FORMAT_FRAME_DLG, nullptr,
                                        SfxCallMode::RECORD|SfxCallMode::SLOT);
                                }
                                return;

                            case SelectionType::DrawObject:
                                ResetMouseButtonDownFlags();
                                EnterDrawTextMode(aDocPos);
                                if ( auto pSwDrawTextShell = dynamic_cast< SwDrawTextShell *>(  m_rView.GetCurShell() )  )
                                    pSwDrawTextShell->Init();
                                return;

                            default: break;
                            }
                        }

                        // if the cursor position was corrected or if a Fly
                        // was selected in ReadOnlyMode, no word selection, except when tiled rendering.
                        if ((!g_bValidCursorPos || rSh.IsFrameSelected()) && !comphelper::LibreOfficeKit::isActive())
                            return;

                        SwField *pField = rSh.GetCurField(true);
                        bool bFootnote = false;

                        if( nullptr != pField ||
                              ( bFootnote = rSh.GetCurFootnote() ))
                        {
                            if (!bIsViewReadOnly)
                            {
                                ResetMouseButtonDownFlags();
                                if( bFootnote )
                                    GetView().GetViewFrame().GetBindings().Execute( FN_EDIT_FOOTNOTE );
                                else
                                {
                                    SwFieldTypesEnum nTypeId = pField->GetTypeId();
                                    SfxViewFrame& rVFrame = GetView().GetViewFrame();
                                    switch( nTypeId )
                                    {
                                    case SwFieldTypesEnum::Postit:
                                    case SwFieldTypesEnum::Script:
                                    {
                                        // if it's a Readonly region, status has to be enabled
                                        sal_uInt16 nSlot = SwFieldTypesEnum::Postit == nTypeId ? FN_POSTIT : FN_JAVAEDIT;
                                        SfxBoolItem aItem(nSlot, true);
                                        rVFrame.GetBindings().SetState(aItem);
                                        rVFrame.GetBindings().Execute(nSlot);
                                        break;
                                    }
                                    case SwFieldTypesEnum::Authority :
                                        rVFrame.GetBindings().Execute(FN_EDIT_AUTH_ENTRY_DLG);
                                    break;
                                    case SwFieldTypesEnum::Input:
                                    case SwFieldTypesEnum::Dropdown:
                                    case SwFieldTypesEnum::SetInput:
                                        rVFrame.GetBindings().Execute(FN_UPDATE_INPUTFIELDS);
                                        break;
                                    default:
                                        rVFrame.GetBindings().Execute(FN_EDIT_FIELD);
                                    }
                                }
                                return;
                            }
                            else if (pField && pField->ExpandField(true, nullptr).getLength())
                            {
                                ResetMouseButtonDownFlags();
                                GetView().GetViewFrame().GetBindings().Execute(FN_COPY_FIELD);
                            }
                        }
                        // in extended mode double and triple
                        // click has no effect.
                        if ( rSh.IsExtMode() || rSh.IsBlockMode() )
                            return;

                        // select word, AdditionalMode if applicable
                        if (KEY_MOD1 == aMEvt.GetModifier() && !rSh.IsAddMode())
                        {
                            rSh.EnterAddMode();
                            rSh.SelWrd( &aDocPos );
                            rSh.LeaveAddMode();
                        }
                        else
                        {
                            if (!rSh.SelWrd(&aDocPos) && comphelper::LibreOfficeKit::isActive())
                                // Double click did not select any word: try to
                                // select the current cell in case we are in a
                                // table.
                                rSh.SelTableBox();
                        }

                        SwContentAtPos aContentAtPos(IsAttrAtPos::FormControl);
                        if( rSh.GetContentAtPos( aDocPos, aContentAtPos ) &&
                                aContentAtPos.aFnd.pFieldmark != nullptr)
                        {
                            Fieldmark *pFieldBM = const_cast< Fieldmark* > ( aContentAtPos.aFnd.pFieldmark );
                            if ( pFieldBM->GetFieldname( ) == ODF_FORMDROPDOWN || pFieldBM->GetFieldname( ) == ODF_FORMDATE )
                            {
                                ResetMouseButtonDownFlags();
                                rSh.getIDocumentMarkAccess()->ClearFieldActivation();
                                GetView().GetViewFrame().GetBindings().Execute(SID_FM_CTL_PROPERTIES);
                                return;
                            }
                        }

                        // tdf#143158 - handle alphabetical index entries
                        SwContentAtPos aToxContentAtPos(IsAttrAtPos::ToxMark);
                        if (rSh.GetContentAtPos(aDocPos, aToxContentAtPos))
                        {
                            const OUString sToxText = aToxContentAtPos.sStr;
                            if (!sToxText.isEmpty() && aToxContentAtPos.pFndTextAttr)
                            {
                                const SwTOXType* pTType
                                    = aToxContentAtPos.pFndTextAttr->GetTOXMark().GetTOXType();
                                if (pTType && pTType->GetType() == TOXTypes::TOX_INDEX)
                                {
                                    ResetMouseButtonDownFlags();
                                    GetView().GetViewFrame().GetBindings().Execute(
                                        FN_EDIT_IDX_ENTRY_DLG);
                                    return;
                                }
                            }
                        }

                        g_bHoldSelection = true;
                        return;
                    }
                    case 3:
                    case 4:
                    {
                        g_bFrameDrag = false;
                        // in extended mode double and triple
                        // click has no effect.
                        if ( rSh.IsExtMode() )
                            return;

                        // if the cursor position was corrected or if a Fly
                        // was selected in ReadOnlyMode, no word selection.
                        if ( !g_bValidCursorPos || rSh.IsFrameSelected() )
                            return;

                        // select line, AdditionalMode if applicable
                        const bool bMod = KEY_MOD1 == aMEvt.GetModifier() && !rSh.IsAddMode();

                        if ( bMod )
                            rSh.EnterAddMode();

                        // Enhanced selection
                        if ( 3 == nNumberOfClicks )
                            rSh.SelSentence( &aDocPos );
                        else
                            rSh.SelPara( &aDocPos );

                        if ( bMod )
                            rSh.LeaveAddMode();

                        g_bHoldSelection = true;
                        return;
                    }

                    default:
                        return;
                }

                [[fallthrough]];
            }
            case MOUSE_LEFT + KEY_SHIFT:
            case MOUSE_LEFT + KEY_SHIFT + KEY_MOD1:
            {
                bool bLockView = m_bWasShdwCursor;

                switch (aMEvt.GetModifier())
                {
                    case KEY_MOD1 + KEY_SHIFT:
                    {
                        if ( !m_bInsDraw && IsDrawObjSelectable( rSh, aDocPos ) )
                        {
                            m_rView.NoRotate();
                            rSh.HideCursor();
                            if ( rSh.IsSelFrameMode() )
                                rSh.SelectObj(aDocPos, SW_ADD_SELECT | SW_ENTER_GROUP);
                            else
                            {   if ( rSh.SelectObj( aDocPos, SW_ADD_SELECT | SW_ENTER_GROUP ) )
                                {
                                    rSh.EnterSelFrameMode( &aDocPos );
                                    SwEditWin::s_nDDStartPosY = aDocPos.Y();
                                    SwEditWin::s_nDDStartPosX = aDocPos.X();
                                    g_bFrameDrag = true;
                                    return;
                                }
                            }
                        }
                        else if( rSh.IsSelFrameMode() &&
                                 rSh.GetDrawView()->PickHandle( aDocPos ))
                        {
                            g_bFrameDrag = true;
                            g_bNoInterrupt = false;
                            return;
                        }
                    }
                    break;
                    case KEY_MOD1:
                    if ( !bExecDrawTextLink )
                    {
                        if (rSh.GetViewOptions()->IsShowOutlineContentVisibilityButton())
                        {
                            // ctrl+left-click on outline node frame
                            SwContentAtPos aContentAtPos(IsAttrAtPos::Outline);
                            if(rSh.GetContentAtPos(aDocPos, aContentAtPos))
                            {
                                SwOutlineNodes::size_type nPos;
                                if (rSh.GetNodes().GetOutLineNds().Seek_Entry(aContentAtPos.aFnd.pNode, &nPos))
                                {
                                    ToggleOutlineContentVisibility(nPos, false);
                                    return;
                                }
                            }
                        }
                        if ( !m_bInsDraw && IsDrawObjSelectable( rSh, aDocPos ) && !lcl_urlOverBackground( rSh, aDocPos ) )
                        {
                            m_rView.NoRotate();
                            rSh.HideCursor();
                            if ( rSh.IsSelFrameMode() )
                                rSh.SelectObj(aDocPos, SW_ENTER_GROUP);
                            else
                            {   if ( rSh.SelectObj( aDocPos, SW_ENTER_GROUP ) )
                                {
                                    rSh.EnterSelFrameMode( &aDocPos );
                                    SwEditWin::s_nDDStartPosY = aDocPos.Y();
                                    SwEditWin::s_nDDStartPosX = aDocPos.X();
                                    g_bFrameDrag = true;
                                    return;
                                }
                            }
                        }
                        else if( rSh.IsSelFrameMode() &&
                                 rSh.GetDrawView()->PickHandle( aDocPos ))
                        {
                            g_bFrameDrag = true;
                            g_bNoInterrupt = false;
                            return;
                        }
                        else
                        {
                            if ( !rSh.IsAddMode() && !rSh.IsExtMode() && !rSh.IsBlockMode() )
                            {
                                rSh.PushMode();
                                g_bModePushed = true;

                                bool bUnLockView = !rSh.IsViewLocked();
                                rSh.LockView( true );
                                rSh.EnterAddMode();
                                if( bUnLockView )
                                    rSh.LockView( false );
                            }
                            bCallBase = false;
                        }
                    }
                    break;
                    case KEY_MOD2:
                    {
                        if ( !rSh.IsAddMode() && !rSh.IsExtMode() && !rSh.IsBlockMode() )
                        {
                            rSh.PushMode();
                            g_bModePushed = true;
                            bool bUnLockView = !rSh.IsViewLocked();
                            rSh.LockView( true );
                            rSh.EnterBlockMode();
                            if( bUnLockView )
                                rSh.LockView( false );
                        }
                        bCallBase = false;
                    }
                    break;
                    case KEY_SHIFT:
                    {
                        if (nNumberOfClicks == 2)
                        {
                            // Left mouse button, shift, double-click: see if we have a graphic and
                            // dispatch its dialog in this case.
                            if (rSh.GetSelectionType() == SelectionType::Graphic)
                            {
                                GetView().GetViewFrame().GetBindings().Execute(
                                    FN_FORMAT_GRAFIC_DLG, nullptr,
                                    SfxCallMode::RECORD | SfxCallMode::SLOT);
                                return;
                            }
                        }

                        if ( !m_bInsDraw && IsDrawObjSelectable( rSh, aDocPos ) )
                        {
                            m_rView.NoRotate();
                            rSh.HideCursor();
                            if ( rSh.IsSelFrameMode() )
                            {
                                rSh.SelectObj(aDocPos, SW_ADD_SELECT);

                                const SdrMarkList& rMarkList = pSdrView->GetMarkedObjectList();
                                if (rMarkList.GetMark(0) == nullptr)
                                {
                                    rSh.LeaveSelFrameMode();
                                    m_rView.AttrChangedNotify(nullptr);
                                    g_bFrameDrag = false;
                                }
                            }
                            else
                            {   if ( rSh.SelectObj( aDocPos ) )
                                {
                                    rSh.EnterSelFrameMode( &aDocPos );
                                    SwEditWin::s_nDDStartPosY = aDocPos.Y();
                                    SwEditWin::s_nDDStartPosX = aDocPos.X();
                                    g_bFrameDrag = true;
                                    return;
                                }
                            }
                        }
                        else
                        {
                            bool bShould = rSh.ShouldObjectBeSelected(aDocPos);
                            if (bShould)
                            {
                                // Left mouse button, shift, non-double-click, not a draw object and
                                // have an object to select: select it.
                                rSh.HideCursor();
                                bool bSelObj = rSh.SelectObj(aDocPos);
                                if (bSelObj)
                                {
                                    rSh.EnterSelFrameMode(&aDocPos);
                                }
                                return;
                            }

                            if ( rSh.IsSelFrameMode() &&
                                 rSh.IsInsideSelectedObj( aDocPos ) )
                            {
                                rSh.EnterSelFrameMode( &aDocPos );
                                SwEditWin::s_nDDStartPosY = aDocPos.Y();
                                SwEditWin::s_nDDStartPosX = aDocPos.X();
                                g_bFrameDrag = true;
                                return;
                            }
                            if ( rSh.IsSelFrameMode() )
                            {
                                rSh.UnSelectFrame();
                                rSh.LeaveSelFrameMode();
                                m_rView.AttrChangedNotify(nullptr);
                                g_bFrameDrag = false;
                            }
                            if ( !rSh.IsExtMode() )
                            {
                                // don't start a selection when an
                                // URL field or a graphic is clicked
                                bool bSttSelect = rSh.HasSelection() ||
                                                PointerStyle::RefHand != GetPointer();

                                if( !bSttSelect )
                                {
                                    bSttSelect = true;
                                    if( bExecHyperlinks )
                                    {
                                        SwContentAtPos aContentAtPos(
                                            IsAttrAtPos::Footnote |
                                            IsAttrAtPos::InetAttr );

                                        if( rSh.GetContentAtPos( aDocPos, aContentAtPos ) )
                                        {
                                            if( !rSh.IsViewLocked() &&
                                                !rSh.IsReadOnlyAvailable() &&
                                                aContentAtPos.IsInProtectSect() )
                                                    bLockView = true;

                                            bSttSelect = false;
                                        }
                                        else if( rSh.IsURLGrfAtPos( aDocPos ))
                                            bSttSelect = false;
                                    }
                                }

                                if( bSttSelect )
                                    rSh.SttSelect();
                            }
                        }
                        bCallBase = false;
                        break;
                    }
                    default:
                        if( !rSh.IsViewLocked() )
                        {
                            SwContentAtPos aContentAtPos( IsAttrAtPos::ClickField |
                                                        IsAttrAtPos::InetAttr );
                            if( rSh.GetContentAtPos( aDocPos, aContentAtPos ) &&
                                !rSh.IsReadOnlyAvailable() &&
                                aContentAtPos.IsInProtectSect() )
                                bLockView = true;
                        }
                }

                if ( rSh.IsGCAttr() )
                {
                    rSh.GCAttr();
                    rSh.ClearGCAttr();
                }

                SwContentAtPos aFieldAtPos(IsAttrAtPos::Field);
                bool bEditableFieldClicked = false;

                // Are we clicking on a field?
                if (rSh.GetContentAtPos(aDocPos, aFieldAtPos))
                {
                    bool bEditableField = (aFieldAtPos.pFndTextAttr != nullptr
                        && aFieldAtPos.pFndTextAttr->Which() == RES_TXTATR_INPUTFIELD);

                    if (!bEditableField)
                    {
                        rSh.CallSetCursor(&aDocPos, bOnlyText);
                        // Unfortunately the cursor may be on field
                        // position or on position after field depending on which
                        // half of the field was clicked on.
                        SwTextAttr const*const pTextField(aFieldAtPos.pFndTextAttr);
                        if (pTextField &&
                            rSh.GetCurrentShellCursor().GetPoint()->GetContentIndex() != pTextField->GetStart())
                        {
                            assert(rSh.GetCurrentShellCursor().GetPoint()->GetContentIndex() == (pTextField->GetStart() + 1));
                            rSh.Left( SwCursorSkipMode::Chars, false, 1, false );
                        }
                        // don't go into the !bOverSelect block below - it moves
                        // the cursor
                        break;
                    }
                    else
                    {
                        bEditableFieldClicked = true;
                    }
                }

                bool bOverSelect = rSh.TestCurrPam( aDocPos );
                bool bOverURLGrf = false;
                if( !bOverSelect )
                    bOverURLGrf = bOverSelect = nullptr != rSh.IsURLGrfAtPos( aDocPos );

                if ( !bOverSelect || rSh.IsInSelect() )
                {
                    MoveCursor( rSh, aDocPos, bOnlyText, bLockView);
                    bCallBase = false;
                }
                if (!bOverURLGrf && !bExecDrawTextLink && !bOnlyText)
                {
                    const SelectionType nSelType = rSh.GetSelectionType();
                    // Check in general, if an object is selectable at given position.
                    // Thus, also text fly frames in background become selectable via Ctrl-Click.
                    if ( ( nSelType & SelectionType::Ole ||
                         nSelType & SelectionType::Graphic ||
                         rSh.IsObjSelectable( aDocPos ) ) && !lcl_urlOverBackground( rSh, aDocPos ) )
                    {
                        SwMvContext aMvContext( &rSh );
                        rSh.EnterSelFrameMode();
                        bCallBase = false;
                    }
                }
                if ( !bOverSelect && bEditableFieldClicked && (!pCursorField ||
                     pCursorField != aFieldAtPos.pFndTextAttr->GetFormatField().GetField()))
                {
                    // select content of Input Field, but exclude CH_TXT_ATR_INPUTFIELDSTART
                    // and CH_TXT_ATR_INPUTFIELDEND
                    rSh.SttSelect();
                    rSh.SelectTextModel( aFieldAtPos.pFndTextAttr->GetStart() + 1,
                                 *(aFieldAtPos.pFndTextAttr->End()) - 1 );
                }
                // don't reset here any longer so that, in case through MouseMove
                // with pressed Ctrl key a multiple-selection should happen,
                // the previous selection is not released in Drag.
                break;
            }
        }
    }
    else if (MOUSE_RIGHT == aMEvt.GetButtons())
    {
        // If right-click while dragging to resize the comment width, stop resizing
        if (mbIsDragSidebar)
        {
            ReleaseCommentGuideLine();
            ReleaseMouse();
            return;
        }

        if (rSh.GetViewOptions()->IsShowOutlineContentVisibilityButton()
            && aMEvt.GetModifier() == KEY_MOD1)
        {
            // ctrl+right-click on outline node frame
            SwContentAtPos aContentAtPos(IsAttrAtPos::Outline);
            if(rSh.GetContentAtPos(aDocPos, aContentAtPos))
            {
                SwOutlineNodes::size_type nPos;
                if (rSh.GetNodes().GetOutLineNds().Seek_Entry(aContentAtPos.aFnd.pNode, &nPos))
                {
                    ToggleOutlineContentVisibility(nPos, !rSh.GetViewOptions()->IsTreatSubOutlineLevelsAsContent());
                    return;
                }
            }
        }
        else if (!aMEvt.GetModifier() && static_cast<sal_uInt8>(aMEvt.GetClicks() % 4) == 1
                 && !rSh.TestCurrPam(aDocPos))
        {
            SwContentAtPos aFieldAtPos(IsAttrAtPos::Field);

            // Are we clicking on a field?
            if (g_bValidCursorPos
                    && rSh.GetContentAtPos(aDocPos, aFieldAtPos)
                    && aFieldAtPos.pFndTextAttr != nullptr
                    && aFieldAtPos.pFndTextAttr->Which() == RES_TXTATR_INPUTFIELD
                    && (!pCursorField || pCursorField != aFieldAtPos.pFndTextAttr->GetFormatField().GetField()))
            {
                // Move the cursor
                MoveCursor( rSh, aDocPos, rSh.IsObjSelectable( aDocPos ), m_bWasShdwCursor );
                bCallBase = false;

                // select content of Input Field, but exclude CH_TXT_ATR_INPUTFIELDSTART
                // and CH_TXT_ATR_INPUTFIELDEND
                rSh.SttSelect();
                rSh.SelectTextModel( aFieldAtPos.pFndTextAttr->GetStart() + 1,
                                *(aFieldAtPos.pFndTextAttr->End()) - 1 );
            }
        }
    }

    if (bCallBase)
        Window::MouseButtonDown(aMEvt);
}

bool SwEditWin::changeMousePointer(Point const & rDocPoint)
{
    SwWrtShell & rShell = m_rView.GetWrtShell();

    SwTab nMouseTabCol;

    if ( SwTab::COL_NONE != (nMouseTabCol = rShell.WhichMouseTabCol( rDocPoint ) ) &&
         ( !rShell.IsObjSelectable( rDocPoint ) ||
             // allow resizing row height, if the image is anchored as character in the cell
             !( SwTab::COL_VERT == nMouseTabCol || SwTab::COL_HORI == nMouseTabCol ) ) )
    {
        PointerStyle nPointer = PointerStyle::Null;
        bool bChkTableSel = false;

        switch ( nMouseTabCol )
        {
            case SwTab::COL_VERT :
            case SwTab::ROW_HORI :
                nPointer = PointerStyle::VSizeBar;
                bChkTableSel = true;
                break;
            case SwTab::ROW_VERT :
            case SwTab::COL_HORI :
                nPointer = PointerStyle::HSizeBar;
                bChkTableSel = true;
                break;
            // Enhanced table selection
            case SwTab::SEL_HORI :
                nPointer = PointerStyle::TabSelectSE;
                break;
            case SwTab::SEL_HORI_RTL :
            case SwTab::SEL_VERT :
                nPointer = PointerStyle::TabSelectSW;
                break;
            case SwTab::COLSEL_HORI :
            case SwTab::ROWSEL_VERT :
                nPointer = PointerStyle::TabSelectS;
                break;
            case SwTab::ROWSEL_HORI :
                nPointer = PointerStyle::TabSelectE;
                break;
            case SwTab::ROWSEL_HORI_RTL :
            case SwTab::COLSEL_VERT :
                nPointer = PointerStyle::TabSelectW;
                break;
            default: break; // prevent compiler warning
        }

        if ( PointerStyle::Null != nPointer &&
            // i#35543 - Enhanced table selection is explicitly allowed in table mode
            ( !bChkTableSel || !rShell.IsTableMode() ) &&
            !comphelper::LibreOfficeKit::isActive() )
        {
            SetPointer( nPointer );
        }

        return true;
    }
    else if (rShell.IsNumLabel(rDocPoint, RULER_MOUSE_MARGINWIDTH))
    {
        // i#42921 - consider vertical mode
        SwTextNode* pNodeAtPos = rShell.GetNumRuleNodeAtPos( rDocPoint );
        const PointerStyle nPointer =
                SwFEShell::IsVerticalModeAtNdAndPos( *pNodeAtPos, rDocPoint )
                ? PointerStyle::VSizeBar
                : PointerStyle::HSizeBar;
        SetPointer( nPointer );

        return true;
    }
    return false;
}

void SwEditWin::MouseMove(const MouseEvent& _rMEvt)
{
    MouseEvent rMEvt(_rMEvt);

    if (comphelper::LibreOfficeKit::isActive())
    {
        if (vcl::Window* pWindow = m_rView.GetPostItMgr()->IsHitSidebarWindow(rMEvt.GetPosPixel()))
        {
            pWindow->MouseMove(rMEvt);
            return;
        }
    }

    if (m_rView.GetPostItMgr()->IsHitSidebarDragArea(rMEvt.GetPosPixel()))
    {
        SetPointer(PointerStyle::HSizeBar);
        return;
    }

    if (mbIsDragSidebar)
    {
        DrawCommentGuideLine(rMEvt.GetPosPixel());
        return;
    }

    //ignore key modifiers for format paintbrush
    {
        bool bExecFormatPaintbrush = m_pApplyTempl && m_pApplyTempl->m_pFormatClipboard
                                &&  m_pApplyTempl->m_pFormatClipboard->HasContent();
        if( bExecFormatPaintbrush )
            rMEvt = MouseEvent( _rMEvt.GetPosPixel(), _rMEvt.GetClicks(),
                                    _rMEvt.GetMode(), _rMEvt.GetButtons() );
    }

    // as long as an action is running the MouseMove should be disconnected
    // otherwise bug 40102 occurs
    SwWrtShell &rSh = m_rView.GetWrtShell();
    if( rSh.ActionPend() )
        return ;

    if (rSh.GetViewOptions()->IsShowOutlineContentVisibilityButton())
    {
        // add/remove outline content hide button
        const SwNodes& rNds = rSh.GetDoc()->GetNodes();
        SwOutlineNodes::size_type nPos;
        SwContentAtPos aSwContentAtPos(IsAttrAtPos::Outline);
        if (rSh.GetContentAtPos(PixelToLogic(rMEvt.GetPosPixel()), aSwContentAtPos))
        {
            // mouse pointer is on an outline paragraph node
            if(aSwContentAtPos.aFnd.pNode && aSwContentAtPos.aFnd.pNode->IsTextNode())
            {
                // Get the outline paragraph frame and compare it to the saved outline frame. If they
                // are not the same, remove the fold button from the saved outline frame, if not
                // already removed, and then add a fold button to the mouse over outline frame if
                // the content is not folded.
                SwContentFrame* pContentFrame =
                        aSwContentAtPos.aFnd.pNode->GetTextNode()->getLayoutFrame(rSh.GetLayout());
                if (pContentFrame != m_pSavedOutlineFrame)
                {
                    if (m_pSavedOutlineFrame)
                    {
                        if (m_pSavedOutlineFrame->isFrameAreaDefinitionValid())
                        {
                            SwTextNode* pTextNode = m_pSavedOutlineFrame->GetTextNodeFirst();
                            if (pTextNode && rNds.GetOutLineNds().Seek_Entry(pTextNode, &nPos) &&
                                    rSh.GetAttrOutlineContentVisible(nPos))
                            {
                                GetFrameControlsManager().RemoveControlsByType(
                                            FrameControlType::Outline, m_pSavedOutlineFrame);
                            }
                        }
                    }
                    m_pSavedOutlineFrame = static_cast<SwTextFrame*>(pContentFrame);
                }
                // show fold button if outline content is visible
                if (rNds.GetOutLineNds().Seek_Entry(aSwContentAtPos.aFnd.pNode->GetTextNode(), &nPos) &&
                        rSh.GetAttrOutlineContentVisible(nPos))
                    GetFrameControlsManager().SetOutlineContentVisibilityButton(pContentFrame);
            }
        }
        else if (m_pSavedOutlineFrame)
        {
            // The saved frame may not still be in the document, e.g., when an outline paragraph
            // is deleted. This causes the call to GetTextNodeFirst to behave badly. Use
            // isFrameAreaDefinitionValid to check if the frame is still in the document.
            if (m_pSavedOutlineFrame->isFrameAreaDefinitionValid())
            {
                // current pointer pos is not over an outline frame
                // previous pointer pos was over an outline frame
                // remove outline content visibility button if showing
                SwTextNode* pTextNode = m_pSavedOutlineFrame->GetTextNodeFirst();
                if (pTextNode && rNds.GetOutLineNds().Seek_Entry(pTextNode, &nPos) &&
                        rSh.GetAttrOutlineContentVisible(nPos))
                {
                    GetFrameControlsManager().RemoveControlsByType(
                                FrameControlType::Outline, m_pSavedOutlineFrame);
                }
            }
            m_pSavedOutlineFrame = nullptr;
        }
    }

    if( m_pShadCursor && 0 != (rMEvt.GetModifier() + rMEvt.GetButtons() ) )
    {
        m_pShadCursor.reset();
    }

    bool bIsViewReadOnly = m_rView.GetDocShell()->IsReadOnly() || (rSh.GetSfxViewShell() && rSh.GetSfxViewShell()->IsLokReadOnlyView());

    CurrShell aCurr( &rSh );

    //aPixPt == Point in Pixel, relative to ChildWin
    //aDocPt == Point in Twips, document coordinates
    const Point aPixPt( rMEvt.GetPosPixel() );
    const Point aDocPt( PixelToLogic( aPixPt ) );

    if ( IsChainMode() )
    {
        UpdatePointer( aDocPt, rMEvt.GetModifier() );
        return;
    }

    SdrView *pSdrView = rSh.GetDrawView();

    const SwCallMouseEvent aLastCallEvent( m_aSaveCallEvent );
    m_aSaveCallEvent.Clear();

    if ( !bIsViewReadOnly && pSdrView && pSdrView->MouseMove(rMEvt,GetOutDev()) )
    {
        SetPointer( PointerStyle::Text );
        return; // evaluate SdrView's event
    }

    const Point aOldPt( rSh.VisArea().Pos() );
    const bool bInsWin = rSh.VisArea().Contains( aDocPt ) || comphelper::LibreOfficeKit::isActive();

    if (rSh.GetViewOptions()->IsShowOutlineContentVisibilityButton())
    {
        if (m_pSavedOutlineFrame && !bInsWin)
        {
            // the mouse pointer has left the building (edit window)
            // remove the outline content visibility button if showing
            if (m_pSavedOutlineFrame->isFrameAreaDefinitionValid())
            {
                const SwNodes& rNds = rSh.GetDoc()->GetNodes();
                SwOutlineNodes::size_type nPos;
                SwTextNode* pTextNode = m_pSavedOutlineFrame->GetTextNodeFirst();
                if (pTextNode && rNds.GetOutLineNds().Seek_Entry(pTextNode, &nPos) &&
                        rSh.GetAttrOutlineContentVisible(nPos))
                {
                    GetFrameControlsManager().RemoveControlsByType(FrameControlType::Outline,
                                                                   m_pSavedOutlineFrame);
                }
            }
            m_pSavedOutlineFrame = nullptr;
        }
    }

    if( m_pShadCursor && !bInsWin )
    {
        m_pShadCursor.reset();
    }

    if( bInsWin && m_xRowColumnSelectionStart )
    {
        EnterArea();
        Point aPos( aDocPt );
        if( rSh.SelectTableRowCol( *m_xRowColumnSelectionStart, &aPos, m_bIsRowDrag ))
            return;
    }

    // position is necessary for OS/2 because obviously after a MB-Down
    // a MB-Move is called immediately.
    if( g_bDDTimerStarted )
    {
        Point aDD( SwEditWin::s_nDDStartPosX, SwEditWin::s_nDDStartPosY );
        aDD = LogicToPixel( aDD );
        tools::Rectangle aRect( aDD.X()-3, aDD.Y()-3, aDD.X()+3, aDD.Y()+3 );
        if ( !aRect.Contains( aPixPt ) )
            StopDDTimer( &rSh, aDocPt );
    }

    if(m_rView.GetDrawFuncPtr())
    {
        if( m_bInsDraw  )
        {
            m_rView.GetDrawFuncPtr()->MouseMove( rMEvt );
            if ( !bInsWin )
            {
                Point aTmp( aDocPt );
                aTmp += rSh.VisArea().Pos() - aOldPt;
                LeaveArea( aTmp );
            }
            else
                EnterArea();
            return;
        }
        else if(!rSh.IsFrameSelected() && !rSh.GetSelectedObjCount())
        {
            SfxBindings &rBnd = rSh.GetView().GetViewFrame().GetBindings();
            Point aRelPos = rSh.GetRelativePagePosition(aDocPt);
            if(aRelPos.X() >= 0)
            {
                FieldUnit eMetric = ::GetDfltMetric(dynamic_cast<SwWebView*>( &GetView())  != nullptr );
                SwModule::get()->PutItem(SfxUInt16Item(SID_ATTR_METRIC, static_cast< sal_uInt16 >(eMetric)));
                const SfxPointItem aTmp1( SID_ATTR_POSITION, aRelPos );
                rBnd.SetState( aTmp1 );
            }
            else
            {
                rBnd.Invalidate(SID_ATTR_POSITION);
            }
            rBnd.Invalidate(SID_ATTR_SIZE);
            const SvxStatusItem aCell( SID_TABLE_CELL, OUString(), StatusCategory::NONE );
            rBnd.SetState( aCell );
        }
    }

    // determine if we only change the mouse pointer and return
    if (!bIsViewReadOnly && bInsWin && !m_pApplyTempl && !rSh.IsInSelect() && changeMousePointer(aDocPt))
    {
        return;
    }

    bool bDelShadCursor = true;

    switch ( rMEvt.GetModifier() + rMEvt.GetButtons() )
    {
        case MOUSE_LEFT:
            if( m_pAnchorMarker )
            {
                // Now we need to refresh the SdrHdl pointer of m_pAnchorMarker.
                // This looks a little bit tricky, but it solves the following
                // problem: the m_pAnchorMarker contains a pointer to an SdrHdl,
                // if the FindAnchorPos-call cause a scrolling of the visible
                // area, it's possible that the SdrHdl will be destroyed and a
                // new one will initialized at the original position(GetHdlPos).
                // So the m_pAnchorMarker has to find the right SdrHdl, if it's
                // the old one, it will find it with position aOld, if this one
                // is destroyed, it will find a new one at position GetHdlPos().

                const Point aOld = m_pAnchorMarker->GetPosForHitTest( *(rSh.GetOut()) );
                Point aNew = rSh.FindAnchorPos( aDocPt );
                SdrHdl* pHdl;
                if( pSdrView && (nullptr!=( pHdl = pSdrView->PickHandle( aOld ) )||
                    nullptr !=(pHdl = pSdrView->PickHandle( m_pAnchorMarker->GetHdlPos()) ) ) &&
                        ( pHdl->GetKind() == SdrHdlKind::Anchor ||
                          pHdl->GetKind() == SdrHdlKind::Anchor_TR ) )
                {
                    m_pAnchorMarker->ChgHdl( pHdl );
                    if( aNew.X() || aNew.Y() )
                    {
                        m_pAnchorMarker->SetPos( aNew );
                        m_pAnchorMarker->SetLastPos( aDocPt );
                    }
                }
                else
                {
                    m_pAnchorMarker.reset();
                }
            }
            if ( m_bInsDraw )
            {
                if ( !m_bMBPressed )
                    break;
                if ( m_bIsInMove || IsMinMove( m_aStartPos, aPixPt ) )
                {
                    if ( !bInsWin )
                        LeaveArea( aDocPt );
                    else
                        EnterArea();
                    if ( m_rView.GetDrawFuncPtr() )
                    {
                        pSdrView->SetOrtho(false);
                        m_rView.GetDrawFuncPtr()->MouseMove( rMEvt );
                    }
                    m_bIsInMove = true;
                }
                return;
            }

            {
            SwWordCountWrapper *pWrdCnt = static_cast<SwWordCountWrapper*>(GetView().GetViewFrame().GetChildWindow(SwWordCountWrapper::GetChildWindowId()));
            if (pWrdCnt)
                pWrdCnt->UpdateCounts();
            }
            [[fallthrough]];

        case MOUSE_LEFT + KEY_SHIFT:
        case MOUSE_LEFT + KEY_SHIFT + KEY_MOD1:
            if ( !m_bMBPressed )
                break;
            [[fallthrough]];
        case MOUSE_LEFT + KEY_MOD1:
            if ( g_bFrameDrag && rSh.IsSelFrameMode() )
            {
                if( !m_bMBPressed )
                    break;

                if ( m_bIsInMove || IsMinMove( m_aStartPos, aPixPt ) )
                {
                    // event processing for resizing
                    if (pSdrView && pSdrView->GetMarkedObjectList().GetMarkCount() != 0)
                    {
                        const Point aSttPt( PixelToLogic( m_aStartPos ) );

                        // can we start?
                        if( SdrHdlKind::User == g_eSdrMoveHdl )
                        {
                            SdrHdl* pHdl = pSdrView->PickHandle( aSttPt );
                            g_eSdrMoveHdl = pHdl ? pHdl->GetKind() : SdrHdlKind::Move;
                        }

                        const SwFrameFormat *const pFlyFormat(rSh.GetFlyFrameFormat());
                        const SvxMacro* pMacro = nullptr;

                        SvMacroItemId nEvent = SdrHdlKind::Move == g_eSdrMoveHdl
                                            ? SvMacroItemId::SwFrmMove
                                            : SvMacroItemId::SwFrmResize;

                        if (nullptr != pFlyFormat)
                            pMacro = pFlyFormat->GetMacro().GetMacroTable().Get(nEvent);
                        if (nullptr != pMacro &&
                        // or notify only e.g. every 20 Twip?
                            m_aRszMvHdlPt != aDocPt )
                        {
                            m_aRszMvHdlPt = aDocPt;
                            sal_uInt32 nPos = 0;
                            SbxArrayRef xArgs = new SbxArray;
                            SbxVariableRef xVar = new SbxVariable;
                            xVar->PutString( pFlyFormat->GetName().toString() );
                            xArgs->Put(xVar.get(), ++nPos);

                            if( SvMacroItemId::SwFrmResize == nEvent )
                            {
                                xVar = new SbxVariable;
                                xVar->PutUShort( static_cast< sal_uInt16 >(g_eSdrMoveHdl) );
                                xArgs->Put(xVar.get(), ++nPos);
                            }

                            xVar = new SbxVariable;
                            xVar->PutLong( aDocPt.X() - aSttPt.X() );
                            xArgs->Put(xVar.get(), ++nPos);
                            xVar = new SbxVariable;
                            xVar->PutLong( aDocPt.Y() - aSttPt.Y() );
                            xArgs->Put(xVar.get(), ++nPos);

                            OUString sRet;

                            ReleaseMouse();

                            rSh.ExecMacro( *pMacro, &sRet, xArgs.get() );

                            CaptureMouse();

                            if( !sRet.isEmpty() && sRet.toInt32()!=0 )
                                return ;
                        }
                    }
                    // event processing for resizing

                    if( bIsViewReadOnly )
                        break;

                    bool bResizeKeepRatio = rSh.GetSelectionType() & SelectionType::Graphic ||
                                            rSh.GetSelectionType() & SelectionType::Media ||
                                            rSh.GetSelectionType() & SelectionType::Ole;
                    bool bisResize = g_eSdrMoveHdl != SdrHdlKind::Move;

                    if (pSdrView)
                    {
                        // Resize proportionally when media is selected and the user drags on a corner
                        const Point aSttPt(PixelToLogic(m_aStartPos));
                        SdrHdl* pHdl = pSdrView->PickHandle(aSttPt);
                        if (pHdl)
                            bResizeKeepRatio = bResizeKeepRatio && pHdl->IsCornerHdl();

                        if (pSdrView->GetDragMode() == SdrDragMode::Crop)
                            bisResize = false;
                        if (rMEvt.IsShift())
                        {
                            pSdrView->SetAngleSnapEnabled(!bResizeKeepRatio);
                            if (bisResize)
                                pSdrView->SetOrtho(!bResizeKeepRatio);
                            else
                                pSdrView->SetOrtho(true);
                        }
                        else
                        {
                            pSdrView->SetAngleSnapEnabled(bResizeKeepRatio);
                            if (bisResize)
                                pSdrView->SetOrtho(bResizeKeepRatio);
                            else
                                pSdrView->SetOrtho(false);
                        }
                    }

                    rSh.Drag( &aDocPt, rMEvt.IsShift() );
                    m_bIsInMove = true;
                }
                else if( bIsViewReadOnly )
                    break;

                if ( !bInsWin )
                {
                    Point aTmp( aDocPt );
                    aTmp += rSh.VisArea().Pos() - aOldPt;
                    LeaveArea( aTmp );
                }
                else if(m_bIsInMove)
                    EnterArea();
                return;
            }
            if ( !rSh.IsSelFrameMode() && !g_bDDINetAttr &&
                (IsMinMove( m_aStartPos,aPixPt ) || m_bIsInMove) &&
                (rSh.IsInSelect() || !rSh.TestCurrPam( aDocPt )) )
            {
                if ( pSdrView )
                {
                    if ( rMEvt.IsShift() )
                        pSdrView->SetOrtho(true);
                    else
                        pSdrView->SetOrtho(false);
                }
                if ( !bInsWin )
                {
                    Point aTmp( aDocPt );
                    aTmp += rSh.VisArea().Pos() - aOldPt;
                    LeaveArea( aTmp );
                }
                else
                {
                    if( !rMEvt.IsSynthetic() &&
                        ( MOUSE_LEFT != rMEvt.GetButtons() ||
                          KEY_MOD1 != rMEvt.GetModifier() ||
                          !rSh.Is_FnDragEQBeginDrag() ||
                          rSh.IsAddMode() ) )
                    {
                        rSh.Drag( &aDocPt, false );
                        g_bValidCursorPos = !(CRSR_POSCHG & rSh.CallSetCursor(&aDocPt, false,
                            ScrollSizeMode::ScrollSizeMouseSelection));
                        EnterArea();
                    }
                }
            }
            g_bDDINetAttr = false;
            break;
        case 0:
        {
            if ( m_pApplyTempl )
            {
                UpdatePointer(aDocPt); // maybe a frame has to be marked here
                break;
            }
            // change ui if mouse is over SwPostItField
            // TODO: do the same thing for redlines IsAttrAtPos::Redline
            SwContentAtPos aContentAtPos( IsAttrAtPos::Field);
            if (rSh.GetContentAtPos(aDocPt, aContentAtPos, false))
            {
                const SwField* pField = aContentAtPos.aFnd.pField;
                if (pField->Which()== SwFieldIds::Postit)
                {
                    m_rView.GetPostItMgr()->SetShadowState(reinterpret_cast<const SwPostItField*>(pField),false);
                }
                else
                    m_rView.GetPostItMgr()->SetShadowState(nullptr,false);
            }
            else
                m_rView.GetPostItMgr()->SetShadowState(nullptr,false);
            [[fallthrough]];
        }
        case KEY_SHIFT:
        case KEY_MOD2:
        case KEY_MOD1:
            if ( !m_bInsDraw )
            {
                bool bTstShdwCursor = true;

                UpdatePointer( aDocPt, rMEvt.GetModifier() );

                const SwFrameFormat* pFormat = nullptr;
                const SwFormatINetFormat* pINet = nullptr;
                SwContentAtPos aContentAtPos( IsAttrAtPos::InetAttr );
                if( rSh.GetContentAtPos( aDocPt, aContentAtPos ) )
                    pINet = static_cast<const SwFormatINetFormat*>(aContentAtPos.aFnd.pAttr);

                const void* pTmp = pINet;

                if( pINet ||
                    nullptr != ( pTmp = pFormat = rSh.GetFormatFromAnyObj( aDocPt )))
                {
                    bTstShdwCursor = false;
                    if( pTmp == pINet )
                        m_aSaveCallEvent.Set( pINet );
                    else
                    {
                        IMapObject* pIMapObj = pFormat->GetIMapObject( aDocPt );
                        if( pIMapObj )
                            m_aSaveCallEvent.Set( pFormat, pIMapObj );
                        else
                            m_aSaveCallEvent.Set( EVENT_OBJECT_URLITEM, pFormat );
                    }

                    // should be over an InternetField with an
                    // embedded macro?
                    if( m_aSaveCallEvent != aLastCallEvent )
                    {
                        if( aLastCallEvent.HasEvent() )
                            rSh.CallEvent( SvMacroItemId::OnMouseOut,
                                            aLastCallEvent, true );
                        // 0 says that the object doesn't have any table
                        if( !rSh.CallEvent( SvMacroItemId::OnMouseOver,
                                        m_aSaveCallEvent ))
                            m_aSaveCallEvent.Clear();
                    }
                }
                else if( aLastCallEvent.HasEvent() )
                {
                    // cursor was on an object
                    rSh.CallEvent( SvMacroItemId::OnMouseOut,
                                    aLastCallEvent, true );
                }

                if( bTstShdwCursor && bInsWin && !bIsViewReadOnly &&
                    !m_bInsFrame &&
                    !rSh.GetViewOptions()->getBrowseMode() &&
                    rSh.GetViewOptions()->IsShadowCursor() &&
                    !(rMEvt.GetModifier() + rMEvt.GetButtons()) &&
                    !rSh.HasSelection() && !GetOutDev()->GetConnectMetaFile() )
                {
                    SwRect aRect;

                    SwFillMode eMode = rSh.GetViewOptions()->GetShdwCursorFillMode();
                    if( rSh.GetShadowCursorPos( aDocPt, eMode, aRect, m_eOrient ))
                    {
                        if( !m_pShadCursor )
                            m_pShadCursor.reset( new SwShadowCursor( *this ) );
                        if( text::HoriOrientation::RIGHT != m_eOrient && text::HoriOrientation::CENTER != m_eOrient )
                            m_eOrient = text::HoriOrientation::LEFT;
                        m_pShadCursor->SetPos( aRect.Pos(), aRect.Height(), static_cast< sal_uInt16 >(m_eOrient) );
                        bDelShadCursor = false;
                    }
                }
            }
            break;
        case MOUSE_LEFT + KEY_MOD2:
            if( rSh.IsBlockMode() && !rMEvt.IsSynthetic() )
            {
                rSh.Drag( &aDocPt, false );
                g_bValidCursorPos = !(CRSR_POSCHG & rSh.CallSetCursor(&aDocPt, false));
                EnterArea();
            }
        break;
    }

    if( bDelShadCursor && m_pShadCursor )
    {
        m_pShadCursor.reset();
    }
    m_bWasShdwCursor = false;
}

/**
 * Button Up
 */
void SwEditWin::MouseButtonUp(const MouseEvent& rMEvt)
{
    if (comphelper::LibreOfficeKit::isActive())
    {
        if (vcl::Window* pWindow = m_rView.GetPostItMgr()->IsHitSidebarWindow(rMEvt.GetPosPixel()))
        {
            pWindow->MouseButtonUp(rMEvt);
            return;
        }
    }

    if (mbIsDragSidebar)
    {
        SetSidebarWidth(rMEvt.GetPosPixel());
        // While dragging the mouse is captured, so we need to release it here
        ReleaseMouse();
        ReleaseCommentGuideLine();
        return;
    }

    bool bCallBase = true;

    bool bCallShadowCursor = m_bWasShdwCursor;
    m_bWasShdwCursor = false;
    if( m_pShadCursor )
    {
        m_pShadCursor.reset();
    }

    m_xRowColumnSelectionStart.reset();

    SdrHdlKind eOldSdrMoveHdl = g_eSdrMoveHdl;
    g_eSdrMoveHdl = SdrHdlKind::User;     // for MoveEvents - reset again

    // preventively reset
    m_rView.SetTabColFromDoc( false );
    m_rView.SetNumRuleNodeFromDoc(nullptr);

    SwWrtShell &rSh = m_rView.GetWrtShell();
    CurrShell aCurr( &rSh );
    SdrView *pSdrView = rSh.GetDrawView();
    if ( pSdrView )
    {
        // tdf34555: ortho was always reset before being used in EndSdrDrag
        // Now, it is reset only if not in Crop mode.
        if (pSdrView->GetDragMode() != SdrDragMode::Crop && !rMEvt.IsShift())
            pSdrView->SetOrtho(false);

        if ( pSdrView->MouseButtonUp( rMEvt,GetOutDev() ) )
        {
            rSh.GetView().GetViewFrame().GetBindings().InvalidateAll(false);
            return; // SdrView's event evaluated
        }
    }
    // only process MouseButtonUp when the Down went to that windows as well.
    if ( !m_bMBPressed )
    {
    // Undo for the watering can is already in CommandHdl
    // that's the way it should be!

        return;
    }

    Point aDocPt( PixelToLogic( rMEvt.GetPosPixel() ) );

    if ( g_bDDTimerStarted )
    {
        StopDDTimer( &rSh, aDocPt );
        m_bMBPressed = false;
        if ( rSh.IsSelFrameMode() )
        {
            rSh.EndDrag( &aDocPt, false );
            g_bFrameDrag = false;
        }
        g_bNoInterrupt = false;
        const Point aDocPos( PixelToLogic( rMEvt.GetPosPixel() ) );
        if ((PixelToLogic(m_aStartPos).Y() == (aDocPos.Y())) && (PixelToLogic(m_aStartPos).X() == (aDocPos.X())))//To make sure it was not moved
        {
            SdrPageView* pPV = nullptr;
            SdrObject* pObj = pSdrView ? pSdrView->PickObj(aDocPos, pSdrView->getHitTolLog(), pPV, SdrSearchOptions::ALSOONMASTER) : nullptr;
            if (pObj)
            {
                if (SwDrawContact* pContact = static_cast<SwDrawContact*>(GetUserCall(pObj)))
                {
                    SwFrameFormat* pFormat = pContact->GetFormat();
                    SwFrameFormat* pShapeFormat
                        = SwTextBoxHelper::getOtherTextBoxFormat(pFormat, RES_FLYFRMFMT);
                    if (!pShapeFormat)
                    {
                        pSdrView->UnmarkAllObj();
                        pSdrView->MarkObj(pObj, pPV);
                        if (rMEvt.IsLeft() && rMEvt.GetClicks() == 1 &&
                            SwModule::get()->GetUsrPref(
                                dynamic_cast<const SwWebView*>(&m_rView) != nullptr)->
                            IsClickChangeRotation())
                            m_rView.ToggleRotate();
                    }
                    else
                    {
                        // If the fly frame is a textbox of a shape, then select the shape instead.
                        SdrObject* pShape = pShapeFormat->FindSdrObject();
                        pSdrView->UnmarkAllObj();
                        pSdrView->MarkObj(pShape, pPV);
                    }
                }
            }
        }
        ReleaseMouse();
        return;
    }

    if( m_pAnchorMarker )
    {
        if(m_pAnchorMarker->GetHdl())
        {
            // #i121463# delete selected after drag
            m_pAnchorMarker->GetHdl()->SetSelected(false);
        }

        Point aPnt( m_pAnchorMarker->GetLastPos() );
        m_pAnchorMarker.reset();
        if( aPnt.X() || aPnt.Y() )
            rSh.FindAnchorPos( aPnt, true );
    }
    if ( m_bInsDraw && m_rView.GetDrawFuncPtr() )
    {
        if ( m_rView.GetDrawFuncPtr()->MouseButtonUp( rMEvt ) )
        {
            if (m_rView.GetDrawFuncPtr()) // could have been destroyed in MouseButtonUp
            {
                m_rView.GetDrawFuncPtr()->Deactivate();

                if (!m_rView.IsDrawMode())
                {
                    m_rView.SetDrawFuncPtr(nullptr);
                    SfxBindings& rBind = m_rView.GetViewFrame().GetBindings();
                    rBind.Invalidate( SID_ATTR_SIZE );
                    rBind.Invalidate( SID_TABLE_CELL );
                }
            }

            if ( rSh.GetSelectedObjCount() )
            {
                rSh.EnterSelFrameMode();
                if (!m_rView.GetDrawFuncPtr())
                    StdDrawMode( SdrObjKind::NONE, true );
            }
            else if ( rSh.IsFrameSelected() )
            {
                rSh.EnterSelFrameMode();
                StopInsFrame();
            }
            else
            {
                const Point aDocPos( PixelToLogic( m_aStartPos ) );
                g_bValidCursorPos = !(CRSR_POSCHG & rSh.CallSetCursor(&aDocPos, false));
                rSh.Edit();
            }

            m_rView.AttrChangedNotify(nullptr);
        }
        else if (rMEvt.GetButtons() == MOUSE_RIGHT && rSh.IsDrawCreate())
            m_rView.GetDrawFuncPtr()->BreakCreate();   // abort drawing

        g_bNoInterrupt = false;
        if (IsMouseCaptured())
            ReleaseMouse();
        return;
    }
    bool bPopMode = false;
    switch ( rMEvt.GetModifier() + rMEvt.GetButtons() )
    {
        case MOUSE_LEFT:
            if ( m_bInsDraw && rSh.IsDrawCreate() )
            {
                if ( m_rView.GetDrawFuncPtr() && m_rView.GetDrawFuncPtr()->MouseButtonUp(rMEvt) )
                {
                    m_rView.GetDrawFuncPtr()->Deactivate();
                    m_rView.AttrChangedNotify(nullptr);
                    if ( rSh.GetSelectedObjCount() )
                        rSh.EnterSelFrameMode();
                    if ( m_rView.GetDrawFuncPtr() && m_bInsFrame )
                        StopInsFrame();
                }
                bCallBase = false;
                break;
            }
            [[fallthrough]];
        case MOUSE_LEFT + KEY_MOD1:
        case MOUSE_LEFT + KEY_MOD2:
        case MOUSE_LEFT + KEY_SHIFT + KEY_MOD1:
            if ( g_bFrameDrag && rSh.IsSelFrameMode() )
            {
                if ( rMEvt.IsMod1() ) // copy and don't move.
                {
                    // abort drag, use internal Copy instead
                    tools::Rectangle aRect;
                    rSh.GetDrawView()->TakeActionRect( aRect );
                    if (!aRect.IsEmpty())
                    {
                        rSh.BreakDrag();
                        Point aEndPt, aSttPt;
                        if ( rSh.GetSelFrameType() & FrameTypeFlags::FLY_ATCNT )
                        {
                            aEndPt = aRect.TopLeft();
                            aSttPt = rSh.GetDrawView()->GetAllMarkedRect().TopLeft();
                        }
                        else
                        {
                            aEndPt = aRect.Center();
                            aSttPt = rSh.GetDrawView()->GetAllMarkedRect().Center();
                        }
                        if ( aSttPt != aEndPt )
                        {
                            rSh.StartUndo( SwUndoId::UI_DRAG_AND_COPY );
                            rSh.Copy(rSh, aSttPt, aEndPt);
                            rSh.EndUndo( SwUndoId::UI_DRAG_AND_COPY );
                        }
                    }
                    else {
                        rSh.EndDrag( &aDocPt, false );
                    }
                }
                else
                {
                    {
                        const SwFrameFormat *const pFlyFormat(rSh.GetFlyFrameFormat());
                        const SvxMacro* pMacro = nullptr;

                        SvMacroItemId nEvent = SdrHdlKind::Move == eOldSdrMoveHdl
                                            ? SvMacroItemId::SwFrmMove
                                            : SvMacroItemId::SwFrmResize;

                        if (nullptr != pFlyFormat)
                            pMacro = pFlyFormat->GetMacro().GetMacroTable().Get(nEvent);
                        if (nullptr != pMacro)
                        {
                            const Point aSttPt( PixelToLogic( m_aStartPos ) );
                            m_aRszMvHdlPt = aDocPt;
                            sal_uInt32 nPos = 0;
                            SbxArrayRef xArgs = new SbxArray;
                            SbxVariableRef xVar = new SbxVariable;
                            xVar->PutString( pFlyFormat->GetName().toString() );
                            xArgs->Put(xVar.get(), ++nPos);

                            if( SvMacroItemId::SwFrmResize == nEvent )
                            {
                                xVar = new SbxVariable;
                                xVar->PutUShort( static_cast< sal_uInt16 >(eOldSdrMoveHdl) );
                                xArgs->Put(xVar.get(), ++nPos);
                            }

                            xVar = new SbxVariable;
                            xVar->PutLong( aDocPt.X() - aSttPt.X() );
                            xArgs->Put(xVar.get(), ++nPos);
                            xVar = new SbxVariable;
                            xVar->PutLong( aDocPt.Y() - aSttPt.Y() );
                            xArgs->Put(xVar.get(), ++nPos);

                            xVar = new SbxVariable;
                            xVar->PutUShort( 1 );
                            xArgs->Put(xVar.get(), ++nPos);

                            ReleaseMouse();

                            rSh.ExecMacro( *pMacro, nullptr, xArgs.get() );

                            CaptureMouse();
                        }

                        if (pFlyFormat)
                        {
                            // See if the fly frame's anchor is in a content control. If so,
                            // interact with it.
                            const SwFormatAnchor& rFormatAnchor = pFlyFormat->GetAnchor();
                            SwNode* pAnchorNode = rFormatAnchor.GetAnchorNode();
                            if (pAnchorNode)
                            {
                                SwTextNode* pTextNode = pAnchorNode->GetTextNode();
                                if (pTextNode)
                                {
                                    SwTextAttr* pAttr = pTextNode->GetTextAttrAt(
                                        rFormatAnchor.GetAnchorContentOffset(), RES_TXTATR_CONTENTCONTROL,
                                        ::sw::GetTextAttrMode::Parent);
                                    if (pAttr)
                                    {
                                        SwTextContentControl* pTextContentControl
                                            = static_txtattr_cast<SwTextContentControl*>(pAttr);
                                        const SwFormatContentControl& rFormatContentControl
                                            = pTextContentControl->GetContentControl();
                                        rSh.GotoContentControl(rFormatContentControl);
                                    }
                                }
                            }
                        }
                    }
                    rSh.EndDrag( &aDocPt, false );
                }
                g_bFrameDrag = false;
                bCallBase = false;
                break;
            }
            bPopMode = true;
            [[fallthrough]];
        case MOUSE_LEFT + KEY_SHIFT:
            if (rSh.IsSelFrameMode())
            {

                rSh.EndDrag( &aDocPt, false );
                g_bFrameDrag = false;
                bCallBase = false;
                break;
            }

            if( g_bHoldSelection )
            {
                // the EndDrag should be called in any case
                g_bHoldSelection = false;
                rSh.EndDrag( &aDocPt, false );
            }
            else
            {
                SwContentAtPos aFieldAtPos (IsAttrAtPos::Field);
                if ( !rSh.IsInSelect() && rSh.TestCurrPam( aDocPt ) &&
                     !rSh.GetContentAtPos( aDocPt, aFieldAtPos ) )
                {
                    const bool bTmpNoInterrupt = g_bNoInterrupt;
                    g_bNoInterrupt = false;
                    {   // create only temporary move context because otherwise
                        // the query to the content form doesn't work!!!
                        SwMvContext aMvContext( &rSh );
                        const Point aDocPos( PixelToLogic( m_aStartPos ) );
                        g_bValidCursorPos = !(CRSR_POSCHG & rSh.CallSetCursor(&aDocPos, false));
                    }
                    g_bNoInterrupt = bTmpNoInterrupt;

                }
                else
                {
                    bool bInSel = rSh.IsInSelect();
                    rSh.EndDrag( &aDocPt, false );

                    // Internetfield? --> call link (load doc!!)
                    if( !bInSel )
                    {
                        LoadUrlFlags nFilter = LoadUrlFlags::NONE;
                        if( KEY_MOD1 == rMEvt.GetModifier() )
                            nFilter |= LoadUrlFlags::NewView;

                        bool bExecHyperlinks = m_rView.GetDocShell()->IsReadOnly();
                        if ( !bExecHyperlinks )
                        {
                            const bool bSecureOption = SvtSecurityOptions::IsOptionSet( SvtSecurityOptions::EOption::CtrlClickHyperlink );
                            if ( (  bSecureOption && rMEvt.GetModifier() == KEY_MOD1 ) ||
                                 ( !bSecureOption && rMEvt.GetModifier() != KEY_MOD1 ) )
                                bExecHyperlinks = true;
                        }

                        const bool bExecSmarttags = rMEvt.GetModifier() == KEY_MOD1;

                        if(m_pApplyTempl)
                            bExecHyperlinks = false;

                        SwContentAtPos aContentAtPos( IsAttrAtPos::Field |
                                                    IsAttrAtPos::InetAttr |
                                                    IsAttrAtPos::SmartTag  | IsAttrAtPos::FormControl |
                                                    IsAttrAtPos::ContentControl);

                        if( rSh.GetContentAtPos( aDocPt, aContentAtPos ) )
                        {
                            // Do it again if we're not on a field/hyperlink to update the cursor accordingly
                            if ( IsAttrAtPos::Field != aContentAtPos.eContentAtPos
                                 && IsAttrAtPos::InetAttr != aContentAtPos.eContentAtPos )
                                rSh.GetContentAtPos( aDocPt, aContentAtPos, true );

                            bool bViewLocked = rSh.IsViewLocked();
                            if( !bViewLocked && !rSh.IsReadOnlyAvailable() &&
                                aContentAtPos.IsInProtectSect() )
                                rSh.LockView( true );

                            ReleaseMouse();

                            if( IsAttrAtPos::Field == aContentAtPos.eContentAtPos )
                            {
                                bool bAddMode(false);
                                // AdditionalMode if applicable
                                if (KEY_MOD1 == rMEvt.GetModifier()
                                    && !rSh.IsAddMode())
                                {
                                    bAddMode = true;
                                    rSh.EnterAddMode();
                                }
                                if ( aContentAtPos.pFndTextAttr != nullptr
                                     && aContentAtPos.pFndTextAttr->Which() == RES_TXTATR_INPUTFIELD )
                                {
                                    if (!rSh.IsInSelect())
                                    {
                                        // create only temporary move context because otherwise
                                        // the query to the content form doesn't work!!!
                                        SwMvContext aMvContext( &rSh );
                                        const Point aDocPos( PixelToLogic( m_aStartPos ) );
                                        g_bValidCursorPos = !(CRSR_POSCHG & rSh.CallSetCursor(&aDocPos, false));
                                    }
                                    else
                                    {
                                        g_bValidCursorPos = true;
                                    }
                                }
                                else
                                {
                                    rSh.ClickToField(*aContentAtPos.aFnd.pField, bExecHyperlinks);
                                    // a bit of a mystery what this is good for?
                                    // in this case we assume it's valid since we
                                    // just selected a field
                                    g_bValidCursorPos = true;
                                }
                                if (bAddMode)
                                {
                                    rSh.LeaveAddMode();
                                }
                            }
                            else if (aContentAtPos.eContentAtPos == IsAttrAtPos::ContentControl)
                            {
                                auto pTextContentControl
                                    = static_txtattr_cast<const SwTextContentControl*>(
                                        aContentAtPos.pFndTextAttr);
                                const SwFormatContentControl& rFormatContentControl
                                    = pTextContentControl->GetContentControl();
                                rSh.GotoContentControl(rFormatContentControl);
                            }
                            else if ( IsAttrAtPos::SmartTag == aContentAtPos.eContentAtPos )
                            {
                                    // execute smarttag menu
                                    if ( bExecSmarttags && SwSmartTagMgr::Get().IsSmartTagsEnabled() )
                                        m_rView.ExecSmartTagPopup( aDocPt );
                            }
                            else if ( IsAttrAtPos::FormControl == aContentAtPos.eContentAtPos )
                            {
                                OSL_ENSURE( aContentAtPos.aFnd.pFieldmark != nullptr, "where is my field ptr???");
                                if ( aContentAtPos.aFnd.pFieldmark != nullptr)
                                {
                                    Fieldmark *fieldBM = const_cast< Fieldmark* > ( aContentAtPos.aFnd.pFieldmark );
                                    if ( fieldBM->GetFieldname( ) == ODF_FORMCHECKBOX )
                                    {
                                        CheckboxFieldmark& rCheckboxFm = dynamic_cast<CheckboxFieldmark&>(*fieldBM);
                                        rCheckboxFm.SetChecked(!rCheckboxFm.IsChecked());
                                        rCheckboxFm.Invalidate();
                                        rSh.InvalidateWindows( SwRect(m_rView.GetVisArea()) );
                                    }
                                    else if ( fieldBM->GetFieldname( ) == ODF_FORMTEXT &&
                                        static_cast< const TextFieldmark* > ( aContentAtPos.aFnd.pFieldmark )->HasDefaultContent() )
                                    {
                                        rSh.GotoFieldmark( aContentAtPos.aFnd.pFieldmark );
                                    }

                                }
                            }
                            else if ( IsAttrAtPos::InetAttr == aContentAtPos.eContentAtPos )
                            {
                                if (comphelper::LibreOfficeKit::isActive())
                                {
                                    OUString val((*static_cast<const SwFormatINetFormat*>(aContentAtPos.aFnd.pAttr)).GetValue());
                                    if (val.startsWith("#"))
                                        bExecHyperlinks = true;
                                }
                                if ( bExecHyperlinks && aContentAtPos.aFnd.pAttr )
                                    rSh.ClickToINetAttr( *static_cast<const SwFormatINetFormat*>(aContentAtPos.aFnd.pAttr), nFilter );
                            }

                            rSh.LockView( bViewLocked );
                            bCallShadowCursor = false;
                        }
                        else
                        {
                            aContentAtPos = SwContentAtPos( IsAttrAtPos::Footnote );
                            if( !rSh.GetContentAtPos( aDocPt, aContentAtPos, true ) && bExecHyperlinks )
                            {
                                SdrViewEvent aVEvt;

                                if (pSdrView)
                                    pSdrView->PickAnything(rMEvt, SdrMouseEventKind::BUTTONDOWN, aVEvt);

                                if (pSdrView && aVEvt.meEvent == SdrEventKind::ExecuteUrl)
                                {
                                    // hit URL field
                                    const SvxURLField *pField = aVEvt.mpURLField;
                                    if (pField)
                                    {
                                        const OUString& sURL(pField->GetURL());
                                        const OUString& sTarget(pField->GetTargetFrame());
                                        ::LoadURL(rSh, sURL, nFilter, sTarget);
                                    }
                                    bCallShadowCursor = false;
                                }
                                else
                                {
                                    // hit graphic
                                    ReleaseMouse();
                                    if( rSh.ClickToINetGrf( aDocPt, nFilter ))
                                        bCallShadowCursor = false;
                                }
                            }
                        }

                        if( bCallShadowCursor &&
                            rSh.GetViewOptions()->IsShadowCursor() &&
                            MOUSE_LEFT == (rMEvt.GetModifier() + rMEvt.GetButtons()) &&
                            !rSh.HasSelection() &&
                            !GetOutDev()->GetConnectMetaFile() &&
                            rSh.VisArea().Contains( aDocPt ))
                        {
                            SwUndoId nLastUndoId(SwUndoId::EMPTY);
                            if (rSh.GetLastUndoInfo(nullptr, & nLastUndoId))
                            {
                                if (SwUndoId::INS_FROM_SHADOWCRSR == nLastUndoId)
                                {
                                    rSh.Undo();
                                }
                            }
                            SwFillMode eMode = rSh.GetViewOptions()->GetShdwCursorFillMode();
                            rSh.SetShadowCursorPos( aDocPt, eMode );
                        }
                    }
                }
                bCallBase = false;

            }

            // reset pushed mode in Down again if applicable
            if ( bPopMode && g_bModePushed )
            {
                rSh.PopMode();
                g_bModePushed = false;
                bCallBase = false;
            }
            break;

        default:
            ReleaseMouse();
            return;
    }

    if( m_pApplyTempl )
    {
        SelectionType eSelection = rSh.GetSelectionType();
        SwFormatClipboard* pFormatClipboard = m_pApplyTempl->m_pFormatClipboard;
        if( pFormatClipboard )//apply format paintbrush
        {
            //get some parameters
            SwWrtShell& rWrtShell = m_rView.GetWrtShell();
            SfxStyleSheetBasePool* pPool=nullptr;
            bool bNoCharacterFormats = false;
            // Paste paragraph properties if the selection contains a whole paragraph or
            // there was no selection at all (i.e. just a left click)
            bool bNoParagraphFormats = rSh.HasSelection() && rSh.IsSelOnePara() && !rSh.IsSelFullPara();

            {
                SwDocShell* pDocSh = m_rView.GetDocShell();
                if(pDocSh)
                    pPool = pDocSh->GetStyleSheetPool();
                if( (rMEvt.GetModifier()&KEY_MOD1) && (rMEvt.GetModifier()&KEY_SHIFT) )
                {
                    bNoCharacterFormats = true;
                    bNoParagraphFormats = false;
                }
                else if( rMEvt.GetModifier() & KEY_MOD1 )
                    bNoParagraphFormats = true;
            }
            //execute paste
            pFormatClipboard->Paste( rWrtShell, pPool, bNoCharacterFormats, bNoParagraphFormats );

            //if the clipboard is empty after paste remove the ApplyTemplate
            if(!pFormatClipboard->HasContent())
                SetApplyTemplate(SwApplyTemplate());

            //tdf#38101 remove temporary highlighting
            m_pUserMarker.reset();
        }
        else if( m_pApplyTempl->nColor )
        {
            sal_uInt16 nId = 0;
            switch( m_pApplyTempl->nColor )
            {
                case SID_ATTR_CHAR_COLOR_EXT:
                    nId = RES_CHRATR_COLOR;
                    break;
                case SID_ATTR_CHAR_BACK_COLOR:
                case SID_ATTR_CHAR_COLOR_BACKGROUND:
                    nId = RES_CHRATR_BACKGROUND;
                    break;
            }
            if( nId && (SelectionType::Text|SelectionType::Table) & eSelection)
            {
                if( rSh.IsSelection() && !rSh.HasReadonlySel() )
                {
                    m_pApplyTempl->nUndo =
                        std::min(m_pApplyTempl->nUndo, rSh.GetDoc()->GetIDocumentUndoRedo().GetUndoActionCount());
                    if (nId == RES_CHRATR_BACKGROUND)
                        ApplyCharBackground(m_aWaterCanTextBackColor, model::ComplexColor(), rSh);
                    else
                        rSh.SetAttrItem( SvxColorItem( m_aWaterCanTextColor, nId ) );
                    rSh.UnSetVisibleCursor();
                    rSh.EnterStdMode();
                    rSh.SetVisibleCursor(aDocPt);
                    bCallBase = false;
                    m_aTemplateTimer.Stop();
                }
                else if(rMEvt.GetClicks() == 1)
                {
                    // no selection -> so turn off watering can
                    m_aTemplateTimer.Start();
                }
            }
        }
        else
        {
            UIName aStyleName;
            switch ( m_pApplyTempl->eType )
            {
                case SfxStyleFamily::Para:
                    if( (( SelectionType::Text | SelectionType::Table )
                         & eSelection ) && !rSh.HasReadonlySel() )
                    {
                        rSh.SetTextFormatColl( m_pApplyTempl->aColl.pTextColl );
                        m_pApplyTempl->nUndo =
                            std::min(m_pApplyTempl->nUndo, rSh.GetDoc()->GetIDocumentUndoRedo().GetUndoActionCount());
                        bCallBase = false;
                        if ( m_pApplyTempl->aColl.pTextColl )
                            aStyleName = m_pApplyTempl->aColl.pTextColl->GetName();
                    }
                    break;
                case SfxStyleFamily::Char:
                    if( (( SelectionType::Text | SelectionType::Table )
                         & eSelection ) && !rSh.HasReadonlySel() )
                    {
                        rSh.SetAttrItem( SwFormatCharFormat(m_pApplyTempl->aColl.pCharFormat) );
                        rSh.UnSetVisibleCursor();
                        rSh.EnterStdMode();
                        rSh.SetVisibleCursor(aDocPt);
                        m_pApplyTempl->nUndo =
                            std::min(m_pApplyTempl->nUndo, rSh.GetDoc()->GetIDocumentUndoRedo().GetUndoActionCount());
                        bCallBase = false;
                        if ( m_pApplyTempl->aColl.pCharFormat )
                            aStyleName = m_pApplyTempl->aColl.pCharFormat->GetName();
                    }
                    break;
                case SfxStyleFamily::Frame :
                {
                    const SwFrameFormat* pFormat = rSh.GetFormatFromObj( aDocPt );
                    if(dynamic_cast<const SwFlyFrameFormat*>( pFormat) )
                    {
                        rSh.SetFrameFormat( m_pApplyTempl->aColl.pFrameFormat, false, &aDocPt );
                        m_pApplyTempl->nUndo =
                            std::min(m_pApplyTempl->nUndo, rSh.GetDoc()->GetIDocumentUndoRedo().GetUndoActionCount());
                        bCallBase = false;
                        if( m_pApplyTempl->aColl.pFrameFormat )
                            aStyleName = m_pApplyTempl->aColl.pFrameFormat->GetName();
                    }
                    break;
                }
                case SfxStyleFamily::Page:
                    // no Undo with page templates
                    rSh.ChgCurPageDesc( *m_pApplyTempl->aColl.pPageDesc );
                    if ( m_pApplyTempl->aColl.pPageDesc )
                        aStyleName = m_pApplyTempl->aColl.pPageDesc->GetName();
                    m_pApplyTempl->nUndo =
                        std::min(m_pApplyTempl->nUndo, rSh.GetDoc()->GetIDocumentUndoRedo().GetUndoActionCount());
                    bCallBase = false;
                    break;
                case SfxStyleFamily::Pseudo:
                    if( !rSh.HasReadonlySel() )
                    {
                        rSh.SetCurNumRule( *m_pApplyTempl->aColl.pNumRule,
                                           false,
                                           m_pApplyTempl->aColl.pNumRule->GetDefaultListId() );
                        bCallBase = false;
                        m_pApplyTempl->nUndo =
                            std::min(m_pApplyTempl->nUndo, rSh.GetDoc()->GetIDocumentUndoRedo().GetUndoActionCount());
                        if( m_pApplyTempl->aColl.pNumRule )
                            aStyleName = m_pApplyTempl->aColl.pNumRule->GetName();
                    }
                    break;
                default: break;
            }

            uno::Reference< frame::XDispatchRecorder > xRecorder =
                    m_rView.GetViewFrame().GetBindings().GetRecorder();
            if ( !aStyleName.isEmpty() && xRecorder.is() )
            {
                SfxShell *pSfxShell = lcl_GetTextShellFromDispatcher( m_rView );
                if ( pSfxShell )
                {
                    SfxRequest aReq(m_rView.GetViewFrame(), SID_STYLE_APPLY);
                    aReq.AppendItem( SfxStringItem( SID_STYLE_APPLY, aStyleName.toString() ) );
                    aReq.AppendItem( SfxUInt16Item( SID_STYLE_FAMILY, static_cast<sal_uInt16>(m_pApplyTempl->eType) ) );
                    aReq.Done();
                }
            }
        }

    }
    ReleaseMouse();
    // Only processed MouseEvents arrive here; only at these this mode can
    // be reset.
    m_bMBPressed = false;

    // Make this call just to be sure. Selecting has finished surely by now.
    // Otherwise the timeout's timer could give problems.
    EnterArea();
    g_bNoInterrupt = false;

    if (bCallBase)
        Window::MouseButtonUp(rMEvt);

    // tdf#161717 - Track changes: Clicking on change in document should highlight related change
    // in "Manage Changes" window/sidebar
    if (m_rView.GetWrtShell().GetCurrRedline())
    {
        SwDocShell* pDocSh = m_rView.GetDocShell();
        if (pDocSh)
            pDocSh->Broadcast(SfxHint(SfxHintId::SwRedlineContentAtPos));
    }

    if (!(pSdrView && rMEvt.GetClicks() == 1 && comphelper::LibreOfficeKit::isActive()))
        return;

    // When tiled rendering, single click on a shape text starts editing already.
    SdrViewEvent aViewEvent;
    SdrHitKind eHit = pSdrView->PickAnything(rMEvt, SdrMouseEventKind::BUTTONUP, aViewEvent);
    const SdrMarkList& rMarkList = pSdrView->GetMarkedObjectList();
    if (eHit == SdrHitKind::TextEditObj && rMarkList.GetMarkCount() == 1)
    {
        if (SdrObject* pObj = rMarkList.GetMark(0)->GetMarkedSdrObj())
        {
            EnterDrawTextMode(pObj->GetLogicRect().Center());
            if ( auto pSwDrawTextShell = dynamic_cast< SwDrawTextShell *>( m_rView.GetCurShell() ) )
                pSwDrawTextShell->Init();
        }
    }
}

/**
 * Apply template
 */
void SwEditWin::SetApplyTemplate(const SwApplyTemplate &rTempl)
{
    static bool bIdle = false;
    m_pApplyTempl.reset();
    SwWrtShell &rSh = m_rView.GetWrtShell();

    if(rTempl.m_pFormatClipboard)
    {
        m_pApplyTempl.reset(new SwApplyTemplate( rTempl ));
        m_pApplyTempl->nUndo = rSh.GetDoc()->GetIDocumentUndoRedo().GetUndoActionCount();
        SetPointer( PointerStyle::Fill );//@todo #i20119# maybe better a new brush pointer here in future
        rSh.NoEdit( false );
        bIdle = rSh.GetViewOptions()->IsIdle();
        rSh.GetViewOptions()->SetIdle( false );
    }
    else if(rTempl.nColor)
    {
        m_pApplyTempl.reset(new SwApplyTemplate( rTempl ));
        m_pApplyTempl->nUndo = rSh.GetDoc()->GetIDocumentUndoRedo().GetUndoActionCount();
        SetPointer( PointerStyle::Fill );
        rSh.NoEdit( false );
        bIdle = rSh.GetViewOptions()->IsIdle();
        rSh.GetViewOptions()->SetIdle( false );
    }
    else if( rTempl.eType != SfxStyleFamily::None )
    {
        m_pApplyTempl.reset(new SwApplyTemplate( rTempl ));
        m_pApplyTempl->nUndo = rSh.GetDoc()->GetIDocumentUndoRedo().GetUndoActionCount();
        SetPointer( PointerStyle::Fill  );
        rSh.NoEdit( false );
        bIdle = rSh.GetViewOptions()->IsIdle();
        rSh.GetViewOptions()->SetIdle( false );
    }
    else
    {
        SetPointer( PointerStyle::Text );
        rSh.UnSetVisibleCursor();

        rSh.GetViewOptions()->SetIdle( bIdle );
        if ( !rSh.IsSelFrameMode() )
            rSh.Edit();
    }

    static const sal_uInt16 aInva[] =
    {
        SID_STYLE_WATERCAN,
        SID_ATTR_CHAR_COLOR_EXT,
        SID_ATTR_CHAR_COLOR_BACKGROUND_EXT,
        0
    };
    m_rView.GetViewFrame().GetBindings().Invalidate(aInva);
}

/**
 * Ctor
 */
SwEditWin::SwEditWin(vcl::Window *pParent, SwView &rMyView):
    DocWindow(pParent, WinBits(WB_CLIPCHILDREN | WB_DIALOGCONTROL)),
    DropTargetHelper( this ),
    DragSourceHelper( this ),

    m_aTimer("SwEditWin"),
    m_nTimerCalls(0),
    m_aKeyInputFlushTimer("SwEditWin m_aKeyInputFlushTimer"),
    m_eBufferLanguage(LANGUAGE_DONTKNOW),
    m_eScrollSizeMode(ScrollSizeMode::ScrollSizeMouseSelection),
    m_aTemplateTimer("SwEditWin m_aTemplateTimer"),
    m_pUserMarkerObj( nullptr ),

    m_rView( rMyView ),

    m_aActHitType(SdrHitKind::NONE),
    m_nDropFormat( SotClipboardFormatId::NONE ),
    m_nDropAction( 0 ),
    m_nDropDestination( SotExchangeDest::NONE ),

    m_eBezierMode(SID_BEZIER_INSERT),
    m_nInsFrameColCount( 1 ),
    m_eDrawMode(SdrObjKind::NONE),

    m_bMBPressed(false),
    m_bInsDraw(false),
    m_bInsFrame(false),
    m_bIsInMove(false),
    m_bIsInDrag(false),
    m_bOldIdle(false),
    m_bOldIdleSet(false),
    m_bChainMode(false),
    m_bWasShdwCursor(false),
    m_bLockInput(false),
    m_bIsRowDrag(false),
    m_bUseInputLanguage(false),
    m_bObjectSelect(false),
    mbIsDragSidebar(false),
    m_nKS_NUMDOWN_Count(0),
    m_nKS_NUMINDENTINC_Count(0),
    m_pFrameControlsManager(new SwFrameControlsManager(this))
{
    set_id(u"writer_edit"_ustr);
    SetHelpId(HID_EDIT_WIN);
    EnableChildTransparentMode();
    SetDialogControlFlags( DialogControlFlags::Return | DialogControlFlags::WantFocus );

    m_bMBPressed = m_bInsDraw = m_bInsFrame =
    m_bIsInDrag = m_bOldIdle = m_bOldIdleSet = m_bChainMode = m_bWasShdwCursor = false;
    // initially use the input language
    m_bUseInputLanguage = true;

    SetMapMode(MapMode(MapUnit::MapTwip));

    SetPointer( PointerStyle::Text );
    m_aTimer.SetInvokeHandler(LINK(this, SwEditWin, TimerHandler));

    m_aKeyInputFlushTimer.SetTimeout( 20 );
    m_aKeyInputFlushTimer.SetInvokeHandler(LINK(this, SwEditWin, KeyInputFlushHandler));

    // TemplatePointer for colors should be reset without
    // selection after single click, but not after double-click (tdf#122442)
    m_aTemplateTimer.SetTimeout(GetSettings().GetMouseSettings().GetDoubleClickTime());
    m_aTemplateTimer.SetInvokeHandler(LINK(this, SwEditWin, TemplateTimerHdl));

    // temporary solution!!! Should set the font of the current
    // insert position at every cursor movement!
    if( !rMyView.GetDocShell()->IsReadOnly() )
    {
        SetInputContext( InputContext(vcl::Font(), InputContextFlags::Text |
                                            InputContextFlags::ExtText ) );
    }
}

SwEditWin::~SwEditWin()
{
    disposeOnce();
}

void SwEditWin::dispose()
{
    m_pShadCursor.reset();

    if( s_pQuickHlpData->m_bIsDisplayed && m_rView.GetWrtShellPtr() )
        s_pQuickHlpData->Stop( m_rView.GetWrtShell() );
    g_bExecuteDrag = false;
    m_pApplyTempl.reset();

    m_rView.SetDrawFuncPtr(nullptr);

    m_pUserMarker.reset();

    m_pAnchorMarker.reset();

    m_pFrameControlsManager->dispose();
    m_pFrameControlsManager.reset();

    DragSourceHelper::dispose();
    DropTargetHelper::dispose();
    vcl::Window::dispose();
}

/**
 * Turn on DrawTextEditMode
 */
void SwEditWin::EnterDrawTextMode( const Point& aDocPos )
{
    if ( m_rView.EnterDrawTextMode(aDocPos) )
    {
        if (m_rView.GetDrawFuncPtr())
        {
            m_rView.GetDrawFuncPtr()->Deactivate();
            m_rView.SetDrawFuncPtr(nullptr);
            m_rView.LeaveDrawCreate();
        }
        m_rView.NoRotate();
        m_rView.AttrChangedNotify(nullptr);
    }
}

/**
 * Turn on DrawMode
 */
bool SwEditWin::EnterDrawMode(const MouseEvent& rMEvt, const Point& aDocPos)
{
    SwWrtShell &rSh = m_rView.GetWrtShell();
    SdrView *pSdrView = rSh.GetDrawView();

    if ( m_rView.GetDrawFuncPtr() )
    {
        if (rSh.IsDrawCreate())
            return true;

        bool bRet = m_rView.GetDrawFuncPtr()->MouseButtonDown( rMEvt );
        m_rView.AttrChangedNotify(nullptr);
        return bRet;
    }

    if ( pSdrView && pSdrView->IsTextEdit() )
    {
        bool bUnLockView = !rSh.IsViewLocked();
        rSh.LockView( true );

        rSh.EndTextEdit(); // clicked aside, end Edit
        rSh.SelectObj( aDocPos );
        if ( !rSh.GetSelectedObjCount() && !rSh.IsFrameSelected() )
            rSh.LeaveSelFrameMode();
        else
        {
            SwEditWin::s_nDDStartPosY = aDocPos.Y();
            SwEditWin::s_nDDStartPosX = aDocPos.X();
            g_bFrameDrag = true;
        }
        if( bUnLockView )
            rSh.LockView( false );
        m_rView.AttrChangedNotify(nullptr);
        return true;
    }
    return false;
}

bool SwEditWin::IsDrawSelMode() const
{
    return IsObjectSelect();
}

void SwEditWin::GetFocus()
{
    if ( m_rView.GetPostItMgr()->HasActiveSidebarWin() )
    {
        m_rView.GetPostItMgr()->GrabFocusOnActiveSidebarWin();
    }
    else
    {
        m_rView.GotFocus();
        Window::GetFocus();
#if !ENABLE_WASM_STRIP_ACCESSIBILITY
        m_rView.GetWrtShell().InvalidateAccessibleFocus();
#endif
    }
}

void SwEditWin::LoseFocus()
{
#if !ENABLE_WASM_STRIP_ACCESSIBILITY
    if (m_rView.GetWrtShellPtr())
        m_rView.GetWrtShell().InvalidateAccessibleFocus();
#endif
    Window::LoseFocus();
    if( s_pQuickHlpData && s_pQuickHlpData->m_bIsDisplayed )
        s_pQuickHlpData->Stop( m_rView.GetWrtShell() );
}

bool SwEditWin::IsViewReadonly() const
{
    SwWrtShell &rSh = m_rView.GetWrtShell();
    SfxViewShell* pNotifySh = rSh.GetSfxViewShell();
    return (m_rView.GetDocShell()->IsReadOnly() && rSh.IsCursorReadonly()) || (pNotifySh && pNotifySh->IsLokReadOnlyView());
}

void SwEditWin::Command( const CommandEvent& rCEvt )
{
    if (isDisposed())
    {
        // If ViewFrame dies shortly, no popup anymore!
        Window::Command(rCEvt);
        return;
    }

    SwWrtShell &rSh = m_rView.GetWrtShell();

    // The command event is send to the window after a possible context
    // menu from an inplace client has been closed. Now we have the chance
    // to deactivate the inplace client without any problem regarding parent
    // windows and code on the stack.
    SfxInPlaceClient* pIPClient = rSh.GetSfxViewShell()->GetIPClient();
    bool bIsOleActive = ( pIPClient && pIPClient->IsObjectInPlaceActive() );
    if ( bIsOleActive && ( rCEvt.GetCommand() == CommandEventId::ContextMenu ))
    {
        rSh.FinishOLEObj();
        return;
    }

    bool bCallBase      = true;

    switch ( rCEvt.GetCommand() )
    {
    case CommandEventId::ContextMenu:
    {
            const sal_uInt16 nId = SwInputChild::GetChildWindowId();
            SwInputChild* pChildWin = static_cast<SwInputChild*>(GetView().GetViewFrame().
                                                GetChildWindow( nId ));

            if (m_rView.GetPostItMgr()->IsHit(rCEvt.GetMousePosPixel()))
                return;

            Point aDocPos( PixelToLogic( rCEvt.GetMousePosPixel() ) );
            if ( !rCEvt.IsMouseEvent() )
                aDocPos = rSh.GetCharRect().Center();

            // Don't trigger the command on a frame anchored to header/footer is not editing it
            FrameControlType eControl;
            bool bOverFly = false;
            bool bPageAnchored = false;
            bool bOverHeaderFooterFly = IsOverHeaderFooterFly( aDocPos, eControl, bOverFly, bPageAnchored );
            // !bOverHeaderFooterFly doesn't mean we have a frame to select
            if ( !bPageAnchored && rCEvt.IsMouseEvent( ) &&
                 ( ( rSh.IsHeaderFooterEdit( ) && !bOverHeaderFooterFly && bOverFly ) ||
                   ( !rSh.IsHeaderFooterEdit( ) && bOverHeaderFooterFly ) ) )
            {
                return;
            }

            if((!pChildWin || pChildWin->GetView() != &m_rView) &&
                !rSh.IsDrawCreate() && !IsDrawAction())
            {
                CurrShell aCurr( &rSh );
                if (!m_pApplyTempl)
                {
                    if (g_bNoInterrupt)
                    {
                        ReleaseMouse();
                        g_bNoInterrupt = false;
                        m_bMBPressed = false;
                    }
                    if ( rCEvt.IsMouseEvent() )
                    {
                        SelectMenuPosition(rSh, rCEvt.GetMousePosPixel());
                        m_rView.StopShellTimer();
                    }
                    const Point aPixPos = LogicToPixel( aDocPos );

                    if ( m_rView.GetDocShell()->IsReadOnly() )
                    {
                        SwReadOnlyPopup aROPopup(aDocPos, m_rView);

                        ui::ContextMenuExecuteEvent aEvent;
                        aEvent.SourceWindow = VCLUnoHelper::GetInterface( this );
                        aEvent.ExecutePosition.X = aPixPos.X();
                        aEvent.ExecutePosition.Y = aPixPos.Y();
                        rtl::Reference<VCLXPopupMenu> xMenu;
                        rtl::Reference<VCLXPopupMenu> xMenuInterface = aROPopup.CreateMenuInterface();
                        if (GetView().TryContextMenuInterception(xMenuInterface, u"private:resource/ReadonlyContextMenu"_ustr, xMenu, aEvent))
                        {
                            if (xMenu.is())
                            {
                                css::uno::Reference<css::awt::XWindowPeer> xParent(aEvent.SourceWindow, css::uno::UNO_QUERY);
                                sal_uInt16 nExecId = xMenu->execute(xParent, css::awt::Rectangle(aPixPos.X(), aPixPos.Y(), 1, 1),
                                                                    css::awt::PopupMenuDirection::EXECUTE_DOWN);
                                if (!::ExecuteMenuCommand(xMenu, m_rView.GetViewFrame(), nExecId))
                                    aROPopup.Execute(this, nExecId);
                            }
                            else
                                aROPopup.Execute(this, aPixPos);
                        }
                    }
                    else if (!m_rView.ExecSpellPopup(aDocPos, rCEvt.IsMouseEvent()))
                        SfxDispatcher::ExecutePopup(this, &aPixPos);
                }
                else if (m_pApplyTempl->nUndo < rSh.GetDoc()->GetIDocumentUndoRedo().GetUndoActionCount())
                {
                    // Undo until we reach the point when we entered this context.
                    rSh.Do(SwWrtShell::UNDO);
                }
                bCallBase = false;
            }
    }
    break;

    case CommandEventId::Wheel:
    case CommandEventId::StartAutoScroll:
    case CommandEventId::AutoScroll:
            if (m_pSavedOutlineFrame && rSh.GetViewOptions()->IsShowOutlineContentVisibilityButton())
            {
                GetFrameControlsManager().RemoveControlsByType(FrameControlType::Outline, m_pSavedOutlineFrame);
                m_pSavedOutlineFrame = nullptr;
            }
            m_pShadCursor.reset();
            bCallBase = !m_rView.HandleWheelCommands( rCEvt );
            break;

    case CommandEventId::GestureZoom:
    {
        if (m_pSavedOutlineFrame && rSh.GetViewOptions()->IsShowOutlineContentVisibilityButton())
        {
            GetFrameControlsManager().RemoveControlsByType(FrameControlType::Outline, m_pSavedOutlineFrame);
            m_pSavedOutlineFrame = nullptr;
        }
        m_pShadCursor.reset();
        bCallBase = !m_rView.HandleGestureZoomCommand(rCEvt);
        break;
    }

    case CommandEventId::GesturePan:
    {
        if (m_pSavedOutlineFrame && rSh.GetViewOptions()->IsShowOutlineContentVisibilityButton())
        {
            GetFrameControlsManager().RemoveControlsByType(FrameControlType::Outline, m_pSavedOutlineFrame);
            m_pSavedOutlineFrame = nullptr;
        }
        m_pShadCursor.reset();
        bCallBase = !m_rView.HandleGesturePanCommand(rCEvt);
        break;
    }

    case CommandEventId::GestureLongPress:
    case CommandEventId::GestureSwipe: //nothing yet
            break;

    case CommandEventId::StartExtTextInput:
    {
        bool bIsViewReadOnly = IsViewReadonly();
        if(!bIsViewReadOnly)
        {
            if( rSh.HasDrawView() && rSh.GetDrawView()->IsTextEdit() )
            {
                bCallBase = false;
                rSh.GetDrawView()->GetTextEditOutlinerView()->Command( rCEvt );
            }
            else
            {
                if( rSh.HasSelection() )
                    rSh.DelRight();

                bCallBase = false;
                LanguageType eInputLanguage = GetInputLanguage();
                rSh.CreateExtTextInput(eInputLanguage);
            }
        }
        break;
    }
    case CommandEventId::EndExtTextInput:
    {
        bool bIsViewReadOnly = IsViewReadonly();

        if(!bIsViewReadOnly)
        {
            if( rSh.HasDrawView() && rSh.GetDrawView()->IsTextEdit() )
            {
                bCallBase = false;
                rSh.GetDrawView()->GetTextEditOutlinerView()->Command( rCEvt );
            }
            else
            {
                bCallBase = false;
                OUString sRecord = rSh.DeleteExtTextInput();
                uno::Reference< frame::XDispatchRecorder > xRecorder =
                        m_rView.GetViewFrame().GetBindings().GetRecorder();

                if ( !sRecord.isEmpty() )
                {
                    // convert quotes in IME text
                    // works on the last input character, this is especially in Korean text often done
                    // quotes that are inside of the string are not replaced!
                    const sal_Unicode aCh = sRecord[sRecord.getLength() - 1];
                    SvxAutoCorrCfg& rACfg = SvxAutoCorrCfg::Get();
                    SvxAutoCorrect* pACorr = rACfg.GetAutoCorrect();
                    if(pACorr &&
                        (( pACorr->IsAutoCorrFlag( ACFlags::ChgQuotes ) && ('\"' == aCh ))||
                        ( pACorr->IsAutoCorrFlag( ACFlags::ChgSglQuotes ) && ( '\'' == aCh))))
                    {
                        rSh.DelLeft();
                        rSh.AutoCorrect( *pACorr, aCh );
                    }

                    if ( xRecorder.is() )
                    {
                        // determine Shell
                        SfxShell *pSfxShell = lcl_GetTextShellFromDispatcher( m_rView );
                        // generate request and record
                        if (pSfxShell)
                        {
                            SfxRequest aReq(m_rView.GetViewFrame(), FN_INSERT_STRING);
                            aReq.AppendItem( SfxStringItem( FN_INSERT_STRING, sRecord ) );
                            aReq.Done();
                        }
                    }
                }
            }
        }
    }
    break;
    case CommandEventId::ExtTextInput:
    {
        bool bIsViewReadOnly = IsViewReadonly();

        if (!bIsViewReadOnly && !rSh.HasReadonlySel())
        {
            if( s_pQuickHlpData->m_bIsDisplayed )
                s_pQuickHlpData->Stop( rSh );

            if( rSh.HasDrawView() && rSh.GetDrawView()->IsTextEdit() )
            {
                bCallBase = false;
                rSh.GetDrawView()->GetTextEditOutlinerView()->Command( rCEvt );
            }
            else
            {
                const CommandExtTextInputData* pData = rCEvt.GetExtTextInputData();
                if( pData )
                {
                    bCallBase = false;
                    rSh.SetExtTextInputData( *pData );
                }
            }
            uno::Reference< frame::XDispatchRecorder > xRecorder =
                        m_rView.GetViewFrame().GetBindings().GetRecorder();
            if(!xRecorder.is())
            {
                SvxAutoCorrCfg& rACfg = SvxAutoCorrCfg::Get();
                if (!rACfg.IsAutoTextTip() || !ShowAutoText(rSh.GetChunkForAutoText()))
                {
                    SvxAutoCorrect* pACorr = rACfg.GetAutoCorrect();
                    if (pACorr && pACorr->GetSwFlags().bAutoCompleteWords)
                        ShowAutoCorrectQuickHelp(rSh.GetPrevAutoCorrWord(*pACorr), *pACorr);
                }
            }
        }

        if (rSh.HasReadonlySel())
        {
            // Inform the user that the request has been ignored.
            rSh.InfoReadOnlyDialog(true);
        }
    }
    break;
    case CommandEventId::CursorPos:
        // will be handled by the base class
        break;

    case CommandEventId::PasteSelection:
        if( !m_rView.GetDocShell()->IsReadOnly() )
        {
            TransferableDataHelper aDataHelper(
                        TransferableDataHelper::CreateFromPrimarySelection());
            if( !aDataHelper.GetXTransferable().is() )
                break;

            SotExchangeDest nDropDestination = GetDropDestination( rCEvt.GetMousePosPixel() );
            if( nDropDestination == SotExchangeDest::NONE )
                break;
            SotClipboardFormatId nDropFormat;
            sal_uInt8 nEventAction, nDropAction;
            SotExchangeActionFlags nActionFlags;
            nDropAction = SotExchange::GetExchangeAction(
                                aDataHelper.GetDataFlavorExVector(),
                                nDropDestination, EXCHG_IN_ACTION_COPY,
                                EXCHG_IN_ACTION_COPY, nDropFormat,
                                nEventAction,
                                SotClipboardFormatId::NONE, nullptr,
                                &nActionFlags );
            if( EXCHG_INOUT_ACTION_NONE != nDropAction )
            {
                const Point aDocPt( PixelToLogic( rCEvt.GetMousePosPixel() ) );
                SwTransferable::PasteData( aDataHelper, rSh, nDropAction, nActionFlags,
                                    nDropFormat, nDropDestination, false,
                                    false, &aDocPt, EXCHG_IN_ACTION_COPY,
                                    true );
            }
        }
        break;
        case CommandEventId::ModKeyChange :
        {
            const CommandModKeyData* pCommandData = rCEvt.GetModKeyData();
            if (!pCommandData->IsDown() && pCommandData->IsMod1() && !pCommandData->IsMod2())
            {
                sal_uInt16 nSlot = 0;
                if(pCommandData->IsLeftShift() && !pCommandData->IsRightShift())
                    nSlot = SID_ATTR_PARA_LEFT_TO_RIGHT;
                else if(!pCommandData->IsLeftShift() && pCommandData->IsRightShift())
                    nSlot = SID_ATTR_PARA_RIGHT_TO_LEFT;
                if(nSlot && SvtCTLOptions::IsCTLFontEnabled())
                    GetView().GetViewFrame().GetDispatcher()->Execute(nSlot);
            }
        }
        break;
        case CommandEventId::InputLanguageChange :
            // i#42732 - update state of fontname if input language changes
            g_bInputLanguageSwitched = true;
            SetUseInputLanguage( true );
        break;
        case CommandEventId::SelectionChange:
        {
            const CommandSelectionChangeData *pData = rCEvt.GetSelectionChangeData();
            rSh.SttCursorMove();
            rSh.GoStartSentence();
            rSh.GetCursor()->GetPoint()->AdjustContent(sal::static_int_cast<sal_uInt16, sal_uLong>(pData->GetStart()));
            rSh.SetMark();
            rSh.GetCursor()->GetMark()->AdjustContent(sal::static_int_cast<sal_uInt16, sal_uLong>(pData->GetEnd() - pData->GetStart()));
            rSh.EndCursorMove( true );
        }
        break;
        case CommandEventId::PrepareReconversion:
        if( rSh.HasSelection() )
        {
            SwPaM *pCursor = rSh.GetCursor();

            if( rSh.IsMultiSelection() )
            {
                if (pCursor && !pCursor->HasMark() &&
                    pCursor->GetPoint() == pCursor->GetMark())
                {
                    rSh.GoPrevCursor();
                    pCursor = rSh.GetCursor();
                }

                // Cancel all selections other than the last selected one.
                while( rSh.GetCursor()->GetNext() != rSh.GetCursor() )
                    delete rSh.GetCursor()->GetNext();
            }

            if( pCursor )
            {
                SwNodeOffset nPosNodeIdx = pCursor->GetPoint()->GetNodeIndex();
                const sal_Int32 nPosIdx = pCursor->GetPoint()->GetContentIndex();
                SwNodeOffset nMarkNodeIdx = pCursor->GetMark()->GetNodeIndex();
                const sal_Int32 nMarkIdx = pCursor->GetMark()->GetContentIndex();

                if( !rSh.GetCursor()->HasMark() )
                    rSh.GetCursor()->SetMark();

                rSh.SttCursorMove();

                if( nPosNodeIdx < nMarkNodeIdx )
                {
                    rSh.GetCursor()->GetPoint()->Assign(nPosNodeIdx, nPosIdx);
                    rSh.GetCursor()->GetMark()->Assign(nPosNodeIdx,
                        rSh.GetCursor()->GetPointContentNode()->Len());
                }
                else if( nPosNodeIdx == nMarkNodeIdx )
                {
                    rSh.GetCursor()->GetPoint()->Assign(nPosNodeIdx, nPosIdx);
                    rSh.GetCursor()->GetMark()->Assign(nMarkNodeIdx, nMarkIdx);
                }
                else
                {
                    rSh.GetCursor()->GetMark()->Assign(nMarkNodeIdx, nMarkIdx);
                    rSh.GetCursor()->GetPoint()->Assign(nMarkNodeIdx,
                        rSh.GetCursor()->GetMarkContentNode()->Len());
                }

                rSh.EndCursorMove( true );
            }
        }
        break;
        case CommandEventId::QueryCharPosition:
        {
            bool bVertical = rSh.IsInVerticalText();
            const SwPosition& rPos = *rSh.GetCursor()->GetPoint();
            SwDocShell* pDocSh = m_rView.GetDocShell();
            SwDoc *pDoc = pDocSh->GetDoc();
            SwExtTextInput* pInput = pDoc->GetExtTextInput( rPos.GetNode(), rPos.GetContentIndex() );
            if ( pInput )
            {
                const SwPosition& rStart = *pInput->Start();
                const SwPosition& rEnd = *pInput->End();
                sal_Int32 nSize = rEnd.GetContentIndex() - rStart.GetContentIndex();
                vcl::Window& rWin = rSh.GetView().GetEditWin();
                if ( nSize == 0 )
                {
                    // When the composition does not exist, use Caret rect instead.
                    const SwRect& aCaretRect ( rSh.GetCharRect() );
                    tools::Rectangle aRect( aCaretRect.Left(), aCaretRect.Top(), aCaretRect.Right(), aCaretRect.Bottom() );
                    rWin.SetCompositionCharRect( &aRect, 1, bVertical );
                }
                else
                {
                    std::unique_ptr<tools::Rectangle[]> aRects(new tools::Rectangle[ nSize ]);
                    int nRectIndex = 0;
                    for ( sal_Int32 nIndex = rStart.GetContentIndex(); nIndex < rEnd.GetContentIndex(); ++nIndex )
                    {
                        const SwPosition aPos( rStart.GetNode(), rStart.GetNode().GetContentNode(), nIndex );
                        SwRect aRect ( rSh.GetCharRect() );
                        rSh.GetCharRectAt( aRect, &aPos );
                        aRects[ nRectIndex ] = tools::Rectangle( aRect.Left(), aRect.Top(), aRect.Right(), aRect.Bottom() );
                        ++nRectIndex;
                    }
                    rWin.SetCompositionCharRect( aRects.get(), nSize, bVertical );
                }
            }
            bCallBase = false;
        }
        break;
        default:
            SAL_WARN("sw.ui", "unknown command.");
        break;
    }
    if (bCallBase)
        Window::Command(rCEvt);
}

/*  i#18686 select the object/cursor at the mouse
    position of the context menu request */
void SwEditWin::SelectMenuPosition(SwWrtShell& rSh, const Point& rMousePos )
{
    const Point aDocPos( PixelToLogic( rMousePos ) );
    const bool bIsInsideSelectedObj( rSh.IsInsideSelectedObj( aDocPos ) );
    //create a synthetic mouse event out of the coordinates
    MouseEvent aMEvt(rMousePos);
    SdrView *pSdrView = rSh.GetDrawView();
    if ( pSdrView )
    {
        // no close of insert_draw and reset of
        // draw mode, if context menu position is inside a selected object.
        if ( !bIsInsideSelectedObj && m_rView.GetDrawFuncPtr() )
        {

            m_rView.GetDrawFuncPtr()->Deactivate();
            m_rView.SetDrawFuncPtr(nullptr);
            m_rView.LeaveDrawCreate();
            SfxBindings& rBind = m_rView.GetViewFrame().GetBindings();
            rBind.Invalidate( SID_ATTR_SIZE );
            rBind.Invalidate( SID_TABLE_CELL );
        }

        // if draw text is active and there's a text selection
        // at the mouse position then do nothing
        if(rSh.GetSelectionType() & SelectionType::DrawObjectEditMode)
        {
            OutlinerView* pOLV = pSdrView->GetTextEditOutlinerView();
            ESelection aSelection = pOLV->GetSelection();
            if(aSelection != ESelection())
            {
                SdrOutliner* pOutliner = pSdrView->GetTextEditOutliner();
                bool bVertical = pOutliner->IsVertical();
                const EditEngine& rEditEng = pOutliner->GetEditEngine();
                Point aEEPos(aDocPos);
                const tools::Rectangle& rOutputArea = pOLV->GetOutputArea();
                // regard vertical mode
                if(bVertical)
                {
                    aEEPos -= rOutputArea.TopRight();
                    //invert the horizontal direction and exchange X and Y
                    tools::Long nTemp = -aEEPos.X();
                    aEEPos.setX( aEEPos.Y() );
                    aEEPos.setY( nTemp );
                }
                else
                    aEEPos -= rOutputArea.TopLeft();

                ESelection aCompare(rEditEng.FindDocPosition(aEEPos));
                // make it a forward selection - otherwise the IsLess/IsGreater do not work :-(
                aSelection.Adjust();
                if(!(aCompare < aSelection) && !(aCompare > aSelection))
                {
                    return;
                }
            }

        }

        if (pSdrView->MouseButtonDown( aMEvt, GetOutDev() ) )
        {
            pSdrView->MouseButtonUp( aMEvt, GetOutDev() );
            rSh.GetView().GetViewFrame().GetBindings().InvalidateAll(false);
            return;
        }
    }
    rSh.ResetCursorStack();

    if ( EnterDrawMode( aMEvt, aDocPos ) )
    {
        return;
    }
    if ( m_rView.GetDrawFuncPtr() && m_bInsFrame )
    {
        StopInsFrame();
        rSh.Edit();
    }

    UpdatePointer( aDocPos );

    if( !rSh.IsSelFrameMode() &&
        !GetView().GetViewFrame().GetDispatcher()->IsLocked() )
    {
        // Test if there is a draw object at that position and if it should be selected.
        bool bShould = rSh.ShouldObjectBeSelected(aDocPos);

        if(bShould)
        {
            m_rView.NoRotate();
            rSh.HideCursor();

            bool bUnLockView = !rSh.IsViewLocked();
            rSh.LockView( true );
            bool bSelObj = rSh.SelectObj( aDocPos );
            if( bUnLockView )
                rSh.LockView( false );

            if( bSelObj )
            {
                // in case the frame was deselected in the macro
                // just the cursor has to be displayed again.
                if( FrameTypeFlags::NONE == rSh.GetSelFrameType() )
                    rSh.ShowCursor();
                else
                {
                    if (rSh.IsFrameSelected() && m_rView.GetDrawFuncPtr())
                    {
                        m_rView.GetDrawFuncPtr()->Deactivate();
                        m_rView.SetDrawFuncPtr(nullptr);
                        m_rView.LeaveDrawCreate();
                        m_rView.AttrChangedNotify(nullptr);
                    }

                    rSh.EnterSelFrameMode( &aDocPos );
                    g_bFrameDrag = true;
                    UpdatePointer( aDocPos );
                    return;
                }
            }

            if (!m_rView.GetDrawFuncPtr())
                rSh.ShowCursor();
        }
    }
    else if ( rSh.IsSelFrameMode() &&
              (m_aActHitType == SdrHitKind::NONE ||
               !bIsInsideSelectedObj))
    {
        m_rView.NoRotate();
        bool bUnLockView = !rSh.IsViewLocked();
        rSh.LockView( true );

        if ( rSh.IsSelFrameMode() )
        {
            rSh.UnSelectFrame();
            rSh.LeaveSelFrameMode();
            m_rView.AttrChangedNotify(nullptr);
        }

        bool bSelObj = rSh.SelectObj( aDocPos, 0/*nFlag*/ );
        if( bUnLockView )
            rSh.LockView( false );

        if( !bSelObj )
        {
            // move cursor here so that it is not drawn in the
            // frame at first; ShowCursor() happens in LeaveSelFrameMode()
            g_bValidCursorPos = !(CRSR_POSCHG & rSh.CallSetCursor(&aDocPos, false));
            rSh.LeaveSelFrameMode();
            m_rView.LeaveDrawCreate();
            m_rView.AttrChangedNotify(nullptr);
        }
        else
        {
            rSh.HideCursor();
            rSh.EnterSelFrameMode( &aDocPos );
            rSh.SelFlyGrabCursor();
            rSh.MakeSelVisible();
            g_bFrameDrag = true;
            if( rSh.IsFrameSelected() &&
                m_rView.GetDrawFuncPtr() )
            {
                m_rView.GetDrawFuncPtr()->Deactivate();
                m_rView.SetDrawFuncPtr(nullptr);
                m_rView.LeaveDrawCreate();
                m_rView.AttrChangedNotify(nullptr);
            }
            UpdatePointer( aDocPos );
        }
    }
    else if ( rSh.IsSelFrameMode() && bIsInsideSelectedObj )
    {
        // Object at the mouse cursor is already selected - do nothing
        return;
    }

    if ( rSh.IsGCAttr() )
    {
        rSh.GCAttr();
        rSh.ClearGCAttr();
    }

    bool bOverSelect = rSh.TestCurrPam( aDocPos );
    bool bOverURLGrf = false;
    if( !bOverSelect )
        bOverURLGrf = bOverSelect = nullptr != rSh.IsURLGrfAtPos( aDocPos );

    if ( !bOverSelect )
    {
        // create only temporary move context because otherwise
        // the query against the content form doesn't work!!!
        SwMvContext aMvContext( &rSh );
        if (rSh.HasSelection())
            rSh.ResetSelect(&aDocPos, false, ScrollSizeMode::ScrollSizeDefault);
        rSh.SwCursorShell::SetCursor(aDocPos, false, /*Block=*/false, /*FieldInfo=*/true);
    }
    if( !bOverURLGrf )
    {
        const SelectionType nSelType = rSh.GetSelectionType();
        if( nSelType == SelectionType::Ole ||
            nSelType == SelectionType::Graphic )
        {
            SwMvContext aMvContext( &rSh );
            if( !rSh.IsFrameSelected() )
                rSh.GotoNextFly();
            rSh.EnterSelFrameMode();
        }
    }
}

void SwEditWin::DrawCommentGuideLine(Point aPointPixel)
{
    const Point aPointLogic = PixelToLogic(aPointPixel);

    sw::sidebarwindows::SidebarPosition eSidebarPosition
        = m_rView.GetPostItMgr()->GetSidebarPos(aPointLogic);
    if (eSidebarPosition == sw::sidebarwindows::SidebarPosition::NONE) // should never happen
        return;

    tools::Long nPosX;
    sal_uInt16 nZoom = m_rView.GetWrtShell().GetViewOptions()->GetZoom();
    if (eSidebarPosition == sw::sidebarwindows::SidebarPosition::RIGHT)
    {
        tools::Long nSidebarRectLeft
            = LogicToPixel(m_rView.GetPostItMgr()->GetSidebarRect(aPointLogic).TopLeft()).X();
        tools::Long nPxWidth = aPointPixel.X() - nSidebarRectLeft;
        nPosX = nSidebarRectLeft + std::clamp<tools::Long>(nPxWidth, 1 * nZoom, 8 * nZoom);
    }
    else
    {
        tools::Long nSidebarRectRight
            = LogicToPixel(m_rView.GetPostItMgr()->GetSidebarRect(aPointLogic).TopRight()).X();
        tools::Long nPxWidth = nSidebarRectRight - aPointPixel.X();
        nPosX = nSidebarRectRight - std::clamp<tools::Long>(nPxWidth, 1 * nZoom, 8 * nZoom);
    }


    // We need two InvertTracking calls here to "erase" the previous and draw the new position at each mouse move
    InvertTracking(aLastCommentSidebarPos, ShowTrackFlags::Clip | ShowTrackFlags::Split);
    const tools::Long nHeight = GetOutDev()->GetOutputSizePixel().Height();
    aLastCommentSidebarPos
        = tools::Rectangle(PixelToLogic(Point(nPosX, 0)), PixelToLogic(Point(nPosX, nHeight)));
    InvertTracking(aLastCommentSidebarPos, ShowTrackFlags::Clip | ShowTrackFlags::Split);
}

void SwEditWin::ReleaseCommentGuideLine()
{
    InvertTracking(aLastCommentSidebarPos, ShowTrackFlags::Clip | ShowTrackFlags::Split);
    aLastCommentSidebarPos = tools::Rectangle();
    mbIsDragSidebar = false;
}

void SwEditWin::SetSidebarWidth(const Point& rPointPixel)
{
    if (aLastCommentSidebarPos.IsEmpty())
        return;
    // aLastCommentSidebarPos right and left positions are the same so either can be used here
    m_rView.GetPostItMgr()->SetSidebarWidth(
        Point(aLastCommentSidebarPos.Right(), PixelToLogic(rPointPixel).Y()));
}

static SfxShell* lcl_GetTextShellFromDispatcher( SwView const & rView )
{
    // determine Shell
    SfxShell* pShell;
    SfxDispatcher* pDispatcher = rView.GetViewFrame().GetDispatcher();
    for(sal_uInt16  i = 0; true; ++i )
    {
        pShell = pDispatcher->GetShell( i );
        if( !pShell || dynamic_cast< const SwTextShell *>( pShell ) !=  nullptr )
            break;
    }
    return pShell;
}

IMPL_LINK_NOARG(SwEditWin, KeyInputFlushHandler, Timer *, void)
{
    FlushInBuffer();
}

void SwEditWin::InitStaticData()
{
    s_pQuickHlpData = new QuickHelpData();
}

void SwEditWin::FinitStaticData()
{
    delete s_pQuickHlpData;
}
/* i#3370 - remove quick help to prevent saving
 * of autocorrection suggestions */
void SwEditWin::StopQuickHelp()
{
    if( HasFocus() && s_pQuickHlpData && s_pQuickHlpData->m_bIsDisplayed  )
        s_pQuickHlpData->Stop( m_rView.GetWrtShell() );
}

IMPL_LINK_NOARG(SwEditWin, TemplateTimerHdl, Timer *, void)
{
    SetApplyTemplate(SwApplyTemplate());
}

void SwEditWin::SetChainMode( bool bOn )
{
    if ( !m_bChainMode )
        StopInsFrame();

    m_pUserMarker.reset();

    m_bChainMode = bOn;

    static const sal_uInt16 aInva[] =
    {
        FN_FRAME_CHAIN, FN_FRAME_UNCHAIN, 0
    };
    m_rView.GetViewFrame().GetBindings().Invalidate(aInva);
}

rtl::Reference<comphelper::OAccessible> SwEditWin::CreateAccessible()
{
#if !ENABLE_WASM_STRIP_ACCESSIBILITY
    SolarMutexGuard aGuard;   // this should have happened already!!!
    SwWrtShell *pSh = m_rView.GetWrtShellPtr();
    OSL_ENSURE( pSh, "no writer shell, no accessible object" );
    if( pSh )
        return pSh->CreateAccessible();
#endif
    return {};
}

void QuickHelpData::Move( QuickHelpData& rCpy )
{
    m_aHelpStrings.clear();
    m_aHelpStrings.swap( rCpy.m_aHelpStrings );

    m_bIsDisplayed = rCpy.m_bIsDisplayed;
    nCurArrPos = rCpy.nCurArrPos;
    m_bAppendSpace = rCpy.m_bAppendSpace;
    m_bIsTip = rCpy.m_bIsTip;
    m_bIsAutoText = rCpy.m_bIsAutoText;
}

void QuickHelpData::ClearContent()
{
    nCurArrPos = nNoPos;
    m_bIsDisplayed = m_bAppendSpace = false;
    nTipId = nullptr;
    m_aHelpStrings.clear();
    m_bIsTip = true;
    m_bIsAutoText = true;
}

void QuickHelpData::Start(SwWrtShell& rSh, const bool bRestart)
{
    if (bRestart)
    {
        nCurArrPos = 0;
    }
    m_bIsDisplayed = true;

    vcl::Window& rWin = rSh.GetView().GetEditWin();
    if( m_bIsTip )
    {
        Point aPt( rWin.OutputToScreenPixel( rWin.LogicToPixel(
                    rSh.GetCharRect().Pos() )));
        aPt.AdjustY( -3 );
        nTipId = Help::ShowPopover(&rWin, tools::Rectangle( aPt, Size( 1, 1 )),
                        CurStr(),
                        QuickHelpFlags::Left | QuickHelpFlags::Bottom);
    }
    else
    {
        OUString sStr(CurStr());
        sStr = sStr.copy(CurLen());
        sal_uInt16 nL = sStr.getLength();
        const ExtTextInputAttr nVal = ExtTextInputAttr::DottedUnderline |
                                ExtTextInputAttr::Highlight;
        const std::vector<ExtTextInputAttr> aAttrs( nL, nVal );
        CommandExtTextInputData aCETID( sStr, aAttrs.data(), nL,
                                        0, false );

        //fdo#33092. If the current input language is the default
        //language that text would appear in if typed, then don't
        //force a language on for the ExtTextInput.
        LanguageType eInputLanguage = rWin.GetInputLanguage();
        if (lcl_isNonDefaultLanguage(eInputLanguage,
            rSh.GetView(), sStr) == INVALID_HINT)
        {
            eInputLanguage = LANGUAGE_DONTKNOW;
        }

        rSh.CreateExtTextInput(eInputLanguage);
        rSh.SetExtTextInputData( aCETID );
    }
}

void QuickHelpData::Stop( SwWrtShell& rSh )
{
    if( !m_bIsTip )
        rSh.DeleteExtTextInput( false );
    else if( nTipId )
    {
        vcl::Window& rWin = rSh.GetView().GetEditWin();
        Help::HidePopover(&rWin, nTipId);
    }
    ClearContent();
}

void QuickHelpData::FillStrArr( SwWrtShell const & rSh, const OUString& rWord )
{
    enum Capitalization { CASE_LOWER, CASE_UPPER, CASE_SENTENCE, CASE_OTHER };

    // Determine word capitalization
    const CharClass& rCC = GetAppCharClass();
    const OUString sWordLower = rCC.lowercase( rWord );
    Capitalization aWordCase = CASE_OTHER;
    if ( !rWord.isEmpty() )
    {
        if ( rWord[0] == sWordLower[0] )
        {
            if ( rWord == sWordLower )
                aWordCase = CASE_LOWER;
        }
        else
        {
            // First character is not lower case i.e. assume upper or title case
            OUString sWordSentence = sWordLower.replaceAt( 0, 1, rtl::OUStringChar(rWord[0]) );
            if ( rWord == sWordSentence )
                aWordCase = CASE_SENTENCE;
            else
            {
                if ( rWord == rCC.uppercase( rWord ) )
                    aWordCase = CASE_UPPER;
            }
        }
    }

    SwCalendarWrapper& rCalendar = s_getCalendarWrapper();
    rCalendar.LoadDefaultCalendar( rSh.GetCurLang() );

    // Add matching calendar month and day names
    for ( const auto& aNames : { rCalendar.getMonths(), rCalendar.getDays() } )
    {
        for ( const auto& rName : aNames )
        {
            const OUString& rStr( rName.FullName );
            // Check string longer than word and case insensitive match
            if( rStr.getLength() > rWord.getLength() &&
                rCC.lowercase( rStr, 0, rWord.getLength() ) == sWordLower )
            {
                OUString sStr;

                //fdo#61251 if it's an exact match, ensure unchanged replacement
                //exists as a candidate
                if (rStr.startsWith(rWord))
                    m_aHelpStrings.emplace_back(rStr, rWord.getLength());
                else
                    sStr = rStr; // to be added if no case conversion is performed below

                if ( aWordCase == CASE_LOWER )
                    sStr = rCC.lowercase(rStr);
                else if ( aWordCase == CASE_SENTENCE )
                    sStr = rCC.lowercase(rStr).replaceAt(0, 1, rtl::OUStringChar(rStr[0]));
                else if ( aWordCase == CASE_UPPER )
                    sStr = rCC.uppercase(rStr);

                if (!sStr.isEmpty())
                    m_aHelpStrings.emplace_back(sStr, rWord.getLength());
            }
        }
    }

    // Add matching current date in ISO 8601 format, for example 2016-01-30
    OUString rStrToday;

    // do not suggest for single years, for example for "2016",
    // only for "201" or "2016-..." (to avoid unintentional text
    // insertion at line ending, for example typing "30 January 2016")
    if (!rWord.isEmpty() && rWord.getLength() != 4 && rWord[0] == '2')
    {
        rStrToday = utl::toISO8601(DateTime(Date(Date::SYSTEM)).GetUNODateTime());
        if (rStrToday.startsWith(rWord))
            m_aHelpStrings.emplace_back(rStrToday, rWord.getLength());
    }

    // Add matching words from AutoCompleteWord list
    const SwAutoCompleteWord& rACList = SwEditShell::GetAutoCompleteWords();
    std::vector<OUString> strings;

    if ( !rACList.GetWordsMatching( rWord, strings ) )
        return;

    for (const OUString & aCompletedString : strings)
    {
        // when we have a matching current date, avoid to suggest
        // other words with the same matching starting characters,
        // for example 2016-01-3 instead of 2016-01-30
        if (!rStrToday.isEmpty() && aCompletedString.startsWith(rWord))
            continue;

        OUString sStr;

        //fdo#61251 if it's an exact match, ensure unchanged replacement
        //exists as a candidate
        if (aCompletedString.startsWith(rWord))
            m_aHelpStrings.emplace_back(aCompletedString, rWord.getLength());
        else
            sStr = aCompletedString; // to be added if no case conversion is performed below

        if (aWordCase == CASE_LOWER)
            sStr = rCC.lowercase(aCompletedString);
        else if (aWordCase == CASE_SENTENCE)
            sStr = rCC.lowercase(aCompletedString)
                       .replaceAt(0, 1, rtl::OUStringChar(aCompletedString[0]));
        else if (aWordCase == CASE_UPPER)
            sStr = rCC.uppercase(aCompletedString);

        if (!sStr.isEmpty())
            m_aHelpStrings.emplace_back(aCompletedString, rWord.getLength());
    }
}

namespace {

class CompareIgnoreCaseAsciiFavorExact
{
    const OUString &m_rOrigWord;
public:
    explicit CompareIgnoreCaseAsciiFavorExact(const OUString& rOrigWord)
        : m_rOrigWord(rOrigWord)
    {
    }

    bool operator()(const std::pair<OUString, sal_uInt16>& s1,
                    const std::pair<OUString, sal_uInt16>& s2) const
    {
        int nRet = s1.first.compareToIgnoreAsciiCase(s2.first);
        if (nRet == 0)
        {
            //fdo#61251 sort stuff that starts with the exact rOrigWord before
            //another ignore-case candidate
            int n1StartsWithOrig = s1.first.startsWith(m_rOrigWord) ? 0 : 1;
            int n2StartsWithOrig = s2.first.startsWith(m_rOrigWord) ? 0 : 1;
            return n1StartsWithOrig < n2StartsWithOrig;
        }
        return nRet < 0;
    }
};

struct EqualIgnoreCaseAscii
{
    bool operator()(const std::pair<OUString, sal_uInt16>& s1,
                    const std::pair<OUString, sal_uInt16>& s2) const
    {
        return s1.first.equalsIgnoreAsciiCase(s2.first);
    }
};

} // anonymous namespace

// TODO Implement an i18n aware sort
void QuickHelpData::SortAndFilter(const OUString &rOrigWord)
{
    std::sort( m_aHelpStrings.begin(),
               m_aHelpStrings.end(),
               CompareIgnoreCaseAsciiFavorExact(rOrigWord) );

    const auto it
        = std::unique(m_aHelpStrings.begin(), m_aHelpStrings.end(), EqualIgnoreCaseAscii());
    m_aHelpStrings.erase( it, m_aHelpStrings.end() );

    nCurArrPos = 0;
}

// For a given chunk of typed text between 3 and 9 characters long that may start at a word boundary
// or in a whitespace and may include whitespaces, SwEditShell::GetChunkForAutoTextcreates a list of
// possible candidates for long AutoText names. Let's say, we have typed text "lorem ipsum  dr f";
// and the cursor is right after the "f". SwEditShell::GetChunkForAutoText would take "  dr f",
// since it's the longest chunk to the left of the cursor no longer than 9 characters, not starting
// in the middle of a word. Then it would create this list from it (in this order, longest first):
//     "  dr f"
//      " dr f"
//       "dr f"
// It cannot add "r f", because it starts in the middle of the word "dr"; also it cannot give " f",
// because it's only 2 characters long.
// Now the result of SwEditShell::GetChunkForAutoText is passed here to SwEditWin::ShowAutoText, and
// then to SwGlossaryList::HasLongName, where all existing autotext entries' long names are tested
// if they start with one of the list elements. The matches are sorted according the position of the
// candidate that matched first, then alphabetically inside the group of suggestions for a given
// candidate. Say, if we have these AutoText entry long names:
//    "Dr Frodo"
//    "Dr Credo"
//    "Or Bilbo"
//    "dr foo"
//    "  Dr Fuzz"
//    " dr Faust"
// the resulting list would be:
//    "  Dr Fuzz" -> matches the first (longest) item in the candidates list
//    " dr Faust" -> matches the second candidate item
//    "Dr Foo" -> first item of the two matching the third candidate; alphabetically sorted
//    "Dr Frodo" -> second item of the two matching the third candidate; alphabetically sorted
// Each of the resulting suggestions knows the length of the candidate it replaces, so accepting the
// first suggestion would replace 6 characters before cursor, while tabbing to and accepting the
// last suggestion would replace only 4 characters to the left of cursor.
bool SwEditWin::ShowAutoText(const std::vector<OUString>& rChunkCandidates)
{
    s_pQuickHlpData->ClearContent();
    if (!rChunkCandidates.empty())
    {
        SwGlossaryList* pList = ::GetGlossaryList();
        pList->HasLongName(rChunkCandidates, s_pQuickHlpData->m_aHelpStrings);
    }

    if (!s_pQuickHlpData->m_aHelpStrings.empty())
    {
        s_pQuickHlpData->Start(m_rView.GetWrtShell(), true);
    }
    return !s_pQuickHlpData->m_aHelpStrings.empty();
}

void SwEditWin::ShowAutoCorrectQuickHelp(
        const OUString& rWord, SvxAutoCorrect& rACorr )
{
    if (rWord.isEmpty())
        return;
    SwWrtShell& rSh = m_rView.GetWrtShell();
    s_pQuickHlpData->ClearContent();

    if( s_pQuickHlpData->m_aHelpStrings.empty() &&
        rACorr.GetSwFlags().bAutoCompleteWords )
    {
        s_pQuickHlpData->m_bIsAutoText = false;
        s_pQuickHlpData->m_bIsTip = rACorr.GetSwFlags().bAutoCmpltShowAsTip;

        // Get the necessary data to show help text.
        s_pQuickHlpData->FillStrArr( rSh, rWord );
    }

    if( !s_pQuickHlpData->m_aHelpStrings.empty() )
    {
        s_pQuickHlpData->SortAndFilter(rWord);
        s_pQuickHlpData->Start(rSh, true);
    }
}

bool SwEditWin::IsInHeaderFooter( const Point &rDocPt, FrameControlType &rControl ) const
{
    SwWrtShell &rSh = m_rView.GetWrtShell();
    const SwPageFrame* pPageFrame = rSh.GetLayout()->GetPageAtPos( rDocPt );

    if ( pPageFrame && pPageFrame->IsOverHeaderFooterArea( rDocPt, rControl ) )
        return true;

    if ( rSh.IsShowHeaderFooterSeparator( FrameControlType::Header ) || rSh.IsShowHeaderFooterSeparator( FrameControlType::Footer ) )
    {
        SwFrameControlsManager &rMgr = rSh.GetView().GetEditWin().GetFrameControlsManager();
        Point aPoint( LogicToPixel( rDocPt ) );

        if ( rSh.IsShowHeaderFooterSeparator( FrameControlType::Header ) )
        {
            SwFrameControlPtr pControl = rMgr.GetControl( FrameControlType::Header, pPageFrame );
            if ( pControl && pControl->Contains( aPoint ) )
            {
                rControl = FrameControlType::Header;
                return true;
            }
        }

        if ( rSh.IsShowHeaderFooterSeparator( FrameControlType::Footer ) )
        {
            SwFrameControlPtr pControl = rMgr.GetControl( FrameControlType::Footer, pPageFrame );
            if ( pControl && pControl->Contains( aPoint ) )
            {
                rControl = FrameControlType::Footer;
                return true;
            }
        }
    }

    return false;
}

bool SwEditWin::IsOverHeaderFooterFly( const Point& rDocPos, FrameControlType& rControl, bool& bOverFly, bool& bPageAnchored ) const
{
    bool bRet = false;
    Point aPt( rDocPos );
    SwWrtShell &rSh = m_rView.GetWrtShell();
    SwPaM aPam( *rSh.GetCurrentShellCursor().GetPoint() );
    rSh.GetLayout()->GetModelPositionForViewPoint( aPam.GetPoint(), aPt, nullptr, true );

    const SwStartNode* pStartFly = aPam.GetPoint()->GetNode().FindFlyStartNode();
    if ( pStartFly )
    {
        bOverFly = true;
        SwFrameFormat* pFlyFormat = pStartFly->GetFlyFormat( );
        if ( pFlyFormat )
        {
            const SwNode* pAnchorNode = pFlyFormat->GetAnchor( ).GetAnchorNode( );
            if ( pAnchorNode )
            {
                bool bInHeader = pAnchorNode->FindHeaderStartNode( ) != nullptr;
                bool bInFooter = pAnchorNode->FindFooterStartNode( ) != nullptr;

                bRet = bInHeader || bInFooter;
                if ( bInHeader )
                    rControl = FrameControlType::Header;
                else if ( bInFooter )
                    rControl = FrameControlType::Footer;
            }
            else
                bPageAnchored = pFlyFormat->GetAnchor( ).GetAnchorId( ) == RndStdIds::FLY_AT_PAGE;
        }
    }
    else
        bOverFly = false;
    return bRet;
}

void SwEditWin::SetUseInputLanguage( bool bNew )
{
    if ( bNew || m_bUseInputLanguage )
    {
        SfxBindings& rBind = GetView().GetViewFrame().GetBindings();
        rBind.Invalidate( SID_ATTR_CHAR_FONT );
        rBind.Invalidate( SID_ATTR_CHAR_FONTHEIGHT );
    }
    m_bUseInputLanguage = bNew;
}

OUString SwEditWin::GetSurroundingText() const
{
    SwWrtShell& rSh = m_rView.GetWrtShell();

    if (rSh.HasDrawView() && rSh.GetDrawView()->IsTextEdit())
        return rSh.GetDrawView()->GetTextEditOutlinerView()->GetSurroundingText();

    OUString sReturn;
    if( rSh.HasSelection() && !rSh.IsMultiSelection() && rSh.IsSelOnePara() )
        rSh.GetSelectedText( sReturn, ParaBreakType::ToOnlyCR  );
    else if( !rSh.HasSelection() )
    {
        bool bUnLockView = !rSh.IsViewLocked();
        rSh.LockView(true);

        // store shell state *before* Push
        ::std::optional<SwCallLink> aLink(std::in_place, rSh);
        rSh.Push();

        // disable accessible events for internal-only helper cursor
        const bool bSendAccessibleEventOld = rSh.IsSendAccessibleCursorEvents();
        rSh.SetSendAccessibleCursorEvents(false);

        // get the sentence around the cursor
        rSh.HideCursor();
        rSh.GoStartSentence();
        rSh.SetMark();
        rSh.GoEndSentence();
        rSh.GetSelectedText( sReturn, ParaBreakType::ToOnlyCR  );

        rSh.Pop(SwCursorShell::PopMode::DeleteCurrent, aLink);
        rSh.SetSendAccessibleCursorEvents(bSendAccessibleEventOld);
        rSh.HideCursor();

        if (bUnLockView)
            rSh.LockView(false);
    }

    return sReturn;
}

Selection SwEditWin::GetSurroundingTextSelection() const
{
    SwWrtShell& rSh = m_rView.GetWrtShell();

    if (rSh.HasDrawView() && rSh.GetDrawView()->IsTextEdit())
        return rSh.GetDrawView()->GetTextEditOutlinerView()->GetSurroundingTextSelection();

    Selection aSel(0, 0);
    if( rSh.HasSelection() )
    {
        OUString sReturn;
        rSh.GetSelectedText( sReturn, ParaBreakType::ToOnlyCR  );
        aSel = Selection( 0, sReturn.getLength() );
    }
    else if (rSh.GetCursor()->GetPoint()->GetNode().GetTextNode())
    {
        bool bUnLockView = !rSh.IsViewLocked();
        rSh.LockView(true);

        // Return the position of the visible cursor in the sentence
        // around the visible cursor.
        TextFrameIndex const nPos(rSh.GetCursorPointAsViewIndex());

        // store shell state *before* Push
        ::std::optional<SwCallLink> aLink(std::in_place, rSh);
        rSh.Push();

        // disable accessible events for internal-only helper cursor
        const bool bSendAccessibleEventOld = rSh.IsSendAccessibleCursorEvents();
        rSh.SetSendAccessibleCursorEvents(false);

        rSh.HideCursor();
        rSh.GoStartSentence();
        TextFrameIndex const nStartPos(rSh.GetCursorPointAsViewIndex());

        rSh.Pop(SwCursorShell::PopMode::DeleteCurrent, aLink);
        rSh.SetSendAccessibleCursorEvents(bSendAccessibleEventOld);
        rSh.ShowCursor();

        if (bUnLockView)
            rSh.LockView(false);

        aSel = Selection(sal_Int32(nPos - nStartPos), sal_Int32(nPos - nStartPos));
    }

    return aSel;
}

bool SwEditWin::DeleteSurroundingText(const Selection& rSelection)
{
    SwWrtShell& rSh = m_rView.GetWrtShell();

    if (rSh.HasDrawView() && rSh.GetDrawView()->IsTextEdit())
        return rSh.GetDrawView()->GetTextEditOutlinerView()->DeleteSurroundingText(rSelection);

    if (rSh.HasSelection())
        return false;

    // rSelection is relative to the start of the sentence, so find that and
    // adjust the range by it
    rSh.Push();

    // disable accessible events for internal-only helper cursor
    const bool bSendAccessibleEventOld = rSh.IsSendAccessibleCursorEvents();
    rSh.SetSendAccessibleCursorEvents(false);

    rSh.HideCursor();
    rSh.GoStartSentence();
    TextFrameIndex const nStartPos(rSh.GetCursorPointAsViewIndex());

    rSh.Pop(SwCursorShell::PopMode::DeleteCurrent);
    rSh.SetSendAccessibleCursorEvents(bSendAccessibleEventOld);
    rSh.ShowCursor();

    if (rSh.SelectTextView(nStartPos + TextFrameIndex(rSelection.Min()), nStartPos + TextFrameIndex(rSelection.Max())))
    {
        rSh.Delete(false);
        return true;
    }

    return false;
}

void SwEditWin::LogicInvalidate(const tools::Rectangle* pRectangle)
{
    SfxLokHelper::notifyInvalidation(&m_rView, pRectangle);
}

void SwEditWin::SetCursorTwipPosition(const Point& rPosition, bool bPoint, bool bClearMark)
{
    if (SdrView* pSdrView = m_rView.GetWrtShell().GetDrawView())
    {
        // Editing shape text, then route the call to editeng.
        if (pSdrView->GetTextEditObject())
        {
            EditView& rEditView = pSdrView->GetTextEditOutlinerView()->GetEditView();
            rEditView.SetCursorLogicPosition(rPosition, bPoint, bClearMark);
            return;
        }
    }

    if (m_rView.GetPostItMgr())
    {
        if (sw::annotation::SwAnnotationWin* pWin = m_rView.GetPostItMgr()->GetActiveSidebarWin())
        {
            // Editing postit text.
            pWin->SetCursorLogicPosition(rPosition, bPoint, bClearMark);
            return;
        }
    }

    // Not an SwWrtShell, as that would make SwCursorShell::GetCursor() inaccessible.
    SwEditShell& rShell = m_rView.GetWrtShell();

    bool bCreateSelection = false;
    {
        SwMvContext aMvContext(&rShell);
        if (bClearMark)
            rShell.ClearMark();
        else
            bCreateSelection = !rShell.HasMark();

        if (bCreateSelection)
            m_rView.GetWrtShell().SttSelect();

        // If the mark is to be updated, then exchange the point and mark before
        // and after, as we can't easily set the mark.
        if (!bPoint)
            rShell.getShellCursor(/*bBlock=*/false)->Exchange();
        rShell.SetCursor(rPosition);
        if (!bPoint)
            rShell.getShellCursor(/*bBlock=*/false)->Exchange();
    }

    if (bCreateSelection)
        m_rView.GetWrtShell().EndSelect();
}

void SwEditWin::SetGraphicTwipPosition(bool bStart, const Point& rPosition)
{
    if (bStart)
    {
        MouseEvent aClickEvent(rPosition, 1, MouseEventModifiers::SIMPLECLICK, MOUSE_LEFT);
        MouseButtonDown(aClickEvent);
        MouseEvent aMoveEvent(Point(rPosition.getX() + MIN_MOVE + 1, rPosition.getY()), 0, MouseEventModifiers::SIMPLEMOVE, MOUSE_LEFT);
        MouseMove(aMoveEvent);
    }
    else
    {
        MouseEvent aMoveEvent(Point(rPosition.getX() - MIN_MOVE - 1, rPosition.getY()), 0, MouseEventModifiers::SIMPLEMOVE, MOUSE_LEFT);
        MouseMove(aMoveEvent);
        MouseEvent aClickEvent(rPosition, 1, MouseEventModifiers::SIMPLECLICK, MOUSE_LEFT);
        MouseButtonUp(aClickEvent);
    }
}

SwFrameControlsManager& SwEditWin::GetFrameControlsManager()
{
    return *m_pFrameControlsManager;
}

void SwEditWin::ToggleOutlineContentVisibility(const size_t nOutlinePos, const bool bSubs)
{
    // bSubs purpose is to set all sub level outline content to the same visibility as
    // nOutlinePos outline content visibility is toggled. It is only applicable when not treating
    // sub outline levels as content.
    SwWrtShell& rSh = GetView().GetWrtShell();

    if (GetView().GetDrawView()->IsTextEdit())
        rSh.EndTextEdit();
    if (GetView().IsDrawMode())
        GetView().LeaveDrawCreate();
    rSh.EnterStdMode();

    if (!bSubs || rSh.GetViewOptions()->IsTreatSubOutlineLevelsAsContent())
    {
        SwNode* pNode = rSh.GetNodes().GetOutLineNds()[nOutlinePos];
        bool bVisible = pNode->GetTextNode()->GetAttrOutlineContentVisible();
        pNode->GetTextNode()->SetAttrOutlineContentVisible(!bVisible);
    }
    else if (bSubs)
    {
        // also toggle sub levels to the same content visibility
        SwOutlineNodes::size_type nPos = nOutlinePos;
        SwOutlineNodes::size_type nOutlineNodesCount
                = rSh.getIDocumentOutlineNodesAccess()->getOutlineNodesCount();
        int nLevel = rSh.getIDocumentOutlineNodesAccess()->getOutlineLevel(nOutlinePos);
        bool bVisible = rSh.IsOutlineContentVisible(nOutlinePos);
        do
        {
            if (rSh.IsOutlineContentVisible(nPos) == bVisible)
                rSh.GetNodes().GetOutLineNds()[nPos]->GetTextNode()->SetAttrOutlineContentVisible(!bVisible);
        } while (++nPos < nOutlineNodesCount
                 && rSh.getIDocumentOutlineNodesAccess()->getOutlineLevel(nPos) > nLevel);
    }

    rSh.InvalidateOutlineContentVisibility();
    rSh.GotoOutline(nOutlinePos);
    rSh.SetModified();
    GetView().GetDocShell()->Broadcast(SfxHint(SfxHintId::DocChanged));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
