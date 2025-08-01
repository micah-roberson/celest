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

#include <osl/mutex.hxx>
#include <comphelper/diagnose_ex.hxx>
#include <tools/urlobj.hxx>
#include <rtl/uri.hxx>
#include <rtl/ustring.hxx>
#include <sal/log.hxx>
#include <utility>
#include <vcl/svapp.hxx>
#include <vcl/wrkwin.hxx>
#include <unotools/pathoptions.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/propertysequence.hxx>
#include <comphelper/propertyvalue.hxx>
#include <comphelper/sequenceashashmap.hxx>
#include <comphelper/storagehelper.hxx>
#include <comphelper/string.hxx>
#include <cppuhelper/implbase.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <com/sun/star/beans/IllegalTypeException.hpp>
#include <com/sun/star/beans/PropertyAttribute.hpp>
#include <com/sun/star/beans/PropertyExistException.hpp>
#include <com/sun/star/beans/XPropertySetInfo.hpp>
#include <com/sun/star/beans/XPropertyContainer.hpp>
#include <com/sun/star/beans/StringPair.hpp>
#include <com/sun/star/ucb/SimpleFileAccess.hpp>
#include <com/sun/star/util/theMacroExpander.hpp>
#include <com/sun/star/util/theOfficeInstallationDirectories.hpp>
#include <com/sun/star/configuration/theDefaultProvider.hpp>
#include <com/sun/star/document/XTypeDetection.hpp>
#include <com/sun/star/document/DocumentProperties.hpp>
#include <com/sun/star/io/TempFile.hpp>
#include <com/sun/star/sdbc/XResultSet.hpp>
#include <com/sun/star/sdbc/XRow.hpp>
#include <com/sun/star/ucb/ContentCreationException.hpp>
#include <com/sun/star/ucb/NameClash.hpp>
#include <com/sun/star/ucb/NameClashException.hpp>
#include <com/sun/star/ucb/XCommandEnvironment.hpp>
#include <com/sun/star/ucb/XContentAccess.hpp>
#include <com/sun/star/frame/ModuleManager.hpp>
#include <com/sun/star/uno/Exception.hpp>
#include <com/sun/star/task/InteractionHandler.hpp>
#include <com/sun/star/ucb/XProgressHandler.hpp>
#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/frame/XDocumentTemplates.hpp>
#include <com/sun/star/frame/XStorable.hpp>
#include <com/sun/star/lang/Locale.hpp>
#include <com/sun/star/lang/XLocalizable.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/ucb/XContent.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/uno/RuntimeException.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/util/thePathSettings.hpp>

#include <svtools/templatefoldercache.hxx>
#include <unotools/configmgr.hxx>
#include <unotools/ucbhelper.hxx>
#include <i18nlangtag/languagetag.hxx>
#include <ucbhelper/content.hxx>
#include <o3tl/string_view.hxx>

#include <sfx2/sfxresid.hxx>
#include <sfxurlrelocator.hxx>
#include "doctemplateslocal.hxx"
#include <sfx2/docfac.hxx>
#include <sfx2/strings.hrc>
#include <doctempl.hrc>

#include <memory>
#include <vector>

constexpr OUStringLiteral SERVICENAME_TYPEDETECTION = u"com.sun.star.document.TypeDetection";

constexpr OUStringLiteral TEMPLATE_ROOT_URL = u"vnd.sun.star.hier:/templates";
constexpr OUString TITLE = u"Title"_ustr;
constexpr OUString IS_FOLDER = u"IsFolder"_ustr;
constexpr OUString IS_DOCUMENT = u"IsDocument"_ustr;
constexpr OUString TARGET_URL = u"TargetURL"_ustr;
constexpr OUStringLiteral TEMPLATE_VERSION = u"TemplateComponentVersion";
constexpr OUStringLiteral TEMPLATE_VERSION_VALUE = u"2";
constexpr OUStringLiteral TYPE_FOLDER = u"application/vnd.sun.star.hier-folder";
constexpr OUStringLiteral TYPE_LINK = u"application/vnd.sun.star.hier-link";
constexpr OUString TYPE_FSYS_FOLDER = u"application/vnd.sun.staroffice.fsys-folder"_ustr;
constexpr OUStringLiteral TYPE_FSYS_FILE = u"application/vnd.sun.staroffice.fsys-file";

constexpr OUString PROPERTY_DIRLIST = u"DirectoryList"_ustr;
constexpr OUString PROPERTY_NEEDSUPDATE = u"NeedsUpdate"_ustr;
constexpr OUString PROPERTY_TYPE = u"TypeDescription"_ustr;

constexpr OUString TARGET_DIR_URL = u"TargetDirURL"_ustr;
constexpr OUStringLiteral COMMAND_DELETE = u"delete";

constexpr OUString STANDARD_FOLDER = u"standard"_ustr;

#define C_DELIM                 ';'

using namespace ::com::sun::star;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::document;
using namespace ::com::sun::star::io;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::sdbc;
using namespace ::com::sun::star::ucb;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::container;
using namespace ::com::sun::star::util;

using namespace ::ucbhelper;
using namespace ::comphelper;

using ::std::vector;

namespace {

class WaitWindow_Impl : public WorkWindow
{
    tools::Rectangle     maRect;
    OUString      maText;
    static constexpr DrawTextFlags gnTextStyle = DrawTextFlags::Center | DrawTextFlags::VCenter | DrawTextFlags::WordBreak | DrawTextFlags::MultiLine;

public:
    WaitWindow_Impl();
    virtual ~WaitWindow_Impl() override;
    virtual void dispose() override;
    virtual void Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect) override;
};

#define X_OFFSET 15
#define Y_OFFSET 15


struct NamePair_Impl
{
    OUString maShortName;
    OUString maLongName;
};

class DocTemplates_EntryData_Impl;
class GroupData_Impl;

typedef vector< std::unique_ptr<GroupData_Impl> > GroupList_Impl;


class TplTaskEnvironment : public ::cppu::WeakImplHelper< ucb::XCommandEnvironment >
{
    uno::Reference< task::XInteractionHandler >               m_xInteractionHandler;

public:
    explicit TplTaskEnvironment( uno::Reference< task::XInteractionHandler> xInteractionHandler )
                                : m_xInteractionHandler(std::move( xInteractionHandler ))
                            {}

    virtual uno::Reference<task::XInteractionHandler> SAL_CALL getInteractionHandler() override
    { return m_xInteractionHandler; }

    virtual uno::Reference<ucb::XProgressHandler> SAL_CALL    getProgressHandler() override
    { return uno::Reference<ucb::XProgressHandler>(); }
};

class SfxDocTplService : public ::cppu::WeakImplHelper< css::lang::XLocalizable, css::frame::XDocumentTemplates, css::lang::XServiceInfo >
{
public:
    explicit SfxDocTplService( const css::uno::Reference < uno::XComponentContext >& xContext );
    virtual ~SfxDocTplService() override;

    virtual OUString SAL_CALL getImplementationName() override
    {
        return u"com.sun.star.comp.sfx2.DocumentTemplates"_ustr;
    }

    virtual sal_Bool SAL_CALL supportsService(OUString const & ServiceName) override
    {
        return cppu::supportsService(this, ServiceName);
    }

    virtual css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override
    {
        css::uno::Sequence< OUString > aSeq { u"com.sun.star.frame.DocumentTemplates"_ustr };
        return aSeq;
    }


    // --- XLocalizable ---
    void SAL_CALL                   setLocale( const css::lang::Locale & eLocale ) override;
    css::lang::Locale SAL_CALL              getLocale() override;

    // --- XDocumentTemplates ---
    css::uno::Reference< css::ucb::XContent > SAL_CALL  getContent() override;
    sal_Bool SAL_CALL               storeTemplate( const OUString& GroupName,
                                                   const OUString& TemplateName,
                                                   const css::uno::Reference< css::frame::XStorable >& Storable ) override;
    sal_Bool SAL_CALL               addTemplate( const OUString& GroupName,
                                                 const OUString& TemplateName,
                                                 const OUString& SourceURL ) override;
    sal_Bool SAL_CALL               removeTemplate( const OUString& GroupName,
                                                    const OUString& TemplateName ) override;
    sal_Bool SAL_CALL               renameTemplate( const OUString& GroupName,
                                                    const OUString& OldTemplateName,
                                                    const OUString& NewTemplateName ) override;
    sal_Bool SAL_CALL               addGroup( const OUString& GroupName ) override;
    sal_Bool SAL_CALL               removeGroup( const OUString& GroupName ) override;
    sal_Bool SAL_CALL               renameGroup( const OUString& OldGroupName,
                                                 const OUString& NewGroupName ) override;
    void SAL_CALL                   update() override;

private:
    bool                        init() { if ( !mbIsInitialized ) init_Impl(); return mbIsInitialized; }

    void                        doUpdate();

    uno::Reference< XComponentContext >              mxContext;
    rtl::Reference< TplTaskEnvironment >             maCmdEnv;
    uno::Reference< XDocumentProperties>             m_xDocProps;
    uno::Reference< XTypeDetection >                 mxType;

    ::osl::Mutex                maMutex;
    Sequence< OUString >        maTemplateDirs;
    Sequence< OUString >        maInternalTemplateDirs;
    OUString                    maRootURL;
    std::vector< NamePair_Impl > maNames;
    lang::Locale                maLocale;
    Content                     maRootContent;
    bool                        mbIsInitialized : 1;
    bool                        mbLocaleSet     : 1;

    SfxURLRelocator_Impl        maRelocator;

    void                        init_Impl();
    void                        getDefaultLocale();
    void                        getDirList();
    void                        readFolderList();
    bool                        needsUpdate();
    OUString                    getLongName( const OUString& rShortName );
    bool                    setTitleForURL( const OUString& rURL, const OUString& aTitle );
    void                    getTitleFromURL( const OUString& rURL, OUString& aTitle, OUString& aType, bool& bDocHasTitle );

    bool                    addEntry( Content& rParentFolder,
                                          const OUString& rTitle,
                                          const OUString& rTargetURL,
                                          const OUString& rType );

    bool                    createFolder( const OUString& rNewFolderURL,
                                              bool  bCreateParent,
                                              bool  bFsysFolder,
                                              Content   &rNewFolder );

    static bool             CreateNewUniqueFolderWithPrefix( std::u16string_view aPath,
                                                                const OUString& aPrefix,
                                                                OUString& aNewFolderName,
                                                                OUString& aNewFolderURL,
                                                                Content& aNewFolder );
    static OUString         CreateNewUniqueFileWithPrefix( std::u16string_view aPath,
                                                                const OUString& aPrefix,
                                                                std::u16string_view aExt );

    std::vector< beans::StringPair > ReadUINamesForTemplateDir_Impl( std::u16string_view aUserPath );
    bool                    UpdateUINamesForTemplateDir_Impl( std::u16string_view aUserPath,
                                                                  const OUString& aGroupName,
                                                                  const OUString& aNewFolderName );
    bool                    ReplaceUINamesForTemplateDir_Impl( std::u16string_view aUserPath,
                                                                  const OUString& aFsysGroupName,
                                                                  std::u16string_view aOldGroupName,
                                                                  const OUString& aNewGroupName );
    void                    RemoveUINamesForTemplateDir_Impl( std::u16string_view aUserPath,
                                                                  std::u16string_view aGroupName );
    bool                    WriteUINamesForTemplateDir_Impl( std::u16string_view aUserPath,
                                                                const std::vector< beans::StringPair >& aUINames );

