/*
 * LUKS - Linux Unified Key Setup v2, internal segment handling
 *
 * Copyright (C) 2018-2019, Red Hat, Inc. All rights reserved.
 * Copyright (C) 2018-2019, Ondrej Kozina
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "luks2_internal.h"

json_object *json_get_segments_jobj(json_object *hdr_jobj)
{
	json_object *jobj_segments;

	if (!hdr_jobj || !json_object_object_get_ex(hdr_jobj, "segments", &jobj_segments))
		return NULL;

	return jobj_segments;
}

/* use only on already validated 'segments' object */
uint64_t json_segments_get_minimal_offset(json_object *jobj_segments, unsigned blockwise)
{
	uint64_t tmp, min = blockwise ? UINT64_MAX >> SECTOR_SHIFT : UINT64_MAX;

	if (!jobj_segments)
		return 0;

	json_object_object_foreach(jobj_segments, key, val) {
		UNUSED(key);

		tmp = json_segment_get_offset(val, blockwise);

		if (!tmp)
			return tmp;

		if (tmp < min)
			min = tmp;
	}

	return min;
}

uint64_t json_segment_get_offset(json_object *jobj_segment, unsigned blockwise)
{
	json_object *jobj;

	if (!jobj_segment ||
	    !json_object_object_get_ex(jobj_segment, "offset", &jobj))
		return 0;

	return blockwise ? json_object_get_uint64(jobj) >> SECTOR_SHIFT : json_object_get_uint64(jobj);
}

const char *json_segment_type(json_object *jobj_segment)
{
	json_object *jobj;

	if (!jobj_segment ||
	    !json_object_object_get_ex(jobj_segment, "type", &jobj))
		return NULL;

	return json_object_get_string(jobj);
}

uint64_t json_segment_get_iv_offset(json_object *jobj_segment)
{
	json_object *jobj;

	if (!jobj_segment ||
	    !json_object_object_get_ex(jobj_segment, "iv_tweak", &jobj))
		return 0;

	return json_object_get_uint64(jobj);
}

uint64_t json_segment_get_size(json_object *jobj_segment, unsigned blockwise)
{
	json_object *jobj;

	if (!jobj_segment ||
	    !json_object_object_get_ex(jobj_segment, "size", &jobj))
		return 0;

	return blockwise ? json_object_get_uint64(jobj) >> SECTOR_SHIFT : json_object_get_uint64(jobj);
}

const char *json_segment_get_cipher(json_object *jobj_segment)
{
	json_object *jobj;

	/* FIXME: Pseudo "null" cipher should be handled elsewhere */
	if (!jobj_segment ||
	    !json_object_object_get_ex(jobj_segment, "encryption", &jobj))
		return "null";

	return json_object_get_string(jobj);
}

int json_segment_get_sector_size(json_object *jobj_segment)
{
	json_object *jobj;

	if (!jobj_segment ||
            !json_object_object_get_ex(jobj_segment, "sector_size", &jobj))
		return -1;

	return json_object_get_int(jobj);
}

json_object *json_segment_get_flags(json_object *jobj_segment)
{
	json_object *jobj;

	if (!jobj_segment || !(json_object_object_get_ex(jobj_segment, "flags", &jobj)))
		return NULL;
	return jobj;
}

json_object *json_segments_get_segment(json_object *jobj_segments, int segment)
{
	json_object *jobj;
	char segment_name[16];

	if (snprintf(segment_name, sizeof(segment_name), "%u", segment) < 1)
		return NULL;

	if (!json_object_object_get_ex(jobj_segments, segment_name, &jobj))
		return NULL;

	return jobj;
}

int json_segments_count(json_object *jobj_segments)
{
	if (!jobj_segments)
		return -EINVAL;

	return json_object_object_length(jobj_segments);
}

static void _get_segment_or_id_by_flag(json_object *jobj_segments, const char *flag, unsigned id, void *retval)
{
	json_object *jobj_flags, **jobj_ret = (json_object **)retval;
	int *ret = (int *)retval;

	if (!flag)
		return;

	json_object_object_foreach(jobj_segments, key, value) {
		if (!json_object_object_get_ex(value, "flags", &jobj_flags))
			continue;
		if (LUKS2_array_jobj(jobj_flags, flag)) {
			if (id)
				*ret = atoi(key);
			else
				*jobj_ret = value;
			return;
		}
	}
}

