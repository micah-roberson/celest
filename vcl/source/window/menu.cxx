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

#include <accessibility/accessiblemenubasecomponent.hxx>
#include <accessibility/vclxaccessiblemenubar.hxx>
#include <accessibility/vclxaccessiblepopupmenu.hxx>

#include <comphelper/diagnose_ex.hxx>
#include <sal/log.hxx>

#include <comphelper/types.hxx>
#include <comphelper/lok.hxx>
#include <vcl/dialoghelper.hxx>
#include <vcl/svapp.hxx>
#include <vcl/mnemonic.hxx>
#include <vcl/image.hxx>
#include <vcl/event.hxx>
#include <vcl/help.hxx>
#include <vcl/toolkit/floatwin.hxx>
#include <vcl/decoview.hxx>
#include <vcl/menu.hxx>
#include <vcl/taskpanelist.hxx>
#include <vcl/settings.hxx>
#include <vcl/commandinfoprovider.hxx>

#include <salinst.hxx>
#include <svdata.hxx>
#include <strings.hrc>
#include <window.h>
#include <salmenu.hxx>
#include <salframe.hxx>

#include "menubarwindow.hxx"
#include "menufloatingwindow.hxx"
#include "menuitemlist.hxx"

#include <com/sun/star/uno/Reference.h>
#include <com/sun/star/lang/XComponent.hpp>
#include <com/sun/star/accessibility/XAccessible.hpp>
#include <vcl/toolkit/unowrap.hxx>
#include <rtl/ustrbuf.hxx>

#include <map>
#include <string_view>
#include <vector>

#include <officecfg/Office/Common.hxx>
#include <officecfg/VCL.hxx>

namespace vcl
{

struct MenuLayoutData : public ControlLayoutData
{
    std::vector< sal_uInt16 >               m_aLineItemIds;
    std::map< sal_uInt16, tools::Rectangle >       m_aVisibleItemBoundRects;
};

}

using namespace vcl;

constexpr auto EXTRAITEMHEIGHT = 4;
constexpr auto SPACE_AROUND_TITLE = 4;

static void ImplSetMenuItemData( MenuItemData* pData )
{
    // convert data
    if ( !pData->aImage )
        pData->eType = MenuItemType::STRING;
    else if ( pData->aText.isEmpty() )
        pData->eType = MenuItemType::IMAGE;
    else
        pData->eType = MenuItemType::STRINGIMAGE;
}

namespace {

void ImplClosePopupToolBox( const VclPtr<vcl::Window>& pWin )
{
    if ( pWin->GetType() == WindowType::TOOLBOX && ImplGetDockingManager()->IsInPopupMode( pWin ) )
    {
        SystemWindow* pFloatingWindow = ImplGetDockingManager()->GetFloatingWindow(pWin);
        if (pFloatingWindow)
            static_cast<FloatingWindow*>(pFloatingWindow)->EndPopupMode( FloatWinPopupEndFlags::CloseAll );
    }
}

// TODO: Move to common code with the same function in toolbox
// Draw the ">>" - more indicator at the coordinates
void lclDrawMoreIndicator(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect)
{
    rRenderContext.Push(PushFlags::FILLCOLOR | PushFlags::LINECOLOR);
    rRenderContext.SetLineColor();

    if (rRenderContext.GetSettings().GetStyleSettings().GetFaceColor().IsDark())
        rRenderContext.SetFillColor(COL_WHITE);
    else
        rRenderContext.SetFillColor(COL_BLACK);
    float fScaleFactor = rRenderContext.GetDPIScaleFactor();

    int linewidth = 1 * fScaleFactor;
    int space = 4 * fScaleFactor;

    tools::Long width = 8 * fScaleFactor;
    tools::Long height = 5 * fScaleFactor;

    //Keep odd b/c drawing code works better
    if ( height % 2 == 0 )
        height--;

    tools::Long heightOrig = height;

    tools::Long x = rRect.Left() + (rRect.getOpenWidth() - width)/2 + 1;
    tools::Long y = rRect.Top() + (rRect.getOpenHeight() - height)/2 + 1;
    while( height >= 1)
    {
        rRenderContext.DrawRect( tools::Rectangle( x, y, x + linewidth, y ) );
        x += space;
        rRenderContext.DrawRect( tools::Rectangle( x, y, x + linewidth, y ) );
        x -= space;
        y++;
        if( height <= heightOrig / 2 + 1) x--;
        else            x++;
        height--;
    }
    rRenderContext.Pop();
}

} // end anonymous namespace


Menu::Menu()
    : mpFirstDel(nullptr),
      pItemList(new MenuItemList),
      pStartedFrom(nullptr),
      m_pWindow(nullptr),
      nTitleHeight(0),
      nEventId(nullptr),
      mnHighlightedItemPos(ITEMPOS_INVALID),
      nMenuFlags(MenuFlags::NONE),
      nSelectedId(0),
      nImgOrChkPos(0),
      nTextPos(0),
      bCanceled(false),
      bInCallback(false),
      bKilled(false)
{
}

Menu::~Menu()
{
    disposeOnce();
}

void Menu::dispose()
{
    ImplCallEventListeners( VclEventId::ObjectDying, ITEMPOS_INVALID );

    m_pWindow.disposeAndClear();

    if (mpAccessible.is())
        mpAccessible->dispose();
    mpAccessible.clear();

    if ( nEventId )
        Application::RemoveUserEvent( nEventId );

    // Notify deletion of this menu
    ImplMenuDelData* pDelData = mpFirstDel;
    while ( pDelData )
    {
        pDelData->mpMenu = nullptr;
        pDelData = pDelData->mpNext;
    }

    bKilled = true;

    // tdf#140225 when clearing pItemList, keep SalMenu in sync with
    // their removal during menu teardown
    for (size_t n = pItemList->size(); n;)
    {
        --n;
        if (mpSalMenu)
            mpSalMenu->RemoveItem(n);
        pItemList->Remove(n);
    }

    assert(!pItemList->size());

    mpLayoutData.reset();

    // Native-support: destroy SalMenu
    mpSalMenu.reset();

    pStartedFrom.reset();
    m_pWindow.reset();
    VclReferenceBase::dispose();
}

void Menu::CreateAutoMnemonics()
{
    MnemonicGenerator aMnemonicGenerator;
    size_t n;
    for ( n = 0; n < pItemList->size(); n++ )
    {
        MenuItemData* pData = pItemList->GetDataFromPos( n );
        if ( ! (pData->nBits & MenuItemBits::NOSELECT ) )
            aMnemonicGenerator.RegisterMnemonic( pData->aText );
    }
    for ( n = 0; n < pItemList->size(); n++ )
    {
        MenuItemData* pData = pItemList->GetDataFromPos( n );
        if ( ! (pData->nBits & MenuItemBits::NOSELECT ) )
            pData->aText = aMnemonicGenerator.CreateMnemonic( pData->aText );
    }
}

void Menu::Activate()
{
    bInCallback = true;

    ImplMenuDelData aDelData( this );

    ImplCallEventListeners( VclEventId::MenuActivate, ITEMPOS_INVALID );

    if( !aDelData.isDeleted() )
    {
        if ( !aActivateHdl.Call( this ) )
        {
            if( !aDelData.isDeleted() )
            {
                Menu* pStartMenu = ImplGetStartMenu();
                if ( pStartMenu && ( pStartMenu != this ) )
                {
                    pStartMenu->bInCallback = true;
                    // MT 11/01: Call EventListener here? I don't know...
                    pStartMenu->aActivateHdl.Call( this );
                    pStartMenu->bInCallback = false;
                }
            }
        }
        bInCallback = false;
    }

    if (!aDelData.isDeleted() && !(nMenuFlags & MenuFlags::NoAutoMnemonics))
        CreateAutoMnemonics();
}

void Menu::Deactivate()
{
    for ( size_t n = pItemList->size(); n; )
    {
        MenuItemData* pData = pItemList->GetDataFromPos( --n );
        if ( pData->bIsTemporary )
        {
            if ( ImplGetSalMenu() )
                ImplGetSalMenu()->RemoveItem( n );

            pItemList->Remove( n );
        }
    }

    bInCallback = true;

    ImplMenuDelData aDelData( this );

    Menu* pStartMenu = ImplGetStartMenu();
    ImplCallEventListeners( VclEventId::MenuDeactivate, ITEMPOS_INVALID );

    if( !aDelData.isDeleted() )
    {
        if ( !aDeactivateHdl.Call( this ) )
        {
            if( !aDelData.isDeleted() )
            {
                if ( pStartMenu && ( pStartMenu != this ) )
                {
                    pStartMenu->bInCallback = true;
                    pStartMenu->aDeactivateHdl.Call( this );
                    pStartMenu->bInCallback = false;
                }
            }
        }
    }

    if( !aDelData.isDeleted() )
    {
        bInCallback = false;
    }
}

void Menu::ImplSelect()
{
    MenuItemData* pData = GetItemList()->GetData( nSelectedId );
    if ( pData && (pData->nBits & MenuItemBits::AUTOCHECK) )
    {
        bool bChecked = IsItemChecked( nSelectedId );
        if ( pData->nBits & MenuItemBits::RADIOCHECK )
        {
            if ( !bChecked )
                CheckItem( nSelectedId );
        }
        else
            CheckItem( nSelectedId, !bChecked );
    }

    // call select
    ImplSVData* pSVData = ImplGetSVData();
    pSVData->maAppData.mpActivePopupMenu = nullptr;        // if new execute in select()
    nEventId = Application::PostUserEvent( LINK( this, Menu, ImplCallSelect ) );
}

void Menu::Select()
{
    ImplMenuDelData aDelData( this );

    ImplCallEventListeners( VclEventId::MenuSelect, GetItemPos( GetCurItemId() ) );
    if (aDelData.isDeleted())
        return;
    if (aSelectHdl.Call(this))
        return;
    if (aDelData.isDeleted())
        return;
    Menu* pStartMenu = ImplGetStartMenu();
    if (!pStartMenu || (pStartMenu == this))
        return;
    pStartMenu->nSelectedId = nSelectedId;
    pStartMenu->sSelectedIdent = sSelectedIdent;
    pStartMenu->aSelectHdl.Call( this );
}

#if defined(MACOSX)
void Menu::ImplSelectWithStart( Menu* pSMenu )
{
    auto pOldStartedFrom = pStartedFrom;
    pStartedFrom = pSMenu;
    auto pOldStartedStarted = pOldStartedFrom ? pOldStartedFrom->pStartedFrom : VclPtr<Menu>();
    Select();
    if( pOldStartedFrom )
        pOldStartedFrom->pStartedFrom = pOldStartedStarted;
    pStartedFrom = pOldStartedFrom;
}
#endif

void Menu::ImplCallEventListeners( VclEventId nEvent, sal_uInt16 nPos )
{
    ImplMenuDelData aDelData( this );

    VclMenuEvent aEvent( this, nEvent, nPos );

    // This is needed by atk accessibility bridge
    if ( nEvent == VclEventId::MenuHighlight )
    {
        Application::ImplCallEventListeners( aEvent );
    }

    if ( !aDelData.isDeleted() )
    {
        // Copy the list, because this can be destroyed when calling a Link...
        std::list<Link<VclMenuEvent&,void>> aCopy( maEventListeners );
        for ( const auto& rLink : aCopy )
        {
            if( std::find(maEventListeners.begin(), maEventListeners.end(), rLink) != maEventListeners.end() )
                rLink.Call( aEvent );
        }
    }
}

void Menu::AddEventListener( const Link<VclMenuEvent&,void>& rEventListener )
{
    maEventListeners.push_back( rEventListener );
}

void Menu::RemoveEventListener( const Link<VclMenuEvent&,void>& rEventListener )
{
    maEventListeners.remove( rEventListener );
}

MenuItemData* Menu::NbcInsertItem(sal_uInt16 nId, MenuItemBits nBits,
                                  const OUString& rStr, Menu* pMenu,
                                  size_t nPos, const OUString &rIdent)
{
    // put Item in MenuItemList
    MenuItemData* pData = pItemList->Insert(nId, MenuItemType::STRING,
                             nBits, rStr, pMenu, nPos, rIdent);

    // update native menu
    if (ImplGetSalMenu() && pData->pSalMenuItem)
        ImplGetSalMenu()->InsertItem(pData->pSalMenuItem.get(), nPos);

    return pData;
}

void Menu::InsertItem(sal_uInt16 nItemId, const OUString& rStr, MenuItemBits nItemBits,
    const OUString &rIdent, sal_uInt16 nPos)
{
    SAL_WARN_IF( !nItemId, "vcl", "Menu::InsertItem(): ItemId == 0" );
    SAL_WARN_IF( GetItemPos( nItemId ) != MENU_ITEM_NOTFOUND, "vcl",
                "Menu::InsertItem(): ItemId already exists" );

    // if Position > ItemCount, append
    if ( nPos >= pItemList->size() )
        nPos = MENU_APPEND;

    // put Item in MenuItemList
    NbcInsertItem(nItemId, nItemBits, rStr, this, nPos, rIdent);

    vcl::Window* pWin = GetWindow();
    mpLayoutData.reset();
    if ( pWin )
    {
        ImplCalcSize( pWin );
        if ( pWin->IsVisible() )
            pWin->Invalidate();
    }
    ImplCallEventListeners( VclEventId::MenuInsertItem, nPos );
}

void Menu::InsertItem(sal_uInt16 nItemId, const Image& rImage,
    MenuItemBits nItemBits, const OUString &rIdent, sal_uInt16 nPos)
{
    InsertItem(nItemId, OUString(), nItemBits, rIdent, nPos);
    SetItemImage( nItemId, rImage );
}

void Menu::InsertItem(sal_uInt16 nItemId, const OUString& rStr,
    const Image& rImage, MenuItemBits nItemBits,
    const OUString &rIdent, sal_uInt16 nPos)
{
    InsertItem(nItemId, rStr, nItemBits, rIdent, nPos);
    SetItemImage( nItemId, rImage );
}

