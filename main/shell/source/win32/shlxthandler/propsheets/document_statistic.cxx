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
#include "precompiled_shell.hxx"

#ifdef _MSC_VER
#pragma warning (disable : 4786 4503)
#endif
#include "document_statistic.hxx"
#include "internal/utilities.hxx"
#include "internal/metainforeader.hxx"
#include "internal/resource.h"
#include "internal/fileextensions.hxx"
#include "internal/config.hxx"
#include "internal/iso8601_converter.hxx"

//#####################################   
const bool READONLY  = false;
const bool WRITEABLE = true;
       
//#####################################
document_statistic_reader_ptr create_document_statistic_reader(const std::string& document_name, CMetaInfoReader* meta_info_accessor)
{
    File_Type_t file_type = get_file_type(document_name);
            
	if (WRITER == file_type)
	    return document_statistic_reader_ptr(new writer_document_statistic_reader(document_name, meta_info_accessor));
	else if (CALC == file_type)
	    return document_statistic_reader_ptr(new calc_document_statistic_reader(document_name, meta_info_accessor));
	else
	    return document_statistic_reader_ptr(new draw_impress_math_document_statistic_reader(document_name, meta_info_accessor));	
}


//#####################################
document_statistic_reader::document_statistic_reader(const std::string& document_name, CMetaInfoReader* meta_info_accessor) :
    document_name_(document_name),
    meta_info_accessor_(meta_info_accessor)
{}

//#####################################    
document_statistic_reader::~document_statistic_reader() 
{}

//#####################################
void document_statistic_reader::read(statistic_group_list_t* group_list)
{
	group_list->clear();    	    	    
    fill_description_section(meta_info_accessor_, group_list);
    fill_origin_section(meta_info_accessor_, group_list);                                      
}

//#####################################
std::string document_statistic_reader::get_document_name() const
{
    return document_name_;
}

//#####################################
void document_statistic_reader::fill_origin_section(CMetaInfoReader *meta_info_accessor, statistic_group_list_t* group_list)
{
    statistic_item_list_t il;
        
    il.push_back(statistic_item(GetResString(IDS_AUTHOR), meta_info_accessor->getTagData( META_INFO_AUTHOR ), READONLY));
    
    il.push_back(statistic_item(GetResString(IDS_MODIFIED),        
        iso8601_date_to_local_date(meta_info_accessor->getTagData(META_INFO_MODIFIED )), READONLY));        
        
    il.push_back(statistic_item(GetResString(IDS_DOCUMENT_NUMBER), meta_info_accessor->getTagData( META_INFO_DOCUMENT_NUMBER ), READONLY));
    
    il.push_back(statistic_item(GetResString(IDS_EDITING_TIME), 
        iso8601_duration_to_local_duration(meta_info_accessor->getTagData( META_INFO_EDITING_TIME )), READONLY));
                        
    group_list->push_back(statistic_group_t(GetResString(IDS_ORIGIN), il));  
}
    
//#####################################
writer_document_statistic_reader::writer_document_statistic_reader(const std::string& document_name, CMetaInfoReader* meta_info_accessor) :
    document_statistic_reader(document_name, meta_info_accessor)    
{}

//#####################################
void writer_document_statistic_reader::fill_description_section(CMetaInfoReader *meta_info_accessor, statistic_group_list_t* group_list)
{
    statistic_item_list_t il;
    
    il.push_back(statistic_item(GetResString(IDS_TITLE),    meta_info_accessor->getTagData( META_INFO_TITLE ),       READONLY));
    il.push_back(statistic_item(GetResString(IDS_COMMENTS), meta_info_accessor->getTagData( META_INFO_DESCRIPTION ), READONLY));
    il.push_back(statistic_item(GetResString(IDS_SUBJECT),  meta_info_accessor->getTagData( META_INFO_SUBJECT ),     READONLY));
    il.push_back(statistic_item(GetResString(IDS_KEYWORDS), meta_info_accessor->getTagData(META_INFO_KEYWORDS ),    READONLY));
    il.push_back(statistic_item(GetResString(IDS_PAGES),    meta_info_accessor->getTagAttribute( META_INFO_DOCUMENT_STATISTIC,META_INFO_PAGES) , READONLY));
    il.push_back(statistic_item(GetResString(IDS_TABLES), meta_info_accessor->getTagAttribute( META_INFO_DOCUMENT_STATISTIC,META_INFO_TABLES) , READONLY));
    il.push_back(statistic_item(GetResString(IDS_GRAPHICS), meta_info_accessor->getTagAttribute( META_INFO_DOCUMENT_STATISTIC,META_INFO_DRAWS) , READONLY));
    il.push_back(statistic_item(GetResString(IDS_OLE_OBJECTS), meta_info_accessor->getTagAttribute( META_INFO_DOCUMENT_STATISTIC,META_INFO_OBJECTS) ,    READONLY));
    il.push_back(statistic_item(GetResString(IDS_PARAGRAPHS), meta_info_accessor->getTagAttribute( META_INFO_DOCUMENT_STATISTIC,META_INFO_PARAGRAPHS) , READONLY));
    il.push_back(statistic_item(GetResString(IDS_WORDS), meta_info_accessor->getTagAttribute( META_INFO_DOCUMENT_STATISTIC,META_INFO_WORDS) , READONLY));
    il.push_back(statistic_item(GetResString(IDS_CHARACTERS), meta_info_accessor->getTagAttribute( META_INFO_DOCUMENT_STATISTIC,META_INFO_CHARACTERS) , READONLY));
                                                                                    
    group_list->push_back(statistic_group_t(GetResString(IDS_DESCRIPTION), il));                                                    
}
    
