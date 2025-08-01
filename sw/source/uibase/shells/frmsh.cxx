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

#include <config_features.h>

#include <hintids.hxx>
#include <svl/whiter.hxx>
#include <sfx2/viewfrm.hxx>
#include <basic/sbstar.hxx>
#include <svl/ptitem.hxx>
#include <svl/stritem.hxx>
#include <svl/intitem.hxx>
#include <svl/eitem.hxx>
#include <editeng/colritem.hxx>
#include <editeng/lineitem.hxx>
#include <editeng/boxitem.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/request.hxx>
#include <sfx2/objface.hxx>
#include <vcl/EnumContext.hxx>
#include <svx/hlnkitem.hxx>
#include <svx/svdview.hxx>
#include <svx/sdangitm.hxx>
#include <vcl/commandinfoprovider.hxx>
#include <sal/log.hxx>

#include <doc.hxx>
#include <drawdoc.hxx>
#include <IDocumentSettingAccess.hxx>
#include <IDocumentDrawModelAccess.hxx>
#include <fmturl.hxx>
#include <fmtclds.hxx>
#include <fmtcnct.hxx>
#include <swmodule.hxx>
#include <wrtsh.hxx>
#include <wview.hxx>
#include <uitool.hxx>
#include <frmfmt.hxx>
#include <frmsh.hxx>
#include <frmmgr.hxx>
#include <edtwin.hxx>
#include <swdtflvr.hxx>
#include <viewopt.hxx>

#include <cmdid.h>
#include <strings.hrc>
#include <swabstdlg.hxx>

#include <svx/svxdlg.hxx>

#include <docsh.hxx>
#include <svx/drawitem.hxx>
#include <memory>

#define ShellClass_SwFrameShell
#include <sfx2/msg.hxx>
#include <swslots.hxx>
#include <grfatr.hxx>
#include <fldmgr.hxx>
#include <flyfrm.hxx>

using ::editeng::SvxBorderLine;
using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;

// Prototypes
static void lcl_FrameGetMaxLineWidth(const SvxBorderLine* pBorderLine, SvxBorderLine& rBorderLine);

SFX_IMPL_INTERFACE(SwFrameShell, SwBaseShell)

void SwFrameShell::InitInterface_Impl()
{
    GetStaticInterface()->RegisterPopupMenu(u"frame"_ustr);

    GetStaticInterface()->RegisterObjectBar(SFX_OBJECTBAR_OBJECT, SfxVisibilityFlags::Invisible, ToolbarId::Frame_Toolbox);
}

void SwFrameShell::ExecMove(SfxRequest& rReq)
{
    SwWrtShell& rSh = GetShell();
    sal_uInt16 nSlot = rReq.GetSlot();
    switch (nSlot)
    {
        case SID_SELECTALL:
            rSh.SelAll();
            rReq.Done();
            break;
    }
}

void SwFrameShell::ExecField(const SfxRequest& rReq)
{
    SwWrtShell& rSh = GetShell();
    sal_uInt16 nSlot = rReq.GetSlot();
    switch (nSlot)
    {
        case FN_POSTIT:
            SwFieldMgr aFieldMgr(&rSh);
            rSh.InsertPostIt(aFieldMgr, rReq);
            break;
    }
}

