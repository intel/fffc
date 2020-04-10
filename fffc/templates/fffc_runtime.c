// Copyright (C) 2020 Intel Corporation
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <float.h>
#include <fcntl.h>
#include <ftw.h>
#include <link.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/auxv.h>
#include <sys/param.h>
#include <sys/personality.h>
#include <sys/resource.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>


// XXX It sucks to take such a heavy platform dependency
// XXX onboard for one system call, but this is what takes
// XXX us back to late 2014 in terms of kernel support.
// XXX glibc support didn't follow until much later.
#ifdef __GLIBC__
#if __GLIBC_MINOR__ > 25
#include <sys/random.h>

static
int internal_getrandom(void *rnd, size_t len) {
    return getrandom(rnd, len, GRND_NONBLOCK);
}

#else
#include <sys/syscall.h>

static
int internal_getrandom(void *rnd, size_t len) {
    return syscall(SYS_getrandom, rnd, len, 1);
}

#define SNAN (__builtin_nans(""))
#define SNANF (__builtin_nansf(""))
#define SNANL (__builtin_nansl(""))

#endif
#endif

#include <subhook.h>

#include "fffc_runtime.h"


static int FFFC_MODE_RAND = 0;
static int FFFC_MODE_COUNT = 1;
static int FFFC_MODE_ITER = 2;
static int FFFC_MODE_NORMAL = 3;
static int FFFC_MODE_SMART_SKIP = 4;
static int FFFC_MODE_RESIZE = 5;
static int FFFC_PREVIOUS_MODE;

static int FFFC_FORK_COUNT = 256;
static int FFFC_GENERATION_COUNT = 256;
static float FFFC_SKIP_RATE = 1.0/256.0;
static int FFFC_RESIZE_PASS_MASK = 0xFF;
static int FFFC_RESIZE_POINTER_MASK = 0x0F;
static int FFFC_BREAK_AT_ITER = -1;
static int FFFC_PARALLEL_COUNT = 16;
static int FFFC_MAX_STATE_COUNT = 1024;
static int FFFC_FEATURES_PER_COUNTER = 8;

static int FFFC_TIMESTAMP_LENGTH = 64;
static int FFFC_RECURSIVE_FDS = 128;
static int FFFC_PARENT_RETRY = 128;

static char* FFFC_DATA_PATH = ".";
static char* FFFC_CRASH_PATH = ".";

static char* FFFC_DEBUG_REPLAY = 0;

static int FFFC_TRACING = 0;

static subhook_t FFFC_TARGET_HOOK;

#define FFFC_LOG_DEBUG 0xFF
#define FFFC_LOG_INFO 0xC0
#define FFFC_LOG_WARN 0x80
#define FFFC_LOG_ERROR 0x40
#define FFFC_LOG_NONE 0x00

static int FFFC_LOG_LEVEL = FFFC_LOG_WARN;

// These are nested, so the order is FFFC_DATA_PATH/GLOBAL_STATE/CALL_STATE/MUTATION_STATE
// When a mutation crashes, the mutation state dir is moved to the FFFC_CRASH_PATH
#define GLOBAL_STATE_FORMAT "%s/fffc_state.%s.%s.XXXXXX"
#define GLOBAL_CRASH_FORMAT "%s/fffc_crashes.%s.%s.XXXXXX"
#define CALL_STATE_FORMAT "%s/%08d.XXXXXX"
#define MUTATION_STATE_FORMAT "%s/%s-%08d-%08llu"

// This lives in the call state directory
#define PARENT_STATE_SUFFIX "/parents"
#define TEMP_PARENT_STATE_SUFFIX "/parents.tmp"
#define FEATURES_STATE_SUFFIX "/features"

// These live in the mutation state directory
#define COVERAGE_STATE_SUFFIX "/coverage"
#define CRASH_STATE_SUFFIX "/crash"
#define LOG_STATE_SUFFIX "/log"
#define STDOUT_SUFFIX "/stdout"
#define STDERR_SUFFIX "/stderr"

// These live in the crash directory
#define SAVED_CRASH_DIR_SUFFIX "/crash.XXXXXX"

#define TEXT_RED(str) "\x1b[31m" str "\x1b[0m"
#define TEXT_YELLOW(str) "\x1b[33;1m" str "\x1b[0m"
#define TEXT_GREEN(str) "\x1b[32m" str "\x1b[0m"
#define TEXT_NORMAL "\x1b[0m"

void __asan_describe_address(void *addr);
const char *__asan_locate_address(void *addr, char *name, size_t name_size,
                                  void **region_address, size_t *region_size);

void __asan_poison_memory_region(void const volatile *addr, size_t size);
void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
void __sanitizer_set_report_path(const char *path);

int fffc_wait() {
	int status;
	while (wait(&status) != -1) {}
	return 0;
}

static
int file_is_gcda(char *fname) {
	fname = strrchr(fname, '.');
	if (fname == NULL) {
		return -1;
	}
	return strcmp(fname, ".gcda") ? 0 : -1;
}

static
int gcda_read_int32(int fd, int *value) {
	unsigned char buf[4];
	if (read(fd, buf, 4) != 4) {
		return -1;
	}
	memcpy(value, buf, 4);
	return 0;
}

static
int gcda_read_int64(int fd, long *value) {
	unsigned char buf[8];
	if (read(fd, buf, 8) != 8) {
		return -1;
	}
	memcpy(value, buf, 8);
	return 0;
}

static
int counter_to_feature(long counter, struct FFFC_coverage_feature *feature) {
	memset(feature, 0, sizeof(struct FFFC_coverage_feature));
	if (!counter) {
		feature->none++;
	} else if (counter < 8) {
		feature->few++;
	} else if (counter < 128) {
		feature->some++;
	} else {
		feature->many++;
	}
	return 0;
}

static
int update_feature(struct FFFC_coverage_feature *current, struct FFFC_coverage_feature *update) {
	current->none += update->none;
	current->few += update->few;
	current->some += update->some;
	current->many += update->many;
	return 0;
}

static
int print_feature(char *msg, struct FFFC_coverage_feature *feature) {
	fffc_print_green(msg);
	fffc_print_green("struct FFFC_coverage_feature {");
	fffc_print_int_green("\tnone", feature->none);
	fffc_print_int_green("\tfew", feature->few);
	fffc_print_int_green("\tsome", feature->some);
	fffc_print_int_green("\tmany", feature->many);
	fffc_print_green("}\n");
	return 0;
}

static
int compute_score(struct FFFC_coverage_feature *current, struct FFFC_coverage_feature *update, double *score) {
	double total = current->none + current->few + current->some + current->many;
	if (!total) {
		*score = 0;
		return 0;
	}
	
	double none_odds = 1 - (current->none / total);
	double few_odds = 1 - (current->few / total);
	double some_odds = 1 - (current->some / total);
	double many_odds = 1 - (current->many / total);

	double points = 0;
	if (update->none) {
		points = none_odds;
	} else if (update->few) {
		points = few_odds;
	} else if (update->some) {
		points = some_odds;
	} else if (update->many) {
		points = many_odds;
	}
	*score += points;

	return 0;
}

static
int read_next_feature(struct FFFC_coverage_feature *feature) {
	int fd = FFFC_GENERATION_STATE.features_read_fd;
	int read_size = sizeof(struct FFFC_coverage_feature);
	int actually_read = read(fd, feature, read_size);
	if (actually_read == 0) {
		bzero(feature, sizeof(struct FFFC_coverage_feature));
	} else if (actually_read != read_size) {
		fffc_print_red("Couldn't read features");
		fffc_print_red(strerror(errno));
		fffc_print_int_red("actually read", actually_read);
		return -1;
	}
	return 0;
}

int write_next_feature(struct FFFC_coverage_feature *feature) {
	int fd = FFFC_GENERATION_STATE.features_write_fd;
	int write_size = sizeof(struct FFFC_coverage_feature);
	if (write(fd, feature, write_size) != write_size) {
		return -1;
	}
	return 0;
}

static
int update_score(long counter, double *current_score) {
	// Get the current feature value, or nothing
	struct FFFC_coverage_feature old_feature;
	if (read_next_feature(&old_feature) < 0) {
		return -1;
	}

	// Convert the counter into a feature value
	struct FFFC_coverage_feature new_feature;
	if (counter_to_feature(counter, &new_feature) < 0) {
		return -1;
	}

	// Generate the score delta
	double delta;
	if (compute_score(&old_feature, &new_feature, &delta) < 0) {
		return -1;
	}
	*current_score += delta;

	// Update the feature value
	if (update_feature(&old_feature, &new_feature) < 0) {
		return -1;
	}

	// Write it back
	if (write_next_feature(&old_feature) < 0) {
		return -1;
	}

	return 0;
}

