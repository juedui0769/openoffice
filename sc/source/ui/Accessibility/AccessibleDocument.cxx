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
#include "precompiled_sc.hxx"


#include "AccessibleDocument.hxx"
#include "AccessibleSpreadsheet.hxx"
#include "tabvwsh.hxx"
#include "AccessibilityHints.hxx"
#include "document.hxx"
#include "drwlayer.hxx"
#include "unoguard.hxx"
#include "shapeuno.hxx"
#include "DrawModelBroadcaster.hxx"
#include "drawview.hxx"
#include "gridwin.hxx"
#include "AccessibleEditObject.hxx"
#include "scresid.hxx"
#ifndef SC_SC_HRC
#include "sc.hrc"
#endif
#include <com/sun/star/accessibility/AccessibleEventId.hpp>
#ifndef _COM_SUN_STAR_ACCESSIBILITY_XACCESSIBLESTATETYPE_HPP_
#include <com/sun/star/accessibility/AccessibleStateType.hpp>
#endif
#ifndef _COM_SUN_STAR_ACCESSIBILITY_XACCESSIBLERELATIONTYPE_HPP_
#include <com/sun/star/accessibility/AccessibleRelationType.hpp>
#endif
#include <com/sun/star/view/XSelectionSupplier.hpp>
#include <com/sun/star/drawing/XShape.hpp>
#include <com/sun/star/drawing/XShapes.hpp>

#ifndef _UTL_ACCESSIBLESTATESETHELPER_HXX
#include <unotools/accessiblestatesethelper.hxx>
#endif
#include <tools/debug.hxx>
#include <tools/gen.hxx>
#include <svx/svdpage.hxx>
#include <svx/svdobj.hxx>
#include <svx/ShapeTypeHandler.hxx>
#include <svx/AccessibleShape.hxx>
#include <svx/AccessibleShapeTreeInfo.hxx>
#include <svx/AccessibleShapeInfo.hxx>
#include <comphelper/sequence.hxx>
#include <sfx2/viewfrm.hxx>
#include <svx/unoshcol.hxx>
#include <svx/unoshape.hxx>
#include <unotools/accessiblerelationsethelper.hxx>
#include <toolkit/helper/convert.hxx>

#include <svx/AccessibleControlShape.hxx>
#include <svx/AccessibleShape.hxx>
#include <svx/ShapeTypeHandler.hxx>
#include <svx/SvxShapeTypes.hxx>
#include <sfx2/objsh.hxx>
#include <editeng/editview.hxx>
#include <editeng/editeng.hxx>
#include <list>
#include <algorithm>
#include "AccessibleCell.hxx"

#include "svx/unoapi.hxx"
#include "scmod.hxx"
using namespace	::com::sun::star;
using namespace	::com::sun::star::accessibility;
using ::std::for_each;

	//=====  internal  ========================================================

struct ScAccessibleShapeData
{
	ScAccessibleShapeData() : pAccShape(NULL), pRelationCell(NULL), bSelected(sal_False), bSelectable(sal_True) {}
	~ScAccessibleShapeData();
	mutable ::accessibility::AccessibleShape* pAccShape;
    mutable ScAddress*          pRelationCell; // if it is NULL this shape is anchored on the table
//    SdrObject*                  pShape;
	com::sun::star::uno::Reference< com::sun::star::drawing::XShape > xShape;
	mutable sal_Bool			bSelected;
    sal_Bool                    bSelectable;
};

ScAccessibleShapeData::~ScAccessibleShapeData()
{
    if (pAccShape)
    {
        pAccShape->dispose();
        pAccShape->release();
    }
}

struct ScShapeDataLess
{
    rtl::OUString msLayerId;
    rtl::OUString msZOrder;
    ScShapeDataLess()
        : msLayerId(RTL_CONSTASCII_USTRINGPARAM( "LayerID" )),
        msZOrder(RTL_CONSTASCII_USTRINGPARAM( "ZOrder" ))
    {
    }
    void ConvertLayerId(sal_Int16& rLayerID) const // changes the number of the LayerId so it the accessibility order
    {
        switch (rLayerID)
        {
        case SC_LAYER_FRONT:
            rLayerID = 1;
            break;
        case SC_LAYER_BACK:
            rLayerID = 0;
            break;
        case SC_LAYER_INTERN:
            rLayerID = 2;
            break;
        case SC_LAYER_CONTROLS:
            rLayerID = 3;
            break;
        }
    }
    sal_Bool LessThanSheet(const ScAccessibleShapeData* pData) const
    {
        sal_Bool bResult(sal_False);
        uno::Reference< beans::XPropertySet> xProps(pData->xShape, uno::UNO_QUERY);
        if (xProps.is())
        {
			uno::Any aPropAny = xProps->getPropertyValue(msLayerId);
			sal_Int16 nLayerID = 0;
			if( (aPropAny >>= nLayerID) )
            {
                if (nLayerID == SC_LAYER_BACK)
                    bResult = sal_True;
            }
        }
        return bResult;
    }
    sal_Bool operator()(const ScAccessibleShapeData* pData1, const ScAccessibleShapeData* pData2) const
    {
        sal_Bool bResult(sal_False);
        if (pData1 && pData2)
        {
            uno::Reference< beans::XPropertySet> xProps1(pData1->xShape, uno::UNO_QUERY);
            uno::Reference< beans::XPropertySet> xProps2(pData2->xShape, uno::UNO_QUERY);
            if (xProps1.is() && xProps2.is())
            {
				uno::Any aPropAny1 = xProps1->getPropertyValue(msLayerId);
                uno::Any aPropAny2 = xProps2->getPropertyValue(msLayerId);
				sal_Int16 nLayerID1(0);
                sal_Int16 nLayerID2(0);
				if( (aPropAny1 >>= nLayerID1) && (aPropAny2 >>= nLayerID2) )
                {
                    if (nLayerID1 == nLayerID2)
                    {
		                uno::Any aAny1 = xProps1->getPropertyValue(msZOrder);
		                sal_Int32 nZOrder1 = 0;
		                uno::Any aAny2 = xProps2->getPropertyValue(msZOrder);
		                sal_Int32 nZOrder2 = 0;
		                if ( (aAny1 >>= nZOrder1) && (aAny2 >>= nZOrder2) )
                            bResult = (nZOrder1 < nZOrder2);
                    }
                    else
                    {
                        ConvertLayerId(nLayerID1);
                        ConvertLayerId(nLayerID2);
                        bResult = (nLayerID1 < nLayerID2);
                    }
                }
            }
        }
        else if (pData1 && !pData2)
            bResult = LessThanSheet(pData1);
        else if (!pData1 && pData2)
            bResult = !LessThanSheet(pData2);
        else
            bResult = sal_False;
        return bResult;
    }
};

struct DeselectShape
{
	void operator() (const ScAccessibleShapeData* pAccShapeData) const
	{
        if (pAccShapeData)
        {
		    pAccShapeData->bSelected = sal_False;
		    if (pAccShapeData->pAccShape)
			    pAccShapeData->pAccShape->ResetState(AccessibleStateType::SELECTED);
        }
	}
};

struct SelectShape
{
    uno::Reference < drawing::XShapes > xShapes;
    SelectShape(uno::Reference<drawing::XShapes>& xTemp) : xShapes(xTemp) {}
	void operator() (const ScAccessibleShapeData* pAccShapeData) const
    {
        if (pAccShapeData && pAccShapeData->bSelectable)
        {
	        pAccShapeData->bSelected = sal_True;
	        if (pAccShapeData->pAccShape)
		        pAccShapeData->pAccShape->SetState(AccessibleStateType::SELECTED);
            if (xShapes.is())
                xShapes->add(pAccShapeData->xShape);
        }
    }
};

struct Destroy
{
    void operator() (ScAccessibleShapeData* pData)
    {
        if (pData)
            DELETEZ(pData);
    }
};

class ScChildrenShapes : public SfxListener,
	public ::accessibility::IAccessibleParent
{
public:
    ScChildrenShapes(ScAccessibleDocument* pAccessibleDocument, ScTabViewShell* pViewShell, ScSplitPos eSplitPos);
    ~ScChildrenShapes();

    ///=====  SfxListener  =====================================================

	virtual void Notify( SfxBroadcaster& rBC, const SfxHint& rHint );

    ///=====  IAccessibleParent  ===============================================

    virtual sal_Bool ReplaceChild (
        ::accessibility::AccessibleShape* pCurrentChild,
		const ::com::sun::star::uno::Reference< ::com::sun::star::drawing::XShape >& _rxShape,
		const long _nIndex,
		const ::accessibility::AccessibleShapeTreeInfo& _rShapeTreeInfo
	)	throw (::com::sun::star::uno::RuntimeException);

	virtual ::accessibility::AccessibleControlShape* GetAccControlShapeFromModel
		(::com::sun::star::beans::XPropertySet* pSet) 
		throw (::com::sun::star::uno::RuntimeException);
	virtual  ::com::sun::star::uno::Reference<
            ::com::sun::star::accessibility::XAccessible>
        GetAccessibleCaption (const ::com::sun::star::uno::Reference<
            ::com::sun::star::drawing::XShape>& xShape)
			throw (::com::sun::star::uno::RuntimeException);
    ///=====  Internal  ========================================================
    void SetDrawBroadcaster();

    sal_Int32 GetCount() const;
    uno::Reference< XAccessible > Get(const ScAccessibleShapeData* pData) const;
    uno::Reference< XAccessible > Get(sal_Int32 nIndex) const;
	uno::Reference< XAccessible > GetAt(const awt::Point& rPoint) const;

	// gets the index of the shape starting on 0 (without the index of the table)
	// returns the selected shape
	sal_Bool IsSelected(sal_Int32 nIndex,
		com::sun::star::uno::Reference<com::sun::star::drawing::XShape>& rShape) const;

    sal_Bool SelectionChanged();

    void Select(sal_Int32 nIndex);
    void DeselectAll(); // deselect also the table
    void SelectAll();
    sal_Int32 GetSelectedCount() const;
    uno::Reference< XAccessible > GetSelected(sal_Int32 nSelectedChildIndex, sal_Bool bTabSelected) const;
    void Deselect(sal_Int32 nChildIndex);

    SdrPage* GetDrawPage() const;

    utl::AccessibleRelationSetHelper* GetRelationSet(const ScAddress* pAddress) const;

    void VisAreaChanged() const;
private:
    typedef std::vector<ScAccessibleShapeData*> SortedShapes;

    mutable SortedShapes maZOrderedShapes; // a null pointer represents the sheet in the correct order

    mutable ::accessibility::AccessibleShapeTreeInfo maShapeTreeInfo;
	mutable com::sun::star::uno::Reference<com::sun::star::view::XSelectionSupplier> xSelectionSupplier;
	mutable sal_uInt32 mnSdrObjCount;
	mutable sal_uInt32 mnShapesSelected;
    ScTabViewShell* mpViewShell;
    ScAccessibleDocument* mpAccessibleDocument;
    ScSplitPos meSplitPos;

    void FillShapes(std::vector < uno::Reference < drawing::XShape > >& rShapes) const;
    sal_Bool FindSelectedShapesChanges(const com::sun::star::uno::Reference<com::sun::star::drawing::XShapes>& xShapes, sal_Bool bCommitChange) const;
	void FillSelectionSupplier() const;

    ScAddress* GetAnchor(const uno::Reference<drawing::XShape>& xShape) const;
    uno::Reference<XAccessibleRelationSet> GetRelationSet(const ScAccessibleShapeData* pData) const;
    void CheckWhetherAnchorChanged(const uno::Reference<drawing::XShape>& xShape) const;
    void SetAnchor(const uno::Reference<drawing::XShape>& xShape, ScAccessibleShapeData* pData) const;
    void AddShape(const uno::Reference<drawing::XShape>& xShape, sal_Bool bCommitChange) const;
    void RemoveShape(const uno::Reference<drawing::XShape>& xShape) const;

    sal_Bool FindShape(const uno::Reference<drawing::XShape>& xShape, SortedShapes::iterator& rItr) const;

	sal_Int8 Compare(const ScAccessibleShapeData* pData1,
		const ScAccessibleShapeData* pData2) const;
};

