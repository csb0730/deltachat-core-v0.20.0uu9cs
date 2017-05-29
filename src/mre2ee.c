/*******************************************************************************
 *
 *                              Delta Chat Core
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
 * File:    mre2ee.c
 * Purpose: Handle End-To-End-Encryption
 *
 ******************************************************************************/


#include <stdlib.h>
#include <string.h>
#include "mrmailbox.h"
#include "mre2ee.h"
#include "mre2ee_driver.h"
#include "mrapeerstate.h"
#include "mraheader.h"
#include "mrkeyring.h"
#include "mrmimeparser.h"
#include "mrtools.h"


static struct mailmime* new_data_part(void* data, size_t data_bytes, char* default_content_type, int default_encoding)
{
  //char basename_buf[PATH_MAX];
  struct mailmime_mechanism * encoding;
  struct mailmime_content * content;
  struct mailmime * mime;
  //int r;
  //char * dup_filename;
  struct mailmime_fields * mime_fields;
  int encoding_type;
  char * content_type_str;
  int do_encoding;

  /*if (filename != NULL) {
    strncpy(basename_buf, filename, PATH_MAX);
    libetpan_basename(basename_buf);
  }*/

  encoding = NULL;

  /* default content-type */
  if (default_content_type == NULL)
    content_type_str = "application/octet-stream";
  else
    content_type_str = default_content_type;

  content = mailmime_content_new_with_str(content_type_str);
  if (content == NULL) {
    goto free_content;
  }

  do_encoding = 1;
  if (content->ct_type->tp_type == MAILMIME_TYPE_COMPOSITE_TYPE) {
    struct mailmime_composite_type * composite;

    composite = content->ct_type->tp_data.tp_composite_type;

    switch (composite->ct_type) {
    case MAILMIME_COMPOSITE_TYPE_MESSAGE:
      if (strcasecmp(content->ct_subtype, "rfc822") == 0)
        do_encoding = 0;
      break;

    case MAILMIME_COMPOSITE_TYPE_MULTIPART:
      do_encoding = 0;
      break;
    }
  }

  if (do_encoding) {
    if (default_encoding == -1)
      encoding_type = MAILMIME_MECHANISM_BASE64;
    else
      encoding_type = default_encoding;

    /* default Content-Transfer-Encoding */
    encoding = mailmime_mechanism_new(encoding_type, NULL);
    if (encoding == NULL) {
      goto free_content;
    }
  }

  mime_fields = mailmime_fields_new_with_data(encoding,
      NULL, NULL, NULL, NULL);
  if (mime_fields == NULL) {
    goto free_content;
  }

  mime = mailmime_new_empty(content, mime_fields);
  if (mime == NULL) {
    goto free_mime_fields;
  }

  /*if ((filename != NULL) && (mime->mm_type == MAILMIME_SINGLE)) {
    // duplicates the file so that the file can be deleted when
    // the MIME part is done
    dup_filename = dup_file(privacy, filename);
    if (dup_filename == NULL) {
      goto free_mime;
    }

    r = mailmime_set_body_file(mime, dup_filename);
    if (r != MAILIMF_NO_ERROR) {
      free(dup_filename);
      goto free_mime;
    }
  }*/
  if( data!=NULL && data_bytes>0 && mime->mm_type == MAILMIME_SINGLE ) {
	mailmime_set_body_text(mime, data, data_bytes);
  }

  return mime;

// free_mime:
  //mailmime_free(mime);
  goto err;
 free_mime_fields:
  mailmime_fields_free(mime_fields);
  mailmime_content_free(content);
  goto err;
 free_content:
  if (encoding != NULL)
    mailmime_mechanism_free(encoding);
  if (content != NULL)
    mailmime_content_free(content);
 err:
  return NULL;
}


/*******************************************************************************
 * Tools
 ******************************************************************************/


static int load_or_generate_public_key__(mrmailbox_t* mailbox, mrkey_t* public_key, const char* self_addr)
{
	static int s_in_key_creation = 0; /* avoid double creation (we unlock the database during creation) */
	int        key_created = 0;
	int        success = 0, key_creation_here = 0;

	if( mailbox == NULL || public_key == NULL ) {
		goto cleanup;
	}

	if( !mrkey_load_self_public__(public_key, self_addr, mailbox->m_sql) )
	{
		/* create the keypair - this may take a moment, however, as this is in a thread, this is no big deal */
		if( s_in_key_creation ) { goto cleanup; }
		key_creation_here = 1;
		s_in_key_creation = 1;

		{
			mrkey_t* private_key = mrkey_new();

			mrmailbox_log_info(mailbox, 0, "Generating keypair ...");

			mrsqlite3_unlock(mailbox->m_sql); /* SIC! unlock database during creation - otherwise the GUI may hang */

				/* The public key must contain the following:
				- a signing-capable primary key Kp
				- a user id
				- a self signature
				- an encryption-capable subkey Ke
				- a binding signature over Ke by Kp
				(see https://autocrypt.readthedocs.io/en/latest/level0.html#type-p-openpgp-based-key-data )*/
				key_created = mre2ee_driver_create_keypair(mailbox, self_addr, public_key, private_key);

			mrsqlite3_lock(mailbox->m_sql);

			if( !key_created ) {
				mrmailbox_log_warning(mailbox, 0, "Cannot create keypair.");
				goto cleanup;
			}

			if( !mre2ee_driver_is_valid_key(mailbox, public_key)
			 || !mre2ee_driver_is_valid_key(mailbox, private_key) ) {
				mrmailbox_log_warning(mailbox, 0, "Generated keys are not valid.");
				goto cleanup;
			}

			if( !mrkey_save_self_keypair__(public_key, private_key, self_addr, mailbox->m_sql) ) {
				mrmailbox_log_warning(mailbox, 0, "Cannot save keypair.");
				goto cleanup;
			}

			mrmailbox_log_info(mailbox, 0, "Keypair generated.");

			mrkey_unref(private_key);
		}
	}

	success = 1;

cleanup:
	if( key_creation_here ) { s_in_key_creation = 0; }
	return success;
}


/*******************************************************************************
 * Main interface
 ******************************************************************************/


void mre2ee_init(mrmailbox_t* mailbox)
{
	if( mailbox == NULL ) {
		return;
	}

	mre2ee_driver_init(mailbox);
}


void mre2ee_exit(mrmailbox_t* mailbox)
{
	if( mailbox == NULL ) {
		return;
	}

	mre2ee_driver_exit(mailbox);
}


/*******************************************************************************
 * Encrypt
 ******************************************************************************/