void Menu::InsertSeparator(const OUString &rIdent, sal_uInt16 nPos)
{
    // do nothing if it's a menu bar
    if (IsMenuBar())
        return;

    // if position > ItemCount, append
    if ( nPos >= pItemList->size() )
        nPos = MENU_APPEND;

    // put separator in item list
    pItemList->InsertSeparator(rIdent, nPos);

    // update native menu
    size_t itemPos = ( nPos != MENU_APPEND ) ? nPos : pItemList->size() - 1;
    MenuItemData *pData = pItemList->GetDataFromPos( itemPos );
    if( ImplGetSalMenu() && pData && pData->pSalMenuItem )
        ImplGetSalMenu()->InsertItem( pData->pSalMenuItem.get(), nPos );

    mpLayoutData.reset();

    ImplCallEventListeners( VclEventId::MenuInsertItem, nPos );
}

void Menu::RemoveItem( sal_uInt16 nPos )
{
    bool bRemove = false;

    if ( nPos < GetItemCount() )
    {
        // update native menu
        if( ImplGetSalMenu() )
            ImplGetSalMenu()->RemoveItem( nPos );

        pItemList->Remove( nPos );
        bRemove = true;
    }

    vcl::Window* pWin = GetWindow();
    if ( pWin )
    {
        ImplCalcSize( pWin );
        if ( pWin->IsVisible() )
            pWin->Invalidate();
    }
    mpLayoutData.reset();

    if ( bRemove )
        ImplCallEventListeners( VclEventId::MenuRemoveItem, nPos );
}

static void ImplCopyItem( Menu* pThis, const Menu& rMenu, sal_uInt16 nPos, sal_uInt16 nNewPos )
{
    MenuItemType eType = rMenu.GetItemType( nPos );

    if ( eType == MenuItemType::DONTKNOW )
        return;

    if ( eType == MenuItemType::SEPARATOR )
        pThis->InsertSeparator( {}, nNewPos );
    else
    {
        sal_uInt16 nId = rMenu.GetItemId( nPos );

        SAL_WARN_IF( pThis->GetItemPos( nId ) != MENU_ITEM_NOTFOUND, "vcl",
                    "Menu::CopyItem(): ItemId already exists" );

        MenuItemData* pData = rMenu.GetItemList()->GetData( nId );

        if (!pData)
            return;

        if ( eType == MenuItemType::STRINGIMAGE )
            pThis->InsertItem( nId, pData->aText, pData->aImage, pData->nBits, pData->sIdent, nNewPos );
        else if ( eType == MenuItemType::STRING )
            pThis->InsertItem( nId, pData->aText, pData->nBits, pData->sIdent, nNewPos );
        else
            pThis->InsertItem( nId, pData->aImage, pData->nBits, pData->sIdent, nNewPos );

        if ( rMenu.IsItemChecked( nId ) )
            pThis->CheckItem( nId );
        if ( !rMenu.IsItemEnabled( nId ) )
            pThis->EnableItem( nId, false );
        pThis->SetHelpId( nId, pData->aHelpId );
        pThis->SetHelpText( nId, pData->aHelpText );
        pThis->SetAccelKey( nId, pData->aAccelKey );
        pThis->SetItemCommand( nId, pData->aCommandStr );
        pThis->SetHelpCommand( nId, pData->aHelpCommandStr );

        PopupMenu* pSubMenu = rMenu.GetPopupMenu( nId );
        if ( pSubMenu )
        {
            // create auto-copy
            VclPtr<PopupMenu> pNewMenu = VclPtr<PopupMenu>::Create( *pSubMenu );
            pThis->SetPopupMenu( nId, pNewMenu );
        }
    }
}

void Menu::Clear()
{
    for ( sal_uInt16 i = GetItemCount(); i; i-- )
        RemoveItem( 0 );
}

sal_uInt16 Menu::GetItemCount() const
{
    return static_cast<sal_uInt16>(pItemList->size());
}

bool Menu::HasValidEntries(bool bCheckPopups) const
{
    bool bValidEntries = false;
    sal_uInt16 nCount = GetItemCount();
    for (sal_uInt16 n = 0; !bValidEntries && (n < nCount); n++)
    {
        MenuItemData* pItem = pItemList->GetDataFromPos(n);
        if (pItem->bEnabled && (pItem->eType != MenuItemType::SEPARATOR))
        {
            if (bCheckPopups && pItem->pSubMenu)
                bValidEntries = pItem->pSubMenu->HasValidEntries(true);
            else
                bValidEntries = true;
        }
    }
    return bValidEntries;
}

sal_uInt16 Menu::ImplGetVisibleItemCount() const
{
    sal_uInt16 nItems = 0;
    for ( size_t n = pItemList->size(); n; )
    {
        if ( ImplIsVisible( --n ) )
            nItems++;
    }
    return nItems;
}

sal_uInt16 Menu::ImplGetFirstVisible() const
{
    for ( size_t n = 0; n < pItemList->size(); n++ )
    {
        if ( ImplIsVisible( n ) )
            return n;
    }
    return ITEMPOS_INVALID;
}

sal_uInt16 Menu::ImplGetPrevVisible( sal_uInt16 nPos ) const
{
    for ( size_t n = nPos; n; )
    {
        if (ImplIsVisible(--n))
            return n;
    }
    return ITEMPOS_INVALID;
}

sal_uInt16 Menu::ImplGetNextVisible( sal_uInt16 nPos ) const
{
    for ( size_t n = nPos+1; n < pItemList->size(); n++ )
    {
        if ( ImplIsVisible( n ) )
            return n;
    }
    return ITEMPOS_INVALID;
}

sal_uInt16 Menu::GetItemId(sal_uInt16 nPos) const
{
    MenuItemData* pData = pItemList->GetDataFromPos( nPos );

    if ( pData )
        return pData->nId;
    else
        return 0;
}

sal_uInt16 Menu::GetItemId(std::u16string_view rIdent) const
{
    for (size_t n = 0; n < pItemList->size(); ++n)
    {
        MenuItemData* pData = pItemList->GetDataFromPos(n);
        if (pData && pData->sIdent == rIdent)
            return pData->nId;
    }
    return MENU_ITEM_NOTFOUND;
}

sal_uInt16 Menu::GetItemPos( sal_uInt16 nItemId ) const
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( pData )
        return static_cast<sal_uInt16>(nPos);
    else
        return MENU_ITEM_NOTFOUND;
}

MenuItemType Menu::GetItemType( sal_uInt16 nPos ) const
{
    MenuItemData* pData = pItemList->GetDataFromPos( nPos );

    if ( pData )
        return pData->eType;
    else
        return MenuItemType::DONTKNOW;
}

OUString Menu::GetItemIdent(sal_uInt16 nId) const
{
    const MenuItemData* pData = pItemList->GetData(nId);
    return pData ? pData->sIdent : OUString();
}

void Menu::SetItemBits( sal_uInt16 nItemId, MenuItemBits nBits )
{
    size_t        nPos;
    MenuItemData* pData = pItemList->GetData(nItemId, nPos);

    if (pData && (pData->nBits != nBits))
    {
        // these two menu item bits are relevant for (accessible) role
        const MenuItemBits nRoleMask = MenuItemBits::CHECKABLE | MenuItemBits::RADIOCHECK;
        const bool bRoleBitsChanged = (pData->nBits & nRoleMask) != (nBits & nRoleMask);

        pData->nBits = nBits;

        // update native menu
        if (ImplGetSalMenu())
            ImplGetSalMenu()->SetItemBits(nPos, nBits);

        if (bRoleBitsChanged)
            ImplCallEventListeners(VclEventId::MenuItemRoleChanged, nPos);
    }
}

MenuItemBits Menu::GetItemBits( sal_uInt16 nItemId ) const
{
    MenuItemBits nBits = MenuItemBits::NONE;
    MenuItemData* pData = pItemList->GetData( nItemId );
    if ( pData )
        nBits = pData->nBits;
    return nBits;
}

void Menu::SetUserValue(sal_uInt16 nItemId, void* nUserValue, MenuUserDataReleaseFunction aFunc)
{
    MenuItemData* pData = pItemList->GetData(nItemId);
    if (pData)
    {
        if (pData->aUserValueReleaseFunc)
            pData->aUserValueReleaseFunc(pData->nUserValue);
        pData->aUserValueReleaseFunc = aFunc;
        pData->nUserValue = nUserValue;
    }
}

void* Menu::GetUserValue( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );
    return pData ? pData->nUserValue : nullptr;
}

void Menu::SetPopupMenu( sal_uInt16 nItemId, PopupMenu* pMenu )
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    // Item does not exist -> return NULL
    if ( !pData )
        return;

    // same menu, nothing to do
    if ( pData->pSubMenu.get() == pMenu )
        return;

    // remove old menu
    auto oldSubMenu = pData->pSubMenu;

    // data exchange
    pData->pSubMenu = pMenu;

    // #112023# Make sure pStartedFrom does not point to invalid (old) data
    if ( pData->pSubMenu )
        pData->pSubMenu->pStartedFrom = nullptr;

    // set native submenu
    if( ImplGetSalMenu() && pData->pSalMenuItem )
    {
        if( pMenu )
            ImplGetSalMenu()->SetSubMenu( pData->pSalMenuItem.get(), pMenu->ImplGetSalMenu(), nPos );
        else
            ImplGetSalMenu()->SetSubMenu( pData->pSalMenuItem.get(), nullptr, nPos );
    }

    oldSubMenu.disposeAndClear();

    ImplCallEventListeners( VclEventId::MenuSubmenuChanged, nPos );
}

PopupMenu* Menu::GetPopupMenu( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        return pData->pSubMenu.get();
    else
        return nullptr;
}

void Menu::SetAccelKey( sal_uInt16 nItemId, const KeyCode& rKeyCode )
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( !pData )
        return;

    if ( pData->aAccelKey == rKeyCode )
        return;

    pData->aAccelKey = rKeyCode;

    // update native menu
    if( ImplGetSalMenu() && pData->pSalMenuItem )
        ImplGetSalMenu()->SetAccelerator( nPos, pData->pSalMenuItem.get(), rKeyCode, rKeyCode.GetName() );
}

KeyCode Menu::GetAccelKey( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        return pData->aAccelKey;
    else
        return KeyCode();
}

KeyEvent Menu::GetActivationKey( sal_uInt16 nItemId ) const
{
    KeyEvent aRet;
    MenuItemData* pData = pItemList->GetData( nItemId );
    if( pData )
    {
        sal_Int32 nPos = pData->aText.indexOf( '~' );
        if( nPos != -1 && nPos < pData->aText.getLength()-1 )
        {
            sal_uInt16 nCode = 0;
            sal_Unicode cAccel = pData->aText[nPos+1];
            if( cAccel >= 'a' && cAccel <= 'z' )
                nCode = KEY_A + (cAccel-'a');
            else if( cAccel >= 'A' && cAccel <= 'Z' )
                nCode = KEY_A + (cAccel-'A');
            else if( cAccel >= '0' && cAccel <= '9' )
                nCode = KEY_0 + (cAccel-'0');

            aRet = KeyEvent( cAccel, KeyCode( nCode, KEY_MOD2 ) );
        }

    }
    return aRet;
}

void Menu::CheckItem( sal_uInt16 nItemId, bool bCheck )
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( !pData || pData->bChecked == bCheck )
        return;

    // if radio-check, then uncheck previous
    if ( bCheck && (pData->nBits & MenuItemBits::AUTOCHECK) &&
         (pData->nBits & MenuItemBits::RADIOCHECK) )
    {
        MenuItemData*   pGroupData;
        sal_uInt16          nGroupPos;
        sal_uInt16          nItemCount = GetItemCount();
        bool            bFound = false;

        nGroupPos = nPos;
        while ( nGroupPos )
        {
            pGroupData = pItemList->GetDataFromPos( nGroupPos-1 );
            if ( pGroupData->nBits & MenuItemBits::RADIOCHECK )
            {
                if ( IsItemChecked( pGroupData->nId ) )
                {
                    CheckItem( pGroupData->nId, false );
                    bFound = true;
                    break;
                }
            }
            else
                break;
            nGroupPos--;
        }

        if ( !bFound )
        {
            nGroupPos = nPos+1;
            while ( nGroupPos < nItemCount )
            {
                pGroupData = pItemList->GetDataFromPos( nGroupPos );
                if ( pGroupData->nBits & MenuItemBits::RADIOCHECK )
                {
                    if ( IsItemChecked( pGroupData->nId ) )
                    {
                        CheckItem( pGroupData->nId, false );
                        break;
                    }
                }
                else
                    break;
                nGroupPos++;
            }
        }
    }

    pData->bChecked = bCheck;

    // update native menu
    if( ImplGetSalMenu() )
        ImplGetSalMenu()->CheckItem( nPos, bCheck );

    ImplCallEventListeners( bCheck ? VclEventId::MenuItemChecked : VclEventId::MenuItemUnchecked, nPos );
}

void Menu::CheckItem( std::u16string_view rIdent , bool bCheck )
{
    CheckItem( GetItemId( rIdent ), bCheck );
}

bool Menu::IsItemCheckable(sal_uInt16 nItemId) const
{
    size_t nPos;
    MenuItemData* pData = pItemList->GetData(nItemId, nPos);

    if (!pData)
        return false;

    return pData->HasCheck();
}

bool Menu::IsItemChecked( sal_uInt16 nItemId ) const
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( !pData )
        return false;

    return pData->bChecked;
}

void Menu::EnableItem( sal_uInt16 nItemId, bool bEnable )
{
    size_t          nPos;
    MenuItemData*   pItemData = pItemList->GetData( nItemId, nPos );

    if ( !(pItemData && ( pItemData->bEnabled != bEnable )) )
        return;

    pItemData->bEnabled = bEnable;

    vcl::Window* pWin = GetWindow();
    if ( pWin && pWin->IsVisible() )
    {
        SAL_WARN_IF(!IsMenuBar(), "vcl", "Menu::EnableItem - Popup visible!" );
        tools::Long nX = 0;
        size_t nCount = pItemList->size();
        for ( size_t n = 0; n < nCount; n++ )
        {
            MenuItemData* pData = pItemList->GetDataFromPos( n );
            if ( n == nPos )
            {
                pWin->Invalidate( tools::Rectangle( Point( nX, 0 ), Size( pData->aSz.Width(), pData->aSz.Height() ) ) );
                break;
            }
            nX += pData->aSz.Width();
        }
    }
    // update native menu
    if( ImplGetSalMenu() )
        ImplGetSalMenu()->EnableItem( nPos, bEnable );

    ImplCallEventListeners( bEnable ? VclEventId::MenuEnable : VclEventId::MenuDisable, nPos );
}

