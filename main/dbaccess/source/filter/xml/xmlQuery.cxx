/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_dbaxml.hxx"
#ifndef DBA_XMLQUERY_HXX
#include "xmlQuery.hxx"
#endif
#ifndef DBA_XMLFILTER_HXX
#include "xmlfilter.hxx"
#endif
#ifndef _XMLOFF_XMLTOKEN_HXX
#include <xmloff/xmltoken.hxx>
#endif
#ifndef _XMLOFF_XMLNMSPE_HXX
#include <xmloff/xmlnmspe.hxx>
#endif
#ifndef _XMLOFF_NMSPMAP_HXX
#include <xmloff/nmspmap.hxx>
#endif
#ifndef DBA_XMLENUMS_HXX
#include "xmlEnums.hxx"
#endif
#ifndef DBACCESS_SHARED_XMLSTRINGS_HRC
#include "xmlstrings.hrc"
#endif
#ifndef _COM_SUN_STAR_BEANS_PROPERTYVALUE_HPP_
#include <com/sun/star/beans/PropertyValue.hpp>
#endif
#ifndef _COM_SUN_STAR_BEANS_XPROPERTYSET_HPP_
#include <com/sun/star/beans/XPropertySet.hpp>
#endif
#ifndef _COM_SUN_STAR_CONTAINER_XNAMECONTAINER_HPP_
#include <com/sun/star/container/XNameContainer.hpp>
#endif
#ifndef _TOOLS_DEBUG_HXX
#include <tools/debug.hxx>
#endif

namespace dbaxml
{
	using namespace ::com::sun::star::uno;
	using namespace ::com::sun::star::beans;
	using namespace ::com::sun::star::container;
	using namespace ::com::sun::star::xml::sax;

DBG_NAME(OXMLQuery)

OXMLQuery::OXMLQuery( ODBFilter& rImport
				,sal_uInt16 nPrfx
				,const ::rtl::OUString& _sLocalName
				,const Reference< XAttributeList > & _xAttrList 
				,const ::com::sun::star::uno::Reference< ::com::sun::star::container::XNameAccess >& _xParentContainer
				) :
	OXMLTable( rImport, nPrfx, _sLocalName,_xAttrList,_xParentContainer,SERVICE_SDB_COMMAND_DEFINITION )
		,m_bEscapeProcessing(sal_True)
{
    DBG_CTOR(OXMLQuery,NULL);

	OSL_ENSURE(_xAttrList.is(),"Attribute list is NULL!");
	const SvXMLNamespaceMap& rMap = rImport.GetNamespaceMap();
	const SvXMLTokenMap& rTokenMap = rImport.GetQueryElemTokenMap();

	sal_Int16 nLength = (_xAttrList.is()) ? _xAttrList->getLength() : 0;
	for(sal_Int16 i = 0; i < nLength; ++i)
	{
		::rtl::OUString sLocalName;
		rtl::OUString sAttrName = _xAttrList->getNameByIndex( i );
		sal_uInt16 nPrefix = rMap.GetKeyByAttrName( sAttrName,&sLocalName );
		rtl::OUString sValue = _xAttrList->getValueByIndex( i );

		switch( rTokenMap.Get( nPrefix, sLocalName ) )
		{
			case XML_TOK_COMMAND:
				m_sCommand = sValue;
				break;
			case XML_TOK_ESCAPE_PROCESSING:
				m_bEscapeProcessing = sValue.equalsAscii("true");
				break;
		}
	}
}
// -----------------------------------------------------------------------------

OXMLQuery::~OXMLQuery()
{

    DBG_DTOR(OXMLQuery,NULL);
}
// -----------------------------------------------------------------------------
SvXMLImportContext* OXMLQuery::CreateChildContext(
		sal_uInt16 nPrefix,
		const ::rtl::OUString& rLocalName,
		const Reference< XAttributeList > & xAttrList )
{
	SvXMLImportContext* pContext = OXMLTable::CreateChildContext(nPrefix, rLocalName,xAttrList );
	if ( !pContext )
	{
		const SvXMLTokenMap& rTokenMap = GetOwnImport().GetQueryElemTokenMap();

		switch( rTokenMap.Get( nPrefix, rLocalName ) )
		{
			case XML_TOK_UPDATE_TABLE:
				{
					GetOwnImport().GetProgressBarHelper()->Increment( PROGRESS_BAR_STEP );
					::rtl::OUString s1;
					fillAttributes(nPrefix, rLocalName,xAttrList,s1,m_sTable,m_sSchema,m_sCatalog);
				}
				break;
		}
	}

	if( !pContext )
		pContext = new SvXMLImportContext( GetImport(), nPrefix, rLocalName );

	return pContext;
}
// -----------------------------------------------------------------------------
void OXMLQuery::setProperties(Reference< XPropertySet > & _xProp )
{
	try
	{
		if ( _xProp.is() )
		{
			OXMLTable::setProperties(_xProp);

			_xProp->setPropertyValue(PROPERTY_COMMAND,makeAny(m_sCommand));
			_xProp->setPropertyValue(PROPERTY_ESCAPE_PROCESSING,makeAny(m_bEscapeProcessing));

			if ( m_sTable.getLength() )
				_xProp->setPropertyValue(PROPERTY_UPDATE_TABLENAME,makeAny(m_sTable));
			if ( m_sCatalog.getLength() )
				_xProp->setPropertyValue(PROPERTY_UPDATE_CATALOGNAME,makeAny(m_sCatalog));
			if ( m_sSchema.getLength() )
				_xProp->setPropertyValue(PROPERTY_UPDATE_SCHEMANAME,makeAny(m_sSchema));
			
			const ODBFilter::TPropertyNameMap& rSettings = GetOwnImport().getQuerySettings();
			ODBFilter::TPropertyNameMap::const_iterator aFind = rSettings.find(m_sName);
			if ( aFind != rSettings.end() )
				_xProp->setPropertyValue(PROPERTY_LAYOUTINFORMATION,makeAny(aFind->second));
		}
	}
	catch(Exception&)
	{
		OSL_ENSURE(0,"OXMLTable::EndElement -> exception catched");
	}
}
//----------------------------------------------------------------------------
} // namespace dbaxml
// -----------------------------------------------------------------------------
