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

#include <config_features.h>
#include <config_fuzzers.h>

#include <osl/diagnose.h>
#include <unotools/charclass.hxx>
#include <editsh.hxx>
#include <fldbas.hxx>
#include <doc.hxx>
#include <IDocumentFieldsAccess.hxx>
#include <IDocumentState.hxx>
#include <docary.hxx>
#include <fmtfld.hxx>
#include <txtfld.hxx>
#include <pamtyp.hxx>
#include <expfld.hxx>
#include <swundo.hxx>
#include <dbmgr.hxx>
#include <hints.hxx>
#include <fieldhint.hxx>
#include <DocumentSettingManager.hxx>
#include <IDocumentContentOperations.hxx>

/// count field types with a ResId, if SwFieldIds::Unknown count all
size_t SwEditShell::GetFieldTypeCount(SwFieldIds nResId ) const
{
    const SwFieldTypes* pFieldTypes = GetDoc()->getIDocumentFieldsAccess().GetFieldTypes();

    if(nResId == SwFieldIds::Unknown)
    {
        return o3tl::narrowing<sal_uInt16>(pFieldTypes->size());
    }

    // all types with the same ResId
    size_t nIdx  = 0;
    for(const auto & pFieldType : *pFieldTypes)
    {
        // same ResId -> increment index
        if(pFieldType->Which() == nResId)
            nIdx++;
    }
    return nIdx;
}

/// get field types with a ResId, if 0 get all
SwFieldType* SwEditShell::GetFieldType(size_t nField, SwFieldIds nResId ) const
{
    const SwFieldTypes* pFieldTypes = GetDoc()->getIDocumentFieldsAccess().GetFieldTypes();

    if(nResId == SwFieldIds::Unknown && nField < pFieldTypes->size())
    {
        return (*pFieldTypes)[nField].get();
    }

    size_t nIdx = 0;
    for(const auto & pFieldType : *pFieldTypes)
    {
        // same ResId -> increment index
        if(pFieldType->Which() == nResId)
        {
            if(nIdx == nField)
                return pFieldType.get();
            nIdx++;
        }
    }
    return nullptr;
}

/// get first type with given ResId and name
SwFieldType* SwEditShell::GetFieldType(SwFieldIds nResId, const OUString& rName) const
{
    return GetDoc()->getIDocumentFieldsAccess().GetFieldType( nResId, rName, false );
}

/// delete field type
void SwEditShell::RemoveFieldType(size_t nField)
{
    GetDoc()->getIDocumentFieldsAccess().RemoveFieldType(nField);
}

/// delete field type based on its name
void SwEditShell::RemoveFieldType(SwFieldIds nResId, const OUString& rStr)
{
    const SwFieldTypes* pFieldTypes = GetDoc()->getIDocumentFieldsAccess().GetFieldTypes();
    const SwFieldTypes::size_type nSize = pFieldTypes->size();
    const CharClass& rCC = GetAppCharClass();

    OUString aTmp( rCC.lowercase( rStr ));

    for(SwFieldTypes::size_type i = 0; i < nSize; ++i)
    {
        // same ResId -> increment index
        SwFieldType* pFieldType = (*pFieldTypes)[i].get();
        if( pFieldType->Which() == nResId )
        {
            if( aTmp == rCC.lowercase( pFieldType->GetName().toString() ) )
            {
                GetDoc()->getIDocumentFieldsAccess().RemoveFieldType(i);
                return;
            }
        }
    }
}

void SwEditShell::FieldToText( SwFieldType const * pType )
{
    if( !pType->HasWriterListeners() )
        return;

    CurrShell aCurr( this );
    StartAllAction();
    StartUndo( SwUndoId::DELETE );
    Push();
    SwPaM* pPaM = GetCursor();
    const SwFieldHint aHint(pPaM, GetLayout());
    pType->CallSwClientNotify(aHint);

    Pop(PopMode::DeleteCurrent);
    EndAllAction();
    EndUndo( SwUndoId::DELETE );
}

/// add a field at the cursor position
bool SwEditShell::InsertField(SwField const & rField, const bool bForceExpandHints)
{
    CurrShell aCurr( this );
    StartAllAction();
    SwFormatField aField( rField );

    const SetAttrMode nInsertFlags = bForceExpandHints
        ? SetAttrMode::FORCEHINTEXPAND
        : SetAttrMode::DEFAULT;

    bool bSuccess(false);
    for(const SwPaM& rPaM : GetCursor()->GetRingContainer()) // for each PaM
    {
        bSuccess |= GetDoc()->getIDocumentContentOperations().InsertPoolItem(rPaM, aField, nInsertFlags);
        OSL_ENSURE( bSuccess, "Doc->Insert(Field) failed");
    }

    EndAllAction();
    return bSuccess;
}

/// Are the PaMs positioned on fields?
static SwTextField* lcl_FindInputField( const SwDoc* pDoc, const SwField& rField )
{
    // Search field via its address. For input fields this needs to be done in protected fields.
    SwTextField* pTField = nullptr;
    if (SwFieldIds::Input == rField.Which()
        || (SwFieldIds::SetExp == rField.Which()
            && static_cast<const SwSetExpField&>(rField).GetInputFlag()
            && (static_cast<SwSetExpFieldType*>(rField.GetTyp())->GetType()
                & SwGetSetExpType::String)))
    {
        pDoc->ForEachFormatField(RES_TXTATR_INPUTFIELD,
            [&rField, &pTField] (const SwFormatField& rFormatField) -> bool
            {
                if( rFormatField.GetField() == &rField )
                {
                    pTField = const_cast<SwFormatField&>(rFormatField).GetTextField();
                    return false;
                }
                return true;
            });
    }
    else if( SwFieldIds::SetExp == rField.Which()
        && static_cast<const SwSetExpField&>(rField).GetInputFlag() )
    {
        pDoc->ForEachFormatField(RES_TXTATR_FIELD,
            [&rField, &pTField] (const SwFormatField& rFormatField) -> bool
            {
                if( rFormatField.GetField() == &rField )
                {
                    pTField = const_cast<SwFormatField&>(rFormatField).GetTextField();
                    return false;
                }
                return true;
            });
    }
    return pTField;
}

void SwEditShell::UpdateOneField(SwField &rField)
{
    CurrShell aCurr( this );
    StartAllAction();
    {
        // If there are no selections so take the value of the current cursor position.
        SwPaM* pCursor = GetCursor();
        SwTextField *pTextField;
        SwFormatField *pFormatField;

        if ( !pCursor->IsMultiSelection() && !pCursor->HasMark())
        {
            pTextField = GetTextFieldAtPos(pCursor->Start(), ::sw::GetTextAttrMode::Default);

            if (!pTextField) // #i30221#
                pTextField = lcl_FindInputField( GetDoc(), rField);

            if (pTextField != nullptr)
            {
                GetDoc()->getIDocumentFieldsAccess().UpdateField(
                    pTextField,
                    rField,
                    true);
            }
        }

        // bOkay (instead of return because of EndAllAction) becomes false,
        // 1) if only one PaM has more than one field or
        // 2) if there are mixed field types
        bool bOkay = true;
        bool bTableSelBreak = false;

        SwMsgPoolItem aFieldHint( RES_TXTATR_FIELD );  // Search-Hint
        SwMsgPoolItem aAnnotationFieldHint( RES_TXTATR_ANNOTATION );
        SwMsgPoolItem aInputFieldHint( RES_TXTATR_INPUTFIELD );
        for(SwPaM& rPaM : GetCursor()->GetRingContainer()) // for each PaM
        {
            if( rPaM.HasMark() && bOkay )    // ... with selection
            {
                // copy of the PaM
                SwPaM aCurPam( *rPaM.GetMark(), *rPaM.GetPoint() );
                SwPaM aPam( *rPaM.GetPoint() );

                SwPosition *pCurStt = aCurPam.Start(), *pCurEnd =
                    aCurPam.End();
                /*
                 * In case that there are two contiguous fields in a PaM, the aPam goes step by step
                 * to the end. aCurPam is reduced in each loop. If aCurPam was searched completely,
                 * the loop terminates because Start = End.
                 */

                // Search for SwTextField ...
                while(  bOkay
                     && pCurStt->GetContentIndex() != pCurEnd->GetContentIndex()
                     && (sw::FindAttrImpl(aPam, aFieldHint, fnMoveForward, aCurPam, true, GetLayout())
                          || sw::FindAttrImpl(aPam, aAnnotationFieldHint, fnMoveForward, aCurPam, false, GetLayout())
                          || sw::FindAttrImpl(aPam, aInputFieldHint, fnMoveForward, aCurPam, false, GetLayout())))
                {
                    // if only one PaM has more than one field  ...
                    if( aPam.Start()->GetContentIndex() != pCurStt->GetContentIndex() )
                        bOkay = false;

                    pTextField = GetTextFieldAtPos(pCurStt, ::sw::GetTextAttrMode::Default);
                    if( nullptr != pTextField )
                    {
                        pFormatField = const_cast<SwFormatField*>(&pTextField->GetFormatField());
                        SwField *pCurField = pFormatField->GetField();

                        // if there are mixed field types
                        if( pCurField->GetTyp()->Which() !=
                            rField.GetTyp()->Which() )
                            bOkay = false;

                        bTableSelBreak = GetDoc()->getIDocumentFieldsAccess().UpdateField(
                            pTextField,
                            rField,
                            false);
                    }
                    // The search area is reduced by the found area:
                    pCurStt->AdjustContent(+1);
                }
            }

            if( bTableSelBreak ) // If table section and table formula are updated -> finish
                break;

        }
    }
    GetDoc()->getIDocumentState().SetModified();
    EndAllAction();
}

void SwEditShell::ConvertOneFieldToText(const SwField& rField)
{
    CurrShell aCurr( this );
    StartAllAction();
    GetDoc()->ConvertFieldToText(rField, *GetLayout());
    EndAllAction();
}

SwDBData const & SwEditShell::GetDBData() const
{
    return GetDoc()->GetDBData();
}

void SwEditShell::ChgDBData(const SwDBData& rNewData)
{
    GetDoc()->ChgDBData(rNewData);
}

void SwEditShell::GetAllUsedDB( std::vector<OUString>& rDBNameList,
                                std::vector<OUString> const * pAllDBNames )
{
    GetDoc()->GetAllUsedDB( rDBNameList, pAllDBNames );
}

void SwEditShell::ChangeDBFields( const std::vector<OUString>& rOldNames,
                                  const OUString& rNewName )
{
    GetDoc()->ChangeDBFields( rOldNames, rNewName );
}

/// Update all expression fields
void SwEditShell::UpdateExpFields(bool bCloseDB)
{
    CurrShell aCurr( this );
    StartAllAction();
    GetDoc()->getIDocumentFieldsAccess().UpdateExpFields(nullptr, true);
    if (bCloseDB)
    {
#if HAVE_FEATURE_DBCONNECTIVITY && !ENABLE_FUZZERS
        GetDoc()->GetDBManager()->CloseAll(); // close all database connections
#endif
    }
    EndAllAction();
}

SwDBManager* SwEditShell::GetDBManager() const
{
#if HAVE_FEATURE_DBCONNECTIVITY && !ENABLE_FUZZERS
    return GetDoc()->GetDBManager();
#else
    return NULL;
#endif
}

/// insert field type
SwFieldType* SwEditShell::InsertFieldType(const SwFieldType& rFieldType)
{
    return GetDoc()->getIDocumentFieldsAccess().InsertFieldType(rFieldType);
}

void SwEditShell::LockExpFields()
{
    GetDoc()->getIDocumentFieldsAccess().LockExpFields();
}

void SwEditShell::UnlockExpFields()
{
    GetDoc()->getIDocumentFieldsAccess().UnlockExpFields();
}

bool SwEditShell::IsExpFieldsLocked() const
{
    return GetDoc()->getIDocumentFieldsAccess().IsExpFieldsLocked();
}

void SwEditShell::SetFieldUpdateFlags( SwFieldUpdateFlags eFlags )
{
    getIDocumentSettingAccess().setFieldUpdateFlags( eFlags );
}

SwFieldUpdateFlags SwEditShell::GetFieldUpdateFlags() const
{
    return getIDocumentSettingAccess().getFieldUpdateFlags( false );
}

void SwEditShell::SetLabelDoc( bool bFlag )
{
    GetDoc()->GetDocumentSettingManager().set(DocumentSettingId::LABEL_DOCUMENT, bFlag );
}

bool SwEditShell::IsLabelDoc() const
{
    return getIDocumentSettingAccess().get(DocumentSettingId::LABEL_DOCUMENT);
}

void SwEditShell::ChangeAuthorityData(const SwAuthEntry* pNewData)
{
    GetDoc()->ChangeAuthorityData(pNewData);
}

bool SwEditShell::IsAnyDatabaseFieldInDoc()const
{
    // Similar to: SwDoc::GetDBDesc
    const SwFieldTypes * pFieldTypes = GetDoc()->getIDocumentFieldsAccess().GetFieldTypes();
    for(const auto & pFieldType : *pFieldTypes)
    {
        if(IsUsed(*pFieldType))
        {
            switch(pFieldType->Which())
            {
                case SwFieldIds::Database:
                case SwFieldIds::DbNextSet:
                case SwFieldIds::DbNumSet:
                case SwFieldIds::DbSetNumber:
                {
                    std::vector<SwFormatField*> vFields;
                    pFieldType->GatherFields(vFields);
                    return vFields.size();
                }
                break;
                default: break;
            }
        }
    }
    return false;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
