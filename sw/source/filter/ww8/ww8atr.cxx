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

/*
 * This file contains methods for the WW8 output
 * (nodes, attributes, formats and chars).
 */


#include <algorithm>
#include <hintids.hxx>

#include <o3tl/safeint.hxx>
#include <vcl/svapp.hxx>
#include <vcl/settings.hxx>
#include <sal/log.hxx>

#include <svl/numformat.hxx>
#include <svl/zformat.hxx>
#include <svl/itemiter.hxx>
#include <svl/whiter.hxx>
#include <svl/grabbagitem.hxx>
#include <editeng/fontitem.hxx>
#include <editeng/tstpitem.hxx>
#include <editeng/adjustitem.hxx>
#include <editeng/spltitem.hxx>
#include <editeng/widwitem.hxx>
#include <editeng/lspcitem.hxx>
#include <editeng/keepitem.hxx>
#include <editeng/shaditem.hxx>
#include <editeng/brushitem.hxx>
#include <editeng/postitem.hxx>
#include <editeng/wghtitem.hxx>
#include <editeng/kernitem.hxx>
#include <editeng/crossedoutitem.hxx>
#include <editeng/cmapitem.hxx>
#include <editeng/wrlmitem.hxx>
#include <editeng/udlnitem.hxx>
#include <editeng/langitem.hxx>
#include <editeng/escapementitem.hxx>
#include <editeng/fhgtitem.hxx>
#include <editeng/colritem.hxx>
#include <editeng/hyphenzoneitem.hxx>
#include <editeng/formatbreakitem.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/ulspitem.hxx>
#include <editeng/boxitem.hxx>
#include <editeng/contouritem.hxx>
#include <editeng/shdditem.hxx>
#include <editeng/autokernitem.hxx>
#include <editeng/pbinitem.hxx>
#include <editeng/emphasismarkitem.hxx>
#include <editeng/twolinesitem.hxx>
#include <editeng/charscaleitem.hxx>
#include <editeng/charrotateitem.hxx>
#include <editeng/charreliefitem.hxx>
#include <editeng/paravertalignitem.hxx>
#include <editeng/pgrditem.hxx>
#include <editeng/frmdiritem.hxx>
#include <editeng/blinkitem.hxx>
#include <editeng/charhiddenitem.hxx>
#include <editeng/paperinf.hxx>
#include <editeng/scripthintitem.hxx>
#include <svx/xfillit0.hxx>
#include <svx/xflgrit.hxx>
#include <o3tl/string_view.hxx>
#include <fmtfld.hxx>
#include <fchrfmt.hxx>
#include <fmtfsize.hxx>
#include <fmtpdsc.hxx>
#include <fmtornt.hxx>
#include <fmtanchr.hxx>
#include <fmtclds.hxx>
#include <fmtsrnd.hxx>
#include <fmtftn.hxx>
#include <fmtflcnt.hxx>
#include <frmatr.hxx>
#include <swtable.hxx>
#include <fmtinfmt.hxx>
#include <txtftn.hxx>
#include <poolfmt.hxx>
#include <doc.hxx>
#include <IDocumentSettingAccess.hxx>
#include <IDocumentFieldsAccess.hxx>
#include <IDocumentStylePoolAccess.hxx>
#include <IDocumentListsAccess.hxx>
#include <list.hxx>
#include <docary.hxx>
#include <pam.hxx>
#include <paratr.hxx>
#include <fldbas.hxx>
#include <docufld.hxx>
#include <expfld.hxx>
#include <pagedesc.hxx>
#include <ndtxt.hxx>
#include <swrect.hxx>
#include <redline.hxx>
#include <reffld.hxx>
#include <ftninfo.hxx>
#include <charfmt.hxx>
#include <section.hxx>
#include <fmtline.hxx>
#include <tox.hxx>
#include <fmtftntx.hxx>
#include <breakit.hxx>
#include <com/sun/star/i18n/ScriptType.hpp>
#include <com/sun/star/i18n/XBreakIterator.hpp>
#include <unotools/localedatawrapper.hxx>
#include <svx/unobrushitemhelper.hxx>
#include <tgrditem.hxx>
#include <flddropdown.hxx>
#include <chpfld.hxx>
#include <fmthdft.hxx>
#include <authfld.hxx>
#include <dbfld.hxx>
#include <docsh.hxx>
#include <flddat.hxx>

#include "sprmids.hxx"

#include <fmtcntnt.hxx>
#include "writerhelper.hxx"
#include "writerwordglue.hxx"
#include "wrtww8.hxx"
#include "ww8par.hxx"
#include "ww8attributeoutput.hxx"
#include "fields.hxx"
#include <i18nlangtag/mslangid.hxx>
#include <i18nlangtag/languagetag.hxx>
#include <officecfg/Office/Common.hxx>


using ::editeng::SvxBorderLine;
using namespace ::com::sun::star;
using namespace sw::util;
using namespace sw::types;

bool WW8Export::CollapseScriptsforWordOk( sal_uInt16 nScript, sal_uInt16 nWhich )
{
    bool bRet = true;
    if ( nScript == i18n::ScriptType::ASIAN )
    {
        //for asian in ww8, there is only one fontsize
        //and one fontstyle (posture/weight) for ww6
        //there is the additional problem that there
        //is only one font setting for all three scripts
        switch ( nWhich )
        {
            case RES_CHRATR_FONTSIZE:
            case RES_CHRATR_POSTURE:
            case RES_CHRATR_WEIGHT:
                bRet = false;
                break;
            case RES_CHRATR_LANGUAGE:
            case RES_CHRATR_CTL_FONT:
            case RES_CHRATR_CTL_FONTSIZE:
            case RES_CHRATR_CTL_LANGUAGE:
            case RES_CHRATR_CTL_POSTURE:
            case RES_CHRATR_CTL_WEIGHT:
            default:
                break;
        }
    }
    else if ( nScript == i18n::ScriptType::COMPLEX )
    {
        //Complex is ok in ww8, but for ww6 there is only
        //one font, one fontsize, one fontsize (weight/posture)
        //and only one language
    }
    else
    {
        //for western in ww8, there is only one fontsize
        //and one fontstyle (posture/weight) for ww6
        //there is the additional problem that there
        //is only one font setting for all three scripts
        switch ( nWhich )
        {
            case RES_CHRATR_CJK_FONTSIZE:
            case RES_CHRATR_CJK_POSTURE:
            case RES_CHRATR_CJK_WEIGHT:
                bRet = false;
                break;
            case RES_CHRATR_CJK_LANGUAGE:
            case RES_CHRATR_CTL_FONT:
            case RES_CHRATR_CTL_FONTSIZE:
            case RES_CHRATR_CTL_LANGUAGE:
            case RES_CHRATR_CTL_POSTURE:
            case RES_CHRATR_CTL_WEIGHT:
            default:
                break;
        }
    }
    return bRet;
}


void MSWordExportBase::ExportPoolItemsToCHP( ww8::PoolItems &rItems, sal_uInt16 nScript, const SvxFontItem *pFont, bool bWriteCombChars )
{
    for ( const auto& rItem : rItems )
    {
        const SfxPoolItem *pItem = rItem.second;
        sal_uInt16 nWhich = pItem->Which();
        if ( ( isCHRATR( nWhich ) || isTXTATR( nWhich ) ) && CollapseScriptsforWordOk( nScript, nWhich ) )
        {
             //In the id definition, RES_TXTATR_INETFMT must precede RES_TXTATR_CHARFMT, so that link style can overwrite char style.
             //and in #i24291# it describes "All we want to do is ensure for now is that if a charfmt exist in the character
             //properties that it rises to the top and is exported first."
             //In bug 119649, it is in such situation, so we need to ignore the link style when doing ms word filter exports and
             //add the second judgement for #i24291# definition.
             if (nWhich == RES_TXTATR_CHARFMT)
             {
                 const SfxPoolItem* pINetItem = SearchPoolItems(rItems, RES_TXTATR_INETFMT);

                 if (pINetItem)
                 {
                     const SwFormatINetFormat& rINet = pINetItem->StaticWhichCast(RES_TXTATR_INETFMT);
                     const SwCharFormat* pINetFormat = GetSwCharFormat(rINet, m_rDoc);
                     if (!pINetFormat)
                         continue;

                     const SwCharFormat* pFormat = pItem->StaticWhichCast(RES_TXTATR_CHARFMT).GetCharFormat();
                     ww8::PoolItems aCharItems, aINetItems;
                     GetPoolItems(pFormat->GetAttrSet(), aCharItems, false);
                     GetPoolItems(pINetFormat->GetAttrSet(), aINetItems, false);
                     for (const auto& rCharItem : aCharItems)
                     {
                         const SfxPoolItem* pCharItem = rCharItem.second;
                         sal_uInt16 nCharWhich = pCharItem->Which();
                         if (!SearchPoolItems(aINetItems, nCharWhich) && !SearchPoolItems(rItems, nCharWhich))
                             AttrOutput().OutputItem(*pCharItem);
                     }
                     continue;
                 }
             }

             // tdf#38778 Fix output of the font in DOC run for fields
             if (pFont &&
                 nWhich == RES_TXTATR_FIELD)
             {
                 AttrOutput().OutputItem( *pFont );
             }

             // tdf#66401 For Combined Characters in docx, MS Word uses half the normal font-size for the field's
             // font-size, but only for <w:sz>. Therefore, we check if we are currently writing a field of type
             // Combined Characters and if so, we half the font size.
             if (bWriteCombChars &&
                 nWhich == RES_CHRATR_FONTSIZE)
             {
                SvxFontHeightItem fontHeight(item_cast<SvxFontHeightItem>( *pItem ));
                fontHeight.SetHeight( fontHeight.GetHeight() / 2 );

                AttrOutput().OutputItem( fontHeight );
             }
             else if (nWhich == RES_CHRATR_COLOR)
             {
                const SvxColorItem& rColor = pItem->StaticWhichCast(RES_CHRATR_COLOR);
                const SfxPoolItem* pBackgroundItem = SearchPoolItems(rItems, RES_CHRATR_BACKGROUND);
                if (rColor.GetValue() == COL_AUTO && pBackgroundItem)
                {
                    const SvxBrushItem& rBrushBackground = pBackgroundItem->StaticWhichCast(RES_CHRATR_BACKGROUND);
                    SvxColorItem aForeground(rBrushBackground.GetColor().IsDark() ? COL_WHITE : COL_BLACK, RES_CHRATR_COLOR);
                    AttrOutput().OutputItem(aForeground);
                }
                else
                {
                    // default
                    AttrOutput().OutputItem( *pItem );
                }
             }
             else if (nWhich == RES_CHRATR_HIGHLIGHT)
             {
                const SvxBrushItem& rBrush = pItem->StaticWhichCast(RES_CHRATR_HIGHLIGHT);
                // The UI easily adds unnecessary highlights, so identify and avoid exporting those.
                // Highlight is not valid in character styles, so must not check there.
                // Check the (para) style hierarchy to find the nearest defined highlight.
                const SfxPoolItem* pInherited = nullptr;
                if ( auto pNd = dynamic_cast< const SwContentNode *>( m_pOutFormatNode ) ) //paragraph
                    pInherited = static_cast<SwTextFormatColl&>( pNd->GetAnyFormatColl() ).GetAttrSet().GetItem(nWhich);
                else if ( m_bStyDef && m_pCurrentStyle && m_pCurrentStyle->DerivedFrom() ) //style
                    pInherited = &m_pCurrentStyle->DerivedFrom()->GetFormatAttr(nWhich);

                // Ignore highlight if style already sets the same one.
                // Also ignore a transparent highlight if there is no inherited highlight to cancel
                if ( (pInherited && *pInherited != *pItem) || (!pInherited && rBrush.GetColor() != COL_TRANSPARENT) )
                     AttrOutput().OutputItem( *pItem );
             }
             else
             {
                AttrOutput().OutputItem( *pItem );
             }
        }
    }
}

/*
 * Output format as follows:
 *      - output the attributes; without parents!
 */

void MSWordExportBase::OutputItemSet( const SfxItemSet& rSet, bool bPapFormat, bool bChpFormat, sal_uInt16 nScript,
                                      bool bExportParentItemSet )
{
    if( !(bExportParentItemSet || rSet.Count()) )
        return;

    m_pISet = &rSet;                  // for double attributes

    // If frame dir is set, but not adjust, then force adjust as well
    if ( bPapFormat && SfxItemState::SET == rSet.GetItemState( RES_FRAMEDIR, bExportParentItemSet ) )
    {
        // No explicit adjust set ?
        const SvxAdjustItem* pItem = nullptr;
        if ( SfxItemState::SET != rSet.GetItemState( RES_PARATR_ADJUST, bExportParentItemSet, &pItem ) )
        {
            if ( nullptr != pItem )
            {
                // then set the adjust used by the parent format
                AttrOutput().OutputItem( *pItem );
            }
        }
    }

    const SwNumRuleItem* pRuleItem;
    if ( bPapFormat && (pRuleItem = rSet.GetItemIfSet( RES_PARATR_NUMRULE, bExportParentItemSet )) )
    {
        AttrOutput().OutputItem( *pRuleItem );

        // switch off the numbering?
        if ( pRuleItem->GetValue().isEmpty() &&
             SfxItemState::SET != rSet.GetItemState(RES_MARGIN_FIRSTLINE, false))
        {
            if (auto pLRItem = rSet.GetItemIfSet(RES_MARGIN_FIRSTLINE))
                // set the LR-Space of the parentformat!
                AttrOutput().OutputItem( *pLRItem );
        }
        if ( pRuleItem->GetValue().isEmpty() &&
             SfxItemState::SET != rSet.GetItemState(RES_MARGIN_TEXTLEFT, false))
        {
             if (auto pLRItem = rSet.GetItemIfSet(RES_MARGIN_TEXTLEFT))
                // set the LR-Space of the parentformat!
                AttrOutput().OutputItem( *pLRItem );
        }
    }

    ww8::PoolItems aItems;
    GetPoolItems( rSet, aItems, bExportParentItemSet );
    if ( bChpFormat )
        ExportPoolItemsToCHP(aItems, nScript, nullptr);
    if ( bPapFormat )
    {
        const bool bAlreadyOutputBrushItem = AttrOutput().MaybeOutputBrushItem(rSet);

        // ITEM: if we have no XATTR_FILLSTYLE also exclude XATTR_FILLGRADIENT to
        // avoid errors (and to not assert there)
        if (!aItems.contains(XATTR_FILLSTYLE))
            aItems.erase(XATTR_FILLGRADIENT);

        // ITEM: here XATTR_FILLSTYLE will be executed before XATTR_FILLGRADIENT due to
        // ww8::PoolItems *is* sorted ascending due to using ItemSort::operator()
        for ( const auto& rItem : aItems )
        {
            const SfxPoolItem* pItem = rItem.second;
            sal_uInt16 nWhich = pItem->Which();
            // Handle fill attributes just like frame attributes for now.
            if ( (nWhich >= RES_PARATR_BEGIN && nWhich < RES_FRMATR_END && nWhich != RES_PARATR_NUMRULE ) ||
                 (nWhich >= XATTR_FILL_FIRST && nWhich < XATTR_FILL_LAST))
                AttrOutput().OutputItem( *pItem );
        }

        // Has to be called after RES_PARATR_GRABBAG is processed.
        const XFillStyleItem* pFill(rSet.GetItem<XFillStyleItem>(XATTR_FILLSTYLE, false));
        if (!bAlreadyOutputBrushItem && pFill
            && (pFill->GetValue() == drawing::FillStyle_SOLID || pFill->GetValue() == drawing::FillStyle_NONE)
            && !rSet.GetItem(RES_BACKGROUND, false))
        {
            const bool bFillStyleNone = pFill->GetValue() == drawing::FillStyle_NONE;
            // No need to write out a NONE background if it can't inherit something else, or if it already inherits a NONE.
            std::unique_ptr<SvxBrushItem> pInherited;
            if (bFillStyleNone)
            {
                if ( auto pNd = dynamic_cast<const SwContentNode*>(m_pOutFormatNode)) //paragraph
                    pInherited = getSvxBrushItemFromSourceSet(static_cast<SwTextFormatColl&>(pNd->GetAnyFormatColl()).GetAttrSet(), RES_BACKGROUND);
                else if (m_bStyDef && m_pCurrentStyle && m_pCurrentStyle->DerivedFrom()) //style
                    pInherited = getSvxBrushItemFromSourceSet(m_pCurrentStyle->DerivedFrom()->GetAttrSet(), RES_BACKGROUND);
            }
            // Construct an SvxBrushItem, as expected by the exporters.
            std::unique_ptr<SvxBrushItem> aBrush(getSvxBrushItemFromSourceSet(rSet, RES_BACKGROUND));
            if (!bFillStyleNone || (pInherited && *pInherited != *aBrush))
                AttrOutput().OutputItem(*aBrush);
        }
#if 0
        else
        {
            // note: *does not work* due to undocumented Word behavior: must be before a:ln element at least
            AttrOutput().MaybeOutputBrushItem(rSet);
        }
#endif
    }
    m_pISet = nullptr;                      // for double attributes
}

void MSWordExportBase::GatherChapterFields()
{
    //If the header/footer contains a chapter field
    SwFieldType* pType = m_rDoc.getIDocumentFieldsAccess().GetSysFieldType( SwFieldIds::Chapter );
    pType->GatherNodeIndex(m_aChapterFieldLocs);
}

bool MSWordExportBase::ContentContainsChapterField(const SwFormatContent &rContent) const
{
    bool bRet = false;
    if ( const SwNodeIndex* pSttIdx = rContent.GetContentIdx() )
    {
        SwNodeIndex aIdx( *pSttIdx, 1 );
        SwNodeIndex aEnd( *pSttIdx->GetNode().EndOfSectionNode() );
        SwNodeOffset nStart = aIdx.GetIndex();
        SwNodeOffset nEnd = aEnd.GetIndex();
        //If the header/footer contains a chapter field
        bRet = std::any_of(m_aChapterFieldLocs.cbegin(), m_aChapterFieldLocs.cend(),
            [nStart, nEnd](SwNodeOffset i) { return ( nStart <= i ) && ( i <= nEnd ); });
    }
    return bRet;
}

bool MSWordExportBase::FormatHdFtContainsChapterField(const SwFrameFormat &rFormat) const
{
    if ( m_aChapterFieldLocs.empty() )
        return false;

    const SwFrameFormat *pFormat = nullptr;

    pFormat = rFormat.GetHeader().GetHeaderFormat();
    if ( pFormat && ContentContainsChapterField( pFormat->GetContent() ) )
        return true;

    pFormat = rFormat.GetFooter().GetFooterFormat();
    return pFormat && ContentContainsChapterField( pFormat->GetContent() );
}

bool MSWordExportBase::SetCurrentPageDescFromNode(const SwNode &rNd)
{
    bool bNewPageDesc = false;
    const SwPageDesc* pCurrent = SwPageDesc::GetPageDescOfNode(rNd);
    OSL_ENSURE(pCurrent && m_pCurrentPageDesc, "Not possible surely");
    if (m_pCurrentPageDesc && pCurrent)
    {
        if (pCurrent != m_pCurrentPageDesc)
        {
            if (m_pCurrentPageDesc->GetFollow() != pCurrent)
                bNewPageDesc = true;
            else
            {
                const SwFrameFormat& rTitleFormat = m_pCurrentPageDesc->GetFirstMaster();
                const SwFrameFormat& rFollowFormat = pCurrent->GetMaster();

                bNewPageDesc = !IsPlausableSingleWordSection(rTitleFormat,
                    rFollowFormat);
            }
            m_pCurrentPageDesc = pCurrent;
        }
        else
        {
            const SwFrameFormat &rFormat = pCurrent->GetMaster();
            bNewPageDesc = FormatHdFtContainsChapterField(rFormat);
        }
    }
    return bNewPageDesc;
}

/**
 * WW only knows Break-After (page break and section breaks),
 * whereas in SW page breaks exist both "before" and "after" and PageDesc exists
 * only "before". Therefore the breaks are iterated two times, namely before
 * and after every line.
 * Depending on the break type they're set before or after the line.
 * Only functions can be called, which do not write in output area pO,
 * because that one only exits once for CHP and PAP and therefore end up in
 * the wrong one.
 */
void MSWordExportBase::OutputSectionBreaks( const SfxItemSet *pSet, const SwNode& rNd, bool isCellOpen )
{
    if ( m_bStyDef || m_bOutKF || m_bInWriteEscher || m_bOutPageDescs )
        return;

    m_bBreakBefore = true;
    bool bNewPageDesc = false;
    const SwFormatPageDesc *pPgDesc=nullptr;
    bool bExtraPageBreakBeforeSectionBreak = false;

    //Output a sectionbreak if there's a new pagedescriptor. Otherwise output a
    //pagebreak if there is a pagebreak here, unless the new page (follow
    //style) is different to the current one, in which case plump for a
    //section.
    bool bBreakSet = false;

    const SwPageDesc * pPageDesc = rNd.FindPageDesc();

    // Even if m_pCurrentPageDesc != pPageDesc ,it might be because of the different header & footer types.
    if (m_pCurrentPageDesc != pPageDesc)
    {
        if (isCellOpen && ( m_pCurrentPageDesc->GetName() != pPageDesc->GetName() ))
        {
            /*
             * If Table cell is open and page header types are different
             * set pSet to NULL as we don't want to add any section breaks inside a table.
             */
            pSet = nullptr;
        }
        else if (!sw::util::IsPlausableSingleWordSection(m_pCurrentPageDesc->GetFirstMaster(), pPageDesc->GetMaster()))
        {
            bBreakSet = true;
            bNewPageDesc = true;
            m_pCurrentPageDesc = pPageDesc;
        }
    }

    if ( pSet && pSet->Count() )
    {
        const SwFormatPageDesc * pItem = pSet->GetItemIfSet( RES_PAGEDESC, false );
        if ( pItem && pItem->GetRegisteredIn() != nullptr)
        {
            bBreakSet = true;
            // Avoid unnecessary section breaks if possible. LO can't notice identical
            // sections during import, so minimize unnecessary duplication
            // by substituting a simple page break when the resulting section is identical,
            // unless this is needed to re-number the page.
            // DOCX only.
            if (!bNewPageDesc && !pItem->GetNumOffset() && !PreferPageBreakBefore()
                && m_pCurrentPageDesc && m_pCurrentPageDesc->GetFollow() == pItem->GetPageDesc())
            {
                // A section break on the very first paragraph is ignored by LO/Word
                // and should NOT be turned into a page break.
                SwNodeIndex aDocEnd(m_rDoc.GetNodes().GetEndOfContent());
                SwNodeIndex aStart(*aDocEnd.GetNode().StartOfSectionNode());
                // Set aStart to the first content node in the section
                SwNodes::GoNext(&aStart);
                assert(aStart <= aDocEnd && "impossible: end section must have one content node");
                if (rNd.GetIndex() > aStart.GetNode().GetIndex())
                   AttrOutput().OutputItem(SvxFormatBreakItem(SvxBreak::PageBefore, RES_BREAK));
            }
            else
                bNewPageDesc = true;

            pPgDesc = pItem;
            m_pCurrentPageDesc = pPgDesc->GetPageDesc();

            // tdf#121666: nodes that have pagebreak + sectionbreak may need to export both breaks
            // tested / implemented with docx format only.
            // If other formats (rtf /doc) need similar fix, then that may can be done similar way.
            if (SfxItemState::SET == pSet->GetItemState(RES_BREAK, false))
            {
                bExtraPageBreakBeforeSectionBreak = true;
            }
        }
        else if ( const SvxFormatBreakItem* pBreak = pSet->GetItemIfSet( RES_BREAK, false ) )
        {
            // Word does not like hard break attributes in some table cells
            bool bRemoveHardBreakInsideTable = false;
            if ( m_bOutTable )
            {
                const SwTableNode* pTableNode = rNd.FindTableNode();
                if ( pTableNode )
                {
                    const SwTableBox* pBox = rNd.GetTableBox();
                    const SwTableLine* pLine = pBox ? pBox->GetUpper() : nullptr;
                    // but only for non-complex tables
                    if ( pLine && !pLine->GetUpper() )
                    {
                        // check if box is not first in that line:
                        if ( 0 < pLine->GetBoxPos( pBox ) && pBox->GetSttNd() )
                        {
                            bRemoveHardBreakInsideTable = true;
                        }
                    }
                }
            }
            bBreakSet = true;

            if ( !bRemoveHardBreakInsideTable )
            {
                OSL_ENSURE(m_pCurrentPageDesc, "should not be possible");
                /*
                 If because of this pagebreak the page desc following the page
                 break is the follow style of the current page desc then output a
                 section break using that style instead.  At least in those cases
                 we end up with the same style in word and writer, nothing can be
                 done when it happens when we get a new pagedesc because we
                 overflow from the first page style.
                */
                if ( m_pCurrentPageDesc )
                {
                    // #i76301# - assure that there is a page break before set at the node.
                    if ( pBreak->GetBreak() == SvxBreak::PageBefore )
                    {
                        bNewPageDesc |= SetCurrentPageDescFromNode( rNd );
                    }
                }

                // If the columns are different in LO's adjacent sections, create a new MS section
                if (!bNewPageDesc && pBreak->GetBreak() == SvxBreak::PageBefore
                    && Sections().CurrentSectionInfo())
                {
                    const SwSectionFormat* pSectionFormat = MSWordExportBase::GetSectionFormat(rNd);
                    if (pSectionFormat)
                    {
                        const SwFormatCol& rNewSect = pSectionFormat->GetFormatAttr(RES_COL);
                        const SwFormatCol& rPrevSect
                            = MSWordSections::GetFormatCol(m_rDoc,
                                                           *Sections().CurrentSectionInfo());
                        if (rNewSect.GetNumCols() != rPrevSect.GetNumCols()
                            || !rNewSect.IsOrtho() || !rPrevSect.IsOrtho()
                            || rNewSect.GetLineStyle() != rPrevSect.GetLineStyle()
                            || rNewSect.GetLineWidth() != rPrevSect.GetLineWidth()
                            || rNewSect.GetLineColor() != rPrevSect.GetLineColor()
                            || rNewSect.GetLineHeight() != rPrevSect.GetLineHeight()
                            || rNewSect.GetLineAdj() != rPrevSect.GetLineAdj())
                        {
                            bNewPageDesc = true;
                        }
                    }
                }

                if ( !bNewPageDesc )
                    AttrOutput().OutputItem( *pBreak );
            }
        }
    }

    /*
    #i9301#
    No explicit page break, let's see if the style had one and we've moved to a
    new page style because of it, if we have to then we take the opportunity to
    set the equivalent word section here. We *could* do it for every paragraph
    that moves onto a new page because of layout, but that would be insane.
    */
    bool bHackInBreak = false;
    if ( !bBreakSet )
    {
        if ( const SwContentNode *pNd = rNd.GetContentNode() )
        {
            const SvxFormatBreakItem &rBreak = pNd->GetAttr( RES_BREAK );
            if ( rBreak.GetBreak() == SvxBreak::PageBefore )
                bHackInBreak = true;
            else
            {   // Even a pagedesc item is set, the break item can be set 'NONE',
                // but a pagedesc item is an implicit page break before...
                const SwFormatPageDesc &rPageDesc = pNd->GetAttr( RES_PAGEDESC );
                if ( rPageDesc.KnowsPageDesc() )
                    bHackInBreak = true;
            }
        }
    }

    if ( bHackInBreak )
    {
        OSL_ENSURE( m_pCurrentPageDesc, "should not be possible" );
        if ( m_pCurrentPageDesc )
            bNewPageDesc = SetCurrentPageDescFromNode( rNd );
    }

    if ( bNewPageDesc && m_pCurrentPageDesc )
    {
        PrepareNewPageDesc( pSet, rNd, pPgDesc, m_pCurrentPageDesc, bExtraPageBreakBeforeSectionBreak );
    }
    m_bBreakBefore = false;
}

// #i76300#
bool MSWordExportBase::OutputFollowPageDesc( const SfxItemSet* pSet, const SwTextNode* pNd )
{
    bool bRet = false;

    if ( pNd &&
         m_pCurrentPageDesc &&
         m_pCurrentPageDesc != m_pCurrentPageDesc->GetFollow() )
    {
        PrepareNewPageDesc( pSet, *pNd, nullptr, m_pCurrentPageDesc->GetFollow() );
        bRet = true;
    }

    return bRet;
}

const SwSectionFormat* MSWordExportBase::GetSectionFormat( const SwNode& rNd )
{
    const SwSectionFormat* pFormat = nullptr;
    const SwSectionNode* pSect = rNd.FindSectionNode();
    if ( pSect &&
         SectionType::Content == pSect->GetSection().GetType() )
    {
        pFormat = pSect->GetSection().GetFormat();
    }

    return pFormat;
}

sal_uLong MSWordExportBase::GetSectionLineNo( const SfxItemSet* pSet, const SwNode& rNd )
{
    const SwFormatLineNumber* pNItem = nullptr;
    if ( pSet )
    {
        pNItem = & pSet->Get( RES_LINENUMBER );
    }
    else if ( const SwContentNode *pNd = rNd.GetContentNode() )
    {
        pNItem = &pNd->GetAttr( RES_LINENUMBER );
    }

    return pNItem? pNItem->GetStartValue() : 0;
}

void WW8Export::PrepareNewPageDesc( const SfxItemSet*pSet,
                                      const SwNode& rNd,
                                      const SwFormatPageDesc* pNewPgDescFormat,
                                      const SwPageDesc* pNewPgDesc,
                                      bool /*bExtraPageBreak*/ )
{
    // The PageDescs will only be inserted in WW8Writer::pSepx with the corresponding
    // position by the occurrences of PageDesc attributes. The construction and
    // output of the attributes and header/footer of the PageDesc are done
    // after the main text and its attributes.

    sal_uLong nFcPos = ReplaceCr( msword::PageBreak ); // Page/Section-Break

    // actually nothing is outputted here, rather the arrays aCps, aSects
    // accordingly completed
    if ( !nFcPos )
        return;

    const SwSectionFormat* pFormat = GetSectionFormat( rNd );
    const sal_uLong nLnNm = GetSectionLineNo( pSet, rNd );

    OSL_ENSURE( pNewPgDescFormat || pNewPgDesc, "Neither page desc format nor page desc provided." );

    if ( pNewPgDescFormat )
    {
        m_pSepx->AppendSep( Fc2Cp( nFcPos ), *pNewPgDescFormat, rNd, pFormat, nLnNm );
    }
    else if ( pNewPgDesc )
    {
        m_pSepx->AppendSep( Fc2Cp( nFcPos ), SwFormatPageDesc(pNewPgDesc), rNd, pFormat, nLnNm );
    }
}