void SwFrameShell::Execute(SfxRequest &rReq)
{
    //First those who do not need FrameMgr.
    SwWrtShell &rSh = GetShell();
    bool bMore = false;
    const SfxItemSet* pArgs = rReq.GetArgs();
    const SfxPoolItem* pItem;
    sal_uInt16 nSlot = rReq.GetSlot();

    switch ( nSlot )
    {
        case FN_FRAME_TO_ANCHOR:
            if ( rSh.IsFrameSelected() )
            {
                rSh.GotoFlyAnchor();
                rSh.EnterStdMode();
                rSh.CallChgLnk();
            }
            break;
        case SID_FRAME_TO_TOP:
            rSh.SelectionToTop();
            break;

        case SID_FRAME_TO_BOTTOM:
            rSh.SelectionToBottom();
            break;

        case FN_FRAME_UP:
            rSh.SelectionToTop( false );
            break;

        case FN_FRAME_DOWN:
            rSh.SelectionToBottom( false );
            break;
        case FN_INSERT_FRAME:
            if (!pArgs)
            {
                // Frame already exists, open frame dialog for editing.
                SfxStringItem aDefPage(FN_FORMAT_FRAME_DLG, u"columns"_ustr);
                rSh.GetView().GetViewFrame().GetDispatcher()->ExecuteList(
                        FN_FORMAT_FRAME_DLG,
                        SfxCallMode::SYNCHRON|SfxCallMode::RECORD,
                        { &aDefPage });

            }
            else
            {
                // Frame already exists, only the number of columns will be changed.
                sal_uInt16 nCols = 1;
                if(const SfxUInt16Item* pColsItem = pArgs->GetItemIfSet(SID_ATTR_COLUMNS, false))
                    nCols = pColsItem->GetValue();

                SfxItemSetFixed<RES_COL,RES_COL> aSet(GetPool());
                rSh.GetFlyFrameAttr( aSet );
                SwFormatCol aCol(aSet.Get(RES_COL));
                // GutterWidth will not always passed, hence get firstly
                // (see view2: Execute on this slot)
                sal_uInt16 nGutterWidth = aCol.GetGutterWidth();
                if(!nCols )
                    nCols++;
                aCol.Init(nCols, nGutterWidth, aCol.GetWishWidth());
                aSet.Put(aCol);
                // Template AutoUpdate
                SwFrameFormat* pFormat = rSh.GetSelectedFrameFormat();
                if(pFormat && pFormat->IsAutoUpdateOnDirectFormat())
                {
                    rSh.AutoUpdateFrame(pFormat, aSet);
                }
                else
                {
                    rSh.StartAllAction();
                    rSh.SetFlyFrameAttr( aSet );
                    rSh.SetModified();
                    rSh.EndAllAction();
                }

            }
            return;

        case SID_HYPERLINK_SETLINK:
        {
            if(pArgs && SfxItemState::SET == pArgs->GetItemState(SID_HYPERLINK_SETLINK, false, &pItem))
            {
                const SvxHyperlinkItem& rHLinkItem = *static_cast<const SvxHyperlinkItem *>(pItem);
                const OUString& rURL = rHLinkItem.GetURL();
                const OUString& rTarget = rHLinkItem.GetTargetFrame();

                SfxItemSetFixed<RES_URL, RES_URL> aSet( rSh.GetAttrPool() );
                rSh.GetFlyFrameAttr( aSet );
                SwFormatURL aURL( aSet.Get( RES_URL ) );

                OUString sOldName(rHLinkItem.GetName().toAsciiUpperCase());
                OUString sFlyName(rSh.GetFlyName().toString().toAsciiUpperCase());
                if (sOldName != sFlyName)
                {
                    OUString sName(sOldName);
                    sal_uInt16 i = 1;
                    while (rSh.FindFlyByName(UIName(sName)))
                    {
                        sName = sOldName + "_" + OUString::number(i++);
                    }
                    rSh.SetFlyName(UIName(sName));
                }
                aURL.SetURL( rURL, false );
                aURL.SetTargetFrameName(rTarget);

                aSet.Put( aURL );
                rSh.SetFlyFrameAttr( aSet );
            }
        }
        break;

        case FN_FRAME_CHAIN:
            rSh.GetView().GetEditWin().SetChainMode( !rSh.GetView().GetEditWin().IsChainMode() );
            break;

        case FN_FRAME_UNCHAIN:
            rSh.Unchain( *rSh.GetFlyFrameFormat() );
            GetView().GetViewFrame().GetBindings().Invalidate(FN_FRAME_CHAIN);
            break;
        case FN_FORMAT_FOOTNOTE_DLG:
        {
            GetView().ExecFormatFootnote();
            break;
        }
        case FN_NUMBERING_OUTLINE_DLG:
        {
            GetView().ExecNumberingOutline(GetPool());
            rReq.Done();
            break;
        }
        case SID_OPEN_XML_FILTERSETTINGS:
        {
            HandleOpenXmlFilterSettings(rReq);
        }
        break;
        case FN_WORDCOUNT_DIALOG:
        {
            GetView().UpdateWordCount(this, nSlot);
            break;
        }
        case FN_UNFLOAT_FRAME:
        {
            rSh.UnfloatFlyFrame();
            rReq.Done();
            break;
        }
        default: bMore = true;
    }

    if ( !bMore )
    {
        return;
    }

    SwFlyFrameAttrMgr aMgr( false, &rSh, Frmmgr_Type::NONE, nullptr );
    bool bUpdateMgr = true;
    bool bCopyToFormat = false;
    switch ( nSlot )
    {
        case SID_OBJECT_ALIGN_MIDDLE:
        case FN_FRAME_ALIGN_VERT_CENTER:
            aMgr.SetVertOrientation( text::VertOrientation::CENTER );
            break;
        case SID_OBJECT_ALIGN_DOWN :
        case FN_FRAME_ALIGN_VERT_BOTTOM:
            aMgr.SetVertOrientation( text::VertOrientation::BOTTOM );
            break;
        case SID_OBJECT_ALIGN_UP :
        case FN_FRAME_ALIGN_VERT_TOP:
            aMgr.SetVertOrientation( text::VertOrientation::TOP );
            break;

        case FN_FRAME_ALIGN_VERT_CHAR_CENTER:
            aMgr.SetVertOrientation( text::VertOrientation::CHAR_CENTER );
            break;

        case FN_FRAME_ALIGN_VERT_CHAR_BOTTOM:
            aMgr.SetVertOrientation( text::VertOrientation::CHAR_BOTTOM );
            break;

        case FN_FRAME_ALIGN_VERT_CHAR_TOP:
            aMgr.SetVertOrientation( text::VertOrientation::CHAR_TOP );
            break;

        case FN_FRAME_ALIGN_VERT_ROW_CENTER:
            aMgr.SetVertOrientation( text::VertOrientation::LINE_CENTER );
            break;

        case FN_FRAME_ALIGN_VERT_ROW_BOTTOM:
            aMgr.SetVertOrientation( text::VertOrientation::LINE_BOTTOM );
            break;

        case FN_FRAME_ALIGN_VERT_ROW_TOP:
            aMgr.SetVertOrientation( text::VertOrientation::LINE_TOP );
            break;
        case SID_OBJECT_ALIGN_CENTER :
        case FN_FRAME_ALIGN_HORZ_CENTER:
            aMgr.SetHorzOrientation( text::HoriOrientation::CENTER );
            break;
        case SID_OBJECT_ALIGN_RIGHT:
        case FN_FRAME_ALIGN_HORZ_RIGHT:
            aMgr.SetHorzOrientation( text::HoriOrientation::RIGHT );
            break;
        case SID_OBJECT_ALIGN_LEFT:
        case FN_FRAME_ALIGN_HORZ_LEFT:
            aMgr.SetHorzOrientation( text::HoriOrientation::LEFT );
            break;

        case FN_SET_FRM_POSITION:
        {
            aMgr.SetAbsPos(static_cast<const SfxPointItem &>(pArgs->Get
                                (FN_SET_FRM_POSITION)).GetValue());
        }
        break;
        case SID_ATTR_BRUSH:
        {
            if(pArgs)
            {
                aMgr.SetAttrSet( *pArgs );
                bCopyToFormat = true;
            }
        }
        break;
        case SID_ATTR_ULSPACE:
        case SID_ATTR_LRSPACE:
        {
            if(pArgs && SfxItemState::SET == pArgs->GetItemState(GetPool().GetWhichIDFromSlotID(nSlot), false, &pItem))
            {
                aMgr.SetAttrSet( *pArgs );
                bCopyToFormat = true;
            }
        }
        break;

        case SID_ATTR_TRANSFORM:
        {
            bool bApplyNewPos = false;
            bool bApplyNewSize = false;

            Point aNewPos = aMgr.GetPos();
            if (pArgs)
            {
                if (const SfxInt32Item* pXItem = pArgs->GetItemIfSet(SID_ATTR_TRANSFORM_POS_X, false))
                {
                    aNewPos.setX( pXItem->GetValue() );
                    bApplyNewPos = true;
                }
                if (const SfxInt32Item* pYItem = pArgs->GetItemIfSet(SID_ATTR_TRANSFORM_POS_Y, false))
                {
                    aNewPos.setY( pYItem->GetValue() );
                    bApplyNewPos = true;
                }
            }

            Size aNewSize = aMgr.GetSize();
            if (pArgs)
            {
                if (const SfxUInt32Item* pWidthItem = pArgs->GetItemIfSet(SID_ATTR_TRANSFORM_WIDTH, false))
                {
                    aNewSize.setWidth( pWidthItem->GetValue() );
                    bApplyNewSize = true;
                }
                if (const SfxUInt32Item* pHeightItem = pArgs->GetItemIfSet(SID_ATTR_TRANSFORM_HEIGHT, false))
                {
                    aNewSize.setHeight( pHeightItem->GetValue() );
                    bApplyNewSize = true;
                }
            }

            if (pArgs && (pArgs->HasItem(SID_ATTR_TRANSFORM_ANGLE) || pArgs->HasItem(SID_ATTR_TRANSFORM_DELTA_ANGLE)))
            {
                SfxItemSetFixed<RES_GRFATR_ROTATION, RES_GRFATR_ROTATION> aSet(rSh.GetAttrPool() );
                rSh.GetCurAttr(aSet);
                const SwRotationGrf& rRotation = aSet.Get(RES_GRFATR_ROTATION);
                const Degree10 nOldRot(rRotation.GetValue());

                if (const SdrAngleItem* pAngleItem = pArgs->GetItemIfSet(SID_ATTR_TRANSFORM_DELTA_ANGLE, false))
                {
                    const Degree10 nDeltaRot = to<Degree10>(pAngleItem->GetValue());
                    aMgr.SetRotation(nOldRot, nOldRot + nDeltaRot, rRotation.GetUnrotatedSize());
                }

                // RotGrfFlyFrame: Get Value and disable is in SwGrfShell::GetAttrStateForRotation, but the
                // value setter uses SID_ATTR_TRANSFORM and a group of three values. Rotation is
                // added now, so use it in this central place. Do no forget to convert angle from
                // 100th degrees in SID_ATTR_TRANSFORM_ANGLE to 10th degrees in RES_GRFATR_ROTATION
                if (const SdrAngleItem* pTransformItem = pArgs->GetItemIfSet(SID_ATTR_TRANSFORM_ANGLE, false))
                {
                    const Degree10 nNewRot = to<Degree10>(pTransformItem->GetValue());

                    // RotGrfFlyFrame: Rotation change here, SwFlyFrameAttrMgr aMgr is available
                    aMgr.SetRotation(nOldRot, nNewRot, rRotation.GetUnrotatedSize());
                }
            }

            if (bApplyNewPos)
            {
                aMgr.SetAbsPos(aNewPos);
            }
            if ( bApplyNewSize )
            {
                aMgr.SetSize( aNewSize );
            }
            if (!bApplyNewPos && !bApplyNewSize)
            {
                bUpdateMgr = false;
            }

        }
        break;

        case FN_FORMAT_FRAME_DLG:
        case FN_DRAW_WRAP_DLG:
        {
            const SelectionType nSel = rSh.GetSelectionType();
            if (nSel & SelectionType::Graphic)
            {
                rSh.GetView().GetViewFrame().GetDispatcher()->Execute(FN_FORMAT_GRAFIC_DLG);
                bUpdateMgr = false;
            }
            else
            {
                SfxItemSetFixed<
                        RES_FRMATR_BEGIN, RES_FRMATR_END - 1,
                        // FillAttribute support:
                        XATTR_FILL_FIRST, XATTR_FILL_LAST,
                        SID_DOCFRAME, SID_DOCFRAME,
                        SID_ATTR_BRUSH, SID_ATTR_BRUSH,
                        SID_ATTR_BORDER_INNER, SID_ATTR_BORDER_INNER,
                        SID_ATTR_LRSPACE, SID_ATTR_ULSPACE,
                        SID_ATTR_PAGE_SIZE, SID_ATTR_PAGE_SIZE,
                        // Items to hand over XPropertyList things like
                        // XColorList, XHatchList, XGradientList, and
                        // XBitmapList to the Area TabPage
                        SID_COLOR_TABLE, SID_PATTERN_LIST,
                        SID_HTML_MODE, SID_HTML_MODE,
                        FN_GET_PRINT_AREA, FN_GET_PRINT_AREA,
                        FN_SURROUND, FN_KEEP_ASPECT_RATIO,
                        FN_SET_FRM_ALT_NAME, FN_SET_FRM_ALT_NAME,
                        FN_UNO_DESCRIPTION, FN_UNO_DESCRIPTION,
                        FN_OLE_IS_MATH, FN_MATH_BASELINE_ALIGNMENT,
                        FN_PARAM_CHAIN_PREVIOUS, FN_PARAM_CHAIN_NEXT>  aSet( GetPool() );

                // create needed items for XPropertyList entries from the DrawModel so that
                // the Area TabPage can access them
                const SwDrawModel* pDrawModel = rSh.GetView().GetDocShell()->GetDoc()->getIDocumentDrawModelAccess().GetDrawModel();
                pDrawModel->PutAreaListItems(aSet);

                const SwViewOption* pVOpt = rSh.GetViewOptions();
                if(nSel & SelectionType::Ole)
                    aSet.Put( SfxBoolItem(FN_KEEP_ASPECT_RATIO, pVOpt->IsKeepRatio()) );
                aSet.Put(SfxUInt16Item(SID_HTML_MODE, ::GetHtmlMode(GetView().GetDocShell())));
                aSet.Put(SfxStringItem(FN_SET_FRM_NAME, rSh.GetFlyName().toString()));
                aSet.Put(SfxStringItem(FN_UNO_DESCRIPTION, rSh.GetObjDescription()));
                if( nSel & SelectionType::Ole )
                {
                    // #i73249#
                    aSet.Put( SfxStringItem( FN_SET_FRM_ALT_NAME, rSh.GetObjTitle() ) );
                }

                const SwRect &rPg = rSh.GetAnyCurRect(CurRectType::Page);
                SwFormatFrameSize aFrameSize(SwFrameSize::Variable, rPg.Width(), rPg.Height());
                aFrameSize.SetWhich(GetPool().GetWhichIDFromSlotID(SID_ATTR_PAGE_SIZE));
                aSet.Put(aFrameSize);

                const SwRect &rPr = rSh.GetAnyCurRect(CurRectType::PagePrt);
                SwFormatFrameSize aPrtSize(SwFrameSize::Variable, rPr.Width(), rPr.Height());
                aPrtSize.SetWhich(GetPool().GetWhichIDFromSlotID(FN_GET_PRINT_AREA));
                aSet.Put(aPrtSize);

                aSet.Put(aMgr.GetAttrSet());
                aSet.SetParent( aMgr.GetAttrSet().GetParent() );

                // On % values initialize size
                SwFormatFrameSize aSize(aSet.Get(RES_FRM_SIZE));
                if (aSize.GetWidthPercent() && aSize.GetWidthPercent() != SwFormatFrameSize::SYNCED)
                    aSize.SetWidth(rSh.GetAnyCurRect(CurRectType::FlyEmbedded).Width());
                if (aSize.GetHeightPercent() && aSize.GetHeightPercent() != SwFormatFrameSize::SYNCED)
                    aSize.SetHeight(rSh.GetAnyCurRect(CurRectType::FlyEmbedded).Height());
                if (aSize != aSet.Get(RES_FRM_SIZE))
                {
                    aSet.Put(aSize);
                }

                // disable vertical positioning for Math Objects anchored 'as char' if baseline alignment is activated
                aSet.Put( SfxBoolItem( FN_MATH_BASELINE_ALIGNMENT,
                        rSh.GetDoc()->getIDocumentSettingAccess().get( DocumentSettingId::MATH_BASELINE_ALIGNMENT ) ) );
                const uno::Reference < embed::XEmbeddedObject > xObj( rSh.GetOleRef() );
                aSet.Put( SfxBoolItem( FN_OLE_IS_MATH, xObj.is() && SotExchange::IsMath( xObj->getClassID() ) ) );

                OUString sDefPage;
                const SfxStringItem* pDlgItem;
                if(pArgs && (pDlgItem = pArgs->GetItemIfSet(FN_FORMAT_FRAME_DLG, false)))
                    sDefPage = pDlgItem->GetValue();

                aSet.Put(SfxFrameItem( SID_DOCFRAME, &GetView().GetViewFrame().GetFrame()));
                FieldUnit eMetric = ::GetDfltMetric(dynamic_cast<SwWebView*>( &GetView()) != nullptr );
                SwModule* mod = SwModule::get();
                mod->PutItem(SfxUInt16Item(SID_ATTR_METRIC, static_cast<sal_uInt16>(eMetric)));
                SwAbstractDialogFactory* pFact = SwAbstractDialogFactory::Create();
                ScopedVclPtr<SfxAbstractTabDialog> pDlg(pFact->CreateFrameTabDialog(
                                                        nSel & SelectionType::Graphic ? u"PictureDialog"_ustr :
                                                        nSel & SelectionType::Ole ? u"ObjectDialog"_ustr:
                                                                                        u"FrameDialog"_ustr,
                                                        GetView().GetViewFrame(),
                                                        GetView().GetFrameWeld(),
                                                        aSet,
                                                        false,
                                                        sDefPage));

                if ( nSlot == FN_DRAW_WRAP_DLG )
                {
                    pDlg->SetCurPageId(u"wrap"_ustr);
                }

                if ( pDlg->Execute() )
                {
                    const SfxItemSet* pOutSet = pDlg->GetOutputItemSet();
                    if(pOutSet)
                    {
                        rReq.Done(*pOutSet);
                        const SfxBoolItem* pRatioItem = nullptr;
                        if(nSel & SelectionType::Ole &&
                            (pRatioItem = pOutSet->GetItemIfSet(FN_KEEP_ASPECT_RATIO)))
                        {
                            SwViewOption aUsrPref( *pVOpt );
                            aUsrPref.SetKeepRatio(pRatioItem->GetValue());
                            mod->ApplyUsrPref(aUsrPref, &GetView());
                        }
                        if (const SfxStringItem* pAltNameItem = pOutSet->GetItemIfSet(FN_SET_FRM_ALT_NAME))
                        {
                            // #i73249#
                            rSh.SetObjTitle(pAltNameItem->GetValue());
                        }
                        if (const SfxStringItem* pDescripItem = pOutSet->GetItemIfSet(FN_UNO_DESCRIPTION))
                            rSh.SetObjDescription(pDescripItem->GetValue());

                        // Template AutoUpdate
                        SwFrameFormat* pFormat = rSh.GetSelectedFrameFormat();
                        if(pFormat && pFormat->IsAutoUpdateOnDirectFormat())
                        {
                            rSh.AutoUpdateFrame(pFormat, *pOutSet);
                            // Anything which is not supported by the format must be set hard.
                            if(const SfxStringItem* pFrameName = pOutSet->GetItemIfSet(FN_SET_FRM_NAME, false))
                                rSh.SetFlyName(UIName(pFrameName->GetValue()));
                            SfxItemSetFixed<
                                    RES_FRM_SIZE, RES_FRM_SIZE,
                                    RES_SURROUND, RES_ANCHOR>  aShellSet( GetPool() );
                            aShellSet.Put(*pOutSet);
                            aMgr.SetAttrSet(aShellSet);
                            if(const SfxStringItem* pFrameName = pOutSet->GetItemIfSet(FN_SET_FRM_NAME, false))
                                rSh.SetFlyName(UIName(pFrameName->GetValue()));
                        }
                        else
                            aMgr.SetAttrSet( *pOutSet );

                        const SwFrameFormat* pCurrFlyFormat = rSh.GetFlyFrameFormat();
                        if(const SfxStringItem* pPreviousItem =
                           pOutSet->GetItemIfSet(FN_PARAM_CHAIN_PREVIOUS, false))
                        {
                            rSh.HideChainMarker();

                            OUString sPrevName = pPreviousItem->GetValue();
                            const SwFormatChain &rChain = pCurrFlyFormat->GetChain();
                            //needs cast - no non-const method available
                            SwFlyFrameFormat* pFlyFormat =
                                rChain.GetPrev();
                            if(pFlyFormat)
                            {
                                if (pFlyFormat->GetName() != sPrevName)
                                {
                                    rSh.Unchain(*pFlyFormat);
                                }
                                else
                                    sPrevName.clear();
                            }

                            if (!sPrevName.isEmpty())
                            {
                                //needs cast - no non-const method available
                                SwFrameFormat* pPrevFormat = rSh.GetDoc()->GetFlyFrameFormatByName(UIName(sPrevName));
                                SAL_WARN_IF(!pPrevFormat, "sw.ui", "No frame found!");
                                if(pPrevFormat)
                                {
                                    rSh.Chain(*pPrevFormat, *pCurrFlyFormat);
                                }
                            }
                            rSh.SetChainMarker();
                        }
                        if(const SfxStringItem* pChainNextItem =
                           pOutSet->GetItemIfSet(FN_PARAM_CHAIN_NEXT, false))
                        {
                            rSh.HideChainMarker();
                            OUString sNextName = pChainNextItem->GetValue();
                            const SwFormatChain &rChain = pCurrFlyFormat->GetChain();
                            //needs cast - no non-const method available
                            SwFlyFrameFormat* pFlyFormat =
                                rChain.GetNext();
                            if(pFlyFormat)
                            {
                                if (pFlyFormat->GetName() != sNextName)
                                {
                                    rSh.Unchain(*const_cast<SwFlyFrameFormat*>(static_cast<const SwFlyFrameFormat*>( pCurrFlyFormat)));
                                }
                                else
                                    sNextName.clear();
                            }

                            if (!sNextName.isEmpty())
                            {
                                //needs cast - no non-const method available
                                SwFrameFormat* pNextFormat = rSh.GetDoc()->GetFlyFrameFormatByName(UIName(sNextName));
                                SAL_WARN_IF(!pNextFormat, "sw.ui", "No frame found!");
                                if(pNextFormat)
                                {
                                    rSh.Chain(*const_cast<SwFrameFormat*>(
                                              pCurrFlyFormat), *pNextFormat);
                                }
                            }
                            rSh.SetChainMarker();
                        }
                    }
                }
                else
                    bUpdateMgr = false;
            }
        }
        break;
        case FN_FRAME_MIRROR_ON_EVEN_PAGES:
        {
            SwFormatHoriOrient aHori(aMgr.GetHoriOrient());
            bool bMirror = !aHori.IsPosToggle();
            aHori.SetPosToggle(bMirror);
            SfxItemSetFixed<RES_HORI_ORIENT, RES_HORI_ORIENT> aSet(GetPool());
            aSet.Put(aHori);
            aMgr.SetAttrSet(aSet);
            bCopyToFormat = true;
            rReq.SetReturnValue(SfxBoolItem(nSlot, bMirror));
        }
        break;
        case FN_NAME_SHAPE:
        {
            bUpdateMgr = false;
            SdrView* pSdrView = rSh.GetDrawViewWithValidMarkList();
            if ( pSdrView &&
                 pSdrView->GetMarkedObjectList().GetMarkCount() == 1 )
            {
                UIName aName(rSh.GetFlyName());
                SvxAbstractDialogFactory* pFact = SvxAbstractDialogFactory::Create();
                VclPtr<AbstractSvxObjectNameDialog> pDlg(
                    pFact->CreateSvxObjectNameDialog(GetView().GetFrameWeld(), aName.toString()));

                pDlg->StartExecuteAsync(
                    [this, pDlg] (sal_Int32 nResult)->void
                    {
                        if (nResult == RET_OK)
                            GetShell().SetFlyName(UIName(pDlg->GetName()));
                        pDlg->disposeOnce();
                    }
                );
            }
        }
        break;
        // #i73249#
        case FN_TITLE_DESCRIPTION_SHAPE:
        {
            bUpdateMgr = false;
            SdrView* pSdrView = rSh.GetDrawViewWithValidMarkList();
            if ( pSdrView &&
                 pSdrView->GetMarkedObjectList().GetMarkCount() == 1 )
            {
                OUString aDescription(rSh.GetObjDescription());
                OUString aTitle(rSh.GetObjTitle());
                bool isDecorative(rSh.IsObjDecorative());

                SvxAbstractDialogFactory* pFact = SvxAbstractDialogFactory::Create();
                VclPtr<AbstractSvxObjectTitleDescDialog> pDlg(
                    pFact->CreateSvxObjectTitleDescDialog(GetView().GetFrameWeld(),
                        aTitle, aDescription, isDecorative));

                pDlg->StartExecuteAsync(
                    [this, pDlg] (sal_Int32 nResult)->void
                    {
                        if (nResult == RET_OK)
                        {
                            GetShell().SetObjDescription(pDlg->GetDescription());
                            GetShell().SetObjTitle(pDlg->GetTitle());
                            GetShell().SetObjDecorative(pDlg->IsDecorative());
                        }
                        pDlg->disposeOnce();
                    }
                );
            }
        }
        break;
        default:
            assert(!"wrong dispatcher");
            return;
    }
    if ( bUpdateMgr )
    {
        SwFrameFormat* pFormat = rSh.GetSelectedFrameFormat();
        if ( bCopyToFormat && pFormat && pFormat->IsAutoUpdateOnDirectFormat() )
        {
            rSh.AutoUpdateFrame(pFormat, aMgr.GetAttrSet());
        }
        else
        {
            aMgr.UpdateFlyFrame();
        }
    }

}

