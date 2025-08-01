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

#include <hintids.hxx>
#include <tools/stream.hxx>
#include <comphelper/string.hxx>
#include <pam.hxx>
#include <doc.hxx>
#include <ndtxt.hxx>
#include <IDocumentRedlineAccess.hxx>
#include <redline.hxx>
#include "wrtasc.hxx"
#include <txatbase.hxx>
#include <txtfld.hxx>
#include <fmtftn.hxx>
#include <fmtfld.hxx>
#include <fldbas.hxx>
#include <ftninfo.hxx>
#include <numrule.hxx>

#include <algorithm>

/*
 * This file contains all output functions of the ASCII-Writer;
 * For all nodes, attributes, formats and chars.
 */

namespace {

class SwASC_AttrIter
{
    SwASCWriter& m_rWrt;
    const SwTextNode& m_rNd;
    sal_Int32 m_nCurrentSwPos;

    sal_Int32 SearchNext( sal_Int32 nStartPos );

public:
    SwASC_AttrIter( SwASCWriter& rWrt, const SwTextNode& rNd, sal_Int32 nStt );

    void NextPos() { m_nCurrentSwPos = SearchNext(m_nCurrentSwPos + 1); }

    sal_Int32 WhereNext() const { return m_nCurrentSwPos; }

    bool OutAttr( sal_Int32 nSwPos );
};

}

SwASC_AttrIter::SwASC_AttrIter(SwASCWriter& rWr, const SwTextNode& rTextNd, sal_Int32 nStt)
    : m_rWrt(rWr)
    , m_rNd(rTextNd)
    , m_nCurrentSwPos(0)
{
    m_nCurrentSwPos = SearchNext(nStt + 1);
}

sal_Int32 SwASC_AttrIter::SearchNext( sal_Int32 nStartPos )
{
    sal_Int32 nMinPos = SAL_MAX_INT32;
    const SwpHints* pTextAttrs = m_rNd.GetpSwpHints();
    if( pTextAttrs )
    {
        // TODO: This can be optimized, if we make use of the fact that the TextAttrs
        // are sorted by starting position. We would need to remember two indices, however.
        for ( size_t i = 0; i < pTextAttrs->Count(); ++i )
        {
            const SwTextAttr* pHt = pTextAttrs->Get(i);
            if ( pHt->HasDummyChar() )
            {
                sal_Int32 nPos = pHt->GetStart();

                if( nPos >= nStartPos && nPos <= nMinPos )
                    nMinPos = nPos;

                if( ( ++nPos ) >= nStartPos && nPos < nMinPos )
                    nMinPos = nPos;
            }
            else if ( pHt->HasContent() )
            {
                const sal_Int32 nHintStart = pHt->GetStart();
                if ( nHintStart >= nStartPos && nHintStart <= nMinPos )
                {
                    nMinPos = nHintStart;
                }

                const sal_Int32 nHintEnd = pHt->End() ? *pHt->End() : COMPLETE_STRING;
                if ( nHintEnd >= nStartPos && nHintEnd < nMinPos )
                {
                    nMinPos = nHintEnd;
                }
            }
        }
    }
    return nMinPos;
}

bool SwASC_AttrIter::OutAttr( sal_Int32 nSwPos )
{
    bool bRet = false;
    const SwpHints* pTextAttrs = m_rNd.GetpSwpHints();
    if( pTextAttrs )
    {
        for( size_t i = 0; i < pTextAttrs->Count(); ++i )
        {
            const SwTextAttr* pHt = pTextAttrs->Get(i);
            if ( ( pHt->HasDummyChar()
                   || pHt->HasContent() )
                 && nSwPos == pHt->GetStart() )
            {
                bRet = true;
                OUString sOut;
                switch( pHt->Which() )
                {
                case RES_TXTATR_FIELD:
                case RES_TXTATR_ANNOTATION:
                case RES_TXTATR_INPUTFIELD:
                    sOut = static_txtattr_cast<SwTextField const*>(pHt)
                            ->GetFormatField().GetField()->ExpandField(true, nullptr);
                    break;

                case RES_TXTATR_FTN:
                    {
                        const SwFormatFootnote& rFootnote = pHt->GetFootnote();
                        if( !rFootnote.GetNumStr().isEmpty() )
                            sOut = rFootnote.GetNumStr();
                        else if( rFootnote.IsEndNote() )
                            sOut = m_rWrt.m_pDoc->GetEndNoteInfo().m_aFormat.GetNumStr(
                                rFootnote.GetNumber());
                        else
                            sOut = m_rWrt.m_pDoc->GetFootnoteInfo().m_aFormat.GetNumStr(
                                rFootnote.GetNumber());
                    }
                    break;
                    case RES_TXTATR_LINEBREAK:
                    {
                        // Downgrade the clearing break to a simple linebreak.
                        sOut = OUStringChar(GetCharOfTextAttr(*pHt));
                        break;
                    }
                }
                if( !sOut.isEmpty() )
                    m_rWrt.Strm().WriteUnicodeOrByteText(sOut);
            }
            else if( nSwPos < pHt->GetStart() )
                break;
        }
    }
    return bRet;
}