void MSWordExportBase::CorrectTabStopInSet( SfxItemSet& rSet, sal_Int32 nAbsLeft )
{
    const SvxTabStopItem *pItem = rSet.GetItem<SvxTabStopItem>(RES_PARATR_TABSTOP);
    if (!pItem)
        return;

    // then it must be corrected for the output
    SvxTabStopItem aTStop(*pItem);
    sal_uInt16 nCnt = 0;
    while (nCnt < aTStop.Count())
    {
        SvxTabStop& rTab = const_cast<SvxTabStop&>(aTStop[ nCnt ]);
        if ( SvxTabAdjust::Default != rTab.GetAdjustment() &&
            rTab.GetTabPos() >= nAbsLeft )
        {
            rTab.GetTabPos() -= nAbsLeft;
            ++nCnt;
        }
        else
        {
            aTStop.Remove(nCnt);
        }
    }
    rSet.Put( aTStop );
}

tools::Long MSWordExportBase::GetParaTabStopOffset() const
{
    tools::Long nOffset = 0;
    // Tabs are absolute by default.
    if (m_rDoc.getIDocumentSettingAccess().get(DocumentSettingId::TABS_RELATIVE_TO_INDENT))
    {
        // don't do it for editengine text, it doesn't implement this anyway
        if (!m_pISet || m_pISet->GetRanges()[0].first < RES_WHICHHINT_END)
        {
            nOffset = GetItem(RES_MARGIN_TEXTLEFT).ResolveTextLeft({});
        }
    }
    return nOffset;
}

sal_uInt8 WW8Export::GetNumId( sal_uInt16 eNumType )
{
    sal_uInt8 nRet = 0;
    switch( eNumType )
    {
    case SVX_NUM_CHARS_UPPER_LETTER:
    case SVX_NUM_CHARS_UPPER_LETTER_N:  nRet = 3;       break;
    case SVX_NUM_CHARS_LOWER_LETTER:
    case SVX_NUM_CHARS_LOWER_LETTER_N:  nRet = 4;       break;
    case SVX_NUM_ROMAN_UPPER:           nRet = 1;       break;
    case SVX_NUM_ROMAN_LOWER:           nRet = 2;       break;
    case style::NumberingType::TEXT_NUMBER: nRet = 5; break;
    case style::NumberingType::TEXT_CARDINAL: nRet = 6; break;
    case style::NumberingType::TEXT_ORDINAL: nRet = 7; break;
    case style::NumberingType::AIU_HALFWIDTH_JA: nRet = 12; break;
    case style::NumberingType::IROHA_HALFWIDTH_JA: nRet = 13; break;
    case style::NumberingType::FULLWIDTH_ARABIC: nRet = 14; break;
    case style::NumberingType::NUMBER_TRADITIONAL_JA: nRet = 16; break;
    case style::NumberingType::CIRCLE_NUMBER: nRet = 18; break;
    case style::NumberingType::AIU_FULLWIDTH_JA: nRet = 20; break;
    case style::NumberingType::IROHA_FULLWIDTH_JA: nRet = 21; break;

    case SVX_NUM_BITMAP:
    case SVX_NUM_CHAR_SPECIAL:          nRet = 23;      break;
    case style::NumberingType::HANGUL_SYLLABLE_KO: nRet = 24; break;// ganada
    case style::NumberingType::HANGUL_JAMO_KO: nRet = 25; break;// chosung
    case style::NumberingType::HANGUL_CIRCLED_SYLLABLE_KO: nRet = 24; break;
    case style::NumberingType::HANGUL_CIRCLED_JAMO_KO: nRet = 25; break;
    case style::NumberingType::TIAN_GAN_ZH: nRet = 30; break;
    case style::NumberingType::DI_ZI_ZH: nRet = 31; break;
    case style::NumberingType::NUMBER_UPPER_ZH_TW: nRet = 34;break;
    case style::NumberingType::NUMBER_UPPER_ZH: nRet = 38; break;
    case style::NumberingType::NUMBER_DIGITAL_KO: nRet = 41; break;
    case style::NumberingType::NUMBER_HANGUL_KO: nRet = 42; break;
    case style::NumberingType::NUMBER_LEGAL_KO: nRet = 43; break;
    case style::NumberingType::NUMBER_DIGITAL2_KO: nRet = 44; break;
    case style::NumberingType::NUMBER_HEBREW: nRet = 45; break;
    case style::NumberingType::CHARS_ARABIC: nRet = 46; break;
    case style::NumberingType::CHARS_HEBREW: nRet = 47; break;
    case style::NumberingType::CHARS_ARABIC_ABJAD: nRet = 48; break;
    case style::NumberingType::CHARS_PERSIAN:
    case style::NumberingType::CHARS_NEPALI: nRet = 49; break;
    case style::NumberingType::CHARS_THAI: nRet = 53; break;
    case style::NumberingType::CHARS_CYRILLIC_LOWER_LETTER_N_RU:
    case style::NumberingType::CHARS_CYRILLIC_LOWER_LETTER_RU: nRet = 58; break;
    case style::NumberingType::CHARS_CYRILLIC_UPPER_LETTER_N_RU:
    case style::NumberingType::CHARS_CYRILLIC_UPPER_LETTER_RU: nRet = 59; break;
    // nothing, WW does the same (undocumented)
    case SVX_NUM_NUMBER_NONE:           nRet = 0xff;    break;
    case SVX_NUM_SYMBOL_CHICAGO:
        // 0x09, msonfcChiManSty
        nRet = 9;
        break;
    case SVX_NUM_ARABIC_ZERO:
        // 0x16, msonfcArabicLZ
        nRet = 22;
        break;
    }
    return nRet;
}

void WW8AttributeOutput::OutlineNumbering(sal_uInt8 /*nLvl*/)
{
    // Handled by ParaOutlineLevel and ParaNumRule
}

// #i77805#
bool WW8Export::DisallowInheritingOutlineNumbering(const SwFormat &rFormat)
{
    bool bRet( false );

    //If there is no numbering on this fmt, but its parent was outline
    //numbered, then in writer this is not inherited, but in word it would
    //be, so we must export "no numbering" and "body level" to make word
    //behave like writer (see #i25755)
    if (SfxItemState::SET != rFormat.GetItemState(RES_PARATR_NUMRULE, false))
    {
        if (const SwFormat *pParent = rFormat.DerivedFrom())
        {
            if (static_cast<const SwTextFormatColl*>(pParent)->IsAssignedToListLevelOfOutlineStyle())
            {
                SwWW8Writer::InsUInt16(*m_pO, NS_sprm::POutLvl::val);
                m_pO->push_back(sal_uInt8(9));
                SwWW8Writer::InsUInt16(*m_pO, NS_sprm::PIlfo::val);
                SwWW8Writer::InsUInt16(*m_pO, 0);

                bRet = true;
            }
        }
    }

    return bRet;
}

void MSWordExportBase::OutputFormat( const SwFormat& rFormat, bool bPapFormat, bool bChpFormat, bool bFlyFormat )
{
    bool bCallOutSet = true;
    const sw::BroadcastingModify* pOldMod = m_pOutFormatNode;
    m_pOutFormatNode = &rFormat;

    switch( rFormat.Which() )
    {
    case RES_CONDTXTFMTCOLL:
    case RES_TXTFMTCOLL:
        if( bPapFormat )
        {
            int nLvl = MAXLEVEL;

            if (static_cast<const SwTextFormatColl&>(rFormat).IsAssignedToListLevelOfOutlineStyle())
                nLvl = static_cast<const SwTextFormatColl&>(rFormat).GetAssignedOutlineStyleLevel();

            if (nLvl >= 0 && nLvl < MAXLEVEL)
            {
                //if outline numbered
                // if Write StyleDefinition then write the OutlineRule
                const SwNumFormat& rNFormat = m_rDoc.GetOutlineNumRule()->Get( o3tl::narrowing<sal_uInt16>( nLvl ) );
                if ( m_bStyDef )
                    AttrOutput().OutlineNumbering(static_cast<sal_uInt8>(nLvl));

                if ( rNFormat.GetPositionAndSpaceMode() ==
                                           SvxNumberFormat::LABEL_WIDTH_AND_POSITION  &&
                     rNFormat.GetAbsLSpace() )
                {
                    SfxItemSet aSet( rFormat.GetAttrSet() );
                    SvxFirstLineIndentItem firstLine(aSet.Get(RES_MARGIN_FIRSTLINE));
                    SvxTextLeftMarginItem leftMargin(aSet.Get(RES_MARGIN_TEXTLEFT));

                    leftMargin.SetTextLeft(SvxIndentValue::twips(leftMargin.ResolveTextLeft({})
                                                                 + rNFormat.GetAbsLSpace()));
                    firstLine.SetTextFirstLineOffset(
                        SvxIndentValue::twips(GetWordFirstLineOffset(rNFormat)));

                    aSet.Put(firstLine);
                    aSet.Put(leftMargin);
                    CorrectTabStopInSet( aSet, rNFormat.GetAbsLSpace() );
                    OutputItemSet( aSet, bPapFormat, bChpFormat,
                        i18n::ScriptType::LATIN, m_bExportModeRTF);
                    bCallOutSet = false;
                }
            }
            else
            {
                //otherwise we might have to remove outline numbering from
                //what gets exported if the parent style was outline numbered
                // #i77805#
                // If inherited outline numbering is suppress, the left/right
                // margins has to be exported explicitly.
                if ( m_bStyDef && DisallowInheritingOutlineNumbering(rFormat) )
                {
                    SfxItemSet aSet( rFormat.GetAttrSet() );
                    SvxFirstLineIndentItem const& rFirstLine(aSet.Get(RES_MARGIN_FIRSTLINE));
                    SvxTextLeftMarginItem const& rLeftMargin(aSet.Get(RES_MARGIN_TEXTLEFT));
                    aSet.Put(rFirstLine);
                    aSet.Put(rLeftMargin);
                    OutputItemSet( aSet, bPapFormat, bChpFormat,
                        css::i18n::ScriptType::LATIN, m_bExportModeRTF);
                    bCallOutSet = false;
                }
            }
        }
        break;

    case RES_CHRFMT:
        break;
    case RES_FLYFRMFMT:
        if (bFlyFormat)
        {
            OSL_ENSURE(m_pParentFrame, "No parent frame, all broken");

            if (m_pParentFrame)
            {
                const SwFrameFormat &rFrameFormat = m_pParentFrame->GetFrameFormat();

                SfxItemSetFixed<RES_FRMATR_BEGIN, RES_FRMATR_END-1,
                    XATTR_FILL_FIRST, XATTR_FILL_LAST>  aSet(m_rDoc.GetAttrPool());
                aSet.Set(rFrameFormat.GetAttrSet());

                // Fly as character becomes a paragraph bound
                // now set the distance to paragraph margin
                if (m_pFlyOffset)
                {
                    aSet.Put(SwFormatHoriOrient(m_pFlyOffset->X()));
                    aSet.Put(SwFormatVertOrient(m_pFlyOffset->Y()));
                    SwFormatAnchor aAnchor(rFrameFormat.GetAnchor());
                    aAnchor.SetType(m_eNewAnchorType);
                    aSet.Put(aAnchor);
                }

                if (SfxItemState::SET != aSet.GetItemState(RES_SURROUND))
                    aSet.Put(SwFormatSurround(css::text::WrapTextMode_NONE));

                const XFillStyleItem* pXFillStyleItem(rFrameFormat.GetAttrSet().GetItem<XFillStyleItem>(XATTR_FILLSTYLE));
                if (pXFillStyleItem)
                {
                    switch (pXFillStyleItem->GetValue())
                    {
                    case drawing::FillStyle_NONE:
                        break;
                    case drawing::FillStyle_SOLID:
                    {
                        // Construct an SvxBrushItem, as expected by the exporters.
                        aSet.Put(getSvxBrushItemFromSourceSet(rFrameFormat.GetAttrSet(), RES_BACKGROUND));
                        break;
                    }
                    default:
                        break;
                    }
                }

                m_bOutFlyFrameAttrs = true;
                //script doesn't matter if not exporting chp
                OutputItemSet(aSet, true, false,
                    i18n::ScriptType::LATIN, m_bExportModeRTF);
                m_bOutFlyFrameAttrs = false;

                bCallOutSet = false;
            }
        }
        break;
    case RES_FRMFMT:
        break;
    default:
        OSL_ENSURE( false, "Which format is exported here?" );
        break;
    }

    if( bCallOutSet )
        OutputItemSet( rFormat.GetAttrSet(), bPapFormat, bChpFormat,
            i18n::ScriptType::LATIN, m_bExportModeRTF);
    m_pOutFormatNode = pOldMod;
}

bool MSWordExportBase::HasRefToAttr(const OUString& rName)
{
    SwFieldType* pType = m_rDoc.getIDocumentFieldsAccess().GetSysFieldType(SwFieldIds::GetRef);
    std::vector<SwGetRefField*> vpRFields;
    pType->GatherRefFields(vpRFields, ReferencesSubtype::SetRefAttr);
    return std::any_of(vpRFields.begin(), vpRFields.end(),
            [rName](SwGetRefField* pF) { return rName == pF->GetSetRefName(); });
}

bool MSWordExportBase::HasRefToFootOrEndnote(const bool isEndNote, const sal_uInt16 nSeqNo)
{
    SwFieldType* pType = m_rDoc.getIDocumentFieldsAccess().GetSysFieldType(SwFieldIds::GetRef);
    std::vector<SwGetRefField*> vpRFields;
    pType->GatherRefFields(vpRFields, isEndNote ? ReferencesSubtype::Endnote : ReferencesSubtype::Footnote);
    return std::any_of(vpRFields.begin(), vpRFields.end(),
            [nSeqNo](SwGetRefField* pF) { return nSeqNo == pF->GetSeqNo(); });
}

OUString MSWordExportBase::GetBookmarkName( ReferencesSubtype nTyp, const OUString* pName, sal_uInt16 nSeqNo )
{
    OUString sRet;
    switch ( nTyp )
    {
        case ReferencesSubtype::SetRefAttr:
            if ( pName )
            {
                sRet = "Ref_" + *pName;
            }
            break;
        case ReferencesSubtype::SequenceField:
        {
            assert(pName);
            sRet = "Ref_" + *pName;
            break;
        }
        case ReferencesSubtype::Bookmark:
            if ( pName )
                sRet = *pName;
            break;
        case ReferencesSubtype::Outline:
            break;      // ???
        case ReferencesSubtype::Footnote:
            sRet = "_RefF" + OUString::number( nSeqNo );
            break;
        case ReferencesSubtype::Endnote:
            sRet = "_RefE" + OUString::number( nSeqNo );
            break;
        default: break; // ReferencesSubtype::Style not handled?
    }
    return BookmarkToWord( sRet ); // #i43956# - encode bookmark accordingly
}

OUString MSWordExportBase::GetStyleRefName(const OUString& rName)
{
    SwTextFormatColls* pTextFormatColls = m_rDoc.GetTextFormatColls();
    SwTextFormatColl* pTextFormat = pTextFormatColls->FindFormatByName(UIName(rName));

    if (pTextFormat == nullptr)
        return "\"" + rName + "\"";
    // Didn't find the style, just keep the original name

    return "\"" + m_pStyles->GetStyleWWName(pTextFormat) + "\"";
}

/* File CHRATR.HXX: */
void WW8AttributeOutput::RTLAndCJKState( bool bIsRTL, sal_uInt16 nScript )
{
    if (bIsRTL)
    {
        if( m_rWW8Export.m_rDoc.GetDocumentType() != SwDoc::DOCTYPE_MSWORD )
        {
            m_rWW8Export.InsUInt16( NS_sprm::CFBiDi::val );
            m_rWW8Export.m_pO->push_back( sal_uInt8(1) );
        }
    }

    // #i46087# patch from james_clark; complex texts needs the undocumented SPRM CComplexScript with param 0x81.
    if (nScript == i18n::ScriptType::COMPLEX && !bIsRTL)
    {
        m_rWW8Export.InsUInt16( NS_sprm::CFComplexScripts::val );
        m_rWW8Export.m_pO->push_back( sal_uInt8(0x81) );
        m_rWW8Export.m_pDop->bUseThaiLineBreakingRules = true;
    }
}

void WW8AttributeOutput::EndParagraph( const ww8::WW8TableNodeInfoInner::Pointer_t& pTextNodeInfoInner )
{
    m_rWW8Export.m_pPapPlc->AppendFkpEntry( m_rWW8Export.Strm().Tell() - (mbOnTOXEnding?2:0), m_rWW8Export.m_pO->size(), m_rWW8Export.m_pO->data() );
    mbOnTOXEnding = false;
    m_rWW8Export.m_pO->clear();

    if ( pTextNodeInfoInner )
    {
        if ( pTextNodeInfoInner->isEndOfLine() )
        {
            TableRowEnd( pTextNodeInfoInner->getDepth() );

            SVBT16 nSty;
            ShortToSVBT16( 0, nSty );
            m_rWW8Export.m_pO->insert( m_rWW8Export.m_pO->end(), nSty, nSty+2 );     // Style #
            TableInfoRow( pTextNodeInfoInner );
            m_rWW8Export.m_pPapPlc->AppendFkpEntry( m_rWW8Export.Strm().Tell(), m_rWW8Export.m_pO->size(), m_rWW8Export.m_pO->data());
            m_rWW8Export.m_pO->clear();
            //For Bug 119650, should break the properties of CHP PLC after a paragraph end.
            m_rWW8Export.m_pChpPlc->AppendFkpEntry(m_rWW8Export.Strm().Tell(), m_rWW8Export.m_pO->size(), m_rWW8Export.m_pO->data());
        }
    }

    // Clear bookmarks of the current paragraph
    m_aBookmarksOfParagraphStart.clear();
    m_aBookmarksOfParagraphEnd.clear();
}

void WW8AttributeOutput::StartRunProperties()
{
    WW8_WrPlcField* pCurrentFields = m_rWW8Export.CurrentFieldPlc();
    m_nFieldResults = pCurrentFields ? pCurrentFields->ResultCount() : 0;
}

void WW8AttributeOutput::StartRun( const SwRedlineData* pRedlineData, sal_Int32 nPos, bool /*bSingleEmptyRun*/ )
{
    if (pRedlineData)
    {
        const OUString &rComment = pRedlineData->GetComment();
        //Only possible to export to main text
        if (!rComment.isEmpty() && (m_rWW8Export.m_nTextTyp == TXT_MAINTEXT) &&
            // tdf#153016 don't export the new automatic comments added by tdf#148032
            rComment != SwResId(STR_REDLINE_COMMENT_DELETED) &&
            rComment != SwResId(STR_REDLINE_COMMENT_ADDED))
        {
            if (m_rWW8Export.m_pAtn->IsNewRedlineComment(pRedlineData))
            {
                m_rWW8Export.m_pAtn->Append( m_rWW8Export.Fc2Cp( m_rWW8Export.Strm().Tell() ), pRedlineData );
                m_rWW8Export.WritePostItBegin( m_rWW8Export.m_pO.get() );
            }
        }
    }

    /// Insert bookmarks started at this run
    auto aRange = m_aBookmarksOfParagraphStart.equal_range(nPos);
    for( auto aIter = aRange.first; aIter != aRange.second; ++aIter)
    {
        GetExport().AppendBookmark(GetExport().BookmarkToWord(aIter->second));
    }
}

void WW8AttributeOutput::OnTOXEnding()
{
    mbOnTOXEnding = true;
}

void WW8AttributeOutput::EndRun( const SwTextNode* /*pNode*/, sal_Int32 nPos, sal_Int32 /*nLen*/, bool bLastRun )
{
    /// Insert bookmarks ended after this run
    auto aRange = m_aBookmarksOfParagraphEnd.equal_range(nPos);
    for( auto aIter = aRange.first; aIter != aRange.second; ++aIter)
    {
        if(bLastRun)
            GetExport().AppendBookmarkEndWithCorrection(GetExport().BookmarkToWord(aIter->second));
        else
            GetExport().AppendBookmark(GetExport().BookmarkToWord(aIter->second));
    }
}

void WW8AttributeOutput::EndRunProperties( const SwRedlineData* pRedlineData )
{
    Redline( pRedlineData );

    WW8_WrPlcField* pCurrentFields = m_rWW8Export.CurrentFieldPlc();
    sal_uInt16 nNewFieldResults = pCurrentFields ? pCurrentFields->ResultCount() : 0;

    bool bExportedFieldResult = ( m_nFieldResults != nNewFieldResults );

    // If we have exported a field result, then we will have been forced to
    // split up the text into a 0x13, 0x14, <result> 0x15 sequence with the
    // properties forced out at the end of the result, so the 0x15 itself
    // should remain clean of all other attributes to avoid #iXXXXX#
    if ( !bExportedFieldResult )
    {
        m_rWW8Export.m_pChpPlc->AppendFkpEntry( m_rWW8Export.Strm().Tell(),
                m_rWW8Export.m_pO->size(), m_rWW8Export.m_pO->data() );
    }
    m_rWW8Export.m_pO->clear();
}

void WW8AttributeOutput::RunText( const OUString& rText, rtl_TextEncoding eCharSet, const OUString& /*rSymbolFont*/ )
{
    RawText(rText, eCharSet);
}

void WW8AttributeOutput::RawText(const OUString& rText, rtl_TextEncoding)
{
    m_rWW8Export.OutSwString(rText, 0, rText.getLength());
}

void WW8AttributeOutput::OutputFKP(bool bForce)
{
    if (!m_rWW8Export.m_pO->empty() || bForce)
    {
        m_rWW8Export.m_pChpPlc->AppendFkpEntry( m_rWW8Export.Strm().Tell(),
                m_rWW8Export.m_pO->size(), m_rWW8Export.m_pO->data() );
        m_rWW8Export.m_pO->clear();
    }
}

void WW8AttributeOutput::ParagraphStyle( sal_uInt16 nStyle )
{
    OSL_ENSURE( m_rWW8Export.m_pO->empty(), " pO is not empty at line end" );

    SVBT16 nSty;
    ShortToSVBT16( nStyle, nSty );
    m_rWW8Export.m_pO->insert( m_rWW8Export.m_pO->end(), nSty, nSty+2 );     // style #
}

void WW8AttributeOutput::OutputWW8Attribute( sal_uInt8 nId, bool bVal )
{
    m_rWW8Export.InsUInt16( 8 == nId ? NS_sprm::CFDStrike::val : NS_sprm::CFBold::val + nId );

    m_rWW8Export.m_pO->push_back( bVal ? 1 : 0 );
}

void WW8AttributeOutput::OutputWW8AttributeCTL( sal_uInt8 nId, bool bVal )
{
    OSL_ENSURE( nId <= 1, "out of range" );
    if (nId > 1)
        return;

    m_rWW8Export.InsUInt16( NS_sprm::CFBoldBi::val + nId );
    m_rWW8Export.m_pO->push_back( bVal ? 1 : 0 );
}

void WW8AttributeOutput::CharFont( const SvxFontItem& rFont )
{
    sal_uInt16 nFontID = m_rWW8Export.GetId( rFont );

    m_rWW8Export.InsUInt16( NS_sprm::CRgFtc0::val );
    m_rWW8Export.InsUInt16( nFontID );
    m_rWW8Export.InsUInt16( NS_sprm::CRgFtc2::val );

    m_rWW8Export.InsUInt16( nFontID );
}

void WW8AttributeOutput::CharFontCTL( const SvxFontItem& rFont )
{
    sal_uInt16 nFontID = m_rWW8Export.GetId( rFont );
    m_rWW8Export.InsUInt16( NS_sprm::CFtcBi::val );
    m_rWW8Export.InsUInt16( nFontID );
}

void WW8AttributeOutput::CharFontCJK( const SvxFontItem& rFont )
{
    sal_uInt16 nFontID = m_rWW8Export.GetId( rFont );
    m_rWW8Export.InsUInt16( NS_sprm::CRgFtc1::val );
    m_rWW8Export.InsUInt16( nFontID );
}

void WW8AttributeOutput::CharWeightCTL( const SvxWeightItem& rWeight )
{
    OutputWW8AttributeCTL( 0, WEIGHT_BOLD == rWeight.GetWeight());
}

void WW8AttributeOutput::CharPostureCTL( const SvxPostureItem& rPosture )
{
    OutputWW8AttributeCTL( 1, ITALIC_NONE != rPosture.GetPosture() );
}

void WW8AttributeOutput::CharPosture( const SvxPostureItem& rPosture )
{
    OutputWW8Attribute( 1, ITALIC_NONE != rPosture.GetPosture() );
}

void WW8AttributeOutput::CharWeight( const SvxWeightItem& rWeight )
{
    OutputWW8Attribute( 0, WEIGHT_BOLD == rWeight.GetWeight() );
}

// Shadowed and Contour are not in WW-UI. JP: ??
void WW8AttributeOutput::CharContour( const SvxContourItem& rContour )
{
    OutputWW8Attribute( 3, rContour.GetValue() );
}

void WW8AttributeOutput::CharShadow( const SvxShadowedItem& rShadow )
{
    OutputWW8Attribute( 4, rShadow.GetValue() );
}

void WW8AttributeOutput::CharKerning( const SvxKerningItem& rKerning )
{
    m_rWW8Export.InsUInt16( NS_sprm::CDxaSpace::val );

    m_rWW8Export.InsUInt16( rKerning.GetValue() );
}

void WW8AttributeOutput::CharAutoKern( const SvxAutoKernItem& rAutoKern )
{
    m_rWW8Export.InsUInt16( NS_sprm::CHpsKern::val );

    m_rWW8Export.InsUInt16( rAutoKern.GetValue() ? 2 : 0 );
}

void WW8AttributeOutput::CharAnimatedText( const SvxBlinkItem& rBlink )
{
    m_rWW8Export.InsUInt16( NS_sprm::CSfxText::val );
    // At the moment the only animated text effect we support is blinking
    m_rWW8Export.m_pO->push_back( rBlink.GetValue() ? 2 : 0 );
}

void WW8AttributeOutput::CharCrossedOut( const SvxCrossedOutItem& rCrossed )
{
    FontStrikeout eSt = rCrossed.GetStrikeout();
    if ( STRIKEOUT_DOUBLE == eSt )
    {
        OutputWW8Attribute( 8, true );
        return;
    }
    if ( STRIKEOUT_NONE != eSt )
    {
        OutputWW8Attribute( 2, true );
        return;
    }

    // otherwise both off
    OutputWW8Attribute( 8, false );
    OutputWW8Attribute( 2, false );
}

void WW8AttributeOutput::CharCaseMap( const SvxCaseMapItem& rCaseMap )
{
    SvxCaseMap eSt = rCaseMap.GetValue();
    switch ( eSt )
    {
        case SvxCaseMap::SmallCaps:
            OutputWW8Attribute( 5, true );
            return;
        case SvxCaseMap::Uppercase:
            OutputWW8Attribute( 6, true );
            return;
        case SvxCaseMap::Capitalize:
            // no such feature in word
            break;
        default:
            // otherwise both off
            OutputWW8Attribute( 5, false );
            OutputWW8Attribute( 6, false );
            return;
    }
}

void WW8AttributeOutput::CharHidden( const SvxCharHiddenItem& rHidden )
{
    OutputWW8Attribute( 7, rHidden.GetValue() );
}

void WW8AttributeOutput::CharBorder( const SvxBoxItem& rBox )
{
    const auto [ pAllBorder, nDist, bShadow ] = FormatCharBorder(rBox);
    (void)nDist;
    WW8Export::Out_BorderLine( *m_rWW8Export.m_pO, pAllBorder, 0, NS_sprm::CBrc80::val, NS_sprm::CBrc::val, bShadow );
}

void WW8AttributeOutput::CharHighlight( const SvxBrushItem& rBrush )
{
    sal_uInt8 nColor = msfilter::util::TransColToIco( rBrush.GetColor() );
    m_rWW8Export.InsUInt16( NS_sprm::CHighlight::val );
    m_rWW8Export.m_pO->push_back( nColor );
}

void WW8AttributeOutput::CharScriptHint(const SvxScriptHintItem& rHint)
{
    sal_uInt8 nHint = 0;
    switch (rHint.GetValue())
    {
        case i18nutil::ScriptHintType::Asian:
            nHint = 1;
            break;

        case i18nutil::ScriptHintType::Complex:
            nHint = 2;
            break;

        default:
            break;
    }

    m_rWW8Export.InsUInt16(NS_sprm::CIdctHint::val);
    m_rWW8Export.m_pO->push_back(nHint);
}

void WW8AttributeOutput::CharUnderline( const SvxUnderlineItem& rUnderline )
{
    m_rWW8Export.InsUInt16( NS_sprm::CKul::val );

    // FIXME: this should likely be a StaticWhichCast(), but some we put something dirty in RES_CHRATR_WORDLINEMODE apparently
    const auto pItem = m_rWW8Export.HasItem(RES_CHRATR_WORDLINEMODE);
    bool bWord = false;
    if(pItem)
    {
        const auto pWordline = pItem->DynamicWhichCast(RES_CHRATR_WORDLINEMODE);
        if(pWordline)
            bWord = pWordline->GetValue();
        else
            SAL_WARN("sw.ww8", "m_rWW8Export has an RES_CHRATR_WORDLINEMODE item, but it's of the wrong type.");
    }

    // WW95 - parameters:   0 = none,   1 = single, 2 = by Word,
    //                      3 = double, 4 = dotted, 5 = hidden
    // WW97 - additional parameters:
    //                      6 = thick,   7 = dash,       8 = dot(not used)
    //                      9 = dotdash 10 = dotdotdash, 11 = wave
    sal_uInt8 b = 0;
    switch ( rUnderline.GetLineStyle() )
    {
        case LINESTYLE_SINGLE:
            b = bWord ? 2 : 1;
            break;
        case LINESTYLE_BOLD:
            b = 6;
            break;
        case LINESTYLE_DOUBLE:
            b = 3;
            break;
        case LINESTYLE_DOTTED:
            b = 4;
            break;
        case LINESTYLE_DASH:
            b = 7;
            break;
        case LINESTYLE_DASHDOT:
            b = 9;
            break;
        case LINESTYLE_DASHDOTDOT:
            b = 10;
            break;
        case LINESTYLE_WAVE:
            b = 11;
            break;
        // new in WW2000
        case LINESTYLE_BOLDDOTTED:
            b = 20;
            break;
        case LINESTYLE_BOLDDASH:
            b = 23;
            break;
        case LINESTYLE_LONGDASH:
            b = 39;
            break;
        case LINESTYLE_BOLDLONGDASH:
            b = 55;
            break;
        case LINESTYLE_BOLDDASHDOT:
            b = 25;
            break;
        case LINESTYLE_BOLDDASHDOTDOT:
            b = 26;
            break;
        case LINESTYLE_BOLDWAVE:
            b = 27;
            break;
        case LINESTYLE_DOUBLEWAVE:
            b = 43;
            break;
        case LINESTYLE_NONE:
            b = 0;
            break;
        default:
            OSL_ENSURE( rUnderline.GetLineStyle() == LINESTYLE_NONE, "Unhandled underline type" );
            break;
    }

    m_rWW8Export.m_pO->push_back( b );
    Color aColor = rUnderline.GetColor();
    if( aColor != COL_TRANSPARENT )
    {
        m_rWW8Export.InsUInt16( NS_sprm::CCvUl::val );

        m_rWW8Export.InsUInt32( wwUtility::RGBToBGR( aColor ) );
    }
}

