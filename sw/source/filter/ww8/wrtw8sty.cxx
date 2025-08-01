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

#include <algorithm>

#include <memory>

#include <com/sun/star/i18n/ScriptType.hpp>
#include <hintids.hxx>
#include <editeng/boxitem.hxx>
#include <editeng/fontitem.hxx>
#include <svx/svdobj.hxx>
#include <svx/svdotext.hxx>
#include <svx/svdouno.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/fhgtitem.hxx>
#include <rtl/character.hxx>
#include <unotools/securityoptions.hxx>

#include <doc.hxx>
#include "wrtww8.hxx"
#include <docary.hxx>
#include <poolfmt.hxx>
#include <fmtpdsc.hxx>
#include <pagedesc.hxx>
#include <ndtxt.hxx>
#include <ftninfo.hxx>
#include <fmthdft.hxx>
#include <section.hxx>
#include <fmtcntnt.hxx>
#include <fmtftn.hxx>
#include <ndindex.hxx>
#include <txtftn.hxx>
#include <charfmt.hxx>
#include <docufld.hxx>
#include <dcontact.hxx>
#include <fmtcnct.hxx>
#include <ftnidx.hxx>
#include <fmtclds.hxx>
#include <lineinfo.hxx>
#include <fmtline.hxx>
#include <swtable.hxx>
#include <redline.hxx>
#include <msfilter.hxx>
#include <swmodule.hxx>
#include <charatr.hxx>

#include "sprmids.hxx"

#include "writerhelper.hxx"
#include "writerwordglue.hxx"
#include <wwstyles.hxx>
#include "ww8par.hxx"
#include "ww8attributeoutput.hxx"
#include "docxattributeoutput.hxx"
#include "rtfattributeoutput.hxx"

#include <unordered_set>

using namespace css;
using namespace sw::util;
using namespace nsHdFtFlags;

/// For the output of sections.
struct WW8_PdAttrDesc
{
    std::unique_ptr<sal_uInt8[]> m_pData;
    sal_uInt16 m_nLen;
    WW8_FC m_nSepxFcPos;
    WW8_PdAttrDesc() : m_nLen(0), m_nSepxFcPos(0xffffffff) /*default: none*/
        { }
};

namespace {

struct WW8_SED
{
    SVBT16 aBits1;      // orientation change + internal, Default: 6
    SVBT32 fcSepx;      //  FC  file offset to beginning of SEPX for section.
                        //  0xFFFFFFFF for no Sprms
    SVBT16 fnMpr;       //  used internally by Windows Word, Default: 0
    SVBT32 fcMpr;       //  FC, points to offset in FC space for MacWord
                        // Default: 0xffffffff ( nothing )
                        //  cbSED is 12 (decimal)), C (hex).
};

}

// class WW8_WrPlc0 is only used for header and footer positioning
// ie there is no content support structure
class WW8_WrPlc0
{
private:
    std::vector<sal_uLong> m_aPos;      // PTRARR of CPs / FCs
    sal_uLong m_nOfs;

    WW8_WrPlc0(WW8_WrPlc0 const&) = delete;
    WW8_WrPlc0& operator=(WW8_WrPlc0 const&) = delete;

public:
    explicit WW8_WrPlc0( sal_uLong nOffset );
    sal_uInt16 Count() const                { return m_aPos.size(); }
    void Append( sal_uLong nStartCpOrFc );
    void Write( SvStream& rStrm );
};

//  Styles

// According to [MS-DOC] v20221115 2.9.271 STSH,
// "The beginning of the rglpstd array is reserved for specific "fixed-index" application-defined
//  styles. A particular fixed-index, application-defined style has the same istd value in every
//  stylesheet. The rglpstd MUST contain an LPStd for each of these fixed-index styles and the order
//  MUST match the order in the following table.
//
//  istd   sti of application-defined style (see sti in StdfBase)
//  0      0
//  1      1
//  2      2
//  3      3
//  4      4
//  5      5
//  6      6
//  7      7
//  8      8
//  9      9
//  10     65
//  11     105
//  12     107
//  13     Reserved for future use
//  14     Reserved for future use"
//
// And [MS-OE376] v20220816 2.1.236 Part 4 Section 2.7.3.9, name (Primary Style Name)
// specifies the following mapping:
//
//  sti    Style name                Style type
//  0      Normal                    paragraph
//  1      heading 1                 paragraph
//  2      heading 2                 paragraph
//  3      heading 3                 paragraph
//  4      heading 4                 paragraph
//  5      heading 5                 paragraph
//  6      heading 6                 paragraph
//  7      heading 7                 paragraph
//  8      heading 8                 paragraph
//  9      heading 9                 paragraph
//  65     Default Paragraph Font    character
//  105    Normal Table              table
//  107    No List                   numbering

#define WW8_RESERVED_SLOTS 15

// GetId( SwCharFormat ) for use in text -> zero is not allowed,
// use "Default Char Style" instead
sal_uInt16 MSWordExportBase::GetId( const SwCharFormat* pFormat ) const
{
    sal_uInt16 nRet = m_pStyles->GetSlot( pFormat );
    return ( nRet != 0x0fff ) ? nRet : 10;      // Default Char Style
}

// GetId( SwTextFormatColl ) for use in TextNodes -> zero is not allowed,
// "Standard" instead
sal_uInt16 MSWordExportBase::GetId( const SwTextFormatColl& rColl ) const
{
    sal_uInt16 nRet = m_pStyles->GetSlot( &rColl );
    return ( nRet != 0xfff ) ? nRet : 0;        // Default TextFormatColl
}

//typedef pFormatT
MSWordStyles::MSWordStyles( MSWordExportBase& rExport, bool bListStyles )
    : m_rExport( rExport ),
    m_bListStyles(bListStyles)
{
    // if exist any Foot-/End-Notes then get from the EndNoteInfo struct
    // the CharFormats. They will create it!
    if ( !m_rExport.m_rDoc.GetFootnoteIdxs().empty() )
    {
        m_rExport.m_rDoc.GetEndNoteInfo().GetAnchorCharFormat( m_rExport.m_rDoc );
        m_rExport.m_rDoc.GetEndNoteInfo().GetCharFormat( m_rExport.m_rDoc );
        m_rExport.m_rDoc.GetFootnoteInfo().GetAnchorCharFormat( m_rExport.m_rDoc );
        m_rExport.m_rDoc.GetFootnoteInfo().GetCharFormat( m_rExport.m_rDoc );
    }

    memset( m_aHeadingParagraphStyles, -1 , MAXLEVEL * sizeof( sal_uInt16));

    BuildStylesTable();
    BuildWwNames();
    BuildStyleIds();
}

MSWordStyles::~MSWordStyles()
{
}

// Sty_SetWWSlot() dependencies for the styles -> zero is allowed
sal_uInt16 MSWordStyles::GetSlot( const SwFormat* pFormat ) const
{
    for (size_t slot = 0; slot < m_aStyles.size(); ++slot)
        if (m_aStyles[slot].format == pFormat)
            return slot;
    return 0xfff;                   // 0xfff: WW: zero
}

/// Get reserved slot number during building the style table.
static sal_uInt16 BuildGetSlot(const SwFormat& rFormat)
{
    switch (sal_uInt16 nId = rFormat.GetPoolFormatId())
    {
        case RES_POOLCOLL_STANDARD:
            return 0;

        case RES_POOLCOLL_HEADLINE1:
        case RES_POOLCOLL_HEADLINE2:
        case RES_POOLCOLL_HEADLINE3:
        case RES_POOLCOLL_HEADLINE4:
        case RES_POOLCOLL_HEADLINE5:
        case RES_POOLCOLL_HEADLINE6:
        case RES_POOLCOLL_HEADLINE7:
        case RES_POOLCOLL_HEADLINE8:
        case RES_POOLCOLL_HEADLINE9:
        {
            sal_uInt16 nRet = nId - (RES_POOLCOLL_HEADLINE1 - 1);
            assert(nRet < WW8_RESERVED_SLOTS);
            return nRet;
        }
    }
    return 0xfff;
}


// Keep in sync with StyleSheetTable::ConvertStyleName
sal_uInt16 MSWordStyles::GetWWId( const SwFormat& rFormat )
{
    sal_uInt16 nRet = ww::stiUser;    // user style as default
    sal_uInt16 nPoolId = rFormat.GetPoolFormatId();
    if( nPoolId == RES_POOLCOLL_STANDARD )
        nRet = ww::stiNormal;
    else if( nPoolId >= RES_POOLCOLL_HEADLINE1 &&
             nPoolId <= RES_POOLCOLL_HEADLINE9 )
        nRet = static_cast< sal_uInt16 >(nPoolId + ww::stiLevFirst - RES_POOLCOLL_HEADLINE1);
    else if( nPoolId >= RES_POOLCOLL_TOX_IDX1 &&
             nPoolId <= RES_POOLCOLL_TOX_IDX3 )
        nRet = static_cast< sal_uInt16 >(nPoolId + ww::stiIndexFirst - RES_POOLCOLL_TOX_IDX1);
    else if( nPoolId >= RES_POOLCOLL_TOX_CNTNT1 &&
             nPoolId <= RES_POOLCOLL_TOX_CNTNT5 )
        nRet = static_cast< sal_uInt16 >(nPoolId + ww::stiToc1 - RES_POOLCOLL_TOX_CNTNT1);
    else if( nPoolId >= RES_POOLCOLL_TOX_CNTNT6 &&
             nPoolId <= RES_POOLCOLL_TOX_CNTNT9 )
        nRet = static_cast< sal_uInt16 >(nPoolId + ww::stiToc6 - RES_POOLCOLL_TOX_CNTNT6);
    else
        switch( nPoolId )
        {
        case RES_POOLCOLL_FOOTNOTE:         nRet = ww::stiFootnoteText;      break;
        case RES_POOLCOLL_MARGINAL:         nRet = ww::stiAtnText;           break;
        case RES_POOLCOLL_HEADER:           nRet = ww::stiHeader;            break;
        case RES_POOLCOLL_FOOTER:           nRet = ww::stiFooter;            break;
        case RES_POOLCOLL_TOX_IDXH:         nRet = ww::stiIndexHeading;      break;
        case RES_POOLCOLL_LABEL:            nRet = ww::stiCaption;           break;
        case RES_POOLCOLL_TOX_ILLUS1:       nRet = ww::stiToCaption;         break;
        case RES_POOLCOLL_ENVELOPE_ADDRESS: nRet = ww::stiEnvAddr;           break;
        case RES_POOLCOLL_SEND_ADDRESS:     nRet = ww::stiEnvRet;            break;
        case RES_POOLCHR_FOOTNOTE_ANCHOR:   nRet = ww::stiFootnoteRef;       break;
        case RES_POOLCHR_LINENUM:           nRet = ww::stiLnn;               break;
        case RES_POOLCHR_PAGENO:            nRet = ww::stiPgn;               break;
        case RES_POOLCHR_ENDNOTE_ANCHOR:    nRet = ww::stiEdnRef;            break;
        case RES_POOLCOLL_ENDNOTE:          nRet = ww::stiEdnText;           break;
        case RES_POOLCOLL_TOX_AUTHORITIESH: nRet = ww::stiToa;               break;
        case RES_POOLCOLL_TOX_CNTNTH:       nRet = ww::stiToaHeading;        break;
        case RES_POOLCOLL_LISTS_BEGIN:      nRet = ww::stiList;              break;
        case RES_POOLCOLL_BULLET_LEVEL1:    nRet = ww::stiListBullet;        break;
        case RES_POOLCOLL_NUM_LEVEL1:       nRet = ww::stiListNumber;        break;
        case RES_POOLCOLL_BULLET_LEVEL2:    nRet = ww::stiListBullet2;       break;
        case RES_POOLCOLL_BULLET_LEVEL3:    nRet = ww::stiListBullet3;       break;
        case RES_POOLCOLL_BULLET_LEVEL4:    nRet = ww::stiListBullet4;       break;
        case RES_POOLCOLL_BULLET_LEVEL5:    nRet = ww::stiListBullet5;       break;
        case RES_POOLCOLL_NUM_LEVEL2:       nRet = ww::stiListNumber2;       break;
        case RES_POOLCOLL_NUM_LEVEL3:       nRet = ww::stiListNumber3;       break;
        case RES_POOLCOLL_NUM_LEVEL4:       nRet = ww::stiListNumber4;       break;
        case RES_POOLCOLL_NUM_LEVEL5:       nRet = ww::stiListNumber5;       break;
        case RES_POOLCOLL_DOC_TITLE:        nRet = ww::stiTitle;             break;
        case RES_POOLCOLL_DOC_APPENDIX:     nRet = ww::stiClosing;           break;
        case RES_POOLCOLL_SIGNATURE:        nRet = ww::stiSignature;         break;
        case RES_POOLCOLL_TEXT:             nRet = ww::stiBodyText;          break;
        case RES_POOLCOLL_TEXT_MOVE:        nRet = ww::stiBodyTextInd1;      break;
        case RES_POOLCOLL_BULLET_NONUM1:    nRet = ww::stiListCont;          break;
        case RES_POOLCOLL_BULLET_NONUM2:    nRet = ww::stiListCont2;         break;
        case RES_POOLCOLL_BULLET_NONUM3:    nRet = ww::stiListCont3;         break;
        case RES_POOLCOLL_BULLET_NONUM4:    nRet = ww::stiListCont4;         break;
        case RES_POOLCOLL_BULLET_NONUM5:    nRet = ww::stiListCont5;         break;
        case RES_POOLCOLL_DOC_SUBTITLE:     nRet = ww::stiSubtitle;          break;
        case RES_POOLCOLL_GREETING:         nRet = ww::stiSalutation;        break;
        case RES_POOLCOLL_TEXT_IDENT:       nRet = ww::stiBodyText1I;        break;
        case RES_POOLCHR_INET_NORMAL:       nRet = ww::stiHyperlink;         break;
        case RES_POOLCHR_INET_VISIT:        nRet = ww::stiHyperlinkFollowed; break;
        case RES_POOLCHR_HTML_STRONG:       nRet = ww::stiStrong;            break;
        case RES_POOLCHR_HTML_EMPHASIS:     nRet = ww::stiEmphasis;          break;
        }
    return nRet;
}

void MSWordStyles::BuildStylesTable()
{
    assert(m_aStyles.empty());
    // Put reserved slots first, then character styles, then paragraph styles
    m_aStyles.resize(WW8_RESERVED_SLOTS);

    const SwCharFormats& rArr = *m_rExport.m_rDoc.GetCharFormats();       // first CharFormat
    // the default character style ( 0 ) will not be outputted !
    for (size_t n = 1; n < rArr.size() && m_aStyles.size() < MSWORD_MAX_STYLES_LIMIT; ++n)
    {
        if (m_rExport.GetExportFormat() == MSWordExportBase::DOCX
            && rArr[n]->GetName().toString().startsWith("ListLabel"))
        {
            // tdf#92335 don't export redundant DOCX import style "ListLabel"
            continue;
        }

        m_aStyles.emplace_back(rArr[n]);
    }

    if (m_aStyles.size() == MSWORD_MAX_STYLES_LIMIT)
    {
        SAL_WARN("sw.ww8", "MSWordStyles::BuildStylesTable: too many styles, have "
                               << rArr.size() << " char styles");
    }

    const SwTextFormatColls& rArr2 = *m_rExport.m_rDoc.GetTextFormatColls();   // then TextFormatColls
    // the default paragraph style ( 0 ) will not be outputted !
    for (size_t n = 1; n < rArr2.size(); ++n)
    {
        SwTextFormatColl* pFormat = rArr2[n];

        sal_uInt16 nSlot = BuildGetSlot(*pFormat);
        if (nSlot != 0xfff)
        {
            m_aStyles[nSlot] = { pFormat };
        }
        else
        {
            if (m_aStyles.size() >= MSWORD_MAX_STYLES_LIMIT)
                continue;
            m_aStyles.emplace_back(pFormat);
            nSlot = m_aStyles.size() - 1;
        }
        if ( pFormat->IsAssignedToListLevelOfOutlineStyle() )
        {
            int nLvl = pFormat->GetAssignedOutlineStyleLevel() ;
            if (nLvl >= 0 && nLvl < MAXLEVEL)
                m_aHeadingParagraphStyles[nLvl] = nSlot;
        }
    }

    if (!m_bListStyles)
        return;

    const SwNumRuleTable& rNumRuleTable = m_rExport.m_rDoc.GetNumRuleTable();
    for (size_t i = 0; i < rNumRuleTable.size() && m_aStyles.size() < MSWORD_MAX_STYLES_LIMIT; ++i)
    {
        const SwNumRule* pNumRule = rNumRuleTable[i];
        if (pNumRule->IsAutoRule() || pNumRule->GetName().toString().startsWith("WWNum"))
            continue;
        m_aStyles.emplace_back(pNumRule);
    }
}

// StyleSheetTable::ConvertStyleName appends the suffix do disambiguate conflicting style names
static OUString StripWWSuffix(const OUString& s)
{
    OUString ret = s;
    (void)ret.endsWith(" (WW)", &ret);
    return ret;
}

void MSWordStyles::BuildWwNames()
{
    std::unordered_set<OUString> aUsed;

    auto makeUniqueName = [&aUsed](OUString& name) {
        // toAsciiLowerCase rules out e.g. user's "normal"; no problem if there are non-ASCII chars
        OUString lower(name.toAsciiLowerCase());
        if (!aUsed.insert(lower).second)
        {
            int nFree = 1;
            while (!aUsed.insert(lower + OUString::number(nFree)).second)
                ++nFree;

            name += OUString::number(nFree);
        }
    };

    // We want to map LO's default style to Word's "Normal" style.
    // Word looks for this specific style name when reading docx files.
    // (It must be the English word regardless of languages and locales)
    assert(!m_aStyles.empty());
    assert(!m_aStyles[0].format || m_aStyles[0].ww_id == ww::stiNormal);
    m_aStyles[0].ww_name = "Normal";
    aUsed.insert(u"normal"_ustr);

    // 1. Handle styles having special wwIds, and thus pre-defined names
    for (auto& entry : m_aStyles)
    {
        if (!entry.ww_name.isEmpty())
            continue; // "Normal" is already added
        if (entry.ww_id >= ww::stiMax)
            continue; // Not a format with special name
        assert(entry.format);

        entry.ww_name = OUString::createFromAscii(ww::GetEnglishNameFromSti(static_cast<ww::sti>(entry.ww_id)));
        makeUniqueName(entry.ww_name);
    }

    // 2. Now handle other styles
    for (auto& entry : m_aStyles)
    {
        if (!entry.ww_name.isEmpty())
            continue;
        if (entry.format)
            entry.ww_name = StripWWSuffix(entry.format->GetName().toString());
        else if (entry.num_rule)
            entry.ww_name = StripWWSuffix(entry.num_rule->GetName().toString());
        else
            continue;
        makeUniqueName(entry.ww_name);
    }
}

OString MSWordStyles::CreateStyleId(std::u16string_view aName)
{
    return OUStringToOString(msfilter::util::CreateDOCXStyleId(aName), RTL_TEXTENCODING_UTF8);
}

void MSWordStyles::BuildStyleIds()
{
    std::unordered_set<OString> aUsed;

    for (auto& entry : m_aStyles)
    {
        OString aStyleId = CreateStyleId(entry.ww_name);

        if (aStyleId.isEmpty())
            aStyleId = "Style"_ostr;

        OString aLower(aStyleId.toAsciiLowerCase());

        // check for uniqueness & construct something unique if we have to
        if (!aUsed.insert(aLower).second)
        {
            int nFree = 1;
            while (!aUsed.insert(aLower + OString::number(nFree)).second)
                ++nFree;

            aStyleId += OString::number(nFree);
        }
        entry.style_id = aStyleId;
    }
}

OString const & MSWordStyles::GetStyleId(sal_uInt16 nSlot) const
{
    assert(!m_aStyles[nSlot].style_id.isEmpty());
    return m_aStyles[nSlot].style_id;
}

const OUString & MSWordStyles::GetStyleWWName(SwFormat const*const pFormat) const
{
    if (auto slot = m_rExport.m_pStyles->GetSlot(pFormat); slot != 0xfff)
    {
        assert(!m_aStyles[slot].ww_name.isEmpty());
        return m_aStyles[slot].ww_name;
    }
    return EMPTY_OUSTRING;
}

/// For WW8 only - extend pO so that the size of pTableStrm is even.
static void impl_SkipOdd(std::unique_ptr<ww::bytes> const& pO, std::size_t nTableStrmTell)
{
    if ( ( nTableStrmTell + pO->size() ) & 1 )     // start on even
        pO->push_back( sal_uInt8(0) );         // Address
}

void WW8AttributeOutput::EndStyle()
{
    impl_SkipOdd( m_rWW8Export.m_pO, m_rWW8Export.m_pTableStrm->Tell() );

    short nLen = m_rWW8Export.m_pO->size() - 2;            // length of the style
    sal_uInt8* p = m_rWW8Export.m_pO->data() + m_nPOPosStdLen1;
    ShortToSVBT16( nLen, p );               // add
    p = m_rWW8Export.m_pO->data() + m_nPOPosStdLen2;
    ShortToSVBT16( nLen, p );               // also

    m_rWW8Export.m_pTableStrm->WriteBytes(m_rWW8Export.m_pO->data(), m_rWW8Export.m_pO->size());
    m_rWW8Export.m_pO->clear();
}

void WW8AttributeOutput::StartStyle( const OUString& rName, StyleType eType, sal_uInt16 nWwBase,
    sal_uInt16 nWwNext, sal_uInt16 /*nWwLink*/, sal_uInt16 nWwId, sal_uInt16 /*nSlot*/, bool bAutoUpdate )
{
    sal_uInt8 aWW8_STD[ sizeof( WW8_STD ) ] = {};
    sal_uInt8* pData = aWW8_STD;

    sal_uInt16 nBit16 = 0x1000;         // fInvalHeight
    nBit16 |= (ww::stiNil & nWwId);
    Set_UInt16( pData, nBit16 );

    nBit16 = nWwBase << 4;          // istdBase
    nBit16 |= (eType == STYLE_TYPE_PARA ? 1 : 2);      // sgc
    Set_UInt16( pData, nBit16 );

    nBit16 = nWwNext << 4;          // istdNext
    nBit16 |= (eType == STYLE_TYPE_PARA ? 2 : 1);      // cupx
    Set_UInt16( pData, nBit16 );

    pData += sizeof( sal_uInt16 );      // bchUpe

    nBit16 = bAutoUpdate ? 1 : 0;  // fAutoRedef : 1
    Set_UInt16( pData, nBit16 );
    // now new:
    // from Ver8 there are two fields more:
    // sal_uInt16    fHidden : 1;       /* hidden from UI?
    // sal_uInt16    : 14;              /* unused bits

    sal_uInt16 nLen = static_cast< sal_uInt16 >( ( pData - aWW8_STD ) + 1 +
                (2 * (rName.getLength() + 1)) );  // temporary

    m_nPOPosStdLen1 = m_rWW8Export.m_pO->size();        // Adr1 for adding the length

    SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, nLen );
    m_rWW8Export.m_pO->insert( m_rWW8Export.m_pO->end(), aWW8_STD, pData );

    m_nPOPosStdLen2 = m_nPOPosStdLen1 + 8;  // Adr2 for adding of "end of upx"

    // write names
    SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, rName.getLength() ); // length
    SwWW8Writer::InsAsString16( *m_rWW8Export.m_pO, rName );
    m_rWW8Export.m_pO->push_back( sal_uInt8(0) );             // Despite P-String 0 at the end!
}

void MSWordStyles::SetStyleDefaults( const SwFormat& rFormat, bool bPap )
{
    const sw::BroadcastingModify* pOldMod = m_rExport.m_pOutFormatNode;
    m_rExport.m_pOutFormatNode = &rFormat;
    bool aFlags[ RES_FRMATR_END - RES_CHRATR_BEGIN ];
    sal_uInt16 nStt, nEnd, n;
    if( bPap )
    {
       nStt = RES_PARATR_BEGIN;
       nEnd = RES_FRMATR_END;
    }
    else
    {
       nStt = RES_CHRATR_BEGIN;
       nEnd = RES_TXTATR_END;
    }

    // dynamic defaults
    const SfxItemPool& rPool = *rFormat.GetAttrSet().GetPool();
    for( n = nStt; n < nEnd; ++n )
        aFlags[ n - RES_CHRATR_BEGIN ] = nullptr != rPool.GetUserDefaultItem( n )
            || SfxItemState::SET == m_rExport.m_rDoc.GetDfltTextFormatColl()->GetItemState( n, false );

    // static defaults, that differs between WinWord and SO
    if( bPap )
    {
        aFlags[ static_cast< sal_uInt16 >(RES_PARATR_WIDOWS) - RES_CHRATR_BEGIN ] = true;
        aFlags[ static_cast< sal_uInt16 >(RES_PARATR_HYPHENZONE) - RES_CHRATR_BEGIN ] = true;
        aFlags[ static_cast< sal_uInt16 >(RES_FRAMEDIR) - RES_CHRATR_BEGIN ] = true;
    }
    else
    {
        aFlags[ RES_CHRATR_FONTSIZE - RES_CHRATR_BEGIN ] = true;
        aFlags[ RES_CHRATR_LANGUAGE - RES_CHRATR_BEGIN ] = true;
    }

    const SfxItemSet* pOldI = m_rExport.GetCurItemSet();
    m_rExport.SetCurItemSet( &rFormat.GetAttrSet() );

    const bool* pFlags = aFlags + ( nStt - RES_CHRATR_BEGIN );
    for ( n = nStt; n < nEnd; ++n, ++pFlags )
    {
        if ( *pFlags && !m_rExport.ignoreAttributeForStyleDefaults( n )
            && SfxItemState::SET != rFormat.GetItemState(n, false))
        {
            //If we are a character property then see if it is one of the
            //western/asian ones that must be collapsed together for export to
            //word. If so default to the western variant.
            if ( bPap || m_rExport.CollapseScriptsforWordOk(
                i18n::ScriptType::LATIN, n) )
            {
                m_rExport.AttrOutput().OutputItem( rFormat.GetFormatAttr( n ) );
            }
        }
    }

    m_rExport.SetCurItemSet( pOldI );
    m_rExport.m_pOutFormatNode = pOldMod;
}

void WW8AttributeOutput::StartStyleProperties( bool bParProp, sal_uInt16 nStyle )
{
    impl_SkipOdd( m_rWW8Export.m_pO, m_rWW8Export.m_pTableStrm->Tell() );

    sal_uInt16 nLen = bParProp ? 2 : 0;         // default length
    m_nStyleLenPos = m_rWW8Export.m_pO->size();   // adding length
                                                // Don't save pointer, because it
                                                // changes by _grow!

    SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, nLen );        // Style-Len

    m_nStyleStartSize = m_rWW8Export.m_pO->size();

    if ( bParProp )
        SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, nStyle );     // Style-Number
}

void MSWordStyles::WriteProperties( const SwFormat* pFormat, bool bParProp, sal_uInt16 nPos,
    bool bInsDefCharSiz )
{
    m_rExport.AttrOutput().StartStyleProperties( bParProp, nPos );

    OSL_ENSURE( m_rExport.m_pCurrentStyle == nullptr, "Current style not NULL" ); // set current style before calling out
    m_rExport.m_pCurrentStyle = pFormat;

    m_rExport.OutputFormat( *pFormat, bParProp, !bParProp );

    OSL_ENSURE( m_rExport.m_pCurrentStyle == pFormat, "current style was changed" );
    // reset current style...
    m_rExport.m_pCurrentStyle = nullptr;

    if ( bInsDefCharSiz  )                   // not derived from other Style
        SetStyleDefaults( *pFormat, bParProp );

    m_rExport.AttrOutput().EndStyleProperties( bParProp );
}

void WW8AttributeOutput::EndStyleProperties( bool /*bParProp*/ )
{
    sal_uInt16 nLen = m_rWW8Export.m_pO->size() - m_nStyleStartSize;
    sal_uInt8* pUpxLen = m_rWW8Export.m_pO->data() + m_nStyleLenPos; // adding length
    ShortToSVBT16( nLen, pUpxLen );                 // add default length
}

void MSWordStyles::GetStyleData( const SwFormat* pFormat, bool& bFormatColl, sal_uInt16& nBase, sal_uInt16& nNext, sal_uInt16& nLink )
{
    bFormatColl = pFormat->Which() == RES_TXTFMTCOLL || pFormat->Which() == RES_CONDTXTFMTCOLL;

    // Default: none
    nBase = 0xfff;

    // Derived from?
    if ( !pFormat->IsDefault() )
        nBase = GetSlot( pFormat->DerivedFrom() );

    const SwFormat* pNext;
    const SwFormat* pLink = nullptr;
    if ( bFormatColl )
    {
        auto pFormatColl = static_cast<const SwTextFormatColl*>(pFormat);
        pNext = &pFormatColl->GetNextTextFormatColl();
        pLink = pFormatColl->GetLinkedCharFormat();
    }
    else
    {
        pNext = pFormat; // CharFormat: next CharFormat == self
        auto pCharFormat = static_cast<const SwCharFormat*>(pFormat);
        pLink = pCharFormat->GetLinkedParaFormat();
    }

    nNext = GetSlot( pNext );

    if (pLink)
    {
        nLink = GetSlot(pLink);
    }
}

