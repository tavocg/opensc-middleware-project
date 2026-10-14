/* Exercise only the terminal-input function; the card entrypoint is unused. */
#define main unused_card_entrypoint
#include "../tools/jcop4-channel-login.c"
#undef main
int main(void)
{
	struct sigaction action; memset(&action, 0, sizeof(action)); action.sa_handler = probe_signal;
	sigemptyset(&action.sa_mask);
	if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL)) return 2;
	unsigned char pin[14] = {0}; size_t size = 0;
	int rv = jcop4_login_read_pin(pin, &size);
	if (!rv && (size != 8 || memcmp(pin, "77777777", 8))) return 3; /* Synthetic test value only. */
	if (rv) for (size_t i = 0; i < sizeof(pin); i++) if (pin[i]) return 4;
	OPENSSL_cleanse(pin, sizeof(pin)); printf("input_result=%d\n", rv); return rv;
}