static
int gcda_handle_counter(int fd, double *score) {
	long counter_value;
	if (gcda_read_int64(fd, &counter_value) < 0) {
		return -1;
	}
	if (update_score(counter_value, score) < 0) {
		return -1;
	}
	return 0;
}

static
int gcda_handle_function(int gcda_fd, double *score) {
	int count;
	if (gcda_read_int32(gcda_fd, &count)) {
		return -1;
	}
	for (int i=0; i < (count/2); i++) {
		gcda_handle_counter(gcda_fd, score);
	}
	return 0;
}

static
int gcda_handle_file(int dirfd, char *filename, double *score) {
	int fd = openat(dirfd, filename, O_RDONLY);
	if (fd < 0) {
		fffc_print_red("Couldn't open coverage file!");
		return -1;
	}
	int value = 0;
	while(!gcda_read_int32(fd, &value)) {
		if (value == 0x1a10000) {
			gcda_handle_function(fd, score);
		}
	}
	close(fd);
	return 0;
}

static
int gcda_handle_directory(char *directory, double *score) {
	int dirfd = open(directory, O_RDONLY);
	if (dirfd < 0) {
		fffc_print_red("Couldn't open state directory");
		return -1;
	}
	int coverage_dirfd = openat(dirfd, "coverage", O_RDONLY);
	if (coverage_dirfd < 0) {
		fffc_print_red("Couldn't open coverage directory");
		close(dirfd);
		return -1;
	}
	DIR *dir = fdopendir(coverage_dirfd);
	if (!dir) {
		fffc_print_red("Couldn't open coverage dirfd");
		close(dirfd);
		close(coverage_dirfd);
		return -1;
	}
	struct dirent *f;
	while ((f = readdir(dir)) != NULL) {
		if (file_is_gcda(f->d_name)) {
			gcda_handle_file(coverage_dirfd, f->d_name, score);
		}
	}

	if (lseek(FFFC_GENERATION_STATE.features_read_fd, 0, SEEK_SET) < 0) {
		fffc_print_red("Couldn't reset the read fd");
		close(dirfd);
		close(coverage_dirfd);
		closedir(dir);
		return -1;
	}
	if (lseek(FFFC_GENERATION_STATE.features_write_fd, 0, SEEK_SET) < 0) {
		fffc_print_red("Couldn't reset the write fd");
		close(dirfd);
		close(coverage_dirfd);
		closedir(dir);
		return -1;
	}

	close(dirfd);
	close(coverage_dirfd);
	closedir(dir);
	return 0;
}

static
int get_state_count(long *count) {
	struct stat st;
	if (fstat(FFFC_GENERATION_STATE.parents_fd, &st) < 0) {
		fffc_print_red("Couldn't get state count");
		return -1;		
	}
	*count = st.st_size / FFFC_MAX_PATH_LENGTH;
	return 0;
}

static
int do_rm(const char *path, const struct stat *c, int t, struct FTW *f) {
	int result = remove(path);
    return 0;
}

static
int rmrf(char *path) {
	return nftw(path, do_rm, FFFC_RECURSIVE_FDS, FTW_DEPTH);
}

static
int qcompare(const void* a, const void* b) {
	double a_score = ((struct FFFC_score_entry*)a)->score;
	double b_score = ((struct FFFC_score_entry*)b)->score;
	if (a_score < b_score) {
		return -1;
	} else if (a_score > b_score) {
		return 1;
	}
	return 0;
}

static
int get_random_state() {
	// Go back to the head of the parent file
	if (lseek(FFFC_GENERATION_STATE.parents_fd, 0, SEEK_SET) < 0) {
		fffc_print_red("Couldn't seek in the parents file");
		return -1;
	}
}

static
int reap(void) {
	// Go back to the head of the parent file
	if (lseek(FFFC_GENERATION_STATE.parents_fd, 0, SEEK_SET) < 0) {
		fffc_print_red("Couldn't seek in the parents file");
		return -1;
	}

	// Get the total number of states
	long original_num_states = 0;
	if (get_state_count(&original_num_states) < 0) {
		fffc_print_red("Unable to get the state count");
		return -1;
	}
	long num_victims = original_num_states - FFFC_MAX_STATE_COUNT;
	if (num_victims < 0) {
		return 0;
	}

	// Create the temporary parent
	char tmp_parents_path[FFFC_MAX_PATH_LENGTH];
	bzero(tmp_parents_path, FFFC_MAX_PATH_LENGTH);
	strcat(tmp_parents_path, FFFC_CALL_STATE.call_state_path);
	strcat(tmp_parents_path, TEMP_PARENT_STATE_SUFFIX);
	int new_parents_fd = open(tmp_parents_path, O_CREAT | O_RDWR, 0644);
	if (new_parents_fd < 0) {
		fffc_print_red("Couldn't open the new parents file");
		fffc_print_red(strerror(errno));
		return -1;
	}

	// Get scores for all the extant states
	struct FFFC_score_entry *scores = malloc(sizeof(struct FFFC_score_entry) * original_num_states);
	bzero(scores, sizeof(struct FFFC_score_entry) * original_num_states);
	for (long i=0; i < original_num_states; i++) {
		char *state_entry = (scores+i)->state_path;
		double *state_score = &((scores+i)->score);
		int bytes_read = read(FFFC_GENERATION_STATE.parents_fd, state_entry, FFFC_MAX_PATH_LENGTH);
		if (bytes_read != FFFC_MAX_PATH_LENGTH) {
			fffc_print_red("Couldn't read parent file");
			return -1;
		}
		if (gcda_handle_directory(state_entry, state_score)) {
			fffc_print_red("Couldn't get coverage!");
			return -1;
		}
	}

	// Sort the states
	qsort(scores, original_num_states, sizeof(struct FFFC_score_entry), qcompare);

	// Copy the non-victims to the new parent path
	long victim_index = 0;
	for (long i=0; i < original_num_states; i++) {
		if (i < num_victims) {
			rmrf(scores[i].state_path);
		} else {
			if (write(new_parents_fd, scores[i].state_path, FFFC_MAX_PATH_LENGTH) != FFFC_MAX_PATH_LENGTH) {
				fffc_print_red("Couldn't write to the temporary parent file.");
				return -1;
			}
		}
	}

	// Replace the old parents file with the new one
	close(new_parents_fd);
	unlink(FFFC_GENERATION_STATE.parents_path);
	rename(tmp_parents_path, FFFC_GENERATION_STATE.parents_path);
	free(scores);
	return 0;
}

static
int fffc_limit_cpu() {
	struct rlimit limit;
	limit.rlim_cur = 1;
	limit.rlim_max = 1;
	if (setrlimit(RLIMIT_CPU, &limit)) {
		fffc_print_red("Unable to set rlimit");
		return -1;
	}
	return 0;
}

static
int fffc_set_child_limits() {
	if (fffc_limit_cpu() < 0) {
		fffc_print_red("Unable to limit cpu");
		return -1;
	}
	return 0;
}

void fffc_install_hook() {
	if (subhook_install(FFFC_TARGET_HOOK) < 0) {
		fffc_print_red("Failed to install hook before returning!");
		fffc_exit();
	}
}

void fffc_remove_hook() {
	if (subhook_remove(FFFC_TARGET_HOOK) < 0) {
		fffc_print_red("Failed to remove hook before calling!");
		fffc_exit();
	}
}

void fffc_restrict_child() {
	if (fffc_set_child_limits() < 0) {
		fffc_print_red("Failed to set limits on child process!");
		fffc_exit();
	}
}

static
int fffc_lfu_lookup(void *ptr, size_t *size) {
	for (int i=0; i < FFFC_LFU_SIZE; i++) {
		if (FFFC_WORKER_STATE.lfu.region_size_lfu_addresses[i] == ptr) {
			*size = FFFC_WORKER_STATE.lfu.region_size_lfu_sizes[i];
			FFFC_WORKER_STATE.lfu.region_size_lfu_frequency[i]++;
			return 0;
		}
	}
	return -1;
}

