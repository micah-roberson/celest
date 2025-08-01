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

#include <vcl/dockwin.hxx>
#include <vcl/toolkit/floatwin.hxx>
#include <vcl/taskpanelist.hxx>

#include <svdata.hxx>
#include "menubarwindow.hxx"

#include <algorithm>

namespace {

AbsoluteScreenPixelPoint ImplTaskPaneListGetPos( const vcl::Window *w )
{
    AbsoluteScreenPixelPoint pos;
    if( w->IsDockingWindow() )
    {
        Point pos1 = static_cast<const DockingWindow*>(w)->GetPosPixel();
        vcl::Window *pF = static_cast<const DockingWindow*>(w)->GetFloatingWindow();
        if( pF )
            pos = pF->OutputToAbsoluteScreenPixel( pF->ScreenToOutputPixel( pos1 ) );
        else
            pos = w->OutputToAbsoluteScreenPixel( pos1 );
    }
    else
        pos = w->OutputToAbsoluteScreenPixel( w->GetPosPixel() );

    return pos;
}

// compares window pos left-to-right
struct LTRSort
{
    bool operator()( const vcl::Window* w1, const vcl::Window* w2 ) const
    {
        Point pos1(ImplTaskPaneListGetPos( w1 ));
        Point pos2(ImplTaskPaneListGetPos( w2 ));

        if( pos1.X() == pos2.X() )
            return ( pos1.Y() < pos2.Y() );
        else
            return ( pos1.X() < pos2.X() );
    }
};

}

static void ImplTaskPaneListGrabFocus( vcl::Window *pWindow, bool bForward )
{
    // put focus in child of floating windows which is typically a toolbar
    // that can deal with the focus
    if( pWindow->ImplIsFloatingWindow() && pWindow->GetWindow( GetWindowType::FirstChild ) )
        pWindow = pWindow->GetWindow( GetWindowType::FirstChild );
    pWindow->ImplGrabFocus( GetFocusFlags::F6 | (bForward ? GetFocusFlags::Forward : GetFocusFlags::Backward));
}

TaskPaneList::TaskPaneList()
{
}

TaskPaneList::~TaskPaneList()
{
}

void TaskPaneList::AddWindow( vcl::Window *pWindow )
{
    if( !pWindow )
        return;

    auto insertionPos = dynamic_cast<MenuBarWindow*>(pWindow) ? mTaskPanes.begin() : mTaskPanes.end();
    for ( auto p = mTaskPanes.begin(); p != mTaskPanes.end(); ++p )
    {
        if ( *p == pWindow )
            // avoid duplicates
            return;

        // If the new window is the child of an existing pane window, or vice versa,
        // ensure that in our pane list, *first* the child window appears, *then*
        // the ancestor window.
        // This is necessary for HandleKeyEvent: There, the list is traveled from the
        // beginning, until the first window is found which has the ChildPathFocus. Now
        // if this would be the ancestor window of another pane window, this would fudge
        // the result
        if ( pWindow->IsWindowOrChild( *p ) )
        {
            insertionPos = p + 1;
            break;
        }
        if ( (*p)->IsWindowOrChild( pWindow ) )
        {
            insertionPos = p;
            break;
        }
    }

    mTaskPanes.insert( insertionPos, pWindow );
    pWindow->ImplIsInTaskPaneList( true );
}

void TaskPaneList::RemoveWindow( vcl::Window *pWindow )
{
    auto p = ::std::find( mTaskPanes.begin(), mTaskPanes.end(), VclPtr<vcl::Window>(pWindow) );
    if( p != mTaskPanes.end() )
    {
        mTaskPanes.erase( p );
        pWindow->ImplIsInTaskPaneList( false );
    }
}

bool TaskPaneList::IsInList( vcl::Window *pWindow )
{
    auto p = ::std::find( mTaskPanes.begin(), mTaskPanes.end(), VclPtr<vcl::Window>(pWindow) );
    return p != mTaskPanes.end();
}