ScChildrenShapes::ScChildrenShapes(ScAccessibleDocument* pAccessibleDocument, ScTabViewShell* pViewShell, ScSplitPos eSplitPos)
    :
    mnShapesSelected(0),
    mpViewShell(pViewShell),
    mpAccessibleDocument(pAccessibleDocument),
    meSplitPos(eSplitPos)
{
	FillSelectionSupplier();
    maZOrderedShapes.push_back(NULL); // add an element which represents the table

	GetCount(); // fill list with filtered shapes (no internal shapes)

	if (mnShapesSelected)
	{
		//set flag on every selected shape
		if (!xSelectionSupplier.is())
			throw uno::RuntimeException();

		uno::Reference<drawing::XShapes> xShapes(xSelectionSupplier->getSelection(), uno::UNO_QUERY);
		if (xShapes.is())
			FindSelectedShapesChanges(xShapes, sal_False);
	}
    if (pViewShell)
    {
	    SfxBroadcaster* pDrawBC = pViewShell->GetViewData()->GetDocument()->GetDrawBroadcaster();
	    if (pDrawBC)
        {
		    StartListening(*pDrawBC);

            maShapeTreeInfo.SetModelBroadcaster( new ScDrawModelBroadcaster(pViewShell->GetViewData()->GetDocument()->GetDrawLayer()) );
            maShapeTreeInfo.SetSdrView(pViewShell->GetViewData()->GetScDrawView());
            maShapeTreeInfo.SetController(NULL);
            maShapeTreeInfo.SetWindow(pViewShell->GetWindowByPos(meSplitPos));
            maShapeTreeInfo.SetViewForwarder(mpAccessibleDocument);
        }
    }
}

ScChildrenShapes::~ScChildrenShapes()
{
    std::for_each(maZOrderedShapes.begin(), maZOrderedShapes.end(), Destroy());
    if (mpViewShell)
    {
	    SfxBroadcaster* pDrawBC = mpViewShell->GetViewData()->GetDocument()->GetDrawBroadcaster();
	    if (pDrawBC)
		    EndListening(*pDrawBC);
    }
}

void ScChildrenShapes::SetDrawBroadcaster()
{
    if (mpViewShell)
    {
	    SfxBroadcaster* pDrawBC = mpViewShell->GetViewData()->GetDocument()->GetDrawBroadcaster();
	    if (pDrawBC)
        {
		    StartListening(*pDrawBC, sal_True);

            maShapeTreeInfo.SetModelBroadcaster( new ScDrawModelBroadcaster(mpViewShell->GetViewData()->GetDocument()->GetDrawLayer()) );
            maShapeTreeInfo.SetSdrView(mpViewShell->GetViewData()->GetScDrawView());
            maShapeTreeInfo.SetController(NULL);
            maShapeTreeInfo.SetWindow(mpViewShell->GetWindowByPos(meSplitPos));
            maShapeTreeInfo.SetViewForwarder(mpAccessibleDocument);
        }
    }
}

void ScChildrenShapes::Notify(SfxBroadcaster&, const SfxHint& rHint)
{
	if ( rHint.ISA( SdrHint ) )
	{
		const SdrHint* pSdrHint = PTR_CAST( SdrHint, &rHint );
        if (pSdrHint)
        {
            SdrObject* pObj = const_cast<SdrObject*>(pSdrHint->GetObject());
            if (pObj && /*(pObj->GetLayer() != SC_LAYER_INTERN) && */(pObj->GetPage() == GetDrawPage()) &&
                (pObj->GetPage() == pObj->GetObjList()) ) //#108480# only do something if the object lies direct on the page
            {
                switch (pSdrHint->GetKind())
                {
                    case HINT_OBJCHG :         // Objekt geaendert
                    {
                        uno::Reference<drawing::XShape> xShape (pObj->getUnoShape(), uno::UNO_QUERY);
                        if (xShape.is())
                        {
                            ScShapeDataLess aLess;
                            std::sort(maZOrderedShapes.begin(), maZOrderedShapes.end(), aLess); // sort, because the z index or layer could be changed
                            CheckWhetherAnchorChanged(xShape);
                        }
                    }
                    break;
                    case HINT_OBJINSERTED :    // Neues Zeichenobjekt eingefuegt
                    {
                        uno::Reference<drawing::XShape> xShape (pObj->getUnoShape(), uno::UNO_QUERY);
                        if (xShape.is())
                            AddShape(xShape, sal_True);
                    }
                    break;
                    case HINT_OBJREMOVED :     // Zeichenobjekt aus Liste entfernt
                    {
                        uno::Reference<drawing::XShape> xShape (pObj->getUnoShape(), uno::UNO_QUERY);
                        if (xShape.is())
                            RemoveShape(xShape);
                    }
                    break;
                    default :
                    {
                        // other events are not interesting
                    }
                    break;
                }
            }
        }
    }
}

sal_Bool ScChildrenShapes::ReplaceChild (::accessibility::AccessibleShape* pCurrentChild,
		const ::com::sun::star::uno::Reference< ::com::sun::star::drawing::XShape >& _rxShape,
		const long _nIndex, const ::accessibility::AccessibleShapeTreeInfo& _rShapeTreeInfo)
    throw (uno::RuntimeException)
{
	// create the new child
	::accessibility::AccessibleShape* pReplacement = ::accessibility::ShapeTypeHandler::Instance().CreateAccessibleObject (
		::accessibility::AccessibleShapeInfo ( _rxShape, pCurrentChild->getAccessibleParent(), this, _nIndex ),
		_rShapeTreeInfo
	);
	::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessible > xNewChild( pReplacement );	// keep this alive (do this before calling Init!)
	if ( pReplacement )
		pReplacement->Init();

    sal_Bool bResult(sal_False);
    if (pCurrentChild && pReplacement)
    {
        DBG_ASSERT(pCurrentChild->GetXShape().get() == pReplacement->GetXShape().get(), "XShape changes and should be inserted sorted");
        SortedShapes::iterator aItr;
        FindShape(pCurrentChild->GetXShape(), aItr);
        if (aItr != maZOrderedShapes.end() && (*aItr))
        {
            if ((*aItr)->pAccShape)
            {
                DBG_ASSERT((*aItr)->pAccShape == pCurrentChild, "wrong child found");
                AccessibleEventObject aEvent;
			    aEvent.EventId = AccessibleEventId::CHILD;
			    aEvent.Source = uno::Reference< XAccessibleContext >(mpAccessibleDocument);
                aEvent.OldValue <<= uno::makeAny(uno::Reference<XAccessible>(pCurrentChild));

			    mpAccessibleDocument->CommitChange(aEvent); // child is gone - event

                pCurrentChild->dispose();
            }
            (*aItr)->pAccShape = pReplacement;
            AccessibleEventObject aEvent;
			aEvent.EventId = AccessibleEventId::CHILD;
			aEvent.Source = uno::Reference< XAccessibleContext >(mpAccessibleDocument);
            aEvent.NewValue <<= uno::makeAny(uno::Reference<XAccessible>(pReplacement));

			mpAccessibleDocument->CommitChange(aEvent); // child is new - event
            bResult = sal_True;
        }
    }
    return bResult;
}

::accessibility::AccessibleControlShape * ScChildrenShapes::GetAccControlShapeFromModel(::com::sun::star::beans::XPropertySet* pSet) throw (::com::sun::star::uno::RuntimeException)
{
	sal_Int32 count = GetCount();
	for (sal_Int32 index=0;index<count;index++)
	{
		ScAccessibleShapeData* pShape = maZOrderedShapes[index];
            	if (pShape)
   	     	{
   	     		::accessibility::AccessibleShape* pAccShape = pShape->pAccShape;
          	 	if (pAccShape  && ::accessibility::ShapeTypeHandler::Instance().GetTypeId (pAccShape->GetXShape()) == ::accessibility::DRAWING_CONTROL)
          	  	{
				::accessibility::AccessibleControlShape *pCtlAccShape = static_cast < ::accessibility::AccessibleControlShape* >(pAccShape);
				if (pCtlAccShape && pCtlAccShape->GetControlModel() == pSet)
					return pCtlAccShape;
			  }
            	}
	}
	return NULL;
}
::com::sun::star::uno::Reference < ::com::sun::star::accessibility::XAccessible >
ScChildrenShapes::GetAccessibleCaption (const ::com::sun::star::uno::Reference < ::com::sun::star::drawing::XShape>& xShape)
			throw (::com::sun::star::uno::RuntimeException)
{
	sal_Int32 count = GetCount();
	for (sal_Int32 index=0;index<count;index++)
	{
		ScAccessibleShapeData* pShape = maZOrderedShapes[index];
			if (pShape && pShape->xShape == xShape )
   	     	{
				::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessible > xNewChild(  pShape->pAccShape );	
//				uno::Reference<XAccessible> xNewChild( pShape->pAccShape , uno::UNO_QUERY );					
				if(xNewChild.get())
				return xNewChild;
			}
	}
	return NULL;
}
sal_Int32 ScChildrenShapes::GetCount() const
{
	SdrPage* pDrawPage = GetDrawPage();
	if (pDrawPage && (maZOrderedShapes.size() == 1)) // the table is always in
	{
		mnSdrObjCount = pDrawPage->GetObjCount();
		maZOrderedShapes.reserve(mnSdrObjCount + 1); // the table is always in
		for (sal_uInt32 i = 0; i < mnSdrObjCount; ++i)
		{
			SdrObject* pObj = pDrawPage->GetObj(i);
			if (pObj/* && (pObj->GetLayer() != SC_LAYER_INTERN)*/)
			{
				uno::Reference< drawing::XShape > xShape (pObj->getUnoShape(), uno::UNO_QUERY);
                AddShape(xShape, sal_False); //inserts in the correct order
			}
		}
	}
	return maZOrderedShapes.size();
}

uno::Reference< XAccessible > ScChildrenShapes::Get(const ScAccessibleShapeData* pData) const
{
    if (!pData)
        return NULL;

	if (!pData->pAccShape)
	{
		::accessibility::ShapeTypeHandler& rShapeHandler = ::accessibility::ShapeTypeHandler::Instance();
        ::accessibility::AccessibleShapeInfo aShapeInfo(pData->xShape, mpAccessibleDocument, const_cast<ScChildrenShapes*>(this));
		pData->pAccShape = rShapeHandler.CreateAccessibleObject(
			aShapeInfo, maShapeTreeInfo);
		if (pData->pAccShape)
        {
			pData->pAccShape->acquire();
			pData->pAccShape->Init();
            if (pData->bSelected)
                pData->pAccShape->SetState(AccessibleStateType::SELECTED);
            if (!pData->bSelectable)
                pData->pAccShape->ResetState(AccessibleStateType::SELECTABLE);
            pData->pAccShape->SetRelationSet(GetRelationSet(pData));
        }
	}
    return pData->pAccShape;
 }

uno::Reference< XAccessible > ScChildrenShapes::Get(sal_Int32 nIndex) const
{
	if (maZOrderedShapes.size() <= 1)
		GetCount(); // fill list with filtered shapes (no internal shapes)

	if (static_cast<sal_uInt32>(nIndex) >= maZOrderedShapes.size())
		return NULL;

    return Get(maZOrderedShapes[nIndex]);
}

uno::Reference< XAccessible > ScChildrenShapes::GetAt(const awt::Point& rPoint) const
{
    uno::Reference<XAccessible> xAccessible;
    if(mpViewShell)
    {
        sal_Int32 i(maZOrderedShapes.size() - 1);
        sal_Bool bFound(sal_False);
        while (!bFound && i >= 0)
        {
            ScAccessibleShapeData* pShape = maZOrderedShapes[i];
            if (pShape)
            {
                if (!pShape->pAccShape)
                    Get(pShape);

                if (pShape->pAccShape)
                {
                    Point aPoint(VCLPoint(rPoint));
                    aPoint -= VCLRectangle(pShape->pAccShape->getBounds()).TopLeft();
                    if (pShape->pAccShape->containsPoint(AWTPoint(aPoint)))
                    {
                        xAccessible = pShape->pAccShape;
                        bFound = sal_True;
                    }
                }
                else
                {
                    DBG_ERRORFILE("I should have an accessible shape now!");
                }
            }
            else
                bFound = sal_True; // this is the sheet and it lies before the rest of the shapes which are background shapes

            --i;
        }
    }
    return xAccessible;
}

sal_Bool ScChildrenShapes::IsSelected(sal_Int32 nIndex,
						uno::Reference<drawing::XShape>& rShape) const
{
	sal_Bool bResult (sal_False);
	if (maZOrderedShapes.size() <= 1)
		GetCount(); // fill list with filtered shapes (no internal shapes)

	if (!xSelectionSupplier.is())
		throw uno::RuntimeException();

    if (!maZOrderedShapes[nIndex])
        return sal_False;

    bResult = maZOrderedShapes[nIndex]->bSelected;
    rShape = maZOrderedShapes[nIndex]->xShape;

#ifdef DBG_UTIL // test whether it is truly selected by a slower method
    uno::Reference< drawing::XShape > xReturnShape;
    sal_Bool bDebugResult(sal_False);
	uno::Reference<container::XIndexAccess> xIndexAccess;
	xSelectionSupplier->getSelection() >>= xIndexAccess;

	if (xIndexAccess.is())
	{
		sal_Int32 nCount(xIndexAccess->getCount());
		if (nCount)
		{
			uno::Reference< drawing::XShape > xShape;
            uno::Reference< drawing::XShape > xIndexShape = maZOrderedShapes[nIndex]->xShape;
			sal_Int32 i(0);
			while (!bDebugResult && (i < nCount))
			{
				xIndexAccess->getByIndex(i) >>= xShape;
				if (xShape.is() && (xIndexShape.get() == xShape.get()))
				{
					bDebugResult = sal_True;
					xReturnShape = xShape;
				}
				else
					++i;
			}
		}
	}
    DBG_ASSERT((bResult == bDebugResult) && ((bResult && (rShape.get() == xReturnShape.get())) || !bResult), "found the wrong shape or result");
#endif

    return bResult;
}

sal_Bool ScChildrenShapes::SelectionChanged()
{
    sal_Bool bResult(sal_False);
	if (!xSelectionSupplier.is())
		throw uno::RuntimeException();

	uno::Reference<drawing::XShapes> xShapes(xSelectionSupplier->getSelection(), uno::UNO_QUERY);

	bResult = FindSelectedShapesChanges(xShapes, sal_True);

    return bResult;
}

void ScChildrenShapes::Select(sal_Int32 nIndex)
{
	if (maZOrderedShapes.size() <= 1)
		GetCount(); // fill list with filtered shapes (no internal shapes)

	if (!xSelectionSupplier.is())
		throw uno::RuntimeException();

    if (!maZOrderedShapes[nIndex])
        return;

	uno::Reference<drawing::XShape> xShape;
	if (!IsSelected(nIndex, xShape) && maZOrderedShapes[nIndex]->bSelectable)
	{
		uno::Reference<drawing::XShapes> xShapes;
		xSelectionSupplier->getSelection() >>= xShapes;

		if (!xShapes.is())
			xShapes = new SvxShapeCollection();

		xShapes->add(maZOrderedShapes[nIndex]->xShape);

        try
        {
            xSelectionSupplier->select(uno::makeAny(xShapes));
		    maZOrderedShapes[nIndex]->bSelected = sal_True;
		    if (maZOrderedShapes[nIndex]->pAccShape)
			    maZOrderedShapes[nIndex]->pAccShape->SetState(AccessibleStateType::SELECTED);
        }
        catch (lang::IllegalArgumentException&)
        {
        }
	}
}

void ScChildrenShapes::DeselectAll()
{
	if (!xSelectionSupplier.is())
		throw uno::RuntimeException();

    sal_Bool bSomethingSelected(sal_True);
    try
    {
        xSelectionSupplier->select(uno::Any()); //deselects all
    }
    catch (lang::IllegalArgumentException&)
    {
        DBG_ERRORFILE("nothing selected before");
        bSomethingSelected = sal_False;
    }

    if (bSomethingSelected)
        std::for_each(maZOrderedShapes.begin(), maZOrderedShapes.end(), DeselectShape());
}

void ScChildrenShapes::SelectAll()
{
	if (!xSelectionSupplier.is())
		throw uno::RuntimeException();

	if (maZOrderedShapes.size() <= 1)
		GetCount(); // fill list with filtered shapes (no internal shapes)

    if (maZOrderedShapes.size() > 1)
    {
	    uno::Reference<drawing::XShapes> xShapes;
	    xShapes = new SvxShapeCollection();

        try
        {
            std::for_each(maZOrderedShapes.begin(), maZOrderedShapes.end(), SelectShape(xShapes));
    	    xSelectionSupplier->select(uno::makeAny(xShapes));
        }
        catch (lang::IllegalArgumentException&)
        {
            SelectionChanged(); // find all selected shapes and set the flags
        }
    }
}

void ScChildrenShapes::FillShapes(std::vector < uno::Reference < drawing::XShape > >& rShapes) const
{
	uno::Reference<container::XIndexAccess> xIndexAccess;
	xSelectionSupplier->getSelection() >>= xIndexAccess;

	if (xIndexAccess.is())
    {
        sal_uInt32 nCount(xIndexAccess->getCount());
        for (sal_uInt32 i = 0; i < nCount; ++i)
        {
    		uno::Reference<drawing::XShape> xShape;
            xIndexAccess->getByIndex(i) >>= xShape;
            if (xShape.is())
                rShapes.push_back(xShape);
        }
    }
}

sal_Int32 ScChildrenShapes::GetSelectedCount() const
{
	if (!xSelectionSupplier.is())
		throw uno::RuntimeException();

    std::vector < uno::Reference < drawing::XShape > > aShapes;
    FillShapes(aShapes);

    return aShapes.size();
}

uno::Reference< XAccessible > ScChildrenShapes::GetSelected(sal_Int32 nSelectedChildIndex, sal_Bool bTabSelected) const
{
    uno::Reference< XAccessible > xAccessible;

	if (maZOrderedShapes.size() <= 1)
		GetCount(); // fill list with shapes

    if (!bTabSelected)
    {
        std::vector < uno::Reference < drawing::XShape > > aShapes;
        FillShapes(aShapes);

		if(aShapes.size()<=0) return xAccessible;
        SortedShapes::iterator aItr;
        if (FindShape(aShapes[nSelectedChildIndex], aItr))
            xAccessible = Get(aItr - maZOrderedShapes.begin());
    }
    else
    {
        SortedShapes::iterator aItr = maZOrderedShapes.begin();
        SortedShapes::iterator aEndItr = maZOrderedShapes.end();
        sal_Bool bFound(sal_False);
        while(!bFound && aItr != aEndItr)
        {
            if (*aItr)
            {
                if ((*aItr)->bSelected)
                {
                    if (nSelectedChildIndex == 0)
                        bFound = sal_True;
                    else
                        --nSelectedChildIndex;
                }
            }
            else
            {
                if (nSelectedChildIndex == 0)
                    bFound = sal_True;
                else
                    --nSelectedChildIndex;
            }
            if (!bFound)
                ++aItr;
        }
        if (bFound && *aItr)
            xAccessible = (*aItr)->pAccShape;
    }

    return xAccessible;
}

void ScChildrenShapes::Deselect(sal_Int32 nChildIndex)
{
	uno::Reference<drawing::XShape> xShape;
	if (IsSelected(nChildIndex, xShape)) // returns false if it is the sheet
	{
		if (xShape.is())
		{
			uno::Reference<drawing::XShapes> xShapes;
			xSelectionSupplier->getSelection() >>= xShapes;
			if (xShapes.is())
				xShapes->remove(xShape);

            try
            {
			    xSelectionSupplier->select(uno::makeAny(xShapes));
            }
            catch (lang::IllegalArgumentException&)
            {
                DBG_ERRORFILE("something not selectable");
            }

			maZOrderedShapes[nChildIndex]->bSelected = sal_False;
			if (maZOrderedShapes[nChildIndex]->pAccShape)
				maZOrderedShapes[nChildIndex]->pAccShape->ResetState(AccessibleStateType::SELECTED);
		}
	}
}


SdrPage* ScChildrenShapes::GetDrawPage() const
{
	SCTAB nTab(mpAccessibleDocument->getVisibleTable());
	SdrPage* pDrawPage = NULL;
	if (mpViewShell)
	{
		ScDocument* pDoc = mpViewShell->GetViewData()->GetDocument();
		if (pDoc && pDoc->GetDrawLayer())
		{
			ScDrawLayer* pDrawLayer = pDoc->GetDrawLayer();
			if (pDrawLayer->HasObjects() && (pDrawLayer->GetPageCount() > nTab))
				pDrawPage = pDrawLayer->GetPage(static_cast<sal_uInt16>(static_cast<sal_Int16>(nTab)));
		}
	}
	return pDrawPage;
}

struct SetRelation
{
    const ScChildrenShapes* mpChildrenShapes;
    mutable utl::AccessibleRelationSetHelper* mpRelationSet;
    const ScAddress* mpAddress;
    SetRelation(const ScChildrenShapes* pChildrenShapes, const ScAddress* pAddress)
        :
        mpChildrenShapes(pChildrenShapes),
        mpRelationSet(NULL),
        mpAddress(pAddress)
    {
    }
	void operator() (const ScAccessibleShapeData* pAccShapeData) const
    {
	    if (pAccShapeData &&
            ((!pAccShapeData->pRelationCell && !mpAddress) ||
            (pAccShapeData->pRelationCell && mpAddress && (*(pAccShapeData->pRelationCell) == *mpAddress))))
        {
            if (!mpRelationSet)
                mpRelationSet = new utl::AccessibleRelationSetHelper();

            AccessibleRelation aRelation;
            aRelation.TargetSet.realloc(1);
            aRelation.TargetSet[0] = mpChildrenShapes->Get(pAccShapeData);
            aRelation.RelationType = AccessibleRelationType::CONTROLLER_FOR;

            mpRelationSet->AddRelation(aRelation);
        }
    }
};

utl::AccessibleRelationSetHelper* ScChildrenShapes::GetRelationSet(const ScAddress* pAddress) const
{
    SetRelation aSetRelation(this, pAddress);
    ::std::for_each(maZOrderedShapes.begin(), maZOrderedShapes.end(), aSetRelation);
    return aSetRelation.mpRelationSet;
}