void mre2ee_encrypt(mrmailbox_t* mailbox, const clist* recipients_addr, struct mailmime* in_out_message, mre2ee_helper_t* helper)
{
	int                    locked = 0, col = 0, do_encrypt = 0;
	mrapeerstate_t*        peerstate = mrapeerstate_new();
	mraheader_t*           autocryptheader = mraheader_new();
	struct mailimf_fields* imffields = NULL; /*just a pointer into mailmime structure, must not be freed*/
	mrkeyring_t*           keyring = mrkeyring_new();
	MMAPString*            plain = mmap_string_new("");
	char*                  ctext = NULL;
	size_t                 ctext_bytes = 0;

	if( helper ) { memset(helper, 0, sizeof(mre2ee_helper_t)); }

	if( mailbox == NULL || recipients_addr == NULL || in_out_message == NULL
	 || in_out_message->mm_parent /* libEtPan's pgp_encrypt_mime() takes the parent as the new root. We just expect the root as being given to this function. */
	 || peerstate == NULL || autocryptheader == NULL || keyring==NULL || plain == NULL || helper == NULL ) {
		goto cleanup;
	}

	mrsqlite3_lock(mailbox->m_sql);
	locked = 1;

		/* encryption enabled? */
		if( mrsqlite3_get_config_int__(mailbox->m_sql, "e2ee_enabled", MR_E2EE_DEFAULT_ENABLED) == 0 ) {
			goto cleanup;
		}

		/* load autocrypt header from db */
		autocryptheader->m_prefer_encrypted = MRA_PE_NOPREFERENCE;
		autocryptheader->m_to = mrsqlite3_get_config__(mailbox->m_sql, "configured_addr", NULL);
		if( autocryptheader->m_to == NULL ) {
			goto cleanup;
		}

		if( !load_or_generate_public_key__(mailbox, autocryptheader->m_public_key, autocryptheader->m_to) ) {
			goto cleanup;
		}

		/* load peerstate information etc. */
		if( clist_count(recipients_addr)==1 ) {
			clistiter* iter1 = clist_begin(recipients_addr);
			const char* recipient_addr = clist_content(iter1);
			if( mrapeerstate_load_from_db__(peerstate, mailbox->m_sql, recipient_addr)
			 && peerstate->m_prefer_encrypted!=MRA_PE_NO ) {
				do_encrypt = 1;
			}
		}

	mrsqlite3_unlock(mailbox->m_sql);
	locked = 0;

	/* encrypt message, if possible */
	if( do_encrypt )
	{
		/* prepare part to encrypt */
		mailprivacy_prepare_mime(in_out_message); /* encode quoted printable all text parts */

		struct mailmime* part_to_encrypt = in_out_message->mm_data.mm_message.mm_msg_mime;

		/* convert part to encrypt to plain text */
		mailmime_write_mem(plain, &col, part_to_encrypt);
		if( plain->str == NULL || plain->len<=0 ) {
			goto cleanup;
		}
		//char* t1=mr_null_terminate(plain->str,plain->len);printf("PLAIN:\n%s\n",t1);free(t1); // DEBUG OUTPUT

		/* encrypt the plain text */
		mrkeyring_add(keyring, peerstate->m_public_key);
		if( !mre2ee_driver_encrypt(mailbox, plain->str, plain->len, keyring, 1, (void**)&ctext, &ctext_bytes) ) {
			goto cleanup;
		}
		helper->m_cdata_to_free = ctext;
		//char* t2=mr_null_terminate(ctext,ctext_bytes);printf("ENCRYPTED:\n%s\n",t2);free(t2); // DEBUG OUTPUT

		/* create MIME-structure that will contain the encrypted text */
		struct mailmime* encrypted_part = new_data_part(NULL, 0, "multipart/encrypted", -1);

		struct mailmime_content* content = encrypted_part->mm_content_type;
		clist_append(content->ct_parameters, mailmime_param_new_with_data("protocol", "application/pgp-encrypted"));

		static char version_content[] = "Version: 1\r\n";
		struct mailmime* version_mime = new_data_part(version_content, strlen(version_content), "application/pgp-encrypted", MAILMIME_MECHANISM_7BIT);
		mailmime_smart_add_part(encrypted_part, version_mime);

		struct mailmime* ctext_part = new_data_part(ctext, ctext_bytes, "application/octet-stream", MAILMIME_MECHANISM_7BIT);
		mailmime_smart_add_part(encrypted_part, ctext_part);

		/* replace the original MIME-structure by the encrypted MIME-structure */
		in_out_message->mm_data.mm_message.mm_msg_mime = encrypted_part;
		encrypted_part->mm_parent = in_out_message;
		part_to_encrypt->mm_parent = NULL;
		mailmime_free(part_to_encrypt);
		//MMAPString* t3=mmap_string_new("");mailmime_write_mem(t3,&col,in_out_message);char* t4=mr_null_terminate(t3->str,t3->len); printf("ENCRYPTED+MIME_ENCODED:\n%s\n",t4);free(t4);mmap_string_free(t3); // DEBUG OUTPUT

		helper->m_encryption_successfull = 1;
	}

	/* add Autocrypt:-header to allow the recipient to send us encrypted messages back */
	if( (imffields=mr_find_mailimf_fields(in_out_message))==NULL ) {
		goto cleanup;
	}

	char* p = mraheader_render(autocryptheader);
	if( p == NULL ) {
		goto cleanup;
	}
	mailimf_fields_add(imffields, mailimf_field_new_custom(strdup("Autocrypt"), p/*takes ownership of pointer*/));

cleanup:
	if( locked ) { mrsqlite3_unlock(mailbox->m_sql); }
	mrapeerstate_unref(peerstate);
	mraheader_unref(autocryptheader);
	mrkeyring_unref(keyring);
	if( plain ) { mmap_string_free(plain); }
}


void mre2ee_thanks(mre2ee_helper_t* helper)
{
	if( helper == NULL ) {
		return;
	}

	free(helper->m_cdata_to_free);
	helper->m_cdata_to_free = NULL;
}


/*******************************************************************************
 * Decrypt
 ******************************************************************************/


static int has_decrypted_pgp_armor(const char* str__, int str_bytes)
{
	const unsigned char *str_end = (const unsigned char*)str__+str_bytes, *p=(const unsigned char*)str__;
	while( p < str_end ) {
		if( *p > ' ' ) {
			break;
		}
		p++;
		str_bytes--;
	}
	if( str_bytes>27 && strncmp((const char*)p, "-----BEGIN PGP MESSAGE-----", 27)==0 ) {
		return 1;
	}
	return 0;
}