bool TaskPaneList::IsCycleKey(const vcl::KeyCode& rKeyCode)
{
    return rKeyCode.GetCode() == KEY_F6 && !rKeyCode.IsMod2() && !(rKeyCode.IsMod1() && rKeyCode.IsShift()); // F6
}

bool TaskPaneList::HandleKeyEvent(const KeyEvent& rKeyEvent)
{

    // F6 cycles through everything and works always

    // MAV, #i104204#
    // The old design was the following one:
    // < Ctrl-TAB cycles through Menubar, Toolbars and Floatingwindows only and is
    // < only active if one of those items has the focus

    // Since the design of Ctrl-Tab looks to be inconsistent ( non-modal dialogs are not reachable
    // and the shortcut conflicts with tab-control shortcut ), it is no more supported
    vcl::KeyCode aKeyCode = rKeyEvent.GetKeyCode();
    bool bForward = !aKeyCode.IsShift();
    if (TaskPaneList::IsCycleKey(aKeyCode))
    {
        // is the focus in the list ?
        auto p = std::find_if(mTaskPanes.begin(), mTaskPanes.end(),
            [](const VclPtr<vcl::Window>& rWinPtr) { return rWinPtr->HasChildPathFocus( true ); });
        if( p != mTaskPanes.end() )
        {
            vcl::Window *pWin = p->get();

            // Ctrl-F6 goes directly to the document
            if( !pWin->IsDialog() && aKeyCode.IsMod1() )
            {
                pWin->ImplGrabFocusToDocument( GetFocusFlags::F6 );
                return true;
            }

            // activate next task pane
            vcl::Window *pNextWin = nullptr;

            pNextWin = FindNextFloat( *p, bForward );

            if( pNextWin != pWin )
            {
                ImplGetSVData()->mpWinData->mbNoSaveFocus = true;
                ImplTaskPaneListGrabFocus( pNextWin, bForward );
                ImplGetSVData()->mpWinData->mbNoSaveFocus = false;
            }
            else
            {
                // we did not find another taskpane, so
                // put focus back into document
                pWin->ImplGrabFocusToDocument( GetFocusFlags::F6 | (bForward ? GetFocusFlags::Forward : GetFocusFlags::Backward));
            }

            return true;
        }

        // the focus is not in the list: activate first float if F6 was pressed
        vcl::Window *pWin;

        pWin = FindNextFloat( nullptr, bForward );
        if( pWin )
        {
            ImplTaskPaneListGrabFocus( pWin, bForward );
            return true;
        }
    }

    return false;
}

// returns first valid item (regardless of type) if pWindow==0, otherwise returns next valid float
vcl::Window* TaskPaneList::FindNextFloat( vcl::Window *pWindow, bool bForward )
{
    ::std::stable_sort( mTaskPanes.begin(), mTaskPanes.end(), LTRSort() );

    if ( !bForward )
        ::std::reverse( mTaskPanes.begin(), mTaskPanes.end() );

    auto p = mTaskPanes.begin();
    if( pWindow )
        p = std::find(mTaskPanes.begin(), mTaskPanes.end(), pWindow);

    while( p != mTaskPanes.end() )
    {
        if( pWindow )   // increment before test
            ++p;
        if( p == mTaskPanes.end() )
            break; // do not wrap, send focus back to document at end of list
        /* #i83908# do not use the menubar if it is native and invisible
        */

        bool bSkip = false; // used to skip infobar when it has no children
        if( (*p)->GetType() == WindowType::WINDOW && (*p)->GetChildCount() == 0 )
            bSkip = true;

        if( !bSkip && (*p)->IsReallyVisible() && !(*p)->ImplIsSplitter() &&
            ( (*p)->GetType() != WindowType::MENUBARWINDOW || static_cast<MenuBarWindow*>(p->get())->CanGetFocus() ) )
        {
            pWindow = (*p).get();
            break;
        }
        if( !pWindow )  // increment after test, otherwise first element is skipped
            ++p;
    }

    if ( !bForward )
        ::std::reverse( mTaskPanes.begin(), mTaskPanes.end() );

    return pWindow;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
