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

#ifndef INCLUDED_VCL_WINDOW_HXX
#define INCLUDED_VCL_WINDOW_HXX

#include <vcl/dllapi.h>
#include <vcl/outdev.hxx>
#include <tools/link.hxx>
#include <vcl/wintypes.hxx>
#include <vcl/vclenum.hxx>
#include <vcl/keycodes.hxx>
#include <vcl/region.hxx>
#include <vcl/uitest/factory.hxx>
#include <vcl/IDialogRenderable.hxx>
#include <rtl/ustring.hxx>
#include <com/sun/star/uno/Reference.hxx>
#include <memory>

struct ImplSVEvent;
struct ImplWinData;
struct ImplFrameData;
struct ImplCalcToTopData;
struct SystemEnvData;
struct SystemParentData;
class ImplBorderWindow;
class Timer;
class DNDListenerContainer;
class DockingManager;
class Scrollable;
class FixedText;
class MouseEvent;
class KeyEvent;
class CommandEvent;
class TrackingEvent;
class HelpEvent;
class DataChangedEvent;
class NotifyEvent;
class SystemWindow;
class SalFrame;
class MenuFloatingWindow;
class VCLXWindow;
class VclWindowEvent;
class AllSettings;
class InputContext;
class VclEventListeners;
class EditView;
enum class ImplPaintFlags;
enum class VclEventId;
enum class PointerStyle;

namespace com::sun::star {
    namespace accessibility {
        class XAccessible;
    }
    namespace awt {
        class XVclWindowPeer;
    }
    namespace datatransfer::clipboard {
        class XClipboard;
    }
    namespace datatransfer::dnd {
        class XDragGestureRecognizer;
        class XDragSource;
        class XDropTarget;
    }
}

namespace comphelper
{
class OAccessible;
}

namespace vcl {
    struct ControlLayoutData;
}

namespace svt { class PopupWindowControllerImpl; }

namespace weld { class Window; }

template<class T> class VclPtr;
namespace tools { class JsonWriter; }

// Type for GetWindow()
enum class GetWindowType
{
    Parent                   =  0,
    FirstChild               =  1,
    LastChild                =  2,
    Prev                     =  3,
    Next                     =  4,
    FirstOverlap             =  5,
    Overlap                  =  7,
    ParentOverlap            =  8,
    Client                   =  9,
    RealParent               = 10,
    Frame                    = 11,
    Border                   = 12,
    FirstTopWindowChild      = 13,
    NextTopWindowSibling     = 16,
};

// Flags for setPosSizePixel()
// These must match the definitions in css::awt::PosSize
enum class PosSizeFlags
{
    NONE             = 0x0000,
    X                = 0x0001,
    Y                = 0x0002,
    Width            = 0x0004,
    Height           = 0x0008,
    Pos              = X | Y,
    Size             = Width | Height,
    PosSize          = Pos | Size,
    All              = PosSize,
};

namespace o3tl
{
    template<> struct typed_flags<PosSizeFlags> : is_typed_flags<PosSizeFlags, 0x000f> {};
}

// Flags for SetZOrder()
enum class ZOrderFlags
{
    NONE              = 0x0000,
    Before            = 0x0001,
    Behind            = 0x0002,
    First             = 0x0004,
    Last              = 0x0008,
};
namespace o3tl
{
    template<> struct typed_flags<ZOrderFlags> : is_typed_flags<ZOrderFlags, 0x000f> {};
}

// Activate-Flags
enum class ActivateModeFlags
{
    NONE        = 0,
    GrabFocus   = 0x0001,
};
namespace o3tl
{
    template<> struct typed_flags<ActivateModeFlags> : is_typed_flags<ActivateModeFlags, 0x0001> {};
}

// ToTop-Flags
enum class ToTopFlags
{
    NONE            = 0x0000,
    RestoreWhenMin  = 0x0001,
    ForegroundTask  = 0x0002,
    NoGrabFocus     = 0x0004,
    GrabFocusOnly   = 0x0008,
};
namespace o3tl
{
    template<> struct typed_flags<ToTopFlags> : is_typed_flags<ToTopFlags, 0x000f> {};
}

// Flags for Invalidate
// must match css::awt::InvalidateStyle
enum class InvalidateFlags
{
    NONE                 = 0x0000,
    /** The child windows are invalidated, too. */
    Children             = 0x0001,
    /** The child windows are not invalidated. */
    NoChildren           = 0x0002,
    /** The invalidated area is painted with the background color/pattern. */
    NoErase              = 0x0004,
    /** The invalidated area is updated immediately. */
    Update               = 0x0008,
    /** The parent window is invalidated, too. */
    Transparent          = 0x0010,
    /** The parent window is not invalidated. */
    NoTransparent        = 0x0020,
    /** The area is invalidated regardless of overlapping child windows. */
    NoClipChildren       = 0x4000,
};
namespace o3tl
{
    template<> struct typed_flags<InvalidateFlags> : is_typed_flags<InvalidateFlags, 0x403f> {};
}

// Flags for Validate
enum class ValidateFlags
{
    NONE                = 0x0000,
    Children            = 0x0001,
    NoChildren          = 0x0002
};
namespace o3tl
{
    template<> struct typed_flags<ValidateFlags> : is_typed_flags<ValidateFlags, 0x0003> {};
}

// Flags for Scroll
enum class ScrollFlags
{
    NONE                     = 0x0000,
    Clip                     = 0x0001,
    Children                 = 0x0002,
    NoChildren               = 0x0004,
    UseClipRegion            = 0x0008,
    Update                   = 0x0010, // paint immediately
};
namespace o3tl
{
    template<> struct typed_flags<ScrollFlags> : is_typed_flags<ScrollFlags, 0x001f> {};
}

// Flags for ParentClipMode
enum class ParentClipMode
{
    NONE             = 0x0000,
    Clip             = 0x0001,
    NoClip           = 0x0002,
};
namespace o3tl
{
    template<> struct typed_flags<ParentClipMode> : is_typed_flags<ParentClipMode, 0x0003> {};
}

// Flags for ShowTracking()
enum class ShowTrackFlags {
    NONE                  = 0x0000,
    Small                 = 0x0001,
    Big                   = 0x0002,
    Split                 = 0x0003,
    Object                = 0x0004,
    StyleMask             = 0x000F,
    TrackWindow           = 0x1000,
    Clip                  = 0x2000,
};
namespace o3tl
{
    template<> struct typed_flags<ShowTrackFlags> : is_typed_flags<ShowTrackFlags, 0x300f> {};
}

// Flags for StartTracking()
enum class StartTrackingFlags
{
    NONE                 = 0x0001,
    KeyMod               = 0x0002,
    ScrollRepeat         = 0x0004,
    ButtonRepeat         = 0x0008,
};

namespace o3tl
{
    template<> struct typed_flags<StartTrackingFlags> : is_typed_flags<StartTrackingFlags, 0x000f> {};
}

// Flags for StartAutoScroll()
enum class StartAutoScrollFlags
{
    NONE                 = 0x0000,
    Vert                 = 0x0001,
    Horz                 = 0x0002,
};
namespace o3tl
{
    template<> struct typed_flags<StartAutoScrollFlags> : is_typed_flags<StartAutoScrollFlags, 0x0003> {};
}

// Flags for StateChanged()
enum class StateChangedType : sal_uInt16
{
    InitShow           = 1,
    Visible            = 2,
    UpdateMode         = 3,
    Enable             = 4,
    Text               = 5,
    Data               = 7,
    State              = 8,
    Style              = 9,
    Zoom               = 10,
    ControlFont        = 13,
    ControlForeground  = 14,
    ControlBackground  = 15,
    ReadOnly           = 16,
    Mirroring          = 18,
    Layout             = 19,
    ControlFocus       = 20
};

// GetFocusFlags
// must match constants in css:awt::FocusChangeReason
enum class GetFocusFlags
{
    NONE                   = 0x0000,
    Tab                    = 0x0001,
    CURSOR                 = 0x0002, // avoid name-clash with X11 #define
    Mnemonic               = 0x0004,
    F6                     = 0x0008,
    Forward                = 0x0010,
    Backward               = 0x0020,
    Around                 = 0x0040,
    UniqueMnemonic         = 0x0100,
    Init                   = 0x0200,
    FloatWinPopupModeEndCancel = 0x0400,
};
namespace o3tl
{
    template<> struct typed_flags<GetFocusFlags> : is_typed_flags<GetFocusFlags, 0x077f> {};
}

// DialogControl-Flags
enum class DialogControlFlags
{
    NONE                       = 0x0000,
    Return                     = 0x0001,
    WantFocus                  = 0x0002,
    FloatWinPopupModeEndCancel = 0x0004,
};
namespace o3tl
{
    template<> struct typed_flags<DialogControlFlags> : is_typed_flags<DialogControlFlags, 0x0007> {};
}

// EndExtTextInput() Flags
enum class EndExtTextInputFlags
{
    NONE           = 0x0000,
    Complete       = 0x0001
};
namespace o3tl
{
    template<> struct typed_flags<EndExtTextInputFlags> : is_typed_flags<EndExtTextInputFlags, 0x0001> {};
}

#define IMPL_MINSIZE_BUTTON_WIDTH       70
#define IMPL_MINSIZE_BUTTON_HEIGHT      22
#define IMPL_EXTRA_BUTTON_WIDTH         18
#define IMPL_EXTRA_BUTTON_HEIGHT        10
#define IMPL_SEP_BUTTON_X               5
#define IMPL_SEP_BUTTON_Y               5
#define IMPL_MINSIZE_MSGBOX_WIDTH       150
#define IMPL_DIALOG_OFFSET              5
#define IMPL_DIALOG_BAR_OFFSET          3
#define IMPL_MSGBOX_OFFSET_EXTRA_X      0
#define IMPL_MSGBOX_OFFSET_EXTRA_Y      2
#define IMPL_SEP_MSGBOX_IMAGE           8

// ImplGetDlgWindow()
enum class GetDlgWindowType
{
    Prev, Next, First
};


#ifdef DBG_UTIL
const char* ImplDbgCheckWindow( const void* pObj );
#endif

namespace vcl { class Window; }
namespace vcl { class Cursor; }
namespace vcl { class WindowOutputDevice; }
class Dialog;
class Edit;
class WindowImpl;
class PaintHelper;
class VclSizeGroup;
class Application;
class WorkWindow;
class MessBox;
class MessageDialog;
class DockingWindow;
class FloatingWindow;
class GroupBox;
class PushButton;
class RadioButton;
class SalInstanceWidget;
class SystemChildWindow;
class ImplDockingWindowWrapper;
class ImplPopupFloatWin;
class LifecycleTest;


enum class WindowHitTest {
    NONE        = 0x0000,
    Inside      = 0x0001,
    Transparent = 0x0002
};
namespace o3tl {
    template<> struct typed_flags<WindowHitTest> : is_typed_flags<WindowHitTest, 0x0003> {};
};


enum class WindowExtendedStyle {
    NONE        = 0x0000,
    Document    = 0x0001,
    DocModified = 0x0002,
    /**
     * This is a frame window that is requested to be hidden (not just "not yet
     * shown").
     */
    DocHidden   = 0x0004,
};
namespace o3tl {
    template<> struct typed_flags<WindowExtendedStyle> : is_typed_flags<WindowExtendedStyle, 0x0007> {};
};

namespace vcl {

class RenderTools
{
public:
    // transparent background for selected or checked items in toolboxes etc.
    // + selection Color with a text color complementing the selection background
    // + rounded edge
    static void DrawSelectionBackground(vcl::RenderContext& rRenderContext, vcl::Window const & rWindow,
                                        const tools::Rectangle& rRect, sal_uInt16 nHighlight,
                                        bool bChecked, bool bDrawBorder, bool bDrawExtBorderOnly,
                                        Color* pSelectionTextColor = nullptr, tools::Long nCornerRadius = 0,
                                        Color const * pPaintColor = nullptr);
};

class VCL_DLLPUBLIC Window : public virtual VclReferenceBase
{
    friend class ::vcl::Cursor;
    friend class ::vcl::WindowOutputDevice;
    friend class ::OutputDevice;
    friend class ::Application;
    friend class ::SystemWindow;
    friend class ::WorkWindow;
    friend class ::Dialog;
    friend class ::Edit;
    friend class ::MessBox;
    friend class ::MessageDialog;
    friend class ::DockingWindow;
    friend class ::FloatingWindow;
    friend class ::GroupBox;
    friend class ::PushButton;
    friend class ::RadioButton;
    friend class ::SalInstanceWidget;
    friend class ::SystemChildWindow;
    friend class ::ImplBorderWindow;
    friend class ::PaintHelper;
    friend class ::LifecycleTest;
    friend class ::VclEventListeners;

    // TODO: improve missing functionality
    // only required because of SetFloatingMode()
    friend class ::ImplDockingWindowWrapper;
    friend class ::ImplPopupFloatWin;
    friend class ::MenuFloatingWindow;

    friend class ::svt::PopupWindowControllerImpl;

private:
    // NOTE: to remove many dependencies of other modules
    //       to this central file, all members are now hidden
    //       in the WindowImpl class and all inline functions
    //       were removed.
    //       (WindowImpl is a pImpl pattern)

    //       Please do *not* add new members or inline functions to class Window,
    //       but use class WindowImpl instead

    std::unique_ptr<WindowImpl> mpWindowImpl;

#ifdef DBG_UTIL
    friend const char* ::ImplDbgCheckWindow( const void* pObj );
#endif

public:

    DECL_DLLPRIVATE_LINK( ImplHandlePaintHdl, Timer*, void );
    DECL_DLLPRIVATE_LINK( ImplGenerateMouseMoveHdl, void*, void );
    DECL_DLLPRIVATE_LINK( ImplTrackTimerHdl, Timer*, void );
    DECL_DLLPRIVATE_LINK( ImplAsyncFocusHdl, void*, void );
    DECL_DLLPRIVATE_LINK( ImplHandleResizeTimerHdl, Timer*, void );


    SAL_DLLPRIVATE static void          ImplInitAppFontData( vcl::Window const * pWindow );

    SAL_DLLPRIVATE vcl::Window*         ImplGetFrameWindow() const;
    weld::Window*                       GetFrameWeld() const;
    vcl::Window*                        GetFrameWindow() const;
    SalFrame*                           ImplGetFrame() const;
    SAL_DLLPRIVATE ImplFrameData*       ImplGetFrameData();

    vcl::Window*                        ImplGetWindow() const; ///< if this is a proxy return the client, otherwise itself
    SAL_DLLPRIVATE ImplWinData*         ImplGetWinData() const;
    SAL_DLLPRIVATE vcl::Window*         ImplGetClientWindow() const;
    SAL_DLLPRIVATE vcl::Window*         ImplGetDlgWindow( sal_uInt16 n, GetDlgWindowType nType, sal_uInt16 nStart = 0, sal_uInt16 nEnd = 0xFFFF, sal_uInt16* pIndex = nullptr );
    SAL_DLLPRIVATE vcl::Window*         ImplGetParent() const;
    SAL_DLLPRIVATE vcl::Window*         ImplFindWindow( const Point& rFramePos );

    SAL_DLLPRIVATE void                 ImplInvalidateFrameRegion( const vcl::Region* pRegion, InvalidateFlags nFlags );
    SAL_DLLPRIVATE void                 ImplInvalidateOverlapFrameRegion( const vcl::Region& rRegion );

    SAL_DLLPRIVATE bool                 ImplSetClipFlag( bool bSysObjOnlySmaller = false );

    SAL_DLLPRIVATE bool                 ImplIsWindowOrChild( const vcl::Window* pWindow, bool bSystemWindow = false ) const;
    SAL_DLLPRIVATE bool                 ImplIsChild( const vcl::Window* pWindow, bool bSystemWindow = false ) const;
    SAL_DLLPRIVATE bool                 ImplIsFloatingWindow() const;
    SAL_DLLPRIVATE bool                 ImplIsPushButton() const;
    SAL_DLLPRIVATE bool                 ImplIsSplitter() const;
    SAL_DLLPRIVATE bool                 ImplIsOverlapWindow() const;

    SAL_DLLPRIVATE void                 ImplIsInTaskPaneList( bool mbIsInTaskList );

    SAL_DLLPRIVATE WindowImpl*          ImplGetWindowImpl() const { return mpWindowImpl.get(); }

    SAL_DLLPRIVATE void                 ImplGrabFocus( GetFocusFlags nFlags );
    SAL_DLLPRIVATE void                 ImplGrabFocusToDocument( GetFocusFlags nFlags );
    SAL_DLLPRIVATE void                 ImplInvertFocus( const tools::Rectangle& rRect );

    SAL_DLLPRIVATE PointerStyle         ImplGetMousePointer() const;
    SAL_DLLPRIVATE void                 ImplCallMouseMove( sal_uInt16 nMouseCode, bool bModChanged = false );
    SAL_DLLPRIVATE void                 ImplGenerateMouseMove();

    SAL_DLLPRIVATE void                 ImplNotifyKeyMouseCommandEventListeners( NotifyEvent& rNEvt );
    SAL_DLLPRIVATE void                 ImplNotifyIconifiedState( bool bIconified );

    SAL_DLLPRIVATE void                 ImplUpdateAll();

    SAL_DLLPRIVATE void                 ImplControlFocus( GetFocusFlags nFlags = GetFocusFlags::NONE );

    SAL_DLLPRIVATE void                 ImplMirrorFramePos( Point &pt ) const;

    SAL_DLLPRIVATE void                 ImplPosSizeWindow( tools::Long nX, tools::Long nY, tools::Long nWidth, tools::Long nHeight, PosSizeFlags nFlags );

    SAL_DLLPRIVATE void                 ImplCallResize();
    SAL_DLLPRIVATE void                 ImplCallMove();

    // These methods call the relevant virtual method when not in/post dispose
    SAL_DLLPRIVATE void                 CompatGetFocus();
    SAL_DLLPRIVATE void                 CompatLoseFocus();
    SAL_DLLPRIVATE void                 CompatStateChanged( StateChangedType nStateChange );
    SAL_DLLPRIVATE void                 CompatDataChanged( const DataChangedEvent& rDCEvt );
    SAL_DLLPRIVATE bool                 CompatPreNotify( NotifyEvent& rNEvt );
    SAL_DLLPRIVATE bool                 CompatNotify( NotifyEvent& rNEvt );

                   void                 IncModalCount();
                   void                 DecModalCount();

    SAL_DLLPRIVATE static void          ImplCalcSymbolRect( tools::Rectangle& rRect );

protected:

    /** This is intended to be used to clear any locally held references to other Window-subclass objects */
    virtual void                        dispose() override;

    SAL_DLLPRIVATE void                 ImplInit( vcl::Window* pParent, WinBits nStyle, SystemParentData* pSystemParentData );

    SAL_DLLPRIVATE void                 ImplInvalidateParentFrameRegion( const vcl::Region& rRegion );
    SAL_DLLPRIVATE void                 ImplValidateFrameRegion( const vcl::Region* rRegion, ValidateFlags nFlags );
    SAL_DLLPRIVATE void                 ImplValidate();
    SAL_DLLPRIVATE void                 ImplMoveInvalidateRegion( const tools::Rectangle& rRect, tools::Long nHorzScroll, tools::Long nVertScroll, bool bChildren );
    SAL_DLLPRIVATE void                 ImplMoveAllInvalidateRegions( const tools::Rectangle& rRect, tools::Long nHorzScroll, tools::Long nVertScroll, bool bChildren );

    SAL_DLLPRIVATE vcl::Window*         ImplGetBorderWindow() const;

    virtual void                        ImplInvalidate( const vcl::Region* pRegion, InvalidateFlags nFlags );

    virtual WindowHitTest               ImplHitTest( const Point& rFramePos );

    SAL_DLLPRIVATE void                 ImplSetMouseTransparent( bool bTransparent );

    SAL_DLLPRIVATE void                 ImplScroll( const tools::Rectangle& rRect, tools::Long nHorzScroll, tools::Long nVertScroll, ScrollFlags nFlags );

    SAL_DLLPRIVATE bool                 ImplSetClipFlagChildren( bool bSysObjOnlySmaller );
    SAL_DLLPRIVATE bool                 ImplSetClipFlagOverlapWindows( bool bSysObjOnlySmaller = false );

    SAL_DLLPRIVATE void                 PushPaintHelper(PaintHelper* pHelper, vcl::RenderContext& rRenderContext);
    SAL_DLLPRIVATE void                 PopPaintHelper(PaintHelper const * pHelper);

private:

    SAL_DLLPRIVATE void                 ImplSetFrameParent( const vcl::Window* pParent );

    SAL_DLLPRIVATE void                 ImplInsertWindow( vcl::Window* pParent );
    SAL_DLLPRIVATE void                 ImplRemoveWindow( bool bRemoveFrameData );

    SAL_DLLPRIVATE SalGraphics*         ImplGetFrameGraphics() const;

    SAL_DLLPRIVATE static void          ImplCallFocusChangeActivate( vcl::Window* pNewOverlapWindow, vcl::Window* pOldOverlapWindow );
    SAL_DLLPRIVATE vcl::Window*         ImplGetFirstOverlapWindow();
    SAL_DLLPRIVATE const vcl::Window*   ImplGetFirstOverlapWindow() const;

    SAL_DLLPRIVATE bool                 ImplIsRealParentPath( const vcl::Window* pWindow ) const;

    SAL_DLLPRIVATE bool                 ImplTestMousePointerSet();

    SAL_DLLPRIVATE void                 ImplResetReallyVisible();
    SAL_DLLPRIVATE void                 ImplSetReallyVisible();

    SAL_DLLPRIVATE void                 ImplCallInitShow();

    SAL_DLLPRIVATE void                 ImplInitResolutionSettings();

    SAL_DLLPRIVATE void                 ImplPointToLogic(vcl::RenderContext const & rRenderContext, vcl::Font& rFont, bool bUseRenderContextDPI = false) const;
    SAL_DLLPRIVATE void                 ImplLogicToPoint(vcl::RenderContext const & rRenderContext, vcl::Font& rFont) const;

    SAL_DLLPRIVATE bool                 ImplSysObjClip( const vcl::Region* pOldRegion );
    SAL_DLLPRIVATE void                 ImplUpdateSysObjChildrenClip();
    SAL_DLLPRIVATE void                 ImplUpdateSysObjOverlapsClip();
    SAL_DLLPRIVATE void                 ImplUpdateSysObjClip();

    SAL_DLLPRIVATE void                 ImplIntersectWindowClipRegion( vcl::Region& rRegion );
    SAL_DLLPRIVATE void                 ImplIntersectWindowRegion( vcl::Region& rRegion );
    SAL_DLLPRIVATE void                 ImplExcludeWindowRegion( vcl::Region& rRegion );
    SAL_DLLPRIVATE void                 ImplExcludeOverlapWindows( vcl::Region& rRegion ) const;
    SAL_DLLPRIVATE void                 ImplExcludeOverlapWindows2( vcl::Region& rRegion );

    SAL_DLLPRIVATE void                 ImplClipBoundaries( vcl::Region& rRegion, bool bThis, bool bOverlaps );
    SAL_DLLPRIVATE bool                 ImplClipChildren( vcl::Region& rRegion ) const;
    SAL_DLLPRIVATE void                 ImplClipAllChildren( vcl::Region& rRegion ) const;
    SAL_DLLPRIVATE void                 ImplClipSiblings( vcl::Region& rRegion ) const;

    SAL_DLLPRIVATE void                 ImplInitWinClipRegion();
    SAL_DLLPRIVATE void                 ImplInitWinChildClipRegion();
    SAL_DLLPRIVATE vcl::Region&         ImplGetWinChildClipRegion();

    SAL_DLLPRIVATE void                 ImplIntersectAndUnionOverlapWindows( const vcl::Region& rInterRegion, vcl::Region& rRegion ) const;
    SAL_DLLPRIVATE void                 ImplIntersectAndUnionOverlapWindows2( const vcl::Region& rInterRegion, vcl::Region& rRegion );
    SAL_DLLPRIVATE void                 ImplCalcOverlapRegionOverlaps( const vcl::Region& rInterRegion, vcl::Region& rRegion ) const;
    SAL_DLLPRIVATE void                 ImplCalcOverlapRegion( const tools::Rectangle& rSourceRect, vcl::Region& rRegion,
                                                               bool bChildren, bool bSiblings );

    /** Invoke the actual painting.

        This function is kind of recursive - it may be called from the
        PaintHelper destructor; and on the other hand it creates PaintHelper
        that (when destructed) calls other ImplCallPaint()'s.
    */
    SAL_DLLPRIVATE void                 ImplCallPaint(const vcl::Region* pRegion, ImplPaintFlags nPaintFlags);

    SAL_DLLPRIVATE void                 ImplCallOverlapPaint();

    SAL_DLLPRIVATE void                 ImplUpdateWindowPtr( vcl::Window* pWindow );
    SAL_DLLPRIVATE void                 ImplUpdateWindowPtr();
    SAL_DLLPRIVATE void                 ImplUpdateOverlapWindowPtr( bool bNewFrame );

    SAL_DLLPRIVATE bool                 ImplUpdatePos();
    SAL_DLLPRIVATE void                 ImplUpdateSysObjPos();

    SAL_DLLPRIVATE void                 ImplUpdateGlobalSettings( AllSettings& rSettings, bool bCallHdl = true ) const;

    SAL_DLLPRIVATE void                 ImplToBottomChild();

    SAL_DLLPRIVATE void                 ImplCalcToTop( ImplCalcToTopData* pPrevData );
    SAL_DLLPRIVATE void                 ImplToTop( ToTopFlags nFlags );
    SAL_DLLPRIVATE void                 ImplStartToTop( ToTopFlags nFlags );
    SAL_DLLPRIVATE void                 ImplFocusToTop( ToTopFlags nFlags, bool bReallyVisible );

    SAL_DLLPRIVATE void                 ImplShowAllOverlaps();
    SAL_DLLPRIVATE void                 ImplHideAllOverlaps();

    SAL_DLLPRIVATE bool                 ImplDlgCtrl( const KeyEvent& rKEvt, bool bKeyInput );
    SAL_DLLPRIVATE bool                 ImplHasDlgCtrl() const;
    SAL_DLLPRIVATE void                 ImplDlgCtrlNextWindow();
    SAL_DLLPRIVATE void                 ImplDlgCtrlFocusChanged( const vcl::Window* pWindow, bool bGetFocus );
    SAL_DLLPRIVATE vcl::Window*         ImplFindDlgCtrlWindow( const vcl::Window* pWindow );

    SAL_DLLPRIVATE static void          ImplNewInputContext();

    SAL_DLLPRIVATE void                 ImplCallActivateListeners(vcl::Window*);
    SAL_DLLPRIVATE void                 ImplCallDeactivateListeners(vcl::Window*);

    SAL_DLLPRIVATE static void          ImplHandleScroll(Scrollable* pHScrl, double nX, Scrollable* pVScrl, double nY);

    SAL_DLLPRIVATE AbsoluteScreenPixelRectangle ImplOutputToUnmirroredAbsoluteScreenPixel( const tools::Rectangle& rRect ) const;
    SAL_DLLPRIVATE tools::Rectangle     ImplUnmirroredAbsoluteScreenToOutputPixel( const AbsoluteScreenPixelRectangle& rRect ) const;
    SAL_DLLPRIVATE tools::Long          ImplGetUnmirroredOutOffX() const;

    // retrieves the list of owner draw decorated windows for this window hierarchy
    SAL_DLLPRIVATE ::std::vector<VclPtr<vcl::Window> >& ImplGetOwnerDrawList();

    SAL_DLLPRIVATE vcl::Window*         ImplGetTopmostFrameWindow() const;

    SAL_DLLPRIVATE bool                 ImplStopDnd();
    SAL_DLLPRIVATE void                 ImplStartDnd();

    virtual void                        ImplPaintToDevice( ::OutputDevice* pTargetOutDev, const Point& rPos );

protected:
    // Single argument ctors shall be explicit.
    SAL_DLLPRIVATE explicit             Window( WindowType eType );

            void                        SetCompoundControl( bool bCompound );

            void                        CallEventListeners( VclEventId nEvent, void* pData = nullptr );

    // FIXME: this is a hack to workaround missing layout functionality
    virtual void                        ImplAdjustNWFSizes();

    virtual void ApplySettings(vcl::RenderContext& rRenderContext);

public:
    // Single argument ctors shall be explicit.
    explicit                            Window( vcl::Window* pParent, WinBits nStyle = 0 );

    virtual                             ~Window() override;

    ::OutputDevice const*               GetOutDev() const;
    ::OutputDevice*                     GetOutDev();

    Color                               GetBackgroundColor() const;
    const Wallpaper &                   GetBackground() const;
    bool                                IsBackground() const;
    const MapMode&                      GetMapMode() const;
    void                                SetBackground();
    void                                SetBackground( const Wallpaper& rBackground );

    virtual void                        MouseMove( const MouseEvent& rMEvt );
    virtual void                        MouseButtonDown( const MouseEvent& rMEvt );
    virtual void                        MouseButtonUp( const MouseEvent& rMEvt );
    virtual void                        KeyInput( const KeyEvent& rKEvt );
    virtual void                        KeyUp( const KeyEvent& rKEvt );
    virtual void                        PrePaint(vcl::RenderContext& rRenderContext);
    virtual void                        Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect);
    virtual void                        PostPaint(vcl::RenderContext& rRenderContext);

    SAL_DLLPRIVATE void                 Erase(vcl::RenderContext& rRenderContext);

    virtual void                        Draw( ::OutputDevice* pDev, const Point& rPos, SystemTextColorFlags nFlags );
    virtual void                        Move();
    virtual void                        Resize();
    virtual void                        Activate();
    virtual void                        Deactivate();
    virtual void                        GetFocus();
    virtual void                        LoseFocus();
    virtual void                        RequestHelp( const HelpEvent& rHEvt );
    virtual void                        Command( const CommandEvent& rCEvt );
    virtual void                        Tracking( const TrackingEvent& rTEvt );
    virtual void                        StateChanged( StateChangedType nStateChange );
    virtual void                        DataChanged( const DataChangedEvent& rDCEvt );
    virtual bool                        PreNotify( NotifyEvent& rNEvt );
    virtual bool                        EventNotify( NotifyEvent& rNEvt );

    void                                AddEventListener( const Link<VclWindowEvent&,void>& rEventListener );
    void                                RemoveEventListener( const Link<VclWindowEvent&,void>& rEventListener );
    void                                AddChildEventListener( const Link<VclWindowEvent&,void>& rEventListener );
    void                                RemoveChildEventListener( const Link<VclWindowEvent&,void>& rEventListener );

    ImplSVEvent *                       PostUserEvent( const Link<void*,void>& rLink, void* pCaller = nullptr, bool bReferenceLink = false );
    void                                RemoveUserEvent( ImplSVEvent * nUserEvent );

                                        // returns the input language used for the last key stroke
                                        // may be LANGUAGE_DONTKNOW if not supported by the OS
    LanguageType                        GetInputLanguage() const;

    void                                SetStyle( WinBits nStyle );
    WinBits                             GetStyle() const;
    SAL_DLLPRIVATE WinBits              GetPrevStyle() const;
    void                                SetExtendedStyle( WindowExtendedStyle nExtendedStyle );
    WindowExtendedStyle                 GetExtendedStyle() const;
    void                                SetType( WindowType eType );
    WindowType                          GetType() const;
    bool                                IsSystemWindow() const;
    SAL_DLLPRIVATE bool                 IsDockingWindow() const;
    bool                                IsDialog() const;
    bool                                IsMenuFloatingWindow() const;
    bool                                IsNativeFrame() const;
    bool                                IsTopWindow() const;
    SystemWindow*                       GetSystemWindow() const;

    /// Can the widget derived from this Window do the double-buffering via RenderContext properly?
    bool                                SupportsDoubleBuffering() const;
    /// Enable/disable double-buffering of the frame window and all its children.
    void                                RequestDoubleBuffering(bool bRequest);

    void                                EnableAllResize();

    void                                SetBorderStyle( WindowBorderStyle nBorderStyle );
    WindowBorderStyle                   GetBorderStyle() const;
    /// Get the left, top, right and bottom widths of the window border
    void                                GetBorder( sal_Int32& rLeftBorder, sal_Int32& rTopBorder,
                                                   sal_Int32& rRightBorder, sal_Int32& rBottomBorder ) const;
    Size                                CalcWindowSize( const Size& rOutSz ) const;
    SAL_DLLPRIVATE Size                 CalcOutputSize( const Size& rWinSz ) const;
    tools::Long                                CalcTitleWidth() const;

    void                                EnableClipSiblings( bool bClipSiblings = true );

    void                                EnableChildTransparentMode( bool bEnable = true );
    bool                                IsChildTransparentModeEnabled() const;