void SwFrameShell::GetState(SfxItemSet& rSet)
{
    SwWrtShell &rSh = GetShell();
    bool bHtmlMode = 0 != ::GetHtmlMode(rSh.GetView().GetDocShell());
    if (!rSh.IsFrameSelected())
        return;

    SfxItemSetFixed<
            RES_LR_SPACE, RES_UL_SPACE,
            RES_PRINT, RES_HORI_ORIENT>  aSet(rSh.GetAttrPool() );
    rSh.GetFlyFrameAttr( aSet );

    bool bProtect = rSh.IsSelObjProtected(FlyProtectFlags::Pos) != FlyProtectFlags::NONE;
    bool bParentCntProt = rSh.IsSelObjProtected( FlyProtectFlags::Content|FlyProtectFlags::Parent ) != FlyProtectFlags::NONE;

    bProtect |= bParentCntProt;

    const FrameTypeFlags eFrameType = rSh.GetFrameType(nullptr,true);
    SwFlyFrameAttrMgr aMgr( false, &rSh, Frmmgr_Type::NONE, nullptr );

    SfxWhichIter aIter( rSet );
    sal_uInt16 nWhich = aIter.FirstWhich();
    while ( nWhich )
    {
        switch ( nWhich )
        {
            case RES_FRM_SIZE:
            {
                const SwFormatFrameSize& aSz(aMgr.GetFrameSize());
                rSet.Put(aSz);
            }
            break;
            case RES_VERT_ORIENT:
            case RES_HORI_ORIENT:
            case SID_ATTR_ULSPACE:
            case SID_ATTR_LRSPACE:
            case RES_LR_SPACE:
            case RES_UL_SPACE:
            case RES_PROTECT:
            case RES_OPAQUE:
            case RES_PRINT:
            case RES_SURROUND:
            {
                rSet.Put(aSet.Get(GetPool().GetWhichIDFromSlotID(nWhich)));
            }
            break;
            case SID_OBJECT_ALIGN:
            {
                if ( bProtect )
                    rSet.DisableItem( nWhich );
            }
            break;
            case SID_OBJECT_ALIGN_LEFT   :
            case SID_OBJECT_ALIGN_CENTER :
            case SID_OBJECT_ALIGN_RIGHT  :
            case FN_FRAME_ALIGN_HORZ_CENTER:
            case FN_FRAME_ALIGN_HORZ_RIGHT:
            case FN_FRAME_ALIGN_HORZ_LEFT:
                if ( (eFrameType & FrameTypeFlags::FLY_INCNT) ||
                     bProtect ||
                     ((nWhich == FN_FRAME_ALIGN_HORZ_CENTER  || nWhich == SID_OBJECT_ALIGN_CENTER) &&
                      bHtmlMode ))
                {
                    rSet.DisableItem( nWhich );
                }
                else
                {
                    sal_Int16 nHoriOrient = -1;
                    switch(nWhich)
                    {
                        case SID_OBJECT_ALIGN_LEFT:
                            nHoriOrient = text::HoriOrientation::LEFT;
                            break;
                        case SID_OBJECT_ALIGN_CENTER:
                            nHoriOrient = text::HoriOrientation::CENTER;
                            break;
                        case SID_OBJECT_ALIGN_RIGHT:
                            nHoriOrient = text::HoriOrientation::RIGHT;
                            break;
                        default:
                            break;
                    }
                    const SwFormatHoriOrient& aHOrient(aMgr.GetHoriOrient());
                    if (nHoriOrient != -1)
                        rSet.Put(SfxBoolItem(nWhich, nHoriOrient == aHOrient.GetHoriOrient()));
                }
            break;
            case FN_FRAME_ALIGN_VERT_ROW_TOP:
            case FN_FRAME_ALIGN_VERT_ROW_CENTER:
            case FN_FRAME_ALIGN_VERT_ROW_BOTTOM:
            case FN_FRAME_ALIGN_VERT_CHAR_TOP:
            case FN_FRAME_ALIGN_VERT_CHAR_CENTER:
            case FN_FRAME_ALIGN_VERT_CHAR_BOTTOM:
                if ( !(eFrameType & FrameTypeFlags::FLY_INCNT) || bProtect
                     || (bHtmlMode && FN_FRAME_ALIGN_VERT_CHAR_BOTTOM == nWhich) )
                    rSet.DisableItem( nWhich );
            break;

            case SID_OBJECT_ALIGN_UP     :
            case SID_OBJECT_ALIGN_MIDDLE :
            case SID_OBJECT_ALIGN_DOWN :

            case FN_FRAME_ALIGN_VERT_TOP:
            case FN_FRAME_ALIGN_VERT_CENTER:
            case FN_FRAME_ALIGN_VERT_BOTTOM:
                if ( bProtect || (bHtmlMode && eFrameType & FrameTypeFlags::FLY_ATCNT))
                    rSet.DisableItem( nWhich );
                else
                {
                    // These slots need different labels depending on whether they are anchored in a character
                    // or on a paragraph/page etc.
                    OUString sNewLabel;
                    if (eFrameType & FrameTypeFlags::FLY_INCNT)
                    {
                        switch (nWhich)
                        {
                            case SID_OBJECT_ALIGN_UP     :
                            case FN_FRAME_ALIGN_VERT_TOP:
                                sNewLabel = SwResId(STR_FRMUI_TOP_BASE);
                                break;
                            case SID_OBJECT_ALIGN_MIDDLE :
                            case FN_FRAME_ALIGN_VERT_CENTER:
                                sNewLabel = SwResId(STR_FRMUI_CENTER_BASE);
                                break;
                            case SID_OBJECT_ALIGN_DOWN :
                            case FN_FRAME_ALIGN_VERT_BOTTOM:
                                if(!bHtmlMode)
                                    sNewLabel = SwResId(STR_FRMUI_BOTTOM_BASE);
                                else
                                    rSet.DisableItem( nWhich );
                            break;
                        }
                    }
                    else
                    {
                        if (nWhich != FN_FRAME_ALIGN_VERT_TOP &&
                                nWhich != SID_OBJECT_ALIGN_UP )
                        {
                            if (aMgr.GetAnchor() == RndStdIds::FLY_AT_FLY)
                            {
                                const SwFrameFormat* pFormat = rSh.IsFlyInFly();
                                if (pFormat)
                                {
                                    const SwFormatFrameSize& rFrameSz = pFormat->GetFrameSize();
                                    if (rFrameSz.GetHeightSizeType() != SwFrameSize::Fixed)
                                    {
                                        rSet.DisableItem( nWhich );
                                        break;
                                    }
                                }
                            }
                        }
                        OUString aModuleName;
                        if (SfxViewFrame* pFrame = GetFrame())
                            aModuleName = vcl::CommandInfoProvider::GetModuleIdentifier(pFrame->GetFrame().GetFrameInterface());
                        switch (nWhich)
                        {
                            case SID_OBJECT_ALIGN_UP :
                            case FN_FRAME_ALIGN_VERT_TOP:
                            {
                                auto aProperties = vcl::CommandInfoProvider::GetCommandProperties(u".uno:AlignTop"_ustr, aModuleName);
                                sNewLabel = vcl::CommandInfoProvider::GetLabelForCommand(aProperties);
                                break;
                            }
                            case SID_OBJECT_ALIGN_MIDDLE:
                            case FN_FRAME_ALIGN_VERT_CENTER:
                            {
                                auto aProperties = vcl::CommandInfoProvider::GetCommandProperties(u".uno:AlignVerticalCenter"_ustr, aModuleName);
                                sNewLabel = vcl::CommandInfoProvider::GetLabelForCommand(aProperties);
                                break;
                            }
                            case SID_OBJECT_ALIGN_DOWN:
                            case FN_FRAME_ALIGN_VERT_BOTTOM:
                            {
                                auto aProperties = vcl::CommandInfoProvider::GetCommandProperties(u".uno:AlignBottom"_ustr, aModuleName);
                                sNewLabel = vcl::CommandInfoProvider::GetLabelForCommand(aProperties);
                                break;
                            }
                        }
                    }
                    if ( !sNewLabel.isEmpty() )
                        rSet.Put( SfxStringItem( nWhich, sNewLabel ));
                }
            break;
            case SID_HYPERLINK_GETLINK:
            {
                SvxHyperlinkItem aHLinkItem;

                SfxItemSetFixed<RES_URL, RES_URL> aURLSet(GetPool());
                rSh.GetFlyFrameAttr( aURLSet );

                if(const SwFormatURL* pFormatURL = aURLSet.GetItemIfSet(RES_URL))
                {
                    aHLinkItem.SetURL(pFormatURL->GetURL());
                    aHLinkItem.SetTargetFrame(pFormatURL->GetTargetFrameName());
                    aHLinkItem.SetName(rSh.GetFlyName().toString());
                }

                aHLinkItem.SetInsertMode(static_cast<SvxLinkInsertMode>(aHLinkItem.GetInsertMode() |
                    (bHtmlMode ? HLINK_HTMLMODE : 0)));

                rSet.Put(aHLinkItem);
            }
            break;

            case FN_FRAME_CHAIN:
            {
                const SelectionType nSel = rSh.GetSelectionType();
                if (nSel & SelectionType::Graphic || nSel & SelectionType::Ole)
                    rSet.DisableItem( FN_FRAME_CHAIN );
                else
                {
                    const SwFrameFormat *pFormat = rSh.GetFlyFrameFormat();
                    if ( bParentCntProt || rSh.GetView().GetEditWin().GetApplyTemplate() ||
                         !pFormat || pFormat->GetChain().GetNext() )
                    {
                        rSet.DisableItem( FN_FRAME_CHAIN );
                    }
                    else
                    {
                        bool bChainMode = rSh.GetView().GetEditWin().IsChainMode();
                        rSet.Put( SfxBoolItem( FN_FRAME_CHAIN, bChainMode ) );
                    }
                }
            }
            break;
            case FN_FRAME_UNCHAIN:
            {
                const SelectionType nSel = rSh.GetSelectionType();
                if (nSel & SelectionType::Graphic || nSel & SelectionType::Ole)
                    rSet.DisableItem( FN_FRAME_UNCHAIN );
                else
                {
                    const SwFrameFormat *pFormat = rSh.GetFlyFrameFormat();
                    if ( bParentCntProt || rSh.GetView().GetEditWin().GetApplyTemplate() ||
                         !pFormat || !pFormat->GetChain().GetNext() )
                    {
                        rSet.DisableItem( FN_FRAME_UNCHAIN );
                    }
                }
            }
            break;
            case SID_FRAME_TO_TOP:
            case SID_FRAME_TO_BOTTOM:
            case FN_FRAME_UP:
            case FN_FRAME_DOWN:
                if ( bParentCntProt )
                    rSet.DisableItem( nWhich );
            break;

            case SID_ATTR_TRANSFORM:
            {
                rSet.DisableItem( nWhich );
            }
            break;

            case SID_ATTR_TRANSFORM_PROTECT_SIZE:
            {
                const FlyProtectFlags eProtection = rSh.IsSelObjProtected( FlyProtectFlags::Size );
                if ( ( eProtection & FlyProtectFlags::Content ) ||
                     ( eProtection & FlyProtectFlags::Size ) )
                {
                    rSet.Put( SfxBoolItem( SID_ATTR_TRANSFORM_PROTECT_SIZE, true ) );
                }
                else
                {
                    rSet.Put( SfxBoolItem( SID_ATTR_TRANSFORM_PROTECT_SIZE, false ) );
                }
            }
            break;

            case SID_ATTR_TRANSFORM_WIDTH:
            {
                rSet.Put( SfxUInt32Item( SID_ATTR_TRANSFORM_WIDTH, aMgr.GetSize().getWidth() ) );
            }
            break;

            case SID_ATTR_TRANSFORM_HEIGHT:
            {
                rSet.Put( SfxUInt32Item( SID_ATTR_TRANSFORM_HEIGHT, aMgr.GetSize().getHeight() ) );
            }
            break;

            case FN_FORMAT_FRAME_DLG:
            {
                const SelectionType nSel = rSh.GetSelectionType();
                if ( bParentCntProt || nSel & SelectionType::Graphic)
                    rSet.DisableItem( nWhich );
            }
            break;
            // #i73249#
            case FN_TITLE_DESCRIPTION_SHAPE:
            case FN_NAME_SHAPE:
            {
                SwWrtShell &rWrtSh = GetShell();
                SdrView* pSdrView = rWrtSh.GetDrawViewWithValidMarkList();
                if ( !pSdrView ||
                     pSdrView->GetMarkedObjectList().GetMarkCount() != 1 )
                {
                    rSet.DisableItem( nWhich );
                }
            }
            break;

            case FN_POSTIT:
            {
                SwFlyFrame* pFly = rSh.GetSelectedFlyFrame();
                if (pFly)
                {
                    SwFrameFormat* pFormat = pFly->GetFormat();
                    if (pFormat)
                    {
                        RndStdIds eAnchorId = pFormat->GetAnchor().GetAnchorId();
                        // SwWrtShell::InsertPostIt() only works on as-char and at-char anchored
                        // images.
                        if (eAnchorId != RndStdIds::FLY_AS_CHAR && eAnchorId != RndStdIds::FLY_AT_CHAR)
                        {
                            rSet.DisableItem(nWhich);
                        }
                    }
                }
            }
            break;

            default:
                /* do nothing */;
                break;
        }
        nWhich = aIter.NextWhich();
    }
}

