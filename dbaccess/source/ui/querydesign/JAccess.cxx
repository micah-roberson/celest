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

#include <JAccess.hxx>
#include <JoinTableView.hxx>
#include <TableWindow.hxx>
#include <com/sun/star/accessibility/AccessibleRole.hpp>
#include <com/sun/star/lang/IndexOutOfBoundsException.hpp>
#include <TableConnection.hxx>
#include <o3tl/safeint.hxx>

namespace dbaui
{
    using namespace ::com::sun::star::accessibility;
    using namespace ::com::sun::star::uno;
    using namespace ::com::sun::star::lang;

    OJoinDesignViewAccess::OJoinDesignViewAccess(OJoinTableView* _pTableView)
        : VCLXAccessibleComponent(_pTableView)
        , m_pTableView(_pTableView)
    {
    }
    OUString SAL_CALL OJoinDesignViewAccess::getImplementationName()
    {
        return u"org.openoffice.comp.dbu.JoinViewAccessibility"_ustr;
    }
    void OJoinDesignViewAccess::clearTableView()
    {
        ::osl::MutexGuard aGuard( m_aMutex );
        m_pTableView = nullptr;
    }
    // XAccessibleContext
    sal_Int64 SAL_CALL OJoinDesignViewAccess::getAccessibleChildCount(  )
    {
        // TODO may be this will change to only visible windows
        // this is the same assumption mt implements
        ::osl::MutexGuard aGuard( m_aMutex  );
        sal_Int64 nChildCount = 0;
        if ( m_pTableView )
            nChildCount = m_pTableView->GetTabWinCount() + m_pTableView->getTableConnections().size();
        return nChildCount;
    }
    Reference< XAccessible > SAL_CALL OJoinDesignViewAccess::getAccessibleChild( sal_Int64 i )
    {
        rtl::Reference<comphelper::OAccessible> pRet;
        ::osl::MutexGuard aGuard( m_aMutex  );
        if(i < 0 || i >= getAccessibleChildCount() || !m_pTableView)
            throw IndexOutOfBoundsException();
        // check if we should return a table window or a connection
        sal_Int64 nTableWindowCount = m_pTableView->GetTabWinCount();
        if( i < nTableWindowCount )
        {
            OJoinTableView::OTableWindowMap::const_iterator aIter = std::next(m_pTableView->GetTabWinMap().begin(), i);
            pRet = aIter->second->GetAccessible();
        }
        else if( o3tl::make_unsigned(i - nTableWindowCount) < m_pTableView->getTableConnections().size() )
            pRet = m_pTableView->getTableConnections()[i - nTableWindowCount]->GetAccessible();
        return pRet;
    }
    sal_Int16 SAL_CALL OJoinDesignViewAccess::getAccessibleRole(  )
    {
        return AccessibleRole::VIEW_PORT;
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
