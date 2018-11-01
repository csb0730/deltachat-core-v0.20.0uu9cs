/*************************************************************************
 * (C) 2018 Bjoern Petersen and contributors.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *************************************************************************/

#ifndef __DC_TOKEN_H__
#define __DC_TOKEN_H__
#ifdef __cplusplus
extern "C" {
#endif


// Token namespaces
typedef enum {
	DC_TOKEN_INVITENUMBER = 100,
	DC_TOKEN_AUTH = 110
} dc_tokennamespc_t;


// Functions to read/write token from/to the database. A token is any string associated with a key.
void     dc_token_save                   (dc_context_t*, dc_tokennamespc_t, uint32_t foreign_id, const char* token);
char*    dc_token_lookup                 (dc_context_t*, dc_tokennamespc_t, uint32_t foreign_id);
int      dc_token_exists                 (dc_context_t*, dc_tokennamespc_t, const char* token);


#ifdef __cplusplus
} /* /extern "C" */
#endif
#endif /* __DC_TOKEN_H__ */