bool Menu::IsItemEnabled( sal_uInt16 nItemId ) const
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( !pData )
        return false;

    return pData->bEnabled;
}

void Menu::ShowItem( sal_uInt16 nItemId, bool bVisible )
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    SAL_WARN_IF(IsMenuBar() && !bVisible , "vcl", "Menu::ShowItem - ignored for menu bar entries!");
    if (IsMenuBar() || !pData || (pData->bVisible == bVisible))
        return;

    vcl::Window* pWin = GetWindow();
    if ( pWin && pWin->IsVisible() )
    {
        SAL_WARN( "vcl", "Menu::ShowItem - ignored for visible popups!" );
        return;
    }
    pData->bVisible = bVisible;

    // update native menu
    if( ImplGetSalMenu() )
        ImplGetSalMenu()->ShowItem( nPos, bVisible );
}

void Menu::SetItemText( sal_uInt16 nItemId, const OUString& rStr )
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( !pData )
        return;

    if ( rStr == pData->aText )
        return;

    pData->aText = rStr;
    // Clear layout for aText.
    pData->aTextGlyphs.Invalidate();
    ImplSetMenuItemData( pData );
    // update native menu
    if( ImplGetSalMenu() && pData->pSalMenuItem )
        ImplGetSalMenu()->SetItemText( nPos, pData->pSalMenuItem.get(), rStr );

    vcl::Window* pWin = GetWindow();
    mpLayoutData.reset();
    if (pWin && IsMenuBar())
    {
        ImplCalcSize( pWin );
        if ( pWin->IsVisible() )
            pWin->Invalidate();
    }

    ImplCallEventListeners( VclEventId::MenuItemTextChanged, nPos );
}

OUString Menu::GetItemText( sal_uInt16 nItemId ) const
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( pData )
        return pData->aText;

    return OUString();
}

void Menu::SetItemImage( sal_uInt16 nItemId, const Image& rImage )
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( !pData )
        return;

    pData->aImage = rImage;
    ImplSetMenuItemData( pData );

    // update native menu
    if( ImplGetSalMenu() && pData->pSalMenuItem )
        ImplGetSalMenu()->SetItemImage( nPos, pData->pSalMenuItem.get(), rImage );
}

Image Menu::GetItemImage( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        return pData->aImage;
    else
        return Image();
}

void Menu::SetItemCommand( sal_uInt16 nItemId, const OUString& rCommand )
{
    size_t        nPos;
    MenuItemData* pData = pItemList->GetData( nItemId, nPos );

    if ( pData )
        pData->aCommandStr = rCommand;
}

OUString Menu::GetItemCommand( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if (pData)
        return pData->aCommandStr;

    return OUString();
}

void Menu::SetHelpCommand( sal_uInt16 nItemId, const OUString& rStr )
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        pData->aHelpCommandStr = rStr;
}

OUString Menu::GetHelpCommand( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        return pData->aHelpCommandStr;

    return OUString();
}

void Menu::SetHelpText( sal_uInt16 nItemId, const OUString& rStr )
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        pData->aHelpText = rStr;
}

OUString Menu::ImplGetHelpText( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if (!pData)
        return OUString();

    if ( pData->aHelpText.isEmpty() &&
         (( !pData->aHelpId.isEmpty()  ) || ( !pData->aCommandStr.isEmpty() )))
    {
        Help* pHelp = Application::GetHelp();
        if ( pHelp )
        {
            if (!pData->aCommandStr.isEmpty())
                pData->aHelpText = pHelp->GetHelpText( pData->aCommandStr );
            if (pData->aHelpText.isEmpty() && !pData->aHelpId.isEmpty())
                pData->aHelpText = pHelp->GetHelpText( pData->aHelpId );
        }
    }

    //Fallback to Menu::GetAccessibleDescription without reentry to GetHelpText()
    if (pData->aHelpText.isEmpty())
        return pData->aAccessibleDescription;
    return pData->aHelpText;
}

OUString Menu::GetHelpText( sal_uInt16 nItemId ) const
{
    return ImplGetHelpText( nItemId );
}

void Menu::SetTipHelpText( sal_uInt16 nItemId, const OUString& rStr )
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
    {
        pData->aTipHelpText = rStr;

        if (ImplGetSalMenu() && pData->pSalMenuItem)
            ImplGetSalMenu()->SetItemTooltip(pData->pSalMenuItem.get(), rStr);
    }
}

OUString Menu::GetTipHelpText( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        return pData->aTipHelpText;

    return OUString();
}

void Menu::SetHelpId( sal_uInt16 nItemId, const OUString& rHelpId )
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        pData->aHelpId = rHelpId;
}

OUString Menu::GetHelpId( sal_uInt16 nItemId ) const
{
    OUString aRet;

    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
    {
        if ( !pData->aHelpId.isEmpty() )
            aRet = pData->aHelpId;
        else
            aRet = pData->aCommandStr;
    }

    return aRet;
}

Menu& Menu::operator=( const Menu& rMenu )
{
    if(this == &rMenu)
        return *this;

    // clean up
    Clear();

    // copy items
    sal_uInt16 nCount = rMenu.GetItemCount();
    for ( sal_uInt16 i = 0; i < nCount; i++ )
        ImplCopyItem( this, rMenu, i, MENU_APPEND );

    aActivateHdl = rMenu.aActivateHdl;
    aDeactivateHdl = rMenu.aDeactivateHdl;
    aSelectHdl = rMenu.aSelectHdl;
    aTitleText = rMenu.aTitleText;
    nTitleHeight = rMenu.nTitleHeight;

    return *this;
}

// Returns true if the item is completely hidden on the GUI and shouldn't
// be possible to interact with
bool Menu::ImplCurrentlyHiddenOnGUI(sal_uInt16 nPos) const
{
    MenuItemData* pData = pItemList->GetDataFromPos(nPos);
    if (pData)
    {
        MenuItemData* pPreviousData = pItemList->GetDataFromPos( nPos - 1 );
        if (pPreviousData && pPreviousData->bHiddenOnGUI)
        {
            return true;
        }
    }
    return false;
}

bool Menu::ImplIsVisible( sal_uInt16 nPos ) const
{
    bool bVisible = true;

    MenuItemData* pData = pItemList->GetDataFromPos( nPos );
    // check general visibility first
    if( pData && !pData->bVisible )
        bVisible = false;

    if ( bVisible && pData && pData->eType == MenuItemType::SEPARATOR )
    {
        if( nPos == 0 ) // no separator should be shown at the very beginning
            bVisible = false;
        else
        {
            // always avoid adjacent separators
            size_t nCount = pItemList->size();
            size_t n;
            MenuItemData* pNextData = nullptr;
            // search next visible item
            for( n = nPos + 1; n < nCount; n++ )
            {
                pNextData = pItemList->GetDataFromPos( n );
                if( pNextData && pNextData->bVisible )
                {
                    if( pNextData->eType == MenuItemType::SEPARATOR || ImplIsVisible(n) )
                        break;
                }
            }
            if( n == nCount ) // no next visible item
                bVisible = false;
            // check for separator
            if( pNextData && pNextData->bVisible && pNextData->eType == MenuItemType::SEPARATOR )
                bVisible = false;

            if( bVisible )
            {
                for( n = nPos; n > 0; n-- )
                {
                    pNextData = pItemList->GetDataFromPos( n-1 );
                    if( pNextData && pNextData->bVisible )
                    {
                        if( pNextData->eType != MenuItemType::SEPARATOR && ImplIsVisible(n-1) )
                            break;
                    }
                }
                if( n == 0 ) // no previous visible item
                    bVisible = false;
            }
        }
    }

    // not allowed for menubar, as I do not know
    // whether a menu-entry will disappear or will appear
    if (bVisible && !IsMenuBar() && (nMenuFlags & MenuFlags::HideDisabledEntries) &&
        !(nMenuFlags & MenuFlags::AlwaysShowDisabledEntries))
    {
        if( !pData ) // e.g. nPos == ITEMPOS_INVALID
            bVisible = false;
        else if ( pData->eType != MenuItemType::SEPARATOR ) // separators handled above
        {
            // tdf#86850 Always display clipboard functions
            if ( pData->aCommandStr == ".uno:Cut" || pData->aCommandStr == ".uno:Copy" || pData->aCommandStr == ".uno:Paste" ||
                 pData->sIdent == ".uno:Cut" || pData->sIdent == ".uno:Copy" || pData->sIdent == ".uno:Paste" )
                bVisible = true;
            else
                // bVisible = pData->bEnabled && ( !pData->pSubMenu || pData->pSubMenu->HasValidEntries( true ) );
                bVisible = pData->bEnabled; // do not check submenus as they might be filled at Activate().
        }
    }

    return bVisible;
}

bool Menu::IsItemPosVisible( sal_uInt16 nItemPos ) const
{
    return IsMenuVisible() && ImplIsVisible( nItemPos );
}

bool Menu::IsMenuVisible() const
{
    return m_pWindow && m_pWindow->IsReallyVisible();
}

bool Menu::ImplIsSelectable( sal_uInt16 nPos ) const
{
    bool bSelectable = true;

    MenuItemData* pData = pItemList->GetDataFromPos( nPos );
    // check general visibility first
    if ( pData && ( pData->nBits & MenuItemBits::NOSELECT ) )
        bSelectable = false;

    return bSelectable;
}

rtl::Reference<comphelper::OAccessible> Menu::CreateAccessible()
{
    rtl::Reference<OAccessibleMenuBaseComponent> xAccessible;
    if (IsMenuBar())
        xAccessible = new VCLXAccessibleMenuBar(this);
    else
        xAccessible = new VCLXAccessiblePopupMenu(this);
    xAccessible->SetStates();
    return xAccessible;
}

rtl::Reference<comphelper::OAccessible> Menu::GetAccessible()
{
    // Since PopupMenu are sometimes shared by different instances of MenuBar, the mpAccessible member gets
    // overwritten and may contain a disposed object when the initial menubar gets set again. So use the
    // mpAccessible member only for sub menus.
    if (pStartedFrom && pStartedFrom != this)
    {
        for ( sal_uInt16 i = 0, nCount = pStartedFrom->GetItemCount(); i < nCount; ++i )
        {
            sal_uInt16 nItemId = pStartedFrom->GetItemId( i );
            if ( static_cast< Menu* >( pStartedFrom->GetPopupMenu( nItemId ) ) == this )
            {
                rtl::Reference<comphelper::OAccessible> pParent = pStartedFrom->GetAccessible();
                if (pParent.is())
                {
                    css::uno::Reference<css::accessibility::XAccessible> xAcc = pParent->getAccessibleChild(i);
                    if (!xAcc)
                        return {};

                    rtl::Reference<comphelper::OAccessible> pAccessible = dynamic_cast<comphelper::OAccessible*>(xAcc.get());
                    assert(pAccessible.is() && "Accessible is not an OAccessible");
                    return pAccessible;
                }
            }
        }
    }
    else if (!mpAccessible.is())
        mpAccessible = CreateAccessible();

    return mpAccessible;
}

void Menu::SetAccessible(const rtl::Reference<comphelper::OAccessible>& rAccessible)
{
    mpAccessible = rAccessible;
}

Size Menu::ImplGetNativeCheckAndRadioSize(vcl::RenderContext const & rRenderContext, tools::Long& rCheckHeight, tools::Long& rRadioHeight ) const
{
    tools::Long nCheckWidth = 0, nRadioWidth = 0;
    rCheckHeight = rRadioHeight = 0;

    if (!IsMenuBar())
    {
        ImplControlValue aVal;
        tools::Rectangle aNativeBounds;
        tools::Rectangle aNativeContent;

        tools::Rectangle aCtrlRegion(tools::Rectangle(Point(), Size(100, 15)));
        if (rRenderContext.IsNativeControlSupported(ControlType::MenuPopup, ControlPart::MenuItemCheckMark))
        {
            if (rRenderContext.GetNativeControlRegion(ControlType::MenuPopup, ControlPart::MenuItemCheckMark,
                                              aCtrlRegion, ControlState::ENABLED, aVal,
                                              aNativeBounds, aNativeContent))
            {
                rCheckHeight = aNativeBounds.GetHeight() - 1;
                nCheckWidth = aNativeContent.GetWidth() - 1;
            }
        }
        if (rRenderContext.IsNativeControlSupported(ControlType::MenuPopup, ControlPart::MenuItemRadioMark))
        {
            if (rRenderContext.GetNativeControlRegion(ControlType::MenuPopup, ControlPart::MenuItemRadioMark,
                                                      aCtrlRegion, ControlState::ENABLED, aVal,
                                                      aNativeBounds, aNativeContent))
            {
                rRadioHeight = aNativeBounds.GetHeight() - 1;
                nRadioWidth = aNativeContent.GetWidth() - 1;
            }
        }
    }
    return Size(std::max(nCheckWidth, nRadioWidth), std::max(rCheckHeight, rRadioHeight));
}

bool Menu::ImplGetNativeSubmenuArrowSize(vcl::RenderContext const & rRenderContext, Size& rArrowSize, tools::Long& rArrowSpacing)
{
    ImplControlValue aVal;
    tools::Rectangle aCtrlRegion(tools::Rectangle(Point(), Size(100, 15)));
    if (rRenderContext.IsNativeControlSupported(ControlType::MenuPopup, ControlPart::SubmenuArrow))
    {
        tools::Rectangle aNativeContent;
        tools::Rectangle aNativeBounds;
        if (rRenderContext.GetNativeControlRegion(ControlType::MenuPopup, ControlPart::SubmenuArrow,
                                                  aCtrlRegion, ControlState::ENABLED,
                                                  aVal, aNativeBounds, aNativeContent))
        {
            Size aSize(aNativeContent.GetWidth(), aNativeContent.GetHeight());
            rArrowSize = aSize;
            rArrowSpacing = aNativeBounds.GetWidth() - aNativeContent.GetWidth();
            return true;
        }
    }
    return false;
}

