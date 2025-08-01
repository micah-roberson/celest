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

#include "epptbase.hxx"
#include "epptdef.hxx"
#include "../ppt/pptanimations.hxx"

#include <o3tl/any.hxx>
#include <vcl/outdev.hxx>
#include <rtl/ustring.hxx>
#include <rtl/strbuf.hxx>
#include <rtl/ustrbuf.hxx>
#include <sal/log.hxx>
#include <tools/UnitConversion.hxx>
#include <com/sun/star/awt/Rectangle.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/drawing/XMasterPageTarget.hpp>
#include <com/sun/star/drawing/XMasterPagesSupplier.hpp>
#include <com/sun/star/drawing/XDrawPagesSupplier.hpp>
#include <com/sun/star/drawing/XDrawPages.hpp>
#include <com/sun/star/animations/TransitionType.hpp>
#include <com/sun/star/animations/TransitionSubType.hpp>
#include <com/sun/star/awt/FontFamily.hpp>
#include <com/sun/star/awt/FontPitch.hpp>
#include <com/sun/star/container/XNamed.hpp>
#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/frame/XModel.hpp>
#include <com/sun/star/presentation/XPresentationPage.hpp>
#include <com/sun/star/text/XSimpleText.hpp>
#include <com/sun/star/style/XStyle.hpp>
#include <com/sun/star/style/XStyleFamiliesSupplier.hpp>
#include <com/sun/star/task/XStatusIndicator.hpp>
#include <unomodel.hxx>

using namespace com::sun::star;

using namespace ::com::sun::star::animations;
using namespace ::com::sun::star::awt::FontFamily;
using namespace ::com::sun::star::awt::FontPitch;
using namespace ::com::sun::star::presentation;

using ::com::sun::star::beans::XPropertySet;
using ::com::sun::star::container::XNameAccess;
using ::com::sun::star::container::XNamed;
using ::com::sun::star::drawing::XMasterPageTarget;
using ::com::sun::star::drawing::XDrawPage;
using ::com::sun::star::frame::XModel;
using ::com::sun::star::style::XStyleFamiliesSupplier;
using ::com::sun::star::style::XStyle;
using ::com::sun::star::task::XStatusIndicator;
using ::com::sun::star::text::XSimpleText;
using ::com::sun::star::uno::Any;
using ::com::sun::star::uno::Exception;
using ::com::sun::star::uno::Reference;
using ::com::sun::star::uno::UNO_QUERY;