static
void fffc_lfu_insert(void *ptr, size_t size) {
	int mindex = 0;
	unsigned int minval = 0;
	for (int i=0; i < FFFC_LFU_SIZE; i++) {
		unsigned int count = FFFC_WORKER_STATE.lfu.region_size_lfu_frequency[i];
		if (FFFC_WORKER_STATE.lfu.region_size_lfu_addresses[i] == ptr) {
			FFFC_WORKER_STATE.lfu.region_size_lfu_frequency[i]++;
			return;
		}
		if (count <= minval) {
			mindex = i;
			minval = count;
		} 
	}
	FFFC_WORKER_STATE.lfu.region_size_lfu_addresses[mindex] = ptr;
	FFFC_WORKER_STATE.lfu.region_size_lfu_sizes[mindex] = size;
	FFFC_WORKER_STATE.lfu.region_size_lfu_frequency[mindex] = 1;
}

long long unsigned fffc_estimate_allocation_size(void *mem) {
	size_t region_size = 0;
	void *region_start = NULL;
	if (fffc_lfu_lookup(mem, &region_size) == 0) {
		return region_size;
	}
	__asan_locate_address(mem, NULL, 0, &region_start, &region_size);
	fffc_lfu_insert(mem, region_size);
	return region_size;
}

void fffc_postcall() {}

void fffc_precall() {
	if (FFFC_WORKER_STATE.break_now && FFFC_TRACING) {
		fffc_print_red("Breaking out as requested");
		raise(SIGTRAP);
	}
}

pid_t fffc_fork() {
	return fork();
}

__attribute__((noreturn))
void fffc_exit(void) {
	_exit(-1);
}

void fffc_handle_fork_error() {
	fffc_print_red("Failed to fork, exiting...");
	fffc_exit();
}

void fffc_print_red(char *s) {
	if (FFFC_LOG_LEVEL >= FFFC_LOG_ERROR) {
		printf(TEXT_RED("%s\n"), s);
	}
}

void fffc_print_int_red(char *s, int d) {
	if (FFFC_LOG_LEVEL >= FFFC_LOG_ERROR) {
		printf(TEXT_RED("%s: %d\n"), s, d);
	}
}

void fffc_print_yellow(char *s) {
	if (FFFC_LOG_LEVEL >= FFFC_LOG_WARN) {
		printf(TEXT_YELLOW("%s\n"), s);
	}
}

void fffc_print_int_yellow(char *s, int d) {
	if (FFFC_LOG_LEVEL >= FFFC_LOG_WARN) {
		printf(TEXT_YELLOW("%s: %d\n"), s, d);
	}
}

void fffc_print_green(char *s) {
	if (FFFC_LOG_LEVEL >= FFFC_LOG_INFO) {
		printf(TEXT_GREEN("%s\n"), s);
	}
}

void fffc_print_int_green(char *s, int d) {
	if (FFFC_LOG_LEVEL >= FFFC_LOG_INFO) {
		printf(TEXT_GREEN("%s: %d\n"), s, d);
	}
}

void fffc_print_crash(int current_iter, int crash_count) {
	if (FFFC_LOG_LEVEL >= FFFC_LOG_WARN) {
		printf(	TEXT_YELLOW("Mutation %d crashed, total crash count %d...\n"), 
				current_iter,
				crash_count);
	}
}

void fffc_print_resume() {
	if (FFFC_LOG_LEVEL >= FFFC_LOG_INFO) {
		printf(TEXT_YELLOW("%s\n"), "Resuming original execution...");
	}
}

void fffc_print_backtrace() {
	if (FFFC_LOG_LEVEL < FFFC_LOG_DEBUG) {
		return;
	}
	void *bt_storage[10];
	printf(TEXT_GREEN("%s\n"), "Called from:");
	backtrace(bt_storage, 10);
	backtrace_symbols_fd(bt_storage, 10, STDOUT_FILENO);
}

void fffc_print_size(long long unsigned i) {
	if (FFFC_LOG_LEVEL >= FFFC_LOG_DEBUG) {
		printf(TEXT_YELLOW("size: %llu\n"), i);
	}
}

void fffc_exit_child() {
	exit(EXIT_SUCCESS);
}

void fffc_exit_success() {
	_exit(EXIT_SUCCESS);
}

int fffc_get_random() {
	return rand();
}

int fffc_random_mask(char *region, long long unsigned int size) {
	long long unsigned int mask_size = size * 8; 	// XXX '8' should be a tunable
	char *rnd = malloc(mask_size);

	// fill the mask
	long long unsigned int remaining = mask_size;
	while (remaining > 256) {
		internal_getrandom(rnd + (mask_size - remaining), 256);
		remaining -= 256;
	}
	internal_getrandom(rnd + (mask_size - remaining), remaining);

	// AND the masks to produce a lower probability mutation, then
	// XOR them into the target region
	for (long long unsigned int i=0; i < size; i++) {
		for (int j=0; j < 8; j++) {
				rnd[i] &= rnd[i+(size*j)];
				region[i] ^= rnd[i];
		}
	}

	free(rnd);
	return 0;
}

int fffc_print_malloc(void) {
	if (FFFC_LOG_LEVEL >= FFFC_LOG_DEBUG) {
		char *p = malloc(sizeof(char));
		printf(TEXT_YELLOW("malloc addr: %p\n"), p);
		free(p);
	}
	return 0;
}

int fffc_wait_for_child(int pid) {
	int status = 0;
	wait(&status);
	if (status != 0) {
		return -1;
	}
	return status;
}

int fffc_get_parallel_count(void) {
	if (fffc_debug()) {
		return 1;
	}
	return FFFC_PARALLEL_COUNT;
}

int fffc_wait_for_workers(void) {
	int status = 0;
	int pid = 0;
	int count = 0;
	while (1) {
		pid = wait(&status);
		count++;
		if (pid == -1) {
			return 0;
		} else if (count == FFFC_PARALLEL_COUNT) {
			return 0;
		}
	}
	fffc_print_red("Unable to wait for workers");
	return -1;
}

void fffc_setup_interceptor(void *target, void *replacement) {
	if (FFFC_TARGET_HOOK) {
		fffc_print_yellow("Hook already set, continuing.");
		return;
	}
	FFFC_TARGET_HOOK = subhook_new(	target,
    	                            replacement,
        	                        SUBHOOK_64BIT_OFFSET);

    if (FFFC_TARGET_HOOK == NULL) {
        fffc_print_red("Failed to create new hook.");
        fffc_exit();
    }
    if (subhook_install(FFFC_TARGET_HOOK) < 0) {
        fffc_print_red("Failed to hook target function.");
        fffc_exit();
    }
}

static struct {
	char *path;
	long long unsigned int addr;
} fffc_dl_iter_additional_data;

static
int fffc_dl_iter_callback(struct dl_phdr_info *info, size_t size, void *data) {
	// Check for the case where all paths are already absolute
	if (strcmp(fffc_dl_iter_additional_data.path, info->dlpi_name) == 0) {
		fffc_dl_iter_additional_data.addr = info->dlpi_addr;
		return -1;
	}

	// Check for the case where they aren't
	char *real_dlpi_name = realpath(info->dlpi_name, NULL);
	if (real_dlpi_name && (strcmp(fffc_dl_iter_additional_data.path, real_dlpi_name) == 0)) {
		fffc_dl_iter_additional_data.addr = info->dlpi_addr;
		free(real_dlpi_name);
		return -1;
	}
	free(real_dlpi_name);

	// Didn't find it, keep going
	return 0;
}

void* fffc_get_pointer_to_symbol(long long unsigned int elf_offset, char *path, int recalculate) {
	// We're in a non-PIE binary
	if (!recalculate) {
		return (void*) elf_offset;
	}

	// Setup for the walk through the various shared objects
	fffc_dl_iter_additional_data.path = path;
	dl_iterate_phdr(fffc_dl_iter_callback, NULL);

	// Apply our offsets
	unsigned long symbol_offset = fffc_dl_iter_additional_data.addr;
	symbol_offset += elf_offset;

	// Cast and go home
	return (void*) symbol_offset;
}

float fffc_get_neg_inf() {
	return -INFINITY;
}

float fffc_get_inf() {
	return INFINITY;
}

float fffc_get_flt_epsilon() {
	return FLT_EPSILON;
}

float fffc_get_flt_max() {
	return FLT_MAX;
}

float fffc_get_flt_min() {
	return FLT_MIN;
}

float fffc_get_snanf() {
	return SNANF;
}

double fffc_get_flt_subnormal() {
	return FLT_MIN/2;
}

double fffc_get_dbl_subnormal() {
	return DBL_MIN/2;
}

double fffc_get_dbl_epsilon() {
	return DBL_EPSILON;
}

double fffc_get_dbl_max() {
	return DBL_MAX;
}

double fffc_get_dbl_min() {
	return DBL_MIN;
}