SwFrameShell::SwFrameShell(SwView &_rView) :
    SwBaseShell( _rView )
{
    SetName(u"Frame"_ustr);

    // #96392# Use this to announce it is the frame shell who creates the selection.
    SwTransferable::CreateSelection( _rView.GetWrtShell(), this );

    SfxShell::SetContextName(vcl::EnumContext::GetContextName(vcl::EnumContext::Context::Frame));
}

SwFrameShell::~SwFrameShell()
{
    // #96392# Only clear the selection if it was this frame shell who created it.
    SwTransferable::ClearSelection( GetShell(), this );
}

void SwFrameShell::ExecFrameStyle(SfxRequest const & rReq)
{
    SwWrtShell &rSh = GetShell();
    bool bDefault = false;
    if (!rSh.IsFrameSelected())
        return;
    // At first pick the default BoxItem out of the pool.
    // If unequal to regular box item, then it has already
    // been changed (New one is no default).
    const SvxBoxItem* pPoolBoxItem = ::GetDfltAttr(RES_BOX);

    const SfxItemSet *pArgs = rReq.GetArgs();
    SfxItemSetFixed<RES_BOX, RES_BOX> aFrameSet(rSh.GetAttrPool());

    rSh.GetFlyFrameAttr( aFrameSet );
    const SvxBoxItem& rBoxItem = aFrameSet.Get(RES_BOX);

    if (SfxPoolItem::areSame(pPoolBoxItem, &rBoxItem))
        bDefault = true;

    std::unique_ptr<SvxBoxItem> aBoxItem(rBoxItem.Clone());

    SvxBorderLine aBorderLine;

    if(pArgs)    // Any controller can sometimes deliver nothing #48169#
    {
        switch (rReq.GetSlot())
        {
            case SID_ATTR_BORDER:
            {
                if (const SvxBoxItem* pBoxItem = pArgs->GetItemIfSet(RES_BOX))
                {
                    std::unique_ptr<SvxBoxItem> aNewBox(pBoxItem->Clone());
                    const SvxBorderLine* pBorderLine;

                    pBorderLine = aBoxItem->GetTop();
                    if (pBorderLine != nullptr)
                        lcl_FrameGetMaxLineWidth(pBorderLine, aBorderLine);
                    pBorderLine = aBoxItem->GetBottom();
                    if (pBorderLine != nullptr)
                        lcl_FrameGetMaxLineWidth(pBorderLine, aBorderLine);
                    pBorderLine = aBoxItem->GetLeft();
                    if (pBorderLine != nullptr)
                        lcl_FrameGetMaxLineWidth(pBorderLine, aBorderLine);
                    pBorderLine = aBoxItem->GetRight();
                    if (pBorderLine != nullptr)
                        lcl_FrameGetMaxLineWidth(pBorderLine, aBorderLine);

                    if(aBorderLine.GetOutWidth() == 0)
                    {
                        aBorderLine.SetBorderLineStyle(
                                SvxBorderLineStyle::SOLID);
                        aBorderLine.SetWidth( SvxBorderLineWidth::Hairline );
                    }
                    //Set distance only if the request is received from the controller.

#if HAVE_FEATURE_SCRIPTING
                    if(!StarBASIC::IsRunning())
#endif
                    {
                        // TODO: should this copy 4 individual Dist instead?
                        aNewBox->SetAllDistances(rBoxItem.GetSmallestDistance());
                    }

                    aBoxItem = std::move(aNewBox);

                    if( aBoxItem->GetTop() != nullptr )
                        aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::TOP);
                    if( aBoxItem->GetBottom() != nullptr )
                        aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::BOTTOM);
                    if( aBoxItem->GetLeft() != nullptr )
                        aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::LEFT);
                    if( aBoxItem->GetRight() != nullptr )
                        aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::RIGHT);
                }
            }
            break;

            case SID_FRAME_LINESTYLE:
            {
                if ( const SvxLineItem* pLineItem = pArgs->GetItemIfSet(SID_FRAME_LINESTYLE, false))
                {
                    if ( pLineItem->GetLine() )
                    {
                        aBorderLine = *(pLineItem->GetLine());

                        if (!aBoxItem->GetTop() && !aBoxItem->GetBottom() &&
                            !aBoxItem->GetLeft() && !aBoxItem->GetRight())
                        {
                            aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::TOP);
                            aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::BOTTOM);
                            aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::LEFT);
                            aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::RIGHT);
                        }
                        else
                        {
                            if( aBoxItem->GetTop() )
                            {
                                aBorderLine.SetColor( aBoxItem->GetTop()->GetColor() );
                                aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::TOP);
                            }
                            if( aBoxItem->GetBottom() )
                            {
                                aBorderLine.SetColor( aBoxItem->GetBottom()->GetColor());
                                aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::BOTTOM);
                            }
                            if( aBoxItem->GetLeft() )
                            {
                                aBorderLine.SetColor( aBoxItem->GetLeft()->GetColor());
                                aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::LEFT);
                            }
                            if( aBoxItem->GetRight() )
                            {
                                aBorderLine.SetColor(aBoxItem->GetRight()->GetColor());
                                aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::RIGHT);
                            }
                        }
                    }
                    else
                    {
                        aBoxItem->SetLine(nullptr, SvxBoxItemLine::TOP);
                        aBoxItem->SetLine(nullptr, SvxBoxItemLine::BOTTOM);
                        aBoxItem->SetLine(nullptr, SvxBoxItemLine::LEFT);
                        aBoxItem->SetLine(nullptr, SvxBoxItemLine::RIGHT);
                    }
                }
            }
            break;

            case SID_FRAME_LINECOLOR:
            {
                if (const SvxColorItem* pColorItem = pArgs->GetItemIfSet(SID_FRAME_LINECOLOR, false))
                {
                    const Color& rNewColor = pColorItem->GetValue();

                    if (!aBoxItem->GetTop() && !aBoxItem->GetBottom() &&
                        !aBoxItem->GetLeft() && !aBoxItem->GetRight())
                    {
                        aBorderLine.SetColor( rNewColor );
                        aBorderLine.SetBorderLineStyle(SvxBorderLineStyle::SOLID);
                        aBorderLine.SetWidth(SvxBorderLineWidth::Hairline);

                        aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::TOP);
                        aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::BOTTOM);
                        aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::LEFT);
                        aBoxItem->SetLine(&aBorderLine, SvxBoxItemLine::RIGHT);
                    }
                    else
                    {
                        if (aBoxItem->GetTop())
                            aBoxItem->GetTop()->SetColor(rNewColor);
                        if (aBoxItem->GetBottom())
                            aBoxItem->GetBottom()->SetColor(rNewColor);
                        if (aBoxItem->GetLeft())
                            aBoxItem->GetLeft()->SetColor(rNewColor);
                        if (aBoxItem->GetRight())
                            aBoxItem->GetRight()->SetColor(rNewColor);
                    }
                }
            }
            break;
        }
    }
    if (bDefault && (aBoxItem->GetTop() || aBoxItem->GetBottom() ||
        aBoxItem->GetLeft() || aBoxItem->GetRight()))
    {
        aBoxItem->SetAllDistances(MIN_BORDER_DIST);
    }
    aFrameSet.Put( std::move(aBoxItem) );
    // Template AutoUpdate
    SwFrameFormat* pFormat = rSh.GetSelectedFrameFormat();
    if(pFormat && pFormat->IsAutoUpdateOnDirectFormat())
    {
        rSh.AutoUpdateFrame(pFormat, aFrameSet);
    }
    else
        rSh.SetFlyFrameAttr( aFrameSet );

}