    OUString                CreateNewGroupFsys( const OUString& rGroupName, Content& aGroup );

    static bool             removeContent( Content& rContent );
    bool                    removeContent( const OUString& rContentURL );

    bool                    setProperty( Content& rContent,
                                             const OUString& rPropName,
                                             const Any& rPropValue );
    bool                    getProperty( Content& rContent,
                                             const OUString& rPropName,
                                             Any& rPropValue );

    void                        createFromContent( GroupList_Impl& rList,
                                                   Content &rContent,
                                                   bool bHierarchy,
                                                   bool bWriteableContent );
    void                        addHierGroup( GroupList_Impl& rList,
                                              const OUString& rTitle,
                                              const OUString& rOwnURL );
    void                        addFsysGroup( GroupList_Impl& rList,
                                              const OUString& rTitle,
                                              const OUString& rUITitle,
                                              const OUString& rOwnURL,
                                              bool bWriteableGroup );
    void                        removeFromHierarchy( DocTemplates_EntryData_Impl const *pData );
    void                        addToHierarchy( GroupData_Impl const *pGroup,
                                                DocTemplates_EntryData_Impl const *pData );

    void                        removeFromHierarchy( GroupData_Impl const *pGroup );
    void                        addGroupToHierarchy( GroupData_Impl *pGroup );

    void                        updateData( DocTemplates_EntryData_Impl const *pData );

    //See: #i66157# and rhbz#1065807
    //return which template dir the rURL is a subpath of
    OUString                    findParentTemplateDir(const OUString& rURL) const;

    //See: #i66157# and rhbz#1065807
    //return true if rURL is a path (or subpath of) a dir which is not a user path
    //which implies neither it or its contents can be removed
    bool                        isInternalTemplateDir(const OUString& rURL) const;
};


class DocTemplates_EntryData_Impl
{
    OUString            maTitle;
    OUString            maType;
    OUString            maTargetURL;
    OUString            maHierarchyURL;

    bool            mbInHierarchy   : 1;
    bool            mbInUse         : 1;
    bool            mbUpdateType    : 1;
    bool            mbUpdateLink    : 1;

public:
   explicit             DocTemplates_EntryData_Impl( OUString aTitle );

    void                setInUse() { mbInUse = true; }
    void                setHierarchy( bool bInHierarchy ) { mbInHierarchy = bInHierarchy; }
    void                setUpdateLink( bool bUpdateLink ) { mbUpdateLink = bUpdateLink; }
    void                setUpdateType( bool bUpdateType ) { mbUpdateType = bUpdateType; }

    bool                getInUse() const { return mbInUse; }
    bool                getInHierarchy() const { return mbInHierarchy; }
    bool                getUpdateLink() const { return mbUpdateLink; }
    bool                getUpdateType() const { return mbUpdateType; }

    const OUString&     getHierarchyURL() const { return maHierarchyURL; }
    const OUString&     getTargetURL() const { return maTargetURL; }
    const OUString&     getTitle() const { return maTitle; }
    const OUString&     getType() const { return maType; }

    void                setHierarchyURL( const OUString& rURL ) { maHierarchyURL = rURL; }
    void                setTargetURL( const OUString& rURL ) { maTargetURL = rURL; }
    void                setType( const OUString& rType ) { maType = rType; }
};


class GroupData_Impl
{
    std::vector< std::unique_ptr<DocTemplates_EntryData_Impl> > maEntries;
    OUString            maTitle;
    OUString            maHierarchyURL;
    OUString            maTargetURL;
    bool            mbInUse         : 1;
    bool            mbInHierarchy   : 1;

public:
    explicit            GroupData_Impl( OUString aTitle );

    void                setInUse() { mbInUse = true; }
    void                setHierarchy( bool bInHierarchy ) { mbInHierarchy = bInHierarchy; }
    void                setHierarchyURL( const OUString& rURL ) { maHierarchyURL = rURL; }
    void                setTargetURL( const OUString& rURL ) { maTargetURL = rURL; }

    bool            getInUse() const { return mbInUse; }
    bool            getInHierarchy() const { return mbInHierarchy; }
    const OUString&     getHierarchyURL() const { return maHierarchyURL; }
    const OUString&     getTargetURL() const { return maTargetURL; }
    const OUString&     getTitle() const { return maTitle; }

    DocTemplates_EntryData_Impl*     addEntry( const OUString& rTitle,
                                  const OUString& rTargetURL,
                                  const OUString& rType,
                                  const OUString& rHierURL );
    size_t                          count() { return maEntries.size(); }
    DocTemplates_EntryData_Impl*    getEntry( size_t nPos ) { return maEntries[ nPos ].get(); }
};


// private SfxDocTplService_Impl

void SfxDocTplService::init_Impl()
{
    const uno::Reference< uno::XComponentContext >& xContext = ::comphelper::getProcessComponentContext();
    uno::Reference < task::XInteractionHandler > xInteractionHandler(
        task::InteractionHandler::createWithParent(xContext, nullptr), uno::UNO_QUERY_THROW );
    maCmdEnv = new TplTaskEnvironment( xInteractionHandler );

    ::osl::ClearableMutexGuard aGuard( maMutex );
    bool bIsInitialized = false;
    bool bNeedsUpdate   = false;

    if ( !mbLocaleSet )
        getDefaultLocale();

    // convert locale to string
    // set maRootContent to the root of the templates hierarchy. Create the
    // entry if necessary

    maRootURL = TEMPLATE_ROOT_URL + "/" + LanguageTag::convertToBcp47(maLocale);

    const OUString aTemplVersPropName( TEMPLATE_VERSION  );
    const OUString aTemplVers( TEMPLATE_VERSION_VALUE  );
    if ( Content::create( maRootURL, maCmdEnv, comphelper::getProcessComponentContext(), maRootContent ) )
    {
        uno::Any aValue;
        OUString aPropValue;
        if ( getProperty( maRootContent, aTemplVersPropName, aValue )
          && ( aValue >>= aPropValue )
          && aPropValue == aTemplVers )
        {
            bIsInitialized = true;
        }
        else
            removeContent( maRootContent );
    }

    if ( !bIsInitialized )
    {
        if ( createFolder( maRootURL, true, false, maRootContent )
          && setProperty( maRootContent, aTemplVersPropName, uno::Any( aTemplVers ) ) )
            bIsInitialized = true;

        bNeedsUpdate = true;
    }

    if ( bIsInitialized )
    {
        try {
            m_xDocProps.set(document::DocumentProperties::create(
                        ::comphelper::getProcessComponentContext()));
        } catch (uno::RuntimeException const&) {
            TOOLS_WARN_EXCEPTION("sfx.doc", "SfxDocTplService_Impl::init_Impl: cannot create DocumentProperties service:");
        }

        mxType.set( mxContext->getServiceManager()->createInstanceWithContext(SERVICENAME_TYPEDETECTION, mxContext), UNO_QUERY );

        getDirList();
        readFolderList();

        if ( bNeedsUpdate )
        {
            aGuard.clear();
            SolarMutexClearableGuard aSolarGuard;

            VclPtrInstance< WaitWindow_Impl > pWin;
            aSolarGuard.clear();
            {
                osl::MutexGuard anotherGuard(maMutex);
                doUpdate();
            }
            SolarMutexGuard aSecondSolarGuard;

            pWin.disposeAndClear();
        }
        else if ( needsUpdate() )
            // the UI should be shown only on the first update
            doUpdate();
    }
    else
    {
        SAL_WARN( "sfx.doc", "init_Impl(): Could not create root" );
    }

    mbIsInitialized = bIsInitialized;
}


void SfxDocTplService::getDefaultLocale()
{
    if ( !mbLocaleSet )
    {
        ::osl::MutexGuard aGuard( maMutex );
        if ( !mbLocaleSet )
        {
            maLocale = LanguageTag::convertToLocale( utl::ConfigManager::getUILocale(), false);
            mbLocaleSet = true;
        }
    }
}

const char* const TEMPLATE_SHORT_NAMES_ARY[] =
{
    "standard",
    "styles",
    "officorr",
    "offimisc",
    "personal",
    "presnt",
    "draw",
    "l10n",
};

void SfxDocTplService::readFolderList()
{
    SolarMutexGuard aGuard;

    static_assert( SAL_N_ELEMENTS(TEMPLATE_SHORT_NAMES_ARY) == SAL_N_ELEMENTS(TEMPLATE_LONG_NAMES_ARY), "mismatch array lengths" );
    const size_t nCount = std::min(SAL_N_ELEMENTS(TEMPLATE_SHORT_NAMES_ARY), SAL_N_ELEMENTS(TEMPLATE_LONG_NAMES_ARY));
    for (size_t i = 0; i < nCount; ++i)
    {
        NamePair_Impl aPair;
        aPair.maShortName  = OUString::createFromAscii(TEMPLATE_SHORT_NAMES_ARY[i]);
        aPair.maLongName   = SfxResId(TEMPLATE_LONG_NAMES_ARY[i]);

        maNames.push_back( aPair );
    }
}


OUString SfxDocTplService::getLongName( const OUString& rShortName )
{
    OUString         aRet;

    for (auto const & rPair : maNames)
    {
        if ( rPair.maShortName == rShortName )
        {
            aRet = rPair.maLongName;
            break;
        }
    }

    if ( aRet.isEmpty() )
        aRet = rShortName;

    return aRet;
}


void SfxDocTplService::getDirList()
{
    Any      aValue;

    // Get the template dir list
    // TODO/LATER: let use service, register listener
    INetURLObject   aURL;
    OUString    aDirs = SvtPathOptions().GetTemplatePath();
    sal_Int32 nCount = comphelper::string::getTokenCount(aDirs, C_DELIM);

    maTemplateDirs = Sequence< OUString >( nCount );

    uno::Reference< util::XMacroExpander > xExpander = util::theMacroExpander::get(mxContext);

    sal_Int32 nIdx{ 0 };
    for (auto& rTemplateDir : asNonConstRange(maTemplateDirs))
    {
        aURL.SetSmartProtocol( INetProtocol::File );
        aURL.SetURL( o3tl::getToken(aDirs, 0, C_DELIM, nIdx ) );
        rTemplateDir = aURL.GetMainURL( INetURLObject::DecodeMechanism::NONE );

        if (xExpander && rTemplateDir.startsWithIgnoreAsciiCase("vnd.sun.star.expand:", &rTemplateDir))
        {
            rTemplateDir
                = rtl::Uri::decode(rTemplateDir, rtl_UriDecodeStrict, RTL_TEXTENCODING_UTF8);
            rTemplateDir = xExpander->expandMacros( rTemplateDir );
        }
    }

    aValue <<= maTemplateDirs;

    css::uno::Reference< css::util::XPathSettings > xPathSettings =
        css::util::thePathSettings::get(mxContext);

    // load internal paths
    Any aAny = xPathSettings->getPropertyValue( u"Template_internal"_ustr );
    aAny >>= maInternalTemplateDirs;

    for (auto& rInternalTemplateDir : asNonConstRange(maInternalTemplateDirs))
    {
        //expand vnd.sun.star.expand: and remove "..." from them
        //to normalize into the expected url patterns
        maRelocator.makeRelocatableURL(rInternalTemplateDir);
        maRelocator.makeAbsoluteURL(rInternalTemplateDir);
    }

    // Store the template dir list
    setProperty( maRootContent, PROPERTY_DIRLIST, aValue );
}