double fffc_get_snan() {
	return SNAN;
}

long double fffc_get_ldbl_subnormal() {
	return LDBL_MIN/2;
}

long double fffc_get_ldbl_epsilon() {
	return LDBL_EPSILON;
}

long double fffc_get_ldbl_max() {
	return LDBL_MAX;
}

long double fffc_get_ldbl_min() {
	return LDBL_MIN;
}

long double fffc_get_snanl() {
	return SNANL;
}

int fffc_get_int_max() {
	return INT_MAX;
}

int fffc_get_int_min() {
	return INT_MIN;
}

short fffc_get_shrt_max() {
	return SHRT_MAX;
}

short fffc_get_shrt_min() {
	return SHRT_MIN;
}

long fffc_get_long_max() {
	return LONG_MAX;
}

long fffc_get_long_min() {
	return LONG_MIN;
}

long long fffc_get_long_long_max() {
	return LLONG_MAX;
}

long long fffc_get_long_long_min() {
	return LLONG_MIN;
}

unsigned long long fffc_get_ulong_max(void) {
	return ULONG_MAX;
}

long long int fffc_strlen(char *s, long long unsigned max_size) {
	return strnlen(s, max_size);
}

int fffc_debug(void) {
	if (FFFC_DEBUG_REPLAY) {
		return 1;
	}
	return 0;
}

static
int override_tracing(void) {
	char *tracing_str = getenv("FFFC_TRACING");
	if (!tracing_str) {
		return 0;
	} else if (strcmp(tracing_str, "True") == 0) {
		FFFC_TRACING = 1;
		fffc_print_green("Using tracing mode.");
	} else {
		return 0;
	}
	return 0;	
}

int fixup_replay_path(char **replay_path) {
	int length = strlen(*replay_path);
	if (length != FFFC_MAX_PATH_LENGTH) {
		fffc_print_yellow("Got invalid replay path.");
		fffc_print_yellow(*replay_path);
		*replay_path = NULL;
		return -1;
	}
	length -= 1;
	for (length; length >= 0; length--) {
		if ((*replay_path)[length] != '/') {
			(*replay_path)[length+1] = 0;
			break;
		}
	}
	if (length < 0) {
		*replay_path = NULL;
		return -1;
	}
	return 0;
}

static
int override_debug_replay_path(void) {
	char *replay_path_str = getenv("FFFC_DEBUG_REPLAY");
	if (!replay_path_str) {
		fffc_print_green("Fuzzing normally; to replay a specific run, set the FFFC_DEBUG_REPLAY=<logfile> environment variable");
		return 0;
	} else if (fixup_replay_path(&replay_path_str)) {
		fffc_print_green("Fuzzing normally; to replay a specific run, set the FFFC_DEBUG_REPLAY=<logfile> environment variable");
	} else {
		fffc_print_green("Using user-provided replay path.");
		FFFC_DEBUG_REPLAY = replay_path_str;
	}
	return 0;	
}

static
int override_working_path(void) {
	char *data_path_str = getenv("FFFC_DATA_PATH");
	if (!data_path_str) {
		fffc_print_green("Using default working path; set via FFFC_DATA_PATH environment variable.");
	} else {
		fffc_print_green("Using user-provided working path.");
		FFFC_DATA_PATH = data_path_str;
	}
	return 0;	
}

static
int override_crash_path(void) {
	char *crash_path_str = getenv("FFFC_CRASH_PATH");
	if (!crash_path_str) {
		fffc_print_green("Using default crash path; set via FFFC_CRASH_PATH environment variable.");
	} else {
		fffc_print_green("Using user-provided crash path.");
		FFFC_CRASH_PATH = crash_path_str;
	}
	return 0;	
}

static
int override_log_level(void) {
	char *log_level_str = getenv("FFFC_LOG_LEVEL");
	if (!log_level_str) {
		fffc_print_green("Using default log level; set via FFFC_LOG_LEVEL environment variable.");
		return 0;
	} else if (strcmp(log_level_str, "DEBUG") == 0) {
		FFFC_LOG_LEVEL = FFFC_LOG_DEBUG;
		fffc_print_green("Using user-provided log level DEBUG");
	} else if (strcmp(log_level_str, "INFO") == 0) {
		FFFC_LOG_LEVEL = FFFC_LOG_INFO;
		fffc_print_green("Using user-provided log level INFO");
	} else if (strcmp(log_level_str, "WARN") == 0) {
		FFFC_LOG_LEVEL = FFFC_LOG_WARN;
		fffc_print_green("Using user-provided log level WARN");
	} else if (strcmp(log_level_str, "ERROR") == 0) {
		FFFC_LOG_LEVEL = FFFC_LOG_ERROR;
		fffc_print_green("Using user-provided log level ERROR");
	} else if (strcmp(log_level_str, "NONE") == 0) {
		FFFC_LOG_LEVEL = FFFC_LOG_NONE;
		fffc_print_green("Using user-provided log level NONE");
	} else {
		fffc_print_red("Invalid value for FFFC_LOG_LEVEL; options are DEBUG, INFO, WARN, ERROR, or NONE.");
	}
	return 0;
}

static
int override_mutation_rate(void) {
	char *mutation_rate_str = getenv("FFFC_MUTATION_RATE");
	if (!mutation_rate_str) {
		fffc_print_green("Using default mutation rate; set via FFFC_MUTATION_RATE environment variable.");
		return 0;
	} else if (strcmp(mutation_rate_str, "NONE") == 0) {
		FFFC_SKIP_RATE = 0.0;
		fffc_print_green("Using user-provided mutation rate NONE");
	} else if (strcmp(mutation_rate_str, "SOME") == 0) {
		FFFC_SKIP_RATE = 1.0/512.0;
		fffc_print_green("Using user-provided mutation rate SOME");
	} else if (strcmp(mutation_rate_str, "LOTS") == 0) {
		FFFC_SKIP_RATE = 1.0/4.0;
		fffc_print_green("Using user-provided mutation rate LOTS");
	} else {
		fffc_print_red("Invalid value for FFFC_MUTATION_RATE; options are NONE, SOME, or LOTS.");
	}
	return 0;
}

static int override_resize_rate(void) {
	char *resize_rate_str = getenv("FFFC_RESIZE_RATE");
	if (!resize_rate_str) {
		fffc_print_green("Using default resize rate; set via FFFC_RESIZE_RATE environment variable.");
		return 0;
	} else if (strcmp(resize_rate_str, "NONE") == 0) {
		FFFC_RESIZE_PASS_MASK = 0xFF;
		FFFC_RESIZE_POINTER_MASK = 0x00;
		fffc_print_green("Using user-provided resize rate NONE");
	} else if (strcmp(resize_rate_str, "SOME") == 0) {
		FFFC_RESIZE_PASS_MASK = 0x0F;
		FFFC_RESIZE_POINTER_MASK = 0x03;
		fffc_print_green("Using user-provided resize rate SOME");
	} else if (strcmp(resize_rate_str, "LOTS") == 0) {
		FFFC_RESIZE_PASS_MASK = 0x01;
		FFFC_RESIZE_POINTER_MASK = 0x03;
		fffc_print_green("Using user-provided resize rate LOTS");
	} else {
		fffc_print_red("Invalid value for FFFC_RESIZE_RATE; options are NONE, SOME, or LOTS.");
	}
	return 0;
}

static int override_state_size(void) {
	char *state_size_str = getenv("FFFC_MAX_STATE_COUNT");
	if (!state_size_str) {
		fffc_print_green("Using default state size; set via the FFFC_MAX_STATE_COUNT environment variable");
		return 0;
	}
	errno = 0;
	int count = strtol(state_size_str, NULL, 10);
	if ((!count && errno) || (count < -1)) {
		fffc_print_red("Invalid value for FFFC_MAX_STATE_COUNT; please put a positive integer.");
	} else {
		fffc_print_int_green("Using user-provided max state count", count);
	}
	FFFC_MAX_STATE_COUNT = count;
}

static int override_mutation_count(void) {
	char *mutation_count_str = getenv("FFFC_MUTATION_COUNT");
	if (!mutation_count_str) {
		fffc_print_green("Using default mutation count; set via FFFC_MUTATION_COUNT environment variable.");
		return 0;
	}
	errno = 0;
	int count = strtol(mutation_count_str, NULL, 10);
	if ((!count && errno) || (count < -1)) {
		fffc_print_red("Invalid value for FFFC_MUTATION_COUNT; please put a positive integer or -1 to run forever.");
	} else {
		fffc_print_int_green("Using user-provided mutation count", count);
	}
	FFFC_FORK_COUNT = count;
	return 0;
}

