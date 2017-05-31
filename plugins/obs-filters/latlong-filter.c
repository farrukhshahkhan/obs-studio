#include <obs-internal.h>
#include <obs-module.h>
#include <graphics/vec2.h>
#include <graphics/math-defs.h>

struct latlong_filter_data {
	obs_source_t                   *context;

	gs_effect_t                    *effect;
	gs_eparam_t                    *param_mul;
	gs_eparam_t                    *param_add;

	bool                           on;      // toggle latlong mode on/off;
	int                            radius;  // radius between [0..100]
	int                            theta;   // in degrees [0 .. 180]
	int                            phi;     // in degrees [0 ..  360]
	int                            fov_theta; // fov = theta-fov_theta/2 .. theta+fov_theta/2
	int                            fov_phi; // fov = phi-fov_phi/2 .. phi+fov_phi/2

	uint32_t                        width, height;
	//struct vec2                    mul_val;
	//struct vec2                    add_val;
};

static void latlong_display_update(struct latlong_filter_data *filter)
{
    obs->data.first_display->latlong_on = filter->on;
    obs->data.first_display->latlong_info.radius    = filter->radius;
    obs->data.first_display->latlong_info.phi       = filter->phi;
    obs->data.first_display->latlong_info.theta     = filter->theta;
    obs->data.first_display->latlong_info.fov_theta = filter->fov_theta;
    obs->data.first_display->latlong_info.fov_phi   = filter->fov_phi;

}
static const char *latlong_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("LatLongFilter");
}

static void *latlong_filter_create(obs_data_t *settings, obs_source_t *context)
{
	struct latlong_filter_data *filter = bzalloc(sizeof(*filter));
	char *effect_path = obs_module_file("latlong_filter.effect");

	filter->context = context;

	obs_enter_graphics();
	filter->effect = gs_effect_create_from_file(effect_path, NULL);
	obs_leave_graphics();

	bfree(effect_path);

	if (!filter->effect) {
		bfree(filter);
		return NULL;
	}

	filter->param_mul = gs_effect_get_param_by_name(filter->effect,
			"mul_val");
	filter->param_add = gs_effect_get_param_by_name(filter->effect,
			"add_val");

//	latlong_display_update(filter);
	obs_source_update(context, settings);
	return filter;
}

static void latlong_filter_destroy(void *data)
{
	struct latlong_filter_data *filter = data;

	obs_enter_graphics();
	gs_effect_destroy(filter->effect);
	obs_leave_graphics();

	bfree(filter);
}

static void latlong_filter_update(void *data, obs_data_t *settings)
{
	struct latlong_filter_data *filter = data;

	filter->on      = obs_data_get_bool(settings, "on");
	filter->radius  = obs_data_get_int(settings, "radius");
	filter->phi     = obs_data_get_int(settings, "phi");
	filter->theta   = obs_data_get_int(settings, "theta");
	filter->fov_phi = obs_data_get_int(settings, "fov_phi")/2;
	filter->fov_theta = filter->fov_phi/2;
//	latlong_display_update(filter);
}

static bool on_toggled(obs_properties_t *props, obs_property_t *p,
		obs_data_t *settings)
{
    bool on = obs_data_get_bool(settings, "on");
    obs_property_set_visible(obs_properties_get(props, "radius"), on);
    obs_property_set_visible(obs_properties_get(props, "phi"), on);
    obs_property_set_visible(obs_properties_get(props, "theta"), on);
    obs_property_set_visible(obs_properties_get(props, "fov_phi"), on);
	UNUSED_PARAMETER(p);
	return true;
}

static obs_properties_t *latlong_filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *p;
	p = obs_properties_add_bool(props, "on", obs_module_text("Lat Long Mode On"));

	obs_property_set_modified_callback(p, on_toggled);

	obs_properties_add_int(props, "radius",   obs_module_text("Radius (1 .. 100)"),       1, 100, 1);
	obs_properties_add_int(props, "theta",    obs_module_text("Theta (Z->XY: 0 .. 180)"), 0, 180, 1);
	obs_properties_add_int(props, "phi",      obs_module_text("Phi (X->Y: 0 .. 360)"),    0, 360, 1);
	obs_properties_add_int(props, "fov_phi",  obs_module_text("FOV (0 .. 360)"),          0, 360, 1);

	UNUSED_PARAMETER(data);
	return props;
}

static void latlong_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "on", true);
	obs_data_set_default_double(settings, "radius", 1);
	obs_data_set_default_int(settings, "fov_phi", 100);
}