bool SfxDocTplService::needsUpdate()
{
    bool bNeedsUpdate = true;
    Any      aValue;

    // Get the template dir list
    bool bHasProperty = getProperty( maRootContent, PROPERTY_NEEDSUPDATE, aValue );

    if ( bHasProperty )
        aValue >>= bNeedsUpdate;

    // the old template component also checks this state, but it is initialized from this component
    // so if this component was already updated the old component does not need such an update
    ::svt::TemplateFolderCache aTempCache;
    if ( !bNeedsUpdate )
        bNeedsUpdate = aTempCache.needsUpdate();

    if ( bNeedsUpdate )
        aTempCache.storeState();

    return bNeedsUpdate;
}


bool SfxDocTplService::setTitleForURL( const OUString& rURL, const OUString& aTitle )
{
    if (m_xDocProps.is())
    {
        try
        {
            m_xDocProps->loadFromMedium(rURL, Sequence<PropertyValue>());
            m_xDocProps->setTitle(aTitle);

            uno::Reference< embed::XStorage > xStorage = ::comphelper::OStorageHelper::GetStorageFromURL(
                    rURL, embed::ElementModes::READWRITE);

            uno::Sequence<beans::PropertyValue> medium( comphelper::InitPropertySequence({
                    { "DocumentBaseURL", Any(rURL) },
                    { "URL", Any(rURL) }
                }));

            m_xDocProps->storeToStorage(xStorage, medium);
            return true;
        }
        catch ( Exception& )
        {
        }
    }
    return false;
}


void SfxDocTplService::getTitleFromURL( const OUString& rURL, OUString& aTitle, OUString& aType, bool& bDocHasTitle )
{
    bDocHasTitle = false;

    if (m_xDocProps.is())
    {
        try
        {
            m_xDocProps->loadFromMedium(rURL, Sequence<PropertyValue>());
            aTitle = m_xDocProps->getTitle();
        }
        catch ( Exception& )
        {
        }
    }

    if ( aType.isEmpty() && mxType.is() )
    {
        const OUString aDocType {mxType->queryTypeByURL( rURL )};
        if ( !aDocType.isEmpty() )
            try
            {
                uno::Reference< container::XNameAccess > xTypeDetection( mxType, uno::UNO_QUERY );
                if (xTypeDetection)
                {
                    SequenceAsHashMap aTypeProps( xTypeDetection->getByName( aDocType ) );
                    aType = aTypeProps.getUnpackedValueOrDefault(
                                u"MediaType"_ustr,
                                OUString() );
                }
            }
            catch( uno::Exception& )
            {}
    }

    if ( aTitle.isEmpty() )
    {
        INetURLObject aURL( rURL );
        aURL.CutExtension();
        aTitle = aURL.getName( INetURLObject::LAST_SEGMENT, true,
                               INetURLObject::DecodeMechanism::WithCharset );
    }
    else
        bDocHasTitle = true;
}


bool SfxDocTplService::addEntry( Content& rParentFolder,
                                          const OUString& rTitle,
                                          const OUString& rTargetURL,
                                          const OUString& rType )
{
    bool bAddedEntry = false;

    INetURLObject aLinkObj( rParentFolder.getURL() );
    aLinkObj.insertName( rTitle, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );
    const OUString aLinkURL {aLinkObj.GetMainURL( INetURLObject::DecodeMechanism::NONE )};

    Content aLink;

    if ( ! Content::create( aLinkURL, maCmdEnv, comphelper::getProcessComponentContext(), aLink ) )
    {
        Sequence< Any > aValues{ Any(rTitle), Any(false), Any(rTargetURL) };

        try
        {
            rParentFolder.insertNewContent( TYPE_LINK, { TITLE, IS_FOLDER, TARGET_URL }, aValues, aLink );
            setProperty( aLink, PROPERTY_TYPE, Any( rType ) );
            bAddedEntry = true;
        }
        catch( Exception& )
        {}
    }
    return bAddedEntry;
}


bool SfxDocTplService::createFolder( const OUString& rNewFolderURL,
                                              bool  bCreateParent,
                                              bool  bFsysFolder,
                                              Content   &rNewFolder )
{
    Content         aParent;
    bool        bCreatedFolder = false;
    INetURLObject   aParentURL( rNewFolderURL );
    const OUString aFolderName {aParentURL.getName( INetURLObject::LAST_SEGMENT, true,
                                                    INetURLObject::DecodeMechanism::WithCharset )};

    // compute the parent folder url from the new folder url
    // and remove the final slash, because Content::create doesn't
    // like it
    aParentURL.removeSegment();
    if ( aParentURL.getSegmentCount() >= 1 )
        aParentURL.removeFinalSlash();

    // if the parent exists, we can continue with the creation of the
    // new folder, we have to create the parent otherwise ( as long as
    // bCreateParent is set to true )
    if ( Content::create( aParentURL.GetMainURL( INetURLObject::DecodeMechanism::NONE ), maCmdEnv, comphelper::getProcessComponentContext(), aParent ) )
    {
        try
        {
            Sequence< Any > aValues{ Any(aFolderName), Any(true) };
            OUString aType;

            if ( bFsysFolder )
                aType = TYPE_FSYS_FOLDER;
            else
                aType = TYPE_FOLDER;

            aParent.insertNewContent( aType, { TITLE, IS_FOLDER }, aValues, rNewFolder );
            bCreatedFolder = true;
        }
        catch( Exception const & )
        {
            TOOLS_WARN_EXCEPTION( "sfx.doc", "createFolder(): Could not create new folder" );
        }
    }
    else if ( bCreateParent )
    {
        // if the parent doesn't exists and bCreateParent is set to true,
        // we try to create the parent and if this was successful, we
        // try to create the new folder again ( but this time, we set
        // bCreateParent to false to avoid endless recursions )
        if ( ( aParentURL.getSegmentCount() >= 1 ) &&
               createFolder( aParentURL.GetMainURL( INetURLObject::DecodeMechanism::NONE ), bCreateParent, bFsysFolder, aParent ) )
        {
            bCreatedFolder = createFolder( rNewFolderURL, false, bFsysFolder, rNewFolder );
        }
    }

    return bCreatedFolder;
}


bool SfxDocTplService::CreateNewUniqueFolderWithPrefix( std::u16string_view aPath,
                                                                const OUString& aPrefix,
                                                                OUString& aNewFolderName,
                                                                OUString& aNewFolderURL,
                                                                Content& aNewFolder )
{
    bool bCreated = false;
    INetURLObject aDirPath( aPath );

    Content aParent;
    uno::Reference< XCommandEnvironment > aQuietEnv;
    if ( Content::create( aDirPath.GetMainURL( INetURLObject::DecodeMechanism::NONE ), aQuietEnv, comphelper::getProcessComponentContext(), aParent ) )
    {
        for ( sal_Int32 nInd = 0; nInd < 32000; nInd++ )
        {
            OUString aTryName = aPrefix;
            if ( nInd )
                aTryName += OUString::number( nInd );

            try
            {
                Sequence< Any > aValues{ Any(aTryName), Any(true) };
                bCreated = aParent.insertNewContent( TYPE_FSYS_FOLDER, { TITLE, IS_FOLDER }, aValues, aNewFolder );
            }
            catch( ucb::NameClashException& )
            {
                // if there is already an element, retry
            }
            catch( Exception& )
            {
                INetURLObject aObjPath( aDirPath );
                aObjPath.insertName( aTryName, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );
                // if there is already an element, retry
                // if there was another error, do not try any more
                if ( !::utl::UCBContentHelper::Exists( aObjPath.GetMainURL( INetURLObject::DecodeMechanism::NONE ) ) )
                    break;
            }

            if ( bCreated )
            {
                aNewFolderName = aTryName;
                aNewFolderURL = aNewFolder.get()->getIdentifier()->getContentIdentifier();
                break;
            }
        }
    }

    return bCreated;
}


OUString SfxDocTplService::CreateNewUniqueFileWithPrefix( std::u16string_view aPath,
                                                                        const OUString& aPrefix,
                                                                        std::u16string_view aExt )
{
    OUString aNewFileURL;
    INetURLObject aDirPath( aPath );

    Content aParent;

    uno::Reference< XCommandEnvironment > aQuietEnv;
    if ( Content::create( aDirPath.GetMainURL( INetURLObject::DecodeMechanism::NONE ), aQuietEnv, comphelper::getProcessComponentContext(), aParent ) )
    {
        for ( sal_Int32 nInd = 0; nInd < 32000; nInd++ )
        {
            Content aNewFile;
            bool bCreated = false;
            OUString aTryName = aPrefix;
            if ( nInd )
                aTryName += OUString::number( nInd );
            if ( aExt.empty() || aExt[0] != '.' )
                aTryName += ".";
            aTryName += aExt;

            try
            {
                Sequence< Any > aValues{ Any(aTryName), Any(true) };
                bCreated = aParent.insertNewContent( TYPE_FSYS_FILE, { TITLE, IS_DOCUMENT }, aValues, aNewFile );
            }
            catch( ucb::NameClashException& )
            {
                // if there is already an element, retry
            }
            catch( Exception& )
            {
                INetURLObject aObjPath( aPath );
                aObjPath.insertName( aTryName, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );
                // if there is already an element, retry
                // if there was another error, do not try any more
                if ( !::utl::UCBContentHelper::Exists( aObjPath.GetMainURL( INetURLObject::DecodeMechanism::NONE ) ) )
                    break;
            }

            if ( bCreated )
            {
                aNewFileURL = aNewFile.get()->getIdentifier()->getContentIdentifier();
                break;
            }
        }
    }

    return aNewFileURL;
}


bool SfxDocTplService::removeContent( Content& rContent )
{
    bool bRemoved = false;
    try
    {
        Any aArg( true );

        rContent.executeCommand( COMMAND_DELETE, aArg );
        bRemoved = true;
    }
    catch ( RuntimeException& ) {}
    catch ( Exception& ) {}

    return bRemoved;
}


bool SfxDocTplService::removeContent( const OUString& rContentURL )
{
    Content aContent;

    if ( Content::create( rContentURL, maCmdEnv, comphelper::getProcessComponentContext(), aContent ) )
        return removeContent( aContent );
    return false;
}


