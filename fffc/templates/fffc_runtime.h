// Copyright (C) 2020 Intel Corporation
// SPDX-License-Identifier: MIT

#ifndef FFFC_LFU_SIZE
#define FFFC_LFU_SIZE 4096
#endif

#ifndef FFFC_QUEUE_SIZE
#define FFFC_QUEUE_SIZE 1024
#endif

#ifndef FFFC_MAX_FILE_SIZE
#define FFFC_MAX_FILE_SIZE (1L << 44)
#endif

#ifndef FFFC_ADDRESS_SPACE_SIZE
#define FFFC_ADDRESS_SPACE_SIZE (1L << 47)
#endif 

// FFFC_ADDRESS_SPACE_SIZE / FFFC_MAX_FILE_SIZE
#ifndef FFFC_STATE_FD_COUNT
#define FFFC_STATE_FD_COUNT 8
#endif

#ifndef FFFC_MAX_PATH_LENGTH
#define FFFC_MAX_PATH_LENGTH 4096
#endif

#ifndef FFFC_PAGE_SIZE
#define FFFC_PAGE_SIZE 4096
#endif

struct FFFC_lfu {
	void* region_size_lfu_addresses[FFFC_LFU_SIZE];
	long long unsigned region_size_lfu_sizes[FFFC_LFU_SIZE];
	unsigned int region_size_lfu_frequency[FFFC_LFU_SIZE];
};

struct FFFC_coverage_feature {
	long none;
	long few;
	long some;
	long many;
};

struct FFFC_score_entry {
	double score;
	char state_path[FFFC_MAX_PATH_LENGTH];
};

struct FFFC_mutation_state {
	int log_fd;
	char log_path[FFFC_MAX_PATH_LENGTH];
	char coverage_path[FFFC_MAX_PATH_LENGTH];
	char crash_path[FFFC_MAX_PATH_LENGTH];
};

struct FFFC_worker_state {
	int worker_number;
	struct FFFC_lfu lfu;
	long long unsigned mutation_counter;
	long long unsigned exec_count;
	long long unsigned crash_count;
	int excessive_crashes;
	int mode;
	int break_now;
	void *mutation_state_dir;
	char mutation_state_path[FFFC_MAX_PATH_LENGTH];
};

struct FFFC_generation_state {
	unsigned long start_time;
	int parents_fd;
	char parents_path[FFFC_MAX_PATH_LENGTH];
	int features_read_fd;
	int features_write_fd;
	char features_path[FFFC_MAX_PATH_LENGTH];
};

struct FFFC_call_state {
	void *call_state_dir;
	char call_state_path[FFFC_MAX_PATH_LENGTH]; 
};

struct FFFC_global_state {
	int call_count;
	int generation_count;
	void *global_state_dir;
	char global_state_path[FFFC_MAX_PATH_LENGTH];
	char global_crash_path[FFFC_MAX_PATH_LENGTH];
	char target_name[FFFC_MAX_PATH_LENGTH];
	void *stack_start;
};

struct FFFC_global_state FFFC_GLOBAL_STATE;
struct FFFC_call_state FFFC_CALL_STATE;
struct FFFC_generation_state FFFC_GENERATION_STATE;
struct FFFC_worker_state FFFC_WORKER_STATE;
struct FFFC_mutation_state FFFC_MUTATION_STATE;

int fffc_get_parallel_count(void);

int fffc_log_allocate(void *location, unsigned long long length);
int fffc_log_begin(void);
int fffc_log_write(void *new, unsigned long long length);
int fffc_log_copy(void *dest, void *src, long long unsigned size);
int fffc_replay_log(void);
int fffc_replay_debug_log(void);
int fffc_check_log_call_matches(void);

int fffc_keep_generating(void);
int fffc_keep_mutating(void);

int fffc_setup_global_state(char *target_name, void *stack_start);
int fffc_cleanup_global_state(void);

