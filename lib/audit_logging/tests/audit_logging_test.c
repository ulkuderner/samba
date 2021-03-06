/*
 * Unit tests for the audit_logging library.
 *
 *  Copyright (C) Andrew Bartlett <abartlet@samba.org> 2018
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * from cmocka.c:
 * These headers or their equivalents should be included prior to
 * including
 * this header file.
 *
 * #include <stdarg.h>
 * #include <stddef.h>
 * #include <setjmp.h>
 *
 * This allows test applications to use custom definitions of C standard
 * library functions and types.
 *
 */

/*
 * Note that the messaging routines (audit_message_send and get_event_server)
 * are not tested by these unit tests.  Currently they are for integration
 * test support, and as such are exercised by the integration tests.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>
#include <time.h>
#include <tevent.h>
#include <config.h>
#include <talloc.h>
#include "lib/util/talloc_stack.h"

#include "lib/util/data_blob.h"
#include "lib/util/time.h"
#include "libcli/util/werror.h"
#include "lib/param/loadparm.h"
#include "libcli/security/dom_sid.h"
#include "librpc/ndr/libndr.h"

#include "lib/audit_logging/audit_logging.h"

#ifdef HAVE_JANSSON
static void test_json_add_int(void **state)
{
	struct json_object object;
	struct json_t *value = NULL;
	double n;

	object = json_new_object();
	json_add_int(&object, "positive_one", 1);
	json_add_int(&object, "zero", 0);
	json_add_int(&object, "negative_one", -1);


	assert_int_equal(3, json_object_size(object.root));

	value = json_object_get(object.root, "positive_one");
	assert_true(json_is_integer(value));
	n = json_number_value(value);
	assert_true(n == 1.0);

	value = json_object_get(object.root, "zero");
	assert_true(json_is_integer(value));
	n = json_number_value(value);
	assert_true(n == 0.0);

	value = json_object_get(object.root, "negative_one");
	assert_true(json_is_integer(value));
	n = json_number_value(value);
	assert_true(n == -1.0);

	json_free(&object);
}

static void test_json_add_bool(void **state)
{
	struct json_object object;
	struct json_t *value = NULL;

	object = json_new_object();
	json_add_bool(&object, "true", true);
	json_add_bool(&object, "false", false);


	assert_int_equal(2, json_object_size(object.root));

	value = json_object_get(object.root, "true");
	assert_true(json_is_boolean(value));
	assert_true(value == json_true());

	value = json_object_get(object.root, "false");
	assert_true(json_is_boolean(value));
	assert_true(value == json_false());

	json_free(&object);
}

static void test_json_add_string(void **state)
{
	struct json_object object;
	struct json_t *value = NULL;
	const char *s = NULL;

	object = json_new_object();
	json_add_string(&object, "null", NULL);
	json_add_string(&object, "empty", "");
	json_add_string(&object, "name", "value");



	assert_int_equal(3, json_object_size(object.root));

	value = json_object_get(object.root, "null");
	assert_true(json_is_null(value));

	value = json_object_get(object.root, "empty");
	assert_true(json_is_string(value));
	s = json_string_value(value);
	assert_string_equal("", s);

	value = json_object_get(object.root, "name");
	assert_true(json_is_string(value));
	s = json_string_value(value);
	assert_string_equal("value", s);
	json_free(&object);
}

static void test_json_add_object(void **state)
{
	struct json_object object;
	struct json_object other;
	struct json_t *value = NULL;

	object = json_new_object();
	other  = json_new_object();
	json_add_object(&object, "null", NULL);
	json_add_object(&object, "other", &other);



	assert_int_equal(2, json_object_size(object.root));

	value = json_object_get(object.root, "null");
	assert_true(json_is_null(value));

	value = json_object_get(object.root, "other");
	assert_true(json_is_object(value));
	assert_ptr_equal(other.root, value);

	json_free(&object);
}

static void test_json_add_to_array(void **state)
{
	struct json_object array;
	struct json_object o1;
	struct json_object o2;
	struct json_object o3;
	struct json_t *value = NULL;

	array = json_new_array();
	assert_true(json_is_array(array.root));

	o1 = json_new_object();
	o2 = json_new_object();
	o3 = json_new_object();

	json_add_object(&array, NULL, &o3);
	json_add_object(&array, "", &o2);
	json_add_object(&array, "will-be-ignored", &o1);
	json_add_object(&array, NULL, NULL);

	assert_int_equal(4, json_array_size(array.root));

	value = json_array_get(array.root, 0);
	assert_ptr_equal(o3.root, value);

	value = json_array_get(array.root, 1);
	assert_ptr_equal(o2.root, value);

	value = json_array_get(array.root, 2);
	assert_ptr_equal(o1.root, value);

	value = json_array_get(array.root, 3);
	assert_true(json_is_null(value));

	json_free(&array);

}

static void test_json_add_timestamp(void **state)
{
	struct json_object object;
	struct json_t *ts = NULL;
	const char *t = NULL;
	int rc;
	int usec, tz;
	char c[2];
	struct tm tm;
	time_t before;
	time_t after;
	time_t actual;


	object = json_new_object();
	before = time(NULL);
	json_add_timestamp(&object);
	after = time(NULL);

	ts = json_object_get(object.root, "timestamp");
	assert_true(json_is_string(ts));

	/*
	 * Convert the returned ISO 8601 timestamp into a time_t
	 * Note for convenience we ignore the value of the microsecond
	 * part of the time stamp.
	 */
	t = json_string_value(ts);
	rc = sscanf(
		t,
		"%4d-%2d-%2dT%2d:%2d:%2d.%6d%1c%4d",
		&tm.tm_year,
		&tm.tm_mon,
		&tm.tm_mday,
		&tm.tm_hour,
		&tm.tm_min,
		&tm.tm_sec,
		&usec,
		c,
		&tz);
	assert_int_equal(9, rc);
	tm.tm_year = tm.tm_year - 1900;
	tm.tm_mon = tm.tm_mon - 1;
	tm.tm_isdst = -1;
	actual = mktime(&tm);

	/*
	 * The timestamp should be before <= actual <= after
	 */
	assert_true(difftime(actual, before) >= 0);
	assert_true(difftime(after, actual) >= 0);

	json_free(&object);
}

