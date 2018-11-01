/*************************************************************************
 * (C) 2018 Bjoern Petersen and contributors.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *************************************************************************/

#ifndef __DC_SAXPARSER_H__
#define __DC_SAXPARSER_H__
#ifdef __cplusplus
extern "C" {
#endif


typedef void (*dc_saxparser_starttag_cb_t) (void* userdata, const char* tag, char** attr);
typedef void (*dc_saxparser_endtag_cb_t)   (void* userdata, const char* tag);
typedef void (*dc_saxparser_text_cb_t)     (void* userdata, const char* text, int len); /* len is only informational, text is already null-terminated */


typedef struct dc_saxparser_t
{
	dc_saxparser_starttag_cb_t starttag_cb;
	dc_saxparser_endtag_cb_t   endtag_cb;
	dc_saxparser_text_cb_t     text_cb;
	void*                      userdata;
} dc_saxparser_t;


void           dc_saxparser_init             (dc_saxparser_t*, void* userData);
void           dc_saxparser_set_tag_handler  (dc_saxparser_t*, dc_saxparser_starttag_cb_t, dc_saxparser_endtag_cb_t);
void           dc_saxparser_set_text_handler (dc_saxparser_t*, dc_saxparser_text_cb_t);

void           dc_saxparser_parse            (dc_saxparser_t*, const char* text);

const char*    dc_attr_find                  (char** attr, const char* key);


/*** library-private **********************************************************/


#ifdef __cplusplus
} /* /extern "C" */
#endif
#endif /* __DC_SAXPARSER_H__ */

