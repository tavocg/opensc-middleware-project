/* Only included by the explicit one-login binary, never the public probe. */
#include "libopensc/iasecc-chipdoc-transaction.h"
#include <termios.h>
#include <poll.h>

static const unsigned char sign_message[] = "libjcop3p11: prueba tecnica PKCS11 JCOP4 RSA SHA-256.\n";
static struct {
	int active, initial_reset_result, final_reset_result, pin_input_ok, verified, negative_verified;
	unsigned int step, initial_resets, final_resets, login_calls, credential_calls, pso_calls;
} sign_guard;
static void sign_initialize(void) { memset(&sign_guard,0,sizeof(sign_guard)); }
static int sign_active(void) { return sign_guard.active; }
static struct iasecc_private_data *sign_private(void)
{
	sc_card_t *c = pub_guard.card;
	if (!c || !pub_guard.card_connected || c->reader != pub_guard.reader || c->ctx != pub_guard.ctx ||
		!iasecc_chipdoc_configured(c) || !pub_atr(&c->atr)) return NULL;
	return c->drv_data;
}
static int sign_reset(sc_reader_t *r,int cold)
{
	struct iasecc_private_data *prv = sign_private();
	if (!sign_guard.active || r != pub_guard.reader || cold || !prv || !pub_guard.original->reset ||
		!pub_atr(&r->atr) || r->active_protocol != SC_PROTO_T1 || pub_guard.card->lock_count <= 0)
		return pub_stop(0,SC_ERROR_NOT_ALLOWED);
	if (!sign_guard.initial_resets) {
		if (pub_guard.blocked || pub_interrupted || sign_guard.step ||
			!iasecc_chipdoc_transaction_owns(pub_guard.card,prv)) return pub_stop(0,SC_ERROR_NOT_ALLOWED);
		sign_guard.initial_resets++; sign_guard.initial_reset_result = pub_guard.original->reset(r,cold);
		if (sign_guard.initial_reset_result) return pub_stop(0,sign_guard.initial_reset_result);
		if (!pub_atr(&r->atr) || r->active_protocol != SC_PROTO_T1) return pub_stop(0,SC_ERROR_CARD_RESET);
		return 0;
	}
	/* Production close detaches the owned transaction and erases crypto first.
	 * Permit that single cleanup even after an APDU failure or cancellation. */
	if (sign_guard.initial_reset_result || sign_guard.final_resets || prv->chipdoc_transaction.active ||
		prv->chipdoc_session.channel) return pub_stop(0,SC_ERROR_NOT_ALLOWED);
	sign_guard.final_resets++; sign_guard.final_reset_result = pub_guard.original->reset(r,cold);
	if (sign_guard.final_reset_result) return pub_stop(0,sign_guard.final_reset_result);
	if (!pub_atr(&r->atr) || r->active_protocol != SC_PROTO_T1) return pub_stop(0,SC_ERROR_CARD_RESET);
	return 0;
}
static int sign_transmit(sc_reader_t *r,sc_apdu_t *a)
{
	static const enum iasecc_chipdoc_phase phases[12] = {
		IASECC_CHIPDOC_BOOTSTRAPPING,IASECC_CHIPDOC_BOOTSTRAPPING,IASECC_CHIPDOC_BOOTSTRAPPING,
		IASECC_CHIPDOC_PREPARED,IASECC_CHIPDOC_PREPARED,IASECC_CHIPDOC_PREPARED,
		IASECC_CHIPDOC_CHANNEL,IASECC_CHIPDOC_MF,IASECC_CHIPDOC_PIN_AVAILABLE,
		IASECC_CHIPDOC_LOGGED_IN,IASECC_CHIPDOC_APPLICATION,IASECC_CHIPDOC_MSE
	};
	static const unsigned char mse[] = {0x80,10,4,0,0x7F,0,7,2,2,3,2,2};
	struct iasecc_private_data *prv = sign_private(); unsigned int step = sign_guard.step; int allowed = 0;
	if (!a || !prv || r != pub_guard.reader || !sign_guard.active || !pub_guard.connected ||
		pub_interrupted || pub_guard.blocked || !pub_atr(&r->atr) || r->active_protocol != SC_PROTO_T1 ||
		r->max_send_size < 139 || r->max_recv_size < 291 || sign_guard.initial_resets != 1 ||
		sign_guard.initial_reset_result || sign_guard.final_resets || step >= 12 ||
		prv->chipdoc_recovery_required || (step >= 6 && !prv->chipdoc_session.channel) ||
		!iasecc_chipdoc_transaction_owns(pub_guard.card,prv) || prv->chipdoc_session.phase != phases[step] ||
		a->control || (a->flags & (SC_APDU_FLAGS_CHAINING|SC_APDU_FLAGS_SM_CHAINING)) ||
		a->lc != a->datalen || (a->lc && !a->data) || !a->resp || a->resplen < a->le)
		return pub_stop(a ? a->ins : 0,SC_ERROR_NOT_ALLOWED);
	if (step == 0) allowed = !a->cla && a->ins == 0xA4 && a->p1 == 4 && a->p2 == 0x0C &&
		a->cse == SC_APDU_CASE_3_SHORT && !a->le && a->lc == pub_profile()->aid_len && !memcmp(a->data,pub_profile()->aid,a->lc);
	else if (step == 1) allowed = !a->cla && a->ins == 0xA4 && a->p1 == 2 && !a->p2 &&
		a->cse == SC_APDU_CASE_4_SHORT && a->le == 256 && a->lc == 2 && !memcmp(a->data,"\xd0\x04",2);
	else if (step == 2) allowed = !a->cla && a->ins == 0xB0 && !a->p1 && !a->p2 &&
		a->cse == SC_APDU_CASE_2_SHORT && a->le == 137 && !a->lc;
	else if (step == 3) allowed = !a->cla && a->ins == 0x22 && a->p1 == 0x41 && a->p2 == 0xA4 &&
		a->cse == SC_APDU_CASE_3_SHORT && !a->le && a->lc == sizeof(mse) && !memcmp(a->data,mse,sizeof(mse));
	else if (step == 4) allowed = a->cla == 0x10 && a->ins == 0x86 && !a->p1 && !a->p2 &&
		a->cse == SC_APDU_CASE_3_SHORT && !a->le && a->lc == 139 && !memcmp(a->data,"\x7c\x81\x88\x80\x81\x85\x04",7);
	else if (step == 5) allowed = a->cla == 0x0C && a->ins == 0xB0 && !a->p1 && !a->p2 &&
		a->cse == SC_APDU_CASE_4_SHORT && a->le == 256 && a->lc == 13 &&
		!memcmp(a->data,"\x97\x01\x89\x8e\x08",5);
	else if (step == 6) allowed = a->cla == 0x0C && a->ins == 0xA4 && !a->p1 && !a->p2 &&
		a->cse == SC_APDU_CASE_4_SHORT && a->le == 256 && a->lc == 32 &&
		!memcmp(a->data,"\x87\x11\x01",3) && !memcmp(a->data+19,"\x97\x01\0\x8e\x08",5);
	else if (step == 7) allowed = a->cla == 0x0C && a->ins == 0x20 && !a->p1 && a->p2 == 1 &&
		a->cse == SC_APDU_CASE_4_SHORT && a->le == 256 && a->lc == 10 && !memcmp(a->data,"\x8e\x08",2);
	else if (step >= 8 && step <= 10) allowed = a->cla == 0x0C && a->cse == SC_APDU_CASE_4_SHORT &&
		a->le == 256 && a->lc == 29 && !memcmp(a->data,"\x87\x11\x01",3) && !memcmp(a->data+19,"\x8e\x08",2) &&
		(step == 8 ? a->ins == 0x20 && !a->p1 && a->p2 == 1 && !sign_guard.credential_calls :
		step == 9 ? a->ins == 0xA4 && a->p1 == 4 && a->p2 == 0x0C : a->ins == 0x22 && a->p1 == 0x81 && a->p2 == 0xB6);
	else if (step == 11) allowed = a->cla == 0x0C && a->ins == 0x2A && a->p1 == 0x9E && a->p2 == 0x9A &&
		a->cse == (SC_APDU_CASE_4_EXT) && a->le == 291 && a->lc == 65 && !sign_guard.pso_calls &&
		prv->chipdoc_session.key_reference == 0x91 && prv->chipdoc_session.key_bits == 2048 &&
		!memcmp(a->data,"\x87\x31\x01",3) && !memcmp(a->data+51,"\x97\x02\x01\0\x8e\x08",6);
	if (!allowed) return pub_stop(a->ins,SC_ERROR_NOT_ALLOWED);
	a->flags |= SC_APDU_FLAGS_NO_RETRY_WL|SC_APDU_FLAGS_NO_GET_RESP|SC_APDU_FLAGS_NO_SM;
	sign_guard.step++; pub_guard.commands++; if (step == 8) sign_guard.credential_calls++; if (step == 11) sign_guard.pso_calls++;
	size_t capacity = a->resplen; int rv = pub_guard.original->transmit(r,a);
	if (rv || a->resplen > capacity || (a->le && a->resplen > a->le) || !pub_atr(&r->atr) || r->active_protocol != SC_PROTO_T1)
		return pub_stop(a->ins,rv ? rv : SC_ERROR_UNKNOWN_DATA_RECEIVED);
	unsigned int sw = a->sw1*256+a->sw2;
	/* Only status policies already handled/authenticated by the driver. No retry. */
	if (sw != 0x9000 && !(step == 7 && sw == 0x63C7) && !(step == 8 && (sw & 0xFFF0) == 0x63C0) && !(step == 11 && sw == 0x6A82)) {
		pub_guard.failure_sw = sw; return pub_stop(a->ins,SC_ERROR_CARD_CMD_FAILED);
	}
	return 0;
}
#ifdef JCOP4_PKCS11_TTY_FIXTURE
static int sign_open_tty(void);
#else
static int sign_open_tty(void) { return open("/dev/tty",O_RDWR|O_CLOEXEC); }
#endif
static int sign_pin_read(unsigned char pin[16],size_t *size)
{
	int fd = sign_open_tty(), result = 1; struct termios saved, hidden; *size = 0;
	memset(pin,0,16);
	if (fd < 0) return 1;
	if (!isatty(fd) || tcgetattr(fd,&saved) || !(saved.c_lflag & ICANON)) goto out;
	hidden = saved; hidden.c_lflag &= ~(ECHO|ECHONL);
	if (tcsetattr(fd,TCSAFLUSH,&hidden)) goto out;
	static const char notice[] = "\nSe firmara una sola vez este texto tecnico:\n";
	static const char prompt[] = "PIN (sin eco; un solo intento): ";
	ssize_t n = -1;
	if (!pub_interrupted && write(fd,notice,sizeof(notice)-1) == (ssize_t)(sizeof(notice)-1) &&
		write(fd,sign_message,sizeof(sign_message)-1) == (ssize_t)(sizeof(sign_message)-1) &&
		write(fd,prompt,sizeof(prompt)-1) == (ssize_t)(sizeof(prompt)-1)) {
		/* Poll prevents cancellation immediately before read from being lost. */
		struct pollfd input = {fd,POLLIN,0}; int ready = 0;
		while (!pub_interrupted && !ready) ready = poll(&input,1,100);
		if (!pub_interrupted && ready > 0 && (input.revents & POLLIN)) n = read(fd,pin,16);
	}
	int restored = tcsetattr(fd,TCSANOW,&saved); ssize_t newline = write(fd,"\n",1);
	if (restored || newline != 1 || pub_interrupted || n < 7 || n > 15 || pin[n-1] != '\n') goto out;
	*size = (size_t)n-1; pin[*size] = 0;
	for (size_t i = 0; i < *size; i++) if (pin[i] < 0x20 || pin[i] > 0x7E) { *size = 0; goto out; }
	result = 0;
out:
	if (result) { int flushed = tcflush(fd,TCIFLUSH); (void)flushed; sc_mem_clear(pin,16); }
	close(fd); return result;
}
static int sign_verify(EVP_PKEY *key,const unsigned char signature[256],int altered)
{
	unsigned char message[sizeof(sign_message)]; memcpy(message,sign_message,sizeof(message)); if (altered) message[0] ^= 1;
	EVP_MD_CTX *ctx = EVP_MD_CTX_new(); int rv = -1;
	if (ctx && EVP_DigestVerifyInit_ex(ctx,NULL,"SHA256",NULL,NULL,key,NULL) > 0)
		rv = EVP_DigestVerify(ctx,signature,256,message,sizeof(message)-1);
	EVP_MD_CTX_free(ctx); sc_mem_clear(message,sizeof(message)); return rv;
}
static int sign_inspect(CK_FUNCTION_LIST_PTR api,CK_SLOT_ID slot)
{
	CK_SESSION_HANDLE session; CK_RV rv = api->C_OpenSession(slot,CKF_SERIAL_SESSION,NULL,NULL,&session);
	printf("signature_session_open_result=%lu\n",rv); if (rv) return 1;
	EVP_PKEY *keys[8] = {0}; unsigned char ids[8][64] = {{0}}, pin[16] = {0}, signature[256] = {0};
	CK_ULONG sizes[8] = {0}, count = 0; CK_OBJECT_HANDLE certs[8], privs[8]; int failed = 1, logged = 0;
	pub_guard.stage = "signature_public_certificate";
	if (pub_find(api,session,CKO_CERTIFICATE,certs,&count) || !count || pub_guard.blocked) goto done;
	for (CK_ULONG i = 0; i < count; i++) {
		unsigned char der[8192]; CK_ULONG size = sizeof(der); sizes[i] = sizeof(ids[i]);
		if (pub_attribute(api,session,certs[i],CKA_ID,ids[i],&sizes[i]) || !sizes[i] || sizes[i] > sizeof(ids[i]) ||
			pub_attribute(api,session,certs[i],CKA_VALUE,der,&size) || !size || size > sizeof(der)) { sc_mem_clear(der,sizeof(der)); goto done; }
		const unsigned char *cursor = der; X509 *cert = d2i_X509(NULL,&cursor,(long)size);
		keys[i] = cert ? X509_get_pubkey(cert) : NULL;
		int valid = keys[i] && cursor == der+size && EVP_PKEY_is_a(keys[i],"RSA") && EVP_PKEY_get_bits(keys[i]) == 2048;
		X509_free(cert); sc_mem_clear(der,sizeof(der)); if (!valid) goto done;
	}
	pub_guard.stage = "signature_pin_input";
	size_t pin_size; if (pub_guard.blocked || pub_interrupted || sign_pin_read(pin,&pin_size)) goto done;
	sign_guard.pin_input_ok = sign_guard.active = 1; sign_guard.login_calls++;
	pub_guard.stage = "signature_login"; rv = api->C_Login(session,CKU_USER,pin,(CK_ULONG)pin_size);
	sc_mem_clear(pin,sizeof(pin)); printf("signature_login_result=%lu\n",rv);
	if (rv || pub_guard.blocked || pub_interrupted) goto done;
	logged = 1; pub_guard.stage = "signature_key";
	CK_ULONG n; if (pub_find(api,session,CKO_PRIVATE_KEY,privs,&n) || n != 1 || pub_guard.blocked) goto done;
	unsigned char id[64]; CK_ULONG size = sizeof(id); if (pub_attribute(api,session,privs[0],CKA_ID,id,&size) || !size || size > sizeof(id)) goto done;
	EVP_PKEY *key = NULL; unsigned int matches = 0;
	for (CK_ULONG i = 0; i < count; i++) if (size == sizes[i] && !memcmp(id,ids[i],size)) { key = keys[i]; matches++; }
	sc_mem_clear(id,sizeof(id)); if (matches != 1) goto done;
	CK_MECHANISM mechanism = {CKM_SHA256_RSA_PKCS,NULL,0}; pub_guard.stage = "signature_init";
	rv = api->C_SignInit(session,&mechanism,privs[0]); printf("signature_init_result=%lu\n",rv);
	if (rv || pub_guard.blocked || pub_interrupted) goto done;
	size = sizeof(signature); pub_guard.stage = "signature_sign";
	rv = api->C_Sign(session,(CK_BYTE_PTR)sign_message,sizeof(sign_message)-1,signature,&size);
	printf("signature_sign_result=%lu\n",rv); if (rv || size != 256 || pub_guard.blocked || pub_interrupted) goto done;
	pub_guard.stage = "signature_verify";
	sign_guard.verified = sign_verify(key,signature,0) == 1;
	sign_guard.negative_verified = sign_verify(key,signature,1) == 0;
	if (!sign_guard.verified || !sign_guard.negative_verified) goto done;
	CK_SESSION_INFO info; rv = api->C_GetSessionInfo(session,&info); printf("signature_session_state_result=%lu\n",rv);
	if (rv || info.state != CKS_RO_PUBLIC_SESSION) goto done;
	failed = 0;
done:
	sc_mem_clear(pin,sizeof(pin)); sc_mem_clear(signature,sizeof(signature)); sc_mem_clear(ids,sizeof(ids));
	for (size_t i = 0; i < 8; i++) EVP_PKEY_free(keys[i]);
	/* Explicit cleanup only. No credential is retained or login retried. */
	if (logged) { rv = api->C_Logout(session); printf("signature_logout_result=%lu\n",rv); if (rv && rv != CKR_USER_NOT_LOGGED_IN) failed = 1; }
	rv = api->C_CloseSession(session); printf("signature_session_close_result=%lu\n",rv); return failed || rv || pub_guard.blocked;
}
static int sign_report(void)
{
	printf("signature_pin_input=%s\nsignature_login_calls=%u\nauth_backend_calls=%u\ncredential_backend_calls=%u\npso_backend_calls=%u\ninitial_reset_attempts=%u\ninitial_reset_result=%d\nfinal_reset_attempts=%u\nfinal_reset_result=%d\nsignature_verification=%s\nchanged_message_verification=%s\n",
		sign_guard.pin_input_ok ? "accepted_not_recorded" : "not_accepted",sign_guard.login_calls,sign_guard.step,
		sign_guard.credential_calls,sign_guard.pso_calls,sign_guard.initial_resets,sign_guard.initial_reset_result,
		sign_guard.final_resets,sign_guard.final_reset_result,sign_guard.verified ? "valid" : "not_confirmed",
		sign_guard.negative_verified ? "rejected" : "not_confirmed");
	return !sign_guard.verified || !sign_guard.negative_verified || sign_guard.step != 12 || sign_guard.login_calls != 1 ||
		sign_guard.credential_calls != 1 || sign_guard.pso_calls != 1 || sign_guard.initial_resets != 1 || sign_guard.final_resets != 1 ||
		sign_guard.initial_reset_result || sign_guard.final_reset_result;
}