static int override_generation_count(void) {
	char *generation_count_str = getenv("FFFC_GENERATION_COUNT");
	if (!generation_count_str) {
		fffc_print_green("Using default generation count; set via FFFC_GENERATION_COUNT environment variable.");
		return 0;
	}
	errno = 0;
	int count = strtol(generation_count_str, NULL, 10);
	if ((!count && errno) || (count < -1)) {
		fffc_print_red("Invalid value for FFFC_GENERATION_COUNT; please put a positive integer or -1 to run forever.");
	} else {
		fffc_print_int_green("Using user-provided generation count", count);
	}
	FFFC_GENERATION_COUNT = count;
	return 0;
}

static
int fuzz_forever(void) {
	return FFFC_FORK_COUNT == -1;
}

int fffc_keep_generating(void) {
	if (fffc_debug()) {
		return FFFC_GLOBAL_STATE.generation_count++ == 0;
	}
	FFFC_GLOBAL_STATE.generation_count += 1;
	if (FFFC_GLOBAL_STATE.generation_count <= FFFC_GENERATION_COUNT) {
		return 1;
	}
	return fuzz_forever();
}

int fffc_keep_mutating(void) {
	if (fffc_debug()) {
		return FFFC_WORKER_STATE.exec_count++ == 0;
	}
	FFFC_WORKER_STATE.exec_count += 1;
	if (FFFC_WORKER_STATE.excessive_crashes) {
		return 0;
	}
	if (FFFC_WORKER_STATE.exec_count <= FFFC_FORK_COUNT) {
		return 1;
	}
	return 0;
}

static
int get_timestamp(char *out, int out_len) {
	time_t current_time = time(NULL);
    struct tm *timestamp = localtime(&current_time);
    strftime(out, out_len, "%c", timestamp);
    for (int i=0; (i < out_len) && out[i]; i++) {
    	if (out[i] == ' ') {
    		out[i] = '_';
    	}
    }
    return 0;
}

int fffc_setup_global_state(char *target_name, void *stack_start) {
	override_log_level();
	override_resize_rate();
	override_mutation_rate();
	override_mutation_count();
	override_generation_count();
	override_crash_path();
	override_working_path();
	override_state_size();
	override_debug_replay_path();
	override_tracing();

	char timestamp[FFFC_TIMESTAMP_LENGTH];
	get_timestamp(timestamp, FFFC_TIMESTAMP_LENGTH);
	long long unsigned len = snprintf(NULL, 0, GLOBAL_STATE_FORMAT, FFFC_DATA_PATH, target_name, timestamp) + 1;
	snprintf(FFFC_GLOBAL_STATE.global_state_path, len, GLOBAL_STATE_FORMAT, FFFC_DATA_PATH, target_name, timestamp);
	if (!mkdtemp(FFFC_GLOBAL_STATE.global_state_path)) {
		fffc_print_red("Couldn't create global state directory");
		fffc_print_int_red(FFFC_GLOBAL_STATE.global_state_path, errno);
		return -1;
	}
	len = snprintf(NULL, 0, GLOBAL_CRASH_FORMAT, FFFC_CRASH_PATH, target_name, timestamp) + 1;
	snprintf(FFFC_GLOBAL_STATE.global_crash_path, len, GLOBAL_CRASH_FORMAT, FFFC_CRASH_PATH, target_name, timestamp);
	if (!mkdtemp(FFFC_GLOBAL_STATE.global_crash_path)) {
		fffc_print_red("Couldn't create global crash directory");
		fffc_print_int_red(FFFC_GLOBAL_STATE.global_crash_path, errno);
		return -1;
	}

	FFFC_GLOBAL_STATE.stack_start = stack_start;
	return 0;
}

int fffc_cleanup_global_state(void) {
	return 0;
}

int fffc_setup_call_state(void) {
	long long unsigned len = snprintf(	NULL,
										0,
										CALL_STATE_FORMAT,
										FFFC_GLOBAL_STATE.global_state_path,
										FFFC_GLOBAL_STATE.call_count) + 1;
	len = snprintf(	FFFC_CALL_STATE.call_state_path,
					len,
					CALL_STATE_FORMAT,
					FFFC_GLOBAL_STATE.global_state_path,
					FFFC_GLOBAL_STATE.call_count);
	if (len == FFFC_MAX_PATH_LENGTH) {
		fffc_print_red("Couldn't setup call state");
		fffc_exit();
	}
	if (!mkdtemp(FFFC_CALL_STATE.call_state_path)) {
		return -1;
	}

	return 0;
}

int fffc_cleanup_call_state(void) {
	return 0;
}

static
unsigned long get_time_micro() {
	struct timeval now;
	gettimeofday(&now, NULL);
	return (1000000 * now.tv_sec) + now.tv_usec;
}

int fffc_setup_generation_state(void) {
	FFFC_GENERATION_STATE.start_time = get_time_micro();

	bzero(FFFC_GENERATION_STATE.parents_path, FFFC_MAX_PATH_LENGTH);
	strcat(FFFC_GENERATION_STATE.parents_path, FFFC_CALL_STATE.call_state_path);
	strcat(FFFC_GENERATION_STATE.parents_path, PARENT_STATE_SUFFIX);
	FFFC_GENERATION_STATE.parents_fd = open(FFFC_GENERATION_STATE.parents_path, O_CREAT | O_RDWR | O_APPEND, 0644);
	if (FFFC_GENERATION_STATE.parents_fd < 0) {
		fffc_print_red("Couldn't open the parents file");
		fffc_print_red(FFFC_GENERATION_STATE.parents_path);
		fffc_print_int_red(strerror(errno), errno);
		fffc_print_int_red("fd", FFFC_GENERATION_STATE.parents_fd);
		return -1;
	}

	bzero(FFFC_GENERATION_STATE.features_path, FFFC_MAX_PATH_LENGTH);
	strcat(FFFC_GENERATION_STATE.features_path, FFFC_CALL_STATE.call_state_path);
	strcat(FFFC_GENERATION_STATE.features_path, FEATURES_STATE_SUFFIX);
	FFFC_GENERATION_STATE.features_read_fd = open(FFFC_GENERATION_STATE.features_path, O_CREAT | O_RDWR | O_APPEND, 0644);
	if (FFFC_GENERATION_STATE.features_read_fd < 0) {
		fffc_print_red("Couldn't open the features file for reading");
		fffc_print_red(strerror(errno));
		return -1;
	}
	FFFC_GENERATION_STATE.features_write_fd = open(FFFC_GENERATION_STATE.features_path, O_RDWR | O_APPEND);
	if (FFFC_GENERATION_STATE.features_write_fd < 0) {
		fffc_print_red("Couldn't open the features file for writing");
		fffc_print_red(strerror(errno));
		return -1;
	}

	return 0;
}

int fffc_cleanup_generation_state(void) {
	reap();
	close(FFFC_GENERATION_STATE.parents_fd);
	close(FFFC_GENERATION_STATE.features_read_fd);
	close(FFFC_GENERATION_STATE.features_write_fd);
	double elapsed = (get_time_micro() - FFFC_GENERATION_STATE.start_time) / 1e6;
	if (FFFC_LOG_LEVEL >= FFFC_LOG_INFO) {
		printf(TEXT_GREEN("Executions per second: %.0f\n"), (FFFC_FORK_COUNT * FFFC_PARALLEL_COUNT) / elapsed);
	}
	return 0;
}

int fffc_setup_worker_state(int worker_number) {
	FFFC_WORKER_STATE.worker_number = worker_number;
	return 0;
}

int fffc_cleanup_worker_state(void) {
	return 0;
}

int fffc_set_mode_count_mutations(void) {
	FFFC_WORKER_STATE.mode = FFFC_MODE_COUNT;
	return 0;
}

int fffc_get_mode_count_mutations() {
	if (FFFC_WORKER_STATE.mode == FFFC_MODE_COUNT) {
		return 1;
	}
	return 0;
}

int fffc_set_mode_resize(void) {
	FFFC_WORKER_STATE.mode = FFFC_MODE_RESIZE;
	return 0;
}

int fffc_get_mode_resize(void) {
	if (FFFC_WORKER_STATE.mode == FFFC_MODE_RESIZE) {
		return 1;
	}
	return 0;
}

int fffc_set_mode_iterative(void) {
	FFFC_WORKER_STATE.mode = FFFC_MODE_ITER;
	return 0;
}

