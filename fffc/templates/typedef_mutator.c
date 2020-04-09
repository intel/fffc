typedef void __TARGET_TYPE__;
typedef long long int ssize_t;
typedef unsigned long long int size_t;

static int fffc_mutator_for_target_type(__TARGET_TYPE__ tmp) {
	return fffc_mutator_for_underlying_type(tmp);
}