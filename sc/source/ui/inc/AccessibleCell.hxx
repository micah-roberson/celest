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

#pragma once

#include <memory>
#include "AccessibleCellBase.hxx"
#include "viewdata.hxx"
#include <com/sun/star/accessibility/AccessibleRelationType.hpp>
#include <com/sun/star/accessibility/XAccessibleExtendedAttributes.hpp>
#include <cppuhelper/implbase1.hxx>
#include <rtl/ref.hxx>
#include <editeng/AccessibleStaticTextBase.hxx>
#include <comphelper/uno3.hxx>

namespace utl { class AccessibleRelationSetHelper; }

class ScTabViewShell;
class ScAccessibleDocument;

typedef cppu::ImplHelper1< css::accessibility::XAccessibleExtendedAttributes>
                    ScAccessibleCellAttributeImpl;

using css::accessibility::AccessibleRelationType;

/** @descr
        This base class provides an implementation of the
        <code>AccessibleCell</code> service.
*/
class ScAccessibleCell final
    :   public  ScAccessibleCellBase,
        public  accessibility::AccessibleStaticTextBase,
        public  ScAccessibleCellAttributeImpl
{
public:
    static rtl::Reference<ScAccessibleCell> create(
        const css::uno::Reference<css::accessibility::XAccessible>& rxParent,
        ScTabViewShell* pViewShell,
        const ScAddress& rCellAddress,
        sal_Int64 nIndex,
        ScSplitPos eSplitPos,
        ScAccessibleDocument* pAccDoc);

private:
    ScAccessibleCell(
        const css::uno::Reference<css::accessibility::XAccessible>& rxParent,
        ScTabViewShell* pViewShell,
        const ScAddress& rCellAddress,
        sal_Int64 nIndex,
        ScSplitPos eSplitPos,
        ScAccessibleDocument* pAccDoc);

    using ScAccessibleCellBase::disposing;
    virtual void SAL_CALL disposing() override;

protected:
    virtual ~ScAccessibleCell() override;

    using ScAccessibleCellBase::IsDefunc;

public:
    ///=====  XInterface  =====================================================

    DECLARE_XINTERFACE()

    ///=====  XTypeProvider  ===================================================

    DECLARE_XTYPEPROVIDER()

    ///=====  XAccessibleComponent  ============================================

    virtual css::uno::Reference< css::accessibility::XAccessible >
        SAL_CALL getAccessibleAtPoint( const css::awt::Point& rPoint ) override;

    virtual void SAL_CALL grabFocus(  ) override;

protected:
    /// Return the object's current bounding box relative to the desktop.
    virtual AbsoluteScreenPixelRectangle GetBoundingBoxOnScreen() override;

    /// Return the object's current bounding box relative to the parent object.
    virtual tools::Rectangle GetBoundingBox() override;

public:
    ///=====  XAccessibleContext  ==============================================

    /// Return the number of currently visible children.
    /// override to calculate this on demand
    virtual sal_Int64 SAL_CALL
        getAccessibleChildCount() override;

    /// Return the specified child or NULL if index is invalid.
    /// override to calculate this on demand
    virtual css::uno::Reference< css::accessibility::XAccessible> SAL_CALL
        getAccessibleChild(sal_Int64 nIndex) override;

    /// Return the set of current states.
    virtual sal_Int64 SAL_CALL
        getAccessibleStateSet() override;

    virtual css::uno::Reference<
        css::accessibility::XAccessibleRelationSet> SAL_CALL
           getAccessibleRelationSet() override;

    virtual OUString SAL_CALL getExtendedAttributes() override;

    // Override this method to handle cell's ParaIndent attribute specially.
    virtual css::uno::Sequence< css::beans::PropertyValue > SAL_CALL getCharacterAttributes( sal_Int32 nIndex, const css::uno::Sequence< OUString >& aRequestedAttributes ) override;
private:
    ScTabViewShell* mpViewShell;
    ScAccessibleDocument* mpAccDoc;

    ScSplitPos meSplitPos;

    bool IsDefunc(sal_Int64 nParentStates);
    virtual bool IsEditable(sal_Int64 nParentStates) override;
    bool IsOpaque() const;
    bool IsFocused() const;
    bool IsSelected();

    static ScDocument* GetDocument(ScTabViewShell* mpViewShell);

    ::std::unique_ptr< SvxEditSource > CreateEditSource(ScTabViewShell* pViewShell, ScAddress aCell, ScSplitPos eSplitPos);

    void FillDependents(utl::AccessibleRelationSetHelper* pRelationSet);
    void FillPrecedents(utl::AccessibleRelationSetHelper* pRelationSet);
    void AddRelation(const ScAddress& rCell,
        const AccessibleRelationType eRelationType,
        ::utl::AccessibleRelationSetHelper* pRelationSet);
    void AddRelation(const ScRange& rRange,
        const AccessibleRelationType eRelationType,
        ::utl::AccessibleRelationSetHelper* pRelationSet);
    bool IsFormulaMode();
    bool IsDropdown() const;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