PHLayout const pPHLayout[] =
{
    { EppLayout::TITLESLIDE,            { 0x0d, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x00, 0x0d, 0x10, true, true, false },
    { EppLayout::TITLEANDBODYSLIDE,     { 0x0d, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x00, 0x0d, 0x0e, true, true, false },
    { EppLayout::TITLEANDBODYSLIDE,     { 0x0d, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x14, 0x0d, 0x0e, true, true, false },
    { EppLayout::TWOCOLUMNSANDTITLE,    { 0x0d, 0x0e, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x00, 0x0d, 0x0e, true, true, true },
    { EppLayout::TWOCOLUMNSANDTITLE,    { 0x0d, 0x0e, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x14, 0x0d, 0x0e, true, true, false },
    { EppLayout::BLANKSLIDE,            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x00, 0x0d, 0x0e, false, false, false },
    { EppLayout::TWOCOLUMNSANDTITLE,    { 0x0d, 0x0e, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x16, 0x0d, 0x0e, true, true, false },
    { EppLayout::TWOCOLUMNSANDTITLE,    { 0x0d, 0x14, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x14, 0x0d, 0x0e, true, true, false },
    { EppLayout::TITLEANDBODYSLIDE,     { 0x0d, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x15, 0x0d, 0x0e, true, false, false },
    { EppLayout::TWOCOLUMNSANDTITLE,    { 0x0d, 0x16, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x16, 0x0d, 0x0e, true, true, false },
    { EppLayout::TWOCOLUMNSANDTITLE,    { 0x0d, 0x0e, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x13, 0x0d, 0x0e, true, true, false },
    { EppLayout::TITLEANDBODYSLIDE,     { 0x0d, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x13, 0x0d, 0x0e, true, false, false },
    { EppLayout::RIGHTCOLUMN2ROWS,      { 0x0d, 0x0e, 0x13, 0x13, 0x00, 0x00, 0x00, 0x00 }, 0x13, 0x0d, 0x0e, true, true, false },
    { EppLayout::TWOCOLUMNSANDTITLE,    { 0x0d, 0x13, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x13, 0x0d, 0x0e, true, true, false },
    { EppLayout::TWOROWSANDTITLE,       { 0x0d, 0x13, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x13, 0x0d, 0x0e, true, true, false },
    { EppLayout::LEFTCOLUMN2ROWS,       { 0x0d, 0x13, 0x13, 0x0e, 0x00, 0x00, 0x00, 0x00 }, 0x13, 0x0d, 0x0e, true, true, false },
    { EppLayout::TOPROW2COLUMN,         { 0x0d, 0x13, 0x13, 0x0e, 0x00, 0x00, 0x00, 0x00 }, 0x13, 0x0d, 0x0e, true, true, false },
    { EppLayout::TWOROWSANDTITLE,       { 0x0d, 0x0e, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x13, 0x0d, 0x0e, true, true, false },
    { EppLayout::FOUROBJECTS,           { 0x0d, 0x13, 0x13, 0x13, 0x13, 0x00, 0x00, 0x00 }, 0x13, 0x0d, 0x0e, true, false, false },
    { EppLayout::ONLYTITLE,             { 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x00, 0x0d, 0x0e, true, false, false },
    { EppLayout::BLANKSLIDE,            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x00, 0x0d, 0x0e, false, false, false },
    { EppLayout::TITLERIGHT2BODIESLEFT, { 0x11, 0x12, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x14, 0x11, 0x12, true, true, false },
    { EppLayout::TITLERIGHTBODYLEFT,    { 0x11, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x00, 0x11, 0x12, true, true, false },
    { EppLayout::TITLEANDBODYSLIDE,     { 0x0d, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x00, 0x0d, 0x12, true, true, false },
    { EppLayout::TWOCOLUMNSANDTITLE,    { 0x0d, 0x16, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x16, 0x0d, 0x12, true, true, false }
};

PPTWriterBase::PPTWriterBase()
    : mbStatusIndicator(false)
    , mbPresObj(false)
    , mbEmptyPresObj(false)
    , mbIsBackgroundDark(false)
    , mnAngle(0)
    , mnPages(0)
    , mnMasterPages(0)
    , maFraction(1, 576)
    , maMapModeSrc(MapUnit::Map100thMM)
    , maMapModeDest(MapUnit::MapInch, Point(), maFraction, maFraction)
    , meLatestPageType(NORMAL)
    , mpStyleSheet(nullptr)
{
    SAL_INFO("sd.eppt", "PPTWriterBase::PPTWriterBase()");
}

PPTWriterBase::PPTWriterBase( const rtl::Reference< SdXImpressDocument > & rXModel,
                              const Reference< XStatusIndicator > & rXStatInd )
    : mXModel(rXModel)
    , mXStatusIndicator(rXStatInd)
    , mbStatusIndicator(false)
    , mbPresObj(false)
    , mbEmptyPresObj(false)
    , mbIsBackgroundDark(false)
    , mnAngle(0)
    , mnPages(0)
    , mnMasterPages(0)
    , maFraction(1, 576)
    , maMapModeSrc(MapUnit::Map100thMM)
    , maMapModeDest(MapUnit::MapInch, Point(), maFraction, maFraction)
    , meLatestPageType (NORMAL)
    , mpStyleSheet(nullptr)
{
}

PPTWriterBase::~PPTWriterBase()
{
    // Possibly unnecessary sanity check for mXStatusIndicator.is().
    // In 3.3 we had a bug report of a crash where it was null,
    // https://bugzilla.novell.com/show_bug.cgi?id=694119 (non-public,
    // bug report, sorry).
    if ( mbStatusIndicator && mXStatusIndicator.is() )
        mXStatusIndicator->end();
}

void PPTWriterBase::exportPPT( const std::vector< css::beans::PropertyValue >& rMediaData )
{
    if ( !InitSOIface() )
        return;

    FontCollectionEntry aDefaultFontDesc( u"Times New Roman"_ustr,
                                          ROMAN,
                                          awt::FontPitch::VARIABLE,
                                                    RTL_TEXTENCODING_MS_1252 );
    maFontCollection.GetId( aDefaultFontDesc ); // default is always times new roman

    if ( !GetPageByIndex( 0, NOTICE ) )
        return;

    sal_Int32 nWidth = 21000;
    if ( ImplGetPropertyValue( mXPagePropSet, u"Width"_ustr ) )
        mAny >>= nWidth;
    sal_Int32 nHeight = 29700;
    if ( ImplGetPropertyValue( mXPagePropSet, u"Height"_ustr ) )
        mAny >>= nHeight;

    maNotesPageSize = MapSize( awt::Size( nWidth, nHeight ) );

    if ( !GetPageByIndex( 0, MASTER ) )
        return;

    nWidth = 28000;
    if ( ImplGetPropertyValue( mXPagePropSet, u"Width"_ustr ) )
        mAny >>= nWidth;
    nHeight = 21000;
    if ( ImplGetPropertyValue( mXPagePropSet, u"Height"_ustr ) )
        mAny >>= nHeight;
    maDestPageSize = MapSize( awt::Size( nWidth, nHeight ) );
    maPageSize = awt::Size(nWidth, nHeight);

    SAL_INFO("sd.eppt", "call exportDocumentPre()");
    exportPPTPre(rMediaData);

    if ( !GetStyleSheets() )
        return;

    if ( !ImplCreateDocument() )
         return;

    sal_uInt32 i;

    for ( i = 0; i < mnMasterPages; i++ )
    {
        if ( !CreateSlideMaster( i ) )
            return;
    }
    if ( !CreateMainNotes() )
        return;

    for ( i = 0; i < mnPages; i++ )
    {
        SAL_INFO("sd.eppt", "call ImplCreateSlide( " << i << " )");
        if ( !CreateSlide( i ) )
            return;
    }

    for ( i = 0; i < mnPages; i++ )
    {
        if ( !CreateNotes( i ) )
            return;
    }

    SAL_INFO("sd.eppt", "call exportDocumentPost()");
    exportPPTPost();
}

bool PPTWriterBase::InitSOIface()
{
    while( true )
    {
        mXDrawPages = mXModel->getMasterPages();
        if ( !mXDrawPages.is() )
            break;
        mnMasterPages = mXDrawPages->getCount();
        mXDrawPages = mXModel->getDrawPages();
        if( !mXDrawPages.is() )
            break;
        mnPages =  mXDrawPages->getCount();
        if ( !GetPageByIndex( 0, NORMAL ) )
            break;

        return true;
    }
    return false;
}

bool PPTWriterBase::GetPageByIndex( sal_uInt32 nIndex, PageType ePageType )
{
    while( true )
    {
        if ( ePageType != meLatestPageType )
        {
            switch( ePageType )
            {
                case NORMAL :
                case NOTICE :
                {
                    mXDrawPages = mXModel->getDrawPages();
                    if( !mXDrawPages.is() )
                        return false;
                }
                break;

                case MASTER :
                {
                    mXDrawPages = mXModel->getMasterPages();
                    if( !mXDrawPages.is() )
                        return false;
                }
                break;
                default:
                    break;
            }
            meLatestPageType = ePageType;
        }
        Any aAny( mXDrawPages->getByIndex( nIndex ) );
        aAny >>= mXDrawPage;
        if ( !mXDrawPage.is() )
            break;
        if ( ePageType == NOTICE )
        {
            Reference< XPresentationPage > aXPresentationPage( mXDrawPage, UNO_QUERY );
            if ( !aXPresentationPage.is() )
                break;
            mXDrawPage = aXPresentationPage->getNotesPage();
            if ( !mXDrawPage.is() )
                break;
        }
        mXPagePropSet.set( mXDrawPage, UNO_QUERY );
        if ( !mXPagePropSet.is() )
            break;

        if (GetPropertyValue( aAny, mXPagePropSet, u"IsBackgroundDark"_ustr ) )
            aAny >>= mbIsBackgroundDark;

        mXShapes = mXDrawPage;
        if ( !mXShapes.is() )
            break;

        /* try to get the "real" background PropertySet. If the normal page is not supporting this property, it is
           taken the property from the master */
        bool bHasBackground = GetPropertyValue( aAny, mXPagePropSet, u"Background"_ustr, true );
        if ( bHasBackground )
            bHasBackground = ( aAny >>= mXBackgroundPropSet );
        if ( !bHasBackground )
        {
            Reference< XMasterPageTarget > aXMasterPageTarget( mXDrawPage, UNO_QUERY );
            if ( aXMasterPageTarget.is() )
            {
                Reference< XDrawPage > aXMasterDrawPage = aXMasterPageTarget->getMasterPage();
                if ( aXMasterDrawPage.is() )
                {
                    Reference< XPropertySet > aXMasterPagePropSet;
                    aXMasterPagePropSet.set( aXMasterDrawPage, UNO_QUERY );
                    if ( aXMasterPagePropSet.is() )
                    {
                        bool bBackground = GetPropertyValue( aAny, aXMasterPagePropSet, u"Background"_ustr );
                        if ( bBackground )
                        {
                            aAny >>= mXBackgroundPropSet;
                        }
                    }
                }
            }
        }
        return true;
    }
    return false;
}

bool PPTWriterBase::CreateSlide( sal_uInt32 nPageNum )
{
    Any aAny;

    if ( !GetPageByIndex( nPageNum, NORMAL ) )
        return false;

    sal_uInt32 nMasterNum = GetMasterIndex( NORMAL );
    SetCurrentStyleSheet( nMasterNum );

    Reference< XPropertySet > aXBackgroundPropSet;
    bool bHasBackground = GetPropertyValue( aAny, mXPagePropSet, u"Background"_ustr );
    if ( bHasBackground )
        bHasBackground = ( aAny >>= aXBackgroundPropSet );

    sal_uInt16 nMode = 7;   // Bit 1: Follow master objects, Bit 2: Follow master scheme, Bit 3: Follow master background
    if ( bHasBackground )
        nMode &=~4;

/* sj: Don't know what's IsBackgroundVisible for, have to ask cl
    if ( GetPropertyValue( aAny, mXPagePropSet, OUString( "IsBackgroundVisible" ) ) )
    {
        bool bBackgroundVisible;
        if ( aAny >>= bBackgroundVisible )
        {
            if ( bBackgroundVisible )
                nMode &= ~4;
        }
    }
*/
    if ( GetPropertyValue( aAny, mXPagePropSet, u"IsBackgroundObjectsVisible"_ustr ) )
    {
        bool bBackgroundObjectsVisible = false;
        if ( aAny >>= bBackgroundObjectsVisible )
        {
            if ( !bBackgroundObjectsVisible )
                nMode &= ~1;
        }
    }

    ImplWriteSlide( nPageNum, nMasterNum, nMode, bHasBackground, aXBackgroundPropSet );

    return true;
};

bool PPTWriterBase::CreateNotes( sal_uInt32 nPageNum )
{
    if ( !GetPageByIndex( nPageNum, NOTICE ) )
        return false;
    SetCurrentStyleSheet( GetMasterIndex( NORMAL ) );

    ImplWriteNotes( nPageNum );

    return true;
};

bool PPTWriterBase::CreateSlideMaster( sal_uInt32 nPageNum )
{
    if ( !GetPageByIndex( nPageNum, MASTER ) )
        return false;
    SetCurrentStyleSheet( nPageNum );

    css::uno::Reference< css::beans::XPropertySet > aXBackgroundPropSet;
    if (ImplGetPropertyValue(mXPagePropSet, u"Background"_ustr))                // load background shape
        mAny >>= aXBackgroundPropSet;

    ImplWriteSlideMaster( nPageNum, aXBackgroundPropSet );

    return true;
};

sal_Int32 PPTWriterBase::GetLayoutOffset( const css::uno::Reference< css::beans::XPropertySet >& rXPropSet )
{
    css::uno::Any aAny;
    sal_Int32 nLayout = 20;
    if ( GetPropertyValue( aAny, rXPropSet, u"Layout"_ustr, true ) )
        aAny >>= nLayout;

    SAL_INFO("sd.eppt", "GetLayoutOffset " << nLayout);

    return nLayout;
}

sal_Int32 PPTWriterBase::GetLayoutOffsetFixed( const css::uno::Reference< css::beans::XPropertySet >& rXPropSet )
{
    sal_Int32 nLayout = GetLayoutOffset( rXPropSet );

    if ( ( nLayout >= 21 ) && ( nLayout <= 26 ) )   // NOTES _> HANDOUT6
        nLayout = 20;
    if ( ( nLayout >= 27 ) && ( nLayout <= 30 ) )   // VERTICAL LAYOUT
        nLayout -= 6;
    else if ( nLayout > 30 )
        nLayout = 20;

    return nLayout;
}

PHLayout const & PPTWriterBase::GetLayout(  const css::uno::Reference< css::beans::XPropertySet >& rXPropSet )
{
    return pPHLayout[ GetLayoutOffsetFixed( rXPropSet ) ];
}

PHLayout const & PPTWriterBase::GetLayout( sal_Int32 nOffset )
{
    if( nOffset >= 0 && nOffset < EPP_LAYOUT_SIZE )
        return pPHLayout[ nOffset ];

    SAL_INFO("sd.eppt", "asked " << nOffset << " for layout outside of 0, " << EPP_LAYOUT_SIZE  << " array scope");

    return pPHLayout[ 0 ];
}

sal_uInt32 PPTWriterBase::GetMasterIndex( PageType ePageType )
{
    sal_uInt32 nRetValue = 0;
    css::uno::Reference< css::drawing::XMasterPageTarget >aXMasterPageTarget( mXDrawPage, css::uno::UNO_QUERY );

    if ( aXMasterPageTarget.is() )
    {
        css::uno::Reference< css::drawing::XDrawPage >aXDrawPage = aXMasterPageTarget->getMasterPage();
        if ( aXDrawPage.is() )
        {
            css::uno::Reference< css::beans::XPropertySet > aXPropertySet( aXDrawPage, css::uno::UNO_QUERY );
            if ( aXPropertySet.is() )
            {
                if ( ImplGetPropertyValue( aXPropertySet, u"Number"_ustr ) )
                    nRetValue |= *o3tl::doAccess<sal_Int16>(mAny);
                if ( nRetValue & 0xffff )           // avoid overflow
                    nRetValue--;
            }
        }
    }
    if ( ePageType == NOTICE )
        nRetValue += mnMasterPages;
    return nRetValue;
}

void PPTWriterBase::SetCurrentStyleSheet( sal_uInt32 nPageNum )
{
    if ( nPageNum >= maStyleSheetList.size() )
        nPageNum = 0;
    mpStyleSheet = maStyleSheetList[ nPageNum ].get();
}

bool PPTWriterBase::GetStyleSheets()
{
    int             nInstance, nLevel;
    bool            bRetValue = false;
    sal_uInt32      nPageNum;

    for ( nPageNum = 0; nPageNum < mnMasterPages; nPageNum++ )
    {
        Reference< XNamed >
            aXNamed;

        Reference< XNameAccess >
            aXNameAccess;

        sal_uInt16 nDefaultTab = ( mXModel.is() && ImplGetPropertyValue( mXModel, u"TabStop"_ustr ) )
            ? static_cast<sal_uInt16>( convertMm100ToMasterUnit(*o3tl::doAccess<sal_Int32>(mAny)) )
            : 1250;

        maStyleSheetList.emplace_back( new PPTExStyleSheet( nDefaultTab, dynamic_cast<PPTExBulletProvider*>(this) ) );
        SetCurrentStyleSheet( nPageNum );
        if ( GetPageByIndex( nPageNum, MASTER ) )
            aXNamed.set( mXDrawPage, UNO_QUERY );

        aXNameAccess = mXModel->getStyleFamilies();

        bRetValue = aXNamed.is() && aXNameAccess.is();
        if  ( bRetValue )
        {
            for ( nInstance = EPP_TEXTTYPE_Title; nInstance <= EPP_TEXTTYPE_CenterTitle; nInstance++ )
            {
                OUString aStyle;
                OUString aFamily;
                switch ( nInstance )
                {
                    case EPP_TEXTTYPE_CenterTitle :
                    case EPP_TEXTTYPE_Title :
                    {
                        aStyle = "title";
                        aFamily = aXNamed->getName();
                    }
                    break;
                    case EPP_TEXTTYPE_Body :
                    {
                        aStyle = "outline1";      // SD_LT_SEPARATOR
                        aFamily = aXNamed->getName();
                    }
                    break;
                    case EPP_TEXTTYPE_Other :
                    {
                        aStyle = "standard";
                        aFamily = "graphics";
                    }
                    break;
                    case EPP_TEXTTYPE_CenterBody :
                    {
                        aStyle = "subtitle";
                        aFamily = aXNamed->getName();
                    }
                    break;
                }
                if ( !aStyle.isEmpty() && !aFamily.isEmpty() )
                {
                    try
                    {
                        Reference< XNameAccess >xNameAccess;
                        if ( aXNameAccess->hasByName( aFamily ) )
                        {
                            Any aAny( aXNameAccess->getByName( aFamily ) );
                            xNameAccess.set(aAny, css::uno::UNO_QUERY);
                            if( xNameAccess.is() )
                            {
                                Reference< XNameAccess > aXFamily;
                                if ( aAny >>= aXFamily )
                                {
                                    if ( aXFamily->hasByName( aStyle ) )
                                    {
                                        aAny = aXFamily->getByName( aStyle );
                                        Reference< XStyle > xStyle(
                                            aAny, css::uno::UNO_QUERY);
                                        if( xStyle.is() )
                                        {
                                            Reference< XStyle > aXStyle;
                                            aAny >>= aXStyle;
                                            Reference< XPropertySet >
                                                xPropSet( aXStyle, UNO_QUERY );
                                            if( xPropSet.is() )
                                                mpStyleSheet->SetStyleSheet( xPropSet, maFontCollection, nInstance, 0 );
                                            for ( nLevel = 1; nLevel < 5; nLevel++ )
                                            {
                                                if ( nInstance == EPP_TEXTTYPE_Body )
                                                {
                                                    sal_Unicode cTemp = aStyle[aStyle.getLength() - 1];
                                                    aStyle = aStyle.subView(0, aStyle.getLength() - 1) + OUStringChar(++cTemp);
                                                    if ( aXFamily->hasByName( aStyle ) )
                                                    {
                                                        aXFamily->getByName( aStyle ) >>= xStyle;
                                                        if( xStyle.is() )
                                                        {
                                                            Reference< XPropertySet >
                                                                xPropertySet( xStyle, UNO_QUERY );
                                                            if ( xPropertySet.is() )
                                                                mpStyleSheet->SetStyleSheet( xPropertySet, maFontCollection, nInstance, nLevel );
                                                        }
                                                    }
                                                }
                                                else
                                                    mpStyleSheet->SetStyleSheet( xPropSet, maFontCollection, nInstance, nLevel );
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    catch( Exception& )
                    {

                    }
                }
            }
            for ( ; nInstance <= EPP_TEXTTYPE_QuarterBody; nInstance++ )
            {

            }
        }
    }
    return bRetValue;
}

bool PPTWriterBase::CreateMainNotes()
{
    if ( !GetPageByIndex( 0, NOTICE ) )
        return false;
    SetCurrentStyleSheet( 0 );

    css::uno::Reference< css::drawing::XMasterPageTarget > aXMasterPageTarget( mXDrawPage, css::uno::UNO_QUERY );

    if ( !aXMasterPageTarget.is() )
        return false;

    mXDrawPage = aXMasterPageTarget->getMasterPage();
    if ( !mXDrawPage.is() )
        return false;

    mXPropSet.set( mXDrawPage, css::uno::UNO_QUERY );
    if ( !mXPropSet.is() )
        return false;

    mXShapes = mXDrawPage;
    if ( !mXShapes.is() )
        return false;

    return ImplCreateMainNotes();
}

awt::Size PPTWriterBase::MapSize( const awt::Size& rSize )
{
    Size aRetSize( OutputDevice::LogicToLogic( Size( rSize.Width, rSize.Height ), maMapModeSrc, maMapModeDest ) );

    if ( !aRetSize.Width() )
        aRetSize.AdjustWidth( 1 );
    if ( !aRetSize.Height() )
        aRetSize.AdjustHeight( 1 );
    return awt::Size( aRetSize.Width(), aRetSize.Height() );
}

awt::Point PPTWriterBase::MapPoint( const awt::Point& rPoint )
{
    Point aRet( OutputDevice::LogicToLogic( Point( rPoint.X, rPoint.Y ), maMapModeSrc, maMapModeDest ) );
    return awt::Point( aRet.X(), aRet.Y() );
}

::tools::Rectangle PPTWriterBase::MapRectangle( const awt::Rectangle& rRect )
{
    css::awt::Point    aPoint( rRect.X, rRect.Y );
    css::awt::Size     aSize( rRect.Width, rRect.Height );
    css::awt::Point    aP( MapPoint( aPoint ) );
    css::awt::Size     aS( MapSize( aSize ) );
    return ::tools::Rectangle( Point( aP.X, aP.Y ), Size( aS.Width, aS.Height ) );
}

bool PPTWriterBase::GetShapeByIndex( sal_uInt32 nIndex, bool bGroup )
{
    while(true)
    {
        if ( !bGroup || ( GetCurrentGroupLevel() == 0 ) )
        {
            Any aAny( mXShapes->getByIndex( nIndex ) );
            aAny >>= mXShape;
        }
        else
        {
            Any aAny( GetCurrentGroupAccess()->getByIndex( GetCurrentGroupIndex() ) );
            aAny >>= mXShape;
        }
        if ( !mXShape.is() )
            break;

        Any aAny( mXShape->queryInterface( cppu::UnoType<XPropertySet>::get()));
        aAny >>= mXPropSet;

        if ( !mXPropSet.is() )
            break;
        maPosition = MapPoint( mXShape->getPosition() );
        maSize = MapSize( mXShape->getSize() );
        maRect = ::tools::Rectangle( Point( maPosition.X, maPosition.Y ), Size( maSize.Width, maSize.Height ) );

        OStringBuffer aTypeBuffer(OUStringToOString(
            mXShape->getShapeType(), RTL_TEXTENCODING_UTF8));
        // remove "com.sun.star."
        aTypeBuffer.remove(0, RTL_CONSTASCII_LENGTH("com.sun.star."));

        sal_Int32 nPos = aTypeBuffer.toString().indexOf("Shape");
        aTypeBuffer.remove(nPos, RTL_CONSTASCII_LENGTH("Shape"));
        mType = aTypeBuffer.makeStringAndClear();

        mbPresObj = mbEmptyPresObj = false;
        if ( ImplGetPropertyValue( u"IsPresentationObject"_ustr ) )
            mAny >>= mbPresObj;

        if ( mbPresObj && ImplGetPropertyValue( u"IsEmptyPresentationObject"_ustr ) )
            mAny >>= mbEmptyPresObj;

        mnAngle = ( PropValue::GetPropertyValue( aAny,
            mXPropSet, u"RotateAngle"_ustr, true ) )
                ? *o3tl::doAccess<sal_Int32>(aAny)
                : 0;

        return true;
    }
    return false;
}

sal_Int8 PPTWriterBase::GetTransition( sal_Int16 nTransitionType, sal_Int16 nTransitionSubtype, FadeEffect eEffect,
    sal_Int32 nTransitionFadeColor, sal_uInt8& nDirection )
{
    sal_Int8 nPPTTransitionType = 0;
    nDirection = 0;

    switch( nTransitionType )
    {
    case TransitionType::FADE :
    {
        if ( nTransitionSubtype == TransitionSubType::CROSSFADE )
            nPPTTransitionType = PPT_TRANSITION_TYPE_SMOOTHFADE;
        else if ( nTransitionSubtype == TransitionSubType::FADEOVERCOLOR )
        {
            if( nTransitionFadeColor == static_cast<sal_Int32>(COL_WHITE) )
                nPPTTransitionType = PPT_TRANSITION_TYPE_FLASH;
            else
                nPPTTransitionType = PPT_TRANSITION_TYPE_FADE;
        }
    }
    break;
    case TransitionType::PUSHWIPE :
    {
        if (nTransitionSubtype == TransitionSubType::COMBVERTICAL ||
            nTransitionSubtype == TransitionSubType::COMBHORIZONTAL)
        {
            nPPTTransitionType = PPT_TRANSITION_TYPE_COMB;
        }
        else
        {
            nPPTTransitionType = PPT_TRANSITION_TYPE_PUSH;
        }
        switch (nTransitionSubtype)
        {
            case TransitionSubType::FROMRIGHT: nDirection = 0; break;
            case TransitionSubType::FROMBOTTOM: nDirection = 1; break;
            case TransitionSubType::FROMLEFT: nDirection = 2; break;
            case TransitionSubType::FROMTOP: nDirection = 3; break;
            case TransitionSubType::COMBHORIZONTAL: nDirection = 0; break;
            case TransitionSubType::COMBVERTICAL: nDirection = 1; break;
        }
    }
    break;
    case TransitionType::PINWHEELWIPE :
    {
        nPPTTransitionType = PPT_TRANSITION_TYPE_WHEEL;
        switch( nTransitionSubtype )
        {
        case TransitionSubType::ONEBLADE: nDirection = 1; break;
        case TransitionSubType::TWOBLADEVERTICAL : nDirection = 2; break;
        case TransitionSubType::THREEBLADE : nDirection = 3; break;
        case TransitionSubType::FOURBLADE: nDirection = 4; break;
        case TransitionSubType::EIGHTBLADE: nDirection = 8; break;
        }
    }
    break;
    case TransitionType::FANWIPE :
    {
        nPPTTransitionType = PPT_TRANSITION_TYPE_WEDGE;
    }
    break;
    case TransitionType::ELLIPSEWIPE :
    {
        switch( nTransitionSubtype ) {
        case TransitionSubType::VERTICAL:
        case TransitionSubType::HORIZONTAL:
            // no ellipse or oval in PPT or OOXML, fallback to circle
        default:
            nPPTTransitionType = PPT_TRANSITION_TYPE_CIRCLE;
        }
    }
    break;
    case TransitionType::FOURBOXWIPE :
    {
        nPPTTransitionType = PPT_TRANSITION_TYPE_PLUS;
    }
    break;
    case TransitionType::IRISWIPE :
    {
        switch( nTransitionSubtype ) {
        case TransitionSubType::RECTANGLE:
            nPPTTransitionType = PPT_TRANSITION_TYPE_ZOOM;
            nDirection = (eEffect == FadeEffect_FADE_FROM_CENTER) ? 0 : 1;
            break;
        default:
            nPPTTransitionType = PPT_TRANSITION_TYPE_DIAMOND;
            break;
        }
    }
    break;
    case TransitionType::ZOOM:
    {
        switch(nTransitionSubtype)
        {
        case TransitionSubType::ROTATEIN:
            nPPTTransitionType = PPT_TRANSITION_TYPE_NEWSFLASH;
            break;
        default:
            break;
        }
    }
    break;
    }

    return nPPTTransitionType;
}

sal_Int8 PPTWriterBase::GetTransition( FadeEffect eEffect, sal_uInt8& nDirection )
{
    sal_Int8 nPPTTransitionType = 0;

    switch ( eEffect )
    {
    default :
    case FadeEffect_RANDOM :
        nPPTTransitionType = PPT_TRANSITION_TYPE_RANDOM;
        break;

    case FadeEffect_HORIZONTAL_STRIPES :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_VERTICAL_STRIPES :
        nPPTTransitionType = PPT_TRANSITION_TYPE_BLINDS;
        break;

    case FadeEffect_VERTICAL_CHECKERBOARD :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_HORIZONTAL_CHECKERBOARD :
        nPPTTransitionType = PPT_TRANSITION_TYPE_CHECKER;
        break;

    case FadeEffect_MOVE_FROM_UPPERLEFT :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_MOVE_FROM_UPPERRIGHT :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_MOVE_FROM_LOWERLEFT :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_MOVE_FROM_LOWERRIGHT :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_MOVE_FROM_TOP :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_MOVE_FROM_LEFT :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_MOVE_FROM_BOTTOM :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_MOVE_FROM_RIGHT :
        nPPTTransitionType = PPT_TRANSITION_TYPE_COVER;
        break;

    case FadeEffect_DISSOLVE :
        nPPTTransitionType = PPT_TRANSITION_TYPE_DISSOLVE;
        break;

    case FadeEffect_VERTICAL_LINES :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_HORIZONTAL_LINES :
        nPPTTransitionType = PPT_TRANSITION_TYPE_RANDOM_BARS;
        break;

    case FadeEffect_CLOSE_HORIZONTAL :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_OPEN_HORIZONTAL :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_CLOSE_VERTICAL :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_OPEN_VERTICAL :
        nPPTTransitionType = PPT_TRANSITION_TYPE_SPLIT;
        break;

    case FadeEffect_FADE_FROM_UPPERLEFT :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_FADE_FROM_UPPERRIGHT :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_FADE_FROM_LOWERLEFT :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_FADE_FROM_LOWERRIGHT :
        nDirection += 4;
        nPPTTransitionType = PPT_TRANSITION_TYPE_STRIPS;
        break;

    case FadeEffect_UNCOVER_TO_LOWERRIGHT :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_UNCOVER_TO_LOWERLEFT :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_UNCOVER_TO_UPPERRIGHT :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_UNCOVER_TO_UPPERLEFT :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_UNCOVER_TO_BOTTOM :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_UNCOVER_TO_RIGHT :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_UNCOVER_TO_TOP :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_UNCOVER_TO_LEFT :
        nPPTTransitionType = PPT_TRANSITION_TYPE_PULL;
        break;

    case FadeEffect_FADE_FROM_TOP :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_FADE_FROM_LEFT :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_FADE_FROM_BOTTOM :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_FADE_FROM_RIGHT :
    case FadeEffect_ROLL_FROM_RIGHT :
        nPPTTransitionType = PPT_TRANSITION_TYPE_WIPE;
        break;

    case FadeEffect_ROLL_FROM_TOP :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_ROLL_FROM_LEFT :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_ROLL_FROM_BOTTOM :
        nDirection++;
        [[fallthrough]];

    case FadeEffect_FADE_TO_CENTER :
        nDirection++;
        [[fallthrough]];
    case FadeEffect_FADE_FROM_CENTER :
        nPPTTransitionType = PPT_TRANSITION_TYPE_ZOOM;
        break;

    case FadeEffect_NONE :
        nDirection = 2;
        break;
    }

    return nPPTTransitionType;
}

bool PPTWriterBase::ContainsOtherShapeThanPlaceholders()
{
    sal_uInt32 nShapes = mXShapes->getCount();
    bool bOtherThanPlaceHolders = false;

    if ( nShapes )
        for ( sal_uInt32 nIndex = 0; ( nIndex < nShapes ) && !bOtherThanPlaceHolders; nIndex++ )
        {
            if ( GetShapeByIndex( nIndex, false ) && mType != "drawing.Page" )
            {
                if( mType == "presentation.Page" || mType == "presentation.Notes" )
                {
                    Reference< XSimpleText > rXText( mXShape, UNO_QUERY );

                    if( rXText.is() && !rXText->getString().isEmpty() )
                        bOtherThanPlaceHolders = true;
                }
                else
                    bOtherThanPlaceHolders = true;
            }
            SAL_INFO("sd.eppt", "mType == " << mType);
        }

    return bOtherThanPlaceHolders;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