bool SfxDocTplService::setProperty( Content& rContent,
                                             const OUString& rPropName,
                                             const Any& rPropValue )
{
    bool bPropertySet = false;

    // Store the property
    try
    {
        Any aPropValue( rPropValue );
        uno::Reference< XPropertySetInfo > aPropInfo = rContent.getProperties();

        // check, whether or not the property exists, create it, when not
        if ( !aPropInfo.is() || !aPropInfo->hasPropertyByName( rPropName ) )
        {
            uno::Reference< XPropertyContainer > xProperties( rContent.get(), UNO_QUERY );
            if ( xProperties.is() )
            {
                try
                {
                    xProperties->addProperty( rPropName, PropertyAttribute::MAYBEVOID, rPropValue );
                }
                catch( PropertyExistException& ) {}
                catch( IllegalTypeException& ) {
                    TOOLS_WARN_EXCEPTION( "sfx.doc", "" );
                }
                catch( IllegalArgumentException& ) {
                    TOOLS_WARN_EXCEPTION( "sfx.doc", "" );
                }
            }
        }

        // To ensure a reloctable office installation, the path to the
        // office installation directory must never be stored directly.
        if ( SfxURLRelocator_Impl::propertyCanContainOfficeDir( rPropName ) )
        {
            OUString aValue;
            if ( rPropValue >>= aValue )
            {
                maRelocator.makeRelocatableURL( aValue );
                aPropValue <<= aValue;
            }
            else
            {
                Sequence< OUString > aValues;
                if ( rPropValue >>= aValues )
                {
                    for ( auto& rValue : asNonConstRange(aValues) )
                    {
                        maRelocator.makeRelocatableURL( rValue );
                    }
                    aPropValue <<= aValues;
                }
                else
                {
                    OSL_FAIL( "Unsupported property value type" );
                }
            }
        }

        // now set the property

        rContent.setPropertyValue( rPropName, aPropValue );
        bPropertySet = true;
    }
    catch ( RuntimeException& ) {}
    catch ( Exception& ) {}

    return bPropertySet;
}


bool SfxDocTplService::getProperty(Content& rContent, const OUString& rPropName, Any& rPropValue)
{
    bool bGotProperty = false;

    // Get the property
    try
    {
        uno::Reference< XPropertySetInfo > aPropInfo = rContent.getProperties();

        // check, whether or not the property exists
        if ( !aPropInfo.is() || !aPropInfo->hasPropertyByName( rPropName ) )
        {
            return false;
        }

        // now get the property

        rPropValue = rContent.getPropertyValue( rPropName );

        // To ensure a reloctable office installation, the path to the
        // office installation directory must never be stored directly.
        if ( SfxURLRelocator_Impl::propertyCanContainOfficeDir( rPropName ) )
        {
            OUString aValue;
            if ( rPropValue >>= aValue )
            {
                maRelocator.makeAbsoluteURL( aValue );
                rPropValue <<= aValue;
            }
            else
            {
                Sequence< OUString > aValues;
                if ( rPropValue >>= aValues )
                {
                    for ( auto& rValue : asNonConstRange(aValues) )
                    {
                        maRelocator.makeAbsoluteURL( rValue );
                    }
                    rPropValue <<= aValues;
                }
                else
                {
                    OSL_FAIL( "Unsupported property value type" );
                }
            }
        }

        bGotProperty = true;
    }
    catch ( RuntimeException& ) {}
    catch ( Exception& ) {}

    return bGotProperty;
}

SfxDocTplService::SfxDocTplService( const uno::Reference< XComponentContext > & xContext )
    : mxContext(xContext), mbIsInitialized(false), mbLocaleSet(false), maRelocator(xContext)
{
}


SfxDocTplService::~SfxDocTplService()
{
    ::osl::MutexGuard aGuard( maMutex );
    maNames.clear();
}


lang::Locale SfxDocTplService::getLocale()
{
    ::osl::MutexGuard aGuard( maMutex );

    if ( !mbLocaleSet )
        getDefaultLocale();

    return maLocale;
}


void SfxDocTplService::setLocale( const lang::Locale &rLocale )
{
    ::osl::MutexGuard aGuard( maMutex );

    if ( mbLocaleSet && (
         ( maLocale.Language != rLocale.Language ) ||
         ( maLocale.Country  != rLocale.Country  ) ||
         ( maLocale.Variant  != rLocale.Variant  ) ) )
        mbIsInitialized = false;

    maLocale    = rLocale;
    mbLocaleSet = true;
}


void SfxDocTplService::update()
{
    if (!init())
        return;

    doUpdate();
}


void SfxDocTplService::doUpdate()
{
    ::osl::MutexGuard aGuard( maMutex );

    const OUString aPropName( PROPERTY_NEEDSUPDATE );
    Any      aValue;

    aValue <<= true;
    setProperty( maRootContent, aPropName, aValue );

    GroupList_Impl  aGroupList;

    // get the entries from the hierarchy
    createFromContent( aGroupList, maRootContent, true, false );

    // get the entries from the template directories
    sal_Int32   nCountDir = maTemplateDirs.getLength();
    const OUString* pDirs = maTemplateDirs.getConstArray();
    Content     aDirContent;

    // the last directory in the list must be writable
    bool bWriteableDirectory = true;

    // the target folder might not exist, for this reason no interaction handler should be used
    uno::Reference< XCommandEnvironment > aQuietEnv;

    while ( nCountDir )
    {
        nCountDir--;
        if ( Content::create( pDirs[ nCountDir ], aQuietEnv, comphelper::getProcessComponentContext(), aDirContent ) )
        {
            createFromContent( aGroupList, aDirContent, false, bWriteableDirectory );
        }

        bWriteableDirectory = false;
    }

    // now check the list
    for(std::unique_ptr<GroupData_Impl>& pGroup : aGroupList)
    {
        if ( pGroup->getInUse() )
        {
            if ( pGroup->getInHierarchy() )
            {
                Content aGroup;
                if ( Content::create( pGroup->getHierarchyURL(), maCmdEnv, comphelper::getProcessComponentContext(), aGroup ) )
                    setProperty( aGroup,
                                 TARGET_DIR_URL,
                                 Any( pGroup->getTargetURL() ) );

                size_t nCount = pGroup->count();
                for ( size_t i=0; i<nCount; i++ )
                {
                    DocTemplates_EntryData_Impl *pData = pGroup->getEntry( i );
                    if ( ! pData->getInUse() )
                    {
                        if ( pData->getInHierarchy() )
                            removeFromHierarchy( pData ); // delete entry in hierarchy
                        else
                            addToHierarchy( pGroup.get(), pData ); // add entry to hierarchy
                    }
                    else if ( pData->getUpdateType() ||
                              pData->getUpdateLink() )
                    {
                        updateData( pData );
                    }
                }
            }
            else
            {
                addGroupToHierarchy( pGroup.get() ); // add group to hierarchy
            }
        }
        else
            removeFromHierarchy( pGroup.get() ); // delete group from hierarchy
    }
    aGroupList.clear();

    aValue <<= false;
    setProperty( maRootContent, aPropName, aValue );
}


std::vector< beans::StringPair > SfxDocTplService::ReadUINamesForTemplateDir_Impl( std::u16string_view aUserPath )
{
    INetURLObject aLocObj( aUserPath );
    aLocObj.insertName( u"groupuinames.xml", false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );
    Content aLocContent;

    // TODO/LATER: Use hashmap in future
    std::vector< beans::StringPair > aUINames;
    if ( Content::create( aLocObj.GetMainURL( INetURLObject::DecodeMechanism::NONE ), uno::Reference < ucb::XCommandEnvironment >(), comphelper::getProcessComponentContext(), aLocContent ) )
    {
        try
        {
            uno::Reference< io::XInputStream > xLocStream = aLocContent.openStream();
            if ( xLocStream.is() )
                aUINames = DocTemplLocaleHelper::ReadGroupLocalizationSequence( xLocStream, mxContext );
        }
        catch( uno::Exception& )
        {}
    }

    return aUINames;
}


bool SfxDocTplService::UpdateUINamesForTemplateDir_Impl( std::u16string_view aUserPath,
                                                                  const OUString& aGroupName,
                                                                  const OUString& aNewFolderName )
{
    std::vector< beans::StringPair > aUINames = ReadUINamesForTemplateDir_Impl( aUserPath );
    sal_Int32 nLen = aUINames.size();

    // it is possible that the name is used already, but it should be checked before
    for ( sal_Int32 nInd = 0; nInd < nLen; nInd++ )
        if ( aUINames[nInd].First == aNewFolderName )
            return false;

    aUINames.resize( ++nLen );
    aUINames[nLen-1].First = aNewFolderName;
    aUINames[nLen-1].Second = aGroupName;

    return WriteUINamesForTemplateDir_Impl( aUserPath, aUINames );
}


bool SfxDocTplService::ReplaceUINamesForTemplateDir_Impl( std::u16string_view aUserPath,
                                                                  const OUString& aDefaultFsysGroupName,
                                                                  std::u16string_view aOldGroupName,
                                                                  const OUString& aNewGroupName )
{
    std::vector< beans::StringPair > aUINames = ReadUINamesForTemplateDir_Impl( aUserPath );
    sal_Int32 nLen = aUINames.size();

    bool bChanged = false;
    for ( sal_Int32 nInd = 0; nInd < nLen; nInd++ )
        if ( aUINames[nInd].Second == aOldGroupName )
        {
            aUINames[nInd].Second = aNewGroupName;
            bChanged = true;
        }

    if ( !bChanged )
    {
        aUINames.resize( ++nLen );
        aUINames[nLen-1].First = aDefaultFsysGroupName;
        aUINames[nLen-1].Second = aNewGroupName;
    }
    return WriteUINamesForTemplateDir_Impl( aUserPath, aUINames );
}


void SfxDocTplService::RemoveUINamesForTemplateDir_Impl( std::u16string_view aUserPath,
                                                                  std::u16string_view aGroupName )
{
    std::vector< beans::StringPair > aUINames = ReadUINamesForTemplateDir_Impl( aUserPath );
    sal_Int32 nLen = aUINames.size();
    std::vector< beans::StringPair > aNewUINames( nLen );
    sal_Int32 nNewLen = 0;

    bool bChanged = false;
    for ( sal_Int32 nInd = 0; nInd < nLen; nInd++ )
        if ( aUINames[nInd].Second == aGroupName )
            bChanged = true;
        else
        {
            nNewLen++;
            aNewUINames[nNewLen-1].First = aUINames[nInd].First;
            aNewUINames[nNewLen-1].Second = aUINames[nInd].Second;
        }

    aNewUINames.resize( nNewLen );

    if (bChanged)
        WriteUINamesForTemplateDir_Impl( aUserPath, aNewUINames );
}


bool SfxDocTplService::WriteUINamesForTemplateDir_Impl( std::u16string_view aUserPath,
                                                             const std::vector< beans::StringPair >& aUINames )
{
    bool bResult = false;
    try {
        uno::Reference< io::XTempFile > xTempFile(
                io::TempFile::create(mxContext),
                uno::UNO_SET_THROW );

        uno::Reference< io::XOutputStream > xOutStream = xTempFile->getOutputStream();
        if ( !xOutStream.is() )
            throw uno::RuntimeException();

        DocTemplLocaleHelper::WriteGroupLocalizationSequence( xOutStream, aUINames, mxContext);
        try {
            // the SAX writer might close the stream
            xOutStream->closeOutput();
        } catch( uno::Exception& )
        {}

        Content aTargetContent( OUString(aUserPath), maCmdEnv, comphelper::getProcessComponentContext() );
        Content aSourceContent( xTempFile->getUri(), maCmdEnv, comphelper::getProcessComponentContext() );
        aTargetContent.transferContent( aSourceContent,
                                        InsertOperation::Copy,
                                        u"groupuinames.xml"_ustr,
                                        ucb::NameClash::OVERWRITE,
                                        u"text/xml"_ustr );

        bResult = true;
    }
    catch ( uno::Exception& )
    {
        TOOLS_WARN_EXCEPTION("sfx.doc", "");
    }

    return bResult;
}