    void                                SetMouseTransparent( bool bTransparent );
    bool                                IsMouseTransparent() const;
    void                                SetPaintTransparent( bool bTransparent );
    bool                                IsPaintTransparent() const;
    void                                SetDialogControlStart( bool bStart );
    SAL_DLLPRIVATE bool                 IsDialogControlStart() const;
    void                                SetDialogControlFlags( DialogControlFlags nFlags );
    SAL_DLLPRIVATE DialogControlFlags   GetDialogControlFlags() const;

    struct PointerState
    {
        sal_Int32 mnState;    // the button state
        Point     maPos;      // mouse position in output coordinates
    };
    PointerState                        GetPointerState();
    bool                                IsMouseOver() const;

    void                                SetInputContext( const InputContext& rInputContext );
    const InputContext&                 GetInputContext() const;
    void                                PostExtTextInputEvent(VclEventId nType, const OUString& rText);
    SAL_DLLPRIVATE void                 EndExtTextInput();
    void                                SetCursorRect( const tools::Rectangle* pRect = nullptr, tools::Long nExtTextInputWidth = 0 );
    SAL_DLLPRIVATE const tools::Rectangle* GetCursorRect() const;
    SAL_DLLPRIVATE tools::Long          GetCursorExtTextInputWidth() const;

    void                                SetCompositionCharRect( const tools::Rectangle* pRect, tools::Long nCompositionLength, bool bVertical = false );

    SAL_DLLPRIVATE void                 UpdateSettings( const AllSettings& rSettings, bool bChild = false );
    SAL_DLLPRIVATE void                 NotifyAllChildren( DataChangedEvent& rDCEvt );

    void                                SetPointFont(vcl::RenderContext& rRenderContext, const vcl::Font& rFont, bool bUseRenderContextDPI = false);
    vcl::Font                           GetPointFont(vcl::RenderContext const & rRenderContext) const;
    void                                SetZoomedPointFont(vcl::RenderContext& rRenderContext, const vcl::Font& rFont);
    SAL_DLLPRIVATE tools::Long          GetDrawPixel( ::OutputDevice const * pDev, tools::Long nPixels ) const;
    vcl::Font                           GetDrawPixelFont( ::OutputDevice const * pDev ) const;

    void SetControlFont();
    void SetControlFont( const vcl::Font& rFont );
    vcl::Font GetControlFont() const;
    bool IsControlFont() const;
    void ApplyControlFont(vcl::RenderContext& rRenderContext, const vcl::Font& rDefaultFont);

    void SetControlForeground();
    void SetControlForeground(const Color& rColor);
    const Color& GetControlForeground() const;
    bool IsControlForeground() const;
    void ApplyControlForeground(vcl::RenderContext& rRenderContext, const Color& rDefaultColor);

    void SetControlBackground();
    void SetControlBackground( const Color& rColor );
    const Color& GetControlBackground() const;
    bool IsControlBackground() const;
    void ApplyControlBackground(vcl::RenderContext& rRenderContext, const Color& rDefaultColor);

    void                                SetParentClipMode( ParentClipMode nMode = ParentClipMode::NONE );
    SAL_DLLPRIVATE ParentClipMode       GetParentClipMode() const;

    SAL_DLLPRIVATE void                 SetWindowRegionPixel();
    SAL_DLLPRIVATE void                 SetWindowRegionPixel( const vcl::Region& rRegion );
    vcl::Region                         GetWindowClipRegionPixel() const;
    vcl::Region                         GetPaintRegion() const;
    bool                                IsInPaint() const;
    // while IsInPaint returns true ExpandPaintClipRegion adds the
    // submitted region to the paint clip region so you can
    // paint additional parts of your window if necessary
    void                                ExpandPaintClipRegion( const vcl::Region& rRegion );

    void                                SetParent( vcl::Window* pNewParent );
    vcl::Window*                        GetParent() const;
    // return the dialog we are contained in or NULL if un-contained
    SAL_DLLPRIVATE Dialog*              GetParentDialog() const;
    bool                                IsAncestorOf( const vcl::Window& rWindow ) const;

    void                                Show( bool bVisible = true, ShowFlags nFlags = ShowFlags::NONE );
    void                                Hide() { Show( false ); }
    bool                                IsVisible() const;
    bool                                IsReallyVisible() const;
    bool                                IsReallyShown() const;
    SAL_DLLPRIVATE bool                 IsInInitShow() const;

    void                                Enable( bool bEnable = true, bool bChild = true );
    void                                Disable( bool bChild = true ) { Enable( false, bChild ); }
    bool                                IsEnabled() const;

    void                                EnableInput( bool bEnable = true, bool bChild = true );
    SAL_DLLPRIVATE void                 EnableInput( bool bEnable, const vcl::Window* pExcludeWindow );
    bool                                IsInputEnabled() const;

    /** Override <code>EnableInput</code>. This can be necessary due to other people
        using EnableInput for whole window hierarchies.

        @param bAlways
        sets always enabled flag

        @param bChild
        if true children are recursively set to AlwaysEnableInput
    */
    void                                AlwaysEnableInput( bool bAlways, bool bChild = true );

    /** returns the current AlwaysEnableInput state
    @return
    true if window is in AlwaysEnableInput state
    */
    SAL_DLLPRIVATE bool                 IsAlwaysEnableInput() const;

    /** A window is in modal mode if one of its children or subchildren
        is a running modal window (a modal dialog)

        @returns sal_True if a child or subchild is a running modal window
    */
    bool                                IsInModalMode() const;

    SAL_DLLPRIVATE void                 SetActivateMode( ActivateModeFlags nMode );
    SAL_DLLPRIVATE ActivateModeFlags    GetActivateMode() const;

    void                                ToTop( ToTopFlags nFlags = ToTopFlags::NONE );
    void                                SetZOrder( vcl::Window* pRefWindow, ZOrderFlags nFlags );
    SAL_DLLPRIVATE void                 EnableAlwaysOnTop( bool bEnable = true );
    SAL_DLLPRIVATE bool                 IsAlwaysOnTopEnabled() const;

    virtual void                        setPosSizePixel( tools::Long nX, tools::Long nY,
                                                         tools::Long nWidth, tools::Long nHeight,
                                                         PosSizeFlags nFlags = PosSizeFlags::All );
    virtual void                        SetPosPixel( const Point& rNewPos );
    virtual Point                       GetPosPixel() const;
    virtual void                        SetSizePixel( const Size& rNewSize );
    virtual Size                        GetSizePixel() const;
    virtual void                        SetPosSizePixel( const Point& rNewPos,
                                                         const Size& rNewSize );
    virtual void                        SetOutputSizePixel( const Size& rNewSize );
    bool                                IsDefaultPos() const;
    SAL_DLLPRIVATE bool                 IsDefaultSize() const;
    Point                               GetOffsetPixelFrom(const vcl::Window& rWindow) const;

    // those conversion routines might deliver different results during UI mirroring
    Point                               OutputToScreenPixel( const Point& rPos ) const;
    Point                               ScreenToOutputPixel( const Point& rPos ) const;
    //  the normalized screen methods work independent from UI mirroring
    Point                               OutputToNormalizedScreenPixel( const Point& rPos ) const;
    SAL_DLLPRIVATE Point                NormalizedScreenToOutputPixel( const Point& rPos ) const;
    AbsoluteScreenPixelPoint            OutputToAbsoluteScreenPixel( const Point& rPos ) const;
    Point                               AbsoluteScreenToOutputPixel( const AbsoluteScreenPixelPoint& rPos ) const;
    AbsoluteScreenPixelRectangle        GetDesktopRectPixel() const;
    //  window extents including border and decoration, relative to passed in window
    tools::Rectangle                    GetWindowExtentsRelative(const vcl::Window& rRelativeWindow) const;
    //  window extents including border and decoration, in absolute screen coordinates
    AbsoluteScreenPixelRectangle        GetWindowExtentsAbsolute() const;

    SAL_DLLPRIVATE bool                 IsScrollable() const;
    virtual void                        Scroll( tools::Long nHorzScroll, tools::Long nVertScroll,
                                                ScrollFlags nFlags = ScrollFlags::NONE );
    void                                Scroll( tools::Long nHorzScroll, tools::Long nVertScroll,
                                                const tools::Rectangle& rRect, ScrollFlags nFlags = ScrollFlags::NONE );
    void                                Invalidate( InvalidateFlags nFlags = InvalidateFlags::NONE );
    void                                Invalidate( const tools::Rectangle& rRect, InvalidateFlags nFlags = InvalidateFlags::NONE );
    void                                Invalidate( const vcl::Region& rRegion, InvalidateFlags nFlags = InvalidateFlags::NONE );
    /**
     * Notification about some rectangle of the output device got invalidated.Used for the main
     * document window.
     *
     * @param pRectangle If 0, that means the whole area, otherwise the area in logic coordinates.
     */
    virtual void                        LogicInvalidate(const tools::Rectangle* pRectangle);

