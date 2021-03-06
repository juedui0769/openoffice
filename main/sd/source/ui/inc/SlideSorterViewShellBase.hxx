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



#ifndef SD_SLIDE_SORTER_VIEW_SHELL_BASE_HXX
#define SD_SLIDE_SORTER_VIEW_SHELL_BASE_HXX

#include "ImpressViewShellBase.hxx"


namespace sd {

/** This class exists to be able to register a factory that creates a
    slide sorter view shell as default.
*/
class SlideSorterViewShellBase
    : public ImpressViewShellBase
{
public:
    TYPEINFO();
    SFX_DECL_VIEWFACTORY(SlideSorterViewShellBase);

    /** This constructor is used by the view factory of the SFX
        macros.
    */
    SlideSorterViewShellBase (SfxViewFrame *pFrame, SfxViewShell* pOldShell);
    virtual ~SlideSorterViewShellBase (void);
};

} // end of namespace sd

#endif