OUString SfxDocTplService::CreateNewGroupFsys( const OUString& rGroupName, Content& aGroup )
{
    OUString aResultURL;

    if ( maTemplateDirs.hasElements() )
    {
        OUString aTargetPath = maTemplateDirs[ maTemplateDirs.getLength() - 1 ];

        // create a new folder with the given name
        Content aNewFolder;
        OUString aNewFolderName;

        // the Fsys name instead of GroupName should be used, the groupuinames must be added also
        if ( !CreateNewUniqueFolderWithPrefix( aTargetPath,
                                                rGroupName,
                                                aNewFolderName,
                                                aResultURL,
                                                aNewFolder )
          && !CreateNewUniqueFolderWithPrefix( aTargetPath,
                                                u"UserGroup"_ustr,
                                                aNewFolderName,
                                                aResultURL,
                                                aNewFolder ) )

            return OUString();

        if ( !UpdateUINamesForTemplateDir_Impl( aTargetPath, rGroupName, aNewFolderName ) )
        {
            // we could not create the groupuinames for the folder, so we delete the group in
            // the folder and return
            removeContent( aNewFolder );
            return OUString();
        }

        // Now set the target url for this group and we are done
        Any aValue( aResultURL );

        if ( ! setProperty( aGroup, TARGET_DIR_URL, aValue ) )
        {
            removeContent( aNewFolder );
            return OUString();
        }
    }

    return aResultURL;
}


sal_Bool SfxDocTplService::addGroup( const OUString& rGroupName )
{
    if (!init())
        return false;

    ::osl::MutexGuard aGuard( maMutex );

    // Check, whether or not there is a group with this name
    Content      aNewGroup;
    OUString        aNewGroupURL;
    INetURLObject   aNewGroupObj( maRootURL );

    aNewGroupObj.insertName( rGroupName, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );

    aNewGroupURL = aNewGroupObj.GetMainURL( INetURLObject::DecodeMechanism::NONE );

    if ( Content::create( aNewGroupURL, maCmdEnv, comphelper::getProcessComponentContext(), aNewGroup ) ||
         ! createFolder( aNewGroupURL, false, false, aNewGroup ) )
    {
        // if there already was a group with this name or the new group
        // could not be created, we return here
        return false;
    }

    // Get the user template path entry ( new group will always
    // be added in the user template path )
    sal_Int32   nIndex;
    OUString    aUserPath;

    nIndex = maTemplateDirs.getLength();
    if ( nIndex )
        nIndex--;
    else
        return false;   // We don't know where to add the group

    aUserPath = maTemplateDirs[ nIndex ];

    // create a new folder with the given name
    Content      aNewFolder;
    OUString        aNewFolderName;
    OUString        aNewFolderURL;

    // the Fsys name instead of GroupName should be used, the groupuinames must be added also
    if ( !CreateNewUniqueFolderWithPrefix( aUserPath,
                                            rGroupName,
                                            aNewFolderName,
                                            aNewFolderURL,
                                            aNewFolder )
      && !CreateNewUniqueFolderWithPrefix( aUserPath,
                                            u"UserGroup"_ustr,
                                            aNewFolderName,
                                            aNewFolderURL,
                                            aNewFolder ) )
    {
        // we could not create the folder, so we delete the group in the
        // hierarchy and return
        removeContent( aNewGroup );
        return false;
    }

    if ( !UpdateUINamesForTemplateDir_Impl( aUserPath, rGroupName, aNewFolderName ) )
    {
        // we could not create the groupuinames for the folder, so we delete the group in the
        // hierarchy, the folder and return
        removeContent( aNewGroup );
        removeContent( aNewFolder );
        return false;
    }

    // Now set the target url for this group and we are done
    Any aValue( aNewFolderURL );

    if ( ! setProperty( aNewGroup, TARGET_DIR_URL, aValue ) )
    {
        removeContent( aNewGroup );
        removeContent( aNewFolder );
        return false;
    }

    return true;
}


sal_Bool SfxDocTplService::removeGroup( const OUString& rGroupName )
{
    // remove all the elements that have the prefix aTargetURL
    // if the group does not have other elements remove it

    if (!init())
        return false;

    ::osl::MutexGuard aGuard( maMutex );

    bool bResult = false;

    // create the group url
    INetURLObject aGroupObj( maRootURL );
    aGroupObj.insertName( rGroupName, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );

    // Get the target url
    Content  aGroup;
    const OUString aGroupURL = aGroupObj.GetMainURL( INetURLObject::DecodeMechanism::NONE );

    if ( Content::create( aGroupURL, maCmdEnv, comphelper::getProcessComponentContext(), aGroup ) )
    {
        const OUString aPropName( TARGET_DIR_URL  );
        Any      aValue;

        OUString    aGroupTargetURL;
        if ( getProperty( aGroup, aPropName, aValue ) )
            aValue >>= aGroupTargetURL;

        if ( aGroupTargetURL.isEmpty() )
            return false; // nothing is allowed to be removed

        if ( !maTemplateDirs.hasElements() )
            return false;

        // check that the fs location is in writable folder and this is not a "My templates" folder
        INetURLObject aGroupParentFolder( aGroupTargetURL );
        if (!aGroupParentFolder.removeSegment())
            return false;

        OUString aGeneralTempPath = findParentTemplateDir(
            aGroupParentFolder.GetMainURL(INetURLObject::DecodeMechanism::NONE));

        if (aGeneralTempPath.isEmpty())
            return false;

        // now get the content of the Group
        uno::Reference< XResultSet > xResultSet;
        Sequence< OUString > aProps { TARGET_URL };

        try
        {
            xResultSet = aGroup.createCursor( aProps, INCLUDE_DOCUMENTS_ONLY );

            if ( xResultSet.is() )
            {
                bool bHasNonRemovable = false;
                bool bHasShared = false;

                uno::Reference< XContentAccess > xContentAccess( xResultSet, UNO_QUERY_THROW );
                uno::Reference< XRow > xRow( xResultSet, UNO_QUERY_THROW );

                while ( xResultSet->next() )
                {
                    OUString aTemplTargetURL( xRow->getString( 1 ) );
                    OUString aHierURL = xContentAccess->queryContentIdentifierString();

                    if ( ::utl::UCBContentHelper::IsSubPath( aGroupTargetURL, aTemplTargetURL ) )
                    {
                        // this is a user template, and it can be removed
                        if ( removeContent( aTemplTargetURL ) )
                            removeContent( aHierURL );
                        else
                            bHasNonRemovable = true;
                    }
                    else
                        bHasShared = true;
                }

                if ( !bHasNonRemovable && !bHasShared )
                {
                    if ( removeContent( aGroupTargetURL )
                      || !::utl::UCBContentHelper::Exists( aGroupTargetURL ) )
                    {
                        removeContent( aGroupURL );
                        RemoveUINamesForTemplateDir_Impl( aGeneralTempPath, rGroupName );
                        bResult = true; // the operation is successful only if the whole group is removed
                    }
                }
                else if ( !bHasNonRemovable )
                {
                    if ( removeContent( aGroupTargetURL )
                      || !::utl::UCBContentHelper::Exists( aGroupTargetURL ) )
                    {
                        RemoveUINamesForTemplateDir_Impl( aGeneralTempPath, rGroupName );
                        setProperty( aGroup, aPropName, uno::Any( OUString() ) );
                    }
                }
            }
        }
        catch ( Exception& ) {}
    }

    return bResult;
}


sal_Bool SfxDocTplService::renameGroup( const OUString& rOldName,
                                    const OUString& rNewName )
{
    if ( rOldName == rNewName )
        return true;

    if (!init())
        return false;

    ::osl::MutexGuard aGuard( maMutex );

    // create the group url
    Content         aGroup;
    INetURLObject   aGroupObj( maRootURL );
    aGroupObj.insertName( rNewName, false,
                          INetURLObject::LAST_SEGMENT,
                          INetURLObject::EncodeMechanism::All );
    OUString        aGroupURL = aGroupObj.GetMainURL( INetURLObject::DecodeMechanism::NONE );

    // Check, if there is a group with the new name, return false
    // if there is one.
    if ( Content::create( aGroupURL, maCmdEnv, comphelper::getProcessComponentContext(), aGroup ) )
        return false;

    aGroupObj.removeSegment();
    aGroupObj.insertName( rOldName, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );
    aGroupURL = aGroupObj.GetMainURL( INetURLObject::DecodeMechanism::NONE );

    // When there is no group with the old name, we can't rename it
    if ( ! Content::create( aGroupURL, maCmdEnv, comphelper::getProcessComponentContext(), aGroup ) )
        return false;

    OUString aGroupTargetURL;
    // there is no need to check whether target dir url is in target path, since if the target path is changed
    // the target dir url should be already generated new
    Any      aValue;
    if ( getProperty( aGroup, TARGET_DIR_URL, aValue ) )
        aValue >>= aGroupTargetURL;

    if ( aGroupTargetURL.isEmpty() )
        return false;

    if ( !maTemplateDirs.hasElements() )
        return false;

    // check that the fs location is in writable folder and this is not a "My templates" folder
    INetURLObject aGroupParentFolder( aGroupTargetURL );
    if (!aGroupParentFolder.removeSegment() ||
        isInternalTemplateDir(aGroupParentFolder.GetMainURL(INetURLObject::DecodeMechanism::NONE)))
    {
        return false;
    }

    // check that the group can be renamed ( all the contents must be in target location )
    bool bCanBeRenamed = false;
    try
    {
        uno::Reference< XResultSet > xResultSet;
        Sequence< OUString > aProps { TARGET_URL };
        xResultSet = aGroup.createCursor( aProps, INCLUDE_DOCUMENTS_ONLY );

        if ( xResultSet.is() )
        {
            uno::Reference< XContentAccess > xContentAccess( xResultSet, UNO_QUERY_THROW );
            uno::Reference< XRow > xRow( xResultSet, UNO_QUERY_THROW );

            while ( xResultSet->next() )
            {
                if ( !::utl::UCBContentHelper::IsSubPath( aGroupTargetURL, xRow->getString( 1 ) ) )
                    throw uno::Exception(u"not sub path"_ustr, nullptr);
            }

            bCanBeRenamed = true;
        }
    }
    catch ( Exception& ) {}

    if ( bCanBeRenamed )
    {
        INetURLObject aGroupTargetObj( aGroupTargetURL );
        const OUString aFsysName = aGroupTargetObj.getName( INetURLObject::LAST_SEGMENT, true, INetURLObject::DecodeMechanism::WithCharset );

        if ( aGroupTargetObj.removeSegment()
          && ReplaceUINamesForTemplateDir_Impl( aGroupTargetObj.GetMainURL( INetURLObject::DecodeMechanism::NONE ),
                                                  aFsysName,
                                                rOldName,
                                                rNewName ) )
        {
            // rename the group in the hierarchy
            Any aTitleValue;
            aTitleValue <<= rNewName;

            return setProperty( aGroup, TITLE, aTitleValue );
        }
    }

    return false;
}