int fffc_get_mode_iterative() {
	if (FFFC_WORKER_STATE.mode == FFFC_MODE_ITER) {
		return 1;
	}
	return 0;
}

int fffc_set_mode_random(void) {
	FFFC_WORKER_STATE.mode = FFFC_MODE_RAND;
	return 0;
}

int fffc_get_mode_random() {
	if (FFFC_WORKER_STATE.mode == FFFC_MODE_RAND) {
		return 1;
	}
	return 0;
}

int fffc_set_mode_normal(void) {
	FFFC_WORKER_STATE.mode = FFFC_MODE_NORMAL;
	return 0;
}

int fffc_get_mode_normal() {
	if (FFFC_WORKER_STATE.mode == FFFC_MODE_NORMAL) {
		return 1;
	}
	return 0;
}

int fffc_set_mode_smart_skip(void) {
	FFFC_WORKER_STATE.mode = FFFC_MODE_SMART_SKIP;
	return 0;
}

int fffc_get_mode_smart_skip() {
	if (FFFC_WORKER_STATE.mode == FFFC_MODE_SMART_SKIP) {
		return 1;
	}
	return 0;
}

int fffc_save_mode(void) {
	FFFC_PREVIOUS_MODE = FFFC_WORKER_STATE.mode;
	return 0;
}

int fffc_restore_mode(void) {
	FFFC_WORKER_STATE.mode = FFFC_PREVIOUS_MODE;
	return 0;
}

int fffc_time_to_resize(void) {
	if (FFFC_RESIZE_PASS_MASK == 0x0) {
		return 0;
	}
	return ((fffc_get_random() & FFFC_RESIZE_PASS_MASK) == 0);
}

static int time_to_resize_pointer(void) {
	if (!FFFC_RESIZE_POINTER_MASK) {
		return 0;
	}
	return (fffc_get_random() & FFFC_RESIZE_POINTER_MASK) == 0;
}

long long int fffc_maybe_munge_pointer(unsigned char **ptr, long long int original_size, long long int stride) {
	// If we aren't in resize mode, bail.
	if (!fffc_get_mode_resize()) {
		return original_size;
	}
	// First, let's check and make sure this isn't titanic-- anything over 128MB we don't try to copy.
	// Same with anything under the stride size.
	if (original_size >= (128 << 20)) {
		return original_size;
	} else if (original_size < stride) {
		return original_size;
	}
	// Check the RNG for whether we should do anything
	if (time_to_resize_pointer()) {
		// Next, we're going to randomly select a new (nonzero) number of elements
		int original_number_of_elements = original_size / stride;
		int new_number_of_elements = (fffc_get_random() % (2 * original_number_of_elements)) + 1;
		int new_starting_element = fffc_get_random() % original_number_of_elements;
		int first_chunk_element_count = MIN(original_number_of_elements - new_starting_element, new_number_of_elements);
		int second_chunk_element_count = new_number_of_elements - first_chunk_element_count;
		// All of these are basically just the above * the element size
		int new_size = new_number_of_elements * stride;
		void *starting_point = *ptr + (new_starting_element * stride);
		int first_chunk_size = first_chunk_element_count * stride;
		int second_chunk_size = second_chunk_element_count * stride;
		// Now we allocate the new area
		unsigned char *alloc = malloc(new_size);
		fffc_log_allocate(alloc, new_size);
		// And populate it.
		memcpy(alloc, starting_point, first_chunk_size);
		fffc_log_copy(alloc, starting_point, first_chunk_size);
		memcpy(alloc + first_chunk_size, *ptr, second_chunk_size);
		fffc_log_copy(alloc + first_chunk_size, *ptr, second_chunk_size);
		// Now write back to the original pointer
		*ptr = alloc;
		fffc_log_write(ptr, sizeof(ptr));
		// Update the lfu entry
		fffc_lfu_insert(*ptr, new_size);
		// And return the new size
		return new_size;
	}
	return original_size;
}

int fffc_pick_one_or_none(int options_count) {
	// Don't mutate at all if we're just counting bytes or resizing
	if (fffc_get_mode_resize()) {
		FFFC_WORKER_STATE.mutation_counter -= options_count;
		return -1;
	}
	if (fffc_get_mode_count_mutations()) {
		FFFC_WORKER_STATE.mutation_counter += options_count;
		return -1;
	}
	// Don't mutate if we're in an iterative mode and this isn't our target
	if (fffc_get_mode_iterative() || fffc_get_mode_normal()) {
		// If we're before the chosen one, decrement the count and continue
		if (FFFC_WORKER_STATE.mutation_counter > options_count) {
			FFFC_WORKER_STATE.mutation_counter -= options_count;
			// Normal mode may *also* mutate randomly, so don't bail there
			if (!fffc_get_mode_normal()) {
				return -1;
			}
		// If we are the chosen one, set the count to zero and mutate
		} else if (FFFC_WORKER_STATE.mutation_counter > 0) {
			FFFC_WORKER_STATE.mutation_counter = 0;
			return fffc_get_random() % options_count;
		// If we're after the chosen one and not normal, just keep going
		} else if (!fffc_get_mode_normal()) {
			return -1;
		}
	}
	int total_options = (1.0 + FFFC_SKIP_RATE) * options_count;
	int skip_options = total_options - options_count;
	int random_choice = fffc_get_random() % total_options;
	return random_choice - skip_options;
}

int fffc_getpid(void) {
	return getpid();
}

void fffc_kill(int pid) {
	kill(pid, SIGKILL);
}

void fffc_usleep(int duration) {
	usleep(duration);
}

static
unsigned long long get_exec_count(void) {
	return FFFC_WORKER_STATE.exec_count;
}

int fffc_inc_exec_count(void) {
	FFFC_WORKER_STATE.exec_count++;
	return 0;
}

static
unsigned long long get_crash_count(void) {
	return FFFC_WORKER_STATE.crash_count;
}

int fffc_inc_crash_count(void) {
	FFFC_WORKER_STATE.crash_count++;
	return 0;
}

int fffc_inc_call_count(void) {
	FFFC_GLOBAL_STATE.call_count++;
	return 0;
}

int fffc_check_for_excessive_crashes(void) {
	long double crashes = get_crash_count();
	long double execs = get_exec_count();

	if ((execs > 10) && ((crashes / execs) > 0.25)) {
		fffc_print_red("Excessive crashes detected, bailing");
		FFFC_WORKER_STATE.excessive_crashes = 1;
	} else if ((execs > 100) && ((crashes / execs) > 0.1)) {
		fffc_print_red("Excessive crashes detected, bailing");
		FFFC_WORKER_STATE.excessive_crashes = 1;
	} else if ((execs > 1000) && ((crashes / execs) > 0.05)) {
		fffc_print_red("Excessive crashes detected, bailing");
		FFFC_WORKER_STATE.excessive_crashes = 1;
	}
	return 0;
}

static
int copy_file(char *src, char *dst) {
	int src_fd = open(src, O_RDONLY);
	int dst_fd = open(dst, O_WRONLY);
    struct stat size_holder;
    fstat(src_fd, &size_holder);
    sendfile(dst_fd, src_fd, 0, size_holder.st_size);
    close(src_fd);
    close(dst_fd);
    return 0;
}

static
// XXX This is a really dangerous function.
// XXX It assumes that dest is null terminated
// XXX It assumes that dest is long enough to hold src
// XXX It assumes that src is null terminated
// XXX It assumes that dest is large enough to hold joinder
// XXX It assumes that dest is already at the null pointer
// XXX no bueno
int append_str(char **dest, char *src, char joinder) {
	if (joinder) {
		**dest = joinder;
		(*dest)++;
	}
	while (*src) {
		char c = *src;
		src++;
		**dest = c;
		(*dest)++;
	}
	return 0;
}

static
int ull_to_str(char *dest, unsigned long long src) {
	char *buf = dest;
	int written = 0;

	// Phase 1: compute the digits and write them to the str
	while (src) {
		int digit = src % 10;
		src = src/10;
		*(buf++) = '0' + digit;
		written++;
	}

	// Phase 2: reverse the digits
	char *begin = dest;
	char *end = buf - 1;
	for (int i=0; i <= (written >> 1); i++) {
		char tmp = *begin;
		*begin = *end;
		*end = tmp;
		*begin++;
		*end--;
	}

	return 0;
}

static
int append_llu(char **dest, unsigned long long src, char joinder) {
	char srcbuf[256];
	bzero(srcbuf, 256);
	ull_to_str(srcbuf, src);
	append_str(dest, srcbuf, joinder);
	return 0;
}