void Menu::ImplAddDel( ImplMenuDelData& rDel )
{
    SAL_WARN_IF( rDel.mpMenu, "vcl", "Menu::ImplAddDel(): cannot add ImplMenuDelData twice !" );
    if( !rDel.mpMenu )
    {
        rDel.mpMenu = this;
        rDel.mpNext = mpFirstDel;
        mpFirstDel = &rDel;
    }
}

void Menu::ImplRemoveDel( ImplMenuDelData& rDel )
{
    rDel.mpMenu = nullptr;
    if ( mpFirstDel == &rDel )
    {
        mpFirstDel = rDel.mpNext;
    }
    else
    {
        ImplMenuDelData* pData = mpFirstDel;
        while ( pData && (pData->mpNext != &rDel) )
            pData = pData->mpNext;

        SAL_WARN_IF( !pData, "vcl", "Menu::ImplRemoveDel(): ImplMenuDelData not registered !" );
        if( pData )
            pData->mpNext = rDel.mpNext;
    }
}

Size Menu::ImplCalcSize( vcl::Window* pWin )
{
    // | Check/Radio/Image| Text| Accel/Popup|

    // for symbols: nFontHeight x nFontHeight
    tools::Long nFontHeight = pWin->GetTextHeight();
    tools::Long nExtra = nFontHeight/4;

    tools::Long nMinMenuItemHeight = nFontHeight;
    tools::Long nCheckHeight = 0, nRadioHeight = 0;
    Size aMarkSize = ImplGetNativeCheckAndRadioSize(*pWin->GetOutDev(), nCheckHeight, nRadioHeight);
    if( aMarkSize.Height() > nMinMenuItemHeight )
        nMinMenuItemHeight = aMarkSize.Height();

    tools::Long aMaxImgWidth = 0;

    const StyleSettings& rSettings = pWin->GetSettings().GetStyleSettings();
    if ( rSettings.GetUseImagesInMenus() )
    {
        if ( 16 > nMinMenuItemHeight )
            nMinMenuItemHeight = 16;
        for ( size_t i = pItemList->size(); i; )
        {
            MenuItemData* pData = pItemList->GetDataFromPos( --i );
            if ( ImplIsVisible( i )
               && (  ( pData->eType == MenuItemType::IMAGE )
                  || ( pData->eType == MenuItemType::STRINGIMAGE )
                  )
               )
            {
                Size aImgSz = pData->aImage.GetSizePixel();
                if ( aImgSz.Width() > aMaxImgWidth )
                    aMaxImgWidth = aImgSz.Width();
                if ( aImgSz.Height() > nMinMenuItemHeight )
                    nMinMenuItemHeight = aImgSz.Height();
                break;
            }
        }
    }

    Size aSz;
    tools::Long nMaxWidth = 0;

    for ( size_t n = pItemList->size(); n; )
    {
        MenuItemData* pData = pItemList->GetDataFromPos( --n );

        pData->aSz.setHeight( 0 );
        pData->aSz.setWidth( 0 );

        if ( ImplIsVisible( n ) )
        {
            tools::Long nWidth = 0;

            // Separator
            if (!IsMenuBar()&& (pData->eType == MenuItemType::SEPARATOR))
            {
                pData->aSz.setHeight( 4 );
            }

            // Image:
            if (!IsMenuBar() && ((pData->eType == MenuItemType::IMAGE) || (pData->eType == MenuItemType::STRINGIMAGE)))
            {
                tools::Long aImgHeight = pData->aImage.GetSizePixel().Height();

                aImgHeight += 4; // add a border for native marks
                if (aImgHeight > pData->aSz.Height())
                    pData->aSz.setHeight(aImgHeight);
            }

            // Check Buttons:
            if (!IsMenuBar() && pData->HasCheck())
            {
                // checks / images take the same place
                if( ( pData->eType != MenuItemType::IMAGE ) && ( pData->eType != MenuItemType::STRINGIMAGE ) )
                {
                    nWidth += aMarkSize.Width() + nExtra * 2;
                    if (aMarkSize.Height() > pData->aSz.Height())
                        pData->aSz.setHeight(aMarkSize.Height());
                }
            }

            // Text:
            if ( (pData->eType == MenuItemType::STRING) || (pData->eType == MenuItemType::STRINGIMAGE) )
            {
                const SalLayoutGlyphs* pGlyphs = pData->GetTextGlyphs(pWin->GetOutDev());
                tools::Long nTextWidth = pWin->GetOutDev()->GetCtrlTextWidth(pData->aText, pGlyphs);
                tools::Long nTextHeight = pWin->GetTextHeight() + EXTRAITEMHEIGHT;

                if (IsMenuBar())
                {
                    if ( nTextHeight > pData->aSz.Height() )
                        pData->aSz.setHeight( nTextHeight );

                    pData->aSz.setWidth( nTextWidth + 4*nExtra );
                    aSz.AdjustWidth(pData->aSz.Width() );
                }
                else
                    pData->aSz.setHeight( std::max( std::max( nTextHeight, pData->aSz.Height() ), nMinMenuItemHeight ) );

                nWidth += nTextWidth;
            }

            // Accel
            if (!IsMenuBar()&& pData->aAccelKey.GetCode() && !officecfg::VCL::VCLSettings::Menu::SuppressAccelerators::get())
            {
                OUString aName = pData->aAccelKey.GetName();
                tools::Long nAccWidth = pWin->GetTextWidth( aName );
                nAccWidth += nExtra;
                nWidth += nAccWidth;
            }

            // SubMenu?
            if (!IsMenuBar() && pData->pSubMenu)
            {
                if ( nFontHeight > nWidth )
                    nWidth += nFontHeight;

                pData->aSz.setHeight( std::max( std::max( nFontHeight, pData->aSz.Height() ), nMinMenuItemHeight ) );
            }

            if (!IsMenuBar())
                aSz.AdjustHeight(pData->aSz.Height() );

            if ( nWidth > nMaxWidth )
                nMaxWidth = nWidth;

        }
    }

    // Additional space for title
    nTitleHeight = 0;
    if (!IsMenuBar() && aTitleText.getLength() > 0) {
        // Set expected font
        pWin->GetOutDev()->Push(PushFlags::FONT);
        vcl::Font aFont = pWin->GetFont();
        aFont.SetWeight(WEIGHT_BOLD);
        pWin->SetFont(aFont);

        // Compute text bounding box
        tools::Rectangle aTextBoundRect;
        pWin->GetOutDev()->GetTextBoundRect(aTextBoundRect, aTitleText);

        // Vertically, one height of char + extra space for decoration
        nTitleHeight =  aTextBoundRect.GetSize().Height() + 4 * SPACE_AROUND_TITLE ;
        aSz.AdjustHeight(nTitleHeight );

        tools::Long nWidth = aTextBoundRect.GetSize().Width() + 4 * SPACE_AROUND_TITLE;
        pWin->GetOutDev()->Pop();
        if ( nWidth > nMaxWidth )
            nMaxWidth = nWidth;
    }

    if (!IsMenuBar())
    {
        // popup menus should not be wider than half the screen
        // except on rather small screens
        // TODO: move GetScreenNumber from SystemWindow to Window ?
        // currently we rely on internal privileges
        unsigned int nDisplayScreen = pWin->ImplGetWindowImpl()->mpFrame->GetUnmirroredGeometry().screen();
        tools::Rectangle aDispRect( Application::GetScreenPosSizePixel( nDisplayScreen ) );
        tools::Long nScreenWidth = aDispRect.GetWidth() >= 800 ? aDispRect.GetWidth() : 800;
        if( nMaxWidth > nScreenWidth/2 )
            nMaxWidth = nScreenWidth/2;

        sal_uInt16 gfxExtra = static_cast<sal_uInt16>(std::max( nExtra, tools::Long(7) )); // #107710# increase space between checkmarks/images/text
        nImgOrChkPos = static_cast<sal_uInt16>(nExtra);
        tools::Long nImgOrChkWidth = 0;
        if( aMarkSize.Height() > 0 ) // NWF case
            nImgOrChkWidth = aMarkSize.Height() + nExtra;
        else // non NWF case
            nImgOrChkWidth = nFontHeight/2 + gfxExtra;
        nImgOrChkWidth = std::max( nImgOrChkWidth, aMaxImgWidth + gfxExtra );
        nTextPos = static_cast<sal_uInt16>(nImgOrChkPos + nImgOrChkWidth);
        nTextPos = nTextPos + gfxExtra;

        aSz.setWidth( nTextPos + nMaxWidth + nExtra );
        aSz.AdjustWidth(4*nExtra );   // a _little_ more ...

        aSz.AdjustWidth(2*ImplGetSVData()->maNWFData.mnMenuFormatBorderX );
        aSz.AdjustHeight(2*ImplGetSVData()->maNWFData.mnMenuFormatBorderY );
    }
    else
    {
        nTextPos = static_cast<sal_uInt16>(2*nExtra);
        aSz.setHeight( nFontHeight+6 );

        // get menubar height from native methods if supported
        if (m_pWindow->IsNativeControlSupported(ControlType::Menubar, ControlPart::Entire))
        {
            ImplControlValue aVal;
            tools::Rectangle aNativeBounds;
            tools::Rectangle aNativeContent;
            Point tmp( 0, 0 );
            tools::Rectangle aCtrlRegion( tmp, Size( 100, 15 ) );
            if (m_pWindow->GetNativeControlRegion(ControlType::Menubar,
                                                  ControlPart::Entire,
                                                  aCtrlRegion,
                                                  ControlState::ENABLED,
                                                  aVal,
                                                  aNativeBounds,
                                                  aNativeContent)
            )
            {
                int nNativeHeight = aNativeBounds.GetHeight();
                if( nNativeHeight > aSz.Height() )
                    aSz.setHeight( nNativeHeight );
            }
        }

        // account for the size of the close button, which actually is a toolbox
        // due to NWF this is variable
        tools::Long nCloseButtonHeight = static_cast<MenuBarWindow*>(m_pWindow.get())->MinCloseButtonSize().Height();
        if (aSz.Height() < nCloseButtonHeight)
            aSz.setHeight( nCloseButtonHeight );
    }

    return aSz;
}

static void ImplPaintCheckBackground(vcl::RenderContext & rRenderContext, vcl::Window const & rWindow, const tools::Rectangle& i_rRect, bool i_bHighlight)
{
    bool bNativeOk = false;
    if (rRenderContext.IsNativeControlSupported(ControlType::Toolbar, ControlPart::Button))
    {
        ImplControlValue    aControlValue;
        aControlValue.setTristateVal(ButtonValue::On);
        tools::Rectangle r = i_rRect;
        r.AdjustBottom(1);

        bNativeOk = rRenderContext.DrawNativeControl(ControlType::Toolbar, ControlPart::Button,
                                                     r,
                                                     ControlState::PRESSED | ControlState::ENABLED,
                                                     aControlValue,
                                                     OUString());
    }

    if (!bNativeOk)
    {
        const StyleSettings& rSettings = rRenderContext.GetSettings().GetStyleSettings();
        Color aColor( i_bHighlight ? rSettings.GetMenuHighlightTextColor() : rSettings.GetHighlightColor() );
        RenderTools::DrawSelectionBackground(rRenderContext, rWindow, i_rRect, 0, i_bHighlight, true, false, nullptr, 2, &aColor);
    }
}

static OUString getShortenedString( const OUString& i_rLong, vcl::RenderContext const & rRenderContext, tools::Long i_nMaxWidth )
{
    sal_Int32 nPos = -1;
    OUString aNonMnem(removeMnemonicFromString(i_rLong, nPos));
    aNonMnem = rRenderContext.GetEllipsisString( aNonMnem, i_nMaxWidth, DrawTextFlags::CenterEllipsis);
    // re-insert mnemonic
    if (nPos != -1)
    {
        if (nPos < aNonMnem.getLength() && i_rLong[nPos+1] == aNonMnem[nPos])
            aNonMnem = OUString::Concat(aNonMnem.subView(0, nPos)) + "~" + aNonMnem.subView(nPos);
    }
    return aNonMnem;
}

void Menu::ImplPaintMenuTitle(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect ) const
{
    // Save previous graphical settings, set new one
    rRenderContext.Push(PushFlags::FONT | PushFlags::FILLCOLOR);
    Wallpaper aOldBackground = rRenderContext.GetBackground();

    Color aBackgroundColor = rRenderContext.GetSettings().GetStyleSettings().GetMenuBarColor();
    rRenderContext.SetBackground(Wallpaper(aBackgroundColor));
    rRenderContext.SetFillColor(aBackgroundColor);
    vcl::Font aFont = rRenderContext.GetFont();
    aFont.SetWeight(WEIGHT_BOLD);
    rRenderContext.SetFont(aFont);

    // Draw background rectangle
    tools::Rectangle aBgRect(rRect);
    int nOuterSpaceX = ImplGetSVData()->maNWFData.mnMenuFormatBorderX;
    aBgRect.Move(SPACE_AROUND_TITLE, SPACE_AROUND_TITLE);
    aBgRect.setWidth(aBgRect.getOpenWidth() - 2 * SPACE_AROUND_TITLE - 2 * nOuterSpaceX);
    aBgRect.setHeight(nTitleHeight - 2 * SPACE_AROUND_TITLE);
    rRenderContext.DrawRect(aBgRect);

    // Draw the text centered
    Point aTextTopLeft(aBgRect.TopLeft());
    tools::Rectangle aTextBoundRect;
    rRenderContext.GetTextBoundRect( aTextBoundRect, aTitleText );
    aTextTopLeft.AdjustX((aBgRect.getOpenWidth() - aTextBoundRect.GetSize().Width()) / 2 );
    aTextTopLeft.AdjustY((aBgRect.GetHeight() - aTextBoundRect.GetSize().Height()) / 2
                        - aTextBoundRect.Top() );
    rRenderContext.DrawText(aTextTopLeft, aTitleText, 0, aTitleText.getLength());

    // Restore
    rRenderContext.Pop();
    rRenderContext.SetBackground(aOldBackground);
}

