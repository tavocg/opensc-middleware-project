/* Compile-time unavailable backend: no OpenSSL3 implementation executed. */
#define IASECC_CHIPDOC_NO_CRYPTO
#include "libopensc/iasecc-chipdoc-sm.c"
#include <assert.h>
#include <stdio.h>
int main(void)
{
	struct sc_card card = {0}; struct iasecc_private_data prv = {0};
	struct iasecc_chipdoc_channel_report report, empty = {0};
	card.drv_data = &prv; prv.chipdoc_profile = IASECC_CHIPDOC_CR_RSA2048;
	prv.chipdoc_session.profile = prv.chipdoc_profile; prv.chipdoc_session.phase = IASECC_CHIPDOC_PREPARED;
	struct iasecc_private_data before = prv; card.caps = SC_CARD_CAP_APDU_EXT;
	memset(&report, 0xA5, sizeof(report));
	assert(iasecc_chipdoc_channel_open(&card, NULL, 137, &report) == SC_ERROR_NOT_SUPPORTED);
	assert(!memcmp(&report, &empty, sizeof(report)) && !memcmp(&prv, &before, sizeof(prv)));
	memset(&report, 0xA5, sizeof(report));
	assert(iasecc_chipdoc_channel_select_mf(&card, IASECC_CHIPDOC_MF_EXTENDED, &report) == SC_ERROR_NOT_SUPPORTED);
	assert(!memcmp(&report, &empty, sizeof(report)) && !memcmp(&prv, &before, sizeof(prv)) && card.caps == SC_CARD_CAP_APDU_EXT);
	struct iasecc_chipdoc_bootstrap_report boot, empty_boot = {0};
	memset(&boot, 0xA5, sizeof(boot));
	assert(iasecc_chipdoc_channel_bootstrap(NULL, NULL, &boot) == SC_ERROR_NOT_SUPPORTED);
	assert(!memcmp(&boot, &empty_boot, sizeof(boot)) && !memcmp(&prv, &before, sizeof(prv)));
	struct iasecc_chipdoc_transaction_report transaction, empty_transaction = {0};
	memset(&transaction, 0xA5, sizeof(transaction));
	assert(iasecc_chipdoc_transaction_begin(NULL, NULL, &transaction) == SC_ERROR_NOT_SUPPORTED);
	assert(!memcmp(&transaction, &empty_transaction, sizeof(transaction)) && !memcmp(&prv, &before, sizeof(prv)));
	int tries = 123; memset(&report, 0xA5, sizeof(report));
	assert(iasecc_chipdoc_channel_query_pin(&card, 1, &tries, &report) == SC_ERROR_NOT_SUPPORTED);
	assert(tries == -1 && !memcmp(&report, &empty, sizeof(report)) && !memcmp(&prv, &before, sizeof(prv)));
	tries = 123; memset(&report, 0xA5, sizeof(report));
	assert(iasecc_chipdoc_channel_verify_pin(&card, NULL, &tries, &report) == SC_ERROR_NOT_SUPPORTED);
	assert(tries == -1 && !memcmp(&report, &empty, sizeof(report)) && !memcmp(&prv, &before, sizeof(prv)));
	assert(iasecc_chipdoc_channel_open(NULL, NULL, 0, NULL) == SC_ERROR_NOT_SUPPORTED);
	puts("PASS unavailable crypto backend returns not-supported with zero reports and unchanged driver/card/session state, no crypto/transport/PIN");
	return 0;
}
