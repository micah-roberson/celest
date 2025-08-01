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

#include <memory>
#include "docxattributeoutput.hxx"
#include "docxhelper.hxx"
#include "docxsdrexport.hxx"
#include "docxexportfilter.hxx"
#include "docxfootnotes.hxx"
#include "writerwordglue.hxx"
#include "ww8par.hxx"
#include <fmtcntnt.hxx>
#include <fmtftn.hxx>
#include <fchrfmt.hxx>
#include <tgrditem.hxx>
#include <fmtruby.hxx>
#include <fmtfollowtextflow.hxx>
#include <fmtanchr.hxx>
#include <breakit.hxx>
#include <redline.hxx>
#include <unoframe.hxx>
#include <textboxhelper.hxx>
#include <rdfhelper.hxx>
#include "wrtww8.hxx"

#include <comphelper/processfactory.hxx>
#include <comphelper/random.hxx>
#include <comphelper/string.hxx>
#include <comphelper/flagguard.hxx>
#include <comphelper/sequence.hxx>
#include <oox/token/namespaces.hxx>
#include <oox/token/tokens.hxx>
#include <oox/export/utils.hxx>
#include <oox/mathml/imexport.hxx>
#include <oox/drawingml/drawingmltypes.hxx>
#include <oox/token/relationship.hxx>
#include <oox/export/vmlexport.hxx>
#include <oox/ole/olehelper.hxx>
#include <oox/export/drawingml.hxx>

#include <editeng/autokernitem.hxx>
#include <editeng/unoprnms.hxx>
#include <editeng/fontitem.hxx>
#include <editeng/tstpitem.hxx>
#include <editeng/spltitem.hxx>
#include <editeng/widwitem.hxx>
#include <editeng/shaditem.hxx>
#include <editeng/brushitem.hxx>
#include <editeng/postitem.hxx>
#include <editeng/wghtitem.hxx>
#include <editeng/kernitem.hxx>
#include <editeng/crossedoutitem.hxx>
#include <editeng/cmapitem.hxx>
#include <editeng/udlnitem.hxx>
#include <editeng/langitem.hxx>
#include <editeng/lspcitem.hxx>
#include <editeng/escapementitem.hxx>
#include <editeng/fhgtitem.hxx>
#include <editeng/colritem.hxx>
#include <editeng/hyphenzoneitem.hxx>
#include <editeng/ulspitem.hxx>
#include <editeng/contouritem.hxx>
#include <editeng/shdditem.hxx>
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
#include <editeng/editobj.hxx>
#include <editeng/keepitem.hxx>
#include <editeng/borderline.hxx>
#include <editeng/scripthintitem.hxx>
#include <sax/tools/converter.hxx>
#include <svx/xdef.hxx>
#include <svx/xfillit0.hxx>
#include <svx/xflclit.hxx>
#include <svx/xflgrit.hxx>
#include <svx/svdouno.hxx>
#include <svx/unobrushitemhelper.hxx>
#include <svl/grabbagitem.hxx>
#include <tools/date.hxx>
#include <tools/datetime.hxx>
#include <tools/datetimeutils.hxx>
#include <svl/whiter.hxx>
#include <rtl/tencinfo.h>
#include <sal/log.hxx>
#include <sot/exchange.hxx>

#include <docufld.hxx>
#include <authfld.hxx>
#include <flddropdown.hxx>
#include <fmtclds.hxx>
#include <fmtinfmt.hxx>
#include <fmtline.hxx>
#include <ftninfo.hxx>
#include <htmltbl.hxx>
#include <lineinfo.hxx>
#include <ndgrf.hxx>
#include <ndole.hxx>
#include <ndtxt.hxx>
#include <pagedesc.hxx>
#include <paratr.hxx>
#include <swmodule.hxx>
#include <swtable.hxx>
#include <txtftn.hxx>
#include <fmtautofmt.hxx>
#include <docsh.hxx>
#include <docary.hxx>
#include <fmtclbl.hxx>
#include <fmtftntx.hxx>
#include <IDocumentSettingAccess.hxx>
#include <IDocumentRedlineAccess.hxx>
#include <grfatr.hxx>
#include <frmatr.hxx>
#include <txtatr.hxx>
#include <frameformats.hxx>
#include <textcontentcontrol.hxx>
#include <formatflysplit.hxx>

#include <o3tl/string_view.hxx>
#include <o3tl/unit_conversion.hxx>
#include <osl/file.hxx>
#include <utility>
#include <vcl/embeddedfontshelper.hxx>

#include <com/sun/star/i18n/ScriptType.hpp>
#include <com/sun/star/i18n/XBreakIterator.hpp>
#include <com/sun/star/chart2/XChartDocument.hpp>
#include <com/sun/star/drawing/ShadingPattern.hpp>
#include <com/sun/star/text/GraphicCrop.hpp>
#include <com/sun/star/embed/EmbedStates.hpp>
#include <com/sun/star/embed/Aspects.hpp>
#include <com/sun/star/text/ControlCharacter.hpp>

#include <algorithm>
#include <cstddef>
#include <stdarg.h>
#include <string_view>

#include <toolkit/helper/vclunohelper.hxx>
#include <unicode/regex.h>
#include <frozen/bits/defines.h>
#include <frozen/bits/elsa_std.h>
#include <frozen/unordered_map.h>
#include <IDocumentDeviceAccess.hxx>
#include <sfx2/printer.hxx>
#include <unotxdoc.hxx>
#include <poolfmt.hxx>
#include <flddat.hxx>

using ::editeng::SvxBorderLine;

using namespace oox;
using namespace docx;
using namespace sax_fastparser;
using namespace sw::util;
using namespace ::com::sun::star;
using namespace ::com::sun::star::drawing;

namespace {

class FFDataWriterHelper
{
    ::sax_fastparser::FSHelperPtr m_pSerializer;
    void writeCommonStart( const OUString& rName,
                           const OUString& rEntryMacro,
                           const OUString& rExitMacro,
                           const OUString& rHelp,
                           const OUString& rHint )
    {
        m_pSerializer->startElementNS(XML_w, XML_ffData);
        m_pSerializer->singleElementNS(XML_w, XML_name, FSNS(XML_w, XML_val), rName);
        m_pSerializer->singleElementNS(XML_w, XML_enabled);
        m_pSerializer->singleElementNS(XML_w, XML_calcOnExit, FSNS(XML_w, XML_val), "0");

        if ( !rEntryMacro.isEmpty() )
            m_pSerializer->singleElementNS( XML_w, XML_entryMacro,
                FSNS(XML_w, XML_val), rEntryMacro );

        if ( !rExitMacro.isEmpty() )
            m_pSerializer->singleElementNS(XML_w, XML_exitMacro, FSNS(XML_w, XML_val), rExitMacro);

        if ( !rHelp.isEmpty() )
            m_pSerializer->singleElementNS( XML_w, XML_helpText,
                FSNS(XML_w, XML_type), "text",
                FSNS(XML_w, XML_val), rHelp );

        if ( !rHint.isEmpty() )
            m_pSerializer->singleElementNS( XML_w, XML_statusText,
                FSNS(XML_w, XML_type), "text",
                FSNS(XML_w, XML_val), rHint );

    }
    void writeFinish()
    {
        m_pSerializer->endElementNS( XML_w, XML_ffData );
    }
public:
    explicit FFDataWriterHelper( ::sax_fastparser::FSHelperPtr  rSerializer ) : m_pSerializer(std::move( rSerializer )){}
    void WriteFormCheckbox( const OUString& rName,
                            const OUString& rEntryMacro,
                            const OUString& rExitMacro,
                            const OUString& rHelp,
                            const OUString& rHint,
                            bool bChecked )
    {
        writeCommonStart( rName, rEntryMacro, rExitMacro, rHelp, rHint );
        // Checkbox specific bits
        m_pSerializer->startElementNS(XML_w, XML_checkBox);
        // currently hardcoding autosize
        // #TODO check if this defaulted
        m_pSerializer->startElementNS(XML_w, XML_sizeAuto);
        m_pSerializer->endElementNS( XML_w, XML_sizeAuto );
        if ( bChecked )
            m_pSerializer->singleElementNS(XML_w, XML_checked);
        m_pSerializer->endElementNS( XML_w, XML_checkBox );
        writeFinish();
    }

    void WriteFormText(  const OUString& rName,
                         const OUString& rEntryMacro,
                         const OUString& rExitMacro,
                         const OUString& rHelp,
                         const OUString& rHint,
                         const OUString& rType,
                         const OUString& rDefaultText,
                         sal_uInt16 nMaxLength,
                         const OUString& rFormat )
    {
        writeCommonStart( rName, rEntryMacro, rExitMacro, rHelp, rHint );

        m_pSerializer->startElementNS(XML_w, XML_textInput);
        if ( !rType.isEmpty() )
            m_pSerializer->singleElementNS(XML_w, XML_type, FSNS(XML_w, XML_val), rType);
        if ( !rDefaultText.isEmpty() )
            m_pSerializer->singleElementNS(XML_w, XML_default, FSNS(XML_w, XML_val), rDefaultText);
        if ( nMaxLength )
            m_pSerializer->singleElementNS( XML_w, XML_maxLength,
                FSNS(XML_w, XML_val), OString::number(nMaxLength) );
        if ( !rFormat.isEmpty() )
            m_pSerializer->singleElementNS(XML_w, XML_format, FSNS(XML_w, XML_val), rFormat);
        m_pSerializer->endElementNS( XML_w, XML_textInput );

        writeFinish();
    }
};

class FieldMarkParamsHelper
{
    const sw::mark::Fieldmark& mrFieldmark;
    public:
    explicit FieldMarkParamsHelper( const sw::mark::Fieldmark& rFieldmark ) : mrFieldmark( rFieldmark ) {}
    SwMarkName const & getName() const { return mrFieldmark.GetName(); }
    template < typename T >
    bool extractParam( const OUString& rKey, T& rResult )
    {
        bool bResult = false;
        if ( mrFieldmark.GetParameters() )
        {
            sw::mark::Fieldmark::parameter_map_t::const_iterator it = mrFieldmark.GetParameters()->find( rKey );
            if ( it != mrFieldmark.GetParameters()->end() )
                bResult = ( it->second >>= rResult );
        }
        return bResult;
    }
};

// [ISO/IEC29500-1:2016] 17.18.50 ST_LongHexNumber (Eight Digit Hexadecimal Value)
OUString NumberToHexBinary(sal_Int32 n)
{
    OUStringBuffer aBuf;
    sax::Converter::convertNumberToHexBinary(aBuf, n);
    return aBuf.makeStringAndClear();
}

// Returns a new reference with the previous content of src; src is empty after this
auto detachFrom(rtl::Reference<sax_fastparser::FastAttributeList>& src)
{
    return rtl::Reference(std::move(src));
}

constexpr auto constThemeColorTypeTokenMap = frozen::make_unordered_map<model::ThemeColorType, const char*>({
    { model::ThemeColorType::Dark1, "dark1" },
    { model::ThemeColorType::Light1, "light1" },
    { model::ThemeColorType::Dark2, "dark2" },
    { model::ThemeColorType::Light2, "light2" },
    { model::ThemeColorType::Accent1, "accent1" },
    { model::ThemeColorType::Accent2, "accent2" },
    { model::ThemeColorType::Accent3, "accent3" },
    { model::ThemeColorType::Accent4, "accent4" },
    { model::ThemeColorType::Accent5, "accent5" },
    { model::ThemeColorType::Accent6, "accent6" },
    { model::ThemeColorType::Hyperlink, "hyperlink" },
    { model::ThemeColorType::FollowedHyperlink, "followedHyperlink" }
});

OString lclGetSchemeType(model::ComplexColor const& rComplexColor)
{
    const auto iter = constThemeColorTypeTokenMap.find(rComplexColor.getThemeColorType());
    assert(iter != constThemeColorTypeTokenMap.end());
    OString sSchemeType = iter->second;
    if (rComplexColor.getThemeColorUsage() == model::ThemeColorUsage::Text)
    {
        if (rComplexColor.getThemeColorType() == model::ThemeColorType::Dark1)
            sSchemeType = "text1"_ostr;
        else if (rComplexColor.getThemeColorType() == model::ThemeColorType::Dark2)
            sSchemeType = "text2"_ostr;
    }
    else if (rComplexColor.getThemeColorUsage() == model::ThemeColorUsage::Background)
    {
        if (rComplexColor.getThemeColorType() == model::ThemeColorType::Light1)
            sSchemeType = "background1"_ostr;
        else if (rComplexColor.getThemeColorType() == model::ThemeColorType::Light2)
            sSchemeType = "background2"_ostr;
    }
    return sSchemeType;
}

void lclAddThemeValuesToCustomAttributes(
    rtl::Reference<sax_fastparser::FastAttributeList>& pAttrList, model::ComplexColor const& rComplexColor,
    sal_Int32 nThemeAttrId, sal_Int32 nThemeTintAttrId, sal_Int32 nThemeShadeAttrId)
{
    if (rComplexColor.isValidThemeType())
    {
        OString sSchemeType = lclGetSchemeType(rComplexColor);

        DocxAttributeOutput::AddToAttrList(pAttrList, FSNS(XML_w, nThemeAttrId), sSchemeType);

        sal_Int16 nLumMod = 10'000;
        sal_Int16 nLumOff = 0;
        sal_Int16 nTint = 0;
        sal_Int16 nShade = 0;

        for (auto const& rTransform : rComplexColor.getTransformations())
        {
            if (rTransform.meType == model::TransformationType::LumMod)
                nLumMod = rTransform.mnValue;
            if (rTransform.meType == model::TransformationType::LumOff)
                nLumOff = rTransform.mnValue;
            if (rTransform.meType == model::TransformationType::Tint)
                nTint = rTransform.mnValue;
            if (rTransform.meType == model::TransformationType::Shade)
                nShade = rTransform.mnValue;
        }
        if (nLumMod == 10'000 && nLumOff == 0)
        {
            if (nTint != 0)
            {
                // Convert from 0-100 into 0-255
                sal_Int16 nTint255 = std::round(255.0 - (double(nTint) / 10000.0) * 255.0);
                DocxAttributeOutput::AddToAttrList(pAttrList, FSNS(XML_w, nThemeTintAttrId), OString::number(nTint255, 16));
            }
            else if (nShade != 0)
            {
                // Convert from 0-100 into 0-255
                sal_Int16 nShade255 = std::round(255.0 - (double(nShade) / 10000.0) * 255.0);
                DocxAttributeOutput::AddToAttrList(pAttrList, FSNS(XML_w, nThemeShadeAttrId), OString::number(nShade255, 16));
            }
        }
        else
        {
            double nPercentage = 0.0;

            if (nLumOff > 0)
                nPercentage = double(nLumOff) / 100.0;
            else
                nPercentage = (-10'000 + double(nLumMod)) / 100.0;

            // Convert from 0-100 into 0-255
            sal_Int16 nTintShade255 = std::round(255.0 - (std::abs(nPercentage) / 100.0) * 255.0);

            if (nPercentage > 0)
                DocxAttributeOutput::AddToAttrList(pAttrList, FSNS(XML_w, nThemeTintAttrId), OString::number(nTintShade255, 16));
            else if (nPercentage < 0)
                DocxAttributeOutput::AddToAttrList(pAttrList, FSNS(XML_w, nThemeShadeAttrId), OString::number(nTintShade255, 16));
        }
    }
}

void lclAddThemeFillColorAttributes(rtl::Reference<sax_fastparser::FastAttributeList>& pAttrList, model::ComplexColor const& rComplexColor)
{
    lclAddThemeValuesToCustomAttributes(pAttrList, rComplexColor, XML_themeFill, XML_themeFillTint, XML_themeFillShade);
}

void lclAddThemeColorAttributes(rtl::Reference<sax_fastparser::FastAttributeList>& pAttrList, model::ComplexColor const& rComplexColor)
{
    lclAddThemeValuesToCustomAttributes(pAttrList, rComplexColor, XML_themeColor, XML_themeTint, XML_themeShade);
}

bool lclHasSolidFillTransformations(const model::ComplexColor& aComplexColor)
{
    const std::vector<model::Transformation>& transformations = aComplexColor.getTransformations();
    auto idx = std::find_if(transformations.begin(), transformations.end(), [](const model::Transformation& transformation) {
        return transformation.meType != model::TransformationType::Shade && transformation.meType != model::TransformationType::Tint;
    });
    return idx != transformations.end();
}

} // end anonymous namespace

void DocxAttributeOutput::RTLAndCJKState( bool bIsRTL, sal_uInt16 /*nScript*/ )
{
    if (bIsRTL)
        m_pSerializer->singleElementNS(XML_w, XML_rtl, FSNS(XML_w, XML_val), "true");
}

/// Are multiple paragraphs disallowed inside this type of SDT?
static bool lcl_isOnelinerSdt(std::u16string_view rName)
{
    return rName == u"Title" || rName == u"Subtitle" || rName == u"Company";
}

// write a floating table directly to docx without the surrounding frame
void DocxAttributeOutput::WriteFloatingTable(ww8::Frame const* pParentFrame)
{
    const SwFrameFormat& rFrameFormat = pParentFrame->GetFrameFormat();
    m_aFloatingTablesOfParagraph.insert(&rFrameFormat);
    const SwNodeIndex* pNodeIndex = rFrameFormat.GetContent().GetContentIdx();

    SwNodeOffset nStt = pNodeIndex ? pNodeIndex->GetIndex() + 1 : SwNodeOffset(0);
    SwNodeOffset nEnd = pNodeIndex ? pNodeIndex->GetNode().EndOfSectionIndex() : SwNodeOffset(0);

    //Save data here and restore when out of scope
    ExportDataSaveRestore aDataGuard(GetExport(), nStt, nEnd, pParentFrame);

    // Stash away info about the current table, so m_tableReference is clean.
    DocxTableExportContext aTableExportContext(*this);

    // set a floatingTableFrame AND unset parent frame,
    // otherwise exporter thinks we are still in a frame
    m_rExport.SetFloatingTableFrame(pParentFrame);
    m_rExport.m_pParentFrame = nullptr;

    GetExport().WriteText();

    m_rExport.SetFloatingTableFrame(nullptr);
}

void DocxAttributeOutput::CheckAndWriteFloatingTables(const SwNode& rNode)
{
    // floating tables in shapes are not supported: exclude this case
    if (m_rExport.SdrExporter().IsDMLAndVMLDrawingOpen())
        return;

    // iterate though all SpzFrameFormats and check whether they are anchored to the current text node
    for( sal_uInt16 nCnt = m_rExport.m_rDoc.GetSpzFrameFormats()->size(); nCnt; )
    {
        const SwFrameFormat* pFrameFormat = (*m_rExport.m_rDoc.GetSpzFrameFormats())[ --nCnt ];
        const SwFormatAnchor& rAnchor = pFrameFormat->GetAnchor();
        const SwNode* pAnchorNode = rAnchor.GetAnchorNode();

        if (!pAnchorNode || !rNode.GetTextNode())
            continue;

        if (*pAnchorNode != *rNode.GetTextNode())
            continue;

        const SwNodeIndex* pStartNode = pFrameFormat->GetContent().GetContentIdx();
        if (!pStartNode)
            continue;

        SwNodeIndex aStartNode = *pStartNode;

        // go to the next node (actual content)
        ++aStartNode;

        // this has to be a table
        if (!aStartNode.GetNode().IsTableNode())
            continue;

        // go to the end of the table
        SwNodeOffset aEndIndex = aStartNode.GetNode().EndOfSectionIndex();
        // go one deeper
        aEndIndex++;
        // this has to be the end of the content
        if (aEndIndex != pFrameFormat->GetContent().GetContentIdx()->GetNode().EndOfSectionIndex())
            continue;

        // check for a grabBag and "TablePosition" attribute -> then we can export the table directly
        SwTableNode* pTableNode = aStartNode.GetNode().GetTableNode();
        SwTable& rTable = pTableNode->GetTable();
        SwFrameFormat* pTableFormat = rTable.GetFrameFormat();
        const SfxGrabBagItem* pTableGrabBag = pTableFormat->GetAttrSet().GetItem<SfxGrabBagItem>(RES_FRMATR_GRABBAG);
        const std::map<OUString, css::uno::Any> & rTableGrabBag = pTableGrabBag->GetGrabBag();
        // no grabbag?
        if (rTableGrabBag.find(u"TablePosition"_ustr) == rTableGrabBag.end() && !pFrameFormat->GetFlySplit().GetValue())
        {
            continue;
        }

        // write table to docx
        ww8::Frame aFrame(*pFrameFormat, *rAnchor.GetContentAnchor());
        WriteFloatingTable(&aFrame);
    }
}

sal_Int32 DocxAttributeOutput::StartParagraph(const ww8::WW8TableNodeInfo::Pointer_t& pTextNodeInfo,
                                              bool bGenerateParaId)
{
    // Paragraphs (in headers/footers/comments/frames etc) can start before another finishes.
    // So a stack is needed to keep track of each paragraph's status separately.
    // Complication: Word can't handle nested text boxes, so those need to be collected together.
    if ( !m_aFramesOfParagraph.size() || !m_nTextFrameLevel )
        m_aFramesOfParagraph.push(std::vector<ww8::Frame>());

    if ( m_nColBreakStatus == COLBRK_POSTPONE )
        m_nColBreakStatus = COLBRK_WRITE;

    // Output table/table row/table cell starts if needed
    if ( pTextNodeInfo )
    {
        // New cell/row?
        if ( m_tableReference.m_nTableDepth > 0 && !m_tableReference.m_bTableCellOpen )
        {
            ww8::WW8TableNodeInfoInner::Pointer_t pDeepInner( pTextNodeInfo->getInnerForDepth( m_tableReference.m_nTableDepth ) );
            if ( pDeepInner->getCell() == 0 )
                StartTableRow( pDeepInner );

            const sal_uInt32 nCell = pDeepInner->getCell();
            const sal_uInt32 nRow = pDeepInner->getRow();

            SyncNodelessCells(pDeepInner, nCell, nRow);
            StartTableCell(pDeepInner, nCell, nRow);
        }

        sal_uInt32 nRow = pTextNodeInfo->getRow();
        sal_uInt32 nCell = pTextNodeInfo->getCell();
        if (nCell == 0)
        {
            // Do we have to start the table?
            // [If we are at the right depth already, it means that we
            // continue the table cell]
            sal_uInt32 nCurrentDepth = pTextNodeInfo->getDepth();

            if ( nCurrentDepth > m_tableReference.m_nTableDepth )
            {
                // Start all the tables that begin here
                for ( sal_uInt32 nDepth = m_tableReference.m_nTableDepth + 1; nDepth <= nCurrentDepth; ++nDepth )
                {
                    ww8::WW8TableNodeInfoInner::Pointer_t pInner( pTextNodeInfo->getInnerForDepth( nDepth ) );

                    StartTable( pInner );
                    StartTableRow( pInner );

                    StartTableCell(pInner, 0, nDepth == nCurrentDepth ? nRow : 0);
                }

                m_tableReference.m_nTableDepth = nCurrentDepth;
            }
        }
    }

    // look ahead for floating tables that were put into a frame during import
    // Do this after opening table/row/cell, so floating tables anchored at cell start go inside
    // the cell, not outside.
    CheckAndWriteFloatingTables(m_rExport.m_pCurPam->GetPointNode());

    // Look up the "sdt end before this paragraph" property early, when it
    // would normally arrive, it would be too late (would be after the
    // paragraph start has been written).
    bool bEndParaSdt = false;
    if (m_aParagraphSdt.m_bStartedSdt)
    {
        SwTextNode* pTextNode = m_rExport.m_pCurPam->GetPointNode().GetTextNode();
        if (pTextNode && pTextNode->GetpSwAttrSet())
        {
            const SfxItemSet* pSet = pTextNode->GetpSwAttrSet();
            if (const SfxPoolItem* pItem = pSet->GetItem(RES_PARATR_GRABBAG))
            {
                const SfxGrabBagItem& rParaGrabBag = static_cast<const SfxGrabBagItem&>(*pItem);
                const std::map<OUString, css::uno::Any>& rMap = rParaGrabBag.GetGrabBag();
                bEndParaSdt = m_aParagraphSdt.m_bStartedSdt && rMap.contains(u"ParaSdtEndBefore"_ustr);
            }
        }
    }
    // TODO also avoid multiline paragraphs in those SDT types for shape text
    bool bOneliner = m_aParagraphSdt.m_bStartedSdt && !m_rExport.SdrExporter().IsDMLAndVMLDrawingOpen() && lcl_isOnelinerSdt(m_aStartedParagraphSdtPrAlias);
    if (bEndParaSdt || (m_aParagraphSdt.m_bStartedSdt && m_bHadSectPr) || bOneliner)
    {
        // This is the common case: "close sdt before the current paragraph" was requested by the next paragraph.
        m_aParagraphSdt.EndSdtBlock(m_pSerializer);
        m_aStartedParagraphSdtPrAlias.clear();
    }
    m_bHadSectPr = false;

    // this mark is used to be able to enclose the paragraph inside a sdr tag.
    // We will only know if we have to do that later.
    m_pSerializer->mark(Tag_StartParagraph_1);

    std::optional<OUString> aParaId;
    sal_Int32 nParaId = 0;
    if (bGenerateParaId)
    {
        nParaId = m_nNextParaId++;
        aParaId = NumberToHexBinary(nParaId);
    }
    m_pSerializer->startElementNS(XML_w, XML_p, FSNS(XML_w14, XML_paraId), aParaId);

    // postpone the output of the run (we get it before the paragraph
    // properties, but must write it after them)
    m_pSerializer->mark(Tag_StartParagraph_2);

    // no section break in this paragraph yet; can be set in SectionBreak()
    m_pSectionInfo.reset();

    m_bParagraphOpened = true;
    m_bIsFirstParagraph = false;
    m_nHyperLinkCount.push_back(0);

    return nParaId;
}

OString DocxAttributeOutput::convertToOOXMLVertOrient(sal_Int16 nOrient)
{
    switch( nOrient )
    {
        case text::VertOrientation::CENTER:
        case text::VertOrientation::LINE_CENTER:
            return "center"_ostr;
        case text::VertOrientation::BOTTOM:
            return "bottom"_ostr;
        case text::VertOrientation::LINE_BOTTOM:
            return "outside"_ostr;
        case text::VertOrientation::TOP:
            return "top"_ostr;
        case text::VertOrientation::LINE_TOP:
            return "inside"_ostr;
        default:
            return OString();
    }
}

OString DocxAttributeOutput::convertToOOXMLHoriOrient(sal_Int16 nOrient, bool bIsPosToggle)
{
    switch( nOrient )
    {
        case text::HoriOrientation::LEFT:
            return bIsPosToggle ? "inside" : "left";
        case text::HoriOrientation::INSIDE:
            return "inside"_ostr;
        case text::HoriOrientation::RIGHT:
            return bIsPosToggle ? "outside" : "right";
        case text::HoriOrientation::OUTSIDE:
            return "outside"_ostr;
        case text::HoriOrientation::CENTER:
        case text::HoriOrientation::FULL:
            return "center"_ostr;
        default:
            return OString();
    }
}

OString DocxAttributeOutput::convertToOOXMLVertOrientRel(sal_Int16 nOrientRel)
{
    switch (nOrientRel)
    {
        case text::RelOrientation::PAGE_PRINT_AREA:
            return "margin"_ostr;
        case text::RelOrientation::PAGE_FRAME:
            return "page"_ostr;
        case text::RelOrientation::FRAME:
        case text::RelOrientation::TEXT_LINE:
        default:
            return "text"_ostr;
    }
}

OString DocxAttributeOutput::convertToOOXMLHoriOrientRel(sal_Int16 nOrientRel)
{
    switch (nOrientRel)
    {
        case text::RelOrientation::PAGE_PRINT_AREA:
            return "margin"_ostr;
        case text::RelOrientation::PAGE_FRAME:
            return "page"_ostr;
        case text::RelOrientation::CHAR:
        case text::RelOrientation::PAGE_RIGHT:
        case text::RelOrientation::FRAME:
        default:
            return "text"_ostr;
    }
}

void FramePrHelper::SetFrame(ww8::Frame* pFrame, sal_Int32 nTableDepth)
{
    assert(!pFrame || !m_pFrame);
    m_pFrame = pFrame;
    m_nTableDepth = nTableDepth;
    if (m_pFrame)
    {
        m_bUseFrameBorders = true;
        m_bUseFrameBackground = true;
        m_bUseFrameTextDirection = true;
    }
}

bool FramePrHelper::UseFrameBorders(sal_Int32 nTableDepth)
{
    if (!m_pFrame || m_nTableDepth < nTableDepth)
        return false;

    return m_bUseFrameBorders;
}

bool FramePrHelper::UseFrameBackground()
{
    if (!m_pFrame)
        return false;

    return m_bUseFrameBackground;
}

bool FramePrHelper::UseFrameTextDirection(sal_Int32 nTableDepth)
{
    if (!m_pFrame || m_nTableDepth < nTableDepth)
        return false;

    return m_bUseFrameTextDirection;
}

void SdtBlockHelper::DeleteAndResetTheLists()
{
    if (m_pTokenChildren.is() )
        m_pTokenChildren.clear();
    if (m_pDataBindingAttrs.is() )
        m_pDataBindingAttrs.clear();
    if (m_pTextAttrs.is())
        m_pTextAttrs.clear();
    if (!m_aAlias.isEmpty())
        m_aAlias.clear();
    if (!m_aTag.isEmpty())
        m_aTag.clear();
    if (!m_aLock.isEmpty())
        m_aLock.clear();
    if (!m_aPlaceHolderDocPart.isEmpty())
        m_aPlaceHolderDocPart.clear();
    if (!m_aColor.isEmpty())
        m_aColor.clear();
    if (!m_aAppearance.isEmpty())
        m_aAppearance.clear();
    m_bShowingPlaceHolder = false;
    m_nId = 0;
    m_nTabIndex = 0;
}

void SdtBlockHelper::WriteSdtBlock(const ::sax_fastparser::FSHelperPtr& pSerializer, bool bRunTextIsOn, bool bParagraphHasDrawing)
{
    if (m_nSdtPrToken <= 0 && !m_pDataBindingAttrs.is() && !m_nId)
        return;

    // sdt start mark
    pSerializer->mark(DocxAttributeOutput::Tag_WriteSdtBlock);

    pSerializer->startElementNS(XML_w, XML_sdt);

    // output sdt properties
    pSerializer->startElementNS(XML_w, XML_sdtPr);

    if (m_nSdtPrToken > 0 && m_pTokenChildren.is())
    {
        if (!m_pTokenAttributes.is())
            pSerializer->startElement(m_nSdtPrToken);
        else
        {
            pSerializer->startElement(m_nSdtPrToken, detachFrom(m_pTokenAttributes));
        }

        if (m_nSdtPrToken == FSNS(XML_w, XML_date) || m_nSdtPrToken == FSNS(XML_w, XML_docPartObj) || m_nSdtPrToken == FSNS(XML_w, XML_docPartList) || m_nSdtPrToken == FSNS(XML_w14, XML_checkbox)) {
            for (auto& it : *m_pTokenChildren)
            {
                pSerializer->singleElement(it.getToken(), FSNS(XML_w, XML_val), it.toCString());
            }
        }

        pSerializer->endElement(m_nSdtPrToken);
    }
    else if ((m_nSdtPrToken > 0) && m_nSdtPrToken != FSNS(XML_w, XML_id) && !(bRunTextIsOn && bParagraphHasDrawing))
    {
        if (!m_pTokenAttributes.is())
            pSerializer->singleElement(m_nSdtPrToken);
        else
        {
            pSerializer->singleElement(m_nSdtPrToken, detachFrom(m_pTokenAttributes));
        }
    }

    WriteExtraParams(pSerializer);

    pSerializer->endElementNS(XML_w, XML_sdtPr);

    // sdt contents start tag
    pSerializer->startElementNS(XML_w, XML_sdtContent);

    // prepend the tags since the sdt start mark before the paragraph
    pSerializer->mergeTopMarks(DocxAttributeOutput::Tag_WriteSdtBlock, sax_fastparser::MergeMarks::PREPEND);

    // write the ending tags after the paragraph
    m_bStartedSdt = true;

    // clear sdt status
    m_nSdtPrToken = 0;
    DeleteAndResetTheLists();
}

void SdtBlockHelper::WriteExtraParams(const ::sax_fastparser::FSHelperPtr& pSerializer)
{
    if (m_nId)
    {
        pSerializer->singleElementNS(XML_w, XML_id, FSNS(XML_w, XML_val), OString::number(m_nId));
    }

    if (m_pDataBindingAttrs.is())
    {
        pSerializer->singleElementNS(XML_w, XML_dataBinding, detachFrom(m_pDataBindingAttrs));
    }

    if (m_pTextAttrs.is())
    {
        pSerializer->singleElementNS(XML_w, XML_text, detachFrom(m_pTextAttrs));
    }

    if (!m_aPlaceHolderDocPart.isEmpty())
    {
        pSerializer->startElementNS(XML_w, XML_placeholder);
        pSerializer->singleElementNS(XML_w, XML_docPart, FSNS(XML_w, XML_val), m_aPlaceHolderDocPart);
        pSerializer->endElementNS(XML_w, XML_placeholder);
    }

    if (m_bShowingPlaceHolder)
        pSerializer->singleElementNS(XML_w, XML_showingPlcHdr);

    if (!m_aColor.isEmpty())
    {
        pSerializer->singleElementNS(XML_w15, XML_color, FSNS(XML_w, XML_val), m_aColor);
    }

    if (!m_aAppearance.isEmpty())
    {
        pSerializer->singleElementNS(XML_w15, XML_appearance, FSNS(XML_w15, XML_val), m_aAppearance);
    }

    if (!m_aAlias.isEmpty())
        pSerializer->singleElementNS(XML_w, XML_alias, FSNS(XML_w, XML_val), m_aAlias);

    if (!m_aTag.isEmpty())
        pSerializer->singleElementNS(XML_w, XML_tag, FSNS(XML_w, XML_val), m_aTag);

    if (m_nTabIndex)
        pSerializer->singleElementNS(XML_w, XML_tabIndex, FSNS(XML_w, XML_val),
                                     OString::number(m_nTabIndex));

    if (!m_aLock.isEmpty())
        pSerializer->singleElementNS(XML_w, XML_lock, FSNS(XML_w, XML_val), m_aLock);
}

void SdtBlockHelper::EndSdtBlock(const ::sax_fastparser::FSHelperPtr& pSerializer)
{
    pSerializer->endElementNS(XML_w, XML_sdtContent);
    pSerializer->endElementNS(XML_w, XML_sdt);
    m_bStartedSdt = false;
}

void SdtBlockHelper::GetSdtParamsFromGrabBag(const uno::Sequence<beans::PropertyValue>& aGrabBagSdt)
{
    for (const beans::PropertyValue& aPropertyValue : aGrabBagSdt)
    {
        if (aPropertyValue.Name == "ooxml:CT_SdtPr_checkbox")
        {
            m_nSdtPrToken = FSNS(XML_w14, XML_checkbox);
            uno::Sequence<beans::PropertyValue> aGrabBag;
            aPropertyValue.Value >>= aGrabBag;
            for (const auto& rProp : aGrabBag)
            {
                if (rProp.Name == "ooxml:CT_SdtCheckbox_checked")
                    DocxAttributeOutput::AddToAttrList(m_pTokenChildren,
                        FSNS(XML_w14, XML_checked), rProp.Value.get<OUString>());
                else if (rProp.Name == "ooxml:CT_SdtCheckbox_checkedState")
                    DocxAttributeOutput::AddToAttrList(m_pTokenChildren,
                        FSNS(XML_w14, XML_checkedState), rProp.Value.get<OUString>());
                else if (rProp.Name == "ooxml:CT_SdtCheckbox_uncheckedState")
                    DocxAttributeOutput::AddToAttrList(m_pTokenChildren,
                        FSNS(XML_w14, XML_uncheckedState), rProp.Value.get<OUString>());
            }
        }
        else if (aPropertyValue.Name == "ooxml:CT_SdtPr_dataBinding" && !m_pDataBindingAttrs.is())
        {
            uno::Sequence<beans::PropertyValue> aGrabBag;
            aPropertyValue.Value >>= aGrabBag;
            for (const auto& rProp : aGrabBag)
            {
                if (rProp.Name == "ooxml:CT_DataBinding_prefixMappings")
                    DocxAttributeOutput::AddToAttrList( m_pDataBindingAttrs,
                                    FSNS( XML_w, XML_prefixMappings ), rProp.Value.get<OUString>());
                else if (rProp.Name == "ooxml:CT_DataBinding_xpath")
                    DocxAttributeOutput::AddToAttrList( m_pDataBindingAttrs,
                                    FSNS( XML_w, XML_xpath ), rProp.Value.get<OUString>());
                else if (rProp.Name == "ooxml:CT_DataBinding_storeItemID")
                    DocxAttributeOutput::AddToAttrList( m_pDataBindingAttrs,
                                    FSNS( XML_w, XML_storeItemID ), rProp.Value.get<OUString>());
            }
        }
        else if (aPropertyValue.Name == "ooxml:CT_SdtPr_text")
        {
            uno::Sequence<beans::PropertyValue> aGrabBag;
            aPropertyValue.Value >>= aGrabBag;
            if (aGrabBag.hasElements())
            {
                for (const auto& rProp : aGrabBag)
                {
                    if (rProp.Name == "ooxml:CT_SdtText_multiLine")
                        DocxAttributeOutput::AddToAttrList(m_pTextAttrs,
                            FSNS(XML_w, XML_multiLine), rProp.Value.get<OUString>());
                }
            }
            else
            {
                // We still have w:text, but no attrs
                m_nSdtPrToken = FSNS(XML_w, XML_text);
            }
        }
        else if (aPropertyValue.Name == "ooxml:CT_SdtPlaceholder_docPart")
        {
            uno::Sequence<beans::PropertyValue> aGrabBag;
            aPropertyValue.Value >>= aGrabBag;
            for (const auto& rProp : aGrabBag)
            {
                if (rProp.Name == "ooxml:CT_SdtPlaceholder_docPart_val")
                    m_aPlaceHolderDocPart = rProp.Value.get<OUString>();
            }
        }
        else if (aPropertyValue.Name == "ooxml:CT_SdtPr_color")
        {
            uno::Sequence<beans::PropertyValue> aGrabBag;
            aPropertyValue.Value >>= aGrabBag;
            for (const auto& rProp : aGrabBag)
            {
                if (rProp.Name == "ooxml:CT_SdtColor_val")
                    m_aColor = rProp.Value.get<OUString>();
            }
        }
        else if (aPropertyValue.Name == "ooxml:CT_SdtPr_appearance")
        {
            if (!(aPropertyValue.Value >>= m_aAppearance))
                SAL_WARN("sw.ww8", "DocxAttributeOutput::GrabBag: unexpected sdt appearance value");
        }
        else if (aPropertyValue.Name == "ooxml:CT_SdtPr_showingPlcHdr")
        {
            if (!(aPropertyValue.Value >>= m_bShowingPlaceHolder))
                SAL_WARN("sw.ww8", "DocxAttributeOutput::GrabBag: unexpected sdt ShowingPlcHdr");
        }
        else if (aPropertyValue.Name == "ooxml:CT_SdtPr_alias" && m_aAlias.isEmpty())
        {
            if (!(aPropertyValue.Value >>= m_aAlias))
                SAL_WARN("sw.ww8", "DocxAttributeOutput::GrabBag: unexpected sdt alias value");
        }
        else if (aPropertyValue.Name == "ooxml:CT_SdtPr_tag" && m_aTag.isEmpty())
        {
            if (!(aPropertyValue.Value >>= m_aTag))
                SAL_WARN("sw.ww8", "DocxAttributeOutput::GrabBag: unexpected sdt tag value");
        }
        else if (aPropertyValue.Name == "ooxml:CT_SdtPr_id")
        {
            if (!(aPropertyValue.Value >>= m_nId))
                SAL_WARN("sw.ww8", "DocxAttributeOutput::GrabBag: unexpected sdt id value");
        }
        else if (aPropertyValue.Name == "ooxml:CT_SdtPr_tabIndex" && !m_nTabIndex)
        {
            if (!(aPropertyValue.Value >>= m_nTabIndex))
                SAL_WARN("sw.ww8", "DocxAttributeOutput::GrabBag: unexpected sdt tabIndex value");
        }
        else if (aPropertyValue.Name == "ooxml:CT_SdtPr_lock" && m_aLock.isEmpty())
        {
            if (!(aPropertyValue.Value >>= m_aLock))
                SAL_WARN("sw.ww8", "DocxAttributeOutput::GrabBag: unexpected sdt lock value");
        }
        else if (aPropertyValue.Name == "ooxml:CT_SdtPr_citation")
            m_nSdtPrToken = FSNS(XML_w, XML_citation);
        else if (aPropertyValue.Name == "ooxml:CT_SdtPr_docPartObj" ||
            aPropertyValue.Name == "ooxml:CT_SdtPr_docPartList")
        {
            if (aPropertyValue.Name == "ooxml:CT_SdtPr_docPartObj")
                m_nSdtPrToken = FSNS(XML_w, XML_docPartObj);
            else if (aPropertyValue.Name == "ooxml:CT_SdtPr_docPartList")
                m_nSdtPrToken = FSNS(XML_w, XML_docPartList);

            uno::Sequence<beans::PropertyValue> aGrabBag;
            aPropertyValue.Value >>= aGrabBag;
            for (const auto& rProp : aGrabBag)
            {
                if (rProp.Name == "ooxml:CT_SdtDocPart_docPartGallery")
                    DocxAttributeOutput::AddToAttrList(m_pTokenChildren,
                        FSNS(XML_w, XML_docPartGallery), rProp.Value.get<OUString>());
                else if (rProp.Name == "ooxml:CT_SdtDocPart_docPartCategory")
                    DocxAttributeOutput::AddToAttrList(m_pTokenChildren,
                        FSNS(XML_w, XML_docPartCategory), rProp.Value.get<OUString>());
                else if (rProp.Name == "ooxml:CT_SdtDocPart_docPartUnique")
                {
                    OUString sValue = rProp.Value.get<OUString>();
                    if (sValue.isEmpty())
                        sValue = "true";
                    DocxAttributeOutput::AddToAttrList(m_pTokenChildren, FSNS(XML_w, XML_docPartUnique),
                        sValue);
                }
            }
        }
        else if (aPropertyValue.Name == "ooxml:CT_SdtPr_equation")
            m_nSdtPrToken = FSNS(XML_w, XML_equation);
        else if (aPropertyValue.Name == "ooxml:CT_SdtPr_picture")
            m_nSdtPrToken = FSNS(XML_w, XML_picture);
        else if (aPropertyValue.Name == "ooxml:CT_SdtPr_group")
            m_nSdtPrToken = FSNS(XML_w, XML_group);
        else
            SAL_WARN("sw.ww8", "GetSdtParamsFromGrabBag unhandled SdtPr grab bag property " << aPropertyValue.Name);
    }
}

void DocxAttributeOutput::PopulateFrameProperties(const SwFrameFormat* pFrameFormat, const Size& rSize)
{
    rtl::Reference<sax_fastparser::FastAttributeList> attrList = FastSerializerHelper::createAttrList();

    const SwFormatHoriOrient& rHoriOrient = pFrameFormat->GetHoriOrient();
    const SwFormatVertOrient& rVertOrient = pFrameFormat->GetVertOrient();
    awt::Point aPos(rHoriOrient.GetPos(), rVertOrient.GetPos());

    // A few assumptions need to be made here, because framePr is a confused mixture
    // of (multiple) paragraph's border properties being transferred to/from a frame.
    // The frame size describes the size BEFORE the PARAGRAPH border spacing is applied.
    // However, we can't actually look at all the paragraphs' borders because they might be
    // different, and all MUST specify the same frame width in order to belong to the same frame.
    // In order for them all to be consistent, the only choice is to use the frame's border spacing.
    // During import, the frame was assigned border spacing based on the contained paragraphs.
    // So now at export time we have to assume that none of this has been changed by the user.

    // 620 (31pt) is the maximum paragraph border spacing allowed in MS Formats,
    // so if the value is greater than that, avoid adjusting the size - the user has interfered.
    const sal_uInt32 nLeftBorderSpacing = pFrameFormat->GetBox().GetDistance(SvxBoxItemLine::LEFT);
    const sal_uInt32 nRighttBorderSpacing = pFrameFormat->GetBox().GetDistance(SvxBoxItemLine::RIGHT);
    sal_uInt32 nAdjustedWidth = rSize.Width();
    if (nLeftBorderSpacing < 621 && nRighttBorderSpacing < 621
        && nAdjustedWidth > nLeftBorderSpacing + nRighttBorderSpacing)
    {
        nAdjustedWidth -= nLeftBorderSpacing + nRighttBorderSpacing;
    }
    attrList->add( FSNS( XML_w, XML_w), OString::number(nAdjustedWidth));
    attrList->add( FSNS( XML_w, XML_h), OString::number(rSize.Height()));

    const OString relativeFromH = convertToOOXMLHoriOrientRel(rHoriOrient.GetRelationOrient());
    const OString relativeFromV = convertToOOXMLVertOrientRel(rVertOrient.GetRelationOrient());
    OString aXAlign = convertToOOXMLHoriOrient(rHoriOrient.GetHoriOrient(), /*bIsPosToggle=*/false);
    OString aYAlign = convertToOOXMLVertOrient(rVertOrient.GetVertOrient());
    if (!aXAlign.isEmpty())
        attrList->add(FSNS(XML_w, XML_xAlign), aXAlign);
    else if (aPos.X)
        attrList->add( FSNS( XML_w, XML_x), OString::number(aPos.X));
    if (!aYAlign.isEmpty() && relativeFromV != "text")
        attrList->add(FSNS(XML_w, XML_yAlign), aYAlign);
    else if (aPos.Y)
        attrList->add( FSNS( XML_w, XML_y), OString::number(aPos.Y));

    sal_Int16 nLeft = pFrameFormat->GetLRSpace().ResolveLeft({});
    sal_Int16 nRight = pFrameFormat->GetLRSpace().ResolveRight({});
    sal_Int16 nUpper = pFrameFormat->GetULSpace().GetUpper();
    sal_Int16 nLower = pFrameFormat->GetULSpace().GetLower();

    // To emulate, on import left was ignored (set to zero) if aligned to left,
    // so just double up the right spacing in order to prevent cutting in half each round-trip.
    if (rHoriOrient.GetHoriOrient() == text::HoriOrientation::LEFT)
        nLeft = nRight;
    else if (rHoriOrient.GetHoriOrient() == text::HoriOrientation::RIGHT)
        nRight = nLeft;

    attrList->add(FSNS(XML_w, XML_hSpace), OString::number((nLeft + nRight) / 2));
    attrList->add(FSNS(XML_w, XML_vSpace), OString::number((nUpper + nLower) / 2));

    switch (pFrameFormat->GetSurround().GetValue())
    {
    case css::text::WrapTextMode_NONE:
        attrList->add( FSNS( XML_w, XML_wrap), "notBeside");
        break;
    case css::text::WrapTextMode_DYNAMIC:
        attrList->add(FSNS(XML_w, XML_wrap), "auto");
        break;
    case css::text::WrapTextMode_PARALLEL:
    default:
        attrList->add(FSNS(XML_w, XML_wrap), "around");
        break;
    }
    attrList->add( FSNS( XML_w, XML_vAnchor), relativeFromV );
    attrList->add( FSNS( XML_w, XML_hAnchor), relativeFromH );
    attrList->add( FSNS( XML_w, XML_hRule), "exact");

    m_pSerializer->singleElementNS( XML_w, XML_framePr, attrList );
}

void DocxAttributeOutput::EndParagraph( const ww8::WW8TableNodeInfoInner::Pointer_t& pTextNodeInfoInner )
{
    // write the paragraph properties + the run, already in the correct order
    m_pSerializer->mergeTopMarks(Tag_StartParagraph_2);
    std::vector<  std::shared_ptr <ww8::Frame> > aFramePrTextbox;
    // Write the anchored frame if any
    // Word can't handle nested text boxes, so write them on the same level.
    ++m_nTextFrameLevel;
    if( m_nTextFrameLevel == 1 && !m_rExport.SdrExporter().IsDMLAndVMLDrawingOpen() )
    {
        comphelper::FlagRestorationGuard aStartedParaSdtGuard(m_aParagraphSdt.m_bStartedSdt, false);

        assert(!m_oPostponedCustomShape);
        m_oPostponedCustomShape.emplace();

        // The for loop can change the size of m_aFramesOfParagraph, so the max size cannot be set in stone before the loop.
        size_t nFrames = m_aFramesOfParagraph.size() ? m_aFramesOfParagraph.top().size() : 0;
        for (size_t nIndex = 0; nIndex < nFrames; ++nIndex)
        {
            m_bParagraphFrameOpen = true;
            ww8::Frame aFrame = m_aFramesOfParagraph.top()[nIndex];
            const SwFrameFormat& rFrameFormat = aFrame.GetFrameFormat();

            if (!m_bWritingHeaderFooter && SwTextBoxHelper::TextBoxIsFramePr(rFrameFormat))
            {
                std::shared_ptr<ww8::Frame> pFramePr = std::make_shared<ww8::Frame>(aFrame);
                aFramePrTextbox.push_back(pFramePr);
            }
            else
            {
                if (m_aRunSdt.m_bStartedSdt)
                {
                    // Run-level SDT still open? Close it before AlternateContent.
                    m_aRunSdt.EndSdtBlock(m_pSerializer);
                }
                m_pSerializer->startElementNS(XML_w, XML_r);
                m_pSerializer->startElementNS(XML_mc, XML_AlternateContent);
                m_pSerializer->startElementNS(XML_mc, XML_Choice, XML_Requires, "wps");
                /**
                    This is to avoid AlternateContent within another AlternateContent.
                       So when Choice is Open, only write the DML Drawing instead of both DML
                       and VML Drawing in another AlternateContent.
                 **/
                SetAlternateContentChoiceOpen( true );
                /** Save the table info's before writing the shape
                        as there might be a new table that might get
                        spawned from within the VML & DML block and alter
                        the contents.
                */
                ww8::WW8TableInfo::Pointer_t xOldTableInfo = m_rExport.m_pTableInfo;
                //Reset the table infos after saving.
                m_rExport.m_pTableInfo = std::make_shared<ww8::WW8TableInfo>();

                /** FDO#71834 :
                       Save the table reference attributes before calling WriteDMLTextFrame,
                       otherwise the StartParagraph function will use the previous existing
                       table reference attributes since the variable is being shared.
                */
                {
                    DocxTableExportContext aDMLTableExportContext(*this);
                    m_rExport.SdrExporter().writeDMLTextFrame(&aFrame);
                }
                m_pSerializer->endElementNS(XML_mc, XML_Choice);
                SetAlternateContentChoiceOpen( false );

                // Reset table infos, otherwise the depth of the cells will be incorrect,
                // in case the text frame had table(s) and we try to export the
                // same table second time.
                m_rExport.m_pTableInfo = std::make_shared<ww8::WW8TableInfo>();
                //reset the tableReference.

                m_pSerializer->startElementNS(XML_mc, XML_Fallback);
                {
                    DocxTableExportContext aVMLTableExportContext(*this);
                    m_rExport.SdrExporter().writeVMLTextFrame(&aFrame);
                }
                m_rExport.m_pTableInfo = std::move(xOldTableInfo);

                m_pSerializer->endElementNS(XML_mc, XML_Fallback);
                m_pSerializer->endElementNS(XML_mc, XML_AlternateContent);
                m_pSerializer->endElementNS( XML_w, XML_r );
                m_bParagraphFrameOpen = false;
            }

            nFrames = m_aFramesOfParagraph.size() ? m_aFramesOfParagraph.top().size() : 0;
        }
        if (!m_oPostponedCustomShape->empty())
        {
            m_pSerializer->startElementNS(XML_w, XML_r);
            WritePostponedCustomShape();
            m_pSerializer->endElementNS( XML_w, XML_r );
        }
        m_oPostponedCustomShape.reset();

        if ( m_aFramesOfParagraph.size() )
            m_aFramesOfParagraph.top().clear();

        if (!pTextNodeInfoInner)
        {
            // Ending a non-table paragraph, clear floating tables before paragraph.
            m_aFloatingTablesOfParagraph.clear();
        }
    }

    --m_nTextFrameLevel;
    if ( m_aFramesOfParagraph.size() && !m_nTextFrameLevel )
        m_aFramesOfParagraph.pop();

    /* If m_nHyperLinkCount > 0 that means hyperlink tag is not yet closed.
     * This is due to nested hyperlink tags. So close it before end of paragraph.
     */
    if(m_nHyperLinkCount.back() > 0)
    {
        for(sal_Int32 nHyperLinkToClose = 0; nHyperLinkToClose < m_nHyperLinkCount.back(); ++nHyperLinkToClose)
            m_pSerializer->endElementNS( XML_w, XML_hyperlink );
    }
    m_nHyperLinkCount.pop_back();

    if (m_aRunSdt.m_bStartedSdt)
    {
        // Run-level SDT still open? Close it now.
        m_aRunSdt.EndSdtBlock(m_pSerializer);
    }

    if (m_bPageBreakAfter)
    {
        // tdf#128889 Trailing page break
        SectionBreak(msword::PageBreak, false);
        m_bPageBreakAfter = false;
    }

    m_pSerializer->endElementNS( XML_w, XML_p );
    // on export sdt blocks are never nested ATM
    if (!m_bAnchorLinkedToNode && !m_aParagraphSdt.m_bStartedSdt)
    {
        m_aParagraphSdt.WriteSdtBlock(m_pSerializer, m_bRunTextIsOn, m_rExport.SdrExporter().IsParagraphHasDrawing());

        if (m_aParagraphSdt.m_bStartedSdt)
        {
            if (m_tableReference.m_bTableCellOpen)
                m_tableReference.m_bTableCellParaSdtOpen = true;
            if (m_rExport.SdrExporter().IsDMLAndVMLDrawingOpen())
                m_rExport.SdrExporter().setParagraphSdtOpen(true);
        }
    }
    else
    {
        //These should be written out to the actual Node and not to the anchor.
        //Clear them as they will be repopulated when the node is processed.
        m_aParagraphSdt.m_nSdtPrToken = 0;
        m_aParagraphSdt.DeleteAndResetTheLists();
    }

    m_pSerializer->mark(Tag_StartParagraph_2);

    // Write framePr
    for ( const auto & pFrame : aFramePrTextbox )
    {
        DocxTableExportContext aTableExportContext(*this);
        m_aFramePr.SetFrame(pFrame.get(), !m_xTableWrt ? -1 : m_tableReference.m_nTableDepth);
        m_rExport.SdrExporter().writeOnlyTextOfFrame(pFrame.get());
        m_aFramePr.SetFrame(nullptr);
    }

    m_pSerializer->mergeTopMarks(Tag_StartParagraph_2, sax_fastparser::MergeMarks::PREPEND);

    //sdtcontent is written so Set m_bParagraphHasDrawing to false
    m_rExport.SdrExporter().setParagraphHasDrawing(false);
    m_bRunTextIsOn = false;
    m_pSerializer->mergeTopMarks(Tag_StartParagraph_1);

    aFramePrTextbox.clear();
    // Check for end of cell, rows, tables here
    FinishTableRowCell( pTextNodeInfoInner );

    if( !m_rExport.SdrExporter().IsDMLAndVMLDrawingOpen() )
        m_bParagraphOpened = false;

    // Clear bookmarks of the current paragraph
    m_aBookmarksOfParagraphStart.clear();
    m_aBookmarksOfParagraphEnd.clear();
}

#define MAX_CELL_IN_WORD 62

void DocxAttributeOutput::SyncNodelessCells(ww8::WW8TableNodeInfoInner::Pointer_t const & pInner, sal_Int32 nCell, sal_uInt32 nRow)
{
    sal_Int32 nOpenCell = m_LastOpenCell.back();
    if (nOpenCell != -1 && nOpenCell != nCell && nOpenCell < MAX_CELL_IN_WORD)
        EndTableCell(nOpenCell);

    sal_Int32 nClosedCell = m_LastClosedCell.back();
    for (sal_Int32 i = nClosedCell+1; i < nCell; ++i)
    {
        if (i >= MAX_CELL_IN_WORD)
            break;

        if (i == 0)
            StartTableRow(pInner);

        StartTableCell(pInner, i, nRow);
        m_pSerializer->singleElementNS(XML_w, XML_p);
        EndTableCell(i);
    }
}

void DocxAttributeOutput::FinishTableRowCell( ww8::WW8TableNodeInfoInner::Pointer_t const & pInner, bool bForceEmptyParagraph )
{
    if ( !pInner )
        return;

    // Where are we in the table
    sal_uInt32 nRow = pInner->getRow();
    sal_Int32 nCell = pInner->getCell();

    InitTableHelper( pInner );

    // HACK
    // msoffice seems to have an internal limitation of 63 columns for tables
    // and refuses to load .docx with more, even though the spec seems to allow that;
    // so simply if there are more columns, don't close the last one msoffice will handle
    // and merge the contents of the remaining ones into it (since we don't close the cell
    // here, following ones will not be opened)
    const bool limitWorkaround = (nCell >= MAX_CELL_IN_WORD && !pInner->isEndOfLine());
    const bool bEndCell = pInner->isEndOfCell() && !limitWorkaround;
    const bool bEndRow = pInner->isEndOfLine();

    if (bEndCell)
    {
        while (pInner->getDepth() < m_tableReference.m_nTableDepth)
        {
            //we expect that the higher depth row was closed, and
            //we are just missing the table close
            assert(m_LastOpenCell.back() == -1 && m_LastClosedCell.back() == -1);
            EndTable();
        }

        SyncNodelessCells(pInner, nCell, nRow);

        sal_Int32 nClosedCell = m_LastClosedCell.back();
        if (nCell == nClosedCell)
        {
            //Start missing trailing cell(s)
            ++nCell;
            StartTableCell(pInner, nCell, nRow);

            //Continue on missing next trailing cell(s)
            ww8::RowSpansPtr xRowSpans = pInner->getRowSpansOfRow();
            sal_Int32 nRemainingCells = xRowSpans->size() - nCell;
            for (sal_Int32 i = 1; i < nRemainingCells; ++i)
            {
                if (bForceEmptyParagraph)
                {
                    m_pSerializer->singleElementNS(XML_w, XML_p);
                }

                EndTableCell(nCell);

                StartTableCell(pInner, nCell, nRow);
            }
        }

        if (bForceEmptyParagraph)
        {
            m_pSerializer->singleElementNS(XML_w, XML_p);
        }

        EndTableCell(nCell);
    }

    // This is a line end
    if (bEndRow)
        EndTableRow();

    // This is the end of the table
    if (pInner->isFinalEndOfLine())
        EndTable();
}

void DocxAttributeOutput::EmptyParagraph()
{
    m_pSerializer->singleElementNS(XML_w, XML_p);
}

void DocxAttributeOutput::SectionBreaks(const SwNode& rNode)
{
    // output page/section breaks
    // Writer can have them at the beginning of a paragraph, or at the end, but
    // in docx, we have to output them in the paragraph properties of the last
    // paragraph in a section.  To get it right, we have to switch to the next
    // paragraph, and detect the section breaks there.
    SwNodeIndex aNextIndex( rNode, 1 );

    if (rNode.IsTextNode() || rNode.IsSectionNode())
    {
        if (aNextIndex.GetNode().IsTextNode())
        {
            const SwTextNode* pTextNode = static_cast<SwTextNode*>(&aNextIndex.GetNode());
            m_rExport.OutputSectionBreaks(pTextNode->GetpSwAttrSet(), *pTextNode, m_tableReference.m_bTableCellOpen);
        }
        else if (aNextIndex.GetNode().IsTableNode())
        {
            const SwTableNode* pTableNode = static_cast<SwTableNode*>(&aNextIndex.GetNode());
            const SwFrameFormat *pFormat = pTableNode->GetTable().GetFrameFormat();
            m_rExport.OutputSectionBreaks(&(pFormat->GetAttrSet()), *pTableNode);
        }
    }
    else if (rNode.IsEndNode())
    {
        if (aNextIndex.GetNode().IsTextNode())
        {
            // Handle section break between a table and a text node following it.
            // Also handle section endings
            const SwTextNode* pTextNode = aNextIndex.GetNode().GetTextNode();
            if (rNode.StartOfSectionNode()->IsTableNode() || rNode.StartOfSectionNode()->IsSectionNode())
                m_rExport.OutputSectionBreaks(pTextNode->GetpSwAttrSet(), *pTextNode, m_tableReference.m_bTableCellOpen);
        }
        else if (aNextIndex.GetNode().IsTableNode())
        {
            // Handle section break between tables.
            const SwTableNode* pTableNode = static_cast<SwTableNode*>(&aNextIndex.GetNode());
            const SwFrameFormat *pFormat = pTableNode->GetTable().GetFrameFormat();
            m_rExport.OutputSectionBreaks(&(pFormat->GetAttrSet()), *pTableNode);
        }
    }
}

void DocxAttributeOutput::StartParagraphProperties()
{
    m_pSerializer->mark(Tag_StartParagraphProperties);

    m_pSerializer->startElementNS(XML_w, XML_pPr);
    m_bOpenedParaPr = true;

    // and output the section break now (if it appeared)
    if (m_pSectionInfo && m_rExport.m_nTextTyp == TXT_MAINTEXT)
    {
        m_rExport.SectionProperties( *m_pSectionInfo );
        m_pSectionInfo.reset();
    }

    InitCollectedParagraphProperties();
}

void DocxAttributeOutput::InitCollectedParagraphProperties()
{
    m_pLRSpaceAttrList.clear();
    m_pParagraphSpacingAttrList.clear();

    // Write the elements in the spec order
    static const sal_Int32 aOrder[] =
    {
        FSNS( XML_w, XML_pStyle ),
        FSNS( XML_w, XML_keepNext ),
        FSNS( XML_w, XML_keepLines ),
        FSNS( XML_w, XML_pageBreakBefore ),
        FSNS( XML_w, XML_framePr ),
        FSNS( XML_w, XML_widowControl ),
        FSNS( XML_w, XML_numPr ),
        FSNS( XML_w, XML_suppressLineNumbers ),
        FSNS( XML_w, XML_pBdr ),
        FSNS( XML_w, XML_shd ),
        FSNS( XML_w, XML_tabs ),
        FSNS( XML_w, XML_suppressAutoHyphens ),
        FSNS( XML_w, XML_kinsoku ),
        FSNS( XML_w, XML_wordWrap ),
        FSNS( XML_w, XML_overflowPunct ),
        FSNS( XML_w, XML_topLinePunct ),
        FSNS( XML_w, XML_autoSpaceDE ),
        FSNS( XML_w, XML_autoSpaceDN ),
        FSNS( XML_w, XML_bidi ),
        FSNS( XML_w, XML_adjustRightInd ),
        FSNS( XML_w, XML_snapToGrid ),
        FSNS( XML_w, XML_spacing ),
        FSNS( XML_w, XML_ind ),
        FSNS( XML_w, XML_contextualSpacing ),
        FSNS( XML_w, XML_mirrorIndents ),
        FSNS( XML_w, XML_suppressOverlap ),
        FSNS( XML_w, XML_jc ),
        FSNS( XML_w, XML_textDirection ),
        FSNS( XML_w, XML_textAlignment ),
        FSNS( XML_w, XML_textboxTightWrap ),
        FSNS( XML_w, XML_outlineLvl ),
        FSNS( XML_w, XML_divId ),
        FSNS( XML_w, XML_cnfStyle ),
        FSNS( XML_w, XML_rPr ),
        FSNS( XML_w, XML_sectPr ),
        FSNS( XML_w, XML_pPrChange )
    };

    // postpone the output so that we can later [in EndParagraphProperties()]
    // prepend the properties before the run
    // coverity[overrun-buffer-arg : FALSE] - coverity has difficulty with css::uno::Sequence
    m_pSerializer->mark(Tag_InitCollectedParagraphProperties, comphelper::containerToSequence(aOrder));
}

void DocxAttributeOutput::WriteCollectedParagraphProperties()
{
    if ( m_rExport.SdrExporter().getFlyAttrList().is() )
    {
        m_pSerializer->singleElementNS( XML_w, XML_framePr,
                                        detachFrom(m_rExport.SdrExporter().getFlyAttrList() ) );
    }

    if (m_pLRSpaceAttrList.is())
    {
        m_pSerializer->singleElementNS(XML_w, XML_ind, detachFrom(m_pLRSpaceAttrList));
    }

    if ( m_pParagraphSpacingAttrList.is() )
    {
        m_pSerializer->singleElementNS( XML_w, XML_spacing, detachFrom( m_pParagraphSpacingAttrList ) );
    }

    if ( m_pBackgroundAttrList.is() )
    {
        m_pSerializer->singleElementNS( XML_w, XML_shd, detachFrom( m_pBackgroundAttrList ) );
        m_aFramePr.SetUseFrameBackground(false);
    }
}

namespace
{

/// Outputs an item set, that contains the formatting of the paragraph marker.
void lcl_writeParagraphMarkerProperties(DocxAttributeOutput& rAttributeOutput, const SfxItemSet& rParagraphMarkerProperties)
{
    const SfxItemSet* pOldI = rAttributeOutput.GetExport().GetCurItemSet();
    rAttributeOutput.GetExport().SetCurItemSet(&rParagraphMarkerProperties);

    SfxWhichIter aIter(rParagraphMarkerProperties);
    sal_uInt16 nWhichId = aIter.FirstWhich();
    const SfxPoolItem* pItem = nullptr;
    // Did we already produce a <w:sz> element?
    bool bFontSizeWritten = false;
    bool bBoldWritten = false;
    while (nWhichId)
    {
        if (aIter.GetItemState(true, &pItem) == SfxItemState::SET)
        {
            if (isCHRATR(nWhichId) || nWhichId == RES_TXTATR_CHARFMT)
            {
                // Will this item produce a <w:sz> element?
                bool bFontSizeItem = nWhichId == RES_CHRATR_FONTSIZE || nWhichId == RES_CHRATR_CJK_FONTSIZE;
                bool bBoldItem = nWhichId == RES_CHRATR_WEIGHT || nWhichId == RES_CHRATR_CJK_WEIGHT;
                if (!(bFontSizeWritten && bFontSizeItem) && !(bBoldWritten && bBoldItem))
                    rAttributeOutput.OutputItem(*pItem);
                if (bFontSizeItem)
                    bFontSizeWritten = true;
                if (bBoldItem)
                    bBoldWritten = true;
            }
            else if (nWhichId == RES_TXTATR_AUTOFMT)
            {
                const SwFormatAutoFormat pAutoFormat = pItem->StaticWhichCast(RES_TXTATR_AUTOFMT);
                lcl_writeParagraphMarkerProperties(rAttributeOutput, *pAutoFormat.GetStyleHandle());
            }
        }
        nWhichId = aIter.NextWhich();
    }
    rAttributeOutput.GetExport().SetCurItemSet(pOldI);
}

const char * const RubyAlignValues[] =
{
    "center",
    "distributeLetter",
    "distributeSpace",
    "left",
    "right",
    "rightVertical"
};


const char *lclConvertWW8JCToOOXMLRubyAlign(sal_Int32 nJC)
{
    const sal_Int32 nElements = SAL_N_ELEMENTS(RubyAlignValues);
    if ( nJC >=0 && nJC < nElements )
        return RubyAlignValues[nJC];
    return RubyAlignValues[0];
}

}

void DocxAttributeOutput::EndParagraphProperties(const SfxItemSet& rParagraphMarkerProperties, const SwRedlineData* pRedlineData, const SwRedlineData* pRedlineParagraphMarkerDeleted, const SwRedlineData* pRedlineParagraphMarkerInserted)
{
    // Call the 'Redline' function. This will add redline (change-tracking) information that regards to paragraph properties.
    // This includes changes like 'Bold', 'Underline', 'Strikethrough' etc.

    // If there is RedlineData present, call WriteCollectedParagraphProperties() for writing pPr before calling Redline().
    // As there will be another pPr for redline and LO might mix both.
    if(pRedlineData)
        WriteCollectedParagraphProperties();
    Redline( pRedlineData );

    WriteCollectedParagraphProperties();

    // Merge the marks for the ordered elements
    m_pSerializer->mergeTopMarks(Tag_InitCollectedParagraphProperties);

    // Write 'Paragraph Mark' properties
    m_pSerializer->startElementNS(XML_w, XML_rPr);
    // mark() before paragraph mark properties child elements.
    InitCollectedRunProperties();

    // The 'm_pFontsAttrList', 'm_pEastAsianLayoutAttrList', 'm_pCharLangAttrList' are used to hold information
    // that should be collected by different properties in the core, and are all flushed together
    // to the DOCX when the function 'WriteCollectedRunProperties' gets called.
    // So we need to store the current status of these lists, so that we can revert back to them when
    // we are done exporting the redline attributes.
    auto pFontsAttrList_Original(detachFrom(m_pFontsAttrList));
    auto pEastAsianLayoutAttrList_Original(detachFrom(m_pEastAsianLayoutAttrList));
    auto pCharLangAttrList_Original(detachFrom(m_pCharLangAttrList));

    lcl_writeParagraphMarkerProperties(*this, rParagraphMarkerProperties);

    // Write the collected run properties that are stored in 'm_pFontsAttrList', 'm_pEastAsianLayoutAttrList', 'm_pCharLangAttrList'
    WriteCollectedRunProperties();

    // Revert back the original values that were stored in 'm_pFontsAttrList', 'm_pEastAsianLayoutAttrList', 'm_pCharLangAttrList'
    m_pFontsAttrList = std::move(pFontsAttrList_Original);
    m_pEastAsianLayoutAttrList = std::move(pEastAsianLayoutAttrList_Original);
    m_pCharLangAttrList = std::move(pCharLangAttrList_Original);

    if ( pRedlineParagraphMarkerDeleted )
    {
        StartRedline(pRedlineParagraphMarkerDeleted, /*bLastRun=*/true, /*bParagraphProps=*/true);
        EndRedline(pRedlineParagraphMarkerDeleted, /*bLastRun=*/true, /*bParagraphProps=*/true);
    }
    if ( pRedlineParagraphMarkerInserted )
    {
        StartRedline(pRedlineParagraphMarkerInserted, /*bLastRun=*/true, /*bParagraphProps=*/true);
        EndRedline(pRedlineParagraphMarkerInserted, /*bLastRun=*/true, /*bParagraphProps=*/true);
    }

    // mergeTopMarks() after paragraph mark properties child elements.
    m_pSerializer->mergeTopMarks(Tag_InitCollectedRunProperties);
    m_pSerializer->endElementNS( XML_w, XML_rPr );

    if (!m_bWritingHeaderFooter && m_aFramePr.Frame())
    {
        const SwFrameFormat& rFrameFormat = m_aFramePr.Frame()->GetFrameFormat();
        assert(SwTextBoxHelper::TextBoxIsFramePr(rFrameFormat) && "by definition, because Frame()");

        const Size aSize = m_aFramePr.Frame()->GetSize();
        PopulateFrameProperties(&rFrameFormat, aSize);

        // if the paragraph itself never called FormatBox, do so now
        if (m_aFramePr.UseFrameBorders(!m_xTableWrt ? -1 : m_tableReference.m_nTableDepth))
            FormatBox(rFrameFormat.GetBox());

        if (m_aFramePr.UseFrameBackground())
        {
            // The frame is usually imported as 100% transparent. Ignore in that case.
            // Background only exports as fully opaque. Emulate - ignore transparency more than 50%
            const SwAttrSet& rSet = rFrameFormat.GetAttrSet();
            const XFillStyleItem* pFillStyle(rSet.GetItem<XFillStyleItem>(XATTR_FILLSTYLE));
            if (pFillStyle && pFillStyle->GetValue() != drawing::FillStyle_NONE)
            {
                std::unique_ptr<SvxBrushItem> pBrush(
                    getSvxBrushItemFromSourceSet(rSet, RES_BACKGROUND));
                if (pBrush->GetColor().GetAlpha() > 127) // more opaque than transparent
                {
                    FormatBackground(*pBrush);
                    WriteCollectedParagraphProperties();
                }
            }
        }

        if (m_aFramePr.UseFrameTextDirection(!m_xTableWrt ? -1 : m_tableReference.m_nTableDepth))
        {
            const SvxFrameDirectionItem& rFrameDir = rFrameFormat.GetFrameDir();
            if (rFrameDir.GetValue() != SvxFrameDirection::Environment)
            {
                assert(!m_rExport.m_bOutPageDescs);
                // hack: use existing variable to write out the full TextDirection attribute.
                // This is valid for paragraphs/styles - just not native in LO, so hack for now.
                m_rExport.m_bOutPageDescs = true;
                FormatFrameDirection(rFrameDir);
                m_rExport.m_bOutPageDescs = false;
            }
        }

        // reset to true in preparation for the next paragraph in the frame
        m_aFramePr.SetUseFrameBorders(true);
        m_aFramePr.SetUseFrameBackground(true);
        m_aFramePr.SetUseFrameTextDirection(true);
    }

    m_pSerializer->endElementNS( XML_w, XML_pPr );

    if (m_rExport.m_bHasBailsMetaData)
    {
        // RDF metadata for this text node.
        SwTextNode* pTextNode = m_rExport.m_pCurPam->GetPointNode().GetTextNode();
        std::map<OUString, OUString> aStatements;
        if (pTextNode)
            aStatements = SwRDFHelper::getTextNodeStatements(u"urn:bails"_ustr, *pTextNode);
        if (!aStatements.empty())
        {
            m_pSerializer->startElementNS(XML_w, XML_smartTag,
                                          FSNS(XML_w, XML_uri), "http://www.w3.org/1999/02/22-rdf-syntax-ns#",
                                          FSNS(XML_w, XML_element), "RDF");
            m_pSerializer->startElementNS(XML_w, XML_smartTagPr);
            for (const auto& rStatement : aStatements)
                m_pSerializer->singleElementNS(XML_w, XML_attr,
                                               FSNS(XML_w, XML_name), rStatement.first,
                                               FSNS(XML_w, XML_val), rStatement.second);
            m_pSerializer->endElementNS(XML_w, XML_smartTagPr);
            m_pSerializer->endElementNS(XML_w, XML_smartTag);
        }
    }

    if ((m_nColBreakStatus == COLBRK_WRITE || m_nColBreakStatus == COLBRK_WRITEANDPOSTPONE)
        && !m_rExport.SdrExporter().IsDMLAndVMLDrawingOpen())
    {
        m_pSerializer->startElementNS(XML_w, XML_r);
        m_pSerializer->singleElementNS(XML_w, XML_br, FSNS(XML_w, XML_type), "column");
        m_pSerializer->endElementNS( XML_w, XML_r );

        if ( m_nColBreakStatus == COLBRK_WRITEANDPOSTPONE )
            m_nColBreakStatus = COLBRK_POSTPONE;
        else
            m_nColBreakStatus = COLBRK_NONE;
    }

    if (m_bPostponedPageBreak && !m_bWritingHeaderFooter
        && !m_rExport.SdrExporter().IsDMLAndVMLDrawingOpen())
    {
        m_pSerializer->startElementNS(XML_w, XML_r);
        m_pSerializer->singleElementNS(XML_w, XML_br, FSNS(XML_w, XML_type), "page");
        m_pSerializer->endElementNS( XML_w, XML_r );

        m_bPostponedPageBreak = false;
    }

    // merge the properties _before_ the run (strictly speaking, just
    // after the start of the paragraph)
    m_pSerializer->mergeTopMarks(Tag_StartParagraphProperties, sax_fastparser::MergeMarks::PREPEND);

    m_bOpenedParaPr = false;
}

void DocxAttributeOutput::SetStateOfFlyFrame( FlyProcessingState nStateOfFlyFrame )
{
    m_nStateOfFlyFrame = nStateOfFlyFrame;
}

void DocxAttributeOutput::SetAnchorIsLinkedToNode( bool bAnchorLinkedToNode )
{
    m_bAnchorLinkedToNode = bAnchorLinkedToNode ;
}

void DocxAttributeOutput::ResetFlyProcessingFlag()
{
    m_bPostponedProcessingFly = false ;
}

bool DocxAttributeOutput::IsFlyProcessingPostponed()
{
    return m_bPostponedProcessingFly;
}

void DocxAttributeOutput::StartRun( const SwRedlineData* pRedlineData, sal_Int32 /*nPos*/, bool /*bSingleEmptyRun*/ )
{
    // Don't start redline data here, possibly there is a hyperlink later, and
    // that has to be started first.
    m_pRedlineData = pRedlineData;

    // this mark is used to be able to enclose the run inside a sdr tag.
    m_pSerializer->mark(Tag_StartRun_1);

    // postpone the output of the start of a run (there are elements that need
    // to be written before the start of the run, but we learn which they are
    // _inside_ of the run)
    m_pSerializer->mark(Tag_StartRun_2); // let's call it "postponed run start"

    // postpone the output of the text (we get it before the run properties,
    // but must write it after them)
    m_pSerializer->mark(Tag_StartRun_3); // let's call it "postponed text"
}

void DocxAttributeOutput::EndRun(const SwTextNode* pNode, sal_Int32 nPos, sal_Int32 nLen, bool bLastRun)
{
    int nFieldsInPrevHyperlink = m_nFieldsInHyperlink;
    // Reset m_nFieldsInHyperlink if a new hyperlink is about to start
    if ( m_pHyperlinkAttrList.is() )
    {
        m_nFieldsInHyperlink = 0;
    }

    // Write field starts
    for ( std::vector<FieldInfos>::iterator pIt = m_Fields.begin() + nFieldsInPrevHyperlink; pIt != m_Fields.end(); )
    {
        // Add the fields starts for all but hyperlinks and TOCs
        if (pIt->bOpen && pIt->pField && pIt->eType != ww::eFORMDROPDOWN &&
            // it is not an input field with extra grabbag params (sdt field)
            (!(pIt->eType == ww::eFILLIN && static_cast<const SwInputField*>(pIt->pField.get())->getGrabBagParams().hasElements()))
            )
        {
            StartField_Impl( pNode, nPos, *pIt );

            // Remove the field from the stack if only the start has to be written
            // Unknown fields should be removed too
            if ( !pIt->bClose || ( pIt->eType == ww::eUNKNOWN ) )
            {
                pIt = m_Fields.erase( pIt );
                continue;
            }

            if (m_nHyperLinkCount.back() > 0 || m_pHyperlinkAttrList.is())
            {
                ++m_nFieldsInHyperlink;
            }
        }
        ++pIt;
    }

    // write the run properties + the text, already in the correct order
    m_pSerializer->mergeTopMarks(Tag_StartRun_3); // merges with "postponed text", see above

    // level down, to be able to prepend the actual run start attribute (just
    // before "postponed run start")
    m_pSerializer->mark(Tag_EndRun_1); // let's call it "actual run start"
    bool bCloseEarlierSDT = false;

    if (m_bEndCharSdt)
    {
        // This is the common case: "close sdt before the current run" was requested by the next run.

        // if another sdt starts in this run, then wait
        // as closing the sdt now, might cause nesting of sdts
        if (m_aRunSdt.m_nSdtPrToken > 0)
            bCloseEarlierSDT = true;
        else
            m_aRunSdt.EndSdtBlock(m_pSerializer);
        m_bEndCharSdt = false;
    }

    if ( m_closeHyperlinkInPreviousRun )
    {
        if (m_nHyperLinkCount.back() > 0)
        {
            for ( int i = 0; i < nFieldsInPrevHyperlink; i++ )
            {
                // If fields begin before hyperlink then
                // it should end before hyperlink close
                EndField_Impl( pNode, nPos, m_Fields.back( ) );
                m_Fields.pop_back();
            }
            m_pSerializer->endElementNS( XML_w, XML_hyperlink );
            m_endPageRef = false;
            m_nHyperLinkCount.back()--;
            m_closeHyperlinkInPreviousRun = false;
        }
        else
        {
            bool bIsStartedHyperlink = false;
            for (const sal_Int32 nLinkCount : m_nHyperLinkCount)
            {
                if (nLinkCount > 0)
                {
                    bIsStartedHyperlink = true;
                    break;
                }
            }
            if (!bIsStartedHyperlink)
                m_closeHyperlinkInPreviousRun = false;
        }
    }

    // Write the hyperlink and toc fields starts
    for ( std::vector<FieldInfos>::iterator pIt = m_Fields.begin(); pIt != m_Fields.end(); )
    {
        // Add the fields starts for hyperlinks, TOCs and index marks
        if (pIt->bOpen && (!pIt->pField || pIt->eType == ww::eFORMDROPDOWN ||
            // InputField with extra grabbag params - it is sdt field
            (pIt->eType == ww::eFILLIN && static_cast<const SwInputField*>(pIt->pField.get())->getGrabBagParams().hasElements())))
        {
            StartRedline( m_pRedlineData, bLastRun );
            StartField_Impl( pNode, nPos, *pIt, true );
            EndRedline( m_pRedlineData, bLastRun );

            if (m_nHyperLinkCount.back() > 0)
                ++m_nFieldsInHyperlink;

            // Remove the field if no end needs to be written
            if (!pIt->bSep)
            {
                pIt = m_Fields.erase( pIt );
                continue;
            }
        }
        if (pIt->bSep && !pIt->pField)
        {
            // for TOXMark:
            // Word ignores bookmarks in field result that is empty;
            // work around this by writing bookmark into field command.
            if (!m_sFieldBkm.isEmpty())
            {
                DoWriteBookmarkTagStart(m_sFieldBkm);
                DoWriteBookmarkTagEnd(m_nNextBookmarkId);
                m_nNextBookmarkId++;
                m_sFieldBkm.clear();
            }
            CmdEndField_Impl(pNode, nPos, true);
            // Remove the field if no end needs to be written
            if (!pIt->bClose)
            {
                pIt = m_Fields.erase( pIt );
                continue;
            }
        }
        ++pIt;
    }

    // Start the hyperlink after the fields separators or we would generate invalid file
    bool newStartedHyperlink(false);
    if ( m_pHyperlinkAttrList.is() )
    {
        // if we are ending a hyperlink and there's another one starting here,
        // don't do this, so that the fields are closed further down when
        // the end hyperlink is handled, which is more likely to put the end in
        // the right place, as far as i can tell (not very far in this muck)
        if (!m_closeHyperlinkInThisRun)
        {
            // end ToX fields that want to end _before_ starting the hyperlink
            for (auto it = m_Fields.rbegin(); it != m_Fields.rend(); )
            {
                if (it->bClose && !it->pField)
                {
                    EndField_Impl( pNode, nPos, *it );
                    it = decltype(m_Fields)::reverse_iterator(m_Fields.erase(it.base() - 1));
                }
                else
                {
                    ++it;
                }
            }
        }
        newStartedHyperlink = true;

        m_pSerializer->startElementNS( XML_w, XML_hyperlink, detachFrom( m_pHyperlinkAttrList ) );
        m_nHyperLinkCount.back()++;
    }

    // XML_r node should be surrounded with bookmark-begin and bookmark-end nodes if it has bookmarks.
    // The same is applied for permission ranges.
    // But due to unit test "testFdo85542" let's output bookmark-begin with bookmark-end.
    DoWriteBookmarksStart(m_rBookmarksStart, m_pMoveRedlineData);
    DoWriteBookmarksEnd(m_rBookmarksEnd, false, false); // Write non-moverange bookmarks
    DoWritePermissionsStart();
    DoWriteAnnotationMarks();
    // if there is some redlining in the document, output it
    bool bSkipRedline = false;
    if (nLen == 1)
    {
        // Don't redline content-controls--Word doesn't do them.
        SwTextAttr* pAttr
            = pNode->GetTextAttrAt(nPos, RES_TXTATR_CONTENTCONTROL, sw::GetTextAttrMode::Default);
        if (pAttr && pAttr->GetStart() == nPos)
        {
            bSkipRedline = true;
        }
    }

    if (!bSkipRedline)
    {
        StartRedline(m_pRedlineData, bLastRun);
    }

    if (m_closeHyperlinkInThisRun && m_nHyperLinkCount.back() > 0 && !m_hyperLinkAnchor.isEmpty()
        && m_hyperLinkAnchor.startsWith("_Toc"))
    {
        OUString sToken;
        m_pSerializer->startElementNS(XML_w, XML_r);
        m_pSerializer->startElementNS(XML_w, XML_rPr);
        m_pSerializer->singleElementNS(XML_w, XML_webHidden);
        m_pSerializer->endElementNS( XML_w, XML_rPr );
        m_pSerializer->startElementNS(XML_w, XML_fldChar, FSNS(XML_w, XML_fldCharType), "begin");
        m_pSerializer->endElementNS( XML_w, XML_fldChar );
        m_pSerializer->endElementNS( XML_w, XML_r );


        m_pSerializer->startElementNS(XML_w, XML_r);
        m_pSerializer->startElementNS(XML_w, XML_rPr);
        m_pSerializer->singleElementNS(XML_w, XML_webHidden);
        m_pSerializer->endElementNS( XML_w, XML_rPr );
        sToken = "PAGEREF " + m_hyperLinkAnchor + " \\h"; // '\h' Creates a hyperlink to the bookmarked paragraph.
        DoWriteCmd( sToken );
        m_pSerializer->endElementNS( XML_w, XML_r );

        // Write the Field separator
        m_pSerializer->startElementNS(XML_w, XML_r);
        m_pSerializer->startElementNS(XML_w, XML_rPr);
        m_pSerializer->singleElementNS(XML_w, XML_webHidden);
        m_pSerializer->endElementNS( XML_w, XML_rPr );
        m_pSerializer->singleElementNS( XML_w, XML_fldChar,
                FSNS( XML_w, XML_fldCharType ), "separate" );
        m_pSerializer->endElementNS( XML_w, XML_r );
        // At start of every "PAGEREF" field m_endPageRef value should be true.
        m_endPageRef = true;
    }

    DoWriteBookmarkStartIfExist(nPos);

    if (nLen != -1)
    {
        SwTextAttr* pAttr = pNode->GetTextAttrAt(nPos, RES_TXTATR_CONTENTCONTROL, ::sw::GetTextAttrMode::Default);
        if (pAttr && pAttr->GetStart() == nPos)
        {
            auto pTextContentControl = static_txtattr_cast<SwTextContentControl*>(pAttr);
            m_pContentControl = pTextContentControl->GetContentControl().GetContentControl();
            if (!m_tableReference.m_bTableCellChanged)
            {
                WriteContentControlStart();
            }
        }
    }

    m_pSerializer->startElementNS(XML_w, XML_r);
    if(GetExport().m_bTabInTOC && m_pHyperlinkAttrList.is())
    {
        RunText(u"\t"_ustr) ;
    }
    m_pSerializer->mergeTopMarks(Tag_EndRun_1, sax_fastparser::MergeMarks::PREPEND); // merges with "postponed run start", see above

    if ( !m_sRawText.isEmpty() )
    {
        RunText( m_sRawText );
        m_sRawText.clear();
    }

    // write the run start + the run content
    m_pSerializer->mergeTopMarks(Tag_StartRun_2); // merges the "actual run start"
    // append the actual run end
    m_pSerializer->endElementNS( XML_w, XML_r );

    // if there is some redlining in the document, output it
    // (except in the case of fields with multiple runs)
    if (!bSkipRedline)
    {
        EndRedline(m_pRedlineData, bLastRun);
    }
    DoWriteBookmarksEnd(m_rBookmarksEnd, false, true); // Write moverange bookmarks

    if (nLen != -1)
    {
        sal_Int32 nEnd = nPos + nLen;
        SwTextAttr* pAttr = pNode->GetTextAttrAt(nPos, RES_TXTATR_CONTENTCONTROL, ::sw::GetTextAttrMode::Default);
        if (pAttr && *pAttr->GetEnd() == nEnd && !m_tableReference.m_bTableCellChanged)
        {
            WriteContentControlEnd();
        }
    }

    // enclose in a sdt block, if necessary: if one is already started, then don't do it for now
    // (so on export sdt blocks are never nested ATM)
    if ( !m_bAnchorLinkedToNode && !m_aRunSdt.m_bStartedSdt)
    {
        m_aRunSdt.WriteSdtBlock(m_pSerializer, m_bRunTextIsOn, m_rExport.SdrExporter().IsParagraphHasDrawing());
    }
    else
    {
        //These should be written out to the actual Node and not to the anchor.
        //Clear them as they will be repopulated when the node is processed.
        m_aRunSdt.m_nSdtPrToken = 0;
        m_aRunSdt.DeleteAndResetTheLists();
    }

    if (bCloseEarlierSDT)
    {
        m_pSerializer->mark(Tag_EndRun_2);
        m_aRunSdt.EndSdtBlock(m_pSerializer);
        m_pSerializer->mergeTopMarks(Tag_EndRun_2, sax_fastparser::MergeMarks::PREPEND);
    }

    m_pSerializer->mergeTopMarks(Tag_StartRun_1);

    // XML_r node should be surrounded with permission-begin and permission-end nodes if it has permission.
    DoWritePermissionsEnd();

    for (const auto& rpMath : m_aPostponedMaths)
        WritePostponedMath(rpMath.pMathObject, rpMath.nMathObjAlignment);
    m_aPostponedMaths.clear();

    for (const auto& rpControl : m_aPostponedFormControls)
        WritePostponedFormControl(rpControl);
    m_aPostponedFormControls.clear();

    WritePostponedActiveXControl(false);

    WritePendingPlaceholder();

    if ( !m_bWritingField )
    {
        m_pRedlineData = nullptr;
    }

    if ( m_closeHyperlinkInThisRun )
    {
        if (m_nHyperLinkCount.back() > 0)
        {
            if( m_endPageRef )
            {
                // Hyperlink is started and fldchar "end" needs to be written for PAGEREF
                m_pSerializer->startElementNS(XML_w, XML_r);
                m_pSerializer->startElementNS(XML_w, XML_rPr);
                m_pSerializer->singleElementNS(XML_w, XML_webHidden);
                m_pSerializer->endElementNS( XML_w, XML_rPr );
                m_pSerializer->singleElementNS( XML_w, XML_fldChar,
                        FSNS( XML_w, XML_fldCharType ), "end" );
                m_pSerializer->endElementNS( XML_w, XML_r );
                m_endPageRef = false;
                m_hyperLinkAnchor.clear();
            }
            for ( int i = 0; i < m_nFieldsInHyperlink; i++ )
            {
                // If fields begin after hyperlink start then
                // it should end before hyperlink close
                EndField_Impl( pNode, nPos, m_Fields.back( ) );
                m_Fields.pop_back();
            }
            m_nFieldsInHyperlink = 0;

            m_pSerializer->endElementNS( XML_w, XML_hyperlink );
            m_nHyperLinkCount.back()--;
            m_closeHyperlinkInThisRun = false;
        }
        else
        {
            bool bIsStartedHyperlink = false;
            for (const sal_Int32 nLinkCount : m_nHyperLinkCount)
            {
                if (nLinkCount > 0)
                {
                    bIsStartedHyperlink = true;
                    break;
                }
            }
            if (!bIsStartedHyperlink)
                m_closeHyperlinkInThisRun = false;
        }
    }

    if (!newStartedHyperlink)
    {
        while ( m_Fields.begin() != m_Fields.end() )
        {
            EndField_Impl( pNode, nPos, m_Fields.front( ) );
            m_Fields.erase( m_Fields.begin( ) );
        }
        m_nFieldsInHyperlink = 0;
    }

    // end ToX fields
    for (auto it = m_Fields.rbegin(); it != m_Fields.rend(); )
    {
        if (it->bClose && !it->pField)
        {
            EndField_Impl( pNode, nPos, *it );
            it = decltype(m_Fields)::reverse_iterator(m_Fields.erase(it.base() - 1));
        }
        else
        {
            ++it;
        }
    }

    if ( m_pRedlineData )
    {
        EndRedline( m_pRedlineData, bLastRun );
        m_pRedlineData = nullptr;
    }

    DoWriteBookmarksStart(m_rFinalBookmarksStart);
    DoWriteBookmarksEnd(m_rFinalBookmarksEnd); // Write all final bookmarks
    DoWriteBookmarkEndIfExist(nPos);
}

void DocxAttributeOutput::DoWriteBookmarkTagStart(const OUString& bookmarkName)
{
    m_pSerializer->singleElementNS(XML_w, XML_bookmarkStart,
        FSNS(XML_w, XML_id), OString::number(m_nNextBookmarkId),
        FSNS(XML_w, XML_name), GetExport().BookmarkToWord(bookmarkName));
}

void DocxAttributeOutput::DoWriteBookmarkTagEnd(sal_Int32 const nId)
{
    m_pSerializer->singleElementNS(XML_w, XML_bookmarkEnd,
        FSNS(XML_w, XML_id), OString::number(nId));
}

void DocxAttributeOutput::DoWriteMoveRangeTagStart(std::u16string_view bookmarkName,
    bool bFrom, const SwRedlineData* pRedlineData)
{
    bool bRemovePersonalInfo = SvtSecurityOptions::IsOptionSet(
        SvtSecurityOptions::EOption::DocWarnRemovePersonalInfo ) && !SvtSecurityOptions::IsOptionSet(
            SvtSecurityOptions::EOption::DocWarnKeepRedlineInfo);

    const OUString& rAuthor(SwModule::get()->GetRedlineAuthor(pRedlineData->GetAuthor()));
    const DateTime& aDateTime = pRedlineData->GetTimeStamp();
    bool bNoDate = bRemovePersonalInfo ||
        ( aDateTime.GetYear() == 1970 && aDateTime.GetMonth() == 1 && aDateTime.GetDay() == 1 );

    rtl::Reference<sax_fastparser::FastAttributeList> pAttributeList
        = sax_fastparser::FastSerializerHelper::createAttrList();

    pAttributeList->add(FSNS(XML_w, XML_id), OString::number(m_nNextBookmarkId));
    pAttributeList->add(FSNS(XML_w, XML_author ), bRemovePersonalInfo
                    ? "Author" + OString::number( GetExport().GetInfoID(rAuthor) )
                    : OUStringToOString(rAuthor, RTL_TEXTENCODING_UTF8));
    if (!bNoDate)
        pAttributeList->add(FSNS(XML_w, XML_date ), DateTimeToOString( aDateTime ));
    pAttributeList->add(FSNS(XML_w, XML_name), bookmarkName);
    m_pSerializer->singleElementNS( XML_w, bFrom ? XML_moveFromRangeStart : XML_moveToRangeStart, pAttributeList );

    // tdf#150166 avoid of unpaired moveRangeEnd at moved ToC
    m_rSavedBookmarksIds.insert(m_nNextBookmarkId);
}

void DocxAttributeOutput::DoWriteMoveRangeTagEnd(sal_Int32 const nId, bool bFrom)
{
    if ( m_rSavedBookmarksIds.count(nId) )
    {
        m_pSerializer->singleElementNS(XML_w, bFrom
                ? XML_moveFromRangeEnd
                : XML_moveToRangeEnd,
            FSNS(XML_w, XML_id), OString::number(nId));

        m_rSavedBookmarksIds.erase(nId);
    }
}

void DocxAttributeOutput::DoWriteBookmarkStartIfExist(sal_Int32 nRunPos)
{
    auto aRange = m_aBookmarksOfParagraphStart.equal_range(nRunPos);
    for( auto aIter = aRange.first; aIter != aRange.second; ++aIter)
    {
        DoWriteBookmarkTagStart(aIter->second);
        m_rOpenedBookmarksIds[aIter->second] = m_nNextBookmarkId;
        m_sLastOpenedBookmark = GetExport().BookmarkToWord(aIter->second);
        m_nNextBookmarkId++;
    }
}

void DocxAttributeOutput::DoWriteBookmarkEndIfExist(sal_Int32 nRunPos)
{
    auto aRange = m_aBookmarksOfParagraphEnd.equal_range(nRunPos);
    for( auto aIter = aRange.first; aIter != aRange.second; ++aIter)
    {
        // Get the id of the bookmark
        auto pPos = m_rOpenedBookmarksIds.find(aIter->second);
        if (pPos != m_rOpenedBookmarksIds.end())
        {
            // Output the bookmark
            DoWriteBookmarkTagEnd(pPos->second);
            m_rOpenedBookmarksIds.erase(aIter->second);
        }
    }
}

/// Write the start bookmarks
void DocxAttributeOutput::DoWriteBookmarksStart(std::vector<OUString>& rStarts, const SwRedlineData* pRedlineData)
{
    for (const OUString & bookmarkName : rStarts)
    {
        // Output the bookmark (including MoveBookmark of the tracked moving)
        bool bMove = false;
        bool bFrom = false;
        OUString sBookmarkName = GetExport().BookmarkToWord(bookmarkName, &bMove, &bFrom);
        if ( bMove )
        {
            // TODO: redline data of MoveBookmark is restored from the first redline of the bookmark
            // range. But a later deletion within a tracked moving is still imported as plain
            // deletion, so check IsMoved() and skip the export of the tracked moving to avoid
            // export with bad author or date
            if ( pRedlineData && pRedlineData->IsMoved() )
                DoWriteMoveRangeTagStart(sBookmarkName, bFrom, pRedlineData);
        }
        else
            DoWriteBookmarkTagStart(bookmarkName);

        m_rOpenedBookmarksIds[bookmarkName] = m_nNextBookmarkId;
        m_sLastOpenedBookmark = sBookmarkName;
        m_nNextBookmarkId++;
    }
    rStarts.clear();
}

/// export the end bookmarks
void DocxAttributeOutput::DoWriteBookmarksEnd(std::vector<OUString>& rEnds, bool bWriteAllBookmarks,
                                              bool bWriteOnlyMoveRanges)
{
    auto bookmarkNameIt = rEnds.begin();
    while (bookmarkNameIt != rEnds.end())
    {
        // Get the id of the bookmark
        auto pPos = m_rOpenedBookmarksIds.find(*bookmarkNameIt);

        if (pPos != m_rOpenedBookmarksIds.end())
        {
            bool bMove = false;
            bool bFrom = false;
            GetExport().BookmarkToWord(*bookmarkNameIt, &bMove, &bFrom);
            // Output the bookmark (including MoveBookmark of the tracked moving)
            if (bWriteAllBookmarks || (bMove == bWriteOnlyMoveRanges))
            {
                if (bMove)
                    DoWriteMoveRangeTagEnd(pPos->second, bFrom);
                else
                    DoWriteBookmarkTagEnd(pPos->second);

                m_rOpenedBookmarksIds.erase(*bookmarkNameIt);
                bookmarkNameIt = rEnds.erase(bookmarkNameIt);
            }
            else
                ++bookmarkNameIt;
        }
        else
            bookmarkNameIt = rEnds.erase(bookmarkNameIt);
    }
}

// For construction of the special bookmark name template for permissions:
// see, PermInsertPosition::createBookmarkName()
//
// Syntax:
// - "permission-for-user:<permission-id>:<permission-user-name>"
// - "permission-for-group:<permission-id>:<permission-group-name>"
//
void DocxAttributeOutput::DoWritePermissionTagStart(const OUString& permission)
{
    if (m_aOpenedPermissions.find(permission) != m_aOpenedPermissions.end())
        return;
    m_aOpenedPermissions.insert(permission);

    std::u16string_view permissionIdAndName;

    sal_Int32 nFSNS;

    if (o3tl::starts_with(permission, u"permission-for-group:", &permissionIdAndName))
    {
        nFSNS = FSNS(XML_w, XML_edGrp);
    }
    else
    {
        auto const ok = o3tl::starts_with(
            permission, u"permission-for-user:", &permissionIdAndName);
        assert(ok); (void)ok;
        nFSNS = FSNS(XML_w, XML_ed);
    }

    const std::size_t separatorIndex = permissionIdAndName.find(u':');
    assert(separatorIndex != std::u16string_view::npos);
    const OUString permissionId(permissionIdAndName.substr(0, separatorIndex));
    const OUString permissionName(permissionIdAndName.substr(separatorIndex + 1));

    m_pSerializer->singleElementNS(XML_w, XML_permStart,
        FSNS(XML_w, XML_id), GetExport().BookmarkToWord(permissionId),
        nFSNS, GetExport().BookmarkToWord(permissionName));
}

// For construction of the special bookmark name template for permissions:
// see, PermInsertPosition::createBookmarkName()
//
// Syntax:
// - "permission-for-user:<permission-id>:<permission-user-name>"
// - "permission-for-group:<permission-id>:<permission-group-name>"
//
void DocxAttributeOutput::DoWritePermissionTagEnd(const OUString& permission)
{
    if (m_aOpenedPermissions.find(permission) == m_aOpenedPermissions.end())
        return;

    std::u16string_view permissionIdAndName;

    auto const ok = o3tl::starts_with(permission, u"permission-for-group:", &permissionIdAndName) ||
        o3tl::starts_with(permission, u"permission-for-user:", &permissionIdAndName);
    assert(ok); (void)ok;

    const std::size_t separatorIndex = permissionIdAndName.find(u':');
    assert(separatorIndex != std::u16string_view::npos);
    const OUString permissionId(permissionIdAndName.substr(0, separatorIndex));

    m_pSerializer->singleElementNS(XML_w, XML_permEnd,
        FSNS(XML_w, XML_id), GetExport().BookmarkToWord(permissionId));
    m_aOpenedPermissions.erase(permission);
}

/// Write the start permissions
void DocxAttributeOutput::DoWritePermissionsStart()
{
    for (const OUString & permission : m_rPermissionsStart)
    {
        DoWritePermissionTagStart(permission);
    }
    m_rPermissionsStart.clear();
}

/// export the end permissions
void DocxAttributeOutput::DoWritePermissionsEnd()
{
    for (const OUString & permission : m_rPermissionsEnd)
    {
        DoWritePermissionTagEnd(permission);
    }
    m_rPermissionsEnd.clear();
}

void DocxAttributeOutput::DoWriteAnnotationMarks()
{
    // Write the start annotation marks
    for ( const auto & rName : m_rAnnotationMarksStart )
    {
        // Output the annotation mark
        /* Ensure that the existing Annotation Marks are not overwritten
           as it causes discrepancy when DocxAttributeOutput::PostitField
           refers to this map & while mapping comment id's in document.xml &
           comment.xml.
        */
        if ( m_rOpenedAnnotationMarksIds.end() == m_rOpenedAnnotationMarksIds.find( rName ) )
        {
            const sal_Int32 nId = m_nNextAnnotationMarkId++;
            m_rOpenedAnnotationMarksIds[rName] = nId;
            m_pSerializer->singleElementNS( XML_w, XML_commentRangeStart,
                FSNS( XML_w, XML_id ), OString::number(nId) );
            m_sLastOpenedAnnotationMark = rName;
        }
    }
    m_rAnnotationMarksStart.clear();

    // export the end annotation marks
    for ( const auto & rName : m_rAnnotationMarksEnd )
    {
        // Get the id of the annotation mark
        auto pPos = m_rOpenedAnnotationMarksIds.find( rName );
        if ( pPos != m_rOpenedAnnotationMarksIds.end(  ) )
        {
            const sal_Int32 nId = ( *pPos ).second;
            m_pSerializer->singleElementNS( XML_w, XML_commentRangeEnd,
                FSNS( XML_w, XML_id ), OString::number(nId) );
            m_rOpenedAnnotationMarksIds.erase( rName );

            m_pSerializer->startElementNS(XML_w, XML_r);
            m_pSerializer->singleElementNS( XML_w, XML_commentReference, FSNS( XML_w, XML_id ),
                                            OString::number(nId) );
            m_pSerializer->endElementNS(XML_w, XML_r);
        }
    }
    m_rAnnotationMarksEnd.clear();
}

void DocxAttributeOutput::WriteFFData(  const FieldInfos& rInfos )
{
    const ::sw::mark::Fieldmark& rFieldmark = *rInfos.pFieldmark;
    FieldMarkParamsHelper params( rFieldmark );

    OUString sEntryMacro;
    params.extractParam(u"EntryMacro"_ustr, sEntryMacro);
    OUString sExitMacro;
    params.extractParam(u"ExitMacro"_ustr, sExitMacro);
    OUString sHelp;
    params.extractParam(u"Help"_ustr, sHelp);
    OUString sHint;
    params.extractParam(u"Hint"_ustr, sHint); // .docx StatusText
    if ( sHint.isEmpty() )
        params.extractParam(u"Description"_ustr, sHint); // .doc StatusText

    if ( rInfos.eType == ww::eFORMDROPDOWN )
    {
        uno::Sequence< OUString> vListEntries;
        SwMarkName sName;
        OUString sSelected;

        params.extractParam( ODF_FORMDROPDOWN_LISTENTRY, vListEntries );
        if (vListEntries.getLength() > ODF_FORMDROPDOWN_ENTRY_COUNT_LIMIT)
            vListEntries = uno::Sequence< OUString>(vListEntries.getArray(), ODF_FORMDROPDOWN_ENTRY_COUNT_LIMIT);

        sName = params.getName();
        sal_Int32 nSelectedIndex = 0;

        if ( params.extractParam( ODF_FORMDROPDOWN_RESULT, nSelectedIndex ) )
        {
            if (nSelectedIndex < vListEntries.getLength() )
                sSelected = vListEntries[ nSelectedIndex ];
        }

        GetExport().DoComboBox( sName.toString(), OUString(), OUString(), sSelected, vListEntries );
    }
    else if ( rInfos.eType == ww::eFORMCHECKBOX )
    {
        const SwMarkName& sName = params.getName();
        bool bChecked = false;

        const sw::mark::CheckboxFieldmark* pCheckboxFm = dynamic_cast<const sw::mark::CheckboxFieldmark*>(&rFieldmark);
        if ( pCheckboxFm && pCheckboxFm->IsChecked() )
            bChecked = true;

        FFDataWriterHelper ffdataOut( m_pSerializer );
        ffdataOut.WriteFormCheckbox( sName.toString(), sEntryMacro, sExitMacro, sHelp, sHint, bChecked );
    }
    else if ( rInfos.eType == ww::eFORMTEXT )
    {
        OUString sType;
        params.extractParam(u"Type"_ustr, sType);
        OUString sDefaultText;
        params.extractParam(u"Content"_ustr, sDefaultText);
        sal_uInt16 nMaxLength = 0;
        params.extractParam(u"MaxLength"_ustr, nMaxLength);
        OUString sFormat;
        params.extractParam(u"Format"_ustr, sFormat);
        FFDataWriterHelper ffdataOut( m_pSerializer );
        ffdataOut.WriteFormText( params.getName().toString(), sEntryMacro, sExitMacro, sHelp, sHint,
                                 sType, sDefaultText, nMaxLength, sFormat );
    }
}

void DocxAttributeOutput::WriteFormDateStart(const OUString& sFullDate, const OUString& sDateFormat, const OUString& sLang, const uno::Sequence<beans::PropertyValue>& aGrabBagSdt)
{
    m_pSerializer->startElementNS(XML_w, XML_sdt);
    m_pSerializer->startElementNS(XML_w, XML_sdtPr);

    if(!sFullDate.isEmpty())
        m_pSerializer->startElementNS(XML_w, XML_date, FSNS(XML_w, XML_fullDate), sFullDate);
    else
        m_pSerializer->startElementNS(XML_w, XML_date);

    // Replace quotation mark used for marking static strings in date format
    OUString sDateFormat1 = sDateFormat.replaceAll("\"", "'");
    m_pSerializer->singleElementNS(XML_w, XML_dateFormat,
                                   FSNS(XML_w, XML_val), sDateFormat1);
    m_pSerializer->singleElementNS(XML_w, XML_lid,
                                   FSNS(XML_w, XML_val), sLang);
    m_pSerializer->singleElementNS(XML_w, XML_storeMappedDataAs,
                                   FSNS(XML_w, XML_val), "dateTime");
    m_pSerializer->singleElementNS(XML_w, XML_calendar,
                                   FSNS(XML_w, XML_val), "gregorian");
    m_pSerializer->endElementNS(XML_w, XML_date);

    if (aGrabBagSdt.hasElements())
    {
        // There are some extra sdt parameters came from grab bag
        SdtBlockHelper aSdtBlock;
        aSdtBlock.GetSdtParamsFromGrabBag(aGrabBagSdt);
        aSdtBlock.WriteExtraParams(m_pSerializer);
    }

    m_pSerializer->endElementNS(XML_w, XML_sdtPr);

    m_pSerializer->startElementNS(XML_w, XML_sdtContent);
}

void DocxAttributeOutput::WriteSdtPlainText(const OUString & sValue, const uno::Sequence<beans::PropertyValue>& aGrabBagSdt)
{
    m_pSerializer->startElementNS(XML_w, XML_sdt);
    m_pSerializer->startElementNS(XML_w, XML_sdtPr);

    if (aGrabBagSdt.hasElements())
    {
        // There are some extra sdt parameters came from grab bag
        SdtBlockHelper aSdtBlock;
        aSdtBlock.GetSdtParamsFromGrabBag(aGrabBagSdt);
        aSdtBlock.WriteExtraParams(m_pSerializer);

        if (aSdtBlock.m_nSdtPrToken && aSdtBlock.m_nSdtPrToken != FSNS(XML_w, XML_id))
        {
            // Write <w:text/> or whatsoever from grabbag
            m_pSerializer->singleElement(aSdtBlock.m_nSdtPrToken);
        }

        // Store databindings data for later writing to corresponding XMLs
        OUString sPrefixMapping, sXpath;
        for (const auto& rProp : aGrabBagSdt)
        {
            if (rProp.Name == "ooxml:CT_SdtPr_dataBinding")
            {
                uno::Sequence<beans::PropertyValue> aDataBindingProps;
                rProp.Value >>= aDataBindingProps;
                for (const auto& rDBProp : aDataBindingProps)
                {
                    if (rDBProp.Name == "ooxml:CT_DataBinding_prefixMappings")
                        sPrefixMapping = rDBProp.Value.get<OUString>();
                    else if (rDBProp.Name == "ooxml:CT_DataBinding_xpath")
                        sXpath = rDBProp.Value.get<OUString>();
                }
            }
        }

        if (sXpath.getLength())
        {
            // Given xpath is sufficient
            m_rExport.AddSdtData(sPrefixMapping, sXpath, sValue);
        }
    }

    m_pSerializer->endElementNS(XML_w, XML_sdtPr);

    m_pSerializer->startElementNS(XML_w, XML_sdtContent);
}

void DocxAttributeOutput::WriteContentControlStart()
{
    if (!m_pContentControl)
    {
        return;
    }

    m_pSerializer->startElementNS(XML_w, XML_sdt);
    m_pSerializer->startElementNS(XML_w, XML_sdtPr);
    if (!m_pContentControl->GetPlaceholderDocPart().isEmpty())
    {
        m_pSerializer->startElementNS(XML_w, XML_placeholder);
        m_pSerializer->singleElementNS(XML_w, XML_docPart, FSNS(XML_w, XML_val),
                                       m_pContentControl->GetPlaceholderDocPart());
        m_pSerializer->endElementNS(XML_w, XML_placeholder);
    }

    if (!m_pContentControl->GetDataBindingPrefixMappings().isEmpty() || !m_pContentControl->GetDataBindingXpath().isEmpty() || !m_pContentControl->GetDataBindingStoreItemID().isEmpty())
    {
        m_pSerializer->singleElementNS( XML_w, XML_dataBinding,
            FSNS(XML_w, XML_prefixMappings), m_pContentControl->GetDataBindingPrefixMappings(),
            FSNS(XML_w, XML_xpath), m_pContentControl->GetDataBindingXpath(),
            FSNS(XML_w, XML_storeItemID), m_pContentControl->GetDataBindingStoreItemID());
    }

    if (!m_pContentControl->GetColor().isEmpty())
    {
        m_pSerializer->singleElementNS(XML_w15, XML_color, FSNS(XML_w, XML_val),
                                       m_pContentControl->GetColor());
    }

    if (!m_pContentControl->GetAppearance().isEmpty())
    {
        m_pSerializer->singleElementNS(XML_w15, XML_appearance, FSNS(XML_w15, XML_val),
                                       m_pContentControl->GetAppearance());
    }

    if (!m_pContentControl->GetAlias().isEmpty())
    {
        m_pSerializer->singleElementNS(XML_w, XML_alias, FSNS(XML_w, XML_val),
                                       m_pContentControl->GetAlias());
    }

    if (!m_pContentControl->GetTag().isEmpty())
    {
        m_pSerializer->singleElementNS(XML_w, XML_tag, FSNS(XML_w, XML_val),
                                       m_pContentControl->GetTag());
    }

    if (m_pContentControl->GetId())
    {
        m_pSerializer->singleElementNS(XML_w, XML_id, FSNS(XML_w, XML_val),
                                       OString::number(m_pContentControl->GetId()));
    }

    if (m_pContentControl->GetTabIndex())
    {
        // write the unsigned value as if it were signed since that is all we can import
        const sal_Int32 nTabIndex = static_cast<sal_Int32>(m_pContentControl->GetTabIndex());
        m_pSerializer->singleElementNS(XML_w, XML_tabIndex, FSNS(XML_w, XML_val),
                                       OString::number(nTabIndex));
    }

    if (!m_pContentControl->GetLock().isEmpty())
    {
        m_pSerializer->singleElementNS(XML_w, XML_lock, FSNS(XML_w, XML_val),
                                       m_pContentControl->GetLock());
    }

    if (m_pContentControl->GetShowingPlaceHolder())
    {
        m_pSerializer->singleElementNS(XML_w, XML_showingPlcHdr);
    }

    if (m_pContentControl->GetPicture())
    {
        m_pSerializer->singleElementNS(XML_w, XML_picture);
    }

    if (m_pContentControl->GetCheckbox())
    {
        m_pSerializer->startElementNS(XML_w14, XML_checkbox);
        m_pSerializer->singleElementNS(XML_w14, XML_checked, FSNS(XML_w14, XML_val),
                                       OString::number(int(m_pContentControl->GetChecked())));
        OUString aCheckedState = m_pContentControl->GetCheckedState();
        if (!aCheckedState.isEmpty())
        {
            m_pSerializer->singleElementNS(XML_w14, XML_checkedState, FSNS(XML_w14, XML_val),
                                           OString::number(aCheckedState[0], /*radix=*/16));
        }
        OUString aUncheckedState = m_pContentControl->GetUncheckedState();
        if (!aUncheckedState.isEmpty())
        {
            m_pSerializer->singleElementNS(XML_w14, XML_uncheckedState, FSNS(XML_w14, XML_val),
                                           OString::number(aUncheckedState[0], /*radix=*/16));
        }
        m_pSerializer->endElementNS(XML_w14, XML_checkbox);
    }

    if (m_pContentControl->GetComboBox() || m_pContentControl->GetDropDown())
    {
        if (m_pContentControl->GetComboBox())
        {
            m_pSerializer->startElementNS(XML_w, XML_comboBox);
        }
        else
        {
            m_pSerializer->startElementNS(XML_w, XML_dropDownList);
        }
        for (const auto& rItem : m_pContentControl->GetListItems())
        {
            if (rItem.m_aDisplayText.isEmpty() && rItem.m_aValue.isEmpty())
            {
                // Empty display text & value would be invalid DOCX, skip the item.
                continue;
            }

            rtl::Reference<FastAttributeList> xAttributes = FastSerializerHelper::createAttrList();
            if (!rItem.m_aDisplayText.isEmpty())
            {
                // If there is no display text, need to omit the attribute, not write an empty one.
                xAttributes->add(FSNS(XML_w, XML_displayText), rItem.m_aDisplayText);
            }

            OUString aValue = rItem.m_aValue;
            if (aValue.isEmpty())
            {
                // Empty value would be invalid DOCX, default to the display text.
                aValue = rItem.m_aDisplayText;
            }
            xAttributes->add(FSNS(XML_w, XML_value), rItem.m_aValue);
            m_pSerializer->singleElementNS(XML_w, XML_listItem, xAttributes);
        }
        if (m_pContentControl->GetComboBox())
        {
            m_pSerializer->endElementNS(XML_w, XML_comboBox);
        }
        else
        {
            m_pSerializer->endElementNS(XML_w, XML_dropDownList);
        }
    }

    if (m_pContentControl->GetDate())
    {
        OUString aCurrentDate = m_pContentControl->GetCurrentDate();
        if (aCurrentDate.isEmpty())
        {
            m_pSerializer->startElementNS(XML_w, XML_date);
        }
        else
        {
            m_pSerializer->startElementNS(XML_w, XML_date, FSNS(XML_w, XML_fullDate), aCurrentDate);
        }
        OUString aDateFormat = m_pContentControl->GetDateFormat().replaceAll("\"", "'");
        if (!aDateFormat.isEmpty())
        {
            m_pSerializer->singleElementNS(XML_w, XML_dateFormat, FSNS(XML_w, XML_val),
                                           aDateFormat);
        }
        OUString aDateLanguage = m_pContentControl->GetDateLanguage();
        if (!aDateLanguage.isEmpty())
        {
            m_pSerializer->singleElementNS(XML_w, XML_lid, FSNS(XML_w, XML_val),
                                           aDateLanguage);
        }
        m_pSerializer->endElementNS(XML_w, XML_date);
    }

    if (!m_pContentControl->GetMultiLine().isEmpty())
    {
        m_pSerializer->singleElementNS(XML_w, XML_text, FSNS(XML_w, XML_multiLine), m_pContentControl->GetMultiLine());
    }
    else if (m_pContentControl->GetPlainText())
    {
        m_pSerializer->singleElementNS(XML_w, XML_text);
    }

    m_pSerializer->endElementNS(XML_w, XML_sdtPr);
    m_pSerializer->startElementNS(XML_w, XML_sdtContent);

    const OUString& rPrefixMapping = m_pContentControl->GetDataBindingPrefixMappings();
    const OUString& rXpath = m_pContentControl->GetDataBindingXpath();
    if (!rXpath.isEmpty())
    {
        // This content control has a data binding, update the data source.
        SwTextContentControl* pTextAttr = m_pContentControl->GetTextAttr();
        SwTextNode* pTextNode = m_pContentControl->GetTextNode();
        if (pTextNode && pTextAttr)
        {
            SwPosition aPoint(*pTextNode, pTextAttr->GetStart());
            SwPosition aMark(*pTextNode, *pTextAttr->GetEnd());
            SwPaM aPam(aMark, aPoint);
            OUString aSnippet = aPam.GetText();
            static sal_Unicode const aForbidden[] = {
                CH_TXTATR_BREAKWORD,
                0
            };
            aSnippet = comphelper::string::removeAny(aSnippet, aForbidden);
            m_rExport.AddSdtData(rPrefixMapping, rXpath, aSnippet);
        }
    }

    m_pContentControl = nullptr;
}

void DocxAttributeOutput::WriteContentControlEnd()
{
    m_pSerializer->endElementNS(XML_w, XML_sdtContent);
    m_pSerializer->endElementNS(XML_w, XML_sdt);
}

void DocxAttributeOutput::WriteSdtDropDownStart(
        OUString const& rName,
        OUString const& rSelected,
        uno::Sequence<OUString> const& rListItems)
{
    m_pSerializer->startElementNS(XML_w, XML_sdt);
    m_pSerializer->startElementNS(XML_w, XML_sdtPr);

    m_pSerializer->singleElementNS(XML_w, XML_alias,
        FSNS(XML_w, XML_val), rName);

    sal_Int32 nId = comphelper::findValue(rListItems, rSelected);
    if (nId == -1)
    {
        nId = 0;
    }

    m_pSerializer->startElementNS(XML_w, XML_dropDownList,
            FSNS(XML_w, XML_lastValue), OString::number(nId));

    for (auto const& rItem : rListItems)
    {
        auto const item(OUStringToOString(rItem, RTL_TEXTENCODING_UTF8));
        m_pSerializer->singleElementNS(XML_w, XML_listItem,
                FSNS(XML_w, XML_value), item,
                FSNS(XML_w, XML_displayText), item);
    }

    m_pSerializer->endElementNS(XML_w, XML_dropDownList);
    m_pSerializer->endElementNS(XML_w, XML_sdtPr);

    m_pSerializer->startElementNS(XML_w, XML_sdtContent);
}

void DocxAttributeOutput::WriteSdtDropDownEnd(OUString const& rSelected,
        uno::Sequence<OUString> const& rListItems)
{
    // note: rSelected might be empty?
    sal_Int32 nId = comphelper::findValue(rListItems, rSelected);
    if (nId == -1)
    {
        nId = 0;
    }

    // the lastValue only identifies the entry in the list, also export
    // currently selected item's displayText as run content (if one exists)
    if (rListItems.size())
    {
        m_pSerializer->startElementNS(XML_w, XML_r);
        m_pSerializer->startElementNS(XML_w, XML_t);
        m_pSerializer->writeEscaped(rListItems[nId]);
        m_pSerializer->endElementNS(XML_w, XML_t);
        m_pSerializer->endElementNS(XML_w, XML_r);
    }

    WriteContentControlEnd();
}

void DocxAttributeOutput::StartField_Impl( const SwTextNode* pNode, sal_Int32 nPos, FieldInfos const & rInfos, bool bWriteRun )
{
    if ( rInfos.pField && rInfos.eType == ww::eUNKNOWN )
    {
        // Expand unsupported fields
        RunText(rInfos.pField->ExpandField(/*bCached=*/true, nullptr));
        return;
    }
    else if ( rInfos.eType == ww::eFORMDATE )
    {
        const sw::mark::DateFieldmark& rFieldmark = dynamic_cast<const sw::mark::DateFieldmark&>(*rInfos.pFieldmark);
        FieldMarkParamsHelper params(rFieldmark);

        OUString sFullDate;
        OUString sCurrentDate;
        params.extractParam( ODF_FORMDATE_CURRENTDATE, sCurrentDate );
        if(!sCurrentDate.isEmpty())
        {
            sFullDate = sCurrentDate + "T00:00:00Z";
        }
        else
        {
            std::pair<bool, double> aResult = rFieldmark.GetCurrentDate();
            if(aResult.first)
            {
                sFullDate = rFieldmark.GetDateInStandardDateFormat(aResult.second) + "T00:00:00Z";
            }
        }

        OUString sDateFormat;
        params.extractParam( ODF_FORMDATE_DATEFORMAT, sDateFormat );
        OUString sLang;
        params.extractParam( ODF_FORMDATE_DATEFORMAT_LANGUAGE, sLang );

        uno::Sequence<beans::PropertyValue> aSdtParams;
        params.extractParam(UNO_NAME_MISC_OBJ_INTEROPGRABBAG, aSdtParams);

        WriteFormDateStart( sFullDate, sDateFormat, sLang, aSdtParams);
        return;
    }
    else if (rInfos.eType == ww::eFORMDROPDOWN && rInfos.pField)
    {
        assert(!rInfos.pFieldmark);
        SwDropDownField const& rField2(*static_cast<SwDropDownField const*>(rInfos.pField.get()));
        WriteSdtDropDownStart(rField2.GetName(),
                rField2.GetSelectedItem(),
                rField2.GetItemSequence());
        return;
    }
    else if (rInfos.eType == ww::eFILLIN)
    {
        const SwInputField* pField = static_cast<SwInputField const*>(rInfos.pField.get());
        if (pField && pField->getGrabBagParams().hasElements())
        {
            WriteSdtPlainText(pField->GetPar1(), pField->getGrabBagParams());
            m_sRawText = pField->GetPar1();  // Write field content also as a fallback
            return;
        }
    }

    if ( rInfos.eType != ww::eNONE ) // HYPERLINK fields are just commands
    {
        if ( bWriteRun )
            m_pSerializer->startElementNS(XML_w, XML_r);

        if ( rInfos.eType == ww::eFORMDROPDOWN )
        {
            m_pSerializer->startElementNS( XML_w, XML_fldChar,
                FSNS( XML_w, XML_fldCharType ), "begin" );
            assert( rInfos.pFieldmark && !rInfos.pField );
            WriteFFData(rInfos);
            m_pSerializer->endElementNS( XML_w, XML_fldChar );

            if ( bWriteRun )
                m_pSerializer->endElementNS( XML_w, XML_r );

            CmdField_Impl( pNode, nPos, rInfos, bWriteRun );
        }
        else
        {
            // Write the field start
            if ( rInfos.pField && (rInfos.pField->Which() == SwFieldIds::DateTime)
                && (static_cast<const SwDateTimeField*>(rInfos.pField.get())->GetSubType() & SwDateTimeSubType::Fixed) )
            {
                m_pSerializer->startElementNS( XML_w, XML_fldChar,
                    FSNS( XML_w, XML_fldCharType ), "begin",
                    FSNS( XML_w, XML_fldLock ), "true" );
            }
            else
            {
                m_pSerializer->startElementNS( XML_w, XML_fldChar,
                    FSNS( XML_w, XML_fldCharType ), "begin" );
            }

            if ( rInfos.pFieldmark )
                WriteFFData(  rInfos );

            m_pSerializer->endElementNS( XML_w, XML_fldChar );

            if ( bWriteRun )
                m_pSerializer->endElementNS( XML_w, XML_r );

            // The hyperlinks fields can't be expanded: the value is
            // normally in the text run
            if ( !rInfos.pField )
                CmdField_Impl( pNode, nPos, rInfos, bWriteRun );
            else
                m_bWritingField = true;
        }
    }
}

void DocxAttributeOutput::DoWriteCmd( std::u16string_view rCmd )
{
    std::u16string_view sCmd = o3tl::trim(rCmd);
    if (o3tl::starts_with(sCmd, u"SEQ"))
    {
        OUString sSeqName( o3tl::trim(msfilter::util::findQuotedText(sCmd, u"SEQ ", '\\')) );
        m_aSeqBookmarksNames[sSeqName].push_back(m_sLastOpenedBookmark);
    }
    // Write the Field command
    sal_Int32 nTextToken = XML_instrText;
    if ( m_pRedlineData && m_pRedlineData->GetType() == RedlineType::Delete )
        nTextToken = XML_delInstrText;

    m_pSerializer->startElementNS(XML_w, nTextToken, FSNS(XML_xml, XML_space), "preserve");
    m_pSerializer->writeEscaped( rCmd );
    m_pSerializer->endElementNS( XML_w, nTextToken );

}

void DocxAttributeOutput::CmdField_Impl( const SwTextNode* pNode, sal_Int32 nPos, FieldInfos const & rInfos, bool bWriteRun )
{
    // Write the Field instruction
    if ( bWriteRun )
    {
        bool bWriteCombChars(false);
        m_pSerializer->startElementNS(XML_w, XML_r);

        if (rInfos.eType == ww::eEQ)
            bWriteCombChars = true;

        DoWriteFieldRunProperties( pNode, nPos, bWriteCombChars );
    }

    sal_Int32 nIdx { rInfos.sCmd.isEmpty() ? -1 : 0 };
    while ( nIdx >= 0 )
    {
        OUString sToken = rInfos.sCmd.getToken( 0, '\t', nIdx );
        if ( rInfos.eType ==  ww::eCREATEDATE
          || rInfos.eType ==  ww::eSAVEDATE
          || rInfos.eType ==  ww::ePRINTDATE
          || rInfos.eType ==  ww::eDATE
          || rInfos.eType ==  ww::eTIME )
        {
           sToken = sToken.replaceAll("NNNN", "dddd");
           sToken = sToken.replaceAll("NN", "ddd");
        }
        else if ( rInfos.eType == ww::eEquals )
        {
            // Use original OOXML formula, if it exists and its conversion hasn't been changed
            bool bIsChanged = true;
            if ( pNode->GetTableBox() )
            {
                if ( const SfxGrabBagItem* pItem = pNode->GetTableBox()->GetFrameFormat()->GetAttrSet().GetItem<SfxGrabBagItem>(RES_FRMATR_GRABBAG) )
                {
                    OUString sActualFormula = sToken.trim();
                    const std::map<OUString, uno::Any>& rGrabBag = pItem->GetGrabBag();
                    std::map<OUString, uno::Any>::const_iterator aStoredFormula = rGrabBag.find(u"CellFormulaConverted"_ustr);
                    if ( aStoredFormula != rGrabBag.end() && sActualFormula.indexOf('=') == 0 &&
                                    o3tl::trim(sActualFormula.subView(1)) == o3tl::trim(aStoredFormula->second.get<OUString>()) )
                    {
                        aStoredFormula = rGrabBag.find(u"CellFormula"_ustr);
                        if ( aStoredFormula != rGrabBag.end() )
                        {
                            sToken = " =" + aStoredFormula->second.get<OUString>();
                            bIsChanged = false;
                        }
                    }
                }
            }

            if ( bIsChanged )
            {
                UErrorCode nErr(U_ZERO_ERROR);
                icu::UnicodeString sInput(sToken.getStr());
                // replace < and > around cell references with parentheses
                // e.g. "<A1>" to "(A1)", "<A1:B2>" to "(A1:B2)"
                icu::RegexMatcher aMatcher("<([A-Z]{1,3}[0-9]+(:[A-Z]{1,3}[0-9]+)?)>", sInput, 0, nErr);
                sInput = aMatcher.replaceAll(icu::UnicodeString("($1)"), nErr);

                // In case the parenthesis has been doubled in the previous replaceAll, remove one of them
                icu::RegexMatcher aMatcher2("[(]([(][A-Z]{1,3}[0-9]+(:[A-Z]{1,3}[0-9]+)?[)])[)]", sInput, 0, nErr);
                sInput = aMatcher2.replaceAll(icu::UnicodeString("$1"), nErr);

                // convert MEAN to AVERAGE
                icu::RegexMatcher aMatcher3("\\bMEAN\\b", sInput, UREGEX_CASE_INSENSITIVE, nErr);
                sToken = aMatcher3.replaceAll(icu::UnicodeString("AVERAGE"), nErr).getTerminatedBuffer();
            }
        }

        // Write the Field command
        DoWriteCmd( sToken );

        // Replace tabs by </instrText><tab/><instrText>
        if ( nIdx > 0 ) // Is another token expected?
            RunText( u"\t"_ustr );
    }

    if ( bWriteRun )
    {
        m_pSerializer->endElementNS( XML_w, XML_r );
    }
}

void DocxAttributeOutput::CmdEndField_Impl(SwTextNode const*const pNode,
        sal_Int32 const nPos, bool const bWriteRun)
{
    // Write the Field separator
        if ( bWriteRun )
        {
            m_pSerializer->startElementNS(XML_w, XML_r);
            DoWriteFieldRunProperties( pNode, nPos );
        }

        m_pSerializer->singleElementNS( XML_w, XML_fldChar,
              FSNS( XML_w, XML_fldCharType ), "separate" );

        if ( bWriteRun )
        {
            m_pSerializer->endElementNS( XML_w, XML_r );
        }
}

/// Writes properties for run that is used to separate field implementation.
/// There are several runs are used:
///     <w:r>
///         <w:rPr>
///             <!-- properties written with StartRunProperties() / EndRunProperties().
///         </w:rPr>
///         <w:fldChar w:fldCharType="begin" />
///     </w:r>
///         <w:r>
///         <w:rPr>
///             <!-- properties written with DoWriteFieldRunProperties()
///         </w:rPr>
///         <w:instrText>TIME \@"HH:mm:ss"</w:instrText>
///     </w:r>
///     <w:r>
///         <w:rPr>
///             <!-- properties written with DoWriteFieldRunProperties()
///         </w:rPr>
///         <w:fldChar w:fldCharType="separate" />
///     </w:r>
///     <w:r>
///         <w:rPr>
///             <!-- properties written with DoWriteFieldRunProperties()
///         </w:rPr>
///         <w:t>14:01:13</w:t>
///         </w:r>
///     <w:r>
///         <w:rPr>
///             <!-- properties written with DoWriteFieldRunProperties()
///         </w:rPr>
///         <w:fldChar w:fldCharType="end" />
///     </w:r>
/// See, tdf#38778
void DocxAttributeOutput::DoWriteFieldRunProperties( const SwTextNode * pNode, sal_Int32 nPos, bool bWriteCombChars)
{
    if (! pNode)
    {
        // nothing to do
        return;
    }

    m_bPreventDoubleFieldsHandling = true;

    {
        m_pSerializer->startElementNS(XML_w, XML_rPr);

        // 1. output webHidden flag
        if(GetExport().m_bHideTabLeaderAndPageNumbers && m_pHyperlinkAttrList.is() )
        {
            m_pSerializer->singleElementNS(XML_w, XML_webHidden);
        }

        // 2. find all active character properties
        SwWW8AttrIter aAttrIt( m_rExport, *pNode );
        aAttrIt.OutAttr( nPos, bWriteCombChars );

        // 3. write the character properties
        WriteCollectedRunProperties();

        m_pSerializer->endElementNS( XML_w, XML_rPr );
    }

    m_bPreventDoubleFieldsHandling = false;
}

void DocxAttributeOutput::EndField_Impl( const SwTextNode* pNode, sal_Int32 nPos, FieldInfos& rInfos )
{
    if (rInfos.eType == ww::eFORMDATE)
    {
        WriteContentControlEnd();
        return;
    }
    else if (rInfos.eType == ww::eFORMDROPDOWN && rInfos.pField)
    {
        // write selected item from End not Start to ensure that any bookmarks
        // precede it
        SwDropDownField const& rField(*static_cast<SwDropDownField const*>(rInfos.pField.get()));
        WriteSdtDropDownEnd(rField.GetSelectedItem(), rField.GetItemSequence());
        return;
    }
    else if (rInfos.eType == ww::eFILLIN && rInfos.pField)
    {
        SwInputField const& rField(*static_cast<SwInputField const*>(rInfos.pField.get()));
        if (rField.getGrabBagParams().hasElements())
        {
            WriteContentControlEnd();
            return;
        }
    }
    // The command has to be written before for the hyperlinks
    if ( rInfos.pField )
    {
        CmdField_Impl( pNode, nPos, rInfos, true );
        CmdEndField_Impl( pNode, nPos, true );
    }

    // Write the bookmark start if any
    if ( !m_sFieldBkm.isEmpty() )
    {
        DoWriteBookmarkTagStart(m_sFieldBkm);
    }

    if (rInfos.pField ) // For hyperlinks and TOX
    {
        // Write the Field latest value
        m_pSerializer->startElementNS(XML_w, XML_r);
        DoWriteFieldRunProperties( pNode, nPos );

        OUString sExpand;
        if(rInfos.eType == ww::eCITATION)
        {
            sExpand = static_cast<SwAuthorityField const*>(rInfos.pField.get())
                        ->ExpandCitation(AUTH_FIELD_TITLE, nullptr);
        }
        else if(rInfos.eType != ww::eFORMDROPDOWN)
        {
            sExpand = rInfos.pField->ExpandField(true, nullptr);
        }
        // newlines embedded in fields are 0x0B in MSO and 0x0A for us
        RunText(sExpand.replace(0x0A, 0x0B));

        m_pSerializer->endElementNS( XML_w, XML_r );
    }

    // Write the bookmark end if any
    if ( !m_sFieldBkm.isEmpty() )
    {
        DoWriteBookmarkTagEnd(m_nNextBookmarkId);

        m_nNextBookmarkId++;
    }

    // Write the Field end
    if ( rInfos.bClose  )
    {
        m_bWritingField = false;
        m_pSerializer->startElementNS(XML_w, XML_r);
        DoWriteFieldRunProperties( pNode, nPos );
        m_pSerializer->singleElementNS(XML_w, XML_fldChar, FSNS(XML_w, XML_fldCharType), "end");
        m_pSerializer->endElementNS( XML_w, XML_r );
    }
    // Write the ref field if a bookmark had to be set and the field
    // should be visible
    if ( !rInfos.pField )
    {
        m_sFieldBkm.clear();
        return;
    }

    bool bIsSetField = rInfos.pField->GetTyp( )->Which( ) == SwFieldIds::SetExp;
    bool bShowRef = bIsSetField && !( static_cast<const SwSetExpField*>(rInfos.pField.get())->GetSubType( ) & SwGetSetExpType::Invisible );

    if (!bShowRef)
    {
        m_sFieldBkm.clear();
    }

    if (m_sFieldBkm.isEmpty())
        return;

    // Write the field beginning
    m_pSerializer->startElementNS(XML_w, XML_r);
    m_pSerializer->singleElementNS( XML_w, XML_fldChar,
        FSNS( XML_w, XML_fldCharType ), "begin" );
    m_pSerializer->endElementNS( XML_w, XML_r );

    rInfos.sCmd = FieldString( ww::eREF );
    rInfos.sCmd += "\"";
    rInfos.sCmd += m_sFieldBkm;
    rInfos.sCmd += "\" ";

    // Clean the field bookmark data to avoid infinite loop
    m_sFieldBkm = OUString( );

    // Write the end of the field
    EndField_Impl( pNode, nPos, rInfos );
}

void DocxAttributeOutput::StartRunProperties()
{
    // postpone the output so that we can later [in EndRunProperties()]
    // prepend the properties before the text
    m_pSerializer->mark(Tag_StartRunProperties);

    m_pSerializer->startElementNS(XML_w, XML_rPr);

    if(GetExport().m_bHideTabLeaderAndPageNumbers && m_pHyperlinkAttrList.is() )
    {
        m_pSerializer->singleElementNS(XML_w, XML_webHidden);
    }
    InitCollectedRunProperties();

    assert( !m_oPostponedGraphic );
    m_oPostponedGraphic.emplace();

    assert( !m_oPostponedDiagrams );
    m_oPostponedDiagrams.emplace();

    assert(!m_oPostponedDMLDrawings);
    m_oPostponedDMLDrawings.emplace();

    assert( !m_oPostponedOLEs );
    m_oPostponedOLEs.emplace();
}

void DocxAttributeOutput::InitCollectedRunProperties()
{
    m_pFontsAttrList = nullptr;
    m_pEastAsianLayoutAttrList = nullptr;
    m_pCharLangAttrList = nullptr;

    // Write the elements in the spec order
    static const sal_Int32 aOrder[] =
    {
        FSNS( XML_w, XML_rStyle ),
        FSNS( XML_w, XML_rFonts ),
        FSNS( XML_w, XML_b ),
        FSNS( XML_w, XML_bCs ),
        FSNS( XML_w, XML_i ),
        FSNS( XML_w, XML_iCs ),
        FSNS( XML_w, XML_caps ),
        FSNS( XML_w, XML_smallCaps ),
        FSNS( XML_w, XML_strike ),
        FSNS( XML_w, XML_dstrike ),
        FSNS( XML_w, XML_outline ),
        FSNS( XML_w, XML_shadow ),
        FSNS( XML_w, XML_emboss ),
        FSNS( XML_w, XML_imprint ),
        FSNS( XML_w, XML_noProof ),
        FSNS( XML_w, XML_snapToGrid ),
        FSNS( XML_w, XML_vanish ),
        FSNS( XML_w, XML_webHidden ),
        FSNS( XML_w, XML_color ),
        FSNS( XML_w, XML_spacing ),
        FSNS( XML_w, XML_w ),
        FSNS( XML_w, XML_kern ),
        FSNS( XML_w, XML_position ),
        FSNS( XML_w, XML_sz ),
        FSNS( XML_w, XML_szCs ),
        FSNS( XML_w, XML_highlight ),
        FSNS( XML_w, XML_u ),
        FSNS( XML_w, XML_effect ),
        FSNS( XML_w, XML_bdr ),
        FSNS( XML_w, XML_shd ),
        FSNS( XML_w, XML_fitText ),
        FSNS( XML_w, XML_vertAlign ),
        FSNS( XML_w, XML_rtl ),
        FSNS( XML_w, XML_cs ),
        FSNS( XML_w, XML_em ),
        FSNS( XML_w, XML_lang ),
        FSNS( XML_w, XML_eastAsianLayout ),
        FSNS( XML_w, XML_specVanish ),
        FSNS( XML_w, XML_oMath ),
        FSNS( XML_w, XML_rPrChange ),
        FSNS( XML_w, XML_del ),
        FSNS( XML_w, XML_ins ),
        FSNS( XML_w, XML_moveFrom ),
        FSNS( XML_w, XML_moveTo ),
        FSNS( XML_w14, XML_glow ),
        FSNS( XML_w14, XML_shadow ),
        FSNS( XML_w14, XML_reflection ),
        FSNS( XML_w14, XML_textOutline ),
        FSNS( XML_w14, XML_textFill ),
        FSNS( XML_w14, XML_scene3d ),
        FSNS( XML_w14, XML_props3d ),
        FSNS( XML_w14, XML_ligatures ),
        FSNS( XML_w14, XML_numForm ),
        FSNS( XML_w14, XML_numSpacing ),
        FSNS( XML_w14, XML_stylisticSets ),
        FSNS( XML_w14, XML_cntxtAlts ),
    };

    // postpone the output so that we can later [in EndParagraphProperties()]
    // prepend the properties before the run
    // coverity[overrun-buffer-arg : FALSE] - coverity has difficulty with css::uno::Sequence
    m_pSerializer->mark(Tag_InitCollectedRunProperties, comphelper::containerToSequence(aOrder));
}

namespace
{

struct NameToId
{
    OUString  maName;
    sal_Int32 maId;
};

constexpr NameToId constNameToIdMapping[] =
{
    { u"glow"_ustr,         FSNS( XML_w14, XML_glow ) },
    { u"shadow"_ustr,       FSNS( XML_w14, XML_shadow ) },
    { u"reflection"_ustr,   FSNS( XML_w14, XML_reflection ) },
    { u"textOutline"_ustr,  FSNS( XML_w14, XML_textOutline ) },
    { u"textFill"_ustr,     FSNS( XML_w14, XML_textFill ) },
    { u"scene3d"_ustr,      FSNS( XML_w14, XML_scene3d ) },
    { u"props3d"_ustr,      FSNS( XML_w14, XML_props3d ) },
    { u"ligatures"_ustr,    FSNS( XML_w14, XML_ligatures ) },
    { u"numForm"_ustr,      FSNS( XML_w14, XML_numForm ) },
    { u"numSpacing"_ustr,   FSNS( XML_w14, XML_numSpacing ) },
    { u"stylisticSets"_ustr,FSNS( XML_w14, XML_stylisticSets ) },
    { u"cntxtAlts"_ustr,    FSNS( XML_w14, XML_cntxtAlts ) },

    { u"val"_ustr,          FSNS( XML_w14, XML_val ) },
    { u"rad"_ustr,          FSNS( XML_w14, XML_rad ) },
    { u"blurRad"_ustr,      FSNS( XML_w14, XML_blurRad ) },
    { u"stA"_ustr,          FSNS( XML_w14, XML_stA ) },
    { u"stPos"_ustr,        FSNS( XML_w14, XML_stPos ) },
    { u"endA"_ustr,         FSNS( XML_w14, XML_endA ) },
    { u"endPos"_ustr,       FSNS( XML_w14, XML_endPos ) },
    { u"dist"_ustr,         FSNS( XML_w14, XML_dist ) },
    { u"dir"_ustr,          FSNS( XML_w14, XML_dir ) },
    { u"fadeDir"_ustr,      FSNS( XML_w14, XML_fadeDir ) },
    { u"sx"_ustr,           FSNS( XML_w14, XML_sx ) },
    { u"sy"_ustr,           FSNS( XML_w14, XML_sy ) },
    { u"kx"_ustr,           FSNS( XML_w14, XML_kx ) },
    { u"ky"_ustr,           FSNS( XML_w14, XML_ky ) },
    { u"algn"_ustr,         FSNS( XML_w14, XML_algn ) },
    { u"w"_ustr,            FSNS( XML_w14, XML_w ) },
    { u"cap"_ustr,          FSNS( XML_w14, XML_cap ) },
    { u"cmpd"_ustr,         FSNS( XML_w14, XML_cmpd ) },
    { u"pos"_ustr,          FSNS( XML_w14, XML_pos ) },
    { u"ang"_ustr,          FSNS( XML_w14, XML_ang ) },
    { u"scaled"_ustr,       FSNS( XML_w14, XML_scaled ) },
    { u"path"_ustr,         FSNS( XML_w14, XML_path ) },
    { u"l"_ustr,            FSNS( XML_w14, XML_l ) },
    { u"t"_ustr,            FSNS( XML_w14, XML_t ) },
    { u"r"_ustr,            FSNS( XML_w14, XML_r ) },
    { u"b"_ustr,            FSNS( XML_w14, XML_b ) },
    { u"lim"_ustr,          FSNS( XML_w14, XML_lim ) },
    { u"prst"_ustr,         FSNS( XML_w14, XML_prst ) },
    { u"rig"_ustr,          FSNS( XML_w14, XML_rig ) },
    { u"lat"_ustr,          FSNS( XML_w14, XML_lat ) },
    { u"lon"_ustr,          FSNS( XML_w14, XML_lon ) },
    { u"rev"_ustr,          FSNS( XML_w14, XML_rev ) },
    { u"h"_ustr,            FSNS( XML_w14, XML_h ) },
    { u"extrusionH"_ustr,   FSNS( XML_w14, XML_extrusionH ) },
    { u"contourW"_ustr,     FSNS( XML_w14, XML_contourW ) },
    { u"prstMaterial"_ustr, FSNS( XML_w14, XML_prstMaterial ) },
    { u"id"_ustr,           FSNS( XML_w14, XML_id ) },

    { u"schemeClr"_ustr,    FSNS( XML_w14, XML_schemeClr ) },
    { u"srgbClr"_ustr,      FSNS( XML_w14, XML_srgbClr ) },
    { u"tint"_ustr,         FSNS( XML_w14, XML_tint ) },
    { u"shade"_ustr,        FSNS( XML_w14, XML_shade ) },
    { u"alpha"_ustr,        FSNS( XML_w14, XML_alpha ) },
    { u"hueMod"_ustr,       FSNS( XML_w14, XML_hueMod ) },
    { u"sat"_ustr,          FSNS( XML_w14, XML_sat ) },
    { u"satOff"_ustr,       FSNS( XML_w14, XML_satOff ) },
    { u"satMod"_ustr,       FSNS( XML_w14, XML_satMod ) },
    { u"lum"_ustr,          FSNS( XML_w14, XML_lum ) },
    { u"lumOff"_ustr,       FSNS( XML_w14, XML_lumOff ) },
    { u"lumMod"_ustr,       FSNS( XML_w14, XML_lumMod ) },
    { u"noFill"_ustr,       FSNS( XML_w14, XML_noFill ) },
    { u"solidFill"_ustr,    FSNS( XML_w14, XML_solidFill ) },
    { u"gradFill"_ustr,     FSNS( XML_w14, XML_gradFill ) },
    { u"gsLst"_ustr,        FSNS( XML_w14, XML_gsLst ) },
    { u"gs"_ustr,           FSNS( XML_w14, XML_gs ) },
    { u"pos"_ustr,          FSNS( XML_w14, XML_pos ) },
    { u"lin"_ustr,          FSNS( XML_w14, XML_lin ) },
    { u"path"_ustr,         FSNS( XML_w14, XML_path ) },
    { u"fillToRect"_ustr,   FSNS( XML_w14, XML_fillToRect ) },
    { u"prstDash"_ustr,     FSNS( XML_w14, XML_prstDash ) },
    { u"round"_ustr,        FSNS( XML_w14, XML_round ) },
    { u"bevel"_ustr,        FSNS( XML_w14, XML_bevel ) },
    { u"miter"_ustr,        FSNS( XML_w14, XML_miter ) },
    { u"camera"_ustr,       FSNS( XML_w14, XML_camera ) },
    { u"lightRig"_ustr,     FSNS( XML_w14, XML_lightRig ) },
    { u"rot"_ustr,          FSNS( XML_w14, XML_rot ) },
    { u"bevelT"_ustr,       FSNS( XML_w14, XML_bevelT ) },
    { u"bevelB"_ustr,       FSNS( XML_w14, XML_bevelB ) },
    { u"extrusionClr"_ustr, FSNS( XML_w14, XML_extrusionClr ) },
    { u"contourClr"_ustr,   FSNS( XML_w14, XML_contourClr ) },
    { u"styleSet"_ustr,     FSNS( XML_w14, XML_styleSet ) },
};

std::optional<sal_Int32> lclGetElementIdForName(std::u16string_view rName)
{
    for (auto const & i : constNameToIdMapping)
    {
        if (rName == i.maName)
        {
            return i.maId;
        }
    }
    return std::optional<sal_Int32>();
}

void lclProcessRecursiveGrabBag(sal_Int32 aElementId, const css::uno::Sequence<css::beans::PropertyValue>& rElements, sax_fastparser::FSHelperPtr const & pSerializer)
{
    css::uno::Sequence<css::beans::PropertyValue> aAttributes;
    rtl::Reference<FastAttributeList> pAttributes = FastSerializerHelper::createAttrList();
    sal_Int32 nElements = 0;

    for (const auto& rElement : rElements)
    {
        if (rElement.Name == "attributes")
        {
            rElement.Value >>= aAttributes;
        }
        else
        {
            ++nElements;
        }
    }

    for (const auto& rAttribute : aAttributes)
    {
        uno::Any aAny = rAttribute.Value;
        OString aValue;

        if(aAny.getValueType() == cppu::UnoType<sal_Int32>::get())
        {
            aValue = OString::number(aAny.get<sal_Int32>());
        }
        else if(aAny.getValueType() == cppu::UnoType<OUString>::get())
        {
            aValue =  OUStringToOString(aAny.get<OUString>(), RTL_TEXTENCODING_ASCII_US);
        }

        std::optional<sal_Int32> aSubElementId = lclGetElementIdForName(rAttribute.Name);
        if(aSubElementId)
            pAttributes->add(*aSubElementId, aValue);
    }

    if (nElements == 0)
    {
        pSerializer->singleElement(aElementId, pAttributes);
    }
    else
    {
        pSerializer->startElement(aElementId, pAttributes);

        for (const auto& rElement : rElements)
        {
            css::uno::Sequence<css::beans::PropertyValue> aSumElements;

            std::optional<sal_Int32> aSubElementId = lclGetElementIdForName(rElement.Name);
            if(aSubElementId)
            {
                rElement.Value >>= aSumElements;
                lclProcessRecursiveGrabBag(*aSubElementId, aSumElements, pSerializer);
            }
        }

        pSerializer->endElement(aElementId);
    }
}

constexpr auto constTransformationToTokenId = frozen::make_unordered_map<model::TransformationType, sal_Int32>({
    { model::TransformationType::Tint, XML_tint },
    { model::TransformationType::Shade, XML_shade },
    { model::TransformationType::Sat, XML_sat },
    { model::TransformationType::SatOff, XML_satOff },
    { model::TransformationType::SatMod, XML_satMod },
    { model::TransformationType::Lum, XML_lum },
    { model::TransformationType::LumOff, XML_lumOff },
    { model::TransformationType::LumMod, XML_lumMod },
});

} // end anonymous namespace

void DocxAttributeOutput::WriteCollectedRunProperties()
{
    // Write all differed properties
    if ( m_pFontsAttrList.is() )
    {
        m_pSerializer->singleElementNS( XML_w, XML_rFonts, detachFrom( m_pFontsAttrList ) );
    }

    if ( m_pColorAttrList.is() )
    {
        m_pSerializer->singleElementNS( XML_w, XML_color, m_pColorAttrList );
    }

    if ( m_pEastAsianLayoutAttrList.is() )
    {
        m_pSerializer->singleElementNS( XML_w, XML_eastAsianLayout,
                                        detachFrom(m_pEastAsianLayoutAttrList ) );
    }

    if ( m_pCharLangAttrList.is() )
    {
        m_pSerializer->singleElementNS( XML_w, XML_lang, detachFrom( m_pCharLangAttrList ) );
    }

    if ((m_nCharTransparence != 0 || lclHasSolidFillTransformations(m_aComplexColor))
            && m_pColorAttrList.is()
            && m_aTextFillGrabBag.empty())
    {
        std::string_view pVal;
        m_pColorAttrList->getAsView(FSNS(XML_w, XML_val), pVal);

        if (!pVal.empty() && pVal != "auto")
        {
            m_pSerializer->startElementNS(XML_w14, XML_textFill);
            m_pSerializer->startElementNS(XML_w14, XML_solidFill);

            if (m_aComplexColor.isValidThemeType())
            {
                OString sSchemeType = lclGetSchemeType(m_aComplexColor);
                m_pSerializer->startElementNS(XML_w14, XML_schemeClr, FSNS(XML_w14, XML_val), sSchemeType);
            }
            else
            {
                m_pSerializer->startElementNS(XML_w14, XML_srgbClr, FSNS(XML_w14, XML_val), pVal.data());
            }

            if (m_nCharTransparence != 0)
            {
                sal_Int32 nTransparence = basegfx::fround(m_nCharTransparence / 255.0 * 100.0) * oox::drawingml::PER_PERCENT;
                m_pSerializer->singleElementNS(XML_w14, XML_alpha, FSNS(XML_w14, XML_val), OString::number(nTransparence));
            }

            for (const model::Transformation & transformation : m_aComplexColor.getTransformations())
            {
                sal_Int32 nValue = transformation.mnValue * drawingml::PER_PERCENT;
                const auto iter = constTransformationToTokenId.find(transformation.meType);
                if (iter != constTransformationToTokenId.end())
                {
                    sal_Int32 nElement = iter->second;
                    m_pSerializer->singleElementNS(XML_w14, nElement, FSNS(XML_w14, XML_val), OString::number(nValue));
                }
            }

            if (m_aComplexColor.isValidThemeType())
            {
                m_pSerializer->endElementNS(XML_w14, XML_schemeClr);
            }
            else
            {
                m_pSerializer->endElementNS(XML_w14, XML_srgbClr);
            }
            m_pSerializer->endElementNS(XML_w14, XML_solidFill);
            m_pSerializer->endElementNS(XML_w14, XML_textFill);
            m_nCharTransparence = 0;
        }
    }
    m_pColorAttrList.clear();

    auto processGrabBag = [this](const beans::PropertyValue& prop)
    {
        std::optional<sal_Int32> aElementId = lclGetElementIdForName(prop.Name);
        if(aElementId)
        {
            uno::Sequence<beans::PropertyValue> aGrabBagSeq;
            prop.Value >>= aGrabBagSeq;
            lclProcessRecursiveGrabBag(*aElementId, aGrabBagSeq, m_pSerializer);
        }
    };

    for (const beans::PropertyValue & i : m_aTextEffectsGrabBag)
    {
        processGrabBag(i);
    }
    for (const beans::PropertyValue & i : m_aTextFillGrabBag)
    {
        processGrabBag(i);
    }
    m_aTextEffectsGrabBag.clear();
    m_aTextFillGrabBag.clear();
    // export vanish and specVanish for the newly created inline headings
    if ( m_bOpenedParaPr && m_rExport.m_bParaInlineHeading )
    {
        m_pSerializer->singleElementNS(XML_w, XML_vanish);
        m_pSerializer->singleElementNS(XML_w, XML_specVanish);
        m_rExport.m_bParaInlineHeading = false;
    }
}

void DocxAttributeOutput::EndRunProperties( const SwRedlineData* pRedlineData )
{
    // Call the 'Redline' function. This will add redline (change-tracking) information that regards to run properties.
    // This includes changes like 'Bold', 'Underline', 'Strikethrough' etc.

    // If there is RedlineData present, call WriteCollectedRunProperties() for writing rPr before calling Redline().
    // As there will be another rPr for redline and LO might mix both.
    if(pRedlineData)
        WriteCollectedRunProperties();
    Redline( pRedlineData );

    WriteCollectedRunProperties();

    // Merge the marks for the ordered elements
    m_pSerializer->mergeTopMarks(Tag_InitCollectedRunProperties);

    m_pSerializer->endElementNS( XML_w, XML_rPr );

    // write footnotes/endnotes if we have any
    FootnoteEndnoteReference();

    WriteLineBreak();

    // merge the properties _before_ the run text (strictly speaking, just
    // after the start of the run)
    m_pSerializer->mergeTopMarks(Tag_StartRunProperties, sax_fastparser::MergeMarks::PREPEND);

    WritePostponedGraphic();

    WritePostponedDiagram();
    //We need to write w:drawing tag after the w:rPr.
    WritePostponedChart();

    //We need to write w:pict tag after the w:rPr.
    WritePostponedDMLDrawing();

    WritePostponedOLE();

    WritePostponedActiveXControl(true);
}

void DocxAttributeOutput::GetSdtEndBefore(const SdrObject* pSdrObj)
{
    if (!pSdrObj)
        return;

    uno::Reference<drawing::XShape> xShape(const_cast<SdrObject*>(pSdrObj)->getUnoShape());
    uno::Reference< beans::XPropertySet > xPropSet( xShape, uno::UNO_QUERY );
    if( !xPropSet.is() )
        return;

    uno::Reference< beans::XPropertySetInfo > xPropSetInfo = xPropSet->getPropertySetInfo();
    uno::Sequence< beans::PropertyValue > aGrabBag;
    if (xPropSetInfo.is() && xPropSetInfo->hasPropertyByName(u"FrameInteropGrabBag"_ustr))
    {
        xPropSet->getPropertyValue(u"FrameInteropGrabBag"_ustr) >>= aGrabBag;
    }
    else if(xPropSetInfo.is() && xPropSetInfo->hasPropertyByName(u"InteropGrabBag"_ustr))
    {
        xPropSet->getPropertyValue(u"InteropGrabBag"_ustr) >>= aGrabBag;
    }

    auto pProp = std::find_if(std::cbegin(aGrabBag), std::cend(aGrabBag),
        [this](const beans::PropertyValue& rProp) {
            return "SdtEndBefore" == rProp.Name && m_aRunSdt.m_bStartedSdt && !m_bEndCharSdt; });
    if (pProp != std::cend(aGrabBag))
        pProp->Value >>= m_bEndCharSdt;
}

void DocxAttributeOutput::WritePostponedGraphic()
{
    for (const auto & rPostponedDiagram : *m_oPostponedGraphic)
        FlyFrameGraphic(rPostponedDiagram.grfNode, rPostponedDiagram.size,
            nullptr, nullptr,
            rPostponedDiagram.pSdrObj);
    m_oPostponedGraphic.reset();
}

void DocxAttributeOutput::WritePostponedDiagram()
{
    for(const auto & rPostponedDiagram : *m_oPostponedDiagrams)
        m_rExport.SdrExporter().writeDiagram(rPostponedDiagram.object,
            *rPostponedDiagram.frame);
    m_oPostponedDiagrams.reset();
}

bool DocxAttributeOutput::FootnoteEndnoteRefTag()
{
    if( m_footnoteEndnoteRefTag == 0 )
        return false;

    // output the character style for MS Word's benefit
    const SwEndNoteInfo& rInfo = m_footnoteEndnoteRefTag == XML_footnoteRef ?
        m_rExport.m_rDoc.GetFootnoteInfo() : m_rExport.m_rDoc.GetEndNoteInfo();
    const SwCharFormat* pCharFormat = rInfo.GetCharFormat( m_rExport.m_rDoc );
    if ( pCharFormat )
    {
        const OString aStyleId(m_rExport.m_pStyles->GetStyleId(m_rExport.GetId(pCharFormat)));
        m_pSerializer->startElementNS(XML_w, XML_rPr);
        m_pSerializer->singleElementNS(XML_w, XML_rStyle, FSNS(XML_w, XML_val), aStyleId);
        m_pSerializer->endElementNS( XML_w, XML_rPr );
    }

    if (m_footnoteCustomLabel.isEmpty())
        m_pSerializer->singleElementNS(XML_w, m_footnoteEndnoteRefTag);
    else
        RunText(m_footnoteCustomLabel);
    m_footnoteEndnoteRefTag = 0;
    return true;
}

/** Output sal_Unicode* as a run text (<t>the text</t>).

    When bMove is true, update rBegin to point _after_ the end of the text +
    1, meaning that it skips one character after the text.  This is to make
    the switch in DocxAttributeOutput::RunText() nicer ;-)
 */
static bool impl_WriteRunText( FSHelperPtr const & pSerializer, sal_Int32 nTextToken,
        const sal_Unicode* &rBegin, const sal_Unicode* pEnd, bool bMove = true,
        const OUString& rSymbolFont = OUString() )
{
    const sal_Unicode *pBegin = rBegin;

    // skip one character after the end
    if ( bMove )
        rBegin = pEnd + 1;

    if ( pBegin >= pEnd )
        return false; // we want to write at least one character

    bool bIsSymbol = !rSymbolFont.isEmpty();

    std::u16string_view aView( pBegin, pEnd - pBegin );
    if (bIsSymbol)
    {
        for (char16_t aChar : aView)
        {
            pSerializer->singleElementNS(XML_w, XML_sym,
                FSNS(XML_w, XML_font), rSymbolFont,
                FSNS(XML_w, XML_char), OString::number(aChar, 16));
        }
    }
    else
    {
        // we have to add 'preserve' when starting/ending with space
        if ( *pBegin == ' ' || *( pEnd - 1 ) == ' ' )
            pSerializer->startElementNS(XML_w, nTextToken, FSNS(XML_xml, XML_space), "preserve");
        else
            pSerializer->startElementNS(XML_w, nTextToken);

        pSerializer->writeEscaped( aView );
        pSerializer->endElementNS( XML_w, nTextToken );
    }

    return true;
}

namespace
{
/// Decides if pRedlineData is a delete or is something on a delete.
RedlineType GetRedlineTypeForTextToken(const SwRedlineData* pRedlineData)
{
    if (!pRedlineData)
    {
        return RedlineType::None;
    }

    if (pRedlineData->GetType() == RedlineType::Delete)
    {
        return RedlineType::Delete;
    }

    const SwRedlineData* pNext = pRedlineData->Next();
    if (!pNext)
    {
        return RedlineType::None;
    }

    if (pNext->GetType() == RedlineType::Delete)
    {
        return RedlineType::Delete;
    }

    return RedlineType::None;
}
}

void DocxAttributeOutput::RunText( const OUString& rText, rtl_TextEncoding /*eCharSet*/, const OUString& rSymbolFont )
{
    if( m_closeHyperlinkInThisRun )
    {
        m_closeHyperlinkInPreviousRun = true;
    }
    m_bRunTextIsOn = true;
    // one text can be split into more <w:t>blah</w:t>'s by line breaks etc.
    const sal_Unicode *pBegin = rText.getStr();
    const sal_Unicode *pEnd = pBegin + rText.getLength();

    // the text run is usually XML_t, with the exception of the deleted (and not moved) text
    sal_Int32 nTextToken = XML_t;

    bool isInMoveBookmark = false;
    for (const auto& openedBookmark : m_rOpenedBookmarksIds)
    {
        if (openedBookmark.first.startsWith(u"__RefMove"))
        {
            isInMoveBookmark = true;
            break;
        }
    }
    // Check also the bookmarks that will be opened just now
    for (const OUString& bookmarkName : m_rBookmarksStart)
    {
        if (bookmarkName.startsWith(u"__RefMove"))
        {
            isInMoveBookmark = true;
            break;
        }
    }
    bool bMoved = isInMoveBookmark && m_pRedlineData && m_pRedlineData->IsMoved() &&
                  // tdf#150166 save tracked moving around TOC as w:ins, w:del
                  SwDoc::GetCurTOX(*m_rExport.m_pCurPam->GetPoint()) == nullptr;

    if (GetRedlineTypeForTextToken(m_pRedlineData) == RedlineType::Delete && !bMoved)
    {
        nTextToken = XML_delText;
    }

    sal_Unicode prevUnicode = *pBegin;

    for ( const sal_Unicode *pIt = pBegin; pIt < pEnd; ++pIt )
    {
        switch ( *pIt )
        {
            case 0x09: // tab
                impl_WriteRunText( m_pSerializer, nTextToken, pBegin, pIt );
                m_pSerializer->singleElementNS(XML_w, XML_tab);
                prevUnicode = *pIt;
                break;
            case 0x0b: // line break
            case static_cast<sal_Unicode>(text::ControlCharacter::LINE_BREAK):
                {
                    if (impl_WriteRunText( m_pSerializer, nTextToken, pBegin, pIt ) || prevUnicode < 0x0020)
                    {
                        m_pSerializer->singleElementNS(XML_w, XML_br);
                        prevUnicode = *pIt;
                    }
                }
                break;
            case 0x1E: //non-breaking hyphen
                impl_WriteRunText( m_pSerializer, nTextToken, pBegin, pIt );
                m_pSerializer->singleElementNS(XML_w, XML_noBreakHyphen);
                prevUnicode = *pIt;
                break;
            case 0x1F: //soft (on demand) hyphen
                impl_WriteRunText( m_pSerializer, nTextToken, pBegin, pIt );
                m_pSerializer->singleElementNS(XML_w, XML_softHyphen);
                prevUnicode = *pIt;
                break;
            default:
                if ( *pIt < 0x0020 ) // filter out the control codes
                {
                    impl_WriteRunText( m_pSerializer, nTextToken, pBegin, pIt );
                    SAL_INFO("sw.ww8", "Ignored control code in a text run: " << unsigned(*pIt) );
                }
                prevUnicode = *pIt;
                break;
        }
    }

    impl_WriteRunText( m_pSerializer, nTextToken, pBegin, pEnd, false, rSymbolFont );
}

void DocxAttributeOutput::RawText(const OUString& rText, rtl_TextEncoding /*eCharSet*/)
{
    m_sRawText = rText;
}

void DocxAttributeOutput::StartRuby( const SwTextNode& rNode, sal_Int32 nPos, const SwFormatRuby& rRuby )
{
    WW8Ruby aWW8Ruby( rNode, rRuby, GetExport() );
    SAL_INFO("sw.ww8", "TODO DocxAttributeOutput::StartRuby( const SwTextNode& rNode, const SwFormatRuby& rRuby )" );
    EndRun( &rNode, nPos, -1 ); // end run before starting ruby to avoid nested runs, and overlap
    assert(!m_closeHyperlinkInThisRun); // check that no hyperlink overlaps ruby
    assert(!m_closeHyperlinkInPreviousRun);
    m_pSerializer->startElementNS(XML_w, XML_r);
    m_pSerializer->startElementNS(XML_w, XML_ruby);
    m_pSerializer->startElementNS(XML_w, XML_rubyPr);

    m_pSerializer->singleElementNS( XML_w, XML_rubyAlign,
            FSNS( XML_w, XML_val ), lclConvertWW8JCToOOXMLRubyAlign(aWW8Ruby.GetJC()) );
    sal_uInt32   nHps = (aWW8Ruby.GetRubyHeight() + 5) / 10;
    sal_uInt32   nHpsBaseText = (aWW8Ruby.GetBaseHeight() + 5) / 10;
    m_pSerializer->singleElementNS(XML_w, XML_hps, FSNS(XML_w, XML_val), OString::number(nHps));

    m_pSerializer->singleElementNS( XML_w, XML_hpsRaise,
            FSNS( XML_w, XML_val ), OString::number(nHpsBaseText) );

    m_pSerializer->singleElementNS( XML_w, XML_hpsBaseText,
            FSNS( XML_w, XML_val ), OString::number(nHpsBaseText) );

    lang::Locale aLocale( SwBreakIt::Get()->GetLocale(
                rNode.GetLang( nPos ) ) );
    OUString sLang( LanguageTag::convertToBcp47( aLocale) );
    m_pSerializer->singleElementNS(XML_w, XML_lid, FSNS(XML_w, XML_val), sLang);

    m_pSerializer->endElementNS( XML_w, XML_rubyPr );

    m_pSerializer->startElementNS(XML_w, XML_rt);
    StartRun( nullptr, nPos );
    StartRunProperties( );

    if (rRuby.GetTextRuby() && rRuby.GetTextRuby()->GetCharFormat())
    {
        const SwCharFormat* pFormat = rRuby.GetTextRuby()->GetCharFormat();
        sal_uInt16 nScript = g_pBreakIt->GetBreakIter()->getScriptType(rRuby.GetText(), 0);
        TypedWhichId<SvxFontItem> nWhichFont = (nScript == i18n::ScriptType::LATIN) ?  RES_CHRATR_FONT : RES_CHRATR_CJK_FONT;
        TypedWhichId<SvxFontHeightItem> nWhichFontSize = (nScript == i18n::ScriptType::LATIN) ?  RES_CHRATR_FONTSIZE : RES_CHRATR_CJK_FONTSIZE;

        CharFont(pFormat->GetFormatAttr(nWhichFont));
        CharFontSize(pFormat->GetFormatAttr(nWhichFontSize));
        CharFontSize(pFormat->GetFormatAttr(RES_CHRATR_CTL_FONTSIZE));
    }

    EndRunProperties( nullptr );
    RunText( rRuby.GetText( ) );
    EndRun( &rNode, nPos, -1 );
    m_pSerializer->endElementNS( XML_w, XML_rt );

    m_pSerializer->startElementNS(XML_w, XML_rubyBase);
    StartRun( nullptr, nPos );
}

void DocxAttributeOutput::EndRuby(const SwTextNode& rNode, sal_Int32 nPos, bool /*bEmptyBaseText*/)
{
    SAL_INFO("sw.ww8", "TODO DocxAttributeOutput::EndRuby()" );
    EndRun( &rNode, nPos, -1 );
    m_pSerializer->endElementNS( XML_w, XML_rubyBase );
    m_pSerializer->endElementNS( XML_w, XML_ruby );
    m_pSerializer->endElementNS( XML_w, XML_r );
    StartRun(nullptr, nPos); // open Run again so OutputTextNode loop can close it
}

bool DocxAttributeOutput::AnalyzeURL( const OUString& rUrl, const OUString& rTarget, OUString* pLinkURL, OUString* pMark )
{
    bool bBookMarkOnly = AttributeOutputBase::AnalyzeURL( rUrl, rTarget, pLinkURL, pMark );
    if (bBookMarkOnly)
        *pMark = GetExport().BookmarkToWord(*pMark);

    if (!pMark->isEmpty() && (bBookMarkOnly || rTarget.isEmpty()))
    {
        OUString sURL = *pLinkURL;

        if ( bBookMarkOnly )
            sURL = FieldString( ww::eHYPERLINK );
        else
            sURL = FieldString( ww::eHYPERLINK ) + "\"" + sURL + "\"";

        sURL += " \\l \"" + *pMark + "\"";

        if ( !rTarget.isEmpty() )
            sURL += " \\n " + rTarget;

        *pLinkURL = sURL;
    }

    return bBookMarkOnly;
}

void DocxAttributeOutput::WriteBookmarkInActParagraph( const OUString& rName, sal_Int32 nFirstRunPos, sal_Int32 nLastRunPos )
{
    m_aBookmarksOfParagraphStart.insert(std::pair<sal_Int32, OUString>(nFirstRunPos, rName));
    m_aBookmarksOfParagraphEnd.insert(std::pair<sal_Int32, OUString>(nLastRunPos, rName));
}

bool DocxAttributeOutput::StartURL(const OUString& rUrl, const OUString& rTarget, const OUString& rName)
{
    OUString sMark;
    OUString sUrl;

    bool bBookmarkOnly = AnalyzeURL( rUrl, rTarget, &sUrl, &sMark );

    m_hyperLinkAnchor = sMark;

    if (!sMark.isEmpty() && !bBookmarkOnly && rTarget.isEmpty())
    {
        m_rExport.OutputField( nullptr, ww::eHYPERLINK, sUrl );
    }
    else
    {
        // Output a hyperlink XML element
        m_pHyperlinkAttrList = FastSerializerHelper::createAttrList();

        if ( !bBookmarkOnly )
        {
            OUString sId = GetExport().GetFilter().addRelation( m_pSerializer->getOutputStream(),
                        oox::getRelationship(Relationship::HYPERLINK),
                        sUrl, true );

            m_pHyperlinkAttrList->add(FSNS(XML_r, XML_id), sId);
            if (!sMark.isEmpty())
            {
                sMark = sMark.replace(' ', '_');
                m_pHyperlinkAttrList->add(FSNS(XML_w, XML_anchor), sMark);
            }
        }
        else
        {
            // Is this a link to a sequence? Then try to replace that with a
            // normal bookmark, as Word won't understand our special
            // <seqname>!<index>|sequence syntax.
            if (sMark.endsWith("|sequence"))
            {
                sal_Int32 nPos = sMark.indexOf('!');
                if (nPos != -1)
                {
                    // Extract <seqname>, the field instruction text has the name quoted.
                    OUString aSequenceName = sMark.copy(0, nPos);
                    // Extract <index>.
                    sal_uInt32 nIndex = o3tl::toUInt32(sMark.subView(nPos + 1, sMark.getLength() - nPos - sizeof("|sequence")));
                    auto it = m_aSeqBookmarksNames.find(aSequenceName);
                    if (it != m_aSeqBookmarksNames.end())
                    {
                        std::vector<OUString>& rNames = it->second;
                        if (rNames.size() > nIndex)
                            // We know the bookmark name for this sequence and this index, do the replacement.
                            sMark = rNames[nIndex];
                    }
                }
            }
            else if (sMark.endsWith("|toxmark"))
            {
                if (auto const it = GetExport().m_TOXMarkBookmarksByURL.find(sMark);
                    it != GetExport().m_TOXMarkBookmarksByURL.end())
                {
                    sMark = it->second;
                }
            }
            // Spaces are prohibited in bookmark name.
            sMark = sMark.replace(' ', '_');
            m_pHyperlinkAttrList->add( FSNS( XML_w, XML_anchor ), sMark );
        }

        if ( !rTarget.isEmpty() )
        {
            m_pHyperlinkAttrList->add(FSNS(XML_w, XML_tgtFrame), rTarget);
        }
        else if (!rName.isEmpty())
        {
            m_pHyperlinkAttrList->add(FSNS(XML_w, XML_tooltip), rName);
        }
    }

    return true;
}

bool DocxAttributeOutput::EndURL(bool const)
{
    m_closeHyperlinkInThisRun = true;
    if (m_nHyperLinkCount.back() > 0 && !m_hyperLinkAnchor.isEmpty()
        && m_hyperLinkAnchor.startsWith("_Toc"))
    {
        m_endPageRef = true;
    }
    return true;
}

void DocxAttributeOutput::FieldVanish(const OUString& rText,
        ww::eField const eType, OUString const*const pBookmarkName)
{
    WriteField_Impl(nullptr, eType, rText, FieldFlags::All, pBookmarkName);
}

// The difference between 'Redline' and 'StartRedline'+'EndRedline' is that:
// 'Redline' is used for tracked changes of formatting information of a run like Bold, Underline. (the '<w:rPrChange>' is inside the 'run' node)
// 'StartRedline' is used to output tracked changes of run insertion and deletion (the run is inside the '<w:ins>' node)
void DocxAttributeOutput::Redline( const SwRedlineData* pRedlineData)
{
    if ( !pRedlineData )
        return;

    bool bRemovePersonalInfo = SvtSecurityOptions::IsOptionSet(
        SvtSecurityOptions::EOption::DocWarnRemovePersonalInfo ) && !SvtSecurityOptions::IsOptionSet(
            SvtSecurityOptions::EOption::DocWarnKeepRedlineInfo);

    OString aId( OString::number( pRedlineData->GetSeqNo() ) );
    const OUString& rAuthor(SwModule::get()->GetRedlineAuthor(pRedlineData->GetAuthor()));
    const DateTime& aDateTime = pRedlineData->GetTimeStamp();
    bool bNoDate = bRemovePersonalInfo ||
        ( aDateTime.GetYear() == 1970 && aDateTime.GetMonth() == 1 && aDateTime.GetDay() == 1 );

    switch( pRedlineData->GetType() )
    {
    case RedlineType::Insert:
        break;

    case RedlineType::Delete:
        break;

    case RedlineType::Format:
    {
        rtl::Reference<sax_fastparser::FastAttributeList> pAttributeList
            = sax_fastparser::FastSerializerHelper::createAttrList();

        pAttributeList->add(FSNS( XML_w, XML_id ), aId);
        pAttributeList->add(FSNS( XML_w, XML_author ), bRemovePersonalInfo
                    ? "Author" + OString::number( GetExport().GetInfoID(rAuthor) )
                    : rAuthor.toUtf8());
        if (!bNoDate)
            pAttributeList->add(FSNS( XML_w, XML_date ), DateTimeToOString( aDateTime ));
        m_pSerializer->startElementNS( XML_w, XML_rPrChange, pAttributeList );

        // Check if there is any extra data stored in the redline object
        if (pRedlineData->GetExtraData())
        {
            const SwRedlineExtraData* pExtraData = pRedlineData->GetExtraData();
            const SwRedlineExtraData_FormatColl* pFormattingChanges = dynamic_cast<const SwRedlineExtraData_FormatColl*>(pExtraData);

            // Check if the extra data is of type 'formatting changes'
            if (pFormattingChanges)
            {
                 // Get the item set that holds all the changes properties
                const SfxItemSet *pChangesSet = pFormattingChanges->GetItemSet();
                if (pChangesSet)
                {
                    m_pSerializer->mark(Tag_Redline_1);

                    m_pSerializer->startElementNS(XML_w, XML_rPr);

                    // Output the redline item set
                    m_rExport.OutputItemSet( *pChangesSet, false, true, i18n::ScriptType::LATIN, m_rExport.m_bExportModeRTF );

                    m_pSerializer->endElementNS( XML_w, XML_rPr );

                    m_pSerializer->mergeTopMarks(Tag_Redline_1, sax_fastparser::MergeMarks::PREPEND);
                }
            }
        }

        m_pSerializer->endElementNS( XML_w, XML_rPrChange );
        break;
    }
    case RedlineType::ParagraphFormat:
    {
        rtl::Reference<sax_fastparser::FastAttributeList> pAttributeList
            = sax_fastparser::FastSerializerHelper::createAttrList();

        pAttributeList->add(FSNS( XML_w, XML_id ), aId);
        pAttributeList->add(FSNS( XML_w, XML_author ), bRemovePersonalInfo
                    ? "Author" + OString::number( GetExport().GetInfoID(rAuthor) )
                    : rAuthor.toUtf8());
        if (!bNoDate)
            pAttributeList->add(FSNS( XML_w, XML_date ), DateTimeToOString( aDateTime ));
        m_pSerializer->startElementNS( XML_w, XML_pPrChange, pAttributeList );

        // Check if there is any extra data stored in the redline object
        if (pRedlineData->GetExtraData())
        {
            const SwRedlineExtraData* pExtraData = pRedlineData->GetExtraData();
            const SwRedlineExtraData_FormatColl* pFormattingChanges = dynamic_cast<const SwRedlineExtraData_FormatColl*>(pExtraData);

            // Check if the extra data is of type 'formatting changes'
            if (pFormattingChanges)
            {
                // Get the item set that holds all the changes properties
                const SfxItemSet *pChangesSet = pFormattingChanges->GetItemSet();
                const UIName & sParaStyleName = pFormattingChanges->GetFormatName();
                if (pChangesSet || !sParaStyleName.isEmpty())
                {
                    m_pSerializer->mark(Tag_Redline_2);

                    m_pSerializer->startElementNS(XML_w, XML_pPr);

                    if (!sParaStyleName.isEmpty())
                    {
                        OString sStyleName;
                        if (auto format = m_rExport.m_rDoc.FindTextFormatCollByName(sParaStyleName))
                            if (auto slot = m_rExport.m_pStyles->GetSlot(format); slot != 0xfff)
                                sStyleName = m_rExport.m_pStyles->GetStyleId(slot);
                        // The resolved style name can be empty at this point, sParaStyleName can be
                        // an arbitrary string from the original document.
                        // Note that Word does *not* roundtrip unknown style names in redlines!
                        if (sStyleName.isEmpty())
                            sStyleName = MSWordStyles::CreateStyleId(sParaStyleName.toString());
                        if (!sStyleName.isEmpty())
                            m_pSerializer->singleElementNS(XML_w, XML_pStyle, FSNS(XML_w, XML_val), sStyleName);
                    }

                    // The 'm_rExport.SdrExporter().getFlyAttrList()', 'm_pParagraphSpacingAttrList' are used to hold information
                    // that should be collected by different properties in the core, and are all flushed together
                    // to the DOCX when the function 'WriteCollectedParagraphProperties' gets called.
                    // So we need to store the current status of these lists, so that we can revert back to them when
                    // we are done exporting the redline attributes.
                    auto pFlyAttrList_Original(detachFrom(m_rExport.SdrExporter().getFlyAttrList()));
                    auto pLRSpaceAttrList_Original(detachFrom(m_pLRSpaceAttrList));
                    auto pParagraphSpacingAttrList_Original(detachFrom(m_pParagraphSpacingAttrList));

                    // Output the redline item set
                    if (pChangesSet)
                        m_rExport.OutputItemSet( *pChangesSet, true, false, i18n::ScriptType::LATIN, m_rExport.m_bExportModeRTF );

                    // Write the collected paragraph properties that are stored in 'm_rExport.SdrExporter().getFlyAttrList()', 'm_pParagraphSpacingAttrList'
                    WriteCollectedParagraphProperties();

                    // Revert back the original values that were stored in 'm_rExport.SdrExporter().getFlyAttrList()', 'm_pParagraphSpacingAttrList'
                    m_rExport.SdrExporter().getFlyAttrList() = std::move(pFlyAttrList_Original);
                    m_pLRSpaceAttrList = std::move(pLRSpaceAttrList_Original);
                    m_pParagraphSpacingAttrList = std::move(pParagraphSpacingAttrList_Original);

                    m_pSerializer->endElementNS( XML_w, XML_pPr );

                    m_pSerializer->mergeTopMarks(Tag_Redline_2, sax_fastparser::MergeMarks::PREPEND);
                }
            }
        }
        m_pSerializer->endElementNS( XML_w, XML_pPrChange );
        break;
    }
    default:
        SAL_WARN("sw.ww8", "Unhandled redline type for export " << SwRedlineTypeToOUString(pRedlineData->GetType()));
        break;
    }
}

// The difference between 'Redline' and 'StartRedline'+'EndRedline' is that:
// 'Redline' is used for tracked changes of formatting information of a run like Bold, Underline. (the '<w:rPrChange>' is inside the 'run' node)
// 'StartRedline' is used to output tracked changes of run insertion and deletion (the run is inside the '<w:ins>' node)
void DocxAttributeOutput::StartRedline(const SwRedlineData* pRedlineData, bool bLastRun,
                                       bool bParagraphProps)
{
    if ( !pRedlineData )
        return;

    // write out stack of this redline recursively (first the oldest)
    if ( !bLastRun )
        StartRedline( pRedlineData->Next(), false );

    OString aId( OString::number( m_nRedlineId++ ) );

    bool bRemovePersonalInfo = SvtSecurityOptions::IsOptionSet(
        SvtSecurityOptions::EOption::DocWarnRemovePersonalInfo ) && !SvtSecurityOptions::IsOptionSet(
            SvtSecurityOptions::EOption::DocWarnKeepRedlineInfo);

    const OUString& rAuthor(SwModule::get()->GetRedlineAuthor(pRedlineData->GetAuthor()));
    OString aAuthor( OUStringToOString( bRemovePersonalInfo
                        ? "Author" + OUString::number( GetExport().GetInfoID(rAuthor) )
                        : rAuthor, RTL_TEXTENCODING_UTF8 ) );

    const DateTime& aDateTime = pRedlineData->GetTimeStamp();
    bool bNoDate = bRemovePersonalInfo ||
        ( aDateTime.GetYear() == 1970 && aDateTime.GetMonth() == 1 && aDateTime.GetDay() == 1 );
    bool isInMoveBookmark = false;
    for (const auto& openedBookmark : m_rOpenedBookmarksIds)
    {
        if (openedBookmark.first.startsWith(u"__RefMove"))
        {
            isInMoveBookmark = true;
            break;
        }
    }
    bool bMoved = (isInMoveBookmark || bParagraphProps) && pRedlineData->IsMoved() &&
                  // tdf#150166 save tracked moving around TOC as w:ins, w:del
                  SwDoc::GetCurTOX(*m_rExport.m_pCurPam->GetPoint()) == nullptr;
    switch ( pRedlineData->GetType() )
    {
        case RedlineType::Insert:
        case RedlineType::Delete:
        {
            sal_Int32 eElement = RedlineType::Insert == pRedlineData->GetType()
                ? ( bMoved ? XML_moveTo : XML_ins )
                : ( bMoved ? XML_moveFrom : XML_del );
            if ( bNoDate )
                m_pSerializer->startElementNS( XML_w, eElement,
                    FSNS( XML_w, XML_id ), aId,
                    FSNS( XML_w, XML_author ), aAuthor );
            else
                m_pSerializer->startElementNS( XML_w, eElement,
                    FSNS( XML_w, XML_id ), aId,
                    FSNS( XML_w, XML_author ), aAuthor,
                    FSNS( XML_w, XML_date ), DateTimeToOString( aDateTime ) );
            break;
        }
        case RedlineType::Format:
            SAL_INFO("sw.ww8", "TODO DocxAttributeOutput::StartRedline()" );
            break;
        default:
            break;
    }
}

void DocxAttributeOutput::EndRedline(const SwRedlineData* pRedlineData, bool bLastRun,
                                     bool bParagraphProps)
{
    if ( !pRedlineData || m_bWritingField )
        return;

    bool isInMoveBookmark = false;
    for (const auto& openedBookmark : m_rOpenedBookmarksIds)
    {
        if (openedBookmark.first.startsWith(u"__RefMove"))
        {
            isInMoveBookmark = true;
            break;
        }
    }
    bool bMoved = (isInMoveBookmark || bParagraphProps) && pRedlineData->IsMoved() &&
                  // tdf#150166 save tracked moving around TOC as w:ins, w:del
                  SwDoc::GetCurTOX(*m_rExport.m_pCurPam->GetPoint()) == nullptr;
    switch ( pRedlineData->GetType() )
    {
        case RedlineType::Insert:
            m_pSerializer->endElementNS( XML_w, bMoved ? XML_moveTo : XML_ins );
            break;

        case RedlineType::Delete:
            m_pSerializer->endElementNS( XML_w, bMoved ? XML_moveFrom : XML_del );
            break;

        case RedlineType::Format:
            SAL_INFO("sw.ww8", "TODO DocxAttributeOutput::EndRedline()" );
            break;
        default:
            break;
    }

    // write out stack of this redline recursively (first the newest)
    if ( !bLastRun )
        EndRedline( pRedlineData->Next(), false );
}

void DocxAttributeOutput::FormatDrop( const SwTextNode& /*rNode*/, const SwFormatDrop& /*rSwFormatDrop*/, sal_uInt16 /*nStyle*/, ww8::WW8TableNodeInfo::Pointer_t /*pTextNodeInfo*/, ww8::WW8TableNodeInfoInner::Pointer_t )
{
    SAL_INFO("sw.ww8", "TODO DocxAttributeOutput::FormatDrop( const SwTextNode& rNode, const SwFormatDrop& rSwFormatDrop, sal_uInt16 nStyle )" );
}

void DocxAttributeOutput::ParagraphStyle( sal_uInt16 nStyle )
{
    OString aStyleId(m_rExport.m_pStyles->GetStyleId(nStyle));

    m_pSerializer->singleElementNS(XML_w, XML_pStyle, FSNS(XML_w, XML_val), aStyleId);
}

static void impl_borderLine( FSHelperPtr const & pSerializer, sal_Int32 elementToken, const SvxBorderLine* pBorderLine, sal_uInt16 nDist,
                             bool bWriteShadow, const table::BorderLine2* pStyleProps = nullptr)
{
    // Compute val attribute value
    // Can be one of:
    //      single, double,
    //      basicWideOutline, basicWideInline
    // OOXml also supports those types of borders, but we'll try to play with the first ones.
    //      thickThinMediumGap, thickThinLargeGap, thickThinSmallGap
    //      thinThickLargeGap, thinThickMediumGap, thinThickSmallGap
    const char* pVal = "nil";
    if ( pBorderLine && !pBorderLine->isEmpty( ) )
    {
        switch (pBorderLine->GetBorderLineStyle())
        {
            case SvxBorderLineStyle::SOLID:
                pVal = "single";
                break;
            case SvxBorderLineStyle::DOTTED:
                pVal = "dotted";
                break;
            case SvxBorderLineStyle::DASHED:
                pVal = "dashed";
                break;
            case SvxBorderLineStyle::DOUBLE:
            case SvxBorderLineStyle::DOUBLE_THIN:
                pVal = "double";
                break;
            case SvxBorderLineStyle::THINTHICK_SMALLGAP:
                pVal = "thinThickSmallGap";
                break;
            case SvxBorderLineStyle::THINTHICK_MEDIUMGAP:
                pVal = "thinThickMediumGap";
                break;
            case SvxBorderLineStyle::THINTHICK_LARGEGAP:
                pVal = "thinThickLargeGap";
                break;
            case SvxBorderLineStyle::THICKTHIN_SMALLGAP:
                pVal = "thickThinSmallGap";
                break;
            case SvxBorderLineStyle::THICKTHIN_MEDIUMGAP:
                pVal = "thickThinMediumGap";
                break;
            case SvxBorderLineStyle::THICKTHIN_LARGEGAP:
                pVal = "thickThinLargeGap";
                break;
            case SvxBorderLineStyle::EMBOSSED:
                pVal = "threeDEmboss";
                break;
            case SvxBorderLineStyle::ENGRAVED:
                pVal = "threeDEngrave";
                break;
            case SvxBorderLineStyle::OUTSET:
                pVal = "outset";
                break;
            case SvxBorderLineStyle::INSET:
                pVal = "inset";
                break;
            case SvxBorderLineStyle::FINE_DASHED:
                pVal = "dashSmallGap";
                break;
            case SvxBorderLineStyle::DASH_DOT:
                pVal = "dotDash";
                break;
            case SvxBorderLineStyle::DASH_DOT_DOT:
                pVal = "dotDotDash";
                break;
            case SvxBorderLineStyle::NONE:
            default:
                break;
        }
    }
    else if (!pStyleProps || !pStyleProps->LineWidth)
        // no line, and no line set by the style either:
        // there is no need to write the property
        return;

    // compare the properties with the theme properties before writing them:
    // if they are equal, it means that they were style-defined and there is
    // no need to write them.
    if (pStyleProps && pBorderLine && !pBorderLine->isEmpty()
        && pBorderLine->GetBorderLineStyle()
               == static_cast<SvxBorderLineStyle>(pStyleProps->LineStyle)
        && pBorderLine->GetColor() == Color(ColorTransparency, pStyleProps->Color)
        && pBorderLine->GetWidth() == o3tl::toTwips(pStyleProps->LineWidth, o3tl::Length::mm100))
    {
        return;
    }

    rtl::Reference<FastAttributeList> pAttr = FastSerializerHelper::createAttrList();
    pAttr->add( FSNS( XML_w, XML_val ), pVal );

    if ( pBorderLine && !pBorderLine->isEmpty() )
    {
        // Compute the sz attribute

        double const fConverted( ::editeng::ConvertBorderWidthToWord(
                pBorderLine->GetBorderLineStyle(), pBorderLine->GetWidth()));
        // The unit is the 8th of point
        sal_Int32 nWidth = sal_Int32( fConverted / 2.5 );
        const sal_Int32 nMinWidth = 2;
        const sal_Int32 nMaxWidth = 96;

        if ( nWidth > nMaxWidth )
            nWidth = nMaxWidth;
        else if ( nWidth < nMinWidth )
            nWidth = nMinWidth;

        pAttr->add( FSNS( XML_w, XML_sz ), OString::number( nWidth ) );

        // Get the distance (in pt)
        pAttr->add(FSNS(XML_w, XML_space), OString::number(rtl::math::round(nDist / 20.0)));

        // Get the color code as an RRGGBB hex value
        OString sColor( msfilter::util::ConvertColor( pBorderLine->GetColor( ) ) );
        pAttr->add( FSNS(XML_w, XML_color), sColor);

        model::ComplexColor const& rComplexColor = pBorderLine->getComplexColor();
        lclAddThemeColorAttributes(pAttr, rComplexColor);
    }

    if (bWriteShadow)
    {
        // Set the shadow value
        pAttr->add( FSNS( XML_w, XML_shadow ), "1" );
    }

    pSerializer->singleElementNS( XML_w, elementToken, pAttr );
}

static OutputBorderOptions lcl_getTableCellBorderOptions(bool bEcma)
{
    OutputBorderOptions rOptions;

    rOptions.tag = XML_tcBorders;
    rOptions.bUseStartEnd = !bEcma;
    rOptions.bWriteTag = true;
    rOptions.bWriteDistance = false;

    return rOptions;
}

static OutputBorderOptions lcl_getBoxBorderOptions()
{
    OutputBorderOptions rOptions;

    rOptions.tag = XML_pBdr;
    rOptions.bUseStartEnd = false;
    rOptions.bWriteTag = false;
    rOptions.bWriteDistance = true;

    return rOptions;
}

static void impl_borders( FSHelperPtr const & pSerializer,
                          const SvxBoxItem& rBox,
                          const OutputBorderOptions& rOptions,
                          std::map<SvxBoxItemLine,
                          css::table::BorderLine2> &rTableStyleConf,
                          const ww8::Frame* pFramePr = nullptr)
{
    static const SvxBoxItemLine aBorders[] =
    {
        SvxBoxItemLine::TOP, SvxBoxItemLine::LEFT, SvxBoxItemLine::BOTTOM, SvxBoxItemLine::RIGHT
    };

    const sal_Int32 aXmlElements[] =
    {
        XML_top,
        rOptions.bUseStartEnd ? XML_start : XML_left,
        XML_bottom,
        rOptions.bUseStartEnd ? XML_end : XML_right
    };
    bool tagWritten = false;
    const SvxBoxItemLine* pBrd = aBorders;

    for( int i = 0; i < 4; ++i, ++pBrd )
    {
        const SvxBorderLine* pLn = rBox.GetLine( *pBrd );
        const table::BorderLine2 *aStyleProps = nullptr;
        auto it = rTableStyleConf.find( *pBrd );
        if( it != rTableStyleConf.end() )
            aStyleProps = &(it->second);

        if (!tagWritten && rOptions.bWriteTag)
        {
            pSerializer->startElementNS(XML_w, rOptions.tag);
            tagWritten = true;
        }

        bool bWriteShadow = false;
        if (rOptions.aShadowLocation == SvxShadowLocation::NONE)
        {
            // The border has no shadow
        }
        else if (rOptions.aShadowLocation == SvxShadowLocation::BottomRight)
        {
            // Special case of 'Bottom-Right' shadow:
            // If the shadow location is 'Bottom-Right' - then turn on the shadow
            // for ALL the sides. This is because in Word - if you select a shadow
            // for a border - it turn on the shadow for ALL the sides (but shows only
            // the bottom-right one).
            // This is so that no information will be lost if passed through LibreOffice
            bWriteShadow = true;
        }
        else
        {
            // If there is a shadow, and it's not the regular 'Bottom-Right',
            // then write only the 'shadowed' sides of the border
            if  (
                    ((rOptions.aShadowLocation == SvxShadowLocation::TopLeft    || rOptions.aShadowLocation == SvxShadowLocation::TopRight  ) && *pBrd == SvxBoxItemLine::TOP   ) ||
                    ((rOptions.aShadowLocation == SvxShadowLocation::TopLeft    || rOptions.aShadowLocation == SvxShadowLocation::BottomLeft) && *pBrd == SvxBoxItemLine::LEFT  ) ||
                    ((rOptions.aShadowLocation == SvxShadowLocation::BottomLeft                                                             ) && *pBrd == SvxBoxItemLine::BOTTOM) ||
                    ((rOptions.aShadowLocation == SvxShadowLocation::TopRight                                                               ) && *pBrd == SvxBoxItemLine::RIGHT )
                )
            {
                bWriteShadow = true;
            }
        }

        sal_uInt16 nDist = 0;
        if (rOptions.bWriteDistance)
        {
            if (rOptions.pDistances)
            {
                if ( *pBrd == SvxBoxItemLine::TOP)
                    nDist = rOptions.pDistances->nTop;
                else if ( *pBrd == SvxBoxItemLine::LEFT)
                    nDist = rOptions.pDistances->nLeft;
                else if ( *pBrd == SvxBoxItemLine::BOTTOM)
                    nDist = rOptions.pDistances->nBottom;
                else if ( *pBrd == SvxBoxItemLine::RIGHT)
                    nDist = rOptions.pDistances->nRight;
            }
            else
            {
                nDist = rBox.GetDistance(*pBrd);
            }
        }

        if (pFramePr)
        {
            assert(rOptions.bWriteDistance && !rOptions.pDistances);

            // In addition to direct properties, and paragraph styles,
            // for framePr-floated paragraphs the frame borders also affect the exported values.

            // For border spacing, there is a special situation to consider
            // because a compat setting ignores left/right paragraph spacing on layout.
            const SwFrameFormat& rFormat = pFramePr->GetFrameFormat();
            const SvxBoxItem& rFramePrBox = rFormat.GetBox();
            const IDocumentSettingAccess& rIDSA = rFormat.GetDoc().getIDocumentSettingAccess();
            if (rIDSA.get(DocumentSettingId::INVERT_BORDER_SPACING)
                && (*pBrd == SvxBoxItemLine::LEFT || *pBrd == SvxBoxItemLine::RIGHT))
            {
                // only the frame's border spacing affects layout - so use that value instead.
                nDist = rFramePrBox.GetDistance(*pBrd);
            }
            else
            {
                nDist += rFramePrBox.GetDistance(*pBrd);
            }

            // Unless the user added a paragraph border, the border normally comes from the frame.
            if (!pLn)
                pLn = rFramePrBox.GetLine(*pBrd);
        }

        impl_borderLine( pSerializer, aXmlElements[i], pLn, nDist, bWriteShadow, aStyleProps );
    }
    if (tagWritten && rOptions.bWriteTag) {
        pSerializer->endElementNS( XML_w, rOptions.tag );
    }
}

void DocxAttributeOutput::ImplCellMargins( FSHelperPtr const & pSerializer, const SvxBoxItem& rBox, sal_Int32 tag, bool bUseStartEnd, const SvxBoxItem* pDefaultMargins)
{
    static const SvxBoxItemLine aBorders[] =
    {
        SvxBoxItemLine::TOP, SvxBoxItemLine::LEFT, SvxBoxItemLine::BOTTOM, SvxBoxItemLine::RIGHT
    };

    const sal_Int32 aXmlElements[] =
    {
        XML_top,
        bUseStartEnd ? XML_start : XML_left,
        XML_bottom,
        bUseStartEnd ? XML_end : XML_right
    };
    bool tagWritten = false;
    const SvxBoxItemLine* pBrd = aBorders;
    for( int i = 0; i < 4; ++i, ++pBrd )
    {
        sal_Int32 nDist = sal_Int32( rBox.GetDistance( *pBrd ) );

        if (pDefaultMargins)
        {
            // Skip output if cell margin == table default margin
            if (sal_Int32( pDefaultMargins->GetDistance( *pBrd ) ) == nDist)
                continue;
        }

        if (!tagWritten) {
            pSerializer->startElementNS(XML_w, tag);
            tagWritten = true;
        }
        pSerializer->singleElementNS( XML_w, aXmlElements[i],
               FSNS( XML_w, XML_w ), OString::number(nDist),
               FSNS( XML_w, XML_type ), "dxa" );
    }
    if (tagWritten) {
        pSerializer->endElementNS( XML_w, tag );
    }
}

void DocxAttributeOutput::TableCellProperties( ww8::WW8TableNodeInfoInner::Pointer_t const & pTableTextNodeInfoInner, sal_uInt32 nCell, sal_uInt32 nRow )
{
    m_pSerializer->startElementNS(XML_w, XML_tcPr);

    const SwTableBox *pTableBox = pTableTextNodeInfoInner->getTableBox( );

    bool const bEcma = GetExport().GetFilter().getVersion() == oox::core::ECMA_376_1ST_EDITION;

    // Output any table cell redlines if there are any attached to this specific cell
    TableCellRedline( pTableTextNodeInfoInner );

    // Cell preferred width
    SwTwips nWidth = GetGridCols( pTableTextNodeInfoInner )->at( nCell );
    if ( nCell )
        nWidth = nWidth - GetGridCols( pTableTextNodeInfoInner )->at( nCell - 1 );
    m_pSerializer->singleElementNS( XML_w, XML_tcW,
           FSNS( XML_w, XML_w ), OString::number(nWidth),
           FSNS( XML_w, XML_type ), "dxa" );

    // Horizontal spans
    const SwWriteTableRows& rRows = m_xTableWrt->GetRows( );
    if (nRow >= rRows.size())
        SAL_WARN("sw.ww8", "DocxAttributeOutput::TableCellProperties: out of range row: " << nRow);
    else
    {
        SwWriteTableRow *pRow = rRows[ nRow ].get();
        const SwWriteTableCells& rTableCells =  pRow->GetCells();
        if (nCell < rTableCells.size() )
        {
            const SwWriteTableCell& rCell = *rTableCells[nCell];
            const sal_uInt16 nColSpan = rCell.GetColSpan();
            if ( nColSpan > 1 )
                m_pSerializer->singleElementNS( XML_w, XML_gridSpan,
                        FSNS( XML_w, XML_val ), OString::number(nColSpan) );
        }
    }

    // Vertical merges
    ww8::RowSpansPtr xRowSpans = pTableTextNodeInfoInner->getRowSpansOfRow();
    sal_Int32 vSpan = (*xRowSpans)[nCell];
    if ( vSpan > 1 )
    {
        m_pSerializer->singleElementNS(XML_w, XML_vMerge, FSNS(XML_w, XML_val), "restart");
    }
    else if ( vSpan < 0 )
    {
        m_pSerializer->singleElementNS(XML_w, XML_vMerge, FSNS(XML_w, XML_val), "continue");
    }

    if (const SfxGrabBagItem* pItem = pTableBox->GetFrameFormat()->GetAttrSet().GetItem<SfxGrabBagItem>(RES_FRMATR_GRABBAG))
    {
        const std::map<OUString, uno::Any>& rGrabBag = pItem->GetGrabBag();
        std::map<OUString, uno::Any>::const_iterator it = rGrabBag.find(u"CellCnfStyle"_ustr);
        if (it != rGrabBag.end())
        {
            uno::Sequence<beans::PropertyValue> aAttributes = it->second.get< uno::Sequence<beans::PropertyValue> >();
            m_pTableStyleExport->CnfStyle(aAttributes);
        }
    }


    const SvxBoxItem& rBox = pTableBox->GetFrameFormat( )->GetBox( );
    const SvxBoxItem& rDefaultBox = (*m_TableFirstCells.rbegin())->getTableBox( )->GetFrameFormat( )->GetBox( );
    {
        // The cell borders
        impl_borders(m_pSerializer, rBox, lcl_getTableCellBorderOptions(bEcma),
                     m_aTableStyleConfs.back());
    }

    TableBackgrounds( pTableTextNodeInfoInner );

    {
        // Cell margins
        DocxAttributeOutput::ImplCellMargins( m_pSerializer, rBox, XML_tcMar, !bEcma, &rDefaultBox );
    }

    TableVerticalCell( pTableTextNodeInfoInner );

    m_pSerializer->endElementNS( XML_w, XML_tcPr );
}

void DocxAttributeOutput::InitTableHelper( ww8::WW8TableNodeInfoInner::Pointer_t const & pTableTextNodeInfoInner )
{
    const SwTable* pTable = pTableTextNodeInfoInner->getTable();
    if (m_xTableWrt && pTable == m_xTableWrt->GetTable())
        return;

    tools::Long nPageSize = 0;
    bool bRelBoxSize = false;

    // Create the SwWriteTable instance to use col spans (and maybe other infos)
    GetTablePageSize( pTableTextNodeInfoInner.get(), nPageSize, bRelBoxSize );

    const SwFrameFormat *pFormat = pTable->GetFrameFormat( );
    const sal_uInt32 nTableSz = static_cast<sal_uInt32>(pFormat->GetFrameSize( ).GetWidth( ));

    const SwHTMLTableLayout *pLayout = pTable->GetHTMLTableLayout();
    if( pLayout && pLayout->IsExportable() )
        m_xTableWrt.reset(new SwWriteTable(pTable, pLayout));
    else
        m_xTableWrt.reset(new SwWriteTable(pTable, pTable->GetTabLines(), nPageSize, nTableSz, false));
}

void DocxAttributeOutput::StartTable( ww8::WW8TableNodeInfoInner::Pointer_t const & pTableTextNodeInfoInner )
{
    m_aTableStyleConfs.emplace_back();

    // In case any paragraph SDT's are open, close them here.
    EndParaSdtBlock();

    m_pSerializer->startElementNS(XML_w, XML_tbl);

    m_TableFirstCells.push_back(pTableTextNodeInfoInner);
    m_LastOpenCell.push_back(-1);
    m_LastClosedCell.push_back(-1);

    InitTableHelper( pTableTextNodeInfoInner );
    TableDefinition( pTableTextNodeInfoInner );
}

void DocxAttributeOutput::EndTable()
{
    m_pSerializer->endElementNS( XML_w, XML_tbl );

    if ( m_tableReference.m_nTableDepth > 0 )
        --m_tableReference.m_nTableDepth;

    m_LastClosedCell.pop_back();
    m_LastOpenCell.pop_back();
    m_TableFirstCells.pop_back();

    // We closed the table; if it is a nested table, the cell that contains it
    // still continues
    // set to true only if we were in a nested table, not otherwise.
    if( !m_TableFirstCells.empty() )
        m_tableReference.m_bTableCellOpen = true;

    // Cleans the table helper
    m_xTableWrt.reset();

    m_aTableStyleConfs.pop_back();
}

void DocxAttributeOutput::StartTableRow( ww8::WW8TableNodeInfoInner::Pointer_t const & pTableTextNodeInfoInner )
{
    m_pSerializer->startElementNS(XML_w, XML_tr);

    // Output the row properties
    m_pSerializer->startElementNS(XML_w, XML_trPr);

    // Header row: tblHeader
    const SwTable *pTable = pTableTextNodeInfoInner->getTable( );
    if ( pTable->GetRowsToRepeat( ) > pTableTextNodeInfoInner->getRow( ) )
        m_pSerializer->singleElementNS(XML_w, XML_tblHeader, FSNS(XML_w, XML_val), "true"); // TODO to overwrite table style may need explicit false

    TableRowRedline( pTableTextNodeInfoInner );
    TableHeight( pTableTextNodeInfoInner );
    TableCanSplit( pTableTextNodeInfoInner );

    const SwTableBox *pTableBox = pTableTextNodeInfoInner->getTableBox();
    const SwTableLine* pTableLine = pTableBox->GetUpper();
    if (const SfxGrabBagItem* pItem = pTableLine->GetFrameFormat()->GetAttrSet().GetItem<SfxGrabBagItem>(RES_FRMATR_GRABBAG))
    {
        const std::map<OUString, uno::Any>& rGrabBag = pItem->GetGrabBag();
        std::map<OUString, uno::Any>::const_iterator it = rGrabBag.find(u"RowCnfStyle"_ustr);
        if (it != rGrabBag.end())
        {
            uno::Sequence<beans::PropertyValue> aAttributes = it->second.get< uno::Sequence<beans::PropertyValue> >();
            m_pTableStyleExport->CnfStyle(aAttributes);
        }
    }


    m_pSerializer->endElementNS( XML_w, XML_trPr );
}

void DocxAttributeOutput::EndTableRow( )
{
    m_pSerializer->endElementNS( XML_w, XML_tr );
    m_LastOpenCell.back() = -1;
    m_LastClosedCell.back() = -1;
}

void DocxAttributeOutput::StartTableCell( ww8::WW8TableNodeInfoInner::Pointer_t const & pTableTextNodeInfoInner, sal_uInt32 nCell, sal_uInt32 nRow )
{
    m_LastOpenCell.back() = nCell;

    InitTableHelper( pTableTextNodeInfoInner );

    // check tracked table column deletion or insertion
    const SwTableBox* pTabBox = pTableTextNodeInfoInner->getTableBox();
    SwRedlineTable::size_type nChange = pTabBox->GetRedline();
    if (nChange != SwRedlineTable::npos)
        m_tableReference.m_bTableCellChanged = true;

    m_pSerializer->startElementNS(XML_w, XML_tc);

    // Write the cell properties here
    TableCellProperties( pTableTextNodeInfoInner, nCell, nRow );

    m_tableReference.m_bTableCellOpen = true;
}

void DocxAttributeOutput::EndTableCell(sal_uInt32 nCell)
{
    m_LastClosedCell.back() = nCell;
    m_LastOpenCell.back() = -1;

    if (m_tableReference.m_bTableCellParaSdtOpen)
        EndParaSdtBlock();

    m_pSerializer->endElementNS( XML_w, XML_tc );

    m_tableReference.m_bTableCellOpen = false;
    m_tableReference.m_bTableCellParaSdtOpen = false;
    m_tableReference.m_bTableCellChanged = false;
}

void DocxAttributeOutput::StartStyles()
{
    m_pSerializer->startElementNS( XML_w, XML_styles,
            FSNS( XML_xmlns, XML_w ),   GetExport().GetFilter().getNamespaceURL(OOX_NS(doc)),
            FSNS( XML_xmlns, XML_w14 ), GetExport().GetFilter().getNamespaceURL(OOX_NS(w14)),
            FSNS( XML_xmlns, XML_mc ),  GetExport().GetFilter().getNamespaceURL(OOX_NS(mce)),
            FSNS( XML_mc, XML_Ignorable ), "w14" );

    DocDefaults();
    LatentStyles();
}

sal_Int32 DocxStringGetToken(DocxStringTokenMap const * pMap, std::u16string_view rName)
{
    OString sName = OUStringToOString(rName, RTL_TEXTENCODING_UTF8);
    while (pMap->pToken)
    {
        if (sName == pMap->pToken)
            return pMap->nToken;
        ++pMap;
    }
    return 0;
}

namespace
{

DocxStringTokenMap const aDefaultTokens[] = {
    {"defQFormat", XML_defQFormat},
    {"defUnhideWhenUsed", XML_defUnhideWhenUsed},
    {"defSemiHidden", XML_defSemiHidden},
    {"count", XML_count},
    {"defUIPriority", XML_defUIPriority},
    {"defLockedState", XML_defLockedState},
    {nullptr, 0}
};

DocxStringTokenMap const aExceptionTokens[] = {
    {"name", XML_name},
    {"locked", XML_locked},
    {"uiPriority", XML_uiPriority},
    {"semiHidden", XML_semiHidden},
    {"unhideWhenUsed", XML_unhideWhenUsed},
    {"qFormat", XML_qFormat},
    {nullptr, 0}
};

}

void DocxAttributeOutput::LatentStyles()
{
    // Do we have latent styles available?
    uno::Sequence<beans::PropertyValue> aInteropGrabBag;
    m_rExport.m_xTextDoc->getPropertyValue(u"InteropGrabBag"_ustr) >>= aInteropGrabBag;
    uno::Sequence<beans::PropertyValue> aLatentStyles;
    auto pProp = std::find_if(std::cbegin(aInteropGrabBag), std::cend(aInteropGrabBag),
        [](const beans::PropertyValue& rProp) { return rProp.Name == "latentStyles"; });
    if (pProp != std::cend(aInteropGrabBag))
        pProp->Value >>= aLatentStyles;
    if (!aLatentStyles.hasElements())
        return;

    // Extract default attributes first.
    rtl::Reference<sax_fastparser::FastAttributeList> pAttributeList = FastSerializerHelper::createAttrList();
    uno::Sequence<beans::PropertyValue> aLsdExceptions;
    for (const auto& rLatentStyle : aLatentStyles)
    {
        if (sal_Int32 nToken = DocxStringGetToken(aDefaultTokens, rLatentStyle.Name))
            pAttributeList->add(FSNS(XML_w, nToken), rLatentStyle.Value.get<OUString>());
        else if (rLatentStyle.Name == "lsdExceptions")
            rLatentStyle.Value >>= aLsdExceptions;
    }

    m_pSerializer->startElementNS(XML_w, XML_latentStyles, detachFrom(pAttributeList));

    // Then handle the exceptions.
    for (const auto& rLsdException : aLsdExceptions)
    {
        pAttributeList = FastSerializerHelper::createAttrList();

        uno::Sequence<beans::PropertyValue> aAttributes;
        rLsdException.Value >>= aAttributes;
        for (const auto& rAttribute : aAttributes)
            if (sal_Int32 nToken = DocxStringGetToken(aExceptionTokens, rAttribute.Name))
                pAttributeList->add(FSNS(XML_w, nToken), rAttribute.Value.get<OUString>());

        m_pSerializer->singleElementNS(XML_w, XML_lsdException, detachFrom(pAttributeList));
    }

    m_pSerializer->endElementNS(XML_w, XML_latentStyles);
}

void DocxAttributeOutput::OutputDefaultItem(const SfxPoolItem& rHt)
{
    bool bMustWrite = true;
    switch (rHt.Which())
    {
        case RES_CHRATR_CASEMAP:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_CASEMAP).GetCaseMap() != SvxCaseMap::NotMapped;
            break;
        case RES_CHRATR_COLOR:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_COLOR).GetValue() != COL_AUTO;
            break;
        case RES_CHRATR_CONTOUR:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_CONTOUR).GetValue();
            break;
        case RES_CHRATR_CROSSEDOUT:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_CROSSEDOUT).GetStrikeout() != STRIKEOUT_NONE;
            break;
        case RES_CHRATR_ESCAPEMENT:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_ESCAPEMENT).GetEscapement() != SvxEscapement::Off;
            break;
        case RES_CHRATR_FONT:
            bMustWrite = true;
            break;
        case RES_CHRATR_FONTSIZE:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_FONTSIZE).GetHeight() != 200; // see StyleSheetTable_Impl::StyleSheetTable_Impl() where we set this default
            break;
        case RES_CHRATR_KERNING:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_KERNING).GetValue() != 0;
            break;
        case RES_CHRATR_LANGUAGE:
            bMustWrite = true;
            break;
        case RES_CHRATR_POSTURE:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_POSTURE).GetPosture() != ITALIC_NONE;
            break;
        case RES_CHRATR_SHADOWED:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_SHADOWED).GetValue();
            break;
        case RES_CHRATR_UNDERLINE:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_UNDERLINE).GetLineStyle() != LINESTYLE_NONE;
            break;
        case RES_CHRATR_WEIGHT:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_WEIGHT).GetWeight() != WEIGHT_NORMAL;
            break;
        case RES_CHRATR_AUTOKERN:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_AUTOKERN).GetValue();
            break;
        case RES_CHRATR_BLINK:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_BLINK).GetValue();
            break;
        case RES_CHRATR_BACKGROUND:
            {
                const SvxBrushItem& rBrushItem = rHt.StaticWhichCast(RES_CHRATR_BACKGROUND);
                bMustWrite = (rBrushItem.GetColor() != COL_AUTO ||
                              rBrushItem.GetShadingValue() != ShadingPattern::CLEAR ||
                              rBrushItem.GetGraphicObject() != nullptr);
            }
            break;

        case RES_CHRATR_CJK_FONT:
            bMustWrite = true;
            break;
        case RES_CHRATR_CJK_FONTSIZE:
            bMustWrite = false; // we have written it already as RES_CHRATR_FONTSIZE
            break;
        case RES_CHRATR_CJK_LANGUAGE:
            bMustWrite = true;
            break;
        case RES_CHRATR_CJK_POSTURE:
            bMustWrite = false; // we have written it already as RES_CHRATR_POSTURE
            break;
        case RES_CHRATR_CJK_WEIGHT:
            bMustWrite = false; // we have written it already as RES_CHRATR_WEIGHT
            break;

        case RES_CHRATR_CTL_FONT:
            bMustWrite = true;
            break;
        case RES_CHRATR_CTL_FONTSIZE:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_CTL_FONTSIZE).GetHeight() != 200; // see StyleSheetTable_Impl::StyleSheetTable_Impl() where we set this default
            break;
        case RES_CHRATR_CTL_LANGUAGE:
            bMustWrite = true;
            break;
        case RES_CHRATR_CTL_POSTURE:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_CTL_POSTURE).GetPosture() != ITALIC_NONE;
            break;
        case RES_CHRATR_CTL_WEIGHT:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_CTL_WEIGHT).GetWeight() != WEIGHT_NORMAL;
            break;

        case RES_CHRATR_ROTATE:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_ROTATE).GetValue() != 0_deg10;
            break;
        case RES_CHRATR_EMPHASIS_MARK:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_EMPHASIS_MARK).GetEmphasisMark() != FontEmphasisMark::NONE;
            break;
        case RES_CHRATR_TWO_LINES:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_TWO_LINES).GetValue();
            break;
        case RES_CHRATR_SCALEW:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_SCALEW).GetValue() != 100;
            break;
        case RES_CHRATR_RELIEF:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_RELIEF).GetValue() != FontRelief::NONE;
            break;
        case RES_CHRATR_HIDDEN:
            bMustWrite = rHt.StaticWhichCast(RES_CHRATR_HIDDEN).GetValue();
            break;
        case RES_CHRATR_BOX:
            {
                const SvxBoxItem& rBoxItem = rHt.StaticWhichCast(RES_CHRATR_BOX);
                bMustWrite = rBoxItem.GetTop() || rBoxItem.GetLeft() ||
                             rBoxItem.GetBottom() || rBoxItem.GetRight() ||
                             rBoxItem.GetSmallestDistance();
            }
            break;
        case RES_CHRATR_HIGHLIGHT:
            {
                const SvxBrushItem& rBrushItem = rHt.StaticWhichCast(RES_CHRATR_HIGHLIGHT);
                bMustWrite = (rBrushItem.GetColor() != COL_AUTO ||
                              rBrushItem.GetShadingValue() != ShadingPattern::CLEAR ||
                              rBrushItem.GetGraphicObject() != nullptr);
            }
            break;

        case RES_PARATR_LINESPACING:
            bMustWrite = rHt.StaticWhichCast(RES_PARATR_LINESPACING).GetInterLineSpaceRule() != SvxInterLineSpaceRule::Off;
            break;
        case RES_PARATR_ADJUST:
            bMustWrite = rHt.StaticWhichCast(RES_PARATR_ADJUST).GetAdjust() != SvxAdjust::Left;
            break;
        case RES_PARATR_SPLIT:
            bMustWrite = !rHt.StaticWhichCast(RES_PARATR_SPLIT).GetValue();
            break;
        case RES_PARATR_WIDOWS:
            bMustWrite = rHt.StaticWhichCast(RES_PARATR_WIDOWS).GetValue();
            break;
        case RES_PARATR_TABSTOP:
            bMustWrite = rHt.StaticWhichCast(RES_PARATR_TABSTOP).Count() != 0;
            break;
        case RES_PARATR_HYPHENZONE:
            bMustWrite = true;
            break;
        case RES_PARATR_NUMRULE:
            bMustWrite = !rHt.StaticWhichCast(RES_PARATR_NUMRULE).GetValue().isEmpty();
            break;
        case RES_PARATR_SCRIPTSPACE:
            bMustWrite = !static_cast< const SfxBoolItem& >(rHt).GetValue();
            break;
        case RES_PARATR_HANGINGPUNCTUATION:
            bMustWrite = !static_cast< const SfxBoolItem& >(rHt).GetValue();
            break;
        case RES_PARATR_FORBIDDEN_RULES:
            bMustWrite = !static_cast< const SfxBoolItem& >(rHt).GetValue();
            break;
        case RES_PARATR_VERTALIGN:
            bMustWrite = rHt.StaticWhichCast(RES_PARATR_VERTALIGN).GetValue() != SvxParaVertAlignItem::Align::Automatic;
            break;
        case RES_PARATR_SNAPTOGRID:
            bMustWrite = !rHt.StaticWhichCast(RES_PARATR_SNAPTOGRID).GetValue();
            break;
        case RES_CHRATR_GRABBAG:
            bMustWrite = true;
            break;

        default:
            SAL_INFO("sw.ww8", "Unhandled SfxPoolItem with id " << rHt.Which() );
            break;
    }

    if (bMustWrite)
        OutputItem(rHt);
}

void DocxAttributeOutput::DocDefaults( )
{
    // Write the '<w:docDefaults>' section here
    m_pSerializer->startElementNS(XML_w, XML_docDefaults);

    // Output the default run properties
    m_pSerializer->startElementNS(XML_w, XML_rPrDefault);

    StartStyleProperties(false, 0);

    for (int i = int(RES_CHRATR_BEGIN); i < int(RES_CHRATR_END); ++i)
        OutputDefaultItem(m_rExport.m_rDoc.GetDefault(i));

    EndStyleProperties(false);

    m_pSerializer->endElementNS(XML_w, XML_rPrDefault);

    // Output the default paragraph properties
    m_pSerializer->startElementNS(XML_w, XML_pPrDefault);

    StartStyleProperties(true, 0);

    for (int i = int(RES_PARATR_BEGIN); i < int(RES_PARATR_END); ++i)
        OutputDefaultItem(m_rExport.m_rDoc.GetDefault(i));

    EndStyleProperties(true);

    m_pSerializer->endElementNS(XML_w, XML_pPrDefault);

    m_pSerializer->endElementNS(XML_w, XML_docDefaults);
}

void DocxAttributeOutput::EndStyles( sal_uInt16 nNumberOfStyles )
{
    // HACK
    // Ms Office seems to have an internal limitation of 4091 styles
    // and refuses to load .docx with more, even though the spec seems to allow that;
    // so simply if there are more styles, don't export those
    const sal_Int32 nCountStylesToWrite = MSWORD_MAX_STYLES_LIMIT - nNumberOfStyles;
    m_pTableStyleExport->TableStyles(nCountStylesToWrite);
    m_pSerializer->endElementNS( XML_w, XML_styles );
}

void DocxAttributeOutput::DefaultStyle()
{
    // are these the values of enum ww::sti (see ../inc/wwstyles.hxx)?
    SAL_INFO("sw.ww8", "TODO DocxAttributeOutput::DefaultStyle()");
}

/* Writes <a:srcRect> tag back to document.xml if a file contains a cropped image.
*  NOTE : Tested on images of type JPEG,EMF/WMF,BMP, PNG and GIF.
*/
void DocxAttributeOutput::WriteSrcRect(
    const css::uno::Reference<css::beans::XPropertySet>& xShapePropSet,
    const SwFrameFormat* pFrameFormat)
{
    uno::Reference<graphic::XGraphic> xGraphic;
    xShapePropSet->getPropertyValue(u"Graphic"_ustr) >>= xGraphic;
    const Graphic aGraphic(xGraphic);

    Size aOriginalSize(aGraphic.GetPrefSize());

    const MapMode aMap100mm( MapUnit::Map100thMM );
    const MapMode aMapMode = aGraphic.GetPrefMapMode();
    if (aMapMode.GetMapUnit() == MapUnit::MapPixel)
    {
        aOriginalSize = Application::GetDefaultDevice()->PixelToLogic(aOriginalSize, aMap100mm);
    }

    css::text::GraphicCrop aGraphicCropStruct;
    xShapePropSet->getPropertyValue(u"GraphicCrop"_ustr) >>= aGraphicCropStruct;
    sal_Int32 nCropL = aGraphicCropStruct.Left;
    sal_Int32 nCropR = aGraphicCropStruct.Right;
    sal_Int32 nCropT = aGraphicCropStruct.Top;
    sal_Int32 nCropB = aGraphicCropStruct.Bottom;

    // simulate border padding as a negative crop.
    const SvxBoxItem* pBoxItem;
    if (pFrameFormat && (pBoxItem = pFrameFormat->GetItemIfSet(RES_BOX, false)))
    {
        nCropL -= pBoxItem->GetDistance( SvxBoxItemLine::LEFT );
        nCropR -= pBoxItem->GetDistance( SvxBoxItemLine::RIGHT );
        nCropT -= pBoxItem->GetDistance( SvxBoxItemLine::TOP );
        nCropB -= pBoxItem->GetDistance( SvxBoxItemLine::BOTTOM );
    }

    if (nCropL == 0 && nCropT == 0 && nCropR == 0 && nCropB == 0)
        return;

    double  widthMultiplier  = 100000.0/aOriginalSize.Width();
    double  heightMultiplier = 100000.0/aOriginalSize.Height();

    sal_Int32 left   = static_cast<sal_Int32>(rtl::math::round(nCropL * widthMultiplier));
    sal_Int32 right  = static_cast<sal_Int32>(rtl::math::round(nCropR * widthMultiplier));
    sal_Int32 top    = static_cast<sal_Int32>(rtl::math::round(nCropT * heightMultiplier));
    sal_Int32 bottom = static_cast<sal_Int32>(rtl::math::round(nCropB * heightMultiplier));

    m_pSerializer->singleElementNS( XML_a, XML_srcRect,
         XML_l, OString::number(left),
         XML_t, OString::number(top),
         XML_r, OString::number(right),
         XML_b, OString::number(bottom) );
}

uno::Reference<css::text::XTextFrame> DocxAttributeOutput::GetUnoTextFrame(
    css::uno::Reference<css::drawing::XShape> xShape)
{
    return SwTextBoxHelper::getUnoTextFrame(xShape);
}

static rtl::Reference<::sax_fastparser::FastAttributeList> CreateDocPrAttrList(
    DocxExport & rExport, std::u16string_view const& rName,
    std::u16string_view const& rTitle, std::u16string_view const& rDescription)
{
    rtl::Reference<::sax_fastparser::FastAttributeList> const pAttrs(FastSerializerHelper::createAttrList());
    pAttrs->add(XML_id, OString::number(rExport.GetFilter().GetUniqueId()));
    pAttrs->add(XML_name, rName);
    if (rExport.GetFilter().getVersion() != oox::core::ECMA_376_1ST_EDITION)
    {
        if (!rDescription.empty())
            pAttrs->add(XML_descr, rDescription);
        if (!rTitle.empty())
            pAttrs->add(XML_title, rTitle);
    }
    else
    {   // tdf#148952 no title attribute, merge it into descr
        if (!rTitle.empty() || !rDescription.empty())
        {
            OUString const value(rTitle.empty() ? OUString(rDescription)
                                 : rDescription.empty()
                                     ? OUString(rTitle)
                                     : OUString::Concat(rTitle) + "\n" + rDescription);
            pAttrs->add(XML_descr, value);
        }
    }
    return pAttrs;
}

void DocxAttributeOutput::FlyFrameGraphic( const SwGrfNode* pGrfNode, const Size& rSize, const SwFlyFrameFormat* pOLEFrameFormat, SwOLENode* pOLENode, const SdrObject* pSdrObj )
{
    SAL_INFO("sw.ww8", "TODO DocxAttributeOutput::FlyFrameGraphic( const SwGrfNode* pGrfNode, const Size& rSize, const SwFlyFrameFormat* pOLEFrameFormat, SwOLENode* pOLENode, const SdrObject* pSdrObj  ) - some stuff still missing" );

    GetSdtEndBefore(pSdrObj);

    // detect mis-use of the API
    assert(pGrfNode || (pOLEFrameFormat && pOLENode));
    const SwFrameFormat* pFrameFormat = pGrfNode ? pGrfNode->GetFlyFormat() : pOLEFrameFormat;
    // create the relation ID
    OString aRelId;
    OUString sSvgRelId;
    sal_Int32 nImageType;
    if ( pGrfNode && pGrfNode->IsLinkedFile() )
    {
        // linked image, just create the relation
        OUString aFileName;
        pGrfNode->GetFileFilterNms( &aFileName, nullptr );

        sal_Int32 const nFragment(aFileName.indexOf('#'));
        sal_Int32 const nForbiddenU(aFileName.indexOf("%5C"));
        sal_Int32 const nForbiddenL(aFileName.indexOf("%5c"));
        if (   (nForbiddenU != -1 && (nFragment == -1 || nForbiddenU < nFragment))
            || (nForbiddenL != -1 && (nFragment == -1 || nForbiddenL < nFragment)))
        {
            SAL_WARN("sw.ww8", "DocxAttributeOutput::FlyFrameGraphic: ignoring image with invalid link URL");
            return;
        }

        // TODO Convert the file name to relative for better interoperability

        aRelId = m_rExport.AddRelation(
                    oox::getRelationship(Relationship::IMAGE),
                    aFileName );

        nImageType = XML_link;
    }
    else
    {
        // inline, we also have to write the image itself
        Graphic aGraphic;
        if (pGrfNode)
            aGraphic = pGrfNode->GetGrf();
        else if (const Graphic* pGraphic = pOLENode->GetGraphic())
            aGraphic = *pGraphic;

        m_rDrawingML.SetFS(m_pSerializer); // to be sure that we write to the right stream
        auto pGraphicExport = m_rDrawingML.createGraphicExport();
        OUString aImageId = pGraphicExport->writeToStorage(aGraphic, false);
        aRelId = OUStringToOString(aImageId, RTL_TEXTENCODING_UTF8);

        if (aGraphic.getVectorGraphicData() && aGraphic.getVectorGraphicData()->getType() == VectorGraphicDataType::Svg)
        {
            sSvgRelId = pGraphicExport->writeToStorage(aGraphic, false, drawingml::GraphicExport::TypeHint::SVG);
        }
        nImageType = XML_embed;
    }

    // In case there are any grab-bag items on the graphic frame, emit them now.
    // These are always character grab-bags, as graphics are at-char or as-char in Word.
    if (const SfxGrabBagItem* pGrabBag = pFrameFormat->GetAttrSet().GetItemIfSet(RES_FRMATR_GRABBAG))
    {
        CharGrabBag(*pGrabBag);
    }

    rtl::Reference<sax_fastparser::FastAttributeList> xFrameAttributes(
        FastSerializerHelper::createAttrList());
    if (pGrfNode)
    {
        const SwAttrSet& rSet = pGrfNode->GetSwAttrSet();
        MirrorGraph eMirror = rSet.Get(RES_GRFATR_MIRRORGRF).GetValue();
        if (eMirror == MirrorGraph::Vertical || eMirror == MirrorGraph::Both)
            // Mirror on the vertical axis is a horizontal flip.
            xFrameAttributes->add(XML_flipH, "1");
        // RES_GRFATR_ROTATION is sal_uInt16; use sal_uInt32 for multiplication later
        if (Degree10 nRot = rSet.Get(RES_GRFATR_ROTATION).GetValue())
        {
            // RES_GRFATR_ROTATION is in 10ths of degree; convert to 100ths for macro
            sal_uInt32 mOOXMLRot = oox::drawingml::ExportRotateClockwisify(to<Degree100>(nRot));
            xFrameAttributes->add(XML_rot, OString::number(mOOXMLRot));
        }
    }

    css::uno::Reference<css::beans::XPropertySet> xShapePropSet;
    if (pSdrObj)
    {
        css::uno::Reference<css::drawing::XShape> xShape(
            const_cast<SdrObject*>(pSdrObj)->getUnoShape(), css::uno::UNO_QUERY);
        xShapePropSet.set(xShape, css::uno::UNO_QUERY);
        assert(xShapePropSet);
    }

    Size aSize = rSize;
    // We need the original (cropped, but unrotated) size of object. So prefer the object data,
    // and only use passed frame size as fallback.
    if (xShapePropSet)
    {
        if (css::awt::Size val; xShapePropSet->getPropertyValue(u"Size"_ustr) >>= val)
            aSize = Size(o3tl::toTwips(val.Width, o3tl::Length::mm100), o3tl::toTwips(val.Height, o3tl::Length::mm100));
    }

    m_rExport.SdrExporter().startDMLAnchorInline(pFrameFormat, aSize);

    // picture description (used for pic:cNvPr later too)
    OUString const descr(pGrfNode ? pGrfNode->GetDescription() : pOLEFrameFormat->GetObjDescription());
    OUString const title(pGrfNode ? pGrfNode->GetTitle() : pOLEFrameFormat->GetObjTitle());
    auto const docPrattrList(CreateDocPrAttrList(
        GetExport(), pFrameFormat->GetName().toString(), title, descr));
    m_pSerializer->startElementNS( XML_wp, XML_docPr, docPrattrList );

    OUString sURL, sRelId;
    if (xShapePropSet)
    {
        xShapePropSet->getPropertyValue(u"HyperLinkURL"_ustr) >>= sURL;
        if(!sURL.isEmpty())
        {
            if (sURL.startsWith("#") && sURL.indexOf(' ') != -1 && !sURL.endsWith("|outline") && !sURL.endsWith("|table") &&
                !sURL.endsWith("|frame") && !sURL.endsWith("|graphic") && !sURL.endsWith("|ole") && !sURL.endsWith("|region"))
            {
                // Spaces are prohibited in bookmark name.
                sURL = sURL.replace(' ', '_');
            }
            sRelId = GetExport().GetFilter().addRelation( m_pSerializer->getOutputStream(),
                        oox::getRelationship(Relationship::HYPERLINK),
                        sURL, !sURL.startsWith("#") );
            m_pSerializer->singleElementNS( XML_a, XML_hlinkClick,
                FSNS( XML_xmlns, XML_a ), "http://schemas.openxmlformats.org/drawingml/2006/main",
                FSNS( XML_r, XML_id ), sRelId);
        }
        AddExtLst(m_pSerializer, GetExport(), xShapePropSet);
    }

    m_pSerializer->endElementNS( XML_wp, XML_docPr );

    m_pSerializer->startElementNS(XML_wp, XML_cNvGraphicFramePr);
    // TODO change aspect?
    m_pSerializer->singleElementNS( XML_a, XML_graphicFrameLocks,
            FSNS( XML_xmlns, XML_a ), GetExport().GetFilter().getNamespaceURL(OOX_NS(dml)),
            XML_noChangeAspect, "1" );
    m_pSerializer->endElementNS( XML_wp, XML_cNvGraphicFramePr );

    m_pSerializer->startElementNS( XML_a, XML_graphic,
            FSNS( XML_xmlns, XML_a ), GetExport().GetFilter().getNamespaceURL(OOX_NS(dml)) );
    m_pSerializer->startElementNS( XML_a, XML_graphicData,
            XML_uri, "http://schemas.openxmlformats.org/drawingml/2006/picture" );

    m_pSerializer->startElementNS( XML_pic, XML_pic,
            FSNS( XML_xmlns, XML_pic ), GetExport().GetFilter().getNamespaceURL(OOX_NS(dmlPicture)) );

    m_pSerializer->startElementNS(XML_pic, XML_nvPicPr);
    // It seems pic:cNvpr and wp:docPr are pretty much the same thing with the same attributes
    m_pSerializer->startElementNS(XML_pic, XML_cNvPr, docPrattrList);

    if(!sURL.isEmpty())
        m_pSerializer->singleElementNS(XML_a, XML_hlinkClick, FSNS(XML_r, XML_id), sRelId);

    m_pSerializer->endElementNS( XML_pic, XML_cNvPr );

    m_pSerializer->startElementNS(XML_pic, XML_cNvPicPr);
    // TODO change aspect?
    m_pSerializer->singleElementNS( XML_a, XML_picLocks,
            XML_noChangeAspect, "1", XML_noChangeArrowheads, "1" );
    m_pSerializer->endElementNS( XML_pic, XML_cNvPicPr );
    m_pSerializer->endElementNS( XML_pic, XML_nvPicPr );

    // the actual picture
    m_pSerializer->startElementNS(XML_pic, XML_blipFill);

/* At this point we are certain that, WriteImage returns empty RelId
   for unhandled graphic type. Therefore we write the picture description
   and not the relation( coz there ain't any), so that the user knows
   there is an image/graphic in the doc but it is broken instead of
   completely discarding it.
*/
    if ( aRelId.isEmpty() )
        m_pSerializer->startElementNS(XML_a, XML_blip);
    else
        m_pSerializer->startElementNS(XML_a, XML_blip, FSNS(XML_r, nImageType), aRelId);

    const SwDrawModeGrf* pGrafModeItem = nullptr;
    if ( pGrfNode && (pGrafModeItem = pGrfNode->GetSwAttrSet().GetItemIfSet(RES_GRFATR_DRAWMODE)))
    {
        GraphicDrawMode nMode = pGrafModeItem->GetValue();
        if (nMode == GraphicDrawMode::Greys)
            m_pSerializer->singleElementNS (XML_a, XML_grayscl);
        else if (nMode == GraphicDrawMode::Mono) //black/white has a 0,5 threshold in LibreOffice
            m_pSerializer->singleElementNS (XML_a, XML_biLevel, XML_thresh, OString::number(50000));
        else if (nMode == GraphicDrawMode::Watermark) //watermark has a brightness/luminance of 0,5 and contrast of -0.7 in LibreOffice
            m_pSerializer->singleElementNS( XML_a, XML_lum, XML_bright, OString::number(70000), XML_contrast, OString::number(-70000) );
    }

    if (!sSvgRelId.isEmpty())
    {
        auto pGraphicExport = m_rDrawingML.createGraphicExport();
        pGraphicExport->writeSvgExtension(sSvgRelId);
    }

    m_pSerializer->endElementNS( XML_a, XML_blip );

    if (xShapePropSet)
        WriteSrcRect(xShapePropSet, pFrameFormat);

    m_pSerializer->startElementNS(XML_a, XML_stretch);
    m_pSerializer->singleElementNS(XML_a, XML_fillRect);
    m_pSerializer->endElementNS( XML_a, XML_stretch );
    m_pSerializer->endElementNS( XML_pic, XML_blipFill );

    // TODO setup the right values below
    m_pSerializer->startElementNS(XML_pic, XML_spPr, XML_bwMode, "auto");

    m_pSerializer->startElementNS(XML_a, XML_xfrm, xFrameAttributes);

    m_pSerializer->singleElementNS(XML_a, XML_off, XML_x, "0", XML_y, "0");
    OString aWidth( OString::number( TwipsToEMU( aSize.Width() ) ) );
    OString aHeight( OString::number( TwipsToEMU( aSize.Height() ) ) );
    m_pSerializer->singleElementNS(XML_a, XML_ext, XML_cx, aWidth, XML_cy, aHeight);
    m_pSerializer->endElementNS( XML_a, XML_xfrm );
    m_pSerializer->startElementNS(XML_a, XML_prstGeom, XML_prst, "rect");
    m_pSerializer->singleElementNS(XML_a, XML_avLst);
    m_pSerializer->endElementNS( XML_a, XML_prstGeom );

    m_rDrawingML.SetFS(m_pSerializer); // to be sure that we write to the right stream
    if (xShapePropSet)
        m_rDrawingML.WriteFill(xShapePropSet, awt::Size(aSize.Width(), aSize.Height()));

    const SvxBoxItem& rBoxItem = pFrameFormat->GetBox();
    const SvxBorderLine* pLeft = rBoxItem.GetLine(SvxBoxItemLine::LEFT);
    const SvxBorderLine* pRight = rBoxItem.GetLine(SvxBoxItemLine::RIGHT);
    const SvxBorderLine* pTop = rBoxItem.GetLine(SvxBoxItemLine::TOP);
    const SvxBorderLine* pBottom = rBoxItem.GetLine(SvxBoxItemLine::BOTTOM);
    if (pLeft || pRight || pTop || pBottom)
        m_rExport.SdrExporter().writeBoxItemLine(rBoxItem);

    m_rExport.SdrExporter().writeDMLEffectLst(*pFrameFormat);

    m_pSerializer->endElementNS( XML_pic, XML_spPr );

    m_pSerializer->endElementNS( XML_pic, XML_pic );

    m_pSerializer->endElementNS( XML_a, XML_graphicData );
    m_pSerializer->endElementNS( XML_a, XML_graphic );
    m_rExport.SdrExporter().endDMLAnchorInline(pFrameFormat);
}

void DocxAttributeOutput::WriteOLE2Obj( const SdrObject* pSdrObj, SwOLENode& rOLENode, const Size& rSize, const SwFlyFrameFormat* pFlyFrameFormat, const sal_Int8 nFormulaAlignment )
{
    if( WriteOLEChart( pSdrObj, rSize, pFlyFrameFormat ))
        return;
    if( WriteOLEMath( rOLENode , nFormulaAlignment))
        return;
    PostponeOLE( rOLENode, rSize, pFlyFrameFormat );
}

bool DocxAttributeOutput::WriteOLEChart( const SdrObject* pSdrObj, const Size& rSize, const SwFlyFrameFormat* pFlyFrameFormat )
{
    uno::Reference< drawing::XShape > xShape( const_cast<SdrObject*>(pSdrObj)->getUnoShape(), uno::UNO_QUERY );
    if (!xShape.is())
        return false;

    uno::Reference<beans::XPropertySet> const xPropSet(xShape, uno::UNO_QUERY);
    if (!xPropSet.is())
        return false;

    OUString clsid; // why is the property of type string, not sequence<byte>?
    xPropSet->getPropertyValue(u"CLSID"_ustr) >>= clsid;
    assert(!clsid.isEmpty());
    SvGlobalName aClassID;
    bool const isValid(aClassID.MakeId(clsid));
    assert(isValid); (void)isValid;

    if (!SotExchange::IsChart(aClassID))
        return false;

    m_aPostponedCharts.push_back(PostponedChart(pSdrObj, rSize, pFlyFrameFormat));
    return true;
}

/*
 * Write chart hierarchy in w:drawing after end element of w:rPr tag.
 */
void DocxAttributeOutput::WritePostponedChart()
{
    if (m_aPostponedCharts.empty())
        return;

    for (const PostponedChart& rChart : m_aPostponedCharts)
    {
        uno::Reference< chart2::XChartDocument > xChartDoc;
        uno::Reference< drawing::XShape > xShape(const_cast<SdrObject*>(rChart.object)->getUnoShape(), uno::UNO_QUERY );
        if( xShape.is() )
        {
            uno::Reference< beans::XPropertySet > xPropSet( xShape, uno::UNO_QUERY );
            if( xPropSet.is() )
                xChartDoc.set( xPropSet->getPropertyValue( u"Model"_ustr ), uno::UNO_QUERY );
        }

        if( xChartDoc.is() )
        {
            SAL_INFO("sw.ww8", "DocxAttributeOutput::WriteOLE2Obj: export chart ");

            m_rExport.SdrExporter().startDMLAnchorInline(rChart.frame, rChart.size);

            OUString sName(u"Object 1"_ustr);
            uno::Reference< container::XNamed > xNamed( xShape, uno::UNO_QUERY );
            if( xNamed.is() )
                sName = xNamed->getName();

            // tdf#153203  export a11y related properties
            uno::Reference<beans::XPropertySet> xShapeProps(xShape, uno::UNO_QUERY);
            OUString const title(xShapeProps->getPropertyValue(u"Title"_ustr).get<OUString>());
            OUString const descr(xShapeProps->getPropertyValue(u"Description"_ustr).get<OUString>());

            /* If there is a scenario where a chart is followed by a shape
               which is being exported as an alternate content then, the
               docPr Id is being repeated, ECMA 20.4.2.5 says that the
               docPr Id should be unique, ensuring the same here.
               */
            auto const docPrattrList(CreateDocPrAttrList(
                GetExport(), sName, title, descr));
            m_pSerializer->singleElementNS(XML_wp, XML_docPr, docPrattrList);

            m_pSerializer->singleElementNS(XML_wp, XML_cNvGraphicFramePr);

            m_pSerializer->startElementNS( XML_a, XML_graphic,
                    FSNS( XML_xmlns, XML_a ), GetExport().GetFilter().getNamespaceURL(OOX_NS(dml)) );

            m_pSerializer->startElementNS( XML_a, XML_graphicData,
                    XML_uri, "http://schemas.openxmlformats.org/drawingml/2006/chart" );

            OString aRelId;
            m_nChartCount++;
            aRelId = m_rExport.OutputChart( xChartDoc, m_nChartCount, m_pSerializer );

            m_pSerializer->singleElementNS( XML_c, XML_chart,
                    FSNS( XML_xmlns, XML_c ), GetExport().GetFilter().getNamespaceURL(OOX_NS(dmlChart)),
                    FSNS( XML_xmlns, XML_r ), GetExport().GetFilter().getNamespaceURL(OOX_NS(officeRel)),
                    FSNS( XML_r, XML_id ), aRelId );

            m_pSerializer->endElementNS( XML_a, XML_graphicData );
            m_pSerializer->endElementNS( XML_a, XML_graphic );

            m_rExport.SdrExporter().endDMLAnchorInline(rChart.frame);
        }
    }

    m_aPostponedCharts.clear();
}

bool DocxAttributeOutput::WriteOLEMath( const SwOLENode& rOLENode ,const sal_Int8 nAlign)
{
    uno::Reference < embed::XEmbeddedObject > xObj(const_cast<SwOLENode&>(rOLENode).GetOLEObj().GetOleRef());
    SvGlobalName aObjName(xObj->getClassID());

    if( !SotExchange::IsMath(aObjName) )
        return false;

    try
    {
        PostponedMathObjects aPostponedMathObject;
        aPostponedMathObject.pMathObject = const_cast<SwOLENode*>( &rOLENode);
        aPostponedMathObject.nMathObjAlignment = nAlign;
        m_aPostponedMaths.push_back(aPostponedMathObject);
    }
    catch (const uno::Exception&)
    {
    }
    return true;
}

void DocxAttributeOutput::WritePostponedMath(const SwOLENode* pPostponedMath, sal_Int8 nAlign)
{
    uno::Reference < embed::XEmbeddedObject > xObj(const_cast<SwOLENode*>(pPostponedMath)->GetOLEObj().GetOleRef());
    if (embed::EmbedStates::LOADED == xObj->getCurrentState())
    {
        // must be running so there is a Component
        try
        {
            xObj->changeState(embed::EmbedStates::RUNNING);
        }
        catch (const uno::Exception&)
        {
        }
    }
    uno::Reference< uno::XInterface > xInterface( xObj->getComponent(), uno::UNO_QUERY );
    if( oox::FormulaImExportBase* formulaexport = dynamic_cast< oox::FormulaImExportBase* >( xInterface.get()))
        formulaexport->writeFormulaOoxml( m_pSerializer, GetExport().GetFilter().getVersion(),
                oox::drawingml::DOCUMENT_DOCX, nAlign);
    else
        OSL_FAIL( "Math OLE object cannot write out OOXML" );
}

void DocxAttributeOutput::WritePostponedFormControl(const SdrObject* pObject)
{
    if (!pObject || pObject->GetObjInventor() != SdrInventor::FmForm)
        return;

    SdrUnoObj *pFormObj = const_cast<SdrUnoObj*>(dynamic_cast< const SdrUnoObj*>(pObject));
    if (!pFormObj)
        return;

    uno::Reference<awt::XControlModel> xControlModel = pFormObj->GetUnoControlModel();
    uno::Reference<lang::XServiceInfo> xInfo(xControlModel, uno::UNO_QUERY);
    if (!xInfo.is())
        return;

    if (xInfo->supportsService(u"com.sun.star.form.component.DateField"_ustr))
    {
        // gather component properties

        OUString sDateFormat;
        uno::Reference<beans::XPropertySet> xPropertySet(xControlModel, uno::UNO_QUERY);

        OString sDate;
        OUString aContentText;
        bool bHasDate = false;
        css::util::Date aUNODate;
        if (xPropertySet->getPropertyValue(u"Date"_ustr) >>= aUNODate)
        {
            bHasDate = true;
            Date aDate(aUNODate.Day, aUNODate.Month, aUNODate.Year);
            sDate = DateToOString(aDate);
            aContentText = DateToDDMMYYYYOUString(aDate);
            sDateFormat = "dd/MM/yyyy";
        }
        else
        {
            aContentText = xPropertySet->getPropertyValue(u"HelpText"_ustr).get<OUString>();
            if(sDateFormat.isEmpty())
                sDateFormat = "dd/MM/yyyy"; // Need to set date format even if there is no date set
        }

        // output component

        m_pSerializer->startElementNS(XML_w, XML_sdt);
        m_pSerializer->startElementNS(XML_w, XML_sdtPr);

        if (bHasDate)
            m_pSerializer->startElementNS(XML_w, XML_date, FSNS(XML_w, XML_fullDate), sDate);
        else
            m_pSerializer->startElementNS(XML_w, XML_date);

        m_pSerializer->singleElementNS(XML_w, XML_dateFormat, FSNS(XML_w, XML_val), sDateFormat);
        m_pSerializer->singleElementNS(XML_w, XML_lid,
                                       FSNS(XML_w, XML_val), "en-US");
        m_pSerializer->singleElementNS(XML_w, XML_storeMappedDataAs,
                                       FSNS(XML_w, XML_val), "dateTime");
        m_pSerializer->singleElementNS(XML_w, XML_calendar,
                                       FSNS(XML_w, XML_val), "gregorian");

        m_pSerializer->endElementNS(XML_w, XML_date);
        m_pSerializer->endElementNS(XML_w, XML_sdtPr);

        m_pSerializer->startElementNS(XML_w, XML_sdtContent);
        m_pSerializer->startElementNS(XML_w, XML_r);

        RunText(aContentText);
        m_pSerializer->endElementNS(XML_w, XML_r);
        m_pSerializer->endElementNS(XML_w, XML_sdtContent);

        m_pSerializer->endElementNS(XML_w, XML_sdt);
    }
    else if (xInfo->supportsService(u"com.sun.star.form.component.ComboBox"_ustr))
    {
        // gather component properties

        uno::Reference<beans::XPropertySet> xPropertySet(xControlModel, uno::UNO_QUERY);
        OUString sText = xPropertySet->getPropertyValue(u"Text"_ustr).get<OUString>();
        const uno::Sequence<OUString> aItems = xPropertySet->getPropertyValue(u"StringItemList"_ustr).get< uno::Sequence<OUString> >();

        // output component

        m_pSerializer->startElementNS(XML_w, XML_sdt);
        m_pSerializer->startElementNS(XML_w, XML_sdtPr);

        m_pSerializer->startElementNS(XML_w, XML_dropDownList);

        for (const auto& rItem : aItems)
        {
            m_pSerializer->singleElementNS(XML_w, XML_listItem,
                                           FSNS(XML_w, XML_displayText), rItem,
                                           FSNS(XML_w, XML_value), rItem);
        }

        m_pSerializer->endElementNS(XML_w, XML_dropDownList);
        m_pSerializer->endElementNS(XML_w, XML_sdtPr);

        m_pSerializer->startElementNS(XML_w, XML_sdtContent);
        m_pSerializer->startElementNS(XML_w, XML_r);
        RunText(sText);
        m_pSerializer->endElementNS(XML_w, XML_r);
        m_pSerializer->endElementNS(XML_w, XML_sdtContent);

        m_pSerializer->endElementNS(XML_w, XML_sdt);
    }
}

void DocxAttributeOutput::WritePostponedActiveXControl(bool bInsideRun)
{
    for( const auto & rPostponedDrawing : m_aPostponedActiveXControls )
    {
        WriteActiveXControl(rPostponedDrawing.object, *rPostponedDrawing.frame, bInsideRun);
    }
    m_aPostponedActiveXControls.clear();
}


void DocxAttributeOutput::WriteActiveXControl(const SdrObject* pObject, const SwFrameFormat& rFrameFormat, bool bInsideRun)
{
    SdrUnoObj *pFormObj = const_cast<SdrUnoObj*>(dynamic_cast< const SdrUnoObj*>(pObject));
    if (!pFormObj)
        return;

    uno::Reference<awt::XControlModel> xControlModel = pFormObj->GetUnoControlModel();
    if (!xControlModel.is())
        return;

    const bool bAnchoredInline = rFrameFormat.GetAnchor().GetAnchorId() == static_cast<RndStdIds>(css::text::TextContentAnchorType_AS_CHARACTER);

    if(!bInsideRun)
    {
        m_pSerializer->startElementNS(XML_w, XML_r);
    }

    // w:pict for floating embedded control and w:object for inline embedded control
    if(bAnchoredInline)
        m_pSerializer->startElementNS(XML_w, XML_object);
    else
        m_pSerializer->startElementNS(XML_w, XML_pict);

    // write ActiveX fragment and ActiveX binary
    uno::Reference<drawing::XShape> xShape(const_cast<SdrObject*>(pObject)->getUnoShape(), uno::UNO_QUERY);
    std::pair<OString,OString> sRelIdAndName = m_rExport.WriteActiveXObject(xShape, xControlModel);

    // VML shape definition
    m_rExport.VMLExporter().SetSkipwzName(true);
    m_rExport.VMLExporter().SetHashMarkForType(true);
    m_rExport.VMLExporter().OverrideShapeIDGen(true, "control_shape_"_ostr);
    OString sShapeId;
    if(bAnchoredInline)
    {
        sShapeId = m_rExport.VMLExporter().AddInlineSdrObject(*pObject, true);
    }
    else
    {
        SwFormatFollowTextFlow const& rFlow(rFrameFormat.GetFollowTextFlow());
        const SwFormatHoriOrient& rHoriOri = rFrameFormat.GetHoriOrient();
        const SwFormatVertOrient& rVertOri = rFrameFormat.GetVertOrient();
        SwFormatSurround const& rSurround(rFrameFormat.GetSurround());
        rtl::Reference<sax_fastparser::FastAttributeList> pAttrList(docx::SurroundToVMLWrap(rSurround));
        sShapeId = m_rExport.VMLExporter().AddSdrObject(*pObject,
            rFlow.GetValue(),
            rHoriOri.GetHoriOrient(), rVertOri.GetVertOrient(),
            rHoriOri.GetRelationOrient(),
            rVertOri.GetRelationOrient(),
            pAttrList.get(),
            true);
    }
    // Restore default values
    m_rExport.VMLExporter().SetSkipwzName(false);
    m_rExport.VMLExporter().SetHashMarkForType(false);
    m_rExport.VMLExporter().OverrideShapeIDGen(false);

    // control
    m_pSerializer->singleElementNS(XML_w, XML_control,
                                    FSNS(XML_r, XML_id), sRelIdAndName.first,
                                    FSNS(XML_w, XML_name), sRelIdAndName.second,
                                    FSNS(XML_w, XML_shapeid), sShapeId);

    if(bAnchoredInline)
        m_pSerializer->endElementNS(XML_w, XML_object);
    else
        m_pSerializer->endElementNS(XML_w, XML_pict);

    if(!bInsideRun)
    {
        m_pSerializer->endElementNS(XML_w, XML_r);
    }
}

bool DocxAttributeOutput::ExportAsActiveXControl(const SdrObject* pObject) const
{
    SdrUnoObj *pFormObj = const_cast<SdrUnoObj*>(dynamic_cast< const SdrUnoObj*>(pObject));
    if (!pFormObj)
        return false;

    uno::Reference<awt::XControlModel> xControlModel = pFormObj->GetUnoControlModel();
    if (!xControlModel.is())
        return false;

    SwDocShell* pShell = m_rExport.m_rDoc.GetDocShell();
    uno::Reference< css::frame::XModel > xModel( pShell ? pShell->GetModel() : nullptr );
    if (!xModel.is())
        return false;

    uno::Reference<lang::XServiceInfo> xInfo(xControlModel, uno::UNO_QUERY);
    if (!xInfo.is())
        return false;

    // See WritePostponedFormControl
    // By now date field and combobox is handled on a different way, so let's not interfere with the other method.
    if(xInfo->supportsService(u"com.sun.star.form.component.DateField"_ustr) ||
       xInfo->supportsService(u"com.sun.star.form.component.ComboBox"_ustr))
        return false;

    oox::ole::OleFormCtrlExportHelper exportHelper(comphelper::getProcessComponentContext(), xModel, xControlModel);
    return exportHelper.isValid();
}

void DocxAttributeOutput::PostponeOLE( SwOLENode& rNode, const Size& rSize, const SwFlyFrameFormat* pFlyFrameFormat )
{
    if( !m_oPostponedOLEs )
        //cannot be postponed, try to write now
        WriteOLE( rNode, rSize, pFlyFrameFormat );
    else
        m_oPostponedOLEs->push_back( PostponedOLE( &rNode, rSize, pFlyFrameFormat ) );
}

/*
 * Write w:object hierarchy for embedded objects after end element of w:rPr tag.
 */
void DocxAttributeOutput::WritePostponedOLE()
{
    if( !m_oPostponedOLEs )
        return;

    for( const auto & rPostponedOLE : *m_oPostponedOLEs )
    {
        WriteOLE( *rPostponedOLE.object, rPostponedOLE.size, rPostponedOLE.frame );
    }

    // clear list of postponed objects
    m_oPostponedOLEs.reset();
}

void DocxAttributeOutput::WriteOLE( SwOLENode& rNode, const Size& rSize, const SwFlyFrameFormat* pFlyFrameFormat )
{
    OSL_ASSERT(pFlyFrameFormat);

    // get interoperability information about embedded objects
    uno::Sequence< beans::PropertyValue > aGrabBag, aObjectsInteropList,aObjectInteropAttributes;
    m_rExport.m_xTextDoc->getPropertyValue( UNO_NAME_MISC_OBJ_INTEROPGRABBAG ) >>= aGrabBag;
    auto pProp = std::find_if(std::cbegin(aGrabBag), std::cend(aGrabBag),
        [](const beans::PropertyValue& rProp) { return rProp.Name == "EmbeddedObjects"; });
    if (pProp != std::cend(aGrabBag))
        pProp->Value >>= aObjectsInteropList;

    SwOLEObj& aObject = rNode.GetOLEObj();
    uno::Reference < embed::XEmbeddedObject > xObj( aObject.GetOleRef() );
    comphelper::EmbeddedObjectContainer* aContainer = aObject.GetObject().GetContainer();
    OUString sObjectName = aContainer->GetEmbeddedObjectName( xObj );

    // set some attributes according to the type of the embedded object
    OUString sProgID, sDrawAspect;
    switch (rNode.GetAspect())
    {
        case embed::Aspects::MSOLE_CONTENT: sDrawAspect = "Content"; break;
        case embed::Aspects::MSOLE_DOCPRINT: sDrawAspect = "DocPrint"; break;
        case embed::Aspects::MSOLE_ICON: sDrawAspect = "Icon"; break;
        case embed::Aspects::MSOLE_THUMBNAIL: sDrawAspect = "Thumbnail"; break;
        default:
            SAL_WARN("sw.ww8", "DocxAttributeOutput::WriteOLE: invalid aspect value");
    }
    auto pObjectsInterop = std::find_if(std::cbegin(aObjectsInteropList), std::cend(aObjectsInteropList),
        [&sObjectName](const beans::PropertyValue& rProp) { return rProp.Name == sObjectName; });
    if (pObjectsInterop != std::cend(aObjectsInteropList))
        pObjectsInterop->Value >>= aObjectInteropAttributes;

    for (const auto& rObjectInteropAttribute : aObjectInteropAttributes)
    {
        if ( rObjectInteropAttribute.Name == "ProgID" )
        {
            rObjectInteropAttribute.Value >>= sProgID;
        }
    }

    // write embedded file
    OString sId = m_rExport.WriteOLEObject(aObject, sProgID);

    if( sId.isEmpty() )
    {
        // the embedded file could not be saved
        // fallback: save as an image
        FlyFrameGraphic( nullptr, rSize, pFlyFrameFormat, &rNode );
        return;
    }

    // write preview image
    const Graphic* pGraphic = rNode.GetGraphic();
    Graphic aGraphic = pGraphic ? *pGraphic : Graphic();
    m_rDrawingML.SetFS(m_pSerializer);
    OUString sImageId = m_rDrawingML.writeGraphicToStorage(aGraphic);

    if ( sDrawAspect == "Content" )
    {
        try
        {
            awt::Size aSize = xObj->getVisualAreaSize( rNode.GetAspect() );

            MapUnit aUnit = VCLUnoHelper::UnoEmbed2VCLMapUnit( xObj->getMapUnit( rNode.GetAspect() ) );
            Size aOriginalSize( OutputDevice::LogicToLogic(Size( aSize.Width, aSize.Height),
                                                MapMode(aUnit), MapMode(MapUnit::MapTwip)));

            m_pSerializer->startElementNS( XML_w, XML_object,
                                   FSNS(XML_w, XML_dxaOrig), OString::number(aOriginalSize.Width()),
                                   FSNS(XML_w, XML_dyaOrig), OString::number(aOriginalSize.Height()) );
        }
        catch ( uno::Exception& )
        {
            m_pSerializer->startElementNS(XML_w, XML_object);
        }
    }
    else
    {
        m_pSerializer->startElementNS(XML_w, XML_object);
    }

    OString sShapeId = "ole_" + sId;

    //OLE Shape definition
    WriteOLEShape(*pFlyFrameFormat, rSize, sShapeId, sImageId);

    //OLE Object definition
    m_pSerializer->singleElementNS(XML_o, XML_OLEObject,
                                   XML_Type, "Embed",
                                   XML_ProgID, sProgID,
                                   XML_ShapeID, sShapeId,
                                   XML_DrawAspect, sDrawAspect,
                                   XML_ObjectID, "_" + OString::number(comphelper::rng::uniform_int_distribution(0, std::numeric_limits<int>::max())),
                                   FSNS( XML_r, XML_id ), sId );

    m_pSerializer->endElementNS(XML_w, XML_object);
}

void DocxAttributeOutput::WriteOLEShape(const SwFlyFrameFormat& rFrameFormat, const Size& rSize,
                                        std::string_view rShapeId, const OUString& rImageId)
{
    assert(m_pSerializer);

    //Here is an attribute list where we collect the attributes what we want to export
    rtl::Reference<FastAttributeList> pAttr = FastSerializerHelper::createAttrList();
    pAttr->add(XML_id, rShapeId);

    //export the fixed shape type for picture frame
    m_pSerializer->write(vml::VMLExport::GetVMLShapeTypeDefinition(rShapeId, true));
    pAttr->add(XML_type, OString::Concat("_x0000_t") + rShapeId);

    //Export the style attribute for position and size
    pAttr->add(XML_style, GetOLEStyle(rFrameFormat, rSize));
    //Get the OLE frame
    const SvxBoxItem& rBox = rFrameFormat.GetAttrSet().GetBox();
    OString sLineType;
    OString sDashType;
    //Word does not handle differently the four sides,
    //so we have to choose, and the left one is the winner:
    if (rBox.GetLeft())
    {
        //Get the left border color and width
        const Color aLineColor = rBox.GetLeft()->GetColor();
        const tools::Long aLineWidth = rBox.GetLeft()->GetWidth();

        //Convert the left OLE border style to OOXML
        //FIXME improve if it's necessary
        switch (rBox.GetLeft()->GetBorderLineStyle())
        {
            case SvxBorderLineStyle::SOLID:
                sLineType = "Single"_ostr;
                sDashType = "Solid"_ostr;
                break;
            case SvxBorderLineStyle::DASHED:
                sLineType = "Single"_ostr;
                sDashType = "Dash"_ostr;
                break;
            case SvxBorderLineStyle::DASH_DOT:
                sLineType = "Single"_ostr;
                sDashType = "DashDot"_ostr;
                break;
            case SvxBorderLineStyle::DASH_DOT_DOT:
                sLineType = "Single"_ostr;
                sDashType = "ShortDashDotDot"_ostr;
                break;
            case SvxBorderLineStyle::DOTTED:
                sLineType = "Single"_ostr;
                sDashType = "Dot"_ostr;
                break;
            case SvxBorderLineStyle::DOUBLE:
                sLineType = "ThinThin"_ostr;
                sDashType = "Solid"_ostr;
                break;
            case SvxBorderLineStyle::DOUBLE_THIN:
                sLineType = "ThinThin"_ostr;
                sDashType = "Solid"_ostr;
                break;
            case SvxBorderLineStyle::EMBOSSED:
                sLineType = "Single"_ostr;
                sDashType = "Solid"_ostr;
                break;
            case SvxBorderLineStyle::ENGRAVED:
                sLineType = "Single"_ostr;
                sDashType = "Solid"_ostr;
                break;
            case SvxBorderLineStyle::FINE_DASHED:
                sLineType = "Single"_ostr;
                sDashType = "Dot"_ostr;
                break;
            case SvxBorderLineStyle::INSET:
                sLineType = "Single"_ostr;
                sDashType = "Solid"_ostr;
                break;
            case SvxBorderLineStyle::OUTSET:
                sLineType = "Single"_ostr;
                sDashType = "Solid"_ostr;
                break;
            case SvxBorderLineStyle::THICKTHIN_LARGEGAP:
            case SvxBorderLineStyle::THICKTHIN_MEDIUMGAP:
            case SvxBorderLineStyle::THICKTHIN_SMALLGAP:
                sLineType = "ThickThin"_ostr;
                sDashType = "Solid"_ostr;
                break;
            case SvxBorderLineStyle::THINTHICK_LARGEGAP:
            case SvxBorderLineStyle::THINTHICK_MEDIUMGAP:
            case SvxBorderLineStyle::THINTHICK_SMALLGAP:
                sLineType = "ThinThick"_ostr;
                sDashType = "Solid"_ostr;
                break;
            case SvxBorderLineStyle::NONE:
                sLineType = ""_ostr;
                sDashType = ""_ostr;
                break;
            default:
                SAL_WARN("sw.ww8", "Unknown line type on OOXML ELE export!");
                break;
        }

        //If there is a line add it for export
        if (!sLineType.isEmpty() && !sDashType.isEmpty())
        {
            pAttr->add(XML_stroked, "t");
            pAttr->add(XML_strokecolor, "#" + msfilter::util::ConvertColor(aLineColor));
            pAttr->add(XML_strokeweight, OString::number(aLineWidth / 20) + "pt");
        }
    }

    //Let's check the filltype of the OLE
    switch (rFrameFormat.GetAttrSet().Get(XATTR_FILLSTYLE).GetValue())
    {
        case drawing::FillStyle::FillStyle_SOLID:
        {
            //If solid, we get the color and add it to the exporter
            const Color rShapeColor = rFrameFormat.GetAttrSet().Get(XATTR_FILLCOLOR).GetColorValue();
            pAttr->add(XML_filled, "t");
            pAttr->add(XML_fillcolor, "#" + msfilter::util::ConvertColor(rShapeColor));
            break;
        }
        case drawing::FillStyle::FillStyle_GRADIENT:
        case drawing::FillStyle::FillStyle_HATCH:
        case drawing::FillStyle::FillStyle_BITMAP:
            //TODO
            break;
        case drawing::FillStyle::FillStyle_NONE:
        {
            pAttr->add(XML_filled, "f");
            break;
        }
        default:
            SAL_WARN("sw.ww8", "Unknown fill type on OOXML OLE export!");
            break;
    }
    pAttr->addNS(XML_o, XML_ole, ""); //compulsory, even if it's empty
    m_pSerializer->startElementNS(XML_v, XML_shape, pAttr);//Write the collected attrs...

    if (!sLineType.isEmpty() && !sDashType.isEmpty()) //If there is a line/dash style it is time to export it
    {
        m_pSerializer->singleElementNS(XML_v, XML_stroke, XML_linestyle, sLineType, XML_dashstyle, sDashType);
    }

    // shape filled with the preview image
    m_pSerializer->singleElementNS(XML_v, XML_imagedata,
                                   FSNS(XML_r, XML_id), rImageId,
                                   FSNS(XML_o, XML_title), "");

    //export wrap settings
    if (rFrameFormat.GetAnchor().GetAnchorId() != RndStdIds::FLY_AS_CHAR) //As-char objs does not have surround.
        ExportOLESurround(rFrameFormat.GetSurround());

    m_pSerializer->endElementNS(XML_v, XML_shape);
}

OString DocxAttributeOutput::GetOLEStyle(const SwFlyFrameFormat& rFormat, const Size& rSize)
{
    //tdf#131539: Export OLE positions in docx:
    //This string will store the position output for the xml
    OString aPos;
    //This string will store the relative position for aPos
    OString aAnch;

    if (rFormat.GetAnchor().GetAnchorId() != RndStdIds::FLY_AS_CHAR)
    {
        //Get the horizontal alignment of the OLE via the frame format, to aHAlign
        OString aHAlign = convertToOOXMLHoriOrient(rFormat.GetHoriOrient().GetHoriOrient(),
            rFormat.GetHoriOrient().IsPosToggle());
        //Get the vertical alignment of the OLE via the frame format to aVAlign
        OString aVAlign = convertToOOXMLVertOrient(rFormat.GetVertOrient().GetVertOrient());

        // Check if the OLE anchored to page:
        const bool bIsPageAnchor = rFormat.GetAnchor().GetAnchorId() == RndStdIds::FLY_AT_PAGE;

        //Get the relative horizontal positions for the anchors
        OString aHAnch
            = bIsPageAnchor
                  ? "page"_ostr
                  : convertToOOXMLHoriOrientRel(rFormat.GetHoriOrient().GetRelationOrient());
        //Get the relative vertical positions for the anchors
        OString aVAnch = convertToOOXMLVertOrientRel(rFormat.GetVertOrient().GetRelationOrient());

        //Choice that the horizontal position is relative or not
        if (!aHAlign.isEmpty())
            aHAlign = ";mso-position-horizontal:" + aHAlign;
        aHAlign = ";mso-position-horizontal-relative:" + aHAnch;

        //Choice that the vertical position is relative or not
        if (!aVAlign.isEmpty())
            aVAlign = ";mso-position-vertical:" + aVAlign;
        aVAlign = ";mso-position-vertical-relative:" + aVAnch;

        //Set the anchoring information into one string for aPos
        aAnch = aHAlign + aVAlign;

        //Query the positions to aPos from frameformat
        aPos =
            "position:absolute;margin-left:" + OString::number(double(rFormat.GetHoriOrient().GetPos()) / 20) +
            "pt;margin-top:" + OString::number(double(rFormat.GetVertOrient().GetPos()) / 20) + "pt;";
    }

    OString sShapeStyle = "width:" + OString::number( double( rSize.Width() ) / 20 ) +
                        "pt;height:" + OString::number( double( rSize.Height() ) / 20 ) +
                        "pt"; //from VMLExport::AddRectangleDimensions(), it does: value/20

    const SvxLRSpaceItem& rLRSpace = rFormat.GetLRSpace();
    if (rLRSpace.IsExplicitZeroMarginValLeft() || rLRSpace.ResolveLeft({}))
        sShapeStyle += ";mso-wrap-distance-left:"
                       + OString::number(double(rLRSpace.ResolveLeft({})) / 20) + "pt";
    if (rLRSpace.IsExplicitZeroMarginValRight() || rLRSpace.ResolveRight({}))
        sShapeStyle += ";mso-wrap-distance-right:"
                       + OString::number(double(rLRSpace.ResolveRight({})) / 20) + "pt";
    const SvxULSpaceItem& rULSpace = rFormat.GetULSpace();
    if (rULSpace.GetUpper())
        sShapeStyle += ";mso-wrap-distance-top:" + OString::number(double(rULSpace.GetUpper()) / 20) + "pt";
    if (rULSpace.GetLower())
        sShapeStyle += ";mso-wrap-distance-bottom:" + OString::number(double(rULSpace.GetLower()) / 20) + "pt";

    //Export anchor setting, if it exists
    if (!aPos.isEmpty() && !aAnch.isEmpty())
        sShapeStyle = aPos + sShapeStyle  + aAnch;

    return sShapeStyle;
}

void DocxAttributeOutput::ExportOLESurround(const SwFormatSurround& rWrap)
{
    const bool bIsContour = rWrap.IsContour(); //Has the shape contour or not
    OString sSurround;
    OString sSide;

    //Map the ODF wrap settings to OOXML one
    switch (rWrap.GetSurround())
    {
        case text::WrapTextMode::WrapTextMode_NONE:
            sSurround = "topAndBottom"_ostr;
            break;
        case text::WrapTextMode::WrapTextMode_PARALLEL:
            sSurround = bIsContour ? "tight"_ostr : "square"_ostr;
            break;
        case text::WrapTextMode::WrapTextMode_DYNAMIC:
            sSide = "largest"_ostr;
            sSurround = bIsContour ? "tight"_ostr : "square"_ostr;
            break;
        case text::WrapTextMode::WrapTextMode_LEFT:
            sSide = "left"_ostr;
            sSurround = bIsContour ? "tight"_ostr : "square"_ostr;
            break;
        case text::WrapTextMode::WrapTextMode_RIGHT:
            sSide = "right"_ostr;
            sSurround = bIsContour ? "tight"_ostr : "square"_ostr;
            break;
        default:
            SAL_WARN("sw.ww8", "Unknown surround type on OOXML export!");
            break;
    }

    //if there is a setting export it:
    if (!sSurround.isEmpty())
    {
        if (sSide.isEmpty())
            m_pSerializer->singleElementNS(XML_w10, XML_wrap, XML_type, sSurround);
        else
            m_pSerializer->singleElementNS(XML_w10, XML_wrap, XML_type, sSurround, XML_side, sSide);
    }
}

void DocxAttributeOutput::WritePostponedCustomShape()
{
    if (!m_oPostponedCustomShape)
        return;

    for( const auto & rPostponedDrawing : *m_oPostponedCustomShape)
    {
        if ( IsAlternateContentChoiceOpen() )
            m_rExport.SdrExporter().writeDMLDrawing(rPostponedDrawing.object, rPostponedDrawing.frame);
        else
            m_rExport.SdrExporter().writeDMLAndVMLDrawing(rPostponedDrawing.object, *rPostponedDrawing.frame);
    }
    m_oPostponedCustomShape.reset();
}

void DocxAttributeOutput::WritePostponedDMLDrawing()
{
    if (!m_oPostponedDMLDrawings)
        return;

    // Clear the list early, this method may be called recursively.
    std::optional< std::vector<PostponedDrawing> > pPostponedDMLDrawings(std::move(m_oPostponedDMLDrawings));
    std::optional< std::vector<PostponedOLE> > pPostponedOLEs(std::move(m_oPostponedOLEs));
    m_oPostponedDMLDrawings.reset();
    m_oPostponedOLEs.reset();

    for( const auto & rPostponedDrawing : *pPostponedDMLDrawings )
    {
        // Avoid w:drawing within another w:drawing.
        if ( IsAlternateContentChoiceOpen() && !( m_rExport.SdrExporter().IsDrawingOpen()) )
           m_rExport.SdrExporter().writeDMLDrawing(rPostponedDrawing.object, rPostponedDrawing.frame);
        else
            m_rExport.SdrExporter().writeDMLAndVMLDrawing(rPostponedDrawing.object, *rPostponedDrawing.frame);
    }

    m_oPostponedOLEs = std::move(pPostponedOLEs);
}

void DocxAttributeOutput::WriteFlyFrame(const ww8::Frame& rFrame)
{
    m_pSerializer->mark(Tag_OutputFlyFrame);

    switch ( rFrame.GetWriterType() )
    {
        case ww8::Frame::eGraphic:
            {
                const SdrObject* pSdrObj = rFrame.GetFrameFormat().FindRealSdrObject();
                const SwNode *pNode = rFrame.GetContent();
                const SwGrfNode *pGrfNode = pNode ? pNode->GetGrfNode() : nullptr;
                if ( pGrfNode )
                {
                    if (!m_oPostponedGraphic)
                    {
                        m_bPostponedProcessingFly = false ;
                        FlyFrameGraphic( pGrfNode, rFrame.GetLayoutSize(), nullptr, nullptr, pSdrObj);
                    }
                    else // we are writing out attributes, but w:drawing should not be inside w:rPr,
                    {    // so write it out later
                        m_bPostponedProcessingFly = true ;
                        m_oPostponedGraphic->push_back(PostponedGraphic(pGrfNode, rFrame.GetLayoutSize(), pSdrObj));
                    }
                }
            }
            break;
        case ww8::Frame::eDrawing:
            {
                const SdrObject* pSdrObj = rFrame.GetFrameFormat().FindRealSdrObject();
                if ( pSdrObj )
                {
                    const bool bIsDiagram(nullptr != pSdrObj && pSdrObj->isDiagram());

                    if (bIsDiagram)
                    {
                        if ( !m_oPostponedDiagrams )
                        {
                            m_bPostponedProcessingFly = false ;
                            m_rExport.SdrExporter().writeDiagram( pSdrObj, rFrame.GetFrameFormat());
                        }
                        else // we are writing out attributes, but w:drawing should not be inside w:rPr,
                        {    // so write it out later
                            m_bPostponedProcessingFly = true ;
                            m_oPostponedDiagrams->push_back( PostponedDiagram( pSdrObj, &(rFrame.GetFrameFormat()) ));
                        }
                    }
                    else
                    {
                        if (!m_oPostponedDMLDrawings)
                        {
                            if ( IsAlternateContentChoiceOpen() )
                            {
                                // Do not write w:drawing inside w:drawing. Instead Postpone the Inner Drawing.
                                if( m_rExport.SdrExporter().IsDrawingOpen() )
                                    m_oPostponedCustomShape->push_back(PostponedDrawing(pSdrObj, &(rFrame.GetFrameFormat())));
                                else
                                    m_rExport.SdrExporter().writeDMLDrawing( pSdrObj, &rFrame.GetFrameFormat());
                            }
                            else
                                m_rExport.SdrExporter().writeDMLAndVMLDrawing( pSdrObj, rFrame.GetFrameFormat());
                            m_bPostponedProcessingFly = false ;
                        }
                        // IsAlternateContentChoiceOpen(): check is to ensure that only one object is getting added. Without this check, plus one object gets added
                        // m_bParagraphFrameOpen: check if the frame is open.
                        else if (IsAlternateContentChoiceOpen() && m_bParagraphFrameOpen)
                            m_oPostponedCustomShape->push_back(PostponedDrawing(pSdrObj, &(rFrame.GetFrameFormat())));
                        else
                        {
                            // we are writing out attributes, but w:drawing should not be inside w:rPr, so write it out later
                            m_bPostponedProcessingFly = true ;
                            m_oPostponedDMLDrawings->push_back(PostponedDrawing(pSdrObj, &(rFrame.GetFrameFormat())));
                        }
                    }
                }
            }
            break;
        case ww8::Frame::eTextBox:
            {
                // If this is a TextBox of a shape, then ignore: it's handled in WriteTextBox().
                if (DocxSdrExport::isTextBox(rFrame.GetFrameFormat()))
                    break;

                // If this is a TextBox containing a table which we already exported directly, ignore it
                if (m_aFloatingTablesOfParagraph.find(&rFrame.GetFrameFormat()) != m_aFloatingTablesOfParagraph.end())
                    break;

                // skip also inline headings already exported before
                const SwFormat* pParent = rFrame.GetFrameFormat().DerivedFrom();
                if ( pParent && pParent->GetPoolFormatId() == RES_POOLFRM_INLINE_HEADING )
                    break;

                // The frame output is postponed to the end of the anchor paragraph
                bool bDuplicate = false;
                const UIName& rName = rFrame.GetFrameFormat().GetName();
                if (m_aFramesOfParagraph.size() && !rName.isEmpty())
                {
                    const unsigned nSize = m_aFramesOfParagraph.top().size();
                    for (unsigned nIndex = 0; nIndex < nSize; ++nIndex)
                    {
                        const UIName& rNameExisting
                            = m_aFramesOfParagraph.top()[nIndex].GetFrameFormat().GetName();

                        if (rName == rNameExisting)
                        {
                            bDuplicate = true;
                            break;
                        }
                    }
                }

                if( !bDuplicate )
                {
                    m_bPostponedProcessingFly = true ;
                    if ( m_aFramesOfParagraph.size() )
                        m_aFramesOfParagraph.top().emplace_back(rFrame);
                }
            }
            break;
        case ww8::Frame::eOle:
            {
                const SwFrameFormat &rFrameFormat = rFrame.GetFrameFormat();
                const SdrObject *pSdrObj = rFrameFormat.FindRealSdrObject();
                if ( pSdrObj )
                {
                    SwNodeIndex aIdx(*rFrameFormat.GetContent().GetContentIdx(), 1);
                    SwOLENode& rOLENd = *aIdx.GetNode().GetOLENode();

                    //output variable for the formula alignment (default inline)
                    sal_Int8 nAlign(FormulaImExportBase::eFormulaAlign::INLINE);
                    auto xObj(rOLENd.GetOLEObj().GetOleRef()); //get the xObject of the formula

                    //tdf133030: Export formula position
                    //If we have a formula with inline anchor...
                    if(SotExchange::IsMath(xObj->getClassID()) && rFrame.IsInline())
                    {
                        SwNode const* const pAnchorNode = rFrameFormat.GetAnchor().GetAnchorNode();
                        if(pAnchorNode)
                        {
                            //Get the text node what the formula anchored to
                            const SwTextNode* pTextNode = pAnchorNode->GetTextNode();
                            if(pTextNode && pTextNode->Len() == 1)
                            {
                                //Get the paragraph alignment
                                auto aParaAdjust = pTextNode->GetSwAttrSet().GetAdjust().GetAdjust();
                                //And set the formula according to the paragraph alignment
                                if (aParaAdjust == SvxAdjust::Center)
                                    nAlign = FormulaImExportBase::eFormulaAlign::CENTER;
                                else if (aParaAdjust == SvxAdjust::Right)
                                    nAlign = FormulaImExportBase::eFormulaAlign::RIGHT;
                                else // left in the case of left and justified paragraph alignments
                                    nAlign = FormulaImExportBase::eFormulaAlign::LEFT;
                            }
                        }
                    }
                    WriteOLE2Obj( pSdrObj, rOLENd, rFrame.GetLayoutSize(), dynamic_cast<const SwFlyFrameFormat*>( &rFrameFormat ), nAlign);
                    m_bPostponedProcessingFly = false ;
                }
            }
            break;
        case ww8::Frame::eFormControl:
            {
                const SdrObject* pObject = rFrame.GetFrameFormat().FindRealSdrObject();
                if(ExportAsActiveXControl(pObject))
                    m_aPostponedActiveXControls.emplace_back(pObject, &(rFrame.GetFrameFormat()));
                else
                    m_aPostponedFormControls.push_back(pObject);
                m_bPostponedProcessingFly = true ;
            }
            break;
        default:
            SAL_INFO("sw.ww8", "TODO DocxAttributeOutput::OutputFlyFrame_Impl( const ww8::Frame& rFrame ) - frame type " <<
                    ( rFrame.GetWriterType() == ww8::Frame::eTextBox ? "eTextBox":
                      ( rFrame.GetWriterType() == ww8::Frame::eOle ? "eOle": "???" ) ) );
            break;
    }

    m_pSerializer->mergeTopMarks(Tag_OutputFlyFrame);
}

void DocxAttributeOutput::OutputFlyFrame_Impl(const ww8::Frame& rFrame, const Point& /*rNdTopLeft*/)
{
    /// The old OutputFlyFrame_Impl() moved to WriteFlyFrame().
    /// Now if a frame anchored inside another frame, it will
    /// not be exported immediately, because OOXML does not
    /// support that feature, instead it postponed and exported
    /// later when the original shape closed.

    if (rFrame.IsInline())
    {
        m_nEmbedFlyLevel++;
        WriteFlyFrame(rFrame);
        m_nEmbedFlyLevel--;
        return;
    }

    if (m_nEmbedFlyLevel == 0)
    {
        if (m_vPostponedFlys.empty())
        {
            m_nEmbedFlyLevel++;
            WriteFlyFrame(rFrame);
            m_nEmbedFlyLevel--;
        }
        else
            for (auto it = m_vPostponedFlys.begin(); it != m_vPostponedFlys.end();)
            {
                m_nEmbedFlyLevel++;
                WriteFlyFrame(*it);
                it = m_vPostponedFlys.erase(it);
                m_nEmbedFlyLevel--;
            }
    }
    else
    {
        bool bFound = false;
        for (const auto& i : m_vPostponedFlys)
        {
            if (i.RefersToSameFrameAs(rFrame))
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            if (auto pParentFly = rFrame.GetContentNode()->GetFlyFormat())
            {
                auto aHori(rFrame.GetFrameFormat().GetHoriOrient());
                aHori.SetPos(aHori.GetPos() + pParentFly->GetHoriOrient().GetPos());
                auto aVori(rFrame.GetFrameFormat().GetVertOrient());
                aVori.SetPos(aVori.GetPos() + pParentFly->GetVertOrient().GetPos());

                const_cast<SwFrameFormat&>(rFrame.GetFrameFormat()).SetFormatAttr(aHori);
                const_cast<SwFrameFormat&>(rFrame.GetFrameFormat()).SetFormatAttr(aVori);
                const_cast<SwFrameFormat&>(rFrame.GetFrameFormat()).SetFormatAttr(pParentFly->GetAnchor());

                m_vPostponedFlys.push_back(rFrame);
            }

        }
    }
}

void DocxAttributeOutput::WriteOutliner(const OutlinerParaObject& rParaObj)
{
    const EditTextObject& rEditObj = rParaObj.GetTextObject();
    MSWord_SdrAttrIter aAttrIter( m_rExport, rEditObj, TXT_HFTXTBOX );

    sal_Int32 nPara = rEditObj.GetParagraphCount();

    m_pSerializer->startElementNS(XML_w, XML_txbxContent);
    for (sal_Int32 n = 0; n < nPara; ++n)
    {
        if( n )
            aAttrIter.NextPara( n );

        OUString aStr( rEditObj.GetText( n ));
        sal_Int32 nCurrentPos = 0;
        sal_Int32 nEnd = aStr.getLength();

        StartParagraph(ww8::WW8TableNodeInfo::Pointer_t(), false);

        // Write paragraph properties.
        StartParagraphProperties();
        aAttrIter.OutParaAttr(false);
        SfxItemSet aParagraphMarkerProperties(m_rExport.m_rDoc.GetAttrPool());
        EndParagraphProperties(aParagraphMarkerProperties, nullptr, nullptr, nullptr);

        do {
            const sal_Int32 nNextAttr = std::min(aAttrIter.WhereNext(), nEnd);

            m_pSerializer->startElementNS(XML_w, XML_r);

            // Write run properties.
            m_pSerializer->startElementNS(XML_w, XML_rPr);
            aAttrIter.OutAttr(nCurrentPos);
            WriteCollectedRunProperties();
            m_pSerializer->endElementNS(XML_w, XML_rPr);

            bool bTextAtr = aAttrIter.IsTextAttr( nCurrentPos );
            if( !bTextAtr )
            {
                OUString aOut( aStr.copy( nCurrentPos, nNextAttr - nCurrentPos ) );
                RunText(aOut);
            }

            if ( !m_sRawText.isEmpty() )
            {
                RunText( m_sRawText );
                m_sRawText.clear();
            }

            m_pSerializer->endElementNS( XML_w, XML_r );

            nCurrentPos = nNextAttr;
            aAttrIter.NextPos();
        }
        while( nCurrentPos < nEnd );
        EndParagraph(ww8::WW8TableNodeInfoInner::Pointer_t());
    }
    m_pSerializer->endElementNS( XML_w, XML_txbxContent );
}

void DocxAttributeOutput::pushToTableExportContext(DocxTableExportContext& rContext)
{
    rContext.m_pTableInfo = m_rExport.m_pTableInfo;
    m_rExport.m_pTableInfo = std::make_shared<ww8::WW8TableInfo>();

    rContext.m_bTableCellOpen = m_tableReference.m_bTableCellOpen;
    m_tableReference.m_bTableCellOpen = false;

    rContext.m_nTableDepth = m_tableReference.m_nTableDepth;
    m_tableReference.m_nTableDepth = 0;

    rContext.m_bStartedParaSdt = m_aParagraphSdt.m_bStartedSdt;
    m_aParagraphSdt.m_bStartedSdt = false;
    rContext.m_bStartedRunSdt = m_aRunSdt.m_bStartedSdt;
    m_aRunSdt.m_bStartedSdt = false;

    rContext.m_nHyperLinkCount = m_nHyperLinkCount.back();
    m_nHyperLinkCount.back() = 0;
}

void DocxAttributeOutput::popFromTableExportContext(DocxTableExportContext const & rContext)
{
    m_rExport.m_pTableInfo = rContext.m_pTableInfo;
    m_tableReference.m_bTableCellOpen = rContext.m_bTableCellOpen;
    m_tableReference.m_nTableDepth = rContext.m_nTableDepth;
    m_aParagraphSdt.m_bStartedSdt = rContext.m_bStartedParaSdt;
    m_aRunSdt.m_bStartedSdt = rContext.m_bStartedRunSdt;
    m_nHyperLinkCount.back() = rContext.m_nHyperLinkCount;
}

void DocxAttributeOutput::WriteTextBox(uno::Reference<drawing::XShape> xShape)
{
    DocxTableExportContext aTableExportContext(*this);

    SwFrameFormat* pTextBox = SwTextBoxHelper::getOtherTextBoxFormat(xShape);
    assert(pTextBox);
    const SwPosition* pAnchor = nullptr;
    const bool bFlyAtPage = pTextBox->GetAnchor().GetAnchorId() == RndStdIds::FLY_AT_PAGE;
    if (bFlyAtPage) //tdf135711
    {
        auto pNdIdx = pTextBox->GetContent().GetContentIdx();
        if (pNdIdx) //Is that possible it is null?
            pAnchor = new SwPosition(*pNdIdx);
    }
    else
    {
        pAnchor = pTextBox->GetAnchor().GetContentAnchor();//This might be null
    }

    if (pAnchor) //pAnchor can be null, so that's why not assert here.
    {
        ww8::Frame aFrame(*pTextBox, *pAnchor);
        m_rExport.SdrExporter().writeDMLTextFrame(&aFrame, /*bTextBoxOnly=*/true);
        if (bFlyAtPage)
        {
            delete pAnchor;
        }
    }
}

void DocxAttributeOutput::WriteVMLTextBox(uno::Reference<drawing::XShape> xShape)
{
    DocxTableExportContext aTableExportContext(*this);

    SwFrameFormat* pTextBox = SwTextBoxHelper::getOtherTextBoxFormat(xShape);
    assert(pTextBox);
    const SwPosition* pAnchor = nullptr;
    if (pTextBox->GetAnchor().GetAnchorId() == RndStdIds::FLY_AT_PAGE) //tdf135711
    {
        auto pNdIdx = pTextBox->GetContent().GetContentIdx();
        if (pNdIdx) //Is that possible it is null?
            pAnchor = new SwPosition(*pNdIdx);
    }
    else
    {
        pAnchor = pTextBox->GetAnchor().GetContentAnchor();//This might be null
    }

    if (pAnchor) //pAnchor can be null, so that's why not assert here.
    {
        ww8::Frame aFrame(*pTextBox, *pAnchor);
        m_rExport.SdrExporter().writeVMLTextFrame(&aFrame, /*bTextBoxOnly=*/true);
        if (pTextBox->GetAnchor().GetAnchorId() == RndStdIds::FLY_AT_PAGE)
        {
            delete pAnchor;
        }
    }
}

oox::drawingml::DrawingML& DocxAttributeOutput::GetDrawingML()
{
    return m_rDrawingML;
}

bool DocxAttributeOutput::MaybeOutputBrushItem(SfxItemSet const& rSet)
{
    const XFillStyleItem* pXFillStyleItem(rSet.GetItem<XFillStyleItem>(XATTR_FILLSTYLE));

    if ((pXFillStyleItem && pXFillStyleItem->GetValue() != drawing::FillStyle_NONE)
        || !m_rExport.SdrExporter().getDMLTextFrameSyntax())
    {
        return false;
    }

    // sw text frames are opaque by default, even with fill none!
    std::unique_ptr<SfxItemSet> const pClone(rSet.Clone());
    XFillColorItem const aColor(OUString(), COL_WHITE);
    pClone->Put(aColor);
    // call getSvxBrushItemForSolid - this also takes XFillTransparenceItem into account
    XFillStyleItem const aSolid(drawing::FillStyle_SOLID);
    pClone->Put(aSolid);
    std::unique_ptr<SvxBrushItem> const pBrush(getSvxBrushItemFromSourceSet(*pClone, RES_BACKGROUND));
    FormatBackground(*pBrush);
    return true;
}

namespace {

/// Functor to do case-insensitive ordering of OUString instances.
struct OUStringIgnoreCase
{
    bool operator() (std::u16string_view lhs, std::u16string_view rhs) const
    {
        return o3tl::compareToIgnoreAsciiCase(lhs, rhs) < 0;
    }
};

}

/// Guesses if a style created in Writer (no grab-bag) should be qFormat or not.
static bool lcl_guessQFormat(const OUString& rName, sal_uInt16 nWwId)
{
    // If the style has no dedicated STI number, then it's probably a custom style -> qFormat.
    if (nWwId == ww::stiUser)
        return true;

    // Allow exported built-in styles UI language neutral
    if ( nWwId == ww::stiNormal ||
        ( nWwId>= ww::stiLev1 && nWwId <= ww::stiLev9 ) ||
            nWwId == ww::stiCaption || nWwId == ww::stiTitle ||
            nWwId == ww::stiSubtitle || nWwId == ww::stiStrong ||
            nWwId == ww::stiEmphasis )
        return true;

    static o3tl::sorted_vector<std::u16string_view, OUStringIgnoreCase> const aAllowlist
    {
        u"No Spacing",
        u"List Paragraph",
        u"Quote",
        u"Intense Quote",
        u"Subtle Emphasis",
        u"Intense Emphasis",
        u"Subtle Reference",
        u"Intense Reference",
        u"Book Title",
        u"TOC Heading",
    };
    // Not custom style? Then we have a list of standard styles which should be qFormat.
    return aAllowlist.find(rName) != aAllowlist.end();
}

void DocxAttributeOutput::StartStyle( const OUString& rName, StyleType eType,
        sal_uInt16 nBase, sal_uInt16 nNext, sal_uInt16 nLink, sal_uInt16 nWwId, sal_uInt16 nSlot, bool bAutoUpdate )
{
    bool bQFormat = false, bUnhideWhenUsed = false, bSemiHidden = false, bLocked = false, bDefault = false, bCustomStyle = false;
    OUString aRsid, aUiPriority;
    rtl::Reference<FastAttributeList> pStyleAttributeList = FastSerializerHelper::createAttrList();
    uno::Any aAny;
    if (eType == STYLE_TYPE_PARA || eType == STYLE_TYPE_CHAR)
    {
        const SwFormat* pFormat = m_rExport.m_pStyles->GetSwFormat(nSlot);
        pFormat->GetGrabBagItem(aAny);
    }
    else
    {
        const SwNumRule* pRule = m_rExport.m_pStyles->GetSwNumRule(nSlot);
        pRule->GetGrabBagItem(aAny);
    }
    const uno::Sequence<beans::PropertyValue> aGrabBag = aAny.get< uno::Sequence<beans::PropertyValue> >();

    for (const auto& rProp : aGrabBag)
    {
        if (rProp.Name == "uiPriority")
            aUiPriority = rProp.Value.get<OUString>();
        else if (rProp.Name == "qFormat")
            bQFormat = true;
        else if (rProp.Name == "rsid")
            aRsid = rProp.Value.get<OUString>();
        else if (rProp.Name == "unhideWhenUsed")
            bUnhideWhenUsed = true;
        else if (rProp.Name == "semiHidden")
            bSemiHidden = true;
        else if (rProp.Name == "locked")
            bLocked = true;
        else if (rProp.Name == "default")
            bDefault = rProp.Value.get<bool>();
        else if (rProp.Name == "customStyle")
            bCustomStyle = rProp.Value.get<bool>();
        else
            SAL_WARN("sw.ww8", "Unhandled style property: " << rProp.Name);
    }

    const char* pType = nullptr;
    switch (eType)
    {
        case STYLE_TYPE_PARA:
            pType = "paragraph";
            break;
        case STYLE_TYPE_CHAR:
            pType = "character";
            break;
        case STYLE_TYPE_LIST: pType = "numbering"; break;
    }
    pStyleAttributeList->add(FSNS( XML_w, XML_type ), pType);
    pStyleAttributeList->add(FSNS(XML_w, XML_styleId), m_rExport.m_pStyles->GetStyleId(nSlot));
    if (bDefault)
        pStyleAttributeList->add(FSNS(XML_w, XML_default), "1");
    if (bCustomStyle)
        pStyleAttributeList->add(FSNS(XML_w, XML_customStyle), "1");
    m_pSerializer->startElementNS( XML_w, XML_style, pStyleAttributeList);
    m_pSerializer->singleElementNS(XML_w, XML_name, FSNS(XML_w, XML_val), rName);

    if ( nBase != 0x0FFF && eType != STYLE_TYPE_LIST)
    {
        m_pSerializer->singleElementNS( XML_w, XML_basedOn,
                FSNS( XML_w, XML_val ), m_rExport.m_pStyles->GetStyleId(nBase) );
    }

    if (nNext != nSlot && nNext != 0x0FFF && eType != STYLE_TYPE_LIST)
    {
        m_pSerializer->singleElementNS( XML_w, XML_next,
                FSNS( XML_w, XML_val ), m_rExport.m_pStyles->GetStyleId(nNext) );
    }

    if (nLink != 0x0FFF && (eType == STYLE_TYPE_PARA || eType == STYLE_TYPE_CHAR))
    {
        m_pSerializer->singleElementNS(XML_w, XML_link, FSNS(XML_w, XML_val),
                                       m_rExport.m_pStyles->GetStyleId(nLink));
    }

    if ( bAutoUpdate )
        m_pSerializer->singleElementNS(XML_w, XML_autoRedefine);

    if (!aUiPriority.isEmpty())
        m_pSerializer->singleElementNS(XML_w, XML_uiPriority, FSNS(XML_w, XML_val), aUiPriority);
    if (bSemiHidden)
        m_pSerializer->singleElementNS(XML_w, XML_semiHidden);
    if (bUnhideWhenUsed)
        m_pSerializer->singleElementNS(XML_w, XML_unhideWhenUsed);

    if (bQFormat || lcl_guessQFormat(rName, nWwId))
        m_pSerializer->singleElementNS(XML_w, XML_qFormat);
    if (bLocked)
        m_pSerializer->singleElementNS(XML_w, XML_locked);
    if (!aRsid.isEmpty())
        m_pSerializer->singleElementNS(XML_w, XML_rsid, FSNS(XML_w, XML_val), aRsid);
}

void DocxAttributeOutput::EndStyle()
{
    m_pSerializer->endElementNS( XML_w, XML_style );
}

void DocxAttributeOutput::StartStyleProperties( bool bParProp, sal_uInt16 /*nStyle*/ )
{
    if ( bParProp )
    {
        m_pSerializer->startElementNS(XML_w, XML_pPr);
        InitCollectedParagraphProperties();
    }
    else
    {
        m_pSerializer->startElementNS(XML_w, XML_rPr);
        InitCollectedRunProperties();
    }
}

void DocxAttributeOutput::EndStyleProperties( bool bParProp )
{
    if ( bParProp )
    {
        WriteCollectedParagraphProperties();

        // Merge the marks for the ordered elements
        m_pSerializer->mergeTopMarks(Tag_InitCollectedParagraphProperties);

        m_pSerializer->endElementNS( XML_w, XML_pPr );
    }
    else
    {
        WriteCollectedRunProperties();

        // Merge the marks for the ordered elements
        m_pSerializer->mergeTopMarks(Tag_InitCollectedRunProperties);

        m_pSerializer->endElementNS( XML_w, XML_rPr );
    }
}

void DocxAttributeOutput::OutlineNumbering(sal_uInt8 const /*nLvl*/)
{
    // Handled by ParaOutlineLevel() instead.
}

void DocxAttributeOutput::ParaOutlineLevel(const SfxUInt16Item& rItem)
{
    sal_uInt16 nOutLvl = std::min(rItem.GetValue(), sal_uInt16(WW8ListManager::nMaxLevel));
    // Outline Level: in LO Body Text = 0, in MS Body Text = 9
    nOutLvl = nOutLvl ? nOutLvl - 1 : 9;
    m_pSerializer->singleElementNS(XML_w, XML_outlineLvl, FSNS(XML_w, XML_val), OString::number(nOutLvl));
}

void DocxAttributeOutput::PageBreakBefore( bool bBreak )
{
    if ( bBreak )
        m_pSerializer->singleElementNS(XML_w, XML_pageBreakBefore);
    else
        m_pSerializer->singleElementNS( XML_w, XML_pageBreakBefore,
                FSNS( XML_w, XML_val ), "false" );
}

void DocxAttributeOutput::SectionBreak( sal_uInt8 nC, bool bBreakAfter, const WW8_SepInfo* pSectionInfo, bool bExtraPageBreak)
{
    switch ( nC )
    {
        case msword::ColumnBreak:
            // The column break should be output in the next paragraph...
            if ( m_nColBreakStatus == COLBRK_WRITE )
                m_nColBreakStatus = COLBRK_WRITEANDPOSTPONE;
            else
                m_nColBreakStatus = COLBRK_POSTPONE;
            break;
        case msword::PageBreak:
            if ( pSectionInfo )
            {
                // Detect when the current node is the last node in the
                // document: the last section is written explicitly in
                // DocxExport::WriteMainText(), don't duplicate that here.
                SwNodeIndex aCurrentNode(m_rExport.m_pCurPam->GetPointNode());
                SwNodeIndex aLastNode(m_rExport.m_rDoc.GetNodes().GetEndOfContent(), -1);
                bool bEmit = aCurrentNode != aLastNode;

                if (!bEmit)
                {
                    // Need to still emit an empty section at the end of the
                    // document in case balanced columns are wanted, since the last
                    // section in Word is always balanced.
                    sal_uInt16 nColumns = 1;
                    bool bBalance = false;
                    if (const SwSectionFormat* pFormat = pSectionInfo->pSectionFormat)
                    {
                        if (pFormat != reinterpret_cast<SwSectionFormat*>(sal_IntPtr(-1)))
                        {
                            nColumns = pFormat->GetCol().GetNumCols();
                            const SwFormatNoBalancedColumns& rNoBalanced = pFormat->GetBalancedColumns();
                            bBalance = !rNoBalanced.GetValue();
                        }
                    }
                    bEmit = (nColumns > 1 && bBalance);
                }

                // don't add section properties if this will be the first
                // paragraph in the document
                if ( !m_bParagraphOpened && !m_bIsFirstParagraph && bEmit )
                {
                    // Create a dummy paragraph if needed
                    m_pSerializer->startElementNS(XML_w, XML_p);
                    m_pSerializer->startElementNS(XML_w, XML_pPr);

                    m_rExport.SectionProperties( *pSectionInfo );

                    m_pSerializer->endElementNS( XML_w, XML_pPr );
                    if (bExtraPageBreak)
                    {
                        m_pSerializer->startElementNS(XML_w, XML_r);
                        m_pSerializer->singleElementNS(XML_w, XML_br, FSNS(XML_w, XML_type), "page");
                        m_pSerializer->endElementNS(XML_w, XML_r);
                    }
                    m_pSerializer->endElementNS( XML_w, XML_p );
                }
                else
                {
                    if (bExtraPageBreak && m_bParagraphOpened)
                    {
                        m_pSerializer->startElementNS(XML_w, XML_r);
                        m_pSerializer->singleElementNS(XML_w, XML_br, FSNS(XML_w, XML_type), "page");
                        m_pSerializer->endElementNS(XML_w, XML_r);
                    }
                    // postpone the output of this; it has to be done inside the
                    // paragraph properties, so remember it until then
                    m_pSectionInfo.reset( new WW8_SepInfo( *pSectionInfo ));
                }
            }
            else if ( m_bParagraphOpened )
            {
                if (bBreakAfter)
                    // tdf#128889
                    m_bPageBreakAfter = true;
                else
                {
                    m_pSerializer->startElementNS(XML_w, XML_r);
                    m_pSerializer->singleElementNS(XML_w, XML_br, FSNS(XML_w, XML_type), "page");
                    m_pSerializer->endElementNS(XML_w, XML_r);
                }
            }
            else
                m_bPostponedPageBreak = true;

            break;
        default:
            SAL_INFO("sw.ww8", "Unknown section break to write: " << nC );
            break;
    }
}

void DocxAttributeOutput::EndParaSdtBlock()
{
    if (m_aParagraphSdt.m_bStartedSdt)
    {
        // Paragraph-level SDT still open? Close it now.
        m_aParagraphSdt.EndSdtBlock(m_pSerializer);
    }
}

void DocxAttributeOutput::StartSection()
{
    m_pSerializer->startElementNS(XML_w, XML_sectPr);
    m_bOpenedSectPr = true;

    // Write the elements in the spec order
    static const sal_Int32 aOrder[] =
    {
        FSNS( XML_w, XML_headerReference ),
        FSNS( XML_w, XML_footerReference ),
        FSNS( XML_w, XML_footnotePr ),
        FSNS( XML_w, XML_endnotePr ),
        FSNS( XML_w, XML_type ),
        FSNS( XML_w, XML_pgSz ),
        FSNS( XML_w, XML_pgMar ),
        FSNS( XML_w, XML_paperSrc ),
        FSNS( XML_w, XML_pgBorders ),
        FSNS( XML_w, XML_lnNumType ),
        FSNS( XML_w, XML_pgNumType ),
        FSNS( XML_w, XML_cols ),
        FSNS( XML_w, XML_formProt ),
        FSNS( XML_w, XML_vAlign ),
        FSNS( XML_w, XML_noEndnote ),
        FSNS( XML_w, XML_titlePg ),
        FSNS( XML_w, XML_textDirection ),
        FSNS( XML_w, XML_bidi ),
        FSNS( XML_w, XML_rtlGutter ),
        FSNS( XML_w, XML_docGrid ),
        FSNS( XML_w, XML_printerSettings ),
        FSNS( XML_w, XML_sectPrChange )
    };

    // postpone the output so that we can later [in EndParagraphProperties()]
    // prepend the properties before the run
    // coverity[overrun-buffer-arg : FALSE] - coverity has difficulty with css::uno::Sequence
    m_pSerializer->mark(Tag_StartSection, comphelper::containerToSequence(aOrder));
    m_bHadSectPr = true;
}

void DocxAttributeOutput::EndSection()
{
    // Write the section properties
    if ( m_pSectionSpacingAttrList.is() )
    {
        m_pSerializer->singleElementNS( XML_w, XML_pgMar, detachFrom( m_pSectionSpacingAttrList ) );
    }

    // Order the elements
    m_pSerializer->mergeTopMarks(Tag_StartSection);

    m_pSerializer->endElementNS( XML_w, XML_sectPr );
    m_bOpenedSectPr = false;
}

void DocxAttributeOutput::SectionFormProtection( bool bProtected )
{
    if ( bProtected )
        m_pSerializer->singleElementNS(XML_w, XML_formProt, FSNS(XML_w, XML_val), "true");
    else
        m_pSerializer->singleElementNS(XML_w, XML_formProt, FSNS(XML_w, XML_val), "false");
}

void DocxAttributeOutput::SectionRtlGutter(const SfxBoolItem& rRtlGutter)
{
    if (!rRtlGutter.GetValue())
    {
        return;
    }

    m_pSerializer->singleElementNS(XML_w, XML_rtlGutter);
}

void DocxAttributeOutput::TextLineBreak(const SwFormatLineBreak& rLineBreak)
{
    m_oLineBreakClear = rLineBreak.GetValue();
}

void DocxAttributeOutput::WriteLineBreak()
{
    if (!m_oLineBreakClear.has_value())
    {
        return;
    }

    rtl::Reference<FastAttributeList> pAttr = FastSerializerHelper::createAttrList();
    pAttr->add(FSNS(XML_w, XML_type), "textWrapping");
    switch (*m_oLineBreakClear)
    {
        case SwLineBreakClear::NONE:
            pAttr->add(FSNS(XML_w, XML_clear), "none");
            break;
        case SwLineBreakClear::LEFT:
            pAttr->add(FSNS(XML_w, XML_clear), "left");
            break;
        case SwLineBreakClear::RIGHT:
            pAttr->add(FSNS(XML_w, XML_clear), "right");
            break;
        case SwLineBreakClear::ALL:
            pAttr->add(FSNS(XML_w, XML_clear), "all");
            break;
    }
    m_oLineBreakClear.reset();

    m_pSerializer->singleElementNS(XML_w, XML_br, pAttr);
}

void DocxAttributeOutput::SectionLineNumbering( sal_uLong nRestartNo, const SwLineNumberInfo& rLnNumInfo )
{
    rtl::Reference<FastAttributeList> pAttr = FastSerializerHelper::createAttrList();
    pAttr->add( FSNS( XML_w, XML_countBy ), OString::number(rLnNumInfo.GetCountBy()));
    pAttr->add( FSNS( XML_w, XML_restart ), rLnNumInfo.IsRestartEachPage() ? "newPage" : "continuous" );
    if( rLnNumInfo.GetPosFromLeft())
        pAttr->add( FSNS( XML_w, XML_distance ), OString::number(rLnNumInfo.GetPosFromLeft()));
    if (nRestartNo > 0)
        // Writer is 1-based, Word is 0-based.
        pAttr->add(FSNS(XML_w, XML_start), OString::number(nRestartNo - 1));
    m_pSerializer->singleElementNS( XML_w, XML_lnNumType, pAttr );
}

void DocxAttributeOutput::SectionTitlePage()
{
    m_pSerializer->singleElementNS(XML_w, XML_titlePg);
}

void DocxAttributeOutput::SectionPageBorders( const SwFrameFormat* pFormat, const SwFrameFormat* /*pFirstPageFormat*/ )
{
    // Output the margins

    const SvxBoxItem& rBox = pFormat->GetBox( );

    const SvxBorderLine* pLeft = rBox.GetLeft( );
    const SvxBorderLine* pTop = rBox.GetTop( );
    const SvxBorderLine* pRight = rBox.GetRight( );
    const SvxBorderLine* pBottom = rBox.GetBottom( );

    if ( !(pBottom || pTop || pLeft || pRight) )
        return;

    OutputBorderOptions aOutputBorderOptions = lcl_getBoxBorderOptions();

    // Check if there is a shadow item
    const SfxPoolItem* pItem = GetExport().HasItem( RES_SHADOW );
    if ( pItem )
    {
        const SvxShadowItem* pShadowItem = static_cast<const SvxShadowItem*>(pItem);
        aOutputBorderOptions.aShadowLocation = pShadowItem->GetLocation();
    }

    // By top margin, impl_borders() means the distance between the top of the page and the header frame.
    editeng::WordPageMargins aMargins = m_pageMargins;
    HdFtDistanceGlue aGlue(pFormat->GetAttrSet());
    if (aGlue.HasHeader())
        aMargins.nTop = aGlue.m_DyaHdrTop;
    // Ditto for bottom margin.
    if (aGlue.HasFooter())
        aMargins.nBottom = aGlue.m_DyaHdrBottom;

    if (pFormat->GetDoc().getIDocumentSettingAccess().get(DocumentSettingId::GUTTER_AT_TOP))
    {
        aMargins.nTop += pFormat->GetLRSpace().GetGutterMargin();
    }
    else
    {
        aMargins.nLeft += pFormat->GetLRSpace().GetGutterMargin();
    }

    aOutputBorderOptions.pDistances = std::make_shared<editeng::WordBorderDistances>();
    editeng::BorderDistancesToWord(rBox, aMargins, *aOutputBorderOptions.pDistances);

    // All distances are relative to the text margins
    m_pSerializer->startElementNS(XML_w, XML_pgBorders,
        FSNS(XML_w, XML_display), "allPages",
        FSNS(XML_w, XML_offsetFrom), aOutputBorderOptions.pDistances->bFromEdge ? "page" : "text");

    std::map<SvxBoxItemLine, css::table::BorderLine2> aEmptyMap; // empty styles map
    impl_borders( m_pSerializer, rBox, aOutputBorderOptions, aEmptyMap );

    m_pSerializer->endElementNS( XML_w, XML_pgBorders );

}

void DocxAttributeOutput::SectionBiDi( bool bBiDi )
{
    if ( bBiDi )
        m_pSerializer->singleElementNS(XML_w, XML_bidi);
}

// Converting Numbering Format Code to string
static OString lcl_ConvertNumberingType(sal_Int16 nNumberingType, const SfxItemSet* pOutSet, OString& rFormat, const OString& sDefault = ""_ostr )
{
    OString aType = sDefault;

    switch ( nNumberingType )
    {
        case SVX_NUM_CHARS_UPPER_LETTER:
        case SVX_NUM_CHARS_UPPER_LETTER_N:  aType = "upperLetter"_ostr; break;

        case SVX_NUM_CHARS_LOWER_LETTER:
        case SVX_NUM_CHARS_LOWER_LETTER_N:  aType = "lowerLetter"_ostr; break;

        case SVX_NUM_ROMAN_UPPER:           aType = "upperRoman"_ostr;  break;
        case SVX_NUM_ROMAN_LOWER:           aType = "lowerRoman"_ostr;  break;
        case SVX_NUM_ARABIC:                aType = "decimal"_ostr;     break;

        case SVX_NUM_BITMAP:
        case SVX_NUM_CHAR_SPECIAL:          aType = "bullet"_ostr;      break;

        case style::NumberingType::CHARS_HEBREW: aType = "hebrew2"_ostr; break;
        case style::NumberingType::NUMBER_HEBREW: aType = "hebrew1"_ostr; break;
        case style::NumberingType::NUMBER_NONE: aType = "none"_ostr; break;
        case style::NumberingType::FULLWIDTH_ARABIC: aType="decimalFullWidth"_ostr; break;
        case style::NumberingType::TIAN_GAN_ZH: aType="ideographTraditional"_ostr; break;
        case style::NumberingType::DI_ZI_ZH: aType="ideographZodiac"_ostr; break;
        case style::NumberingType::NUMBER_LOWER_ZH:
            aType="taiwaneseCountingThousand"_ostr;
            if (pOutSet) {
                const SvxLanguageItem& rLang = pOutSet->Get( RES_CHRATR_CJK_LANGUAGE);
                const LanguageType eLang = rLang.GetLanguage();

                if (LANGUAGE_CHINESE_SIMPLIFIED == eLang) {
                    aType="chineseCountingThousand"_ostr;
                }
            }
        break;
        case style::NumberingType::NUMBER_UPPER_ZH_TW: aType="ideographLegalTraditional"_ostr;break;
        case style::NumberingType::NUMBER_UPPER_ZH: aType="chineseLegalSimplified"_ostr; break;
        case style::NumberingType::NUMBER_TRADITIONAL_JA: aType="japaneseLegal"_ostr;break;
        case style::NumberingType::AIU_FULLWIDTH_JA: aType="aiueoFullWidth"_ostr;break;
        case style::NumberingType::AIU_HALFWIDTH_JA: aType="aiueo"_ostr;break;
        case style::NumberingType::IROHA_FULLWIDTH_JA: aType="iroha"_ostr;break;
        case style::NumberingType::IROHA_HALFWIDTH_JA: aType="irohaFullWidth"_ostr;break;
        case style::NumberingType::HANGUL_SYLLABLE_KO: aType="ganada"_ostr;break;
        case style::NumberingType::HANGUL_JAMO_KO: aType="chosung"_ostr;break;
        case style::NumberingType::NUMBER_HANGUL_KO: aType="koreanCounting"_ostr; break;
        case style::NumberingType::NUMBER_LEGAL_KO: aType = "koreanLegal"_ostr; break;
        case style::NumberingType::NUMBER_DIGITAL_KO: aType = "koreanDigital"_ostr; break;
        case style::NumberingType::NUMBER_DIGITAL2_KO: aType = "koreanDigital2"_ostr; break;
        case style::NumberingType::CIRCLE_NUMBER: aType="decimalEnclosedCircle"_ostr; break;
        case style::NumberingType::CHARS_ARABIC: aType="arabicAlpha"_ostr; break;
        case style::NumberingType::CHARS_ARABIC_ABJAD: aType="arabicAbjad"_ostr; break;
        case style::NumberingType::CHARS_THAI: aType="thaiLetters"_ostr; break;
        case style::NumberingType::CHARS_PERSIAN:
        case style::NumberingType::CHARS_NEPALI: aType="hindiVowels"_ostr; break;
        case style::NumberingType::CHARS_CYRILLIC_UPPER_LETTER_RU:
        case style::NumberingType::CHARS_CYRILLIC_UPPER_LETTER_N_RU: aType = "russianUpper"_ostr; break;
        case style::NumberingType::CHARS_CYRILLIC_LOWER_LETTER_RU:
        case style::NumberingType::CHARS_CYRILLIC_LOWER_LETTER_N_RU: aType = "russianLower"_ostr; break;
        case style::NumberingType::TEXT_NUMBER: aType="ordinal"_ostr; break;
        case style::NumberingType::TEXT_CARDINAL: aType="cardinalText"_ostr; break;
        case style::NumberingType::TEXT_ORDINAL: aType="ordinalText"_ostr; break;
        case style::NumberingType::SYMBOL_CHICAGO: aType="chicago"_ostr; break;
        case style::NumberingType::ARABIC_ZERO: aType = "decimalZero"_ostr; break;
        case style::NumberingType::ARABIC_ZERO3:
            aType = "custom"_ostr;
            rFormat = "001, 002, 003, ..."_ostr;
            break;
        case style::NumberingType::ARABIC_ZERO4:
            aType = "custom"_ostr;
            rFormat = "0001, 0002, 0003, ..."_ostr;
            break;
        case style::NumberingType::ARABIC_ZERO5:
            aType = "custom"_ostr;
            rFormat = "00001, 00002, 00003, ..."_ostr;
            break;
/*
        Fallback the rest to the suggested default.
        case style::NumberingType::NATIVE_NUMBERING:
        case style::NumberingType::HANGUL_CIRCLED_JAMO_KO:
        case style::NumberingType::HANGUL_CIRCLED_SYLLABLE_KO:
        case style::NumberingType::CHARS_GREEK_UPPER_LETTER:
        case style::NumberingType::CHARS_GREEK_LOWER_LETTER:
        case style::NumberingType::PAGE_DESCRIPTOR:
        case style::NumberingType::TRANSLITERATION:
        case style::NumberingType::CHARS_KHMER:
        case style::NumberingType::CHARS_LAO:
        case style::NumberingType::CHARS_TIBETAN:
        case style::NumberingType::CHARS_CYRILLIC_UPPER_LETTER_BG:
        case style::NumberingType::CHARS_CYRILLIC_LOWER_LETTER_BG:
        case style::NumberingType::CHARS_CYRILLIC_UPPER_LETTER_N_BG:
        case style::NumberingType::CHARS_CYRILLIC_LOWER_LETTER_N_BG:
        case style::NumberingType::CHARS_MYANMAR:
        case style::NumberingType::CHARS_CYRILLIC_UPPER_LETTER_SR:
        case style::NumberingType::CHARS_CYRILLIC_LOWER_LETTER_SR:
        case style::NumberingType::CHARS_CYRILLIC_UPPER_LETTER_N_SR:
        case style::NumberingType::CHARS_CYRILLIC_LOWER_LETTER_N_SR:
        case style::NumberingType::CHARS_CYRILLIC_UPPER_LETTER_UK:
        case style::NumberingType::CHARS_CYRILLIC_LOWER_LETTER_UK:
        case style::NumberingType::CHARS_CYRILLIC_UPPER_LETTER_N_UK:
        case style::NumberingType::CHARS_CYRILLIC_LOWER_LETTER_N_UK:
*/
        default: break;
    }
    return aType;
}


void DocxAttributeOutput::SectionPageNumbering( sal_uInt16 nNumType, const ::std::optional<sal_uInt16>& oPageRestartNumber )
{
    // FIXME Not called properly with page styles like "First Page"

    rtl::Reference<FastAttributeList> pAttr = FastSerializerHelper::createAttrList();

    // std::nullopt means no restart: then don't output that attribute if it is negative
    if ( oPageRestartNumber )
       pAttr->add( FSNS( XML_w, XML_start ), OString::number( *oPageRestartNumber ) );

    // nNumType corresponds to w:fmt. See WW8Export::GetNumId() for more precisions
    OString aCustomFormat;
    OString aFormat(lcl_ConvertNumberingType(nNumType, nullptr, aCustomFormat));
    if (!aFormat.isEmpty() && aCustomFormat.isEmpty())
        pAttr->add(FSNS(XML_w, XML_fmt), aFormat);

    m_pSerializer->singleElementNS( XML_w, XML_pgNumType, pAttr );

    // see 2.6.12 pgNumType (Page Numbering Settings)
    SAL_INFO("sw.ww8", "TODO DocxAttributeOutput::SectionPageNumbering()" );
}

void DocxAttributeOutput::SectionType( sal_uInt8 nBreakCode )
{
    /*  break code:   0 No break, 1 New column
        2 New page, 3 Even page, 4 Odd page
        */
    const char* pType;
    switch ( nBreakCode )
    {
        case 1:  pType = "nextColumn"; break;
        case 2:  pType = "nextPage";   break;
        case 3:  pType = "evenPage";   break;
        case 4:  pType = "oddPage";    break;
        default: pType = "continuous"; break;
    }

    m_pSerializer->singleElementNS(XML_w, XML_type, FSNS(XML_w, XML_val), pType);
}

void DocxAttributeOutput::TextVerticalAdjustment( const drawing::TextVerticalAdjust nVA )
{
    switch( nVA )
    {
        case drawing::TextVerticalAdjust_CENTER:
            m_pSerializer->singleElementNS(XML_w, XML_vAlign, FSNS(XML_w, XML_val), "center");
            break;
        case drawing::TextVerticalAdjust_BOTTOM:
            m_pSerializer->singleElementNS(XML_w, XML_vAlign, FSNS(XML_w, XML_val), "bottom");
            break;
        case drawing::TextVerticalAdjust_BLOCK:  //justify
            m_pSerializer->singleElementNS(XML_w, XML_vAlign, FSNS(XML_w, XML_val), "both");
            break;
        default:
            break;
    }
}

void DocxAttributeOutput::StartFont( const OUString& rFamilyName ) const
{
    m_pSerializer->startElementNS(XML_w, XML_font, FSNS(XML_w, XML_name), rFamilyName);
}

void DocxAttributeOutput::EndFont() const
{
    m_pSerializer->endElementNS( XML_w, XML_font );
}

void DocxAttributeOutput::FontAlternateName( const OUString& rName ) const
{
    m_pSerializer->singleElementNS(XML_w, XML_altName, FSNS(XML_w, XML_val), rName);
}

void DocxAttributeOutput::FontCharset( sal_uInt8 nCharSet, rtl_TextEncoding nEncoding ) const
{
    rtl::Reference<FastAttributeList> pAttr = FastSerializerHelper::createAttrList();

    OString aCharSet( OString::number( nCharSet, 16 ) );
    if ( aCharSet.getLength() == 1 )
        aCharSet = "0" + aCharSet;
    pAttr->add(FSNS(XML_w, XML_val), aCharSet);

    if (GetExport().GetFilter().getVersion() != oox::core::ECMA_376_1ST_EDITION)
    {
        if( const char* charset = rtl_getMimeCharsetFromTextEncoding( nEncoding ))
            pAttr->add( FSNS( XML_w, XML_characterSet ), charset );
    }

    m_pSerializer->singleElementNS( XML_w, XML_charset, pAttr );
}

void DocxAttributeOutput::FontFamilyType( FontFamily eFamily ) const
{
    const char* pFamily;
    switch ( eFamily )
    {
        case FAMILY_ROMAN:      pFamily = "roman"; break;
        case FAMILY_SWISS:      pFamily = "swiss"; break;
        case FAMILY_MODERN:     pFamily = "modern"; break;
        case FAMILY_SCRIPT:     pFamily = "script"; break;
        case FAMILY_DECORATIVE: pFamily = "decorative"; break;
        default:                pFamily = "auto"; break; // no font family
    }

    m_pSerializer->singleElementNS(XML_w, XML_family, FSNS(XML_w, XML_val), pFamily);
}

void DocxAttributeOutput::FontPitchType( FontPitch ePitch ) const
{
    const char* pPitch;
    switch ( ePitch )
    {
        case PITCH_VARIABLE: pPitch = "variable"; break;
        case PITCH_FIXED:    pPitch = "fixed"; break;
        default:             pPitch = "default"; break; // no info about the pitch
    }

    m_pSerializer->singleElementNS(XML_w, XML_pitch, FSNS(XML_w, XML_val), pPitch);
}

void DocxAttributeOutput::EmbedFont( std::u16string_view name, FontFamily family, FontPitch pitch )
{
    if( !m_rExport.m_rDoc.getIDocumentSettingAccess().get( DocumentSettingId::EMBED_FONTS ))
        return; // no font embedding with this document
    bool foundFont
        = EmbedFontStyle(name, XML_embedRegular, family, ITALIC_NONE, WEIGHT_NORMAL, pitch);
    foundFont
        = EmbedFontStyle(name, XML_embedBold, family, ITALIC_NONE, WEIGHT_BOLD, pitch) || foundFont;
    foundFont = EmbedFontStyle(name, XML_embedItalic, family, ITALIC_NORMAL, WEIGHT_NORMAL, pitch)
                || foundFont;
    foundFont = EmbedFontStyle(name, XML_embedBoldItalic, family, ITALIC_NORMAL, WEIGHT_BOLD, pitch)
                || foundFont;
    if (!foundFont)
        EmbedFontStyle(name, XML_embedRegular, family, ITALIC_NONE, WEIGHT_DONTKNOW, pitch);
}

static char toHexChar( int value )
{
    return value >= 10 ? value + 'A' - 10 : value + '0';
}

bool DocxAttributeOutput::EmbedFontStyle(std::u16string_view name, int tag, FontFamily family,
                                         FontItalic italic, FontWeight weight, FontPitch pitch)
{
    // Embed font if at least viewing is allowed (in which case the opening app must check
    // the font license rights too and open either read-only or not use the font for editing).
    OUString fontUrl = EmbeddedFontsHelper::fontFileUrl( name, family, italic, weight, pitch,
        EmbeddedFontsHelper::FontRights::ViewingAllowed );
    if( fontUrl.isEmpty())
        return false;
    // TODO IDocumentSettingAccess::EMBED_SYSTEM_FONTS
    if( !m_FontFilesMap.count( fontUrl ))
    {
        osl::File file( fontUrl );
        if( file.open( osl_File_OpenFlag_Read ) != osl::File::E_None )
            return false;
        uno::Reference< css::io::XOutputStream > xOutStream = m_rExport.GetFilter().openFragmentStream(
            "word/fonts/font" + OUString::number(m_nextFontId) + ".odttf",
            u"application/vnd.openxmlformats-officedocument.obfuscatedFont"_ustr );
        // Not much point in trying hard with the obfuscation key, whoever reads the spec can read the font anyway,
        // so just alter the first and last part of the key.
        char fontKeyStr[] = "{00014A78-CABC-4EF0-12AC-5CD89AEFDE00}";
        sal_uInt8 fontKey[ 16 ] = { 0, 0xDE, 0xEF, 0x9A, 0xD8, 0x5C, 0xAC, 0x12, 0xF0, 0x4E,
            0xBC, 0xCA, 0x78, 0x4A, 0x01, 0 };
        fontKey[ 0 ] = fontKey[ 15 ] = m_nextFontId % 256;
        fontKeyStr[ 1 ] = fontKeyStr[ 35 ] = toHexChar(( m_nextFontId % 256 ) / 16 );
        fontKeyStr[ 2 ] = fontKeyStr[ 36 ] = toHexChar(( m_nextFontId % 256 ) % 16 );
        unsigned char buffer[ 4096 ];
        sal_uInt64 readSize;
        file.read( buffer, 32, readSize );
        if( readSize < 32 )
        {
            SAL_WARN( "sw.ww8", "Font file size too small (" << fontUrl << ")" );
            xOutStream->closeOutput();
            return false;
        }
        for( int i = 0;
             i < 16;
             ++i )
        {
            buffer[ i ] ^= fontKey[ i ];
            buffer[ i + 16 ] ^= fontKey[ i ];
        }
        xOutStream->writeBytes( uno::Sequence< sal_Int8 >( reinterpret_cast< const sal_Int8* >( buffer ), 32 ));
        for(;;)
        {
            sal_Bool eof;
            if( file.isEndOfFile( &eof ) != osl::File::E_None )
            {
                SAL_WARN( "sw.ww8", "Error reading font file " << fontUrl );
                xOutStream->closeOutput();
                return false;
            }
            if( eof )
                break;
            if( file.read( buffer, 4096, readSize ) != osl::File::E_None )
            {
                SAL_WARN( "sw.ww8", "Error reading font file " << fontUrl );
                xOutStream->closeOutput();
                return false;
            }
            if( readSize == 0 )
                break;
            // coverity[overrun-buffer-arg : FALSE] - coverity has difficulty with css::uno::Sequence
            xOutStream->writeBytes( uno::Sequence< sal_Int8 >( reinterpret_cast< const sal_Int8* >( buffer ), readSize ));
        }
        xOutStream->closeOutput();
        EmbeddedFontRef ref;
        ref.relId = OUStringToOString( GetExport().GetFilter().addRelation( m_pSerializer->getOutputStream(),
            oox::getRelationship(Relationship::FONT),
            Concat2View("fonts/font" + OUString::number( m_nextFontId ) + ".odttf") ), RTL_TEXTENCODING_UTF8 );
        ref.fontKey = fontKeyStr;
        m_FontFilesMap[ fontUrl ] = std::move(ref);
        ++m_nextFontId;
    }
    m_pSerializer->singleElementNS( XML_w, tag,
        FSNS( XML_r, XML_id ), m_FontFilesMap[ fontUrl ].relId,
        FSNS( XML_w, XML_fontKey ), m_FontFilesMap[ fontUrl ].fontKey );
    return true;
}

OString DocxAttributeOutput::TransHighlightColor( sal_uInt8 nIco )
{
    switch (nIco)
    {
        case 0: return "none"_ostr; break;
        case 1: return "black"_ostr; break;
        case 2: return "blue"_ostr; break;
        case 3: return "cyan"_ostr; break;
        case 4: return "green"_ostr; break;
        case 5: return "magenta"_ostr; break;
        case 6: return "red"_ostr; break;
        case 7: return "yellow"_ostr; break;
        case 8: return "white"_ostr; break;
        case 9: return "darkBlue"_ostr; break;
        case 10: return "darkCyan"_ostr; break;
        case 11: return "darkGreen"_ostr; break;
        case 12: return "darkMagenta"_ostr; break;
        case 13: return "darkRed"_ostr; break;
        case 14: return "darkYellow"_ostr; break;
        case 15: return "darkGray"_ostr; break;
        case 16: return "lightGray"_ostr; break;
        default: return OString(); break;
    }
}

void DocxAttributeOutput::NumberingDefinition( sal_uInt16 nId, const SwNumRule &rRule )
{
    // nId is the same both for abstract numbering definition as well as the
    // numbering definition itself
    // TODO check that this is actually true & fix if not ;-)
    OString aId( OString::number( nId ) );

    m_pSerializer->startElementNS(XML_w, XML_num, FSNS(XML_w, XML_numId), aId);

    m_pSerializer->singleElementNS(XML_w, XML_abstractNumId, FSNS(XML_w, XML_val), aId);

#if OSL_DEBUG_LEVEL > 1
    // TODO ww8 version writes this, anything to do about it here?
    if ( rRule.IsContinusNum() )
        SAL_INFO("sw", "TODO DocxAttributeOutput::NumberingDefinition()" );
#else
    (void) rRule; // to quiet the warning...
#endif

    m_pSerializer->endElementNS( XML_w, XML_num );
}

// Not all attributes of SwNumFormat are important for export, so can't just use embedded in
// that classes comparison.
static bool lcl_ListLevelsAreDifferentForExport(const SwNumFormat & rFormat1, const SwNumFormat & rFormat2)
{
    if (rFormat1 == rFormat2)
        // They are equal, nothing to do
        return false;

    if (!rFormat1.GetCharFormat() != !rFormat2.GetCharFormat())
        // One has charformat, other not. they are different
        return true;

    if (rFormat1.GetCharFormat() && rFormat2.GetCharFormat())
    {
        const SwAttrSet & a1 = rFormat1.GetCharFormat()->GetAttrSet();
        const SwAttrSet & a2 = rFormat2.GetCharFormat()->GetAttrSet();

        if (!(a1 == a2))
            // Difference in charformat: they are different
            return true;
    }

    // Compare numformats with empty charformats
    SwNumFormat modified1 = rFormat1;
    SwNumFormat modified2 = rFormat2;
    modified1.SetCharFormatName(OUString());
    modified2.SetCharFormatName(OUString());
    modified1.SetCharFormat(nullptr);
    modified2.SetCharFormat(nullptr);
    return modified1 != modified2;
}

void DocxAttributeOutput::OverrideNumberingDefinition(
        SwNumRule const& rRule,
        sal_uInt16 const nNum, sal_uInt16 const nAbstractNum, const std::map< size_t, size_t > & rLevelOverrides )
{
    m_pSerializer->startElementNS(XML_w, XML_num, FSNS(XML_w, XML_numId), OString::number(nNum));

    m_pSerializer->singleElementNS(XML_w, XML_abstractNumId, FSNS(XML_w, XML_val), OString::number(nAbstractNum));

    SwNumRule const& rAbstractRule = *(*m_rExport.m_pUsedNumTable)[nAbstractNum - 1];
    sal_uInt8 const nLevels = static_cast<sal_uInt8>(rRule.IsContinusNum()
        ? WW8ListManager::nMinLevel : WW8ListManager::nMaxLevel);
    sal_uInt8 nPreviousOverrideLevel = 0;
    for (sal_uInt8 nLevel = 0; nLevel < nLevels; ++nLevel)
    {
        const auto levelOverride = rLevelOverrides.find(nLevel);
        bool bListsAreDifferent = lcl_ListLevelsAreDifferentForExport(rRule.Get(nLevel), rAbstractRule.Get(nLevel));

        // Export list override only if it is different to abstract one
        // or we have a level numbering override
        if (bListsAreDifferent || levelOverride != rLevelOverrides.end())
        {
            // If there are "gaps" in w:lvlOverride numbers, MS Word can have issues with numbering.
            // So we need to emit default override tokens up to current one.
            while (nPreviousOverrideLevel < nLevel)
            {
                const SwNumFormat& rFormat = rRule.Get(nPreviousOverrideLevel);
                m_pSerializer->startElementNS(XML_w, XML_lvlOverride, FSNS(XML_w, XML_ilvl), OString::number(nPreviousOverrideLevel));
                // tdf#153104: absent startOverride is treated by Word as "startOverride value 0".
                m_pSerializer->singleElementNS(XML_w, XML_startOverride, FSNS(XML_w, XML_val), OString::number(rFormat.GetStart()));
                m_pSerializer->endElementNS(XML_w, XML_lvlOverride);
                nPreviousOverrideLevel++;
            }

            m_pSerializer->startElementNS(XML_w, XML_lvlOverride, FSNS(XML_w, XML_ilvl), OString::number(nLevel));

            if (bListsAreDifferent)
            {
                GetExport().NumberingLevel(rRule, nLevel);
            }
            if (levelOverride != rLevelOverrides.end())
            {
                // list numbering restart override
                m_pSerializer->singleElementNS(XML_w, XML_startOverride,
                    FSNS(XML_w, XML_val), OString::number(levelOverride->second));
            }

            m_pSerializer->endElementNS(XML_w, XML_lvlOverride);
        }
    }

    m_pSerializer->endElementNS( XML_w, XML_num );
}

void DocxAttributeOutput::StartAbstractNumbering( sal_uInt16 nId )
{
    const SwNumRule* pRule = (*m_rExport.m_pUsedNumTable)[nId - 1];
    m_bExportingOutline = pRule && pRule->IsOutlineRule();
    m_pSerializer->startElementNS( XML_w, XML_abstractNum,
            FSNS( XML_w, XML_abstractNumId ), OString::number(nId) );
}

void DocxAttributeOutput::EndAbstractNumbering()
{
    m_pSerializer->endElementNS( XML_w, XML_abstractNum );
}

void DocxAttributeOutput::NumberingLevel( sal_uInt8 nLevel,
        sal_uInt16 nStart,
        sal_uInt16 nNumberingType,
        SvxAdjust eAdjust,
        const sal_uInt8 * /*pNumLvlPos*/,
        sal_uInt8 nFollow,
        const wwFont *pFont,
        const SfxItemSet *pOutSet,
        sal_Int16 nIndentAt,
        sal_Int16 nFirstLineIndex,
        sal_Int16 nListTabPos,
        const OUString &rNumberingString,
        const SvxBrushItem* pBrush,
        bool isLegal)
{
    m_pSerializer->startElementNS(XML_w, XML_lvl, FSNS(XML_w, XML_ilvl), OString::number(nLevel));

    // start with the nStart value. Do not write w:start if Numbered Lists
    // starts from zero.As it's an optional parameter.
    // refer ECMA 376 Second edition Part-1
    if(0 != nLevel || 0 != nStart)
    {
        m_pSerializer->singleElementNS( XML_w, XML_start,
                FSNS( XML_w, XML_val ), OString::number(nStart) );
    }

    if (m_bExportingOutline)
    {
        sal_uInt16 nId = m_rExport.m_pStyles->GetHeadingParagraphStyleId( nLevel );
        if ( nId != SAL_MAX_UINT16 )
            m_pSerializer->singleElementNS( XML_w, XML_pStyle ,
                FSNS( XML_w, XML_val ), m_rExport.m_pStyles->GetStyleId(nId) );
    }

    if (isLegal)
        m_pSerializer->singleElementNS(XML_w, XML_isLgl);

    // format
    OString aCustomFormat;
    OString aFormat(lcl_ConvertNumberingType(nNumberingType, pOutSet, aCustomFormat, "decimal"_ostr));

    {
        if (aCustomFormat.isEmpty())
        {
            m_pSerializer->singleElementNS(XML_w, XML_numFmt, FSNS(XML_w, XML_val), aFormat);
        }
        else
        {
            m_pSerializer->startElementNS(XML_mc, XML_AlternateContent);
            m_pSerializer->startElementNS(XML_mc, XML_Choice, XML_Requires, "w14");

            m_pSerializer->singleElementNS(XML_w, XML_numFmt, FSNS(XML_w, XML_val), aFormat,
                                           FSNS(XML_w, XML_format), aCustomFormat);

            m_pSerializer->endElementNS(XML_mc, XML_Choice);
            m_pSerializer->startElementNS(XML_mc, XML_Fallback);
            m_pSerializer->singleElementNS(XML_w, XML_numFmt, FSNS(XML_w, XML_val), "decimal");
            m_pSerializer->endElementNS(XML_mc, XML_Fallback);
            m_pSerializer->endElementNS(XML_mc, XML_AlternateContent);
        }
    }

    // suffix
    const char *pSuffix = nullptr;
    switch ( nFollow )
    {
        case 1:  pSuffix = "space";   break;
        case 2:  pSuffix = "nothing"; break;
        default: /*pSuffix = "tab";*/ break;
    }
    if ( pSuffix )
        m_pSerializer->singleElementNS(XML_w, XML_suff, FSNS(XML_w, XML_val), pSuffix);

    // text
    OUStringBuffer aBuffer( rNumberingString.getLength() + WW8ListManager::nMaxLevel );

    const sal_Unicode *pPrev = rNumberingString.getStr();
    const sal_Unicode *pIt = rNumberingString.getStr();
    while ( pIt < rNumberingString.getStr() + rNumberingString.getLength() )
    {
        // convert the level values to %NUMBER form
        // (we don't use pNumLvlPos at all)
        // FIXME so far we support the ww8 limit of levels only
        if ( *pIt < sal_Unicode( WW8ListManager::nMaxLevel ) )
        {
            aBuffer.append( OUString::Concat(std::u16string_view(pPrev, pIt - pPrev))
                + "%"
                + OUString::number(sal_Int32( *pIt ) + 1 ));

            pPrev = pIt + 1;
        }
        ++pIt;
    }
    if ( pPrev < pIt )
        aBuffer.append( pPrev, pIt - pPrev );

    // If bullet char is empty, set lvlText as empty
    if ( rNumberingString == OUStringChar('\0') && nNumberingType == SVX_NUM_CHAR_SPECIAL )
    {
        m_pSerializer->singleElementNS(XML_w, XML_lvlText, FSNS(XML_w, XML_val), "");
    }
    else
    {
        // Writer's "zero width space" suffix is necessary, so that LabelFollowedBy shows up, but Word doesn't require that.
        OUString aLevelText = aBuffer.makeStringAndClear();
        static OUString aZeroWidthSpace(u'\x200B');
        if (aLevelText == aZeroWidthSpace)
            aLevelText.clear();
        m_pSerializer->singleElementNS(XML_w, XML_lvlText, FSNS(XML_w, XML_val), aLevelText);
    }

    // bullet
    if (nNumberingType == SVX_NUM_BITMAP && pBrush)
    {
        int nIndex = m_rExport.GetGrfIndex(*pBrush);
        if (nIndex != -1)
        {
            m_pSerializer->singleElementNS(XML_w, XML_lvlPicBulletId,
                    FSNS(XML_w, XML_val), OString::number(nIndex));
        }
    }

    // justification
    const char *pJc;
    bool const ecmaDialect = m_rExport.GetFilter().getVersion() == oox::core::ECMA_376_1ST_EDITION;
    switch ( eAdjust )
    {
        case SvxAdjust::Center: pJc = "center"; break;
        case SvxAdjust::Right:  pJc = !ecmaDialect ? "end" : "right";  break;
        default:                pJc = !ecmaDialect ? "start" : "left";   break;
    }
    m_pSerializer->singleElementNS(XML_w, XML_lvlJc, FSNS(XML_w, XML_val), pJc);

    // indentation
    m_pSerializer->startElementNS(XML_w, XML_pPr);
    if( nListTabPos >= 0 )
    {
        m_pSerializer->startElementNS(XML_w, XML_tabs);
        m_pSerializer->singleElementNS( XML_w, XML_tab,
                FSNS( XML_w, XML_val ), "num",
                FSNS( XML_w, XML_pos ), OString::number(nListTabPos) );
        m_pSerializer->endElementNS( XML_w, XML_tabs );
    }

    sal_Int32 nToken = ecmaDialect ? XML_left : XML_start;
    sal_Int32 nIndentToken = nFirstLineIndex > 0 ? XML_firstLine : XML_hanging;
    m_pSerializer->singleElementNS( XML_w, XML_ind,
            FSNS( XML_w, nToken ), OString::number(nIndentAt),
            FSNS( XML_w, nIndentToken ), OString::number(abs(nFirstLineIndex)) );
    m_pSerializer->endElementNS( XML_w, XML_pPr );

    // font
    if ( pOutSet )
    {
        m_pSerializer->startElementNS(XML_w, XML_rPr);

        SfxItemSet aTempSet(*pOutSet);
        if ( pFont )
        {
            GetExport().GetId( *pFont ); // ensure font info is written to fontTable.xml
            OString aFamilyName( OUStringToOString( pFont->GetFamilyName(), RTL_TEXTENCODING_UTF8 ) );
            m_pSerializer->singleElementNS( XML_w, XML_rFonts,
                    FSNS( XML_w, XML_ascii ), aFamilyName,
                    FSNS( XML_w, XML_hAnsi ), aFamilyName,
                    FSNS( XML_w, XML_cs ), aFamilyName,
                    FSNS( XML_w, XML_hint ), "default" );
            aTempSet.ClearItem(RES_CHRATR_FONT);
            aTempSet.ClearItem(RES_CHRATR_CTL_FONT);
        }
        m_rExport.OutputItemSet(aTempSet, false, true, i18n::ScriptType::LATIN, m_rExport.m_bExportModeRTF);

        WriteCollectedRunProperties();

        m_pSerializer->endElementNS( XML_w, XML_rPr );
    }

    // TODO anything to do about nListTabPos?

    m_pSerializer->endElementNS( XML_w, XML_lvl );
}

void DocxAttributeOutput::CharCaseMap( const SvxCaseMapItem& rCaseMap )
{
    switch ( rCaseMap.GetValue() )
    {
        case SvxCaseMap::SmallCaps:
            m_pSerializer->singleElementNS(XML_w, XML_smallCaps);
            break;
        case SvxCaseMap::Uppercase:
            m_pSerializer->singleElementNS(XML_w, XML_caps);
            break;
        default: // Something that ooxml does not support
            m_pSerializer->singleElementNS(XML_w, XML_smallCaps, FSNS(XML_w, XML_val), "false");
            m_pSerializer->singleElementNS(XML_w, XML_caps, FSNS(XML_w, XML_val), "false");
            break;
    }
}

void DocxAttributeOutput::CharColor(const SvxColorItem& rColorItem)
{
    const Color aColor = rColorItem.getColor();
    const model::ComplexColor& aComplexColor = rColorItem.getComplexColor();

    OString aColorString = msfilter::util::ConvertColor(aColor);

    std::string_view pExistingValue;
    if (m_pColorAttrList.is() && m_pColorAttrList->getAsView(FSNS(XML_w, XML_val), pExistingValue))
    {
        assert(aColorString.equalsL(pExistingValue.data(), pExistingValue.size()));
        return;
    }

    lclAddThemeColorAttributes(m_pColorAttrList, aComplexColor);

    AddToAttrList(m_pColorAttrList, FSNS(XML_w, XML_val), aColorString);
    m_nCharTransparence = 255 - aColor.GetAlpha();
    m_aComplexColor = aComplexColor;
}

void DocxAttributeOutput::CharContour( const SvxContourItem& rContour )
{
    if ( rContour.GetValue() )
        m_pSerializer->singleElementNS(XML_w, XML_outline);
    else
        m_pSerializer->singleElementNS(XML_w, XML_outline, FSNS(XML_w, XML_val), "false");
}

void DocxAttributeOutput::CharCrossedOut( const SvxCrossedOutItem& rCrossedOut )
{
    switch ( rCrossedOut.GetStrikeout() )
    {
        case STRIKEOUT_DOUBLE:
            m_pSerializer->singleElementNS(XML_w, XML_dstrike);
            break;
        case STRIKEOUT_NONE:
            m_pSerializer->singleElementNS(XML_w, XML_dstrike, FSNS(XML_w, XML_val), "false");
            m_pSerializer->singleElementNS(XML_w, XML_strike, FSNS(XML_w, XML_val), "false");
            break;
        default:
            m_pSerializer->singleElementNS(XML_w, XML_strike);
            break;
    }
}

void DocxAttributeOutput::CharEscapement( const SvxEscapementItem& rEscapement )
{
    OString sIss;
    short nEsc = rEscapement.GetEsc(), nProp = rEscapement.GetProportionalHeight();

    bool bParaStyle = false;
    if (m_rExport.m_bStyDef && m_rExport.m_pCurrentStyle)
    {
        bParaStyle = m_rExport.m_pCurrentStyle->Which() == RES_TXTFMTCOLL;
    }

    // Simplify styles to avoid impossible complexity. Import and export as defaults only
    if ( m_rExport.m_bStyDef && nEsc && !(bParaStyle && nEsc < 0))
    {
        nProp = DFLT_ESC_PROP;
        nEsc = (nEsc > 0) ? DFLT_ESC_AUTO_SUPER : DFLT_ESC_AUTO_SUB;
    }

    if ( !nEsc )
    {
        sIss = "baseline"_ostr;
        nEsc = 0;
        nProp = 100;
    }
    else if ( DFLT_ESC_PROP == nProp || nProp < 1 || nProp > 100 )
    {
        if ( DFLT_ESC_SUB == nEsc || DFLT_ESC_AUTO_SUB == nEsc )
            sIss = "subscript"_ostr;
        else if ( DFLT_ESC_SUPER == nEsc || DFLT_ESC_AUTO_SUPER == nEsc )
            sIss = "superscript"_ostr;
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

    if ( !sIss.isEmpty() )
        m_pSerializer->singleElementNS(XML_w, XML_vertAlign, FSNS(XML_w, XML_val), sIss);

    if (!(sIss.isEmpty() || sIss.match("baseline")))
        return;

    const SvxFontHeightItem& rItem = m_rExport.GetItem(RES_CHRATR_FONTSIZE);
    float fHeight = rItem.GetHeight();
    OString sPos = OString::number( round(( fHeight * nEsc ) / 1000) );
    m_pSerializer->singleElementNS(XML_w, XML_position, FSNS(XML_w, XML_val), sPos);

    if( ( 100 != nProp || sIss.match( "baseline" ) ) && !m_rExport.m_bFontSizeWritten )
    {
        OString sSize = OString::number( round(( fHeight * nProp ) / 1000) );
        m_pSerializer->singleElementNS(XML_w, XML_sz, FSNS(XML_w, XML_val), sSize);
    }
}

void DocxAttributeOutput::CharFont( const SvxFontItem& rFont)
{
    GetExport().GetId( rFont ); // ensure font info is written to fontTable.xml
    const OUString& sFontName(rFont.GetFamilyName());
    if (sFontName.isEmpty())
        return;

    if (m_pFontsAttrList &&
        (   m_pFontsAttrList->hasAttribute(FSNS( XML_w, XML_ascii )) ||
            m_pFontsAttrList->hasAttribute(FSNS( XML_w, XML_hAnsi ))    )
        )
    {
        // tdf#38778: do to fields output into DOC the font could be added before and after field declaration
        // that all sub runs of the field will have correct font inside.
        // For DOCX we should do not add the same font information twice in the same node
        return;
    }

    AddToAttrList( m_pFontsAttrList,
        FSNS( XML_w, XML_ascii ), sFontName,
        FSNS( XML_w, XML_hAnsi ), sFontName );
}

void DocxAttributeOutput::CharFontSize( const SvxFontHeightItem& rFontSize)
{
    OString fontSize = OString::number( ( rFontSize.GetHeight() + 5 ) / 10 );

    switch ( rFontSize.Which() )
    {
        case RES_CHRATR_FONTSIZE:
        case RES_CHRATR_CJK_FONTSIZE:
            m_pSerializer->singleElementNS(XML_w, XML_sz, FSNS(XML_w, XML_val), fontSize);
            break;
        case RES_CHRATR_CTL_FONTSIZE:
            m_pSerializer->singleElementNS(XML_w, XML_szCs, FSNS(XML_w, XML_val), fontSize);
            break;
    }
}

void DocxAttributeOutput::CharKerning( const SvxKerningItem& rKerning )
{
    OString aKerning = OString::number(  rKerning.GetValue() );
    m_pSerializer->singleElementNS(XML_w, XML_spacing, FSNS(XML_w, XML_val), aKerning);
}

void DocxAttributeOutput::CharLanguage( const SvxLanguageItem& rLanguage )
{
    OUString aLanguageCode(LanguageTag( rLanguage.GetLanguage()).getBcp47MS());

    switch ( rLanguage.Which() )
    {
        case RES_CHRATR_LANGUAGE:
            AddToAttrList( m_pCharLangAttrList, FSNS( XML_w, XML_val ), aLanguageCode );
            break;
        case RES_CHRATR_CJK_LANGUAGE:
            AddToAttrList( m_pCharLangAttrList, FSNS( XML_w, XML_eastAsia ), aLanguageCode );
            break;
        case RES_CHRATR_CTL_LANGUAGE:
            AddToAttrList( m_pCharLangAttrList, FSNS( XML_w, XML_bidi ), aLanguageCode );
            break;
    }
}

void DocxAttributeOutput::CharPosture( const SvxPostureItem& rPosture )
{
    if ( rPosture.GetPosture() != ITALIC_NONE )
        m_pSerializer->singleElementNS(XML_w, XML_i);
    else
        m_pSerializer->singleElementNS(XML_w, XML_i, FSNS(XML_w, XML_val), "false");
}

void DocxAttributeOutput::CharShadow( const SvxShadowedItem& rShadow )
{
    if ( rShadow.GetValue() )
        m_pSerializer->singleElementNS(XML_w, XML_shadow);
    else
        m_pSerializer->singleElementNS(XML_w, XML_shadow, FSNS(XML_w, XML_val), "false");
}

void DocxAttributeOutput::CharUnderline( const SvxUnderlineItem& rUnderline )
{
    const char *pUnderlineValue;

    switch ( rUnderline.GetLineStyle() )
    {
        case LINESTYLE_SINGLE:         pUnderlineValue = "single";          break;
        case LINESTYLE_BOLD:           pUnderlineValue = "thick";           break;
        case LINESTYLE_DOUBLE:         pUnderlineValue = "double";          break;
        case LINESTYLE_DOTTED:         pUnderlineValue = "dotted";          break;
        case LINESTYLE_DASH:           pUnderlineValue = "dash";            break;
        case LINESTYLE_DASHDOT:        pUnderlineValue = "dotDash";         break;
        case LINESTYLE_DASHDOTDOT:     pUnderlineValue = "dotDotDash";      break;
        case LINESTYLE_WAVE:           pUnderlineValue = "wave";            break;
        case LINESTYLE_BOLDDOTTED:     pUnderlineValue = "dottedHeavy";     break;
        case LINESTYLE_BOLDDASH:       pUnderlineValue = "dashedHeavy";     break;
        case LINESTYLE_LONGDASH:       pUnderlineValue = "dashLongHeavy";   break;
        case LINESTYLE_BOLDLONGDASH:   pUnderlineValue = "dashLongHeavy";   break;
        case LINESTYLE_BOLDDASHDOT:    pUnderlineValue = "dashDotHeavy";    break;
        case LINESTYLE_BOLDDASHDOTDOT: pUnderlineValue = "dashDotDotHeavy"; break;
        case LINESTYLE_BOLDWAVE:       pUnderlineValue = "wavyHeavy";       break;
        case LINESTYLE_DOUBLEWAVE:     pUnderlineValue = "wavyDouble";      break;
        case LINESTYLE_NONE:           // fall through
        default:                       pUnderlineValue = "none";            break;
    }

    Color aUnderlineColor = rUnderline.GetColor();
    bool  bUnderlineHasColor = !aUnderlineColor.IsTransparent();
    if (bUnderlineHasColor)
    {
        model::ComplexColor const& rComplexColor = rUnderline.getComplexColor();
        // Underline has a color
        rtl::Reference<FastAttributeList> pAttrList = FastSerializerHelper::createAttrList();
        pAttrList->add(FSNS(XML_w, XML_val), pUnderlineValue);
        pAttrList->add(FSNS(XML_w, XML_color), msfilter::util::ConvertColor(aUnderlineColor));
        lclAddThemeColorAttributes(pAttrList, rComplexColor);
        m_pSerializer->singleElementNS(XML_w, XML_u, pAttrList);

    }
    else
    {
        // Underline has no color
        m_pSerializer->singleElementNS(XML_w, XML_u, FSNS(XML_w, XML_val), pUnderlineValue);
    }
}

void DocxAttributeOutput::CharWeight( const SvxWeightItem& rWeight )
{
    if ( rWeight.GetWeight() == WEIGHT_BOLD )
        m_pSerializer->singleElementNS(XML_w, XML_b);
    else
        m_pSerializer->singleElementNS(XML_w, XML_b, FSNS(XML_w, XML_val), "false");
}

void DocxAttributeOutput::CharAutoKern( const SvxAutoKernItem& rAutoKern )
{
    // auto kerning is bound to a minimum font size in Word - but is just a boolean in Writer :-(
    // kerning is based on half-point sizes, so 2 enables kerning for fontsize 1pt or higher. (1 is treated as size 12, and 0 is treated as disabled.)
    const OString sFontSize = OString::number( static_cast<sal_uInt32>(rAutoKern.GetValue()) * 2 );
    m_pSerializer->singleElementNS(XML_w, XML_kern, FSNS(XML_w, XML_val), sFontSize);
}

void DocxAttributeOutput::CharAnimatedText( const SvxBlinkItem& rBlink )
{
    if ( rBlink.GetValue() )
        m_pSerializer->singleElementNS(XML_w, XML_effect, FSNS(XML_w, XML_val), "blinkBackground");
    else
        m_pSerializer->singleElementNS(XML_w, XML_effect, FSNS(XML_w, XML_val), "none");
}

constexpr OUStringLiteral MSWORD_CH_SHADING_FILL = u"FFFFFF"; // The attribute w:fill of w:shd, for MS-Word's character shading,
constexpr OUStringLiteral MSWORD_CH_SHADING_COLOR = u"auto"; // The attribute w:color of w:shd, for MS-Word's character shading,
constexpr OUStringLiteral MSWORD_CH_SHADING_VAL = u"pct15"; // The attribute w:value of w:shd, for MS-Word's character shading,

void DocxAttributeOutput::CharBackground( const SvxBrushItem& rBrush )
{
    // Check if the brush shading pattern is 'PCT15'. If so - write it back to the DOCX
    if (rBrush.GetShadingValue() == ShadingPattern::PCT15)
    {
        m_pSerializer->singleElementNS( XML_w, XML_shd,
            FSNS( XML_w, XML_val ), MSWORD_CH_SHADING_VAL,
            FSNS( XML_w, XML_color ), MSWORD_CH_SHADING_COLOR,
            FSNS( XML_w, XML_fill ), MSWORD_CH_SHADING_FILL );
    }
    else
    {
        m_pSerializer->singleElementNS( XML_w, XML_shd,
            FSNS( XML_w, XML_fill ), msfilter::util::ConvertColor(rBrush.GetColor()),
            FSNS( XML_w, XML_val ), "clear" );
    }
}

void DocxAttributeOutput::CharFontCJK( const SvxFontItem& rFont )
{
    if (m_pFontsAttrList && m_pFontsAttrList->hasAttribute(FSNS(XML_w, XML_eastAsia)))
    {
        // tdf#38778: do to fields output into DOC the font could be added before and after field declaration
        // that all sub runs of the field will have correct font inside.
        // For DOCX we should do not add the same font information twice in the same node
        return;
    }

    AddToAttrList( m_pFontsAttrList, FSNS( XML_w, XML_eastAsia ), rFont.GetFamilyName() );
}

void DocxAttributeOutput::CharPostureCJK( const SvxPostureItem& rPosture )
{
    if ( rPosture.GetPosture() != ITALIC_NONE )
        m_pSerializer->singleElementNS(XML_w, XML_i);
    else
        m_pSerializer->singleElementNS(XML_w, XML_i, FSNS(XML_w, XML_val), "false");
}

void DocxAttributeOutput::CharWeightCJK( const SvxWeightItem& rWeight )
{
    if ( rWeight.GetWeight() == WEIGHT_BOLD )
        m_pSerializer->singleElementNS(XML_w, XML_b);
    else
        m_pSerializer->singleElementNS(XML_w, XML_b, FSNS(XML_w, XML_val), "false");
}

void DocxAttributeOutput::CharFontCTL( const SvxFontItem& rFont )
{
    if (m_pFontsAttrList && m_pFontsAttrList->hasAttribute(FSNS(XML_w, XML_cs)))
    {
        // tdf#38778: do to fields output into DOC the font could be added before and after field declaration
        // that all sub runs of the field will have correct font inside.
        // For DOCX we should do not add the same font information twice in the same node
        return;
    }

    AddToAttrList( m_pFontsAttrList, FSNS( XML_w, XML_cs ), rFont.GetFamilyName() );
}

void DocxAttributeOutput::CharPostureCTL( const SvxPostureItem& rPosture)
{
    if ( rPosture.GetPosture() != ITALIC_NONE )
        m_pSerializer->singleElementNS(XML_w, XML_iCs);
    else
        m_pSerializer->singleElementNS(XML_w, XML_iCs, FSNS(XML_w, XML_val), "false");
}

void DocxAttributeOutput::CharWeightCTL( const SvxWeightItem& rWeight )
{
    if ( rWeight.GetWeight() == WEIGHT_BOLD )
        m_pSerializer->singleElementNS(XML_w, XML_bCs);
    else
        m_pSerializer->singleElementNS(XML_w, XML_bCs, FSNS(XML_w, XML_val), "false");
}

void DocxAttributeOutput::CharBidiRTL( const SfxPoolItem& )
{
}

void DocxAttributeOutput::CharRotate( const SvxCharRotateItem& rRotate)
{
    // Not rotated?
    if ( !rRotate.GetValue())
        return;

    AddToAttrList( m_pEastAsianLayoutAttrList, FSNS( XML_w, XML_vert ), "true" );

    if (rRotate.IsFitToLine())
        AddToAttrList( m_pEastAsianLayoutAttrList, FSNS( XML_w, XML_vertCompress ), "true" );
}

void DocxAttributeOutput::CharEmphasisMark( const SvxEmphasisMarkItem& rEmphasisMark )
{
    const char *pEmphasis;
    const FontEmphasisMark v = rEmphasisMark.GetEmphasisMark();

    if (v == (FontEmphasisMark::Dot | FontEmphasisMark::PosAbove))
        pEmphasis = "dot";
    else if (v == (FontEmphasisMark::Accent | FontEmphasisMark::PosAbove))
        pEmphasis = "comma";
    else if (v == (FontEmphasisMark::Circle | FontEmphasisMark::PosAbove))
        pEmphasis = "circle";
    else if (v == (FontEmphasisMark::Dot|FontEmphasisMark::PosBelow))
        pEmphasis = "underDot";
    else
        pEmphasis = "none";

    m_pSerializer->singleElementNS(XML_w, XML_em, FSNS(XML_w, XML_val), pEmphasis);
}

void DocxAttributeOutput::CharTwoLines( const SvxTwoLinesItem& rTwoLines )
{
    if ( !rTwoLines.GetValue() )
        return;

    AddToAttrList( m_pEastAsianLayoutAttrList, FSNS( XML_w, XML_combine ), "true" );

    sal_Unicode cStart = rTwoLines.GetStartBracket();
    sal_Unicode cEnd = rTwoLines.GetEndBracket();

    if (!cStart && !cEnd)
        return;

    std::string_view sBracket;
    if ((cStart == '{') || (cEnd == '}'))
        sBracket = "curly";
    else if ((cStart == '<') || (cEnd == '>'))
        sBracket = "angle";
    else if ((cStart == '[') || (cEnd == ']'))
        sBracket = "square";
    else
        sBracket = "round";
    AddToAttrList( m_pEastAsianLayoutAttrList, FSNS( XML_w, XML_combineBrackets ), sBracket );
}

void DocxAttributeOutput::CharScaleWidth( const SvxCharScaleWidthItem& rScaleWidth )
{
    // Clamp CharScaleWidth to OOXML limits ([1..600])
    const sal_Int16 nScaleWidth( std::max<sal_Int16>( 1,
        std::min<sal_Int16>( rScaleWidth.GetValue(), 600 ) ) );
    m_pSerializer->singleElementNS( XML_w, XML_w,
        FSNS( XML_w, XML_val ), OString::number(nScaleWidth) );
}

void DocxAttributeOutput::CharRelief( const SvxCharReliefItem& rRelief )
{
    switch ( rRelief.GetValue() )
    {
        case FontRelief::Embossed:
            m_pSerializer->singleElementNS(XML_w, XML_emboss);
            break;
        case FontRelief::Engraved:
            m_pSerializer->singleElementNS(XML_w, XML_imprint);
            break;
        default:
            m_pSerializer->singleElementNS(XML_w, XML_emboss, FSNS(XML_w, XML_val), "false");
            m_pSerializer->singleElementNS(XML_w, XML_imprint, FSNS(XML_w, XML_val), "false");
            break;
    }
}

void DocxAttributeOutput::CharHidden( const SvxCharHiddenItem& rHidden )
{
    if ( rHidden.GetValue() )
    {
        m_pSerializer->singleElementNS(XML_w, XML_vanish);
        // export specVanish for inline headings
        if (m_bOpenedParaPr && m_rExport.m_bParaInlineHeading)
        {
            m_pSerializer->singleElementNS(XML_w, XML_specVanish);
            // don't export extra vanish/specVanish
            m_rExport.m_bParaInlineHeading = false;
        }
    }
    else
        m_pSerializer->singleElementNS(XML_w, XML_vanish, FSNS(XML_w, XML_val), "false");
}

void DocxAttributeOutput::CharBorder(const SvxBoxItem& rBox)
{
    const auto [ pAllBorder, nDist, bShadow ] = FormatCharBorder(rBox);
    css::table::BorderLine2 rStyleBorder;
    const SvxBoxItem* pInherited = nullptr;
    if ( GetExport().m_bStyDef && GetExport().m_pCurrentStyle && GetExport().m_pCurrentStyle->DerivedFrom() )
        pInherited = GetExport().m_pCurrentStyle->DerivedFrom()->GetAttrSet().GetItem<SvxBoxItem>(RES_CHRATR_BOX);
    else if ( m_rExport.m_pChpIter ) // incredibly undocumented, but this is the character-style info, right?
    {
        if (const SvxBoxItem* pPoolItem = GetExport().m_pChpIter->HasTextItem(RES_CHRATR_BOX))
        {
            pInherited = pPoolItem;
        }
    }

    if ( pInherited )
        rStyleBorder = SvxBoxItem::SvxLineToLine(pInherited->GetRight(), false);

    impl_borderLine( m_pSerializer, XML_bdr, pAllBorder, nDist, bShadow, &rStyleBorder );
}

void DocxAttributeOutput::CharHighlight( const SvxBrushItem& rHighlight )
{
    const OString sColor = TransHighlightColor( msfilter::util::TransColToIco(rHighlight.GetColor()) );
    if ( !sColor.isEmpty() )
    {
        m_pSerializer->singleElementNS(XML_w, XML_highlight, FSNS(XML_w, XML_val), sColor);
    }
}

void DocxAttributeOutput::CharScriptHint(const SvxScriptHintItem& rHint)
{
    switch (rHint.GetValue())
    {
        case i18nutil::ScriptHintType::Asian:
            AddToAttrList(m_pFontsAttrList, FSNS(XML_w, XML_hint), "eastAsia");
            break;

        case i18nutil::ScriptHintType::Complex:
            AddToAttrList(m_pFontsAttrList, FSNS(XML_w, XML_hint), "cs");
            break;

        default:
            break;
    }
}

void DocxAttributeOutput::TextINetFormat( const SwFormatINetFormat& rLink )
{
    const SwCharFormat* pFormat = m_rExport.m_rDoc.FindCharFormatByName(rLink.GetINetFormat());
    if (pFormat)
    {
        OString aStyleId(m_rExport.m_pStyles->GetStyleId(m_rExport.GetId(pFormat)));
        if (!aStyleId.equalsIgnoreAsciiCase("DefaultStyle"))
            m_pSerializer->singleElementNS(XML_w, XML_rStyle, FSNS(XML_w, XML_val), aStyleId);
    }
}

void DocxAttributeOutput::TextCharFormat( const SwFormatCharFormat& rCharFormat )
{
    OString aStyleId(m_rExport.m_pStyles->GetStyleId(m_rExport.GetId(rCharFormat.GetCharFormat())));

    m_pSerializer->singleElementNS(XML_w, XML_rStyle, FSNS(XML_w, XML_val), aStyleId);
}

void DocxAttributeOutput::RefField( const SwField&  rField, const OUString& rRef )
{
    SwFieldIds nType = rField.GetTyp( )->Which( );
    if ( nType == SwFieldIds::GetExp )
    {
        OUString sCmd = FieldString( ww::eREF ) +
            "\"" + rRef + "\" ";

        m_rExport.OutputField( &rField, ww::eREF, sCmd );
    }

    // There is nothing to do here for the set fields
}

void DocxAttributeOutput::HiddenField(const SwField& /*rField*/)
{
    SAL_INFO("sw.ww8", "TODO DocxAttributeOutput::HiddenField()" );
}

void DocxAttributeOutput::PostitField( const SwField* pField )
{
    assert( dynamic_cast< const SwPostItField* >( pField ));
    const SwPostItField* pPostItField = static_cast<const SwPostItField*>(pField);
    sal_Int32 nId = 0;
    auto it = m_rOpenedAnnotationMarksIds.find(pPostItField->GetName());
    if (it != m_rOpenedAnnotationMarksIds.end())
        // If the postit field has an annotation mark associated, we already have an id.
        nId = it->second;
    else
        // Otherwise get a new one.
        nId = m_nNextAnnotationMarkId++;
    m_postitFields.emplace_back(pPostItField, PostItDOCXData{ nId });
}

void DocxAttributeOutput::WritePostitFieldReference()
{
    while( m_postitFieldsMaxId < m_postitFields.size())
    {
        OString idstr = OString::number(m_postitFields[m_postitFieldsMaxId].second.id);

        // In case this file is inside annotation marks, we want to write the
        // comment reference after the annotation mark is closed, not here.
        const SwMarkName& idname = m_postitFields[m_postitFieldsMaxId].first->GetName();
        auto it = m_rOpenedAnnotationMarksIds.find( idname );
        if ( it == m_rOpenedAnnotationMarksIds.end(  ) )
            m_pSerializer->singleElementNS(XML_w, XML_commentReference, FSNS(XML_w, XML_id), idstr);
        ++m_postitFieldsMaxId;
    }
}

DocxAttributeOutput::hasProperties DocxAttributeOutput::WritePostitFields()
{
    bool bRemovePersonalInfo = SvtSecurityOptions::IsOptionSet(
        SvtSecurityOptions::EOption::DocWarnRemovePersonalInfo ) && !SvtSecurityOptions::IsOptionSet(
            SvtSecurityOptions::EOption::DocWarnKeepRedlineInfo);

    hasProperties eResult = hasProperties::no;
    for (auto& [f1, data1] : m_postitFields)
    {
        if (f1->GetParentId() != 0 || f1->GetParentPostItId() != 0)
        {
            for (size_t i = 0; i < m_postitFields.size(); i++)
            {
                auto& [f2, data2] = m_postitFields[i];
                if ((f1->GetParentId() != 0 && f2->GetParaId() == f1->GetParentId())
                    || (f1->GetParentPostItId() != 0
                        && f2->GetPostItId() == f1->GetParentPostItId()))
                {
                    if (data2.parentStatus == ParentStatus::None)
                        data2.parentStatus = ParentStatus::IsParent;
                    data1.parentStatus = ParentStatus::HasParent;
                    data1.parentIndex = i;
                    break;
                }
            }
        }
    }
    for (auto& [f, data] : m_postitFields)
    {
        const DateTime aDateTime = f->GetDateTime();
        bool bNoDate = bRemovePersonalInfo ||
            ( aDateTime.GetYear() == 1970 && aDateTime.GetMonth() == 1 && aDateTime.GetDay() == 1 );

        rtl::Reference<sax_fastparser::FastAttributeList> pAttributeList
            = sax_fastparser::FastSerializerHelper::createAttrList();

        pAttributeList->add(FSNS( XML_w, XML_id ), OString::number(data.id));
        pAttributeList->add(FSNS( XML_w, XML_author ), bRemovePersonalInfo
                 ? "Author" + OString::number( GetExport().GetInfoID(f->GetPar1()) )
                 : f->GetPar1().toUtf8());
        if (!bNoDate)
            pAttributeList->add(FSNS( XML_w, XML_date ), DateTimeToOString( aDateTime ));
        pAttributeList->add(FSNS( XML_w, XML_initials ), bRemovePersonalInfo
                 ? OString::number( GetExport().GetInfoID(f->GetInitials()) )
                 : f->GetInitials().toUtf8());
        m_pSerializer->startElementNS( XML_w, XML_comment, pAttributeList );

        // Make sure to give parent/child fields a paraId
        const bool bNeedParaId = f->GetResolved() || data.parentStatus != ParentStatus::None;
        if (bNeedParaId)
            eResult = hasProperties::yes;

        if (f->GetTextObject() != nullptr)
        {
            // richtext
            data.lastParaId
                = GetExport().WriteOutliner(*f->GetTextObject(), TXT_ATN, bNeedParaId);
        }
        else
        {
            // just plain text - eg. when the field was created via the
            // .uno:InsertAnnotation API
            std::optional<OUString> aParaId;
            if (bNeedParaId)
            {
                data.lastParaId = m_nNextParaId++;
                aParaId = NumberToHexBinary(data.lastParaId);
            }
            m_pSerializer->startElementNS(XML_w, XML_p, FSNS(XML_w14, XML_paraId), aParaId);
            m_pSerializer->startElementNS(XML_w, XML_r);
            m_pSerializer->singleElementNS(XML_w, XML_annotationRef);
            m_pSerializer->endElementNS(XML_w, XML_r);
            m_pSerializer->startElementNS(XML_w, XML_r);
            RunText(f->GetText());
            m_pSerializer->endElementNS(XML_w, XML_r);
            m_pSerializer->endElementNS(XML_w, XML_p);
        }

        m_pSerializer->endElementNS( XML_w, XML_comment );
    }
    return eResult;
}

void DocxAttributeOutput::WritePostItFieldsResolved()
{
    for (auto& [f, data] : m_postitFields)
    {
        // Parent fields don't need to be exported here if they don't have a resolved attribute
        if (!f->GetResolved() && data.parentStatus != ParentStatus::HasParent)
            continue;
        OUString idstr = NumberToHexBinary(data.lastParaId);
        std::optional<OUString> sDone, sParentId;
        if (f->GetParentId() != 0 || f->GetParentPostItId() != 0)
        {
            if (data.parentStatus == ParentStatus::HasParent)
            {
                // Since parent fields have been resolved first, they should already have an id
                const PostItDOCXData& aParentFieldData = m_postitFields[data.parentIndex].second;
                sParentId = NumberToHexBinary(aParentFieldData.lastParaId);
            }
            else
            {
                SAL_WARN("sw.ww8", "SwPostItField has a parent id, but a matching parent was not found");
            }
        }
        if (f->GetResolved())
            sDone = "1";
        m_pSerializer->singleElementNS(XML_w15, XML_commentEx,
            FSNS(XML_w15, XML_paraId), idstr,
            FSNS(XML_w15, XML_done), sDone,
            FSNS(XML_w15, XML_paraIdParent), sParentId);
    }
}

bool DocxAttributeOutput::DropdownField( const SwField* pField )
{
    ww::eField eType = ww::eFORMDROPDOWN;
    OUString sCmd = FieldString( eType  );
    GetExport( ).OutputField( pField, eType, sCmd );

    return false;
}

bool DocxAttributeOutput::PlaceholderField( const SwField* pField )
{
    assert( m_PendingPlaceholder == nullptr );
    m_PendingPlaceholder = pField;
    return false; // do not expand
}

void DocxAttributeOutput::WritePendingPlaceholder()
{
    if( m_PendingPlaceholder == nullptr )
        return;
    const SwField* pField = m_PendingPlaceholder;
    m_PendingPlaceholder = nullptr;
    m_pSerializer->startElementNS(XML_w, XML_sdt);
    m_pSerializer->startElementNS(XML_w, XML_sdtPr);
    if( !pField->GetPar2().isEmpty())
        m_pSerializer->singleElementNS(XML_w, XML_alias, FSNS(XML_w, XML_val), pField->GetPar2());
    m_pSerializer->singleElementNS(XML_w, XML_temporary);
    m_pSerializer->singleElementNS(XML_w, XML_showingPlcHdr);
    m_pSerializer->singleElementNS(XML_w, XML_text);
    m_pSerializer->endElementNS( XML_w, XML_sdtPr );
    m_pSerializer->startElementNS(XML_w, XML_sdtContent);
    m_pSerializer->startElementNS(XML_w, XML_r);
    RunText( pField->GetPar1());
    m_pSerializer->endElementNS( XML_w, XML_r );
    m_pSerializer->endElementNS( XML_w, XML_sdtContent );
    m_pSerializer->endElementNS( XML_w, XML_sdt );
}

void DocxAttributeOutput::SetField( const SwField& rField, ww::eField eType, const OUString& rCmd )
{
    // field bookmarks are handled in the EndRun method
    GetExport().OutputField(&rField, eType, rCmd );
}

void DocxAttributeOutput::WriteExpand( const SwField* pField )
{
    // Will be written in the next End Run
    m_rExport.OutputField( pField, ww::eUNKNOWN, OUString() );
}

void DocxAttributeOutput::WriteField_Impl(const SwField *const pField,
    ww::eField const eType, const OUString& rFieldCmd, FieldFlags const nMode,
    OUString const*const pBookmarkName)
{
    if (m_bPreventDoubleFieldsHandling)
        return;

    struct FieldInfos infos;
    if (pField)
        infos.pField = pField->CopyField();
    infos.sCmd = rFieldCmd;
    infos.eType = eType;
    infos.bClose = bool(FieldFlags::Close & nMode);
    infos.bSep = bool(FieldFlags::CmdEnd & nMode);
    infos.bOpen = bool(FieldFlags::Start & nMode);
    m_Fields.push_back( infos );

    if (pBookmarkName)
    {
        m_sFieldBkm = *pBookmarkName;
    }

    if ( !pField )
        return;

    SwFieldIds nType = pField->GetTyp( )->Which( );

    // TODO Any other field types here ?
    if ( nType == SwFieldIds::SetExp )
    {
        const SwSetExpField *pSet = static_cast<const SwSetExpField*>( pField );
        if ( pSet->GetSubType() & SwGetSetExpType::String )
            m_sFieldBkm = pSet->GetPar1( );
    }
    else if ( nType == SwFieldIds::Dropdown )
    {
        const SwDropDownField* pDropDown = static_cast<const SwDropDownField*>( pField );
        m_sFieldBkm = pDropDown->GetName( );
    }
}

void DocxAttributeOutput::WriteFormData_Impl( const ::sw::mark::Fieldmark& rFieldmark )
{
    if ( !m_Fields.empty() )
        m_Fields.begin()->pFieldmark = &rFieldmark;
}

void DocxAttributeOutput::WriteBookmarks_Impl( std::vector< OUString >& rStarts, std::vector< OUString >& rEnds, const SwRedlineData* pRedlineData )
{
    for ( const OUString & name : rStarts )
    {
        if (name.startsWith("permission-for-group:") ||
            name.startsWith("permission-for-user:"))
        {
            m_rPermissionsStart.push_back(name);
        }
        else
        {
            m_rBookmarksStart.push_back(name);
            m_pMoveRedlineData = const_cast<SwRedlineData*>(pRedlineData);
        }
    }
    rStarts.clear();

    for ( const OUString & name : rEnds )
    {
        if (name.startsWith("permission-for-group:") ||
            name.startsWith("permission-for-user:"))
        {
            m_rPermissionsEnd.push_back(name);
        }
        else
        {
            m_rBookmarksEnd.push_back(name);
        }
    }
    rEnds.clear();
}

void DocxAttributeOutput::WriteFinalBookmarks_Impl( std::vector< OUString >& rStarts, std::vector< OUString >& rEnds )
{
    for ( const OUString & name : rStarts )
    {
        if (name.startsWith("permission-for-group:") ||
            name.startsWith("permission-for-user:"))
        {
            m_rPermissionsStart.push_back(name);
        }
        else
        {
            m_rFinalBookmarksStart.push_back(name);
        }
    }
    rStarts.clear();

    for ( const OUString & name : rEnds )
    {
        if (name.startsWith("permission-for-group:") ||
            name.startsWith("permission-for-user:"))
        {
            m_rPermissionsEnd.push_back(name);
        }
        else
        {
            m_rFinalBookmarksEnd.push_back(name);
        }
    }
    rEnds.clear();
}

void DocxAttributeOutput::WriteAnnotationMarks_Impl( std::vector< SwMarkName >& rStarts,
        std::vector< SwMarkName >& rEnds )
{
    m_rAnnotationMarksStart.insert(m_rAnnotationMarksStart.end(), rStarts.begin(), rStarts.end());
    rStarts.clear();

    m_rAnnotationMarksEnd.insert(m_rAnnotationMarksEnd.end(), rEnds.begin(), rEnds.end());
    rEnds.clear();
}

void DocxAttributeOutput::TextFootnote_Impl( const SwFormatFootnote& rFootnote )
{
    const SwEndNoteInfo& rInfo = rFootnote.IsEndNote()?
        m_rExport.m_rDoc.GetEndNoteInfo(): m_rExport.m_rDoc.GetFootnoteInfo();

    // footnote/endnote run properties
    const SwCharFormat* pCharFormat = rInfo.GetAnchorCharFormat( m_rExport.m_rDoc );

    OString aStyleId(m_rExport.m_pStyles->GetStyleId(m_rExport.GetId(pCharFormat)));

    m_pSerializer->singleElementNS(XML_w, XML_rStyle, FSNS(XML_w, XML_val), aStyleId);

    // remember the footnote/endnote to
    // 1) write the footnoteReference/endnoteReference in EndRunProperties()
    // 2) be able to dump them all to footnotes.xml/endnotes.xml
    if ( !rFootnote.IsEndNote() && m_rExport.m_rDoc.GetFootnoteInfo().m_ePos != FTNPOS_CHAPTER )
        m_pFootnotesList->add( rFootnote );
    else
        m_pEndnotesList->add( rFootnote );
}

void DocxAttributeOutput::FootnoteEndnoteReference()
{
    sal_Int32 nId;
    const SwFormatFootnote *pFootnote = m_pFootnotesList->getCurrent( nId );
    sal_Int32 nToken = XML_footnoteReference;

    // both cannot be set at the same time - if they are, it's a bug
    if ( !pFootnote )
    {
        pFootnote = m_pEndnotesList->getCurrent( nId );
        nToken = XML_endnoteReference;
    }

    if ( !pFootnote )
        return;

    // write it
    if ( pFootnote->GetNumStr().isEmpty() )
    {
        // autonumbered
        m_pSerializer->singleElementNS(XML_w, nToken, FSNS(XML_w, XML_id), OString::number(nId));
    }
    else
    {
        // not autonumbered
        m_pSerializer->singleElementNS( XML_w, nToken,
                FSNS( XML_w, XML_customMarkFollows ), "1",
                FSNS( XML_w, XML_id ), OString::number(nId) );

        RunText( pFootnote->GetNumStr() );
    }
}

static void WriteFootnoteSeparatorHeight(
    ::sax_fastparser::FSHelperPtr const& pSerializer, SwTwips const nHeight)
{
    // try to get the height by setting font size of the paragraph
    if (nHeight != 0)
    {
        pSerializer->startElementNS(XML_w, XML_pPr);
        pSerializer->startElementNS(XML_w, XML_rPr);
        pSerializer->singleElementNS(XML_w, XML_sz, FSNS(XML_w, XML_val),
            OString::number((nHeight + 5) / 10));
        pSerializer->endElementNS(XML_w, XML_rPr);
        pSerializer->endElementNS(XML_w, XML_pPr);
    }
}

void DocxAttributeOutput::FootnotesEndnotes( bool bFootnotes )
{
    const FootnotesVector& rVector = bFootnotes? m_pFootnotesList->getVector(): m_pEndnotesList->getVector();

    sal_Int32 nBody = bFootnotes? XML_footnotes: XML_endnotes;
    sal_Int32 nItem = bFootnotes? XML_footnote:  XML_endnote;

    m_pSerializer->startElementNS( XML_w, nBody, m_rExport.MainXmlNamespaces() );

    sal_Int32 nIndex = 0;

    // separator
    // note: can only be defined for the whole document, not per section
    m_pSerializer->startElementNS( XML_w, nItem,
            FSNS( XML_w, XML_id ), OString::number(nIndex++),
            FSNS( XML_w, XML_type ), "separator" );
    m_pSerializer->startElementNS(XML_w, XML_p);

    bool bSeparator = true;
    SwTwips nHeight(0);
    if (bFootnotes)
    {
        const SwPageFootnoteInfo& rFootnoteInfo = m_rExport.m_rDoc.GetPageDesc(0).GetFootnoteInfo();
        // Request separator only if both width and thickness are non-zero.
        bSeparator = rFootnoteInfo.GetLineStyle() != SvxBorderLineStyle::NONE
                  && rFootnoteInfo.GetLineWidth() > 0
                  && double(rFootnoteInfo.GetWidth()) > 0;
        nHeight = sw::FootnoteSeparatorHeight(m_rExport.m_rDoc, rFootnoteInfo);

        const IDocumentSettingAccess& rIDSA = m_rExport.m_rDoc.getIDocumentSettingAccess();
        if (rIDSA.get(DocumentSettingId::CONTINUOUS_ENDNOTES))
        {
            // Don't request separator if this is a Word-style separator, which is handled at a
            // layout level.
            nHeight = 0;
        }
    }

    WriteFootnoteSeparatorHeight(m_pSerializer, nHeight);

    m_pSerializer->startElementNS(XML_w, XML_r);
    if (bSeparator)
        m_pSerializer->singleElementNS(XML_w, XML_separator);
    m_pSerializer->endElementNS( XML_w, XML_r );
    m_pSerializer->endElementNS( XML_w, XML_p );
    m_pSerializer->endElementNS( XML_w, nItem );

    // separator
    m_pSerializer->startElementNS( XML_w, nItem,
            FSNS( XML_w, XML_id ), OString::number(nIndex++),
            FSNS( XML_w, XML_type ), "continuationSeparator" );
    m_pSerializer->startElementNS(XML_w, XML_p);

    WriteFootnoteSeparatorHeight(m_pSerializer, nHeight);

    m_pSerializer->startElementNS(XML_w, XML_r);
    if (bSeparator)
    {
        m_pSerializer->singleElementNS(XML_w, XML_continuationSeparator);
    }
    m_pSerializer->endElementNS( XML_w, XML_r );
    m_pSerializer->endElementNS( XML_w, XML_p );
    m_pSerializer->endElementNS( XML_w, nItem );

    // if new special ones are added, update also WriteFootnoteEndnotePr()

    // footnotes/endnotes themselves
    for ( const auto& rpItem : rVector )
    {
        m_footnoteEndnoteRefTag = bFootnotes ? XML_footnoteRef : XML_endnoteRef;
        m_footnoteCustomLabel = rpItem->GetNumStr();

        m_pSerializer->startElementNS(XML_w, nItem, FSNS(XML_w, XML_id), OString::number(nIndex));

        const SwNodeIndex* pIndex = rpItem->GetTextFootnote()->GetStartNode();
        m_rExport.WriteSpecialText( pIndex->GetIndex() + 1,
                pIndex->GetNode().EndOfSectionIndex(),
                bFootnotes? TXT_FTN: TXT_EDN );

        m_pSerializer->endElementNS( XML_w, nItem );
        ++nIndex;
    }

    m_pSerializer->endElementNS( XML_w, nBody );

}

void DocxAttributeOutput::WriteFootnoteEndnotePr( ::sax_fastparser::FSHelperPtr const & fs, int tag,
    const SwEndNoteInfo& info, int listtag )
{
    fs->startElementNS(XML_w, tag);

    SwSectionFormats& rSections = m_rExport.m_rDoc.GetSections();
    if (!rSections.empty())
    {
        SwSectionFormat* pFormat = rSections[0];
        bool bEndnAtEnd = pFormat->GetEndAtTextEnd().IsAtEnd();
        if (bEndnAtEnd)
        {
            fs->singleElementNS(XML_w, XML_pos, FSNS(XML_w, XML_val), "sectEnd");
        }
    }

    OString aCustomFormat;
    OString fmt = lcl_ConvertNumberingType(info.m_aFormat.GetNumberingType(), nullptr, aCustomFormat);
    if (!fmt.isEmpty() && aCustomFormat.isEmpty())
        fs->singleElementNS(XML_w, XML_numFmt, FSNS(XML_w, XML_val), fmt);
    if( info.m_nFootnoteOffset != 0 )
        fs->singleElementNS( XML_w, XML_numStart, FSNS( XML_w, XML_val ),
            OString::number(info.m_nFootnoteOffset + 1) );

    const SwFootnoteInfo* pFootnoteInfo = dynamic_cast<const SwFootnoteInfo*>(&info);
    if( pFootnoteInfo )
    {
        switch( pFootnoteInfo->m_eNum )
        {
            case FTNNUM_PAGE:       fmt = "eachPage"_ostr; break;
            case FTNNUM_CHAPTER:    fmt = "eachSect"_ostr; break;
            default:                fmt.clear();      break;
        }
        if (!fmt.isEmpty())
            fs->singleElementNS(XML_w, XML_numRestart, FSNS(XML_w, XML_val), fmt);
    }

    if( listtag != 0 ) // we are writing to settings.xml, write also special footnote/endnote list
    { // there are currently only two hardcoded ones ( see FootnotesEndnotes())
        fs->singleElementNS(XML_w, listtag, FSNS(XML_w, XML_id), "0");
        fs->singleElementNS(XML_w, listtag, FSNS(XML_w, XML_id), "1");
    }
    fs->endElementNS( XML_w, tag );
}

void DocxAttributeOutput::SectFootnoteEndnotePr()
{
    if( HasFootnotes())
        WriteFootnoteEndnotePr( m_pSerializer, XML_footnotePr, m_rExport.m_rDoc.GetFootnoteInfo(), 0 );
    if( HasEndnotes())
        WriteFootnoteEndnotePr( m_pSerializer, XML_endnotePr, m_rExport.m_rDoc.GetEndNoteInfo(), 0 );
}

void DocxAttributeOutput::ParaLineSpacing_Impl( short nSpace, short nMulti )
{
    if ( nSpace < 0 )
    {
        AddToAttrList( m_pParagraphSpacingAttrList,
                FSNS( XML_w, XML_lineRule ), "exact",
                FSNS( XML_w, XML_line ), OString::number( -nSpace ) );
    }
    else if( nSpace > 0 && nMulti )
    {
        AddToAttrList( m_pParagraphSpacingAttrList,
                FSNS( XML_w, XML_lineRule ), "auto",
                FSNS( XML_w, XML_line ), OString::number( nSpace ) );
    }
    else
    {
        AddToAttrList( m_pParagraphSpacingAttrList,
                FSNS( XML_w, XML_lineRule ), "atLeast",
                FSNS( XML_w, XML_line ), OString::number( nSpace ) );
    }
}

void DocxAttributeOutput::ParaAdjust( const SvxAdjustItem& rAdjust )
{
    const char *pAdjustString;

    bool const bEcma = GetExport().GetFilter().getVersion() == oox::core::ECMA_376_1ST_EDITION;

    const SfxItemSet* pItems = GetExport().GetCurItemSet();
    const SvxFrameDirectionItem* rFrameDir = pItems?
        pItems->GetItem( RES_FRAMEDIR ) : nullptr;

    SvxFrameDirection nDir = SvxFrameDirection::Environment;
    if( rFrameDir != nullptr )
        nDir = rFrameDir->GetValue();
    if ( nDir == SvxFrameDirection::Environment )
        nDir = GetExport( ).GetDefaultFrameDirection( );
    bool bRtl = ( nDir == SvxFrameDirection::Horizontal_RL_TB );

    switch ( rAdjust.GetAdjust() )
    {
        case SvxAdjust::Left:
            if ( bEcma )
            {
                if ( bRtl )
                    pAdjustString = "right";
                else
                    pAdjustString = "left";
            }
            else if ( bRtl )
                pAdjustString = "end";
            else
                pAdjustString = "start";
            break;
        case SvxAdjust::Right:
            if ( bEcma )
            {
                if ( bRtl )
                    pAdjustString = "left";
                else
                    pAdjustString = "right";
            }
            else if ( bRtl )
                pAdjustString = "start";
            else
                pAdjustString = "end";
            break;
        case SvxAdjust::BlockLine:
        case SvxAdjust::Block:
        {
            if (rAdjust.GetLastBlock() == SvxAdjust::Block)
                pAdjustString = "distribute";
            else
                pAdjustString = "both";
            switch ( rAdjust.GetPropWordSpacingMinimum() )
            {
                case 133:
                    if ( rAdjust.GetPropWordSpacingMaximum() == 133 )
                        pAdjustString = "lowKashida";
                    break;
                case 200:
                    if ( rAdjust.GetPropWordSpacingMaximum() == 200 )
                        pAdjustString = "mediumKashida";
                    break;
                case 300:
                    if ( rAdjust.GetPropWordSpacingMaximum() == 300 )
                        pAdjustString = "highKashida";
                    break;
                default:
                    break;
            }
            break;
        }
        case SvxAdjust::Center:
            pAdjustString = "center";
            break;
        default:
            return; // not supported attribute
    }
    m_pSerializer->singleElementNS(XML_w, XML_jc, FSNS(XML_w, XML_val), pAdjustString);
}

void DocxAttributeOutput::ParaSplit( const SvxFormatSplitItem& rSplit )
{
    if (rSplit.GetValue())
        m_pSerializer->singleElementNS(XML_w, XML_keepLines, FSNS(XML_w, XML_val), "false");
    else
        m_pSerializer->singleElementNS(XML_w, XML_keepLines);
}

void DocxAttributeOutput::ParaWidows( const SvxWidowsItem& rWidows )
{
    if (rWidows.GetValue())
        m_pSerializer->singleElementNS(XML_w, XML_widowControl);
    else
        m_pSerializer->singleElementNS(XML_w, XML_widowControl, FSNS(XML_w, XML_val), "false");
}

static void impl_WriteTabElement( FSHelperPtr const & pSerializer,
                                  const SvxTabStop& rTab, tools::Long tabsOffset )
{
    rtl::Reference<FastAttributeList> pTabElementAttrList = FastSerializerHelper::createAttrList();

    switch (rTab.GetAdjustment())
    {
    case SvxTabAdjust::Right:
        pTabElementAttrList->add( FSNS( XML_w, XML_val ), "right" );
        break;
    case SvxTabAdjust::Decimal:
        pTabElementAttrList->add( FSNS( XML_w, XML_val ), "decimal" );
        break;
    case SvxTabAdjust::Center:
        pTabElementAttrList->add( FSNS( XML_w, XML_val ), "center" );
        break;
    case SvxTabAdjust::Default:
    case SvxTabAdjust::Left:
    default:
        pTabElementAttrList->add( FSNS( XML_w, XML_val ), "left" );
        break;
    }

    // Write position according to used offset of the whole paragraph.
    // In DOCX, w:pos specifies the position of the current custom tab stop with respect to the current page margins.
    // But in ODT, zero position could be page margins or paragraph indent according to used settings.
    // This is handled outside of this method and provided for us in tabsOffset parameter.
    pTabElementAttrList->add( FSNS( XML_w, XML_pos ), OString::number( rTab.GetTabPos() + tabsOffset ) );

    sal_Unicode cFillChar = rTab.GetFill();

    if ('.' == cFillChar )
        pTabElementAttrList->add( FSNS( XML_w, XML_leader ), "dot" );
    else if ( '-' == cFillChar )
        pTabElementAttrList->add( FSNS( XML_w, XML_leader ), "hyphen" );
    else if ( u'\x00B7' == cFillChar ) // middle dot
        pTabElementAttrList->add( FSNS( XML_w, XML_leader ), "middleDot" );
    else if ( '_' == cFillChar )
        pTabElementAttrList->add( FSNS( XML_w, XML_leader ), "underscore" );
    else
        pTabElementAttrList->add( FSNS( XML_w, XML_leader ), "none" );

    pSerializer->singleElementNS(XML_w, XML_tab, pTabElementAttrList);
}

void DocxAttributeOutput::ParaTabStop( const SvxTabStopItem& rTabStop )
{
    const SvxTabStopItem* pInheritedTabs = nullptr;
    if ( GetExport().m_pStyAttr )
        pInheritedTabs = GetExport().m_pStyAttr->GetItem<SvxTabStopItem>(RES_PARATR_TABSTOP);
    else if ( GetExport().m_pCurrentStyle && GetExport().m_pCurrentStyle->DerivedFrom() )
        pInheritedTabs = GetExport().m_pCurrentStyle->DerivedFrom()->GetAttrSet().GetItem<SvxTabStopItem>(RES_PARATR_TABSTOP);
    const sal_uInt16 nInheritedTabCount = pInheritedTabs ? pInheritedTabs->Count() : 0;
    const sal_uInt16 nCount = rTabStop.Count();

    // <w:tabs> must contain at least one <w:tab>, so don't write it empty
    if ( !nCount && !nInheritedTabCount )
        return;
    if( nCount == 1 && rTabStop[ 0 ].GetAdjustment() == SvxTabAdjust::Default )
    {
        GetExport().setDefaultTabStop( rTabStop[ 0 ].GetTabPos());
        return;
    }

    // do not output inherited tabs twice (inside styles and inside inline properties)
    if ( nCount == nInheritedTabCount && nCount > 0 )
    {
        if ( *pInheritedTabs == rTabStop )
            return;
    }

    m_pSerializer->startElementNS(XML_w, XML_tabs);

    // Get offset for tabs
    // In DOCX, w:pos specifies the position of the current custom tab stop with respect to the current page margins.
    // But in ODT, zero position could be page margins or paragraph indent according to used settings.
    tools::Long tabsOffset = m_rExport.GetParaTabStopOffset();

    // clear unused inherited tabs - otherwise the style will add them back in
    sal_Int32 nCurrTab = 0;
    for ( sal_uInt16 i = 0; i < nInheritedTabCount; ++i )
    {
        while ( nCurrTab < nCount && rTabStop[nCurrTab] < pInheritedTabs->At(i) )
            ++nCurrTab;

        if ( nCurrTab == nCount || pInheritedTabs->At(i) < rTabStop[nCurrTab] )
        {
            m_pSerializer->singleElementNS( XML_w, XML_tab,
                FSNS( XML_w, XML_val ), "clear",
                FSNS( XML_w, XML_pos ), OString::number(pInheritedTabs->At(i).GetTabPos()) );
        }
    }

    for (sal_uInt16 i = 0; i < nCount; i++ )
    {
        if( rTabStop[i].GetAdjustment() != SvxTabAdjust::Default )
            impl_WriteTabElement( m_pSerializer, rTabStop[i], tabsOffset );
        else
            GetExport().setDefaultTabStop( rTabStop[i].GetTabPos());
    }

    m_pSerializer->endElementNS( XML_w, XML_tabs );
}

void DocxAttributeOutput::ParaHyphenZone( const SvxHyphenZoneItem& rHyphenZone )
{
    m_pSerializer->singleElementNS( XML_w, XML_suppressAutoHyphens,
            FSNS( XML_w, XML_val ), OString::boolean( !rHyphenZone.IsHyphen() ) );
}

void DocxAttributeOutput::ParaNumRule_Impl( const SwTextNode* pTextNd, sal_Int32 nLvl, sal_Int32 nNumId )
{
    if ( USHRT_MAX == nNumId )
        return;

    // LibreOffice is not very flexible with "Outline Numbering" (aka "Outline" numbering style).
    // Only ONE numbering rule ("Outline") can be associated with a style-assigned-listLevel,
    // and no other style is able to inherit these numId/nLvl settings - only text nodes can.
    // So listLevel only exists in paragraph properties EXCEPT for up to ten styles that have been
    // assigned to one of these special Chapter Numbering listlevels (by default Heading 1-10).
    const sal_Int32 nTableSize = m_rExport.m_pUsedNumTable ? m_rExport.m_pUsedNumTable->size() : 0;
    const SwNumRule* pRule = nNumId > 0 && nNumId <= nTableSize ? (*m_rExport.m_pUsedNumTable)[nNumId-1] : nullptr;
    const SwTextFormatColl* pColl = pTextNd ? pTextNd->GetTextColl() : nullptr;
    // Do not duplicate numbering that is inherited from the (Chapter numbering) style
    // (since on import we duplicate style numbering/listlevel to the paragraph).
    if (pColl && pColl->IsAssignedToListLevelOfOutlineStyle()
        && nLvl == pColl->GetAssignedOutlineStyleLevel() && pRule && pRule->IsOutlineRule())
    {
        // By definition of how LO is implemented, assignToListLevel is only possible
        // when the style is also using OutlineRule for numbering. Adjust logic if that changes.
        assert(pRule->GetName() == pColl->GetNumRule(true).GetValue());
        return;
    }

    m_pSerializer->startElementNS(XML_w, XML_numPr);
    m_pSerializer->singleElementNS(XML_w, XML_ilvl, FSNS(XML_w, XML_val), OString::number(nLvl));
    m_pSerializer->singleElementNS(XML_w, XML_numId, FSNS(XML_w, XML_val), OString::number(nNumId));
    m_pSerializer->endElementNS(XML_w, XML_numPr);
}

void DocxAttributeOutput::ParaScriptSpace( const SfxBoolItem& rScriptSpace )
{
    m_pSerializer->singleElementNS( XML_w, XML_autoSpaceDE,
           FSNS( XML_w, XML_val ), OString::boolean( rScriptSpace.GetValue() ) );
}

void DocxAttributeOutput::ParaHangingPunctuation( const SfxBoolItem& rItem )
{
    m_pSerializer->singleElementNS( XML_w, XML_overflowPunct,
           FSNS( XML_w, XML_val ), OString::boolean( rItem.GetValue() ) );
}

void DocxAttributeOutput::ParaForbiddenRules( const SfxBoolItem& rItem )
{
    m_pSerializer->singleElementNS( XML_w, XML_kinsoku,
           FSNS( XML_w, XML_val ), OString::boolean( rItem.GetValue() ) );
}

void DocxAttributeOutput::ParaVerticalAlign( const SvxParaVertAlignItem& rAlign )
{
    const char *pAlignString;

    switch ( rAlign.GetValue() )
    {
        case SvxParaVertAlignItem::Align::Baseline:
            pAlignString = "baseline";
            break;
        case SvxParaVertAlignItem::Align::Top:
            pAlignString = "top";
            break;
        case SvxParaVertAlignItem::Align::Center:
            pAlignString = "center";
            break;
        case SvxParaVertAlignItem::Align::Bottom:
            pAlignString = "bottom";
            break;
        case SvxParaVertAlignItem::Align::Automatic:
            pAlignString = "auto";
            break;
        default:
            return; // not supported attribute
    }
    m_pSerializer->singleElementNS(XML_w, XML_textAlignment, FSNS(XML_w, XML_val), pAlignString);
}

void DocxAttributeOutput::ParaSnapToGrid( const SvxParaGridItem& rGrid )
{
    m_pSerializer->singleElementNS( XML_w, XML_snapToGrid,
            FSNS( XML_w, XML_val ), OString::boolean( rGrid.GetValue() ) );
}

void DocxAttributeOutput::FormatFrameSize( const SwFormatFrameSize& rSize )
{
    if (m_rExport.SdrExporter().getTextFrameSyntax() && m_rExport.SdrExporter().getFlyFrameSize())
    {
        const Size* pSize = m_rExport.SdrExporter().getFlyFrameSize();
        m_rExport.SdrExporter().getTextFrameStyle().append(";width:" + OString::number(double(pSize->Width()) / 20));
        m_rExport.SdrExporter().getTextFrameStyle().append("pt;height:" + OString::number(double(pSize->Height()) / 20) + "pt");
    }
    else if (m_rExport.SdrExporter().getDMLTextFrameSyntax())
    {
    }
    else if ( m_rExport.m_bOutFlyFrameAttrs )
    {
        if ( rSize.GetWidth() && rSize.GetWidthSizeType() == SwFrameSize::Fixed )
            AddToAttrList( m_rExport.SdrExporter().getFlyAttrList(),
                    FSNS( XML_w, XML_w ), OString::number( rSize.GetWidth( ) ) );

        if ( rSize.GetHeight() )
        {
            std::string_view sRule( "exact" );
            if ( rSize.GetHeightSizeType() == SwFrameSize::Minimum )
                sRule = "atLeast";
            AddToAttrList( m_rExport.SdrExporter().getFlyAttrList(),
                    FSNS( XML_w, XML_hRule ), sRule,
                    FSNS( XML_w, XML_h ), OString::number( rSize.GetHeight( ) ) );
        }
    }
    else if ( m_rExport.m_bOutPageDescs )
    {
        rtl::Reference<FastAttributeList> attrList = FastSerializerHelper::createAttrList( );
        if ( m_rExport.m_pCurrentPageDesc->GetLandscape( ) )
            attrList->add( FSNS( XML_w, XML_orient ), "landscape" );

        attrList->add( FSNS( XML_w, XML_w ), OString::number( rSize.GetWidth( ) ) );
        attrList->add( FSNS( XML_w, XML_h ), OString::number( rSize.GetHeight( ) ) );

        m_pSerializer->singleElementNS( XML_w, XML_pgSz, attrList );
    }
}

void DocxAttributeOutput::FormatPaperBin(const SvxPaperBinItem& rPaperBin)
{
    sal_Int8 nPaperBin = rPaperBin.GetValue();
    rtl::Reference<FastAttributeList> attrList = FastSerializerHelper::createAttrList( );
    SfxPrinter* pPrinter = m_rExport.m_rDoc.getIDocumentDeviceAccess().getPrinter(true);
    sal_Int16 nPaperSource = pPrinter->GetSourceIndexByPaperBin(nPaperBin);
    attrList->add( FSNS( XML_w, XML_first ), OString::number(nPaperSource) );
    attrList->add( FSNS( XML_w, XML_other ), OString::number(nPaperSource) );
    m_pSerializer->singleElementNS( XML_w, XML_paperSrc, attrList );
}

void DocxAttributeOutput::FormatFirstLineIndent(SvxFirstLineIndentItem const& rFirstLine)
{
    // tdf#83844: export FONT_CJK_ADVANCE first line indent as hangingChars/firstLineChars
    auto stValue = rFirstLine.GetTextFirstLineOffset();
    if (stValue.m_nUnit == css::util::MeasureUnit::FONT_CJK_ADVANCE)
    {
        if (stValue.m_dValue >= 0.0)
        {
            AddToAttrList(m_pLRSpaceAttrList, FSNS(XML_w, XML_firstLineChars),
                          OString::number(stValue.m_dValue * 100.0));
        }
        else
        {
            AddToAttrList(m_pLRSpaceAttrList, FSNS(XML_w, XML_hangingChars),
                          OString::number(stValue.m_dValue * -100.0));
        }

        return;
    }

    sal_Int32 const nFirstLineAdjustment(rFirstLine.ResolveTextFirstLineOffset({}));
    if (nFirstLineAdjustment > 0)
    {
        AddToAttrList(m_pLRSpaceAttrList, FSNS(XML_w, XML_firstLine),
                OString::number(nFirstLineAdjustment));
    }
    else
    {
        AddToAttrList(m_pLRSpaceAttrList, FSNS(XML_w, XML_hanging),
                OString::number(- nFirstLineAdjustment));
    }
}

void DocxAttributeOutput::FormatTextLeftMargin(SvxTextLeftMarginItem const& rTextLeftMargin)
{
    SvxTextLeftMarginItem const* pTextLeftMargin(&rTextLeftMargin);
    ::std::optional<SvxTextLeftMarginItem> oCopy;
    if (dynamic_cast<SwContentNode const*>(GetExport().m_pOutFormatNode) != nullptr)
    {
        auto pTextNd(static_cast<SwTextNode const*>(GetExport().m_pOutFormatNode));
        // WW doesn't have a concept of a paragraph that's in a list but not
        // counted in the list - see AttributeOutputBase::ParaNumRule()
        // forcing non-existent numId="0" in this case.
        // This means WW won't apply the indents from the numbering,
        // so try to add them as paragraph properties here.
        if (!pTextNd->IsCountedInList())
        {
            SfxItemSet temp(SfxItemSet::makeFixedSfxItemSet<RES_MARGIN_TEXTLEFT, RES_MARGIN_TEXTLEFT>(m_rExport.m_rDoc.GetAttrPool()));
            pTextNd->GetParaAttr(temp, 0, 0, false, true, true, nullptr);
            if (auto *const pItem = temp.GetItem(RES_MARGIN_TEXTLEFT))
            {
                oCopy.emplace(*pItem);
                pTextLeftMargin = &*oCopy;
            }
        }
    }
    bool const bEcma1st(m_rExport.GetFilter().getVersion() == oox::core::ECMA_376_1ST_EDITION);

    // tdf#83844: export FONT_CJK_ADVANCE left margin as leftChars/startChars
    auto stValue = pTextLeftMargin->GetTextLeft();
    if (stValue.m_nUnit == css::util::MeasureUnit::FONT_CJK_ADVANCE)
    {
        // tdf#83844: DOCX stores left and leftChars differently with hanging
        // indentation. The left margin must be adjusted before exporting.
        const SfxItemSet* pSet = GetExport().m_pISet;
        if (pSet && pSet->HasItem(RES_MARGIN_FIRSTLINE))
        {
            const SvxFirstLineIndentItem* pItem = pSet->GetItem(RES_MARGIN_FIRSTLINE);
            auto stFirstLine = pItem->GetTextFirstLineOffset();
            if (stFirstLine.m_nUnit == css::util::MeasureUnit::FONT_CJK_ADVANCE
                && stFirstLine.m_dValue < 0.0)
            {
                stValue.m_dValue += stFirstLine.m_dValue;
            }
        }

        AddToAttrList(m_pLRSpaceAttrList, FSNS(XML_w, (bEcma1st ? XML_leftChars : XML_startChars)),
                      OString::number(stValue.m_dValue * 100.0));
        return;
    }

    AddToAttrList(m_pLRSpaceAttrList, FSNS(XML_w, (bEcma1st ? XML_left : XML_start)),
                  OString::number(pTextLeftMargin->ResolveTextLeft({})));
}

void DocxAttributeOutput::FormatRightMargin(SvxRightMarginItem const& rRightMargin)
{
    bool const bEcma1st(m_rExport.GetFilter().getVersion() == oox::core::ECMA_376_1ST_EDITION);

    // tdf#83844: export FONT_CJK_ADVANCE right margin as rightChars/endChars
    auto stValue = rRightMargin.GetRight();
    if (stValue.m_nUnit == css::util::MeasureUnit::FONT_CJK_ADVANCE)
    {
        AddToAttrList(m_pLRSpaceAttrList, FSNS(XML_w, (bEcma1st ? XML_rightChars : XML_endChars)),
                      OString::number(stValue.m_dValue * 100.0));
        return;
    }

    AddToAttrList(m_pLRSpaceAttrList, FSNS(XML_w, (bEcma1st ? XML_right : XML_end)),
                  OString::number(rRightMargin.ResolveRight({})));
}

void DocxAttributeOutput::FormatLRSpace( const SvxLRSpaceItem& rLRSpace )
{
    bool const bEcma = m_rExport.GetFilter().getVersion() == oox::core::ECMA_376_1ST_EDITION;
    if (m_rExport.SdrExporter().getTextFrameSyntax())
    {
        m_rExport.SdrExporter().getTextFrameStyle().append(
            ";mso-wrap-distance-left:" + OString::number(double(rLRSpace.ResolveLeft({})) / 20)
            + "pt");
        m_rExport.SdrExporter().getTextFrameStyle().append(
            ";mso-wrap-distance-right:" + OString::number(double(rLRSpace.ResolveRight({})) / 20)
            + "pt");
    }
    else if (m_rExport.SdrExporter().getDMLTextFrameSyntax())
    {
    }
    else if ( m_rExport.m_bOutFlyFrameAttrs )
    {
        AddToAttrList(m_rExport.SdrExporter().getFlyAttrList(), FSNS(XML_w, XML_hSpace),
                      OString::number((rLRSpace.ResolveLeft({}) + rLRSpace.ResolveRight({})) / 2));
    }
    else if ( m_rExport.m_bOutPageDescs )
    {
        m_pageMargins.nLeft = 0;
        m_pageMargins.nRight = 0;

        const SvxBoxItem* pBoxItem = m_rExport.HasItem(RES_BOX);
        if (pBoxItem)
        {
            m_pageMargins.nLeft = pBoxItem->CalcLineSpace( SvxBoxItemLine::LEFT, /*bEvenIfNoLine*/true );
            m_pageMargins.nRight = pBoxItem->CalcLineSpace( SvxBoxItemLine::RIGHT, /*bEvenIfNoLine*/true );
        }

        m_pageMargins.nLeft += sal::static_int_cast<sal_uInt16>(rLRSpace.ResolveLeft({}));
        m_pageMargins.nRight += sal::static_int_cast<sal_uInt16>(rLRSpace.ResolveRight({}));

        // if page layout is 'left' then left/right margin may need to be exchanged
        // as it is exported as mirrored layout starting with even page
        if (m_rExport.isMirroredMargin()
            && UseOnPage::Left == (m_rExport.m_pCurrentPageDesc->ReadUseOn() & UseOnPage::All))
        {
            std::swap(m_pageMargins.nLeft, m_pageMargins.nRight);
        }

        sal_uInt16 nGutter = rLRSpace.GetGutterMargin();

        AddToAttrList( m_pSectionSpacingAttrList,
                FSNS( XML_w, XML_left ), OString::number( m_pageMargins.nLeft ),
                FSNS( XML_w, XML_right ), OString::number( m_pageMargins.nRight ),
                FSNS( XML_w, XML_gutter ), OString::number( nGutter ) );
    }
    else
    {
        // note: this is not possible for SwTextNode but is for EditEngine!
        SvxLRSpaceItem const* pLRSpace(&rLRSpace);
        ::std::optional<SvxLRSpaceItem> oLRSpace;
        assert(dynamic_cast<SwContentNode const*>(GetExport().m_pOutFormatNode) == nullptr);
        rtl::Reference<FastAttributeList> pLRSpaceAttrList = FastSerializerHelper::createAttrList();
        if ((0 != pLRSpace->ResolveTextLeft({})) || (pLRSpace->IsExplicitZeroMarginValLeft()))
        {
            pLRSpaceAttrList->add(FSNS(XML_w, (bEcma ? XML_left : XML_start)),
                                  OString::number(pLRSpace->ResolveTextLeft({})));
        }
        if ((0 != pLRSpace->ResolveRight({})) || (pLRSpace->IsExplicitZeroMarginValRight()))
        {
            pLRSpaceAttrList->add(FSNS(XML_w, (bEcma ? XML_right : XML_end)),
                                  OString::number(pLRSpace->ResolveRight({})));
        }
        // tdf#83844: TODO: export FONT_CJK_ADVANCE first line indent as HangingChars/FirstLineChars
        sal_Int32 const nFirstLineAdjustment = pLRSpace->ResolveTextFirstLineOffset({});
        if (nFirstLineAdjustment > 0)
            pLRSpaceAttrList->add( FSNS( XML_w, XML_firstLine ), OString::number( nFirstLineAdjustment ) );
        else
            pLRSpaceAttrList->add( FSNS( XML_w, XML_hanging ), OString::number( - nFirstLineAdjustment ) );
        m_pSerializer->singleElementNS( XML_w, XML_ind, pLRSpaceAttrList );
    }
}

void DocxAttributeOutput::FormatULSpace( const SvxULSpaceItem& rULSpace )
{

    if (m_rExport.SdrExporter().getTextFrameSyntax())
    {
        m_rExport.SdrExporter().getTextFrameStyle().append(";mso-wrap-distance-top:" + OString::number(double(rULSpace.GetUpper()) / 20) + "pt");
        m_rExport.SdrExporter().getTextFrameStyle().append(";mso-wrap-distance-bottom:" + OString::number(double(rULSpace.GetLower()) / 20) + "pt");
    }
    else if (m_rExport.SdrExporter().getDMLTextFrameSyntax())
    {
    }
    else if ( m_rExport.m_bOutFlyFrameAttrs )
    {
        AddToAttrList( m_rExport.SdrExporter().getFlyAttrList(), FSNS( XML_w, XML_vSpace ),
                OString::number(
                    ( rULSpace.GetLower() + rULSpace.GetUpper() ) / 2 ) );
    }
    else if (m_rExport.m_bOutPageDescs )
    {
        OSL_ENSURE( m_rExport.GetCurItemSet(), "Impossible" );
        if ( !m_rExport.GetCurItemSet() )
            return;

        HdFtDistanceGlue aDistances( *m_rExport.GetCurItemSet() );

        sal_Int32 nHeader = 0;
        if ( aDistances.HasHeader() )
            nHeader = sal_Int32( aDistances.m_DyaHdrTop );
        else if (m_rExport.m_pFirstPageFormat)
        {
            HdFtDistanceGlue aFirstPageDistances(m_rExport.m_pFirstPageFormat->GetAttrSet());
            if (aFirstPageDistances.HasHeader())
            {
                // The follow page style has no header, but the first page style has. In Word terms,
                // this means that the header margin of "the" section is coming from the first page
                // style.
                nHeader = sal_Int32(aFirstPageDistances.m_DyaHdrTop);
            }
        }

        // Page top
        m_pageMargins.nTop = aDistances.m_DyaTop;

        sal_Int32 nFooter = 0;
        if ( aDistances.HasFooter() )
            nFooter = sal_Int32( aDistances.m_DyaHdrBottom );
        else if (m_rExport.m_pFirstPageFormat)
        {
            HdFtDistanceGlue aFirstPageDistances(m_rExport.m_pFirstPageFormat->GetAttrSet());
            if (aFirstPageDistances.HasFooter())
            {
                // The follow page style has no footer, but the first page style has. In Word terms,
                // this means that the footer margin of "the" section is coming from the first page
                // style.
                nFooter = sal_Int32(aFirstPageDistances.m_DyaHdrBottom);
            }
        }

        // Page Bottom
        m_pageMargins.nBottom = aDistances.m_DyaBottom;

        AddToAttrList( m_pSectionSpacingAttrList,
                FSNS( XML_w, XML_header ), OString::number( nHeader ),
                FSNS( XML_w, XML_top ), OString::number( m_pageMargins.nTop ),
                FSNS( XML_w, XML_footer ), OString::number( nFooter ),
                FSNS( XML_w, XML_bottom ), OString::number( m_pageMargins.nBottom ) );
    }
    else
    {
        SAL_INFO("sw.ww8", "DocxAttributeOutput::FormatULSpace: setting spacing" << rULSpace.GetUpper() );
        // check if before auto spacing was set during import and spacing we get from actual object is same
        // that we set in import. If yes just write beforeAutoSpacing tag.
        if (m_bParaBeforeAutoSpacing && m_nParaBeforeSpacing == rULSpace.GetUpper())
        {
            AddToAttrList( m_pParagraphSpacingAttrList,
                    FSNS( XML_w, XML_beforeAutospacing ), "1" );
        }
        else if (m_bParaBeforeAutoSpacing && m_nParaBeforeSpacing == -1)
        {
            AddToAttrList( m_pParagraphSpacingAttrList,
                    FSNS( XML_w, XML_beforeAutospacing ), "0",
                    FSNS( XML_w, XML_before ), OString::number( rULSpace.GetUpper() ) );
        }
        else
        {
            AddToAttrList( m_pParagraphSpacingAttrList,
                    FSNS( XML_w, XML_before ), OString::number( rULSpace.GetUpper() ) );
        }
        m_bParaBeforeAutoSpacing = false;
        // check if after auto spacing was set during import and spacing we get from actual object is same
        // that we set in import. If yes just write afterAutoSpacing tag.
        if (m_bParaAfterAutoSpacing && m_nParaAfterSpacing == rULSpace.GetLower())
        {
            AddToAttrList( m_pParagraphSpacingAttrList,
                    FSNS( XML_w, XML_afterAutospacing ), "1" );
        }
        else if (m_bParaAfterAutoSpacing && m_nParaAfterSpacing == -1)
        {
            AddToAttrList( m_pParagraphSpacingAttrList,
                    FSNS( XML_w, XML_afterAutospacing ), "0",
                    FSNS( XML_w, XML_after ), OString::number( rULSpace.GetLower()) );
        }
        else
        {
            AddToAttrList( m_pParagraphSpacingAttrList,
                    FSNS( XML_w, XML_after ), OString::number( rULSpace.GetLower()) );
        }
        m_bParaAfterAutoSpacing = false;

        if (rULSpace.GetContext())
            m_pSerializer->singleElementNS(XML_w, XML_contextualSpacing);
        else
        {
            // Write out Contextual Spacing = false if it would have inherited a true.
            const SvxULSpaceItem* pInherited = nullptr;
            if (auto pNd = dynamic_cast<const SwContentNode*>(m_rExport.m_pOutFormatNode)) //paragraph
                pInherited = &static_cast<SwTextFormatColl&>(pNd->GetAnyFormatColl()).GetAttrSet().GetULSpace();
            else if (m_rExport.m_bStyDef && m_rExport.m_pCurrentStyle && m_rExport.m_pCurrentStyle->DerivedFrom()) //style
                pInherited = &m_rExport.m_pCurrentStyle->DerivedFrom()->GetULSpace();

            if (pInherited && pInherited->GetContext())
                m_pSerializer->singleElementNS(XML_w, XML_contextualSpacing, FSNS(XML_w, XML_val), "false");
        }
    }
}

namespace docx {

rtl::Reference<FastAttributeList> SurroundToVMLWrap(SwFormatSurround const& rSurround)
{
    std::string_view sType;
    std::string_view sSide;
    switch (rSurround.GetSurround())
    {
        case css::text::WrapTextMode_NONE:
            sType = "topAndBottom";
            break;
        case css::text::WrapTextMode_PARALLEL:
            sType = "square";
            break;
        case css::text::WrapTextMode_DYNAMIC:
            sType = "square";
            sSide = "largest";
            break;
        case css::text::WrapTextMode_LEFT:
            sType = "square";
            sSide = "left";
            break;
        case css::text::WrapTextMode_RIGHT:
            sType = "square";
            sSide = "right";
            break;
        case css::text::WrapTextMode_THROUGH:
            /* empty type and side means through */
        default:
            sType = "none";
            break;
    }
    rtl::Reference<FastAttributeList> pAttrList;
    if (!sType.empty())
        DocxAttributeOutput::AddToAttrList(pAttrList, XML_type, sType);
    if (!sSide.empty())
        DocxAttributeOutput::AddToAttrList(pAttrList, XML_side, sSide);
    return pAttrList;
}

} // namespace docx

void DocxAttributeOutput::FormatSurround( const SwFormatSurround& rSurround )
{
    if (m_rExport.SdrExporter().getTextFrameSyntax())
    {
        rtl::Reference<FastAttributeList> pAttrList(docx::SurroundToVMLWrap(rSurround));
        if (pAttrList)
        {
            m_rExport.SdrExporter().setFlyWrapAttrList(pAttrList);
        }
    }
    else if (m_rExport.SdrExporter().getDMLTextFrameSyntax())
    {
    }
    else if ( m_rExport.m_bOutFlyFrameAttrs )
    {
        std::string_view sWrap;
        switch ( rSurround.GetSurround( ) )
        {
            case css::text::WrapTextMode_NONE:
                sWrap = "none";
                break;
            case css::text::WrapTextMode_THROUGH:
                sWrap = "through";
                break;
            case css::text::WrapTextMode_DYNAMIC:
            case css::text::WrapTextMode_PARALLEL:
            case css::text::WrapTextMode_LEFT:
            case css::text::WrapTextMode_RIGHT:
            default:
                sWrap = "around";
        }

        AddToAttrList( m_rExport.SdrExporter().getFlyAttrList(), FSNS( XML_w, XML_wrap ), sWrap );
    }
}

void DocxAttributeOutput::FormatVertOrientation( const SwFormatVertOrient& rFlyVert )
{
    OString sAlign   = convertToOOXMLVertOrient( rFlyVert.GetVertOrient() );
    OString sVAnchor = convertToOOXMLVertOrientRel( rFlyVert.GetRelationOrient() );

    if (m_rExport.SdrExporter().getTextFrameSyntax())
    {
        m_rExport.SdrExporter().getTextFrameStyle().append(";margin-top:" + OString::number(double(rFlyVert.GetPos()) / 20) + "pt");
        if ( !sAlign.isEmpty() )
            m_rExport.SdrExporter().getTextFrameStyle().append(";mso-position-vertical:" + sAlign);
        m_rExport.SdrExporter().getTextFrameStyle().append(";mso-position-vertical-relative:" + sVAnchor);
    }
    else if (m_rExport.SdrExporter().getDMLTextFrameSyntax())
    {
    }
    else if ( m_rExport.m_bOutFlyFrameAttrs )
    {
        if ( !sAlign.isEmpty() )
            AddToAttrList( m_rExport.SdrExporter().getFlyAttrList(), FSNS( XML_w, XML_yAlign ), sAlign );
        else
            AddToAttrList( m_rExport.SdrExporter().getFlyAttrList(), FSNS( XML_w, XML_y ),
                OString::number( rFlyVert.GetPos() ) );
        AddToAttrList( m_rExport.SdrExporter().getFlyAttrList(), FSNS( XML_w, XML_vAnchor ), sVAnchor );
    }
}

void DocxAttributeOutput::FormatHorizOrientation( const SwFormatHoriOrient& rFlyHori )
{
    OString sAlign   = convertToOOXMLHoriOrient( rFlyHori.GetHoriOrient(), rFlyHori.IsPosToggle() );
    OString sHAnchor = convertToOOXMLHoriOrientRel( rFlyHori.GetRelationOrient() );

    if (m_rExport.SdrExporter().getTextFrameSyntax())
    {
        m_rExport.SdrExporter().getTextFrameStyle().append(";margin-left:" + OString::number(double(rFlyHori.GetPos()) / 20) + "pt");
        if ( !sAlign.isEmpty() )
            m_rExport.SdrExporter().getTextFrameStyle().append(";mso-position-horizontal:" + sAlign);
        m_rExport.SdrExporter().getTextFrameStyle().append(";mso-position-horizontal-relative:" + sHAnchor);
    }
    else if (m_rExport.SdrExporter().getDMLTextFrameSyntax())
    {
    }
    else if ( m_rExport.m_bOutFlyFrameAttrs )
    {
        if ( !sAlign.isEmpty() )
            AddToAttrList( m_rExport.SdrExporter().getFlyAttrList(), FSNS( XML_w, XML_xAlign ), sAlign );
        else
            AddToAttrList( m_rExport.SdrExporter().getFlyAttrList(), FSNS( XML_w, XML_x ),
                OString::number( rFlyHori.GetPos() ) );
        AddToAttrList( m_rExport.SdrExporter().getFlyAttrList(), FSNS( XML_w, XML_hAnchor ), sHAnchor );
    }
}

void DocxAttributeOutput::FormatAnchor( const SwFormatAnchor& )
{
    // Fly frames: anchors here aren't matching the anchors in docx
}

static std::optional<sal_Int32> lcl_getDmlAlpha(const SvxBrushItem& rBrush)
{
    std::optional<sal_Int32> oRet;
    sal_Int32 nTransparency = 255 - rBrush.GetColor().GetAlpha();
    if (nTransparency)
    {
        // Convert transparency to percent
        sal_Int8 nTransparencyPercent = SvxBrushItem::TransparencyToPercent(nTransparency);

        // Calculate alpha value
        // Consider oox/source/drawingml/color.cxx : getTransparency() function.
        sal_Int32 nAlpha = ::oox::drawingml::MAX_PERCENT - ( ::oox::drawingml::PER_PERCENT * nTransparencyPercent );
        oRet = nAlpha;
    }
    return oRet;
}

void DocxAttributeOutput::FormatBackground( const SvxBrushItem& rBrush )
{
    const Color aColor = rBrush.GetColor();
    model::ComplexColor const& rComplexColor = rBrush.getComplexColor();
    OString sColor = msfilter::util::ConvertColor( aColor.GetRGBColor() );
    std::optional<sal_Int32> oAlpha = lcl_getDmlAlpha(rBrush);
    if (m_rExport.SdrExporter().getTextFrameSyntax())
    {
        // Handle 'Opacity'
        if (oAlpha)
        {
            // Calculate opacity value
            // Consider oox/source/vml/vmlformatting.cxx : decodeColor() function.
            double fOpacity = static_cast<double>(*oAlpha) * 65535 / ::oox::drawingml::MAX_PERCENT;

            AddToAttrList( m_rExport.SdrExporter().getFlyFillAttrList(), XML_opacity, OString::number(fOpacity) + "f" );
        }

        AddToAttrList(m_rExport.SdrExporter().getFlyAttrList(), XML_fillcolor, "#" + sColor );
        lclAddThemeFillColorAttributes(m_rExport.SdrExporter().getFlyAttrList(), rComplexColor);
    }
    else if (m_rExport.SdrExporter().getDMLTextFrameSyntax())
    {
        bool bImageBackground = false;
        const SfxPoolItem* pItem = GetExport().HasItem(XATTR_FILLSTYLE);
        if (pItem)
        {
            const XFillStyleItem* pFillStyle = static_cast<const XFillStyleItem*>(pItem);
            if(pFillStyle->GetValue() == drawing::FillStyle_BITMAP)
            {
                bImageBackground = true;
            }
        }
        if (!bImageBackground)
        {
            m_pSerializer->startElementNS(XML_a, XML_solidFill);
            m_pSerializer->startElementNS(XML_a, XML_srgbClr, XML_val, sColor);
            if (oAlpha)
                m_pSerializer->singleElementNS(XML_a, XML_alpha,
                                              XML_val, OString::number(*oAlpha));
            m_pSerializer->endElementNS(XML_a, XML_srgbClr);
            m_pSerializer->endElementNS(XML_a, XML_solidFill);
        }
    }
    else if ( !m_rExport.m_bOutPageDescs )
    {
        // compare fill color with the original fill color
        OString sOriginalFill = OUStringToOString(
                m_sOriginalBackgroundColor, RTL_TEXTENCODING_UTF8 );

        if ( aColor == COL_AUTO )
            sColor = "auto"_ostr;

        if( !m_pBackgroundAttrList.is() )
        {
            m_pBackgroundAttrList = FastSerializerHelper::createAttrList();
            m_pBackgroundAttrList->add(FSNS(XML_w, XML_fill), sColor);
            m_pBackgroundAttrList->add( FSNS( XML_w, XML_val ), "clear" );
        }
        else if ( sOriginalFill != sColor )
        {
            // fill was modified during edition, theme fill attribute must be dropped
            m_pBackgroundAttrList = FastSerializerHelper::createAttrList();
            m_pBackgroundAttrList->add(FSNS(XML_w, XML_fill), sColor);
            m_pBackgroundAttrList->add( FSNS( XML_w, XML_val ), "clear" );
        }
        m_sOriginalBackgroundColor.clear();
    }
}

void DocxAttributeOutput::FormatFillStyle( const XFillStyleItem& rFillStyle )
{
    if (!m_bIgnoreNextFill)
        m_oFillStyle = rFillStyle.GetValue();
    else
    {
        m_bIgnoreNextFill = false;
        // ITEM: Still need to signal that ::FormatFillStyle was called so that
        // ::FormatFillGradient does not assert but do nothing
        m_oFillStyle = drawing::FillStyle_NONE;
    }

    // Don't round-trip grabbag OriginalBackground if the background has been cleared.
    if ( m_pBackgroundAttrList.is() && m_sOriginalBackgroundColor != "auto" && rFillStyle.GetValue() == drawing::FillStyle_NONE )
        m_pBackgroundAttrList.clear();
}

void DocxAttributeOutput::FormatFillGradient( const XFillGradientItem& rFillGradient )
{
    assert(m_oFillStyle && "ITEM: FormatFillStyle *has* to be called before FormatFillGradient(!)");
    if (m_oFillStyle && *m_oFillStyle == drawing::FillStyle_GRADIENT && !m_rExport.SdrExporter().getDMLTextFrameSyntax())
    {
        const basegfx::BGradient& rGradient = rFillGradient.GetGradientValue();
        OString sStartColor = msfilter::util::ConvertColor(Color(rGradient.GetColorStops().front().getStopColor()));
        OString sEndColor = msfilter::util::ConvertColor(Color(rGradient.GetColorStops().back().getStopColor()));

        const sal_Int32 nAngle = toDegrees(rGradient.GetAngle());
        if (nAngle != 0)
            AddToAttrList( m_rExport.SdrExporter().getFlyFillAttrList(),
                    XML_angle, OString::number(nAngle));

        // LO does linear gradients top to bottom, while MSO does bottom to top.
        // LO does axial gradients inner to outer, while MSO does outer to inner.
        OString sColor1 = sEndColor; // LO end color is MSO start color
        OString sColor2 = sStartColor; // LO start color is MSO end color

        switch (rGradient.GetGradientStyle())
        {
            case css::awt::GradientStyle_AXIAL:
            case css::awt::GradientStyle_LINEAR:
            {
                bool bIsSymmetrical = rGradient.GetGradientStyle() == css::awt::GradientStyle_AXIAL;
                if (!bIsSymmetrical)
                {
                    const basegfx::BColorStops& rColorStops = rGradient.GetColorStops();
                    if (rColorStops.size() > 2 && rColorStops.isSymmetrical())
                    {
                        for (auto& rStop : rColorStops)
                        {
                            if (basegfx::fTools::less(rStop.getStopOffset(), 0.5))
                                continue;
                            if (basegfx::fTools::more(rStop.getStopOffset(), 0.5))
                                break;

                            // from MSO export perspective, the inner color is the end color
                            sColor2 = msfilter::util::ConvertColor(Color(rStop.getStopColor()));
                            bIsSymmetrical = true;
                        }
                    }
                }

                if (bIsSymmetrical)
                    AddToAttrList( m_rExport.SdrExporter().getFlyFillAttrList(), XML_focus, "50%" );

                AddToAttrList(m_rExport.SdrExporter().getFlyFillAttrList(), XML_type, "gradient");
                break;
            }
            case css::awt::GradientStyle_RADIAL:
            case css::awt::GradientStyle_ELLIPTICAL:
            case css::awt::GradientStyle_SQUARE:
            case css::awt::GradientStyle_RECT:
                AddToAttrList(m_rExport.SdrExporter().getFlyFillAttrList(), XML_type,
                              "gradientRadial");
                // Since "focus" is not being written here, it defaults to 0.
                // A zero focus triggers a swap at LO import time, so a reverse swap is needed here.
                sColor1 = sStartColor;
                sColor2 = sEndColor;
                break;
            default:
                break;
        }

        AddToAttrList( m_rExport.SdrExporter().getFlyAttrList(), XML_fillcolor, "#" + sColor1 );
        AddToAttrList( m_rExport.SdrExporter().getFlyFillAttrList(), XML_color2, "#" + sColor2 );
    }
    else if (m_oFillStyle && *m_oFillStyle == drawing::FillStyle_GRADIENT && m_rExport.SdrExporter().getDMLTextFrameSyntax())
    {
        SwFrameFormat & rFormat(
                const_cast<SwFrameFormat&>(m_rExport.m_pParentFrame->GetFrameFormat()));
        rtl::Reference<SwXTextFrame> const xPropertySet =
            SwXTextFrame::CreateXTextFrame(rFormat.GetDoc(), &rFormat);
        m_rDrawingML.SetFS(m_pSerializer);
        m_rDrawingML.WriteGradientFill(uno::Reference<beans::XPropertySet>(static_cast<SwXFrame*>(xPropertySet.get())));
    }
    m_oFillStyle.reset();
}

void DocxAttributeOutput::FormatBox( const SvxBoxItem& rBox )
{
    if (m_rExport.SdrExporter().getDMLTextFrameSyntax())
    {
        // ugh, exporting fill here is quite some hack... this OutputItemSet abstraction is quite leaky
        // <a:gradFill> should be before <a:ln>.
        const SfxPoolItem* pItem = GetExport().HasItem(XATTR_FILLSTYLE);
        if (pItem)
        {
            const XFillStyleItem* pFillStyle = static_cast<const XFillStyleItem*>(pItem);
            FormatFillStyle(*pFillStyle);
            if (m_oFillStyle && *m_oFillStyle == drawing::FillStyle_BITMAP)
            {
                const SdrObject* pSdrObj = m_rExport.m_pParentFrame->GetFrameFormat().FindRealSdrObject();
                if (pSdrObj)
                {
                    uno::Reference< drawing::XShape > xShape( const_cast<SdrObject*>(pSdrObj)->getUnoShape(), uno::UNO_QUERY );
                    uno::Reference< beans::XPropertySet > xPropertySet( xShape, uno::UNO_QUERY );
                    m_rDrawingML.SetFS(m_pSerializer);
                    m_rDrawingML.WriteBlipFill(xPropertySet, u"BackGraphic"_ustr);
                }
            }
        }

        pItem = GetExport().HasItem(XATTR_FILLGRADIENT);
        if (pItem)
        {
            const XFillGradientItem* pFillGradient = static_cast<const XFillGradientItem*>(pItem);
            FormatFillGradient(*pFillGradient);
        }
        m_bIgnoreNextFill = true;
    }
    if (m_rExport.SdrExporter().getTextFrameSyntax() || m_rExport.SdrExporter().getDMLTextFrameSyntax())
    {
        const SvxBorderLine* pLeft = rBox.GetLeft( );
        const SvxBorderLine* pTop = rBox.GetTop( );
        const SvxBorderLine* pRight = rBox.GetRight( );
        const SvxBorderLine* pBottom = rBox.GetBottom( );

        if (pLeft && pRight && pTop && pBottom &&
                *pLeft == *pRight && *pLeft == *pTop && *pLeft == *pBottom)
        {
            // Check border style
            SvxBorderLineStyle eBorderStyle = pTop->GetBorderLineStyle();
            if (eBorderStyle == SvxBorderLineStyle::NONE)
            {
                if (m_rExport.SdrExporter().getTextFrameSyntax())
                {
                    AddToAttrList( m_rExport.SdrExporter().getFlyAttrList(),
                            XML_stroked, "f", XML_strokeweight, "0pt" );
                }
            }
            else
            {
                OString sColor(msfilter::util::ConvertColor(pTop->GetColor()));
                double const fConverted(editeng::ConvertBorderWidthToWord(pTop->GetBorderLineStyle(), pTop->GetWidth()));

                if (m_rExport.SdrExporter().getTextFrameSyntax())
                {
                    sal_Int32 nWidth = sal_Int32(fConverted / 20);
                    AddToAttrList( m_rExport.SdrExporter().getFlyAttrList(),
                            XML_strokecolor, "#" + sColor,
                            XML_strokeweight, OString::number(nWidth) + "pt" );
                    if( SvxBorderLineStyle::DASHED == pTop->GetBorderLineStyle() ) // Line Style is Dash type
                        AddToAttrList( m_rExport.SdrExporter().getDashLineStyle(),
                            XML_dashstyle, "dash" );
                }
                else
                    m_rExport.SdrExporter().writeBoxItemLine(rBox);
            }
        }

        if (m_rExport.SdrExporter().getDMLTextFrameSyntax())
        {
            m_rExport.SdrExporter().getBodyPrAttrList()->add(XML_lIns, OString::number(TwipsToEMU(rBox.GetDistance(SvxBoxItemLine::LEFT))));
            m_rExport.SdrExporter().getBodyPrAttrList()->add(XML_tIns, OString::number(TwipsToEMU(rBox.GetDistance(SvxBoxItemLine::TOP))));
            m_rExport.SdrExporter().getBodyPrAttrList()->add(XML_rIns, OString::number(TwipsToEMU(rBox.GetDistance(SvxBoxItemLine::RIGHT))));
            m_rExport.SdrExporter().getBodyPrAttrList()->add(XML_bIns, OString::number(TwipsToEMU(rBox.GetDistance(SvxBoxItemLine::BOTTOM))));
            return;
        }

        // v:textbox's inset attribute: inner margin values for textbox text - write only non-default values
        double fDistanceLeftTwips = double(rBox.GetDistance(SvxBoxItemLine::LEFT));
        double fDistanceTopTwips = double(rBox.GetDistance(SvxBoxItemLine::TOP));
        double fDistanceRightTwips = double(rBox.GetDistance(SvxBoxItemLine::RIGHT));
        double fDistanceBottomTwips = double(rBox.GetDistance(SvxBoxItemLine::BOTTOM));

        // Convert 'TWIPS' to 'INCH' (because in Word the default values are in Inches)
        double fDistanceLeftInch = o3tl::convert(fDistanceLeftTwips, o3tl::Length::twip, o3tl::Length::in);
        double fDistanceTopInch = o3tl::convert(fDistanceTopTwips, o3tl::Length::twip, o3tl::Length::in);
        double fDistanceRightInch = o3tl::convert(fDistanceRightTwips, o3tl::Length::twip, o3tl::Length::in);
        double fDistanceBottomInch = o3tl::convert(fDistanceBottomTwips, o3tl::Length::twip, o3tl::Length::in);

        // This code will write ONLY the non-default values. The values are in 'left','top','right','bottom' order.
        // so 'bottom' is checked if it is default and if it is non-default - all the values will be written
        // otherwise - 'right' is checked if it is default and if it is non-default - all the values except for 'bottom' will be written
        // and so on.
        OStringBuffer aInset;
        if(!aInset.isEmpty() || fDistanceBottomInch != 0.05)
            aInset.insert(0, Concat2View("," + OString::number(fDistanceBottomInch) + "in"));

        if(!aInset.isEmpty() || fDistanceRightInch != 0.1)
            aInset.insert(0, Concat2View("," + OString::number(fDistanceRightInch) + "in"));

        if(!aInset.isEmpty() || fDistanceTopInch != 0.05)
            aInset.insert(0, Concat2View("," + OString::number(fDistanceTopInch) + "in"));

        if(!aInset.isEmpty() || fDistanceLeftInch != 0.1)
            aInset.insert(0, Concat2View(OString::number(fDistanceLeftInch) + "in"));

        if (!aInset.isEmpty())
            m_rExport.SdrExporter().getTextboxAttrList()->add(XML_inset, aInset);

        return;
    }

    OutputBorderOptions aOutputBorderOptions = lcl_getBoxBorderOptions();
    // Check if there is a shadow item
    const SfxPoolItem* pItem = GetExport().HasItem( RES_SHADOW );
    if ( pItem )
    {
        const SvxShadowItem* pShadowItem = static_cast<const SvxShadowItem*>(pItem);
        aOutputBorderOptions.aShadowLocation = pShadowItem->GetLocation();
    }

    if ( m_bOpenedSectPr && !GetWritingHeaderFooter())
        return;

    // Not inside a section

    // Open the paragraph's borders tag
    m_pSerializer->startElementNS(XML_w, XML_pBdr);

    std::map<SvxBoxItemLine, css::table::BorderLine2> aStyleBorders;
    const SvxBoxItem* pInherited = nullptr;
    if ( GetExport().m_pStyAttr )
        pInherited = GetExport().m_pStyAttr->GetItem<SvxBoxItem>(RES_BOX);
    else if ( GetExport().m_pCurrentStyle && GetExport().m_pCurrentStyle->DerivedFrom() )
        pInherited = GetExport().m_pCurrentStyle->DerivedFrom()->GetAttrSet().GetItem<SvxBoxItem>(RES_BOX);

    if ( pInherited )
    {
        aStyleBorders[ SvxBoxItemLine::TOP ] = SvxBoxItem::SvxLineToLine(pInherited->GetTop(), /*bConvert=*/false);
        aStyleBorders[ SvxBoxItemLine::BOTTOM ] = SvxBoxItem::SvxLineToLine(pInherited->GetBottom(), false);
        aStyleBorders[ SvxBoxItemLine::LEFT ] = SvxBoxItem::SvxLineToLine(pInherited->GetLeft(), false);
        aStyleBorders[ SvxBoxItemLine::RIGHT ] = SvxBoxItem::SvxLineToLine(pInherited->GetRight(), false);
    }
    bool bUseFrame = m_aFramePr.UseFrameBorders(!m_xTableWrt ? -1 : m_tableReference.m_nTableDepth);
    impl_borders(m_pSerializer, rBox, aOutputBorderOptions, aStyleBorders,
                 bUseFrame ? m_aFramePr.Frame() : nullptr);

    // Close the paragraph's borders tag
    m_pSerializer->endElementNS( XML_w, XML_pBdr );

    m_aFramePr.SetUseFrameBorders(false);
}

void DocxAttributeOutput::FormatColumns_Impl( sal_uInt16 nCols, const SwFormatCol& rCol, bool bEven, SwTwips nPageSize )
{
    // Get the columns attributes
    rtl::Reference<FastAttributeList> pColsAttrList = FastSerializerHelper::createAttrList();

    pColsAttrList->add( FSNS( XML_w, XML_num ), OString::number( nCols ) );

    std::string_view pEquals = "false";
    if ( bEven )
    {
        sal_uInt16 nWidth = rCol.GetGutterWidth( true );
        pColsAttrList->add( FSNS( XML_w, XML_space ), OString::number( nWidth ) );

        pEquals = "true";
    }

    pColsAttrList->add( FSNS( XML_w, XML_equalWidth ), pEquals );

    bool bHasSep = (COLADJ_NONE != rCol.GetLineAdj());

    pColsAttrList->add( FSNS( XML_w, XML_sep ), OString::boolean( bHasSep ) );

    // Write the element
    m_pSerializer->startElementNS( XML_w, XML_cols, pColsAttrList );

    // Write the columns width if non-equals
    const SwColumns & rColumns = rCol.GetColumns(  );
    if ( !bEven )
    {
        for ( sal_uInt16 n = 0; n < nCols; ++n )
        {
            rtl::Reference<FastAttributeList> pColAttrList = FastSerializerHelper::createAttrList();
            sal_uInt16 nWidth = rCol.CalcPrtColWidth( n, o3tl::narrowing<sal_uInt16>(nPageSize) );
            pColAttrList->add( FSNS( XML_w, XML_w ), OString::number( nWidth ) );

            if ( n + 1 != nCols )
            {
                sal_uInt16 nSpacing = rColumns[n].GetRight( ) + rColumns[n + 1].GetLeft( );
                pColAttrList->add( FSNS( XML_w, XML_space ), OString::number( nSpacing ) );
            }

            m_pSerializer->singleElementNS( XML_w, XML_col, pColAttrList );
        }
    }

    m_pSerializer->endElementNS( XML_w, XML_cols );
}

void DocxAttributeOutput::FormatKeep( const SvxFormatKeepItem& rItem )
{
    m_pSerializer->singleElementNS( XML_w, XML_keepNext,
            FSNS( XML_w, XML_val ), OString::boolean( rItem.GetValue() ) );
}

void DocxAttributeOutput::FormatTextGrid( const SwTextGridItem& rGrid )
{
    rtl::Reference<FastAttributeList> pGridAttrList = FastSerializerHelper::createAttrList();

    std::string_view sGridType;
    switch ( rGrid.GetGridType( ) )
    {
        default:
        case SwTextGrid::NONE:
            sGridType = "default";
            break;
        case SwTextGrid::LinesOnly:
            sGridType = "lines";
            break;
        case SwTextGrid::LinesAndChars:
            if ( rGrid.IsSnapToChars( ) )
                sGridType = "snapToChars";
            else
                sGridType = "linesAndChars";
            break;
    }
    pGridAttrList->add(FSNS(XML_w, XML_type), sGridType);

    sal_uInt16 nHeight = rGrid.GetBaseHeight() + rGrid.GetRubyHeight();
    pGridAttrList->add( FSNS( XML_w, XML_linePitch ),
            OString::number( nHeight ) );

    pGridAttrList->add( FSNS( XML_w, XML_charSpace ),
            OString::number( GridCharacterPitch( rGrid ) ) );

    m_pSerializer->singleElementNS( XML_w, XML_docGrid, pGridAttrList );
}

void DocxAttributeOutput::FormatLineNumbering( const SwFormatLineNumber& rNumbering )
{
    if ( !rNumbering.IsCount( ) )
        m_pSerializer->singleElementNS(XML_w, XML_suppressLineNumbers);
    else
        m_pSerializer->singleElementNS(XML_w, XML_suppressLineNumbers, FSNS(XML_w, XML_val), "0");
}

void DocxAttributeOutput::FormatFrameDirection( const SvxFrameDirectionItem& rDirection )
{
    OString sTextFlow;
    bool bBiDi = false;
    SvxFrameDirection nDir = rDirection.GetValue();

    if ( nDir == SvxFrameDirection::Environment )
        nDir = GetExport( ).GetDefaultFrameDirection( );

    switch ( nDir )
    {
        default:
        case SvxFrameDirection::Horizontal_LR_TB:
            sTextFlow = "lrTb"_ostr;
            break;
        case SvxFrameDirection::Horizontal_RL_TB:
            sTextFlow = "lrTb"_ostr;
            bBiDi = true;
            break;
        case SvxFrameDirection::Vertical_LR_TB: // ~ vert="mongolianVert"
            sTextFlow = "tbLrV"_ostr;
            break;
        case SvxFrameDirection::Vertical_RL_TB: // ~ vert="eaVert"
            sTextFlow = "tbRl"_ostr;
            break;
        case SvxFrameDirection::Vertical_LR_BT: // ~ vert="vert270"
            sTextFlow = "btLr"_ostr;
            break;
        case SvxFrameDirection::Vertical_RL_TB90: // ~ vert="vert"
            sTextFlow = "tbRlV"_ostr;
            break;
    }

    if ( m_rExport.m_bOutPageDescs )
    {
        m_pSerializer->singleElementNS(XML_w, XML_textDirection, FSNS(XML_w, XML_val), sTextFlow);
        if ( bBiDi )
            m_pSerializer->singleElementNS(XML_w, XML_bidi);
    }
    else if ( !m_rExport.m_bOutFlyFrameAttrs )
    {
        if ( bBiDi )
            m_pSerializer->singleElementNS(XML_w, XML_bidi, FSNS(XML_w, XML_val), "1");
        else
            m_pSerializer->singleElementNS(XML_w, XML_bidi, FSNS(XML_w, XML_val), "0");
        m_aFramePr.SetUseFrameTextDirection(false);
    }
}

void DocxAttributeOutput::ParaGrabBag(const SfxGrabBagItem& rItem)
{
    const std::map<OUString, css::uno::Any>& rMap = rItem.GetGrabBag();
    for ( const auto & rGrabBagElement : rMap )
    {
        if (rGrabBagElement.first == "MirrorIndents")
            m_pSerializer->singleElementNS(XML_w, XML_mirrorIndents);
        else if (rGrabBagElement.first == "ParaTopMarginBeforeAutoSpacing")
        {
            m_bParaBeforeAutoSpacing = true;
            // get fixed value which was set during import
            rGrabBagElement.second >>= m_nParaBeforeSpacing;
            m_nParaBeforeSpacing = o3tl::toTwips(m_nParaBeforeSpacing, o3tl::Length::mm100);
            SAL_INFO("sw.ww8", "DocxAttributeOutput::ParaGrabBag: property =" << rGrabBagElement.first << " : m_nParaBeforeSpacing= " << m_nParaBeforeSpacing);
        }
        else if (rGrabBagElement.first == "ParaBottomMarginAfterAutoSpacing")
        {
            m_bParaAfterAutoSpacing = true;
            // get fixed value which was set during import
            rGrabBagElement.second >>= m_nParaAfterSpacing;
            m_nParaAfterSpacing = o3tl::toTwips(m_nParaAfterSpacing, o3tl::Length::mm100);
            SAL_INFO("sw.ww8", "DocxAttributeOutput::ParaGrabBag: property =" << rGrabBagElement.first << " : m_nParaBeforeSpacing= " << m_nParaAfterSpacing);
        }
        else if (rGrabBagElement.first == "CharThemeFill")
        {
            uno::Sequence<beans::PropertyValue> aGrabBagSeq;
            rGrabBagElement.second >>= aGrabBagSeq;

            for (const auto& rProp : aGrabBagSeq)
            {
                OUString sVal = rProp.Value.get<OUString>();

                if (sVal.isEmpty())
                    continue;

                if (rProp.Name == "val")
                    AddToAttrList(m_pBackgroundAttrList, FSNS(XML_w, XML_val), sVal);
                else if (rProp.Name == "color")
                    AddToAttrList(m_pBackgroundAttrList, FSNS(XML_w, XML_color), sVal);
                else if (rProp.Name == "themeColor")
                    AddToAttrList(m_pBackgroundAttrList, FSNS(XML_w, XML_themeColor), sVal);
                else if (rProp.Name == "themeTint")
                    AddToAttrList(m_pBackgroundAttrList, FSNS(XML_w, XML_themeTint), sVal);
                else if (rProp.Name == "themeShade")
                    AddToAttrList(m_pBackgroundAttrList, FSNS(XML_w, XML_themeShade), sVal);
                else if (rProp.Name == "fill")
                    AddToAttrList(m_pBackgroundAttrList, FSNS(XML_w, XML_fill), sVal);
                else if (rProp.Name == "themeFill")
                    AddToAttrList(m_pBackgroundAttrList, FSNS(XML_w, XML_themeFill), sVal);
                else if (rProp.Name == "themeFillTint")
                    AddToAttrList(m_pBackgroundAttrList, FSNS(XML_w, XML_themeFillTint), sVal);
                else if (rProp.Name == "themeFillShade")
                    AddToAttrList(m_pBackgroundAttrList, FSNS(XML_w, XML_themeFillShade), sVal);
                else if (rProp.Name == "originalColor")
                    rProp.Value >>= m_sOriginalBackgroundColor;
            }
        }
        else if (rGrabBagElement.first == "SdtPr")
        {
            const uno::Sequence<beans::PropertyValue> aGrabBagSdt =
                    rGrabBagElement.second.get< uno::Sequence<beans::PropertyValue> >();
            m_aParagraphSdt.GetSdtParamsFromGrabBag(aGrabBagSdt);
            m_aStartedParagraphSdtPrAlias = m_aParagraphSdt.m_aAlias;
        }
        else if (rGrabBagElement.first == "ParaCnfStyle")
        {
            uno::Sequence<beans::PropertyValue> aAttributes = rGrabBagElement.second.get< uno::Sequence<beans::PropertyValue> >();
            m_pTableStyleExport->CnfStyle(aAttributes);
        }
        else if (rGrabBagElement.first == "ParaSdtEndBefore")
        {
            // Handled already in StartParagraph().
        }
        else if (rGrabBagElement.first == "ParaInlineHeading")
            m_rExport.m_bParaInlineHeading = true;
        else
            SAL_WARN("sw.ww8", "DocxAttributeOutput::ParaGrabBag: unhandled grab bag property " << rGrabBagElement.first );
    }
}

void DocxAttributeOutput::CharGrabBag( const SfxGrabBagItem& rItem )
{
    if (m_bPreventDoubleFieldsHandling)
        return;

    const std::map< OUString, css::uno::Any >& rMap = rItem.GetGrabBag();

    // get original values of theme-derived properties to check if they have changed during the edition
    bool bWriteCSTheme = true;
    bool bWriteAsciiTheme = true;
    bool bWriteEastAsiaTheme = true;
    OUString sOriginalValue;
    for ( const auto & rGrabBagElement : rMap )
    {
        if ( m_pFontsAttrList.is() && rGrabBagElement.first == "CharThemeFontNameCs" )
        {
            if ( rGrabBagElement.second >>= sOriginalValue )
                bWriteCSTheme =
                        ( m_pFontsAttrList->getOptionalValue( FSNS( XML_w, XML_cs ) ) == sOriginalValue );
        }
        else if ( m_pFontsAttrList.is() && rGrabBagElement.first == "CharThemeFontNameAscii" )
        {
            if ( rGrabBagElement.second >>= sOriginalValue )
                bWriteAsciiTheme =
                        ( m_pFontsAttrList->getOptionalValue( FSNS( XML_w, XML_ascii ) ) == sOriginalValue );
        }
        else if ( m_pFontsAttrList.is() && rGrabBagElement.first == "CharThemeFontNameEastAsia" )
        {
            if ( rGrabBagElement.second >>= sOriginalValue )
                bWriteEastAsiaTheme =
                        ( m_pFontsAttrList->getOptionalValue( FSNS( XML_w, XML_eastAsia ) ) == sOriginalValue );
        }
    }

    // save theme attributes back to the run properties
    OUString str;
    for ( const auto & rGrabBagElement : rMap )
    {
        if ( rGrabBagElement.first == "CharThemeNameAscii" && bWriteAsciiTheme )
        {
            rGrabBagElement.second >>= str;
            AddToAttrList( m_pFontsAttrList, FSNS( XML_w, XML_asciiTheme ), str );
        }
        else if ( rGrabBagElement.first == "CharThemeNameCs" && bWriteCSTheme )
        {
            rGrabBagElement.second >>= str;
            AddToAttrList( m_pFontsAttrList, FSNS( XML_w, XML_cstheme ), str );
        }
        else if ( rGrabBagElement.first == "CharThemeNameEastAsia" && bWriteEastAsiaTheme )
        {
            rGrabBagElement.second >>= str;
            AddToAttrList( m_pFontsAttrList, FSNS( XML_w, XML_eastAsiaTheme ), str );
        }
        else if ( rGrabBagElement.first == "CharThemeNameHAnsi" && bWriteAsciiTheme )
        // this is not a mistake: in LibO we don't directly support the hAnsi family
        // of attributes so we save the same value from ascii attributes instead
        {
            rGrabBagElement.second >>= str;
            AddToAttrList( m_pFontsAttrList, FSNS( XML_w, XML_hAnsiTheme ), str );
        }
        else if( rGrabBagElement.first == "CharThemeFontNameCs"   ||
                rGrabBagElement.first == "CharThemeFontNameAscii" ||
                rGrabBagElement.first == "CharThemeFontNameEastAsia" ||
                rGrabBagElement.first == "CharThemeOriginalColor" )
        {
            // just skip these, they were processed before
        }
        else if(rGrabBagElement.first == "CharGlowTextEffect" ||
                rGrabBagElement.first == "CharShadowTextEffect" ||
                rGrabBagElement.first == "CharReflectionTextEffect" ||
                rGrabBagElement.first == "CharTextOutlineTextEffect" ||
                rGrabBagElement.first == "CharScene3DTextEffect" ||
                rGrabBagElement.first == "CharProps3DTextEffect" ||
                rGrabBagElement.first == "CharLigaturesTextEffect" ||
                rGrabBagElement.first == "CharNumFormTextEffect" ||
                rGrabBagElement.first == "CharNumSpacingTextEffect" ||
                rGrabBagElement.first == "CharStylisticSetsTextEffect" ||
                rGrabBagElement.first == "CharCntxtAltsTextEffect")
        {
            beans::PropertyValue aPropertyValue;
            rGrabBagElement.second >>= aPropertyValue;
            m_aTextEffectsGrabBag.push_back(aPropertyValue);
        }
        else if (rGrabBagElement.first == "CharTextFillTextEffect")
        {
            beans::PropertyValue aPropertyValue;
            rGrabBagElement.second >>= aPropertyValue;
            m_aTextFillGrabBag.push_back(aPropertyValue);
        }
        else if (rGrabBagElement.first == "SdtEndBefore")
        {
            if (m_aRunSdt.m_bStartedSdt)
                m_bEndCharSdt = true;
        }
        else if (rGrabBagElement.first == "SdtPr" && FLY_NOT_PROCESSED != m_nStateOfFlyFrame )
        {
            const uno::Sequence<beans::PropertyValue> aGrabBagSdt =
                    rGrabBagElement.second.get< uno::Sequence<beans::PropertyValue> >();
            m_aRunSdt.GetSdtParamsFromGrabBag(aGrabBagSdt);
        }
        else
            SAL_INFO("sw.ww8", "DocxAttributeOutput::CharGrabBag: unhandled grab bag property " << rGrabBagElement.first);
    }
}

DocxAttributeOutput::DocxAttributeOutput( DocxExport &rExport, const FSHelperPtr& pSerializer, oox::drawingml::DrawingML* pDrawingML )
    : AttributeOutputBase(rExport.GetFilter().getFileUrl()),
      m_rExport( rExport ),
      m_pSerializer( pSerializer ),
      m_rDrawingML( *pDrawingML ),
      m_bEndCharSdt(false),
      m_endPageRef( false ),
      m_pFootnotesList( new ::docx::FootnotesList() ),
      m_pEndnotesList( new ::docx::FootnotesList() ),
      m_footnoteEndnoteRefTag( 0 ),
      m_pRedlineData( nullptr ),
      m_nRedlineId( 0 ),
      m_bOpenedSectPr( false ),
      m_bHadSectPr(false),
      m_bOpenedParaPr( false ),
      m_bRunTextIsOn( false ),
      m_bWritingHeaderFooter( false ),
      m_bAnchorLinkedToNode(false),
      m_bWritingField( false ),
      m_bPreventDoubleFieldsHandling( false ),
      m_nNextBookmarkId( 0 ),
      m_nNextAnnotationMarkId( 0 ),
      m_nEmbedFlyLevel(0),
      m_pMoveRedlineData(nullptr),
      m_bParagraphOpened( false ),
      m_bParagraphFrameOpen( false ),
      m_bIsFirstParagraph( true ),
      m_bAlternateContentChoiceOpen( false ),
      m_bPostponedProcessingFly( false ),
      m_nColBreakStatus( COLBRK_NONE ),
      m_bPostponedPageBreak( false ),
      m_nTextFrameLevel( 0 ),
      m_closeHyperlinkInThisRun( false ),
      m_closeHyperlinkInPreviousRun( false ),
      m_nFieldsInHyperlink( 0 ),
      m_bExportingOutline(false),
      m_nChartCount(0),
      m_PendingPlaceholder( nullptr ),
      m_postitFieldsMaxId( 0 ),
      m_nextFontId( 1 ),
      m_bIgnoreNextFill(false),
      m_pTableStyleExport(std::make_shared<DocxTableStyleExport>(rExport.m_rDoc, pSerializer)),
      m_bParaBeforeAutoSpacing(false),
      m_bParaAfterAutoSpacing(false),
      m_nParaBeforeSpacing(0),
      m_nParaAfterSpacing(0),
      m_bParaInlineHeading(false)
    , m_nStateOfFlyFrame( FLY_NOT_PROCESSED )
{
    m_nHyperLinkCount.push_back(0);
}

DocxAttributeOutput::~DocxAttributeOutput()
{
}

DocxExport& DocxAttributeOutput::GetExport()
{
    return m_rExport;
}

void DocxAttributeOutput::SetSerializer( ::sax_fastparser::FSHelperPtr const & pSerializer )
{
    m_pSerializer = pSerializer;
    m_pTableStyleExport->SetSerializer(pSerializer);
}

bool DocxAttributeOutput::HasFootnotes() const
{
    return !m_pFootnotesList->isEmpty();
}

bool DocxAttributeOutput::HasEndnotes() const
{
    return !m_pEndnotesList->isEmpty();
}

bool DocxAttributeOutput::HasPostitFields() const
{
    return !m_postitFields.empty();
}

void DocxAttributeOutput::BulletDefinition(int nId, const Graphic& rGraphic, Size aSize)
{
    m_pSerializer->startElementNS(XML_w, XML_numPicBullet,
            FSNS(XML_w, XML_numPicBulletId), OString::number(nId));

    // Size is in twips, we need it in points.
    OString aStyle = "width:" + OString::number(double(aSize.Width()) / 20)+ "pt;"
                     "height:" + OString::number(double(aSize.Height()) / 20) + "pt";
    m_pSerializer->startElementNS(XML_w, XML_pict);
    m_pSerializer->startElementNS( XML_v, XML_shape,
            XML_style, aStyle,
            FSNS(XML_o, XML_bullet), "t");

    OUString aRelId = m_rDrawingML.writeGraphicToStorage(rGraphic);
    m_pSerializer->singleElementNS( XML_v, XML_imagedata,
            FSNS(XML_r, XML_id), aRelId,
            FSNS(XML_o, XML_title), "");

    m_pSerializer->endElementNS(XML_v, XML_shape);
    m_pSerializer->endElementNS(XML_w, XML_pict);

    m_pSerializer->endElementNS(XML_w, XML_numPicBullet);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
