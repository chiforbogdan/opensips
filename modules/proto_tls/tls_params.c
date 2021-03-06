/*
 * Copyright (C) 2015 OpenSIPS Solutions
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *
 *
 * History:
 * -------
 *  2015-02-18  first version (bogdan)
 */


#include <string.h>

#include "../../dprint.h"
#include "../../resolve.h"  /* for str2ip() */
#include "../../ut.h"
#include "tls_params.h"
#include "tls_domain.h"


static int parse_domain_def(char *val, str *id, struct ip_addr **ip,
											unsigned int *port, str *domain)
{
	char *p = (char*)val;
	str s;

	/* format is ID=ip:port or  ID=string */

	/* first get the ID */
	id->s = p;
	if ( (p=strchr( p, '='))==NULL )
		goto parse_err;
	id->len = p-id->s;
	p++;

	/* get IP */
	s.s = p;
	if ( (p=strchr( p, ':'))==NULL )
		goto has_domain;
	s.len = p-s.s;
	p++;
	if ( (*ip=str2ip( &s ))==NULL ) {
		LM_ERR("[%.*s] is not an ip\n", s.len, s.s);
		goto parse_err;
	}

	/* what is left should be a port */
	s.s = p;
	s.len = val + strlen(val) - p;
	if (str2int( &s, port)<0) {
		LM_ERR("[%.*s] is not a port\n", s.len, s.s);
		goto parse_err;
	}

	return 0;

has_domain:
	/* what is left should be a domain */
	domain->s = s.s;
	domain->len = val + strlen(val) - s.s;
	*ip = NULL;

	return 0;
parse_err:
	LM_ERR("invalid TSL domain [%s] (error around pos %d)\n",
		val, (int)(long)(p-val) );
	return -1;
}


int tlsp_add_srv_domain(modparam_t type, void *val)
{
	struct ip_addr *ip;
	unsigned int port;
	str domain;
	str id;

	if (parse_domain_def( (char*)val, &id, &ip, &port, &domain)<0 )
		return -1;

	if (ip==NULL) {
		LM_ERR("server domains do not support 'domain name' in definition\n");
		return -1;
	}

	/* add domain */
	if (tls_new_server_domain( &id, ip, port )<0) {
		LM_ERR("failed to add new server domain [%s]\n",(char*)val);
		return -1;
	}

	return 1;
}


int tlsp_add_cli_domain(modparam_t type, void *val)
{
	struct ip_addr *ip;
	unsigned int port;
	str domain;
	str id;

	if (parse_domain_def( (char*)val, &id, &ip, &port, &domain)<0 )
		return -1;

	/* add domain */
	if (ip==NULL) {
		if (tls_new_client_domain_name( &id, &domain)<0) {
			LM_ERR("failed to add new client domain name [%s]\n",(char*)val);
			return -1;
		}
	} else {
		if (tls_new_client_domain( &id, ip, port )<0) {
			LM_ERR("failed to add new client domain [%s]\n",(char*)val);
			return -1;
		}
	}

	return 1;
}

static void split_param_val(char *in, str *id, str *val)
{
	char *p = (char*)in;

	/* format is '[ID=]value' */

	/* first try to get the ID */
	id->s = p;
	if ( (p=strchr( p, ':'))==NULL ) {
		/* just value */
		val->s = id->s;
		val->len = strlen(id->s);
		id->s = NULL;
		id->len = 0;
		return;
	}
	/* ID found */
	id->len = p-id->s;
	p++;

	/* what is left should be the value */
	val->s = p;
	val->len = in + strlen(in) - p;
	return;
}


#define set_domain_attr( _id, _field, _val) \
	do { \
		struct tls_domain *_d; \
		if ((_id).s) { \
			/* specific TLS domain */ \
			if ( (_d=tls_find_domain_by_id(&(_id)))==NULL ) { \
				LM_ERR("TLS domain [%.*s] not defined in [%s]\n", \
					(_id).len, (_id).s, (char*)in); \
				return -1; \
			} \
			_d->_field = _val; \
		} else { \
			/* set default domains */ \
			tls_default_server_domain._field = _val; \
			tls_default_client_domain._field = _val; \
		} \
	} while(0)



int tlsp_set_method(modparam_t type, void *in)
{
	str id;
	str val;
	int method;

	split_param_val( (char*)in, &id, &val);

	if ( strcasecmp( val.s, "SSLV23")==0 || strcasecmp( val.s, "TLSany")==0 )
		method = TLS_USE_SSLv23;
	else if ( strcasecmp( val.s, "TLSV1")==0 )
		method = TLS_USE_TLSv1;
	else if ( strcasecmp( val.s, "TLSV1_2")==0 )
		method = TLS_USE_TLSv1_2;
	else {
		LM_ERR("unsupported method [%s]\n",val.s);
		return -1;
	}

	set_domain_attr( id, method, method);
	return 1;
}


int tlsp_set_verify(modparam_t type, void *in)
{
	str id;
	str val;
	unsigned int verify;

	split_param_val( (char*)in, &id, &val);

	if (str2int( &val, &verify)!=0) {
		LM_ERR("option is not a number [%s]\n",val.s);
		return -1;
	}

	set_domain_attr( id, verify_cert, verify);
	return 1;
}


int tlsp_set_require(modparam_t type, void *in)
{
	str id;
	str val;
	unsigned int req;

	split_param_val( (char*)in, &id, &val);

	if (str2int( &val, &req)!=0) {
		LM_ERR("option is not a number [%s]\n",val.s);
		return -1;
	}

	set_domain_attr( id, require_client_cert, req);
	return 1;
}


int tlsp_set_certificate(modparam_t type, void *in)
{
	str id;
	str val;

	split_param_val( (char*)in, &id, &val);

	set_domain_attr( id, cert_file, val.s);
	return 1;
}


int tlsp_set_pk(modparam_t type, void *in)
{
	str id;
	str val;

	split_param_val( (char*)in, &id, &val);

	set_domain_attr( id, pkey_file, val.s);
	return 1;
}


int tlsp_set_calist(modparam_t type, void *in)
{
	str id;
	str val;

	split_param_val( (char*)in, &id, &val);

	set_domain_attr( id, ca_file, val.s);
	return 1;
}


int tlsp_set_cadir(modparam_t type, void *in)
{
	str id;
	str val;

	split_param_val( (char*)in, &id, &val);

	set_domain_attr( id, ca_directory, val.s);
	return 1;
}


int tlsp_set_cplist(modparam_t type, void *in)
{
	str id;
	str val;

	split_param_val( (char*)in, &id, &val);

	set_domain_attr( id, ciphers_list, val.s);
	return 1;
}


int tlsp_set_dhparams(modparam_t type, void *in)
{
	str id;
	str val;

	split_param_val( (char*)in, &id, &val);

	set_domain_attr( id, tmp_dh_file, val.s);
	return 1;
}


int tlsp_set_eccurve(modparam_t type, void *in)
{
	str id;
	str val;

	split_param_val( (char*)in, &id, &val);

	set_domain_attr( id, tls_ec_curve, val.s);
	return 1;
}


