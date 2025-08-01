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

#include "PresenterController.hxx"

#include "PresenterAccessibility.hxx"
#include "PresenterCanvasHelper.hxx"
#include "PresenterCurrentSlideObserver.hxx"
#include "PresenterScreen.hxx"
#include "PresenterPaintManager.hxx"
#include "PresenterPaneBase.hxx"
#include "PresenterPaneContainer.hxx"
#include "PresenterPaneBorderPainter.hxx"
#include "PresenterTheme.hxx"
#include "PresenterViewFactory.hxx"
#include "PresenterWindowManager.hxx"
#include <DrawController.hxx>
#include <framework/ConfigurationController.hxx>
#include <framework/ConfigurationChangeEvent.hxx>
#include <framework/Pane.hxx>
#include <ResourceId.hxx>

#include <com/sun/star/awt/Key.hpp>
#include <com/sun/star/awt/KeyModifier.hpp>
#include <com/sun/star/awt/MouseButton.hpp>
#include <com/sun/star/container/XNamed.hpp>
#include <com/sun/star/drawing/XDrawView.hpp>
#include <com/sun/star/drawing/XDrawPagesSupplier.hpp>
#include <com/sun/star/frame/FrameSearchFlag.hpp>
#include <com/sun/star/frame/XDispatchProvider.hpp>
#include <com/sun/star/presentation/AnimationEffect.hpp>
#include <com/sun/star/presentation/XPresentation.hpp>
#include <com/sun/star/presentation/XPresentationSupplier.hpp>
#include <com/sun/star/rendering/TextDirection.hpp>
#include <com/sun/star/util/URLTransformer.hpp>

#include <rtl/ustrbuf.hxx>
#include <svx/unoapi.hxx>
#include <utility>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::presentation;
using namespace ::com::sun::star::drawing::framework;

