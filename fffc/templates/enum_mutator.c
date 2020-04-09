typedef void __TARGET_TYPE__;

static int fffc_mutator_for_target_type(__TARGET_TYPE__ x) {
	int values[] = {0};
	unsigned long long values_len = 0;
	unsigned long long idx = fffc_get_random() % values_len;
	*storage = values[idx];
	return 0;
}