void WW8AttributeOutput::DefaultStyle()
{
    m_rWW8Export.m_pTableStrm->WriteUInt16(0);   // empty Style
}

void MSWordStyles::OutputStyle(sal_uInt16 nSlot)
{
    const auto& entry = m_aStyles[nSlot];

    if (entry.num_rule)
    {
        m_rExport.AttrOutput().StartStyle( entry.ww_name, STYLE_TYPE_LIST,
            /*nBase =*/ 0, /*nWwNext =*/ 0, /*nWwLink =*/ 0, /*nWWId =*/ 0, nSlot,
            /*bAutoUpdateFormat =*/ false );

        m_rExport.AttrOutput().EndStyle();
    }
    else if (!entry.format)
    {
        m_rExport.AttrOutput().DefaultStyle();
    }
    else
    {
        bool bFormatColl;
        sal_uInt16 nBase, nWwNext;
        sal_uInt16 nWwLink = 0x0FFF;

        GetStyleData(entry.format, bFormatColl, nBase, nWwNext, nWwLink);

        m_rExport.AttrOutput().StartStyle(entry.ww_name, (bFormatColl ? STYLE_TYPE_PARA : STYLE_TYPE_CHAR),
                nBase, nWwNext, nWwLink, m_aStyles[nSlot].ww_id, nSlot,
                entry.format->IsAutoUpdateOnDirectFormat() );

        if ( bFormatColl )
            WriteProperties( entry.format, true, nSlot, nBase==0xfff );           // UPX.papx

        WriteProperties( entry.format, false, nSlot, bFormatColl && nBase==0xfff );  // UPX.chpx

        m_rExport.AttrOutput().EndStyle();
    }
}

void WW8AttributeOutput::StartStyles()
{
    WW8Fib& rFib = *m_rWW8Export.m_pFib;

    sal_uInt64 nCurPos = m_rWW8Export.m_pTableStrm->Tell();
    if ( nCurPos & 1 )                   // start on even
    {
        m_rWW8Export.m_pTableStrm->WriteChar( char(0) );        // Address
        ++nCurPos;
    }
    rFib.m_fcStshfOrig = rFib.m_fcStshf = nCurPos;
    m_nStyleCountPos = nCurPos + 2;     // count is added later

    static const sal_uInt8 aStShi[] = {
        0x12, 0x00,
        0x0F, 0x00, 0x0A, 0x00, 0x01, 0x00, 0x5B, 0x00,
        0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00 };

    m_rWW8Export.m_pTableStrm->WriteBytes(&aStShi, sizeof(aStShi));
}

void WW8AttributeOutput::EndStyles( sal_uInt16 nNumberOfStyles )
{
    WW8Fib& rFib = *m_rWW8Export.m_pFib;

    rFib.m_lcbStshfOrig = rFib.m_lcbStshf = m_rWW8Export.m_pTableStrm->Tell() - rFib.m_fcStshf;
    SwWW8Writer::WriteShort( *m_rWW8Export.m_pTableStrm, m_nStyleCountPos, nNumberOfStyles );
}

void MSWordStyles::OutputStylesTable()
{
    m_rExport.m_bStyDef = true;

    m_rExport.AttrOutput().StartStyles();

    // HACK
    // Ms Office seems to have an internal limitation of 4091 styles
    // and refuses to load .docx with more, even though the spec seems to allow that;
    // so simply if there are more styles, don't export those
    // Implementing check for all exports DOCX, DOC, RTF
    assert(m_aStyles.size() <= MSWORD_MAX_STYLES_LIMIT);
    for (size_t slot = 0; slot < m_aStyles.size(); ++slot)
        OutputStyle(slot);

    m_rExport.AttrOutput().EndStyles(m_aStyles.size());

    m_rExport.m_bStyDef = false;
}

//          Fonts

wwFont::wwFont(std::u16string_view rFamilyName, FontPitch ePitch, FontFamily eFamily,
        rtl_TextEncoding eChrSet)
    : mbAlt(false), mePitch(ePitch), meFamily(eFamily), meChrSet(eChrSet)
{
    FontMapExport aResult(rFamilyName);
    msFamilyNm = aResult.msPrimary;
    msAltNm = aResult.msSecondary;
    if (!msAltNm.isEmpty() && msAltNm != msFamilyNm &&
        (msFamilyNm.getLength() + msAltNm.getLength() + 2 <= 65) )
    {
        //max size of szFfn in 65 chars
        mbAlt = true;
    }

    maWW8_FFN[0] = static_cast<sal_uInt8>( 6 - 1 + 0x22 + ( 2 * ( 1 + msFamilyNm.getLength() ) ));
    if (mbAlt)
        maWW8_FFN[0] = static_cast< sal_uInt8 >(maWW8_FFN[0] + 2 * ( 1 + msAltNm.getLength()));

    sal_uInt8 aB = 0;
    switch(ePitch)
    {
        case PITCH_VARIABLE:
            aB |= 2;    // aF.prg = 2
            break;
        case PITCH_FIXED:
            aB |= 1;
            break;
        default:        // aF.prg = 0 : DEFAULT_PITCH (windows.h)
            break;
    }
    aB |= 1 << 2;   // aF.fTrueType = 1; don't know any better;

    switch(eFamily)
    {
        case FAMILY_ROMAN:
            aB |= 1 << 4;   // aF.ff = 1;
            break;
        case FAMILY_SWISS:
            aB |= 2 << 4;   // aF.ff = 2;
            break;
        case FAMILY_MODERN:
            aB |= 3 << 4;   // aF.ff = 3;
            break;
        case FAMILY_SCRIPT:
            aB |= 4 << 4;   // aF.ff = 4;
            break;
        case FAMILY_DECORATIVE:
            aB |= 5 << 4;   // aF.ff = 5;
            break;
        default:            // aF.ff = 0; FF_DONTCARE (windows.h)
            break;
    }
    maWW8_FFN[1] = aB;

    ShortToSVBT16( 400, &maWW8_FFN[2] );        // don't know any better
                                                // 400 == FW_NORMAL (windows.h)

    //#i61927# For unicode fonts like Arial Unicode, Word 97+ sets the chs
    //to SHIFTJIS presumably to capture that it's a multi-byte encoding font
    //but Word95 doesn't do this, and sets it to 0 (ANSI), so we should do the
    //same
    maWW8_FFN[4] = sw::ms::rtl_TextEncodingToWinCharset(eChrSet);

    if (mbAlt)
        maWW8_FFN[5] = static_cast< sal_uInt8 >(msFamilyNm.getLength() + 1);
}

void wwFont::Write(SvStream *pTableStrm) const
{
    pTableStrm->WriteBytes(maWW8_FFN, sizeof(maWW8_FFN));    // fixed part
    // from Ver8 following two fields intersected,
    // we ignore them.
    //char  panose[ 10 ];       //  0x6   PANOSE
    //char  fs[ 24     ];       //  0x10  FONTSIGNATURE
    SwWW8Writer::FillCount(*pTableStrm, 0x22);
    SwWW8Writer::WriteString16(*pTableStrm, msFamilyNm, true);
    if (mbAlt)
        SwWW8Writer::WriteString16(*pTableStrm, msAltNm, true);
}

void wwFont::WriteDocx( DocxAttributeOutput* rAttrOutput ) const
{
    // no font embedding, panose id, subsetting, ... implemented

    if (msFamilyNm.isEmpty())
        return;

    rAttrOutput->StartFont( msFamilyNm );

    if ( mbAlt )
        rAttrOutput->FontAlternateName( msAltNm );
    rAttrOutput->FontCharset( sw::ms::rtl_TextEncodingToWinCharset( meChrSet ), meChrSet );
    rAttrOutput->FontFamilyType( meFamily );
    rAttrOutput->FontPitchType( mePitch );
    rAttrOutput->EmbedFont( msFamilyNm, meFamily, mePitch );

    rAttrOutput->EndFont();
}

void wwFont::WriteRtf( const RtfAttributeOutput* rAttrOutput ) const
{
    rAttrOutput->FontFamilyType( meFamily, *this );
    rAttrOutput->FontPitchType( mePitch );
    rAttrOutput->FontCharset(
        sw::ms::rtl_TextEncodingToWinCharsetRTF(msFamilyNm, msAltNm, meChrSet));
    rAttrOutput->StartFont( msFamilyNm );
    if ( mbAlt )
        rAttrOutput->FontAlternateName( msAltNm );
    rAttrOutput->EndFont();
}

bool operator<(const wwFont &r1, const wwFont &r2)
{
    int nRet = memcmp(r1.maWW8_FFN, r2.maWW8_FFN, sizeof(r1.maWW8_FFN));
    if (nRet == 0)
    {
        nRet = r1.msFamilyNm.compareTo(r2.msFamilyNm);
        if (nRet == 0)
            nRet = r1.msAltNm.compareTo(r2.msAltNm);
    }
    return nRet < 0;
}

sal_uInt16 wwFontHelper::GetId(const wwFont &rFont)
{
    sal_uInt16 nRet;
    std::map<wwFont, sal_uInt16>::const_iterator aIter = maFonts.find(rFont);
    if (aIter != maFonts.end())
        nRet = aIter->second;
    else
    {
        nRet = static_cast< sal_uInt16 >(maFonts.size());
        maFonts[rFont] = nRet;
    }
    return nRet;
}

void wwFontHelper::InitFontTable(MSWordExportBase& rExport)
{
    GetId(wwFont(u"Times New Roman", PITCH_VARIABLE,
        FAMILY_ROMAN, RTL_TEXTENCODING_MS_1252));

    GetId(wwFont(u"Symbol", PITCH_VARIABLE, FAMILY_ROMAN,
        RTL_TEXTENCODING_SYMBOL));

    GetId(wwFont(u"Arial", PITCH_VARIABLE, FAMILY_SWISS,
        RTL_TEXTENCODING_MS_1252));

    GetId(*GetDfltAttr(RES_CHRATR_FONT));

    if (const SvxFontItem* pFont = rExport.m_rDoc.GetAttrPool().GetUserDefaultItem(RES_CHRATR_FONT))
        GetId(*pFont);

    if (!m_bLoadAllFonts)
        return;

    const TypedWhichId<SvxFontItem> aTypes[] { RES_CHRATR_FONT, RES_CHRATR_CJK_FONT, RES_CHRATR_CTL_FONT };
    for (const TypedWhichId<SvxFontItem> & pId : aTypes)
    {
        rExport.m_rDoc.ForEachCharacterFontItem(pId, /*bIgnoreAutoStyles*/false,
            [this] (const SvxFontItem& rFontItem) -> bool
            {
                GetId(rFontItem);
                return true;
            });
    }

    // Bullets in lists may need own fonts; and may even want to substitute fonts (see
    // MSWordExportBase::SubstituteBullet). We need to collect these here, too.
    rExport.EnsureUsedNumberingTable();
    for (const SwNumRule* pRule : *rExport.m_pUsedNumTable)
    {
        assert(pRule);
        int n = pRule->IsContinusNum() ? WW8ListManager::nMinLevel : WW8ListManager::nMaxLevel;
        for (int nLvl = 0; nLvl < n; ++nLvl)
        {
            const SwNumFormat& rFormat = pRule->Get(nLvl);

            if (rFormat.GetNumberingType() == SVX_NUM_CHAR_SPECIAL
                || rFormat.GetNumberingType() == SVX_NUM_BITMAP)
            {
                const auto [s, pFont] = rExport.GetNumberingLevelBulletStringAndFont(rFormat);
                (void)s;
                assert(pFont);
                GetId(*pFont);
            }
        }
    }
}

sal_uInt16 wwFontHelper::GetId(const SvxFontItem& rFont)
{
    wwFont aFont(rFont.GetFamilyName(), rFont.GetPitch(), rFont.GetFamily(),
        rFont.GetCharSet());
    return GetId(aFont);
}

std::vector< const wwFont* > wwFontHelper::AsVector() const
{
    std::vector<const wwFont *> aFontList( maFonts.size() );

    for ( const auto& aFont : maFonts )
        aFontList[aFont.second] = &aFont.first;

    return aFontList;
}

void wwFontHelper::WriteFontTable(SvStream *pTableStream, WW8Fib& rFib)
{
    rFib.m_fcSttbfffn = pTableStream->Tell();
    /*
     * Reserve some space to fill in the len after we know how big it is
     */
    SwWW8Writer::WriteLong(*pTableStream, 0);

    /*
     * Convert from fast insertion map to linear vector in the order that we
     * want to write.
     */
    std::vector<const wwFont *> aFontList( AsVector() );

    /*
     * Write them all to pTableStream
     */
    for ( auto aFont : aFontList )
        aFont->Write(pTableStream);

    /*
     * Write the position and len in the FIB
     */
    rFib.m_lcbSttbfffn = pTableStream->Tell() - rFib.m_fcSttbfffn;
    SwWW8Writer::WriteLong( *pTableStream, rFib.m_fcSttbfffn, maFonts.size());
}

void wwFontHelper::WriteFontTable( DocxAttributeOutput& rAttrOutput )
{
    std::vector<const wwFont *> aFontList( AsVector() );

    for ( auto aFont : aFontList )
        aFont->WriteDocx(&rAttrOutput);
}

void wwFontHelper::WriteFontTable( const RtfAttributeOutput& rAttrOutput )
{
    std::vector<const wwFont *> aFontList( AsVector() );

    for ( auto aFont : aFontList )
        aFont->WriteRtf(&rAttrOutput);
}

WW8_WrPlc0::WW8_WrPlc0( sal_uLong nOffset )
    : m_nOfs( nOffset )
{
}

void WW8_WrPlc0::Append( sal_uLong nStartCpOrFc )
{
    m_aPos.push_back( nStartCpOrFc - m_nOfs );
}

void WW8_WrPlc0::Write( SvStream& rStrm )
{
    for( const auto& rPos : m_aPos )
    {
        rStrm.WriteUInt32(rPos);
    }
}

// class MSWordSections : translate PageDescs into Sections
//      also deals with header and footer