namespace sdext::presenter {

IPresentationTime::~IPresentationTime()
{
}

PresenterController::InstanceContainer PresenterController::maInstances;

::rtl::Reference<PresenterController> PresenterController::Instance (
    const css::uno::Reference<css::frame::XFrame>& rxFrame)
{
    InstanceContainer::const_iterator iInstance (maInstances.find(rxFrame));
    if (iInstance != maInstances.end())
        return iInstance->second;
    else
        return ::rtl::Reference<PresenterController>();
}

PresenterController::PresenterController (
    unotools::WeakReference<PresenterScreen> xScreen,
    const Reference<XComponentContext>& rxContext,
    const rtl::Reference<::sd::DrawController>& rxController,
    const Reference<presentation::XSlideShowController>& rxSlideShowController,
    rtl::Reference<PresenterPaneContainer> xPaneContainer,
    const rtl::Reference<sd::framework::ResourceId>& rxMainPaneId)
    : mxScreen(std::move(xScreen)),
      mxComponentContext(rxContext),
      mxController(rxController),
      mxSlideShowController(rxSlideShowController),
      mxMainPaneId(rxMainPaneId),
      mpPaneContainer(std::move(xPaneContainer)),
      mnCurrentSlideIndex(-1),
      mpWindowManager(new PresenterWindowManager(rxContext,mpPaneContainer,this)),
      mpCanvasHelper(std::make_shared<PresenterCanvasHelper>()),
      mnPendingSlideNumber(-1)
{
    OSL_ASSERT(mxController.is());

    if ( ! mxSlideShowController.is())
        throw lang::IllegalArgumentException(
            u"missing slide show controller"_ustr,
            static_cast<XWeak*>(this),
            2);

    new PresenterCurrentSlideObserver(this,rxSlideShowController);

    // Listen for configuration changes.
    mxConfigurationController = mxController->getConfigurationController();
    if (mxConfigurationController.is())
    {
        mxConfigurationController->addConfigurationChangeListener(
            this,
            sd::framework::ConfigurationChangeEventType::ResourceActivation);
        mxConfigurationController->addConfigurationChangeListener(
            this,
            sd::framework::ConfigurationChangeEventType::ResourceDeactivation);
        mxConfigurationController->addConfigurationChangeListener(
            this,
            sd::framework::ConfigurationChangeEventType::ConfigurationUpdateEnd);
    }

    // Listen for the frame being activated.
    Reference<frame::XFrame> xFrame (mxController->getFrame());
    if (xFrame.is())
        xFrame->addFrameActionListener(this);

    // Create the border painter.
    mpPaneBorderPainter = new PresenterPaneBorderPainter(rxContext);
    mpWindowManager->SetPaneBorderPainter(mpPaneBorderPainter);

    mxSlideShowController->activate();
    Reference<beans::XPropertySet> xProperties (mxSlideShowController, UNO_QUERY);
    if (xProperties.is())
    {
        Reference<awt::XWindow> xWindow (
            xProperties->getPropertyValue(u"ParentWindow"_ustr), UNO_QUERY);
        if (xWindow.is())
            xWindow->addKeyListener(this);
    }

    UpdateCurrentSlide(0);

    maInstances[mxController->getFrame()] = this;

    // Create a URLTransformer.
    mxUrlTransformer.set(util::URLTransformer::create(mxComponentContext));
}

PresenterController::~PresenterController()
{
}

void PresenterController::disposing(std::unique_lock<std::mutex>&)
{
    maInstances.erase(mxController->getFrame());

    if (mxMainWindow.is())
    {
        mxMainWindow->removeKeyListener(this);
        mxMainWindow->removeMouseListener(this);
        mxMainWindow = nullptr;
    }
    if (mxConfigurationController.is())
        mxConfigurationController->removeConfigurationChangeListener(this);

    if (mxController.is())
    {
        Reference<frame::XFrame> xFrame (mxController->getFrame());
        if (xFrame.is())
            xFrame->removeFrameActionListener(this);
        mxController = nullptr;
    }

    rtl::Reference<PresenterWindowManager> xWindowManagerComponent = std::move(mpWindowManager);
    if (xWindowManagerComponent.is())
        xWindowManagerComponent->dispose();

    mxComponentContext = nullptr;
    mxConfigurationController = nullptr;
    mxSlideShowController = nullptr;
    mxMainPaneId = nullptr;
    mpPaneContainer = nullptr;
    mnCurrentSlideIndex = -1;
    mxCurrentSlide = nullptr;
    mxNextSlide = nullptr;
    mpTheme.reset();
    {
        rtl::Reference<PresenterPaneBorderPainter> xComponent = std::move(mpPaneBorderPainter);
        if (xComponent.is())
            xComponent->dispose();
    }
    mpCanvasHelper.reset();
    mpPaintManager.reset();
    mnPendingSlideNumber = -1;
    {
        Reference<lang::XComponent> xComponent (mxUrlTransformer, UNO_QUERY);
        mxUrlTransformer = nullptr;
        if (xComponent.is())
            xComponent->dispose();
    }
}

void PresenterController::UpdateCurrentSlide (const sal_Int32 nOffset)
{
    // std::cerr << "Updating current Slide to " << nOffset << std::endl;
    GetSlides(nOffset);
    UpdatePaneTitles();
    UpdateViews();

    // Update the accessibility object.
    if (mpAccessibleObject.is())
        mpAccessibleObject->NotifyCurrentSlideChange();
}

void PresenterController::GetSlides (const sal_Int32 nOffset)
{
    if ( ! mxSlideShowController.is())
        return;

    // Get the current slide from the slide show controller.
    mxCurrentSlide = nullptr;
    Reference<container::XIndexAccess> xIndexAccess(mxSlideShowController, UNO_QUERY);
    try
    {
        sal_Int32 nSlideIndex = mxSlideShowController->getCurrentSlideIndex() + nOffset;
        if (mxSlideShowController->isPaused())
            nSlideIndex = -1;

        if (xIndexAccess.is() && nSlideIndex>=0)
        {
            if (nSlideIndex < xIndexAccess->getCount())
            {
                mnCurrentSlideIndex = nSlideIndex;
                mxCurrentSlide.set( xIndexAccess->getByIndex(nSlideIndex), UNO_QUERY);
            }
        }
    }
    catch (RuntimeException&)
    {
    }

    // Get the next slide.
    mxNextSlide = nullptr;
    try
    {
        const sal_Int32 nNextSlideIndex (mxSlideShowController->getNextSlideIndex()+nOffset);
        if (nNextSlideIndex >= 0)
        {
            if (xIndexAccess.is())
            {
                if (nNextSlideIndex < xIndexAccess->getCount())
                    mxNextSlide.set( xIndexAccess->getByIndex(nNextSlideIndex), UNO_QUERY);
            }
        }
    }
    catch (RuntimeException&)
    {
    }
}

namespace
{
OUString lcl_replacePlaceholders(const OUString& rTemplate, std::u16string_view sCurrentSlideNumber,
                             std::u16string_view sCurrentSlideName, std::u16string_view sSlideCount)
{
    // Placeholders
    static constexpr OUStringLiteral sCurrentSlideNumberPlaceholder(u"CURRENT_SLIDE_NUMBER");
    static constexpr OUStringLiteral sCurrentSlideNamePlaceholder(u"CURRENT_SLIDE_NAME");
    static constexpr OUStringLiteral sSlideCountPlaceholder(u"SLIDE_COUNT");

    OUStringBuffer sResult;
    sResult.ensureCapacity(rTemplate.getLength());

    sal_Int32 nIndex (0);
    while (true)
    {
        sal_Int32 nStartIndex = rTemplate.indexOf('%', nIndex);
        if (nStartIndex < 0)
        {
            // Add the remaining part of the string.
            sResult.append(rTemplate.subView(nIndex));
            break;
        }
        else
        {
            // Add the part preceding the next %.
            sResult.append(rTemplate.subView(nIndex, nStartIndex-nIndex));

            // Get the placeholder
            ++nStartIndex;
            const sal_Int32 nEndIndex (rTemplate.indexOf('%', nStartIndex+1));
            const std::u16string_view sPlaceholder (rTemplate.subView(nStartIndex, nEndIndex-nStartIndex));
            nIndex = nEndIndex+1;

            // Replace the placeholder with its current value.
            if (sPlaceholder == sCurrentSlideNumberPlaceholder)
                sResult.append(sCurrentSlideNumber);
            else if (sPlaceholder == sCurrentSlideNamePlaceholder)
                sResult.append(sCurrentSlideName);
            else if (sPlaceholder == sSlideCountPlaceholder)
                sResult.append(sSlideCount);
        }
    }

    return sResult.makeStringAndClear();
}
}

void PresenterController::UpdatePaneTitles()
{
    if ( ! mxSlideShowController.is())
        return;

    // Get string for slide count.
    const OUString sSlideCount = OUString::number(mxSlideShowController->getSlideCount());

    // Get string for current slide index.
    OUString sCurrentSlideNumber (OUString::number(mnCurrentSlideIndex + 1));

    // Get name of the current slide.
    OUString sCurrentSlideName;
    Reference<container::XNamed> xNamedSlide (mxCurrentSlide, UNO_QUERY);
    if (xNamedSlide.is())
        sCurrentSlideName = xNamedSlide->getName();
    Reference<beans::XPropertySet> xSlideProperties (mxCurrentSlide, UNO_QUERY);
    if (xSlideProperties.is())
    {
        try
        {
            OUString sName;
            if (xSlideProperties->getPropertyValue(u"LinkDisplayName"_ustr) >>= sName)
            {
                // Find out whether the name of the current slide has been
                // automatically created or has been set by the user.
                if (sName != sCurrentSlideName)
                    sCurrentSlideName = sName;
            }
        }
        catch (const beans::UnknownPropertyException&)
        {
        }
    }

    // Replace the placeholders with their current values.
    for (auto& rxPane : mpPaneContainer->maPanes)
    {
        OSL_ASSERT(rxPane != nullptr);

        rxPane->msAccessibleName = lcl_replacePlaceholders(rxPane->msAccessibleNameTemplate, sCurrentSlideNumber,
                                                           sCurrentSlideName, sSlideCount);

        if (rxPane->msTitleTemplate.isEmpty())
            continue;

        rxPane->msTitle = lcl_replacePlaceholders(rxPane->msTitleTemplate, sCurrentSlideNumber, sCurrentSlideName,
                                                  sSlideCount);
        if (rxPane->mxPane.is())
            rxPane->mxPane->SetTitle(rxPane->msTitle);
    }
}

void PresenterController::UpdateViews()
{
    // Tell all views about the slides they should display.
    for (const auto& rxPane : mpPaneContainer->maPanes)
    {
        Reference<drawing::XDrawView> xDrawView (cppu::getXWeak(rxPane->mxView.get()), UNO_QUERY);
        if (xDrawView.is())
            xDrawView->setCurrentPage(mxCurrentSlide);
    }
}

void PresenterController::CheckNextSlideUpdate(const Reference<drawing::XShape>& rxShape)
{
    if (!mxNextSlide)
        return;

    // check if shape is member of page or it's masterPage
    if(IsXShapeAssociatedWithXDrawPage(rxShape, mxNextSlide))
        UpdateViews();
}

SharedBitmapDescriptor
    PresenterController::GetViewBackground (const OUString& rsViewURL) const
{
    if (mpTheme != nullptr)
    {
        const OUString sStyleName (mpTheme->GetStyleName(rsViewURL));
        return mpTheme->GetBitmap(sStyleName, u"Background"_ustr);
    }
    return SharedBitmapDescriptor();
}

PresenterTheme::SharedFontDescriptor
    PresenterController::GetViewFont (const OUString& rsViewURL) const
{
    if (mpTheme != nullptr)
    {
        const OUString sStyleName (mpTheme->GetStyleName(rsViewURL));
        return mpTheme->GetFont(sStyleName);
    }
    return PresenterTheme::SharedFontDescriptor();
}

const std::shared_ptr<PresenterTheme>& PresenterController::GetTheme() const
{
    return mpTheme;
}

const ::rtl::Reference<PresenterWindowManager>& PresenterController::GetWindowManager() const
{
    return mpWindowManager;
}

const Reference<presentation::XSlideShowController>&
    PresenterController::GetSlideShowController() const
{
    return mxSlideShowController;
}

const rtl::Reference<PresenterPaneContainer>& PresenterController::GetPaneContainer() const
{
    return mpPaneContainer;
}

const ::rtl::Reference<PresenterPaneBorderPainter>& PresenterController::GetPaneBorderPainter() const
{
    return mpPaneBorderPainter;
}

const std::shared_ptr<PresenterCanvasHelper>& PresenterController::GetCanvasHelper() const
{
    return mpCanvasHelper;
}

const std::shared_ptr<PresenterPaintManager>& PresenterController::GetPaintManager() const
{
    return mpPaintManager;
}

void PresenterController::ShowView (const OUString& rsViewURL)
{
    PresenterPaneContainer::SharedPaneDescriptor pDescriptor (
        mpPaneContainer->FindViewURL(rsViewURL));
    if (!pDescriptor)
        return;

    pDescriptor->mbIsActive = true;
    mxConfigurationController->requestResourceActivation(
        pDescriptor->mxPaneId,
        sd::framework::ResourceActivationMode::ADD);
    mxConfigurationController->requestResourceActivation(
        new sd::framework::ResourceId(
            rsViewURL,
            pDescriptor->mxPaneId),
        sd::framework::ResourceActivationMode::REPLACE);
}

void PresenterController::HideView (const OUString& rsViewURL)
{
    PresenterPaneContainer::SharedPaneDescriptor pDescriptor (
        mpPaneContainer->FindViewURL(rsViewURL));
    if (pDescriptor)
    {
        mxConfigurationController->requestResourceDeactivation(
            new sd::framework::ResourceId(
                rsViewURL,
                pDescriptor->mxPaneId));
    }
}

void PresenterController::DispatchUnoCommand (const OUString& rsCommand) const
{
    if ( ! mxUrlTransformer.is())
        return;

    util::URL aURL;
    aURL.Complete = rsCommand;
    mxUrlTransformer->parseStrict(aURL);

    Reference<frame::XDispatch> xDispatch (GetDispatch(aURL));
    if ( ! xDispatch.is())
        return;

    xDispatch->dispatch(aURL, Sequence<beans::PropertyValue>());
}

Reference<css::frame::XDispatch> PresenterController::GetDispatch (const util::URL& rURL) const
{
    if ( ! mxController.is())
        return nullptr;

    Reference<frame::XDispatchProvider> xDispatchProvider (mxController->getFrame(), UNO_QUERY);
    if ( ! xDispatchProvider.is())
        return nullptr;

    return xDispatchProvider->queryDispatch(
        rURL,
        OUString(),
        frame::FrameSearchFlag::SELF);
}

util::URL PresenterController::CreateURLFromString (const OUString& rsURL) const
{
    util::URL aURL;

    if (mxUrlTransformer.is())
    {
        aURL.Complete = rsURL;
        mxUrlTransformer->parseStrict(aURL);
    }

    return aURL;
}

const Reference<drawing::XDrawPage>& PresenterController::GetCurrentSlide() const
{
    return mxCurrentSlide;
}

bool PresenterController::HasTransition (Reference<drawing::XDrawPage> const & rxPage)
{
    bool bTransition = false;
    if( rxPage.is() )
    {
        Reference<beans::XPropertySet> xSlidePropertySet (rxPage, UNO_QUERY);
        try
        {
            sal_uInt16 aTransitionType = 0;
            xSlidePropertySet->getPropertyValue(u"TransitionType"_ustr) >>= aTransitionType;
            if (aTransitionType > 0)
            {
                bTransition = true;
            }
        }
        catch (const beans::UnknownPropertyException&)
        {
        }
    }
    return bTransition;
}

bool PresenterController::HasCustomAnimation (Reference<drawing::XDrawPage> const & rxPage)
{
    bool bCustomAnimation = false;
    if( rxPage.is() )
    {
        sal_uInt32 i, nCount = rxPage->getCount();
        for ( i = 0; i < nCount; i++ )
        {
            Reference<drawing::XShape> xShape(rxPage->getByIndex(i), UNO_QUERY);
            Reference<beans::XPropertySet> xShapePropertySet(xShape, UNO_QUERY);
            presentation::AnimationEffect aEffect = presentation::AnimationEffect_NONE;
            presentation::AnimationEffect aTextEffect = presentation::AnimationEffect_NONE;
            try
            {
                xShapePropertySet->getPropertyValue(u"Effect"_ustr) >>= aEffect;
                xShapePropertySet->getPropertyValue(u"TextEffect"_ustr) >>= aTextEffect;
            }
            catch (const beans::UnknownPropertyException&)
            {
            }
            if( aEffect != presentation::AnimationEffect_NONE ||
                aTextEffect != presentation::AnimationEffect_NONE )
            {
                bCustomAnimation = true;
                break;
            }
        }
    }
    return bCustomAnimation;
}

void PresenterController::HandleMouseClick (const awt::MouseEvent& rEvent)
{
    if (!mxSlideShowController.is())
        return;

    switch (rEvent.Buttons)
    {
        case awt::MouseButton::LEFT:
            if (rEvent.Modifiers == awt::KeyModifier::MOD2)
                mxSlideShowController->gotoNextSlide();
            else
                mxSlideShowController->gotoNextEffect();
            break;

        case awt::MouseButton::RIGHT:
            mxSlideShowController->gotoPreviousSlide();
            break;

        default:
            // Other or multiple buttons.
            break;
    }
}

void PresenterController::RequestViews (
    const bool bIsSlideSorterActive,
    const bool bIsNotesViewActive,
    const bool bIsHelpViewActive)
{
    for (const auto& rxPane : mpPaneContainer->maPanes)
    {
        bool bActivate (true);
        const OUString sViewURL (rxPane->msViewURL);
        if (sViewURL == PresenterViewFactory::msNotesViewURL)
        {
            bActivate = bIsNotesViewActive && !bIsSlideSorterActive && !bIsHelpViewActive;
        }
        else if (sViewURL == PresenterViewFactory::msSlideSorterURL)
        {
            bActivate = bIsSlideSorterActive;
        }
        else if (sViewURL == PresenterViewFactory::msCurrentSlidePreviewViewURL
            || sViewURL == PresenterViewFactory::msNextSlidePreviewViewURL)
        {
            bActivate = !bIsSlideSorterActive && ! bIsHelpViewActive;
        }
        else if (sViewURL == PresenterViewFactory::msToolBarViewURL)
        {
            bActivate = true;
        }
        else if (sViewURL == PresenterViewFactory::msHelpViewURL)
        {
            bActivate = bIsHelpViewActive;
        }

        if (bActivate)
            ShowView(sViewURL);
        else
            HideView(sViewURL);
    }
}

void PresenterController::SetPresentationTime(IPresentationTime* pPresentationTime)
{
    mpPresentationTime = pPresentationTime;
}

IPresentationTime* PresenterController::GetPresentationTime()
{
    return mpPresentationTime;
}

//----- ConfigurationChangeListener ------------------------------------------

void PresenterController::notifyConfigurationChange (
    const sd::framework::ConfigurationChangeEvent& rEvent)
{
    {
        std::unique_lock l(m_aMutex);
        throwIfDisposed(l);
    }

    switch (rEvent.Type)
    {
        case sd::framework::ConfigurationChangeEventType::ResourceActivation:
            if (rEvent.ResourceId->compareTo(mxMainPaneId) == 0)
            {
                InitializeMainPane(dynamic_cast<sd::framework::Pane*>(rEvent.ResourceObject.get()));
            }
            else if (rEvent.ResourceId->isBoundTo(mxMainPaneId,AnchorBindingMode_DIRECT))
            {
                // A pane bound to the main pane has been created and is
                // stored in the pane container.
                rtl::Reference<sd::framework::AbstractPane> xPane = dynamic_cast<sd::framework::AbstractPane*>(rEvent.ResourceObject.get());
                if (xPane.is())
                {
                    mpPaneContainer->FindPaneId(xPane->getResourceId());
                }
            }
            else if (rEvent.ResourceId->isBoundTo(mxMainPaneId,AnchorBindingMode_INDIRECT))
            {
                // A view bound to one of the panes has been created and is
                // stored in the pane container along with its pane.
                rtl::Reference<sd::framework::AbstractView> xView = dynamic_cast<sd::framework::AbstractView*>(rEvent.ResourceObject.get());
                if (xView.is())
                {
                    mpPaneContainer->StoreView(xView);
                    UpdateViews();
                    mpWindowManager->NotifyViewCreation(xView);
                }
            }
            break;

        case sd::framework::ConfigurationChangeEventType::ResourceDeactivation:
            if (rEvent.ResourceId->isBoundTo(mxMainPaneId,AnchorBindingMode_INDIRECT))
            {
                // If this is a view then remove it from the pane container.
                rtl::Reference<sd::framework::AbstractView> xView = dynamic_cast<sd::framework::AbstractView*>(rEvent.ResourceObject.get());
                if (xView.is())
                {
                    PresenterPaneContainer::SharedPaneDescriptor pDescriptor(
                        mpPaneContainer->RemoveView(xView));

                    // A possibly opaque view has been removed.  Update()
                    // updates the clip polygon.
                    mpWindowManager->Update();
                    // Request the repainting of the area previously
                    // occupied by the view.
                    if (pDescriptor)
                        GetPaintManager()->Invalidate(pDescriptor->mxBorderWindow);
                }
            }
            break;

        case sd::framework::ConfigurationChangeEventType::ConfigurationUpdateEnd:
            if (mpAccessibleObject.is())
                mpAccessibleObject->UpdateAccessibilityHierarchy();
            UpdateCurrentSlide(0);
            break;

        default: break;
    }
}

//----- XEventListener --------------------------------------------------------

void SAL_CALL PresenterController::disposing (
    const lang::EventObject& rEvent)
{
    if (mpAccessibleObject)
        mpAccessibleObject->dispose();

    if (rEvent.Source.get() == static_cast<cppu::OWeakObject*>(mxController.get()))
        mxController = nullptr;
    else if (rEvent.Source == cppu::getXWeak(mxConfigurationController.get()))
        mxConfigurationController = nullptr;
    else if (rEvent.Source == mxSlideShowController)
        mxSlideShowController = nullptr;
    else if (rEvent.Source == mxMainWindow)
        mxMainWindow = nullptr;
}

//----- XFrameActionListener --------------------------------------------------

void SAL_CALL PresenterController::frameAction (
    const frame::FrameActionEvent& rEvent)
{
    if (rEvent.Action == frame::FrameAction_FRAME_ACTIVATED)
    {
        if (mxSlideShowController.is())
            mxSlideShowController->activate();
    }
}

//----- XKeyListener ----------------------------------------------------------

void SAL_CALL PresenterController::keyPressed (const awt::KeyEvent& rEvent)
{
    // Tell all views about the unhandled key event.
    for (const auto& rxPane : mpPaneContainer->maPanes)
    {
        if ( ! rxPane->mbIsActive)
            continue;

        Reference<awt::XKeyListener> xKeyListener (cppu::getXWeak(rxPane->mxView.get()), UNO_QUERY);
        if (xKeyListener.is())
            xKeyListener->keyPressed(rEvent);
    }
}

void SAL_CALL PresenterController::keyReleased (const awt::KeyEvent& rEvent)
{
    if (rEvent.Source != mxMainWindow)
        return;

    switch (rEvent.KeyCode)
    {
        case awt::Key::ESCAPE:
        case awt::Key::SUBTRACT:
        {
            if( mxController.is() )
            {
                Reference< XPresentationSupplier > xPS( mxController->getModel(), UNO_QUERY );
                if( xPS.is() )
                {
                    Reference< XPresentation > xP( xPS->getPresentation() );
                    if( xP.is() )
                        xP->end();
                }
            }
        }
        break;

        case awt::Key::PAGEDOWN:
            if (mxSlideShowController.is())
            {
                if (rEvent.Modifiers == awt::KeyModifier::MOD2)
                    mxSlideShowController->gotoNextSlide();
                else
                    mxSlideShowController->gotoNextEffect();
            }
            break;

        case awt::Key::RIGHT:
        case awt::Key::SPACE:
        case awt::Key::DOWN:
        case awt::Key::XF86FORWARD:
            if (mxSlideShowController.is())
            {
                mxSlideShowController->gotoNextEffect();
            }
            break;

        case awt::Key::PAGEUP:
            if (mxSlideShowController.is())
            {
                if (rEvent.Modifiers == awt::KeyModifier::MOD2)
                    mxSlideShowController->gotoPreviousSlide();
                else
                    mxSlideShowController->gotoPreviousEffect();
            }
            break;

        case awt::Key::LEFT:
        case awt::Key::UP:
        case awt::Key::BACKSPACE:
        case awt::Key::XF86BACK:
            if (mxSlideShowController.is())
            {
                mxSlideShowController->gotoPreviousEffect();
            }
            break;

        case awt::Key::P:
            if (mxSlideShowController.is())
            {
                bool bPenEnabled = mxSlideShowController->getUsePen();
                mxSlideShowController->setUsePen( !bPenEnabled );
            }
            break;

        // tdf#149351 Ctrl+A disables pointer as pen mode
        case awt::Key::A:
            if (mxSlideShowController.is())
            {
                if (rEvent.Modifiers == awt::KeyModifier::MOD1)
                {
                    mxSlideShowController->setUsePen( false );
                }
            }
            break;

        case awt::Key::E:
            if (mxSlideShowController.is())
            {
                mxSlideShowController->setEraseAllInk( true );
            }
            break;

        case awt::Key::HOME:
            if (mxSlideShowController.is())
            {
                mxSlideShowController->gotoFirstSlide();
            }
            break;

        case awt::Key::END:
            if (mxSlideShowController.is())
            {
                mxSlideShowController->gotoLastSlide();
            }
            break;

        case awt::Key::W:
        case awt::Key::COMMA:
            if (mxSlideShowController.is())
            {
                if (mxSlideShowController->isPaused())
                    mxSlideShowController->resume();
                else
                    mxSlideShowController->blankScreen(0x00ffffff);
            }
            break;

        case awt::Key::B:
        case awt::Key::POINT:
            if (mxSlideShowController.is())
            {
                if (mxSlideShowController->isPaused())
                    mxSlideShowController->resume();
                else
                    mxSlideShowController->blankScreen(0x00000000);
            }
            break;

        case awt::Key::NUM0:
        case awt::Key::NUM1:
        case awt::Key::NUM2:
        case awt::Key::NUM3:
        case awt::Key::NUM4:
        case awt::Key::NUM5:
        case awt::Key::NUM6:
        case awt::Key::NUM7:
        case awt::Key::NUM8:
        case awt::Key::NUM9:
            HandleNumericKeyPress(rEvent.KeyCode-awt::Key::NUM0, rEvent.Modifiers);
            break;

        case awt::Key::RETURN:
            if (mnPendingSlideNumber > 0)
            {
                if (mxSlideShowController.is())
                    mxSlideShowController->gotoSlideIndex(mnPendingSlideNumber - 1);
                mnPendingSlideNumber = -1;
            }
            else
            {
                if (mxSlideShowController.is())
                    mxSlideShowController->gotoNextEffect();
            }

            break;

        case awt::Key::F1:
            // Toggle the help view.
            if (mpWindowManager)
            {
                if (mpWindowManager->GetViewMode() != PresenterWindowManager::VM_Help)
                    mpWindowManager->SetViewMode(PresenterWindowManager::VM_Help);
                else
                    mpWindowManager->SetHelpViewState(false);
            }

            break;

        default:
            // Tell all views about the unhandled key event.
            for (const auto& rxPane : mpPaneContainer->maPanes)
            {
                if ( ! rxPane->mbIsActive)
                    continue;

                Reference<awt::XKeyListener> xKeyListener (cppu::getXWeak(rxPane->mxView.get()), UNO_QUERY);
                if (xKeyListener.is())
                    xKeyListener->keyReleased(rEvent);
            }
            break;
    }
}

void PresenterController::HandleNumericKeyPress (
    const sal_Int32 nKey,
    const sal_Int32 nModifiers)
{
    switch (nModifiers)
    {
        case 0:
            if (mnPendingSlideNumber == -1)
                mnPendingSlideNumber = 0;
            UpdatePendingSlideNumber(mnPendingSlideNumber * 10 + nKey);
            break;

        case awt::KeyModifier::MOD1:
            // Ctrl-1, Ctrl-2, and Ctrl-3 are used to switch between views
            // (slide view, notes view, normal). Ctrl-4 switches monitors
            mnPendingSlideNumber = -1;
            if (!mpWindowManager)
                return;
            switch(nKey)
            {
                case 1:
                    mpWindowManager->SetViewMode(PresenterWindowManager::VM_Standard);
                    break;
                case 2:
                    mpWindowManager->SetViewMode(PresenterWindowManager::VM_Notes);
                    break;
                case 3:
                    mpWindowManager->SetViewMode(PresenterWindowManager::VM_SlideOverview);
                    break;
                case 4:
                    SwitchMonitors();
                    break;
                default:
                    // Ignore unsupported key.
                    break;
            }
            break;

        default:
            // Ignore unsupported modifiers.
            break;
    }
}

//----- XMouseListener --------------------------------------------------------

void SAL_CALL PresenterController::mousePressed (const css::awt::MouseEvent&)
{
    if (mxMainWindow.is())
        mxMainWindow->setFocus();
}

void SAL_CALL PresenterController::mouseReleased (const css::awt::MouseEvent&) {}

void SAL_CALL PresenterController::mouseEntered (const css::awt::MouseEvent&) {}

void SAL_CALL PresenterController::mouseExited (const css::awt::MouseEvent&) {}

void PresenterController::InitializeMainPane (const rtl::Reference<sd::framework::Pane>& rxPane)
{
    if ( ! rxPane.is())
        return;

    mpAccessibleObject = PresenterAccessible::Create(this, rxPane);

    LoadTheme(rxPane);

    // Main pane has been created and is now observed by the window
    // manager.
    mpWindowManager->SetParentPane(rxPane);
    mpWindowManager->SetTheme(mpTheme);

    if (mpPaneBorderPainter)
        mpPaneBorderPainter->SetTheme(mpTheme);

    // Add key listener
    mxMainWindow = rxPane->getWindow();
    if (mxMainWindow.is())
    {
        mxMainWindow->addKeyListener(this);
        mxMainWindow->addMouseListener(this);
    }
    rxPane->setVisible(true);

    mpPaintManager = std::make_shared<PresenterPaintManager>(mxMainWindow, mpPaneContainer);

    mxCanvas.set(rxPane->getCanvas(), UNO_QUERY);

    if (mxSlideShowController.is())
        mxSlideShowController->activate();

    UpdateCurrentSlide(0);
}

void PresenterController::LoadTheme (const rtl::Reference<sd::framework::AbstractPane>& rxPane)
{
    // Create (load) the current theme.
    if (rxPane.is())
        mpTheme = std::make_shared<PresenterTheme>(mxComponentContext, rxPane->getCanvas());
}

double PresenterController::GetSlideAspectRatio() const
{
    double nSlideAspectRatio (28.0/21.0);

    try
    {
        if (mxController.is())
        {
            Reference<drawing::XDrawPagesSupplier> xSlideSupplier (
                mxController->getModel(), UNO_QUERY_THROW);
            Reference<drawing::XDrawPages> xSlides (xSlideSupplier->getDrawPages());
            if (xSlides.is() && xSlides->getCount()>0)
            {
                Reference<beans::XPropertySet> xProperties(xSlides->getByIndex(0),UNO_QUERY_THROW);
                sal_Int32 nWidth (28000);
                sal_Int32 nHeight (21000);
                if ((xProperties->getPropertyValue(u"Width"_ustr) >>= nWidth)
                    && (xProperties->getPropertyValue(u"Height"_ustr) >>= nHeight)
                    && nHeight > 0)
                {
                    nSlideAspectRatio = double(nWidth) / double(nHeight);
                }
            }
        }
    }
    catch (RuntimeException&)
    {
        OSL_ASSERT(false);
    }

    return nSlideAspectRatio;
}

void PresenterController::UpdatePendingSlideNumber (const sal_Int32 nPendingSlideNumber)
{
    mnPendingSlideNumber = nPendingSlideNumber;

    if (mpTheme == nullptr)
        return;

    if ( ! mxMainWindow.is())
        return;

    PresenterTheme::SharedFontDescriptor pFont (
        mpTheme->GetFont(u"PendingSlideNumberFont"_ustr));
    if (!pFont)
        return;

    pFont->PrepareFont(mxCanvas);
    if ( ! pFont->mxFont.is())
        return;

    const OUString sText (OUString::number(mnPendingSlideNumber));
    rendering::StringContext aContext (sText, 0, sText.getLength());
    pFont->mxFont->createTextLayout(
            aContext,
            rendering::TextDirection::WEAK_LEFT_TO_RIGHT,
            0);
}

void PresenterController::SwitchMonitors()
{
    rtl::Reference<PresenterScreen> pScreen( mxScreen );
    if (!pScreen)
        return;

    pScreen->SwitchMonitors();
}

void PresenterController::ExitPresenter()
{
    if( mxController.is() )
    {
            Reference< XPresentationSupplier > xPS( mxController->getModel(), UNO_QUERY );
            if( xPS.is() )
            {
                Reference< XPresentation > xP( xPS->getPresentation() );
                if( xP.is() )
                    xP->end();
            }
    }
}

} // end of namespace ::sdext::presenter

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