sal_Bool ScChildrenShapes::FindSelectedShapesChanges(const uno::Reference<drawing::XShapes>& xShapes, sal_Bool /* bCommitChange */) const
{
    sal_Bool bResult(sal_False);
    SortedShapes aShapesList;
    uno::Reference<container::XIndexAccess> xIndexAcc(xShapes, uno::UNO_QUERY);
    if (xIndexAcc.is())
    {
        mnShapesSelected = xIndexAcc->getCount();
        for (sal_uInt32 i = 0; i < mnShapesSelected; ++i)
        {
			uno::Reference< drawing::XShape > xShape;
            xIndexAcc->getByIndex(i) >>= xShape;
            if (xShape.is())
            {
                ScAccessibleShapeData* pShapeData = new ScAccessibleShapeData();
                pShapeData->xShape = xShape;
                aShapesList.push_back(pShapeData);
            }
        }
    }
    else
        mnShapesSelected = 0;
	SdrObject *pFocusedObj = NULL;
	if( mnShapesSelected == 1 && aShapesList.size() == 1)
	{
		pFocusedObj = GetSdrObjectFromXShape(aShapesList[0]->xShape);
	}
    ScShapeDataLess aLess;
    std::sort(aShapesList.begin(), aShapesList.end(), aLess);
	SortedShapes vecSelectedShapeAdd;
	SortedShapes vecSelectedShapeRemove;
	sal_Bool bHasSelect=sal_False;
    SortedShapes::iterator aXShapesItr(aShapesList.begin());
    SortedShapes::const_iterator aXShapesEndItr(aShapesList.end());
    SortedShapes::iterator aDataItr(maZOrderedShapes.begin());
    SortedShapes::const_iterator aDataEndItr(maZOrderedShapes.end());
    SortedShapes::const_iterator aFocusedItr = aDataEndItr;
    while((aDataItr != aDataEndItr))
    {
        if (*aDataItr) // is it realy a shape or only the sheet
        {
            sal_Int8 nComp(0);
            if (aXShapesItr == aXShapesEndItr)
                nComp = -1; // simulate that the Shape is lower, so the selction state will be removed
            else
                nComp = Compare(*aDataItr, *aXShapesItr);
            if (nComp == 0)
            {
                if (!(*aDataItr)->bSelected)
                {
                    (*aDataItr)->bSelected = sal_True;
                    if ((*aDataItr)->pAccShape)
                    {
                        (*aDataItr)->pAccShape->SetState(AccessibleStateType::SELECTED);
                        (*aDataItr)->pAccShape->ResetState(AccessibleStateType::FOCUSED);
                        bResult = sal_True;
						vecSelectedShapeAdd.push_back((*aDataItr));
                    }
                    aFocusedItr = aDataItr;
                }
				else
				{
					 bHasSelect = sal_True;
				}
                ++aDataItr;
                ++aXShapesItr;
            }
            else if (nComp < 0)
            {
                if ((*aDataItr)->bSelected)
                {
                    (*aDataItr)->bSelected = sal_False;
                    if ((*aDataItr)->pAccShape)
                    {
                        (*aDataItr)->pAccShape->ResetState(AccessibleStateType::SELECTED);
                        (*aDataItr)->pAccShape->ResetState(AccessibleStateType::FOCUSED);
                        bResult = sal_True;
						vecSelectedShapeRemove.push_back(*aDataItr);
                    }
                }
                ++aDataItr;
            }
            else
            {
                DBG_ERRORFILE("here is a selected shape which is not in the childlist");
                ++aXShapesItr;
                --mnShapesSelected;
            }
        }
        else
            ++aDataItr;
    }
	bool bWinFocus=false;
	if (mpViewShell)
	{
		ScGridWindow* pWin = static_cast<ScGridWindow*>(mpViewShell->GetWindowByPos(meSplitPos));
		if (pWin)
		{
			bWinFocus = pWin->HasFocus();
		}
	}
	const SdrMarkList* pMarkList = NULL;
	SdrObject* pMarkedObj = NULL;
	SdrObject* pUpObj = NULL;
	sal_Bool bIsFocuseMarked = sal_True;
	if( mpViewShell && mnShapesSelected == 1 && bWinFocus)	
	{		
		ScDrawView* pScDrawView = mpViewShell->GetViewData()->GetScDrawView();
		if( pScDrawView )
		{
			if( pScDrawView->GetMarkedObjectList().GetMarkCount() == 1 )
			{
				pMarkList = &(pScDrawView->GetMarkedObjectList());
				pMarkedObj = pMarkList->GetMark(0)->GetMarkedSdrObj();
				uno::Reference< drawing::XShape > xMarkedXShape (pMarkedObj->getUnoShape(), uno::UNO_QUERY);
				if( aFocusedItr != aDataEndItr &&
					(*aFocusedItr)->xShape.is() && 
					xMarkedXShape.is() && 
					(*aFocusedItr)->xShape != xMarkedXShape )
					bIsFocuseMarked = sal_False;
			}
		}
	}
    //if ((aFocusedItr != aDataEndItr) && (*aFocusedItr)->pAccShape && (mnShapesSelected == 1))
    if ( bIsFocuseMarked && (aFocusedItr != aDataEndItr) && (*aFocusedItr)->pAccShape && (mnShapesSelected == 1) && bWinFocus)
	{
        (*aFocusedItr)->pAccShape->SetState(AccessibleStateType::FOCUSED);
	}
	else if( pFocusedObj && bWinFocus && pMarkList && pMarkList->GetMarkCount() == 1 && mnShapesSelected == 1 )
	{		
		if( pMarkedObj )
		{
			uno::Reference< drawing::XShape > xMarkedXShape (pMarkedObj->getUnoShape(), uno::UNO_QUERY);
			pUpObj = pMarkedObj->GetUpGroup();

			if( pMarkedObj == pFocusedObj )
			{
				if( pUpObj )
				{
					uno::Reference< drawing::XShape > xUpGroupXShape (pUpObj->getUnoShape(), uno::UNO_QUERY);
					uno::Reference < XAccessible > xAccGroupShape = 
						const_cast<ScChildrenShapes*>(this)->GetAccessibleCaption( xUpGroupXShape );
					if( xAccGroupShape.is() )
					{
						::accessibility::AccessibleShape* pAccGroupShape =  
							static_cast< ::accessibility::AccessibleShape* >(xAccGroupShape.get());
						if( pAccGroupShape )
						{
							sal_Int32 nCount =  pAccGroupShape->getAccessibleChildCount();
							for( sal_Int32 i = 0; i < nCount; i++ )
							{
								uno::Reference<XAccessible> xAccShape = pAccGroupShape->getAccessibleChild(i);
								if (xAccShape.is())
								{
									::accessibility::AccessibleShape* pChildAccShape =  static_cast< ::accessibility::AccessibleShape* >(xAccShape.get());
									uno::Reference< drawing::XShape > xChildShape = pChildAccShape->GetXShape();
									if (xChildShape == xMarkedXShape)
									{
			                			pChildAccShape->SetState(AccessibleStateType::FOCUSED);
									}
									else
									{
										pChildAccShape->ResetState(AccessibleStateType::FOCUSED);
									}
								}
							}
						}
					}			
				}
			}
		}
	}
	if (vecSelectedShapeAdd.size() >= 10 )
	{
		AccessibleEventObject aEvent;
		aEvent.EventId = AccessibleEventId::SELECTION_CHANGED_WITHIN;
		aEvent.Source = uno::Reference< XAccessible >(mpAccessibleDocument);
		mpAccessibleDocument->CommitChange(aEvent);
	}
	else
	{
		SortedShapes::iterator vi = vecSelectedShapeAdd.begin();
		for (; vi != vecSelectedShapeAdd.end() ; ++vi )
		{
			AccessibleEventObject aEvent;
			if (bHasSelect)
			{
				aEvent.EventId = AccessibleEventId::SELECTION_CHANGED_ADD;
			}
			else
			{
				aEvent.EventId = AccessibleEventId::SELECTION_CHANGED;
			}
			aEvent.Source = uno::Reference< XAccessible >(mpAccessibleDocument);
			uno::Reference< XAccessible > xChild( (*vi)->pAccShape);
			aEvent.NewValue <<= xChild;
			mpAccessibleDocument->CommitChange(aEvent);
		}
	}
	SortedShapes::iterator vi = vecSelectedShapeRemove.begin();
	for (; vi != vecSelectedShapeRemove.end() ; ++vi )
	{
		AccessibleEventObject aEvent;
		aEvent.EventId =  AccessibleEventId::SELECTION_CHANGED_REMOVE;		
		aEvent.Source = uno::Reference< XAccessible >(mpAccessibleDocument);
		uno::Reference< XAccessible > xChild( (*vi)->pAccShape);
		aEvent.NewValue <<= xChild;
		mpAccessibleDocument->CommitChange(aEvent);
	}
    std::for_each(aShapesList.begin(), aShapesList.end(), Destroy());

    return bResult;
}

void ScChildrenShapes::FillSelectionSupplier() const
{
	if (!xSelectionSupplier.is() && mpViewShell)
	{
		SfxViewFrame* pViewFrame = mpViewShell->GetViewFrame();
		if (pViewFrame)
		{
			xSelectionSupplier = uno::Reference<view::XSelectionSupplier>(pViewFrame->GetFrame().GetController(), uno::UNO_QUERY);
			if (xSelectionSupplier.is())
			{
                if (mpAccessibleDocument)
                    xSelectionSupplier->addSelectionChangeListener(mpAccessibleDocument);
				uno::Reference<drawing::XShapes> xShapes (xSelectionSupplier->getSelection(), uno::UNO_QUERY);
				if (xShapes.is())
					mnShapesSelected = xShapes->getCount();
			}
		}
	}
}

ScAddress* ScChildrenShapes::GetAnchor(const uno::Reference<drawing::XShape>& xShape) const
{
    ScAddress* pAddress = NULL;
    if (mpViewShell)
    {
	    SvxShape* pShapeImp = SvxShape::getImplementation(xShape);
        uno::Reference<beans::XPropertySet> xShapeProp(xShape, uno::UNO_QUERY);
	    if (pShapeImp && xShapeProp.is())
	    {
		    SdrObject *pSdrObj = pShapeImp->GetSdrObject();
		    if (pSdrObj)
		    {
			    if (ScDrawLayer::GetAnchor(pSdrObj) == SCA_CELL)
			    {
                    ScDocument* pDoc = mpViewShell->GetViewData()->GetDocument();
				    if (pDoc)
				    {
                        rtl::OUString sCaptionShape(RTL_CONSTASCII_USTRINGPARAM("com.sun.star.drawing.CaptionShape"));
					    awt::Point aPoint(xShape->getPosition());
					    awt::Size aSize(xShape->getSize());
					    rtl::OUString sType(xShape->getShapeType());
					    Rectangle aRectangle(aPoint.X, aPoint.Y, aPoint.X + aSize.Width, aPoint.Y + aSize.Height);
					    if ( sType.equals(sCaptionShape) )
					    {
						    awt::Point aRelativeCaptionPoint;
                            rtl::OUString sCaptionPoint( RTL_CONSTASCII_USTRINGPARAM( "CaptionPoint" ));
						    xShapeProp->getPropertyValue( sCaptionPoint ) >>= aRelativeCaptionPoint;
						    Point aCoreRelativeCaptionPoint(aRelativeCaptionPoint.X, aRelativeCaptionPoint.Y);
						    Point aCoreAbsoluteCaptionPoint(aPoint.X, aPoint.Y);
						    aCoreAbsoluteCaptionPoint += aCoreRelativeCaptionPoint;
						    aRectangle.Union(Rectangle(aCoreAbsoluteCaptionPoint, aCoreAbsoluteCaptionPoint));
					    }
					    ScRange aRange = pDoc->GetRange(mpAccessibleDocument->getVisibleTable(), aRectangle);
                        pAddress = new ScAddress(aRange.aStart);
				    }
			    }
//			    else
//				    do nothing, because it is always a NULL Pointer
		    }
	    }
    }

    return pAddress;
}

uno::Reference<XAccessibleRelationSet> ScChildrenShapes::GetRelationSet(const ScAccessibleShapeData* pData) const
{
    utl::AccessibleRelationSetHelper* pRelationSet = new utl::AccessibleRelationSetHelper();

    if(pData && pRelationSet && mpAccessibleDocument)
    {
        uno::Reference<XAccessible> xAccessible = mpAccessibleDocument->GetAccessibleSpreadsheet(); // should be the current table
        if (pData->pRelationCell && xAccessible.is())
        {
            uno::Reference<XAccessibleTable> xAccTable (xAccessible->getAccessibleContext(), uno::UNO_QUERY);
            if (xAccTable.is())
                xAccessible = xAccTable->getAccessibleCellAt(pData->pRelationCell->Row(), pData->pRelationCell->Col());
        }
        AccessibleRelation aRelation;
        aRelation.TargetSet.realloc(1);
        aRelation.TargetSet[0] = xAccessible;
        aRelation.RelationType = AccessibleRelationType::CONTROLLED_BY;
        pRelationSet->AddRelation(aRelation);
    }

    return pRelationSet;
}

void ScChildrenShapes::CheckWhetherAnchorChanged(const uno::Reference<drawing::XShape>& xShape) const
{
    SortedShapes::iterator aItr;
    if (FindShape(xShape, aItr))
        SetAnchor(xShape, *aItr);
}