static void decrypt_part(mrmailbox_t* mailbox, struct mailmime* mime, const mrkeyring_t* private_keyring)
{
	struct mailmime_data*        mime_data;
	int                          mime_transfer_encoding = MAILMIME_MECHANISM_BINARY;
	char*                        transfer_decoding_buffer = NULL; /* mmap_string_unref()'d if set */
	const char*                  decoded_data = NULL; /* must not be free()'d */
	size_t                       decoded_data_bytes = 0;
	void*                        plain_buf = NULL;
	size_t                       plain_bytes = 0;

	/* get data pointer from `mime` */
	mime_data = mime->mm_data.mm_single;
	if( mime_data->dt_type != MAILMIME_DATA_TEXT   /* MAILMIME_DATA_FILE indicates, the data is in a file; AFAIK this is not used on parsing */
	 || mime_data->dt_data.dt_text.dt_data == NULL
	 || mime_data->dt_data.dt_text.dt_length <= 0 ) {
		goto cleanup;
	}

	/* check headers in `mime` */
	if( mime->mm_mime_fields != NULL ) {
		clistiter* cur;
		for( cur = clist_begin(mime->mm_mime_fields->fld_list); cur != NULL; cur = clist_next(cur) ) {
			struct mailmime_field* field = (struct mailmime_field*)clist_content(cur);
			if( field ) {
				if( field->fld_type == MAILMIME_FIELD_TRANSFER_ENCODING && field->fld_data.fld_encoding ) {
					mime_transfer_encoding = field->fld_data.fld_encoding->enc_type;
				}
			}
		}
	}

	/* regard `Content-Transfer-Encoding:` */
	if( mime_transfer_encoding == MAILMIME_MECHANISM_7BIT
	 || mime_transfer_encoding == MAILMIME_MECHANISM_8BIT
	 || mime_transfer_encoding == MAILMIME_MECHANISM_BINARY )
	{
		decoded_data       = mime_data->dt_data.dt_text.dt_data;
		decoded_data_bytes = mime_data->dt_data.dt_text.dt_length;
		if( decoded_data == NULL || decoded_data_bytes <= 0 ) {
			goto cleanup; /* no error - but no data */
		}
	}
	else
	{
		int r;
		size_t current_index = 0;
		r = mailmime_part_parse(mime_data->dt_data.dt_text.dt_data, mime_data->dt_data.dt_text.dt_length,
			&current_index, mime_transfer_encoding,
			&transfer_decoding_buffer, &decoded_data_bytes);
		if( r != MAILIMF_NO_ERROR || transfer_decoding_buffer == NULL || decoded_data_bytes <= 0 ) {
			goto cleanup;
		}
		decoded_data = transfer_decoding_buffer;
	}

	/* encrypted, decoded data in decoded_data now ... */
    if( !has_decrypted_pgp_armor(decoded_data, decoded_data_bytes) ) {
		goto cleanup;
    }

    if( !mre2ee_driver_decrypt(mailbox, decoded_data, decoded_data_bytes, private_keyring, 1, &plain_buf, &plain_bytes) ) {
		goto cleanup;
    }

cleanup:
	if( transfer_decoding_buffer ) {
		mmap_string_unref(transfer_decoding_buffer);
	}
}


static void decrypt_recursive(mrmailbox_t* mailbox, struct mailmime* mime, const mrkeyring_t* private_keyring)
{
	struct mailmime_content* ct;
	clistiter*               cur;

	if( mailbox == NULL || mime == NULL ) {
		return;
	}

	if( mime->mm_type == MAILMIME_MULTIPLE )
	{
		ct = mime->mm_content_type;
		if( ct && ct->ct_subtype && strcmp(ct->ct_subtype, "encrypted")==0 ) {
			/* decrypt "multipart/encrypted" -- child parts are eg. "application/pgp-encrypted" (uninteresting, version only),
			"application/octet-stream" (the interesting data part) and optional, unencrypted help files */
			for( cur=clist_begin(mime->mm_data.mm_multipart.mm_mp_list); cur!=NULL; cur=clist_next(cur)) {
				decrypt_part(mailbox, (struct mailmime*)clist_content(cur), private_keyring);
			}
		}
		else {
			for( cur=clist_begin(mime->mm_data.mm_multipart.mm_mp_list); cur!=NULL; cur=clist_next(cur)) {
				decrypt_recursive(mailbox, (struct mailmime*)clist_content(cur), private_keyring);
			}
		}
	}
	else if( mime->mm_type == MAILMIME_MESSAGE )
	{
		decrypt_recursive(mailbox, mime->mm_data.mm_message.mm_msg_mime, private_keyring);
	}
}