int fffc_setup_mutation_state(char *target_name) {
	// Get the dirname
	unsigned long long previous_gens = FFFC_GLOBAL_STATE.generation_count * FFFC_FORK_COUNT * FFFC_PARALLEL_COUNT;
	unsigned long long this_gen = FFFC_WORKER_STATE.worker_number * FFFC_FORK_COUNT;
	unsigned long long iter = FFFC_WORKER_STATE.exec_count + previous_gens + this_gen;
	char *fmt = MUTATION_STATE_FORMAT;

	// Format the directory name
	char *path = FFFC_WORKER_STATE.mutation_state_path;
	bzero(path, FFFC_MAX_PATH_LENGTH);
	append_str(&path, FFFC_CALL_STATE.call_state_path, 0);
	append_str(&path, target_name, '/');
	append_llu(&path, iter, '-');

	// Make the directory
	if (mkdir(FFFC_WORKER_STATE.mutation_state_path, 0755) < 0) {
		if (FFFC_LOG_LEVEL >= FFFC_LOG_ERROR) {
			printf(TEXT_RED("Couldn't create data directory: %s\n"), strerror(errno));
		}
		return -1;
	}

	// Make the logfile
	bzero(FFFC_MUTATION_STATE.log_path, FFFC_MAX_PATH_LENGTH);
	strcat(FFFC_MUTATION_STATE.log_path, FFFC_WORKER_STATE.mutation_state_path);
	strcat(FFFC_MUTATION_STATE.log_path, LOG_STATE_SUFFIX);
	FFFC_MUTATION_STATE.log_fd = open(FFFC_MUTATION_STATE.log_path, O_CREAT | O_RDWR, 0644);
	if (FFFC_MUTATION_STATE.log_fd < 0) {
		if (FFFC_LOG_LEVEL >= FFFC_LOG_ERROR) {
			printf(TEXT_RED("Couldn't create logfile: %s\n"), strerror(errno));
		}
		return -1;
	}

	// Copy over a random logfile
	if (FFFC_GLOBAL_STATE.generation_count > 1) {
		char parent[FFFC_MAX_PATH_LENGTH];
		bzero(parent, FFFC_MAX_PATH_LENGTH);
		int found = 0;
		for (int i=0; i < FFFC_PARENT_RETRY; i++) {
			long parent_index = rand() % FFFC_MAX_STATE_COUNT;
			long parent_offset = parent_index * FFFC_MAX_PATH_LENGTH;
			if (pread(FFFC_GENERATION_STATE.parents_fd, parent, FFFC_MAX_PATH_LENGTH, parent_offset) < FFFC_MAX_PATH_LENGTH) {
				continue;
			}
			found = 1;
			strcat(parent, LOG_STATE_SUFFIX);
			copy_file(parent, FFFC_MUTATION_STATE.log_path);
			break;
		}
		if (!found) {
			fffc_print_yellow("Couldn't get parent log; this is probably due to excessive crashes.");
			fffc_log_begin();
		}
	} else {
		fffc_log_begin();
	}

	// Make the coverage directory
	bzero(FFFC_MUTATION_STATE.coverage_path, FFFC_MAX_PATH_LENGTH);
	strcat(FFFC_MUTATION_STATE.coverage_path, FFFC_WORKER_STATE.mutation_state_path);
	strcat(FFFC_MUTATION_STATE.coverage_path, COVERAGE_STATE_SUFFIX);
	if (mkdir(FFFC_MUTATION_STATE.coverage_path, 0755) < 0) {
		if (FFFC_LOG_LEVEL >= FFFC_LOG_ERROR) {
			printf(TEXT_RED("Couldn't create coverage directory: %s -> %s\n"), FFFC_MUTATION_STATE.coverage_path, strerror(errno));
		}
		return -1;
	}
	setenv("GCOV_PREFIX", FFFC_MUTATION_STATE.coverage_path, 1);
	setenv("GCOV_PREFIX_STRIP", "100", 1);

	// Get the crash filename
	bzero(FFFC_MUTATION_STATE.crash_path, FFFC_MAX_PATH_LENGTH);
	strcat(FFFC_MUTATION_STATE.crash_path, FFFC_WORKER_STATE.mutation_state_path);
	strcat(FFFC_MUTATION_STATE.crash_path, CRASH_STATE_SUFFIX);
	__sanitizer_set_report_path(FFFC_MUTATION_STATE.crash_path);

	// Redirect stdout
	if (!fffc_debug()) {
		char stdout_path[FFFC_MAX_PATH_LENGTH];
		bzero(stdout_path, FFFC_MAX_PATH_LENGTH);
		strcat(stdout_path, FFFC_WORKER_STATE.mutation_state_path);
		strcat(stdout_path, STDOUT_SUFFIX);
		int stdout_fd = open(stdout_path, O_CREAT | O_RDWR, 0644);
		if (stdout_fd < 0) {
			fffc_print_red("Unable to create standard output file.");
			return -1;
		}
		if (dup2(stdout_fd, STDOUT_FILENO) < 0) {
			fffc_print_red("Unable to redirect stdout");
			return -1;
		}
		if (close(stdout_fd) < 0) {
			fffc_print_red("Unable to close redirected stdout");
			return -1;
		}

		// ... and stderr
		char stderr_path[FFFC_MAX_PATH_LENGTH];
		bzero(stderr_path, FFFC_MAX_PATH_LENGTH);
		strcat(stderr_path, FFFC_WORKER_STATE.mutation_state_path);
		strcat(stderr_path, STDERR_SUFFIX);
		int stderr_fd = open(stderr_path, O_CREAT | O_RDWR, 0644);
		if (stderr_fd < 0) {
			fffc_print_red("Unable to create standard error file.");
			return -1;
		}
		if (dup2(stderr_fd, STDERR_FILENO) < 0) {
			fffc_print_red("Unable to redirect stderr");
			return -1;
		}
		if (close(stderr_fd) < 0) {
			fffc_print_red("Unable to close redirected stderr");
			return -1;
		}
	}

	return 0;
}

int fffc_print_pointer(void *p) {
	printf("p: %p\n", p);
	return 0;
}

void fffc_print_ul_red(char *msg, long unsigned ul) {
	if (FFFC_LOG_LEVEL >= FFFC_LOG_DEBUG) {
		printf(TEXT_RED("%s: %lu\n"), msg, ul);
	}
}

static
int move_to_crashes() {
	char new_crash_dir[FFFC_MAX_PATH_LENGTH];
	bzero(new_crash_dir, FFFC_MAX_PATH_LENGTH);
	strcat(new_crash_dir, FFFC_GLOBAL_STATE.global_crash_path);
	strcat(new_crash_dir, SAVED_CRASH_DIR_SUFFIX);
	if (!mkdtemp(new_crash_dir)) {
		fffc_print_red("Couldn't create saved crash directory");
		fffc_print_red(strerror(errno));
		fffc_print_red(new_crash_dir);
		return -1;
	}
	if (rename(FFFC_WORKER_STATE.mutation_state_path, new_crash_dir)) {
		fffc_print_red("Rename failed");
		fffc_print_red(strerror(errno));
		return -1;
	}
	return 0;	
}

static
int move_to_parents() {
	errno = 0;
	int written = write(FFFC_GENERATION_STATE.parents_fd, FFFC_WORKER_STATE.mutation_state_path, FFFC_MAX_PATH_LENGTH);
	if (written != FFFC_MAX_PATH_LENGTH) {
		fffc_print_int_red("Didn't write whole path", written);
	}
	if (errno) {
		fffc_print_int_red(strerror(errno), errno);
	}
	return 0;
}

int fffc_cleanup_mutation_state(int crashed) {
	close(FFFC_MUTATION_STATE.log_fd);
	if (crashed) {
		move_to_crashes();
	} else {
		move_to_parents();
	}
	return 0;
}

struct FFFC_log_event {
	long long unsigned version;
	long long unsigned event_type;
	long long unsigned location;
	long long unsigned length;
	union {
		char value[16];
		struct {
			long unsigned call_count;
			void *stack_start;
		};
	};
};

static int FFFC_EVENT_TYPE_ALLOCATE = 0;
static int FFFC_EVENT_TYPE_BEGIN = 1;
static int FFFC_EVENT_TYPE_COPY = 2;
static int FFFC_EVENT_TYPE_WRITE = 3;

