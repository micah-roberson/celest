# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_UnpackedTarball_UnpackedTarball,coinmp))

$(eval $(call gb_UnpackedTarball_set_tarball,coinmp,$(COINMP_TARBALL)))

$(eval $(call gb_UnpackedTarball_fix_end_of_line,coinmp,\
	CoinMP/MSVisualStudio/v9/CoinMP.sln \
))

$(eval $(call gb_UnpackedTarball_set_patchlevel,coinmp,0))

$(eval $(call gb_UnpackedTarball_update_autoconf_configs,coinmp))
$(eval $(call gb_UnpackedTarball_update_autoconf_configs,coinmp,\
	BuildTools \
	Cbc \
	Cgl \
	Clp \
	CoinMP \
	CoinUtils \
	Data/Sample \
	Osi \
))

ifneq ($(MSYSTEM),)
# use binary flag so patch from git-bash won't choke on mixed line-endings in patches
$(eval $(call gb_UnpackedTarball_set_patchflags,coinmp,--binary))
endif

# * external/coinmp/Wnon-c-typedef-for-linkage.patch upstream at
#   <https://list.coin-or.org/pipermail/coin-discuss/2020-February/003972.html> "[Coin-discuss]
#   Small patch to fix Clang -Wnon-c-typedef-for-linkage in Clp":
# * external/coinmp/const.patch.1 upstream at
#   <https://github.com/coin-or/CoinMP/pull/26> and
#   <https://github.com/coin-or/Clp/pull/315>
$(eval $(call gb_UnpackedTarball_add_patches,coinmp,\
	external/coinmp/no-binaries.patch.1 \
	external/coinmp/werror-undef.patch.0 \
	external/coinmp/coinmp-msvc-disable-sse2.patch.1 \
	$(if $(filter MSC,$(COM)),external/coinmp/windows.build.patch.1) \
	external/coinmp/ubsan.patch.0 \
	external/coinmp/rpath.patch \
	external/coinmp/libtool.patch \
	external/coinmp/Wnon-c-typedef-for-linkage.patch \
	external/coinmp/register.patch \
	external/coinmp/configure-exit.patch \
	external/coinmp/pedantic-errors.patch \
	external/coinmp/bind2nd.patch.1 \
	external/coinmp/clang-with-path.patch \
	external/coinmp/odr.patch \
	external/coinmp/const.patch.1 \
	external/coinmp/const2.patch.1 \
	external/coinmp/const3.patch.1 \
))

# vim: set noet sw=4 ts=4:
