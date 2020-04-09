typedef void __TARGET_TYPE__;

int fffc_mutator_for_target_type(__TARGET_TYPE__ storage) {
	long long int size = fffc_estimate_allocation_size((void*)*storage);
	unsigned long long int member_size = fffc_get_sizeof_type();
	size = fffc_maybe_munge_pointer((unsigned char**)storage, size, member_size);
	if (size < 0) {
		return 0;
	}
	void *data = (void*)*storage;
	while (size > member_size) {
		fffc_mutator_for_underlying_type(data);
		data += member_size;
		size -= member_size;
	}
	if (member_size != 1) {
		fffc_mutator_for_underlying_type(data);
	} else if (*(unsigned char*)data != 0) {
		fffc_mutator_for_underlying_type(data);	
	}

	return 0;
}