json_object *json_segments_get_segment_by_flag(json_object *jobj_segments, const char *flag)
{
	json_object *jobj_segment = NULL;

	if (jobj_segments)
		_get_segment_or_id_by_flag(jobj_segments, flag, 0, &jobj_segment);

	return jobj_segment;
}

void json_segment_remove_flag(json_object *jobj_segment, const char *flag)
{
	json_object *jobj_flags, *jobj_flags_new;

	if (!jobj_segment)
		return;

	jobj_flags = json_segment_get_flags(jobj_segment);
	if (!jobj_flags)
		return;

	jobj_flags_new = LUKS2_array_remove(jobj_flags, flag);
	if (!jobj_flags_new)
		return;

	if (json_object_array_length(jobj_flags_new) <= 0) {
		json_object_put(jobj_flags_new);
		json_object_object_del(jobj_segment, "flags");
	} else
		json_object_object_add(jobj_segment, "flags", jobj_flags_new);
}

static json_object *_segment_create_generic(const char *type, uint64_t offset, const uint64_t *length)
{
	json_object *jobj = json_object_new_object();
	if (!jobj)
		return NULL;

	json_object_object_add(jobj, "type",		json_object_new_string(type));
	json_object_object_add(jobj, "offset",		json_object_new_uint64(offset));
	json_object_object_add(jobj, "size",		length ? json_object_new_uint64(*length) : json_object_new_string("dynamic"));

	return jobj;
}

json_object *json_segment_create_linear(uint64_t offset, const uint64_t *length)
{
	return _segment_create_generic("linear", offset, length);
}

json_object *json_segment_create_crypt(uint64_t offset,
				  uint64_t iv_offset, const uint64_t *length,
				  const char *cipher, uint32_t sector_size)
{
	json_object *jobj = _segment_create_generic("crypt", offset, length);
	if (!jobj)
		return NULL;

	json_object_object_add(jobj, "iv_tweak",	json_object_new_uint64(iv_offset));
	json_object_object_add(jobj, "encryption",	json_object_new_string(cipher));
	json_object_object_add(jobj, "sector_size",	json_object_new_int(sector_size));

	return jobj;
}

int LUKS2_segment_ignore(json_object *jobj_segment)
{
	int i;
	json_object *jobj_flags, *jobj;

	if (!jobj_segment)
		return 0;

	json_object_object_get_ex(jobj_segment, "flags", &jobj_flags);

	if (!jobj_flags)
		return 0;

	for (i = 0; i < (int)json_object_array_length(jobj_flags); i++) {
		jobj = json_object_array_get_idx(jobj_flags, i);
		if (!strncmp(json_object_get_string(jobj), "reencrypt-", 10))
			return 1;
	}

	return 0;
}

uint64_t LUKS2_segment_offset(struct luks2_hdr *hdr, int segment, unsigned blockwise)
{
	return json_segment_get_offset(LUKS2_get_segment_jobj(hdr, segment), blockwise);
}

int json_segments_segment_in_reencrypt(json_object *jobj_segments)
{
	json_object *jobj_flags;

	json_object_object_foreach(jobj_segments, slot, val) {
		if (!json_object_object_get_ex(val, "flags", &jobj_flags) ||
		    !LUKS2_array_jobj(jobj_flags, "in-reencryption"))
			continue;

		return atoi(slot);
	}

	return -1;
}

uint64_t LUKS2_segment_size(struct luks2_hdr *hdr, int segment, unsigned blockwise)
{
	return json_segment_get_size(LUKS2_get_segment_jobj(hdr, segment), blockwise);
}

int LUKS2_segment_is_type(struct luks2_hdr *hdr, int segment, const char *type)
{
	return !strcmp(json_segment_type(LUKS2_get_segment_jobj(hdr, segment)) ?: "", type);
}