void Menu::ImplPaint(vcl::RenderContext& rRenderContext, Size const & rSize,
                     sal_uInt16 nBorder, tools::Long nStartY, MenuItemData const * pThisItemOnly,
                     bool bHighlighted, bool bLayout, bool bRollover) const
{
    // for symbols: nFontHeight x nFontHeight
    tools::Long nFontHeight = rRenderContext.GetTextHeight();
    tools::Long nExtra = nFontHeight / 4;

    tools::Long nCheckHeight = 0, nRadioHeight = 0;
    ImplGetNativeCheckAndRadioSize(rRenderContext, nCheckHeight, nRadioHeight);

    DecorationView aDecoView(&rRenderContext);
    const StyleSettings& rSettings = rRenderContext.GetSettings().GetStyleSettings();

    Point aTopLeft, aTmpPos;

    int nOuterSpaceX = 0;
    if (!IsMenuBar())
    {
        nOuterSpaceX = ImplGetSVData()->maNWFData.mnMenuFormatBorderX;
        aTopLeft.AdjustX(nOuterSpaceX );
        aTopLeft.AdjustY(ImplGetSVData()->maNWFData.mnMenuFormatBorderY );
    }

    // for the computations, use size of the underlying window, not of RenderContext
    Size aOutSz(rSize);

    size_t nCount = pItemList->size();
    if (bLayout)
        mpLayoutData->m_aVisibleItemBoundRects.clear();

    // Paint title
    if (!pThisItemOnly && !IsMenuBar() && nTitleHeight > 0)
        ImplPaintMenuTitle(rRenderContext, tools::Rectangle(aTopLeft, aOutSz));

    bool bHiddenItems = false; // are any items on the GUI hidden

    for (size_t n = 0; n < nCount; n++)
    {
        MenuItemData* pData = pItemList->GetDataFromPos( n );
        if (ImplIsVisible(n) && (!pThisItemOnly || (pData == pThisItemOnly)))
        {
            if (pThisItemOnly)
            {
                if (IsMenuBar())
                {
                    if (!ImplGetSVData()->maNWFData.mbRolloverMenubar)
                    {
                        if (bRollover)
                            rRenderContext.SetTextColor(rSettings.GetMenuBarRolloverTextColor());
                        else if (bHighlighted)
                            rRenderContext.SetTextColor(rSettings.GetMenuBarHighlightTextColor());
                    }
                    else
                    {
                        if (bHighlighted)
                            rRenderContext.SetTextColor(rSettings.GetMenuBarHighlightTextColor());
                        else if (bRollover)
                            rRenderContext.SetTextColor(rSettings.GetMenuBarRolloverTextColor());
                    }
                    if (!bRollover && !bHighlighted)
                        rRenderContext.SetTextColor(rSettings.GetMenuBarTextColor());
                }
                else if (bHighlighted)
                    rRenderContext.SetTextColor(rSettings.GetMenuHighlightTextColor());
            }

            Point aPos(aTopLeft);
            aPos.AdjustY(nBorder );
            aPos.AdjustY(nStartY );

            if (aPos.Y() >= 0)
            {
                tools::Long nTextOffsetY = (pData->aSz.Height() - nFontHeight) / 2;
                if (IsMenuBar())
                    nTextOffsetY += (aOutSz.Height()-pData->aSz.Height()) / 2;
                DrawTextFlags   nTextStyle   = DrawTextFlags::NONE;
                DrawSymbolFlags nSymbolStyle = DrawSymbolFlags::NONE;
                DrawImageFlags  nImageStyle  = DrawImageFlags::NONE;

                // submenus without items are not disabled when no items are
                // contained. The application itself should check for this!
                // Otherwise it could happen entries are disabled due to
                // asynchronous loading
                if (!pData->bEnabled || !m_pWindow->IsEnabled())
                {
                    nTextStyle   |= DrawTextFlags::Disable;
                    nSymbolStyle |= DrawSymbolFlags::Disable;
                    nImageStyle  |= DrawImageFlags::Disable;
                }

                // Separator
                if (!bLayout && !IsMenuBar() && (pData->eType == MenuItemType::SEPARATOR))
                {
                    bool bNativeOk = false;
                    if (rRenderContext.IsNativeControlSupported(ControlType::MenuPopup, ControlPart::Separator))
                    {
                        ControlState nState = ControlState::NONE;
                        if (pData->bEnabled && m_pWindow->IsEnabled())
                            nState |= ControlState::ENABLED;
                        if (bHighlighted)
                            nState |= ControlState::SELECTED;
                        Size aSz(pData->aSz);
                        aSz.setWidth( aOutSz.Width() - 2*nOuterSpaceX );
                        tools::Rectangle aItemRect(aPos, aSz);
                        MenupopupValue aVal(nTextPos - GUTTERBORDER, aItemRect);
                        bNativeOk = rRenderContext.DrawNativeControl(ControlType::MenuPopup, ControlPart::Separator,
                                                                     aItemRect, nState, aVal, OUString());
                    }
                    if (!bNativeOk)
                    {
                        aTmpPos.setY( aPos.Y() + ((pData->aSz.Height() - 2) / 2) );
                        aTmpPos.setX( aPos.X() + 2 + nOuterSpaceX );
                        rRenderContext.SetLineColor(rSettings.GetShadowColor());
                        rRenderContext.DrawLine(aTmpPos, Point(aOutSz.Width() - 3 - 2 * nOuterSpaceX, aTmpPos.Y()));
                        aTmpPos.AdjustY( 1 );
                        rRenderContext.SetLineColor(rSettings.GetLightColor());
                        rRenderContext.DrawLine(aTmpPos, Point(aOutSz.Width() - 3 - 2 * nOuterSpaceX, aTmpPos.Y()));
                        rRenderContext.SetLineColor();
                    }
                }

                tools::Rectangle aOuterCheckRect(Point(aPos.X()+nImgOrChkPos, aPos.Y()),
                                          Size(pData->aSz.Height(), pData->aSz.Height()));

                // CheckMark
                if (!bLayout && !IsMenuBar() && pData->HasCheck())
                {
                    // draw selection transparent marker if checked
                    // onto that either a checkmark or the item image
                    // will be painted
                    // however do not do this if native checks will be painted since
                    // the selection color too often does not fit the theme's check and/or radio

                    if( (pData->eType != MenuItemType::IMAGE) && (pData->eType != MenuItemType::STRINGIMAGE))
                    {
                        if (rRenderContext.IsNativeControlSupported(ControlType::MenuPopup,
                                                                    (pData->nBits & MenuItemBits::RADIOCHECK)
                                                                        ? ControlPart::MenuItemCheckMark
                                                                        : ControlPart::MenuItemRadioMark))
                        {
                            ControlPart nPart = ((pData->nBits & MenuItemBits::RADIOCHECK)
                                                 ? ControlPart::MenuItemRadioMark
                                                 : ControlPart::MenuItemCheckMark);

                            ControlState nState = ControlState::NONE;

                            if (pData->bChecked)
                                nState |= ControlState::PRESSED;

                            if (pData->bEnabled && m_pWindow->IsEnabled())
                                nState |= ControlState::ENABLED;

                            if (bHighlighted)
                                nState |= ControlState::SELECTED;

                            tools::Long nCtrlHeight = (pData->nBits & MenuItemBits::RADIOCHECK) ? nCheckHeight : nRadioHeight;
                            aTmpPos.setX( aOuterCheckRect.Left() + (aOuterCheckRect.GetWidth() - nCtrlHeight) / 2 );
                            aTmpPos.setY( aOuterCheckRect.Top() + (aOuterCheckRect.GetHeight() - nCtrlHeight) / 2 );

                            tools::Rectangle aCheckRect(aTmpPos, Size(nCtrlHeight, nCtrlHeight));
                            Size aSz(pData->aSz);
                            aSz.setWidth( aOutSz.Width() - 2 * nOuterSpaceX );
                            tools::Rectangle aItemRect(aPos, aSz);
                            MenupopupValue aVal(nTextPos - GUTTERBORDER, aItemRect);
                            rRenderContext.DrawNativeControl(ControlType::MenuPopup, nPart, aCheckRect,
                                                             nState, aVal, OUString());
                        }
                        else if (pData->bChecked) // by default do nothing for unchecked items
                        {
                            ImplPaintCheckBackground(rRenderContext, *m_pWindow, aOuterCheckRect, pThisItemOnly && bHighlighted);

                            SymbolType eSymbol;
                            Size aSymbolSize;
                            if (pData->nBits & MenuItemBits::RADIOCHECK)
                            {
                                eSymbol = SymbolType::RADIOCHECKMARK;
                                aSymbolSize = Size(nFontHeight / 2, nFontHeight / 2);
                            }
                            else
                            {
                                eSymbol = SymbolType::CHECKMARK;
                                aSymbolSize = Size((nFontHeight * 25) / 40, nFontHeight / 2);
                            }
                            aTmpPos.setX( aOuterCheckRect.Left() + (aOuterCheckRect.GetWidth() - aSymbolSize.Width()) / 2 );
                            aTmpPos.setY( aOuterCheckRect.Top() + (aOuterCheckRect.GetHeight() - aSymbolSize.Height()) / 2 );
                            tools::Rectangle aRect(aTmpPos, aSymbolSize);
                            aDecoView.DrawSymbol(aRect, eSymbol, rRenderContext.GetTextColor(), nSymbolStyle);
                        }
                    }
                }

                // Image:
                if (!bLayout && !IsMenuBar() && ((pData->eType == MenuItemType::IMAGE) || (pData->eType == MenuItemType::STRINGIMAGE)))
                {
                    // Don't render an image for a check thing
                    if (pData->bChecked)
                        ImplPaintCheckBackground(rRenderContext, *m_pWindow, aOuterCheckRect, pThisItemOnly && bHighlighted);

                    Image aImage = pData->aImage;

                    aTmpPos = aOuterCheckRect.TopLeft();
                    aTmpPos.AdjustX((aOuterCheckRect.GetWidth() - aImage.GetSizePixel().Width()) / 2 );
                    aTmpPos.AdjustY((aOuterCheckRect.GetHeight() - aImage.GetSizePixel().Height()) / 2 );
                    rRenderContext.DrawImage(aTmpPos, aImage, nImageStyle);
                }

                // Text:
                if ((pData->eType == MenuItemType::STRING ) || (pData->eType == MenuItemType::STRINGIMAGE))
                {
                    aTmpPos.setX( aPos.X() + nTextPos );
                    aTmpPos.setY( aPos.Y() );
                    aTmpPos.AdjustY(nTextOffsetY );
                    DrawTextFlags nStyle = nTextStyle;

                    const Menu *pMenu = this;
                    while (!pMenu->IsMenuBar() && pMenu->pStartedFrom)
                        pMenu = pMenu->pStartedFrom;
                    if (!pMenu->IsMenuBar() || !static_cast<MenuBarWindow*>(pMenu->m_pWindow.get())->GetMBWHideAccel())
                        nStyle |= DrawTextFlags::Mnemonic;

                    if (pData->bIsTemporary)
                        nStyle |= DrawTextFlags::Disable;
                    std::vector< tools::Rectangle >* pVector = bLayout ? &mpLayoutData->m_aUnicodeBoundRects : nullptr;
                    OUString* pDisplayText = bLayout ? &mpLayoutData->m_aDisplayText : nullptr;
                    if (bLayout)
                    {
                        mpLayoutData->m_aLineIndices.push_back(mpLayoutData->m_aDisplayText.getLength());
                        mpLayoutData->m_aLineItemIds.push_back(pData->nId);
                    }
                    // #i47946# with NWF painted menus the background is transparent
                    // since DrawCtrlText can depend on the background (e.g. for
                    // DrawTextFlags::Disable), temporarily set a background which
                    // hopefully matches the NWF background since it is read
                    // from the system style settings
                    bool bSetTmpBackground = !rRenderContext.IsBackground()
                                           && rRenderContext.IsNativeControlSupported(ControlType::MenuPopup, ControlPart::Entire);
                    if (bSetTmpBackground)
                    {
                        Color aBg = IsMenuBar() ? rRenderContext.GetSettings().GetStyleSettings().GetMenuBarColor()
                                                : rRenderContext.GetSettings().GetStyleSettings().GetMenuColor();
                        rRenderContext.SetBackground(Wallpaper(aBg));
                    }
                    // how much space is there for the text?
                    tools::Long nMaxItemTextWidth = aOutSz.Width() - aTmpPos.X() - nExtra - nOuterSpaceX;
                    if (!IsMenuBar() && pData->aAccelKey.GetCode() && !officecfg::VCL::VCLSettings::Menu::SuppressAccelerators::get())
                    {
                        OUString aAccText = pData->aAccelKey.GetName();
                        nMaxItemTextWidth -= rRenderContext.GetTextWidth(aAccText) + 3 * nExtra;
                    }
                    if (!IsMenuBar() && pData->pSubMenu)
                    {
                        nMaxItemTextWidth -= nFontHeight - nExtra;
                    }

                    OUString aItemText(pData->aText);
                    pData->bHiddenOnGUI = false;

                    if (IsMenuBar()) // In case of menubar if we are out of bounds we shouldn't paint the item
                    {
                        if (nMaxItemTextWidth < rRenderContext.GetTextWidth(aItemText))
                        {
                            aItemText = "";
                            pData->bHiddenOnGUI = true;
                            bHiddenItems = true;
                        }
                    }
                    else
                    {
                        aItemText = getShortenedString(aItemText, rRenderContext, nMaxItemTextWidth);
                        pData->bHiddenOnGUI = false;
                    }

                    const SalLayoutGlyphs* pGlyphs = pData->GetTextGlyphs(&rRenderContext);
                    if (aItemText != pData->aText)
                        // Can't use pre-computed glyphs, item text was
                        // changed.
                        pGlyphs = nullptr;
                    rRenderContext.DrawCtrlText(aTmpPos, aItemText, 0, aItemText.getLength(),
                                                nStyle, pVector, pDisplayText, pGlyphs);
                    if (bSetTmpBackground)
                        rRenderContext.SetBackground();
                }

                // Accel
                if (!bLayout && !IsMenuBar() && pData->aAccelKey.GetCode() && !officecfg::VCL::VCLSettings::Menu::SuppressAccelerators::get())
                {
                    OUString aAccText = pData->aAccelKey.GetName();
                    aTmpPos.setX( aOutSz.Width() - rRenderContext.GetTextWidth(aAccText) );
                    aTmpPos.AdjustX( -(4 * nExtra) );

                    aTmpPos.AdjustX( -nOuterSpaceX );
                    aTmpPos.setY( aPos.Y() );
                    aTmpPos.AdjustY(nTextOffsetY );
                    rRenderContext.DrawCtrlText(aTmpPos, aAccText, 0, aAccText.getLength(), nTextStyle);
                }

                // SubMenu?
                if (!bLayout && !IsMenuBar() && pData->pSubMenu)
                {
                    bool bNativeOk = false;
                    if (rRenderContext.IsNativeControlSupported(ControlType::MenuPopup, ControlPart::SubmenuArrow))
                    {
                        ControlState nState = ControlState::NONE;
                        Size aTmpSz(0, 0);
                        tools::Long aSpacing = 0;

                        if (!ImplGetNativeSubmenuArrowSize(rRenderContext, aTmpSz, aSpacing))
                        {
                            aTmpSz = Size(nFontHeight, nFontHeight);
                            aSpacing = nOuterSpaceX;
                        }

                        if (pData->bEnabled && m_pWindow->IsEnabled())
                            nState |= ControlState::ENABLED;
                        if (bHighlighted)
                            nState |= ControlState::SELECTED;

                        aTmpPos.setX( aOutSz.Width() - aTmpSz.Width() - aSpacing - nOuterSpaceX );
                        aTmpPos.setY( aPos.Y() + ( pData->aSz.Height() - aTmpSz.Height() ) / 2 );
                        aTmpPos.AdjustY(nExtra / 2 );

                        tools::Rectangle aItemRect(aTmpPos, aTmpSz);
                        MenupopupValue aVal(nTextPos - GUTTERBORDER, aItemRect);
                        bNativeOk = rRenderContext.DrawNativeControl(ControlType::MenuPopup, ControlPart::SubmenuArrow,
                                                                     aItemRect, nState, aVal, OUString());
                    }
                    if (!bNativeOk)
                    {
                        aTmpPos.setX( aOutSz.Width() - nFontHeight + nExtra - nOuterSpaceX );
                        aTmpPos.setY( aPos.Y() );
                        aTmpPos.AdjustY(nExtra/2 );
                        aTmpPos.AdjustY((pData->aSz.Height() / 2) - (nFontHeight / 4) );
                        if (pData->nBits & MenuItemBits::POPUPSELECT)
                        {
                            rRenderContext.SetTextColor(rSettings.GetMenuTextColor());
                            Point aTmpPos2(aPos);
                            aTmpPos2.setX( aOutSz.Width() - nFontHeight - nFontHeight/4 );
                            aDecoView.DrawFrame(tools::Rectangle(aTmpPos2, Size(nFontHeight + nFontHeight / 4,
                                                                         pData->aSz.Height())),
                                                DrawFrameStyle::Group);
                        }
                        aDecoView.DrawSymbol(tools::Rectangle(aTmpPos, Size(nFontHeight / 2, nFontHeight / 2)),
                                             SymbolType::SPIN_RIGHT, rRenderContext.GetTextColor(), nSymbolStyle);
                    }
                }

                if (pThisItemOnly && bHighlighted)
                {
                    // This restores the normal menu or menu bar text
                    // color for when it is no longer highlighted.
                    if (IsMenuBar())
                        rRenderContext.SetTextColor(rSettings.GetMenuBarTextColor());
                    else
                        rRenderContext.SetTextColor(rSettings.GetMenuTextColor());
                }
            }
            if( bLayout )
            {
                if (!IsMenuBar())
                    mpLayoutData->m_aVisibleItemBoundRects[ n ] = tools::Rectangle(aTopLeft, Size(aOutSz.Width(), pData->aSz.Height()));
                else
                    mpLayoutData->m_aVisibleItemBoundRects[ n ] = tools::Rectangle(aTopLeft, pData->aSz);
            }
        }

        if (!IsMenuBar())
            aTopLeft.AdjustY(pData->aSz.Height() );
        else
            aTopLeft.AdjustX(pData->aSz.Width() );
    }

    // draw "more" (">>") indicator if some items have been hidden as they go out of visible area
    if (bHiddenItems)
    {
        sal_Int32 nSize = nFontHeight;
        tools::Rectangle aRectangle(Point(aOutSz.Width() - nSize, (aOutSz.Height() / 2) - (nSize / 2)), Size(nSize, nSize));
        lclDrawMoreIndicator(rRenderContext, aRectangle);
    }
}

