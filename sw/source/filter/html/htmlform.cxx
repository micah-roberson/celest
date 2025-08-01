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

#include <sal/config.h>

#include <string_view>

#include <hintids.hxx>
#include <comphelper/documentinfo.hxx>
#include <comphelper/string.hxx>
#include <utility>
#include <vcl/svapp.hxx>
#include <tools/UnitConversion.hxx>

#include <o3tl/string_view.hxx>
#include <comphelper/diagnose_ex.hxx>
#include <vcl/unohelp.hxx>
#include <svtools/htmlkywd.hxx>
#include <svtools/htmltokn.h>
#include <svl/urihelper.hxx>
#include <sfx2/docfile.hxx>
#include <sfx2/event.hxx>
#include <sfx2/sfxsids.hrc>
#include <sfx2/viewfrm.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/ulspitem.hxx>
#include <editeng/brushitem.hxx>
#include <editeng/colritem.hxx>
#include <editeng/fontitem.hxx>
#include <editeng/fhgtitem.hxx>
#include <editeng/wghtitem.hxx>
#include <editeng/postitem.hxx>
#include <editeng/udlnitem.hxx>
#include <editeng/crossedoutitem.hxx>
#include <svx/svdouno.hxx>
#include <cppuhelper/implbase.hxx>
#include <com/sun/star/form/ListSourceType.hpp>
#include <com/sun/star/form/FormButtonType.hpp>
#include <com/sun/star/form/FormSubmitEncoding.hpp>
#include <com/sun/star/form/FormSubmitMethod.hpp>
#include <com/sun/star/drawing/XDrawPageSupplier.hpp>
#include <com/sun/star/script/XEventAttacherManager.hpp>
#include <com/sun/star/text/WrapTextMode.hpp>
#include <com/sun/star/text/HoriOrientation.hpp>
#include <com/sun/star/text/VertOrientation.hpp>
#include <com/sun/star/text/TextContentAnchorType.hpp>
#include <com/sun/star/container/XIndexContainer.hpp>
#include <com/sun/star/drawing/XControlShape.hpp>
#include <com/sun/star/awt/XTextLayoutConstrains.hpp>
#include <com/sun/star/awt/XLayoutConstrains.hpp>
#include <com/sun/star/awt/XImageConsumer.hpp>
#include <com/sun/star/awt/ImageStatus.hpp>
#include <com/sun/star/form/XImageProducerSupplier.hpp>
#include <com/sun/star/form/XForm.hpp>
#include <doc.hxx>
#include <IDocumentLayoutAccess.hxx>
#include <IDocumentUndoRedo.hxx>
#include <pam.hxx>
#include <swtable.hxx>
#include <fmtanchr.hxx>
#include <htmltbl.hxx>
#include <docsh.hxx>
#include <viewsh.hxx>
#include <unodraw.hxx>
#include <unotextrange.hxx>
#include <unotxdoc.hxx>

#include "swcss1.hxx"
#include "swhtml.hxx"
#include "htmlform.hxx"

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::form;

const sal_uInt16 TABINDEX_MIN = 0;
const sal_uInt16 TABINDEX_MAX = 32767;

HTMLOptionEnum<FormSubmitMethod> const aHTMLFormMethodTable[] =
{
    { OOO_STRING_SVTOOLS_HTML_METHOD_get,   FormSubmitMethod_GET    },
    { OOO_STRING_SVTOOLS_HTML_METHOD_post,  FormSubmitMethod_POST   },
    { nullptr,                              FormSubmitMethod(0)     }
};

HTMLOptionEnum<FormSubmitEncoding> const aHTMLFormEncTypeTable[] =
{
    { OOO_STRING_SVTOOLS_HTML_ET_url,       FormSubmitEncoding_URL          },
    { OOO_STRING_SVTOOLS_HTML_ET_multipart, FormSubmitEncoding_MULTIPART    },
    { OOO_STRING_SVTOOLS_HTML_ET_text,      FormSubmitEncoding_TEXT         },
    { nullptr,                              FormSubmitEncoding(0)           }
};

namespace {

enum HTMLWordWrapMode { HTML_WM_OFF, HTML_WM_HARD, HTML_WM_SOFT };

}

HTMLOptionEnum<HTMLWordWrapMode> const aHTMLTextAreaWrapTable[] =
{
    { OOO_STRING_SVTOOLS_HTML_WW_off,      HTML_WM_OFF     },
    { OOO_STRING_SVTOOLS_HTML_WW_hard,     HTML_WM_HARD    },
    { OOO_STRING_SVTOOLS_HTML_WW_soft,     HTML_WM_SOFT    },
    { OOO_STRING_SVTOOLS_HTML_WW_physical, HTML_WM_HARD    },
    { OOO_STRING_SVTOOLS_HTML_WW_virtual,  HTML_WM_SOFT    },
    { nullptr,                             HTMLWordWrapMode(0) }
};

const SvMacroItemId aEventTypeTable[] =
{
    SvMacroItemId::HtmlOnSubmitForm,
    SvMacroItemId::HtmlOnResetForm,
    SvMacroItemId::HtmlOnGetFocus,
    SvMacroItemId::HtmlOnLoseFocus,
    SvMacroItemId::HtmlOnClick,
    SvMacroItemId::HtmlOnClickItem,
    SvMacroItemId::HtmlOnChange,
    SvMacroItemId::HtmlOnSelect,
    SvMacroItemId::NONE
};

const OUString aEventListenerTable[] =
{
    u"XSubmitListener"_ustr,
    u"XResetListener"_ustr,
    u"XFocusListener"_ustr,
    u"XFocusListener"_ustr,
    u"XApproveActionListener"_ustr,
    u"XItemListener"_ustr,
    u"XChangeListener"_ustr,
    u""_ustr
};

const OUString aEventMethodTable[] =
{
    u"approveSubmit"_ustr,
    u"approveReset"_ustr,
    u"focusGained"_ustr,
    u"focusLost"_ustr,
    u"approveAction"_ustr,
    u"itemStateChanged"_ustr,
    u"changed"_ustr,
    u""_ustr
};

const char * const aEventSDOptionTable[] =
{
    OOO_STRING_SVTOOLS_HTML_O_SDonsubmit,
    OOO_STRING_SVTOOLS_HTML_O_SDonreset,
    OOO_STRING_SVTOOLS_HTML_O_SDonfocus,
    OOO_STRING_SVTOOLS_HTML_O_SDonblur,
    OOO_STRING_SVTOOLS_HTML_O_SDonclick,
    OOO_STRING_SVTOOLS_HTML_O_SDonclick,
    OOO_STRING_SVTOOLS_HTML_O_SDonchange,
    nullptr
};

const char * const aEventOptionTable[] =
{
    OOO_STRING_SVTOOLS_HTML_O_onsubmit,
    OOO_STRING_SVTOOLS_HTML_O_onreset,
    OOO_STRING_SVTOOLS_HTML_O_onfocus,
    OOO_STRING_SVTOOLS_HTML_O_onblur,
    OOO_STRING_SVTOOLS_HTML_O_onclick,
    OOO_STRING_SVTOOLS_HTML_O_onclick,
    OOO_STRING_SVTOOLS_HTML_O_onchange,
    nullptr
};

class SwHTMLForm_Impl
{
    SwDocShell                  *m_pDocShell;

    SvKeyValueIterator          *m_pHeaderAttrs;

    // Cached interfaces
    uno::Reference< drawing::XDrawPage >            m_xDrawPage;
    uno::Reference< container::XIndexContainer >    m_xForms;
    uno::Reference< drawing::XShapes >              m_xShapes;
    uno::Reference< XMultiServiceFactory >          m_xServiceFactory;

    uno::Reference< script::XEventAttacherManager >     m_xControlEventManager;
    uno::Reference< script::XEventAttacherManager >     m_xFormEventManager;

    // Context information
    uno::Reference< container::XIndexContainer >    m_xFormComps;
    uno::Reference< beans::XPropertySet >           m_xFCompPropertySet;
    uno::Reference< drawing::XShape >               m_xShape;

    OUString                    m_sText;
    std::vector<OUString>         m_aStringList;
    std::vector<OUString>         m_aValueList;
    std::vector<sal_uInt16>     m_aSelectedList;

public:
    explicit SwHTMLForm_Impl( SwDocShell *pDSh ) :
        m_pDocShell( pDSh ),
        m_pHeaderAttrs( pDSh ? pDSh->GetHeaderAttributes() : nullptr )
    {
        OSL_ENSURE( m_pDocShell, "No DocShell, no Controls" );
    }

    const uno::Reference< XMultiServiceFactory >& GetServiceFactory();
    void GetDrawPage();
    const uno::Reference< drawing::XShapes >& GetShapes();
    const uno::Reference< script::XEventAttacherManager >& GetControlEventManager();
    const uno::Reference< script::XEventAttacherManager >& GetFormEventManager();
    const uno::Reference< container::XIndexContainer >& GetForms();

    const uno::Reference< container::XIndexContainer >& GetFormComps() const
    {
        return m_xFormComps;
    }

    void SetFormComps( const uno::Reference< container::XIndexContainer >& r )
    {
        m_xFormComps = r;
    }

    void ReleaseFormComps() { m_xFormComps = nullptr; m_xControlEventManager = nullptr; }

    const uno::Reference< beans::XPropertySet >& GetFCompPropSet() const
    {
        return m_xFCompPropertySet;
    }

    void SetFCompPropSet( const uno::Reference< beans::XPropertySet >& r )
    {
        m_xFCompPropertySet = r;
    }

    void ReleaseFCompPropSet() { m_xFCompPropertySet = nullptr; }

    const uno::Reference< drawing::XShape >& GetShape() const { return m_xShape; }
    void SetShape( const uno::Reference< drawing::XShape >& r ) { m_xShape = r; }

    OUString& GetText() { return m_sText; }
    void EraseText() { m_sText.clear(); }

    std::vector<OUString>& GetStringList() { return m_aStringList; }
    void EraseStringList()
    {
        m_aStringList.clear();
    }

    std::vector<OUString>& GetValueList() { return m_aValueList; }
    void EraseValueList()
    {
        m_aValueList.clear();
    }

    std::vector<sal_uInt16>& GetSelectedList() { return m_aSelectedList; }
    void EraseSelectedList()
    {
        m_aSelectedList.clear();
    }

    SvKeyValueIterator *GetHeaderAttrs() const { return m_pHeaderAttrs; }
};

const uno::Reference< XMultiServiceFactory >& SwHTMLForm_Impl::GetServiceFactory()
{
    if( !m_xServiceFactory.is() && m_pDocShell )
    {
        m_xServiceFactory =
            uno::Reference< XMultiServiceFactory >( m_pDocShell->GetBaseModel() );
        OSL_ENSURE( m_xServiceFactory.is(),
                "XServiceFactory not received from model" );
    }
    return m_xServiceFactory;
}

void SwHTMLForm_Impl::GetDrawPage()
{
    if( !m_xDrawPage.is() && m_pDocShell )
    {
        rtl::Reference< SwXTextDocument > xTextDoc( m_pDocShell->GetBaseModel() );
        OSL_ENSURE( xTextDoc.is(),
                "drawing::XDrawPageSupplier not received from model" );
        m_xDrawPage = xTextDoc->getDrawPage();
        OSL_ENSURE( m_xDrawPage.is(), "drawing::XDrawPage not received" );
    }
}

