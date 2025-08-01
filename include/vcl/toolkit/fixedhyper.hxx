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

#if !defined(VCL_DLLIMPLEMENTATION) && !defined(TOOLKIT_DLLIMPLEMENTATION) && !defined(VCL_INTERNALS)
#error "don't use this in new code"
#endif

#include <config_options.h>
#include <vcl/dllapi.h>
#include <vcl/toolkit/fixed.hxx>

class UNLESS_MERGELIBS(VCL_DLLPUBLIC) FixedHyperlink final : public FixedText
{
private:
    tools::Long                m_nTextLen;
    PointerStyle        m_aOldPointer;
    Link<FixedHyperlink&,void> m_aClickHdl;
    OUString            m_sURL;

    /** initializes the font (link color and underline).

        Called by the Ctors.
    */
    void                Initialize();

    /** is position X position hitting text */
    SAL_DLLPRIVATE bool ImplIsOverText(Point rPosition) const;

    DECL_DLLPRIVATE_LINK(HandleClick, FixedHyperlink&, void);

    /** overwrites Window::MouseMove().

        Changes the pointer only over the text.
    */
    virtual void        MouseMove( const MouseEvent& rMEvt ) override;

    /** overwrites Window::MouseButtonUp().

        Calls the set link if the mouse is over the text.
    */
    virtual void        MouseButtonUp( const MouseEvent& rMEvt ) override;

    /** overwrites Window::RequestHelp().

        Shows tooltip only if the mouse is over the text.
    */
    virtual void        RequestHelp( const HelpEvent& rHEvt ) override;

public:
    /** ctors

        With WinBits.
    */
    FixedHyperlink( vcl::Window* pParent, WinBits nWinStyle = 0 );

    virtual rtl::Reference<comphelper::OAccessible> CreateAccessible() override;

    /** overwrites Window::GetFocus().

        Changes the color of the text and shows a focus rectangle.
    */
    virtual void        GetFocus() override;

    /** overwrites Window::LoseFocus().

        Changes the color of the text and hides the focus rectangle.
    */
    virtual void        LoseFocus() override;

    /** overwrites Window::KeyInput().

        KEY_RETURN and KEY_SPACE calls the link handler.
    */
    virtual void        KeyInput( const KeyEvent& rKEvt ) override;

    virtual bool        set_property(const OUString &rKey, const OUString &rValue) override;


    /** sets m_aClickHdl with rLink.

        m_aClickHdl is called if the text is clicked.
    */
    void         SetClickHdl( const Link<FixedHyperlink&,void>& rLink ) { m_aClickHdl = rLink; }
    const Link<FixedHyperlink&,void>& GetClickHdl() const { return m_aClickHdl; }

    // ::FixedHyperbaseLink

    /** sets the URL of the hyperlink and uses it as tooltip. */
    void                SetURL(const OUString& rNewURL);

    /** returns the URL of the hyperlink.

        @return
            m_sURL
    */
    const OUString&     GetURL() const { return m_sURL;}

    /** sets new text and recalculates the text length. */
    virtual void        SetText(const OUString& rNewDescription) override;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
