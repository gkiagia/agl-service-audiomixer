/*
 * Copyright (C) 2019 Collabora Ltd.
 *   @author George Kiagiadakis <george.kiagiadakis@collabora.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <json-c/json.h>

#include <afb/afb-binding.h>

#define NAME "pipewire-media-session"

int session_comm(const char *command, char *reply, size_t reply_size)
{
	int fd;
	struct sockaddr_un addr;
	int name_size;
	const char *runtime_dir = NULL;
	ssize_t r;

	if ((runtime_dir = getenv("XDG_RUNTIME_DIR")) == NULL) {
		fprintf(stderr, "connect failed: XDG_RUNTIME_DIR not set in the environment");
		return -1;
	}

	fd = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		perror("socket");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	name_size = snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/"NAME, runtime_dir) + 1;

	if (name_size > (int) sizeof(addr.sun_path)) {
		fprintf(stderr, "socket path \"%s/"NAME"\" plus null terminator exceeds 108 bytes",
			runtime_dir);
		close(fd);
		return -1;
	}

	if (connect(fd,(const struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("connect");
		close(fd);
		return -1;
	}

	while (true) {
		r = write(fd, command, strlen(command));
		if (r < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			perror("write");
			r = 0;
			break;
		}
		break;
	}
	if (r == 0) {
		fprintf(stderr, "Nothing was written");
		close(fd);
		return -1;
	}

	while (true) {
		r = read(fd, reply, reply_size);
		if (r < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			perror("read");
			r = 0;
			break;
		}
		break;
	}
	if (r == 0) {
		fprintf(stderr, "EOF");
		close(fd);
		return -1;
	}

	reply[r] = '\0';
	close(fd);

	return 0;
}

static void volume(struct afb_req request)
{
	json_object *ret_json;
	const char *role = afb_req_value(request, "role");
	const char *value = afb_req_value(request, "value");
	int volume = -1;
	char command[100];
	char reply[10];

	if(value) {
		volume = atoi(value);
		if (volume < 0 || volume > 100) {
			afb_req_fail(request, "failed",
				"Invalid volume value (must be between 0 and 100)");
			return;
		}
	}

	snprintf(command, sizeof(command), "volume %s %d", role, volume);
	if (session_comm(command, reply, sizeof(reply)) < 0) {
		afb_req_fail(request, "failed", "media-session communication failed");
		return;
	}

	volume = atoi(reply);
	if (volume < 0) {
		afb_req_fail(request, "failed", "media-session replied -1");
		return;
	}

	ret_json = json_object_new_object();
	json_object_object_add(ret_json, "volume", json_object_new_int(volume));
	afb_req_success(request, ret_json, NULL);
}

static void mute(struct afb_req request)
{
	json_object *ret_json;
	const char *role = afb_req_value(request, "role");
	const char *value = afb_req_value(request, "value");
	int mute = -1;
	char command[100];
	char reply[10];

	if(value) {
		mute = atoi(value);
		if (mute < 0 || mute > 1) {
			afb_req_fail(request, "failed",
				"Invalid mute value (must be between 0 and 1)");
			return;
		}
	}

	snprintf(command, sizeof(command), "mute %s %d", role, mute);
	if (session_comm(command, reply, sizeof(reply)) < 0) {
		afb_req_fail(request, "failed", "media-session communication failed");
		return;
	}

	mute = atoi(reply);
	if (mute < 0) {
		afb_req_fail(request, "failed", "media-session replied -1");
		return;
	}

	ret_json = json_object_new_object();
	json_object_object_add(ret_json, "mute", json_object_new_int(mute));
	afb_req_success(request, ret_json, NULL);
}

static void zone(struct afb_req request)
{
	json_object *ret_json;
	const char *role = afb_req_value(request, "role");
	const char *value = afb_req_value(request, "value");
	int zone = -1;
	char command[100];
	char reply[10];

	if(value) {
		zone = atoi(value);
		if (zone < 0 || zone > 4) {
			afb_req_fail(request, "failed",
				"Invalid mute value (must be between 0 and 4)");
			return;
		}
	}

	snprintf(command, sizeof(command), "zone %s %d", role, zone);
	if (session_comm(command, reply, sizeof(reply)) < 0) {
		afb_req_fail(request, "failed", "media-session communication failed");
		return;
	}

	zone = atoi(reply);
	if (zone < 0) {
		afb_req_fail(request, "failed", "media-session replied -1");
		return;
	}

	ret_json = json_object_new_object();
	json_object_object_add(ret_json, "zone", json_object_new_int(zone));
	afb_req_success(request, ret_json, NULL);
}

static const struct afb_verb_v2 verbs[]= {
	{ .verb = "volume", .session = AFB_SESSION_NONE, .callback = volume, .info = "Get/Set volume" },
	{ .verb = "mute",   .session = AFB_SESSION_NONE, .callback = mute,   .info = "Get/Set mute" },
	{ .verb = "zone",   .session = AFB_SESSION_NONE, .callback = zone,   .info = "Get/Set zone" },
	{ }
};

static int init()
{
	return 0;
}

const struct afb_binding_v2 afbBindingV2 = {
	.info = "audiomixer service",
	.api  = "audiomixer",
	.verbs = verbs,
	.init = init,
};