const uno::Reference< container::XIndexContainer >& SwHTMLForm_Impl::GetForms()
{
    if( !m_xForms.is() )
    {
        GetDrawPage();
        if( m_xDrawPage.is() )
        {
            uno::Reference< XFormsSupplier > xFormsSupplier( m_xDrawPage, UNO_QUERY );
            OSL_ENSURE( xFormsSupplier.is(),
                    "XFormsSupplier not received from drawing::XDrawPage" );

            uno::Reference< container::XNameContainer > xNameCont =
                xFormsSupplier->getForms();
            m_xForms.set( xNameCont, UNO_QUERY );

            OSL_ENSURE( m_xForms.is(), "XForms not received" );
        }
    }
    return m_xForms;
}

const uno::Reference< drawing::XShapes > & SwHTMLForm_Impl::GetShapes()
{
    if( !m_xShapes.is() )
    {
        GetDrawPage();
        if( m_xDrawPage.is() )
        {
            m_xShapes = m_xDrawPage;
            OSL_ENSURE( m_xShapes.is(),
                    "XShapes not received from drawing::XDrawPage" );
        }
    }
    return m_xShapes;
}

const uno::Reference< script::XEventAttacherManager >&
                                    SwHTMLForm_Impl::GetControlEventManager()
{
    if( !m_xControlEventManager.is() && m_xFormComps.is() )
    {
        m_xControlEventManager =
            uno::Reference< script::XEventAttacherManager >( m_xFormComps, UNO_QUERY );
        OSL_ENSURE( m_xControlEventManager.is(),
    "uno::Reference< XEventAttacherManager > not received from xFormComps" );
    }

    return m_xControlEventManager;
}

const uno::Reference< script::XEventAttacherManager >&
    SwHTMLForm_Impl::GetFormEventManager()
{
    if( !m_xFormEventManager.is() )
    {
        GetForms();
        if( m_xForms.is() )
        {
            m_xFormEventManager =
                uno::Reference< script::XEventAttacherManager >( m_xForms, UNO_QUERY );
            OSL_ENSURE( m_xFormEventManager.is(),
        "uno::Reference< XEventAttacherManager > not received from xForms" );
        }
    }

    return m_xFormEventManager;
}

namespace {

class SwHTMLImageWatcher :
    public cppu::WeakImplHelper< awt::XImageConsumer, XEventListener >
{
    uno::Reference< drawing::XShape >       m_xShape;     // the control
    uno::Reference< XImageProducerSupplier >    m_xSrc;
    uno::Reference< awt::XImageConsumer >   m_xThis;      // reference to self
    bool                            m_bSetWidth;
    bool                            m_bSetHeight;

    void clear();

public:
    SwHTMLImageWatcher( uno::Reference< drawing::XShape > xShape,
                        bool bWidth, bool bHeight );

    // startProduction can not be called in the constructor because it can
    // destruct itself, hence a separate method.
    void start() { m_xSrc->getImageProducer()->startProduction(); }

    // UNO binding

    // XImageConsumer
    virtual void SAL_CALL init( sal_Int32 Width, sal_Int32 Height) override;
    virtual void SAL_CALL setColorModel(
            sal_Int16 BitCount, const uno::Sequence< sal_Int32 >& RGBAPal,
            sal_Int32 RedMask, sal_Int32 GreenMask, sal_Int32 BlueMask,
            sal_Int32 AlphaMask) override;
    virtual void SAL_CALL setPixelsByBytes(
            sal_Int32 X, sal_Int32 Y, sal_Int32 Width, sal_Int32 Height,
            const uno::Sequence< sal_Int8 >& ProducerData,
            sal_Int32 Offset, sal_Int32 Scansize) override;
    virtual void SAL_CALL setPixelsByLongs(
            sal_Int32 X, sal_Int32 Y, sal_Int32 Width, sal_Int32 Height,
            const uno::Sequence< sal_Int32 >& ProducerData,
            sal_Int32 Offset, sal_Int32 Scansize) override;
    virtual void SAL_CALL complete(
            sal_Int32 Status,
            const uno::Reference< awt::XImageProducer > & Producer) override;

    // XEventListener
    virtual void SAL_CALL disposing( const EventObject& Source ) override;
};

}

SwHTMLImageWatcher::SwHTMLImageWatcher(
        uno::Reference< drawing::XShape > xShape,
        bool bWidth, bool bHeight ) :
    m_xShape(std::move( xShape )),
    m_bSetWidth( bWidth ), m_bSetHeight( bHeight )
{
    // Remember the source of the image
    uno::Reference< drawing::XControlShape > xControlShape( m_xShape, UNO_QUERY );
    uno::Reference< awt::XControlModel > xControlModel(
            xControlShape->getControl() );
    m_xSrc.set( xControlModel, UNO_QUERY );
    OSL_ENSURE( m_xSrc.is(), "No XImageProducerSupplier" );

    // Register as Event-Listener on the shape to be able to release it on dispose.
    uno::Reference< XEventListener > xEvtLstnr = static_cast<XEventListener *>(this);
    uno::Reference< XComponent > xComp( m_xShape, UNO_QUERY );
    xComp->addEventListener( xEvtLstnr );

    // Lastly we keep a reference to ourselves so we are not destroyed
    // (should not be necessary since we're still registered elsewhere)
    m_xThis = static_cast<awt::XImageConsumer *>(this);

    // Register at ImageProducer to retrieve the size...
    m_xSrc->getImageProducer()->addConsumer( m_xThis );
}

void SwHTMLImageWatcher::clear()
{
    // Unregister on Shape
    uno::Reference< XEventListener > xEvtLstnr = static_cast<XEventListener *>(this);
    uno::Reference< XComponent > xComp( m_xShape, UNO_QUERY );
    xComp->removeEventListener( xEvtLstnr );

    // Unregister on ImageProducer
    uno::Reference<awt::XImageProducer> xProd = m_xSrc->getImageProducer();
    if( xProd.is() )
        xProd->removeConsumer( m_xThis );
}

void SwHTMLImageWatcher::init( sal_Int32 Width, sal_Int32 Height )
{
    OSL_ENSURE( m_bSetWidth || m_bSetHeight,
            "Width or height has to be adjusted" );

    // If no width or height is given, it is initialized to those of
    // the empty graphic that is available before the stream of a graphic
    // that is to be displayed asynchronous is available.
    if( !Width && !Height )
        return;

    awt::Size aNewSz;
    aNewSz.Width = o3tl::convert(Width, o3tl::Length::px, o3tl::Length::mm100);
    aNewSz.Height = o3tl::convert(Height, o3tl::Length::px, o3tl::Length::mm100);

    if( !m_bSetWidth || !m_bSetHeight )
    {
        awt::Size aSz( m_xShape->getSize() );
        if( m_bSetWidth && aNewSz.Height )
        {
            aNewSz.Width *= aSz.Height;
            aNewSz.Width /= aNewSz.Height;
            aNewSz.Height = aSz.Height;
        }
        if( m_bSetHeight && aNewSz.Width )
        {
            aNewSz.Height *= aSz.Width;
            aNewSz.Height /= aNewSz.Width;
            aNewSz.Width = aSz.Width;
        }
    }
    if( aNewSz.Width < MINFLY )
        aNewSz.Width = MINFLY;
    if( aNewSz.Height < MINFLY )
        aNewSz.Height = MINFLY;

    m_xShape->setSize( aNewSz );
    if( m_bSetWidth )
    {
        // If the control is anchored to a table, the column have to be recalculated

        // To get to the SwXShape* we need an interface that is implemented by SwXShape

        uno::Reference< beans::XPropertySet > xPropSet( m_xShape, UNO_QUERY );
        SwXShape *pSwShape = comphelper::getFromUnoTunnel<SwXShape>(xPropSet);

        OSL_ENSURE( pSwShape, "Where is SW-Shape?" );
        if( pSwShape )
        {
            SwFrameFormat *pFrameFormat = pSwShape->GetFrameFormat();

            const SwDoc& rDoc = pFrameFormat->GetDoc();
            SwNode* pAnchorNode = pFrameFormat->GetAnchor().GetAnchorNode();
            SwTableNode *pTableNd;
            if (pAnchorNode && nullptr != (pTableNd = pAnchorNode->FindTableNode()))
            {
                const bool bLastGrf = !pTableNd->GetTable().DecGrfsThatResize();
                SwHTMLTableLayout *pLayout =
                    pTableNd->GetTable().GetHTMLTableLayout();
                if( pLayout )
                {
                    const sal_uInt16 nBrowseWidth =
                        pLayout->GetBrowseWidthByTable( rDoc );

                    if ( nBrowseWidth )
                    {
                        pLayout->Resize( nBrowseWidth, true, true,
                                         bLastGrf ? HTMLTABLE_RESIZE_NOW
                                                  : 500 );
                    }
                }
            }
        }
    }

    // unregister and delete self
    clear();
    m_xThis = nullptr;
}

void SwHTMLImageWatcher::setColorModel(
        sal_Int16, const Sequence< sal_Int32 >&, sal_Int32, sal_Int32,
        sal_Int32, sal_Int32 )
{
}

void SwHTMLImageWatcher::setPixelsByBytes(
        sal_Int32, sal_Int32, sal_Int32, sal_Int32,
        const Sequence< sal_Int8 >&, sal_Int32, sal_Int32 )
{
}

void SwHTMLImageWatcher::setPixelsByLongs(
        sal_Int32, sal_Int32, sal_Int32, sal_Int32,
        const Sequence< sal_Int32 >&, sal_Int32, sal_Int32 )
{
}

void SwHTMLImageWatcher::complete( sal_Int32 Status,
        const uno::Reference< awt::XImageProducer >& )
{
    if( awt::ImageStatus::IMAGESTATUS_ERROR == Status || awt::ImageStatus::IMAGESTATUS_ABORTED == Status )
    {
        // unregister and delete self
        clear();
        m_xThis = nullptr;
    }
}

void SwHTMLImageWatcher::disposing(const lang::EventObject& evt)
{
    uno::Reference< awt::XImageConsumer > xTmp;

    // We need to release the shape if it is disposed of
    if( evt.Source == m_xShape )
    {
        clear();
        xTmp = static_cast<awt::XImageConsumer*>(this);
        m_xThis = nullptr;
    }
}

void SwHTMLParser::DeleteFormImpl()
{
    delete m_pFormImpl;
    m_pFormImpl = nullptr;
}

static void lcl_html_setFixedFontProperty(
        const uno::Reference< beans::XPropertySet >& rPropSet )
{
    vcl::Font aFixedFont( OutputDevice::GetDefaultFont(
                                    DefaultFontType::FIXED, LANGUAGE_ENGLISH_US,
                                    GetDefaultFontFlags::OnlyOne )  );
    Any aTmp;
    aTmp <<= aFixedFont.GetFamilyName();
    rPropSet->setPropertyValue(u"FontName"_ustr, aTmp );

    aTmp <<= aFixedFont.GetStyleName();
    rPropSet->setPropertyValue(u"FontStyleName"_ustr,
                                aTmp );

    aTmp <<= static_cast<sal_Int16>(aFixedFont.GetFamilyTypeMaybeAskConfig());
    rPropSet->setPropertyValue(u"FontFamily"_ustr, aTmp );

    aTmp <<= static_cast<sal_Int16>(aFixedFont.GetCharSet());
    rPropSet->setPropertyValue(u"FontCharset"_ustr,
                                aTmp );

    aTmp <<= static_cast<sal_Int16>(aFixedFont.GetPitchMaybeAskConfig());
    rPropSet->setPropertyValue(u"FontPitch"_ustr, aTmp );

    aTmp <<= float(10.0);
    rPropSet->setPropertyValue(u"FontHeight"_ustr, aTmp );
}

