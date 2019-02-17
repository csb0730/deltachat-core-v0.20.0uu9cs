#ifndef __DC_OAUTH2_H__
#define __DC_OAUTH2_H__
#ifdef __cplusplus
extern "C" {
#endif


#define DC_REGENERATE 0x01

char* dc_get_oauth2_access_token(dc_context_t*, const char* code, int flags);
char* dc_get_oauth2_addr        (dc_context_t*, const char* addr);


#ifdef __cplusplus
} /* /extern "C" */
#endif
#endif /* __DC_OAUTH2_H__ */
