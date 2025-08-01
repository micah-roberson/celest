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

#ifndef INCLUDED_SW_SOURCE_FILTER_WW8_RTFATTRIBUTEOUTPUT_HXX
#define INCLUDED_SW_SOURCE_FILTER_WW8_RTFATTRIBUTEOUTPUT_HXX

#include <memory>
#include <com/sun/star/drawing/FillStyle.hpp>

#include "attributeoutputbase.hxx"
#include "rtfstringbuffer.hxx"

#include <wrtswtbl.hxx>

#include <rtl/strbuf.hxx>
#include <editeng/boxitem.hxx>
#include <unotools/securityoptions.hxx>

#include <optional>

class SwGrfNode;
class SwOLENode;
class SwFlyFrameFormat;
class RtfExport;

/// The class that has handlers for various resource types when exporting as RTF
class RtfAttributeOutput : public AttributeOutputBase
{
    friend class RtfStringBufferValue;
    friend class SaveRunState;

public:
    /// Export the state of RTL/CJK.
    void RTLAndCJKState(bool bIsRTL, sal_uInt16 nScript) override;

    /// Start of the paragraph.
    sal_Int32 StartParagraph(const ww8::WW8TableNodeInfo::Pointer_t& pTextNodeInfo,
                             bool bGenerateParaId) override;

    /// End of the paragraph.
    void EndParagraph(const ww8::WW8TableNodeInfoInner::Pointer_t& pTextNodeInfoInner) override;

    /// Empty paragraph.
    void EmptyParagraph() override;

    /// Called in order to output section breaks.
    void SectionBreaks(const SwNode& rNode) override;

    /// Called before we start outputting the attributes.
    void StartParagraphProperties() override;

    /// Called after we end outputting the attributes.
    void EndParagraphProperties(const SfxItemSet& rParagraphMarkerProperties,
                                const SwRedlineData* pRedlineData,
                                const SwRedlineData* pRedlineParagraphMarkerDeleted,
                                const SwRedlineData* pRedlineParagraphMarkerInserted) override;

    /// Start of the text run.
    void StartRun(const SwRedlineData* pRedlineData, sal_Int32 nPos,
                  bool bSingleEmptyRun = false) override;

    /// End of the text run.
    void EndRun(const SwTextNode* pNode, sal_Int32 nPos, sal_Int32 nLen,
                bool bLastRun = false) override;

    /// Called before we start outputting the attributes.
    void StartRunProperties() override;

    /// Called after we end outputting the attributes.
    void EndRunProperties(const SwRedlineData* pRedlineData) override;

    /// Output text (inside a run).
    void RunText(const OUString& rText, rtl_TextEncoding eCharSet = RTL_TEXTENCODING_UTF8,
                 const OUString& rSymbolFont = OUString()) override;

    // Access to (anyway) private buffers, used by the sdr exporter
    OStringBuffer& RunText();

    enum GetCharacterPropertiesMode
    {
        StyleDefinition,
        ForRun,
    };
    OString MoveProperties(GetCharacterPropertiesMode mode);

    /// Output text (without markup).
    void RawText(const OUString& rText, rtl_TextEncoding eCharSet) override;

    /// Output ruby start.
    void StartRuby(const SwTextNode& rNode, sal_Int32 nPos, const SwFormatRuby& rRuby) override;

    /// Output ruby end.
    void EndRuby(const SwTextNode& rNode, sal_Int32 nPos, bool bEmptyBaseText) override;

    /// Output URL start.
    bool StartURL(const OUString& rUrl, const OUString& rTarget,
                  const OUString& rName = OUString()) override;

    /// Output URL end.
    bool EndURL(bool isAtEndOfParagraph) override;

    void FieldVanish(const OUString& rText, ww::eField eType,
                     OUString const* pBookmarkName) override;

    /// Output redlining.
    ///
    /// The common attribute that can be among the run properties.
    void Redline(const SwRedlineData* pRedline) override;