sal_Bool SfxDocTplService::storeTemplate( const OUString& rGroupName,
                                               const OUString& rTemplateName,
                                               const uno::Reference< frame::XStorable >& rStorable )
{
    if (!init())
        return false;

    ::osl::MutexGuard aGuard( maMutex );

    // Check, whether or not there is a group with this name
    // Return false, if there is no group with the given name
    Content         aGroup, aTemplateToRemove;
    INetURLObject   aGroupObj( maRootURL );
    bool        bRemoveOldTemplateContent = false;

    aGroupObj.insertName( rGroupName, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );
    const OUString aGroupURL {aGroupObj.GetMainURL( INetURLObject::DecodeMechanism::NONE )};

    if ( ! Content::create( aGroupURL, maCmdEnv, comphelper::getProcessComponentContext(), aGroup ) )
        return false;

    OUString aGroupTargetURL;
    Any      aValue;
    if ( getProperty( aGroup, TARGET_DIR_URL, aValue ) )
        aValue >>= aGroupTargetURL;


    // Check, if there's a template with the given name in this group
    // the target template should be overwritten if it is imported by user
    // in case the template is installed by office installation of by an add-in
    // it can not be replaced
    aGroupObj.insertName( rTemplateName, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );
    const OUString aTemplateURL {aGroupObj.GetMainURL( INetURLObject::DecodeMechanism::NONE )};

    OUString aTemplateToRemoveTargetURL;

    if ( Content::create( aTemplateURL, maCmdEnv, comphelper::getProcessComponentContext(), aTemplateToRemove ) )
    {
        bRemoveOldTemplateContent = true;
        if ( getProperty( aTemplateToRemove, TARGET_URL, aValue ) )
            aValue >>= aTemplateToRemoveTargetURL;

        if ( aGroupTargetURL.isEmpty() || !maTemplateDirs.hasElements()
          || (!aTemplateToRemoveTargetURL.isEmpty() && isInternalTemplateDir(aTemplateToRemoveTargetURL)) )
            return false; // it is not allowed to remove the template
    }

    try
    {
        const uno::Reference< uno::XComponentContext >& xContext = ::comphelper::getProcessComponentContext();

        // get document service name
        uno::Reference< frame::XModuleManager2 > xModuleManager( frame::ModuleManager::create(xContext) );
        const OUString sDocServiceName {xModuleManager->identify( uno::Reference< uno::XInterface >( rStorable, uno::UNO_QUERY ) )};
        if ( sDocServiceName.isEmpty() )
            throw uno::RuntimeException();

        // get the actual filter name
        uno::Reference< lang::XMultiServiceFactory > xConfigProvider =
                configuration::theDefaultProvider::get( xContext );

        uno::Sequence<uno::Any> aArgs(comphelper::InitAnyPropertySequence(
        {
            {"nodepath", uno::Any(u"/org.openoffice.Setup/Office/Factories/"_ustr)}
        }));
        uno::Reference< container::XNameAccess > xSOFConfig(
            xConfigProvider->createInstanceWithArguments(
                                    u"com.sun.star.configuration.ConfigurationAccess"_ustr,
                                    aArgs ),
            uno::UNO_QUERY_THROW );

        uno::Reference< container::XNameAccess > xApplConfig;
        xSOFConfig->getByName( sDocServiceName ) >>= xApplConfig;
        if ( !xApplConfig.is() )
            throw uno::RuntimeException();

        OUString aFilterName;
        xApplConfig->getByName(u"ooSetupFactoryActualTemplateFilter"_ustr) >>= aFilterName;
        if ( aFilterName.isEmpty() )
            throw uno::RuntimeException();

        // find the related type name
        uno::Reference< container::XNameAccess > xFilterFactory(
            mxContext->getServiceManager()->createInstanceWithContext(u"com.sun.star.document.FilterFactory"_ustr, mxContext),
            uno::UNO_QUERY_THROW );

        uno::Sequence< beans::PropertyValue > aFilterData;
        xFilterFactory->getByName( aFilterName ) >>= aFilterData;
        OUString aTypeName;
        for (const auto& rProp : aFilterData)
            if ( rProp.Name == "Type" )
                rProp.Value >>= aTypeName;

        if ( aTypeName.isEmpty() )
            throw uno::RuntimeException();

        // find the mediatype and extension
        uno::Reference< container::XNameAccess > xTypeDetection =
            mxType.is() ?
                uno::Reference< container::XNameAccess >( mxType, uno::UNO_QUERY_THROW ) :
                uno::Reference< container::XNameAccess >(
                    mxContext->getServiceManager()->createInstanceWithContext(u"com.sun.star.document.TypeDetection"_ustr, mxContext),
                    uno::UNO_QUERY_THROW );

        SequenceAsHashMap aTypeProps( xTypeDetection->getByName( aTypeName ) );
        uno::Sequence< OUString > aAllExt =
            aTypeProps.getUnpackedValueOrDefault(u"Extensions"_ustr, Sequence< OUString >() );
        if ( !aAllExt.hasElements() )
            throw uno::RuntimeException();

        const OUString aMediaType {aTypeProps.getUnpackedValueOrDefault(u"MediaType"_ustr, OUString() )};
        const OUString& aExt {aAllExt[0]};

        if ( aMediaType.isEmpty() || aExt.isEmpty() )
            throw uno::RuntimeException();

        // construct destination url
        if ( aGroupTargetURL.isEmpty() )
        {
            aGroupTargetURL = CreateNewGroupFsys( rGroupName, aGroup );

            if ( aGroupTargetURL.isEmpty() )
                throw uno::RuntimeException();
        }

        OUString aNewTemplateTargetURL = CreateNewUniqueFileWithPrefix( aGroupTargetURL, rTemplateName, aExt );
        if ( aNewTemplateTargetURL.isEmpty() )
        {
            aNewTemplateTargetURL = CreateNewUniqueFileWithPrefix( aGroupTargetURL, u"UserTemplate"_ustr, aExt );

            if ( aNewTemplateTargetURL.isEmpty() )
                throw uno::RuntimeException();
        }

        // store template
        uno::Sequence< PropertyValue > aStoreArgs{
            comphelper::makePropertyValue(u"FilterName"_ustr, aFilterName),
            comphelper::makePropertyValue(u"DocumentTitle"_ustr, rTemplateName)
        };

        if( !::utl::UCBContentHelper::EqualURLs( aNewTemplateTargetURL, rStorable->getLocation() ))
            rStorable->storeToURL( aNewTemplateTargetURL, aStoreArgs );
        else
            rStorable->store();

        // the storing was successful, now the old template with the same name can be removed if it existed
        if ( !aTemplateToRemoveTargetURL.isEmpty() )
        {
            removeContent( aTemplateToRemoveTargetURL );

            /*
             * pb: #i79496#
             * if the old template was the standard template
             * it is necessary to change the standard template with the new file name
             */
            const OUString sStdTmplFile = SfxObjectFactory::GetStandardTemplate( sDocServiceName );
            if ( INetURLObject( sStdTmplFile ) == INetURLObject( aTemplateToRemoveTargetURL ) )
            {
                SfxObjectFactory::SetStandardTemplate( sDocServiceName, aNewTemplateTargetURL );
            }
        }

        if ( bRemoveOldTemplateContent )
            removeContent( aTemplateToRemove );

        // add the template to hierarchy
        return addEntry( aGroup, rTemplateName, aNewTemplateTargetURL, aMediaType );
    }
    catch( Exception& )
    {
        // the template was not stored
        return false;
    }
}


sal_Bool SfxDocTplService::addTemplate( const OUString& rGroupName,
                                             const OUString& rTemplateName,
                                             const OUString& rSourceURL )
{
    if (!init())
        return false;

    ::osl::MutexGuard aGuard( maMutex );

    // Check, whether or not there is a group with this name
    // Return false, if there is no group with the given name
    Content         aGroup, aTemplate, aTargetGroup;
    INetURLObject   aGroupObj( maRootURL );

    aGroupObj.insertName( rGroupName, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );
    const OUString aGroupURL = aGroupObj.GetMainURL( INetURLObject::DecodeMechanism::NONE );

    if ( ! Content::create( aGroupURL, maCmdEnv, comphelper::getProcessComponentContext(), aGroup ) )
        return false;

    // Check, if there's a template with the given name in this group
    // Return false, if there already is a template
    aGroupObj.insertName( rTemplateName, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );
    const OUString aTemplateURL {aGroupObj.GetMainURL( INetURLObject::DecodeMechanism::NONE )};

    if ( Content::create( aTemplateURL, maCmdEnv, comphelper::getProcessComponentContext(), aTemplate ) )
        return false;

    // get the target url of the group
    OUString    aTargetURL;
    Any         aValue;

    if ( getProperty( aGroup, TARGET_DIR_URL, aValue ) )
        aValue >>= aTargetURL;

    if ( aTargetURL.isEmpty() )
    {
        aTargetURL = CreateNewGroupFsys( rGroupName, aGroup );

        if ( aTargetURL.isEmpty() )
            return false;
    }

    // Get the content type
    OUString aTitle, aType;

    bool bDocHasTitle = false;
    getTitleFromURL( rSourceURL, aTitle, aType, bDocHasTitle );

    INetURLObject   aSourceObj( rSourceURL );
    if ( rTemplateName == aTitle )
    {
        // addTemplate will sometimes be called just to add an entry in the
        // hierarchy; the target URL and the source URL will be the same in
        // this scenario
        // TODO/LATER: get rid of this old hack

        INetURLObject   aTargetObj( aTargetURL );

        aTargetObj.insertName( rTemplateName, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );
        aTargetObj.setExtension( aSourceObj.getExtension() );

        const OUString aTargetURL2 = aTargetObj.GetMainURL( INetURLObject::DecodeMechanism::NONE );

        if ( aTargetURL2 == rSourceURL )
            return addEntry( aGroup, rTemplateName, aTargetURL2, aType );
    }

    // copy the template into the new group (targeturl)

    INetURLObject aTmpURL( aSourceObj );
    aTmpURL.CutExtension();
    const OUString aPattern {aTmpURL.getName( INetURLObject::LAST_SEGMENT, true, INetURLObject::DecodeMechanism::WithCharset )};

    const OUString aNewTemplateTargetURL {CreateNewUniqueFileWithPrefix( aTargetURL, aPattern, aSourceObj.getExtension() )};
    INetURLObject aNewTemplateTargetObj( aNewTemplateTargetURL );
    const OUString aNewTemplateTargetName {aNewTemplateTargetObj.getName( INetURLObject::LAST_SEGMENT, true, INetURLObject::DecodeMechanism::WithCharset )};
    if ( aNewTemplateTargetURL.isEmpty() || aNewTemplateTargetName.isEmpty() )
        return false;

    // get access to source file
    Content aSourceContent;
    uno::Reference < ucb::XCommandEnvironment > xEnv;
    INetURLObject   aSourceURL( rSourceURL );
    if( ! Content::create( aSourceURL.GetMainURL( INetURLObject::DecodeMechanism::NONE ), xEnv, comphelper::getProcessComponentContext(), aSourceContent ) )
        return false;

    if( ! Content::create( aTargetURL, xEnv, comphelper::getProcessComponentContext(), aTargetGroup ) )
        return false;

    // transfer source file
    try
    {
        aTargetGroup.transferContent( aSourceContent,
                                      InsertOperation::Copy,
                                      aNewTemplateTargetName,
                                      NameClash::OVERWRITE,
                                      aType );

        // allow to edit the added template
        Content aResultContent;
        if ( Content::create( aNewTemplateTargetURL, xEnv, comphelper::getProcessComponentContext(), aResultContent ) )
        {
            static constexpr OUString aPropertyName( u"IsReadOnly"_ustr );
            uno::Any aProperty;
            bool bReadOnly = false;
            if ( getProperty( aResultContent, aPropertyName, aProperty ) && ( aProperty >>= bReadOnly ) && bReadOnly )
                setProperty( aResultContent, aPropertyName, uno::Any( false ) );
        }
    }
    catch ( ContentCreationException& )
    { return false; }
    catch ( Exception& )
    { return false; }


    // either the document has title and it is the same as requested, or we have to set it
    bool bCorrectTitle = ( bDocHasTitle && aTitle == rTemplateName );
    if ( !bCorrectTitle )
    {
        if ( !bDocHasTitle )
        {
            INetURLObject aNewTmpObj(std::move(aNewTemplateTargetObj));
            aNewTmpObj.CutExtension();
            bCorrectTitle = ( aNewTmpObj.getName( INetURLObject::LAST_SEGMENT, true, INetURLObject::DecodeMechanism::WithCharset ) == rTemplateName );
        }

        if ( !bCorrectTitle )
            bCorrectTitle = setTitleForURL( aNewTemplateTargetURL, rTemplateName );
    }

    if ( bCorrectTitle )
    {
        // create a new entry in the hierarchy
        return addEntry( aGroup, rTemplateName, aNewTemplateTargetURL, aType );
    }

    // TODO/LATER: The user could be notified here that the renaming has failed
    // create a new entry in the hierarchy
    addEntry( aGroup, aTitle, aNewTemplateTargetURL, aType );
    return false;
}

bool SfxDocTplService::isInternalTemplateDir(const OUString& rURL) const
{
    return std::any_of(maInternalTemplateDirs.begin(), maInternalTemplateDirs.end(),
        [&rURL](const OUString& rDir) { return ::utl::UCBContentHelper::IsSubPath(rDir, rURL); });
}

OUString SfxDocTplService::findParentTemplateDir(const OUString& rURL) const
{
    const OUString* pDirs = std::find_if(maTemplateDirs.begin(), maTemplateDirs.end(),
        [&rURL](const OUString& rDir) { return ::utl::UCBContentHelper::IsSubPath(rDir, rURL); });
    if (pDirs != maTemplateDirs.end())
        return *pDirs;
    return OUString();
}

sal_Bool SfxDocTplService::removeTemplate( const OUString& rGroupName,
                                       const OUString& rTemplateName )
{
    if (!init())
        return false;
    ::osl::MutexGuard aGuard( maMutex );

    // Check, whether or not there is a group with this name
    // Return false, if there is no group with the given name
    Content         aGroup, aTemplate;
    INetURLObject   aGroupObj( maRootURL );

    aGroupObj.insertName( rGroupName, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );
    const OUString aGroupURL {aGroupObj.GetMainURL( INetURLObject::DecodeMechanism::NONE )};

    if ( ! Content::create( aGroupURL, maCmdEnv, comphelper::getProcessComponentContext(), aGroup ) )
        return false;

    // Check, if there's a template with the given name in this group
    // Return false, if there is no template
    aGroupObj.insertName( rTemplateName, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );
    const OUString aTemplateURL {aGroupObj.GetMainURL( INetURLObject::DecodeMechanism::NONE )};

    if ( !Content::create( aTemplateURL, maCmdEnv, comphelper::getProcessComponentContext(), aTemplate ) )
        return false;

    // get the target URL from the template
    OUString    aTargetURL;
    Any         aValue;

    if ( getProperty( aTemplate, TARGET_URL, aValue ) )
        aValue >>= aTargetURL;

    // delete the target template
    if ( !aTargetURL.isEmpty() )
    {
        if (isInternalTemplateDir(aTargetURL))
            return false;

        removeContent( aTargetURL );
    }

    // delete the template entry
    return removeContent( aTemplate );
}


sal_Bool SfxDocTplService::renameTemplate( const OUString& rGroupName,
                                           const OUString& rOldName,
                                           const OUString& rNewName )
{
    if ( rOldName == rNewName )
        return true;
    if (!init())
        return false;

    ::osl::MutexGuard aGuard( maMutex );

    // Check, whether or not there is a group with this name
    // Return false, if there is no group with the given name
    Content         aGroup, aTemplate;
    INetURLObject   aGroupObj( maRootURL );

    aGroupObj.insertName( rGroupName, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );
    const OUString aGroupURL {aGroupObj.GetMainURL( INetURLObject::DecodeMechanism::NONE )};

    if ( ! Content::create( aGroupURL, maCmdEnv, comphelper::getProcessComponentContext(), aGroup ) )
        return false;

    // Check, if there's a template with the new name in this group
    // Return false, if there is one
    aGroupObj.insertName( rNewName, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );
    OUString aTemplateURL {aGroupObj.GetMainURL( INetURLObject::DecodeMechanism::NONE )};

    if ( Content::create( aTemplateURL, maCmdEnv, comphelper::getProcessComponentContext(), aTemplate ) )
        return false;

    // Check, if there's a template with the old name in this group
    // Return false, if there is no template
    aGroupObj.removeSegment();
    aGroupObj.insertName( rOldName, false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );
    aTemplateURL = aGroupObj.GetMainURL( INetURLObject::DecodeMechanism::NONE );

    if ( !Content::create( aTemplateURL, maCmdEnv, comphelper::getProcessComponentContext(), aTemplate ) )
        return false;

    OUString    aTemplateTargetURL;
    Any         aTargetValue;

    if ( getProperty( aTemplate, TARGET_URL, aTargetValue ) )
        aTargetValue >>= aTemplateTargetURL;

    if ( !setTitleForURL( aTemplateTargetURL, rNewName ) )
        return false;

    // rename the template entry in the cache
    Any         aTitleValue;
    aTitleValue <<= rNewName;

    return setProperty( aTemplate, TITLE, aTitleValue );
}



//--- XDocumentTemplates ---

uno::Reference< ucb::XContent > SAL_CALL SfxDocTplService::getContent()
{
    if ( init() )
        return maRootContent.get();
    return nullptr;
}


WaitWindow_Impl::WaitWindow_Impl() : WorkWindow(nullptr, WB_BORDER | WB_3DLOOK)
{
    tools::Rectangle aRect(0, 0, 300, 30000);
    maText = SfxResId(RID_CNT_STR_WAITING);
    maRect = GetTextRect(aRect, maText, gnTextStyle);
    aRect = maRect;
    aRect.AdjustRight(2 * X_OFFSET );
    aRect.AdjustBottom(2 * Y_OFFSET );
    maRect.SetPos(Point(X_OFFSET, Y_OFFSET));
    SetOutputSizePixel(aRect.GetSize());

    Show();
    PaintImmediately();
    GetOutDev()->Flush();
}


WaitWindow_Impl::~WaitWindow_Impl()
{
    disposeOnce();
}

void  WaitWindow_Impl::dispose()
{
    Hide();
    WorkWindow::dispose();
}


void WaitWindow_Impl::Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle& /*rRect*/)
{
    rRenderContext.DrawText(maRect, maText, gnTextStyle);
}

void SfxDocTplService::addHierGroup( GroupList_Impl& rList,
                                          const OUString& rTitle,
                                          const OUString& rOwnURL )
{
    // now get the content of the Group
    Content aContent;
    uno::Reference<XResultSet> xResultSet;

    try
    {
        aContent = Content(rOwnURL, maCmdEnv, comphelper::getProcessComponentContext());
        xResultSet = aContent.createCursor( { TITLE, TARGET_URL, PROPERTY_TYPE }, INCLUDE_DOCUMENTS_ONLY );
    }
    catch (ContentCreationException&)
    {
        TOOLS_WARN_EXCEPTION( "sfx.doc", "" );
    }
    catch (Exception&) {}

    if ( !xResultSet.is() )
        return;

    GroupData_Impl *pGroup = new GroupData_Impl( rTitle );
    pGroup->setHierarchy( true );
    pGroup->setHierarchyURL( rOwnURL );
    rList.push_back( std::unique_ptr<GroupData_Impl>(pGroup) );

    uno::Reference< XContentAccess > xContentAccess( xResultSet, UNO_QUERY );
    uno::Reference< XRow > xRow( xResultSet, UNO_QUERY );

    try
    {
        while ( xResultSet->next() )
        {
            bool             bUpdateType = false;
            DocTemplates_EntryData_Impl  *pData;

            const OUString aTitle( xRow->getString( 1 ) );
            const OUString aTargetDir( xRow->getString( 2 ) );
            OUString aType( xRow->getString( 3 ) );
            const OUString aHierURL {xContentAccess->queryContentIdentifierString()};

            if ( aType.isEmpty() )
            {
                OUString aTmpTitle;

                bool bDocHasTitle = false;
                getTitleFromURL( aTargetDir, aTmpTitle, aType, bDocHasTitle );

                if ( !aType.isEmpty() )
                    bUpdateType = true;
            }

            pData = pGroup->addEntry( aTitle, aTargetDir, aType, aHierURL );
            pData->setUpdateType( bUpdateType );
        }
    }
    catch ( Exception& ) {}
}


void SfxDocTplService::addFsysGroup( GroupList_Impl& rList,
                                          const OUString& rTitle,
                                          const OUString& rUITitle,
                                          const OUString& rOwnURL,
                                          bool bWriteableGroup )
{
    OUString aTitle;

    if ( rUITitle.isEmpty() )
    {
        // reserved FS names that should not be used
        if ( rTitle == "wizard" )
            return;
        else if ( rTitle == "internal" )
            return;

        aTitle = getLongName( rTitle );
    }
    else
        aTitle = rUITitle;

    if ( aTitle.isEmpty() )
        return;

    GroupData_Impl* pGroup = nullptr;
    for (const std::unique_ptr<GroupData_Impl>& i : rList)
    {
        if ( i->getTitle() == aTitle )
        {
            pGroup = i.get();
            break;
        }
    }

    if ( !pGroup )
    {
        pGroup = new GroupData_Impl( aTitle );
        rList.push_back( std::unique_ptr<GroupData_Impl>(pGroup) );
    }

    if ( bWriteableGroup )
        pGroup->setTargetURL( rOwnURL );

    pGroup->setInUse();

    // now get the content of the Group
    Content                 aContent;
    uno::Reference< XResultSet > xResultSet;
    Sequence< OUString >    aProps { TITLE };

    try
    {
        // this method is only used during checking of the available template-folders
        // that should happen quietly
        uno::Reference< XCommandEnvironment > aQuietEnv;
        aContent = Content( rOwnURL, aQuietEnv, comphelper::getProcessComponentContext() );
        xResultSet = aContent.createCursor( aProps, INCLUDE_DOCUMENTS_ONLY );
    }
    catch ( Exception& ) {}

    if ( !xResultSet.is() )
        return;

    uno::Reference< XContentAccess > xContentAccess( xResultSet, UNO_QUERY );
    uno::Reference< XRow > xRow( xResultSet, UNO_QUERY );

    try
    {
        while ( xResultSet->next() )
        {
            OUString aChildTitle( xRow->getString( 1 ) );
            const OUString aTargetURL {xContentAccess->queryContentIdentifierString()};
            OUString aType;

            if ( aChildTitle == "sfx.tlx" || aChildTitle == "groupuinames.xml" )
                continue;

            bool bDocHasTitle = false;
            getTitleFromURL( aTargetURL, aChildTitle, aType, bDocHasTitle );

            pGroup->addEntry( aChildTitle, aTargetURL, aType, OUString() );
        }
    }
    catch ( Exception& ) {}
}