MSWordSections::MSWordSections( MSWordExportBase& rExport )
    : mbDocumentIsProtected( false )
{
    const SwSectionFormat *pFormat = nullptr;
    rExport.m_pCurrentPageDesc = &rExport.m_rDoc.GetPageDesc( 0 );

    const SwNode* pNd = rExport.m_pCurPam->GetPointContentNode();
    const SfxItemSet* pSet = pNd ? &static_cast<const SwContentNode*>(pNd)->GetSwAttrSet() : nullptr;

    sal_uLong nRstLnNum =  pSet ? pSet->Get( RES_LINENUMBER ).GetStartValue() : 0;

    const SwTableNode* pTableNd = rExport.m_pCurPam->GetPointNode().FindTableNode();
    const SwSectionNode* pSectNd = nullptr;
    if ( pTableNd )
    {
        pSet = &pTableNd->GetTable().GetFrameFormat()->GetAttrSet();
        pNd = pTableNd;
    }
    else if (pNd && nullptr != ( pSectNd = pNd->FindSectionNode() ))
    {
        if ( SectionType::ToxHeader == pSectNd->GetSection().GetType() &&
             pSectNd->StartOfSectionNode()->IsSectionNode() )
        {
            pSectNd = pSectNd->StartOfSectionNode()->GetSectionNode();
        }

        if ( SectionType::ToxContent == pSectNd->GetSection().GetType() )
        {
            pNd = pSectNd;
            rExport.m_pCurPam->GetPoint()->Assign(*pNd);
        }

        if ( SectionType::Content == pSectNd->GetSection().GetType() )
            pFormat = pSectNd->GetSection().GetFormat();
    }

    // tdf#118393: FILESAVE: DOCX Export loses header/footer
    rExport.m_bFirstTOCNodeWithSection = pSectNd &&
        (   SectionType::ToxHeader  == pSectNd->GetSection().GetType() ||
            SectionType::ToxContent == pSectNd->GetSection().GetType()  );

    // Try to get page descriptor of the first node
    const SwFormatPageDesc* pDescItem;
    if ( pSet &&
         (pDescItem = pSet->GetItemIfSet( RES_PAGEDESC )) &&
         pDescItem->GetPageDesc() )
    {
        AppendSection( *pDescItem, *pNd, pFormat, nRstLnNum );
    }
    else
        AppendSection( rExport.m_pCurrentPageDesc, pFormat, nRstLnNum, /*bIsFirstParagraph=*/true );
}

WW8_WrPlcSepx::WW8_WrPlcSepx( MSWordExportBase& rExport )
    : MSWordSections( rExport )
    , m_bHeaderFooterWritten( false )
{
    // to be in sync with the AppendSection() call in the MSWordSections
    // constructor
    m_aCps.push_back( 0 );
}

MSWordSections::~MSWordSections()
{
}

WW8_WrPlcSepx::~WW8_WrPlcSepx()
{
}

bool MSWordSections::HeaderFooterWritten()
{
    return false; // only relevant for WW8
}

bool WW8_WrPlcSepx::HeaderFooterWritten()
{
    return m_bHeaderFooterWritten;
}

sal_uInt16 MSWordSections::CurrentNumberOfColumns( const SwDoc &rDoc ) const
{
    OSL_ENSURE( !m_aSects.empty(), "no segment inserted yet" );
    if ( m_aSects.empty() )
        return 1;

    return GetFormatCol(rDoc, m_aSects.back()).GetNumCols();
}

const SwFormatCol& MSWordSections::GetFormatCol(const SwDoc &rDoc, const WW8_SepInfo& rInfo)
{
    const SwPageDesc* pPd = rInfo.pPageDesc;
    if ( !pPd )
        pPd = &rDoc.GetPageDesc( 0 );

    const SfxItemSet &rSet = pPd->GetMaster().GetAttrSet();
    SfxItemSetFixed<RES_COL, RES_COL> aSet( *rSet.GetPool() );
    aSet.SetParent( &rSet );

    //0xffffffff, what the hell is going on with that!, fixme most terribly
    if ( rInfo.pSectionFormat && reinterpret_cast<SwSectionFormat*>(sal_IntPtr(-1)) != rInfo.pSectionFormat )
        aSet.Put( rInfo.pSectionFormat->GetFormatAttr( RES_COL ) );

    return aSet.Get(RES_COL);
}

const WW8_SepInfo* MSWordSections::CurrentSectionInfo()
{
    if ( !m_aSects.empty() )
        return &m_aSects.back();

    return nullptr;
}

void MSWordSections::AppendSection( const SwPageDesc* pPd,
    const SwSectionFormat* pSectionFormat, sal_uLong nLnNumRestartNo, bool bIsFirstParagraph )
{
    if (HeaderFooterWritten()) {
        return; // #i117955# prevent new sections in endnotes
    }
    m_aSects.emplace_back( pPd, pSectionFormat, nLnNumRestartNo, std::nullopt, nullptr, bIsFirstParagraph );
    NeedsDocumentProtected( m_aSects.back() );
}

void WW8_WrPlcSepx::AppendSep( WW8_CP nStartCp, const SwPageDesc* pPd,
    const SwSectionFormat* pSectionFormat, sal_uLong nLnNumRestartNo )
{
    if (HeaderFooterWritten()) {
        return; // #i117955# prevent new sections in endnotes
    }
    m_aCps.push_back( nStartCp );
    AppendSection( pPd, pSectionFormat, nLnNumRestartNo );
}

void MSWordSections::AppendSection( const SwFormatPageDesc& rPD,
    const SwNode& rNd, const SwSectionFormat* pSectionFormat, sal_uLong nLnNumRestartNo )
{
    if (HeaderFooterWritten()) {
        return; // #i117955# prevent new sections in endnotes
    }

    WW8_SepInfo aI( rPD.GetPageDesc(), pSectionFormat, nLnNumRestartNo, rPD.GetNumOffset(), &rNd );

    m_aSects.push_back( aI );
    NeedsDocumentProtected( aI );
}

void WW8_WrPlcSepx::AppendSep( WW8_CP nStartCp, const SwFormatPageDesc& rPD,
    const SwNode& rNd, const SwSectionFormat* pSectionFormat, sal_uLong nLnNumRestartNo )
{
    if (HeaderFooterWritten()) {
        return; // #i117955# prevent new sections in endnotes
    }
    m_aCps.push_back( nStartCp );
    AppendSection( rPD, rNd, pSectionFormat, nLnNumRestartNo );
}

void WW8_WrPlcSepx::WriteFootnoteEndText( WW8Export& rWrt, sal_uLong nCpStt )
{
    sal_uInt8 nInfoFlags = 0;
    const SwFootnoteInfo& rInfo = rWrt.m_rDoc.GetFootnoteInfo();
    if( !rInfo.m_aErgoSum.isEmpty() )  nInfoFlags |= 0x02;
    if( !rInfo.m_aQuoVadis.isEmpty() ) nInfoFlags |= 0x04;

    sal_uInt8 nEmptyStt = 0;
    if( nInfoFlags )
    {
        m_pTextPos->Append( nCpStt );  // empty footnote separator

        if( 0x02 & nInfoFlags )         // Footnote continuation separator
        {
            m_pTextPos->Append( nCpStt );
            rWrt.WriteStringAsPara( rInfo.m_aErgoSum );
            rWrt.WriteStringAsPara( OUString() );
            nCpStt = rWrt.Fc2Cp( rWrt.Strm().Tell() );
        }
        else
            m_pTextPos->Append( nCpStt );

        if( 0x04 & nInfoFlags )         // Footnote continuation notice
        {
            m_pTextPos->Append( nCpStt );
            rWrt.WriteStringAsPara( rInfo.m_aQuoVadis );
            rWrt.WriteStringAsPara( OUString() );
            nCpStt = rWrt.Fc2Cp( rWrt.Strm().Tell() );
        }
        else
            m_pTextPos->Append( nCpStt );

        nEmptyStt = 3;
    }

    while( 6 > nEmptyStt++ )
        m_pTextPos->Append( nCpStt );

    // set the flags at the Dop right away
    WW8Dop& rDop = *rWrt.m_pDop;
    // Footnote Info
    switch( rInfo.m_eNum )
    {
    case FTNNUM_PAGE:       rDop.rncFootnote = 2; break;
    case FTNNUM_CHAPTER:    rDop.rncFootnote  = 1; break;
    default: rDop.rncFootnote  = 0; break;
    }                                   // rncFootnote
    rDop.nfcFootnoteRef = WW8Export::GetNumId( rInfo.m_aFormat.GetNumberingType() );
    rDop.nFootnote = rInfo.m_nFootnoteOffset + 1;
    rDop.fpc = rWrt.m_bFootnoteAtTextEnd ? 2 : 1;

    // Endnote Info
    rDop.rncEdn = 0;                        // rncEdn: Don't Restart
    const SwEndNoteInfo& rEndInfo = rWrt.m_rDoc.GetEndNoteInfo();
    rDop.nfcEdnRef = WW8Export::GetNumId( rEndInfo.m_aFormat.GetNumberingType() );
    rDop.nEdn = rEndInfo.m_nFootnoteOffset + 1;
    rDop.epc = rWrt.m_bEndAtTextEnd ? 3 : 0;
}

void MSWordSections::SetHeaderFlag( sal_uInt8& rHeadFootFlags, const SwFormat& rFormat,
    sal_uInt8 nFlag )
{
    const SwFormatHeader* pItem = rFormat.GetItemIfSet(RES_HEADER);
    if( pItem && pItem->IsActive() && pItem->GetHeaderFormat() )
        rHeadFootFlags |= nFlag;
}

void MSWordSections::SetFooterFlag( sal_uInt8& rHeadFootFlags, const SwFormat& rFormat,
    sal_uInt8 nFlag )
{
    const SwFormatFooter* pItem = rFormat.GetItemIfSet(RES_FOOTER);
    if( pItem && pItem->IsActive() && pItem->GetFooterFormat() )
        rHeadFootFlags |= nFlag;
}

void WW8_WrPlcSepx::OutHeaderFooter( WW8Export& rWrt, bool bHeader,
                     const SwFormat& rFormat, sal_uLong& rCpPos, sal_uInt8 nHFFlags,
                     sal_uInt8 nFlag,  sal_uInt8 nBreakCode)
{
    if ( nFlag & nHFFlags )
    {
        m_pTextPos->Append( rCpPos );
        rWrt.WriteHeaderFooterText( rFormat, bHeader);
        rWrt.WriteStringAsPara( OUString() ); // CR to the end ( otherwise WW complains )
        rCpPos = rWrt.Fc2Cp( rWrt.Strm().Tell() );
    }
    else
    {
        m_pTextPos->Append( rCpPos );
        if ((bHeader? rWrt.m_bHasHdr : rWrt.m_bHasFtr) && nBreakCode!=0)
        {
            rWrt.WriteStringAsPara( OUString() ); // Empty paragraph for empty header/footer
            rWrt.WriteStringAsPara( OUString() ); // a CR that WW8 needs for end of the stream
            rCpPos = rWrt.Fc2Cp( rWrt.Strm().Tell() );
        }
    }
}

void MSWordSections::NeedsDocumentProtected(const WW8_SepInfo &rInfo)
{
    if (rInfo.IsProtected())
        mbDocumentIsProtected = true;
}

bool WW8_SepInfo::IsProtected() const
{
    bool bRet = false;
    if (
         pSectionFormat &&
         (reinterpret_cast<SwSectionFormat*>(sal_IntPtr(-1)) != pSectionFormat)
       )
    {
        const SwSection *pSection = pSectionFormat->GetSection();
        if (pSection && pSection->IsProtect())
        {
            bRet = true;
        }
    }
    return bRet;
}

void MSWordSections::CheckForFacinPg( const WW8Export& rWrt ) const
{
    // 2 values getting set
    //      Dop.fFacingPages            == Header and Footer different
    //      Dop.fSwapBordersFacingPgs   == mirrored borders
    sal_uInt16 nEnd = 0;
    for( const WW8_SepInfo& rSepInfo : m_aSects )
    {
        if( !rSepInfo.pSectionFormat )
        {
            const SwPageDesc* pPd = rSepInfo.pPageDesc;
            if( pPd->GetFollow() && pPd != pPd->GetFollow() &&
                pPd->GetFollow()->GetFollow() == pPd->GetFollow() &&
                rSepInfo.pPDNd &&
                pPd->IsFollowNextPageOfNode( *rSepInfo.pPDNd ) )
                // so this is first page and subsequent, so only respect follow
                pPd = pPd->GetFollow();

            // left-/right chain of pagedescs ?
            else if( !( 1 & nEnd ) &&
                pPd->GetFollow() && pPd != pPd->GetFollow() &&
                pPd->GetFollow()->GetFollow() == pPd &&
                (( UseOnPage::Left == ( UseOnPage::All & pPd->ReadUseOn() ) &&
                   UseOnPage::Right == ( UseOnPage::All & pPd->GetFollow()->ReadUseOn() )) ||
                 ( UseOnPage::Right == ( UseOnPage::All & pPd->ReadUseOn() ) &&
                   UseOnPage::Left == ( UseOnPage::All & pPd->GetFollow()->ReadUseOn() )) ))
            {
                rWrt.m_pDop->fFacingPages = rWrt.m_pDop->fMirrorMargins = true;
                nEnd |= 1;
            }

            if( !( 1 & nEnd ) &&
                ( !pPd->IsHeaderShared() || !pPd->IsFooterShared() ))
            {
                rWrt.m_pDop->fFacingPages = true;
                nEnd |= 1;
            }
            if( !( 2 & nEnd ) &&
                UseOnPage::Mirror == ( UseOnPage::Mirror & pPd->ReadUseOn() ))
            {
                rWrt.m_pDop->fSwapBordersFacingPgs =
                    rWrt.m_pDop->fMirrorMargins = true;
                nEnd |= 2;
            }

            if( 3 == nEnd )
                break;      // We do not need to go any further
        }
    }
}

bool MSWordSections::HasBorderItem( const SwFormat& rFormat )
{
    const SvxBoxItem* pItem = rFormat.GetItemIfSet(RES_BOX);
    return pItem &&
            (   pItem->GetTop() ||
                pItem->GetBottom()  ||
                pItem->GetLeft()  ||
                pItem->GetRight() );
}

void WW8AttributeOutput::StartSection()
{
    m_rWW8Export.m_pO->clear();
}

