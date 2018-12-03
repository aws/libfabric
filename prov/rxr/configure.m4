dnl Configury specific to the libfabric rxr provider
dnl Copyright (c) 2019 Amazon.com, Inc. or its affiliates. All rights reserved.

dnl Called to configure this provider
dnl
dnl Arguments:
dnl
dnl $1: action if configured successfully
dnl $2: action if not configured successfully
dnl
AC_DEFUN([FI_RXR_CONFIGURE],[
	# Determine if we can support the rxr provider
	rxr_h_happy=0
	rxr_h_enable_poisoning=0
	AS_IF([test x"$enable_rxr" != x"no"], [rxr_h_happy=1])
	AC_ARG_ENABLE([rxr-mem-poisoning],
		[AS_HELP_STRING([--enable-rxr-mem-poisoning],
			[Enable RxR memory poisoning support for debugging @<:@default=no@:>@])
		],
		[rxr_h_enable_poisoning=$enableval],
		[rxr_h_enable_poisoning=no])
	AS_IF([test x"$rxr_h_enable_poisoning" == x"yes"],
		[AC_DEFINE([ENABLE_RXR_POISONING], [1],
			[RxR memory poisoning support for debugging])],
		[])
	AS_IF([test $rxr_h_happy -eq 1], [$1], [$2])
])