void mre2ee_decrypt(mrmailbox_t* mailbox, struct mailmime* in_out_message)
{
	struct mailimf_fields* imffields = mr_find_mailimf_fields(in_out_message); /*just a pointer into mailmime structure, must not be freed*/
	mraheader_t*           autocryptheader = mraheader_new();
	int                    autocryptheader_fine = 0;
	time_t                 message_time = 0;
	mrapeerstate_t*        peerstate = mrapeerstate_new();
	int                    locked = 0;
	char*                  from = NULL, *self_addr = NULL;
	mrkeyring_t*           private_keyring = mrkeyring_new();

	if( mailbox==NULL || in_out_message==NULL
	 || imffields==NULL || autocryptheader==NULL || peerstate==NULL || private_keyring==NULL ) {
		goto cleanup;
	}

	/* Autocrypt preparations:
	- Set message_time and from (both may be unset)
	- Get the autocrypt header, if any.
	- Do not abort on errors - we should try at last the decyption below */
	if( imffields )
	{
		struct mailimf_field* field = mr_find_mailimf_field(imffields, MAILIMF_FIELD_FROM);
		if( field && field->fld_data.fld_from ) {
			from = mr_find_first_addr(field->fld_data.fld_from->frm_mb_list);
		}

		field = mr_find_mailimf_field(imffields, MAILIMF_FIELD_ORIG_DATE);
		if( field && field->fld_data.fld_orig_date ) {
			struct mailimf_orig_date* orig_date = field->fld_data.fld_orig_date;
			if( orig_date ) {
				message_time = mr_timestamp_from_date(orig_date->dt_date_time); /* is not yet checked against bad times! */
				if( message_time != MR_INVALID_TIMESTAMP && message_time > time(NULL) ) {
					message_time = time(NULL);
				}
			}
		}
	}

	autocryptheader_fine = mraheader_set_from_imffields(autocryptheader, imffields);
	if( autocryptheader_fine ) {
		if( from == NULL ) {
			from = safe_strdup(autocryptheader->m_to);
		}
		else if( strcasecmp(autocryptheader->m_to, from /*SIC! compare to= against From: - the key is for answering!*/)!=0 ) {
			autocryptheader_fine = 0;
		}

		if( !mre2ee_driver_is_valid_key(mailbox, autocryptheader->m_public_key) ) {
			autocryptheader_fine = 0;
		}
	}

	/* modify the peerstate (eg. if there is a peer but not autocrypt header, stop encryption) */
	mrsqlite3_lock(mailbox->m_sql);
	locked = 1;

		/* apply Autocrypt:-header only if encryption is enabled (if we're out of beta, we should do this always to track the correct state; now we want no bugs spread widely to the databases :-) */
		if( mrsqlite3_get_config_int__(mailbox->m_sql, "e2ee_enabled", MR_E2EE_DEFAULT_ENABLED) != 0
		 && message_time > 0
		 && from )
		{
			if( mrapeerstate_load_from_db__(peerstate, mailbox->m_sql, from) ) {
				if( autocryptheader_fine ) {
					mrapeerstate_apply_header(peerstate, autocryptheader, message_time);
					mrapeerstate_save_to_db__(peerstate, mailbox->m_sql, 0/*no not create*/);
				}
				else {
					if( message_time > peerstate->m_last_seen ) {
						mrapeerstate_degrade_encryption(peerstate, message_time);
						mrapeerstate_save_to_db__(peerstate, mailbox->m_sql, 0/*no not create*/);
					}
				}
			}
			else if( autocryptheader_fine ) {
				mrapeerstate_init_from_header(peerstate, autocryptheader, message_time);
				mrapeerstate_save_to_db__(peerstate, mailbox->m_sql, 1/*create*/);
			}
		}

		/* load private key for decryption */
		if( (self_addr=mrsqlite3_get_config__(mailbox->m_sql, "configured_addr", NULL))==NULL ) {
			goto cleanup;
		}

		if( !mrkeyring_load_self_private__(private_keyring, self_addr, mailbox->m_sql) ) {
			goto cleanup;
		}

	mrsqlite3_unlock(mailbox->m_sql);
	locked = 0;

	/* finally, decrypt */
	decrypt_recursive(mailbox, in_out_message, private_keyring);

cleanup:
	if( locked ) { mrsqlite3_unlock(mailbox->m_sql); }
	mraheader_unref(autocryptheader);
	mrapeerstate_unref(peerstate);
	mrkeyring_unref(private_keyring);
	free(from);
	free(self_addr);
}