int fffc_setup_call_state(void);
int fffc_cleanup_call_state(void);

int fffc_setup_generation_state(void);
int fffc_cleanup_generation_state(void);

int fffc_setup_worker_state(int worker_number);
int fffc_cleanup_worker_state(void);

int fffc_setup_mutation_state(char *target_name);
int fffc_cleanup_mutation_state(int crashed);

int fffc_reap(void);

int fffc_debug(void);

int fffc_inc_exec_count(void);
int fffc_inc_call_count(void);
int fffc_inc_crash_count(void);
int fffc_check_for_excessive_crashes(void);

void fffc_usleep(int duration);

long long unsigned fffc_estimate_allocation_size(void *ptr);

void fffc_setup_interceptor(void *target, void *replacement);

void fffc_precall(void);
void fffc_postcall(void);
void fffc_remove_hook(void);
void fffc_install_hook(void);
void* fffc_get_pointer_to_symbol(long long unsigned int elf_offset, char* path, int recalculate);

int fffc_fork(void);
void fffc_exit(void);
void fffc_exit_child(void);
void fffc_exit_success(void);

void fffc_print_red(char *s);
void fffc_print_int_red(char *s, int d);
void fffc_print_ul_red(char *s, long unsigned d);
void fffc_print_yellow(char *s);
void fffc_print_int_yellow(char *s, int d);
void fffc_print_green(char *s);
void fffc_print_int_green(char *s, int d);

void fffc_print_crash(int current_iter, int crash_count);
void fffc_print_resume(void);
void fffc_print_size(long long unsigned i);
void fffc_print_backtrace(void);

int fffc_get_random(void);
int fffc_random_mask(char *region, long long unsigned int size);

int fffc_wait_for_child(int pid);
int fffc_wait_for_worker(void);
int fffc_wait(void);
int fffc_wait_for_workers(void);

void fffc_restrict_child(void);

float fffc_get_neg_inf(void);
float fffc_get_inf(void);
float fffc_get_flt_epsilon(void);
float fffc_get_flt_max(void);
float fffc_get_flt_min(void);
double fffc_get_flt_subnormal(void);
float fffc_get_snanf(void);
double fffc_get_dbl_subnormal(void);
double fffc_get_dbl_epsilon(void);
double fffc_get_dbl_max(void);
double fffc_get_dbl_min(void);
double fffc_get_snan(void);
long double fffc_get_ldbl_subnormal(void);
long double fffc_get_ldbl_epsilon(void);
long double fffc_get_ldbl_max(void);
long double fffc_get_ldbl_min(void);
long double fffc_get_snanl(void);
int fffc_get_int_max(void);
int fffc_get_int_min(void);
short fffc_get_shrt_max(void);
short fffc_get_shrt_min(void);
long fffc_get_long_max(void);
long fffc_get_long_min(void);
long long fffc_get_long_long_max(void);
long long fffc_get_long_long_min(void);
unsigned long long fffc_get_ulong_max(void);

long long int fffc_strlen(char *s, long long unsigned max_size);

int fffc_pick_one_or_none(int options_count);
long long int fffc_maybe_munge_pointer(	unsigned char **ptr,
										long long int original_size,
										long long int stride);
int fffc_time_to_resize(void);

int fffc_set_mode_count_mutations(void);
int fffc_get_mode_count_mutations(void);

int fffc_set_mode_iterative(void);
int fffc_get_mode_iterative(void);

int fffc_set_mode_random(void);
int fffc_get_mode_random(void);

int fffc_set_mode_normal(void);
int fffc_get_mode_normal(void);

int fffc_set_mode_smart_skip(void);
int fffc_get_mode_smart_skip(void);

int fffc_set_mode_resize(void);
int fffc_get_mode_resize(void);

int fffc_save_mode(void);
int fffc_restore_mode(void);

int fffc_print_malloc(void);
int fffc_print_pointer(void *p);
int fffc_get_env(void);

int fffc_check_aslr(void);