void WW8AttributeOutput::CharLanguage( const SvxLanguageItem& rLanguage )
{
    sal_uInt16 nId = 0;
    switch ( rLanguage.Which() )
    {
        case RES_CHRATR_LANGUAGE:
            nId = NS_sprm::CRgLid0_80::val;
            break;
        case RES_CHRATR_CJK_LANGUAGE:
            nId = NS_sprm::CRgLid1_80::val;
            break;
        case RES_CHRATR_CTL_LANGUAGE:
            nId = NS_sprm::CLidBi::val;
            break;
    }

    if ( !nId )
        return;

    // use sprmCRgLid0_80 rather than sprmCLid
    m_rWW8Export.InsUInt16( nId );
    m_rWW8Export.InsUInt16( static_cast<sal_uInt16>(rLanguage.GetLanguage()) );

    // Word 2000 and above apparently require both old and new versions of
    // these sprms to be set, without it spellchecking doesn't work
    if ( nId == NS_sprm::CRgLid0_80::val )
    {
        m_rWW8Export.InsUInt16( NS_sprm::CRgLid0::val );
        m_rWW8Export.InsUInt16( static_cast<sal_uInt16>(rLanguage.GetLanguage()) );
    }
    else if ( nId == NS_sprm::CRgLid1_80::val )
    {
        m_rWW8Export.InsUInt16( NS_sprm::CRgLid1::val );
        m_rWW8Export.InsUInt16( static_cast<sal_uInt16>(rLanguage.GetLanguage()) );
    }
}

void WW8AttributeOutput::CharEscapement( const SvxEscapementItem& rEscapement )
{
    sal_uInt8 b = 0xFF;
    short nEsc = rEscapement.GetEsc(), nProp = rEscapement.GetProportionalHeight();
    if ( !nEsc )
    {
        b = 0;
        nEsc = 0;
        nProp = 100;
    }
    else if ( DFLT_ESC_PROP == nProp || nProp < 1 || nProp > 100 )
    {
        if ( DFLT_ESC_SUB == nEsc || DFLT_ESC_AUTO_SUB == nEsc )
            b = 2;
        else if ( DFLT_ESC_SUPER == nEsc || DFLT_ESC_AUTO_SUPER == nEsc )
            b = 1;
    }
    else if ( DFLT_ESC_AUTO_SUPER == nEsc )
    {
        // Raised by the differences between the ascenders (ascent = baseline to top of highest letter).
        // The ascent is generally about 80% of the total font height.
        // That is why DFLT_ESC_PROP (58) leads to 33% (DFLT_ESC_SUPER)
        nEsc = .8 * (100 - nProp);
    }
    else if ( DFLT_ESC_AUTO_SUB == nEsc )
    {
        // Lowered by the differences between the descenders (descent = baseline to bottom of lowest letter).
        // The descent is generally about 20% of the total font height.
        // That is why DFLT_ESC_PROP (58) leads to 8% (DFLT_ESC_SUB)
        nEsc = .2 * -(100 - nProp);
    }

    if ( 0xFF != b )
    {
        m_rWW8Export.InsUInt16( NS_sprm::CIss::val );

        m_rWW8Export.m_pO->push_back( b );
    }

    if ( 0 != b && 0xFF != b )
        return;

    double fHeight = m_rWW8Export.GetItem( RES_CHRATR_FONTSIZE ).GetHeight();
    m_rWW8Export.InsUInt16( NS_sprm::CHpsPos::val );

    m_rWW8Export.InsUInt16(static_cast<short>( round(fHeight * nEsc / 1000) ));

    if( 100 != nProp || !b )
    {
        m_rWW8Export.InsUInt16( NS_sprm::CHps::val );
        m_rWW8Export.InsUInt16(msword_cast<sal_uInt16>( round(fHeight * nProp / 1000) ));
    }
}

void WW8AttributeOutput::CharFontSize( const SvxFontHeightItem& rHeight )
{
    sal_uInt16 nId = 0;
    switch ( rHeight.Which() )
    {
        case RES_CHRATR_FONTSIZE:
        case RES_CHRATR_CJK_FONTSIZE:
            nId = NS_sprm::CHps::val;
            break;
        case RES_CHRATR_CTL_FONTSIZE:
            nId = NS_sprm::CHpsBi::val;
            break;
    }

    if ( nId )
    {
        m_rWW8Export.InsUInt16( nId );

        m_rWW8Export.InsUInt16( ( rHeight.GetHeight() + 5 ) / 10 );
    }
}

void WW8AttributeOutput::CharScaleWidth( const SvxCharScaleWidthItem& rScaleWidth )
{
    m_rWW8Export.InsUInt16( NS_sprm::CCharScale::val );
    m_rWW8Export.InsUInt16( rScaleWidth.GetValue() );
}

void WW8AttributeOutput::CharRelief( const SvxCharReliefItem& rRelief )
{
    sal_uInt16 nId;
    switch ( rRelief.GetValue() )
    {
        case FontRelief::Embossed:   nId = NS_sprm::CFEmboss::val;     break;
        case FontRelief::Engraved:   nId = NS_sprm::CFImprint::val;    break;
        default:                nId = 0;                        break;
    }

    if( nId )
    {
        m_rWW8Export.InsUInt16( nId );
        m_rWW8Export.m_pO->push_back( sal_uInt8(0x81) );
    }
    else
    {
        // switch both flags off
        m_rWW8Export.InsUInt16( NS_sprm::CFEmboss::val );
        m_rWW8Export.m_pO->push_back( sal_uInt8(0x0) );
        m_rWW8Export.InsUInt16( NS_sprm::CFImprint::val );
        m_rWW8Export.m_pO->push_back( sal_uInt8(0x0) );
    }
}

void WW8AttributeOutput::CharBidiRTL( const SfxPoolItem& rHt )
{
    const SfxInt16Item& rAttr = static_cast<const SfxInt16Item&>(rHt);
    if( rAttr.GetValue() == 1 )
    {
        m_rWW8Export.InsUInt16(0x85a);
        m_rWW8Export.m_pO->push_back(sal_uInt8(1));
    }
}

void WW8AttributeOutput::CharRotate( const SvxCharRotateItem& rRotate )
{
    // #i28331# - check that a Value is set
    if ( !rRotate.GetValue() )
        return;

    if (m_rWW8Export.IsInTable())
        return;

    // #i36867 In word the text in a table is rotated via the TC or NS_sprm::TTextFlow::val
    // This means you can only rotate all or none of the text adding NS_sprm::CFELayout::val
    // here corrupts the table, hence !m_rWW8Export.bIsInTable

    m_rWW8Export.InsUInt16( NS_sprm::CFELayout::val );
    m_rWW8Export.m_pO->push_back( sal_uInt8(0x06) ); //len 6
    m_rWW8Export.m_pO->push_back( sal_uInt8(0x01) );

    m_rWW8Export.InsUInt16( rRotate.IsFitToLine() ? 1 : 0 );
    static const sal_uInt8 aZeroArr[ 3 ] = { 0, 0, 0 };
    m_rWW8Export.m_pO->insert( m_rWW8Export.m_pO->end(), aZeroArr, aZeroArr+3);
}

void WW8AttributeOutput::CharEmphasisMark( const SvxEmphasisMarkItem& rEmphasisMark )
{
    sal_uInt8 nVal;
    const FontEmphasisMark v = rEmphasisMark.GetEmphasisMark();
    if (v == FontEmphasisMark::NONE)
        nVal = 0;
    else if (v == (FontEmphasisMark::Accent | FontEmphasisMark::PosAbove))
        nVal = 2;
    else if (v == (FontEmphasisMark::Circle | FontEmphasisMark::PosAbove))
        nVal = 3;
    else if (v == (FontEmphasisMark::Dot | FontEmphasisMark::PosBelow))
        nVal = 4;
    else
        // case 1:
        nVal = 1;

    m_rWW8Export.InsUInt16( NS_sprm::CKcd::val );
    m_rWW8Export.m_pO->push_back( nVal );
}

/**
 * TransBrush converts SW-Brushes to WW. The result is WW8_SHD.
 * Non-standard colours of SW won't be converted now to the mixed values
 * ( 0 .. 95% ) of WW.
 * Also if transparent, e.g. for tables a transparent brush is returned
 *
 * @return real brush ( not transparent )
 */
bool WW8Export::TransBrush(const Color& rCol, WW8_SHD& rShd)
{
    if( rCol.IsTransparent() )
        rShd = WW8_SHD();               // all zeros: transparent
    else
    {
        rShd.SetFore( 0);
        rShd.SetBack( msfilter::util::TransColToIco( rCol ) );
        rShd.SetStyle( 0 );
    }
    return !rCol.IsTransparent();
}

static sal_uInt32 SuitableBGColor(Color nIn)
{
    if (nIn == COL_AUTO)
        return 0xFF000000;
    return wwUtility::RGBToBGR(nIn);
}

void WW8AttributeOutput::CharColor( const SvxColorItem& rColor )
{
    m_rWW8Export.InsUInt16( NS_sprm::CIco::val );

    sal_uInt8 nColor = msfilter::util::TransColToIco( rColor.GetValue() );
    m_rWW8Export.m_pO->push_back( nColor );

    if (nColor)
    {
        m_rWW8Export.InsUInt16( NS_sprm::CCv::val );
        m_rWW8Export.InsUInt32( wwUtility::RGBToBGR( rColor.GetValue() ) );
    }
}

void WW8AttributeOutput::CharBackground( const SvxBrushItem& rBrush )
{
    WW8_SHD aSHD;

    WW8Export::TransBrush( rBrush.GetColor(), aSHD );
    // sprmCShd80
    m_rWW8Export.InsUInt16( NS_sprm::CShd80::val );
    m_rWW8Export.InsUInt16( aSHD.GetValue() );

    //Quite a few unknowns, some might be transparency or something
    //of that nature...
    m_rWW8Export.InsUInt16( NS_sprm::CShd::val );
    m_rWW8Export.m_pO->push_back( 10 );
    m_rWW8Export.InsUInt32( 0xFF000000 );
    m_rWW8Export.InsUInt32( SuitableBGColor( rBrush.GetColor() ) );
    m_rWW8Export.InsUInt16( 0x0000);
}

namespace sw { namespace util {

const SwCharFormat* GetSwCharFormat(const SwFormatINetFormat& rINet, SwDoc& rDoc)
{
    if (rINet.GetValue().isEmpty())
        return nullptr;

    const sal_uInt16 nId = rINet.GetINetFormatId();
    const UIName& rStr = rINet.GetINetFormat();
    if (rStr.isEmpty())
    {
        OSL_ENSURE( false, "WW8AttributeOutput::TextINetFormat(..) - missing unvisited character format at hyperlink attribute" );
    }

    return IsPoolUserFormat( nId )
               ? rDoc.FindCharFormatByName( rStr )
               : rDoc.getIDocumentStylePoolAccess().GetCharFormatFromPool( nId );
}

} }

void WW8AttributeOutput::TextINetFormat( const SwFormatINetFormat& rINet )
{
    const SwCharFormat* pFormat = GetSwCharFormat(rINet, m_rWW8Export.m_rDoc);
    if (!pFormat)
        return;

    m_rWW8Export.InsUInt16( NS_sprm::CIstd::val );

    m_rWW8Export.InsUInt16( m_rWW8Export.GetId( pFormat ) );
}

// #i43956# - add optional parameter <pLinkStr>
// It's needed to write the hyperlink data for a certain cross-reference
// - it contains the name of the link target, which is a bookmark.
// add optional parameter <bIncludeEmptyPicLocation>
// It is needed to write an empty picture location for page number field separators
static void InsertSpecialChar( WW8Export& rWrt, sal_uInt8 c,
                               OUString const * pLinkStr,
                               bool bIncludeEmptyPicLocation = false )
{
    ww::bytes aItems;
    rWrt.GetCurrentItems(aItems);

    if (c == 0x13)
        rWrt.m_pChpPlc->AppendFkpEntry(rWrt.Strm().Tell());
    else
        rWrt.m_pChpPlc->AppendFkpEntry(rWrt.Strm().Tell(), aItems.size(), aItems.data());

    rWrt.WriteChar(c);

    // store empty sprmCPicLocation for field separator
    if ( bIncludeEmptyPicLocation &&
         ( c == 0x13 || c == 0x14 || c == 0x15 ) )
    {
        SwWW8Writer::InsUInt16( aItems, NS_sprm::CPicLocation::val );
        SwWW8Writer::InsUInt32( aItems, 0x00000000 );
    }

    // #i43956# - write hyperlink data and attributes
    if ( c == 0x01 && pLinkStr)
    {
        // write hyperlink data to data stream
        SvStream& rStrm = *rWrt.m_pDataStrm;
        // position of hyperlink data
        const sal_uInt64 nLinkPosInDataStrm = rStrm.Tell();
        // write empty header
        const sal_uInt16 nEmptyHdrLen = 0x44;
        sal_uInt8 aEmptyHeader[ nEmptyHdrLen ] = { 0 };
        aEmptyHeader[ 4 ] = 0x44;
        rStrm.WriteBytes( aEmptyHeader, nEmptyHdrLen );
        // writer fixed header
        const sal_uInt16 nFixHdrLen = 0x19;
        sal_uInt8 const aFixHeader[ nFixHdrLen ] =
        {
            0x08, 0xD0, 0xC9, 0xEA, 0x79, 0xF9, 0xBA, 0xCE,
            0x11, 0x8C, 0x82, 0x00, 0xAA, 0x00, 0x4B, 0xA9,
            0x0B, 0x02, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
            0x00,
        };
        rStrm.WriteBytes( aFixHeader, nFixHdrLen );
        // write reference string including length+1
        sal_uInt32 nStrLen( pLinkStr->getLength() + 1 );
        SwWW8Writer::WriteLong( rStrm, nStrLen );
        SwWW8Writer::WriteString16( rStrm, *pLinkStr, false );
        // write additional two NULL Bytes
        SwWW8Writer::WriteLong( rStrm, 0 );
        // write length of hyperlink data
        const sal_uInt64 nCurrPos = rStrm.Tell();
        rStrm.Seek( nLinkPosInDataStrm );
        rStrm.WriteUInt32(nCurrPos - nLinkPosInDataStrm);
        rStrm.Seek( nCurrPos );

        // write attributes of hyperlink character 0x01
        SwWW8Writer::InsUInt16( aItems, NS_sprm::CFFldVanish::val );
        aItems.push_back( sal_uInt8(0x81) );
        SwWW8Writer::InsUInt16( aItems, NS_sprm::CPicLocation::val );
        SwWW8Writer::InsUInt32( aItems, nLinkPosInDataStrm );
        SwWW8Writer::InsUInt16( aItems, NS_sprm::CFData::val );
        aItems.push_back( sal_uInt8(0x01) );
    }

    //Technically we should probably Remove all attribs
    //here for the 0x13, 0x14, 0x15, but our import
    //is slightly lacking
    //aItems.Remove(0, aItems.Count());
    // fSpec-Attribute true
    SwWW8Writer::InsUInt16( aItems, NS_sprm::CFSpec::val );
    aItems.push_back( 1 );

    rWrt.m_pChpPlc->AppendFkpEntry(rWrt.Strm().Tell(), aItems.size(), aItems.data());
}

static OUString lcl_GetExpandedField(const SwField &rField)
{
    //replace LF 0x0A with VT 0x0B
    return rField.ExpandField(true, nullptr).replace(0x0A, 0x0B);
}

WW8_WrPlcField* WW8Export::CurrentFieldPlc() const
{
    WW8_WrPlcField* pFieldP = nullptr;
    switch (m_nTextTyp)
    {
        case TXT_MAINTEXT:
            pFieldP = m_pFieldMain.get();
            break;
        case TXT_HDFT:
            pFieldP = m_pFieldHdFt.get();
            break;
        case TXT_FTN:
            pFieldP = m_pFieldFootnote.get();
            break;
        case TXT_EDN:
            pFieldP = m_pFieldEdn.get();
            break;
        case TXT_ATN:
            pFieldP = m_pFieldAtn.get();
            break;
        case TXT_TXTBOX:
            pFieldP = m_pFieldTextBxs.get();
            break;
        case TXT_HFTXTBOX:
            pFieldP = m_pFieldHFTextBxs.get();
            break;
        default:
            OSL_ENSURE( false, "what type of SubDoc is that?" );
    }
    return pFieldP;
}

void WW8Export::OutputField( const SwField* pField, ww::eField eFieldType,
    const OUString& rFieldCmd, FieldFlags nMode )
{
    OUString sFieldCmd(rFieldCmd);
    switch (eFieldType)
    {
        // map fields that are not supported in WW8 as of Word 2003
        case ww::eBIBLIOGRAPHY:
            eFieldType = ww::eQUOTE;
            assert(rFieldCmd == FieldString(ww::eBIBLIOGRAPHY));
            sFieldCmd = FieldString(ww::eQUOTE);
            break;
        case ww::eCITATION:
            eFieldType = ww::eQUOTE;
            assert(o3tl::starts_with(o3tl::trim(rFieldCmd), u"CITATION"));
            sFieldCmd = rFieldCmd.replaceFirst(FieldString(ww::eCITATION),
                                               FieldString(ww::eQUOTE));
            break;
        default:
            break;
    }

    assert(eFieldType <= 0x5F); // 95 is the highest documented one

    WW8_WrPlcField* pFieldP = CurrentFieldPlc();

    const bool bIncludeEmptyPicLocation = ( eFieldType == ww::ePAGE );
    if (FieldFlags::Start & nMode)
    {
        sal_uInt8 aField13[2] = { 0x13, 0x00 };  // will change
        //#i3958#, Needed to make this field work correctly in Word 2000
        if (eFieldType == ww::eSHAPE)
            aField13[0] |= 0x80;
        aField13[1] = static_cast< sal_uInt8 >(eFieldType);  // add type
        pFieldP->Append( Fc2Cp( Strm().Tell() ), aField13 );
        InsertSpecialChar( *this, 0x13, nullptr, bIncludeEmptyPicLocation );
    }
    if (FieldFlags::CmdStart & nMode)
    {
        SwWW8Writer::WriteString16(Strm(), sFieldCmd, false);
        // #i43956# - write hyperlink character including
        // attributes and corresponding binary data for certain reference fields.
        bool bHandleBookmark = false;

        if (pField)
        {
            if (pField->GetTyp()->Which() == SwFieldIds::GetRef &&
                ( eFieldType == ww::ePAGEREF || eFieldType == ww::eREF ||
                  eFieldType == ww::eNOTEREF || eFieldType == ww::eFOOTREF ))
                bHandleBookmark = true;
        }

        if ( bHandleBookmark )
        {
            // retrieve reference destination - the name of the bookmark
            OUString aLinkStr;
            const SwGetRefField& rRField = *static_cast<const SwGetRefField*>(pField);
            const ReferencesSubtype nSubType = rRField.GetSubType();
            if ( nSubType == ReferencesSubtype::SetRefAttr ||
                 nSubType == ReferencesSubtype::Bookmark )
            {
                const SwMarkName& aRefName(rRField.GetSetRefName());
                aLinkStr = GetBookmarkName( nSubType, &aRefName.toString(), 0 );
            }
            else if ( nSubType == ReferencesSubtype::Footnote ||
                      nSubType == ReferencesSubtype::Endnote )
            {
                aLinkStr = GetBookmarkName( nSubType, nullptr, rRField.GetSeqNo() );
            }
            else if ( nSubType == ReferencesSubtype::SequenceField )
            {
                aLinkStr = pField->GetPar2();
            }
            // insert hyperlink character including attributes and data.
            InsertSpecialChar( *this, 0x01, &aLinkStr );
        }
    }
    if (FieldFlags::CmdEnd & nMode)
    {
        static const sal_uInt8 aField14[2] = { 0x14, 0xff };
        pFieldP->Append( Fc2Cp( Strm().Tell() ), aField14 );
        pFieldP->ResultAdded();
        InsertSpecialChar( *this, 0x14, nullptr, bIncludeEmptyPicLocation );
    }
    if (FieldFlags::End & nMode)
    {
        OUString sOut;
        if( pField )
            sOut = lcl_GetExpandedField(*pField);
        else
            sOut = sFieldCmd;
        if( !sOut.isEmpty() )
        {
            SwWW8Writer::WriteString16(Strm(), sOut, false);

            if (pField)
            {
                if (pField->GetTyp()->Which() == SwFieldIds::Input &&
                    eFieldType == ww::eFORMTEXT)
                {
                    sal_uInt8 aArr[12];
                    sal_uInt8 *pArr = aArr;

                    Set_UInt16( pArr, NS_sprm::CPicLocation::val );
                    Set_UInt32( pArr, 0x0 );

                    Set_UInt16( pArr, NS_sprm::CFSpec::val );
                    Set_UInt8( pArr, 1 );

                    Set_UInt16( pArr, NS_sprm::CFNoProof::val );
                    Set_UInt8( pArr, 1 );

                    m_pChpPlc->AppendFkpEntry( Strm().Tell(), static_cast< short >(pArr - aArr), aArr );
                }
            }
        }
    }
    if (!(FieldFlags::Close & nMode))
        return;

    sal_uInt8 aField15[2] = { 0x15, 0x80 };

    if (pField)
    {
        if (pField->GetTyp()->Which() == SwFieldIds::Input &&
            eFieldType == ww::eFORMTEXT)
        {
            SwInputFieldSubType nSubType = static_cast<const SwInputField*>(pField)->GetSubType();

            if (nSubType == SwInputFieldSubType::Text)
                aField15[0] |= (0x4 << 5);
        }
        // This ought to apply to any field, but just to be safe, start off with DATE/TIME only.
        if (pField->GetTyp()->Which() == SwFieldIds::DateTime)
        {
            SwDateTimeSubType nSubType = static_cast<const SwDateTimeField*>(pField)->GetSubType();
            if (nSubType & SwDateTimeSubType::Fixed)
                //bit 5 - Locked: do not recalculate field
                aField15[1] |= 0x10;
        }
    }

    pFieldP->Append( Fc2Cp( Strm().Tell() ), aField15 );
    InsertSpecialChar( *this, 0x15, nullptr, bIncludeEmptyPicLocation );
}

void WW8Export::StartCommentOutput(std::u16string_view rName)
{
    const OUString sStr{ FieldString(ww::eQUOTE) + "[" + rName + "] " };
    OutputField(nullptr, ww::eQUOTE, sStr, FieldFlags::Start | FieldFlags::CmdStart);
}

void WW8Export::EndCommentOutput(std::u16string_view rName)
{
    const OUString sStr{ OUString::Concat(" [") + rName + "] " };
    OutputField(nullptr, ww::eQUOTE, sStr, FieldFlags::CmdEnd | FieldFlags::End |
        FieldFlags::Close);
}

sal_uInt16 MSWordExportBase::GetId( const SwTOXType& rTOXType )
{
    std::vector<const SwTOXType*>::iterator it
        = std::find( m_aTOXArr.begin(), m_aTOXArr.end(), &rTOXType );
    if ( it != m_aTOXArr.end() )
    {
        return it - m_aTOXArr.begin();
    }
    m_aTOXArr.push_back( &rTOXType );
    return m_aTOXArr.size() - 1;
}

// return values:  1 - no PageNum,
//                 2 - TabStop before PageNum,
//                 3 - Text before PageNum - rText hold the text
//                 4 - no Text and no TabStop before PageNum
static int lcl_CheckForm( const SwForm& rForm, sal_uInt8 nLvl, OUString& rText )
{
    int nRet = 4;
    rText.clear();

    // #i21237#
    SwFormTokens aPattern = rForm.GetPattern(nLvl);
    SwFormTokens::iterator aIt = aPattern.begin();
    FormTokenType eTType;

    // #i61362#
    if (! aPattern.empty())
    {
        bool bPgNumFnd = false;

        // #i21237#
        while( ++aIt != aPattern.end() && !bPgNumFnd )
        {
            eTType = aIt->eTokenType;

            switch( eTType )
            {
            case TOKEN_PAGE_NUMS:
                bPgNumFnd = true;
                break;

            case TOKEN_TAB_STOP:
                nRet = 2;
                break;
            case TOKEN_TEXT:
            {
                nRet = 3;
                sal_Int32 nCount = std::min<sal_Int32>(5, aIt->sText.getLength());
                rText = aIt->sText.copy(0, nCount); // #i21237#
                break;
            }
            case TOKEN_LINK_START:
            case TOKEN_LINK_END:
                break;

            default:
                nRet = 4;
                break;
            }
        }

        if( !bPgNumFnd )
            nRet = 1;
    }

    return nRet;
}

static bool lcl_IsHyperlinked(const SwForm& rForm, sal_uInt16 nTOXLvl)
{
    bool bRes = false;
    for (sal_uInt16 nI = 1; nI <= nTOXLvl; ++nI)
    {
        // #i21237#
        SwFormTokens aPattern = rForm.GetPattern(nI);

        if ( !aPattern.empty() )
        {
            SwFormTokens::iterator aIt = aPattern.begin();

            FormTokenType eTType;

            // #i21237#
            while ( ++aIt != aPattern.end() )
            {
                eTType = aIt->eTokenType;
                switch (eTType)
                {
                case TOKEN_LINK_START:
                case TOKEN_LINK_END:
                    bRes = true;
                break;
                default:
                    ;
                }
            }
        }
    }
    return bRes;
}

void AttributeOutputBase::GenerateBookmarksForSequenceField(const SwTextNode& rNode, SwWW8AttrIter& rAttrIter)
{
    if(GetExport().GetExportFormat() == MSWordExportBase::ExportFormat::RTF) // Not implemented for RTF
        return;

    const SwpHints* pTextAttrs = rNode.GetpSwpHints();
    if (!pTextAttrs)
        return;

    for( size_t i = 0; i < pTextAttrs->Count(); ++i )
    {
        const SwTextAttr* pHt = pTextAttrs->Get(i);
        if (pHt->GetAttr().Which() == RES_TXTATR_FIELD)
        {
            const SwFormatField& rField = static_cast<const SwFormatField&>(pHt->GetAttr());
            const SwField* pField = rField.GetField();
            // Need to have bookmarks only for sequence fields
            if (pField && pField->GetTyp()->Which() == SwFieldIds::SetExp && static_cast<const SwSetExpField*>(pField)->GetSubType() == SwGetSetExpType::Sequence)
            {
                const sal_uInt16 nSeqFieldNumber = static_cast<const SwSetExpField*>(pField)->GetSeqNumber();
                const UIName sObjectName = static_cast<const SwSetExpFieldType*>(pField->GetTyp())->GetName();
                const SwFieldTypes* pFieldTypes = GetExport().m_rDoc.getIDocumentFieldsAccess().GetFieldTypes();
                bool bHaveFullBkm = false;
                bool bHaveLabelAndNumberBkm = false;
                bool bHaveCaptionOnlyBkm = false;
                bool bHaveNumberOnlyBkm = false;
                bool bRunSplittedAtSep = false;
                for( auto const & pFieldType : *pFieldTypes )
                {
                    if( SwFieldIds::GetRef == pFieldType->Which() )
                    {
                        SwIterator<SwFormatField,SwFieldType> aIter( *pFieldType );
                        for( SwFormatField* pFormatField = aIter.First(); pFormatField; pFormatField = aIter.Next() )
                        {
                            SwGetRefField* pRefField = static_cast<SwGetRefField*>(pFormatField->GetField());
                            // If we have a reference to the current sequence field
                            if(pRefField->GetSeqNo() == nSeqFieldNumber && pRefField->GetSetRefName() == SwMarkName(sObjectName.toString()))
                            {
                                // Need to create a separate run for separator character
                                SwWW8AttrIter aLocalAttrIter( GetExport(), rNode ); // We need a local iterator having the right number of runs
                                const OUString& aText = rNode.GetText();
                                const sal_Int32 nCategoryStart = aText.indexOf(pRefField->GetSetRefName().toString());
                                const sal_Int32 nPosBeforeSeparator = std::max(nCategoryStart, pHt->GetStart());
                                bool bCategoryFirst = nCategoryStart < pHt->GetStart();
                                sal_Int32 nSeparatorPos = 0;
                                if (bCategoryFirst)
                                {
                                    nSeparatorPos = aLocalAttrIter.WhereNext();
                                    while (nSeparatorPos <= nPosBeforeSeparator)
                                    {
                                        aLocalAttrIter.NextPos();
                                        nSeparatorPos = aLocalAttrIter.WhereNext();
                                    }
                                }
                                else
                                {
                                    nSeparatorPos = nCategoryStart + pRefField->GetSetRefName().toString().getLength();
                                }
                                sal_Int32 nRefTextPos = 0;
                                if(nSeparatorPos < aText.getLength())
                                {
                                    nRefTextPos = SwGetExpField::GetReferenceTextPos(pHt->GetFormatField(), GetExport().m_rDoc, nSeparatorPos);
                                    if(nRefTextPos != nSeparatorPos)
                                    {
                                        if(!bRunSplittedAtSep)
                                        {
                                            if(!bCategoryFirst)
                                                rAttrIter.SplitRun(nSeparatorPos);
                                            rAttrIter.SplitRun(nRefTextPos);
                                            bRunSplittedAtSep = true;
                                        }
                                        if(!bCategoryFirst)
                                            aLocalAttrIter.SplitRun(nSeparatorPos);
                                        aLocalAttrIter.SplitRun(nRefTextPos);
                                    }
                                    else if (bCategoryFirst)
                                    {
                                        if(!bRunSplittedAtSep)
                                        {
                                            rAttrIter.SplitRun(nSeparatorPos);
                                            bRunSplittedAtSep = true;
                                        }
                                        aLocalAttrIter.SplitRun(nSeparatorPos);
                                    }
                                }
                                // Generate bookmarks on the right position
                                OUString sName("Ref_" + pRefField->GetSetRefName().toString() + OUString::number(pRefField->GetSeqNo()));
                                switch (pRefField->GetFormat())
                                {
                                    case RefFieldFormat::Page:
                                    case RefFieldFormat::AsPageStyle:
                                    case RefFieldFormat::Content:
                                    case RefFieldFormat::UpDown:
                                        if(!bHaveFullBkm)
                                        {
                                            sal_Int32 nLastAttrStart = 0;
                                            sal_Int32 nActAttr = aLocalAttrIter.WhereNext();
                                            while (nActAttr < rNode.GetText().getLength())
                                            {
                                                nLastAttrStart = nActAttr;
                                                aLocalAttrIter.NextPos();
                                                nActAttr = aLocalAttrIter.WhereNext();
                                            }
                                            WriteBookmarkInActParagraph( sName + "_full", std::min(nCategoryStart, pHt->GetStart()), nLastAttrStart );
                                            bHaveFullBkm = true;
                                        }
                                        break;
                                    case RefFieldFormat::CategoryAndNumber:
                                    {
                                        if(!bHaveLabelAndNumberBkm)
                                        {
                                            sName += "_label_and_number";
                                            if(bCategoryFirst)
                                                WriteBookmarkInActParagraph( sName, std::min(nCategoryStart, pHt->GetStart()), std::max(nCategoryStart, pHt->GetStart()) );
                                            else
                                            {
                                                // Find the last run which contains category text
                                                SwWW8AttrIter aLocalAttrIter2( GetExport(), rNode );
                                                sal_Int32 nCatLastRun = 0;
                                                sal_Int32 nNextAttr = aLocalAttrIter2.WhereNext();
                                                while (nNextAttr < nSeparatorPos)
                                                {
                                                    nCatLastRun = nNextAttr;
                                                    aLocalAttrIter2.NextPos();
                                                    nNextAttr = aLocalAttrIter2.WhereNext();
                                                }
                                                WriteBookmarkInActParagraph( sName, pHt->GetStart(), nCatLastRun );
                                            }
                                            bHaveLabelAndNumberBkm = true;
                                        }
                                        break;
                                    }
                                    case RefFieldFormat::CaptionText:
                                    {
                                        if(!bHaveCaptionOnlyBkm)
                                        {
                                            // Find last run
                                            sal_Int32 nLastAttrStart = 0;
                                            sal_Int32 nActAttr = aLocalAttrIter.WhereNext();
                                            while (nActAttr < rNode.GetText().getLength())
                                            {
                                                nLastAttrStart = nActAttr;
                                                aLocalAttrIter.NextPos();
                                                nActAttr = aLocalAttrIter.WhereNext();
                                            }
                                            WriteBookmarkInActParagraph( sName + "_caption_only", nRefTextPos, nLastAttrStart );
                                            bHaveCaptionOnlyBkm = true;
                                        }
                                        break;
                                    }
                                    case RefFieldFormat::Numbering:
                                    {
                                        if(!bHaveNumberOnlyBkm)
                                        {
                                            WriteBookmarkInActParagraph( sName + "_number_only", pHt->GetStart(), pHt->GetStart() );
                                            bHaveNumberOnlyBkm = true;
                                        }
                                        break;
                                    }
                                    default: break;
                                }
                            }
                        }
                    }
                }
                return;
            }
        }
    }
}