void WW8AttributeOutput::SectFootnoteEndnotePr()
{
    const SwFootnoteInfo& rInfo = m_rWW8Export.m_rDoc.GetFootnoteInfo();
    const SwEndNoteInfo& rEndNoteInfo = m_rWW8Export.m_rDoc.GetEndNoteInfo();
    m_rWW8Export.InsUInt16( NS_sprm::SRncFtn::val );
    switch( rInfo.m_eNum )
    {
    case FTNNUM_PAGE:     m_rWW8Export.m_pO->push_back( sal_uInt8/*rncRstPage*/ (2) ); break;
    case FTNNUM_CHAPTER:  m_rWW8Export.m_pO->push_back( sal_uInt8/*rncRstSect*/ (1) ); break;
    default:              m_rWW8Export.m_pO->push_back( sal_uInt8/*rncCont*/ (0) ); break;
    }

    m_rWW8Export.InsUInt16(NS_sprm::SNfcFtnRef::val);
    sal_uInt8 nId = WW8Export::GetNumId(rInfo.m_aFormat.GetNumberingType());
    SwWW8Writer::InsUInt16(*m_rWW8Export.m_pO, nId);
    m_rWW8Export.InsUInt16(NS_sprm::SNfcEdnRef::val);
    nId = WW8Export::GetNumId(rEndNoteInfo.m_aFormat.GetNumberingType());
    SwWW8Writer::InsUInt16(*m_rWW8Export.m_pO, nId);
}

void WW8AttributeOutput::SectionFormProtection( bool bProtected )
{
    //If the document is to be exported as protected, then if a segment
    //is not protected, set the unlocked flag
    if ( m_rWW8Export.m_pSepx->DocumentIsProtected() && !bProtected )
    {
        SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, NS_sprm::SFProtected::val );
        m_rWW8Export.m_pO->push_back( 1 );
    }
}

void WW8AttributeOutput::SectionLineNumbering( sal_uLong nRestartNo, const SwLineNumberInfo& rLnNumInfo )
{
    // sprmSNLnnMod - activate Line Numbering and define Modulo
    SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, NS_sprm::SNLnnMod::val );
    SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, rLnNumInfo.GetCountBy() );

    // sprmSDxaLnn - xPosition of Line Number
    SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, NS_sprm::SDxaLnn::val );
    SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, rLnNumInfo.GetPosFromLeft() );

    // sprmSLnc - restart number: 0 per page, 1 per section, 2 never restart
    if ( nRestartNo || !rLnNumInfo.IsRestartEachPage() )
    {
        SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, NS_sprm::SLnc::val );
        m_rWW8Export.m_pO->push_back( nRestartNo ? 1 : 2 );
    }

    // sprmSLnnMin - Restart the Line Number with given value
    if ( nRestartNo )
    {
        SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, NS_sprm::SLnnMin::val );
        SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, o3tl::narrowing<sal_uInt16>(nRestartNo) - 1 );
    }
}

void WW8AttributeOutput::SectionTitlePage()
{
    // sprmSFTitlePage
    SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, NS_sprm::SFTitlePage::val );
    m_rWW8Export.m_pO->push_back( 1 );
}

void WW8AttributeOutput::SectionPageBorders( const SwFrameFormat* pPdFormat, const SwFrameFormat* pPdFirstPgFormat )
{
    // write border of page
    sal_uInt16 nPgBorder = MSWordSections::HasBorderItem( *pPdFormat ) ? 0 : USHRT_MAX;
    if ( pPdFormat != pPdFirstPgFormat )
    {
        if ( MSWordSections::HasBorderItem( *pPdFirstPgFormat ) )
        {
            if ( USHRT_MAX == nPgBorder )
            {
                nPgBorder = 1;
                // only the first page outlined -> Get the BoxItem from the correct format
                m_rWW8Export.m_pISet = &pPdFirstPgFormat->GetAttrSet();
                OutputItem( pPdFirstPgFormat->GetFormatAttr( RES_BOX ) );
            }
        }
        else if ( !nPgBorder )
            nPgBorder = 2;
    }

    // [MS-DOC] 2.9.255 SPgbPropOperand; 2.9.185 PgbOffsetFrom
    if (m_bFromEdge)
        nPgBorder |= (1<<5);

    if ( USHRT_MAX != nPgBorder )
    {
        // write the Flag and Border Attribute
        SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, NS_sprm::SPgbProp::val );
        SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, nPgBorder );
    }
}

void WW8AttributeOutput::SectionBiDi( bool bBiDi )
{
    SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, NS_sprm::SFBiDi::val );
    m_rWW8Export.m_pO->push_back( bBiDi? 1: 0 );
}

void WW8AttributeOutput::SectionPageNumbering( sal_uInt16 nNumType, const ::std::optional<sal_uInt16>& oPageRestartNumber )
{
    // sprmSNfcPgn
    sal_uInt8 nb = WW8Export::GetNumId( nNumType );
    SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, NS_sprm::SNfcPgn::val );
    m_rWW8Export.m_pO->push_back( nb );

    if ( oPageRestartNumber )
    {
        // sprmSFPgnRestart
        SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, NS_sprm::SFPgnRestart::val );
        m_rWW8Export.m_pO->push_back( 1 );

        // sprmSPgnStart
        SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, NS_sprm::SPgnStart97::val );
        SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, *oPageRestartNumber );
    }
}

void WW8AttributeOutput::SectionType( sal_uInt8 nBreakCode )
{
    if ( 2 != nBreakCode ) // new page is the default
    {
        SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, NS_sprm::SBkc::val );
        m_rWW8Export.m_pO->push_back( nBreakCode );
    }
}

void WW8Export::SetupSectionPositions( WW8_PdAttrDesc* pA )
{
    if ( !pA )
        return;

    if ( !m_pO->empty() ) // are there attributes ?
    {
        pA->m_nLen = m_pO->size();
        pA->m_pData.reset(new sal_uInt8 [m_pO->size()]);
        // store for later
        memcpy( pA->m_pData.get(), m_pO->data(), m_pO->size() );
        m_pO->clear(); // clear HdFt-Text
    }
    else // no attributes there
    {
        pA->m_pData.reset();
        pA->m_nLen = 0;
    }
}

void WW8AttributeOutput::TextVerticalAdjustment( const drawing::TextVerticalAdjust nVA )
{
    if ( drawing::TextVerticalAdjust_TOP == nVA ) // top alignment is the default
        return;

    sal_uInt8 nMSVA = 0;
    switch( nVA )
    {
        case drawing::TextVerticalAdjust_CENTER:
            nMSVA = 1;
            break;
        case drawing::TextVerticalAdjust_BOTTOM:  //Writer = 2, Word = 3
            nMSVA = 3;
            break;
        case drawing::TextVerticalAdjust_BLOCK:   //Writer = 3, Word = 2
            nMSVA = 2;
            break;
        default:
            break;
    }
    SwWW8Writer::InsUInt16( *m_rWW8Export.m_pO, NS_sprm::SVjc::val );
    m_rWW8Export.m_pO->push_back( nMSVA );
}

void WW8Export::WriteHeadersFooters( sal_uInt8 nHeadFootFlags,
        const SwFrameFormat& rFormat, const SwFrameFormat& rLeftHeaderFormat, const SwFrameFormat& rLeftFooterFormat, const SwFrameFormat& rFirstPageFormat, sal_uInt8 nBreakCode, bool /*bEvenAndOddHeaders*/ )
{
    sal_uLong nCpPos = Fc2Cp( Strm().Tell() );

    IncrementHdFtIndex();
    if ( !(nHeadFootFlags & WW8_HEADER_EVEN) && m_pDop->fFacingPages )
        m_pSepx->OutHeaderFooter( *this, true, rFormat, nCpPos, nHeadFootFlags, WW8_HEADER_ODD, nBreakCode );
    else
        m_pSepx->OutHeaderFooter( *this, true, rLeftHeaderFormat, nCpPos, nHeadFootFlags, WW8_HEADER_EVEN, nBreakCode );
    IncrementHdFtIndex();
    m_pSepx->OutHeaderFooter( *this, true, rFormat, nCpPos, nHeadFootFlags, WW8_HEADER_ODD, nBreakCode );

    IncrementHdFtIndex();
    if ( !(nHeadFootFlags & WW8_FOOTER_EVEN) && m_pDop->fFacingPages )
        m_pSepx->OutHeaderFooter( *this, false, rFormat, nCpPos, nHeadFootFlags, WW8_FOOTER_ODD, nBreakCode );
    else
        m_pSepx->OutHeaderFooter( *this, false, rLeftFooterFormat, nCpPos, nHeadFootFlags, WW8_FOOTER_EVEN, nBreakCode );
    IncrementHdFtIndex();
    m_pSepx->OutHeaderFooter( *this, false, rFormat, nCpPos, nHeadFootFlags, WW8_FOOTER_ODD, nBreakCode );

    //#i24344# Drawing objects cannot be directly shared between main hd/ft
    //and title hd/ft so we need to differentiate them
    IncrementHdFtIndex();
    m_pSepx->OutHeaderFooter( *this, true, rFirstPageFormat, nCpPos, nHeadFootFlags, WW8_HEADER_FIRST, nBreakCode );
    m_pSepx->OutHeaderFooter( *this, false, rFirstPageFormat, nCpPos, nHeadFootFlags, WW8_FOOTER_FIRST, nBreakCode );
}

namespace
{
/**
 * Determines if the continuous section break we start should use page style properties (header,
 * footer, margins) from the next page style of the previous section.
 */
bool UsePrevSectionNextStyle(sal_uInt8 nBreakCode, const SwPageDesc* pPd,
                             const WW8_SepInfo& rSepInfo)
{
    if (nBreakCode != 0)
    {
        // Not a continuous section break.
        return false;
    }

    if (!pPd->GetFollow())
    {
        // Page style has no follow style.
        return false;
    }

    // We start a continuous section break without headers/footers. Possibly the importer had
    // headers/footers for this section break and put them to the closest page break's page style's
    // next page style. See "find a node in the section that has a page break" in writerfilter/.
    // Try the last-in-practice paragraph of the previous section.
    const SwSectionFormat* pSection = rSepInfo.pSectionFormat;
    if (pSection == reinterpret_cast<SwSectionFormat*>(sal_IntPtr(-1)))
    {
        return false;
    }

    const SwNodeIndex* pSectionStart = pSection->GetContent().GetContentIdx();
    if (!pSectionStart)
    {
        return false;
    }

    SwPaM aPaM(*pSectionStart);
    aPaM.Move(fnMoveBackward);
    if (!aPaM.GetPointNode().IsTextNode())
    {
        return false;
    }

    SwTextNode* pTextNode = aPaM.GetPointNode().GetTextNode();
    const SwAttrSet* pParaProps = &pTextNode->GetSwAttrSet();
    sal_uInt32 nCharHeight = pParaProps->GetSize().GetHeight();
    if (nCharHeight > 20)
    {
        return false;
    }

    aPaM.Move(fnMoveBackward);
    if (!aPaM.GetPointNode().IsTextNode())
    {
        return false;
    }

    pTextNode = aPaM.GetPointNode().GetTextNode();
    pParaProps = &pTextNode->GetSwAttrSet();
    return pParaProps->HasItem(RES_PAGEDESC);
}
}