void ScChildrenShapes::SetAnchor(const uno::Reference<drawing::XShape>& xShape, ScAccessibleShapeData* pData) const
{
    if (pData)
    {
        ScAddress* pAddress = GetAnchor(xShape);
        if ((pAddress && pData->pRelationCell && (*pAddress != *(pData->pRelationCell))) ||
            (!pAddress && pData->pRelationCell) || (pAddress && !pData->pRelationCell))
        {
            if (pData->pRelationCell)
                delete pData->pRelationCell;
            pData->pRelationCell = pAddress;
            if (pData->pAccShape)
                pData->pAccShape->SetRelationSet(GetRelationSet(pData));
        }
    }
}

void ScChildrenShapes::AddShape(const uno::Reference<drawing::XShape>& xShape, sal_Bool bCommitChange) const
{
    SortedShapes::iterator aFindItr;
    if (!FindShape(xShape, aFindItr))
    {
        ScAccessibleShapeData* pShape = new ScAccessibleShapeData();
        pShape->xShape = xShape;
        SortedShapes::iterator aNewItr = maZOrderedShapes.insert(aFindItr, pShape);
        SetAnchor(xShape, pShape);

        uno::Reference< beans::XPropertySet > xShapeProp(xShape, uno::UNO_QUERY);
        if (xShapeProp.is())
        {
            uno::Any aPropAny = xShapeProp->getPropertyValue(rtl::OUString(RTL_CONSTASCII_USTRINGPARAM(  "LayerID" )));
		    sal_Int16 nLayerID = 0;
		    if( aPropAny >>= nLayerID )
		    {
                if( (nLayerID == SC_LAYER_INTERN) || (nLayerID == SC_LAYER_HIDDEN) )
                    pShape->bSelectable = sal_False;
                else
                    pShape->bSelectable = sal_True;
            }
        }


        if (!xSelectionSupplier.is())
		    throw uno::RuntimeException();

        uno::Reference<container::XEnumerationAccess> xEnumAcc(xSelectionSupplier->getSelection(), uno::UNO_QUERY);
	    if (xEnumAcc.is())
	    {
            uno::Reference<container::XEnumeration> xEnum = xEnumAcc->createEnumeration();
            if (xEnum.is())
            {
                uno::Reference<drawing::XShape> xSelectedShape;
                sal_Bool bFound(sal_False);
                while (!bFound && xEnum->hasMoreElements())
                {
                    xEnum->nextElement() >>= xSelectedShape;
                    if (xShape.is() && (xShape.get() == xSelectedShape.get()))
                    {
                        pShape->bSelected = sal_True;
                        bFound = sal_True;
                    }
                }
            }
        }
        if (mpAccessibleDocument && bCommitChange)
        {
			AccessibleEventObject aEvent;
			aEvent.EventId = AccessibleEventId::CHILD;
			aEvent.Source = uno::Reference< XAccessibleContext >(mpAccessibleDocument);
			aEvent.NewValue <<= Get(aNewItr - maZOrderedShapes.begin());

			mpAccessibleDocument->CommitChange(aEvent); // new child - event
        }
    }
    else
    {
        DBG_ERRORFILE("shape is always in the list");
    }
}

void ScChildrenShapes::RemoveShape(const uno::Reference<drawing::XShape>& xShape) const
{
    SortedShapes::iterator aItr;
    if (FindShape(xShape, aItr))
    {
        if (mpAccessibleDocument)
        {
            uno::Reference<XAccessible> xOldAccessible (Get(aItr - maZOrderedShapes.begin()));

            delete *aItr;
            maZOrderedShapes.erase(aItr);

            AccessibleEventObject aEvent;
			aEvent.EventId = AccessibleEventId::CHILD;
			aEvent.Source = uno::Reference< XAccessibleContext >(mpAccessibleDocument);
            aEvent.OldValue <<= uno::makeAny(xOldAccessible);

			mpAccessibleDocument->CommitChange(aEvent); // child is gone - event
        }
        else
        {
            delete *aItr;
            maZOrderedShapes.erase(aItr);
        }
    }
    else
    {
        DBG_ERRORFILE("shape was not in internal list");
    }
}

sal_Bool ScChildrenShapes::FindShape(const uno::Reference<drawing::XShape>& xShape, ScChildrenShapes::SortedShapes::iterator& rItr) const
{
    sal_Bool bResult(sal_False);
    ScAccessibleShapeData aShape;
    aShape.xShape = xShape;
    ScShapeDataLess aLess;
    rItr = std::lower_bound(maZOrderedShapes.begin(), maZOrderedShapes.end(), &aShape, aLess);
    if ((rItr != maZOrderedShapes.end()) && (*rItr != NULL) && ((*rItr)->xShape.get() == xShape.get()))
        bResult = sal_True; // if the shape is found

#ifdef DBG_UTIL // test whether it finds truly the correct shape (perhaps it is not really sorted)
    SortedShapes::iterator aDebugItr = maZOrderedShapes.begin();
    SortedShapes::iterator aEndItr = maZOrderedShapes.end();
    sal_Bool bFound(sal_False);
    while (!bFound && aDebugItr != aEndItr)
    {
        if (*aDebugItr && ((*aDebugItr)->xShape.get() == xShape.get()))
            bFound = sal_True;
        else
            ++aDebugItr;
    }
    sal_Bool bResult2 = (aDebugItr != maZOrderedShapes.end());
    DBG_ASSERT((bResult == bResult2) && ((bResult && (rItr == aDebugItr)) || !bResult), "wrong Shape found");
#endif
    return bResult;
}

sal_Int8 ScChildrenShapes::Compare(const ScAccessibleShapeData* pData1,
		const ScAccessibleShapeData* pData2) const
{
    ScShapeDataLess aLess;

    sal_Bool bResult1(aLess(pData1, pData2));
    sal_Bool bResult2(aLess(pData2, pData1));

	sal_Int8 nResult(0);
    if (!bResult1 && bResult2)
        nResult = 1;
    else if (bResult1 && !bResult2)
        nResult = -1;

	return nResult;
}

struct ScVisAreaChanged
{
    ScAccessibleDocument* mpAccDoc;
    ScVisAreaChanged(ScAccessibleDocument* pAccDoc) : mpAccDoc(pAccDoc) {}
	void operator() (const ScAccessibleShapeData* pAccShapeData) const
    {
	    if (pAccShapeData && pAccShapeData->pAccShape)
        {
            pAccShapeData->pAccShape->ViewForwarderChanged(::accessibility::IAccessibleViewForwarderListener::VISIBLE_AREA, mpAccDoc);
        }
    }
};

void ScChildrenShapes::VisAreaChanged() const
{
    ScVisAreaChanged aVisAreaChanged(mpAccessibleDocument);
    std::for_each(maZOrderedShapes.begin(), maZOrderedShapes.end(), aVisAreaChanged);
}

// ============================================================================

ScAccessibleDocument::ScAccessibleDocument(
        const uno::Reference<XAccessible>& rxParent,
		ScTabViewShell* pViewShell,
		ScSplitPos eSplitPos)
	: ScAccessibleDocumentBase(rxParent),
	mpViewShell(pViewShell),
	meSplitPos(eSplitPos),
	mpAccessibleSpreadsheet(NULL),
    mpChildrenShapes(NULL),
    mpTempAccEdit(NULL),
	mbCompleteSheetSelected(sal_False)
{
	if (pViewShell)
    {
		pViewShell->AddAccessibilityObject(*this);
	    Window *pWin = pViewShell->GetWindowByPos(eSplitPos);
	    if( pWin )
	    {
		    pWin->AddChildEventListener( LINK( this, ScAccessibleDocument, WindowChildEventListener ));
		    sal_uInt16 nCount =   pWin->GetChildCount();
		    for( sal_uInt16 i=0; i < nCount; ++i )
		    {
			    Window *pChildWin = pWin->GetChild( i );
			    if( pChildWin &&
				    AccessibleRole::EMBEDDED_OBJECT == pChildWin->GetAccessibleRole() )
				    AddChild( pChildWin->GetAccessible(), sal_False );
		    }
	    }
        if (pViewShell->GetViewData()->HasEditView( eSplitPos ))
        {
            uno::Reference<XAccessible> xAcc = new ScAccessibleEditObject(this, pViewShell->GetViewData()->GetEditView(eSplitPos),
                pViewShell->GetWindowByPos(eSplitPos), GetCurrentCellName(), GetCurrentCellDescription(),
                CellInEditMode);
            AddChild(xAcc, sal_False);
        }
    }
    maVisArea = GetVisibleArea_Impl();
}

void ScAccessibleDocument::Init()
{
    if(!mpChildrenShapes)
        mpChildrenShapes = new ScChildrenShapes(this, mpViewShell, meSplitPos);
}

ScAccessibleDocument::~ScAccessibleDocument(void)
{
	if (!ScAccessibleContextBase::IsDefunc() && !rBHelper.bInDispose)
	{
		// increment refcount to prevent double call off dtor
		osl_incrementInterlockedCount( &m_refCount );
		dispose();
	}
}

void SAL_CALL ScAccessibleDocument::disposing()
{
    ScUnoGuard aGuard;
	FreeAccessibleSpreadsheet();
	if (mpViewShell)
	{
        Window *pWin = mpViewShell->GetWindowByPos(meSplitPos);
	    if( pWin )
		    pWin->RemoveChildEventListener( LINK( this, ScAccessibleDocument, WindowChildEventListener ));

        mpViewShell->RemoveAccessibilityObject(*this);
		mpViewShell = NULL;
	}
    if (mpChildrenShapes)
        DELETEZ(mpChildrenShapes);

	ScAccessibleDocumentBase::disposing();
}

void SAL_CALL ScAccessibleDocument::disposing( const lang::EventObject& /* Source */ )
		throw (uno::RuntimeException)
{
	disposing();
}

	//=====  SfxListener  =====================================================

IMPL_LINK( ScAccessibleDocument, WindowChildEventListener, VclSimpleEvent*, pEvent )
{
	DBG_ASSERT( pEvent && pEvent->ISA( VclWindowEvent ), "Unknown WindowEvent!" );
	if ( pEvent && pEvent->ISA( VclWindowEvent ) )
	{
		VclWindowEvent *pVclEvent = static_cast< VclWindowEvent * >( pEvent );
		DBG_ASSERT( pVclEvent->GetWindow(), "Window???" );
		switch ( pVclEvent->GetId() )
		{
        case VCLEVENT_WINDOW_SHOW:  // send create on show for direct accessible children
			{
				Window* pChildWin = static_cast < Window * >( pVclEvent->GetData() );
				if( pChildWin && AccessibleRole::EMBEDDED_OBJECT == pChildWin->GetAccessibleRole() )
				{
					AddChild( pChildWin->GetAccessible(), sal_True );
				}
			}
			break;
        case VCLEVENT_WINDOW_HIDE:  // send destroy on hide for direct accessible children
			{
				Window* pChildWin = static_cast < Window * >( pVclEvent->GetData() );
				if( pChildWin && AccessibleRole::EMBEDDED_OBJECT == pChildWin->GetAccessibleRole() )
				{
					RemoveChild( pChildWin->GetAccessible(), sal_True );
				}
			}
			break;
		}
	}
	return 0;
}