static void lcl_FrameGetMaxLineWidth(const SvxBorderLine* pBorderLine, SvxBorderLine& rBorderLine)
{
    if(pBorderLine->GetWidth() > rBorderLine.GetWidth())
        rBorderLine.SetWidth(pBorderLine->GetWidth());

    rBorderLine.SetBorderLineStyle(pBorderLine->GetBorderLineStyle());
    rBorderLine.SetColor(pBorderLine->GetColor());
}

void SwFrameShell::GetLineStyleState(SfxItemSet &rSet)
{
    SwWrtShell &rSh = GetShell();
    bool bParentCntProt = rSh.IsSelObjProtected( FlyProtectFlags::Content|FlyProtectFlags::Parent ) != FlyProtectFlags::NONE;

    if (bParentCntProt)
    {
        if (rSh.IsFrameSelected())
            rSet.DisableItem( SID_FRAME_LINECOLOR );

        rSet.DisableItem( SID_ATTR_BORDER );
        rSet.DisableItem( SID_FRAME_LINESTYLE );
    }
    else
    {
        if (rSh.IsFrameSelected())
        {
            SfxItemSetFixed<RES_BOX, RES_BOX> aFrameSet( rSh.GetAttrPool() );

            rSh.GetFlyFrameAttr(aFrameSet);

            const SvxBorderLine* pLine = aFrameSet.Get(RES_BOX).GetTop();
            rSet.Put(SvxColorItem(pLine ? pLine->GetColor() : Color(), SID_FRAME_LINECOLOR));
        }
    }
}