void MSWordExportBase::SectionProperties( const WW8_SepInfo& rSepInfo, WW8_PdAttrDesc* pA )
{
    const SwPageDesc* pPd = rSepInfo.pPageDesc;

    if ( rSepInfo.pSectionFormat && !pPd )
        pPd = &m_rDoc.GetPageDesc( 0 );

    m_pCurrentPageDesc = pPd;

    if ( !pPd )
        return;

    bool bOldPg = m_bOutPageDescs;
    m_bOutPageDescs = true;
    const SwPageDesc* pSavedPageDesc = pPd;

    AttrOutput().StartSection();

    AttrOutput().SectFootnoteEndnotePr();

    // forms
    AttrOutput().SectionFormProtection( rSepInfo.IsProtected() );

    // line numbers
    const SwLineNumberInfo& rLnNumInfo = m_rDoc.GetLineNumberInfo();
    if ( rLnNumInfo.IsPaintLineNumbers() )
        AttrOutput().SectionLineNumbering( rSepInfo.nLnNumRestartNo, rLnNumInfo );

    /*  sprmSBkc, break code:   0 No break, 1 New column
        2 New page, 3 Even page, 4 Odd page
        */
    sal_uInt8 nBreakCode = 2;            // default start new page
    bool bOutPgDscSet = true, bLeftRightPgChain = false, bOutputStyleItemSet = false;
    const SwFrameFormat* pPdFormat = &pPd->GetMaster();
    bool bUsePrevSectionNextStyle = false;
    if ( rSepInfo.pSectionFormat )
    {
        // if pSectionFormat is set, then there is a SectionNode
        //  valid pointer -> start Section ,
        //  0xfff -> Section terminated
        nBreakCode = 0;         // consecutive section

        if (rSepInfo.pPDNd && (rSepInfo.pPDNd->IsContentNode() || rSepInfo.pPDNd->IsTableNode()))
        {
            const SfxItemSet* pSet
                = rSepInfo.pPDNd->IsContentNode()
                      ? &rSepInfo.pPDNd->GetContentNode()->GetSwAttrSet()
                      : &rSepInfo.pPDNd->GetTableNode()->GetTable().GetFrameFormat()->GetAttrSet();

            if (!NoPageBreakSection(pSet))
                nBreakCode = 2;
        }

        if (reinterpret_cast<SwSectionFormat*>(sal_IntPtr(-1)) != rSepInfo.pSectionFormat)
        {
            if ( nBreakCode == 0 )
                bOutPgDscSet = false;

            // produce Itemset, which inherits PgDesk-Attr-Set:
            // as child also the parent is searched if 'deep'-OutputItemSet
            const SfxItemSet* pPdSet = &pPdFormat->GetAttrSet();

            bUsePrevSectionNextStyle = GetExportFormat() == ExportFormat::DOCX
                                       && UsePrevSectionNextStyle(nBreakCode, pPd, rSepInfo);
            if (bUsePrevSectionNextStyle)
            {
                // Take page margins from the previous section's next style.
                pPdSet = &pPd->GetFollow()->GetMaster().GetAttrSet();
            }

            SfxItemSet aSet( *pPdSet->GetPool(), pPdSet->GetRanges() );
            aSet.SetParent( pPdSet );

            // at the child ONLY change column structure according to Sect-Attr.

            const SvxLRSpaceItem &rSectionLR =
                rSepInfo.pSectionFormat->GetFormatAttr( RES_LR_SPACE );
            const SvxLRSpaceItem &rPageLR =
                pPdFormat->GetFormatAttr( RES_LR_SPACE );

            SvxLRSpaceItem aResultLR(
                SvxIndentValue::twips(rPageLR.ResolveLeft({}) + rSectionLR.ResolveLeft({})),
                SvxIndentValue::twips(rPageLR.ResolveRight({}) + rSectionLR.ResolveRight({})),
                SvxIndentValue::zero(), RES_LR_SPACE);
            //i120133: The Section width should consider section indent value.
            if (rSectionLR.ResolveLeft({}) + rSectionLR.ResolveRight({}) != 0)
            {
                const SwFormatCol& rCol = rSepInfo.pSectionFormat->GetFormatAttr(RES_COL);
                SwFormatCol aCol(rCol);
                aCol.SetAdjustValue(rSectionLR.ResolveLeft({}) + rSectionLR.ResolveRight({}));
                aSet.Put(aCol);
            }
            else
                aSet.Put(rSepInfo.pSectionFormat->GetFormatAttr(RES_COL));

            aSet.Put( aResultLR );

            // and write into the WW-File
            const SfxItemSet* pOldI = m_pISet;
            m_pISet = &aSet;

            // Switch off test on default item values, if page description
            // set (value of <bOutPgDscSet>) isn't written.
            AttrOutput().OutputStyleItemSet( aSet, bOutPgDscSet );
            bOutputStyleItemSet = true;

            //Cannot export as normal page framedir, as continuous sections
            //cannot contain any grid settings like proper sections
            AttrOutput().SectionBiDi( SvxFrameDirection::Horizontal_RL_TB == TrueFrameDirection( *rSepInfo.pSectionFormat ) );

            m_pISet = pOldI;
        }
    }

    // Libreoffice 4.0 introduces support for page styles (SwPageDesc) with
    // a different header/footer for the first page.  The same effect can be
    // achieved by chaining two page styles together (SwPageDesc::GetFollow)
    // which are identical except for header/footer.
    // The latter method was previously used by the doc/docx import filter.
    // In both of these cases, we emit a single Word section with different
    // first page header/footer.
    const SwFrameFormat* pPdFirstPgFormat = &pPd->GetFirstMaster();
    bool titlePage = !pPd->IsFirstShared();
    if ( bOutPgDscSet )
    {
        // if a Follow is set and it does not point to itself,
        // then there is a page chain.
        // If this emulates a "first page", we can detect it here and write
        // it as title page.
        // With Left/Right changes it's different - we have to detect where
        // the change of pages is, but here it's too late for that!
        if ( pPd->GetFollow() && pPd != pPd->GetFollow() &&
             pPd->GetFollow()->GetFollow() == pPd->GetFollow() &&
             pPd->IsHeaderShared() && pPd->IsFooterShared() &&
             ( !rSepInfo.pPDNd || pPd->IsFollowNextPageOfNode( *rSepInfo.pPDNd ) ) )
        {
            const SwPageDesc *pFollow = pPd->GetFollow();
            const SwFrameFormat& rFollowFormat = pFollow->GetMaster();
            if (sw::util::IsPlausableSingleWordSection(*pPdFirstPgFormat, rFollowFormat))
            {
                if (titlePage)
                {
                    // Do nothing. First format is already set.
                }
                else if (rSepInfo.pPDNd)
                    pPdFirstPgFormat = pPd->GetPageFormatOfNode( *rSepInfo.pPDNd );
                else
                    pPdFirstPgFormat = &pPd->GetMaster();

                m_pCurrentPageDesc = pPd = pFollow;
                pPdFormat = &rFollowFormat;

                // has different headers/footers for the title page
                titlePage = true;
            }
        }
        else if (nBreakCode == 2 && pPd == m_pPreviousSectionPageDesc && pPd->GetFollow() == pPd)
        {
            // The first title page has already been displayed in the previous section. Drop it.
            titlePage = false;
        }

        const SfxItemSet* pOldI = m_pISet;

        const SfxPoolItem* pItem;
        if ( titlePage && SfxItemState::SET ==
                pPdFirstPgFormat->GetItemState( RES_PAPER_BIN, true, &pItem ) )
        {
            m_pISet = &pPdFirstPgFormat->GetAttrSet();
            m_bOutFirstPage = true;
            AttrOutput().OutputItem( *pItem );
            m_bOutFirstPage = false;
        }

        // left-/right chain of pagedescs ?
        if ( pPd->GetFollow() && pPd != pPd->GetFollow() &&
                pPd->GetFollow()->GetFollow() == pPd &&
                (( UseOnPage::Left == ( UseOnPage::All & pPd->ReadUseOn() ) &&
                   UseOnPage::Right == ( UseOnPage::All & pPd->GetFollow()->ReadUseOn() )) ||
                 ( UseOnPage::Right == ( UseOnPage::All & pPd->ReadUseOn() ) &&
                   UseOnPage::Left == ( UseOnPage::All & pPd->GetFollow()->ReadUseOn() )) ))
        {
            bLeftRightPgChain = true;

            // which is the reference point? (left or right?)
            // assume it is on the right side!
            if ( UseOnPage::Left == ( UseOnPage::All & pPd->ReadUseOn() ) )
            {
                nBreakCode = 3;
                pPdFormat = &pPd->GetMaster();  //use the current page for settings (margins/width etc)
                pPd = pPd->GetFollow(); //switch to the right page for the right/odd header/footer
            }
            else
                nBreakCode = 4;
        }

        m_pISet = &pPdFormat->GetAttrSet();
        if (!bOutputStyleItemSet)
        {
            if (titlePage)
            {
                m_pFirstPageFormat = pPdFirstPgFormat;
            }

            AttrOutput().OutputStyleItemSet( pPdFormat->GetAttrSet(), false );

            if (titlePage)
            {
                m_pFirstPageFormat = nullptr;
            }
        }
        AttrOutput().SectionPageBorders( pPdFormat, pPdFirstPgFormat );
        m_pISet = pOldI;

        // then the rest of the settings from PageDesc
        AttrOutput().SectionPageNumbering( pPd->GetNumType().GetNumberingType(), rSepInfo.oPgRestartNo );

        // will it be only left or only right pages?
        if ( 2 == nBreakCode )
        {
            if ( UseOnPage::Left == ( UseOnPage::All & pPd->ReadUseOn() ) )
                nBreakCode = 3;
            else if ( UseOnPage::Right == ( UseOnPage::All & pPd->ReadUseOn() ) )
                nBreakCode = 4;
        }
    }

    if (titlePage)
        AttrOutput().SectionTitlePage();

    AttrOutput().SectionType( nBreakCode );

    if( rSepInfo.pPageDesc ) {
        AttrOutput().TextVerticalAdjustment( rSepInfo.pPageDesc->GetVerticalAdjustment() );
    }

    // Header or Footer
    sal_uInt8 nHeadFootFlags = 0;
    // Should we output a w:evenAndOddHeaders tag or not?
    // N.B.: despite its name this tag affects _both_ headers and footers!
    bool bEvenAndOddHeaders = true;
    bool bEvenAndOddFooters = true;

    const SwFrameFormat* pPdLeftHeaderFormat = nullptr;
    const SwFrameFormat* pPdLeftFooterFormat = nullptr;
    if (bLeftRightPgChain)
    {
        const SwFrameFormat* pHeaderFormat = pPd->GetStashedFrameFormat(true, true, true);
        const SwFrameFormat* pFooterFormat = pPd->GetStashedFrameFormat(false, true, true);
        if (pHeaderFormat)
        {
            pPdLeftHeaderFormat = pHeaderFormat;
            bEvenAndOddHeaders = false;
        }
        else
        {
            pPdLeftHeaderFormat = &pPd->GetFollow()->GetFirstLeft();
        }
        if (pFooterFormat)
        {
            pPdLeftFooterFormat = pFooterFormat;
            bEvenAndOddFooters = false;
        }
        else
        {
            pPdLeftFooterFormat = &pPd->GetFollow()->GetFirstLeft();
        }
    }
    else
    {
        const SwFrameFormat* pHeaderFormat = pPd->GetStashedFrameFormat(true, true, false);
        const SwFrameFormat* pFooterFormat = pPd->GetStashedFrameFormat(false, true, false);
        if (pHeaderFormat)
        {
            pPdLeftHeaderFormat = pHeaderFormat;
            bEvenAndOddHeaders = false;
        }
        else
        {
            pPdLeftHeaderFormat = &pPd->GetLeft();
        }
        if (pFooterFormat)
        {
            pPdLeftFooterFormat = pFooterFormat;
            bEvenAndOddFooters = false;
        }
        else
        {
            pPdLeftFooterFormat = &pPd->GetLeft();
        }
    }

    // Ensure that headers are written if section is first paragraph
    if (nBreakCode != 0 || (rSepInfo.pSectionFormat && rSepInfo.bIsFirstParagraph))
    {
        if ( titlePage )
        {
            // there is a First Page:
            MSWordSections::SetHeaderFlag( nHeadFootFlags, *pPdFirstPgFormat, WW8_HEADER_FIRST );
            MSWordSections::SetFooterFlag( nHeadFootFlags, *pPdFirstPgFormat, WW8_FOOTER_FIRST );
        }
        else
        {
            if ( pPd->GetStashedFrameFormat(true, true, true) && pPdLeftHeaderFormat && pPdLeftHeaderFormat->GetHeader().GetHeaderFormat() )
            {
                MSWordSections::SetHeaderFlag( nHeadFootFlags, *pPdLeftHeaderFormat, WW8_HEADER_FIRST );
            }
            if ( pPd->GetStashedFrameFormat(false, true, true) && pPdLeftFooterFormat && pPdLeftFooterFormat->GetFooter().GetFooterFormat() )
            {
                MSWordSections::SetFooterFlag( nHeadFootFlags, *pPdLeftFooterFormat, WW8_FOOTER_FIRST );
            }
        }

        MSWordSections::SetHeaderFlag( nHeadFootFlags, *pPdFormat, WW8_HEADER_ODD );
        MSWordSections::SetFooterFlag( nHeadFootFlags, *pPdFormat, WW8_FOOTER_ODD );

        if ( !pPd->IsHeaderShared() || bLeftRightPgChain )
        {
            MSWordSections::SetHeaderFlag( nHeadFootFlags, *pPdLeftHeaderFormat, WW8_HEADER_EVEN );
        }
        else if ( pPd->IsHeaderShared() && pPd->GetStashedFrameFormat(true, true, false) && pPdLeftHeaderFormat && pPdLeftHeaderFormat->GetHeader().GetHeaderFormat() )
        {
            MSWordSections::SetHeaderFlag( nHeadFootFlags, *pPdLeftHeaderFormat, WW8_HEADER_EVEN );
            bEvenAndOddHeaders = false;
        }

        if ( !pPd->IsFooterShared() || bLeftRightPgChain )
        {
            MSWordSections::SetFooterFlag( nHeadFootFlags, *pPdLeftFooterFormat, WW8_FOOTER_EVEN );
        }
        else if ( pPd->IsFooterShared() && pPd->GetStashedFrameFormat(false, true, false) && pPdLeftFooterFormat && pPdLeftFooterFormat->GetFooter().GetFooterFormat() )
        {
            MSWordSections::SetFooterFlag( nHeadFootFlags, *pPdLeftFooterFormat, WW8_FOOTER_EVEN );
            bEvenAndOddFooters = false;
        }
    }

    // binary filters only
    SetupSectionPositions( pA );

    /*
       !!!!!!!!!!!
    // borders at header and footer texts would be done like this:
    // This should use something like pOut,
    // which is repeated with every special text line.
    const SwFrameFormat* pFFormat = rFt.GetFooterFormat();
    const SvxBoxItem& rBox = pFFormat->GetBox(false);
    OutWW8_SwFormatBox1( m_rWW8Export.pOut, rBox, false);
    !!!!!!!!!!!
    You can turn this into paragraph attributes, which are then observed in each paragraph.
    Applies to background / border.
    !!!!!!!!!!!
    */

    const SwTextNode *pOldPageRoot = GetHdFtPageRoot();
    SetHdFtPageRoot( rSepInfo.pPDNd ? rSepInfo.pPDNd->GetTextNode() : nullptr );

    if (bUsePrevSectionNextStyle && nHeadFootFlags == 0)
    {
        // Take headers/footers from the previous section's next style.
        pPdFormat = &pPd->GetFollow()->GetMaster();
        MSWordSections::SetHeaderFlag(nHeadFootFlags, *pPdFormat, WW8_HEADER_ODD);
        MSWordSections::SetFooterFlag(nHeadFootFlags, *pPdFormat, WW8_FOOTER_ODD);
    }

    WriteHeadersFooters( nHeadFootFlags, *pPdFormat, *pPdLeftHeaderFormat, *pPdLeftFooterFormat, *pPdFirstPgFormat, nBreakCode, bEvenAndOddHeaders && bEvenAndOddFooters );

    SetHdFtPageRoot( pOldPageRoot );

    AttrOutput().EndSection();

    // outside of the section properties again
    m_bOutPageDescs = bOldPg;
    m_pPreviousSectionPageDesc = pSavedPageDesc;
}

bool WW8_WrPlcSepx::WriteKFText( WW8Export& rWrt )
{
    sal_uLong nCpStart = rWrt.Fc2Cp( rWrt.Strm().Tell() );

    OSL_ENSURE( !m_pTextPos, "who set the pointer?" );
    m_pTextPos.reset( new WW8_WrPlc0( nCpStart ) );

    WriteFootnoteEndText( rWrt, nCpStart );
    CheckForFacinPg( rWrt );

    unsigned int nOldIndex = rWrt.GetHdFtIndex();
    rWrt.SetHdFtIndex( 0 );

    for (const WW8_SepInfo & rSepInfo : m_aSects)
    {
        auto pAttrDesc = std::make_shared<WW8_PdAttrDesc>();
        m_SectionAttributes.push_back(pAttrDesc);

        rWrt.SectionProperties( rSepInfo, pAttrDesc.get() );

        // FIXME: this writes the section properties, but not of all sections;
        // it's possible that later in the document (e.g. in endnotes) sections
        // are added, but they won't have their properties written here!
        m_bHeaderFooterWritten = true;
    }
    rWrt.SetHdFtIndex( nOldIndex ); //0

    if ( m_pTextPos->Count() )
    {
        // HdFt available?
        sal_uLong nCpEnd = rWrt.Fc2Cp( rWrt.Strm().Tell() );
        m_pTextPos->Append( nCpEnd );  // End of last Header/Footer for PlcfHdd

        if ( nCpEnd > nCpStart )
        {
            ++nCpEnd;
            m_pTextPos->Append( nCpEnd + 1 );  // End of last Header/Footer for PlcfHdd

            rWrt.WriteStringAsPara( OUString() ); // CR to the end ( otherwise WW complains )
        }
        rWrt.m_pFieldHdFt->Finish( nCpEnd, rWrt.m_pFib->m_ccpText + rWrt.m_pFib->m_ccpFootnote );
        rWrt.m_pFib->m_ccpHdr = nCpEnd - nCpStart;
    }
    else
    {
        m_pTextPos.reset();
    }

    return rWrt.m_pFib->m_ccpHdr != 0;
}