    virtual bool                        InvalidateByForeignEditView(EditView* );
    /**
     * Notification about some rectangle of the output device got invalidated. Used for the
     * dialogs and floating windows (e.g. context menu, popup).
     *
     * @param pRectangle If 0, that means the whole area, otherwise the area in pixel coordinates.
     */
    virtual void                        PixelInvalidate(const tools::Rectangle* pRectangle);
    void                                Validate();
    SAL_DLLPRIVATE bool                 HasPaintEvent() const;
    void                                PaintImmediately();

    // toggles new docking support, enabled via toolkit
    void                                EnableDocking( bool bEnable = true );
    // retrieves the single dockingmanager instance
    static DockingManager*              GetDockingManager();

    void                                EnablePaint( bool bEnable );
    bool                                IsPaintEnabled() const;
    void                                SetUpdateMode( bool bUpdate );
    bool                                IsUpdateMode() const;
    void                                SetParentUpdateMode( bool bUpdate );

    void                                GrabFocus();
    bool                                HasFocus() const;
    bool                                HasChildPathFocus( bool bSystemWindow = false ) const;
    bool                                IsActive() const;
    bool                                HasActiveChildFrame() const;
    GetFocusFlags                       GetGetFocusFlags() const;
    void                                GrabFocusToDocument();
    VclPtr<vcl::Window>                 GetFocusedWindow() const;

    /**
     * Set this when you need to act as if the window has focus even if it
     * doesn't.  This is necessary for implementing tab stops inside floating
     * windows, but floating windows don't get focus from the system.
     */
    SAL_DLLPRIVATE void                 SetFakeFocus( bool bFocus );

    bool                                IsCompoundControl() const;

    SAL_DLLPRIVATE static VclPtr<vcl::Window> SaveFocus();
    SAL_DLLPRIVATE static void          EndSaveFocus(const VclPtr<vcl::Window>& xFocusWin);

    void                                LocalStartDrag();
    void                                CaptureMouse();
    void                                ReleaseMouse();
    bool                                IsMouseCaptured() const;

    virtual void                        SetPointer( PointerStyle );
    PointerStyle                        GetPointer() const;
    void                                EnableChildPointerOverwrite( bool bOverwrite );
    void                                SetPointerPosPixel( const Point& rPos );
    Point                               GetPointerPosPixel();
    SAL_DLLPRIVATE Point                GetLastPointerPosPixel();
    /// Similar to SetPointerPosPixel(), but sets the frame data's last mouse position instead.
    void                                SetLastMousePos(const Point& rPos);
    void                                ShowPointer( bool bVisible );
    void                                EnterWait();
    void                                LeaveWait();
    bool                                IsWait() const;

    void                                SetCursor( vcl::Cursor* pCursor );
    vcl::Cursor*                        GetCursor() const;

    void                                SetZoom( const Fraction& rZoom );
    const Fraction&                     GetZoom() const;
    bool                                IsZoom() const;
    tools::Long                                CalcZoom( tools::Long n ) const;

    virtual void                        SetText( const OUString& rStr );
    virtual OUString                    GetText() const;
    // return the actual text displayed
    // this may have e.g. accelerators removed or portions
    // replaced by ellipses
    virtual OUString                    GetDisplayText() const;
    // gets the visible background color. for transparent windows
    // this may be the parent's background color; for controls
    // this may be a child's background color (e.g. ListBox)
    virtual const Wallpaper&            GetDisplayBackground() const;

    void                                SetHelpText( const OUString& rHelpText );
    const OUString&                     GetHelpText() const;

    void                                SetQuickHelpText( const OUString& rHelpText );
    const OUString&                     GetQuickHelpText() const;

    void                                SetHelpId( const OUString& );
    const OUString&                     GetHelpId() const;

    sal_uInt16                          GetChildCount() const;
    vcl::Window*                        GetChild( sal_uInt16 nChild ) const;
    vcl::Window*                        GetWindow( GetWindowType nType ) const;
    bool                                IsChild( const vcl::Window* pWindow ) const;
    bool                                IsWindowOrChild( const vcl::Window* pWindow, bool bSystemWindow = false  ) const;

    /// Add all children to rAllChildren recursively.
    SAL_DLLPRIVATE void                 CollectChildren(::std::vector<vcl::Window *>& rAllChildren );

    virtual void                        ShowFocus(const tools::Rectangle& rRect);
    void                                HideFocus();

    // transparent background for selected or checked items in toolboxes etc.
    void                                DrawSelectionBackground( const tools::Rectangle& rRect, sal_uInt16 highlight, bool bChecked, bool bDrawBorder );

    void                                ShowTracking( const tools::Rectangle& rRect,
                                                      ShowTrackFlags nFlags = ShowTrackFlags::Small );
    void                                HideTracking();
    void                                InvertTracking( const tools::Rectangle& rRect, ShowTrackFlags nFlags );

    void                                StartTracking( StartTrackingFlags nFlags = StartTrackingFlags::NONE );
    void                                EndTracking( TrackingEventFlags nFlags = TrackingEventFlags::NONE );
    bool                                IsTracking() const;

    SAL_DLLPRIVATE void                 StartAutoScroll( StartAutoScrollFlags nFlags );
    SAL_DLLPRIVATE void                 EndAutoScroll();

    bool                                HandleScrollCommand( const CommandEvent& rCmd,
                                                             Scrollable* pHScrl,
                                                             Scrollable* pVScrl );

    virtual const SystemEnvData*        GetSystemData() const;

    // API to set/query the component interfaces
    virtual css::uno::Reference< css::awt::XVclWindowPeer >
                                        GetComponentInterface( bool bCreate = true );

    void                                SetComponentInterface( css::uno::Reference< css::awt::XVclWindowPeer > const & xIFace );

    void                                SetUseFrameData(bool bUseFrameData);

    /// Interface to register for dialog / window tunneling.
    void                                SetLOKNotifier(const vcl::ILibreOfficeKitNotifier* pNotifier, bool bParent = false);
    const vcl::ILibreOfficeKitNotifier* GetLOKNotifier() const;
    vcl::LOKWindowId                    GetLOKWindowId() const;

    /// Find the nearest parent with LOK Notifier; can be itself if this Window has LOK notifier set.
    VclPtr<vcl::Window>                 GetParentWithLOKNotifier();

    /// Indicate that LOK is not going to use this dialog any more.
    void                                ReleaseLOKNotifier();

    /// Find an existing Window based on the LOKWindowId.
    static VclPtr<vcl::Window>          FindLOKWindow(vcl::LOKWindowId nWindowId);

    /// check if LOK Window container is empty
    SAL_DLLPRIVATE static bool          IsLOKWindowsEmpty();

    /// Dumps itself and potentially its children to a property tree, to be written easily to JSON.
    virtual void DumpAsPropertyTree(tools::JsonWriter&);

    /// Use OS specific way to bring user attention to current window
    virtual void FlashWindow() const;

    void SetTaskBarProgress(int nCurrentProgress);
    void SetTaskBarState(VclTaskBarStates eTaskBarState);

    /** @name Accessibility
     */
    ///@{
public:
    rtl::Reference<comphelper::OAccessible> GetAccessible(bool bCreate = true);
    void SetAccessible(const rtl::Reference<comphelper::OAccessible>& rpAccessible);

    vcl::Window*                        GetAccessibleParentWindow() const;
    sal_uInt16                          GetAccessibleChildWindowCount();
    vcl::Window*                        GetAccessibleChildWindow( sal_uInt16 n );

    css::uno::Reference<css::accessibility::XAccessible> GetAccessibleParent() const;
    // Explicitly set an accessible parent (usually not needed)
    void                                SetAccessibleParent(const css::uno::Reference<css::accessibility::XAccessible>& rxParent);

    void                                SetAccessibleRole( sal_uInt16 nRole );
    sal_uInt16                          GetAccessibleRole() const;

    void                                SetAccessibleName( const OUString& rName );
    OUString                            GetAccessibleName() const;

    SAL_DLLPRIVATE void                 SetAccessibleDescription( const OUString& rDescr );
    OUString                            GetAccessibleDescription() const;