namespace {

class SwASC_RedlineIter
{
private:
    SwTextNode const& m_rNode;
    IDocumentRedlineAccess const& m_rIDRA;
    SwRedlineTable::size_type m_nextRedline;

public:
    SwASC_RedlineIter(SwASCWriter const& rWriter, SwTextNode const& rNode)
        : m_rNode(rNode)
        , m_rIDRA(rNode.GetDoc().getIDocumentRedlineAccess())
        , m_nextRedline(rWriter.m_bHideDeleteRedlines
            ? m_rIDRA.GetRedlinePos(m_rNode, RedlineType::Delete)
            : SwRedlineTable::npos)
    {
    }

    bool CheckNodeDeleted()
    {
        if (m_nextRedline == SwRedlineTable::npos)
        {
            return false;
        }
        SwRangeRedline const*const pRedline(m_rIDRA.GetRedlineTable()[m_nextRedline]);
        return pRedline->Start()->GetNodeIndex() < m_rNode.GetIndex()
            && m_rNode.GetIndex() < pRedline->End()->GetNodeIndex();
    }

    std::pair<sal_Int32, sal_Int32> GetNextRedlineSkip()
    {
        sal_Int32 nRedlineStart(COMPLETE_STRING);
        sal_Int32 nRedlineEnd(COMPLETE_STRING);
        for ( ; m_nextRedline < m_rIDRA.GetRedlineTable().size(); ++m_nextRedline)
        {
            SwRangeRedline const*const pRedline(m_rIDRA.GetRedlineTable()[m_nextRedline]);
            if (pRedline->GetType() != RedlineType::Delete)
            {
                continue;
            }
            auto [pStart, pEnd] = pRedline->StartEnd(); // SwPosition*
            if (m_rNode.GetIndex() < pStart->GetNodeIndex())
            {
                m_nextRedline = SwRedlineTable::npos;
                break; // done
            }
            if (nRedlineStart == COMPLETE_STRING)
            {
                nRedlineStart = pStart->GetNodeIndex() == m_rNode.GetIndex()
                        ? pStart->GetContentIndex()
                        : 0;
            }
            else
            {
                if (pStart->GetContentIndex() != nRedlineEnd)
                {
                    assert(nRedlineEnd < pStart->GetContentIndex());
                    break; // no increment, revisit it next call
                }
            }
            nRedlineEnd = pEnd->GetNodeIndex() == m_rNode.GetIndex()
                    ? pEnd->GetContentIndex()
                    : COMPLETE_STRING;
        }
        return std::make_pair(nRedlineStart, nRedlineEnd);
    }
};

}

// Output of the node