    void FormatDrop(const SwTextNode& rNode, const SwFormatDrop& rSwFormatDrop, sal_uInt16 nStyle,
                    ww8::WW8TableNodeInfo::Pointer_t pTextNodeInfo,
                    ww8::WW8TableNodeInfoInner::Pointer_t pTextNodeInfoInner) override;

    /// Output style.
    void ParagraphStyle(sal_uInt16 nStyle) override;

    void
    TableInfoCell(const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner) override;
    void
    TableInfoRow(const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner) override;
    void
    TableDefinition(const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner) override;
    void TablePositioning(const SwFrameFormat* pFlyFormat);
    void TableDefaultBorders(
        const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner) override;
    void
    TableBackgrounds(const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner) override;
    void
    TableRowRedline(const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner) override;
    void
    TableCellRedline(const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner) override;
    void TableHeight(const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner) override;
    void
    TableCanSplit(const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner) override;
    void TableBidi(const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner) override;
    void TableVerticalCell(
        const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner) override;
    void TableNodeInfoInner(const ww8::WW8TableNodeInfoInner::Pointer_t& pNodeInfoInner) override;
    void
    TableOrientation(const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner) override;
    void
    TableSpacing(const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner) override;
    void TableRowEnd(sal_uInt32 nDepth) override;

    /// Start of the styles table.
    void StartStyles() override;

    /// End of the styles table.
    void EndStyles(sal_uInt16 nNumberOfStyles) override;

    /// Write default style.
    void DefaultStyle() override;

    /// Start of a style in the styles table.
    void StartStyle(const OUString& rName, StyleType eType, sal_uInt16 nBase, sal_uInt16 nNext,
                    sal_uInt16 nLink, sal_uInt16 nWwId, sal_uInt16 nSlot,
                    bool bAutoUpdate) override;

    /// End of a style in the styles table.
    void EndStyle() override;

    /// Start of (paragraph or run) properties of a style.
    void StartStyleProperties(bool bParProp, sal_uInt16 nStyle) override;

    /// End of (paragraph or run) properties of a style.
    void EndStyleProperties(bool bParProp) override;

    /// Numbering rule and Id.
    void OutlineNumbering(sal_uInt8 nLvl) override;

    /// Page break
    /// As a paragraph property - the paragraph should be on the next page.
    void PageBreakBefore(bool bBreak) override;

    /// Write a section break
    /// msword::ColumnBreak or msword::PageBreak
    void SectionBreak(sal_uInt8 nC, bool bBreakAfter, const WW8_SepInfo* pSectionInfo = nullptr,
                      bool bExtraPageBreak = false) override;

    /// Start of the section properties.
    void StartSection() override;

    /// End of the section properties.
    void EndSection() override;

    /// Protection of forms.
    void SectionFormProtection(bool bProtected) override;

    /// Numbering of the lines in the document.
    void SectionLineNumbering(sal_uLong nRestartNo, const SwLineNumberInfo& rLnNumInfo) override;

    /// Has different headers/footers for the title page.
    void SectionTitlePage() override;

    /// Description of the page borders.
    void SectionPageBorders(const SwFrameFormat* pFormat,
                            const SwFrameFormat* pFirstPageFormat) override;

    /// Columns populated from right/numbers on the right side?
    void SectionBiDi(bool bBiDi) override;

    /// The style of the page numbers.
    ///
    void SectionPageNumbering(sal_uInt16 nNumType,
                              const ::std::optional<sal_uInt16>& oPageRestartNumber) override;

    /// The type of breaking.
    void SectionType(sal_uInt8 nBreakCode) override;

    void SectFootnoteEndnotePr() override;

    void WriteFootnoteEndnotePr(bool bFootnote, const SwEndNoteInfo& rInfo);