Menu* Menu::ImplGetStartMenu()
{
    Menu* pStart = this;
    while ( pStart && pStart->pStartedFrom && ( pStart->pStartedFrom != pStart ) )
        pStart = pStart->pStartedFrom;
    return pStart;
}

void Menu::ImplCallHighlight(sal_uInt16 nItem)
{
    ImplMenuDelData aDelData( this );

    nSelectedId = 0;
    sSelectedIdent.clear();
    MenuItemData* pData = pItemList->GetDataFromPos(nItem);
    if (pData)
    {
        nSelectedId = pData->nId;
        sSelectedIdent = pData->sIdent;
    }
    ImplCallEventListeners( VclEventId::MenuHighlight, GetItemPos( GetCurItemId() ) );

    if( !aDelData.isDeleted() )
    {
        nSelectedId = 0;
        sSelectedIdent.clear();
    }
}

IMPL_LINK_NOARG(Menu, ImplCallSelect, void*, void)
{
    nEventId = nullptr;
    Select();
}

Menu* Menu::ImplFindSelectMenu()
{
    Menu* pSelMenu = nEventId ? this : nullptr;

    for ( size_t n = GetItemList()->size(); n && !pSelMenu; )
    {
        MenuItemData* pData = GetItemList()->GetDataFromPos( --n );

        if ( pData->pSubMenu )
            pSelMenu = pData->pSubMenu->ImplFindSelectMenu();
    }

    return pSelMenu;
}

Menu* Menu::ImplFindMenu( sal_uInt16 nItemId )
{
    Menu* pSelMenu = nullptr;

    for ( size_t n = GetItemList()->size(); n && !pSelMenu; )
    {
        MenuItemData* pData = GetItemList()->GetDataFromPos( --n );

        if( pData->nId == nItemId )
            pSelMenu = this;
        else if ( pData->pSubMenu )
            pSelMenu = pData->pSubMenu->ImplFindMenu( nItemId );
    }

    return pSelMenu;
}

void Menu::RemoveDisabledEntries( bool bRemoveEmptyPopups )
{
    sal_uInt16 n = 0;
    while (n < GetItemCount())
    {
        bool bRemove = false;
        MenuItemData* pItem = pItemList->GetDataFromPos( n );
        if ( pItem->eType == MenuItemType::SEPARATOR )
        {
            if ( !n || ( GetItemType( n-1 ) == MenuItemType::SEPARATOR ) )
                bRemove = true;
        }
        else
            bRemove = !pItem->bEnabled;

        if ( pItem->pSubMenu )
        {
            pItem->pSubMenu->RemoveDisabledEntries();
            if ( bRemoveEmptyPopups && !pItem->pSubMenu->GetItemCount() )
                bRemove = true;
        }

        if (bRemove)
            RemoveItem(n);
        else
            ++n;
    }

    if ( GetItemCount() )
    {
        sal_uInt16 nLast = GetItemCount() - 1;
        MenuItemData* pItem = pItemList->GetDataFromPos( nLast );
        if ( pItem->eType == MenuItemType::SEPARATOR )
            RemoveItem( nLast );
    }
    mpLayoutData.reset();
}

void Menu::UpdateNativeMenu()
{
    if ( ImplGetSalMenu() )
        ImplGetSalMenu()->Update();
}

void Menu::MenuBarKeyInput(const KeyEvent&)
{
}

void Menu::ImplKillLayoutData() const
{
    mpLayoutData.reset();
}

void Menu::ImplFillLayoutData() const
{
    if (!(m_pWindow && m_pWindow->IsReallyVisible()))
        return;

    mpLayoutData.reset(new MenuLayoutData);
    if (IsMenuBar())
    {
        ImplPaint(*m_pWindow->GetOutDev(), m_pWindow->GetOutputSizePixel(), 0, 0, nullptr, false, true); // FIXME
    }
    else
    {
        MenuFloatingWindow* pFloat = static_cast<MenuFloatingWindow*>(m_pWindow.get());
        ImplPaint(*m_pWindow->GetOutDev(), m_pWindow->GetOutputSizePixel(), pFloat->nScrollerHeight, pFloat->ImplGetStartY(),
                  nullptr, false, true); //FIXME
    }
}

tools::Rectangle Menu::GetCharacterBounds( sal_uInt16 nItemID, tools::Long nIndex ) const
{
    tools::Long nItemIndex = -1;
    if( ! mpLayoutData )
        ImplFillLayoutData();
    if( mpLayoutData )
    {
        auto it = std::find(mpLayoutData->m_aLineItemIds.begin(), mpLayoutData->m_aLineItemIds.end(), nItemID);
        if (it != mpLayoutData->m_aLineItemIds.end())
        {
            nItemIndex = mpLayoutData->m_aLineIndices[std::distance(mpLayoutData->m_aLineItemIds.begin(), it)];
        }
    }
    return (mpLayoutData && nItemIndex != -1) ? mpLayoutData->GetCharacterBounds( nItemIndex+nIndex ) : tools::Rectangle();
}

tools::Long Menu::GetIndexForPoint( const Point& rPoint, sal_uInt16& rItemID ) const
{
    rItemID = 0;
    if( ! mpLayoutData )
        ImplFillLayoutData();
    if( mpLayoutData )
    {
        // coverity[ tainted_data_return : FALSE ] version 2023.12.2
        tools::Long nIndex = mpLayoutData->GetIndexForPoint( rPoint );
        for( size_t i = 0; i < mpLayoutData->m_aLineIndices.size(); i++ )
        {
            if( mpLayoutData->m_aLineIndices[i] <= nIndex &&
                (i == mpLayoutData->m_aLineIndices.size()-1 || mpLayoutData->m_aLineIndices[i+1] > nIndex) )
            {
                rItemID = mpLayoutData->m_aLineItemIds[i];
                // return index relative to item
                return nIndex - mpLayoutData->m_aLineIndices[i];
            }
        }
    }
    return -1;
}

tools::Rectangle Menu::GetBoundingRectangle( sal_uInt16 nPos ) const
{
    tools::Rectangle aRet;

    if (!mpLayoutData )
        ImplFillLayoutData();
    if (mpLayoutData)
    {
        std::map< sal_uInt16, tools::Rectangle >::const_iterator it = mpLayoutData->m_aVisibleItemBoundRects.find( nPos );
        if( it != mpLayoutData->m_aVisibleItemBoundRects.end() )
            aRet = it->second;
    }
    return aRet;
}

void Menu::SetAccessibleName( sal_uInt16 nItemId, const OUString& rStr )
{
    size_t nPos;
    MenuItemData* pData = pItemList->GetData( nItemId, nPos );

    if (pData && !rStr.equals(pData->aAccessibleName))
    {
        pData->aAccessibleName = rStr;
        ImplCallEventListeners(VclEventId::MenuAccessibleNameChanged, nPos);
    }
}

OUString Menu::GetAccessibleName( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        return pData->aAccessibleName;

    return OUString();
}

void Menu::SetAccessibleDescription( sal_uInt16 nItemId, const OUString& rStr )
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        pData->aAccessibleDescription = rStr;
}

OUString Menu::GetAccessibleDescription( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if (pData && !pData->aAccessibleDescription.isEmpty())
        return pData->aAccessibleDescription;

    return GetHelpText(nItemId);
}

void Menu::GetSystemMenuData(SystemMenuData& rData) const
{
    if (ImplGetSalMenu())
    {
        ImplGetSalMenu()->GetSystemMenuData(rData);
    }
}

bool Menu::IsHighlighted( sal_uInt16 nItemPos ) const
{
    bool bRet = false;

    if (m_pWindow)
    {
        if (IsMenuBar())
            bRet = ( nItemPos == static_cast< MenuBarWindow * > (m_pWindow.get())->GetHighlightedItem() );
        else
            bRet = ( nItemPos == static_cast< MenuFloatingWindow * > (m_pWindow.get())->GetHighlightedItem() );
    }

    return bRet;
}

void Menu::HighlightItem( sal_uInt16 nItemPos )
{
    if ( !m_pWindow )
        return;

    if (IsMenuBar())
    {
        MenuBarWindow* pMenuWin = static_cast< MenuBarWindow* >( m_pWindow.get() );
        pMenuWin->SetAutoPopup( false );
        pMenuWin->ChangeHighlightItem( nItemPos, false );
    }
    else
    {
        static_cast< MenuFloatingWindow* >( m_pWindow.get() )->ChangeHighlightItem( nItemPos, false );
    }
}

void Menu::DumpAsPropertyTree(tools::JsonWriter& rJsonWriter) const
{
    rJsonWriter.put("id", "__MENU__"); // we have single instance of menu at the time per session
    rJsonWriter.put("type", "menu");
    rJsonWriter.put("count", GetItemCount());
    {
        auto aEntries = rJsonWriter.startArray("entries");
        for (size_t i = 0; i < GetItemCount(); i++)
        {
            auto aEntry = rJsonWriter.startStruct();
            sal_uInt16 nId = GetItemId(i);
            rJsonWriter.put("row", GetItemIdent(nId));
            {
                auto aColumns = rJsonWriter.startArray("columns");
                auto aColumn = rJsonWriter.startStruct();
                rJsonWriter.put("text", GetItemText(nId));
            }
        }
    }
}

