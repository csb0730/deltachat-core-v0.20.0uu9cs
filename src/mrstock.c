/*******************************************************************************
 *
 *                             Messenger Backend
 *                      Copyright (C) 2017 Björn Petersen
 *                   Contact: r10s@b44t.com, http://b44t.com
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see http://www.gnu.org/licenses/ .
 *
 *******************************************************************************
 *
 * File:    mrstock.h
 * Purpose: Add translated strings that are used by the messager backend.
 *          As the logging functions may use these strings, do not log any
 *          errors from here.
 *
 ******************************************************************************/


#include <stdlib.h>
#include <memory.h>
#include "mrmailbox.h"
#include "mrtools.h"



/*******************************************************************************
 * Main interface
 ******************************************************************************/


mrmailbox_t* s_localize_mb_obj = NULL;


static char* default_string(int id, int qty)
{
	switch( id ) {
		case MR_STR_NOMESSAGES:    return safe_strdup("No messages.");
		case MR_STR_SELF:          return safe_strdup("Me");
		case MR_STR_DRAFT:         return safe_strdup("Draft");
		case MR_STR_MEMBER:        return mr_mprintf("%i member(s)", qty);
		case MR_STR_CONTACT:       return mr_mprintf("%i contact(s)", qty);
		case MR_STR_VOICEMESSAGE:  return safe_strdup("Voice message");
		case MR_STR_DEADDROP:      return safe_strdup("Mailbox");
		case MR_STR_IMAGE:         return safe_strdup("Image");
		case MR_STR_VIDEO:         return safe_strdup("Video");
		case MR_STR_AUDIO:         return safe_strdup("Audio");
		case MR_STR_FILE:          return safe_strdup("File");
		case MR_STR_STATUSLINE:    return safe_strdup("Sent with my Delta Chat Messenger");
		case MR_STR_NEWGROUPDRAFT: return safe_strdup("Hello, I've just created the group \"%1$s\" for us.");
		case MR_STR_MSGGRPNAME:    return safe_strdup("Group name changed from \"%1$s\" to \"%2$s\".");
		case MR_STR_MSGGRPIMAGE:   return safe_strdup("Group image changed.");
		case MR_STR_MSGADDMEMBER:  return safe_strdup("Member %1$s added.");
		case MR_STR_MSGDELMEMBER:  return safe_strdup("Member %1$s removed.");
		case MR_STR_MSGGROUPLEFT:  return safe_strdup("Group left.");
		case MR_STR_ERROR:         return safe_strdup("Error: %1$s");
		case MR_STR_SELFNOTINGRP:  return safe_strdup("You must be a member of the group to perform this action.");
		case MR_STR_NOTCONNECTED:  return safe_strdup("Not connected.");
	}
	return safe_strdup("ErrStr");
}


char* mrstock_str(int id) /* get the string with the given ID, the result must be free()'d! */
{
	char* ret = NULL;
	if( s_localize_mb_obj && s_localize_mb_obj->m_cb ) {
		ret = (char*)s_localize_mb_obj->m_cb(s_localize_mb_obj, MR_EVENT_GET_STRING, id, 0);
	}
	if( ret == NULL ) {
		ret = default_string(id, 0);
	}
	return ret;
}


static char* repl_string(char* haystack /*string will be modified!*/, const char* needle, const char* replacement)
{
	/* replace needle by the given replacement, the input string will be modified, the result must be free()'d */
	char* p2 = strstr(haystack, needle);
	if( p2==NULL ) { return strdup(haystack); }
	*p2 = 0;
	p2 += strlen(needle);
	return mr_mprintf("%s%s%s", haystack, replacement? replacement : "", p2);
}


char* mrstock_str_repl_string(int id, const char* to_insert)
{
	char* p1 = mrstock_str(id);
	char* p2 = repl_string(p1, "%1$s", to_insert);
	free(p1);
	return p2;
}


char* mrstock_str_repl_int(int id, int to_insert_int)
{
	char* ret, *to_insert_str = mr_mprintf("%i", (int)to_insert_int);
	ret = mrstock_str_repl_string(id, to_insert_str);
	free(to_insert_str);
	return ret;
}


char* mrstock_str_repl_string2(int id, const char* to_insert, const char* to_insert2)
{
	char* p1 = mrstock_str(id);
	char* p2 = repl_string(p1, "%1$s", to_insert);
	free(p1);
	p1 = repl_string(p2, "%2$s", to_insert2);
	free(p2);
	return p1;
}


char* mrstock_str_repl_pl(int id, int cnt)
{
	char* ret = NULL;
	if( s_localize_mb_obj && s_localize_mb_obj->m_cb ) {
		ret = (char*)s_localize_mb_obj->m_cb(s_localize_mb_obj, MR_EVENT_GET_QUANTITY_STRING, id, cnt);
	}
	if( ret == NULL ) {
		ret = default_string(id, cnt);
	}
	return ret;
}
