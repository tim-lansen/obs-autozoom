/*
Plugin Name
Copyright (C) <Year> <Developer> <Email Address>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include "plugin-macros.generated.h"

const char *mdf_get_name(void *unused);
obs_properties_t *mdf_properties(void *data);
void mdf_defaults(obs_data_t *settings);
void mdf_update(void *data, obs_data_t *s);
void *mdf_create(obs_data_t *settings, obs_source_t *context);
void mdf_destroy(void *data);
struct obs_source_frame *mdf_video(void *data, struct obs_source_frame *frame);
void mdf_remove(void *data, obs_source_t *parent);

struct obs_source_info motion_detect_filter = {
	.id = "motion_zone",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_ASYNC,
	.get_name = mdf_get_name,
	.get_properties = mdf_properties,
	.get_defaults = mdf_defaults,
	.update = mdf_update,
	.create = mdf_create,
	.destroy = mdf_destroy,

	.filter_video = mdf_video,
	.filter_remove = mdf_remove,
};

const char *cte_get_name(void *unused);
void *cte_create(obs_data_t *settings, obs_source_t *context);
void cte_destroy(void *data);
void cte_update(void *data, obs_data_t *s);
obs_properties_t *cte_properties(void *data);
void cte_defaults(obs_data_t *settings);
void cte_tick(void *data, float seconds);
void cte_render(void *data, gs_effect_t *effect);

struct obs_source_info crop_track_effect = {
	.id = "crop_track",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = cte_get_name,
	.create = cte_create,
	.destroy = cte_destroy,
	.update = cte_update,
	.get_properties = cte_properties,
	.get_defaults = cte_defaults,
	.video_tick = cte_tick,
	.video_render = cte_render,
	//.get_width = crop_auto_width,
	//.get_height = crop_auto_height,
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
	obs_register_source(&crop_track_effect);
	obs_register_source(&motion_detect_filter);
	return true;
}

void obs_module_unload(void) {}