MenuBarWindow* MenuBar::getMenuBarWindow()
{
    // so far just a dynamic_cast, hopefully to be turned into something saner
    // at some stage
    MenuBarWindow *pWin = dynamic_cast<MenuBarWindow*>(m_pWindow.get());
    //either there is no window (fdo#87663) or it is a MenuBarWindow
    assert(!m_pWindow || pWin);
    return pWin;
}

MenuBar::MenuBar()
    : mbCloseBtnVisible(false),
      mbFloatBtnVisible(false),
      mbHideBtnVisible(false),
      mbDisplayable(true)
{
    mpSalMenu = ImplGetSVData()->mpDefInst->CreateMenu(true, this);
}

MenuBar::MenuBar( const MenuBar& rMenu )
    : mbCloseBtnVisible(false),
      mbFloatBtnVisible(false),
      mbHideBtnVisible(false),
      mbDisplayable(true)
{
    mpSalMenu = ImplGetSVData()->mpDefInst->CreateMenu(true, this);
    *this = rMenu;
}

MenuBar::~MenuBar()
{
    disposeOnce();
}

void MenuBar::dispose()
{
    ImplDestroy(true);
    Menu::dispose();
}

void MenuBar::ClosePopup(PopupMenu *pPopupMenu)
{
    MenuBarWindow* pMenuWin = getMenuBarWindow();
    if (!pMenuWin)
        return;
    pMenuWin->PopupClosed(pPopupMenu);
}

void MenuBar::MenuBarKeyInput(const KeyEvent& rEvent)
{
    m_pWindow->KeyInput(rEvent);
}

void MenuBar::ShowCloseButton(bool bShow)
{
    ShowButtons( bShow, mbFloatBtnVisible, mbHideBtnVisible );
}

void MenuBar::ShowButtons( bool bClose, bool bFloat, bool bHide )
{
    if ((bClose != mbCloseBtnVisible) ||
        (bFloat != mbFloatBtnVisible) ||
        (bHide  != mbHideBtnVisible))
    {
        mbCloseBtnVisible = bClose;
        mbFloatBtnVisible = bFloat;
        mbHideBtnVisible = bHide;
        MenuBarWindow* pMenuWin = getMenuBarWindow();
        if (pMenuWin)
            pMenuWin->ShowButtons(bClose, bFloat, bHide);
    }
}

void MenuBar::LayoutChanged()
{
    MenuBarWindow* pMenuWin = getMenuBarWindow();
    if (pMenuWin)
        pMenuWin->LayoutChanged();
}

void MenuBar::SetDisplayable( bool bDisplayable )
{
    if( bDisplayable != mbDisplayable )
    {
        if ( ImplGetSalMenu() )
            ImplGetSalMenu()->ShowMenuBar( bDisplayable );

        mbDisplayable = bDisplayable;
        LayoutChanged();
    }
}

VclPtr<MenuBarWindow> MenuBar::ImplCreate(vcl::Window* pParent, MenuBarWindow* pWindow)
{
    VclPtr<MenuBarWindow> pMenuBarWindow = pWindow;
    if (!pMenuBarWindow)
    {
        pMenuBarWindow = VclPtr<MenuBarWindow>::Create(pParent);
    }

    pStartedFrom = nullptr;
    m_pWindow = pMenuBarWindow;
    pMenuBarWindow->SetMenu(this);
    tools::Long nHeight = ImplCalcSize(pMenuBarWindow).Height();

    // depending on the native implementation or the displayable flag
    // the menubar windows is suppressed (ie, height=0)
    if (!IsDisplayable() || (ImplGetSalMenu() && ImplGetSalMenu()->VisibleMenuBar()))
    {
        nHeight = 0;
    }

    pMenuBarWindow->SetHeight(nHeight);
    return pMenuBarWindow;
}

void MenuBar::ImplDestroy(bool bDelete)
{
    MenuBarWindow* pMenuWin = getMenuBarWindow();
    if (pMenuWin && bDelete)
    {
        pMenuWin->KillActivePopup();
        pMenuWin->disposeOnce();
    }
    m_pWindow = nullptr;
    if (mpSalMenu)
    {
        mpSalMenu->ShowMenuBar(false);
    }
}

bool MenuBar::ImplHandleKeyEvent( const KeyEvent& rKEvent )
{
    // No keyboard processing when our menubar is invisible
    if (!IsDisplayable())
        return false;

    // No keyboard processing when system handles the menu.
    SalMenu *pNativeMenu = ImplGetSalMenu();
    if (pNativeMenu && pNativeMenu->VisibleMenuBar())
    {
        // Except when the event is the F6 cycle pane event and we can put our
        // focus into it (i.e. the gtk3 menubar case but not the mac/unity case
        // where it's not part of the application window)
        if (!TaskPaneList::IsCycleKey(rKEvent.GetKeyCode()))
            return false;
        if (!pNativeMenu->CanGetFocus())
            return false;
    }

    bool bDone = false;
    // check for enabled, if this method is called from another window...
    vcl::Window* pWin = GetWindow();
    if (pWin && pWin->IsEnabled() && pWin->IsInputEnabled()  && !pWin->IsInModalMode())
    {
        MenuBarWindow* pMenuWin = getMenuBarWindow();
        bDone = pMenuWin && pMenuWin->HandleKeyEvent(rKEvent, false/*bFromMenu*/);
    }
    return bDone;
}

bool MenuBar::ImplHandleCmdEvent( const CommandEvent& rCEvent )
{
    // No keyboard processing when system handles the menu or our menubar is invisible
    if( !IsDisplayable() ||
        ( ImplGetSalMenu() && ImplGetSalMenu()->VisibleMenuBar() ) )
        return false;

    // check for enabled, if this method is called from another window...
    MenuBarWindow* pWin = static_cast<MenuBarWindow*>(GetWindow());
    if ( pWin && pWin->IsEnabled() && pWin->IsInputEnabled()  && ! pWin->IsInModalMode() )
    {
        if (rCEvent.GetCommand() == CommandEventId::ModKeyChange && ImplGetSVData()->maNWFData.mbAutoAccel)
        {
            const CommandModKeyData* pCData = rCEvent.GetModKeyData ();
            if (pWin->m_nHighlightedItem == ITEMPOS_INVALID)
            {
                if (pCData && pCData->IsMod2() && pCData->IsDown())
                    pWin->SetMBWHideAccel(false);
                pWin->Invalidate(InvalidateFlags::Update);
            }
            return true;
        }
    }
    return false;
}

void MenuBar::SelectItem(sal_uInt16 nId)
{
    if (!m_pWindow)
        return;

    m_pWindow->GrabFocus();
    nId = GetItemPos( nId );

    MenuBarWindow* pMenuWin = getMenuBarWindow();
    if (pMenuWin)
    {
        // #99705# popup the selected menu
        pMenuWin->SetAutoPopup( true );
        if (ITEMPOS_INVALID != pMenuWin->GetHighlightedItem())
        {
            pMenuWin->KillActivePopup();
            pMenuWin->ChangeHighlightItem( ITEMPOS_INVALID, false );
        }
        if (nId != ITEMPOS_INVALID)
            pMenuWin->ChangeHighlightItem( nId, false );
    }
}

// handler for native menu selection and command events
bool Menu::HandleMenuActivateEvent(Menu* pMenu)
{
    if( pMenu )
    {
        ImplMenuDelData aDelData( this );

        pMenu->pStartedFrom = this;
        pMenu->bInCallback = true;
        pMenu->Activate();

        if( !aDelData.isDeleted() )
            pMenu->bInCallback = false;
    }
    return true;
}

bool Menu::HandleMenuDeActivateEvent(Menu* pMenu)
{
    if( pMenu )
    {
        ImplMenuDelData aDelData( this );

        pMenu->pStartedFrom = this;
        pMenu->bInCallback = true;
        pMenu->Deactivate();
        if( !aDelData.isDeleted() )
            pMenu->bInCallback = false;
    }
    return true;
}

bool MenuBar::HandleMenuHighlightEvent(Menu* pMenu, sal_uInt16 nHighlightEventId)
{
    if( !pMenu )
        pMenu = ImplFindMenu(nHighlightEventId);
    if( pMenu )
    {
        ImplMenuDelData aDelData( pMenu );

        if( mnHighlightedItemPos != ITEMPOS_INVALID )
            pMenu->ImplCallEventListeners( VclEventId::MenuDehighlight, mnHighlightedItemPos );

        if( !aDelData.isDeleted() )
        {
            pMenu->mnHighlightedItemPos = pMenu->GetItemPos( nHighlightEventId );
            pMenu->nSelectedId = nHighlightEventId;
            pMenu->sSelectedIdent = pMenu->GetItemIdent( nHighlightEventId );
            pMenu->pStartedFrom = this;
            pMenu->ImplCallHighlight( pMenu->mnHighlightedItemPos );
        }
        return true;
    }
    else
        return false;
}

bool Menu::HandleMenuCommandEvent(Menu* pMenu, sal_uInt16 nCommandEventId)
{
    if( !pMenu )
        pMenu = ImplFindMenu(nCommandEventId);
    if( pMenu )
    {
        pMenu->nSelectedId = nCommandEventId;
        pMenu->sSelectedIdent = pMenu->GetItemIdent(nCommandEventId);
        pMenu->pStartedFrom = this;
        pMenu->ImplSelect();
        return true;
    }
    else
        return false;
}

sal_uInt16 MenuBar::AddMenuBarButton( const Image& i_rImage, const Link<MenuBarButtonCallbackArg&,bool>& i_rLink, const OUString& i_rToolTip )
{
    MenuBarWindow* pMenuWin = getMenuBarWindow();
    return pMenuWin ? pMenuWin->AddMenuBarButton(i_rImage, i_rLink, i_rToolTip) : 0;
}

void MenuBar::SetMenuBarButtonHighlightHdl( sal_uInt16 nId, const Link<MenuBarButtonCallbackArg&,bool>& rLink )
{
    MenuBarWindow* pMenuWin = getMenuBarWindow();
    if (!pMenuWin)
        return;
    pMenuWin->SetMenuBarButtonHighlightHdl(nId, rLink);
}

void MenuBar::RemoveMenuBarButton( sal_uInt16 nId )
{
    MenuBarWindow* pMenuWin = getMenuBarWindow();
    if (!pMenuWin)
        return;
    pMenuWin->RemoveMenuBarButton(nId);
}

tools::Rectangle MenuBar::GetMenuBarButtonRectPixel( sal_uInt16 nId )
{
    MenuBarWindow* pMenuWin = getMenuBarWindow();
    return pMenuWin ? pMenuWin->GetMenuBarButtonRectPixel(nId) : tools::Rectangle();
}

bool MenuBar::HandleMenuButtonEvent( sal_uInt16 i_nButtonId )
{
    MenuBarWindow* pMenuWin = getMenuBarWindow();
    return pMenuWin && pMenuWin->HandleMenuButtonEvent(i_nButtonId);
}

int MenuBar::GetMenuBarHeight() const
{
    const SalMenu *pNativeMenu = ImplGetSalMenu();
    int nMenubarHeight;
    if (pNativeMenu)
        nMenubarHeight = pNativeMenu->GetMenuBarHeight();
    else
    {
        vcl::Window* pMenubarWin = GetWindow();
        nMenubarHeight = pMenubarWin ? pMenubarWin->GetOutputSizePixel().Height() : 0;
    }
    return nMenubarHeight;
}

MenuFloatingWindow * PopupMenu::ImplGetFloatingWindow() const {
    return static_cast<MenuFloatingWindow*>(GetWindow());
}

PopupMenu::PopupMenu()
{
    mpSalMenu = ImplGetSVData()->mpDefInst->CreateMenu(false, this);
}

PopupMenu::PopupMenu( const PopupMenu& rMenu )
{
    mpSalMenu = ImplGetSVData()->mpDefInst->CreateMenu(false, this);
    *this = rMenu;
}

PopupMenu::~PopupMenu()
{
    disposeOnce();
}

void PopupMenu::ClosePopup(PopupMenu* pPopupMenu)
{
    MenuFloatingWindow* p = ImplGetFloatingWindow();
    if (p && pPopupMenu && p->GetActivePopup() == pPopupMenu)
        p->KillActivePopup();
}

namespace vcl
{
    bool IsInPopupMenuExecute()
    {
        return PopupMenu::GetActivePopupMenu() != nullptr;
    }
}

PopupMenu* PopupMenu::GetActivePopupMenu()
{
    ImplSVData* pSVData = ImplGetSVData();
    return pSVData->maAppData.mpActivePopupMenu;
}

void PopupMenu::EndExecute()
{
    if (GetWindow())
        ImplGetFloatingWindow()->EndExecute( 0 );
}

void PopupMenu::SelectItem(sal_uInt16 nId)
{
    if (!GetWindow())
        return;

    if( nId != ITEMPOS_INVALID )
    {
        size_t nPos = 0;
        MenuItemData* pData = GetItemList()->GetData( nId, nPos );
        if (pData && pData->pSubMenu)
            ImplGetFloatingWindow()->ChangeHighlightItem( nPos, true );
        else
            ImplGetFloatingWindow()->EndExecute( nId );
    }
    else
    {
        MenuFloatingWindow* pFloat = ImplGetFloatingWindow();
        pFloat->GrabFocus();

        for( size_t nPos = 0; nPos < GetItemList()->size(); nPos++ )
        {
            MenuItemData* pData = GetItemList()->GetDataFromPos( nPos );
            if( pData->pSubMenu )
            {
                pFloat->KillActivePopup();
            }
        }
        pFloat->ChangeHighlightItem( ITEMPOS_INVALID, false );
    }
}

void PopupMenu::SetSelectedEntry( sal_uInt16 nId )
{
    nSelectedId = nId;
    sSelectedIdent = GetItemIdent(nId);
}

sal_uInt16 PopupMenu::Execute( vcl::Window* pExecWindow, const Point& rPopupPos )
{
    return Execute( pExecWindow, tools::Rectangle( rPopupPos, rPopupPos ), PopupMenuFlags::ExecuteDown );
}