static auto GetSeparatorForLocale() -> OUString
{
    switch (sal_uInt16(MsLangId::getSystemLanguage()))
    {
        case sal_uInt16(LANGUAGE_GERMAN):
        case sal_uInt16(LANGUAGE_GERMAN_AUSTRIAN):
        case sal_uInt16(LANGUAGE_GERMAN_LIECHTENSTEIN):
        case sal_uInt16(LANGUAGE_GERMAN_LUXEMBOURG):
        case sal_uInt16(LANGUAGE_GERMAN_SWISS):
            return u";"_ustr;
        default:
            return u","_ustr;
    }
}

void AttributeOutputBase::StartTOX( const SwSection& rSect )
{
    if ( const SwTOXBase* pTOX = rSect.GetTOXBase() )
    {
        static const char sEntryEnd[] = "\" ";

        ww::eField eCode = ww::eTOC;
        OUString sStr = pTOX ->GetMSTOCExpression();
        if ( sStr.isEmpty() )
        {
            OUString sUserTypeName;
            auto aType = pTOX->GetType();
            // user index, it needs INDEX with \f
            if ( TOX_USER == aType )
            {
                 sUserTypeName = pTOX->GetTOXType()->GetTypeName();
                 if ( !sUserTypeName.isEmpty() )
                      aType = TOX_INDEX;
            }
            switch (aType)
            {
            case TOX_INDEX:
                eCode = ww::eINDEX;
                sStr = FieldString(eCode);

                {
                    const SwFormatCol& rCol = rSect.GetFormat()->GetFormatAttr( RES_COL );
                    const SwColumns& rColumns = rCol.GetColumns();
                    sal_Int32 nCol = rColumns.size();

                    if ( 0 < nCol )
                    {
                        // Add a continuous section break
                        if( GetExport().AddSectionBreaksForTOX() )
                        {
                            SwSection *pParent = rSect.GetParent();
                            WW8_SepInfo rInfo(&GetExport().m_rDoc.GetPageDesc(0),
                                pParent ? pParent->GetFormat() : nullptr, 0/*nRstLnNum*/);
                            GetExport( ).AttrOutput().SectionBreak( msword::PageBreak, false, &rInfo );
                        }

                        sStr += "\\c \"" + OUString::number( nCol ) + "\"";
                    }
                }

                if (pTOX->GetTOXForm().IsCommaSeparated())
                    sStr += "\\r ";

                if (SwTOIOptions::AlphaDelimiter & pTOX->GetOptions())
                    sStr += "\\h \"A\" ";

                if (!sUserTypeName.isEmpty())
                {
                    sStr += "\\f \"" + sUserTypeName + "\"";
                }

                if (!pTOX->GetTOXForm().IsCommaSeparated())
                {
                    // In case of Run-in style no separators are added.
                    OUString aFillText;
                    for (sal_uInt8 n = 1; n <= 3; ++n)
                    {
                        OUString aText;
                        int nRet = ::lcl_CheckForm(pTOX->GetTOXForm(), n, aText);

                        if( 3 == nRet )
                            aFillText = aText;
                        else if ((4 == nRet) || (2 == nRet))
                            aFillText = "\t";
                        else
                            aFillText.clear();
                    }
                    sStr += "\\e \"" + aFillText + sEntryEnd;
                }
                break;

            case TOX_ILLUSTRATIONS:
            case TOX_OBJECTS:
            case TOX_TABLES:
                if (!pTOX->IsFromObjectNames())
                {
                    sStr = FieldString(eCode) + "\\c ";
                    const UIName& seqName = pTOX->GetSequenceName();
                    if(!seqName.isEmpty())
                    {
                        sStr += "\"" + seqName.toString() + sEntryEnd;
                    }
                    OUString aText;
                    int nRet = ::lcl_CheckForm( pTOX->GetTOXForm(), 1, aText );
                    if (1 == nRet)
                        sStr += "\\n ";
                    else if( 3 == nRet || 4 == nRet )
                    {
                        sStr += "\\p \"" + aText + sEntryEnd;
                    }
                }
                if (lcl_IsHyperlinked(pTOX->GetTOXForm(), 1))
                {
                    sStr += "\\h ";
                }
                if (pTOX->GetCreateType() & SwTOXElement::Template)
                {
                    UIName const& rStyle(pTOX->GetStyleNames(0));
                    assert(rStyle.toString().indexOf(TOX_STYLE_DELIMITER) == -1);
                    SwTextFormatColl const*const pColl = GetExport().m_rDoc.FindTextFormatCollByName(rStyle);
                    if (pColl)
                    {
                        OUString const converted(GetExport().m_pStyles->GetStyleWWName(pColl));
                        if (!converted.isEmpty())
                        {
                            sStr += "\\t \"" + converted + sEntryEnd;
                        }
                    }
                }
                break;

            case TOX_AUTHORITIES:
                eCode = ww::eBIBLIOGRAPHY;
                sStr = FieldString(eCode);
                 break;
                //      case TOX_USER:
                //      case TOX_CONTENT:
            default:
                {
                    sStr = FieldString(eCode);

                    OUString sTOption;
                    // tdf#153082 Word's separator interpretation in DOCX
                    // fields varies by system locale.
                    auto const tsep(GetSeparatorForLocale());
                    sal_uInt16 n, nTOXLvl = pTOX->GetLevel();
                    if( !nTOXLvl )
                        ++nTOXLvl;

                    if(SwTOXElement::TableLeader & pTOX->GetCreateType())
                    {
                        sStr +="\\z " ;
                        GetExport( ).m_bHideTabLeaderAndPageNumbers = true ;
                    }
                    if(SwTOXElement::TableInToc & pTOX->GetCreateType())
                    {
                        sStr +="\\w " ;
                        GetExport( ).m_bTabInTOC = true ;
                    }
                    if(SwTOXElement::Newline & pTOX->GetCreateType())
                    {
                        sStr +="\\x " ;
                    }
                    if( SwTOXElement::Mark & pTOX->GetCreateType() )
                    {
                        sStr += "\\f ";

                        if( TOX_USER == pTOX->GetType() )
                        {
                            sStr += "\""
                                + OUStringChar(static_cast<char>( 'A' + GetExport( ).GetId( *pTOX->GetTOXType() ) ))
                                + sEntryEnd;
                        }
                    }
                    if(SwTOXElement::Bookmark & pTOX->GetCreateType())
                    {
                        sStr += "\\b \"" + pTOX->GetBookmarkName() + sEntryEnd;
                    }

                    if( SwTOXElement::OutlineLevel & pTOX->GetCreateType() )
                    {
                        // Take the TOC value of the max level to evaluate to as
                        // the starting point for the \o flag, but reduce it to the
                        // value of the highest outline level filled by a *standard*
                        // Heading 1 - 9 style because \o "Builds a table of
                        // contents from paragraphs formatted with built-in heading
                        // styles". And afterward fill in any outline styles left
                        // uncovered by that range to the \t flag

                        // i.e. for
                        // Heading 1
                        // Heading 2
                        // custom-style
                        // Heading 4
                        // output
                        // \o 1-2 \tcustom-style,3,Heading 3,4

                        // Search over all the outline styles used and figure out
                        // what is the minimum outline level (if any) filled by a
                        // non-standard style for that level, i.e. ignore headline
                        // styles 1-9 and find the lowest valid outline level
                        sal_uInt8 nPosOfLowestNonStandardLvl = MAXLEVEL;
                        const SwTextFormatColls& rColls = *GetExport().m_rDoc.GetTextFormatColls();
                        for( n = rColls.size(); n; )
                        {
                            const SwTextFormatColl* pColl = rColls[ --n ];
                            sal_uInt16 nPoolId = pColl->GetPoolFormatId();
                            if (
                                //Is a Non-Standard Outline Style
                                (RES_POOLCOLL_HEADLINE1 > nPoolId || RES_POOLCOLL_HEADLINE9 < nPoolId) &&
                                //Has a valid outline level
                                (pColl->IsAssignedToListLevelOfOutlineStyle()) &&
                                // Is less than the lowest known non-standard level
                                (pColl->GetAssignedOutlineStyleLevel() < nPosOfLowestNonStandardLvl)
                                )
                            {
                                nPosOfLowestNonStandardLvl = ::sal::static_int_cast<sal_uInt8>(pColl->GetAssignedOutlineStyleLevel());
                            }
                        }

                        sal_uInt8 nMaxMSAutoEvaluate = nPosOfLowestNonStandardLvl < nTOXLvl ? nPosOfLowestNonStandardLvl : static_cast<sal_uInt8>(nTOXLvl);

                        //output \o 1-X where X is the highest normal outline style to be included in the toc
                        if ( nMaxMSAutoEvaluate )
                        {
                            if (nMaxMSAutoEvaluate > WW8ListManager::nMaxLevel)
                                nMaxMSAutoEvaluate = WW8ListManager::nMaxLevel;

                            sStr += "\\o \"1-" + OUString::number(nMaxMSAutoEvaluate) + sEntryEnd;
                        }

                        //collect up any other styles in the writer TOC which will
                        //not already appear in the MS TOC and place then into the
                        //\t option
                        if( nMaxMSAutoEvaluate < nTOXLvl )
                        {
                            // collect this templates into the \t option
                            for( n = rColls.size(); n;)
                            {
                                const SwTextFormatColl* pColl = rColls[ --n ];
                                if (!pColl->IsAssignedToListLevelOfOutlineStyle())
                                    continue;
                                sal_uInt8 nTestLvl =  ::sal::static_int_cast<sal_uInt8>(pColl->GetAssignedOutlineStyleLevel());
                                if (nTestLvl < nTOXLvl && nTestLvl >= nMaxMSAutoEvaluate)
                                {
                                    if (!sTOption.isEmpty())
                                        sTOption += tsep;
                                    sTOption += pColl->GetName().toString() + tsep + OUString::number(nTestLvl + 1);
                                }
                            }
                        }
                    }

                    if( SwTOXElement::ParagraphOutlineLevel & pTOX->GetCreateType() )
                    {
                        sStr +="\\u " ;
                    }

                    if( SwTOXElement::Template & pTOX->GetCreateType() )
                    {
                        // #i99641# - Consider additional styles regardless of TOX-outlinelevel
                        for( n = 0; n < MAXLEVEL; ++n )
                        {
                            const UIName& rStyles = pTOX->GetStyleNames( n );
                            if( !rStyles.isEmpty() )
                            {
                                sal_Int32 nPos = 0;
                                const OUString sLvl{tsep + OUString::number(n + 1)};
                                do {
                                    const OUString sStyle( rStyles.toString().getToken( 0, TOX_STYLE_DELIMITER, nPos ));
                                    if( !sStyle.isEmpty() )
                                    {
                                        SwTextFormatColl* pColl = GetExport().m_rDoc.FindTextFormatCollByName(UIName(sStyle));
                                        if (pColl)
                                        {
                                            OUString const converted(GetExport().m_pStyles->GetStyleWWName(pColl));
                                            if (!converted.isEmpty() &&
                                                (!pColl->IsAssignedToListLevelOfOutlineStyle()
                                                 || pColl->GetAssignedOutlineStyleLevel() < nTOXLvl))
                                            {
                                                if( !sTOption.isEmpty() )
                                                    sTOption += tsep;
                                                sTOption += converted + sLvl;
                                            }
                                        }
                                    }
                                } while( -1 != nPos );
                            }
                        }
                    }

                    // No 'else' branch; why the below snippet is a block I have no idea.
                    {
                        OUString aFillText;
                        sal_uInt8 nNoPgStt = MAXLEVEL, nNoPgEnd = MAXLEVEL;
                        bool bFirstFillText = true, bOnlyText = true;
                        for( n = 0; n < nTOXLvl; ++n )
                        {
                            OUString aText;
                            int nRet = ::lcl_CheckForm( pTOX->GetTOXForm(),
                                static_cast< sal_uInt8 >(n+1), aText );
                            if( 1 == nRet )
                            {
                                bOnlyText = false;
                                if( MAXLEVEL == nNoPgStt )
                                    nNoPgStt = static_cast< sal_uInt8 >(n+1);
                            }
                            else
                            {
                                if( MAXLEVEL != nNoPgStt &&
                                    MAXLEVEL == nNoPgEnd )
                                    nNoPgEnd = sal_uInt8(n);

                                bOnlyText = bOnlyText && 3 == nRet;
                                if( 3 == nRet || 4 == nRet )
                                {
                                    if( bFirstFillText )
                                        aFillText = aText;
                                    else if( aFillText != aText )
                                        aFillText.clear();
                                    bFirstFillText = false;
                                }
                            }
                        }
                        if( MAXLEVEL != nNoPgStt )
                        {
                            if (WW8ListManager::nMaxLevel < nNoPgEnd)
                                nNoPgEnd = WW8ListManager::nMaxLevel;
                            sStr += "\\n "
                                + OUString::number( nNoPgStt )
                                + "-"
                                + OUString::number( nNoPgEnd )
                                + " ";
                        }
                        if( bOnlyText )
                        {
                            sStr += "\\p \"" + aFillText + sEntryEnd;
                        }
                    }

                    if( !sTOption.isEmpty() )
                    {
                        sStr += "\\t \"" + sTOption + sEntryEnd;
                    }

                    if (lcl_IsHyperlinked(pTOX->GetTOXForm(), nTOXLvl))
                        sStr += "\\h";
                    break;
                }
            }
        }

        if (!sStr.isEmpty())
        {
            GetExport( ).m_bInWriteTOX = true;
            if (GetExport().GetExportFormat() == MSWordExportBase::ExportFormat::RTF)
            {   // tdf#129574: required for RTF; doesn't work with DOCX
                StartRun(nullptr, -42, true);
            }
            GetExport( ).OutputField( nullptr, eCode, sStr, FieldFlags::Start | FieldFlags::CmdStart |
                FieldFlags::CmdEnd );
            if (GetExport().GetExportFormat() == MSWordExportBase::ExportFormat::RTF)
            {
                EndRun(nullptr, -42, -1, true);
            }
        }
    }

    GetExport( ).m_bStartTOX = false;
}

void AttributeOutputBase::EndTOX( const SwSection& rSect,bool bCareEnd )
{
    const SwTOXBase* pTOX = rSect.GetTOXBase();
    if ( pTOX )
    {
        ww::eField eCode = TOX_INDEX == pTOX->GetType() ? ww::eINDEX : ww::eTOC;
        GetExport( ).OutputField( nullptr, eCode, OUString(), FieldFlags::Close );

        if ( pTOX->GetType() == TOX_INDEX && GetExport().AddSectionBreaksForTOX() )
        {
            const SwFormatCol& rCol = rSect.GetFormat()->GetFormatAttr( RES_COL );
            const SwColumns& rColumns = rCol.GetColumns();
            sal_Int32 nCol = rColumns.size();

            if ( 0 < nCol )
            {
                WW8_SepInfo rInfo( &GetExport().m_rDoc.GetPageDesc( 0 ), rSect.GetFormat(), 0/*nRstLnNum*/ );
                GetExport( ).AttrOutput().SectionBreak( msword::PageBreak, false, &rInfo );
            }
        }
    }
    GetExport( ).m_bInWriteTOX = false;
    GetExport( ).m_bHideTabLeaderAndPageNumbers = false;
    if (bCareEnd)
        OnTOXEnding();
}

bool MSWordExportBase::GetNumberFormat(const SwField& rField, OUString& rStr)
{
    // Returns a date or time format string by using the US NfKeywordTable
    bool bHasFormat = false;
    SvNumberFormatter* pNFormatr = m_rDoc.GetNumberFormatter();
    sal_uInt32 nFormatIdx = rField.GetUntypedFormat();
    const SvNumberformat* pNumFormat = pNFormatr->GetEntry( nFormatIdx );
    if( pNumFormat )
    {
        LanguageType nLng = rField.GetLanguage();
        SAL_WARN_IF(nLng == LANGUAGE_DONTKNOW, "sw.ww8", "unexpected LANGUAGE_DONTKNOW");
        if (nLng == LANGUAGE_NONE || nLng == LANGUAGE_DONTKNOW)
        {
            nLng = pNumFormat->GetLanguage();
        }
        const LocaleDataWrapper* pLocDat = LocaleDataWrapper::get(LanguageTag(nLng));

        OUString sFormat(pNumFormat->GetMappedFormatstring(GetNfKeywordTable(),
            *pLocDat));

        if (!sFormat.isEmpty())
        {
            sw::ms::SwapQuotesInField(sFormat);

            rStr = "\\@\"" + sFormat + "\" " ;
            bHasFormat = true;
        }
    }
    return bHasFormat;
}

void AttributeOutputBase::GetNumberPara( OUString& rStr, const SwField& rField )
{
    switch(rField.GetUntypedFormat())
    {
        case SVX_NUM_CHARS_UPPER_LETTER:
        case SVX_NUM_CHARS_UPPER_LETTER_N:
            rStr += "\\* ALPHABETIC ";
            break;
        case SVX_NUM_CHARS_LOWER_LETTER:
        case SVX_NUM_CHARS_LOWER_LETTER_N:
            rStr += "\\* alphabetic ";
            break;
        case SVX_NUM_ROMAN_UPPER:
            rStr += "\\* ROMAN ";
            break;
        case SVX_NUM_ROMAN_LOWER:
            rStr += "\\* roman ";
            break;
        case SVX_NUM_TEXT_NUMBER:
            rStr += "\\* Ordinal ";
            break;
        case SVX_NUM_TEXT_ORDINAL:
            rStr += "\\* Ordtext ";
            break;
        case SVX_NUM_TEXT_CARDINAL:
            rStr += "\\* Cardtext ";
            break;
        default:
            OSL_ENSURE(rField.GetUntypedFormat() == SVX_NUM_ARABIC,
                "Unknown numbering type exported as default of Arabic");
            [[fallthrough]];
        case SVX_NUM_ARABIC:
            rStr += "\\* ARABIC ";
            break;
        case SVX_NUM_PAGEDESC:
            //Nothing, use word's default
            break;
    }
}

void WW8Export::WritePostItBegin( ww::bytes* pOut )
{
    sal_uInt8 aArr[ 3 ];
    sal_uInt8* pArr = aArr;

    // sprmCFSpec true
    Set_UInt16( pArr, NS_sprm::CFSpec::val );
    Set_UInt8( pArr, 1 );

    m_pChpPlc->AppendFkpEntry( Strm().Tell() );
    WriteChar( 0x05 );              // Annotation reference

    if( pOut )
        pOut->insert( pOut->end(), aArr, pArr );
    else
        m_pChpPlc->AppendFkpEntry( Strm().Tell(), static_cast< short >(pArr - aArr), aArr );
}

OUString FieldString(ww::eField eIndex)
{
    if (const char *pField = ww::GetEnglishFieldName(eIndex))
        return " " + OUString::createFromAscii(pField) + " ";
    return u"  "_ustr;
}

void WW8AttributeOutput::HiddenField( const SwField& rField )
{
    //replace LF 0x0A with VT 0x0B
    const OUString sExpand(rField.GetPar2().replace(0x0A, 0x0B));

    m_rWW8Export.m_pChpPlc->AppendFkpEntry(m_rWW8Export.Strm().Tell());
    SwWW8Writer::WriteString16(m_rWW8Export.Strm(), sExpand, false);
    static const sal_uInt8 aArr[] =
    {
        0x3C, 0x08, 0x1
    };
    m_rWW8Export.m_pChpPlc->AppendFkpEntry(m_rWW8Export.Strm().Tell(), sizeof(aArr), aArr);
}

void WW8AttributeOutput::SetField( const SwField& rField, ww::eField eType, const OUString& rCmd )
{
    const SwSetExpField* pSet = static_cast<const SwSetExpField*>(&rField);
    const OUString aVar = pSet->GetPar2();

    sal_uLong nFrom = m_rWW8Export.Fc2Cp(m_rWW8Export.Strm().Tell());

    GetExport().OutputField(&rField, eType, rCmd, FieldFlags::Start |
        FieldFlags::CmdStart | FieldFlags::CmdEnd);

    /*
    Is there a bookmark at the start position of this field, if so
    move it to the 0x14 of the result of the field.  This is what word
    does. MoveFieldMarks moves any bookmarks at this position to
    the beginning of the field result, and marks the bookmark as a
    fieldbookmark which is to be ended before the field end mark
    instead of after it like a normal bookmark.
    */
    m_rWW8Export.MoveFieldMarks(nFrom,m_rWW8Export.Fc2Cp(m_rWW8Export.Strm().Tell()));

    if (!aVar.isEmpty())
    {
        SwWW8Writer::WriteString16(m_rWW8Export.Strm(), aVar, false);
    }
    GetExport().OutputField(&rField, eType, rCmd, FieldFlags::Close);
}

void WW8AttributeOutput::PostitField( const SwField* pField )
{
    const SwPostItField *pPField = static_cast<const SwPostItField*>(pField);
    m_rWW8Export.m_pAtn->Append( m_rWW8Export.Fc2Cp( m_rWW8Export.Strm().Tell() ), pPField );
    m_rWW8Export.WritePostItBegin( m_rWW8Export.m_pO.get() );
}

bool WW8AttributeOutput::DropdownField( const SwField* pField )
{
    const SwDropDownField& rField2 = *static_cast<const SwDropDownField*>(pField);
    uno::Sequence<OUString> aItems =
        rField2.GetItemSequence();
    GetExport().DoComboBox(rField2.GetName(),
                       rField2.GetHelp(),
                       rField2.GetToolTip(),
                       rField2.GetSelectedItem(), aItems);
    return false;
}

bool WW8AttributeOutput::PlaceholderField( const SwField* )
{
    return true; // expand to text?
}

void WW8AttributeOutput::RefField( const SwField &rField, const OUString &rRef)
{
    const OUString sStr{ FieldString( ww::eREF ) + "\"" + rRef + "\" " };
    m_rWW8Export.OutputField( &rField, ww::eREF, sStr, FieldFlags::Start |
        FieldFlags::CmdStart | FieldFlags::CmdEnd );
    const OUString sVar = lcl_GetExpandedField( rField );
    if ( !sVar.isEmpty() )
    {
        SwWW8Writer::WriteString16( m_rWW8Export.Strm(), sVar, false );
    }
    m_rWW8Export.OutputField( &rField, ww::eREF, sStr, FieldFlags::Close );
}

void WW8AttributeOutput::WriteExpand( const SwField* pField )
{
    SwWW8Writer::WriteString16( m_rWW8Export.Strm(), lcl_GetExpandedField( *pField ), false );
}

namespace
{
// Escapes a token string for storing in Word formats. Its import counterpart
// is lcl_ExtractToken in writerfilter/source/dmapper/DomainMapper_Impl.cxx
OUString EscapeToken(const OUString& rCommand)
{
    bool bWasEscaped = false;

    const int nBufferLen = rCommand.getLength()*1.5;
    OUStringBuffer sResult(nBufferLen);
    sResult.append('"'); // opening quote
    for (sal_Int32 i = 0; i < rCommand.getLength(); ++i)
    {
        sal_Unicode ch = rCommand[i];
        switch (ch)
        {
        case '\\':
        case '"':
            // Backslashes and doublequotes must be escaped
            bWasEscaped = true;
            sResult.append('\\');
            break;
        case ' ':
            // Spaces require quotation
            bWasEscaped = true;
            break;
        }
        sResult.append(ch);
    }

    if (bWasEscaped)
    {
        sResult.append('"'); // closing quote
        return sResult.makeStringAndClear();
    }
    // No escapement/quotation was required
    return rCommand;
}
}

