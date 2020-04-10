#include "fffc_runtime.h"
#include "mutator.h"
___FFFC_INFERRED_HEADER___


static char FFFC_target_name[] = "___FFFC_TARGET_NAME___";
static char FFFC_target_binary[] = "___FFFC_BINARY_PATH__";

static int FFFC_WORKER_NUMBER = 0;

___FFFC_TARGET_DECL___

static
___FFFC_PROXY_SIG___ {
	if (!fffc_debug()) {
		if (fffc_time_to_resize()) {
			fffc_save_mode();
			fffc_set_mode_resize();
			___FFFC_ARGUMENT_MUTATORS___
			fffc_restore_mode();
		}
	}
	fffc_restrict_child();
	fffc_replay_log();
	if (!fffc_debug()) {
		___FFFC_ARGUMENT_MUTATORS___
	}
	fffc_precall();
	___FFFC_CALL___
	fffc_postcall();
	fffc_exit_child();
}

static
___FFFC_WORKER_SIG___ {
	fffc_setup_worker_state(FFFC_WORKER_NUMBER);
	if (!fffc_debug()) {
		fffc_set_mode_count_mutations();
		___FFFC_ARGUMENT_MUTATORS___
		fffc_set_mode_normal();
	}

	while (fffc_keep_mutating()) {
		int crashed = 0;
		fffc_setup_mutation_state(FFFC_target_name);
		int child = fffc_fork();
		if (!child) {
			___FFFC_PROXY_CALL___
		} else {
			if (fffc_wait_for_child(child) < 0) {
				fffc_inc_crash_count();
				crashed = 1;
			}
			fffc_check_for_excessive_crashes();
		}
		fffc_cleanup_mutation_state(crashed);
	}
	fffc_cleanup_worker_state();
	fffc_exit_success();
}

static 
___FFFC_PARALLEL_SIG___ {

	// Increment the global call count
	fffc_inc_call_count();

	// Fork off the original process for the caller process pool
	int monitor_id = fffc_fork();
	fffc_remove_hook();
	if (monitor_id) {
		fffc_usleep(100);
		fffc_wait();
		___FFFC_CALL___
		fffc_install_hook();
		if (fffc_debug() && (fffc_check_log_call_matches() == 0)) {
			fffc_exit_success();
		}
		___FFFC_RETURN___
	}

	// Skip this call if we're debugging and it's wrong
	if (fffc_debug() && (fffc_check_log_call_matches() < 0)) {
		fffc_exit();	
	}

	// Set up the per-call state
	if (fffc_setup_call_state() < 0) {
		fffc_print_red("Failed to setup call state");
		fffc_exit();
	}

	// Run each of the generations
	while (fffc_keep_generating()) {
		fffc_setup_generation_state();
		int num_processes = fffc_get_parallel_count();
		for (int i=0; i < num_processes; i++) {
			FFFC_WORKER_NUMBER = i;
			int pid = fffc_fork();
			if (pid == 0) {
				___FFFC_WORKER_CALL___
				fffc_exit();
			}
		}
		fffc_wait_for_workers();
		fffc_cleanup_generation_state();
	}

	// And die
	fffc_cleanup_call_state();
	fffc_exit();
	__builtin_unreachable();
}

__attribute__((constructor))
static
void hook() {
	void *stack_start = 0;
	fffc_check_aslr();
	if (fffc_setup_global_state(FFFC_target_name, &stack_start) < 0) {
		fffc_print_red("Couldn't setup global state");
		fffc_exit();
	}
	FFFC_target = fffc_get_pointer_to_symbol(___FFFC_OFFSET__, FFFC_target_binary, ___FFFC_RECALCULATE_OFFSET___);
	fffc_setup_interceptor((void*)FFFC_target, (void*)FFFC_parallel_replacement);
}
