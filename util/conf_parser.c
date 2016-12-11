/*
 * MiniEAP configuration file parser
 * 
 * Configuration file consists of lines in the following format:
 *  KEY=VALUE
 * Comment lines must begin with #
 * No inline comments allowed.
 * Leading spaces are ignored.
 *
 * The parser will create a linked list for each key-value pair
 * in the file.
 */

#include <linkedlist.h>
#include <minieap_common.h>
#include <conf_parser.h>
#include <logging.h>
#include <misc.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

static char* g_conf_file;
static LIST_ELEMENT* g_conf_list;

/*
 * These macros will change the ptr if they find what they want successfully
 */
#define FIND_NEXT_CHAR(start, ptr, chr, maxlen) \
	do { \
		char* tmp = ptr; \
		for (ptr = start; ptr < start + maxlen && *ptr != chr; ptr++); \
		if (*ptr != chr) ptr = tmp; \
	} while (0);

/*
 * Find 1st non-space char, modify ptr if found
 */
#define LTRIM(start, ptr, maxlen) \
	do { \
		char* tmp = ptr; \
		for (ptr = start; ptr < start + maxlen && isspace(*ptr); ptr++); \
		if (!isspace(*ptr)) ptr = tmp; \
	} while (0);

/*
 * Trim trailing spaces.
 * Will modify original string.
 */
#define RTRIM(start, maxlen) \
	do { \
		char* tmp; \
		int actual_len = strnlen(start, maxlen); \
		for (tmp = start + actual_len - 1; tmp >= start && isspace(*tmp); tmp--); \
		if (isspace(*tmp)) *tmp = 0; \
	} while (0);

void conf_parser_set_file_path(char* path) {
	g_conf_file = path;
}

RESULT conf_parser_parse_now() {
	if (!g_conf_list) {
		return FAILURE;
	}

	FILE* fp = fopen(g_conf_file, "r");
	if (fp <= 0) {
		PR_ERRNO("无法打开配置文件");
		return FAILURE;
	}

	char line_buf[MAX_LINE_LEN] = {0};
	char* start_pos;
	char* delim_pos;
	CONFIG_PAIR* conf_pair;
	while (fgets(line_buf, MAX_LINE_LEN, fp)) {
		int line_len = strnlen(line_buf, MAX_LINE_LEN);
		LTRIM(line_buf, start_pos, line_len);
		if (*start_pos == '#') {
			continue;
		}
		FIND_NEXT_CHAR(start_pos, delim_pos, '=', line_len - (start_pos - line_buf));
		if (delim_pos != start_pos) {
			// If delimiter is found
			RTRIM(delim_pos, line_len - (delim_pos - line_buf));

			conf_pair = (CONFIG_PAIR*)malloc(sizeof(CONFIG_PAIR));
			if (conf_pair <= 0) {
				PR_ERRNO("无法为配置文件参数项分配内存");
				fclose(fp);
				return FAILURE;
			}
			conf_pair->key = strndup(start_pos, delim_pos - start_pos);
			conf_pair->value = strndup(delim_pos + 1, line_len - (delim_pos - line_buf) - 1); // Without '='
			insert_data(&g_conf_list, conf_pair);
		} else {
			PR_WARN("配置文件格式错误：%s", line_buf);
		}
	}

	fclose(fp);
	return SUCCESS;
}

static RESULT conf_pair_key_cmpfunc(void* to_find, void* node) {
#define TO_CONFIG_PAIR(x) ((CONFIG_PAIR*)x)
	return strcmp((char*)to_find, TO_CONFIG_PAIR(node)->key);
}

RESULT conf_parser_get_value(const char* key, char* buf, int buflen) {
	CONFIG_PAIR* pair = lookup_data(g_conf_list, (void*)key, conf_pair_key_cmpfunc);
	if (!pair) {
		return FAILURE;
	}

	if (strlen(pair->value) >= buflen) {
		return FAILURE;
	}

	strcpy(buf, pair->value);
	return SUCCESS;
}

RESULT conf_parser_set_value(const char* key, const char* value) {
	CONFIG_PAIR* pair = lookup_data(g_conf_list, (void*)key, conf_pair_key_cmpfunc);
	if (pair) {
		chk_free((void**)&pair->value);
		pair->value = strdup(value);
	} else {
		pair = (CONFIG_PAIR*)malloc(sizeof(CONFIG_PAIR));
		if (pair <= 0) {
			PR_ERRNO("无法为新的配置项分配内存空间");
			return FAILURE;
		}
		pair->key = strdup(key);
		pair->value = strdup(value);
		insert_data(&g_conf_list, pair);
	}

	return SUCCESS;
}

static void conf_write_one_pair(void* node, void* fp) {
	fprintf((FILE*)fp, "%s=%s\n", TO_CONFIG_PAIR(node)->key, TO_CONFIG_PAIR(node)->value);
}

RESULT conf_parser_save_file() {
	if (!g_conf_list) {
		return FAILURE;
	}

	FILE* fp = fopen(g_conf_file, "w");
	if (fp <= 0) {
		PR_ERRNO("无法打开配置文件");
		return FAILURE;
	}

	list_traverse(g_conf_list, conf_write_one_pair, fp);
	fclose(fp);
	return SUCCESS;
}