void AttributeOutputBase::TextField( const SwFormatField& rField )
{
    const SwField* pField = rField.GetField();
    bool bWriteExpand = false;

    switch (pField->GetTyp()->Which())
    {
    case SwFieldIds::GetExp:
        {
            const SwGetExpField *pGet = static_cast<const SwGetExpField*>(pField);
            if (pGet->GetSubType() == SwGetSetExpType::String)
            {
                RefField( *pGet, pGet->GetFormula() );
            }
            else
                bWriteExpand = true;
        }
        break;
    case SwFieldIds::SetExp:
        {
            const SwSetExpField *pSet = static_cast<const SwSetExpField*>(pField);
            SwGetSetExpType nSubType = pSet->GetSubType();
            if (SwGetSetExpType::Sequence == nSubType)
            {
                OUString sStr;
                if (GetExport().FieldsQuoted())
                    sStr = FieldString(ww::eSEQ) + pField->GetTyp()->GetName().toString() + " ";
                else
                    sStr = FieldString(ww::eSEQ) + "\"" + pField->GetTyp()->GetName().toString() +"\" ";
                GetNumberPara( sStr, *pField );
                GetExport().OutputField(pField, ww::eSEQ, sStr);
            }
            else if (nSubType & SwGetSetExpType::String)
            {
                bool bShowAsWell = false;
                ww::eField eFieldNo;
                const OUString sVar = pSet->GetPar2();
                OUString sStr;
                if (pSet->GetInputFlag())
                {
                    sStr = FieldString(ww::eASK) + "\""
                        + pSet->GetPar1() + "\" "
                        + pSet->GetPromptText() + " \\d "
                        + sVar;
                    eFieldNo = ww::eASK;
                }
                else
                {
                    sStr = FieldString(ww::eSET)
                        + pSet->GetPar1() + " \""
                        + sVar + "\" ";
                    eFieldNo = ww::eSET;
                    bShowAsWell = !(nSubType & SwGetSetExpType::Invisible);
                }

                SetField( *pField, eFieldNo, sStr );

                if (bShowAsWell)
                    RefField( *pSet, pSet->GetPar1() );
            }
            else
                bWriteExpand = true;
        }
        break;
    case SwFieldIds::PageNumber:
        {
            OUString sStr = FieldString(ww::ePAGE);
            GetNumberPara(sStr, *pField);
            GetExport().OutputField(pField, ww::ePAGE, sStr);
        }
        break;
    case SwFieldIds::Filename:
        {
            OUString sStr = FieldString(ww::eFILENAME);
            if (static_cast<const SwFileNameField*>(pField)->GetFormat() == SwFileNameFormat::PathName)
                sStr += "\\p ";
            GetExport().OutputField(pField, ww::eFILENAME, sStr);
        }
        break;
    case SwFieldIds::Database:
        {
            OUString sStr = FieldString(ww::eMERGEFIELD)
                + EscapeToken(static_cast<SwDBFieldType *>(pField->GetTyp())->GetColumnName()) + " ";
            GetExport().OutputField(pField, ww::eMERGEFIELD, sStr);
        }
        break;
    case SwFieldIds::DatabaseName:
        {
            SwDBData aData = GetExport().m_rDoc.GetDBData();
            const OUString sStr = FieldString(ww::eDATABASE)
                + aData.sDataSource
                + OUStringChar(DB_DELIM)
                + aData.sCommand;
            GetExport().OutputField(pField, ww::eDATABASE, sStr);
        }
        break;
    case SwFieldIds::Author:
        {
            ww::eField eField =
                ((SwAuthorFormat::Shortcut & static_cast<const SwAuthorField*>(pField)->GetFormat()) ? ww::eUSERINITIALS : ww::eUSERNAME);
            GetExport().OutputField(pField, eField, FieldString(eField));
        }
        break;
    case SwFieldIds::TemplateName:
        GetExport().OutputField(pField, ww::eTEMPLATE, FieldString(ww::eTEMPLATE));
        break;
    case SwFieldIds::DocInfo:    // Last printed, last edited,...
        {
            auto pDocInfoField = static_cast<const SwDocInfoField*>(pField);
            const SwDocInfoSubType nSubType = pDocInfoField->GetSubType();
            if( SwDocInfoSubType::SubFixed & nSubType )
                bWriteExpand = true;

            OUString sStr;
            ww::eField eField(ww::eNONE);
            switch (SwDocInfoSubType::LowerMask & nSubType)
            {
                case SwDocInfoSubType::Title:
                    eField = ww::eTITLE;
                    break;
                case SwDocInfoSubType::Subject:
                    eField = ww::eSUBJECT;
                    break;
                case SwDocInfoSubType::Keys:
                    eField = ww::eKEYWORDS;
                    break;
                case SwDocInfoSubType::Comment:
                    eField = ww::eCOMMENTS;
                    break;
                case SwDocInfoSubType::DocNo:
                    eField = ww::eREVNUM;
                    break;
                case SwDocInfoSubType::Edit:
                    eField = ww::eEDITTIME;
                    break;
                case SwDocInfoSubType::Create:
                    if (SwDocInfoSubType::SubAuthor == (nSubType & SwDocInfoSubType::SubMask))
                        eField = ww::eAUTHOR;
                    else if (GetExport().GetNumberFormat(*pField, sStr) || sStr.isEmpty())
                        eField = ww::eCREATEDATE;

                    // Create author/time are always imported as fixed. Safe to ignore on export
                    bWriteExpand = false;
                    break;

                case SwDocInfoSubType::Change:
                    if (SwDocInfoSubType::SubAuthor == (nSubType & SwDocInfoSubType::SubMask))
                    {
                        eField = ww::eLASTSAVEDBY;
                        bWriteExpand=false;
                    }
                    else if (GetExport().GetNumberFormat(*pField, sStr) || sStr.isEmpty())
                        eField = ww::eSAVEDATE;
                    break;

                case SwDocInfoSubType::Print:
                    if (SwDocInfoSubType::SubAuthor != (nSubType & SwDocInfoSubType::SubMask) &&
                        (GetExport().GetNumberFormat(*pField, sStr) || sStr.isEmpty()))
                        eField = ww::ePRINTDATE;
                    break;
                case SwDocInfoSubType::Custom:
                    eField = ww::eDOCPROPERTY;
                    sStr = "\"" + pDocInfoField->GetName() + "\"";
                    bWriteExpand = false;
                    break;
                default:
                    break;
            }

            if (!bWriteExpand && eField != ww::eNONE)
            {
                GetExport().OutputField(pField, eField, FieldString(eField) + sStr);
            }
            else
                bWriteExpand = true;
        }
        break;
    case SwFieldIds::DateTime:
        {
            auto pDateTimeField = static_cast<const SwDateTimeField*>(pField);
            const SwDateTimeSubType nSubType = pDateTimeField->GetSubType();
            OUString sStr;
            if (!GetExport().GetNumberFormat(*pField, sStr))
                bWriteExpand = true;
            else
            {
                ww::eField eField = (SwDateTimeSubType::Date & nSubType) ? ww::eDATE : ww::eTIME;
                GetExport().OutputField(pField, eField, FieldString(eField) + sStr);
            }
        }
        break;
    case SwFieldIds::DocStat:
        {
            auto pDocStatField = static_cast<const SwDocStatField*>(pField);
            const SwDocStatSubType nSubType = pDocStatField->GetSubType();
            ww::eField eField = ww::eNONE;

            switch (nSubType)
            {
                case SwDocStatSubType::Page:
                    eField = ww::eNUMPAGES;
                    break;
                case SwDocStatSubType::Word:
                    eField = ww::eNUMWORDS;
                    break;
                case SwDocStatSubType::Character:
                    eField = ww::eNUMCHARS;
                    break;
                default: break;
            }

            if (eField != ww::eNONE)
            {
                OUString sStr = FieldString(eField);
                GetNumberPara(sStr, *pField);
                GetExport().OutputField(pField, eField, sStr);
            }
            else
                bWriteExpand = true;
        }
        break;
    case SwFieldIds::ExtUser:
        {
            auto pExtUserField = static_cast<const SwExtUserField*>(pField);
            const SwExtUserSubType nSubType = pExtUserField->GetSubType();
            ww::eField eField = ww::eNONE;
            switch (static_cast<SwExtUserSubType>(0xFF & static_cast<sal_uInt16>(nSubType)))
            {
                case SwExtUserSubType::Firstname:
                case SwExtUserSubType::Name:
                    eField = ww::eUSERNAME;
                    break;
                case SwExtUserSubType::Shortcut:
                    eField = ww::eUSERINITIALS;
                    break;
                case SwExtUserSubType::Street:
                case SwExtUserSubType::Country:
                case SwExtUserSubType::Zip:
                case SwExtUserSubType::City:
                    eField = ww::eUSERADDRESS;
                    break;
                default: break;
            }

            if (eField != ww::eNONE)
            {
                GetExport().OutputField(pField, eField, FieldString(eField));
            }
            else
                bWriteExpand = true;
        }
        break;
    case SwFieldIds::TableOfAuthorities:
    {
        OUString sRet(static_cast<SwAuthorityField const*>(pField)
                        ->ExpandCitation(AUTH_FIELD_IDENTIFIER, nullptr));
        // FIXME: DomainMapper_Impl::CloseFieldCommand() stuffs fully formed
        // field instructions in here, but if the field doesn't originate
        // from those filters it won't have that
        if (!o3tl::starts_with(o3tl::trim(sRet), u"CITATION"))
        {
            sRet = FieldString(ww::eCITATION) + " \"" + sRet + "\"";
        }
        GetExport().OutputField( pField, ww::eCITATION, sRet );
    }
    break;
    case SwFieldIds::Postit:
        //Sadly only possible for word in main document text
        if (GetExport().m_nTextTyp == TXT_MAINTEXT)
        {
            PostitField( pField );
        }
        break;
    case SwFieldIds::Input:
        {
            const SwInputField * pInputField = dynamic_cast<const SwInputField *>(pField);

            if (pInputField && pInputField->isFormField())
                GetExport().DoFormText(pInputField);
            else
            {
                const OUString sStr = FieldString(ww::eFILLIN) + "\""
                    + pField->GetPar2() + "\"";

                GetExport().OutputField(pField, ww::eFILLIN, sStr);
            }
        }
        break;
    case SwFieldIds::GetRef:
        {
            ww::eField eField = ww::eNONE;
            OUString sStr;
            const SwGetRefField& rRField = *static_cast<const SwGetRefField*>(pField);
            const ReferencesSubtype nSubType = rRField.GetSubType();
            switch (nSubType)
            {
                case ReferencesSubtype::SetRefAttr:
                case ReferencesSubtype::Bookmark:
                    switch (rRField.GetFormat())
                    {
                        case RefFieldFormat::AsPageStyle:
                        case RefFieldFormat::Page:
                            eField = ww::ePAGEREF;
                            break;
                        default:
                            eField = ww::eREF;
                            break;
                    }
                    {
                        const SwMarkName& aRefName(rRField.GetSetRefName());
                        sStr = FieldString(eField)
                               + GetExport().GetBookmarkName(nSubType, &aRefName.toString(), 0);
                    }
                    switch (rRField.GetFormat())
                    {
                        case RefFieldFormat::Number:
                            sStr += " \\r";
                            break;
                        case RefFieldFormat::NumberNoContext:
                            sStr += " \\n";
                            break;
                        case RefFieldFormat::NumberFullContext:
                            sStr += " \\w";
                            break;
                        default: break;
                    }
                    break;
                case ReferencesSubtype::SequenceField:
                {
                    // Not implemented for RTF
                    if(GetExport().GetExportFormat() == MSWordExportBase::ExportFormat::RTF)
                        break;

                    switch (rRField.GetFormat())
                    {
                        case RefFieldFormat::Page:
                        case RefFieldFormat::AsPageStyle:
                            eField = ww::ePAGEREF;
                            break;
                        default:
                            eField = ww::eREF;
                            break;
                    }
                    // Generate a unique bookmark name
                    {
                        OUString sName{rRField.GetSetRefName().toString() + OUString::number(rRField.GetSeqNo())};
                        switch (rRField.GetFormat())
                        {
                            case RefFieldFormat::Page:
                            case RefFieldFormat::AsPageStyle:
                            case RefFieldFormat::Content:
                            case RefFieldFormat::UpDown:
                                    sName += "_full";
                                    break;
                            case RefFieldFormat::CategoryAndNumber:
                                    sName += "_label_and_number";
                                    break;
                            case RefFieldFormat::CaptionText:
                                    sName += "_caption_only";
                                    break;
                            case RefFieldFormat::Numbering:
                                    sName += "_number_only";
                                    break;
                            default: // Ignore other types of reference fields
                                    eField = ww::eNONE;
                                    break;
                        }
                        sStr = FieldString(eField) + GetExport().GetBookmarkName(nSubType, &sName, 0);
                    }
                    switch (rRField.GetFormat())
                    {
                        case RefFieldFormat::Number:
                            sStr += " \\r";
                            break;
                        case RefFieldFormat::NumberNoContext:
                            sStr += " \\n";
                            break;
                        case RefFieldFormat::NumberFullContext:
                            sStr += " \\w";
                            break;
                        default: break;
                    }
                    break;
                }
                case ReferencesSubtype::Footnote:
                case ReferencesSubtype::Endnote:
                    switch (rRField.GetFormat())
                    {
                        case RefFieldFormat::AsPageStyle:
                        case RefFieldFormat::Page:
                            eField = ww::ePAGEREF;
                            break;
                        case RefFieldFormat::UpDown:
                            eField = ww::eREF;
                            break;
                        default:
                            eField =
                                ReferencesSubtype::Endnote == nSubType ? ww::eNOTEREF : ww::eFOOTREF;
                            break;
                    }
                    sStr = FieldString(eField)
                           + GetExport().GetBookmarkName(nSubType, nullptr, rRField.GetSeqNo());
                    break;
                case ReferencesSubtype::Style:
                    sStr = FieldString(ww::eSTYLEREF)
                           + GetExport().GetStyleRefName(pField->GetPar1());
                    eField = ww::eSTYLEREF;
                    break;
                default: break; // ReferencesSubtype::Outline not handled ?
            }

            OUString sExtraFlags = u"\\h "_ustr; // by default, include a hyperlink

            switch (eField)
            {
                case ww::eNONE:
                    bWriteExpand = true;
                    break;
                case ww::eSTYLEREF:
                    sExtraFlags = ""; // styleref fields do not work if they have a hyperlink

                    {
                        sal_uInt16 stylerefFlags = static_cast<const SwGetRefField*>(pField)->GetFlags();
                        if ((stylerefFlags & REFFLDFLAG_STYLE_FROM_BOTTOM) == REFFLDFLAG_STYLE_FROM_BOTTOM) {
                            sExtraFlags += "\\l ";
                        }
                        if ((stylerefFlags & REFFLDFLAG_STYLE_HIDE_NON_NUMERICAL) == REFFLDFLAG_STYLE_HIDE_NON_NUMERICAL) {
                            sExtraFlags += "\\t ";
                        }
                    }

                    [[fallthrough]];
                default:
                    switch (rRField.GetFormat())
                    {
                        case RefFieldFormat::Number:
                            sStr += " \\r " + sExtraFlags;
                            break;
                        case RefFieldFormat::NumberFullContext:
                            sStr += " \\w " + sExtraFlags;
                            break;
                        case RefFieldFormat::UpDown:
                            sStr += " \\p " + sExtraFlags;
                            break;
                        case RefFieldFormat::NumberNoContext:
                        case RefFieldFormat::Chapter:
                            sStr += " \\n " + sExtraFlags;
                            break;
                        default:
                            sStr += " " + sExtraFlags;
                            break;
                    }
                    GetExport().OutputField(pField, eField, sStr);
            }
        }
        break;
    case SwFieldIds::CombinedChars:
        {
        /*
        We need a font size to fill in the defaults, if these are overridden
        (as they generally are) by character properties then those properties
        win.

        The fontsize that is used in MS for determining the defaults is always
        the CJK fontsize even if the text is not in that language, in OOo the
        largest fontsize used in the field is the one we should take, but
        whatever we do, word will actually render using the fontsize set for
        CJK text. Nevertheless we attempt to guess whether the script is in
        asian or western text based up on the first character and use the
        font size of that script as our default.
        */
        assert(g_pBreakIt && g_pBreakIt->GetBreakIter().is());
        sal_uInt16 nScript = g_pBreakIt->GetBreakIter()->getScriptType( pField->GetPar1(), 0);

        TypedWhichId<SvxFontHeightItem> nFontHeightWhich = GetWhichOfScript(RES_CHRATR_FONTSIZE,nScript);
        tools::Long nHeight = GetExport().GetItem(nFontHeightWhich).GetHeight();

        nHeight = (nHeight + 10) / 20; //Font Size in points;

        /*
        Divide the combined char string into its up and down part. Get the
        font size and fill in the defaults as up == half the font size and
        down == a fifth the font size
        */
        const sal_Int32 nAbove = (pField->GetPar1().getLength()+1)/2;
        const OUString sStr = FieldString(ww::eEQ)
            + "\\o (\\s\\up "
            + OUString::number(nHeight/2)
            + "("
            + pField->GetPar1().subView(0, nAbove)
            + "), \\s\\do "
            + OUString::number(nHeight/5)
            + "("
            + pField->GetPar1().subView(nAbove)
            + "))";
        GetExport().OutputField(pField, ww::eEQ, sStr);
        }
        break;
    case SwFieldIds::Dropdown:
        bWriteExpand = DropdownField( pField );
        break;
    case SwFieldIds::Chapter:
        bWriteExpand = true;

        if (rField.GetTextField())
        {
            const SwTextNode *pTextNd = GetExport().GetHdFtPageRoot();
            if (!pTextNd)
            {
                pTextNd = GetExport().m_pCurPam->GetPointNode().GetTextNode();
            }

            if (pTextNd)
            {
                SwChapterField aCopy(*static_cast<const SwChapterField*>(pField));
                aCopy.ChangeExpansion(*pTextNd, false);

                OUString sStr;
                if (GetExport().m_bOutKF) {
                    // In headers and footers, use the chapter number as the style name
                    sStr = FieldString(ww::eSTYLEREF)
                        + " "
                        + OUString::number(aCopy.GetLevel() + 1)
                        + " \\* MERGEFORMAT ";
                } else {
                    // Otherwise, get the style of the text and use it as the style name
                    const SwTextNode* pOutlineNd = pTextNd->FindOutlineNodeOfLevel(aCopy.GetLevel());

                    if (!pOutlineNd) break;
                    // Sometimes we can't find the outline node, in that case let's just fallback to exporting the text

                    sStr = FieldString(ww::eSTYLEREF)
                         + GetExport().GetStyleRefName(pOutlineNd->GetFormatColl()->GetName().toString());
                }

                GetExport().OutputField(pField, ww::eSTYLEREF, sStr);
                bWriteExpand = false;
            }
        }
        break;
    case SwFieldIds::HiddenText:
        {
            OUString sExpand(pField->GetPar2());
            if (!sExpand.isEmpty())
            {
                auto eSubType = static_cast<const SwHiddenTextField*>(pField)->GetSubType();
                if (eSubType == SwFieldTypesEnum::ConditionalText)
                {
                    OUString aCond = pField->GetPar1();
                    OUString aTrueFalse = pField->GetPar2();
                    sal_Int32 nPos = aTrueFalse.indexOf('|');
                    OUString aTrue;
                    OUString aFalse;
                    if (nPos == -1)
                    {
                        aTrue = aTrueFalse;
                    }
                    else
                    {
                        aTrue = aTrueFalse.subView(0, nPos);
                        aFalse = aTrueFalse.subView(nPos + 1);
                    }
                    if (aTrue.getLength() > 1 && aTrue.startsWith("\"") && aTrue.endsWith("\""))
                        aTrue = aTrue.copy(1, aTrue.getLength() - 2);
                    if (aFalse.getLength() > 1 && aFalse.startsWith("\"") && aFalse.endsWith("\""))
                        aFalse = aFalse.copy(1, aFalse.getLength() - 2);

                    // Substitute a single quote for an illegal double quote if one exists
                    OUString aCmd = FieldString(ww::eIF) + aCond + " \""
                        + aTrue.replaceAll("\"", "'") + "\" \"" + aFalse.replaceAll("\"", "'")
                            + "\"";
                    GetExport().OutputField(pField, ww::eIF, aCmd);
                }
                else
                    HiddenField(*pField);
            }
        }
        break;
    case SwFieldIds::JumpEdit:
        bWriteExpand = PlaceholderField( pField );
        break;
    case SwFieldIds::Macro:
        {
            const OUString sStr = " MACROBUTTON "
                + pField->GetPar1().replaceFirst("StarOffice.Standard.Modul1.", "")
                + " "
                + lcl_GetExpandedField(*pField);
            GetExport().OutputField( pField, ww::eMACROBUTTON, sStr );
        }
        break;
    case SwFieldIds::Table:
        {
            ww::eField eField = ww::eEquals;
            OUString aExpand = OUString::Concat(" =") + o3tl::trim(pField->GetFieldName());
            GetExport().OutputField(pField, eField, aExpand);
        }
        break;
    case SwFieldIds::User:
    {
        ww::eField eField = ww::eDOCVARIABLE;
        OUString aExpand = FieldString(eField) + pField->GetPar1() + " ";
        GetExport().OutputField(pField, eField, aExpand);
    }
    break;
    default:
        bWriteExpand = true;
        break;
    }

    if (bWriteExpand)
        WriteExpand( pField );
}

void AttributeOutputBase::TextFlyContent( const SwFormatFlyCnt& rFlyContent )
{
    if ( auto pTextNd = dynamic_cast< const SwContentNode *>( GetExport().m_pOutFormatNode )  )
    {
        Point const origin;
        Point aLayPos = pTextNd->FindLayoutRect( &origin ).Pos();

        SwPosition aPos( *pTextNd );
        ww8::Frame aFrame( *rFlyContent.GetFrameFormat(), std::move(aPos) );

        OutputFlyFrame_Impl( aFrame, aLayPos );
    }
}

// TOXMarks are still missing

// WW allows detailed settings for hyphenation only for the whole document.
// One could implement following mimic: The values of the style "Standard" will
// be set in the Document Properties ( DOP ) if they exist.

// ACK. This suggestion fits exactly to our implementation of the import,
// therefore I'll implement that right now. (KHZ, 07/15/2000)
void WW8AttributeOutput::ParaHyphenZone( const SvxHyphenZoneItem& rHyphenZone )
{
    // sprmPFNoAutoHyph
    m_rWW8Export.InsUInt16( NS_sprm::PFNoAutoHyph::val );

    m_rWW8Export.m_pO->push_back( rHyphenZone.IsHyphen() ? 0 : 1 );
}

void WW8AttributeOutput::ParaScriptSpace( const SfxBoolItem& rScriptSpace )
{
    m_rWW8Export.InsUInt16( NS_sprm::PFAutoSpaceDE::val );
    m_rWW8Export.m_pO->push_back( rScriptSpace.GetValue() ? 1 : 0 );
}

void WW8AttributeOutput::ParaHangingPunctuation( const SfxBoolItem& rItem )
{
    m_rWW8Export.InsUInt16( NS_sprm::PFOverflowPunct::val );
    m_rWW8Export.m_pO->push_back( rItem.GetValue() ? 1 : 0 );
}

void WW8AttributeOutput::ParaForbiddenRules( const SfxBoolItem& rItem )
{
    m_rWW8Export.InsUInt16( NS_sprm::PFKinsoku::val );
    m_rWW8Export.m_pO->push_back( rItem.GetValue() ? 1 : 0 );
}

void WW8AttributeOutput::ParaSnapToGrid( const SvxParaGridItem& rGrid )
{
    // sprmPFUsePgsuSettings

    m_rWW8Export.InsUInt16( NS_sprm::PFUsePgsuSettings::val );
    m_rWW8Export.m_pO->push_back( rGrid.GetValue() ? 1 : 0 );
}

void WW8AttributeOutput::ParaVerticalAlign( const SvxParaVertAlignItem& rAlign )
{
    // sprmPWAlignFont

    m_rWW8Export.InsUInt16( NS_sprm::PWAlignFont::val );

    SvxParaVertAlignItem::Align nAlign = rAlign.GetValue();
    sal_uInt16 nVal;
    switch ( nAlign )
    {
        case SvxParaVertAlignItem::Align::Baseline:
            nVal = 2;
            break;
        case SvxParaVertAlignItem::Align::Top:
            nVal = 0;
            break;
        case SvxParaVertAlignItem::Align::Center:
            nVal = 1;
            break;
        case SvxParaVertAlignItem::Align::Bottom:
            nVal = 3;
            break;
        case SvxParaVertAlignItem::Align::Automatic:
            nVal = 4;
            break;
        default:
            nVal = 4;
            OSL_FAIL( "Unknown vert alignment" );
            break;
    }
    m_rWW8Export.InsUInt16( nVal );
}

// NoHyphen: I didn't find an equal in the SW UI and WW UI

// RefMark, NoLineBreakHere are still missing

void WW8Export::WriteFootnoteBegin( const SwFormatFootnote& rFootnote, ww::bytes* pOutArr )
{
    ww::bytes aAttrArr;
    const bool bAutoNum = rFootnote.GetNumStr().isEmpty();
    if( bAutoNum )
    {
        static const sal_uInt8 aSpec[] =
        {
            0x03, 0x6a, 0, 0, 0, 0, // sprmCObjLocation
            0x55, 0x08, 1           // sprmCFSpec
        };

        aAttrArr.insert(aAttrArr.end(), aSpec, aSpec+sizeof(aSpec));
    }

    // sprmCIstd
    const SwEndNoteInfo* pInfo;
    if( rFootnote.IsEndNote() )
        pInfo = &m_rDoc.GetEndNoteInfo();
    else
        pInfo = &m_rDoc.GetFootnoteInfo();
    const SwCharFormat* pCFormat = pOutArr
                                ? pInfo->GetAnchorCharFormat( m_rDoc )
                                : pInfo->GetCharFormat( m_rDoc );
    SwWW8Writer::InsUInt16( aAttrArr, NS_sprm::CIstd::val );
    SwWW8Writer::InsUInt16( aAttrArr, GetId( pCFormat ) );

                                                // fSpec-Attribute true
                            // For Auto-Number a special character must go
                            // into the text and therefore a fSpec attribute
    m_pChpPlc->AppendFkpEntry( Strm().Tell() );
    if( bAutoNum )
        WriteChar( 0x02 );              // auto number character
    else
        // user numbering
        OutSwString(rFootnote.GetNumStr(), 0, rFootnote.GetNumStr().getLength());

    if( pOutArr )
    {
        // insert at start of array, so the "hard" attribute overrule the
        // attributes of the character template
        pOutArr->insert( pOutArr->begin(), aAttrArr.begin(), aAttrArr.end() );
    }
    else
    {
        std::unique_ptr<ww::bytes> pOwnOutArr(new ww::bytes);

        // insert at start of array, so the "hard" attribute overrule the
        // attributes of the character template
        pOwnOutArr->insert(pOwnOutArr->begin(), aAttrArr.begin(), aAttrArr.end());

        // write for the ftn number in the content, the font of the anchor
        const SwTextFootnote* pTextFootnote = rFootnote.GetTextFootnote();
        if( pTextFootnote )
        {
            std::unique_ptr<ww::bytes> pOld = std::move(m_pO);
            m_pO = std::move(pOwnOutArr);
            SfxItemSetFixed<RES_CHRATR_FONT, RES_CHRATR_FONT> aSet( m_rDoc.GetAttrPool() );

            pCFormat = pInfo->GetCharFormat( m_rDoc );

            pTextFootnote->GetTextNode().GetParaAttr(aSet,
                pTextFootnote->GetStart(), pTextFootnote->GetStart() + 1, true);
            if (aSet.Count())
            {
                m_pAttrOutput->OutputItem( aSet.Get( RES_CHRATR_FONT ) );
            }
            else
            {
                m_pAttrOutput->OutputItem( pCFormat->GetAttrSet().Get(RES_CHRATR_FONT) );
            }
            pOwnOutArr = std::move(m_pO);
            m_pO = std::move(pOld);
        }
        m_pChpPlc->AppendFkpEntry( Strm().Tell(), pOwnOutArr->size(),
                                                pOwnOutArr->data() );
    }
}

static bool lcl_IsAtTextEnd(const SwFormatFootnote& rFootnote)
{
    bool bRet = true;
    if( rFootnote.GetTextFootnote() )
    {
        sal_uInt16 nWh = rFootnote.IsEndNote() ? sal_uInt16(RES_END_AT_TXTEND)
                                      : sal_uInt16(RES_FTN_AT_TXTEND);
        const SwSectionNode* pSectNd = rFootnote.GetTextFootnote()->GetTextNode().
                                                        FindSectionNode();
        while( pSectNd && FTNEND_ATPGORDOCEND ==
                static_cast<const SwFormatFootnoteEndAtTextEnd&>(pSectNd->GetSection().GetFormat()->
                GetFormatAttr( nWh)).GetValue() )
            pSectNd = pSectNd->StartOfSectionNode()->FindSectionNode();

        if (!pSectNd)
            bRet = false;   // the is ftn/end collected at Page- or Doc-End
    }
    return bRet;
}

void AttributeOutputBase::TextFootnote( const SwFormatFootnote& rFootnote )
{
    ReferencesSubtype nTyp;
    if ( rFootnote.IsEndNote() )
    {
        nTyp = ReferencesSubtype::Endnote;
        if ( GetExport().m_bEndAtTextEnd )
            GetExport().m_bEndAtTextEnd = lcl_IsAtTextEnd( rFootnote );
    }
    else
    {
        nTyp = ReferencesSubtype::Footnote;
        if ( GetExport().m_bFootnoteAtTextEnd )
            GetExport().m_bFootnoteAtTextEnd = lcl_IsAtTextEnd( rFootnote );
    }

    // if any reference to this footnote/endnote then insert an internal
    // Bookmark.
    OUString sBkmkNm;
    if ( GetExport().HasRefToFootOrEndnote( rFootnote.IsEndNote(), rFootnote.GetTextFootnote()->GetSeqRefNo()))
    {
        sBkmkNm = GetExport().GetBookmarkName(nTyp, nullptr,
                                    rFootnote.GetTextFootnote()->GetSeqRefNo() );
        GetExport().AppendBookmark( sBkmkNm );
    }

    TextFootnote_Impl( rFootnote );

    if ( !sBkmkNm.isEmpty() )
        GetExport().AppendBookmark( sBkmkNm ); // FIXME: Why is it added twice?  Shouldn't this one go to WW8AttributeOutput::TextFootnote_Impl()?
}

void WW8AttributeOutput::TextFootnote_Impl( const SwFormatFootnote& rFootnote )
{
    WW8_WrPlcFootnoteEdn* pFootnoteEnd;
    if ( rFootnote.IsEndNote() || GetExport().m_rDoc.GetFootnoteInfo().m_ePos == FTNPOS_CHAPTER )
        pFootnoteEnd = m_rWW8Export.m_pEdn.get();
    else
        pFootnoteEnd = m_rWW8Export.m_pFootnote.get();

    pFootnoteEnd->Append( m_rWW8Export.Fc2Cp( m_rWW8Export.Strm().Tell() ), rFootnote );
    m_rWW8Export.WriteFootnoteBegin( rFootnote, m_rWW8Export.m_pO.get() );
}

void WW8AttributeOutput::TextCharFormat( const SwFormatCharFormat& rCharFormat )
{
    if( rCharFormat.GetCharFormat() )
    {
        m_rWW8Export.InsUInt16( NS_sprm::CIstd::val );

        m_rWW8Export.InsUInt16( m_rWW8Export.GetId( rCharFormat.GetCharFormat() ) );
    }
}

/*
 See ww8par6.cxx Read_DoubleLine for some more info
 */
void WW8AttributeOutput::CharTwoLines( const SvxTwoLinesItem& rTwoLines )
{
    // #i28331# - check that bOn is set
    if ( !rTwoLines.GetValue() )
        return;

    m_rWW8Export.InsUInt16( NS_sprm::CFELayout::val );
    m_rWW8Export.m_pO->push_back( sal_uInt8(0x06) ); //len 6
    m_rWW8Export.m_pO->push_back( sal_uInt8(0x02) );

    sal_Unicode cStart = rTwoLines.GetStartBracket();
    sal_Unicode cEnd = rTwoLines.GetEndBracket();

    /*
    As per usual we have problems. We can have separate left and right brackets
    in OOo, it doesn't appear that you can in word. Also in word there appear
    to only be a limited number of possibilities, we can use pretty much
    anything.

    So if we have none, we export none, if either bracket is set to a known
    word type we export both as that type (with the bracket winning out in
    the case of a conflict simply being the order of test here.

    Upshot being a documented created in word will be reexported with no
    ill effects.
    */

    sal_uInt16 nType;
    if (!cStart && !cEnd)
        nType = 0;
    else if ((cStart == '{') || (cEnd == '}'))
        nType = 4;
    else if ((cStart == '<') || (cEnd == '>'))
        nType = 3;
    else if ((cStart == '[') || (cEnd == ']'))
        nType = 2;
    else
        nType = 1;
    m_rWW8Export.InsUInt16( nType );
    static const sal_uInt8 aZeroArr[ 3 ] = { 0, 0, 0 };
    m_rWW8Export.m_pO->insert( m_rWW8Export.m_pO->end(), aZeroArr, aZeroArr+3);
}