void SwHTMLParser::SetControlSize( const uno::Reference< drawing::XShape >& rShape,
                                   const Size& rTextSz,
                                   bool bMinWidth,
                                   bool bMinHeight )
{
    if( !rTextSz.Width() && !rTextSz.Height() && !bMinWidth  && !bMinHeight )
        return;

    // To get to SwXShape* we need an interface that is implemented by SwXShape

    uno::Reference< beans::XPropertySet > xPropSet( rShape, UNO_QUERY );

    SwViewShell *pVSh = m_xDoc->getIDocumentLayoutAccess().GetCurrentViewShell();
    if( !pVSh && !m_nEventId )
    {
        // If there is no view shell by now and the doc shell is an internal
        // one, no view shell will be created. That for, we have to do that of
        // our own. This happens if a linked section is inserted or refreshed.
        SwDocShell *pDocSh = m_xDoc->GetDocShell();
        if( pDocSh )
        {
            if ( pDocSh->GetMedium() )
            {
                // if there is no hidden property in the MediaDescriptor it should be removed after loading
                const SfxBoolItem* pHiddenItem = pDocSh->GetMedium()->GetItemSet().GetItem(SID_HIDDEN, false);
                m_bRemoveHidden = ( pHiddenItem == nullptr || !pHiddenItem->GetValue() );
            }

            m_pTempViewFrame = SfxViewFrame::LoadHiddenDocument( *pDocSh, SFX_INTERFACE_NONE );
            CallStartAction();
            pVSh = m_xDoc->getIDocumentLayoutAccess().GetCurrentViewShell();
            // this ridiculous hack also enables Undo, so turn it off again
            m_xDoc->GetIDocumentUndoRedo().DoUndo(false);
        }
    }

    SwXShape *pSwShape = comphelper::getFromUnoTunnel<SwXShape>(xPropSet);

    OSL_ENSURE( pSwShape, "Where is SW-Shape?" );

    // has to be a Draw-Format
    SwFrameFormat *pFrameFormat = pSwShape ? pSwShape->GetFrameFormat() : nullptr ;
    OSL_ENSURE( pFrameFormat && RES_DRAWFRMFMT == pFrameFormat->Which(), "No DrawFrameFormat" );

    // look if a SdrObject exists for it
    const SdrObject *pObj = pFrameFormat ? pFrameFormat->FindSdrObject() : nullptr;
    OSL_ENSURE( pObj, "SdrObject not found" );
    OSL_ENSURE( pObj && SdrInventor::FmForm == pObj->GetObjInventor(), "wrong Inventor" );

    const SdrView* pDrawView = pVSh ? pVSh->GetDrawView() : nullptr;

    const SdrUnoObj *pFormObj = dynamic_cast<const SdrUnoObj*>( pObj  );
    uno::Reference< awt::XControl > xControl;
    if ( pDrawView && pVSh->GetWin() && pFormObj )
        xControl = pFormObj->GetUnoControl( *pDrawView, *pVSh->GetWin()->GetOutDev() );

    awt::Size aSz( rShape->getSize() );
    awt::Size aNewSz( 0, 0 );

    // #i71248# ensure we got a XControl before applying corrections
    if(xControl.is())
    {
        if( bMinWidth || bMinHeight )
        {
            uno::Reference< awt::XLayoutConstrains > xLC( xControl, UNO_QUERY );
            awt::Size aTmpSz( xLC->getPreferredSize() );
            if( bMinWidth )
                aNewSz.Width = aTmpSz.Width;
            if( bMinHeight )
                aNewSz.Height = aTmpSz.Height;
        }
        if( rTextSz.Width() || rTextSz.Height())
        {
            uno::Reference< awt::XTextLayoutConstrains > xLC( xControl, UNO_QUERY );
            OSL_ENSURE( xLC.is(), "no XTextLayoutConstrains" );
            if( xLC.is() )
            {
                awt::Size aTmpSz( rTextSz.Width(), rTextSz.Height() );
                if( -1 == rTextSz.Width() )
                {
                    aTmpSz.Width = 0;
                    aTmpSz.Height = m_nSelectEntryCnt;
                }
                aTmpSz = xLC->getMinimumSize( static_cast< sal_Int16 >(aTmpSz.Width), static_cast< sal_Int16 >(aTmpSz.Height) );
                if( rTextSz.Width() )
                    aNewSz.Width = aTmpSz.Width;
                if( rTextSz.Height() )
                    aNewSz.Height = aTmpSz.Height;
            }
        }
    }

    aNewSz.Width = o3tl::convert(aNewSz.Width, o3tl::Length::px, o3tl::Length::mm100);
    aNewSz.Height = o3tl::convert(aNewSz.Height, o3tl::Length::px, o3tl::Length::mm100);
    if( aNewSz.Width )
    {
        if( aNewSz.Width < MINLAY )
            aNewSz.Width = MINLAY;
        aSz.Width = aNewSz.Width;
    }
    if( aNewSz.Height )
    {
        if( aNewSz.Height < MINLAY )
            aNewSz.Height = MINLAY;
        aSz.Height = aNewSz.Height;
    }

    rShape->setSize( aSz );
}

static bool lcl_html_setEvents(
        const uno::Reference< script::XEventAttacherManager > & rEvtMn,
        sal_uInt32 nPos, const SvxMacroTableDtor& rMacroTable,
        const std::vector<OUString>& rUnoMacroTable,
        const std::vector<OUString>& rUnoMacroParamTable,
        const OUString& rType )
{
    // First the number of events has to be determined
    sal_Int32 nEvents = 0;

    for( int i = 0; SvMacroItemId::NONE != aEventTypeTable[i]; ++i )
    {
        const SvxMacro *pMacro = rMacroTable.Get( aEventTypeTable[i] );
        // As long as not all events are implemented the table also holds empty strings
        if( pMacro && !aEventListenerTable[i].isEmpty() )
            nEvents++;
    }
    for( const auto &rStr : rUnoMacroTable )
    {
        sal_Int32 nIndex = 0;
        if( o3tl::getToken(rStr, 0, '-', nIndex ).empty() || -1 == nIndex )
            continue;
        if( o3tl::getToken(rStr, 0, '-', nIndex ).empty() || -1 == nIndex )
            continue;
        if( nIndex < rStr.getLength() )
            nEvents++;
    }

    if( 0==nEvents )
        return false;

    Sequence<script::ScriptEventDescriptor> aDescs( nEvents );
    script::ScriptEventDescriptor* pDescs = aDescs.getArray();
    sal_Int32 nEvent = 0;

    for( int i=0; SvMacroItemId::NONE != aEventTypeTable[i]; ++i )
    {
        const SvxMacro *pMacro = rMacroTable.Get( aEventTypeTable[i] );
        if( pMacro && !aEventListenerTable[i].isEmpty() )
        {
            script::ScriptEventDescriptor& rDesc = pDescs[nEvent++];
            rDesc.ListenerType = aEventListenerTable[i];
            rDesc.EventMethod = aEventMethodTable[i];
            rDesc.ScriptType = pMacro->GetLanguage();
            rDesc.ScriptCode = pMacro->GetMacName();
        }
    }

    for( const auto &rStr : rUnoMacroTable )
    {
        sal_Int32 nIndex = 0;
        OUString sListener( rStr.getToken( 0, '-', nIndex ) );
        if( sListener.isEmpty() || -1 == nIndex )
            continue;

        OUString sMethod( rStr.getToken( 0, '-', nIndex ) );
        if( sMethod.isEmpty() || -1 == nIndex )
            continue;

        OUString sCode( rStr.copy( nIndex ) );
        if( sCode.isEmpty() )
            continue;

        script::ScriptEventDescriptor& rDesc = pDescs[nEvent++];
        rDesc.ListenerType = sListener;
        rDesc.EventMethod = sMethod;
        rDesc.ScriptType = rType;
        rDesc.ScriptCode = sCode;
        rDesc.AddListenerParam.clear();

        if(!rUnoMacroParamTable.empty())
        {
            OUString sSearch = sListener + "-" +sMethod + "-";
            sal_Int32 nLen = sSearch.getLength();
            for(const auto & rParam : rUnoMacroParamTable)
            {
                if( rParam.startsWith( sSearch ) && rParam.getLength() > nLen )
                {
                    rDesc.AddListenerParam = rParam.copy(nLen);
                    break;
                }
            }
        }
    }
    rEvtMn->registerScriptEvents( nPos, aDescs );
    return true;
}

static void lcl_html_getEvents( const OUString& rOption, std::u16string_view rValue,
                                std::vector<OUString>& rUnoMacroTable,
                                std::vector<OUString>& rUnoMacroParamTable )
{
    if( rOption.startsWithIgnoreAsciiCase( OOO_STRING_SVTOOLS_HTML_O_sdevent ) )
    {
        OUString aEvent = OUString::Concat(rOption.subView( strlen( OOO_STRING_SVTOOLS_HTML_O_sdevent ) )) +
            "-" + rValue;
        rUnoMacroTable.push_back(aEvent);
    }
    else if( rOption.startsWithIgnoreAsciiCase( OOO_STRING_SVTOOLS_HTML_O_sdaddparam ) )
    {
        OUString aParam = OUString::Concat(rOption.subView( strlen( OOO_STRING_SVTOOLS_HTML_O_sdaddparam ) )) +
            "-" + rValue;
        rUnoMacroParamTable.push_back(aParam);
    }
}