    /// Definition of a numbering instance.
    void NumberingDefinition(sal_uInt16 nId, const SwNumRule& rRule) override;

    /// Start of the abstract numbering definition instance.
    void StartAbstractNumbering(sal_uInt16 nId) override;

    /// End of the abstract numbering definition instance.
    void EndAbstractNumbering() override;

    /// All the numbering level information.
    void NumberingLevel(sal_uInt8 nLevel, sal_uInt16 nStart, sal_uInt16 nNumberingType,
                        SvxAdjust eAdjust, const sal_uInt8* pNumLvlPos, sal_uInt8 nFollow,
                        const wwFont* pFont, const SfxItemSet* pOutSet, sal_Int16 nIndentAt,
                        sal_Int16 nFirstLineIndex, sal_Int16 nListTabPos,
                        const OUString& rNumberingString,
                        const SvxBrushItem* pBrush, //For i120928,to export graphic of bullet
                        bool isLegal) override;

    void WriteField_Impl(const SwField* pField, ww::eField eType, std::u16string_view rFieldCmd,
                         FieldFlags nMode);
    void WriteBookmarks_Impl(std::vector<OUString>& rStarts, std::vector<OUString>& rEnds);
    void WriteAnnotationMarks_Impl(std::vector<SwMarkName>& rStarts,
                                   std::vector<SwMarkName>& rEnds);
    void WriteHeaderFooter_Impl(const SwFrameFormat& rFormat, bool bHeader, const char* pStr,
                                bool bTitlepg);
    void WriteBookmarkInActParagraph(const OUString& /*rName*/, sal_Int32 /*nFirstRunPos*/,
                                     sal_Int32 /*nLastRunPos*/) override{};

protected:
    /// Output frames - the implementation.
    void OutputFlyFrame_Impl(const ww8::Frame& rFrame, const Point& rNdTopLeft) override;

    /// Sfx item Sfx item RES_CHRATR_CASEMAP
    void CharCaseMap(const SvxCaseMapItem& rCaseMap) override;

    /// Sfx item Sfx item RES_CHRATR_COLOR
    void CharColor(const SvxColorItem& rColor) override;

    /// Sfx item Sfx item RES_CHRATR_CONTOUR
    void CharContour(const SvxContourItem& rContour) override;

    /// Sfx item RES_CHRATR_CROSSEDOUT
    void CharCrossedOut(const SvxCrossedOutItem& rCrossedOut) override;

    /// Sfx item RES_CHRATR_ESCAPEMENT
    void CharEscapement(const SvxEscapementItem& rEscapement) override;

    /// Sfx item RES_CHRATR_FONT
    void CharFont(const SvxFontItem& rFont) override;

    /// Sfx item RES_CHRATR_FONTSIZE
    void CharFontSize(const SvxFontHeightItem& rFontSize) override;

    /// Sfx item RES_CHRATR_KERNING
    void CharKerning(const SvxKerningItem& rKerning) override;

    /// Sfx item RES_CHRATR_LANGUAGE
    void CharLanguage(const SvxLanguageItem& rLanguage) override;

    /// Sfx item RES_CHRATR_POSTURE
    void CharPosture(const SvxPostureItem& rPosture) override;

    /// Sfx item RES_CHRATR_SHADOWED
    void CharShadow(const SvxShadowedItem& rShadow) override;

    /// Sfx item RES_CHRATR_UNDERLINE
    void CharUnderline(const SvxUnderlineItem& rUnderline) override;

    /// Sfx item RES_CHRATR_WEIGHT
    void CharWeight(const SvxWeightItem& rWeight) override;

    /// Sfx item RES_CHRATR_AUTOKERN
    void CharAutoKern(const SvxAutoKernItem& rAutoKern) override;

    /// Sfx item RES_CHRATR_BLINK
    void CharAnimatedText(const SvxBlinkItem& rBlink) override;