void AttributeOutputBase::FormatLineNumberingBase(const SwFormatLineNumber& rNumbering)
{
    // always write out suppressLineNumberings - even if it matches the parent,
    // so that even if the parent is modified, special styles/situations won't lose that suppression
    if (!rNumbering.IsCount())
    {
        FormatLineNumbering(rNumbering);
        return;
    }

    // Don't spam suppressLineNumberings = false - that is the default when not specified at all
    if (auto pNd = dynamic_cast<const SwContentNode*>(GetExport().m_pOutFormatNode)) //paragraph
    {
        // useless IsCount can be added to paragraph when specifying MID_LINENUMBER_STARTVALUE
        const auto& rSet = static_cast<SwTextFormatColl&>(pNd->GetAnyFormatColl()).GetAttrSet();
        const SwFormatLineNumber& rInherited = rSet.GetLineNumber();
        if (rInherited.IsCount() && rInherited.GetStartValue() != rNumbering.GetStartValue())
            return; // same IsCount as parent style
    }
    else if (GetExport().m_bStyDef) //style
    {
        if (GetExport().m_pCurrentStyle && GetExport().m_pCurrentStyle->DerivedFrom())
        {
            const auto& rSet = GetExport().m_pCurrentStyle->DerivedFrom()->GetAttrSet();
            if (rSet.GetLineNumber().IsCount())
                return; // same as parent style
        }
        else
            return; // same as default value
    }

    FormatLineNumbering(rNumbering);
}

void AttributeOutputBase::ParaOutlineLevelBase( const SfxUInt16Item& rItem )
{
    sal_uInt16 nOutLvl = rItem.GetValue();

    // Do not write out default level (Body Text) if there is no inheritance, or if the level matches the inherited value
    const SfxUInt16Item* pInherited = nullptr;
    if (auto pNd = dynamic_cast<const SwContentNode*>(GetExport().m_pOutFormatNode)) //paragraph
        pInherited = static_cast<SwTextFormatColl&>(pNd->GetAnyFormatColl()).GetAttrSet().GetItem<SfxUInt16Item>(RES_PARATR_OUTLINELEVEL);
    else if (GetExport().m_bStyDef && GetExport().m_pCurrentStyle && GetExport().m_pCurrentStyle->DerivedFrom()) //style
        pInherited = GetExport().m_pCurrentStyle->DerivedFrom()->GetAttrSet().GetItem<SfxUInt16Item>(RES_PARATR_OUTLINELEVEL);
    if ((pInherited && pInherited->GetValue() == nOutLvl)
        || (!pInherited && !nOutLvl))
        return;

    ParaOutlineLevel(rItem);
}

void AttributeOutputBase::ParaNumRule( const SwNumRuleItem& rNumRule )
{
    if (rNumRule.GetValue().isEmpty())
    {
        ParaNumRule_Impl(nullptr, 0, 0);
        return;
    }
    const SwNumRule* pRule = GetExport().m_rDoc.FindNumRulePtr(
                                    rNumRule.GetValue() );
    if (!pRule)
        return;

    sal_uInt16 nNumId = GetExport().GetNumberingId(*pRule) + 1;
    sal_uInt8 nLvl = 0;

    const SwTextNode* pTextNd = dynamic_cast<const SwTextNode*>(GetExport().m_pOutFormatNode);
    if (pTextNd)
    {
        if( pTextNd->IsCountedInList())
        {
            nLvl = std::clamp(pTextNd->GetActualListLevel(), 0, MAXLEVEL - 1);
            const bool bListRestart = pTextNd->IsListRestart();

            if (GetExport().GetExportFormat() == MSWordExportBase::DOCX) // FIXME
            {
                // tdf#95848 find the abstract list definition
                OUString const listId(pTextNd->GetListId());
                if (!listId.isEmpty()
                    && (listId != pRule->GetDefaultListId() // default list id uses the 1:1 mapping
                        || bListRestart)    // or restarting previous list
                    )
                {
                    SwList const*const pList(
                        GetExport().m_rDoc.getIDocumentListsAccess().getListByName(listId));
                    if (pList)
                    {
                        SwNumRule const*const pAbstractRule(
                            GetExport().m_rDoc.FindNumRulePtr(
                                pList->GetDefaultListStyleName()));
                        assert(pAbstractRule);
                        if (pAbstractRule == pRule && !bListRestart)
                        {
                            // different list, but no override
                            nNumId = GetExport().DuplicateAbsNum(listId, *pAbstractRule) + 1;
                        }
                        else
                        {
                            nNumId = GetExport().OverrideNumRule(
                                    *pRule, listId, *pAbstractRule) + 1;

                            if (bListRestart)
                            {
                                // For restarted lists we should also keep value for
                                // future w:lvlOverride / w:startOverride
                                GetExport().AddListLevelOverride(nNumId-1, pTextNd->GetActualListLevel(),
                                    pTextNd->GetActualListStartValue());
                            }
                        }
                    }
                }
            }
            else if (bListRestart)
            {
                sal_uInt16 nStartWith = static_cast<sal_uInt16>(pTextNd->GetActualListStartValue());
                nNumId = GetExport().DuplicateNumRule(pRule, nLvl, nStartWith);
                if (USHRT_MAX != nNumId)
                    ++nNumId;
            }
        }
        else
        {
            // #i44815# adjust numbering for numbered paragraphs
            // without number. These paragraphs will receive a
            // list id 0, which WW interprets as 'no number'.
            nNumId = 0;
        }
    }
    else if ( auto pC = dynamic_cast< const SwTextFormatColl *>( GetExport().m_pOutFormatNode ) )
    {
        if (pC->IsAssignedToListLevelOfOutlineStyle())
            nLvl = static_cast< sal_uInt8 >( pC->GetAssignedOutlineStyleLevel() );
        else
        {
            const SfxItemSet* pSet = GetExport().m_pISet;
            if (pSet && pSet->HasItem(RES_PARATR_LIST_LEVEL))
            {
                const SfxInt16Item* pItem = pSet->GetItem(RES_PARATR_LIST_LEVEL);
                nLvl = pItem->GetValue();
            }
        }
    }

    if ( nLvl >= WW8ListManager::nMaxLevel )
        nLvl = WW8ListManager::nMaxLevel - 1;

    ParaNumRule_Impl( pTextNd, nLvl, nNumId);
}

void WW8AttributeOutput::ParaNumRule_Impl(const SwTextNode* /*pTextNd*/,
        sal_Int32 const nLvl, sal_Int32 const nNumId)
{
    if (USHRT_MAX == nNumId)
        return;

    // write sprmPIlvl and sprmPIlfo
    SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, NS_sprm::PIlvl::val );
    m_rWW8Export.m_pO->push_back( ::sal::static_int_cast<sal_uInt8>(nLvl) );
    SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, NS_sprm::PIlfo::val );
    SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, ::sal::static_int_cast<sal_uInt16>(nNumId) );
}

/* File FRMATR.HXX */

void WW8AttributeOutput::FormatFrameSize( const SwFormatFrameSize& rSize )
{
    if( m_rWW8Export.m_bOutFlyFrameAttrs )                   // Flys
    {
        if( m_rWW8Export.m_bOutGrf )
            return;                // Fly around graphic -> Auto-size

        //???? What about percentages ???
        if ( rSize.GetWidth() && rSize.GetWidthSizeType() == SwFrameSize::Fixed)
        {
            //"sprmPDxaWidth"
            m_rWW8Export.InsUInt16( NS_sprm::PDxaWidth::val );
            m_rWW8Export.InsUInt16( rSize.GetWidth() );
        }

        if ( rSize.GetHeight() )
        {
            // sprmPWHeightAbs
            m_rWW8Export.InsUInt16( NS_sprm::PWHeightAbs::val );

            sal_uInt16 nH = 0;
            switch ( rSize.GetHeightSizeType() )
            {
                case SwFrameSize::Variable: break;
                case SwFrameSize::Fixed: nH = o3tl::narrowing<sal_uInt16>(rSize.GetHeight()) & 0x7fff; break;
                default:           nH = o3tl::narrowing<sal_uInt16>(rSize.GetHeight()) | 0x8000; break;
            }
            m_rWW8Export.InsUInt16( nH );
        }
    }
    else if( m_rWW8Export.m_bOutPageDescs )            // PageDesc : width + height
    {
        if( m_rWW8Export.m_pCurrentPageDesc->GetLandscape() )
        {
            /*sprmSBOrientation*/
            m_rWW8Export.InsUInt16( NS_sprm::SBOrientation::val );
            m_rWW8Export.m_pO->push_back( 2 );
        }

        /*sprmSXaPage*/
        m_rWW8Export.InsUInt16( NS_sprm::SXaPage::val );
        m_rWW8Export.InsUInt16(
            msword_cast<sal_uInt16>(SvxPaperInfo::GetSloppyPaperDimension(rSize.GetWidth())));

        /*sprmSYaPage*/
        m_rWW8Export.InsUInt16( NS_sprm::SYaPage::val );
        m_rWW8Export.InsUInt16(
            msword_cast<sal_uInt16>(SvxPaperInfo::GetSloppyPaperDimension(rSize.GetHeight())));
    }
}

// FillOrder is still missing

/**
 * ReplaceCr() is used for Pagebreaks and Pagedescs. An already written CR
 * will be replaced by a break character. Replace must be called right after
 * the writing of CR.
 *
 * @return FilePos + 1 of the replaced CR or 0 if nothing was replaced.
 */
sal_uInt64 WW8Export::ReplaceCr( sal_uInt8 nChar )
{
    OSL_ENSURE( nChar, "replaced with 0 crashes WW97/95" );

    bool bReplaced = false;
    SvStream& rStrm = Strm();
    sal_uInt64 nRetPos = 0, nPos = rStrm.Tell();
    //If there is at least two characters already output
    if (nPos - 2 >= o3tl::make_unsigned(m_pFib->m_fcMin))
    {
        sal_uInt16 nUCode=0;

        rStrm.SeekRel(-2);
        rStrm.ReadUInt16( nUCode );
        //If the last char was a cr
        if (nUCode == 0x0d)             // CR ?
        {
            if ((nChar == 0x0c) &&
                (nPos - 4 >= o3tl::make_unsigned(m_pFib->m_fcMin)))
            {
                rStrm.SeekRel(-4);
                rStrm.ReadUInt16( nUCode );
            }
            else
            {
                rStrm.SeekRel(-2);
                nUCode = 0x0;
            }
            //And the para is not of len 0, then replace this cr with the mark
            //#120140# If there is a cr before a column break, need replace the cr. So remove the "nChar==0x0e" check.
            if( nUCode == 0x0d )
                bReplaced = false;
            else
            {
                bReplaced = true;
                WriteChar(nChar);
                nRetPos = nPos;
            }
        }
        else if ((nUCode == 0x0c) && (nChar == 0x0e))
        {
            // a column break after a section has no effect in writer
            bReplaced = true;
        }
        rStrm.Seek( nPos );
    }
    else
        bReplaced = true;

    if (!bReplaced)
    {
        // then write as normal char
        WriteChar(nChar);
        m_pPiece->SetParaBreak();
        m_pPapPlc->AppendFkpEntry(rStrm.Tell());
        m_pChpPlc->AppendFkpEntry(rStrm.Tell());
        nRetPos = rStrm.Tell();
    }
    return nRetPos;
}

void WW8AttributeOutput::TableRowEnd(sal_uInt32 nDepth)
{
    if ( nDepth == 1 )
        m_rWW8Export.WriteChar( 0x07 );
    else if ( nDepth > 1 )
        m_rWW8Export.WriteChar( 0x0d );

    //Technically in a word document this is a different value for a row ends
    //that are not row ends directly after a cell with a graphic. But it
    //doesn't seem to make a difference
    //pMagicTable->Append(Fc2Cp(Strm().Tell()),0x1B6);
}

void AttributeOutputBase::FormatPageDescription( const SwFormatPageDesc& rPageDesc )
{
    if ( GetExport().m_bStyDef )
        if (auto pC = dynamic_cast< const SwTextFormatColl *>( GetExport().m_pOutFormatNode ) )
        {
            if ( (SfxItemState::SET != pC->GetItemState( RES_BREAK, false ) ) && rPageDesc.KnowsPageDesc() )
                FormatBreak( SvxFormatBreakItem( SvxBreak::PageBefore, RES_BREAK ) );
        }
}

void WW8AttributeOutput::PageBreakBefore( bool bBreak )
{
    // sprmPPageBreakBefore/sprmPFPageBreakBefore
    m_rWW8Export.InsUInt16( NS_sprm::PFPageBreakBefore::val );

    m_rWW8Export.m_pO->push_back( bBreak ? 1 : 0 );
}

/**
 * breaks write nothing in the output field rWrt.pO,
 * but only in the text stream (requirement so they can
 * be called from Out_Break...)
 */
void AttributeOutputBase::FormatBreak( const SvxFormatBreakItem& rBreak )
{
    if ( GetExport().m_bStyDef )
    {
        switch ( rBreak.GetBreak() )
        {
            case SvxBreak::NONE:
            case SvxBreak::PageBefore:
            case SvxBreak::PageBoth:
                PageBreakBefore( rBreak.GetValue() != SvxBreak::NONE );
                break;
            default:
                break;
        }
    }
    else if ( !GetExport().m_pParentFrame )
    {
        sal_uInt8 nC = 0;
        bool bBefore = false;
        // #i76300# - Note: Can only be <true>, if <bBefore> equals <false>.
        bool bCheckForFollowPageDesc = false;

        switch ( rBreak.GetBreak() )
        {
            case SvxBreak::NONE:                                // disabled
                if ( !GetExport().m_bBreakBefore )
                    PageBreakBefore( false );
                return;

            case SvxBreak::ColumnBefore:                       // ColumnBreak
                bBefore = true;
                [[fallthrough]];
            case SvxBreak::ColumnAfter:
            case SvxBreak::ColumnBoth:
                if ( GetExport().m_rDoc.getIDocumentSettingAccess().get( DocumentSettingId::TREAT_SINGLE_COLUMN_BREAK_AS_PAGE_BREAK )
                     || GetExport().Sections().CurrentNumberOfColumns( GetExport().m_rDoc ) > 1 )
                {
                    nC = msword::ColumnBreak;
                }
                break;

            case SvxBreak::PageBefore:                         // PageBreak
                // From now on(fix for #i77900#) we prefer to save a page break
                // as paragraph attribute (if the exporter is OK with that),
                // this has to be done after the export of the paragraph ( =>
                // !GetExport().bBreakBefore )
                if (GetExport().PreferPageBreakBefore())
                {
                    if (!GetExport().m_bBreakBefore)
                        PageBreakBefore(true);
                }
                else
                {
                    bBefore = true;
                    nC = msword::PageBreak;
                }
                break;
            case SvxBreak::PageAfter:
            case SvxBreak::PageBoth:
                nC = msword::PageBreak;
                // #i76300# - check for follow page description,
                // if current writing attributes of a paragraph.
                if ( dynamic_cast< const SwTextNode* >( GetExport().m_pOutFormatNode ) &&
                     GetExport().GetCurItemSet() )
                {
                    bCheckForFollowPageDesc = true;
                }
                break;

            default:
                break;
        }

        if ( ( bBefore == GetExport().m_bBreakBefore ) && nC )
        {
            // #i76300#
            bool bFollowPageDescWritten = false;
            if ( bCheckForFollowPageDesc )
            {
                bFollowPageDescWritten =
                    GetExport().OutputFollowPageDesc( GetExport().GetCurItemSet(),
                            dynamic_cast<const SwTextNode*>( GetExport().m_pOutFormatNode ) );
            }
            if ( !bFollowPageDescWritten )
            {
                SectionBreak(nC, !bBefore);
            }
        }
    }
}

void WW8AttributeOutput::SectionBreak( sal_uInt8 nC, bool /*bBreakAfter*/, const WW8_SepInfo* /*pSectionInfo*/, bool /*bExtraPageBreak*/ )
{
    m_rWW8Export.ReplaceCr( nC );
}

sal_uInt32 AttributeOutputBase::GridCharacterPitch( const SwTextGridItem& rGrid ) const
{
    MSWordStyles * pStyles = GetExport().m_pStyles.get();
    const SwFormat * pSwFormat = pStyles->GetSwFormat(0);

    sal_uInt32 nPageCharSize = 0;

    if (pSwFormat != nullptr)
    {
        nPageCharSize = pSwFormat->GetFormatAttr(RES_CHRATR_FONTSIZE).GetHeight();
    }
    sal_uInt16 nPitch = rGrid.IsSquaredMode() ? rGrid.GetBaseHeight() :
        rGrid.GetBaseWidth( );

    sal_Int32 nCharWidth = nPitch - nPageCharSize;
    sal_Int32 nFraction = nCharWidth % 20;
    if ( nCharWidth < 0 )
        nFraction = 20 + nFraction;
    nFraction = ( nFraction * 0xFFF ) / 20;
    nFraction = ( nFraction & 0x00000FFF );

    sal_Int32 nMain = nCharWidth / 20;
    if ( nCharWidth < 0 )
        nMain -= 1;
    nMain = nMain * 0x1000;
    nMain = ( nMain & 0xFFFFF000 );

    return sal_uInt32( nFraction + nMain );
}

void WW8AttributeOutput::FormatTextGrid( const SwTextGridItem& rGrid )
{
    if (!m_rWW8Export.m_bOutPageDescs)
        return;

    sal_uInt16 nGridType = 0;
    switch ( rGrid.GetGridType() )
    {
        default:
            OSL_FAIL("Unknown grid type");
            [[fallthrough]];
        case SwTextGrid::NONE:
            nGridType = 0;
            break;
        case SwTextGrid::LinesOnly:
            nGridType = 2;
            break;
        case SwTextGrid::LinesAndChars:
            if ( rGrid.IsSnapToChars() )
                nGridType = 3;
            else
                nGridType = 1;
            break;
    }
    m_rWW8Export.InsUInt16( NS_sprm::SClm::val );
    m_rWW8Export.InsUInt16( nGridType );

    sal_uInt16 nHeight = rGrid.GetBaseHeight() + rGrid.GetRubyHeight();
    m_rWW8Export.InsUInt16( NS_sprm::SDyaLinePitch::val );
    m_rWW8Export.InsUInt16( nHeight );

    m_rWW8Export.InsUInt16( NS_sprm::SDxtCharSpace::val );
    m_rWW8Export.InsUInt32( GridCharacterPitch( rGrid ) );
}

void WW8AttributeOutput::FormatPaperBin( const SvxPaperBinItem& rPaperBin )
{
    if ( !m_rWW8Export.m_bOutPageDescs )
        return;

    sal_uInt16 nVal;
    switch ( rPaperBin.GetValue() )
    {
        case 0: nVal = 15;  break;      // Automatically select
        case 1: nVal = 1;   break;      // Upper paper tray
        case 2: nVal = 4;   break;      // Manual paper feed
        default: nVal = 0;  break;
    }

    if ( nVal )
    {
        m_rWW8Export.InsUInt16( m_rWW8Export.m_bOutFirstPage
                ? NS_sprm::SDmBinFirst::val : NS_sprm::SDmBinOther::val );

        m_rWW8Export.InsUInt16( nVal );
    }
}

void WW8AttributeOutput::FormatFirstLineIndent(SvxFirstLineIndentItem const& rFirstLine)
{
    auto stOffset = rFirstLine.GetTextFirstLineOffset();
    if (stOffset.m_nUnit == css::util::MeasureUnit::FONT_CJK_ADVANCE)
    {
        // sprmPDxcLeft1
        m_rWW8Export.InsUInt16(0x4457);
        m_rWW8Export.InsInt16(stOffset.m_dValue * 100.0);
    }
    else
    {
        // sprmPDxaLeft1
        m_rWW8Export.InsUInt16(0x8460);
        m_rWW8Export.InsUInt16(rFirstLine.ResolveTextFirstLineOffset({}));
    }
}

void WW8AttributeOutput::FormatTextLeftMargin(SvxTextLeftMarginItem const& rTextLeftMargin)
{
    auto stOffset = rTextLeftMargin.GetTextLeft();
    if (stOffset.m_nUnit == css::util::MeasureUnit::FONT_CJK_ADVANCE)
    {
        // tdf#80596: DOC stores sprmPDxaLeft and sprmPDxcLeft differently with
        // hanging indentation. The left margin must be adjusted before exporting.
        const SfxItemSet* pSet = GetExport().m_pISet;
        if (pSet && pSet->HasItem(RES_MARGIN_FIRSTLINE))
        {
            const SvxFirstLineIndentItem* pItem = pSet->GetItem(RES_MARGIN_FIRSTLINE);
            auto stFirstLine = pItem->GetTextFirstLineOffset();
            if (stFirstLine.m_nUnit == css::util::MeasureUnit::FONT_CJK_ADVANCE
                && stFirstLine.m_dValue < 0.0)
            {
                stOffset.m_dValue += stFirstLine.m_dValue;
            }
        }

        // sprmPDxcLeft
        m_rWW8Export.InsUInt16(0x4456);
        m_rWW8Export.InsInt16(stOffset.m_dValue * 100.0);
    }
    else
    {
        // sprmPDxaLeft
        m_rWW8Export.InsUInt16(0x845E);
        m_rWW8Export.InsUInt16(rTextLeftMargin.ResolveTextLeft({}));
    }
}

void WW8AttributeOutput::FormatRightMargin(SvxRightMarginItem const& rRightMargin)
{
    auto stOffset = rRightMargin.GetRight();
    if (stOffset.m_nUnit == css::util::MeasureUnit::FONT_CJK_ADVANCE)
    {
        // sprmPDxcRight
        m_rWW8Export.InsUInt16(0x4455);
        m_rWW8Export.InsInt16(stOffset.m_dValue * 100.0);
    }
    else
    {
        // sprmPDxaRight
        m_rWW8Export.InsUInt16(0x845D);
        m_rWW8Export.InsUInt16(rRightMargin.ResolveRight({}));
    }
}

void WW8AttributeOutput::FormatLRSpace( const SvxLRSpaceItem& rLR )
{
    // Flys are still missing ( see RTF )

    if ( m_rWW8Export.m_bOutFlyFrameAttrs )                   // Flys
    {
        // sprmPDxaFromText10
        m_rWW8Export.InsUInt16( NS_sprm::LN_PDxaFromText10 );
        // use average, since WW only knows one value
        m_rWW8Export.InsUInt16(
            (rLR.ResolveLeft({}) + rLR.ResolveRight({})) / 2);
    }
    else if ( m_rWW8Export.m_bOutPageDescs )                // PageDescs
    {
        m_pageMargins.nLeft = 0;
        m_pageMargins.nRight = 0;

        if ( const SvxBoxItem* pBoxItem = m_rWW8Export.HasItem( RES_BOX ) )
        {
            m_pageMargins.nLeft = pBoxItem->CalcLineSpace( SvxBoxItemLine::LEFT, /*bEvenIfNoLine*/true );
            m_pageMargins.nRight = pBoxItem->CalcLineSpace( SvxBoxItemLine::RIGHT, /*bEvenIfNoLine*/true );
        }

        m_pageMargins.nLeft += sal::static_int_cast<sal_uInt16>(rLR.ResolveLeft({}));
        m_pageMargins.nRight += sal::static_int_cast<sal_uInt16>(rLR.ResolveRight({}));
        sal_uInt16 nGutter = rLR.GetGutterMargin();

        // sprmSDxaLeft
        m_rWW8Export.InsUInt16( NS_sprm::SDxaLeft::val );
        m_rWW8Export.InsUInt16( m_pageMargins.nLeft );

        // sprmSDxaRight
        m_rWW8Export.InsUInt16( NS_sprm::SDxaRight::val );
        m_rWW8Export.InsUInt16( m_pageMargins.nRight );

        if (nGutter)
        {
            // sprmSDzaGutter
            m_rWW8Export.InsUInt16(NS_sprm::SDzaGutter::val);
            m_rWW8Export.InsUInt16(nGutter);
        }
    }
    else
    {                                          // normal paragraphs
        if (rLR.GetTextLeft().m_nUnit == css::util::MeasureUnit::FONT_CJK_ADVANCE)
        {
            // sprmPDxcLeft
            m_rWW8Export.InsUInt16(0x4456);
            m_rWW8Export.InsUInt16(rLR.GetTextLeft().m_dValue * 100.0);
        }
        else
        {
            // sprmPDxaLeft
            m_rWW8Export.InsUInt16(0x845E);
            m_rWW8Export.InsUInt16(rLR.ResolveTextLeft({}));
        }

        if (rLR.GetRight().m_nUnit == css::util::MeasureUnit::FONT_CJK_ADVANCE)
        {
            // sprmPDxcRight
            m_rWW8Export.InsUInt16(0x4455);
            m_rWW8Export.InsUInt16(rLR.GetRight().m_dValue * 100.0);
        }
        else
        {
            // sprmPDxaRight
            m_rWW8Export.InsUInt16(0x845D);
            m_rWW8Export.InsUInt16(rLR.ResolveRight({}));
        }

        if (rLR.GetTextFirstLineOffset().m_nUnit == css::util::MeasureUnit::FONT_CJK_ADVANCE)
        {
            // sprmPDxcLeft1
            m_rWW8Export.InsUInt16(0x4457);
            m_rWW8Export.InsUInt16(
                rLR.GetTextFirstLineOffset().m_dValue * 100.0);
        }
        else
        {
            // sprmPDxaLeft1
            m_rWW8Export.InsUInt16(0x8460);
            m_rWW8Export.InsUInt16(rLR.ResolveTextFirstLineOffset({}));
        }
    }
}

void WW8AttributeOutput::SectionRtlGutter(const SfxBoolItem& rRtlGutter)
{
    if (!rRtlGutter.GetValue())
    {
        return;
    }

    // sprmSFRTLGutter
    m_rWW8Export.InsUInt16(NS_sprm::SFRTLGutter::val);
    m_rWW8Export.m_pO->push_back(1);
}

void WW8AttributeOutput::TextLineBreak(const SwFormatLineBreak& rLineBreak)
{
    // Write the linebreak itself.
    m_rWW8Export.WriteChar(0x0b);

    // sprmCLbcCRJ
    m_rWW8Export.InsUInt16(NS_sprm::CLbcCRJ::val);
    m_rWW8Export.m_pO->push_back(rLineBreak.GetEnumValue());
}

void WW8AttributeOutput::FormatULSpace( const SvxULSpaceItem& rUL )
{
    // Flys are still missing ( see RTF )

    if ( m_rWW8Export.m_bOutFlyFrameAttrs )                   // Flys
    {
        // sprmPDyaFromText
        m_rWW8Export.InsUInt16( NS_sprm::PDyaFromText::val );
        // use average, since WW only knows one value
        m_rWW8Export.InsUInt16( ( rUL.GetUpper() + rUL.GetLower() ) / 2 );
    }
    else if ( m_rWW8Export.m_bOutPageDescs )            // Page-UL
    {
        OSL_ENSURE( m_rWW8Export.GetCurItemSet(), "Impossible" );
        if ( !m_rWW8Export.GetCurItemSet() )
            return;

        HdFtDistanceGlue aDistances( *m_rWW8Export.GetCurItemSet() );

        if ( aDistances.HasHeader() )
        {
            //sprmSDyaHdrTop
            m_rWW8Export.InsUInt16( NS_sprm::SDyaHdrTop::val );
            m_rWW8Export.InsUInt16( aDistances.m_DyaHdrTop );
        }

        // sprmSDyaTop
        m_rWW8Export.InsUInt16( NS_sprm::SDyaTop::val );
        m_rWW8Export.InsUInt16( aDistances.m_DyaTop );
        m_pageMargins.nTop = aDistances.m_DyaTop;

        if ( aDistances.HasFooter() )
        {
            //sprmSDyaHdrBottom
            m_rWW8Export.InsUInt16( NS_sprm::SDyaHdrBottom::val );
            m_rWW8Export.InsUInt16( aDistances.m_DyaHdrBottom );
        }

        //sprmSDyaBottom
        m_rWW8Export.InsUInt16( NS_sprm::SDyaBottom::val );
        m_rWW8Export.InsUInt16( aDistances.m_DyaBottom );
        m_pageMargins.nBottom = aDistances.m_DyaBottom;
    }
    else
    {
        // sprmPDyaBefore
        m_rWW8Export.InsUInt16( NS_sprm::PDyaBefore::val );
        m_rWW8Export.InsUInt16( rUL.GetUpper() );
        // sprmPDyaAfter
        m_rWW8Export.InsUInt16( NS_sprm::PDyaAfter::val );
        m_rWW8Export.InsUInt16( rUL.GetLower() );
        // sprmPFContextualSpacing

        // Write out Contextual Spacing = false if it would have inherited a true.
        const SvxULSpaceItem* pInherited = nullptr;
        if (!rUL.GetContext())
        {
            if (auto pNd = dynamic_cast<const SwContentNode*>(m_rWW8Export.m_pOutFormatNode)) //paragraph
                pInherited = &static_cast<SwTextFormatColl&>(pNd->GetAnyFormatColl()).GetAttrSet().GetULSpace();
            else if (m_rWW8Export.m_bStyDef && m_rWW8Export.m_pCurrentStyle && m_rWW8Export.m_pCurrentStyle->DerivedFrom()) //style
                pInherited = &m_rWW8Export.m_pCurrentStyle->DerivedFrom()->GetULSpace();
        }
        if (rUL.GetContext() || (pInherited && pInherited->GetContext()))
        {
            m_rWW8Export.InsUInt16(NS_sprm::PFContextualSpacing::val);
            m_rWW8Export.m_pO->push_back( static_cast<sal_uInt8>(rUL.GetContext()) );
        }
    }
}

// print, opaque, protect are still missing

void WW8AttributeOutput::FormatSurround( const SwFormatSurround& rSurround )
{
    if ( m_rWW8Export.m_bOutFlyFrameAttrs )
    {
        m_rWW8Export.InsUInt16( NS_sprm::PWr::val );

        m_rWW8Export.m_pO->push_back(
                ( css::text::WrapTextMode_NONE != rSurround.GetSurround() ) ? 2 : 1 );
    }
}

void WW8AttributeOutput::FormatVertOrientation( const SwFormatVertOrient& rFlyVert )
{

    //!!!! anchor type and corresponding borders are still missing
    if ( !m_rWW8Export.m_bOutFlyFrameAttrs )
        return;

    short nPos;
    switch( rFlyVert.GetVertOrient() )
    {
        case text::VertOrientation::NONE:
            nPos = static_cast<short>(rFlyVert.GetPos());
            break;
        case text::VertOrientation::CENTER:
        case text::VertOrientation::LINE_CENTER:
            nPos = -8;
            break;
        case text::VertOrientation::BOTTOM:
        case text::VertOrientation::LINE_BOTTOM:
            nPos = -12;
            break;
        case text::VertOrientation::TOP:
        case text::VertOrientation::LINE_TOP:
        default:
            nPos = -4;
            break;
    }

    // sprmPDyaAbs
    m_rWW8Export.InsUInt16( NS_sprm::PDyaAbs::val );
    m_rWW8Export.InsUInt16( nPos );
}

void WW8AttributeOutput::FormatHorizOrientation( const SwFormatHoriOrient& rFlyHori )
{
    if ( !m_rWW8Export.m_pParentFrame )
    {
        OSL_ENSURE( m_rWW8Export.m_pParentFrame, "HoriOrient without mpParentFrame !!" );
        return;
    }

    //!!!! anchor type and corresponding borders are still missing
    if ( !m_rWW8Export.m_bOutFlyFrameAttrs )
        return;

    short nPos;
    switch( rFlyHori.GetHoriOrient() )
    {
        case text::HoriOrientation::NONE:
            nPos = static_cast<short>(rFlyHori.GetPos());
            if( !nPos )
                nPos = 1;   // WW: 0 is reserved
            break;
        case text::HoriOrientation::LEFT:
            nPos = rFlyHori.IsPosToggle() ? -12 : 0;
            break;
        case text::HoriOrientation::RIGHT:
            nPos = rFlyHori.IsPosToggle() ? -16 : -8;
            break;
        case text::HoriOrientation::CENTER:
        case text::HoriOrientation::FULL: // FULL only for tables
        default:
            nPos = -4;
            break;
    }

    // sprmPDxaAbs
    m_rWW8Export.InsUInt16( NS_sprm::PDxaAbs::val );
    m_rWW8Export.InsUInt16( nPos );
}

void WW8AttributeOutput::FormatAnchor( const SwFormatAnchor& rAnchor )
{
    OSL_ENSURE( m_rWW8Export.m_pParentFrame, "Anchor without mpParentFrame !!" );

    if ( !m_rWW8Export.m_bOutFlyFrameAttrs )
        return;

    sal_uInt8 nP = 0;
    switch ( rAnchor.GetAnchorId() )
    {
        case RndStdIds::FLY_AT_PAGE:
            // vertical: page | horizontal: page
            nP |= (1 << 4) | (2 << 6);
            break;
        // in case of Fly as characters: set paragraph-bound!!!
        case RndStdIds::FLY_AT_FLY:
        case RndStdIds::FLY_AT_CHAR:
        case RndStdIds::FLY_AT_PARA:
        case RndStdIds::FLY_AS_CHAR:
            // vertical: page | horizontal: page
            nP |= (2 << 4) | (0 << 6);
            break;
        default:
            break;
    }

    // sprmPPc
    m_rWW8Export.InsUInt16( NS_sprm::PPc::val );
    m_rWW8Export.m_pO->push_back( nP );
}

void WW8AttributeOutput::FormatBackground( const SvxBrushItem& rBrush )
{
    // WW cannot have background in a section
    if ( m_rWW8Export.m_bOutPageDescs )
        return;

    WW8_SHD aSHD;
    WW8Export::TransBrush( rBrush.GetColor(), aSHD );

    m_rWW8Export.InsUInt16( NS_sprm::PShd80::val );
    m_rWW8Export.InsUInt16( aSHD.GetValue() );

    m_rWW8Export.InsUInt16( NS_sprm::PShd::val );
    m_rWW8Export.m_pO->push_back( 10 ); //size of operand: MUST be 10
    m_rWW8Export.InsUInt32( 0xFF000000 ); //cvFore: Foreground BGR = cvAuto
    m_rWW8Export.InsUInt32( SuitableBGColor( rBrush.GetColor() ) ); //cvBack
    m_rWW8Export.InsUInt16( 0x0000 ); //iPat: specifies the pattern used for shading = clear/100% background
}

void WW8AttributeOutput::FormatFillStyle( const XFillStyleItem& rFillStyle )
{
    mbFillStyleIsSet = true;

    // WW cannot have background in a section
    if ( m_rWW8Export.m_bOutPageDescs )
        return;

    // see MSWordExportBase::OutputItemSet for how _SOLID is handled
    if ( rFillStyle.GetValue() != drawing::FillStyle_NONE )
        return;

    //Shd80Nil
    m_rWW8Export.InsUInt16( NS_sprm::PShd80::val );
    m_rWW8Export.InsUInt16( 0xffff );

    //cvAuto
    m_rWW8Export.InsUInt16( NS_sprm::PShd::val );
    m_rWW8Export.m_pO->push_back( 10 );
    m_rWW8Export.InsUInt32( 0xFF000000 );
    m_rWW8Export.InsUInt32( 0xFF000000 );
    m_rWW8Export.InsUInt16( 0x0000 );
}

void WW8AttributeOutput::FormatFillGradient( const XFillGradientItem& /*rFillGradient*/ )
{
    assert(mbFillStyleIsSet && "ITEM: FormatFillStyle *has* to be called before FormatFillGradient(!)");
}

WW8_BRCVer9 WW8Export::TranslateBorderLine(const SvxBorderLine& rLine,
    sal_uInt16 nDist, bool bShadow)
{
    sal_uInt32 nColBGR = 0;
    sal_uInt16 nWidth = ::editeng::ConvertBorderWidthToWord(
            rLine.GetBorderLineStyle(), rLine.GetWidth());
    sal_uInt8 brcType = 0;

    if( nWidth ) // line ?
    {
        // BRC.brcType
        brcType = 0;
        // All the border types values are available on
        // http://msdn.microsoft.com/en-us/library/dd908142%28v=office.12%29.aspx
        switch (rLine.GetBorderLineStyle())
        {
            case SvxBorderLineStyle::SOLID:
                {
                    if ( rLine.GetWidth( ) == SvxBorderLineWidth::Hairline )
                        brcType = 5;
                    else
                        brcType = 1;
                }
                break;
            case SvxBorderLineStyle::DOTTED:
                brcType = 6;
                break;
            case SvxBorderLineStyle::DASHED:
                brcType = 7;
                break;
            case SvxBorderLineStyle::DOUBLE:
            case SvxBorderLineStyle::DOUBLE_THIN:
                brcType = 3;
                break;
            case SvxBorderLineStyle::THINTHICK_SMALLGAP:
                brcType = 11;
                break;
            case SvxBorderLineStyle::THINTHICK_MEDIUMGAP:
                brcType = 14;
                break;
            case SvxBorderLineStyle::THINTHICK_LARGEGAP:
                brcType = 17;
                break;
            case SvxBorderLineStyle::THICKTHIN_SMALLGAP:
                brcType = 12;
                break;
            case SvxBorderLineStyle::THICKTHIN_MEDIUMGAP:
                brcType = 15;
                break;
            case SvxBorderLineStyle::THICKTHIN_LARGEGAP:
                brcType = 18;
                break;
            case SvxBorderLineStyle::EMBOSSED:
                brcType = 24;
                break;
            case SvxBorderLineStyle::ENGRAVED:
                brcType = 25;
                break;
            case SvxBorderLineStyle::OUTSET:
                brcType = 26;
                break;
            case SvxBorderLineStyle::INSET:
                brcType = 27;
                break;
            case SvxBorderLineStyle::FINE_DASHED:
                brcType = 22;
                break;
            case SvxBorderLineStyle::DASH_DOT:
                brcType = 8;
                break;
            case SvxBorderLineStyle::DASH_DOT_DOT:
                brcType = 9;
                break;
            default:
                break;
        }

        // convert width from twips (1/20 pt) to eighths of a point
        nWidth = (( nWidth * 8 ) + 10 ) / 20;
        if( 0xff < nWidth )
            nWidth = 0xff;

        if( 0 == nWidth )                       // really thin line
            nWidth = 1;                         // don't omit

        // BRC.cv
        nColBGR = wwUtility::RGBToBGR(rLine.GetColor().GetRGBColor());
    }

    // BRC.dptSpace
    sal_uInt16 nLDist = rtl::math::round(nDist / 20.0); // unit of measurement: pt
    if( nLDist > 0x1f )
        nLDist = 0x1f;

    return WW8_BRCVer9(nColBGR, sal_uInt8(nWidth), brcType, sal_uInt8(nLDist),
        bShadow, false);
}

/**
 * Gets passed a WW8Bytes*, so the function also can be used for the table border.
 *
 * @param nSprmNo  If nSprmNo == 0, then the opcode isn't outputted.
 * @param bShadow  SHOULDN'T be set for table cells !
 */
void WW8Export::Out_BorderLine(ww::bytes& rO, const SvxBorderLine* pLine,
    sal_uInt16 nDist, sal_uInt16 nSprmNo, sal_uInt16 nSprmNoVer9, bool bShadow)
{
    OSL_ENSURE( ( nSprmNo == 0 ) ||
            ( nSprmNo >= 38 && nSprmNo <= 41 ) ||
            ( nSprmNo >= NS_sprm::PBrcTop80::val
              && nSprmNo <= NS_sprm::PBrcRight80::val ) ||
            ( nSprmNo >= NS_sprm::SBrcTop80::val
              && nSprmNo <= NS_sprm::SBrcRight80::val ),
            "Sprm for border out is of range" );

    WW8_BRCVer9 aBrcVer9;
    WW8_BRC aBrcVer8;

    if( pLine && pLine->GetBorderLineStyle() != SvxBorderLineStyle::NONE )
    {
        aBrcVer9 = TranslateBorderLine( *pLine, nDist, bShadow );
        sal_uInt8 ico = msfilter::util::TransColToIco( msfilter::util::BGRToRGB(aBrcVer9.cv()) );
        aBrcVer8 = WW8_BRC( aBrcVer9.dptLineWidth(), aBrcVer9.brcType(), ico,
            aBrcVer9.dptSpace(), aBrcVer9.fShadow(), aBrcVer9.fFrame() );
    }

    // WW97-SprmIds
    if ( nSprmNo != 0 )
        SwWW8Writer::InsUInt16( rO, nSprmNo );

    rO.insert( rO.end(), aBrcVer8.aBits1, aBrcVer8.aBits2+2 );

    if ( nSprmNoVer9 != 0 )
    {
        SwWW8Writer::InsUInt16( rO, nSprmNoVer9 );
        rO.push_back(sizeof(WW8_BRCVer9));
        rO.insert( rO.end(), aBrcVer9.aBits1, aBrcVer9.aBits2+4);
    }
}

/**
 * is for all boxes except in tables. pO of the WW8Writer is used
 *
 * @param rBox
 * @param bShadow
 */
void WW8Export::Out_SwFormatBox(const SvxBoxItem& rBox, bool bShadow)
{
    static const SvxBoxItemLine aBorders[] =
    {
        SvxBoxItemLine::TOP, SvxBoxItemLine::LEFT, SvxBoxItemLine::BOTTOM, SvxBoxItemLine::RIGHT
    };
    static const sal_uInt16 aPBrc[] =
    {
        // WW8 SPRMs
        NS_sprm::PBrcTop80::val, NS_sprm::PBrcLeft80::val,
        NS_sprm::PBrcBottom80::val, NS_sprm::PBrcRight80::val,
        // WW9 SPRMs
        NS_sprm::PBrcTop::val, NS_sprm::PBrcLeft::val,
        NS_sprm::PBrcBottom::val, NS_sprm::PBrcRight::val
    };
    static const sal_uInt16 aSBrc[] =
    {
        // WW8 SPRMs
        NS_sprm::SBrcTop80::val, NS_sprm::SBrcLeft80::val,
        NS_sprm::SBrcBottom80::val, NS_sprm::SBrcRight80::val,
        // WW9 SPRMs
        NS_sprm::SBrcTop::val, NS_sprm::SBrcLeft::val,
        NS_sprm::SBrcBottom::val, NS_sprm::SBrcRight::val
    };

    const SvxBoxItemLine* pBrd = aBorders;
    for( sal_uInt16 i = 0; i < 4; ++i, ++pBrd )
    {
        const SvxBorderLine* pLn = rBox.GetLine( *pBrd );

        sal_uInt16 nSprmNo, nSprmNoVer9 = 0;
        if (m_bOutPageDescs)
        {
            nSprmNo = aSBrc[i];
            nSprmNoVer9 = aSBrc[i+4];
        }
        else
        {
            nSprmNo = aPBrc[i];
            nSprmNoVer9 = aPBrc[i+4];
        }

        Out_BorderLine( *m_pO, pLn, rBox.GetDistance( *pBrd ), nSprmNo,
            nSprmNoVer9, bShadow );
    }
}

/**
 * FormatBox2() is for TC structures in tables. The Sprm opcode isn't written
 * because it is packed into the TC structure without opcode.
 * dxpSpace always becomes 0, because WW requires that in tables
 * ( table borders otherwise will fray )
 *
 * @param rO A WW8Bytes pointer is passed in as output parameter
 */
void WW8Export::Out_SwFormatTableBox( ww::bytes& rO, const SvxBoxItem * pBox )
{
    // possible and maybe better would be 0xffff
    static const SvxBoxItemLine aBorders[] =
    {
        SvxBoxItemLine::TOP, SvxBoxItemLine::LEFT, SvxBoxItemLine::BOTTOM, SvxBoxItemLine::RIGHT
    };
    static const SvxBorderLine aBorderLine;

    for(const SvxBoxItemLine & rBorder : aBorders)
    {
        const SvxBorderLine* pLn;
        if (pBox != nullptr)
            pLn = pBox->GetLine( rBorder );
        else
            pLn = & aBorderLine;

        Out_BorderLine(rO, pLn, 0, 0, 0, false);
    }
}

void WW8Export::Out_CellRangeBorders( const SvxBoxItem * pBox, sal_uInt8 nStart,
       sal_uInt8 nLimit )
{
    if ( !pBox )
        return;

    static const SvxBoxItemLine aBorders[] =
    {
        SvxBoxItemLine::TOP, SvxBoxItemLine::LEFT, SvxBoxItemLine::BOTTOM, SvxBoxItemLine::RIGHT
    };

    for( int i = 0; i < 4; ++i )
    {
        const SvxBorderLine* pLn = pBox->GetLine( aBorders[i] );
        if (!pLn)
            continue;

        InsUInt16( NS_sprm::TSetBrc::val );
        m_pO->push_back( 11 );
        m_pO->push_back( nStart );
        m_pO->push_back( nLimit );
        m_pO->push_back( 1<<i );
        WW8_BRCVer9 aBrcVer9 = TranslateBorderLine( *pLn, 0, false );
        m_pO->insert( m_pO->end(), aBrcVer9.aBits1, aBrcVer9.aBits2+4 );
    }
}

void WW8AttributeOutput::FormatBox( const SvxBoxItem& rBox )
{
    // Fly around graphic -> here no border, because the
    // graphics header already has the border
    if ( m_rWW8Export.m_bOutGrf )
        return;

    bool bShadow = false;
    const SvxShadowItem* pShadowItem = m_rWW8Export.HasItem( RES_SHADOW );
    if ( pShadowItem )
    {
        bShadow = ( pShadowItem->GetLocation() != SvxShadowLocation::NONE )
                  && ( pShadowItem->GetWidth() != 0 );
    }

    SvxBoxItem aBox(rBox);
    if (m_rWW8Export.m_bOutPageDescs)
    {
        editeng::WordBorderDistances aDistances;
        editeng::BorderDistancesToWord(aBox, m_pageMargins, aDistances);

        aBox.SetDistance(aDistances.nTop, SvxBoxItemLine::TOP);
        aBox.SetDistance(aDistances.nLeft, SvxBoxItemLine::LEFT);
        aBox.SetDistance(aDistances.nBottom, SvxBoxItemLine::BOTTOM);
        aBox.SetDistance(aDistances.nRight, SvxBoxItemLine::RIGHT);

        m_bFromEdge = aDistances.bFromEdge;
    }

    m_rWW8Export.Out_SwFormatBox( aBox, bShadow );
}

SwTwips WW8Export::CurrentPageWidth(SwTwips &rLeft, SwTwips &rRight) const
{
    const SwFrameFormat* pFormat = m_pCurrentPageDesc ? &m_pCurrentPageDesc->GetMaster()
        : &m_rDoc.GetPageDesc(0).GetMaster();

    const SvxLRSpaceItem& rLR = pFormat->GetLRSpace();
    SwTwips nPageSize = pFormat->GetFrameSize().GetWidth();
    rLeft = rLR.ResolveLeft({});
    rRight = rLR.ResolveRight({});
    return nPageSize;
}

void WW8AttributeOutput::FormatColumns_Impl( sal_uInt16 nCols, const SwFormatCol & rCol, bool bEven, SwTwips nPageSize )
{
    // CColumns
    m_rWW8Export.InsUInt16( NS_sprm::SCcolumns::val );
    m_rWW8Export.InsUInt16( nCols - 1 );

    // DxaColumns
    m_rWW8Export.InsUInt16( NS_sprm::SDxaColumns::val );
    m_rWW8Export.InsUInt16( rCol.GetGutterWidth( true ) );

    // LBetween
    m_rWW8Export.InsUInt16( NS_sprm::SLBetween::val );
    m_rWW8Export.m_pO->push_back( COLADJ_NONE == rCol.GetLineAdj(  )? 0 : 1 );

    const SwColumns & rColumns = rCol.GetColumns(  );

    // FEvenlySpaced
    m_rWW8Export.InsUInt16( NS_sprm::SFEvenlySpaced::val );
    m_rWW8Export.m_pO->push_back( bEven ? 1 : 0 );

    if ( bEven )
        return;

    for ( sal_uInt16 n = 0; n < nCols; ++n )
    {
        //sprmSDxaColWidth
        m_rWW8Export.InsUInt16( NS_sprm::SDxaColWidth::val );
        m_rWW8Export.m_pO->push_back( static_cast<sal_uInt8>(n) );
        m_rWW8Export.InsUInt16( rCol.
                                CalcPrtColWidth( n,
                                                 o3tl::narrowing<sal_uInt16>(nPageSize) ) );

        if ( n + 1 != nCols )
        {
            //sprmSDxaColSpacing
            m_rWW8Export.InsUInt16( NS_sprm::SDxaColSpacing::val );
            m_rWW8Export.m_pO->push_back( static_cast<sal_uInt8>(n) );
            m_rWW8Export.InsUInt16( rColumns[n].GetRight(  ) +
                                    rColumns[n + 1].GetLeft(  ) );
        }
    }
}

void AttributeOutputBase::FormatColumns( const SwFormatCol& rCol )
{
    const SwColumns& rColumns = rCol.GetColumns();

    sal_uInt16 nCols = rColumns.size();
    if (1 >= nCols || GetExport( ).m_bOutFlyFrameAttrs)
        return;

    // get the page width without borders !!

    const SwFrameFormat* pFormat = GetExport( ).m_pCurrentPageDesc ? &GetExport( ).m_pCurrentPageDesc->GetMaster() : &const_cast<const SwDoc&>(GetExport().m_rDoc).GetPageDesc(0).GetMaster();
    const SvxFrameDirectionItem &frameDirection = pFormat->GetFrameDir();
    SwTwips nPageSize;
    if ( frameDirection.GetValue() == SvxFrameDirection::Vertical_RL_TB || frameDirection.GetValue() == SvxFrameDirection::Vertical_LR_TB )
    {
        const SvxULSpaceItem &rUL = pFormat->GetULSpace();
        nPageSize = pFormat->GetFrameSize().GetHeight();
        nPageSize -= rUL.GetUpper() + rUL.GetLower();

        const SwFormatHeader *header = pFormat->GetAttrSet().GetItem(RES_HEADER);
        if ( header )
        {
            const SwFrameFormat *headerFormat = header->GetHeaderFormat();
            if (headerFormat)
            {
                nPageSize -= headerFormat->GetFrameSize().GetHeight();
            }
        }
        const SwFormatFooter *footer = pFormat->GetAttrSet().GetItem(RES_FOOTER);
        if ( footer )
        {
            const SwFrameFormat *footerFormat = footer->GetFooterFormat();
            if ( footerFormat )
            {
                nPageSize -= footerFormat->GetFrameSize().GetHeight();
            }
        }
    }
    else
    {
        const SvxLRSpaceItem &rLR = pFormat->GetLRSpace();
        nPageSize = pFormat->GetFrameSize().GetWidth();
        nPageSize -= rLR.ResolveLeft({}) + rLR.ResolveRight({});
        //i120133: The Section width should consider page indent value.
        nPageSize -= rCol.GetAdjustValue();

    }

    // look if all columns are equal
    bool bEven = rCol.IsOrtho();
    if (!bEven)
    {
        bEven = true;
        sal_uInt16 n;
        sal_uInt16 nColWidth = rCol.CalcPrtColWidth( 0, o3tl::narrowing<sal_uInt16>(nPageSize) );
        for ( n = 1; n < nCols; n++ )
        {
            short nDiff = nColWidth -
                rCol.CalcPrtColWidth( n, o3tl::narrowing<sal_uInt16>(nPageSize) );

            if ( nDiff > 10 || nDiff < -10 )      // Tolerance: 10 tw
            {
                bEven = false;
                break;
            }
        }
    }

    FormatColumns_Impl( nCols, rCol, bEven, nPageSize );
}

// "Paragraphs together"
void WW8AttributeOutput::FormatKeep( const SvxFormatKeepItem& rKeep )
{
    // sprmFKeepFollow
    m_rWW8Export.InsUInt16( NS_sprm::PFKeepFollow::val );

    m_rWW8Export.m_pO->push_back( rKeep.GetValue() ? 1 : 0 );
}

// exclude a paragraph from Line Numbering
void WW8AttributeOutput::FormatLineNumbering( const SwFormatLineNumber& rNumbering )
{
    // sprmPFNoLineNumb
    m_rWW8Export.InsUInt16( NS_sprm::PFNoLineNumb::val );

    m_rWW8Export.m_pO->push_back( rNumbering.IsCount() ? 0 : 1 );
}

/* File PARATR.HXX  */

void WW8AttributeOutput::ParaLineSpacing_Impl( short nSpace, short nMulti )
{
    // sprmPDyaLine
    m_rWW8Export.InsUInt16( NS_sprm::PDyaLine::val );

    m_rWW8Export.InsUInt16( nSpace );
    m_rWW8Export.InsUInt16( nMulti );
}

void AttributeOutputBase::ParaLineSpacing( const SvxLineSpacingItem& rSpacing )
{
    short nSpace = 240, nMulti = 0;

    switch ( rSpacing.GetLineSpaceRule() )
    {
        default:
            break;
        case SvxLineSpaceRule::Fix: // Fix
            nSpace = -static_cast<short>(rSpacing.GetLineHeight());
            break;
        case SvxLineSpaceRule::Min: // At least
            nSpace = static_cast<short>(rSpacing.GetLineHeight());
            break;
        case SvxLineSpaceRule::Auto:
        {
            if( rSpacing.GetInterLineSpaceRule() == SvxInterLineSpaceRule::Fix ) // Leading
            {
                // doesn't exist in WW - how do you get the MaxLineHeight?
                nSpace = rSpacing.GetInterLineSpace();
                sal_uInt16 nScript =
                    i18n::ScriptType::LATIN;
                const SwAttrSet *pSet = nullptr;
                if ( auto pFormat = dynamic_cast< const SwFormat *>( GetExport().m_pOutFormatNode ) )
                {
                    pSet = &pFormat->GetAttrSet();
                }
                else if ( auto pNd = dynamic_cast< const SwTextNode *>( GetExport().m_pOutFormatNode )  )
                {
                    pSet = &pNd->GetSwAttrSet();
                    nScript = g_pBreakIt->GetBreakIter()->getScriptType(pNd->GetText(), 0);
                }
                OSL_ENSURE( pSet, "No attrset for lineheight :-(" );
                if ( pSet )
                {
                    nSpace = nSpace + static_cast<short>( AttrSetToLineHeight( GetExport().m_rDoc.getIDocumentSettingAccess(),
                        *pSet, *Application::GetDefaultDevice(), nScript ) );
                }
            }
            else // Proportional
            {
                if ( rSpacing.GetInterLineSpaceRule() != SvxInterLineSpaceRule::Off )
                    nSpace = static_cast<short>(round(240.0 * rSpacing.GetPropLineSpace() / 100));
                nMulti = 1;
            }
        }
        break;
    }
    // if nSpace is negative, it is a fixed size in 1/20 of a point
    // if nSpace is positive and nMulti is 1, it is 1/240 of a single line height
    // otherwise, it is a minimum size in 1/20 of a point
    ParaLineSpacing_Impl( nSpace, nMulti );
}

void WW8AttributeOutput::ParaAdjust( const SvxAdjustItem& rAdjust )
{
    // sprmPJc
    sal_uInt8 nAdj;
    sal_uInt8 nAdjBiDi;
    switch ( rAdjust.GetAdjust() )
    {
        case SvxAdjust::Left:
            nAdj = 0;
            nAdjBiDi = 2;
            break;
        case SvxAdjust::Right:
            nAdj = 2;
            nAdjBiDi = 0;
            break;
        case SvxAdjust::BlockLine:
        case SvxAdjust::Block:
            nAdj = nAdjBiDi = rAdjust.GetLastBlock() == SvxAdjust::Block ? 4 : 3;
            break;
        case SvxAdjust::Center:
            nAdj = nAdjBiDi = 1;
            break;
        default:
            return;    // not a supported Attribute
    }

    m_rWW8Export.InsUInt16(NS_sprm::PJc80::val);
    m_rWW8Export.m_pO->push_back(nAdj);

    /*
    Sadly for left to right paragraphs both these values are the same,
    for right to left paragraphs the bidi one is the reverse of the
    normal one.
    */
    m_rWW8Export.InsUInt16(NS_sprm::PJc::val); //bidi version ?
    bool bBiDiSwap = false;
    if (m_rWW8Export.m_pOutFormatNode)
    {
        SvxFrameDirection nDirection = SvxFrameDirection::Horizontal_LR_TB;
        if (auto pTN = dynamic_cast<const SwTextNode*>(m_rWW8Export.m_pOutFormatNode))
        {
            SwPosition aPos(*pTN);
            nDirection = m_rWW8Export.m_rDoc.GetTextDirection(aPos);
        }
        else if (auto pC = dynamic_cast<const SwTextFormatColl*>(m_rWW8Export.m_pOutFormatNode))
        {
            const SvxFrameDirectionItem &rItem = pC->GetFormatAttr(RES_FRAMEDIR);
            nDirection = rItem.GetValue();
        }
        if ( ( nDirection == SvxFrameDirection::Horizontal_RL_TB ) ||
             ( nDirection == SvxFrameDirection::Environment && AllSettings::GetLayoutRTL() ) )
        {
            bBiDiSwap = true;
        }
    }

    if (bBiDiSwap)
        m_rWW8Export.m_pO->push_back(nAdjBiDi);
    else
        m_rWW8Export.m_pO->push_back(nAdj);
}

void WW8AttributeOutput::FormatFrameDirection( const SvxFrameDirectionItem& rDirection )
{
    sal_uInt16 nTextFlow=0;
    bool bBiDi = false;
    SvxFrameDirection nDir = rDirection.GetValue();

    if ( nDir == SvxFrameDirection::Environment )
        nDir = GetExport( ).GetDefaultFrameDirection( );


    switch ( nDir )
    {
        default:
            //Can't get an unknown type here
            OSL_FAIL("Unknown frame direction");
            [[fallthrough]];
        case SvxFrameDirection::Horizontal_LR_TB:
            nTextFlow = 0;
            break;
        case SvxFrameDirection::Horizontal_RL_TB:
            nTextFlow = 0;
            bBiDi = true;
            break;
        case SvxFrameDirection::Vertical_LR_TB:  //word doesn't have this
        case SvxFrameDirection::Vertical_RL_TB:
            nTextFlow = 1;
            break;
    }

    if ( m_rWW8Export.m_bOutPageDescs )
    {
        m_rWW8Export.InsUInt16( NS_sprm::STextFlow::val );
        m_rWW8Export.InsUInt16( nTextFlow );
        m_rWW8Export.InsUInt16( NS_sprm::SFBiDi::val );
        m_rWW8Export.m_pO->push_back( bBiDi ? 1 : 0 );
    }
    else if ( !m_rWW8Export.m_bOutFlyFrameAttrs )  //paragraph/style
    {
        m_rWW8Export.InsUInt16( NS_sprm::PFBiDi::val );
        m_rWW8Export.m_pO->push_back( bBiDi ? 1 : 0 );
    }
}

void WW8AttributeOutput::ParaGrabBag(const SfxGrabBagItem& /*rItem*/)
{
}

void WW8AttributeOutput::CharGrabBag(const SfxGrabBagItem& /*rItem*/)
{
}

void WW8AttributeOutput::ParaOutlineLevel(const SfxUInt16Item& rItem)
{
    sal_uInt16 nOutLvl = std::min(rItem.GetValue(), sal_uInt16(WW8ListManager::nMaxLevel));
    // Outline Level: in LO Body Text = 0, in MS Body Text = 9
    nOutLvl = nOutLvl ? nOutLvl - 1 : 9;
    m_rWW8Export.InsUInt16( NS_sprm::POutLvl::val );
    m_rWW8Export.m_pO->push_back( nOutLvl );
}

// "Separate paragraphs"
void WW8AttributeOutput::ParaSplit( const SvxFormatSplitItem& rSplit )
{
    // sprmPFKeep
    m_rWW8Export.InsUInt16( NS_sprm::PFKeep::val );
    m_rWW8Export.m_pO->push_back( rSplit.GetValue() ? 0 : 1 );
}

/**
 * Only convert the item "SvxWidowItem" and not the orphans, because
 * in WW only one attribute "paragraph control" exists for both and
 * in SW probably both or none is set by the user.
 */
void WW8AttributeOutput::ParaWidows( const SvxWidowsItem& rWidows )
{
    // sprmPFWidowControl
    m_rWW8Export.InsUInt16( NS_sprm::PFWidowControl::val );
    m_rWW8Export.m_pO->push_back( rWidows.GetValue() ? 1 : 0 );
}

namespace {

class SwWW8WrTabu
{
    std::unique_ptr<sal_uInt8[]> m_pDel;            // DelArray
    std::unique_ptr<sal_uInt8[]> m_pAddPos;         // AddPos-Array
    std::unique_ptr<sal_uInt8[]> m_pAddTyp;         // AddTyp-Array
    sal_uInt16 m_nAdd;            // number of tabs to be added
    sal_uInt16 m_nDel;            // number of tabs to be deleted

    SwWW8WrTabu(const SwWW8WrTabu&) = delete;
    SwWW8WrTabu& operator=(const SwWW8WrTabu&) = delete;

public:
    SwWW8WrTabu(sal_uInt16 nDelMax, sal_uInt16 nAddMax);

    void Add(const SvxTabStop &rTS, tools::Long nAdjustment);
    void Del(const SvxTabStop &rTS, tools::Long nAdjustment);
    void PutAll(WW8Export& rWW8Wrt);
};

}

SwWW8WrTabu::SwWW8WrTabu(sal_uInt16 nDelMax, sal_uInt16 nAddMax)
    : m_nAdd(0), m_nDel(0)
{
    if (nDelMax)
        m_pDel.reset( new sal_uInt8[nDelMax * 2] );
    m_pAddPos.reset( new sal_uInt8[nAddMax * 2] );
    m_pAddTyp.reset( new sal_uInt8[nAddMax] );
}

/**
 * insert a tab in the WW structure
 */
void SwWW8WrTabu::Add(const SvxTabStop & rTS, tools::Long nAdjustment)
{
    // insert tab position
    ShortToSVBT16(msword_cast<sal_Int16>(rTS.GetTabPos() + nAdjustment),
        m_pAddPos.get() + (m_nAdd * 2));

    // insert tab type
    sal_uInt8 nPara = 0;
    switch (rTS.GetAdjustment())
    {
        case SvxTabAdjust::Right:
            nPara = 2;
            break;
        case SvxTabAdjust::Center:
            nPara = 1;
            break;
        case SvxTabAdjust::Decimal:
            /*
            There is nothing we can do btw the decimal separator has been
            customized, but if you think different remember that different
            locales have different separators, i.e. german is a , while english
            is a .
            */
            nPara = 3;
            break;
        default:
            break;
    }

    switch( rTS.GetFill() )
    {
        case '.':   // dotted leader
            nPara |= 1 << 3;
            break;
        case '_':   // Single line leader
            nPara |= 3 << 3;
            break;
        case '-':   // hyphenated leader
            nPara |= 2 << 3;
            break;
        case '=':   // heavy line leader
            nPara |= 4 << 3;
            break;
    }

    m_pAddTyp[m_nAdd] = nPara;
    ++m_nAdd;
}

/**
 * Insert a to be deleted tab in the WW structure
 */
void SwWW8WrTabu::Del(const SvxTabStop &rTS, tools::Long nAdjustment)
{
    // insert tab position
    ShortToSVBT16(msword_cast<sal_Int16>(rTS.GetTabPos() + nAdjustment),
        m_pDel.get() + (m_nDel * 2));
    ++m_nDel;
}

/**
 * Writes the attribute to rWrt.pO
 */
void SwWW8WrTabu::PutAll(WW8Export& rWrt)
{
    if (!m_nAdd && !m_nDel) //If it's a no-op
        return;
    OSL_ENSURE(m_nAdd <= 255, "more than 255 added tabstops?");
    OSL_ENSURE(m_nDel <= 255, "more than 244 removed tabstops?");
    if (m_nAdd > 255)
        m_nAdd = 255;
    if (m_nDel > 255)
        m_nDel = 255;

    sal_uInt16 nSiz = 2 * m_nDel + 3 * m_nAdd + 2;
    if (nSiz > 255)
        nSiz = 255;

    rWrt.InsUInt16(NS_sprm::PChgTabsPapx::val);
    // insert cch
    rWrt.m_pO->push_back(msword_cast<sal_uInt8>(nSiz));
    // write DelArr
    rWrt.m_pO->push_back(msword_cast<sal_uInt8>(m_nDel));
    rWrt.OutSprmBytes(m_pDel.get(), m_nDel * 2);
    // write InsArr
    rWrt.m_pO->push_back(msword_cast<sal_uInt8>(m_nAdd));
    rWrt.OutSprmBytes(m_pAddPos.get(), 2 * m_nAdd);         // AddPosArray
    rWrt.OutSprmBytes(m_pAddTyp.get(), m_nAdd);             // AddTypArray
}

static void ParaTabStopAdd( WW8Export& rWrt,
                            const SvxTabStopItem& rTStops,
                            const tools::Long nLParaMgn )
{
    SwWW8WrTabu aTab( 0, rTStops.Count());

    for( sal_uInt16 n = 0; n < rTStops.Count(); n++ )
    {
        const SvxTabStop& rTS = rTStops[n];
        // ignore default tabs
        if (SvxTabAdjust::Default != rTS.GetAdjustment())
            aTab.Add(rTS, nLParaMgn);
    }
    aTab.PutAll( rWrt );
}

static bool lcl_IsEqual(tools::Long nOneLeft, const SvxTabStop &rOne,
    tools::Long nTwoLeft, const SvxTabStop &rTwo)
{
    return(
            nOneLeft == nTwoLeft &&
            rOne.GetAdjustment() == rTwo.GetAdjustment() &&
            rOne.GetDecimal() == rTwo.GetDecimal() &&
            rOne.GetFill() == rTwo.GetFill()
          );
}

static void ParaTabStopDelAdd( WW8Export& rWrt,
                               const SvxTabStopItem& rTStyle,
                               const tools::Long nLStypeMgn,
                               const SvxTabStopItem& rTNew,
                               const tools::Long nLParaMgn )
{
    SwWW8WrTabu aTab(rTStyle.Count(), rTNew.Count());

    sal_uInt16 nO = 0;      // rTStyle Index
    sal_uInt16 nN = 0;      // rTNew Index

    do {
        const SvxTabStop* pTO;
        tools::Long nOP;
        if( nO < rTStyle.Count() )                  // old not yet at the end?
        {
            pTO = &rTStyle[ nO ];
            nOP = pTO->GetTabPos() + nLStypeMgn;
            if( SvxTabAdjust::Default == pTO->GetAdjustment() )
            {
                nO++;                                // ignore default tab
                continue;
            }
        }
        else
        {
            pTO = nullptr;
            nOP = LONG_MAX;
        }

        const SvxTabStop* pTN;
        tools::Long nNP;
        if( nN < rTNew.Count() )                    // new not yet at the end
        {
            pTN = &rTNew[ nN ];
            nNP = pTN->GetTabPos() + nLParaMgn;
            if( SvxTabAdjust::Default == pTN->GetAdjustment() )
            {
                nN++;                               // ignore default tab
                continue;
            }
        }
        else
        {
            pTN = nullptr;
            nNP = LONG_MAX;
        }

        if( nOP == LONG_MAX && nNP == LONG_MAX )
            break;                                  // everything done

        if( nOP < nNP )                             // next tab is old
        {
            assert(pTO);
            aTab.Del(*pTO, nLStypeMgn);             // must be deleted
            nO++;
        }
        else if( nNP < nOP )                        // next tab is new
        {
            assert(pTN);
            aTab.Add(*pTN, nLParaMgn);              // must be inserted
            nN++;
        }
        else
        {
            assert(pTO && pTN);
            if (lcl_IsEqual(nOP, *pTO, nNP, *pTN)) // tabs are equal
            {
                nO++;                                   // nothing to do
                nN++;
            }
            else                                        // tabs same position, different type
            {
                aTab.Del(*pTO, nLStypeMgn);             // delete old one
                aTab.Add(*pTN, nLParaMgn);              // insert new one
                nO++;
                nN++;
            }
        }
    } while( true );

    aTab.PutAll( rWrt );
}

void WW8AttributeOutput::ParaTabStop( const SvxTabStopItem& rTabStops )
{
    const bool bTabsRelativeToIndex = m_rWW8Export.m_pCurPam->GetDoc().getIDocumentSettingAccess().get( DocumentSettingId::TABS_RELATIVE_TO_INDENT );

    tools::Long nCurrentLeft = 0;
    if ( bTabsRelativeToIndex )
    {
        if (const SfxPoolItem* pItem = m_rWW8Export.HasItem(RES_MARGIN_TEXTLEFT))
        {
            if (const auto pLeft = pItem->DynamicWhichCast(RES_MARGIN_TEXTLEFT))
            {
                nCurrentLeft = pLeft->ResolveTextLeft({});
            }
            else
                // FIXME: This fails in sw.ww8export/testCommentExport::Load_Verify_Reload_Verify
                SAL_WARN("sw.ww8", "m_rWW8Export has an RES_LR_SPACE item, but it's of the wrong type.");
        }
    }

    // #i100264#
    if ( m_rWW8Export.m_bStyDef &&
         m_rWW8Export.m_pCurrentStyle != nullptr &&
         m_rWW8Export.m_pCurrentStyle->DerivedFrom() != nullptr )
    {
        SvxTabStopItem aParentTabs( 0, 0, SvxTabAdjust::Default, RES_PARATR_TABSTOP );
        const SwFormat *pParentStyle = m_rWW8Export.m_pCurrentStyle->DerivedFrom();
        {
            if (const SvxTabStopItem* pParentTabs = pParentStyle->GetAttrSet().GetItem<SvxTabStopItem>(RES_PARATR_TABSTOP))
            {
                aParentTabs.Insert( pParentTabs );
            }
        }

        // #i120938# - consider left indentation of style and its parent style
        tools::Long nParentLeft = 0;
        if ( bTabsRelativeToIndex )
        {
            SvxTextLeftMarginItem const& rLeftMargin(pParentStyle->GetAttrSet().Get(RES_MARGIN_TEXTLEFT));
            nParentLeft = rLeftMargin.ResolveTextLeft({});
        }

        ParaTabStopDelAdd( m_rWW8Export, aParentTabs, nParentLeft, rTabStops, nCurrentLeft );
        return;
    }

    const SvxTabStopItem* pStyleTabs = nullptr;
    if ( !m_rWW8Export.m_bStyDef && m_rWW8Export.m_pStyAttr )
    {
        pStyleTabs = m_rWW8Export.m_pStyAttr->GetItem<SvxTabStopItem>(RES_PARATR_TABSTOP);
    }

    if ( !pStyleTabs )
    {
        ParaTabStopAdd(m_rWW8Export, rTabStops, nCurrentLeft);
    }
    else
    {
        tools::Long nStyleLeft = 0;
        if ( bTabsRelativeToIndex )
        {
            SvxTextLeftMarginItem const& rLeftMargin(m_rWW8Export.m_pStyAttr->Get(RES_MARGIN_TEXTLEFT));
            nStyleLeft = rLeftMargin.ResolveTextLeft({});
        }

        ParaTabStopDelAdd( m_rWW8Export,
                           *pStyleTabs, nStyleLeft,
                           rTabStops, nCurrentLeft);
    }
}

void AttributeOutputBase::OutputItem( const SfxPoolItem& rHt )
{
    // FIXME maybe use 'item_cast', like 'item_cast<SvxCharHiddenItem>( rHt )'?
    switch ( rHt.Which() )
    {
        case RES_CHRATR_CASEMAP:
            CharCaseMap(rHt.StaticWhichCast(RES_CHRATR_CASEMAP));
            break;
        case RES_CHRATR_COLOR:
            CharColor(rHt.StaticWhichCast(RES_CHRATR_COLOR));
            break;
        case RES_CHRATR_CONTOUR:
            CharContour(rHt.StaticWhichCast(RES_CHRATR_CONTOUR));
            break;
        case RES_CHRATR_CROSSEDOUT:
            CharCrossedOut(rHt.StaticWhichCast(RES_CHRATR_CROSSEDOUT));
            break;
        case RES_CHRATR_ESCAPEMENT:
            CharEscapement(rHt.StaticWhichCast(RES_CHRATR_ESCAPEMENT));
            break;
        case RES_CHRATR_FONT:
            CharFont(rHt.StaticWhichCast(RES_CHRATR_FONT));
            break;
        case RES_CHRATR_FONTSIZE:
            CharFontSize(rHt.StaticWhichCast(RES_CHRATR_FONTSIZE));
            break;
        case RES_CHRATR_KERNING:
            CharKerning(rHt.StaticWhichCast(RES_CHRATR_KERNING));
            break;
        case RES_CHRATR_LANGUAGE:
            CharLanguage(rHt.StaticWhichCast(RES_CHRATR_LANGUAGE));
            break;
        case RES_CHRATR_POSTURE:
            CharPosture(rHt.StaticWhichCast(RES_CHRATR_POSTURE));
            break;
        case RES_CHRATR_SHADOWED:
            CharShadow(rHt.StaticWhichCast(RES_CHRATR_SHADOWED));
            break;
        case RES_CHRATR_UNDERLINE:
            CharUnderline(rHt.StaticWhichCast(RES_CHRATR_UNDERLINE));
            break;
        case RES_CHRATR_WEIGHT:
            CharWeight(rHt.StaticWhichCast(RES_CHRATR_WEIGHT));
            break;
        case RES_CHRATR_AUTOKERN:
            CharAutoKern(rHt.StaticWhichCast(RES_CHRATR_AUTOKERN));
            break;
        case RES_CHRATR_BLINK:
            CharAnimatedText(rHt.StaticWhichCast(RES_CHRATR_BLINK));
            break;
        case RES_CHRATR_BACKGROUND:
            CharBackgroundBase(rHt.StaticWhichCast(RES_CHRATR_BACKGROUND));
            break;

        case RES_CHRATR_CJK_FONT:
            CharFontCJK(rHt.StaticWhichCast(RES_CHRATR_CJK_FONT));
            break;
        case RES_CHRATR_CJK_FONTSIZE:
            CharFontSizeCJK(rHt.StaticWhichCast(RES_CHRATR_CJK_FONTSIZE));
            break;
        case RES_CHRATR_CJK_LANGUAGE:
            CharLanguageCJK(rHt.StaticWhichCast(RES_CHRATR_CJK_LANGUAGE));
            break;
        case RES_CHRATR_CJK_POSTURE:
            CharPostureCJK(rHt.StaticWhichCast(RES_CHRATR_CJK_POSTURE));
            break;
        case RES_CHRATR_CJK_WEIGHT:
            CharWeightCJK( rHt.StaticWhichCast(RES_CHRATR_CJK_WEIGHT));
            break;

        case RES_CHRATR_CTL_FONT:
            CharFontCTL(rHt.StaticWhichCast(RES_CHRATR_CTL_FONT));
            break;
        case RES_CHRATR_CTL_FONTSIZE:
            CharFontSizeCTL(rHt.StaticWhichCast(RES_CHRATR_CTL_FONTSIZE));
            break;
        case RES_CHRATR_CTL_LANGUAGE:
            CharLanguageCTL(rHt.StaticWhichCast(RES_CHRATR_CTL_LANGUAGE));
            break;
        case RES_CHRATR_CTL_POSTURE:
            CharPostureCTL(rHt.StaticWhichCast(RES_CHRATR_CTL_POSTURE));
            break;
        case RES_CHRATR_CTL_WEIGHT:
            CharWeightCTL(rHt.StaticWhichCast(RES_CHRATR_CTL_WEIGHT));
            break;

        case RES_CHRATR_ROTATE:
            CharRotate(rHt.StaticWhichCast(RES_CHRATR_ROTATE));
            break;
        case RES_CHRATR_EMPHASIS_MARK:
            CharEmphasisMark(rHt.StaticWhichCast(RES_CHRATR_EMPHASIS_MARK));
            break;
        case RES_CHRATR_TWO_LINES:
            CharTwoLines(rHt.StaticWhichCast(RES_CHRATR_TWO_LINES));
            break;
        case RES_CHRATR_SCALEW:
            CharScaleWidth(rHt.StaticWhichCast(RES_CHRATR_SCALEW));
            break;
        case RES_CHRATR_RELIEF:
            CharRelief(rHt.StaticWhichCast(RES_CHRATR_RELIEF));
            break;
        case RES_CHRATR_HIDDEN:
            CharHidden(rHt.StaticWhichCast(RES_CHRATR_HIDDEN));
            break;
        case RES_CHRATR_BOX:
            CharBorder(rHt.StaticWhichCast(RES_CHRATR_BOX));
            break;
        case RES_CHRATR_HIGHLIGHT:
            CharHighlight(rHt.StaticWhichCast(RES_CHRATR_HIGHLIGHT));
            break;
        case RES_CHRATR_BIDIRTL:
            CharBidiRTL( rHt );
            break;
        case RES_CHRATR_SCRIPT_HINT:
            CharScriptHint(rHt.StaticWhichCast(RES_CHRATR_SCRIPT_HINT));
            break;
        case RES_TXTATR_INETFMT:
            TextINetFormat( static_cast< const SwFormatINetFormat& >( rHt ) );
            break;
        case RES_TXTATR_CHARFMT:
            TextCharFormat( static_cast< const SwFormatCharFormat& >( rHt ) );
            break;

        case RES_TXTATR_FIELD:
        case RES_TXTATR_ANNOTATION:
        case RES_TXTATR_INPUTFIELD:
            TextField( static_cast< const SwFormatField& >( rHt ) );
            break;

        case RES_TXTATR_FLYCNT:
            TextFlyContent( static_cast< const SwFormatFlyCnt& >( rHt ) );
            break;
        case RES_TXTATR_FTN:
            TextFootnote( static_cast< const SwFormatFootnote& >( rHt ) );
            break;

        case RES_PARATR_LINESPACING:
            ParaLineSpacing(rHt.StaticWhichCast(RES_PARATR_LINESPACING));
            break;
        case RES_PARATR_ADJUST:
            ParaAdjust(rHt.StaticWhichCast(RES_PARATR_ADJUST));
            break;
        case RES_PARATR_SPLIT:
            ParaSplit(rHt.StaticWhichCast(RES_PARATR_SPLIT));
            break;
        case RES_PARATR_WIDOWS:
            ParaWidows(rHt.StaticWhichCast(RES_PARATR_WIDOWS));
            break;
        case RES_PARATR_TABSTOP:
            ParaTabStop(rHt.StaticWhichCast(RES_PARATR_TABSTOP));
            break;
        case RES_PARATR_HYPHENZONE:
            ParaHyphenZone(rHt.StaticWhichCast(RES_PARATR_HYPHENZONE));
            break;
        case RES_PARATR_NUMRULE:
            ParaNumRule(rHt.StaticWhichCast(RES_PARATR_NUMRULE));
            break;
        case RES_PARATR_SCRIPTSPACE:
            ParaScriptSpace( static_cast< const SfxBoolItem& >( rHt ) );
            break;
        case RES_PARATR_HANGINGPUNCTUATION:
            ParaHangingPunctuation( static_cast< const SfxBoolItem& >( rHt ) );
            break;
        case RES_PARATR_FORBIDDEN_RULES:
            ParaForbiddenRules( static_cast< const SfxBoolItem& >( rHt ) );
            break;
        case RES_PARATR_VERTALIGN:
            ParaVerticalAlign(rHt.StaticWhichCast(RES_PARATR_VERTALIGN));
            break;
        case RES_PARATR_SNAPTOGRID:
            ParaSnapToGrid(rHt.StaticWhichCast(RES_PARATR_SNAPTOGRID));
            break;

        case RES_FRM_SIZE:
            FormatFrameSize( static_cast< const SwFormatFrameSize& >( rHt ) );
            break;
        case RES_PAPER_BIN:
            FormatPaperBin(rHt.StaticWhichCast(RES_PAPER_BIN));
            break;
        case RES_MARGIN_FIRSTLINE:
            FormatFirstLineIndent(rHt.StaticWhichCast(RES_MARGIN_FIRSTLINE));
            break;
        case RES_MARGIN_TEXTLEFT:
            FormatTextLeftMargin(rHt.StaticWhichCast(RES_MARGIN_TEXTLEFT));
            break;
        case RES_MARGIN_RIGHT:
            FormatRightMargin(rHt.StaticWhichCast(RES_MARGIN_RIGHT));
            break;
        case RES_LR_SPACE:
            FormatLRSpace(rHt.StaticWhichCast(RES_LR_SPACE));
            break;
        case RES_UL_SPACE:
            FormatULSpace(rHt.StaticWhichCast(RES_UL_SPACE));
            break;
        case RES_PAGEDESC:
            FormatPageDescription( static_cast< const SwFormatPageDesc& >( rHt ) );
            break;
        case RES_BREAK:
            FormatBreak(rHt.StaticWhichCast(RES_BREAK));
            break;
        case RES_SURROUND:
            FormatSurround( static_cast< const SwFormatSurround& >( rHt ) );
            break;
        case RES_VERT_ORIENT:
            FormatVertOrientation( static_cast< const SwFormatVertOrient& >( rHt ) );
            break;
        case RES_HORI_ORIENT:
            FormatHorizOrientation( static_cast< const SwFormatHoriOrient& >( rHt ) );
            break;
        case RES_ANCHOR:
            FormatAnchor( static_cast< const SwFormatAnchor& >( rHt ) );
            break;
        case RES_BACKGROUND:
            FormatBackground(rHt.StaticWhichCast(RES_BACKGROUND));
            break;
        case XATTR_FILLSTYLE:
            FormatFillStyle( static_cast< const XFillStyleItem& >( rHt ) );
            break;
        case XATTR_FILLGRADIENT:
            FormatFillGradient( static_cast< const XFillGradientItem& >( rHt ) );
            break;
        case RES_BOX:
            FormatBox(rHt.StaticWhichCast(RES_BOX));
            break;
        case RES_COL:
            FormatColumns( static_cast< const SwFormatCol& >( rHt ) );
            break;
        case RES_KEEP:
            FormatKeep(rHt.StaticWhichCast(RES_KEEP));
            break;
        case RES_TEXTGRID:
            FormatTextGrid(rHt.StaticWhichCast(RES_TEXTGRID));
            break;
        case RES_LINENUMBER:
            FormatLineNumberingBase(static_cast<const SwFormatLineNumber&>(rHt));
            break;
        case RES_FRAMEDIR:
            FormatFrameDirection(rHt.StaticWhichCast(RES_FRAMEDIR));
            break;
        case RES_PARATR_GRABBAG:
            ParaGrabBag(rHt.StaticWhichCast(RES_PARATR_GRABBAG));
            break;
        case RES_PARATR_OUTLINELEVEL:
            ParaOutlineLevelBase(rHt.StaticWhichCast(RES_PARATR_OUTLINELEVEL));
            break;
        case RES_CHRATR_GRABBAG:
            CharGrabBag(rHt.StaticWhichCast(RES_CHRATR_GRABBAG));
            break;
        case RES_RTL_GUTTER:
            SectionRtlGutter(rHt.StaticWhichCast(RES_RTL_GUTTER));
            break;
        case RES_TXTATR_LINEBREAK:
            TextLineBreak(static_cast<const SwFormatLineBreak&>(rHt));
            break;

        default:
            SAL_INFO("sw.ww8", "Unhandled SfxPoolItem with id " << rHt.Which() );
            break;
    }
}

void AttributeOutputBase::OutputStyleItemSet( const SfxItemSet& rSet, bool bTestForDefault )
{
    // based on OutputItemSet() from wrt_fn.cxx

    const SfxItemPool& rPool = *rSet.GetPool();
    const SfxItemSet* pSet = &rSet;
    if ( !pSet->Count() )
    {
        while ( nullptr != ( pSet = pSet->GetParent() ) && !pSet->Count() )
            ;

        if ( !pSet )
            return;
    }

    const SfxPoolItem* pItem;
    if ( !pSet->GetParent() )
    {
        assert(rSet.Count() && "Was already handled or?");
        // ITEM: Here SfxItemIter is used, but the called functionality
        // is dependent on the 'order' in which the Items are processed. This has
        // mainly to do with ::OutputItem and it's use of FormatFillGradient/FormatFillStyle
        // overloads -> XFillStyleItem *needs* to be known before XFillGradientItem
        // since it is locally 'remembered' in the processor (see m_oFillStyle).
        // This implicitly relies on a specific order of the Items: even with the situation
        // that WhichIDs are sorted and thus was the output of SfxItemIter before this
        // change, this is a risky assumption - the WhichIDs and thus the order of the
        // Items are not necessarily fixed (and were not designed to have a specific
        // 'order' to be made use of, it's coincidence).
        // Thus in the long run OutputItem itself might have to becorrected to work without
        // that assumption: hand a access method to it (callback/ItemSet) so that it might
        // ask e.g. for current FillStyle when FillGradient is processed.
        // I will now loop and remember XATTR_FILLSTYLE and XATTR_FILLGRADIENT when set,
        // executing when *both* are set and in the needed order.
        // Added asserts to ::FormatFillGradient impls when before not ::FormatFillStyle
        // was called
        const SfxPoolItem* pFillStyle(nullptr);
        const SfxPoolItem* pGradient(nullptr);

        for (SfxItemIter aIter(*pSet); !aIter.IsAtEnd(); aIter.NextItem())
        {
            pItem = aIter.GetCurItem();

            if (nullptr != pItem)
            {
                const sal_uInt16 nWhich(pItem->Which());

                if (XATTR_FILLSTYLE == nWhich)
                    pFillStyle = pItem;
                else if (XATTR_FILLGRADIENT == nWhich)
                    pGradient = pItem;
                else
                    OutputItem(*pItem);
            }
        }

        if (nullptr != pFillStyle && nullptr != pGradient)
        {
            // ITEM: execute when both are set and do so in needed order
            OutputItem(*pFillStyle);
            OutputItem(*pGradient);
        }
    }
    else
    {
        SfxWhichIter aIter( *pSet );
        sal_uInt16 nWhich = aIter.FirstWhich();
        while ( nWhich )
        {
            if ( SfxItemState::SET == aIter.GetItemState( true/*bDeep*/, &pItem ) &&
                 ( !bTestForDefault ||
                   nWhich == RES_UL_SPACE ||
                   nWhich == RES_LR_SPACE ||
                   *pItem != rPool.GetUserOrPoolDefaultItem( nWhich ) ||
                   ( pSet->GetParent() && *pItem != pSet->GetParent()->Get( nWhich ) ) ) )
            {
                OutputItem( *pItem );
            }
            nWhich = aIter.NextWhich();
        }
    }
}

std::tuple<const ::editeng::SvxBorderLine*, sal_uInt16, bool>
AttributeOutputBase::FormatCharBorder(const SvxBoxItem& rBox, const SfxItemSet* pItemSet) const
{
    // Get one of the borders (if there is any border then in docx also will be)
    const SvxBorderLine* pBorderLine = nullptr;
    sal_uInt16 nDist = 0;
    if( rBox.GetTop() )
    {
       pBorderLine = rBox.GetTop();
       nDist = rBox.GetDistance( SvxBoxItemLine::TOP );
    }
    else if( rBox.GetLeft() )
    {
       pBorderLine = rBox.GetLeft();
       nDist = rBox.GetDistance( SvxBoxItemLine::LEFT );
    }
    else if( rBox.GetBottom() )
    {
       pBorderLine = rBox.GetBottom();
       nDist = rBox.GetDistance( SvxBoxItemLine::BOTTOM );
    }
    else if( rBox.GetRight() )
    {
       pBorderLine = rBox.GetRight();
       nDist = rBox.GetDistance( SvxBoxItemLine::RIGHT );
    }

    const SfxPoolItem* pItem;
    if (pItemSet)
        pItem = pItemSet->GetItemIfSet(RES_CHRATR_SHADOW);
    else
        pItem = GetExport().HasItem(RES_CHRATR_SHADOW);
    const SvxShadowItem* pShadowItem = static_cast<const SvxShadowItem*>(pItem);
    const bool bShadow = pBorderLine &&
        pShadowItem && pShadowItem->GetLocation() != SvxShadowLocation::NONE &&
        pShadowItem->GetWidth() > 0;

    return { pBorderLine, nDist, bShadow };
}

/*
 * This function is used to check if the current SwTextNode (paragraph) has a redline object
 * that is attached to the paragraph marker.
 * This is done by checking if the range (SwPaM) of the redline is :
 * - Start = the last character of the current paragraph
 * - End = the first character of the next paragraph
 */
const SwRedlineData* AttributeOutputBase::GetParagraphMarkerRedline( const SwTextNode& rNode, RedlineType aRedlineType)
{
    // ToDo : this is not the most ideal ... should start maybe from 'nCurRedlinePos'
    for(SwRangeRedline* pRedl : GetExport().m_rDoc.getIDocumentRedlineAccess().GetRedlineTable())
    {
        // Only check redlines that are of type 'Delete'
        if ( pRedl->GetRedlineData().GetType() != aRedlineType )
            continue;

        SwNodeOffset uStartNodeIndex = pRedl->Start()->GetNodeIndex();
        SwNodeOffset uEndNodeIndex   = pRedl->End()->GetNodeIndex();
        SwNodeOffset uNodeIndex = rNode.GetIndex();

        if( uStartNodeIndex <= uNodeIndex && uNodeIndex < uEndNodeIndex )
            return &( pRedl->GetRedlineData() );
    }
    return nullptr;
}

void AttributeOutputBase::CharBackgroundBase( const SvxBrushItem& rBrush )
{
    bool bConvertToShading = !officecfg::Office::Common::Filter::Microsoft::Export::CharBackgroundToHighlighting::get();
    bool bHasShadingMarker = false;

    // MS Word doesn't support highlight in character styles. Always export those as shading.
    if ( !bConvertToShading && GetExport().m_bStyDef )
    {
        const SwFormat* pFormat = dynamic_cast<const SwFormat*>( GetExport().m_pOutFormatNode );
        bConvertToShading = pFormat && pFormat->Which() == RES_CHRFMT;
    }

    // Check shading marker
    const SfxPoolItem* pItem = GetExport().HasItem(RES_CHRATR_GRABBAG);
    if( pItem )
    {
        const SfxGrabBagItem aGrabBag = static_cast< const SfxGrabBagItem& >(*pItem);
        const std::map<OUString, css::uno::Any>& rMap = aGrabBag.GetGrabBag();
        auto aIterator = rMap.find(u"CharShadingMarker"_ustr);
        if( aIterator != rMap.end() )
        {
            aIterator->second >>= bHasShadingMarker;
        }
    }

    if( bConvertToShading || bHasShadingMarker )
    {
        CharBackground(rBrush);
    }
    else
    {
        // Don't create a duplicate entry when converting to highlight. An existing one has priority.
        // Character runs seem to need a different method to detect duplicates? Just continue to ignore that situation.
        if (GetExport().m_aCurrentCharPropStarts.size() || !GetExport().HasItem(RES_CHRATR_HIGHLIGHT))
            CharHighlight(rBrush);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