int LUKS2_last_segment_by_type(struct luks2_hdr *hdr, const char *type)
{
	json_object *jobj_segments;
	int last_found = -1;

	if (!type)
		return -1;

	if (!json_object_object_get_ex(hdr->jobj, "segments", &jobj_segments))
		return -1;

	json_object_object_foreach(jobj_segments, slot, val) {
		if (LUKS2_segment_ignore(val))
			continue;
		if (strcmp(type, json_segment_type(val) ?: ""))
			continue;

		if (atoi(slot) > last_found)
			last_found = atoi(slot);
	}

	return last_found;
}

int LUKS2_segment_by_type(struct luks2_hdr *hdr, const char *type)
{
	json_object *jobj_segments;
	int first_found = -1;

	if (!type)
		return -EINVAL;

	if (!json_object_object_get_ex(hdr->jobj, "segments", &jobj_segments))
		return -EINVAL;

	json_object_object_foreach(jobj_segments, slot, val) {
		if (LUKS2_segment_ignore(val))
			continue;
		if (strcmp(type, json_segment_type(val) ?: ""))
			continue;

		if (first_found < 0)
			first_found = atoi(slot);
		else if (atoi(slot) < first_found)
			first_found = atoi(slot);
	}

	return first_found;
}

int LUKS2_segment_first_unused_id(struct luks2_hdr *hdr)
{
	json_object *jobj_segments;
	int id, last_id = -1;

	if (!json_object_object_get_ex(hdr->jobj, "segments", &jobj_segments))
		return -EINVAL;

	json_object_object_foreach(jobj_segments, slot, val) {
		UNUSED(val);
		id = atoi(slot);
		if (id > last_id)
			last_id = id;
	}

	return last_id + 1;
}

int LUKS2_segment_set_flag(json_object *jobj_segment, const char *flag)
{
	json_object *jobj_flags;

	if (!jobj_segment || !flag)
		return -EINVAL;

	if (!json_object_object_get_ex(jobj_segment, "flags", &jobj_flags)) {
		jobj_flags = json_object_new_array();
		if (!jobj_flags)
			return -ENOMEM;
		json_object_object_add(jobj_segment, "flags", jobj_flags);
	}

	if (LUKS2_array_jobj(jobj_flags, flag))
		return 0;

	json_object_array_add(jobj_flags, json_object_new_string(flag));

	return 0;
}

int LUKS2_segments_set(struct crypt_device *cd, struct luks2_hdr *hdr,
		       json_object *jobj_segments, int commit)
{
	json_object_object_add(hdr->jobj, "segments", jobj_segments);

	return commit ? LUKS2_hdr_write(cd, hdr) : 0;
}

const char *LUKS2_debug_dump_segments(struct luks2_hdr *hdr)
{
	return json_object_to_json_string_ext(LUKS2_get_segments_jobj(hdr), JSON_C_TO_STRING_PRETTY);
}

int LUKS2_get_segment_id_by_flag(struct luks2_hdr *hdr, const char *flag)
{
	int ret = -ENOENT;
	json_object *jobj_segments = LUKS2_get_segments_jobj(hdr);

	if (jobj_segments)
		_get_segment_or_id_by_flag(jobj_segments, flag, 1, &ret);

	return ret;
}

json_object *LUKS2_get_segment_by_flag(struct luks2_hdr *hdr, const char *flag)
{
	json_object *jobj_segment = NULL,
		    *jobj_segments = LUKS2_get_segments_jobj(hdr);

	if (jobj_segments)
		_get_segment_or_id_by_flag(jobj_segments, flag, 0, &jobj_segment);

	return jobj_segment;
}

json_object *LUKS2_get_ignored_segments(struct luks2_hdr *hdr)
{
	json_object *jobj_segments, *jobj = json_object_new_object();
	int i = 0;

	if (!jobj || !json_object_object_get_ex(hdr->jobj, "segments", &jobj_segments))
		return NULL;

	json_object_object_foreach(jobj_segments, key, value) {
		UNUSED(key);
		if (LUKS2_segment_ignore(value))
			json_object_object_add_by_uint(jobj, i++, json_object_get(value));
	}

	return jobj;
}