void WW8_WrPlcSepx::WriteSepx( SvStream& rStrm ) const
{
    OSL_ENSURE(m_SectionAttributes.size() == static_cast<size_t>(m_aSects.size())
        , "WriteSepx(): arrays out of sync!");
    for (const auto & rSectionAttribute : m_SectionAttributes) // all sections
    {
        WW8_PdAttrDesc *const pA = rSectionAttribute.get();
        if (pA->m_nLen && pA->m_pData != nullptr)
        {
            pA->m_nSepxFcPos = rStrm.Tell();
            rStrm.WriteUInt16(pA->m_nLen);
            rStrm.WriteBytes(pA->m_pData.get(), pA->m_nLen);
        }
    }
}

void WW8_WrPlcSepx::WritePlcSed( WW8Export& rWrt ) const
{
    OSL_ENSURE(m_SectionAttributes.size() == static_cast<size_t>(m_aSects.size())
        , "WritePlcSed(): arrays out of sync!");
    OSL_ENSURE( m_aCps.size() == m_aSects.size() + 1, "WrPlcSepx: DeSync" );
    sal_uInt64 nFcStart = rWrt.m_pTableStrm->Tell();

    for( decltype(m_aSects)::size_type i = 0; i <= m_aSects.size(); i++ )
    {
        sal_uInt32 nP = m_aCps[i];
        rWrt.m_pTableStrm->WriteUInt32(nP);
    }

    WW8_SED aSed = {{4, 0},{0, 0, 0, 0},{0, 0},{0xff, 0xff, 0xff, 0xff}};

    for (const auto & rSectionAttribute : m_SectionAttributes)
    {
        // Sepx-Pos
        UInt32ToSVBT32( rSectionAttribute->m_nSepxFcPos, aSed.fcSepx );
        rWrt.m_pTableStrm->WriteBytes(&aSed, sizeof(aSed));
    }
    rWrt.m_pFib->m_fcPlcfsed = nFcStart;
    rWrt.m_pFib->m_lcbPlcfsed = rWrt.m_pTableStrm->Tell() - nFcStart;
}

void WW8_WrPlcSepx::WritePlcHdd( WW8Export& rWrt ) const
{
    // Don't write out the PlcfHdd if ccpHdd is 0: it's a validation failure case.
    if( rWrt.m_pFib->m_ccpHdr != 0 && m_pTextPos && m_pTextPos->Count() )
    {
        rWrt.m_pFib->m_fcPlcfhdd = rWrt.m_pTableStrm->Tell();
        m_pTextPos->Write( *rWrt.m_pTableStrm );             // Plc0
        rWrt.m_pFib->m_lcbPlcfhdd = rWrt.m_pTableStrm->Tell() -
                                rWrt.m_pFib->m_fcPlcfhdd;
    }
}

void MSWordExportBase::WriteHeaderFooterText( const SwFormat& rFormat, bool bHeader )
{
    const SwFormatContent *pContent;
    if ( bHeader )
    {
        m_bHasHdr = true;
        const SwFormatHeader& rHd = rFormat.GetHeader();
        OSL_ENSURE( rHd.GetHeaderFormat(), "Header text is not here" );

        if ( !rHd.GetHeaderFormat() )
            return;

        pContent = &rHd.GetHeaderFormat()->GetContent();
    }
    else
    {
        m_bHasFtr = true;
        const SwFormatFooter& rFt = rFormat.GetFooter();
        OSL_ENSURE( rFt.GetFooterFormat(), "Footer text is not here" );

        if ( !rFt.GetFooterFormat() )
            return;

        pContent = &rFt.GetFooterFormat()->GetContent();
    }

    const SwNodeIndex* pSttIdx = pContent->GetContentIdx();

    if ( pSttIdx )
    {
        SwNodeIndex aIdx( *pSttIdx, 1 ),
        aEnd( *pSttIdx->GetNode().EndOfSectionNode() );
        SwNodeOffset nStart = aIdx.GetIndex();
        SwNodeOffset nEnd = aEnd.GetIndex();

        // range, i.e. valid node
        if ( nStart < nEnd )
        {
            bool bOldKF = m_bOutKF;
            m_bOutKF = true;
            WriteSpecialText( nStart, nEnd, TXT_HDFT );
            m_bOutKF = bOldKF;
        }
        else
            pSttIdx = nullptr;
    }

    if ( !pSttIdx )
    {
        // there is no Header/Footer, but a CR is still necessary
        OSL_ENSURE( pSttIdx, "Header/Footer text is not really present" );
        AttrOutput().EmptyParagraph();
    }
}

// class WW8_WrPlcFootnoteEdn : Collect the Footnotes and Endnotes and output their text
// and Plcs at the end of the document.
// WW8_WrPlcFootnoteEdn is the class for Footnotes and Endnotes

WW8_WrPlcSubDoc::WW8_WrPlcSubDoc()
{
}

WW8_WrPlcSubDoc::~WW8_WrPlcSubDoc()
{
}

void WW8_WrPlcFootnoteEdn::Append( WW8_CP nCp, const SwFormatFootnote& rFootnote )
{
    m_aCps.push_back( nCp );
    m_aContent.push_back( &rFootnote );
}

WW8_Annotation::WW8_Annotation(const SwPostItField* pPostIt, WW8_CP nRangeStart, WW8_CP nRangeEnd)
    :
        m_nRangeStart(nRangeStart),
        m_nRangeEnd(nRangeEnd),
        mpAuthorIDs(new SvtSecurityMapPersonalInfo)
{
    mpRichText = pPostIt->GetTextObject();
    if (!mpRichText)
        msSimpleText = pPostIt->GetText();
    initPersonalInfo(pPostIt->GetPar1(), pPostIt->GetInitials(),
                     DateTime(pPostIt->GetDate(), pPostIt->GetTime()));
}

WW8_Annotation::WW8_Annotation(const SwRedlineData* pRedline)
    :
        mpRichText(nullptr),
        msSimpleText(pRedline->GetComment()),
        m_nRangeStart(0),
        m_nRangeEnd(0),
        mpAuthorIDs(new SvtSecurityMapPersonalInfo)
{
    initPersonalInfo(SwModule::get()->GetRedlineAuthor(pRedline->GetAuthor()), u""_ustr,
                     pRedline->GetTimeStamp());
}

void WW8_Annotation::initPersonalInfo(const OUString& sAuthor, const OUString& sInitials,
                                      DateTime aDateTime)
{
    bool bRemovePersonalInfo
        = SvtSecurityOptions::IsOptionSet(SvtSecurityOptions::EOption::DocWarnRemovePersonalInfo)
          && !SvtSecurityOptions::IsOptionSet(
                 SvtSecurityOptions::EOption::DocWarnKeepNoteAuthorDateInfo);
    msOwner = bRemovePersonalInfo ? "Author" + OUString::number(mpAuthorIDs->GetInfoID(sAuthor))
                                  : sAuthor;
    m_sInitials = bRemovePersonalInfo ? "A" + OUString::number(mpAuthorIDs->GetInfoID(sAuthor))
                                      : sInitials;
    maDateTime = bRemovePersonalInfo ? DateTime(DateTime::EMPTY) : aDateTime;
}

bool WW8_Annotation::HasRange() const
{
    if (m_nRangeStart != m_nRangeEnd)
    {
        return true;
    }

    return !m_bIgnoreEmpty;
}

void WW8_WrPlcAnnotations::AddRangeStartPosition(const OUString& rName, WW8_CP nStartCp,
                                                 bool bIgnoreEmpty)
{
    m_aRangeStartPositions[rName] = std::make_pair(nStartCp, bIgnoreEmpty);
}

void WW8_WrPlcAnnotations::Append( WW8_CP nCp, const SwPostItField *pPostIt )
{
    m_aCps.push_back( nCp );
    WW8_Annotation* p;
    if( m_aRangeStartPositions.find(pPostIt->GetName().toString()) != m_aRangeStartPositions.end() )
    {
        auto [nStartCp, bIgnoreEmpty] = m_aRangeStartPositions[pPostIt->GetName().toString()];
        p = new WW8_Annotation(pPostIt, nStartCp, nCp);
        p->m_bIgnoreEmpty = bIgnoreEmpty;
        m_aRangeStartPositions.erase(pPostIt->GetName().toString());
    }
    else
    {
        p = new WW8_Annotation(pPostIt, nCp, nCp);
    }
    m_aContent.push_back( p );
}

void WW8_WrPlcAnnotations::Append( WW8_CP nCp, const SwRedlineData *pRedline )
{
    maProcessedRedlines.insert(pRedline);
    m_aCps.push_back( nCp );
    WW8_Annotation* p = new WW8_Annotation(pRedline);
    m_aContent.push_back( p );
}

bool WW8_WrPlcAnnotations::IsNewRedlineComment( const SwRedlineData *pRedline )
{
    return maProcessedRedlines.find(pRedline) == maProcessedRedlines.end();
}

WW8_WrPlcAnnotations::~WW8_WrPlcAnnotations()
{
    for(const void * p : m_aContent)
        delete static_cast<WW8_Annotation const *>(p);
}

bool WW8_WrPlcSubDoc::WriteGenericText( WW8Export& rWrt, sal_uInt8 nTTyp,
    WW8_CP& rCount )
{
    sal_uInt16 nLen = m_aContent.size();
    if ( !nLen )
        return false;

    sal_uLong nCpStart = rWrt.Fc2Cp( rWrt.Strm().Tell() );
    m_pTextPos.reset( new WW8_WrPlc0( nCpStart ) );
    sal_uInt16 i;

    switch ( nTTyp )
    {
        case TXT_ATN:
            for ( i = 0; i < nLen; i++ )
            {
                // beginning for PlcfAtnText
                m_pTextPos->Append( rWrt.Fc2Cp( rWrt.Strm().Tell() ));

                rWrt.WritePostItBegin();
                const WW8_Annotation& rAtn = *static_cast<const WW8_Annotation*>(m_aContent[i]);
                if (rAtn.mpRichText)
                    rWrt.WriteOutliner(*rAtn.mpRichText, nTTyp);
                else
                {
                    OUString sText(rAtn.msSimpleText);
                    rWrt.WriteStringAsPara(sText.replace(0x0A, 0x0B));
                }
            }
            break;

        case TXT_TXTBOX:
        case TXT_HFTXTBOX:
            for ( i = 0; i < nLen; i++ )
            {
                // textbox content
                WW8_CP nCP = rWrt.Fc2Cp( rWrt.Strm().Tell() );
                m_aCps.insert( m_aCps.begin()+i, nCP );
                m_pTextPos->Append( nCP );

                if( m_aContent[ i ] != nullptr )
                {
                    // is it a writer or sdr - textbox?
                    const SdrObject& rObj = *static_cast<SdrObject const *>(m_aContent[ i ]);
                    if (rObj.GetObjInventor() == SdrInventor::FmForm)
                    {
                        sal_uInt8 nOldTyp = rWrt.m_nTextTyp;
                        rWrt.m_nTextTyp = nTTyp;
                        rWrt.GetOCXExp().ExportControl(rWrt, dynamic_cast<const SdrUnoObj&>(rObj));
                        rWrt.m_nTextTyp = nOldTyp;
                    }
                    else if( auto pText = DynCastSdrTextObj(&rObj) )
                        rWrt.WriteSdrTextObj(*pText, nTTyp);
                    else
                    {
                        const SwFrameFormat* pFormat = ::FindFrameFormat( &rObj );
                        assert(pFormat && "where is the format?");

                        const SwNodeIndex* pNdIdx = pFormat->GetContent().GetContentIdx();
                        assert(pNdIdx && "where is the StartNode of the Textbox?");
                        rWrt.WriteSpecialText( pNdIdx->GetIndex() + 1,
                                               pNdIdx->GetNode().EndOfSectionIndex(),
                                               nTTyp );
                        {
                            SwNodeIndex aContentIdx = *pNdIdx;
                            ++aContentIdx;
                            if ( aContentIdx.GetNode().IsTableNode() )
                            {
                                bool bContainsOnlyTables = true;
                                do {
                                    aContentIdx = *(aContentIdx.GetNode().EndOfSectionNode());
                                    ++aContentIdx;
                                    if ( !aContentIdx.GetNode().IsTableNode() &&
                                         aContentIdx.GetIndex() != pNdIdx->GetNode().EndOfSectionIndex() )
                                    {
                                        bContainsOnlyTables = false;
                                    }
                                } while ( aContentIdx.GetNode().IsTableNode() );
                                if ( bContainsOnlyTables )
                                {
                                    // Additional paragraph containing a space to
                                    // assure that by WW created RTF from written WW8
                                    // does not crash WW.
                                    rWrt.WriteStringAsPara( u" "_ustr );
                                }
                            }
                        }
                    }
                }
                else if (i < m_aSpareFormats.size() && m_aSpareFormats[i])
                {
                    const SwFrameFormat& rFormat = *m_aSpareFormats[i];
                    const SwNodeIndex* pNdIdx = rFormat.GetContent().GetContentIdx();
                    rWrt.WriteSpecialText( pNdIdx->GetIndex() + 1,
                               pNdIdx->GetNode().EndOfSectionIndex(), nTTyp );
                }

                // CR at end of one textbox text ( otherwise WW gpft :-( )
                rWrt.WriteStringAsPara( OUString() );
            }
            break;

        case TXT_EDN:
        case TXT_FTN:
            for ( i = 0; i < nLen; i++ )
            {
                // beginning for PlcfFootnoteText/PlcfEdnText
                m_pTextPos->Append( rWrt.Fc2Cp( rWrt.Strm().Tell() ));

                // Note content
                const SwFormatFootnote* pFootnote = static_cast<SwFormatFootnote const *>(m_aContent[ i ]);
                rWrt.WriteFootnoteBegin( *pFootnote );
                const SwNodeIndex* pIdx = pFootnote->GetTextFootnote()->GetStartNode();
                assert(pIdx && "Where is the start node of Foot-/Endnote?");
                rWrt.WriteSpecialText( pIdx->GetIndex() + 1,
                                       pIdx->GetNode().EndOfSectionIndex(),
                                       nTTyp );
            }
            break;

        default:
            OSL_ENSURE( false, "What kind of SubDocType is that?" );
    }

    m_pTextPos->Append( rWrt.Fc2Cp( rWrt.Strm().Tell() ));
    // CR to the end ( otherwise WW complains )
    rWrt.WriteStringAsPara( OUString() );

    WW8_CP nCpEnd = rWrt.Fc2Cp( rWrt.Strm().Tell() );
    m_pTextPos->Append( nCpEnd );
    rCount = nCpEnd - nCpStart;

    return ( rCount != 0 );
}