    /// Sfx item RES_CHRATR_BACKGROUND
    void CharBackground(const SvxBrushItem& rBrush) override;

    /// Sfx item RES_CHRATR_CJK_FONT
    void CharFontCJK(const SvxFontItem& rFont) override;

    /// Sfx item RES_CHRATR_CJK_FONTSIZE
    void CharFontSizeCJK(const SvxFontHeightItem& rFontSize) override;

    /// Sfx item RES_CHRATR_CJK_LANGUAGE
    void CharLanguageCJK(const SvxLanguageItem& rLanguageItem) override;

    /// Sfx item RES_CHRATR_CJK_POSTURE
    void CharPostureCJK(const SvxPostureItem& rPosture) override;

    /// Sfx item RES_CHRATR_CJK_WEIGHT
    void CharWeightCJK(const SvxWeightItem& rWeight) override;

    /// Sfx item RES_CHRATR_CTL_FONT
    void CharFontCTL(const SvxFontItem& rFont) override;

    /// Sfx item RES_CHRATR_CTL_FONTSIZE
    void CharFontSizeCTL(const SvxFontHeightItem& rFontSize) override;

    /// Sfx item RES_CHRATR_CTL_LANGUAGE
    void CharLanguageCTL(const SvxLanguageItem& rLanguageItem) override;

    /// Sfx item RES_CHRATR_CTL_POSTURE
    void CharPostureCTL(const SvxPostureItem& rPosture) override;

    /// Sfx item RES_CHRATR_CTL_WEIGHT
    void CharWeightCTL(const SvxWeightItem& rWeight) override;

    /// Sfx item RES_CHRATR_BidiRTL
    void CharBidiRTL(const SfxPoolItem& rItem) override;

    /// Sfx item RES_CHRATR_ROTATE
    void CharRotate(const SvxCharRotateItem& rRotate) override;

    /// Sfx item RES_CHRATR_EMPHASIS_MARK
    void CharEmphasisMark(const SvxEmphasisMarkItem& rEmphasisMark) override;

    /// Sfx item RES_CHRATR_TWO_LINES
    void CharTwoLines(const SvxTwoLinesItem& rTwoLines) override;

    /// Sfx item RES_CHRATR_SCALEW
    void CharScaleWidth(const SvxCharScaleWidthItem& rScaleWidth) override;

    /// Sfx item RES_CHRATR_RELIEF
    void CharRelief(const SvxCharReliefItem& rRelief) override;

    /// Sfx item RES_CHRATR_HIDDEN
    void CharHidden(const SvxCharHiddenItem& rHidden) override;

    /// Sfx item RES_CHRATR_BOX
    void CharBorder(const SvxBoxItem& rBox) override;

    /// Sfx item RES_CHRATR_HIGHLIGHT
    void CharHighlight(const SvxBrushItem& rBrush) override;

    /// Sfx item RES_CHRATR_SCRIPT_HINT
    void CharScriptHint(const SvxScriptHintItem& rHint) override;

    /// Sfx item RES_TXTATR_INETFMT
    void TextINetFormat(const SwFormatINetFormat& rURL) override;

    /// Sfx item RES_TXTATR_CHARFMT
    void TextCharFormat(const SwFormatCharFormat& rCharFormat) override;

    /// Sfx item RES_TXTATR_FTN
    void TextFootnote_Impl(const SwFormatFootnote& rFootnote) override;

    /// Sfx item RES_PARATR_LINESPACING
    void ParaLineSpacing_Impl(short nSpace, short nMulti) override;

    /// Sfx item RES_PARATR_ADJUST
    void ParaAdjust(const SvxAdjustItem& rAdjust) override;

    /// Sfx item RES_PARATR_SPLIT
    void ParaSplit(const SvxFormatSplitItem& rSplit) override;

    /// Sfx item RES_PARATR_WIDOWS
    void ParaWidows(const SvxWidowsItem& rWidows) override;

