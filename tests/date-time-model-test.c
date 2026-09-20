// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Pure model/catalogue test. Persistence is exercised through platform CI once
 * the platform integration harness is enabled.
 */
#include "system-settings/date-time-model.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    InfiltratrClockProfile profile = INFILTRATR_CLOCK_PROFILE_SYSTEM;
    size_t i;
    bool saw_decimal = false;

    assert(ss_date_time_model_profile_count() == 4U);
    for (i = 0U; i < ss_date_time_model_profile_count(); ++i) {
        assert(ss_date_time_model_profile_at(i, &profile));
        assert(infiltratr_clock_profile_id(profile) != NULL);
        assert(infiltratr_clock_profile_name(profile) != NULL);
        if (profile == INFILTRATR_CLOCK_PROFILE_DECIMAL_10) {
            saw_decimal = true;
            assert(strcmp(infiltratr_clock_profile_id(profile),
                          "decimal-10") == 0);
        }
    }
    assert(saw_decimal);
    return 0;
}
