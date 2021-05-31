
#include <stdlib.h>
#include <string.h>

#include "CJson/cJSON.h"
#include "LoadJSON.h"
#include "Logger/Logger.h"

char *p_app_json;
char *p_info_json;

int load_json(const char *pJsonPath, char **buffer) {
	FILE *json_file = fopen(pJsonPath, "rb");
	if (NULL == json_file) {
		XLOG_TRACE("error load json file.");
		return -1;
	}

	fseek(json_file, 0, SEEK_END);
	unsigned long json_size = ftell(json_file);
	rewind(json_file);

	char *json_temp = (char *)malloc(json_size + 1);
	memset(json_temp, 0, json_size + 1);
	size_t ret = fread(json_temp, 1, json_size, json_file);
	if (ret != json_size) {
		XLOG_TRACE("Load json file {} ERROR.", pJsonPath);
		return -1;
	}
	json_temp[json_size] = '\0';

	cJSON *app_json = cJSON_Parse(json_temp);
	free(json_temp);
	if (!app_json) {
		XLOG_TRACE("cJSON_Parse {} ERROR.", pJsonPath);
		return -1;
	}

	*buffer = cJSON_PrintBuffered(app_json, json_size + 1, 0);
	cJSON_Delete(app_json);
	if (!buffer) {
		XLOG_TRACE("cJSON_PrintBuffered ERROR.");
		return -1;
	}

	return 0;
}

int load_app_json(const char *pJsonPath) {
	return load_json(pJsonPath, &p_app_json);
}

int load_info_json(const char *pJsonPath) {
	return load_json(pJsonPath, &p_info_json);
}

int load_all_json(const char *pJsonDirPath) {
	int ret = 0;
	char app_json_path[256] = {0};
	strcat(app_json_path, pJsonDirPath);
	strncat(app_json_path, "/app.json", strlen("/app.json"));
	ret = load_app_json(app_json_path);
	if (ret) {
		XLOG_TRACE("load_app_json ERROR.");
		return -1;
	}

	char info_json_path[256] = {0};
	strcat(info_json_path, pJsonDirPath);
	strncat(info_json_path, "/info.json", strlen("/info.json"));
	ret = load_info_json(info_json_path);
	if (ret) {
		XLOG_TRACE("load_info_json ERROR.");
		return -1;
	}

	return 0;
}