    /// Sfx item RES_PARATR_TABSTOP
    void ParaTabStop(const SvxTabStopItem& rTabStop) override;

    /// Sfx item RES_PARATR_HYPHENZONE
    void ParaHyphenZone(const SvxHyphenZoneItem& rHyphenZone) override;

    /// Sfx item RES_PARATR_NUMRULE
    void ParaNumRule_Impl(const SwTextNode* pTextNd, sal_Int32 nLvl, sal_Int32 nNumId) override;

    /// Sfx item RES_PARATR_SCRIPTSPACE
    void ParaScriptSpace(const SfxBoolItem& rScriptSpace) override;

    /// Sfx item RES_PARATR_HANGINGPUNCTUATION
    void ParaHangingPunctuation(const SfxBoolItem& rItem) override;

    /// Sfx item RES_PARATR_FORBIDDEN_RULES
    void ParaForbiddenRules(const SfxBoolItem& rItem) override;

    /// Sfx item RES_PARATR_VERTALIGN
    void ParaVerticalAlign(const SvxParaVertAlignItem& rAlign) override;

    /// Sfx item RES_PARATR_SNAPTOGRID
    void ParaSnapToGrid(const SvxParaGridItem& rItem) override;

    /// Sfx item RES_FRM_SIZE
    void FormatFrameSize(const SwFormatFrameSize& rSize) override;

    /// Sfx item RES_PAPER_BIN
    void FormatPaperBin(const SvxPaperBinItem& rItem) override;

    /// Sfx item RES_MARGIN_FIRSTLINE
    virtual void FormatFirstLineIndent(const SvxFirstLineIndentItem& rFirstLine) override;
    /// Sfx item RES_MARGIN_TEXTLEFT
    virtual void FormatTextLeftMargin(const SvxTextLeftMarginItem& rTextLeftMargin) override;
    /// Sfx item RES_MARGIN_RIGHT
    virtual void FormatRightMargin(const SvxRightMarginItem& rRightMargin) override;

    /// Sfx item RES_LR_SPACE
    void FormatLRSpace(const SvxLRSpaceItem& rLRSpace) override;

    /// Sfx item RES_UL_SPACE
    void FormatULSpace(const SvxULSpaceItem& rULSpace) override;

    /// Sfx item RES_SURROUND
    void FormatSurround(const SwFormatSurround& rSurround) override;

    /// Sfx item RES_VERT_ORIENT
    void FormatVertOrientation(const SwFormatVertOrient& rFlyVert) override;

    /// Sfx item RES_HORI_ORIENT
    void FormatHorizOrientation(const SwFormatHoriOrient& rFlyHori) override;

    /// Sfx item RES_ANCHOR
    void FormatAnchor(const SwFormatAnchor& rAnchor) override;

    /// Sfx item RES_BACKGROUND
    void FormatBackground(const SvxBrushItem& rBrush) override;

    /// Sfx item RES_FILL_STYLE
    void FormatFillStyle(const XFillStyleItem& rFillStyle) override;

    /// Sfx item RES_FILL_GRADIENT
    void FormatFillGradient(const XFillGradientItem& rFillGradient) override;

    /// Sfx item RES_BOX
    void FormatBox(const SvxBoxItem& rBox) override;

    /// Sfx item RES_COL
    void FormatColumns_Impl(sal_uInt16 nCols, const SwFormatCol& rCol, bool bEven,
                            SwTwips nPageSize) override;

    /// Sfx item RES_KEEP
    void FormatKeep(const SvxFormatKeepItem& rItem) override;

    /// Sfx item RES_TEXTGRID
    void FormatTextGrid(const SwTextGridItem& rItem) override;

    /// Sfx item RES_LINENUMBER
    void FormatLineNumbering(const SwFormatLineNumber& rNumbering) override;

    /// Sfx item RES_FRAMEDIR
    void FormatFrameDirection(const SvxFrameDirectionItem& rDirection) override;