void ScAccessibleDocument::Notify( SfxBroadcaster& rBC, const SfxHint& rHint )
{
	if (rHint.ISA( ScAccGridWinFocusLostHint ) )
	{
		const ScAccGridWinFocusLostHint& rRef = (const ScAccGridWinFocusLostHint&)rHint;
		if (rRef.GetOldGridWin() == meSplitPos)
        {
            if (mxTempAcc.is() && mpTempAccEdit)
                mpTempAccEdit->LostFocus();
            else if (mpAccessibleSpreadsheet)
                mpAccessibleSpreadsheet->LostFocus();
            else
                CommitFocusLost();
        }
	}
    else if (rHint.ISA( ScAccGridWinFocusGotHint ) )
	{
		const ScAccGridWinFocusGotHint& rRef = (const ScAccGridWinFocusGotHint&)rHint;
		if (rRef.GetNewGridWin() == meSplitPos)
        {
			uno::Reference<XAccessible> xAccessible;
			if (mpChildrenShapes)
			{
				sal_Bool bTabMarked(IsTableSelected());				
				xAccessible = mpChildrenShapes->GetSelected(0, bTabMarked);				
			}
			if( xAccessible.is() )
			{				
				uno::Any aNewValue;
				aNewValue<<=AccessibleStateType::FOCUSED;
				static_cast< ::accessibility::AccessibleShape* >(xAccessible.get())->
					CommitChange(AccessibleEventId::STATE_CHANGED,
								aNewValue,
								uno::Any() );
			}
			else
			{
            if (mxTempAcc.is() && mpTempAccEdit)
                mpTempAccEdit->GotFocus();
            else if (mpAccessibleSpreadsheet)
                mpAccessibleSpreadsheet->GotFocus();
            else
                CommitFocusGained();
			}            
        }
	}
	else if (rHint.ISA( SfxSimpleHint ))
	{
		const SfxSimpleHint& rRef = (const SfxSimpleHint&)rHint;
		// only notify if child exist, otherwise it is not necessary
		if ((rRef.GetId() == SC_HINT_ACC_TABLECHANGED) &&
			mpAccessibleSpreadsheet)
		{
			FreeAccessibleSpreadsheet();
            if (mpChildrenShapes)
                DELETEZ(mpChildrenShapes);

            // #124567# Accessibility: Shapes / form controls after reload not accessible
            if ( !mpChildrenShapes )
            {
                mpChildrenShapes = new ScChildrenShapes( this, mpViewShell, meSplitPos );
            }
			//Invoke Init() to rebuild the mpChildrenShapes variable
			this->Init();
			AccessibleEventObject aEvent;
			aEvent.EventId = AccessibleEventId::INVALIDATE_ALL_CHILDREN;
			aEvent.Source = uno::Reference< XAccessibleContext >(this);
			CommitChange(aEvent); // all childs changed
			if (mpAccessibleSpreadsheet)
				mpAccessibleSpreadsheet->FireFirstCellFocus();
		}
        else if (rRef.GetId() == SC_HINT_ACC_MAKEDRAWLAYER)
        {
            if (mpChildrenShapes)
                mpChildrenShapes->SetDrawBroadcaster();
        }
        else if ((rRef.GetId() == SC_HINT_ACC_ENTEREDITMODE)) // this event comes only on creating edit field of a cell
        {
            if (mpViewShell && mpViewShell->GetViewData()->HasEditView(meSplitPos))
            {
				EditEngine* pEditEng = mpViewShell->GetViewData()->GetEditView(meSplitPos)->GetEditEngine();
				if (pEditEng && pEditEng->GetUpdateMode())
				{
					mpTempAccEdit = new ScAccessibleEditObject(this, mpViewShell->GetViewData()->GetEditView(meSplitPos),
						mpViewShell->GetWindowByPos(meSplitPos), GetCurrentCellName(),
						rtl::OUString(String(ScResId(STR_ACC_EDITLINE_DESCR))), CellInEditMode);
					uno::Reference<XAccessible> xAcc = mpTempAccEdit;

					AddChild(xAcc, sal_True);

					if (mpAccessibleSpreadsheet)
						mpAccessibleSpreadsheet->LostFocus();
					else
						CommitFocusLost();

					mpTempAccEdit->GotFocus();
				}
            }
        }
        else if (rRef.GetId() == SC_HINT_ACC_LEAVEEDITMODE)
        {
            if (mxTempAcc.is())
            {
                if (mpTempAccEdit)
                    mpTempAccEdit->LostFocus();

                mpTempAccEdit = NULL;
                RemoveChild(mxTempAcc, sal_True);
                if (mpAccessibleSpreadsheet && mpViewShell->IsActive())
                    mpAccessibleSpreadsheet->GotFocus();
                else if( mpViewShell->IsActive())
                    CommitFocusGained();
            }
        }
        else if ((rRef.GetId() == SC_HINT_ACC_VISAREACHANGED) || (rRef.GetId() == SC_HINT_ACC_WINDOWRESIZED))
        {
            Rectangle aOldVisArea(maVisArea);
            maVisArea = GetVisibleArea_Impl();

            if (maVisArea != aOldVisArea)
            {
                if (maVisArea.GetSize() != aOldVisArea.GetSize())
                {
			        AccessibleEventObject aEvent;
			        aEvent.EventId = AccessibleEventId::BOUNDRECT_CHANGED;
			        aEvent.Source = uno::Reference< XAccessibleContext >(this);

			        CommitChange(aEvent);

                    if (mpAccessibleSpreadsheet)
                        mpAccessibleSpreadsheet->BoundingBoxChanged();
                }
                else if (mpAccessibleSpreadsheet)
                {
                    mpAccessibleSpreadsheet->VisAreaChanged();
                }
                if (mpChildrenShapes)
                    mpChildrenShapes->VisAreaChanged();
            }
        }
	}

	ScAccessibleDocumentBase::Notify(rBC, rHint);
}

void SAL_CALL ScAccessibleDocument::selectionChanged( const lang::EventObject& /* aEvent */ )
		throw (uno::RuntimeException)
{
	sal_Bool bSelectionChanged(sal_False);
	if (mpAccessibleSpreadsheet)
	{
		sal_Bool bOldSelected(mbCompleteSheetSelected);
		mbCompleteSheetSelected = IsTableSelected();
		if (bOldSelected != mbCompleteSheetSelected)
		{
			mpAccessibleSpreadsheet->CompleteSelectionChanged(mbCompleteSheetSelected);
			bSelectionChanged = sal_True;
		}
	}

    if (mpChildrenShapes && mpChildrenShapes->SelectionChanged())
        bSelectionChanged = sal_True;

	if (bSelectionChanged)
	{
		AccessibleEventObject aEvent;
		aEvent.EventId = AccessibleEventId::SELECTION_CHANGED;
		aEvent.Source = uno::Reference< XAccessibleContext >(this);

		CommitChange(aEvent);
	}
    if(mpChildrenShapes )
	{
		mpChildrenShapes->SelectionChanged();
	}
}

	//=====  XInterface  =====================================================

uno::Any SAL_CALL ScAccessibleDocument::queryInterface( uno::Type const & rType )
	throw (uno::RuntimeException)
{
	uno::Any aAnyTmp;
	if(rType == ::getCppuType((com::sun::star::uno::Reference<XAccessibleGetAccFlowTo> *)NULL) )
       {
	     com::sun::star::uno::Reference<XAccessibleGetAccFlowTo> AccFromXShape = this;
            aAnyTmp <<= AccFromXShape;
	     return aAnyTmp;
       }
	uno::Any aAny (ScAccessibleDocumentImpl::queryInterface(rType));
	return aAny.hasValue() ? aAny : ScAccessibleContextBase::queryInterface(rType);
}

void SAL_CALL ScAccessibleDocument::acquire()
	throw ()
{
	ScAccessibleContextBase::acquire();
}

void SAL_CALL ScAccessibleDocument::release()
	throw ()
{
	ScAccessibleContextBase::release();
}

	//=====  XAccessibleComponent  ============================================

uno::Reference< XAccessible > SAL_CALL ScAccessibleDocument::getAccessibleAtPoint(
		const awt::Point& rPoint )
		throw (uno::RuntimeException)
{
	uno::Reference<XAccessible> xAccessible = NULL;
    if (containsPoint(rPoint))
    {
    	ScUnoGuard aGuard;
        IsObjectValid();
        if (mpChildrenShapes)
            xAccessible = mpChildrenShapes->GetAt(rPoint);
	    if(!xAccessible.is())
        {
            if (mxTempAcc.is())
            {
                uno::Reference< XAccessibleContext > xCont(mxTempAcc->getAccessibleContext());
                uno::Reference< XAccessibleComponent > xComp(xCont, uno::UNO_QUERY);
                if (xComp.is())
                {
                    Rectangle aBound(VCLRectangle(xComp->getBounds()));
                    if (aBound.IsInside(VCLPoint(rPoint)))
                        xAccessible = mxTempAcc;
                }
            }
            if (!xAccessible.is())
		        xAccessible = GetAccessibleSpreadsheet();
        }
    }
	return xAccessible;
}

void SAL_CALL ScAccessibleDocument::grabFocus(  )
		throw (uno::RuntimeException)
{
	ScUnoGuard aGuard;
    IsObjectValid();
	if (getAccessibleParent().is())
	{
		uno::Reference<XAccessibleComponent> xAccessibleComponent(getAccessibleParent()->getAccessibleContext(), uno::UNO_QUERY);
		if (xAccessibleComponent.is())
		{
			xAccessibleComponent->grabFocus();
			// grab only focus if it does not have the focus and it is not hidden
			if (mpViewShell && mpViewShell->GetViewData() &&
				(mpViewShell->GetViewData()->GetActivePart() != meSplitPos) &&
				mpViewShell->GetWindowByPos(meSplitPos)->IsVisible())
			{
				mpViewShell->ActivatePart(meSplitPos);
			}
		}
	}
}

	//=====  XAccessibleContext  ==============================================

    ///	Return the number of currently visible children.
sal_Int32 SAL_CALL
    ScAccessibleDocument::getAccessibleChildCount(void)
    throw (uno::RuntimeException)
{
	ScUnoGuard aGuard;
    IsObjectValid();
    sal_Int32 nCount(1);
    if (mpChildrenShapes)
        nCount = mpChildrenShapes->GetCount(); // returns the count of the shapes inclusive the table

    if (mxTempAcc.is())
        ++nCount;

	return nCount;
}

    ///	Return the specified child or NULL if index is invalid.
uno::Reference<XAccessible> SAL_CALL
    ScAccessibleDocument::getAccessibleChild(sal_Int32 nIndex)
    throw (uno::RuntimeException,
		lang::IndexOutOfBoundsException)
{
	ScUnoGuard aGuard;
    IsObjectValid();
	uno::Reference<XAccessible> xAccessible;
	if (nIndex >= 0)
	{
        sal_Int32 nCount(1);
        if (mpChildrenShapes)
        {
            xAccessible = mpChildrenShapes->Get(nIndex); // returns NULL if it is the table or out of range
            nCount = mpChildrenShapes->GetCount(); //there is always a table
        }
        if (!xAccessible.is())
        {
            if (nIndex < nCount)
                xAccessible = GetAccessibleSpreadsheet();
            else if (nIndex == nCount && mxTempAcc.is())
                xAccessible = mxTempAcc;
        }
	}

    if (!xAccessible.is())
        throw lang::IndexOutOfBoundsException();

	return xAccessible;
}

    ///	Return the set of current states.
uno::Reference<XAccessibleStateSet> SAL_CALL
    ScAccessibleDocument::getAccessibleStateSet(void)
    throw (uno::RuntimeException)
{
	ScUnoGuard aGuard;
	uno::Reference<XAccessibleStateSet> xParentStates;
	if (getAccessibleParent().is())
	{
		uno::Reference<XAccessibleContext> xParentContext = getAccessibleParent()->getAccessibleContext();
		xParentStates = xParentContext->getAccessibleStateSet();
	}
	utl::AccessibleStateSetHelper* pStateSet = new utl::AccessibleStateSetHelper();
	if (IsDefunc(xParentStates))
		pStateSet->AddState(AccessibleStateType::DEFUNC);
    else
    {
	    if (IsEditable(xParentStates))
		    pStateSet->AddState(AccessibleStateType::EDITABLE);
	    pStateSet->AddState(AccessibleStateType::ENABLED);
	    pStateSet->AddState(AccessibleStateType::OPAQUE);
	    if (isShowing())
		    pStateSet->AddState(AccessibleStateType::SHOWING);
	    if (isVisible())
		    pStateSet->AddState(AccessibleStateType::VISIBLE);
    }
	return pStateSet;
}

::rtl::OUString SAL_CALL
    ScAccessibleDocument::getAccessibleName(void)
    throw (::com::sun::star::uno::RuntimeException)
{
	rtl::OUString sName = String(ScResId(STR_ACC_DOC_SPREADSHEET));
	ScDocument* pScDoc = GetDocument();
	if ( pScDoc )
	{
		rtl::OUString sFileName = pScDoc->getDocAccTitle();
		if ( !sFileName.getLength() )
		{
			SfxObjectShell* pObjSh = pScDoc->GetDocumentShell();
			if ( pObjSh )
			{
				sFileName = pObjSh->GetTitle( SFX_TITLE_APINAME );
			}
		}
		rtl::OUString sReadOnly;
		if (pScDoc->getDocReadOnly())
		{
			sReadOnly = String(ScResId(STR_ACC_DOC_SPREADSHEET_READONLY));
		}
		if ( sFileName.getLength() )
		{
			sName = sFileName + sReadOnly + rtl::OUString(RTL_CONSTASCII_USTRINGPARAM(" - ")) + sName;
		}
	}
	return sName;
}
	///=====  XAccessibleSelection  ===========================================