static FloatWinPopupFlags lcl_TranslateFlags(PopupMenuFlags nFlags)
{
    FloatWinPopupFlags nPopupModeFlags = FloatWinPopupFlags::NONE;
    if ( nFlags & PopupMenuFlags::ExecuteDown )
        nPopupModeFlags = FloatWinPopupFlags::Down;
    else if ( nFlags & PopupMenuFlags::ExecuteUp )
        nPopupModeFlags = FloatWinPopupFlags::Up;
    else if ( nFlags & PopupMenuFlags::ExecuteRight )
        nPopupModeFlags = FloatWinPopupFlags::Right;
    else
        nPopupModeFlags = FloatWinPopupFlags::Down;

    if (nFlags & PopupMenuFlags::NoMouseUpClose )                      // allow popup menus to stay open on mouse button up
        nPopupModeFlags |= FloatWinPopupFlags::NoMouseUpClose;    // useful if the menu was opened on mousebutton down (eg toolbox configuration)

    return nPopupModeFlags;
}

sal_uInt16 PopupMenu::Execute( vcl::Window* pExecWindow, const tools::Rectangle& rRect, PopupMenuFlags nFlags )
{
    ENSURE_OR_RETURN( pExecWindow, "PopupMenu::Execute: need a non-NULL window!", 0 );
    return ImplExecute( pExecWindow, rRect, lcl_TranslateFlags(nFlags), nullptr, false );
}

void PopupMenu::ImplFlushPendingSelect()
{
    // is there still Select?
    Menu* pSelect = ImplFindSelectMenu();
    if (pSelect)
    {
        // Select should be called prior to leaving execute in a popup menu!
        Application::RemoveUserEvent( pSelect->nEventId );
        pSelect->nEventId = nullptr;
        pSelect->Select();
    }
}

sal_uInt16 PopupMenu::ImplExecute(const VclPtr<vcl::Window>& pParentWin, const tools::Rectangle& rRect,
                                  FloatWinPopupFlags nPopupModeFlags, Menu* pSFrom, bool bPreSelectFirst)
{
    // tdf#126054 hold this until after function completes
    VclPtr<PopupMenu> xThis(this);

    bool bRealExecute = false;
    const sal_uInt16 nItemCount = GetItemCount();
    if (!pSFrom && (vcl::IsInPopupMenuExecute() || !nItemCount))
        return 0;

    mpLayoutData.reset();

    ImplSVData* pSVData = ImplGetSVData();

    pStartedFrom = pSFrom;
    nSelectedId = 0;
    sSelectedIdent.clear();
    bCanceled = false;

    VclPtr<vcl::Window> xFocusId;
    if ( !pStartedFrom )
    {
        pSVData->mpWinData->mbNoDeactivate = true;
        xFocusId = Window::SaveFocus();
        bRealExecute = true;
    }
    else
    {
        // assure that only one menu is open at a time
        if (pStartedFrom->IsMenuBar() && pSVData->mpWinData->mpFirstFloat)
            pSVData->mpWinData->mpFirstFloat->EndPopupMode(FloatWinPopupEndFlags::Cancel
                                                           | FloatWinPopupEndFlags::CloseAll);
    }

    tools::Rectangle aRect(rRect);
    aRect.SetPos(pParentWin->OutputToScreenPixel(aRect.TopLeft()));

    nPopupModeFlags |= FloatWinPopupFlags::NoKeyClose | FloatWinPopupFlags::AllMouseButtonClose | FloatWinPopupFlags::GrabFocus;
    if (bRealExecute)
        nPopupModeFlags |= FloatWinPopupFlags::NewLevel;

    // MenuFlags get clobbered in the Activate function. Restore them after calling.
    MenuFlags nMenuFlagsSaved = GetMenuFlags();
    bInCallback = true; // set it here, if Activate overridden
    Activate();
    bInCallback = false;
    SetMenuFlags(nMenuFlagsSaved);

    if (pParentWin->isDisposed())
        return 0;

    if ( bCanceled || bKilled )
        return 0;

    if (!nItemCount)
        return 0;

    // The flag MenuFlags::HideDisabledEntries is inherited.
    if ( pSFrom )
    {
        if ( pSFrom->nMenuFlags & MenuFlags::HideDisabledEntries )
            nMenuFlags |= MenuFlags::HideDisabledEntries;
        else
            nMenuFlags &= ~MenuFlags::HideDisabledEntries;
    }

    sal_uInt16 nVisibleEntries = ImplGetVisibleItemCount();
    if ( !nVisibleEntries )
    {
        OUString aTmpEntryText(VclResId(SV_RESID_STRING_NOSELECTIONPOSSIBLE));

        MenuItemData* pData = NbcInsertItem(0xFFFF, MenuItemBits::NONE, aTmpEntryText, nullptr, 0xFFFF, {});
        size_t nPos = 0;
        pData = pItemList->GetData( pData->nId, nPos );
        assert(pData);
        if (pData)
        {
            pData->bIsTemporary = true;
        }
        ImplCallEventListeners(VclEventId::MenuSubmenuChanged, nPos);
    }

    VclPtr<MenuFloatingWindow> pWin = VclPtrInstance<MenuFloatingWindow>(this, pParentWin, WB_BORDER | WB_SYSTEMWINDOW);
    if (comphelper::LibreOfficeKit::isActive() && get_id() == "editviewspellmenu")
    {
        VclPtr<vcl::Window> xNotifierParent = pParentWin->GetParentWithLOKNotifier();
        assert(xNotifierParent && xNotifierParent->GetLOKNotifier() && "editview menu without LOKNotifier");
        pWin->SetLOKNotifier(xNotifierParent->GetLOKNotifier());
    }

    if( pSVData->maNWFData.mbFlatMenu )
        pWin->SetBorderStyle( WindowBorderStyle::NOBORDER );
    else
        pWin->SetBorderStyle( pWin->GetBorderStyle() | WindowBorderStyle::MENU );

    m_pWindow.disposeAndClear();
    m_pWindow = pWin;

    Size aSz = ImplCalcSize( pWin );

    AbsoluteScreenPixelRectangle aDesktopRect(pWin->GetDesktopRectPixel());
    if( Application::GetScreenCount() > 1 )
    {
        vcl::Window* pDeskW = m_pWindow->GetWindow( GetWindowType::RealParent );
        if( ! pDeskW )
            pDeskW = m_pWindow;
        AbsoluteScreenPixelPoint aDesktopTL(pDeskW->OutputToAbsoluteScreenPixel(aRect.TopLeft()));
        aDesktopRect = Application::GetScreenPosSizePixel(
            Application::GetBestScreen(AbsoluteScreenPixelRectangle(aDesktopTL, aRect.GetSize())));
    }

    tools::Long nMaxHeight = aDesktopRect.GetHeight();

    //rhbz#1021915. If a menu won't fit in the desired location the default
    //mode is to place it somewhere it will fit.  e.g. above, left, right. For
    //some cases, e.g. menubars, it's desirable to limit the options to
    //above/below and force the menu to scroll if it won't fit
    if (nPopupModeFlags & FloatWinPopupFlags::NoHorzPlacement)
    {
        vcl::Window* pRef = pWin;
        if ( pRef->GetParent() )
            pRef = pRef->GetParent();

        AbsoluteScreenPixelRectangle devRect(pRef->OutputToAbsoluteScreenPixel(aRect.TopLeft()),
                                             pRef->OutputToAbsoluteScreenPixel(aRect.BottomRight()));

        tools::Long nHeightAbove = devRect.Top() - aDesktopRect.Top();
        tools::Long nHeightBelow = aDesktopRect.Bottom() - devRect.Bottom();
        nMaxHeight = std::min(nMaxHeight, std::max(nHeightAbove, nHeightBelow));
    }

    // In certain cases this might be misdetected with a height of 0, leading to menus not being displayed.
    // So assume that the available screen size matches at least the system requirements. With menu origin
    // in the middle, nMaxHeight will be at least half of screen height.
    SAL_WARN_IF(nMaxHeight < 768 / 2, "vcl",
                "Available height misdetected as " << nMaxHeight
                                                   << "px. Setting to 384px instead.");
    nMaxHeight = std::max(nMaxHeight, tools::Long(768 / 2));

    if (pStartedFrom && pStartedFrom->IsMenuBar())
        nMaxHeight -= pParentWin->GetSizePixel().Height();
    sal_Int32 nLeft, nTop, nRight, nBottom;
    m_pWindow->GetBorder( nLeft, nTop, nRight, nBottom );
    nMaxHeight -= nTop+nBottom;
    if ( aSz.Height() > nMaxHeight )
    {
        pWin->EnableScrollMenu( true );
        sal_uInt16 nStart = ImplGetFirstVisible();
        sal_uInt16 nEntries = ImplCalcVisEntries( nMaxHeight, nStart );
        aSz.setHeight( ImplCalcHeight( nEntries ) );
    }

    pWin->SetFocusId( xFocusId );
    pWin->SetOutputSizePixel( aSz );

    const bool bNative = Run(pWin, bRealExecute, bPreSelectFirst, nPopupModeFlags, pSFrom, aRect);
    FinishRun(pWin, pParentWin, bRealExecute, bNative);
    return nSelectedId;
}

bool PopupMenu::Run(const VclPtr<MenuFloatingWindow>& pWin, const bool bRealExecute, const bool bPreSelectFirst,
                    const FloatWinPopupFlags nPopupModeFlags, Menu* pSFrom, const tools::Rectangle& rRect)
{
    SalMenu* pMenu = ImplGetSalMenu();
    if (pMenu && bRealExecute && pMenu->ShowNativePopupMenu(pWin, rRect, nPopupModeFlags))
        return true;

    pWin->StartPopupMode(rRect, nPopupModeFlags);
    if (pSFrom)
    {
        sal_uInt16 aPos;
        if (pSFrom->IsMenuBar())
            aPos = static_cast<MenuBarWindow *>(pSFrom->m_pWindow.get())->GetHighlightedItem();
        else
            aPos = static_cast<MenuFloatingWindow *>(pSFrom->m_pWindow.get())->GetHighlightedItem();

        pWin->SetPosInParent(aPos);  // store position to be sent in SUBMENUDEACTIVATE
        pSFrom->ImplCallEventListeners(VclEventId::MenuSubmenuActivate, aPos);
    }

    if ( bPreSelectFirst )
    {
        for (size_t n = 0; n < static_cast<size_t>(GetItemCount()); n++)
        {
            MenuItemData* pData = pItemList->GetDataFromPos( n );
            if (  (  pData->bEnabled
                  || !Application::GetSettings().GetStyleSettings().GetSkipDisabledInMenus()
                  )
               && ( pData->eType != MenuItemType::SEPARATOR )
               && ImplIsVisible( n )
               && ImplIsSelectable( n )
               )
            {
                pWin->ChangeHighlightItem(n, false);
                break;
            }
        }
    }

    if (bRealExecute)
        pWin->Execute();

    return false;
}

void PopupMenu::FinishRun(const VclPtr<MenuFloatingWindow>& pWin, const VclPtr<vcl::Window>& pParentWin, const bool bRealExecute, const bool bIsNativeMenu)
{
    if (!bRealExecute || pWin->isDisposed())
        return;

    if (!bIsNativeMenu)
    {
        VclPtr<vcl::Window> xFocusId = pWin->GetFocusId();
        assert(xFocusId == nullptr && "Focus should already be restored by MenuFloatingWindow::End");
        pWin->ImplEndPopupMode(FloatWinPopupEndFlags::NONE, xFocusId);

        if (nSelectedId)  // then clean up .. ( otherwise done by TH )
        {
            PopupMenu* pSub = pWin->GetActivePopup();
            while ( pSub )
            {
                pSub->ImplGetFloatingWindow()->EndPopupMode();
                pSub = pSub->ImplGetFloatingWindow()->GetActivePopup();
            }
        }
    }
    else
        pWin->StopExecute();

    pWin->doShutdown();
    ImplClosePopupToolBox(pParentWin);
    ImplFlushPendingSelect();
}

sal_uInt16 PopupMenu::ImplCalcVisEntries( tools::Long nMaxHeight, sal_uInt16 nStartEntry, sal_uInt16* pLastVisible ) const
{
    nMaxHeight -= 2 * ImplGetFloatingWindow()->GetScrollerHeight();

    tools::Long nHeight = 0;
    size_t nEntries = pItemList->size();
    sal_uInt16 nVisEntries = 0;

    if ( pLastVisible )
        *pLastVisible = 0;

    for ( size_t n = nStartEntry; n < nEntries; n++ )
    {
        if ( ImplIsVisible( n ) )
        {
            MenuItemData* pData = pItemList->GetDataFromPos( n );
            nHeight += pData->aSz.Height();
            if ( nHeight > nMaxHeight )
                break;

            if ( pLastVisible )
                *pLastVisible = n;
            nVisEntries++;
        }
    }
    return nVisEntries;
}

tools::Long PopupMenu::ImplCalcHeight( sal_uInt16 nEntries ) const
{
    tools::Long nHeight = 0;

    sal_uInt16 nFound = 0;
    for ( size_t n = 0; ( nFound < nEntries ) && ( n < pItemList->size() ); n++ )
    {
        if ( ImplIsVisible( static_cast<sal_uInt16>(n) ) )
        {
            MenuItemData* pData = pItemList->GetDataFromPos( n );
            nHeight += pData->aSz.Height();
            nFound++;
        }
    }

    nHeight += 2*ImplGetFloatingWindow()->GetScrollerHeight();

    return nHeight;
}

css::uno::Reference<css::awt::XPopupMenu> PopupMenu::CreateMenuInterface()
{
    UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper();
    if ( pWrapper )
        return pWrapper->CreateMenuInterface(this);
    return nullptr;
}

ImplMenuDelData::ImplMenuDelData( const Menu* pMenu )
: mpNext( nullptr )
, mpMenu( nullptr )
{
    if( pMenu )
        const_cast< Menu* >( pMenu )->ImplAddDel( *this );
}

ImplMenuDelData::~ImplMenuDelData()
{
    if( mpMenu )
        const_cast< Menu* >( mpMenu.get() )->ImplRemoveDel( *this );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
