 int fffc_mutator_for_target_type(float storage) {
 	int choice = fffc_pick_one_or_none(19);
 	if (choice <= 0) {
 		return 0;
 	} else if (choice < 8) {
		fffc_random_mask((char*)storage, sizeof(float));
	} else if (choice == 8) {
		*storage += 1.0;
	} else if (choice == 9) {
		*storage -= 1.0;
	} else if (choice == 10) {
		*storage += fffc_get_flt_epsilon();
	} else if (choice == 11) {
		*storage = 0.0;
	} else if (choice == 12) {
		*storage = -1.0;
	} else if (choice == 13) {
		*storage = fffc_get_flt_max();
	} else if (choice == 14) {
		*storage = fffc_get_flt_min();
	} else if (choice == 15) {
		*storage = fffc_get_neg_inf();
	} else if (choice == 16) {
		*storage = fffc_get_inf();
	} else if (choice == 17) {
		*storage = fffc_get_snanf();
	} else if (choice == 18) {
		*storage = fffc_get_flt_subnormal();
	}
	fffc_log_write(storage, sizeof(float));
	return 0;
}

int fffc_mutator_for_target_type(double storage) {
 	int choice = fffc_pick_one_or_none(19);
  	if (choice <= 0) {
 		return 0;
 	} else if (choice < 8) {
		fffc_random_mask((char*)storage, sizeof(double));
	} else if (choice == 8) {
		*storage += 1.0;
	} else if (choice == 9) {
		*storage -= 1.0;
	} else if (choice == 10) {
		*storage += fffc_get_dbl_epsilon();
	} else if (choice == 11) {
		*storage = 0.0;
	} else if (choice == 12) {
		*storage = -1.0;
	} else if (choice == 13) {
		*storage = fffc_get_dbl_max();
	} else if (choice == 14) {
		*storage = fffc_get_dbl_min();
	} else if (choice == 15) {
		*storage = fffc_get_neg_inf();
	} else if (choice == 16) {
		*storage = fffc_get_inf();
	} else if (choice == 17) {
		*storage = fffc_get_snan();
	} else if (choice == 18) {
		*storage = fffc_get_dbl_subnormal();
	}
	fffc_log_write(storage, sizeof(double));
	return 0;
}

int fffc_mutator_for_target_type(long double storage) {
 	int choice = fffc_pick_one_or_none(19);
  	if (choice <= 0) {
 		return 0;
 	} else if (choice < 8) {
		fffc_random_mask((char*)storage, sizeof(long double));
	} else if (choice == 8) {
		*storage += 1.0;
	} else if (choice == 9) {
		*storage -= 1.0;
	} else if (choice == 10) {
		*storage += fffc_get_ldbl_epsilon();
	} else if (choice == 11) {
		*storage = 0.0;
	} else if (choice == 12) {
		*storage = -1.0;
	} else if (choice == 13) {
		*storage = fffc_get_ldbl_max();
	} else if (choice == 14) {
		*storage = fffc_get_ldbl_min();
	} else if (choice == 15) {
		*storage = fffc_get_neg_inf();
	} else if (choice == 16) {
		*storage = fffc_get_inf();
	} else if (choice == 17) {
		*storage = fffc_get_snanl();
	} else if (choice == 18) {
		*storage = fffc_get_ldbl_subnormal();
	}
	fffc_log_write(storage, sizeof(long double));
	return 0;
}

int fffc_mutator_for_target_type(char storage) {
 	int choice = fffc_pick_one_or_none(16);
  	if (choice <= 0) {
 		return 0;
 	} else if (choice < 8) {
		*storage ^= fffc_get_random() & 0xFF;
	} else if (choice == 8) {
		*storage = 0;
	} else if (choice == 9) {
		*storage = -1;
	} else if (choice == 10) {
		*storage = '&';
	} else if (choice == 11) {
		*storage = '>';
	} else if (choice == 12) {
		*storage = ',';
	} else if (choice == 13) {
		*storage = 92;
	} else if (choice == 14) {
		*storage = '	';
	} else if (choice == 15) {
		*storage = '/';
	}
	fffc_log_write(storage, sizeof(char));
	return 0;
}

int fffc_mutator_for_target_type(unsigned char storage) {
 	int choice = fffc_pick_one_or_none(16);
  	if (choice <= 0) {
 		return 0;
 	} else if (choice < 8) {
		*storage ^= fffc_get_random() & 0xFF;
	} else if (choice == 8) {
		*storage = 0;
	} else if (choice == 9) {
		*storage = -1;
	} else if (choice == 10) {
		*storage = '&';
	} else if (choice == 11) {
		*storage = '>';
	} else if (choice == 12) {
		*storage = ',';
	} else if (choice == 13) {
		*storage = 92;
	} else if (choice == 14) {
		*storage = '	';
	} else if (choice == 15) {
		*storage = '/';
	}
	fffc_log_write(storage, sizeof(char));
	return 0;
}

int fffc_mutator_for_target_type(signed char storage) {
 	int choice = fffc_pick_one_or_none(16);
  	if (choice <= 0) {
 		return 0;
 	} else if (choice < 8) {
		*storage ^= fffc_get_random() & 0xFF;
	} else if (choice == 8) {
		*storage = 0;
	} else if (choice == 9) {
		*storage = -1;
	} else if (choice == 10) {
		*storage = '&';
	} else if (choice == 11) {
		*storage = '>';
	} else if (choice == 12) {
		*storage = ',';
	} else if (choice == 13) {
		*storage = 92;
	} else if (choice == 14) {
		*storage = '	';
	} else if (choice == 15) {
		*storage = '/';
	}
	fffc_log_write(storage, sizeof(char));
	return 0;
}

int fffc_mutator_for_target_type(int storage) {
 	int choice = fffc_pick_one_or_none(16);
  	if (choice <= 0) {
 		return 0;
 	} else if (choice < 4) {
		*storage ^= fffc_get_random() & 0xFFFF;
	} else if ((choice > 0) && (choice < 8)) {
		*storage ^= fffc_get_random() & 0xFFFF0000;
	} else if (choice == 8) {
		*storage += 1;
	} else if (choice == 9) {
		*storage -= 1;
	} else if (choice == 10) {
		*storage <<= 1;
	} else if (choice == 11) {
		*storage = -*storage;
	} else if (choice == 12) {
		*storage ^= 0xFF;
	} else if (choice == 13) {
		*storage ^= 0xFF000000;
	} else if (choice == 14) {
		*storage = fffc_get_int_max();
	} else if (choice == 15) {
		*storage = fffc_get_int_min();
	}
	fffc_log_write(storage, sizeof(int));
	return 0;
}

int fffc_mutator_for_target_type(short unsigned int storage) {
 	int choice = fffc_pick_one_or_none(13);
  	if (choice <= 0) {
 		return 0;
 	} else if (choice < 4) {
		*storage ^= fffc_get_random() & 0xFFFF;
	} else if (choice == 5) {
		*storage += 1;
	} else if (choice == 6) {
		*storage -= 1;
	} else if (choice == 7) {
		*storage <<= 1;
	} else if (choice == 8) {
		*storage = -*storage;
	} else if (choice == 9) {
		*storage ^= 0xFF;
	} else if (choice == 10) {
		*storage ^= 0xFF000000;
	} else if (choice == 11) {
		*storage = fffc_get_shrt_max();
	} else if (choice == 12) {
		*storage = 0;
	}
	fffc_log_write(storage, sizeof(short unsigned int));
	return 0;
}

int fffc_mutator_for_target_type(unsigned short storage) {
 	int choice = fffc_pick_one_or_none(13);
  	if (choice <= 0) {
 		return 0;
 	} else if (choice < 4) {
		*storage ^= fffc_get_random() & 0xFFFF;
	} else if (choice == 5) {
		*storage += 1;
	} else if (choice == 6) {
		*storage -= 1;
	} else if (choice == 7) {
		*storage <<= 1;
	} else if (choice == 8) {
		*storage = -*storage;
	} else if (choice == 9) {
		*storage ^= 0xFF;
	} else if (choice == 10) {
		*storage ^= 0xFF000000;
	} else if (choice == 11) {
		*storage = fffc_get_shrt_max();
	} else if (choice == 12) {
		*storage = 0;
	}
	fffc_log_write(storage, sizeof(short unsigned int));
	return 0;
}

int fffc_mutator_for_target_type(short storage) {
	int choice = fffc_pick_one_or_none(13);
 	if (choice <= 0) {
 		return 0;
 	} else if (choice < 4) {
		*storage ^= fffc_get_random() & 0xFFFF;
	} else if (choice == 5) {
		*storage += 1;
	} else if (choice == 6) {
		*storage -= 1;
	} else if (choice == 7) {
		*storage <<= 1;
	} else if (choice == 8) {
		*storage = -*storage;
	} else if (choice == 9) {
		*storage ^= 0xFF;
	} else if (choice == 10) {
		*storage ^= 0xFF000000;
	} else if (choice == 11) {
		*storage = fffc_get_shrt_max();
	} else if (choice == 12) {
		*storage = 0;
	}
	fffc_log_write(storage, sizeof(short));
	return 0;
}

int fffc_mutator_for_target_type(short int storage) {
 	int choice = fffc_pick_one_or_none(13);
  	if (choice <= 0) {
 		return 0;
 	} else if (choice < 4) {
		*storage ^= fffc_get_random() & 0xFFFF;
	} else if (choice == 5) {
		*storage += 1;
	} else if (choice == 6) {
		*storage -= 1;
	} else if (choice == 7) {
		*storage <<= 1;
	} else if (choice == 8) {
		*storage = -*storage;
	} else if (choice == 9) {
		*storage ^= 0xFF;
	} else if (choice == 10) {
		*storage ^= 0xFF000000;
	} else if (choice == 11) {
		*storage = fffc_get_shrt_max();
	} else if (choice == 12) {
		*storage = 0;
	}
	fffc_log_write(storage, sizeof(short));
	return 0;
}

int fffc_mutator_for_target_type(long int storage) {
	int choice = fffc_pick_one_or_none(25);
 	if (choice <= 0) {
 		return 0;
 	} else if (choice < 4) {
		*storage ^= fffc_get_random() & 0x0000FFFF;
	} else if ((choice > 0) && (choice < 8)) {
		*storage ^= fffc_get_random() & 0xFFFF0000;
	} else if ((choice > 0) && (choice < 12)) {
		*storage ^= fffc_get_random() & 0xFF0000FF;
	} else if ((choice > 0) && (choice < 16)) {
		*storage ^= fffc_get_random() & 0x00FFFF00;
	} else if (choice == 17) {
		*storage += 1;
	} else if (choice == 18) {
		*storage -= 1;
	} else if (choice == 19) {
		*storage <<= 1;
	} else if (choice == 20) {
		*storage = -*storage;
	} else if (choice == 21) {
		*storage ^= 0xFF;
	} else if (choice == 22) {
		*storage ^= 0xFF000000;
	} else if (choice == 23) {
		*storage = fffc_get_long_max();
	} else if (choice == 24) {
		*storage = fffc_get_long_min();
	}
	fffc_log_write(storage, sizeof(long int));
	return 0;
}

int fffc_mutator_for_target_type(long long int storage) {
	int choice = fffc_pick_one_or_none(24);
 	if (choice <= 0) {
 		return 0;
 	} else if (choice < 4) {
		*storage ^= fffc_get_random() & 0x000000000000FFFFL;
	} else if ((choice > 0) && (choice < 8)) {
		*storage ^= fffc_get_random() & 0xFFFF000000000000L;
	} else if ((choice > 0) && (choice < 12)) {
		*storage ^= fffc_get_random() & 0x0000FFFF00000000L;
	} else if ((choice > 0) && (choice < 16)) {
		*storage ^= fffc_get_random() & 0x00000000FFFF0000L;
	} else if (choice == 16) {
		*storage += 1;
	} else if (choice == 17) {
		*storage -= 1;
	} else if (choice == 18) {
		*storage <<= 1;
	} else if (choice == 19) {
		*storage = -*storage;
	} else if (choice == 20) {
		*storage ^= 0x00000000000000FFL;
	} else if (choice == 21) {
		*storage ^= 0xFF00000000000000L;
	} else if (choice == 22) {
		*storage = fffc_get_long_long_max();
	} else if (choice == 23) {
		*storage = fffc_get_long_long_min();
	}
	fffc_log_write(storage, sizeof(long long int));
	return 0;
}

int fffc_mutator_for_target_type(long long unsigned int storage) {
	int choice = fffc_pick_one_or_none(24);
 	if (choice <= 0) {
 		return 0;
 	} else if (choice < 4) {
		*storage ^= fffc_get_random() & 0x000000000000FFFFL;
	} else if ((choice > 0) && (choice < 8)) {
		*storage ^= fffc_get_random() & 0xFFFF000000000000L;
	} else if ((choice > 0) && (choice < 12)) {
		*storage ^= fffc_get_random() & 0x0000FFFF00000000L;
	} else if ((choice > 0) && (choice < 16)) {
		*storage ^= fffc_get_random() & 0x00000000FFFF0000L;
	} else if (choice == 16) {
		*storage += 1;
	} else if (choice == 17) {
		*storage -= 1;
	} else if (choice == 18) {
		*storage <<= 1;
	} else if (choice == 19) {
		*storage = -*storage;
	} else if (choice == 20) {
		*storage ^= 0x00000000000000FFL;
	} else if (choice == 21) {
		*storage ^= 0xFF00000000000000L;
	} else if (choice == 22) {
		*storage = fffc_get_long_long_max();
	} else if (choice == 23) {
		*storage = 0;
	}
	fffc_log_write(storage, sizeof(long long unsigned int));
	return 0;
}

int fffc_mutator_for_target_type(unsigned int storage) {
	int choice = fffc_pick_one_or_none(16);
 	if (choice <= 0) {
 		return 0;
 	} else if (choice < 4) {
		*storage ^= fffc_get_random() & 0xFFFF;
	} else if ((choice > 0) && (choice < 8)) {
		*storage ^= fffc_get_random() & 0xFFFF0000;
	} else if (choice == 8) {
		*storage += 1;
	} else if (choice == 9) {
		*storage -= 1;
	} else if (choice == 10) {
		*storage <<= 1;
	} else if (choice == 11) {
		*storage = -*storage;
	} else if (choice == 12) {
		*storage ^= 0xFF;
	} else if (choice == 13) {
		*storage ^= 0xFF000000;
	} else if (choice == 14) {
		*storage = fffc_get_int_max();
	} else if (choice == 15) {
		*storage = 0;
	}
	fffc_log_write(storage, sizeof(int));
	return 0;
}

int fffc_mutator_for_target_type(long unsigned int storage) {
	int choice = fffc_pick_one_or_none(25);
 	if (choice <= 0) {
 		return 0;
 	} else if (choice < 4) {
		*storage ^= fffc_get_random() & 0x0000FFFF;
	} else if ((choice > 0) && (choice < 8)) {
		*storage ^= fffc_get_random() & 0xFFFF0000;
	} else if ((choice > 0) && (choice < 12)) {
		*storage ^= fffc_get_random() & 0xFF0000FF;
	} else if ((choice > 0) && (choice < 16)) {
		*storage ^= fffc_get_random() & 0x00FFFF00;
	} else if (choice == 17) {
		*storage += 1;
	} else if (choice == 18) {
		*storage -= 1;
	} else if (choice == 19) {
		*storage <<= 1;
	} else if (choice == 20) {
		*storage = -*storage;
	} else if (choice == 21) {
		*storage ^= 0xFF;
	} else if (choice == 22) {
		*storage ^= 0xFF000000;
	} else if (choice == 23) {
		*storage = fffc_get_long_max();
	} else if (choice == 24) {
		*storage = 0;
	}
	fffc_log_write(storage, sizeof(long unsigned int));
	return 0;
}

int fffc_mutator_for_target_type(void storage) {
	return 0;
}