void  SwFrameShell::StateInsert(SfxItemSet &rSet)
{
    const SelectionType nSel = GetShell().GetSelectionType();
    if ( (nSel & SelectionType::Graphic)
        || (nSel & SelectionType::Ole) )
    {
        rSet.DisableItem(FN_INSERT_FRAME);
    }
    else if ( GetShell().CursorInsideInputField() )
    {
        rSet.DisableItem(FN_INSERT_FRAME);
    }
}

void SwFrameShell::GetDrawAttrStateTextFrame(SfxItemSet &rSet)
{
    SwWrtShell &rSh = GetShell();

    if(rSh.IsFrameSelected())
    {
        rSh.GetFlyFrameAttr(rSet);
    }
    else
    {
        SdrView* pSdrView = rSh.GetDrawViewWithValidMarkList();

        if(pSdrView)
        {
            rSet.Put(pSdrView->GetDefaultAttr());
        }
    }
}

void SwFrameShell::ExecDrawAttrArgsTextFrame(SfxRequest const & rReq)
{
    const SfxItemSet* pArgs = rReq.GetArgs();
    SwWrtShell& rSh = GetShell();

    if(pArgs)
    {
        if(rSh.IsFrameSelected())
        {
            rSh.SetFlyFrameAttr(const_cast< SfxItemSet& >(*pArgs));
        }
        else
        {
            SdrView* pSdrView = rSh.GetDrawViewWithValidMarkList();

            if(pSdrView)
            {
                pSdrView->SetDefaultAttr(*pArgs, false);
            }
        }
    }
    else
    {
        SfxDispatcher* pDis = rSh.GetView().GetViewFrame().GetDispatcher();

        switch(rReq.GetSlot())
        {
            case SID_ATTR_FILL_STYLE:
            case SID_ATTR_FILL_COLOR:
            case SID_ATTR_FILL_GRADIENT:
            case SID_ATTR_FILL_HATCH:
            case SID_ATTR_FILL_BITMAP:
            case SID_ATTR_FILL_TRANSPARENCE:
            case SID_ATTR_FILL_FLOATTRANSPARENCE:
            {
                pDis->Execute(SID_ATTRIBUTES_AREA);
                break;
            }
        }
    }
}

