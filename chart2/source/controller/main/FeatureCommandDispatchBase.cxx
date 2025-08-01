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

#include "FeatureCommandDispatchBase.hxx"

using namespace ::com::sun::star;

using ::com::sun::star::uno::Reference;
using ::com::sun::star::uno::Sequence;

namespace chart
{

FeatureCommandDispatchBase::FeatureCommandDispatchBase( const Reference< uno::XComponentContext >& rxContext )
    :CommandDispatch( rxContext )
    ,m_nFeatureId( ChartCommandID::NONE )
{
}

FeatureCommandDispatchBase::~FeatureCommandDispatchBase()
{
}

void FeatureCommandDispatchBase::initialize()
{
    CommandDispatch::initialize();
    describeSupportedFeatures();
}

bool FeatureCommandDispatchBase::isFeatureSupported( const OUString& rCommandURL )
{
    return m_aSupportedFeatures.contains(rCommandURL);
}

void FeatureCommandDispatchBase::fireStatusEvent( const OUString& rURL,
    const Reference< frame::XStatusListener >& xSingleListener /* = 0 */ )
{
    if ( rURL.isEmpty() )
    {
        for (auto const& elem : m_aSupportedFeatures)
        {
            FeatureState aFeatureState( getState(elem.first) );
            fireStatusEventForURL( elem.first, aFeatureState.aState, aFeatureState.bEnabled, xSingleListener );
        }
    }
    else
    {
        FeatureState aFeatureState( getState( rURL ) );
        fireStatusEventForURL( rURL, aFeatureState.aState, aFeatureState.bEnabled, xSingleListener );
    }
}

// XDispatch
void FeatureCommandDispatchBase::dispatch( const util::URL& URL,
    const Sequence< beans::PropertyValue >& Arguments )
{
    OUString aCommand( URL.Complete );
    if ( getState( aCommand ).bEnabled )
    {
        execute( aCommand, Arguments );
    }
}

void FeatureCommandDispatchBase::implDescribeSupportedFeature( const OUString& sCommandURL,
    ChartCommandID nId, sal_Int16 nGroup )
{
    m_aSupportedFeatures[sCommandURL] = { sCommandURL, nGroup, nId };
}

} //  namespace chart

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