static Writer& OutASC_SwTextNode( Writer& rWrt, SwContentNode& rNode )
{
    const SwTextNode& rNd = static_cast<SwTextNode&>(rNode);

    sal_Int32 nStrPos = rWrt.m_pCurrentPam->GetPoint()->GetContentIndex();
    const sal_Int32 nNodeEnd = rNd.Len();
    sal_Int32 nEnd = nNodeEnd;
    bool bLastNd =  rWrt.m_pCurrentPam->GetPoint()->GetNode() == rWrt.m_pCurrentPam->GetMark()->GetNode();
    if( bLastNd )
        nEnd = rWrt.m_pCurrentPam->GetMark()->GetContentIndex();

    bool bIsOneParagraph = rWrt.m_pOrigPam->Start()->GetNode() == rWrt.m_pOrigPam->End()->GetNode() && !getenv("SW_ASCII_COPY_NUMBERING");

    SwASC_AttrIter aAttrIter( static_cast<SwASCWriter&>(rWrt), rNd, nStrPos );
    SwASC_RedlineIter redlineIter(static_cast<SwASCWriter&>(rWrt), rNd);

    if (redlineIter.CheckNodeDeleted())
    {
        return rWrt;
    }

    const SwNumRule* pNumRule = rNd.GetNumRule();
    if (pNumRule && !nStrPos && rWrt.m_bExportParagraphNumbering && !bIsOneParagraph)
    {
        bool bIsOutlineNumRule = pNumRule == rNd.GetDoc().GetOutlineNumRule();
        OUString level;
        if (!bIsOutlineNumRule)
        {
            bool bSkipIndentationForHeadings = (rNd.GetLeftMarginWithNum() == 0)  //ensures the left margin of current node or numbering level is 0
                                               && (rNd.GetAttrOutlineLevel() > 0) //all heading styles have outline level > 0
                                               && (rNd.GetLeftMarginForTabCalculation() == 0); //ensures no indentation from tab stops is applied (left margin is 0)

            if(!bSkipIndentationForHeadings)
            {
                // indent each numbering level by 4 spaces
                for (int i = 0; i <= rNd.GetActualListLevel(); ++i)
                    level += "    ";
            }
        }
        // set up bullets or numbering
        OUString numString(rNd.GetNumString());
        if (numString.isEmpty() && !bIsOutlineNumRule)
        {
            if (rNd.HasBullet() && !rNd.HasVisibleNumberingOrBullet())
                numString = " ";
            else if (rNd.HasBullet())
                numString = OUString(numfunc::GetBulletChar(rNd.GetActualListLevel()));
            else if (!rNd.HasBullet() && !rNd.HasVisibleNumberingOrBullet())
                numString = "  ";
        }

        if (!level.isEmpty() || !numString.isEmpty())
            rWrt.Strm().WriteUnicodeOrByteText(Concat2View(level + numString + " "));
    }

    OUString aStr( rNd.GetText() );
    if( rWrt.m_bASCII_ParaAsBlank )
        aStr = aStr.replace(0x0A, ' ');

    const bool bExportSoftHyphens = RTL_TEXTENCODING_UCS2 == rWrt.GetAsciiOptions().GetCharSet() ||
                                    RTL_TEXTENCODING_UTF8 == rWrt.GetAsciiOptions().GetCharSet();

    std::pair<sal_Int32, sal_Int32> curRedline(redlineIter.GetNextRedlineSkip());
    for (;;) {
        const sal_Int32 nNextAttr = std::min(aAttrIter.WhereNext(), nEnd);

        bool isOutAttr(false);
        if (nStrPos < curRedline.first || curRedline.second <= nStrPos)
        {
            isOutAttr = aAttrIter.OutAttr(nStrPos);
        }

        if (!isOutAttr)
        {
            OUStringBuffer buf;
            while (true)
            {
                if (nNextAttr <= curRedline.first)
                {
                    buf.append(aStr.subView(nStrPos, nNextAttr - nStrPos));
                    break;
                }
                else if (nStrPos < curRedline.second)
                {
                    if (nStrPos < curRedline.first)
                    {
                        buf.append(aStr.subView(nStrPos, curRedline.first - nStrPos));
                    }
                    if (curRedline.second <= nNextAttr)
                    {
                        nStrPos = curRedline.second;
                        curRedline = redlineIter.GetNextRedlineSkip();
                    }
                    else
                    {
                        nStrPos = nNextAttr;
                        break;
                    }
                }
                else
                {
                    curRedline = redlineIter.GetNextRedlineSkip();
                }
            }
            OUString aOutStr(buf.makeStringAndClear());
            if ( !bExportSoftHyphens )
                aOutStr = aOutStr.replaceAll(OUStringChar(CHAR_SOFTHYPHEN), "");

            // all INWORD should be already removed by OutAttr
            // but the field-marks are not attributes so filter those
            static sal_Unicode const forbidden [] = {
                    CH_TXT_ATR_INPUTFIELDSTART,
                    CH_TXT_ATR_INPUTFIELDEND,
                    CH_TXT_ATR_FORMELEMENT,
                    CH_TXT_ATR_FIELDSTART,
                    CH_TXT_ATR_FIELDSEP,
                    CH_TXT_ATR_FIELDEND,
                    CH_TXTATR_BREAKWORD,
                    0
                };
            aOutStr = comphelper::string::removeAny(aOutStr, forbidden);

            rWrt.Strm().WriteUnicodeOrByteText( aOutStr );
        }
        nStrPos = nNextAttr;
        if (nStrPos >= nEnd)
        {
            break;
        }
        aAttrIter.NextPos();
    }

    if( !bLastNd ||
        ( ( !rWrt.m_bWriteClipboardDoc && !rWrt.m_bASCII_NoLastLineEnd )
            && !nStrPos && nEnd == nNodeEnd ) )
        rWrt.Strm().WriteUnicodeOrByteText( static_cast<SwASCWriter&>(rWrt).GetLineEnd());

    return rWrt;
}

/*
 * Create the table for the ASCII function pointers to the output
 * function.
 * There are local structures that only need to be known to the ASCII DLL.
 */

const SwNodeFnTab aASCNodeFnTab = {
/* RES_TXTNODE  */                   OutASC_SwTextNode,
/* RES_GRFNODE  */                   nullptr,
/* RES_OLENODE  */                   nullptr
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