void SAL_CALL
	ScAccessibleDocument::selectAccessibleChild( sal_Int32 nChildIndex )
		throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
{
	ScUnoGuard aGuard;
    IsObjectValid();

    if (mpChildrenShapes)
    {
        sal_Int32 nCount(mpChildrenShapes->GetCount()); //all shapes and the table
        if (mxTempAcc.is())
            ++nCount;
        if (nChildIndex < 0 || nChildIndex >= nCount)
            throw lang::IndexOutOfBoundsException();

        uno::Reference < XAccessible > xAccessible = mpChildrenShapes->Get(nChildIndex);
        if (xAccessible.is())
        {
		    sal_Bool bWasTableSelected(IsTableSelected());

            if (mpChildrenShapes)
                mpChildrenShapes->Select(nChildIndex); // throws no lang::IndexOutOfBoundsException if Index is to high

		    if (bWasTableSelected)
			    mpViewShell->SelectAll();
        }
        else
        {
		    if (mpViewShell)
			    mpViewShell->SelectAll();
        }
    }
}

sal_Bool SAL_CALL
	ScAccessibleDocument::isAccessibleChildSelected( sal_Int32 nChildIndex )
		throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
{
	ScUnoGuard aGuard;
    IsObjectValid();
	sal_Bool bResult(sal_False);

    if (mpChildrenShapes)
    {
        sal_Int32 nCount(mpChildrenShapes->GetCount()); //all shapes and the table
        if (mxTempAcc.is())
            ++nCount;
        if (nChildIndex < 0 || nChildIndex >= nCount)
            throw lang::IndexOutOfBoundsException();

        uno::Reference < XAccessible > xAccessible = mpChildrenShapes->Get(nChildIndex);
        if (xAccessible.is())
        {
		    uno::Reference<drawing::XShape> xShape;
		    bResult = mpChildrenShapes->IsSelected(nChildIndex, xShape); // throws no lang::IndexOutOfBoundsException if Index is to high
        }
        else
        {
            if (mxTempAcc.is() && nChildIndex == nCount)
                bResult = sal_True;
            else
    		    bResult = IsTableSelected();
        }
    }
	return bResult;
}

void SAL_CALL
	ScAccessibleDocument::clearAccessibleSelection(  )
		throw (uno::RuntimeException)
{
	ScUnoGuard aGuard;
    IsObjectValid();

    if (mpChildrenShapes)
        mpChildrenShapes->DeselectAll(); //deselects all (also the table)
}

void SAL_CALL
	ScAccessibleDocument::selectAllAccessibleChildren(  )
		throw (uno::RuntimeException)
{
	ScUnoGuard aGuard;
    IsObjectValid();

    if (mpChildrenShapes)
        mpChildrenShapes->SelectAll();

	// select table after shapes, because while selecting shapes the table will be deselected
	if (mpViewShell)
	{
		mpViewShell->SelectAll();
	}
}

sal_Int32 SAL_CALL
	ScAccessibleDocument::getSelectedAccessibleChildCount(  )
		throw (uno::RuntimeException)
{
	ScUnoGuard aGuard;
    IsObjectValid();
	sal_Int32 nCount(0);

    if (mpChildrenShapes)
        nCount = mpChildrenShapes->GetSelectedCount();

	if (IsTableSelected())
		++nCount;

    if (mxTempAcc.is())
        ++nCount;

	return nCount;
}

uno::Reference<XAccessible > SAL_CALL
	ScAccessibleDocument::getSelectedAccessibleChild( sal_Int32 nSelectedChildIndex )
		throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
{
	ScUnoGuard aGuard;
    IsObjectValid();
	uno::Reference<XAccessible> xAccessible;
    if (mpChildrenShapes)
    {
        sal_Int32 nCount(getSelectedAccessibleChildCount()); //all shapes and the table
        if (nSelectedChildIndex < 0 || nSelectedChildIndex >= nCount)
            throw lang::IndexOutOfBoundsException();

        sal_Bool bTabMarked(IsTableSelected());

        if (mpChildrenShapes)
            xAccessible = mpChildrenShapes->GetSelected(nSelectedChildIndex, bTabMarked); // throws no lang::IndexOutOfBoundsException if Index is to high
        if (mxTempAcc.is() && nSelectedChildIndex == nCount - 1)
            xAccessible = mxTempAcc;
        else if (bTabMarked)
            xAccessible = GetAccessibleSpreadsheet();
    }

    DBG_ASSERT(xAccessible.is(), "here should always be an accessible object or a exception throwed");

	return xAccessible;
}

void SAL_CALL
	ScAccessibleDocument::deselectAccessibleChild( sal_Int32 nChildIndex )
		throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
{
	ScUnoGuard aGuard;
    IsObjectValid();

    if (mpChildrenShapes)
    {
        sal_Int32 nCount(mpChildrenShapes->GetCount()); //all shapes and the table
        if (mxTempAcc.is())
            ++nCount;
        if (nChildIndex < 0 || nChildIndex >= nCount)
            throw lang::IndexOutOfBoundsException();

        sal_Bool bTabMarked(IsTableSelected());

        uno::Reference < XAccessible > xAccessible = mpChildrenShapes->Get(nChildIndex);
        if (xAccessible.is())
        {
            if (mpChildrenShapes)
                mpChildrenShapes->Deselect(nChildIndex); // throws no lang::IndexOutOfBoundsException if Index is to high

		    if (bTabMarked)
			    mpViewShell->SelectAll(); // select the table again
        }
        else if (bTabMarked)
            mpViewShell->Unmark();
    }
}

	//=====  XServiceInfo  ====================================================

::rtl::OUString SAL_CALL
    ScAccessibleDocument::getImplementationName(void)
    throw (uno::RuntimeException)
{
	return ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM ("ScAccessibleDocument"));
}

uno::Sequence< ::rtl::OUString> SAL_CALL
	ScAccessibleDocument::getSupportedServiceNames(void)
        throw (uno::RuntimeException)
{
	uno::Sequence< ::rtl::OUString > aSequence = ScAccessibleContextBase::getSupportedServiceNames();
    sal_Int32 nOldSize(aSequence.getLength());
    aSequence.realloc(nOldSize + 1);
    ::rtl::OUString* pNames = aSequence.getArray();

	pNames[nOldSize] = rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("com.sun.star.AccessibleSpreadsheetDocumentView"));

	return aSequence;
}

//=====  XTypeProvider  =======================================================

uno::Sequence< uno::Type > SAL_CALL ScAccessibleDocument::getTypes()
		throw (uno::RuntimeException)
{
	return comphelper::concatSequences(ScAccessibleDocumentImpl::getTypes(), ScAccessibleContextBase::getTypes());
}

uno::Sequence<sal_Int8> SAL_CALL
	ScAccessibleDocument::getImplementationId(void)
    throw (uno::RuntimeException)
{
    ScUnoGuard aGuard;
    IsObjectValid();
	static uno::Sequence<sal_Int8> aId;
	if (aId.getLength() == 0)
	{
		aId.realloc (16);
		rtl_createUuid (reinterpret_cast<sal_uInt8 *>(aId.getArray()), 0, sal_True);
	}
	return aId;
}

///=====  IAccessibleViewForwarder  ========================================

sal_Bool ScAccessibleDocument::IsValid (void) const
{
    ScUnoGuard aGuard;
    IsObjectValid();
    return (!ScAccessibleContextBase::IsDefunc() && !rBHelper.bInDispose);
}

Rectangle ScAccessibleDocument::GetVisibleArea_Impl() const
{
    Rectangle aVisRect(GetBoundingBox());

    Point aPoint(mpViewShell->GetViewData()->GetPixPos(meSplitPos)); // returns a negative Point
    aPoint.setX(-aPoint.getX());
    aPoint.setY(-aPoint.getY());
    aVisRect.SetPos(aPoint);

    ScGridWindow* pWin = static_cast<ScGridWindow*>(mpViewShell->GetWindowByPos(meSplitPos));
    if (pWin)
        aVisRect = pWin->PixelToLogic(aVisRect, pWin->GetDrawMapMode());

    return aVisRect;
}

Rectangle ScAccessibleDocument::GetVisibleArea() const
{
    ScUnoGuard aGuard;
    IsObjectValid();
    return maVisArea;
}

Point ScAccessibleDocument::LogicToPixel (const Point& rPoint) const
{
    ScUnoGuard aGuard;
    IsObjectValid();
    Point aPoint;
    ScGridWindow* pWin = static_cast<ScGridWindow*>(mpViewShell->GetWindowByPos(meSplitPos));
    if (pWin)
    {
        aPoint = pWin->LogicToPixel(rPoint, pWin->GetDrawMapMode());
        aPoint += pWin->GetWindowExtentsRelative(NULL).TopLeft();
    }
    return aPoint;
}

Size ScAccessibleDocument::LogicToPixel (const Size& rSize) const
{
    ScUnoGuard aGuard;
    IsObjectValid();
    Size aSize;
    ScGridWindow* pWin = static_cast<ScGridWindow*>(mpViewShell->GetWindowByPos(meSplitPos));
    if (pWin)
        aSize = pWin->LogicToPixel(rSize, pWin->GetDrawMapMode());
    return aSize;
}

Point ScAccessibleDocument::PixelToLogic (const Point& rPoint) const
{
    ScUnoGuard aGuard;
    IsObjectValid();
    Point aPoint;
    ScGridWindow* pWin = static_cast<ScGridWindow*>(mpViewShell->GetWindowByPos(meSplitPos));
    if (pWin)
    {
        aPoint -= pWin->GetWindowExtentsRelative(NULL).TopLeft();
        aPoint = pWin->PixelToLogic(rPoint, pWin->GetDrawMapMode());
    }
    return aPoint;
}

Size ScAccessibleDocument::PixelToLogic (const Size& rSize) const
{
    ScUnoGuard aGuard;
    IsObjectValid();
    Size aSize;
    ScGridWindow* pWin = static_cast<ScGridWindow*>(mpViewShell->GetWindowByPos(meSplitPos));
    if (pWin)
        aSize = pWin->PixelToLogic(rSize, pWin->GetDrawMapMode());
    return aSize;
}

    //=====  internal  ========================================================

utl::AccessibleRelationSetHelper* ScAccessibleDocument::GetRelationSet(const ScAddress* pAddress) const
{
    utl::AccessibleRelationSetHelper* pRelationSet = NULL;
    if (mpChildrenShapes)
        pRelationSet = mpChildrenShapes->GetRelationSet(pAddress);
    return pRelationSet;
}

::rtl::OUString SAL_CALL
    ScAccessibleDocument::createAccessibleDescription(void)
    throw (uno::RuntimeException)
{
    rtl::OUString sDescription = String(ScResId(STR_ACC_DOC_DESCR));
	return sDescription;
}

::rtl::OUString SAL_CALL
    ScAccessibleDocument::createAccessibleName(void)
    throw (uno::RuntimeException)
{
	ScUnoGuard aGuard;
    IsObjectValid();
	rtl::OUString sName = String(ScResId(STR_ACC_DOC_NAME));
	sal_Int32 nNumber(sal_Int32(meSplitPos) + 1);
	sName += rtl::OUString::valueOf(nNumber);
	return sName;
}

Rectangle ScAccessibleDocument::GetBoundingBoxOnScreen() const
	throw (uno::RuntimeException)
{
	Rectangle aRect;
	if (mpViewShell)
	{
		Window* pWindow = mpViewShell->GetWindowByPos(meSplitPos);
		if (pWindow)
			aRect = pWindow->GetWindowExtentsRelative(NULL);
	}
	return aRect;
}