static void test_json_add_stringn(void **state)
{
	struct json_object object;
	struct json_t *value = NULL;
	const char *s = NULL;

	object = json_new_object();
	json_add_stringn(&object, "null", NULL, 10);
	json_add_stringn(&object, "null-zero-len", NULL, 0);
	json_add_stringn(&object, "empty", "", 1);
	json_add_stringn(&object, "empty-zero-len", "", 0);
	json_add_stringn(&object, "value-less-than-len", "123456", 7);
	json_add_stringn(&object, "value-greater-than-len", "abcd", 3);
	json_add_stringn(&object, "value-equal-len", "ZYX", 3);
	json_add_stringn(&object, "value-len-is-zero", "this will be null", 0);


	assert_int_equal(8, json_object_size(object.root));

	value = json_object_get(object.root, "null");
	assert_true(json_is_null(value));

	value = json_object_get(object.root, "null-zero-len");
	assert_true(json_is_null(value));

	value = json_object_get(object.root, "empty");
	assert_true(json_is_string(value));
	s = json_string_value(value);
	assert_string_equal("", s);

	value = json_object_get(object.root, "empty-zero-len");
	assert_true(json_is_null(value));

	value = json_object_get(object.root, "value-greater-than-len");
	assert_true(json_is_string(value));
	s = json_string_value(value);
	assert_string_equal("abc", s);
	assert_int_equal(3, strlen(s));

	value = json_object_get(object.root, "value-equal-len");
	assert_true(json_is_string(value));
	s = json_string_value(value);
	assert_string_equal("ZYX", s);
	assert_int_equal(3, strlen(s));

	value = json_object_get(object.root, "value-len-is-zero");
	assert_true(json_is_null(value));

	json_free(&object);
}

static void test_json_add_version(void **state)
{
	struct json_object object;
	struct json_t *version = NULL;
	struct json_t *v = NULL;
	double n;

	object = json_new_object();
	json_add_version(&object, 3, 1);

	assert_int_equal(1, json_object_size(object.root));

	version = json_object_get(object.root, "version");
	assert_true(json_is_object(version));
	assert_int_equal(2, json_object_size(version));

	v = json_object_get(version, "major");
	assert_true(json_is_integer(v));
	n = json_number_value(v);
	assert_true(n == 3.0);

	v = json_object_get(version, "minor");
	assert_true(json_is_integer(v));
	n = json_number_value(v);
	assert_true(n == 1.0);

	json_free(&object);
}

static void test_json_add_address(void **state)
{
	struct json_object object;
	struct json_t *value = NULL;
	struct tsocket_address *ip4  = NULL;
	struct tsocket_address *ip6  = NULL;
	struct tsocket_address *pipe = NULL;
	const char *s = NULL;
	int rc;

	TALLOC_CTX *ctx = talloc_new(NULL);

	object = json_new_object();

	json_add_address(&object, "null", NULL);

	rc = tsocket_address_inet_from_strings(
		ctx,
		"ip",
		"127.0.0.1",
		21,
		&ip4);
	assert_int_equal(0, rc);
	json_add_address(&object, "ip4", ip4);

	rc = tsocket_address_inet_from_strings(
		ctx,
		"ip",
		"2001:db8:0:0:1:0:0:1",
		42,
		&ip6);
	assert_int_equal(0, rc);
	json_add_address(&object, "ip6", ip6);

	rc = tsocket_address_unix_from_path(ctx, "/samba/pipe", &pipe);
	assert_int_equal(0, rc);
	json_add_address(&object, "pipe", pipe);

	assert_int_equal(4, json_object_size(object.root));

	value = json_object_get(object.root, "null");
	assert_true(json_is_null(value));

	value = json_object_get(object.root, "ip4");
	assert_true(json_is_string(value));
	s = json_string_value(value);
	assert_string_equal("ipv4:127.0.0.1:21", s);

	value = json_object_get(object.root, "ip6");
	assert_true(json_is_string(value));
	s = json_string_value(value);
	assert_string_equal("ipv6:2001:db8::1:0:0:1:42", s);

	value = json_object_get(object.root, "pipe");
	assert_true(json_is_string(value));
	s = json_string_value(value);
	assert_string_equal("unix:/samba/pipe", s);

	json_free(&object);
	TALLOC_FREE(ctx);
}

