#include "report_buffer_lab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim_newline(char *line) {
	size_t length;

	length = strlen(line);
	if (length > 0 && line[length - 1] == '\n') {
		line[length - 1] = '\0';
	}
	length = strlen(line);
	if (length > 0 && line[length - 1] == '\r') {
		line[length - 1] = '\0';
	}
}

int load_orders(FILE *in, struct order_record records[], size_t capacity, struct lab_stats *stats) {
	char line[LAB_MAX_LINE_LEN];
	char format[64];
	size_t count;

	if (in == NULL || records == NULL || stats == NULL) {
		return -1;
	}

	memset(stats, 0, sizeof(*stats));
	count = 0;

	snprintf(format, sizeof(format), "%%%zu[^|]|%%d|%%d|%%%zu[^|]%%n",
		 sizeof(records[0].name) - 1, sizeof(records[0].category) - 1);

	while (fgets(line, sizeof(line), in) != NULL) {
		struct order_record rec;
		int consumed = 0;
		size_t name_len;

		stats->input_reads++;
		trim_newline(line);

		if (line[0] == '\0') {
			continue;
		}

		if (count == capacity) {
			fprintf(stderr, "too many records\n");
			return -1;
		}

		memset(&rec, 0, sizeof(rec));
		if (sscanf(line, format, rec.name, &rec.quantity, &rec.unit_price,
			   rec.category, &consumed) != 4 || line[consumed] != '\0') {
			fprintf(stderr, "malformed line %zu: %s\n", stats->input_reads, line);
			return -1;
		}

		rec.total_price = rec.quantity * rec.unit_price;

		name_len = strlen(rec.name);
		if (name_len > stats->longest_name) {
			stats->longest_name = name_len;
		}
		stats->grand_total += rec.total_price;
		if (count == 0 || rec.total_price > stats->max_total) {
			stats->max_total = rec.total_price;
		}

		records[count++] = rec;
	}

	stats->records_loaded = count;
	return 0;
}

int build_report(const struct order_record records[], size_t count, const struct lab_stats *stats,
		 char *out, size_t out_size) {
	size_t used;
	size_t i;
	int written;

	if (records == NULL || stats == NULL || out == NULL || out_size == 0) {
		return -1;
	}

	used = 0;

	written = snprintf(out + used, out_size - used, "report | rows=%zu | longest=%zu\n",
			   stats->records_loaded, stats->longest_name);
	if (written < 0 || (size_t)written >= out_size - used) {
		return -1;
	}
	used += (size_t)written;

	for (i = 0; i < count; i++) {
		written = snprintf(out + used, out_size - used,
				   "%02zu | %-*s | qty=%2d | unit=%3d | total=%3d | cat=%s\n",
				   i + 1, (int)stats->longest_name, records[i].name,
				   records[i].quantity, records[i].unit_price,
				   records[i].total_price, records[i].category);
		if (written < 0 || (size_t)written >= out_size - used) {
			return -1;
		}
		used += (size_t)written;
	}

	written = snprintf(out + used, out_size - used,
			   "summary | grand_total=%d | max_total=%d | reads=%zu | writes=%zu\n",
			   stats->grand_total, stats->max_total, stats->input_reads,
			   stats->output_writes);
	if (written < 0 || (size_t)written >= out_size - used) {
		return -1;
	}

	return 0;
}

int main(int argc, char **argv) {
	FILE *in;
	struct order_record records[LAB_MAX_RECORDS];
	struct lab_stats stats;
	char report[LAB_REPORT_CAPACITY];
	size_t report_length;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <orders-file>\n", argv[0]);
		return 1;
	}

	in = fopen(argv[1], "r");
	if (in == NULL) {
		perror("fopen");
		return 1;
	}

	if (load_orders(in, records, LAB_MAX_RECORDS, &stats) != 0) {
		fclose(in);
		return 1;
	}

	if (fclose(in) != 0) {
		perror("fclose");
		return 1;
	}

	stats.output_writes = 1;

	if (build_report(records, stats.records_loaded, &stats, report, sizeof(report)) != 0) {
		fprintf(stderr, "failed to build report\n");
		return 1;
	}

	report_length = strlen(report);
	if (fwrite(report, 1, report_length, stdout) != report_length) {
		perror("fwrite");
		return 1;
	}

	return 0;
}
