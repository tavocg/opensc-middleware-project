/* Regression checks against the actual IAS/ECC FCI parser, without a card.
 * The EF.DIR fixture contains only application-independent file metadata.
 */
#include <stdio.h>
#include <string.h>

#include "libopensc/card-iasecc.c"
#include "libopensc/dir.c"

static int failures;

static void check_observed_dir(void)
{
	struct sc_context ctx = {0};
	struct sc_card card = {0};
	unsigned char data[256] = {
		0x61, 0x3E, 0x4F, 0x0E, 0xE8, 0x28, 0xBD, 0x08, 0x0F, 0xD2,
		0x50, 0x47, 0x65, 0x6E, 0x65, 0x72, 0x69, 0x63, 0x50, 0x20,
		0x43, 0x68, 0x69, 0x70, 0x44, 0x6F, 0x63,
		0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		0x30, 0x04, 0x04, 0x02, 0x50, 0x31, 0xA0, 0x04, 0x04, 0x02, 0x50, 0x32
	};
	unsigned char *cursor = data;
	size_t remaining = sizeof(data);
	const unsigned char aid[] = {0xE8, 0x28, 0xBD, 0x08, 0x0F, 0xD2, 0x50, 0x47, 0x65, 0x6E, 0x65, 0x72, 0x69, 0x63};
	int result;

	ctx.app_name = "iasecc-fci-regression";
	card.ctx = &ctx;
	result = parse_dir_record(&card, &cursor, &remaining, -1);
	if (result != SC_SUCCESS || card.app_count != 1 || card.app[0]->aid.len != sizeof(aid) ||
			memcmp(card.app[0]->aid.value, aid, sizeof(aid)) || !card.app[0]->label ||
			strncmp(card.app[0]->label, "ChipDoc", 7) || card.app[0]->path.type != SC_PATH_TYPE_DF_NAME) {
		fprintf(stderr, "FAIL captured EF.DIR application record: result=%d, apps=%d\n", result, card.app_count);
		failures++;
	} else {
		puts("PASS captured EF.DIR application record (ECC Generic PKI / ChipDoc)");
	}
	sc_free_apps(&card);
}

static void check_acl(const char *name, const unsigned char *acl, size_t len,
		int expected_result, unsigned operation, unsigned method, unsigned reference)
{
	struct sc_context ctx = {0};
	struct sc_card card = {0};
	struct sc_file *file = sc_file_new();
	const struct sc_acl_entry *entry;
	unsigned char fcp[64] = {
		0x62, 0, 0x80, 0x02, 0x01, 0x00, 0x82, 0x01, 0x01,
		0x83, 0x02, 0x2F, 0x00, 0xA1, 0, 0x8C, 0
	};
	int result;

	if (!file || len > sizeof(fcp) - 17) {
		fprintf(stderr, "FAIL %s: invalid fixture or allocation\n", name);
		failures++;
		sc_file_free(file);
		return;
	}
	ctx.app_name = "iasecc-fci-regression";
	card.ctx = &ctx;
	fcp[1] = 15 + len;
	fcp[14] = 2 + len;
	fcp[16] = len;
	memcpy(fcp + 17, acl, len);
	result = iasecc_process_fci(&card, file, fcp, 17 + len);
	entry = sc_file_get_acl_entry(file, operation);
	if (result != expected_result || (expected_result == SC_SUCCESS &&
			(!entry || entry->method != method ||
			(method != SC_AC_NONE && method != SC_AC_NEVER && entry->key_ref != reference)))) {
		fprintf(stderr, "FAIL %s: result=%d, ACL %s\n", name, result, entry ? "present" : "missing");
		failures++;
	} else {
		printf("PASS %s\n", name);
	}
	sc_file_free(file);
}