    /// Sfx item RES_PARATR_GRABBAG
    void ParaGrabBag(const SfxGrabBagItem& rItem) override;

    /// Sfx item RES_CHRATR_GRABBAG
    void CharGrabBag(const SfxGrabBagItem& rItem) override;

    /// Sfx item RES_PARATR_OUTLINELEVEL
    void ParaOutlineLevel(const SfxUInt16Item& rItem) override;

    /// Write the expanded field
    void WriteExpand(const SwField* pField) override;

    void RefField(const SwField& rField, const OUString& rRef) override;
    void HiddenField(const SwField& rField) override;
    void SetField(const SwField& rField, ww::eField eType, const OUString& rCmd) override;
    void PostitField(const SwField* pField) override;
    bool DropdownField(const SwField* pField) override;
    bool PlaceholderField(const SwField* pField) override;

    void SectionRtlGutter(const SfxBoolItem& rRtlGutter) override;

    void TextLineBreak(const SwFormatLineBreak& rLineBreak) override;

private:
    /// Reference to the export, where to get the data from
    RtfExport& m_rExport;

    OStringBuffer m_aTabStop;

    /// Access to the page style of the previous paragraph.
    const SwPageDesc* m_pPrevPageDesc;

    /// Output graphic fly frames.
    void FlyFrameGraphic(const SwFlyFrameFormat* pFlyFrameFormat, const SwGrfNode* pGrfNode);
    void FlyFrameOLE(const SwFlyFrameFormat* pFlyFrameFormat, SwOLENode& rOLENode,
                     const Size& rSize);
    void FlyFrameOLEReplacement(const SwFlyFrameFormat* pFlyFrameFormat, SwOLENode& rOLENode,
                                const Size& rSize);
    /// Math export.
    bool FlyFrameOLEMath(const SwFlyFrameFormat* pFlyFrameFormat, SwOLENode& rOLENode,
                         const Size& rSize);

    /*
     * Table methods.
     */
    void InitTableHelper(const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner);
    void StartTable();
    void StartTableRow(const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner);
    void StartTableCell();
    void TableCellProperties(const ww8::WW8TableNodeInfoInner::Pointer_t& pTableTextNodeInfoInner);
    void EndTableCell();
    void EndTableRow();
    void EndTable();

    /// End cell, row, and even the entire table if necessary.
    void FinishTableRowCell(const ww8::WW8TableNodeInfoInner::Pointer_t& pInner);

    void WriteTextFootnoteNumStr(const SwFormatFootnote& rFootnote);

    void EndRubyField();