uno::Reference< drawing::XShape > SwHTMLParser::InsertControl(
        const uno::Reference< XFormComponent > & rFComp,
        const uno::Reference< beans::XPropertySet > & rFCompPropSet,
        const Size& rSize, sal_Int16 eVertOri, sal_Int16 eHoriOri,
        SfxItemSet& rCSS1ItemSet, SvxCSS1PropertyInfo& rCSS1PropInfo,
        const SvxMacroTableDtor& rMacroTable, const std::vector<OUString>& rUnoMacroTable,
        const std::vector<OUString>& rUnoMacroParamTable, bool bSetFCompPropSet,
        bool bHidden )
{
    uno::Reference< drawing::XShape >  xShape;

    const uno::Reference< container::XIndexContainer > & rFormComps =
        m_pFormImpl->GetFormComps();
    Any aAny( &rFComp, cppu::UnoType<XFormComponent>::get());
    rFormComps->insertByIndex( rFormComps->getCount(), aAny );

    if( !bHidden )
    {
        Any aTmp;
        sal_Int32 nLeftSpace = 0;
        sal_Int32 nRightSpace = 0;
        sal_Int32 nUpperSpace = 0;
        sal_Int32 nLowerSpace = 0;

        const uno::Reference< XMultiServiceFactory > & rServiceFactory =
            m_pFormImpl->GetServiceFactory();
        if( !rServiceFactory.is() )
            return xShape;

        uno::Reference< XInterface > xCreate = rServiceFactory->createInstance( u"com.sun.star.drawing.ControlShape"_ustr );
        if( !xCreate.is() )
            return xShape;

        xShape.set( xCreate, UNO_QUERY );

        OSL_ENSURE( xShape.is(), "XShape not received" );
        awt::Size aTmpSz;
        aTmpSz.Width  = rSize.Width();
        aTmpSz.Height = rSize.Height();
        xShape->setSize( aTmpSz );

        uno::Reference< beans::XPropertySet > xShapePropSet( xCreate, UNO_QUERY );

        // set left/right border
        // note: parser never creates SvxLeftMarginItem! must be converted
        if (const SvxTextLeftMarginItem *const pLeft = rCSS1ItemSet.GetItemIfSet(RES_MARGIN_TEXTLEFT))
        {
            if( rCSS1PropInfo.m_bLeftMargin )
            {
                // should be SvxLeftMarginItem... "cast" it
                nLeftSpace = convertTwipToMm100(pLeft->ResolveTextLeft({}));
                rCSS1PropInfo.m_bLeftMargin = false;
            }
            rCSS1ItemSet.ClearItem(RES_MARGIN_TEXTLEFT);
        }
        if (const SvxRightMarginItem *const pRight = rCSS1ItemSet.GetItemIfSet(RES_MARGIN_RIGHT))
        {
            if( rCSS1PropInfo.m_bRightMargin )
            {
                nRightSpace = convertTwipToMm100(pRight->ResolveRight({}));
                rCSS1PropInfo.m_bRightMargin = false;
            }
            rCSS1ItemSet.ClearItem(RES_MARGIN_RIGHT);
        }
        if( nLeftSpace || nRightSpace )
        {
            Any aAny2;
            aAny2 <<= nLeftSpace;
            xShapePropSet->setPropertyValue(u"LeftMargin"_ustr, aAny2 );

            aAny2 <<= nRightSpace;
            xShapePropSet->setPropertyValue(u"RightMargin"_ustr, aAny2 );
        }

        // set upper/lower border
        if( const SvxULSpaceItem *pULItem = rCSS1ItemSet.GetItemIfSet( RES_UL_SPACE ) )
        {
            // Flatten first line indent
            if( rCSS1PropInfo.m_bTopMargin )
            {
                nUpperSpace = convertTwipToMm100( pULItem->GetUpper() );
                rCSS1PropInfo.m_bTopMargin = false;
            }
            if( rCSS1PropInfo.m_bBottomMargin )
            {
                nLowerSpace = convertTwipToMm100( pULItem->GetLower() );
                rCSS1PropInfo.m_bBottomMargin = false;
            }

            rCSS1ItemSet.ClearItem( RES_UL_SPACE );
        }
        if( nUpperSpace || nLowerSpace )
        {
            uno::Any aAny2;
            aAny2 <<= nUpperSpace;
            xShapePropSet->setPropertyValue(u"TopMargin"_ustr, aAny2 );

            aAny2 <<= nLowerSpace;
            xShapePropSet->setPropertyValue(u"BottomMargin"_ustr, aAny2 );
        }

        uno::Reference< beans::XPropertySetInfo > xPropSetInfo =
            rFCompPropSet->getPropertySetInfo();
        OUString sPropName = u"BackgroundColor"_ustr;
        const SvxBrushItem* pBrushItem = rCSS1ItemSet.GetItemIfSet( RES_BACKGROUND );
        if( pBrushItem && xPropSetInfo->hasPropertyByName( sPropName ) )
        {
            const Color &rColor = pBrushItem->GetColor();
            /// copy color, if color is not "no fill"/"auto fill"
            if( rColor != COL_TRANSPARENT )
            {
                /// copy complete color with transparency
                aTmp <<= rColor;
                rFCompPropSet->setPropertyValue( sPropName, aTmp );
            }

        }

        sPropName = "TextColor";
        const SvxColorItem* pColorItem = rCSS1ItemSet.GetItemIfSet( RES_CHRATR_COLOR );
        if( pColorItem && xPropSetInfo->hasPropertyByName( sPropName ) )
        {
            aTmp <<= static_cast<sal_Int32>(pColorItem->GetValue().GetRGBColor());
            rFCompPropSet->setPropertyValue( sPropName, aTmp );
        }

        sPropName = "FontHeight";
        const SvxFontHeightItem* pFontHeightItem = rCSS1ItemSet.GetItemIfSet( RES_CHRATR_FONTSIZE );
        if( pFontHeightItem && xPropSetInfo->hasPropertyByName( sPropName ) )
        {
            float fVal = static_cast< float >( pFontHeightItem->GetHeight() / 20.0 );
            aTmp <<= fVal;
            rFCompPropSet->setPropertyValue( sPropName, aTmp );
        }

        if( const SvxFontItem* pFontItem = rCSS1ItemSet.GetItemIfSet( RES_CHRATR_FONT ) )
        {
            sPropName = "FontName";
            if( xPropSetInfo->hasPropertyByName( sPropName ) )
            {
                aTmp <<= pFontItem->GetFamilyName();
                rFCompPropSet->setPropertyValue( sPropName, aTmp );
            }
            sPropName = "FontStyleName";
            if( xPropSetInfo->hasPropertyByName( sPropName ) )
            {
                aTmp <<= pFontItem->GetStyleName();
                rFCompPropSet->setPropertyValue( sPropName, aTmp );
            }
            sPropName = "FontFamily";
            if( xPropSetInfo->hasPropertyByName( sPropName ) )
            {
                aTmp <<= static_cast<sal_Int16>(pFontItem->GetFamily()) ;
                rFCompPropSet->setPropertyValue( sPropName, aTmp );
            }
            sPropName = "FontCharset";
            if( xPropSetInfo->hasPropertyByName( sPropName ) )
            {
                aTmp <<= static_cast<sal_Int16>(pFontItem->GetCharSet()) ;
                rFCompPropSet->setPropertyValue( sPropName, aTmp );
            }
            sPropName = "FontPitch";
            if( xPropSetInfo->hasPropertyByName( sPropName ) )
            {
                aTmp <<= static_cast<sal_Int16>(pFontItem->GetPitch()) ;
                rFCompPropSet->setPropertyValue( sPropName, aTmp );
            }
        }

        sPropName = "FontWeight";
        const SvxWeightItem* pWeightItem = rCSS1ItemSet.GetItemIfSet( RES_CHRATR_WEIGHT );
        if( pWeightItem && xPropSetInfo->hasPropertyByName( sPropName ) )
        {
            float fVal = vcl::unohelper::ConvertFontWeight(
                    pWeightItem->GetWeight() );
            aTmp <<= fVal;
            rFCompPropSet->setPropertyValue( sPropName, aTmp );
        }

        sPropName = "FontSlant";
        const SvxPostureItem* pPostureItem = rCSS1ItemSet.GetItemIfSet( RES_CHRATR_POSTURE );
        if( pPostureItem && xPropSetInfo->hasPropertyByName( sPropName ) )
        {
            aTmp <<= static_cast<sal_Int16>(pPostureItem->GetPosture());
            rFCompPropSet->setPropertyValue( sPropName, aTmp );
        }

        sPropName = "FontUnderline";
        const SvxUnderlineItem* pUnderlineItem = rCSS1ItemSet.GetItemIfSet( RES_CHRATR_UNDERLINE );
        if( pUnderlineItem && xPropSetInfo->hasPropertyByName( sPropName ) )
        {
            aTmp <<= static_cast<sal_Int16>(pUnderlineItem->GetLineStyle());
            rFCompPropSet->setPropertyValue( sPropName, aTmp );
        }

        sPropName = "FontStrikeout";
        const SvxCrossedOutItem* pCrossedOutItem = rCSS1ItemSet.GetItemIfSet( RES_CHRATR_CROSSEDOUT );
        if( pCrossedOutItem && xPropSetInfo->hasPropertyByName( sPropName ) )
        {
            aTmp <<= static_cast<sal_Int16>(pCrossedOutItem->GetStrikeout());
            rFCompPropSet->setPropertyValue( sPropName, aTmp );
        }

        rtl::Reference< SwXTextRange >  xTextRg;
        text::TextContentAnchorType nAnchorType = text::TextContentAnchorType_AS_CHARACTER;
        bool bSetPos = false, bSetSurround = false;
        sal_Int32 nXPos = 0, nYPos = 0;
        text::WrapTextMode nSurround = text::WrapTextMode_NONE;
        if( SVX_CSS1_POS_ABSOLUTE == rCSS1PropInfo.m_ePosition &&
            SVX_CSS1_LTYPE_TWIP == rCSS1PropInfo.m_eLeftType &&
            SVX_CSS1_LTYPE_TWIP == rCSS1PropInfo.m_eTopType )
        {
            const SwStartNode *pFlySttNd =
                m_pPam->GetPoint()->GetNode().FindFlyStartNode();

            if( pFlySttNd )
            {
                nAnchorType = text::TextContentAnchorType_AT_FRAME;
                SwPaM aPaM( *pFlySttNd );

                uno::Reference< text::XText >  xDummyTextRef; // dirty, but works according to OS...
                xTextRg = new SwXTextRange( aPaM, xDummyTextRef );
            }
            else
            {
                nAnchorType = text::TextContentAnchorType_AT_PAGE;
            }
            nXPos = convertTwipToMm100( rCSS1PropInfo.m_nLeft ) + nLeftSpace;
            nYPos = convertTwipToMm100( rCSS1PropInfo.m_nTop ) + nUpperSpace;
            bSetPos = true;

            nSurround = text::WrapTextMode_THROUGH;
            bSetSurround = true;
        }
        else if( SvxAdjust::Left == rCSS1PropInfo.m_eFloat ||
                 text::HoriOrientation::LEFT == eHoriOri )
        {
            nAnchorType = text::TextContentAnchorType_AT_PARAGRAPH;
            nXPos = nLeftSpace;
            nYPos = nUpperSpace;
            bSetPos = true;
            nSurround = text::WrapTextMode_RIGHT;
            bSetSurround = true;
        }
        else if( text::VertOrientation::NONE != eVertOri )
        {
            sal_Int16 nVertOri = text::VertOrientation::NONE;
            switch( eVertOri )
            {
            case text::VertOrientation::TOP:
                nVertOri = text::VertOrientation::TOP;
                break;
            case text::VertOrientation::CENTER:
                nVertOri = text::VertOrientation::CENTER;
                break;
            case text::VertOrientation::BOTTOM:
                nVertOri = text::VertOrientation::BOTTOM;
                break;
            case text::VertOrientation::CHAR_TOP:
                nVertOri = text::VertOrientation::CHAR_TOP;
                break;
            case text::VertOrientation::CHAR_CENTER:
                nVertOri = text::VertOrientation::CHAR_CENTER;
                break;
            case text::VertOrientation::CHAR_BOTTOM:
                nVertOri = text::VertOrientation::CHAR_BOTTOM;
                break;
            case text::VertOrientation::LINE_TOP:
                nVertOri = text::VertOrientation::LINE_TOP;
                break;
            case text::VertOrientation::LINE_CENTER:
                nVertOri = text::VertOrientation::LINE_CENTER;
                break;
            case text::VertOrientation::LINE_BOTTOM:
                nVertOri = text::VertOrientation::LINE_BOTTOM;
                break;
            // coverity[dead_error_begin] - following conditions exist to avoid compiler warning
            case text::VertOrientation::NONE:
                nVertOri = text::VertOrientation::NONE;
                break;
            }
            aTmp <<= nVertOri ;
            xShapePropSet->setPropertyValue(u"VertOrient"_ustr, aTmp );
        }

        aTmp <<= nAnchorType ;
        xShapePropSet->setPropertyValue(u"AnchorType"_ustr, aTmp );

        if( text::TextContentAnchorType_AT_PAGE == nAnchorType )
        {
            aTmp <<= sal_Int16(1) ;
            xShapePropSet->setPropertyValue(u"AnchorPageNo"_ustr, aTmp );
        }
        else
        {
            if( !xTextRg.is() )
            {
                uno::Reference< text::XText >  xDummyTextRef; // dirty but works according to OS...
                xTextRg = new SwXTextRange( *m_pPam, xDummyTextRef );
            }

            aTmp <<= uno::Reference< text::XTextRange >(xTextRg);
            xShapePropSet->setPropertyValue(u"TextRange"_ustr, aTmp );
        }

        if( bSetPos )
        {
            aTmp <<= sal_Int16(text::HoriOrientation::NONE);
            xShapePropSet->setPropertyValue(u"HoriOrient"_ustr, aTmp );
            aTmp <<= nXPos ;
            xShapePropSet->setPropertyValue(u"HoriOrientPosition"_ustr, aTmp );

            aTmp <<= sal_Int16(text::VertOrientation::NONE);
            xShapePropSet->setPropertyValue(u"VertOrient"_ustr, aTmp );
            aTmp <<= nYPos ;
            xShapePropSet->setPropertyValue(u"VertOrientPosition"_ustr, aTmp );
        }
        if( bSetSurround )
        {
            aTmp <<= nSurround ;
            xShapePropSet->setPropertyValue(u"Surround"_ustr, aTmp );
        }

        m_pFormImpl->GetShapes()->add(xShape);

        // Set ControlModel to ControlShape
        uno::Reference< drawing::XControlShape > xControlShape( xShape, UNO_QUERY );
        uno::Reference< awt::XControlModel >  xControlModel( rFComp, UNO_QUERY );
        xControlShape->setControl( xControlModel );
    }

    // Since the focus is set at insertion of the controls, focus events will be sent
    // To prevent previous JavaScript-Events from being called, these events will only be set retroactively
    if( !rMacroTable.empty() || !rUnoMacroTable.empty() )
    {
        bool bHasEvents = lcl_html_setEvents( m_pFormImpl->GetControlEventManager(),
                            rFormComps->getCount() - 1,
                            rMacroTable, rUnoMacroTable, rUnoMacroParamTable,
                            GetScriptTypeString(m_pFormImpl->GetHeaderAttrs()) );
        if (bHasEvents)
            NotifyMacroEventRead();
    }

    if( bSetFCompPropSet )
    {
        m_pFormImpl->SetFCompPropSet( rFCompPropSet );
    }

    return xShape;
}

