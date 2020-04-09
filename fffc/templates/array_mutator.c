typedef void __TARGET_TYPE__;

int fffc_mutator_for_target_type(__TARGET_TYPE__ storage) {
	unsigned long long size = sizeof(storage);
	unsigned long long nmemb = size/fffc_get_sizeof_type();
	for (unsigned long long i=0; i < nmemb; i++) {
		fffc_mutator_for_underlying_type(&(*storage)[i]);
	}
	return 0;
}