#if 0
static void calc_latlong_dimensions(struct latlong_filter_data *filter,
		struct vec2 *mul_val, struct vec2 *add_val)
{
	obs_source_t *target = obs_filter_get_target(filter->context);
	uint32_t width;
	uint32_t height;
	float x, y, z;
	float radius, theta, phi;
	static int printcount = 0;
	bool printit;
	printcount++;
if (printcount%1000 == 0)
    printit = true;
else
    printit = false;

// FSK: Create and pass a 4x4 matric of rotation matrix with theta/phi values
	if (!target) {
		width = 0;
		height = 0;
		return;
	} else {
		width = obs_source_get_base_width(target);
		height = obs_source_get_base_height(target);
	}
if (printit)
blog(LOG_INFO, "Base widthxheight %dx%d",width, height);
	radius  = filter->radius;
	phi     = filter->phi / (180.0f / M_PI); // deg to rad
	theta   = filter->theta / (180.0f / M_PI); // deg to rad
	x = radius * sinf(theta) * cosf(phi);
	y = radius * sinf(theta) * sinf(phi);
	z = radius * cosf(theta);
if (printit)
{
blog(LOG_INFO, "Radius,phi, theta %f, %f, %f",radius,phi,theta);
blog(LOG_INFO, "x,y,z %f, %f, %f",x,y,z);
}
//	UNUSED_PARAMETER(z);

//FSK: This should be in texture relative coords... 0..1?
	filter->left   = (int) (x * filter->width/2); //+ filter->width/2;
	filter->right  = (int) (x * filter->width/2) + filter->width;
	filter->top    = (int) (y * filter->height/2); //+ filter->height/2;
	filter->bottom = (int) (y * filter->height/2) + filter->height;

if (printit)
{
blog(LOG_INFO, "bottom-left %d, %d",filter->left,filter->bottom);
blog(LOG_INFO, "top-right   %d, %d",filter->right,filter->top);
}

//	if (filter->width  < 1) filter->width  = 1;
//	if (filter->height < 1) filter->height = 1;

	// Find scale/translate values...
	if (width && filter->width) {
		mul_val->x = 1.0; //(float)filter->width / (float)width;
		if (filter->left < 0)
		    add_val->x = (float)(width + filter->left);
		else
		    add_val->x = (float)filter->left;
	}

	if (height && filter->height) {
		mul_val->y = 1.0; //(float)filter->height / (float)height;
		if (filter->top < 0)
		    add_val->y = (float)(height + filter->top);
		else
		    add_val->y = (float)filter->top;
	}
if (printit)
{
blog(LOG_INFO, "mul-val(x,y)  %f, %f",mul_val->x,mul_val->y);
blog(LOG_INFO, "add-val(x,y)  %f, %f",add_val->x,add_val->y);
}
}
#endif

static void latlong_filter_tick(void *data, float seconds)
{
	struct latlong_filter_data *filter = data;

	obs_source_t *target = obs_filter_get_target(filter->context);
	filter->width  = obs_source_get_base_width(target);
	filter->height = obs_source_get_base_height(target);

#if 0
	vec2_zero(&filter->mul_val);
	vec2_zero(&filter->add_val);
	calc_latlong_dimensions(filter, &filter->mul_val, &filter->add_val);
#endif

	UNUSED_PARAMETER(seconds);
}

static void latlong_filter_render(void *data, gs_effect_t *effect)
{
	struct latlong_filter_data *filter = data;

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
				OBS_NO_DIRECT_RENDERING))
		return;

//	gs_effect_set_vec2(filter->param_mul, &filter->mul_val);
//	gs_effect_set_vec2(filter->param_add, &filter->add_val);

	obs_source_process_filter_end(filter->context, filter->effect,
			filter->width, filter->height);

	UNUSED_PARAMETER(effect);
}

static uint32_t latlong_filter_width(void *data)
{
	struct latlong_filter_data *latlong = data;
	return (uint32_t)latlong->width;
}

static uint32_t latlong_filter_height(void *data)
{
	struct latlong_filter_data *latlong = data;
	return (uint32_t)latlong->height;
}

struct obs_source_info latlong_filter = {
	.id                            = "latlong_filter",
	.type                          = OBS_SOURCE_TYPE_FILTER,
	.output_flags                  = OBS_SOURCE_VIDEO,
	.get_name                      = latlong_filter_get_name,
	.create                        = latlong_filter_create,
	.destroy                       = latlong_filter_destroy,
	.update                        = latlong_filter_update,
	.get_properties                = latlong_filter_properties,
	.get_defaults                  = latlong_filter_defaults,
	.video_tick                    = latlong_filter_tick,
	.video_render                  = latlong_filter_render,
	.get_width                     = latlong_filter_width,
	.get_height                    = latlong_filter_height
};
