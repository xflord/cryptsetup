#!/bin/bash

. lib.sh

#
# *** Description ***
#
# generate primary header with luks2 keyslot kdf object
# having left over params.
#
# secondary header is corrupted on purpose as well
#

# $1 full target dir
# $2 full source luks2 image

function generate()
{
	# add keyslot 1 to second digest
	obj_len=$(jq -c -M '.keyslots."2".kdf | length' $TMPDIR/json0)
	json_str=$(jq -r -c -M '.keyslots."2".kdf.type = "argon2i" | .keyslots."2".kdf.iterations = 1001 | .keyslots."2".kdf.hash = "sha256"' $TMPDIR/json0)
	test ${#json_str} -lt $((LUKS2_JSON_SIZE*512)) || exit 2

	write_luks2_json "$json_str" $TMPDIR/json0

	lib_mangle_json_hdr0_kill_hdr1
}

function check()
{
	lib_hdr1_killed || exit 2

	read_luks2_json0 $TGT_IMG $TMPDIR/json_res0
	chks_res0=$(read_sha256_checksum $TGT_IMG)
	test "$chks0" = "$chks_res0" || exit 2
	new_obj_len=$(jq -c -M '.keyslots."2".kdf | length' $TMPDIR/json_res0)
	test $((obj_len+2)) -eq $new_obj_len || exit 2
}

lib_prepare $@
generate
check
lib_cleanup
