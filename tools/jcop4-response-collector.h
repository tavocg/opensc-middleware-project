/* Candidate physical response reassembly, not secure-messaging validation.
 * Offline only until a bounded real adapter authorizes each GET RESPONSE.
 * The owner must retain its connection/lock and pending SM response. */
#ifndef JCOP4_RESPONSE_COLLECTOR_H
#define JCOP4_RESPONSE_COLLECTOR_H
#include <stddef.h>
#include <string.h>
#include <openssl/crypto.h>

#define JCOP4_RESPONSE_FRAME_LIMIT 421
#define JCOP4_RESPONSE_GET_LIMIT 3
enum jcop4_response_cla { JCOP4_RESPONSE_CLA_ZERO, JCOP4_RESPONSE_CLA_PROTECTED };
enum jcop4_response_result {
	JCOP4_RESPONSE_ARGUMENT, JCOP4_RESPONSE_SIZE, JCOP4_RESPONSE_CAPACITY,
	JCOP4_RESPONSE_STATUS, JCOP4_RESPONSE_NO_PROGRESS, JCOP4_RESPONSE_BUDGET,
	JCOP4_RESPONSE_TRANSPORT, JCOP4_RESPONSE_COMPLETE_UNAUTHENTICATED
};
struct jcop4_response_report {
	enum jcop4_response_result result;
	unsigned int get_count, physical_sw;
	int transport;
};
typedef int (*jcop4_response_exchange)(void *, const unsigned char *, size_t,
	unsigned char *, size_t, size_t *);

/* Initial response already transmitted by the owner; do not replay it.
 * Limit is an explicit candidate protected-frame bound, only293 or421.
 * CLA choice has no default or fallback. Only61xx authorizes a five-byte
 * C0/00/00/Le continuation. No6C retry, sealing/MAC/SSC changes or other APDU.
 * Success returns opaque UNAUTHENTICATED bytes: caller MUST verify MAC and
 * semantic status before exposing plaintext or a signature. Error leaves
 * output untouched and written0; owner invalidates SM and closes the card. */
static int jcop4_response_collect(const unsigned char *first, size_t first_size,
		enum jcop4_response_cla policy, size_t frame_limit, jcop4_response_exchange exchange, void *context,
		unsigned char *output, size_t capacity, size_t *written, struct jcop4_response_report *report)
{
	unsigned char assembled[JCOP4_RESPONSE_FRAME_LIMIT] = {0}, fragment[258] = {0}, wire[5] = {0,0xC0,0,0,0};
	const unsigned char *part = first; size_t part_size = first_size, total = 0, requested = 256;
	struct jcop4_response_report local = {JCOP4_RESPONSE_ARGUMENT,0,0,0}; int result = 1;
	if (written) *written = 0;
	if (!first || !output || !written || !report || !exchange ||
		(policy != JCOP4_RESPONSE_CLA_ZERO && policy != JCOP4_RESPONSE_CLA_PROTECTED) ||
		(frame_limit != 293 && frame_limit != 421) || capacity < frame_limit) goto out;
	wire[0] = policy == JCOP4_RESPONSE_CLA_ZERO ? 0 : 0x0C;
	for (;;) {
		local.result = JCOP4_RESPONSE_SIZE;
		if (part_size < 2 || part_size > 258 || part_size - 2 > requested) goto out;
		size_t data_size = part_size - 2;
		local.physical_sw = (unsigned int)part[part_size - 2] * 256 + part[part_size - 1];
		local.result = JCOP4_RESPONSE_CAPACITY;
		if (data_size > frame_limit - 2 - total) goto out;
		if (data_size) memcpy(assembled + total, part, data_size);
		total += data_size;
		local.result = JCOP4_RESPONSE_STATUS;
		if (local.physical_sw == 0x9000) {
			local.result = JCOP4_RESPONSE_SIZE;
			if (total < 14) goto out;
			assembled[total++] = 0x90; assembled[total++] = 0;
			memcpy(output, assembled, total); *written = total;
			local.result = JCOP4_RESPONSE_COMPLETE_UNAUTHENTICATED; result = 0; break;
		}
		if ((local.physical_sw & 0xFF00) != 0x6100) goto out;
		local.result = JCOP4_RESPONSE_NO_PROGRESS;
		if (local.get_count && !data_size) goto out;
		local.result = JCOP4_RESPONSE_BUDGET;
		if (local.get_count == JCOP4_RESPONSE_GET_LIMIT) goto out;
		requested = local.physical_sw & 255; if (!requested) requested = 256;
		local.result = JCOP4_RESPONSE_CAPACITY;
		if (requested > frame_limit - 2 - total) goto out;
		wire[4] = (unsigned char)requested;
		memset(fragment, 0, sizeof(fragment)); part_size = 0; local.get_count++;
		local.transport = exchange(context, wire, sizeof(wire), fragment, sizeof(fragment), &part_size);
		local.result = JCOP4_RESPONSE_TRANSPORT;
		if (local.transport) goto out;
		part = fragment;
	}
out:
	if (report) *report = local;
	OPENSSL_cleanse(assembled, sizeof(assembled)); OPENSSL_cleanse(fragment, sizeof(fragment));
	OPENSSL_cleanse(wire, sizeof(wire)); return result;
}
#endif
