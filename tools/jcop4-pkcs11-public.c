/* Bounded public inspection of the locally built PKCS11 module. No login API.
 * One selected module reader/context/connection; guards precede APDU transport.
 * Native PCSC context discovery can also query other readers' capabilities.
 * No certificate, identity, IDs or raw response is written to the report. */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "pkcs11/pkcs11.h"
#include "libopensc/internal.h"
#include "libopensc/iasecc.h"
#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/x509.h>
#include <fcntl.h>
#include <unistd.h>
#ifndef JCOP4_LOCAL_MODULE_PATH
#error Build must pin the local module path
#endif
static FILE *pub_report;
/* Library stdout/stderr are discarded; only our fixed metadata reaches it. */
#define printf(...) fprintf(pub_report,__VA_ARGS__)
static volatile sig_atomic_t pub_interrupted;
static struct {
	int (*create)(sc_context_t **,const sc_context_param_t *);
	int (*release)(sc_context_t *);
	int (*detect)(sc_context_t *);
	int (*presence)(sc_reader_t *);
	unsigned int (*count)(sc_context_t *);
	sc_reader_t *(*reader)(sc_context_t *,unsigned int);
	int (*connect)(sc_reader_t *,sc_card_t **);
	int (*disconnect)(sc_card_t *);
} pub_real;
static struct {
	sc_context_t *ctx; sc_reader_t *reader; sc_card_t *card;
	const struct sc_reader_operations *original; struct sc_reader_operations filtered;
	unsigned int reader_index, creates, connects, reader_connects, commands, reads, selects;
	unsigned int locks, unlocks, record, selected, read_budget[9];
	unsigned int releases, reader_disconnects, card_disconnects;
	int app, blocked, connected, card_connected, context_ready, policy_verified, release_result, disconnect_result, card_disconnect_result;
	unsigned int failure_ins, failure_sw; int failure_transport;
	const char *stage, *failure_stage;
} pub_guard;
static const struct { unsigned int fid, size; } pub_files[] = {
	{0x2F00,1024},{0x5031,1024},{0x5032,1024},{0x7001,300},{0x7002,2048},
	{0x7004,4096},{0x7005,2368},{0xB001,3000},{0xB002,3000}
};
static const struct iasecc_chipdoc_profile *pub_profile(void)
{ return iasecc_chipdoc_profile(IASECC_CHIPDOC_CR_RSA2048); }
static int pub_atr(const struct sc_atr *atr)
{ const struct iasecc_chipdoc_profile *p = pub_profile(); return atr && atr->len == p->atr_len && !memcmp(atr->value,p->atr,p->atr_len); }
static void pub_signal(int sig) { (void)sig; pub_interrupted = 1; }
static int pub_stop(unsigned int ins, int result)
{
	if (!pub_guard.blocked) { pub_guard.failure_ins = ins; pub_guard.failure_transport = result; pub_guard.failure_stage = pub_guard.stage; }
	pub_guard.blocked = 1; return result;
}
static int pub_reader_connect(sc_reader_t *r)
{
	if (r != pub_guard.reader || pub_interrupted || pub_guard.reader_connects++ || !pub_atr(&r->atr) ||
		(r->flags & SC_READER_ENABLE_ESCAPE)) return pub_stop(0,SC_ERROR_NOT_ALLOWED);
	int rv = pub_guard.original->connect(r);
	if (rv) return pub_stop(0,rv);
	pub_guard.connected = 1;
	if (!pub_atr(&r->atr) || r->active_protocol != SC_PROTO_T1 || r->max_send_size < 139 || r->max_recv_size < 291 ||
		(r->flags & SC_READER_ENABLE_ESCAPE)) return pub_stop(0,SC_ERROR_NOT_SUPPORTED);
	pub_guard.policy_verified = 1;
	return 0;
}
static int pub_reader_disconnect(sc_reader_t *r)
{
	if (r != pub_guard.reader || !pub_guard.connected) return SC_ERROR_NOT_ALLOWED;
	pub_guard.reader_disconnects++;
	pub_guard.disconnect_result = pub_guard.original->disconnect(r); pub_guard.connected = 0;
	return pub_guard.disconnect_result;
}
static int pub_lock(sc_reader_t *r)
{ if (r != pub_guard.reader || pub_guard.blocked || pub_interrupted) return SC_ERROR_NOT_ALLOWED; int rv = pub_guard.original->lock(r); if (!rv) pub_guard.locks++; return rv; }
static int pub_unlock(sc_reader_t *r)
{ if (r != pub_guard.reader) return SC_ERROR_NOT_ALLOWED; int rv = pub_guard.original->unlock(r); if (!rv) pub_guard.unlocks++; return rv; }
static int pub_reset(sc_reader_t *r,int cold) { (void)r; (void)cold; return pub_stop(0,SC_ERROR_NOT_ALLOWED); }
static int pub_transmit(sc_reader_t *r,sc_apdu_t *a)
{
	unsigned int fid = 0, file = 9, offset = 0; int selected_app = pub_guard.app;
	if (!a || r != pub_guard.reader || !pub_guard.connected || !pub_atr(&r->atr) || r->active_protocol != SC_PROTO_T1 ||
		pub_interrupted || pub_guard.blocked || pub_guard.commands >= 128 || a->cla || a->control ||
		(a->flags & (SC_APDU_FLAGS_CHAINING|SC_APDU_FLAGS_SM_CHAINING)) || a->lc != a->datalen || (a->lc && !a->data))
		return pub_stop(a ? a->ins : 0,SC_ERROR_NOT_ALLOWED);
	if (a->ins == 0xA4) {
		if ((a->p2 != 0 && a->p2 != 0x0C) ||
			(a->cse != SC_APDU_CASE_3_SHORT && a->cse != SC_APDU_CASE_4_SHORT) ||
			(a->cse == SC_APDU_CASE_3_SHORT ? a->le != 0 : !a->resp || !a->le || a->le > 256))
			return pub_stop(a->ins,SC_ERROR_NOT_ALLOWED);
		if (a->p1 == 4 && a->lc == pub_profile()->aid_len && !memcmp(a->data,pub_profile()->aid,a->lc)) { fid = 0x5015; selected_app = 1; }
		else if (a->lc == 2 && a->data[0] == 0x3F && !a->data[1] && !a->p1) { fid = 0x3F00; selected_app = 0; }
		else {
			if (a->lc != 2 || (a->p1 != 0 && a->p1 != 8 && a->p1 != 9)) return pub_stop(a->ins,SC_ERROR_NOT_ALLOWED);
			fid = a->data[0]*256+a->data[1];
			if (fid == 0x2F00 && a->p1 == 8) selected_app = 0;
			else if (!pub_guard.app || a->p1 == 8 || fid == 0x2F00) return pub_stop(a->ins,SC_ERROR_NOT_ALLOWED);
			for (file = 0; file < 9 && pub_files[file].fid != fid; file++);
			if (file == 9) return pub_stop(a->ins,SC_ERROR_NOT_ALLOWED);
		}
	} else if (a->ins == 0xB0 || a->ins == 0xB2) {
		if (a->cse != SC_APDU_CASE_2_SHORT || a->lc || !a->resp || !a->le || a->le > 256 || a->resplen < a->le ||
			(a->p1 & 0x80)) return pub_stop(a->ins,SC_ERROR_NOT_ALLOWED);
		for (file = 0; file < 9 && pub_files[file].fid != pub_guard.selected; file++);
		if (file == 9 || (file && !pub_guard.app)) return pub_stop(a->ins,SC_ERROR_NOT_ALLOWED);
		if (a->ins == 0xB2) {
			if (file || a->p2 != 4 || a->p1 != pub_guard.record+1 || a->p1 > 4) return pub_stop(a->ins,SC_ERROR_NOT_ALLOWED);
		} else {
			offset = a->p1*256+a->p2;
			if (offset >= pub_files[file].size || a->le > pub_files[file].size-offset) return pub_stop(a->ins,SC_ERROR_NOT_ALLOWED);
		}
		if (a->le > pub_guard.read_budget[file]) return pub_stop(a->ins,SC_ERROR_NOT_ALLOWED);
	} else return pub_stop(a->ins,SC_ERROR_NOT_ALLOWED);
	a->flags |= SC_APDU_FLAGS_NO_RETRY_WL|SC_APDU_FLAGS_NO_GET_RESP|SC_APDU_FLAGS_NO_SM;
	pub_guard.commands++;
	if (a->ins == 0xA4) pub_guard.selects++;
	else { pub_guard.reads++; pub_guard.read_budget[file] -= a->le; }
	size_t capacity = a->resplen; int rv = pub_guard.original->transmit(r,a);
	if (rv || a->resplen > capacity || a->resplen > a->le || !pub_atr(&r->atr) || r->active_protocol != SC_PROTO_T1)
		return pub_stop(a->ins,rv ? rv : SC_ERROR_UNKNOWN_DATA_RECEIVED);
	unsigned int sw = a->sw1*256+a->sw2;
	if (sw != 0x9000 && !(a->ins == 0xB0 && sw == 0x6282) && !(a->ins == 0xB2 && sw == 0x6A83)) {
		if (!pub_guard.blocked) pub_guard.failure_sw = sw;
		return pub_stop(a->ins,SC_ERROR_CARD_CMD_FAILED);
	}
	if (a->ins == 0xA4) { pub_guard.selected = fid; pub_guard.app = selected_app; pub_guard.record = 0; }
	if (a->ins == 0xB2) pub_guard.record++;
	return 0;
}
/* These symbols are exported only by the diagnostic executable. The actual
 * module/context APIs are forwarded; namespace presents the chosen reader. */
int sc_context_create(sc_context_t **out,const sc_context_param_t *opts)
{
	pub_guard.stage = "context";
	if (!out || !opts || pub_guard.creates++ || !opts->app_name || strcmp(opts->app_name,"opensc-pkcs11")) return SC_ERROR_NOT_ALLOWED;
	int rv = pub_real.create(out,opts); if (rv) return rv;
	pub_guard.ctx = *out;
	if (!*out || (*out)->debug || !(*out)->ossl3ctx || pub_guard.reader_index >= pub_real.count(*out)) return SC_ERROR_NOT_SUPPORTED;
	if (!(*out)->card_drivers[0] || !(*out)->card_drivers[0]->short_name ||
		strcmp((*out)->card_drivers[0]->short_name,"iasecc") || (*out)->card_drivers[1] ||
		(*out)->card_drivers[0]->dll || !(*out)->reader_driver || !(*out)->reader_driver->short_name ||
		strcmp((*out)->reader_driver->short_name,"pcsc") ||
		((*out)->forced_driver && (*out)->forced_driver != (*out)->card_drivers[0])) return SC_ERROR_NOT_SUPPORTED;
	pub_guard.reader = pub_real.reader(*out,pub_guard.reader_index);
	sc_reader_t *r = pub_guard.reader;
	if (!r || !r->ops || !r->ops->connect || !r->ops->disconnect || !r->ops->transmit || !r->ops->lock || !r->ops->unlock ||
		!(pub_real.presence(r) & SC_READER_CARD_PRESENT) ||
		!pub_atr(&r->atr) || (r->flags & SC_READER_ENABLE_ESCAPE)) return SC_ERROR_NOT_SUPPORTED;
	pub_guard.original = r->ops; pub_guard.filtered = *r->ops;
	pub_guard.filtered.connect = pub_reader_connect; pub_guard.filtered.disconnect = pub_reader_disconnect;
	pub_guard.filtered.transmit = pub_transmit; pub_guard.filtered.lock = pub_lock; pub_guard.filtered.unlock = pub_unlock;
	pub_guard.filtered.reset = pub_reset; pub_guard.filtered.perform_verify = NULL; pub_guard.filtered.perform_pace = NULL;
	pub_guard.filtered.display_message = NULL; pub_guard.filtered.set_protocol = NULL; pub_guard.filtered.use_reader = NULL;
	r->ops = &pub_guard.filtered; pub_guard.context_ready = 1;
	return 0;
}
int sc_release_context(sc_context_t *ctx)
{
	if (ctx != pub_guard.ctx) return pub_real.release(ctx);
	pub_guard.releases++;
	if (pub_guard.connected && pub_guard.original) (void)pub_reader_disconnect(pub_guard.reader);
	if (pub_guard.reader && pub_guard.original) pub_guard.reader->ops = pub_guard.original;
	pub_guard.release_result = pub_real.release(ctx); pub_guard.ctx = NULL; pub_guard.reader = NULL;
	return pub_guard.release_result;
}
unsigned int sc_ctx_get_reader_count(sc_context_t *ctx)
{ return ctx && ctx == pub_guard.ctx && pub_guard.context_ready ? 1 : pub_real.count(ctx); }
sc_reader_t *sc_ctx_get_reader(sc_context_t *ctx,unsigned int index)
{ return ctx && ctx == pub_guard.ctx && pub_guard.context_ready ? index ? NULL : pub_guard.reader : pub_real.reader(ctx,index); }
int sc_ctx_detect_readers(sc_context_t *ctx)
{ return ctx && ctx == pub_guard.ctx && pub_guard.context_ready ? 0 : pub_real.detect(ctx); }
int sc_connect_card(sc_reader_t *r,sc_card_t **out)
{
	pub_guard.stage = "card";
	if (r != pub_guard.reader || pub_guard.connects++ || pub_guard.blocked || !out) return SC_ERROR_NOT_ALLOWED;
	int rv = pub_real.connect(r,out); if (rv) return pub_stop(0,rv);
	pub_guard.card = *out; pub_guard.card_connected = 1;
	if (!pub_guard.connected || !iasecc_chipdoc_configured(*out) || (*out)->type != SC_CARD_TYPE_IASECC_CHIPDOC ||
		(*out)->sm_ctx.sm_mode != SM_MODE_NONE || !pub_atr(&(*out)->atr)) return pub_stop(0,SC_ERROR_NOT_SUPPORTED);
	pub_guard.stage = "directory";
	if ((*out)->app_count < 0 && (rv = sc_enum_apps(*out))) return pub_stop(0,rv);
	if ((*out)->app_count != 1 || !(*out)->app[0] || (*out)->app[0]->aid.len != pub_profile()->aid_len ||
		memcmp((*out)->app[0]->aid.value,pub_profile()->aid,pub_profile()->aid_len) ||
		(*out)->app[0]->path.type != SC_PATH_TYPE_DF_NAME || (*out)->app[0]->path.len != pub_profile()->aid_len ||
		memcmp((*out)->app[0]->path.value,pub_profile()->aid,pub_profile()->aid_len)) return pub_stop(0,SC_ERROR_NOT_SUPPORTED);
	pub_guard.stage = "native_binding";
	return 0;
}
int sc_disconnect_card(sc_card_t *card)
{
	if (card != pub_guard.card || !pub_guard.card_connected) return SC_ERROR_NOT_ALLOWED;
	pub_guard.card_disconnects++;
	struct iasecc_private_data *prv = card->drv_data;
	if (card->lock_count || !prv || prv->chipdoc_transaction.active || prv->chipdoc_session.channel || prv->chipdoc_recovery_required)
		(void)pub_stop(0,SC_ERROR_NOT_ALLOWED);
	int rv = pub_real.disconnect(card); pub_guard.card_disconnect_result = rv; pub_guard.card_connected = 0; pub_guard.card = NULL; return rv;
}
static CK_RV pub_find(CK_FUNCTION_LIST_PTR api,CK_SESSION_HANDLE session,CK_OBJECT_CLASS type,CK_OBJECT_HANDLE objects[8],CK_ULONG *count)
{
	CK_ATTRIBUTE attr = {CKA_CLASS,&type,sizeof(type)}; CK_RV rv = api->C_FindObjectsInit(session,&attr,1);
	if (rv) return rv;
	rv = api->C_FindObjects(session,objects,8,count); CK_RV ended = api->C_FindObjectsFinal(session);
	return rv ? rv : ended ? ended : *count >= 8 ? CKR_GENERAL_ERROR : CKR_OK;
}
static CK_RV pub_attribute(CK_FUNCTION_LIST_PTR api,CK_SESSION_HANDLE s,CK_OBJECT_HANDLE o,CK_ATTRIBUTE_TYPE type,void *buf,CK_ULONG *size)
{ CK_ATTRIBUTE a = {type,buf,*size}; CK_RV rv = api->C_GetAttributeValue(s,o,&a,1); *size = a.ulValueLen; return rv; }
static int pub_inspect(CK_FUNCTION_LIST_PTR api,CK_SLOT_ID slot)
{
	CK_TOKEN_INFO token; CK_RV rv = api->C_GetTokenInfo(slot,&token); printf("token_result=%lu\n",rv);
	if (rv || pub_guard.blocked) return 1;
	if ((token.flags & (CKF_WRITE_PROTECTED|CKF_LOGIN_REQUIRED|CKF_USER_PIN_INITIALIZED)) !=
		(CKF_WRITE_PROTECTED|CKF_LOGIN_REQUIRED|CKF_USER_PIN_INITIALIZED) ||
		(token.flags & (CKF_PROTECTED_AUTHENTICATION_PATH|CKF_RNG)) || token.ulMinPinLen != 6 || token.ulMaxPinLen != 14) return 1;
	printf("token_policy=readonly_software_pin_6_14_no_rng\n");
	CK_MECHANISM_TYPE types[64]; CK_ULONG n = 64; rv = api->C_GetMechanismList(slot,types,&n); printf("mechanism_result=%lu\n",rv);
	if (rv || n > 64 || pub_guard.blocked) return 1;
	unsigned int signing = 0;
	for (CK_ULONG i = 0; i < n; i++) {
		CK_MECHANISM_INFO info; if (api->C_GetMechanismInfo(slot,types[i],&info)) return 1;
		if (info.flags & (CKF_DECRYPT|CKF_ENCRYPT|CKF_GENERATE|CKF_GENERATE_KEY_PAIR|CKF_WRAP|CKF_UNWRAP|CKF_DERIVE)) return 1;
		if (info.flags & CKF_SIGN) {
			if ((types[i] != CKM_RSA_PKCS && types[i] != CKM_SHA256_RSA_PKCS) || info.ulMinKeySize != 2048 || info.ulMaxKeySize != 2048) return 1;
			signing++;
		} else if (info.flags != CKF_DIGEST) return 1;
	}
	if (signing != 2) return 1;
	printf("mechanism_policy=rsa2048_pkcs_sha256_sign_and_software_digests\n");
	CK_SESSION_HANDLE session; rv = api->C_OpenSession(slot,CKF_SERIAL_SESSION,NULL,NULL,&session); printf("session_open_result=%lu\n",rv);
	if (rv) return 1;
	int failed = 1; CK_OBJECT_HANDLE certs[8], pubs[8], privs[8]; CK_ULONG cert_count, pub_count, priv_count;
	pub_guard.stage = "public_objects";
	if (pub_find(api,session,CKO_CERTIFICATE,certs,&cert_count) || pub_find(api,session,CKO_PUBLIC_KEY,pubs,&pub_count) ||
		pub_find(api,session,CKO_PRIVATE_KEY,privs,&priv_count) || pub_guard.blocked || !cert_count || !pub_count || priv_count) goto done;
	for (CK_ULONG i = 0; i < cert_count; i++) {
		unsigned char id[64], der[8192]; CK_ULONG id_size = sizeof(id), der_size = sizeof(der);
		if (pub_attribute(api,session,certs[i],CKA_ID,id,&id_size) || !id_size || id_size > sizeof(id) ||
			pub_attribute(api,session,certs[i],CKA_VALUE,der,&der_size) || !der_size || der_size > sizeof(der)) goto done;
		const unsigned char *cursor = der; X509 *cert = d2i_X509(NULL,&cursor,(long)der_size);
		EVP_PKEY *key = cert ? X509_get_pubkey(cert) : NULL;
		int valid = key && cursor == der+der_size && EVP_PKEY_is_a(key,"RSA") && EVP_PKEY_get_bits(key) == 2048;
		EVP_PKEY_free(key); X509_free(cert); sc_mem_clear(der,sizeof(der));
		if (!valid) goto done;
		int matched = 0;
		for (CK_ULONG j = 0; j < pub_count; j++) {
			unsigned char other[64]; CK_ULONG size = sizeof(other);
			if (pub_attribute(api,session,pubs[j],CKA_ID,other,&size) || size > sizeof(other)) goto done;
			if (size == id_size && !memcmp(other,id,size)) matched = 1;
			sc_mem_clear(other,sizeof(other));
		}
		sc_mem_clear(id,sizeof(id)); if (!matched) goto done;
	}
	CK_SESSION_INFO info; if (api->C_GetSessionInfo(session,&info) || info.state != CKS_RO_PUBLIC_SESSION || pub_guard.blocked) goto done;
	printf("certificate_count=%lu\npublic_key_count=%lu\nprivate_key_visibility=hidden_without_login\ncertificate_policy=rsa2048_der_with_matching_public_ids_no_identity_validation\n",cert_count,pub_count);
	failed = 0;
done:
	rv = api->C_CloseSession(session); printf("session_close_result=%lu\n",rv); return failed || rv || pub_guard.blocked;
}
#ifndef JCOP4_PKCS11_PUBLIC_FIXTURE
static int pub_resolve(void)
{
#define RESOLVE(field,name) do { pub_real.field = dlsym(RTLD_NEXT,name); if (!pub_real.field) return 1; } while (0)
	RESOLVE(create,"sc_context_create"); RESOLVE(release,"sc_release_context"); RESOLVE(count,"sc_ctx_get_reader_count");
	RESOLVE(detect,"sc_ctx_detect_readers"); RESOLVE(presence,"sc_detect_card_presence");
	RESOLVE(reader,"sc_ctx_get_reader"); RESOLVE(connect,"sc_connect_card"); RESOLVE(disconnect,"sc_disconnect_card");
#undef RESOLVE
	return 0;
}
#endif
int main(int argc,char **argv)
{
	if (argc != 3 || !argv[1][0] || strcmp(argv[2],JCOP4_LOCAL_MODULE_PATH)) return 2;
	for (const char *p = argv[1]; *p; p++) if (*p < '0' || *p > '9') return 2;
	char *end; errno = 0; unsigned long index = strtoul(argv[1],&end,10);
	if (errno || *end || argv[1][0] == '-' || index > 1024) return 2;
	int saved = dup(STDOUT_FILENO), null = open("/dev/null",O_WRONLY|O_CLOEXEC);
	if (saved < 0 || null < 0) { if (saved >= 0) close(saved); if (null >= 0) close(null); return 1; }
	pub_report = fdopen(saved,"w"); if (!pub_report) { close(saved); close(null); return 1; }
	if (dup2(null,STDOUT_FILENO) < 0 || dup2(null,STDERR_FILENO) < 0) { close(null); fclose(pub_report); pub_report = NULL; return 1; }
	close(null); setvbuf(pub_report,NULL,_IOLBF,0);
	memset(&pub_guard,0,sizeof(pub_guard)); pub_guard.reader_index = (unsigned int)index; pub_guard.stage = "module";
	for (size_t i = 0; i < 9; i++) pub_guard.read_budget[i] = pub_files[i].size*2;
	pub_interrupted = 0; signal(SIGINT,pub_signal); signal(SIGTERM,pub_signal);
#ifndef JCOP4_PKCS11_PUBLIC_FIXTURE
	if (pub_resolve()) { fclose(pub_report); pub_report = NULL; return 1; }
#endif
	void *module = dlopen(argv[2],RTLD_NOW|RTLD_LOCAL); if (!module) { fclose(pub_report); pub_report = NULL; return 1; }
	CK_C_GetFunctionList get = dlsym(module,"C_GetFunctionList"); CK_FUNCTION_LIST_PTR api = NULL;
	Dl_info origin; int failed = 1, initialized = 0;
	if (!get || get(&api) || !api || !dladdr((void *)api->C_Initialize,&origin) || strcmp(origin.dli_fname,argv[2])) goto done;
	printf("module_identity=local_build\n");
	CK_RV rv = api->C_Initialize(NULL); printf("initialize_result=%lu\n",rv); if (rv) goto done;
	initialized = 1;
	CK_SLOT_ID slots[8]; CK_ULONG count = 8; rv = api->C_GetSlotList(CK_TRUE,slots,&count); printf("slot_list_result=%lu\n",rv);
	if (rv || count != 1 || pub_guard.blocked || !pub_guard.context_ready || !pub_guard.card_connected) goto done;
	pub_guard.stage = "token_and_mechanisms";
	failed = pub_inspect(api,slots[0]);
done:
	if (initialized) { CK_RV finalized = api->C_Finalize(NULL); printf("finalize_result=%lu\n",finalized); if (finalized) failed = 1; }
	printf("stage=%s\nguard_blocked=%d\nbackend_call_count=%u\nselect_count=%u\nread_count=%u\nfirst_failure_ins=%u\nfirst_failure_sw=%u\nfirst_failure_transport=%d\nreader_disconnect_result=%d\ncontext_release_result=%d\n",pub_guard.stage,pub_guard.blocked,pub_guard.commands,pub_guard.selects,pub_guard.reads,pub_guard.failure_ins,pub_guard.failure_sw,pub_guard.failure_transport,pub_guard.disconnect_result,pub_guard.release_result);
	printf("failure_stage=%s\ncard_disconnect_result=%d\nlock_count=%u\nunlock_count=%u\n",pub_guard.failure_stage ? pub_guard.failure_stage : "none",pub_guard.card_disconnect_result,pub_guard.locks,pub_guard.unlocks);
	printf("reader_policy=%s\n",pub_guard.policy_verified ? "validated_t1_known_atr_limits139_291" : "not_validated");
	printf("context_create_attempts=%u\ncontext_release_attempts=%u\ncard_connect_attempts=%u\ncard_disconnect_attempts=%u\nreader_connect_attempts=%u\nreader_disconnect_attempts=%u\n",pub_guard.creates,pub_guard.releases,pub_guard.connects,pub_guard.card_disconnects,pub_guard.reader_connects,pub_guard.reader_disconnects);
	if (pub_guard.blocked || pub_interrupted || pub_guard.card_connected || pub_guard.connected || pub_guard.ctx || pub_guard.locks != pub_guard.unlocks ||
		pub_guard.release_result || pub_guard.disconnect_result || pub_guard.card_disconnect_result) failed = 1;
	printf("public_inspection_result=%s\n",failed ? "incomplete_stop_no_pin" : "validated_public_only_no_login_or_signature");
	dlclose(module); fclose(pub_report); pub_report = NULL; return failed;
}