void SwHTMLParser::NewForm( bool bAppend )
{
    // Does a form already exist?
    if( m_pFormImpl && m_pFormImpl->GetFormComps().is() )
        return;

    if( bAppend )
    {
        if( m_pPam->GetPoint()->GetContentIndex() )
            AppendTextNode( AM_SPACE );
        else
            AddParSpace();
    }

    if( !m_pFormImpl )
        m_pFormImpl = new SwHTMLForm_Impl( m_xDoc->GetDocShell() );

    OUString aAction( m_sBaseURL );
    OUString sName, sTarget;
    FormSubmitEncoding nEncType = FormSubmitEncoding_URL;
    FormSubmitMethod nMethod = FormSubmitMethod_GET;
    SvxMacroTableDtor aMacroTable;
    std::vector<OUString> aUnoMacroTable;
    std::vector<OUString> aUnoMacroParamTable;
    SvKeyValueIterator *pHeaderAttrs = m_pFormImpl->GetHeaderAttrs();
    ScriptType eDfltScriptType = GetScriptType( pHeaderAttrs );
    const OUString& rDfltScriptType = GetScriptTypeString( pHeaderAttrs );

    const HTMLOptions& rHTMLOptions = GetOptions();
    for (size_t i = rHTMLOptions.size(); i; )
    {
        const HTMLOption& rOption = rHTMLOptions[--i];
        ScriptType eScriptType2 = eDfltScriptType;
        SvMacroItemId nEvent = SvMacroItemId::NONE;
        bool bSetEvent = false;

        switch( rOption.GetToken() )
        {
        case HtmlOptionId::ACTION:
            aAction = rOption.GetString();
            break;
        case HtmlOptionId::METHOD:
            nMethod = rOption.GetEnum( aHTMLFormMethodTable, nMethod );
            break;
        case HtmlOptionId::ENCTYPE:
            nEncType = rOption.GetEnum( aHTMLFormEncTypeTable, nEncType );
            break;
        case HtmlOptionId::TARGET:
            sTarget = rOption.GetString();
            break;
        case HtmlOptionId::NAME:
            sName = rOption.GetString();
            break;

        case HtmlOptionId::SDONSUBMIT:
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONSUBMIT:
            nEvent = SvMacroItemId::HtmlOnSubmitForm;
            bSetEvent = true;
            break;

        case HtmlOptionId::SDONRESET:
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONRESET:
            nEvent = SvMacroItemId::HtmlOnResetForm;
            bSetEvent = true;
            break;

        default:
            lcl_html_getEvents( rOption.GetTokenString(),
                                rOption.GetString(),
                                aUnoMacroTable, aUnoMacroParamTable );
            break;
        }

        if( bSetEvent )
        {
            OUString sEvent( rOption.GetString() );
            if( !sEvent.isEmpty() )
            {
                sEvent = convertLineEnd(sEvent, GetSystemLineEnd());
                OUString aScriptType2;
                if( EXTENDED_STYPE==eScriptType2 )
                    aScriptType2 = rDfltScriptType;
                aMacroTable.Insert( nEvent, SvxMacro( sEvent, aScriptType2, eScriptType2 ) );
            }
        }
    }

    const uno::Reference< XMultiServiceFactory > & rSrvcMgr =
        m_pFormImpl->GetServiceFactory();
    if( !rSrvcMgr.is() )
        return;

    uno::Reference< XInterface > xInt;
    uno::Reference<XForm> xForm;
    try
    {
        xInt = rSrvcMgr->createInstance(u"com.sun.star.form.component.Form"_ustr);
        if (!xInt.is())
            return;
        xForm.set(xInt, UNO_QUERY);
        SAL_WARN_IF(!xForm.is(), "sw", "no XForm for com.sun.star.form.component.Form?");
        if (!xForm.is())
            return;
    }
    catch (...)
    {
        TOOLS_WARN_EXCEPTION("sw", "");
        return;
    }

    uno::Reference< container::XIndexContainer > xFormComps( xForm, UNO_QUERY );
    m_pFormImpl->SetFormComps( xFormComps );

    uno::Reference< beans::XPropertySet > xFormPropSet( xForm, UNO_QUERY );

    Any aTmp;
    aTmp <<= sName;
    xFormPropSet->setPropertyValue(u"Name"_ustr, aTmp );

    if( !aAction.isEmpty() )
    {
        aAction = URIHelper::SmartRel2Abs(INetURLObject(m_sBaseURL), aAction, Link<OUString *, bool>(), false);
    }
    else
    {
        // use directory at empty URL
        INetURLObject aURLObj( m_aPathToFile );
        aAction = aURLObj.GetPartBeforeLastName();
    }
    aTmp <<= aAction;
    xFormPropSet->setPropertyValue(u"TargetURL"_ustr,
                                    aTmp );

    aTmp <<= nMethod;
    xFormPropSet->setPropertyValue(u"SubmitMethod"_ustr,
                                    aTmp );

    aTmp <<= nEncType;
    xFormPropSet->setPropertyValue(u"SubmitEncoding"_ustr, aTmp );

    if( !sTarget.isEmpty() )
    {
        aTmp <<= sTarget;
        xFormPropSet->setPropertyValue( u"TargetFrame"_ustr, aTmp );
    }

    const uno::Reference< container::XIndexContainer > & rForms =
        m_pFormImpl->GetForms();
    Any aAny( &xForm, cppu::UnoType<XForm>::get());
    rForms->insertByIndex( rForms->getCount(), aAny );
    if( !aMacroTable.empty() )
    {
        bool bHasEvents = lcl_html_setEvents( m_pFormImpl->GetFormEventManager(),
                            rForms->getCount() - 1,
                            aMacroTable, aUnoMacroTable, aUnoMacroParamTable,
                            rDfltScriptType );
        if (bHasEvents)
            NotifyMacroEventRead();
    }
}

void SwHTMLParser::EndForm( bool bAppend )
{
    if( m_pFormImpl && m_pFormImpl->GetFormComps().is() )
    {
        if( bAppend )
        {
            if( m_pPam->GetPoint()->GetContentIndex() )
                AppendTextNode( AM_SPACE );
            else
                AddParSpace();
        }

        m_pFormImpl->ReleaseFormComps();
    }
}