    SAL_DLLPRIVATE void                 SetAccessibleRelationLabeledBy( vcl::Window* pLabeledBy );
    vcl::Window*                        GetAccessibleRelationLabeledBy() const;

    SAL_DLLPRIVATE void                 SetAccessibleRelationLabelFor( vcl::Window* pLabelFor );
    vcl::Window*                        GetAccessibleRelationLabelFor() const;

    vcl::Window*                        GetAccessibleRelationMemberOf() const;

    // to avoid sending accessibility events in cases like closing dialogs
    // checks complete parent path
    bool                                IsAccessibilityEventsSuppressed();

    KeyEvent                            GetActivationKey() const;

protected:
    virtual rtl::Reference<comphelper::OAccessible> CreateAccessible();

    // These eventually are supposed to go when everything is converted to .ui
    SAL_DLLPRIVATE vcl::Window*         getLegacyNonLayoutAccessibleRelationMemberOf() const;
    SAL_DLLPRIVATE vcl::Window*         getLegacyNonLayoutAccessibleRelationLabeledBy() const;
    SAL_DLLPRIVATE vcl::Window*         getLegacyNonLayoutAccessibleRelationLabelFor() const;

    // Let Label override the code part of GetAccessibleRelationLabelFor
    virtual vcl::Window*                getAccessibleRelationLabelFor() const;
    virtual sal_uInt16                  getDefaultAccessibleRole() const;
    virtual OUString                    getDefaultAccessibleName() const;

    /*
     * Advisory Sizing - what is a good size for this widget
     *
     * Retrieves the preferred size of a widget ignoring
     * "width-request" and "height-request" properties.
     *
     * Implement this in sub-classes to tell layout
     * the preferred widget size.
     *
     * Use get_preferred_size to retrieve this value
     * cached and mediated via height and width requests
     */
    virtual Size GetOptimalSize() const;
    /// clear OptimalSize cache
    SAL_DLLPRIVATE void InvalidateSizeCache();
private:

    SAL_DLLPRIVATE bool                 ImplIsAccessibleCandidate() const;
    ///@}

    /*
     * Retrieves the preferred size of a widget taking
     * into account the "width-request" and "height-request" properties.
     *
     * Overrides the result of GetOptimalSize to honor the
     * width-request and height-request properties.
     *
     * So the same as get_ungrouped_preferred_size except
     * it ignores groups. A building block of get_preferred_size
     * that access the size cache
     *
     * @see get_preferred_size
     */
    SAL_DLLPRIVATE Size get_ungrouped_preferred_size() const;
public:
    /*  records all DrawText operations within the passed rectangle;
     *  a synchronous paint is sent to achieve this
     */
    void                                RecordLayoutData( vcl::ControlLayoutData* pLayout, const tools::Rectangle& rRect );

    // set and retrieve for Toolkit
    VCLXWindow*                         GetWindowPeer() const;
    void                                SetWindowPeer( css::uno::Reference< css::awt::XVclWindowPeer > const & xPeer, VCLXWindow* pVCLXWindow );

    // remember if it was generated by Toolkit
    SAL_DLLPRIVATE bool                 IsCreatedWithToolkit() const;
    void                                SetCreatedWithToolkit( bool b );

    // Drag and Drop interfaces
    rtl::Reference<DNDListenerContainer> GetDropTarget();
    css::uno::Reference< css::datatransfer::dnd::XDragSource > GetDragSource();

    // Clipboard/Selection interfaces
    css::uno::Reference< css::datatransfer::clipboard::XClipboard > GetClipboard();
    /// Sets a custom clipboard for the window's frame, instead of creating it on-demand using css::datatransfer::clipboard::SystemClipboard.
    void SetClipboard(css::uno::Reference<css::datatransfer::clipboard::XClipboard> const & xClipboard);

    /*
     * Widgets call this to inform their owner container that the widget wants
     * to renegotiate its size. Should be called when a widget has a new size
     * request. e.g. a FixedText Control gets a new label.
     *
     * akin to gtk_widget_queue_resize
     */
    virtual void queue_resize(StateChangedType eReason = StateChangedType::Layout);

    /*
     * Sets the "height-request" property
     *
     * Override for height request of the widget, or -1 if natural request
     * should be used.
     *
     * @see get_preferred_size, set_width_request
     */
    void set_height_request(sal_Int32 nHeightRequest);
    sal_Int32 get_height_request() const;

    /*
     * Sets the "width-request" property
     *
     * Override for width request of the widget, or -1 if natural request
     * should be used.
     *
     * @see get_preferred_size, set_height_request
     */
    void set_width_request(sal_Int32 nWidthRequest);
    sal_Int32 get_width_request() const;

    /*
     * Retrieves the preferred size of a widget taking
     * into account the "width-request" and "height-request" properties.
     *
     * Overrides the result of GetOptimalSize to honor the
     * width-request and height-request properties.
     *
     * @see GetOptimalSize
     *
     * akin to gtk_widget_get_preferred_size
     */
    Size get_preferred_size() const;

    /*
     * How to horizontally align this widget
     */
    SAL_DLLPRIVATE VclAlign get_halign() const;
    SAL_DLLPRIVATE void set_halign(VclAlign eAlign);

    /*
     * How to vertically align this widget
     */
    SAL_DLLPRIVATE VclAlign get_valign() const;
    SAL_DLLPRIVATE void set_valign(VclAlign eAlign);

    /*
     * Whether the widget would like to use any available extra horizontal
     * space.
     */
    bool get_hexpand() const;
    void set_hexpand(bool bExpand);

    /*
     * Whether the widget would like to use any available extra vertical
     * space.
     */
    bool get_vexpand() const;
    void set_vexpand(bool bExpand);

    /*
     * Whether the widget would like to use any available extra space.
     */
    bool get_expand() const;
    void set_expand(bool bExpand);

    /*
     * Whether the widget should receive extra space when the parent grows
     */
    SAL_DLLPRIVATE bool get_fill() const;
    SAL_DLLPRIVATE void set_fill(bool bFill);

    void set_border_width(sal_Int32 nBorderWidth);
    SAL_DLLPRIVATE sal_Int32 get_border_width() const;

    SAL_DLLPRIVATE void set_margin_start(sal_Int32 nWidth);
    SAL_DLLPRIVATE sal_Int32 get_margin_start() const;

    SAL_DLLPRIVATE void set_margin_end(sal_Int32 nWidth);
    SAL_DLLPRIVATE sal_Int32 get_margin_end() const;

    void set_margin_top(sal_Int32 nWidth);
    SAL_DLLPRIVATE sal_Int32 get_margin_top() const;

    SAL_DLLPRIVATE void set_margin_bottom(sal_Int32 nWidth);
    SAL_DLLPRIVATE sal_Int32 get_margin_bottom() const;

    /*
     * How the widget is packed with reference to the start or end of the parent
     */
    SAL_DLLPRIVATE VclPackType get_pack_type() const;
    SAL_DLLPRIVATE void set_pack_type(VclPackType ePackType);

    /*
     * The extra space to put between the widget and its neighbors
     */
    SAL_DLLPRIVATE sal_Int32 get_padding() const;
    SAL_DLLPRIVATE void set_padding(sal_Int32 nPadding);

    /*
     * The number of columns that the widget spans
     */
    SAL_DLLPRIVATE sal_Int32 get_grid_width() const;
    SAL_DLLPRIVATE void set_grid_width(sal_Int32 nCols);

    /*
     * The column number to attach the left side of the widget to
     */
    SAL_DLLPRIVATE sal_Int32 get_grid_left_attach() const;
    SAL_DLLPRIVATE void set_grid_left_attach(sal_Int32 nAttach);

    /*
     * The number of row that the widget spans
     */
    SAL_DLLPRIVATE sal_Int32 get_grid_height() const;
    SAL_DLLPRIVATE void set_grid_height(sal_Int32 nRows);

    /*
     * The row number to attach the top side of the widget to
     */
    SAL_DLLPRIVATE sal_Int32 get_grid_top_attach() const;
    SAL_DLLPRIVATE void set_grid_top_attach(sal_Int32 nAttach);

    /*
     * If true this child appears in a secondary layout group of children
     * e.g. help buttons in a buttonbox
     */
    SAL_DLLPRIVATE bool get_secondary() const;
    SAL_DLLPRIVATE void set_secondary(bool bSecondary);