void SwFrameShell::ExecDrawDlgTextFrame(SfxRequest const & rReq)
{
    switch(rReq.GetSlot())
    {
        case SID_ATTRIBUTES_AREA:
        {
            SwWrtShell& rSh = GetShell();

            if(rSh.IsFrameSelected())
            {
                SdrModel& rModel = rSh.GetDrawView()->GetModel();
                SfxItemSet aNewAttr(rModel.GetItemPool());

                // get attributes from FlyFrame
                rSh.GetFlyFrameAttr(aNewAttr);

                SvxAbstractDialogFactory* pFact = SvxAbstractDialogFactory::Create();
                VclPtr<AbstractSvxAreaTabDialog> pDlg(pFact->CreateSvxAreaTabDialog(
                    GetView().GetFrameWeld(),
                    &aNewAttr,
                    &rModel,
                    false,
                    false));

                pDlg->StartExecuteAsync([pDlg, this](sal_Int32 nResult){
                    if(nResult == RET_OK)
                    {
                        // set attributes at FlyFrame
                        GetShell().SetFlyFrameAttr(const_cast< SfxItemSet& >(*pDlg->GetOutputItemSet()));

                        static const sal_uInt16 aInval[] =
                        {
                            SID_ATTR_FILL_STYLE,
                            SID_ATTR_FILL_COLOR,
                            SID_ATTR_FILL_TRANSPARENCE,
                            SID_ATTR_FILL_FLOATTRANSPARENCE,
                            0
                        };

                        SfxBindings &rBnd = GetView().GetViewFrame().GetBindings();

                        rBnd.Invalidate(aInval);
                        rBnd.Update(SID_ATTR_FILL_STYLE);
                        rBnd.Update(SID_ATTR_FILL_COLOR);
                        rBnd.Update(SID_ATTR_FILL_TRANSPARENCE);
                        rBnd.Update(SID_ATTR_FILL_FLOATTRANSPARENCE);
                    }
                    pDlg->disposeOnce();
                });
            }

            break;
        }
    }
}

void SwFrameShell::DisableStateTextFrame(SfxItemSet &rSet)
{
    SfxWhichIter aIter(rSet);
    sal_uInt16 nWhich(aIter.FirstWhich());

    while(nWhich)
    {
        switch(nWhich)
        {
            case SID_ATTRIBUTES_AREA:
            {
                SwWrtShell& rSh = GetShell();

                if(!rSh.IsFrameSelected())
                {
                    rSet.DisableItem(nWhich);
                }

                break;
            }
            default:
            {
                rSet.DisableItem(nWhich);
                break;
            }
        }

        nWhich = aIter.NextWhich();
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
