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

#include "accessibletabbar.hxx"
#include "accessibletabbarpagelist.hxx"

#include <svtools/tabbar.hxx>
#include <com/sun/star/accessibility/AccessibleEventId.hpp>
#include <com/sun/star/accessibility/AccessibleRole.hpp>
#include <com/sun/star/accessibility/AccessibleStateType.hpp>
#include <com/sun/star/awt/XDevice.hpp>
#include <com/sun/star/awt/XVclWindowPeer.hpp>
#include <com/sun/star/lang/IndexOutOfBoundsException.hpp>
#include <comphelper/accessiblecontexthelper.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <unotools/accessiblerelationsethelper.hxx>
#include <i18nlangtag/languagetag.hxx>
#include <o3tl/safeint.hxx>
#include <vcl/svapp.hxx>
#include <vcl/settings.hxx>
#include <vcl/unohelp.hxx>

namespace accessibility
{

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::accessibility;
using namespace ::comphelper;


AccessibleTabBar::AccessibleTabBar( TabBar* pTabBar )
    :ImplInheritanceHelper( pTabBar )
{
    if ( m_pTabBar )
        m_aAccessibleChildren.assign( m_pTabBar->GetAccessibleChildWindowCount() + 1, Reference< XAccessible >() );
}


void AccessibleTabBar::ProcessWindowEvent( const VclWindowEvent& rVclWindowEvent )
{
    Any aOldValue, aNewValue;

    switch ( rVclWindowEvent.GetId() )
    {
        case VclEventId::WindowEnabled:
        {
            aNewValue <<= AccessibleStateType::SENSITIVE;
            NotifyAccessibleEvent( AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
            aNewValue <<= AccessibleStateType::ENABLED;
            NotifyAccessibleEvent( AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
        }
        break;
        case VclEventId::WindowDisabled:
        {
            aOldValue <<= AccessibleStateType::ENABLED;
            NotifyAccessibleEvent( AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
            aOldValue <<= AccessibleStateType::SENSITIVE;
            NotifyAccessibleEvent( AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
        }
        break;
        case VclEventId::WindowGetFocus:
        {
            aNewValue <<= AccessibleStateType::FOCUSED;
            NotifyAccessibleEvent( AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
        }
        break;
        case VclEventId::WindowLoseFocus:
        {
            aOldValue <<= AccessibleStateType::FOCUSED;
            NotifyAccessibleEvent( AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
        }
        break;
        case VclEventId::WindowShow:
        {
            aNewValue <<= AccessibleStateType::SHOWING;
            NotifyAccessibleEvent( AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
        }
        break;
        case VclEventId::WindowHide:
        {
            aOldValue <<= AccessibleStateType::SHOWING;
            NotifyAccessibleEvent( AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
        }
        break;
        default:
        {
            AccessibleTabBarBase::ProcessWindowEvent( rVclWindowEvent );
        }
        break;
    }
}


void AccessibleTabBar::FillAccessibleStateSet( sal_Int64& rStateSet )
{
    if ( !m_pTabBar )
        return;

    if ( m_pTabBar->IsEnabled() )
    {
        rStateSet |= AccessibleStateType::ENABLED;
        rStateSet |= AccessibleStateType::SENSITIVE;
    }

    rStateSet |= AccessibleStateType::FOCUSABLE;

    if ( m_pTabBar->HasFocus() )
        rStateSet |= AccessibleStateType::FOCUSED;

    rStateSet |= AccessibleStateType::VISIBLE;

    if ( m_pTabBar->IsVisible() )
        rStateSet |= AccessibleStateType::SHOWING;

    if ( m_pTabBar->GetStyle() & WB_SIZEABLE )
        rStateSet |= AccessibleStateType::RESIZABLE;
}

// OAccessible

awt::Rectangle AccessibleTabBar::implGetBounds()
{
    awt::Rectangle aBounds;
    if ( m_pTabBar )
        aBounds = vcl::unohelper::ConvertToAWTRect(
            tools::Rectangle(m_pTabBar->GetPosPixel(), m_pTabBar->GetSizePixel()));

    return aBounds;
}


// XComponent


void AccessibleTabBar::disposing()
{
    AccessibleTabBarBase::disposing();

    // dispose all children
    for (const Reference<XAccessible>& i : m_aAccessibleChildren)
    {
        Reference< XComponent > xComponent( i, UNO_QUERY );
        if ( xComponent.is() )
            xComponent->dispose();
    }
    m_aAccessibleChildren.clear();
}


// XServiceInfo


OUString AccessibleTabBar::getImplementationName()
{
    return u"com.sun.star.comp.svtools.AccessibleTabBar"_ustr;
}


sal_Bool AccessibleTabBar::supportsService( const OUString& rServiceName )
{
    return cppu::supportsService(this, rServiceName);
}


Sequence< OUString > AccessibleTabBar::getSupportedServiceNames()
{
    return  { u"com.sun.star.awt.AccessibleTabBar"_ustr };
}

// XAccessibleContext


sal_Int64 AccessibleTabBar::getAccessibleChildCount()
{
    OExternalLockGuard aGuard( this );

    return m_aAccessibleChildren.size();
}


Reference< XAccessible > AccessibleTabBar::getAccessibleChild( sal_Int64 i )
{
    OExternalLockGuard aGuard( this );

    if ( i < 0 || o3tl::make_unsigned(i) >= m_aAccessibleChildren.size() )
        throw IndexOutOfBoundsException();

    Reference< XAccessible > xChild = m_aAccessibleChildren[i];
    if ( !xChild.is() )
    {
        if ( m_pTabBar )
        {
            sal_Int32 nCount = m_pTabBar->GetAccessibleChildWindowCount();

            if ( i < nCount )
            {
                vcl::Window* pChild = m_pTabBar->GetAccessibleChildWindow( static_cast<sal_uInt16>(i) );
                if ( pChild )
                    xChild = pChild->GetAccessible();
            }
            else if ( i == nCount )
            {
                xChild = new AccessibleTabBarPageList( m_pTabBar, i );
            }

            // insert into child list
            m_aAccessibleChildren[i] = xChild;
        }
    }

    return xChild;
}


Reference< XAccessible > AccessibleTabBar::getAccessibleParent(  )
{
    OExternalLockGuard aGuard( this );

    Reference< XAccessible > xParent;
    if ( m_pTabBar )
        xParent = m_pTabBar->GetAccessibleParent();

    return xParent;
}


sal_Int64 AccessibleTabBar::getAccessibleIndexInParent(  )
{
    OExternalLockGuard aGuard( this );

    sal_Int64 nIndexInParent = -1;
    if ( m_pTabBar )
    {
        vcl::Window* pParent = m_pTabBar->GetAccessibleParentWindow();
        if ( pParent )
        {
            for ( sal_uInt16 i = 0, nCount = pParent->GetAccessibleChildWindowCount(); i < nCount; ++i )
            {
                vcl::Window* pChild = pParent->GetAccessibleChildWindow( i );
                if ( pChild == static_cast< vcl::Window* >( m_pTabBar ) )
                {
                    nIndexInParent = i;
                    break;
                }
            }
        }
    }

    return nIndexInParent;
}


sal_Int16 AccessibleTabBar::getAccessibleRole(  )
{
    OExternalLockGuard aGuard( this );

    return AccessibleRole::PANEL;
}

OUString AccessibleTabBar::getAccessibleDescription( )
{
    OExternalLockGuard aGuard( this );

    if (m_pTabBar)
        return m_pTabBar->GetAccessibleDescription();

    return OUString();
}

OUString AccessibleTabBar::getAccessibleName(  )
{
    OExternalLockGuard aGuard( this );

    if (m_pTabBar)
        return m_pTabBar->GetAccessibleName();

    return OUString();
}

Reference< XAccessibleRelationSet > AccessibleTabBar::getAccessibleRelationSet(  )
{
    OExternalLockGuard aGuard( this );

    return new utl::AccessibleRelationSetHelper;
}

sal_Int64 AccessibleTabBar::getAccessibleStateSet(  )
{
    OExternalLockGuard aGuard( this );

    sal_Int64 nStateSet = 0;

    if ( !rBHelper.bDisposed && !rBHelper.bInDispose )
    {
        FillAccessibleStateSet( nStateSet );
    }
    else
    {
        nStateSet |= AccessibleStateType::DEFUNC;
    }

    return nStateSet;
}


Locale AccessibleTabBar::getLocale(  )
{
    OExternalLockGuard aGuard( this );

    return Application::GetSettings().GetLanguageTag().getLocale();
}

// XAccessibleComponent
Reference< XAccessible > AccessibleTabBar::getAccessibleAtPoint( const awt::Point& rPoint )
{
    OExternalLockGuard aGuard( this );

    for( sal_Int64 i = 0; i < getAccessibleChildCount(); ++i )
    {
        Reference< XAccessible > xAcc = getAccessibleChild( i );
        if ( xAcc.is() )
        {
            Reference< XAccessibleComponent > xComp( xAcc->getAccessibleContext(), UNO_QUERY );
            if ( xComp.is() )
            {
                tools::Rectangle aRect = vcl::unohelper::ConvertToVCLRect(xComp->getBounds());
                Point aPos = vcl::unohelper::ConvertToVCLPoint(rPoint);
                if ( aRect.Contains( aPos ) )
                {
                    return xAcc;
                }
            }
        }
    }

    return nullptr;
}

void AccessibleTabBar::grabFocus(  )
{
    OExternalLockGuard aGuard( this );

    if ( m_pTabBar )
        m_pTabBar->GrabFocus();
}


sal_Int32 AccessibleTabBar::getForeground(  )
{
    OExternalLockGuard aGuard( this );

    Color nColor;
    if ( m_pTabBar )
    {
        if ( m_pTabBar->IsControlForeground() )
            nColor = m_pTabBar->GetControlForeground();
        else
        {
            vcl::Font aFont;
            if ( m_pTabBar->IsControlFont() )
                aFont = m_pTabBar->GetControlFont();
            else
                aFont = m_pTabBar->GetFont();
            nColor = aFont.GetColor();
        }
    }

    return sal_Int32(nColor);
}


sal_Int32 AccessibleTabBar::getBackground(  )
{
    OExternalLockGuard aGuard( this );

    Color nColor;
    if ( m_pTabBar )
    {
        if ( m_pTabBar->IsControlBackground() )
            nColor = m_pTabBar->GetControlBackground();
        else
            nColor = m_pTabBar->GetBackground().GetColor();
    }

    return sal_Int32(nColor);
}


// XAccessibleExtendedComponent

OUString AccessibleTabBar::getTitledBorderText(  )
{
    OExternalLockGuard aGuard( this );

    OUString sText;
    if ( m_pTabBar )
        sText = m_pTabBar->GetText();

    return sText;
}


OUString AccessibleTabBar::getToolTipText(  )
{
    OExternalLockGuard aGuard( this );

    OUString sText;
    if ( m_pTabBar )
        sText = m_pTabBar->GetQuickHelpText();

    return sText;
}


}   // namespace accessibility


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