static
int build_begin_event(unsigned long long call_count, struct FFFC_log_event *event) {
	memset(event, 0, sizeof(struct FFFC_log_event));
	event->version = 0;
	event->event_type = FFFC_EVENT_TYPE_BEGIN;
	event->call_count = call_count;
	event->stack_start = FFFC_GLOBAL_STATE.stack_start;
	return 0;
}

static
int build_copy_event(void *dest, void *src, long long unsigned size, struct FFFC_log_event *event) {
	memset(event, 0, sizeof(struct FFFC_log_event));
	event->version = 0;
	event->event_type = FFFC_EVENT_TYPE_COPY;
	event->length = size;
	void **data = (void**)event->value;
	data[0] = src;
	data[1] = dest;
	return 0;
}

static
int build_write_event(void *new, unsigned long long length, struct FFFC_log_event *event) {
	if (length > 16) {
		fffc_print_int_red("Cannot log event of size", length);
		return -1;
	}
	memset(event, 0, sizeof(struct FFFC_log_event));
	event->version = 0;
	event->event_type = FFFC_EVENT_TYPE_WRITE;
	event->location = (long long unsigned) new;
	event->length = length;
	memcpy(event->value, new, length);
	return 0;
}

static
int build_allocate_event(void *location, unsigned long long length, struct FFFC_log_event *event) {
	memset(event, 0, sizeof(struct FFFC_log_event));
	event->version = 0;
	event->event_type = FFFC_EVENT_TYPE_ALLOCATE;
	event->location = (long long unsigned) location;
	event->length = length;
	return 0;
}

static
int write_event_to_log(struct FFFC_log_event *event) {
	lseek64(FFFC_MUTATION_STATE.log_fd, 0, SEEK_END);
	int written = write(FFFC_MUTATION_STATE.log_fd, (void*)event, sizeof(struct FFFC_log_event));
	if (written < sizeof(struct FFFC_log_event)) {
		fffc_print_yellow("Warning: unable to write events to log, corruption may result.");
	}
	return 0;
}

int fffc_log_allocate(void *location, unsigned long long length) {
	struct FFFC_log_event event;
	build_allocate_event(location, length, &event);
	write_event_to_log(&event);
	return 0;
}

int fffc_log_begin(void) {
	struct FFFC_log_event event;
	build_begin_event(FFFC_GLOBAL_STATE.call_count, &event);
	write_event_to_log(&event);
	return 0;
}

int fffc_log_copy(void *dest, void *src, long long unsigned size) {
	struct FFFC_log_event event;
	build_copy_event(dest, src, size, &event);
	write_event_to_log(&event);
	return 0;
}

int fffc_log_write(void *new, unsigned long long length) {
	struct FFFC_log_event event;
	build_write_event(new, length, &event);
	write_event_to_log(&event);
	return 0;
}

static
int replay_allocate_event(struct FFFC_log_event *event) {
	// validate what we can
	if (event->version != 0) {
		fffc_print_int_red("Cannot replay events from version", event->version);
		return -1;
	}
	if (event->event_type != FFFC_EVENT_TYPE_ALLOCATE) {
		fffc_print_int_red("Cannot replay non-allocate events", event->event_type);
		return -1;
	}
	// go for it
	void *allocation = malloc(event->length);
	return 0;
}

static
int replay_begin_event(struct FFFC_log_event *event) {
	// validate what we can
	if (event->version != 0) {
		fffc_print_int_red("Cannot replay events from version", event->version);
		return -1;
	}
	if (event->event_type != FFFC_EVENT_TYPE_BEGIN) {
		fffc_print_int_red("Cannot replay non-begin events", event->event_type);
		return -1;
	}
	if (event->call_count != FFFC_GLOBAL_STATE.call_count) {
		if (fffc_debug()) {
			fffc_print_int_yellow("Not at the right execution count for replay", event->call_count);
			fffc_print_int_yellow("Correct execution count would be", FFFC_GLOBAL_STATE.call_count);
		}
		return -1;
	}
	if ((unsigned long)event->stack_start != (unsigned long)FFFC_GLOBAL_STATE.stack_start) {
		fffc_print_red("Stack is not at the right logation for replay:");
		fffc_print_pointer(FFFC_GLOBAL_STATE.stack_start);
		fffc_print_red("Correct location would be:");
		fffc_print_pointer(event->stack_start);
		return -1;
	}
	return 0;
}

static
int replay_copy_event(struct FFFC_log_event *event) {
	// validate what we can
	if (event->version != 0) {
		fffc_print_int_red("Cannot replay events from version", event->version);
		return -1;
	}
	if (event->event_type != FFFC_EVENT_TYPE_COPY) {
		fffc_print_int_red("Cannot replay non-copy events", event->event_type);
		return -1;
	}
	void **data = (void**)event->value;
	memcpy(data[1], data[0], event->length);
	return 0;
}

static
int replay_write_event(struct FFFC_log_event *event) {
	// validate what we can
	if (event->version != 0) {
		fffc_print_int_red("Cannot replay events from version", event->version);
		return -1;
	}
	if (event->event_type != FFFC_EVENT_TYPE_WRITE) {
		fffc_print_int_red("Cannot replay non-write events", event->event_type);
		return -1;
	}
	// location can be freaking anywhere
	if (event->length > 16) {
		fffc_print_int_red("Cannot write events of length", event->length);
		return -1;
	}
	// We got here, let's party
	memcpy((void*)event->location, event->value, event->length);
	return 0;
}

static
int replay_event(struct FFFC_log_event *event) {
	if (event->event_type == FFFC_EVENT_TYPE_ALLOCATE) {
		return replay_allocate_event(event);
	}
	if (event->event_type == FFFC_EVENT_TYPE_BEGIN) {
		return replay_begin_event(event);
	}
	if (event->event_type == FFFC_EVENT_TYPE_COPY) {
		return replay_copy_event(event);
	}
	if (event->event_type == FFFC_EVENT_TYPE_WRITE) {
		return replay_write_event(event);
	}
	fffc_print_int_red("Got invalid event type", event->event_type);
	return -1;
}

static
int read_event_from_log(int fd, struct FFFC_log_event *event) {
	int keep_going = read(fd, (void*)event, sizeof(struct FFFC_log_event));
	return (keep_going == sizeof(struct FFFC_log_event));
}

static
int replay_log(int fd) {
	struct FFFC_log_event current_event;
	lseek64(fd, 0, SEEK_SET);
	int keep_going = read_event_from_log(fd, &current_event);
	int event_count = 0;
	while (keep_going) {
		if (replay_event(&current_event)) {
			if (!fffc_debug()) {
				fffc_print_int_red("Broke replaying event number", event_count);
			}
			return -1;
		}
		keep_going = read_event_from_log(fd, &current_event);
		event_count++;
	}
	return 0;
}

int fffc_replay_log(void) {
	if (fffc_debug()) {
		return fffc_replay_debug_log();
	}
	return replay_log(FFFC_MUTATION_STATE.log_fd);
}

int fffc_replay_debug_log(void) {
	int fd = open(FFFC_DEBUG_REPLAY, O_RDONLY);
	if (fd < 0) {
		fffc_print_red("Couldn't open specified log");
		exit(-1);
	}
	int retval = replay_log(fd);
	close(fd);
	FFFC_WORKER_STATE.break_now = 1;
	return retval;
}

int fffc_check_log_call_matches(void) {
	int fd = open(FFFC_DEBUG_REPLAY, O_RDONLY);
	if (fd < 0) {
		fffc_print_red("Couldn't open specified log");
		exit(-1);
	}
	struct FFFC_log_event event;
	read_event_from_log(fd, &event);
	int retval = replay_begin_event(&event);
	close(fd);
	return retval;
}

extern char **environ;

static
int get_env_size(long unsigned *pointer_size, long unsigned *data_size) {
	char **e = environ;
	while (*e) {
		*pointer_size += sizeof(*e);
		*data_size += strlen(*e) + 1;
		fffc_print_yellow(*e);
		e++;
	}
	*pointer_size += sizeof(*e);

	return 0;
}

int fffc_get_env(void) {
	long unsigned ps = 0;
	long unsigned ds = 0;
	get_env_size(&ps, &ds);
	fffc_print_ul_red("Pointer size", ps);
	fffc_print_ul_red("Data size", ds);
	return 0;
}

int fffc_check_aslr(void) {
	int pers = personality(0xFFFFFFFF);
    int aslr_is_on = (pers & ADDR_NO_RANDOMIZE) != ADDR_NO_RANDOMIZE;
    if (aslr_is_on) {
        fffc_print_red("ASLR is enabled; cannot continue. Please see README.md.");
        fffc_exit();
    }
    return 0;
}