static bool lcl_AuthorComp( const std::pair<OUString,OUString>& aFirst, const std::pair<OUString,OUString>& aSecond)
{
    return aFirst.first < aSecond.first;
}

static bool lcl_PosComp( const std::pair<WW8_CP, int>& aFirst, const std::pair<WW8_CP, int>& aSecond)
{
    return aFirst.first < aSecond.first;
}

void WW8_WrPlcSubDoc::WriteGenericPlc( WW8Export& rWrt, sal_uInt8 nTTyp,
    WW8_FC& rTextStart, sal_Int32& rTextCount, WW8_FC& rRefStart, sal_Int32& rRefCount ) const
{

    sal_uInt64 nFcStart = rWrt.m_pTableStrm->Tell();
    sal_uInt16 nLen = m_aCps.size();
    if ( !nLen )
        return;

    OSL_ENSURE( m_aCps.size() + 2 == m_pTextPos->Count(), "WritePlc: DeSync" );

    std::vector<std::pair<OUString,OUString> > aStrArr;
    WW8Fib& rFib = *rWrt.m_pFib;              // n+1-th CP-Pos according to the manual
    bool bWriteCP = true;

    switch ( nTTyp )
    {
        case TXT_ATN:
            {
                std::vector< std::pair<WW8_CP, int> > aRangeStartPos; // The second of the pair is the original index before sorting.
                std::vector< std::pair<WW8_CP, int> > aRangeEndPos; // Same, so we can map between the indexes before/after sorting.
                std::map<int, int> aAtnStartMap; // Maps from annotation index to start index.
                std::map<int, int> aStartAtnMap; // Maps from start index to annotation index.
                std::map<int, int> aStartEndMap; // Maps from start index to end index.
                // then write first the GrpXstAtnOwners
                int nIdx = 0;
                for ( sal_uInt16 i = 0; i < nLen; ++i )
                {
                    const WW8_Annotation& rAtn = *static_cast<const WW8_Annotation*>(m_aContent[i]);
                    aStrArr.emplace_back(rAtn.msOwner,rAtn.m_sInitials);
                    // record start and end positions for ranges
                    if (rAtn.HasRange())
                    {
                        aRangeStartPos.emplace_back(rAtn.m_nRangeStart, nIdx);
                        aRangeEndPos.emplace_back(rAtn.m_nRangeEnd, nIdx);
                        ++nIdx;
                    }
                }

                //sort and remove duplicates
                std::sort(aStrArr.begin(), aStrArr.end(),&lcl_AuthorComp);
                auto aIter = std::unique(aStrArr.begin(), aStrArr.end());
                aStrArr.erase(aIter, aStrArr.end());

                // Also sort the start and end positions. We need to reference
                // the start index in the annotation table and also need to
                // reference the end index in the start table, so build a map
                // that knows what index to reference, after sorting.
                std::sort(aRangeStartPos.begin(), aRangeStartPos.end(), &lcl_PosComp);
                for (decltype(aRangeStartPos)::size_type i = 0; i < aRangeStartPos.size(); ++i)
                {
                    aAtnStartMap[aRangeStartPos[i].second] = i;
                    aStartAtnMap[i] = aRangeStartPos[i].second;
                }
                std::sort(aRangeEndPos.begin(), aRangeEndPos.end(), &lcl_PosComp);
                for (decltype(aRangeEndPos)::size_type i = 0; i < aRangeEndPos.size(); ++i)
                    aStartEndMap[aAtnStartMap[ aRangeEndPos[i].second ]] = i;

                for ( decltype(aStrArr)::size_type i = 0; i < aStrArr.size(); ++i )
                {
                    const OUString& sAuthor = aStrArr[i].first;
                    SwWW8Writer::WriteShort(*rWrt.m_pTableStrm, sAuthor.getLength());
                    SwWW8Writer::WriteString16(*rWrt.m_pTableStrm, sAuthor,
                            false);
                }

                rFib.m_fcGrpStAtnOwners = nFcStart;
                nFcStart = rWrt.m_pTableStrm->Tell();
                rFib.m_lcbGrpStAtnOwners = nFcStart - rFib.m_fcGrpStAtnOwners;

                // Commented text ranges
                if( !aRangeStartPos.empty() )
                {
                    // Commented text ranges starting positions (Plcfbkf.aCP)
                    rFib.m_fcPlcfAtnbkf = nFcStart;
                    for ( decltype(aRangeStartPos)::size_type i = 0; i < aRangeStartPos.size(); ++i )
                    {
                        SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, aRangeStartPos[i].first );
                    }
                    SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, rFib.m_ccpText + 1);

                    // Commented text ranges additional information (Plcfbkf.aFBKF)
                    for ( decltype(aRangeStartPos)::size_type i = 0; i < aRangeStartPos.size(); ++i )
                    {
                        SwWW8Writer::WriteShort( *rWrt.m_pTableStrm, aStartEndMap[i] ); // FBKF.ibkl
                        SwWW8Writer::WriteShort( *rWrt.m_pTableStrm, 0 ); // FBKF.bkc
                    }

                    nFcStart = rWrt.m_pTableStrm->Tell();
                    rFib.m_lcbPlcfAtnbkf = nFcStart - rFib.m_fcPlcfAtnbkf;

                    // Commented text ranges ending positions (PlcfBkl.aCP)
                    rFib.m_fcPlcfAtnbkl = nFcStart;
                    for ( decltype(aRangeEndPos)::size_type i = 0; i < aRangeEndPos.size(); ++i )
                    {
                        SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, aRangeEndPos[i].first );
                    }
                    SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, rFib.m_ccpText + 1);

                    nFcStart = rWrt.m_pTableStrm->Tell();
                    rFib.m_lcbPlcfAtnbkl = nFcStart - rFib.m_fcPlcfAtnbkl;

                    // Commented text ranges as bookmarks (SttbfAtnBkmk)
                    rFib.m_fcSttbfAtnbkmk = nFcStart;
                    SwWW8Writer::WriteShort( *rWrt.m_pTableStrm, sal_Int16(sal_uInt16(0xFFFF)) ); // SttbfAtnBkmk.fExtend
                    SwWW8Writer::WriteShort( *rWrt.m_pTableStrm, aRangeStartPos.size() ); // SttbfAtnBkmk.cData
                    SwWW8Writer::WriteShort( *rWrt.m_pTableStrm, 0xA );                   // SttbfAtnBkmk.cbExtra

                    for ( decltype(aRangeStartPos)::size_type i = 0; i < aRangeStartPos.size(); ++i )
                    {
                        SwWW8Writer::WriteShort( *rWrt.m_pTableStrm, 0 );         // SttbfAtnBkmk.cchData
                        // One ATNBE structure for all text ranges
                        SwWW8Writer::WriteShort( *rWrt.m_pTableStrm, 0x0100 );    // ATNBE.bmc
                        SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, aStartAtnMap[i] );          // ATNBE.lTag
                        SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, -1 );         // ATNBE.lTagOld
                    }

                    nFcStart = rWrt.m_pTableStrm->Tell();
                    rFib.m_lcbSttbfAtnbkmk = nFcStart - rFib.m_fcSttbfAtnbkmk;
                }

                // Write the extended >= Word XP ATRD records
                for( sal_uInt16 i = 0; i < nLen; ++i )
                {
                    const WW8_Annotation& rAtn = *static_cast<const WW8_Annotation*>(m_aContent[i]);

                    sal_uInt32 nDTTM = sw::ms::DateTime2DTTM(rAtn.maDateTime);

                    SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, nDTTM );
                    SwWW8Writer::WriteShort( *rWrt.m_pTableStrm, 0 );
                    SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, 0 );
                    SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, 0 );
                    SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, 0 );
                }

                rFib.m_fcAtrdExtra = nFcStart;
                nFcStart = rWrt.m_pTableStrm->Tell();
                rFib.m_lcbAtrdExtra = nFcStart - rFib.m_fcAtrdExtra;
                rFib.m_fcHplxsdr = 0x01010002;  //WTF, but apparently necessary
                rFib.m_lcbHplxsdr = 0;
            }
            break;
        case TXT_TXTBOX:
        case TXT_HFTXTBOX:
            {
                m_pTextPos->Write( *rWrt.m_pTableStrm );
                const std::vector<sal_uInt32>* pShapeIds = GetShapeIdArr();
                OSL_ENSURE( pShapeIds, "Where are the ShapeIds?" );

                for ( sal_uInt16 i = 0; i < nLen; ++i )
                {
                    // write textbox story - FTXBXS
                    // is it a writer or sdr - textbox?
                    const SdrObject* pObj = static_cast<SdrObject const *>(m_aContent[ i ]);
                    sal_Int32 nCnt = 1;
                    if (DynCastSdrTextObj( pObj ))
                    {
                        // find the "highest" SdrObject of this
                        const SwFrameFormat& rFormat = *::FindFrameFormat( pObj );

                        const SwFormatChain* pChn = &rFormat.GetChain();
                        while ( pChn->GetNext() )
                        {
                            // has a chain?
                            // then calc the cur pos in the chain
                            ++nCnt;
                            pChn = &pChn->GetNext()->GetChain();
                        }
                    }
                    if( nullptr == pObj )
                    {
                        if (i < m_aSpareFormats.size() && m_aSpareFormats[i])
                        {
                            const SwFrameFormat& rFormat = *m_aSpareFormats[i];

                            const SwFormatChain* pChn = &rFormat.GetChain();
                            while( pChn->GetNext() )
                            {
                                // has a chain?
                                // then calc the cur pos in the chain
                                ++nCnt;
                                pChn = &pChn->GetNext()->GetChain();
                            }
                        }
                    }
                    // long cTxbx / iNextReuse
                    SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, nCnt );
                    // long cReusable
                    SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, 0 );
                    // short fReusable
                    SwWW8Writer::WriteShort( *rWrt.m_pTableStrm, 0 );
                    // long reserved
                    SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, -1 );
                    // long lid
                    SwWW8Writer::WriteLong( *rWrt.m_pTableStrm,
                            (*pShapeIds)[i]);
                    // long txidUndo
                    SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, 0 );
                }
                SwWW8Writer::FillCount( *rWrt.m_pTableStrm, 22 );
                bWriteCP = false;
            }
            break;
    }

    if ( bWriteCP )
    {
        // write CP Positions
        for ( sal_uInt16 i = 0; i < nLen; i++ )
            SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, m_aCps[ i ] );

        // n+1-th CP-Pos according to the manual
        SwWW8Writer::WriteLong( *rWrt.m_pTableStrm,
                rFib.m_ccpText + rFib.m_ccpFootnote + rFib.m_ccpHdr + rFib.m_ccpEdn +
                rFib.m_ccpTxbx + rFib.m_ccpHdrTxbx + 1 );

        if ( TXT_ATN == nTTyp )
        {
            sal_uInt16 nlTag = 0;
            for ( sal_uInt16 i = 0; i < nLen; ++i )
            {
                const WW8_Annotation& rAtn = *static_cast<const WW8_Annotation*>(m_aContent[i]);

                //aStrArr is sorted
                auto aIter = std::lower_bound(aStrArr.begin(),
                        aStrArr.end(), std::pair<OUString,OUString>(rAtn.msOwner,OUString()),
                        &lcl_AuthorComp);
                OSL_ENSURE(aIter != aStrArr.end() && aIter->first == rAtn.msOwner,
                        "Impossible");
                sal_uInt16 nFndPos = static_cast< sal_uInt16 >(aIter - aStrArr.begin());
                OUString sInitials( aIter->second );
                sal_uInt8 nInitialsLen = static_cast<sal_uInt8>(sInitials.getLength());
                if ( nInitialsLen > 9 )
                {
                    sInitials = sInitials.copy( 0, 9 );
                    nInitialsLen = 9;
                }

                // xstUsrInitl[ 10 ] pascal-style String holding initials
                // of annotation author
                SwWW8Writer::WriteShort(*rWrt.m_pTableStrm, nInitialsLen);
                SwWW8Writer::WriteString16(*rWrt.m_pTableStrm, sInitials,
                        false);
                SwWW8Writer::FillCount( *rWrt.m_pTableStrm,
                        (9 - nInitialsLen) * 2 );

                // documents layout of WriteShort's below:

                // SVBT16 ibst;      // index into GrpXstAtnOwners
                // SVBT16 ak;        // not used
                // SVBT16 grfbmc;    // not used
                // SVBT32 ITagBkmk;  // when not -1, this tag identifies the ATNBE

                SwWW8Writer::WriteShort( *rWrt.m_pTableStrm, nFndPos );
                SwWW8Writer::WriteShort( *rWrt.m_pTableStrm, 0 );
                SwWW8Writer::WriteShort( *rWrt.m_pTableStrm, 0 );
                if (rAtn.HasRange())
                {
                    SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, nlTag );
                    ++nlTag;
                }
                else
                    SwWW8Writer::WriteLong( *rWrt.m_pTableStrm, -1 );
            }
        }
        else
        {
            sal_uInt16 nNo = 0;
            for ( sal_uInt16 i = 0; i < nLen; ++i )             // write Flags
            {
                const SwFormatFootnote* pFootnote = static_cast<SwFormatFootnote const *>(m_aContent[ i ]);
                SwWW8Writer::WriteShort( *rWrt.m_pTableStrm,
                        !pFootnote->GetNumStr().isEmpty() ? 0 : ++nNo );
            }
        }
    }
    rRefStart = nFcStart;
    nFcStart = rWrt.m_pTableStrm->Tell();
    rRefCount = nFcStart - rRefStart;

    m_pTextPos->Write( *rWrt.m_pTableStrm );

    switch ( nTTyp )
    {
        case TXT_TXTBOX:
        case TXT_HFTXTBOX:
            for ( sal_uInt16 i = 0; i < nLen; ++i )
            {
                // write break descriptor (BKD)
                // short itxbxs
                SwWW8Writer::WriteShort( *rWrt.m_pTableStrm, i );
                // short dcpDepend
                SwWW8Writer::WriteShort( *rWrt.m_pTableStrm, 0 );
                // short flags : icol/fTableBreak/fColumnBreak/fMarked/
                //               fUnk/fTextOverflow
                SwWW8Writer::WriteShort( *rWrt.m_pTableStrm, 0x800 );
            }
            SwWW8Writer::FillCount( *rWrt.m_pTableStrm, 6 );
            break;
    }

    rTextStart = nFcStart;
    rTextCount = rWrt.m_pTableStrm->Tell() - nFcStart;
}

const std::vector<sal_uInt32>* WW8_WrPlcSubDoc::GetShapeIdArr() const
{
    return nullptr;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