Rectangle ScAccessibleDocument::GetBoundingBox() const
	throw (uno::RuntimeException)
{
	Rectangle aRect;
	if (mpViewShell)
	{
		Window* pWindow = mpViewShell->GetWindowByPos(meSplitPos);
		if (pWindow)
			aRect = pWindow->GetWindowExtentsRelative(pWindow->GetAccessibleParentWindow());
	}
	return aRect;
}

SCTAB ScAccessibleDocument::getVisibleTable() const
{
	SCTAB nVisibleTable(0);
	if (mpViewShell && mpViewShell->GetViewData())
		nVisibleTable = mpViewShell->GetViewData()->GetTabNo();
	return nVisibleTable;
}

uno::Reference < XAccessible >
	ScAccessibleDocument::GetAccessibleSpreadsheet()
{
	if (!mpAccessibleSpreadsheet && mpViewShell)
	{
		mpAccessibleSpreadsheet = new ScAccessibleSpreadsheet(this, mpViewShell, getVisibleTable(), meSplitPos);
		mpAccessibleSpreadsheet->acquire();
		mpAccessibleSpreadsheet->Init();
		mbCompleteSheetSelected = IsTableSelected();
	}
	return mpAccessibleSpreadsheet;
}

void ScAccessibleDocument::FreeAccessibleSpreadsheet()
{
	if (mpAccessibleSpreadsheet)
	{
		mpAccessibleSpreadsheet->dispose();
		mpAccessibleSpreadsheet->release();
		mpAccessibleSpreadsheet = NULL;
	}
}

sal_Bool ScAccessibleDocument::IsTableSelected() const
{
	sal_Bool bResult (sal_False);
	if(mpViewShell)
	{
		SCTAB nTab(getVisibleTable());
        //#103800#; use a copy of MarkData
        ScMarkData aMarkData(mpViewShell->GetViewData()->GetMarkData());
		aMarkData.MarkToMulti();
		if (aMarkData.IsAllMarked(ScRange(ScAddress(0, 0, nTab),ScAddress(MAXCOL, MAXROW, nTab))))
			bResult = sal_True;
	}
	return bResult;
}

sal_Bool ScAccessibleDocument::IsDefunc(
	const uno::Reference<XAccessibleStateSet>& rxParentStates)
{
	return ScAccessibleContextBase::IsDefunc() || (mpViewShell == NULL) || !getAccessibleParent().is() ||
		(rxParentStates.is() && rxParentStates->contains(AccessibleStateType::DEFUNC));
}

sal_Bool ScAccessibleDocument::IsEditable(
    const uno::Reference<XAccessibleStateSet>& /* rxParentStates */)
{
	// what is with document protection or readonly documents?
	return sal_True;
}

void ScAccessibleDocument::AddChild(const uno::Reference<XAccessible>& xAcc, sal_Bool bFireEvent)
{
    DBG_ASSERT(!mxTempAcc.is(), "this object should be removed before");
    if (xAcc.is())
    {
        mxTempAcc = xAcc;
		if( bFireEvent )
		{
			AccessibleEventObject aEvent;
                        aEvent.Source = uno::Reference<XAccessibleContext>(this);
			aEvent.EventId = AccessibleEventId::CHILD;
			aEvent.NewValue <<= mxTempAcc;
			CommitChange( aEvent );
		}
    }
}

void ScAccessibleDocument::RemoveChild(const uno::Reference<XAccessible>& xAcc, sal_Bool bFireEvent)
{
    DBG_ASSERT(mxTempAcc.is(), "this object should be added before");
    if (xAcc.is())
    {
        DBG_ASSERT(xAcc.get() == mxTempAcc.get(), "only the same object should be removed");
		if( bFireEvent )
		{
			AccessibleEventObject aEvent;
                        aEvent.Source = uno::Reference<XAccessibleContext>(this);
			aEvent.EventId = AccessibleEventId::CHILD;
			aEvent.OldValue <<= mxTempAcc;
			CommitChange( aEvent );
		}
        mxTempAcc = NULL;
    }
}

rtl::OUString ScAccessibleDocument::GetCurrentCellName() const
{
	String sName( ScResId(STR_ACC_CELL_NAME) );
    if (mpViewShell)
    {
	    String sAddress;
	    // Document not needed, because only the cell address, but not the tablename is needed
	    mpViewShell->GetViewData()->GetCurPos().Format( sAddress, SCA_VALID, NULL );
	    sName.SearchAndReplaceAscii("%1", sAddress);
    }
    return rtl::OUString(sName);
}

rtl::OUString ScAccessibleDocument::GetCurrentCellDescription() const
{
    return rtl::OUString();
}
ScDocument *ScAccessibleDocument::GetDocument() const
{
	return mpViewShell ? mpViewShell->GetViewData()->GetDocument() : NULL;  
}
ScAddress   ScAccessibleDocument::GetCurCellAddress() const
{ 
	return mpViewShell ? mpViewShell->GetViewData()->GetCurPos() :ScAddress(); 
}
uno::Any SAL_CALL ScAccessibleDocument::getExtendedAttributes() 
		throw (::com::sun::star::lang::IndexOutOfBoundsException, ::com::sun::star::uno::RuntimeException) 
{
	
	uno::Any anyAtrribute;
	
	rtl::OUString sName;
	rtl::OUString sValue;
	sal_uInt16 sheetIndex;
	String sSheetName;
	sheetIndex = getVisibleTable();
	if(GetDocument()==NULL)
		return anyAtrribute;
	GetDocument()->GetName(sheetIndex,sSheetName);
	sName = rtl::OUString::createFromAscii("page-name:");
	sValue = sName + sSheetName ;
	sName = rtl::OUString::createFromAscii(";page-number:");
	sValue += sName;
	sValue += String::CreateFromInt32(sheetIndex+1) ;
	sName = rtl::OUString::createFromAscii(";total-pages:");
	sValue += sName;
	sValue += String::CreateFromInt32(GetDocument()->GetTableCount());
	sValue +=  rtl::OUString::createFromAscii(";");
	anyAtrribute <<= sValue;
	return anyAtrribute;
}
com::sun::star::uno::Sequence< com::sun::star::uno::Any > ScAccessibleDocument::GetScAccFlowToSequence()
{
	if ( getAccessibleChildCount() )
	{
		uno::Reference < XAccessible > xSCTableAcc = getAccessibleChild( 0 ); // table
		if ( xSCTableAcc.is() )
		{		
			uno::Reference < XAccessibleSelection > xAccSelection( xSCTableAcc, uno::UNO_QUERY );
			sal_Int32 nSelCount = xAccSelection->getSelectedAccessibleChildCount();
			if( nSelCount )
			{
				uno::Reference < XAccessible > xSel = xAccSelection->getSelectedAccessibleChild( 0 ); // selected cell
				if ( xSel.is() )
				{
					uno::Reference < XAccessibleContext > xSelContext( xSel->getAccessibleContext() );
					if ( xSelContext.is() )
					{										
						if ( xSelContext->getAccessibleRole() == AccessibleRole::TABLE_CELL )
						{
							sal_Int32 nParaCount = 0;
							uno::Sequence <uno::Any> aSequence(nSelCount);
							for ( sal_Int32 i = 0; i < nSelCount; i++ )
							{
								xSel = xAccSelection->getSelectedAccessibleChild( i )	;
								if ( xSel.is() )
								{
									xSelContext = xSel->getAccessibleContext();
									if ( xSelContext.is() )
									{
										if ( xSelContext->getAccessibleRole() == AccessibleRole::TABLE_CELL )
										{
											aSequence[nParaCount] = uno::makeAny( xSel );
											nParaCount++;
										}
									}
								}
							}
							return aSequence;
						}
					}
				}
			}									
		}
	}
	uno::Sequence <uno::Any> aEmpty;
	return aEmpty;
}
::com::sun::star::uno::Sequence< ::com::sun::star::uno::Any >
		SAL_CALL ScAccessibleDocument::get_AccFlowTo(const ::com::sun::star::uno::Any& rAny, sal_Int32 nType)
		throw ( ::com::sun::star::uno::RuntimeException )
{
	const sal_Int32 SPELLCHECKFLOWTO = 1;
	const sal_Int32 FINDREPLACEFLOWTO = 2;
	if ( nType == SPELLCHECKFLOWTO )
	{
		uno::Reference< ::com::sun::star::drawing::XShape > xShape;
		rAny >>= xShape;
		if ( xShape.is() )
		{
			uno::Reference < XAccessible > xAcc = mpChildrenShapes->GetAccessibleCaption(xShape);
			uno::Reference < XAccessibleSelection > xAccSelection( xAcc, uno::UNO_QUERY );
			if ( xAccSelection.is() )
			{
				if ( xAccSelection->getSelectedAccessibleChildCount() ) 
				{
					uno::Reference < XAccessible > xSel = xAccSelection->getSelectedAccessibleChild( 0 );
					if ( xSel.is() )
					{
						uno::Reference < XAccessibleContext > xSelContext( xSel->getAccessibleContext() );
						if ( xSelContext.is() )
						{
							//if in sw we find the selected paragraph here
							if ( xSelContext->getAccessibleRole() == AccessibleRole::PARAGRAPH )
							{
								uno::Sequence<uno::Any> aRet( 1 );
								aRet[0] = uno::makeAny( xSel );
								return aRet;
							}
						}
					}
				}
			}
		}
		else
		{
			if ( getSelectedAccessibleChildCount() ) 
			{
				uno::Reference < XAccessible > xSel = getSelectedAccessibleChild( 0 );
				if ( xSel.is() )
				{
					uno::Reference < XAccessibleContext > xSelContext( xSel->getAccessibleContext() );
					if ( xSelContext.is() )
					{
						uno::Reference < XAccessibleSelection > xAccChildSelection( xSel, uno::UNO_QUERY );
						if ( xAccChildSelection.is() )
						{
							if ( xAccChildSelection->getSelectedAccessibleChildCount() )
							{
								uno::Reference < XAccessible > xChildSel = xAccChildSelection->getSelectedAccessibleChild( 0 );
								if ( xChildSel.is() )
								{
									uno::Reference < ::com::sun::star::accessibility::XAccessibleContext > xChildSelContext( xChildSel->getAccessibleContext() );
									if ( xChildSelContext.is() &&
										xChildSelContext->getAccessibleRole() == ::com::sun::star::accessibility::AccessibleRole::PARAGRAPH )
									{
										uno::Sequence<uno::Any> aRet( 1 );
										aRet[0] = uno::makeAny( xChildSel );
										return aRet;	
									}
								}
							}
						}
					}
				}
			}
		}
	}
	else if ( nType == FINDREPLACEFLOWTO )
	{
		sal_Bool bSuccess(sal_False);
		rAny >>= bSuccess;
		if ( bSuccess )
		{
			uno::Sequence< uno::Any> aSeq = GetScAccFlowToSequence();
			if ( aSeq.getLength() )
			{
				return aSeq;
			}
			else if( mpAccessibleSpreadsheet )
			{
				uno::Reference < XAccessible > xFindCellAcc = mpAccessibleSpreadsheet->GetActiveCell();
				// add xFindCellAcc to the return the Sequence
				uno::Sequence< uno::Any> aSeq2(1);
				aSeq2[0] = uno::makeAny( xFindCellAcc );
				return aSeq2;
			}
		}
	}
	uno::Sequence< uno::Any> aEmpty;
	return aEmpty;
}
void ScAccessibleDocument::SwitchViewFireFocus()
{
	if (mpAccessibleSpreadsheet)
	{
		mpAccessibleSpreadsheet->FireFirstCellFocus();
	}
}

sal_Int32 SAL_CALL ScAccessibleDocument::getForeground(  )
        throw (uno::RuntimeException)
{
    return COL_BLACK;
}

sal_Int32 SAL_CALL ScAccessibleDocument::getBackground(  )
        throw (uno::RuntimeException)
{
	ScUnoGuard aGuard;
    IsObjectValid();
    return SC_MOD()->GetColorConfig().GetColorValue( ::svtools::DOCCOLOR ).nColor;
}