static void test_json_add_sid(void **state)
{
	struct json_object object;
	struct json_t *value = NULL;
	const char *SID = "S-1-5-21-2470180966-3899876309-2637894779";
	struct dom_sid sid;
	const char *s = NULL;


	object = json_new_object();

	json_add_sid(&object, "null", NULL);

	assert_true(string_to_sid(&sid, SID));
	json_add_sid(&object, "sid", &sid);

	assert_int_equal(2, json_object_size(object.root));

	value = json_object_get(object.root, "null");
	assert_true(json_is_null(value));

	value = json_object_get(object.root, "sid");
	assert_true(json_is_string(value));
	s = json_string_value(value);
	assert_string_equal(SID, s);
	json_free(&object);
}

static void test_json_add_guid(void **state)
{
	struct json_object object;
	struct json_t *value = NULL;
	const char *GUID = "3ab88633-1e57-4c1a-856c-d1bc4b15bbb1";
	struct GUID guid;
	const char *s = NULL;
	NTSTATUS status;


	object = json_new_object();

	json_add_guid(&object, "null", NULL);

	status = GUID_from_string(GUID, &guid);
	assert_true(NT_STATUS_IS_OK(status));
	json_add_guid(&object, "guid", &guid);

	assert_int_equal(2, json_object_size(object.root));

	value = json_object_get(object.root, "null");
	assert_true(json_is_null(value));

	value = json_object_get(object.root, "guid");
	assert_true(json_is_string(value));
	s = json_string_value(value);
	assert_string_equal(GUID, s);

	json_free(&object);
}

static void test_json_to_string(void **state)
{
	struct json_object object;
	char *s = NULL;

	TALLOC_CTX *ctx = talloc_new(NULL);

	object = json_new_object();
	object.error = true;

	s = json_to_string(ctx, &object);
	assert_null(s);

	object.error = false;
	s = json_to_string(ctx, &object);
	assert_string_equal("{}", s);
	TALLOC_FREE(s);

	json_add_string(&object, "name", "value");
	s = json_to_string(ctx, &object);
	assert_string_equal("{\"name\": \"value\"}", s);
	TALLOC_FREE(s);

	json_free(&object);
	TALLOC_FREE(ctx);
}
#endif

static void test_audit_get_timestamp(void **state)
{
	const char *t = NULL;
	char *c;
	struct tm tm;
	time_t before;
	time_t after;
	time_t actual;

	TALLOC_CTX *ctx = talloc_new(NULL);

	before = time(NULL);
	t = audit_get_timestamp(ctx);
	after = time(NULL);


	c = strptime(t, "%a, %d %b %Y %H:%M:%S", &tm);
	tm.tm_isdst = -1;
	if (c != NULL && *c == '.') {
		char *e;
		strtod(c, &e);
		c = e;
	}
	if (c != NULL && *c == ' ') {
		struct tm tz;
		c = strptime(c, " %Z", &tz);
	}
	assert_non_null(c);
	assert_int_equal(0, strlen(c));

	actual = mktime(&tm);

	/*
	 * The timestamp should be before <= actual <= after
	 */
	assert_true(difftime(actual, before) >= 0);
	assert_true(difftime(after, actual) >= 0);

	TALLOC_FREE(ctx);
}

int main(int argc, const char **argv)
{
	const struct CMUnitTest tests[] = {
#ifdef HAVE_JANSSON
		cmocka_unit_test(test_json_add_int),
		cmocka_unit_test(test_json_add_bool),
		cmocka_unit_test(test_json_add_string),
		cmocka_unit_test(test_json_add_object),
		cmocka_unit_test(test_json_add_to_array),
		cmocka_unit_test(test_json_add_timestamp),
		cmocka_unit_test(test_json_add_stringn),
		cmocka_unit_test(test_json_add_version),
		cmocka_unit_test(test_json_add_address),
		cmocka_unit_test(test_json_add_sid),
		cmocka_unit_test(test_json_add_guid),
		cmocka_unit_test(test_json_to_string),
#endif
		cmocka_unit_test(test_audit_get_timestamp),
	};

	cmocka_set_message_output(CM_OUTPUT_SUBUNIT);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