void SwHTMLParser::InsertInput()
{
    assert(m_vPendingStack.empty());

    if( !m_pFormImpl || !m_pFormImpl->GetFormComps().is() )
        return;

    OUString sImgSrc, aId, aClass, aStyle, sName;
    OUString sText;
    SvxMacroTableDtor aMacroTable;
    std::vector<OUString> aUnoMacroTable;
    std::vector<OUString> aUnoMacroParamTable;
    sal_uInt16 nSize = 0;
    sal_Int16 nMaxLen = 0;
    sal_Int16 nChecked = TRISTATE_FALSE;
    sal_Int32 nTabIndex = TABINDEX_MAX + 1;
    HTMLInputType eType = HTMLInputType::Text;
    bool bDisabled = false, bValue = false;
    bool bSetGrfWidth = false, bSetGrfHeight = false;
    bool bHidden = false;
    tools::Long nWidth=0, nHeight=0;
    sal_Int16 eVertOri = text::VertOrientation::TOP;
    sal_Int16 eHoriOri = text::HoriOrientation::NONE;
    SvKeyValueIterator *pHeaderAttrs = m_pFormImpl->GetHeaderAttrs();
    ScriptType eDfltScriptType = GetScriptType( pHeaderAttrs );
    const OUString& rDfltScriptType = GetScriptTypeString( pHeaderAttrs );

    HtmlOptionId nKeepCRLFToken = HtmlOptionId::VALUE;
    const HTMLOptions& rHTMLOptions = GetOptions( &nKeepCRLFToken );
    for (size_t i = rHTMLOptions.size(); i; )
    {
        const HTMLOption& rOption = rHTMLOptions[--i];
        ScriptType eScriptType2 = eDfltScriptType;
        SvMacroItemId nEvent = SvMacroItemId::NONE;
        bool bSetEvent = false;

        switch( rOption.GetToken() )
        {
        case HtmlOptionId::ID:
            aId = rOption.GetString();
            break;
        case HtmlOptionId::STYLE:
            aStyle = rOption.GetString();
            break;
        case HtmlOptionId::CLASS:
            aClass = rOption.GetString();
            break;
        case HtmlOptionId::TYPE:
            eType = rOption.GetInputType();
            break;
        case HtmlOptionId::NAME:
            sName = rOption.GetString();
            break;
        case HtmlOptionId::VALUE:
            sText = rOption.GetString();
            bValue = true;
            break;
        case HtmlOptionId::CHECKED:
            nChecked = TRISTATE_TRUE;
            break;
        case HtmlOptionId::DISABLED:
            bDisabled = true;
            break;
        case HtmlOptionId::MAXLENGTH:
            nMaxLen = static_cast<sal_Int16>(rOption.GetNumber());
            break;
        case HtmlOptionId::SIZE:
            nSize = o3tl::narrowing<sal_uInt16>(rOption.GetNumber());
            break;
        case HtmlOptionId::SRC:
            sImgSrc = rOption.GetString();
            break;
        case HtmlOptionId::WIDTH:
            // only save pixel values at first!
            nWidth = rOption.GetNumber();
            break;
        case HtmlOptionId::HEIGHT:
            // only save pixel values at first!
            nHeight = rOption.GetNumber();
            break;
        case HtmlOptionId::ALIGN:
            eVertOri =
                rOption.GetEnum( aHTMLImgVAlignTable, eVertOri );
            eHoriOri =
                rOption.GetEnum( aHTMLImgHAlignTable, eHoriOri );
            break;
        case HtmlOptionId::TABINDEX:
            // only save pixel values at first!
            nTabIndex = rOption.GetNumber();
            break;

        case HtmlOptionId::SDONFOCUS:
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONFOCUS:
            nEvent = SvMacroItemId::HtmlOnGetFocus;
            bSetEvent = true;
            break;

        case HtmlOptionId::SDONBLUR:               // actually only EDIT
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONBLUR:
            nEvent = SvMacroItemId::HtmlOnLoseFocus;
            bSetEvent = true;
            break;

        case HtmlOptionId::SDONCLICK:
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONCLICK:
            nEvent = SvMacroItemId::HtmlOnClick;
            bSetEvent = true;
            break;

        case HtmlOptionId::SDONCHANGE:             // actually only EDIT
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONCHANGE:
            nEvent = SvMacroItemId::HtmlOnChange;
            bSetEvent = true;
            break;

        case HtmlOptionId::SDONSELECT:             // actually only EDIT
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONSELECT:
            nEvent = SvMacroItemId::HtmlOnSelect;
            bSetEvent = true;
            break;

        default:
            lcl_html_getEvents( rOption.GetTokenString(),
                                rOption.GetString(),
                                aUnoMacroTable, aUnoMacroParamTable );
            break;
        }

        if( bSetEvent )
        {
            OUString sEvent( rOption.GetString() );
            if( !sEvent.isEmpty() )
            {
                sEvent = convertLineEnd(sEvent, GetSystemLineEnd());
                OUString aScriptType2;
                if( EXTENDED_STYPE==eScriptType2 )
                    aScriptType2 = rDfltScriptType;
                aMacroTable.Insert( nEvent, SvxMacro( sEvent, aScriptType2, eScriptType2 ) );
            }
        }
    }

    if( HTMLInputType::Image==eType )
    {
        // Image controls without image URL are ignored (same as MS)
        if( sImgSrc.isEmpty() )
            return;
    }
    else
    {
        // evaluation of ALIGN for all controls is not a good idea as long as
        // paragraph bound controls do not influence the height of the cells of a table
        eVertOri = text::VertOrientation::TOP;
        eHoriOri = text::HoriOrientation::NONE;
    }

    // Default is HTMLInputType::Text
    const char *pType = "TextField";
    bool bKeepCRLFInValue = false;
    switch( eType )
    {
    case HTMLInputType::Checkbox:
        pType = "CheckBox";
        bKeepCRLFInValue = true;
        break;

    case HTMLInputType::Radio:
        pType = "RadioButton";
        bKeepCRLFInValue = true;
        break;

    case HTMLInputType::Password:
        bKeepCRLFInValue = true;
        break;

    case HTMLInputType::Button:
        bKeepCRLFInValue = true;
        [[fallthrough]];
    case HTMLInputType::Submit:
    case HTMLInputType::Reset:
        pType = "CommandButton";
        break;

    case HTMLInputType::Image:
        pType = "ImageButton";
        break;

    case HTMLInputType::File:
        pType = "FileControl";
        break;

    case HTMLInputType::Hidden:
        pType = "HiddenControl";
        bKeepCRLFInValue = true;
        break;
    default:
        ;
    }

    // For some controls CR/LF has to be deleted from VALUE
    if( !bKeepCRLFInValue )
    {
        sText = sText.replaceAll("\r", "").replaceAll("\n", "");
    }

    const uno::Reference< XMultiServiceFactory > & rServiceFactory =
        m_pFormImpl->GetServiceFactory();
    if( !rServiceFactory.is() )
        return;

    OUString sServiceName = "com.sun.star.form.component." +
        OUString::createFromAscii(pType);
    uno::Reference< XInterface > xInt =
        rServiceFactory->createInstance( sServiceName );
    if( !xInt.is() )
        return;

    uno::Reference< XFormComponent > xFComp( xInt, UNO_QUERY );
    if( !xFComp.is() )
        return;

    uno::Reference< beans::XPropertySet > xPropSet( xFComp, UNO_QUERY );

    Any aTmp;
    aTmp <<= sName;
    xPropSet->setPropertyValue(u"Name"_ustr, aTmp );

    if( HTMLInputType::Hidden != eType  )
    {
        if( nTabIndex >= TABINDEX_MIN && nTabIndex <= TABINDEX_MAX  )
        {
            aTmp <<= static_cast<sal_Int16>(nTabIndex) ;
            xPropSet->setPropertyValue(u"TabIndex"_ustr, aTmp );
        }

        if( bDisabled )
        {
            xPropSet->setPropertyValue(u"Enabled"_ustr, Any(false) );
        }
    }

    aTmp <<= sText;

    Size aSz( 0, 0 );       // defaults
    Size aTextSz( 0, 0 );   // Text size
    bool bMinWidth = false, bMinHeight = false;
    bool bUseSize = false;
    switch( eType )
    {
    case HTMLInputType::Checkbox:
    case HTMLInputType::Radio:
        {
            if( !bValue )
                aTmp <<= u"" OOO_STRING_SVTOOLS_HTML_on ""_ustr;
            xPropSet->setPropertyValue(u"RefValue"_ustr,
                                        aTmp );
            aTmp <<= OUString();
            xPropSet->setPropertyValue(u"Label"_ustr,
                                        aTmp );
            // RadioButton: The DefaultChecked property should only be set
            // if the control has been created and activateTabOrder has been called
            // because otherwise it would still belong to the previous group.
            if( HTMLInputType::Checkbox == eType )
            {
                aTmp <<= nChecked ;
                xPropSet->setPropertyValue(u"DefaultState"_ustr, aTmp );
            }

            const SvxMacro* pMacro = aMacroTable.Get( SvMacroItemId::HtmlOnClick );
            if( pMacro )
            {
                aMacroTable.Insert( SvMacroItemId::HtmlOnClickItem, *pMacro );
                aMacroTable.Erase( SvMacroItemId::HtmlOnClick );
            }
            // evaluating SIZE shouldn't be necessary here?
            bMinWidth = bMinHeight = true;
        }
        break;

    case HTMLInputType::Image:
        {
            // SIZE = WIDTH
            aSz.setWidth(o3tl::convert(nWidth, o3tl::Length::px, o3tl::Length::mm100));
            aSz.setHeight(o3tl::convert(nHeight, o3tl::Length::px, o3tl::Length::mm100));
            aTmp <<= FormButtonType_SUBMIT;
            xPropSet->setPropertyValue(u"ButtonType"_ustr, aTmp );

            aTmp <<= sal_Int16(0)  ;
            xPropSet->setPropertyValue(u"Border"_ustr,
                                        aTmp );
        }
        break;

    case HTMLInputType::Button:
    case HTMLInputType::Submit:
    case HTMLInputType::Reset:
        {
            FormButtonType eButtonType;
            switch( eType )
            {
            case HTMLInputType::Button:
                eButtonType = FormButtonType_PUSH;
                break;
            case HTMLInputType::Submit:
                eButtonType = FormButtonType_SUBMIT;
                if (sText.isEmpty())
                    sText = OOO_STRING_SVTOOLS_HTML_IT_submit;
                break;
            case HTMLInputType::Reset:
                eButtonType = FormButtonType_RESET;
                if (sText.isEmpty())
                    sText = OOO_STRING_SVTOOLS_HTML_IT_reset;
                break;
            default:
                ;
            }
            aTmp <<= sText;
            xPropSet->setPropertyValue(u"Label"_ustr,
                                        aTmp );

            aTmp <<= eButtonType;
            xPropSet->setPropertyValue(u"ButtonType"_ustr, aTmp );

            bMinWidth = bMinHeight = true;
            bUseSize = true;
        }
        break;

    case HTMLInputType::Password:
    case HTMLInputType::Text:
    case HTMLInputType::File:
        if( HTMLInputType::File != eType )
        {
            // The VALUE of file control will be ignored for security reasons
            xPropSet->setPropertyValue(u"DefaultText"_ustr, aTmp );
            if( nMaxLen != 0 )
            {
                aTmp <<= nMaxLen ;
                xPropSet->setPropertyValue(u"MaxTextLen"_ustr, aTmp );
            }
        }

        if( HTMLInputType::Password == eType )
        {
            aTmp <<= sal_Int16('*') ;
            xPropSet->setPropertyValue(u"EchoChar"_ustr, aTmp );
        }

        lcl_html_setFixedFontProperty( xPropSet );

        if( !nSize )
            nSize = 20;
        aTextSz.setWidth( nSize );
        bMinHeight = true;
        break;

    case HTMLInputType::Hidden:
        xPropSet->setPropertyValue(u"HiddenValue"_ustr, aTmp );
        bHidden = true;
        break;
    default:
        ;
    }

    if( bUseSize && nSize>0 )
    {
        aSz.setWidth(o3tl::convert(nSize, o3tl::Length::px, o3tl::Length::mm100));
        OSL_ENSURE( !aTextSz.Width(), "text width is present" );
        bMinWidth = false;
    }

    SfxItemSet aCSS1ItemSet( m_xDoc->GetAttrPool(), m_pCSS1Parser->GetWhichMap() );
    SvxCSS1PropertyInfo aCSS1PropInfo;
    if( HasStyleOptions( aStyle, aId, aClass ) )
    {
        (void)ParseStyleOptions(aStyle, aId, aClass, aCSS1ItemSet, aCSS1PropInfo);
        if( !aId.isEmpty() )
            InsertBookmark( aId );
    }

    if( SVX_CSS1_LTYPE_TWIP== aCSS1PropInfo.m_eWidthType )
    {
        aSz.setWidth( convertTwipToMm100( aCSS1PropInfo.m_nWidth ) );
        aTextSz.setWidth( 0 );
        bMinWidth = false;
    }
    if( SVX_CSS1_LTYPE_TWIP== aCSS1PropInfo.m_eHeightType )
    {
        aSz.setHeight( convertTwipToMm100( aCSS1PropInfo.m_nHeight ) );
        aTextSz.setHeight( 0 );
        bMinHeight = false;
    }

    // Set sensible default values if the image button has no valid size
    if( HTMLInputType::Image== eType )
    {
        if( !aSz.Width() )
        {
            aSz.setWidth( HTML_DFLT_IMG_WIDTH );
            bSetGrfWidth = true;
            if (m_xTable)
                IncGrfsThatResizeTable();
        }
        if( !aSz.Height() )
        {
            aSz.setHeight( HTML_DFLT_IMG_HEIGHT );
            bSetGrfHeight = true;
        }
    }
    if( aSz.Width() < MINFLY )
        aSz.setWidth( MINFLY );
    if( aSz.Height() < MINFLY )
        aSz.setHeight( MINFLY );

    uno::Reference< drawing::XShape > xShape = InsertControl(
                                             xFComp, xPropSet, aSz,
                                             eVertOri, eHoriOri,
                                             aCSS1ItemSet, aCSS1PropInfo,
                                             aMacroTable, aUnoMacroTable,
                                             aUnoMacroParamTable, false,
                                             bHidden );
    if( aTextSz.Width() || aTextSz.Height() || bMinWidth || bMinHeight )
    {
        OSL_ENSURE( !(bSetGrfWidth || bSetGrfHeight), "Adjust graphic size???" );
        SetControlSize( xShape, aTextSz, bMinWidth, bMinHeight );
    }

    if( HTMLInputType::Radio == eType )
    {
        aTmp <<= nChecked ;
        xPropSet->setPropertyValue(u"DefaultState"_ustr, aTmp );
    }

    if( HTMLInputType::Image == eType )
    {
        // Set the URL after inserting the graphic because the Download can
        // only register with XModel after the control has been inserted.
        aTmp <<= URIHelper::SmartRel2Abs(INetURLObject(m_sBaseURL), sImgSrc, Link<OUString *, bool>(), false);
        xPropSet->setPropertyValue(u"ImageURL"_ustr,
                                    aTmp );
    }

    if( bSetGrfWidth || bSetGrfHeight )
    {
        rtl::Reference<SwHTMLImageWatcher> pWatcher =
            new SwHTMLImageWatcher( xShape, bSetGrfWidth, bSetGrfHeight );
        pWatcher->start();
    }
}