    /*
     * If true this child is exempted from homogeneous sizing
     * e.g. special button in a buttonbox
     */
    SAL_DLLPRIVATE bool get_non_homogeneous() const;
    SAL_DLLPRIVATE void set_non_homogeneous(bool bNonHomogeneous);

    /*
     * Sets a widget property
     *
     * @return false if property is unknown
     */
    virtual bool set_property(const OUString &rKey, const OUString &rValue);

    /*
     * Sets a font attribute
     *
     * @return false if attribute is unknown
     */
    SAL_DLLPRIVATE bool set_font_attribute(const OUString &rKey, std::u16string_view rValue);

    /*
     * Adds this widget to the xGroup VclSizeGroup
     *
     */
    SAL_DLLPRIVATE void add_to_size_group(const std::shared_ptr<VclSizeGroup>& xGroup);
    SAL_DLLPRIVATE void remove_from_all_size_groups();

    /*
     * add/remove mnemonic label
     */
    SAL_DLLPRIVATE void add_mnemonic_label(FixedText *pLabel);
    SAL_DLLPRIVATE void remove_mnemonic_label(FixedText *pLabel);
    SAL_DLLPRIVATE const std::vector<VclPtr<FixedText> >& list_mnemonic_labels() const;

    /*
     * Move this widget to be the nNewPosition'd child of its parent
     */
    SAL_DLLPRIVATE void reorderWithinParent(sal_uInt16 nNewPosition);

    /**
     * Sets an ID.
     */
    void set_id(const OUString& rID);

    /**
     * Get the ID of the window.
     */
    const OUString& get_id() const;


    //  Native Widget Rendering functions


    // form controls must never use native widgets, this can be toggled here
    void    EnableNativeWidget( bool bEnable = true );
    bool    IsNativeWidgetEnabled() const;

    // a helper method for a Control's Draw method
    void PaintToDevice( ::OutputDevice* pDevice, const Point& rPos );

    //  Keyboard access functions

    /** Query the states of keyboard indicators - Caps Lock, Num Lock and
        Scroll Lock.  Use the following mask to retrieve the state of each
        indicator:

            KeyIndicatorState::CAPS_LOCK
            KeyIndicatorState::NUM_LOCK
            KeyIndicatorState::SCROLL_LOCK
      */
    KeyIndicatorState GetIndicatorState() const;

    void SimulateKeyPress( sal_uInt16 nKeyCode ) const;

    virtual OUString GetSurroundingText() const;
    virtual Selection GetSurroundingTextSelection() const;
    virtual bool DeleteSurroundingText(const Selection& rSelection);

    virtual FactoryFunction GetUITestFactory() const;

    virtual bool IsChart() const { return false; }
    virtual bool IsStarMath() const { return false; }

    SAL_DLLPRIVATE void SetHelpHdl(const Link<vcl::Window&, bool>& rLink);
    SAL_DLLPRIVATE void SetMnemonicActivateHdl(const Link<vcl::Window&, bool>& rLink);
    void SetModalHierarchyHdl(const Link<bool, void>& rLink);
    SAL_DLLPRIVATE void SetDumpAsPropertyTreeHdl(const Link<tools::JsonWriter&, void>& rLink);

    Size                        GetOutputSizePixel() const;
    SAL_DLLPRIVATE tools::Rectangle GetOutputRectPixel() const;

    Point                       LogicToPixel( const Point& rLogicPt ) const;
    Size                        LogicToPixel( const Size& rLogicSize ) const;
    tools::Rectangle            LogicToPixel( const tools::Rectangle& rLogicRect ) const;
    vcl::Region                 LogicToPixel( const vcl::Region& rLogicRegion )const;
    Point                       LogicToPixel( const Point& rLogicPt,
                                              const MapMode& rMapMode ) const;
    Size                        LogicToPixel( const Size& rLogicSize,
                                              const MapMode& rMapMode ) const;
    tools::Rectangle            LogicToPixel( const tools::Rectangle& rLogicRect,
                                              const MapMode& rMapMode ) const;

    Point                       PixelToLogic( const Point& rDevicePt ) const;
    Size                        PixelToLogic( const Size& rDeviceSize ) const;
    tools::Rectangle                   PixelToLogic( const tools::Rectangle& rDeviceRect ) const;
    tools::PolyPolygon          PixelToLogic( const tools::PolyPolygon& rDevicePolyPoly ) const;
    SAL_DLLPRIVATE vcl::Region  PixelToLogic( const vcl::Region& rDeviceRegion ) const;
    Point                       PixelToLogic( const Point& rDevicePt,
                                              const MapMode& rMapMode ) const;
    Size                        PixelToLogic( const Size& rDeviceSize,
                                              const MapMode& rMapMode ) const;
    tools::Rectangle            PixelToLogic( const tools::Rectangle& rDeviceRect,
                                              const MapMode& rMapMode ) const;

    Size                        LogicToLogic( const Size&       rSzSource,
                                              const MapMode*    pMapModeSource,
                                              const MapMode*    pMapModeDest ) const;

    const AllSettings&          GetSettings() const;
    void SetSettings( const AllSettings& rSettings );
    void SetSettings( const AllSettings& rSettings, bool bChild );

    tools::Rectangle            GetTextRect( const tools::Rectangle& rRect,
                                             const OUString& rStr, DrawTextFlags nStyle = DrawTextFlags::WordBreak,
                                             TextRectInfo* pInfo = nullptr,
                                             const vcl::TextLayoutCommon* _pTextLayout = nullptr ) const;
    float                       GetDPIScaleFactor() const;
    tools::Long                 GetOutOffXPixel() const;
    tools::Long                 GetOutOffYPixel() const;

    void                        EnableMapMode( bool bEnable = true );
    bool                        IsMapModeEnabled() const;
    SAL_DLLPRIVATE void         SetMapMode();
    void                        SetMapMode( const MapMode& rNewMapMode );

    // Enabling/disabling RTL only makes sense for OutputDevices that use a mirroring SalGraphicsLayout
    virtual void                EnableRTL( bool bEnable = true);
    bool                        IsRTLEnabled() const;

    void                        SetFont( const vcl::Font& rNewFont );
    const vcl::Font&            GetFont() const;

    /** Width of the text.

        See also GetTextBoundRect() for more explanation + code examples.
    */
    tools::Long                 GetTextWidth( const OUString& rStr, sal_Int32 nIndex = 0, sal_Int32 nLen = -1,
                                    vcl::text::TextLayoutCache const* = nullptr,
                                    SalLayoutGlyphs const*const pLayoutCache = nullptr) const;

    /** Height where any character of the current font fits; in logic coordinates.

        See also GetTextBoundRect() for more explanation + code examples.
    */
    tools::Long                 GetTextHeight() const;
    float                       approximate_digit_width() const;

    void                        SetTextColor( const Color& rColor );
    const Color&                GetTextColor() const;

    void                        SetTextFillColor();
    void                        SetTextFillColor( const Color& rColor );
    Color                       GetTextFillColor() const;
    SAL_DLLPRIVATE bool         IsTextFillColor() const;

    void                        SetTextLineColor();
    void                        SetTextLineColor( const Color& rColor );
    const Color&                GetTextLineColor() const;
    bool                        IsTextLineColor() const;

    SAL_DLLPRIVATE void         SetOverlineColor();
    SAL_DLLPRIVATE void         SetOverlineColor( const Color& rColor );
    SAL_DLLPRIVATE const Color& GetOverlineColor() const;
    SAL_DLLPRIVATE bool         IsOverlineColor() const;

    void                        SetTextAlign( TextAlign eAlign );
    SAL_DLLPRIVATE TextAlign    GetTextAlign() const;

    /** Query the platform layer for control support
     */
    SAL_DLLPRIVATE bool         IsNativeControlSupported( ControlType nType, ControlPart nPart ) const;

    /** Query the native control's actual drawing region (including adornment)
     */
    SAL_DLLPRIVATE bool         GetNativeControlRegion(
                                    ControlType nType,
                                    ControlPart nPart,
                                    const tools::Rectangle& rControlRegion,
                                    ControlState nState,
                                    const ImplControlValue& aValue,
                                    tools::Rectangle &rNativeBoundingRegion,
                                    tools::Rectangle &rNativeContentRegion ) const;
protected:
    SAL_DLLPRIVATE float        approximate_char_width() const;
private:
    SAL_DLLPRIVATE void         ImplEnableRTL(bool bEnable);
};

}

#endif // INCLUDED_VCL_WINDOW_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
