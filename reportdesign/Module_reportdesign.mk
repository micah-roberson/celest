# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_Module_Module,reportdesign))

$(eval $(call gb_Module_add_targets,reportdesign,\
    Library_rpt \
    Library_rptui \
    UIConfig_dbreport \
))

$(eval $(call gb_Module_add_l10n_targets,reportdesign,\
    AllLangMoTarget_rpt \
))

# deactivated since sb123;
# apparently fails because OOo does not find JVM?
#$(eval $(call gb_Module_add_subsequentcheck_targets,reportdesign,\
	JunitTest_reportdesign_complex \
))

# screenshots
$(eval $(call gb_Module_add_screenshot_targets,reportdesign,\
    CppunitTest_reportdesign_dialogs_test \
))

# vim: set noet sw=4 ts=4:
