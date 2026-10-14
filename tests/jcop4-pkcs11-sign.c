/* Real diagnostic/module/crypto; only reader/context and a PTY are software. */
#define JCOP4_PKCS11_PUBLIC_TEST_FIXTURE
#define JCOP4_PKCS11_SIGN_DIAGNOSTIC
#define JCOP4_PKCS11_TTY_FIXTURE
#include "jcop4-pkcs11-public.c"
#include <poll.h>
static int tty_slave = -1;
static int assertion_fd = -1, test_variant;
/* Assertions remain diagnosable while the production tool suppresses stderr. */
void __assert_fail(const char *expression,const char *file,unsigned int line,const char *function)
{
	dprintf(assertion_fd,"software fixture variant=%d assertion=%s file=%s:%u function=%s stage=%s auth_step=%u\n",
		test_variant,expression,file,line,function,pub_guard.stage ? pub_guard.stage : "none",sign_guard.step);
	abort();
}
static int sign_open_tty(void) { return tty_slave < 0 ? -1 : dup(tty_slave); }

static void signature_case(int variant)
{
	test_variant = variant;
	setup(); fresh_peer(); unsigned int n;
	assert(EVP_Digest(sign_message,sizeof(sign_message)-1,digest,&n,EVP_sha256(),NULL) && n == 32);
	if (variant == 1) query_sw = 0x63C6;
	if (variant == 2) verify_sw = verify_outer = 0x63C6;
	if (variant == 3) verify_mode = 2; /* Bare9000 is not authenticated. */
	if (variant == 4) { failure_stage = 3; failure_mode = 2; }
	if (variant == 5) { reset_fail_at = 2; expected_finish_failure = 1; }
	if (variant == 6) { query_mode = 3; }
	if (variant == 7) { reset_fail_at = 1; expected_finish_failure = 1; }
	if (variant == 13) expected_pin_size = 14;
	char output[8192]; int rv = public_run("1",output);
	if (!variant && rv) fprintf(stderr,"signature fixture report:\n%s",output);
	assert(!connected && !pub_guard.ctx && !reader_reserved && !test_card.lock_count);
	assert(!strstr(output,"PRIVATE_") && !strstr(output,"Synthetic"));
#ifdef JCOP4_PKCS11_RSA_PKCS
	assert(strstr(output,"signature_mechanism=CKM_RSA_PKCS\n") && strstr(output,"canonical_sha256_digestinfo"));
#else
	assert(strstr(output,"signature_mechanism=CKM_SHA256_RSA_PKCS\n") && strstr(output,"fixed_message_hashed_by_module"));
#endif
	if (!variant || variant == 13) {
		assert(!rv && physical == 12 && credential_attempts == 1 && signing_calls == 3 && resets == 2);
		assert(sign_guard.step == 12 && sign_guard.verified && sign_guard.negative_verified);
		assert(strstr(output,"validated_rsa2048_sha256_pkcs11_closed"));
	} else {
		assert(rv && strstr(output,"incomplete_stop_no_retry") && sign_guard.login_calls <= 1 && credential_attempts <= 1 && sign_guard.pso_calls <= 1);
		if (variant == 1 || variant == 6 || variant == 7 || variant >= 8) assert(!credential_attempts && !sign_guard.pso_calls);
		if (variant >= 8) assert(!physical && !resets && !sign_guard.login_calls);
		if (variant == 2 || variant == 3) assert(credential_attempts == 1 && !sign_guard.pso_calls && resets == 2);
		if (variant == 4 || variant == 5) assert(sign_guard.pso_calls == 1 && !sign_guard.verified && resets == 2);
		if (variant == 7) assert(resets == 1 && !physical);
	}
}
static void run_with_tty(int variant)
{
	test_variant = variant;
	int master = posix_openpt(O_RDWR|O_NOCTTY|O_CLOEXEC); assert(master >= 0);
	assert(!grantpt(master) && !unlockpt(master)); tty_slave = open(ptsname(master),O_RDWR|O_NOCTTY|O_CLOEXEC); assert(tty_slave >= 0);
	struct termios before, after; assert(!tcgetattr(tty_slave,&before)); before.c_lflag |= ECHO|ICANON; assert(!tcsetattr(tty_slave,TCSANOW,&before));
	fflush(NULL); pid_t child = fork(); assert(child >= 0);
	if (!child) { close(master); signature_case(variant); _exit(0); }
	char notice[1024] = {0}; size_t used = 0;
	while (!strstr(notice,"un solo intento): ")) {
		struct pollfd ready = {master,POLLIN,0}; assert(poll(&ready,1,10000) == 1);
		ssize_t got = read(master,notice+used,sizeof(notice)-used-1); assert(got > 0); used += got; assert(used < sizeof(notice)-1);
	}
	assert(!tcgetattr(tty_slave,&after) && !(after.c_lflag & (ECHO|ECHONL)));
	if (variant == 8) assert(write(master,"12345\n",6) == 6);
	else if (variant == 9) assert(write(master,"123456789012345678901234567890123\n",34) == 34);
	else if (variant == 10) { unsigned char bad[7] = {0x80,'2','3','4','5','6','\n'}; assert(write(master,bad,sizeof(bad)) == sizeof(bad)); }
	else if (variant == 11) assert(!kill(child,SIGINT));
	else if (variant == 12) assert(!kill(child,SIGTERM));
	else { unsigned char line[15]; size_t size = variant == 13 ? 14 : 6; memcpy(line,fictitious_pin,size); line[size++] = '\n'; assert(write(master,line,size) == (ssize_t)size); }
	int status; assert(waitpid(child,&status,0) == child && WIFEXITED(status) && !WEXITSTATUS(status));
	assert(!tcgetattr(tty_slave,&after) && before.c_lflag == after.c_lflag && before.c_iflag == after.c_iflag &&
		before.c_oflag == after.c_oflag && before.c_cflag == after.c_cflag && !memcmp(before.c_cc,after.c_cc,sizeof(before.c_cc)));
	struct pollfd pending = {tty_slave,POLLIN,0}; assert(poll(&pending,1,0) == 0);
	struct pollfd ready = {master,POLLIN,0}; if (poll(&ready,1,0) == 1) {
		char tail[128]; ssize_t got = read(master,tail,sizeof(tail)); assert(got > 0);
		for (ssize_t i = 0; i < got; i++) assert(tail[i] == '\r' || tail[i] == '\n');
	}
	close(tty_slave); close(master); tty_slave = -1;
}
int main(void)
{
	assertion_fd = dup(STDERR_FILENO); assert(assertion_fd >= 0);
	(void)thread_ops; (void)slot; (void)session; (void)second;
	test_provider.libctx = OSSL_LIB_CTX_new(); alternate_provider.libctx = OSSL_LIB_CTX_new(); assert(test_provider.libctx && alternate_provider.libctx);
	test_provider.defprov = OSSL_PROVIDER_load(test_provider.libctx,"default"); alternate_provider.defprov = OSSL_PROVIDER_load(alternate_provider.libctx,"default");
	assert(test_provider.defprov && alternate_provider.defprov); init(&peer); memcpy(initial_saved,peer.initial,137);
	EVP_PKEY_CTX *rsa = EVP_PKEY_CTX_new_from_name(test_provider.libctx,"RSA",NULL); assert(rsa);
	assert(EVP_PKEY_keygen_init(rsa) > 0 && EVP_PKEY_CTX_set_rsa_keygen_bits(rsa,2048) > 0 && EVP_PKEY_keygen(rsa,&rsa_key) > 0); EVP_PKEY_CTX_free(rsa);
	for (size_t i = 0; i < sizeof(fictitious_pin); i++) fictitious_pin[i] = 0x20+i;
	run_with_tty(0); run_with_tty(13); puts("PASS production diagnostic/local module/native binding/one login/one extended PSO verifies RSA and rejects changed message, accepts synthetic PIN at6/14, closes owned transaction/context, and never reports certificate/IDs/PIN; synthetic reader and actual PTY");
	for (int i = 1; i <= 7; i++) run_with_tty(i);
	puts("PASS authenticated unavailable PIN state, simulated incorrect PIN, bare9000, bad PSO MAC, final reset failure, query61 and initial reset failure stop with at most one credential/PSO and no retries");
	for (int i = 8; i <= 12; i++) run_with_tty(i);
	puts("PASS real canonical terminal input rejects short/long/nonASCII and handles INT/TERM before login, hides synthetic credential echo and restores complete terminal flags in success and failures");
	memset(&pub_guard,0,sizeof(pub_guard)); sign_initialize(); assert(sign_reset(NULL,0) < 0 && !sign_guard.initial_resets);
	puts("PASS signing-mode reset outside an explicit operation is blocked before backend");
	setup(); assert(!initialize());
	for (int variant = 0; variant < 12; variant++) {
		memset(&pub_guard,0,sizeof(pub_guard)); sign_initialize();
		pub_guard.card = &test_card; pub_guard.card_connected = pub_guard.connected = 1; pub_guard.reader = &test_reader;
		pub_guard.ctx = &test_ctx; pub_guard.original = &reader_ops;
		struct iasecc_private_data *prv = test_card.drv_data; test_card.lock_count = 1;
		prv->chipdoc_transaction = (struct iasecc_chipdoc_transaction){.card=&test_card,.reader=&test_reader,.context=&test_ctx,.active=1,.owned_lock=1};
		prv->chipdoc_session.phase = IASECC_CHIPDOC_PIN_AVAILABLE; prv->chipdoc_session.channel = (void *)1;
		sign_guard.active = 1; sign_guard.step = 8; sign_guard.initial_resets = 1;
		unsigned char body[29] = {0x87,0x11,1}, response[256]; body[19] = 0x8E; body[20] = 8;
		sc_apdu_t a = {0}; a.cse = SC_APDU_CASE_4_SHORT; a.cla = 0x0C; a.ins = 0x20; a.p2 = 1;
		a.lc = a.datalen = 29; a.data = body; a.le = a.resplen = 256; a.resp = response;
		switch (variant) {
		case 0: a.cla = 0; break; case 1: a.cse = SC_APDU_CASE_4_EXT; break; case 2: a.ins = 0x24; break;
		case 3: a.p2 = 2; break; case 4: sign_guard.step = 9; break; case 5: a.flags = SC_APDU_FLAGS_CHAINING; break;
		case 6: a.control = 1; break; case 7: a.datalen = 28; break; case 8: a.data = NULL; break;
		case 9: sign_guard.credential_calls = 1; break; case 10: sign_guard.step = 12; break;
		case 11: prv->chipdoc_transaction.owned_lock = 0; break;
		}
		unsigned int before = test_physical;
		assert(sign_transmit(&test_reader,&a) < 0 && pub_guard.blocked && test_physical == before);
		assert(sign_transmit(&test_reader,&a) < 0 && test_physical == before);
		prv->chipdoc_session.channel = NULL; memset(&prv->chipdoc_transaction,0,sizeof(prv->chipdoc_transaction));
	}
	test_card.lock_count = 0; iasecc_chipdoc_invalidate_card(&test_card); finish_initialized();
	puts("PASS auth guard rejects plaintext credential, wrong case/INS/reference/phase, chaining/control/length/missing body, repeated credential/extra command and lost ownership before backend; first rejection is permanent");
	free_files(); free_native(); scconf_free(test_ctx.conf); close(assertion_fd);
	EVP_PKEY_free(rsa_key); EVP_PKEY_free(peer.peer); jcop4_ca_pending_clear(&peer.derived);
	OSSL_PROVIDER_unload(test_provider.defprov); OSSL_LIB_CTX_free(test_provider.libctx);
	OSSL_PROVIDER_unload(alternate_provider.defprov); OSSL_LIB_CTX_free(alternate_provider.libctx); return 0;
}
