/*
 * Copyright 2015-2017 Two Pore Guys, Inc.
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <glib.h>

typedef struct {
	MyObject *obj;
	OtherObject *helper;
} MyObjectFixture;

static void
my_object_fixture_set_up (MyObjectFixture *fixture,
			  gconstpointer user_data)
{
	fixture->obj = my_object_new ();
	my_object_set_prop1 (fixture->obj, "some-value");
	my_object_do_some_complex_setup (fixture->obj, user_data);

	fixture->helper = other_object_new ();
}

static void
my_object_fixture_tear_down (MyObjectFixture *fixture,
			     gconstpointer user_data)
{
	g_clear_object (&fixture->helper);
	g_clear_object (&fixture->obj);
}

static void
test_my_object_test1 (MyObjectFixture *fixture,
		      gconstpointer user_data)
{
	g_assert_cmpstr (my_object_get_property (fixture->obj), ==, "initial-value");
}

static void
test_my_object_test2 (MyObjectFixture *fixture,
		      gconstpointer user_data)
{
	my_object_do_some_work_using_helper (fixture->obj, fixture->helper);
	g_assert_cmpstr (my_object_get_property (fixture->obj), ==, "updated-value");
}

int
main (int argc, char *argv[])
{
	setlocale (LC_ALL, "");

	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

	// Define the tests.
	g_test_add ("/my-object/test1", MyObjectFixture, "some-user-data",
		    my_object_fixture_set_up, test_my_object_test1,
		    my_object_fixture_tear_down);
	g_test_add ("/my-object/test2", MyObjectFixture, "some-user-data",
		    my_object_fixture_set_up, test_my_object_test2,
		    my_object_fixture_tear_down);

	return g_test_run ();
}