void SwHTMLParser::NewTextArea()
{
    assert(m_vPendingStack.empty());

    OSL_ENSURE( !m_bTextArea, "TextArea in TextArea?" );
    OSL_ENSURE( !m_pFormImpl || !m_pFormImpl->GetFCompPropSet().is(),
            "TextArea in Control?" );

    if( !m_pFormImpl || !m_pFormImpl->GetFormComps().is() )
    {
        // Close special treatment for TextArea in the parser
        FinishTextArea();
        return;
    }

    OUString aId, aClass, aStyle;
    OUString sName;
    sal_Int32 nTabIndex = TABINDEX_MAX + 1;
    SvxMacroTableDtor aMacroTable;
    std::vector<OUString> aUnoMacroTable;
    std::vector<OUString> aUnoMacroParamTable;
    sal_uInt16 nRows = 0, nCols = 0;
    HTMLWordWrapMode nWrap = HTML_WM_OFF;
    bool bDisabled = false;
    SvKeyValueIterator *pHeaderAttrs = m_pFormImpl->GetHeaderAttrs();
    ScriptType eDfltScriptType = GetScriptType( pHeaderAttrs );
    const OUString& rDfltScriptType = GetScriptTypeString( pHeaderAttrs );

    const HTMLOptions& rHTMLOptions = GetOptions();
    for (size_t i = rHTMLOptions.size(); i; )
    {
        const HTMLOption& rOption = rHTMLOptions[--i];
        ScriptType eScriptType2 = eDfltScriptType;
        SvMacroItemId nEvent = SvMacroItemId::NONE;
        bool bSetEvent = false;

        switch( rOption.GetToken() )
        {
        case HtmlOptionId::ID:
            aId = rOption.GetString();
            break;
        case HtmlOptionId::STYLE:
            aStyle = rOption.GetString();
            break;
        case HtmlOptionId::CLASS:
            aClass = rOption.GetString();
            break;
        case HtmlOptionId::NAME:
            sName = rOption.GetString();
            break;
        case HtmlOptionId::DISABLED:
            bDisabled = true;
            break;
        case HtmlOptionId::ROWS:
            nRows = o3tl::narrowing<sal_uInt16>(rOption.GetNumber());
            break;
        case HtmlOptionId::COLS:
            nCols = o3tl::narrowing<sal_uInt16>(rOption.GetNumber());
            break;
        case HtmlOptionId::WRAP:
            nWrap = rOption.GetEnum( aHTMLTextAreaWrapTable, nWrap );
            break;

        case HtmlOptionId::TABINDEX:
            nTabIndex = rOption.GetSNumber();
            break;

        case HtmlOptionId::SDONFOCUS:
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONFOCUS:
            nEvent = SvMacroItemId::HtmlOnGetFocus;
            bSetEvent = true;
            break;

        case HtmlOptionId::SDONBLUR:
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONBLUR:
            nEvent = SvMacroItemId::HtmlOnLoseFocus;
            bSetEvent = true;
            break;

        case HtmlOptionId::SDONCLICK:
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONCLICK:
            nEvent = SvMacroItemId::HtmlOnClick;
            bSetEvent = true;
            break;

        case HtmlOptionId::SDONCHANGE:
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONCHANGE:
            nEvent = SvMacroItemId::HtmlOnChange;
            bSetEvent = true;
            break;

        case HtmlOptionId::SDONSELECT:
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONSELECT:
            nEvent = SvMacroItemId::HtmlOnSelect;
            bSetEvent = true;
            break;

        default:
            lcl_html_getEvents( rOption.GetTokenString(),
                                rOption.GetString(),
                                aUnoMacroTable, aUnoMacroParamTable );
            break;
        }

        if( bSetEvent )
        {
            OUString sEvent( rOption.GetString() );
            if( !sEvent.isEmpty() )
            {
                sEvent = convertLineEnd(sEvent, GetSystemLineEnd());
                if( EXTENDED_STYPE==eScriptType2 )
                    m_aScriptType = rDfltScriptType;
                aMacroTable.Insert( nEvent, SvxMacro( sEvent, m_aScriptType, eScriptType2 ) );
            }
        }
    }

    const uno::Reference< lang::XMultiServiceFactory > & rSrvcMgr =
        m_pFormImpl->GetServiceFactory();
    if( !rSrvcMgr.is() )
    {
        FinishTextArea();
        return;
    }
    uno::Reference< uno::XInterface >  xInt = rSrvcMgr->createInstance(
        u"com.sun.star.form.component.TextField"_ustr );
    if( !xInt.is() )
    {
        FinishTextArea();
        return;
    }

    uno::Reference< XFormComponent > xFComp( xInt, UNO_QUERY );
    OSL_ENSURE( xFComp.is(), "no FormComponent?" );

    uno::Reference< beans::XPropertySet > xPropSet( xFComp, UNO_QUERY );

    Any aTmp;
    aTmp <<= sName;
    xPropSet->setPropertyValue(u"Name"_ustr, aTmp );

    aTmp <<= true;
    xPropSet->setPropertyValue(u"MultiLine"_ustr, aTmp );
    xPropSet->setPropertyValue(u"VScroll"_ustr, aTmp );
    if( HTML_WM_OFF == nWrap )
        xPropSet->setPropertyValue(u"HScroll"_ustr, aTmp );
    if( HTML_WM_HARD == nWrap )
        xPropSet->setPropertyValue(u"HardLineBreaks"_ustr, aTmp );

    if( nTabIndex >= TABINDEX_MIN && nTabIndex <= TABINDEX_MAX  )
    {
        aTmp <<= static_cast<sal_Int16>(nTabIndex) ;
        xPropSet->setPropertyValue(u"TabIndex"_ustr, aTmp );
    }

    lcl_html_setFixedFontProperty( xPropSet );

    if( bDisabled )
    {
        xPropSet->setPropertyValue(u"Enabled"_ustr, Any(false) );
    }

    OSL_ENSURE( m_pFormImpl->GetText().isEmpty(), "Text is not empty!" );

    if( !nCols )
        nCols = 20;
    if( !nRows )
        nRows = 1;

    Size aTextSz( nCols, nRows );

    SfxItemSet aCSS1ItemSet( m_xDoc->GetAttrPool(), m_pCSS1Parser->GetWhichMap() );
    SvxCSS1PropertyInfo aCSS1PropInfo;
    if( HasStyleOptions( aStyle, aId, aClass ) )
    {
        (void)ParseStyleOptions(aStyle, aId, aClass, aCSS1ItemSet, aCSS1PropInfo);
        if( !aId.isEmpty() )
            InsertBookmark( aId );
    }

    Size aSz( MINFLY, MINFLY );
    if( SVX_CSS1_LTYPE_TWIP== aCSS1PropInfo.m_eWidthType )
    {
        aSz.setWidth( convertTwipToMm100( aCSS1PropInfo.m_nWidth ) );
        aTextSz.setWidth( 0 );
    }
    if( SVX_CSS1_LTYPE_TWIP== aCSS1PropInfo.m_eHeightType )
    {
        aSz.setHeight( convertTwipToMm100( aCSS1PropInfo.m_nHeight ) );
        aTextSz.setHeight( 0 );
    }
    if( aSz.Width() < MINFLY )
        aSz.setWidth( MINFLY );
    if( aSz.Height() < MINFLY )
        aSz.setHeight( MINFLY );

    uno::Reference< drawing::XShape > xShape = InsertControl( xFComp, xPropSet, aSz,
                                      text::VertOrientation::TOP, text::HoriOrientation::NONE,
                                      aCSS1ItemSet, aCSS1PropInfo,
                                      aMacroTable, aUnoMacroTable,
                                      aUnoMacroParamTable );
    if( aTextSz.Width() || aTextSz.Height() )
        SetControlSize( xShape, aTextSz, false, false );

    // create new context
    std::unique_ptr<HTMLAttrContext> xCntxt(new HTMLAttrContext(HtmlTokenId::TEXTAREA_ON));

    // temporarily disable PRE/Listing/XMP
    SplitPREListingXMP(xCntxt.get());
    PushContext(xCntxt);

    m_bTextArea = true;
    m_bTAIgnoreNewPara = true;
}

void SwHTMLParser::EndTextArea()
{
    OSL_ENSURE( m_bTextArea, "no TextArea or wrong type" );
    assert(m_pFormImpl && m_pFormImpl->GetFCompPropSet().is() &&
            "TextArea missing");

    const uno::Reference< beans::XPropertySet > & rPropSet =
        m_pFormImpl->GetFCompPropSet();

    Any aTmp;
    aTmp <<= m_pFormImpl->GetText();
    rPropSet->setPropertyValue(u"DefaultText"_ustr, aTmp );
    m_pFormImpl->EraseText();

    m_pFormImpl->ReleaseFCompPropSet();

    // get context
    std::unique_ptr<HTMLAttrContext> xCntxt(PopContext(HtmlTokenId::TEXTAREA_ON));
    if (xCntxt)
    {
        // end attributes
        EndContext(xCntxt.get());
    }

    m_bTextArea = false;
}

void SwHTMLParser::InsertTextAreaText( HtmlTokenId nToken )
{
    OSL_ENSURE( m_bTextArea, "no TextArea or wrong type" );
    OSL_ENSURE( m_pFormImpl && m_pFormImpl->GetFCompPropSet().is(),
            "TextArea missing" );

    OUString& rText = m_pFormImpl->GetText();
    switch( nToken)
    {
    case HtmlTokenId::TEXTTOKEN:
        rText += aToken;
        break;
    case HtmlTokenId::NEWPARA:
        if( !m_bTAIgnoreNewPara )
            rText += "\n";
        break;
    default:
        rText += "<";
        rText += sSaveToken;
        if( !aToken.isEmpty() )
        {
            rText += " ";
            rText += aToken;
        }
        rText += ">";
    }

    m_bTAIgnoreNewPara = false;
}