void SfxDocTplService::createFromContent( GroupList_Impl& rList,
                                               Content &rContent,
                                               bool bHierarchy,
                                               bool bWriteableContent )
{
    const OUString aTargetURL {rContent.get()->getIdentifier()->getContentIdentifier()};

    // when scanning the file system, we have to add the 'standard' group, too
    if ( ! bHierarchy )
    {
        const OUString aUIStdTitle {getLongName( STANDARD_FOLDER )};
        addFsysGroup( rList, OUString(), aUIStdTitle, aTargetURL, bWriteableContent );
    }

    // search for predefined UI names
    INetURLObject aLayerObj( aTargetURL );

    // TODO/LATER: Use hashmap in future
    std::vector< beans::StringPair > aUINames;
    if ( !bHierarchy )
        aUINames = ReadUINamesForTemplateDir_Impl( aLayerObj.GetMainURL( INetURLObject::DecodeMechanism::NONE ) );

    uno::Reference< XResultSet > xResultSet;
    Sequence< OUString > aProps { TITLE };

    try
    {
        xResultSet = rContent.createCursor( aProps, INCLUDE_FOLDERS_ONLY );
    }
    catch ( Exception& ) {}

    if ( !xResultSet.is() )
        return;

    uno::Reference< XContentAccess > xContentAccess( xResultSet, UNO_QUERY );
    uno::Reference< XRow > xRow( xResultSet, UNO_QUERY );

    try
    {
        while ( xResultSet->next() )
        {
            // TODO/LATER: clarify the encoding of the Title
            const OUString aTitle( xRow->getString( 1 ) );
            const OUString aTargetSubfolderURL( xContentAccess->queryContentIdentifierString() );

            if ( bHierarchy )
                addHierGroup( rList, aTitle, aTargetSubfolderURL );
            else
            {
                OUString aUITitle;
                for (const beans::StringPair & rUIName : aUINames)
                    if ( rUIName.First == aTitle )
                    {
                        aUITitle = rUIName.Second;
                        break;
                    }

                addFsysGroup( rList, aTitle, aUITitle, aTargetSubfolderURL, bWriteableContent );
            }
        }
    }
    catch ( Exception& ) {}
}


void SfxDocTplService::removeFromHierarchy( DocTemplates_EntryData_Impl const *pData )
{
    Content aTemplate;

    if ( Content::create( pData->getHierarchyURL(), maCmdEnv, comphelper::getProcessComponentContext(), aTemplate ) )
    {
        removeContent( aTemplate );
    }
}


void SfxDocTplService::addToHierarchy( GroupData_Impl const *pGroup,
                                            DocTemplates_EntryData_Impl const *pData )
{
    Content aGroup, aTemplate;

    if ( ! Content::create( pGroup->getHierarchyURL(), maCmdEnv, comphelper::getProcessComponentContext(), aGroup ) )
        return;

    // Check, if there's a template with the given name in this group
    // Return if there is already a template
    INetURLObject aGroupObj( pGroup->getHierarchyURL() );

    aGroupObj.insertName( pData->getTitle(), false,
                      INetURLObject::LAST_SEGMENT,
                      INetURLObject::EncodeMechanism::All );

    const OUString aTemplateURL {aGroupObj.GetMainURL( INetURLObject::DecodeMechanism::NONE )};

    if ( Content::create( aTemplateURL, maCmdEnv, comphelper::getProcessComponentContext(), aTemplate ) )
        return;

    addEntry( aGroup, pData->getTitle(),
              pData->getTargetURL(),
              pData->getType() );
}


void SfxDocTplService::updateData( DocTemplates_EntryData_Impl const *pData )
{
    Content aTemplate;

    if ( ! Content::create( pData->getHierarchyURL(), maCmdEnv, comphelper::getProcessComponentContext(), aTemplate ) )
        return;

    if ( pData->getUpdateType() )
    {
        setProperty( aTemplate, PROPERTY_TYPE, Any( pData->getType() ) );
    }

    if ( pData->getUpdateLink() )
    {
        setProperty( aTemplate, TARGET_URL, Any( pData->getTargetURL() ) );
    }
}


void SfxDocTplService::addGroupToHierarchy( GroupData_Impl *pGroup )
{
    Content aGroup;

    INetURLObject aNewGroupObj( maRootURL );
    aNewGroupObj.insertName( pGroup->getTitle(), false,
          INetURLObject::LAST_SEGMENT,
          INetURLObject::EncodeMechanism::All );

    const OUString aNewGroupURL {aNewGroupObj.GetMainURL( INetURLObject::DecodeMechanism::NONE )};

    if ( createFolder( aNewGroupURL, false, false, aGroup ) )
    {
        setProperty( aGroup, TARGET_DIR_URL, Any( pGroup->getTargetURL() ) );
        pGroup->setHierarchyURL( aNewGroupURL );

        size_t nCount = pGroup->count();
        for ( size_t i = 0; i < nCount; i++ )
        {
            DocTemplates_EntryData_Impl *pData = pGroup->getEntry( i );
            addToHierarchy( pGroup, pData ); // add entry to hierarchy
        }
    }
}


void SfxDocTplService::removeFromHierarchy( GroupData_Impl const *pGroup )
{
    Content aGroup;

    if ( Content::create( pGroup->getHierarchyURL(), maCmdEnv, comphelper::getProcessComponentContext(), aGroup ) )
    {
        removeContent( aGroup );
    }
}


GroupData_Impl::GroupData_Impl( OUString aTitle )
     : maTitle(std::move(aTitle)), mbInUse(false), mbInHierarchy(false)
{
}


DocTemplates_EntryData_Impl* GroupData_Impl::addEntry( const OUString& rTitle,
                                          const OUString& rTargetURL,
                                          const OUString& rType,
                                          const OUString& rHierURL )
{
    DocTemplates_EntryData_Impl* pData = nullptr;
    bool EntryFound = false;

    for (auto const & p : maEntries)
    {
        pData = p.get();
        if ( pData->getTitle() == rTitle )
        {
            EntryFound = true;
            break;
        }
    }

    if ( !EntryFound )
    {
        pData = new DocTemplates_EntryData_Impl( rTitle );
        pData->setTargetURL( rTargetURL );
        pData->setType( rType );
        if ( !rHierURL.isEmpty() )
        {
            pData->setHierarchyURL( rHierURL );
            pData->setHierarchy( true );
        }
        maEntries.emplace_back( pData );
    }
    else
    {
        if ( !rHierURL.isEmpty() )
        {
            pData->setHierarchyURL( rHierURL );
            pData->setHierarchy( true );
        }

        if ( pData->getInHierarchy() )
            pData->setInUse();

        if ( rTargetURL != pData->getTargetURL() )
        {
            pData->setTargetURL( rTargetURL );
            pData->setUpdateLink( true );
        }
    }

    return pData;
}


DocTemplates_EntryData_Impl::DocTemplates_EntryData_Impl( OUString aTitle )
     : maTitle(std::move(aTitle)), mbInHierarchy(false), mbInUse(false), mbUpdateType(false), mbUpdateLink(false)
{
}

}

// static
bool SfxURLRelocator_Impl::propertyCanContainOfficeDir(
                                        std::u16string_view rPropName )
{
    // Note: TargetURL is handled by UCB itself (because it is a property
    //       with a predefined semantic). Additional Core properties introduced
    //       be a client app must be handled by the client app itself, because
    //       the UCB does not know the semantics of those properties.
    return ( rPropName == TARGET_DIR_URL || rPropName == PROPERTY_DIRLIST );
}


SfxURLRelocator_Impl::SfxURLRelocator_Impl( uno::Reference< XComponentContext > xContext )
: mxContext(std::move( xContext ))
{
}


SfxURLRelocator_Impl::~SfxURLRelocator_Impl()
{
}


void SfxURLRelocator_Impl::initOfficeInstDirs()
{
    if ( !mxOfficeInstDirs.is() )
    {
        std::scoped_lock aGuard( maMutex );
        if ( !mxOfficeInstDirs.is() )
        {
            OSL_ENSURE( mxContext.is(), "No service manager!" );

            mxOfficeInstDirs = theOfficeInstallationDirectories::get(mxContext);
        }
    }
}


void SfxURLRelocator_Impl::implExpandURL( OUString& io_url )
{
    const INetURLObject aParser( io_url );
    if ( aParser.GetProtocol() != INetProtocol::VndSunStarExpand )
        return;

    io_url = aParser.GetURLPath( INetURLObject::DecodeMechanism::WithCharset );
    try
    {
        if ( !mxMacroExpander.is() )
        {
            mxMacroExpander.set( theMacroExpander::get(mxContext), UNO_SET_THROW );
        }
        io_url = mxMacroExpander->expandMacros( io_url );
    }
    catch( const Exception& )
    {
        DBG_UNHANDLED_EXCEPTION("sfx.doc");
    }
}


void SfxURLRelocator_Impl::makeRelocatableURL( OUString & rURL )
{
    if ( !rURL.isEmpty() )
    {
        initOfficeInstDirs();
        implExpandURL( rURL );
        rURL = mxOfficeInstDirs->makeRelocatableURL( rURL );
    }
}


void SfxURLRelocator_Impl::makeAbsoluteURL( OUString & rURL )
{
    if ( !rURL.isEmpty() )
    {
        initOfficeInstDirs();
        implExpandURL( rURL );
        rURL = mxOfficeInstDirs->makeAbsoluteURL( rURL );
    }
}

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface *
com_sun_star_comp_sfx2_DocumentTemplates_get_implementation(
    css::uno::XComponentContext *context,
    css::uno::Sequence<css::uno::Any> const &)
{
    return cppu::acquire(new SfxDocTplService(context));
}

OUString DocTemplLocaleHelper::GetStandardGroupString()
{
    return SfxResId(TEMPLATE_LONG_NAMES_ARY[0]);
}

std::vector<OUString> DocTemplLocaleHelper::GetBuiltInGroupNames()
{
    std::vector<OUString> aGroups;
    for(auto const & aGroupName : TEMPLATE_LONG_NAMES_ARY)
        aGroups.push_back(SfxResId(aGroupName));
    return aGroups;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