static void check_observed_fcp(void)
{
	struct sc_context ctx = {0};
	struct sc_card card = {0};
	struct sc_file *file = sc_file_new();
	const struct sc_acl_entry *read, *remove;
	const unsigned char fcp[] = {
		0x62, 0x1F, 0x80, 0x02, 0x01, 0x00, 0x82, 0x01, 0x01,
		0x83, 0x02, 0x2F, 0x00, 0x88, 0x01, 0xF0, 0x8A, 0x01,
		0x05, 0xA1, 0x0C, 0x8C, 0x04, 0x40, 0x00, 0x01, 0x00,
		0x9C, 0x04, 0x40, 0x00, 0x01, 0x00
	};
	int result;

	if (!file) {
		failures++;
		return;
	}
	ctx.app_name = "iasecc-fci-regression";
	card.ctx = &ctx;
	result = iasecc_process_fci(&card, file, fcp, sizeof(fcp));
	read = sc_file_get_acl_entry(file, SC_AC_OP_READ);
	remove = sc_file_get_acl_entry(file, SC_AC_OP_DELETE);
	if (result != SC_SUCCESS || file->size != 256 || !read || read->method != SC_AC_NONE ||
			!remove || remove->method != SC_AC_NONE) {
		fprintf(stderr, "FAIL captured EF.DIR: result=%d, size=%zu, READ ACL %s\n",
			result, file->size, read ? "present" : "missing");
		failures++;
	} else {
		puts("PASS captured EF.DIR FCP");
	}
	sc_file_free(file);
}

int main(void)
{
	const unsigned char single[] = {0x01, 0x00};
	const unsigned char split[] = {0x40, 0x00, 0x01, 0x00};
	const unsigned char reverse[] = {0x01, 0x00, 0x40, 0x00};
	const unsigned char protected[] = {0x40, 0x00, 0x03, 0x11, 0x11};
	const unsigned char repeated[] = {0x01, 0x11, 0x01, 0x11};
	const unsigned char alternatives[] = {0x01, 0x11, 0x01, 0x12};
	const unsigned char truncated[] = {0x03, 0x00};
	const unsigned char denied[] = {0x01, 0xFF};
	const unsigned char ignored[] = {0x20, 0x11, 0x20, 0x12, 0x01, 0};
	const unsigned char ignored_truncated[] = {0x01, 0, 0x20};
	const unsigned char split_denied[] = {0x40, 0, 0x01, 0xFF};

	iso_ops = sc_get_iso7816_driver()->ops;
	check_acl("single READ", single, sizeof(single), SC_SUCCESS, SC_AC_OP_READ, SC_AC_NONE, 0);
	check_observed_fcp();
	check_acl("split READ", split, sizeof(split), SC_SUCCESS, SC_AC_OP_READ, SC_AC_NONE, 0);
	check_acl("reversed groups", reverse, sizeof(reverse), SC_SUCCESS, SC_AC_OP_DELETE, SC_AC_NONE, 0);
	check_acl("protected READ", protected, sizeof(protected), SC_SUCCESS, SC_AC_OP_READ, SC_AC_SEN, 1);
	check_acl("protected UPDATE", protected, sizeof(protected), SC_SUCCESS, SC_AC_OP_UPDATE, SC_AC_SEN, 1);
	check_acl("identical READ rule", repeated, sizeof(repeated), SC_SUCCESS, SC_AC_OP_READ, SC_AC_SEN, 1);
	check_acl("unsupported alternatives", alternatives, sizeof(alternatives), SC_ERROR_NOT_SUPPORTED, SC_AC_OP_READ, 0, 0);
	check_acl("truncated group", truncated, sizeof(truncated), SC_ERROR_INVALID_DATA, SC_AC_OP_READ, 0, 0);
	check_acl("denied READ", denied, sizeof(denied), SC_SUCCESS, SC_AC_OP_READ, SC_AC_NEVER, 0);
	check_acl("ignored operation alternatives preserve READ", ignored, sizeof(ignored), SC_SUCCESS, SC_AC_OP_READ, SC_AC_NONE, 0);
	check_acl("truncated ignored operation is rejected", ignored_truncated, sizeof(ignored_truncated), SC_ERROR_INVALID_DATA, SC_AC_OP_READ, 0, 0);
	check_acl("later denied READ is preserved", split_denied, sizeof(split_denied), SC_SUCCESS, SC_AC_OP_READ, SC_AC_NEVER, 0);
	check_observed_dir();
	return failures ? 1 : 0;
}