void SwHTMLParser::NewSelect()
{
    assert(m_vPendingStack.empty());

    OSL_ENSURE( !m_bSelect, "Select in Select?" );
    OSL_ENSURE( !m_pFormImpl || !m_pFormImpl->GetFCompPropSet().is(),
            "Select in Control?" );

    if( !m_pFormImpl || !m_pFormImpl->GetFormComps().is() )
        return;

    OUString aId, aClass, aStyle;
    OUString sName;
    sal_Int32 nTabIndex = TABINDEX_MAX + 1;
    SvxMacroTableDtor aMacroTable;
    std::vector<OUString> aUnoMacroTable;
    std::vector<OUString> aUnoMacroParamTable;
    bool bMultiple = false;
    bool bDisabled = false;
    m_nSelectEntryCnt = 1;
    SvKeyValueIterator *pHeaderAttrs = m_pFormImpl->GetHeaderAttrs();
    ScriptType eDfltScriptType = GetScriptType( pHeaderAttrs );
    const OUString& rDfltScriptType = GetScriptTypeString( pHeaderAttrs );

    const HTMLOptions& rHTMLOptions = GetOptions();
    for (size_t i = rHTMLOptions.size(); i; )
    {
        const HTMLOption& rOption = rHTMLOptions[--i];
        ScriptType eScriptType2 = eDfltScriptType;
        SvMacroItemId nEvent = SvMacroItemId::NONE;
        bool bSetEvent = false;

        switch( rOption.GetToken() )
        {
        case HtmlOptionId::ID:
            aId = rOption.GetString();
            break;
        case HtmlOptionId::STYLE:
            aStyle = rOption.GetString();
            break;
        case HtmlOptionId::CLASS:
            aClass = rOption.GetString();
            break;
        case HtmlOptionId::NAME:
            sName = rOption.GetString();
            break;
        case HtmlOptionId::MULTIPLE:
            bMultiple = true;
            break;
        case HtmlOptionId::DISABLED:
            bDisabled = true;
            break;
        case HtmlOptionId::SIZE:
            m_nSelectEntryCnt = o3tl::narrowing<sal_uInt16>(rOption.GetNumber());
            break;

        case HtmlOptionId::TABINDEX:
            nTabIndex = rOption.GetSNumber();
            break;

        case HtmlOptionId::SDONFOCUS:
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONFOCUS:
            nEvent = SvMacroItemId::HtmlOnGetFocus;
            bSetEvent = true;
            break;

        case HtmlOptionId::SDONBLUR:
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONBLUR:
            nEvent = SvMacroItemId::HtmlOnLoseFocus;
            bSetEvent = true;
            break;

        case HtmlOptionId::SDONCLICK:
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONCLICK:
            nEvent = SvMacroItemId::HtmlOnClick;
            bSetEvent = true;
            break;

        case HtmlOptionId::SDONCHANGE:
            eScriptType2 = STARBASIC;
            [[fallthrough]];
        case HtmlOptionId::ONCHANGE:
            nEvent = SvMacroItemId::HtmlOnChange;
            bSetEvent = true;
            break;

        default:
            lcl_html_getEvents( rOption.GetTokenString(),
                                rOption.GetString(),
                                aUnoMacroTable, aUnoMacroParamTable );
            break;
        }

        if( bSetEvent )
        {
            OUString sEvent( rOption.GetString() );
            if( !sEvent.isEmpty() )
            {
                sEvent = convertLineEnd(sEvent, GetSystemLineEnd());
                if( EXTENDED_STYPE==eScriptType2 )
                    m_aScriptType = rDfltScriptType;
                aMacroTable.Insert( nEvent, SvxMacro( sEvent, m_aScriptType, eScriptType2 ) );
            }
        }
    }

    const uno::Reference< lang::XMultiServiceFactory > & rSrvcMgr =
        m_pFormImpl->GetServiceFactory();
    if( !rSrvcMgr.is() )
    {
        FinishTextArea();
        return;
    }
    uno::Reference< uno::XInterface >  xInt = rSrvcMgr->createInstance(
        u"com.sun.star.form.component.ListBox"_ustr );
    if( !xInt.is() )
    {
        FinishTextArea();
        return;
    }

    uno::Reference< XFormComponent > xFComp( xInt, UNO_QUERY );
    OSL_ENSURE(xFComp.is(), "no FormComponent?");

    uno::Reference< beans::XPropertySet >  xPropSet( xFComp, UNO_QUERY );

    Any aTmp;
    aTmp <<= sName;
    xPropSet->setPropertyValue(u"Name"_ustr, aTmp );

    if( nTabIndex >= TABINDEX_MIN && nTabIndex <= TABINDEX_MAX  )
    {
        aTmp <<= static_cast<sal_Int16>(nTabIndex) ;
        xPropSet->setPropertyValue(u"TabIndex"_ustr, aTmp );
    }

    if( bDisabled )
    {
        xPropSet->setPropertyValue(u"Enabled"_ustr, Any(false) );
    }

    Size aTextSz( 0, 0 );
    bool bMinWidth = true, bMinHeight = true;
    if( !bMultiple && 1==m_nSelectEntryCnt )
    {
        xPropSet->setPropertyValue(u"Dropdown"_ustr, Any(true) );
    }
    else
    {
        if( m_nSelectEntryCnt <= 1 )      // 4 lines is default
            m_nSelectEntryCnt = 4;

        if( bMultiple )
        {
            xPropSet->setPropertyValue(u"MultiSelection"_ustr, Any(true) );
        }
        aTextSz.setHeight( m_nSelectEntryCnt );
        bMinHeight = false;
    }

    SfxItemSet aCSS1ItemSet( m_xDoc->GetAttrPool(), m_pCSS1Parser->GetWhichMap() );
    SvxCSS1PropertyInfo aCSS1PropInfo;
    if( HasStyleOptions( aStyle, aId, aClass ) )
    {
        (void)ParseStyleOptions(aStyle, aId, aClass, aCSS1ItemSet, aCSS1PropInfo);
        if( !aId.isEmpty() )
            InsertBookmark( aId );
    }

    Size aSz( MINFLY, MINFLY );
    m_bFixSelectWidth = true;
    if( SVX_CSS1_LTYPE_TWIP== aCSS1PropInfo.m_eWidthType )
    {
        aSz.setWidth( convertTwipToMm100( aCSS1PropInfo.m_nWidth ) );
        m_bFixSelectWidth = false;
        bMinWidth = false;
    }
    if( SVX_CSS1_LTYPE_TWIP== aCSS1PropInfo.m_eHeightType )
    {
        aSz.setHeight( convertTwipToMm100( aCSS1PropInfo.m_nHeight ) );
        aTextSz.setHeight( 0 );
        bMinHeight = false;
    }
    if( aSz.Width() < MINFLY )
        aSz.setWidth( MINFLY );
    if( aSz.Height() < MINFLY )
        aSz.setHeight( MINFLY );

    uno::Reference< drawing::XShape >  xShape = InsertControl( xFComp, xPropSet, aSz,
                                      text::VertOrientation::TOP, text::HoriOrientation::NONE,
                                      aCSS1ItemSet, aCSS1PropInfo,
                                      aMacroTable, aUnoMacroTable,
                                      aUnoMacroParamTable );
    if( m_bFixSelectWidth )
        m_pFormImpl->SetShape( xShape );
    if( aTextSz.Height() || bMinWidth || bMinHeight )
        SetControlSize( xShape, aTextSz, bMinWidth, bMinHeight );

    // create new context
    std::unique_ptr<HTMLAttrContext> xCntxt(new HTMLAttrContext(HtmlTokenId::SELECT_ON));

    // temporarily disable PRE/Listing/XMP
    SplitPREListingXMP(xCntxt.get());
    PushContext(xCntxt);

    m_bSelect = true;
}

void SwHTMLParser::EndSelect()
{
    assert(m_vPendingStack.empty());

    OSL_ENSURE( m_bSelect, "no Select" );
    assert(m_pFormImpl && m_pFormImpl->GetFCompPropSet().is() &&
            "no select control");

    const uno::Reference< beans::XPropertySet > & rPropSet =
        m_pFormImpl->GetFCompPropSet();

    size_t nEntryCnt = m_pFormImpl->GetStringList().size();
    if(!m_pFormImpl->GetStringList().empty())
    {
        Sequence<OUString> aList( static_cast<sal_Int32>(nEntryCnt) );
        Sequence<OUString> aValueList( static_cast<sal_Int32>(nEntryCnt) );
        OUString *pStrings = aList.getArray();
        OUString *pValues = aValueList.getArray();

        for(size_t i = 0; i < nEntryCnt; ++i)
        {
            OUString sText(m_pFormImpl->GetStringList()[i]);
            sText = comphelper::string::stripEnd(sText, ' ');
            pStrings[i] = sText;

            sText = m_pFormImpl->GetValueList()[i];
            pValues[i] = sText;
        }

        rPropSet->setPropertyValue(u"StringItemList"_ustr, Any(aList) );

        rPropSet->setPropertyValue(u"ListSourceType"_ustr, Any(ListSourceType_VALUELIST) );

        rPropSet->setPropertyValue(u"ListSource"_ustr, Any(aValueList) );

        size_t nSelCnt = m_pFormImpl->GetSelectedList().size();
        if( !nSelCnt && 1 == m_nSelectEntryCnt && nEntryCnt )
        {
            // In a dropdown list an entry should always be selected.
            m_pFormImpl->GetSelectedList().insert( m_pFormImpl->GetSelectedList().begin(), 0 );
            nSelCnt = 1;
        }
        Sequence<sal_Int16> aSelList( static_cast<sal_Int32>(nSelCnt) );
        sal_Int16 *pSels = aSelList.getArray();
        for(size_t i = 0; i < nSelCnt; ++i)
        {
            pSels[i] = static_cast<sal_Int16>(m_pFormImpl->GetSelectedList()[i]);
        }
        rPropSet->setPropertyValue(u"DefaultSelection"_ustr, Any(aSelList) );

        m_pFormImpl->EraseStringList();
        m_pFormImpl->EraseValueList();
    }

    m_pFormImpl->EraseSelectedList();

    if( m_bFixSelectWidth )
    {
        OSL_ENSURE( m_pFormImpl->GetShape().is(), "Shape not saved" );
        Size aTextSz( -1, 0 );
        SetControlSize( m_pFormImpl->GetShape(), aTextSz, false, false );
    }

    m_pFormImpl->ReleaseFCompPropSet();

    // get context
    std::unique_ptr<HTMLAttrContext> xCntxt(PopContext(HtmlTokenId::SELECT_ON));
    if (xCntxt)
    {
        // close attributes
        EndContext(xCntxt.get());
    }

    m_bSelect = false;
}

void SwHTMLParser::InsertSelectOption()
{
    OSL_ENSURE( m_bSelect, "no Select" );
    OSL_ENSURE( m_pFormImpl && m_pFormImpl->GetFCompPropSet().is(),
            "no Select-Control" );

    m_bLBEntrySelected = false;
    OUString aValue;

    const HTMLOptions& rHTMLOptions = GetOptions();
    for (size_t i = rHTMLOptions.size(); i; )
    {
        const HTMLOption& rOption = rHTMLOptions[--i];
        switch( rOption.GetToken() )
        {
        case HtmlOptionId::ID:
            // leave out for now
            break;
        case HtmlOptionId::SELECTED:
            m_bLBEntrySelected = true;
            break;
        case HtmlOptionId::VALUE:
            aValue = rOption.GetString();
            if( aValue.isEmpty() )
                aValue = "$$$empty$$$";
            break;
        default: break;
        }
    }

    sal_uInt16 nEntryCnt = m_pFormImpl->GetStringList().size();
    m_pFormImpl->GetStringList().push_back(OUString());
    m_pFormImpl->GetValueList().push_back(aValue);
    if( m_bLBEntrySelected )
    {
        m_pFormImpl->GetSelectedList().push_back( nEntryCnt );
    }
}

void SwHTMLParser::InsertSelectText()
{
    OSL_ENSURE( m_bSelect, "no select" );
    OSL_ENSURE( m_pFormImpl && m_pFormImpl->GetFCompPropSet().is(),
            "no select control" );

    if(m_pFormImpl->GetStringList().empty())
        return;

    OUString& rText = m_pFormImpl->GetStringList().back();

    if( !aToken.isEmpty() && ' '==aToken[ 0 ] )
    {
        sal_Int32 nLen = rText.getLength();
        if( !nLen || ' '==rText[nLen-1])
            aToken.remove( 0, 1 );
    }
    if( !aToken.isEmpty() )
        rText += aToken;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
