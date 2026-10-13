/* Read the launcher's actual config with OpenSC's parser and option loader.
 * No context initialization, reader connection, credentials or Java.
 */
#include "pkcs11/misc.c"

int main(int argc, char **argv)
{
	struct sc_context ctx = {0};
	struct sc_pkcs11_config options;
	scconf_block **apps;
	scconf_block *pkcs15;
	int valid = 0;

	if (argc != 3 || (strcmp(argv[2], "0") && strcmp(argv[2], "1")))
		return 2;
	ctx.conf = scconf_new(argv[1]);
	if (!ctx.conf)
		return 1;
	if (scconf_parse(ctx.conf) != 1)
		goto out;
	apps = scconf_find_blocks(ctx.conf, ctx.conf->root, "app", "default");
	if (!apps)
		goto out;
	ctx.conf_blocks[0] = apps[0];
	free(apps);
	if (!ctx.conf_blocks[0])
		goto out;
	load_pkcs11_parameters(&options, &ctx);
	pkcs15 = sc_get_conf_block(&ctx, "framework", "pkcs15", 1);
	valid = options.lock_login == (argv[2][0] == '1') && !options.atomic &&
		pkcs15 && !scconf_get_bool(pkcs15, "use_pin_caching", 1) &&
		!scconf_get_bool(pkcs15, "use_file_caching", 1) &&
		scconf_get_bool(pkcs15, "try_emulation_first", 0) &&
		scconf_get_int(ctx.conf_blocks[0], "debug", -1) == 0;
out:
	scconf_free(ctx.conf);
	return valid ? 0 : 1;
}