//#######################################
calc_document_statistic_reader::calc_document_statistic_reader(
    const std::string& document_name, CMetaInfoReader* meta_info_accessor) :
    document_statistic_reader(document_name, meta_info_accessor)
{}

//#######################################
void calc_document_statistic_reader::fill_description_section(
    CMetaInfoReader *meta_info_accessor,statistic_group_list_t* group_list)
{
    statistic_item_list_t il;
    
    il.push_back(statistic_item(GetResString(IDS_TITLE),       meta_info_accessor->getTagData( META_INFO_TITLE ),       READONLY));
    il.push_back(statistic_item(GetResString(IDS_COMMENTS),    meta_info_accessor->getTagData( META_INFO_DESCRIPTION ), READONLY));
    il.push_back(statistic_item(GetResString(IDS_SUBJECT),     meta_info_accessor->getTagData( META_INFO_SUBJECT ),     READONLY));
    il.push_back(statistic_item(GetResString(IDS_KEYWORDS),    meta_info_accessor->getTagData(META_INFO_KEYWORDS ),    READONLY));    
    il.push_back(statistic_item(GetResString(IDS_TABLES),      meta_info_accessor->getTagAttribute( META_INFO_DOCUMENT_STATISTIC,META_INFO_TABLES) ,  READONLY));
    il.push_back(statistic_item(GetResString(IDS_CELLS),       meta_info_accessor->getTagAttribute( META_INFO_DOCUMENT_STATISTIC,META_INFO_CELLS) ,   READONLY));
    il.push_back(statistic_item(GetResString(IDS_OLE_OBJECTS), meta_info_accessor->getTagAttribute( META_INFO_DOCUMENT_STATISTIC,META_INFO_OBJECTS) , READONLY));
                                                                                        
    group_list->push_back(statistic_group_t(GetResString(IDS_DESCRIPTION), il));                                                    
}

//#######################################
draw_impress_math_document_statistic_reader::draw_impress_math_document_statistic_reader(
    const std::string& document_name, CMetaInfoReader* meta_info_accessor) :
    document_statistic_reader(document_name, meta_info_accessor)
{}

//#######################################
void draw_impress_math_document_statistic_reader::fill_description_section(
    CMetaInfoReader *meta_info_accessor, statistic_group_list_t* group_list)
{
    statistic_item_list_t il;
    
    il.push_back(statistic_item(GetResString(IDS_TITLE),       meta_info_accessor->getTagData( META_INFO_TITLE ),       READONLY));
    il.push_back(statistic_item(GetResString(IDS_COMMENTS),    meta_info_accessor->getTagData( META_INFO_DESCRIPTION ), READONLY));
    il.push_back(statistic_item(GetResString(IDS_SUBJECT),     meta_info_accessor->getTagData( META_INFO_SUBJECT ),     READONLY));
    il.push_back(statistic_item(GetResString(IDS_KEYWORDS),    meta_info_accessor->getTagData(META_INFO_KEYWORDS ),    READONLY));
    il.push_back(statistic_item(GetResString(IDS_PAGES),       meta_info_accessor->getTagAttribute( META_INFO_DOCUMENT_STATISTIC,META_INFO_PAGES) ,   READONLY));    
    il.push_back(statistic_item(GetResString(IDS_OLE_OBJECTS), meta_info_accessor->getTagAttribute( META_INFO_DOCUMENT_STATISTIC,META_INFO_OBJECTS) , READONLY));
                                                                                    
    group_list->push_back(statistic_group_t(GetResString(IDS_DESCRIPTION), il));                                                    
}