    static void OutputCharCaseMap(const SvxCaseMapItem& rCaseMap, OStringBuffer& buf);
    void OutputCharColor(const SvxColorItem& rColor, OStringBuffer& buf) const;
    static void OutputCharContour(const SvxContourItem& rContour, OStringBuffer& buf);
    static void OutputCharCrossedOut(const SvxCrossedOutItem& rCrossedOut, OStringBuffer& buf);
    void OutputCharEscapement(const SvxEscapementItem& rEscapement, OStringBuffer& buf) const;
    void OutputCharFontAssoc(const SvxFontItem& rFont, OStringBuffer& buf, bool assoc) const;
    static void OutputCharFontSizeAssoc(const SvxFontHeightItem& rFontSize, OStringBuffer& buf,
                                        bool assoc);
    static void OutputCharKerning(const SvxKerningItem& rKerning, OStringBuffer& buf);
    static void OutputCharLanguageAssoc(const SvxLanguageItem& rLanguage, OStringBuffer& buf,
                                        bool assoc);
    static void OutputCharLanguageCJK(const SvxLanguageItem& rLanguage, OStringBuffer& buf);
    static void OutputCharPostureAssoc(const SvxPostureItem& rPosture, OStringBuffer& buf,
                                       bool assoc);
    static void OutputCharShadow(const SvxShadowedItem& rShadow, OStringBuffer& buf);
    // OutputCharUnderline is special. Some underline keywords have associated variants, some don't.
    // Therefore, the two functions each output their parts of keywords.
    void OutputCharUnderline(const SvxUnderlineItem& rUnderline, OStringBuffer& buf) const;
    void OutputCharUnderlineAssoc(const SvxUnderlineItem& rUnderline, OStringBuffer& buf,
                                  bool assoc) const;
    static void OutputCharWeightAssoc(const SvxWeightItem& rWeight, OStringBuffer& buf, bool assoc);
    static void OutputCharAutoKern(const SvxAutoKernItem& rAutoKern, OStringBuffer& buf);
    static void OutputCharAnimatedText(const SvxBlinkItem& rBlink, OStringBuffer& buf);
    void OutputCharBackground(const SvxBrushItem& rBrush, OStringBuffer& buf) const;
    static void OutputCharRotate(const SvxCharRotateItem& rRotate, OStringBuffer& buf);
    static void OutputCharEmphasisMark(const SvxEmphasisMarkItem& rEmphasisMark,
                                       OStringBuffer& buf);
    static void OutputCharTwoLines(const SvxTwoLinesItem& rTwoLines, OStringBuffer& buf);
    static void OutputCharScaleWidth(const SvxCharScaleWidthItem& rScaleWidth, OStringBuffer& buf);
    static void OutputCharRelief(const SvxCharReliefItem& rRelief, OStringBuffer& buf);
    static void OutputCharHidden(const SvxCharHiddenItem& rHidden, OStringBuffer& buf);
    void OutputCharBorder(const SvxBoxItem& rBox, OStringBuffer& buf) const;
    static void OutputCharHighlight(const SvxBrushItem& rBrush, OStringBuffer& buf);
    void OutputTextCharFormat(const SwFormatCharFormat& rCharFormat, OStringBuffer& buf) const;

    void OutputFormattingItem(const SfxPoolItem& item, OStringBuffer& buf) const;
    void OutputCharAssocFormattingItem(const SfxPoolItem& item, OStringBuffer& buf,
                                       bool assoc) const;

    void ApplyCharFormatItems(const SfxItemSet& items);
    void ApplyCharFormatProperties(const SfxPoolItem& charFormatItem);

    /*
     * Current style name and its ID.
     */
    OUString m_rStyleName;
    sal_uInt16 m_nStyleId;
    /*
     * Current list ID.
     */
    sal_uInt16 m_nListId;
    /*
     * This is needed because the call order is: run text, run properties, paragraph properties.
     * What we need is the opposite.
     */
    RtfStringBuffer m_aRun;
    RtfStringBuffer m_aRunText;
    /*
     * This is written after runs.
     */
    OStringBuffer m_aAfterRuns;
    /*
     * Same for colors and stylesheets: first we just want to output colors,
     * need to buffer the stylesheet table to output it after the color one.
     */
    OStringBuffer m_aStylesheet;
    /*
     * This one just holds the style commands in the current style.
     */
    OStringBuffer m_aParaFormatting;

    // for properties that shouldn't be output themselves, but need to be found when searching
    // items in m_aCharFormatting including parent
    SfxAllItemSet m_aCharFormattingParent;

    SfxAllItemSet m_aCharFormatting;

    bool m_bIsRTL;
    sal_uInt16 m_nScript;
    bool m_bControlLtrRtl;

    sal_Int32 m_nNextAnnotationMarkId;
    sal_Int32 m_nCurrentAnnotationMarkId;
    /// Maps annotation mark names to ID's.
    std::map<SwMarkName, sal_Int32> m_rOpenedAnnotationMarksIds;

    /*
     * The current table helper.
     */
    std::unique_ptr<SwWriteTable> m_pTableWrt;

    /*
     * Remember if we are in an open cell, or not.
     */
    bool m_bTableCellOpen;

    /*
     * Remember the current table depth.
     */
    sal_uInt32 m_nTableDepth;

    /*
     * Remember if we wrote a \cell or not.
     */
    bool m_bTableAfterCell;

    /*
     * For late output of row definitions.
     */
    OStringBuffer m_aRowDefs;

    /*
     * Is a column break needed after the next \par?
     */
    bool m_nColBreakNeeded;

    /*
     * If section breaks should be buffered to m_aSectionBreaks
     */
    bool m_bBufferSectionBreaks;
    OStringBuffer m_aSectionBreaks;

    /*
     * If section headers (and footers) should be buffered to
     * m_aSectionHeaders.
     */
    bool m_bBufferSectionHeaders;
    OStringBuffer m_aSectionHeaders;

    /*
     * Support for starting multiple tables at the same cell.
     * If the current table is the last started one.
     */
    bool m_bLastTable;
    /*
     * List of already started but not yet defined tables (need to be defined
     * after the nested tables).
     */
    std::vector<OString> m_aTables;
    /*
     * If cell info is already output.
     */
    bool m_bWroteCellInfo;

    /// If we ended a table row without starting a new one.
    bool m_bTableRowEnded;

    /// Number of cells from the table definition, by depth.
    std::map<sal_uInt32, sal_uInt32> m_aCells;

    bool m_bIsBeforeFirstParagraph;

    /// If we're in a paragraph that has a single empty run only.
    bool m_bSingleEmptyRun;

    bool m_bInRun;

    bool m_bInRuby;

    /// Maps ID's to postit fields, used in atrfstart/end and atnref.
    std::map<sal_uInt16, const SwPostItField*> m_aPostitFields;

    /// When exporting fly frames, this holds the real size of the frame.
    const Size* m_pFlyFrameSize;

    std::vector<std::pair<OString, OString>> m_aFlyProperties;

    std::optional<css::drawing::FillStyle> m_oFillStyle;

    /// If we're in the process of exporting a hyperlink, then its URL.
    std::stack<OUString> m_aURLs;

    /// If original file had \sbauto.
    bool m_bParaBeforeAutoSpacing;
    /// If m_bParaBeforeAutoSpacing is set, value of \sb.
    sal_Int32 m_nParaBeforeSpacing;
    /// If original file had \saauto.
    bool m_bParaAfterAutoSpacing;
    /// If m_bParaBeforeAutoSpacing is set, value of \sa.
    sal_Int32 m_nParaAfterSpacing;

    editeng::WordPageMargins m_aPageMargins;

    OString m_aParagraphMarkerProperties;

public:
    explicit RtfAttributeOutput(RtfExport& rExport);

    ~RtfAttributeOutput() override;

    /// Return the right export class.
    MSWordExportBase& GetExport() override;

    // These are used by wwFont::WriteRtf()
    /// Start the font.
    void StartFont(std::u16string_view rFamilyName) const;

    /// End the font.
    void EndFont() const;

    /// Alternate name for the font.
    void FontAlternateName(std::u16string_view rName) const;

    /// Font charset.
    void FontCharset(sal_uInt8 nCharSet) const;

    /// Font family.
    void FontFamilyType(FontFamily eFamily, const wwFont& rFont) const;

    /// Font pitch.
    void FontPitchType(FontPitch ePitch) const;

    void BulletDefinition(int nId, const Graphic& rGraphic, Size aSize) override;

    /// Handles just the {\shptxt ...} part of a shape export.
    void writeTextFrame(const ww8::Frame& rFrame, bool bTextBox = false);

    OStringBuffer& GetTabStop() { return m_aTabStop; }

    const SwPageDesc* GetPrevPageDesc() const { return m_pPrevPageDesc; }
};

#endif // INCLUDED_SW_SOURCE_FILTER_WW8_RTFATTRIBUTEOUTPUT_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
