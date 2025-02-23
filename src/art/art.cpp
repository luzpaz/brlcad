/*                     A R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file art/art.cpp
 *
 * Once you have appleseed installed, run BRL-CAD's CMake with APPLESEED_ROOT
 * set to enable this program:
 *
 * cmake .. -DAPPLESEED_ROOT=/path/to/appleseed -DBRLCAD_PNG=SYSTEM -DBRLCAD_ZLIB=SYSTEM
 *
 * (the appleseed root path should contain bin, lib and include directories)
 *
 * On Linux, if using the prebuilt binary you'll need to set LD_LIBRARY_PATH:
 * export LD_LIBRARY_PATH=/path/to/appleseed/lib
 *
 *
 * The example scene object used by helloworld is found at:
 * https://raw.githubusercontent.com/appleseedhq/appleseed/master/sandbox/examples/cpp/helloworld/data/scene.obj
 *
 * basic example helloworld code from
 * https://github.com/appleseedhq/appleseed/blob/master/sandbox/examples/cpp/helloworld/helloworld.cpp
 * has the following license:
 *
 * This software is released under the MIT license.
 *
 * Copyright (c) 2010-2013 Francois Beaune, Jupiter Jazz Limited
 * Copyright (c) 2014-2018 Francois Beaune, The appleseedhq Organization
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "common.h"
/* avoid redefining the appleseed sleep function */
#undef sleep

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include <cstddef>
#include <memory>
#include <string>

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wpedantic"
#  pragma GCC diagnostic ignored "-Wignored-qualifiers"
#  if (__GNUC__ >= 8)
#    pragma GCC diagnostic ignored "-Wclass-memaccess"
#  endif
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wpedantic"
#  pragma clang diagnostic ignored "-Wignored-qualifiers"
#endif


// appleseed.renderer headers. Only include header files from renderer/api/.
#include "renderer/api/bsdf.h"
#include "renderer/api/camera.h"
#include "renderer/api/color.h"
#include "renderer/api/environment.h"
#include "renderer/api/environmentedf.h"
#include "renderer/api/environmentshader.h"
#include "renderer/api/frame.h"
#include "renderer/api/light.h"
#include "renderer/api/log.h"
#include "renderer/api/material.h"
#include "renderer/api/object.h"
#include "renderer/api/project.h"
#include "renderer/api/rendering.h"
#include "renderer/api/scene.h"
#include "renderer/api/surfaceshader.h"
#include "renderer/api/utility.h"
#include "renderer/api/shadergroup.h"

// appleseed.foundation headers.
#include "foundation/core/appleseed.h"
#include "foundation/math/matrix.h"
#include "foundation/math/scalar.h"
#include "foundation/math/transform.h"
#include "foundation/math/vector.h"
#include "foundation/utility/containers/dictionary.h"
#include "foundation/utility/log/consolelogtarget.h"
#include "foundation/utility/autoreleaseptr.h"
#include "foundation/utility/searchpaths.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif


#include "vmath.h"	/* vector math macros */
#include "raytrace.h"	    /* librt interface definitions */
#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/vls.h"
#include "art.h"
#include "rt/tree.h"
#include "ged.h"
#include "ged/commands.h"
#include "ged/defines.h"

struct application APP;
struct resource* resources;
size_t samples = 25;

extern "C" {
    FILE* outfp = NULL;
    int	save_overlaps = 1;
    size_t		n_malloc;		/* Totals at last check */
    size_t		n_free;
    size_t		n_realloc;
    mat_t view2model;
    mat_t model2view;
    struct icv_image* bif = NULL;
}

/* NOTE: stub in empty rt_do_tab to satisfy ../rt/opt.c - this means
 * we can't run the commands, but they are tied deeply into the various
 * src/rt files and a significant refactor is in order to properly
 * extract that functionality into a library... */

extern "C" {
    // struct command_tab rt_do_tab[] = { {NULL, NULL, NULL, 0, 0, 0} };
    void option(const char *cat, const char *opt, const char *des, int verbose);
    void usage(const char* argv0, int verbose);
    int get_args(int argc, const char* argv[]);


    extern char* outputfile;
    extern int objc;
    extern char** objv;
    extern size_t npsw;
    extern struct bu_ptbl* cmd_objs;
    extern size_t width, height;
    extern fastf_t azimuth, elevation;
    extern point_t eye_model; /* model-space location of eye */
    extern fastf_t eye_backoff; /* dist from eye to center */
    extern mat_t Viewrotscale;
    extern fastf_t viewsize;
    extern fastf_t aspect;

    void grid_setup();
}

// holds application specific paramaters
struct bu_structparse view_parse[] = {
    {"%d", 1, "samples", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "s", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

// holds regular parse variables
struct bu_structparse set_parse[] = {
    /* daisy-chain to additional app-specific parameters */
    {"%p",	1, "Application-Specific Parameters", bu_byteoffset(view_parse[0]),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",	0, (char *)0,		0,						BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

// set callback function
int cm_set(const int argc, const char **argv)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;

    if (argc <= 1) {
	bu_struct_print("Generic and Application-Specific Parameter Values",
			set_parse, (char *)0);
	return 0;
    }

    bu_vls_from_argv(&str, argc-1, (const char **)argv+1);
    if (bu_struct_parse(&str, set_parse, (char *)0, NULL) < 0) {
	bu_vls_free(&str);
  bu_log("ERROR HERE");
	return -1;
    }
    // bu_log("str: %s", bu_vls_cstr(&str));
    bu_vls_free(&str);
    return 0;
}

struct command_tab rt_do_tab[] = {
    {"set", 	"", "show or set parameters",
     cm_set,		1, 999},
    {(char *)0, (char *)0, (char *)0,
     0,		0, 0	/* END */}
};

/* Initializes module specific options
 * NOTE: to have an accurate usage() menu, we overwrite the indexes of all the
 * options from rt/usage.cpp which we don't support
 */
void init_options(void) {
  /* Set the byte offsets at run time */
  view_parse[ 0].sp_offset = bu_byteoffset(samples);
  view_parse[ 1].sp_offset = bu_byteoffset(samples);


  // for now, just support -c set samples=x
  // TODO: update to support more options
  option("", "-o filename", "Render to specified image file (e.g., image.png or image.pix)", 0);
  option("", "-F framebuffer", "Render to a framebuffer (defaults to a window)", 100);
  option("", "-s #", "Square image size (default: 512 - implies 512x512 image)", 100);
  option("", "-w # -n #", "Image pixel dimensions as width and height", 100);
  // option("", "-C #/#/#", "Set background image color to R/G/B values (default: 0/0/1)", 0);
  // option("", "-W", "Set background image color to white", 0);
  option("", "-R", "Disable reporting of overlaps", 100);
  option("", "-? or -h", "Display help", 1);

  option("Raytrace", "-a # -e #", "Azimuth and elevation in degrees (default: -a 35 -e 25)", 100);
  option("Raytrace", "-p #", "Perspective angle, degrees side to side (0 <= # < 180)", 100);
  option("Raytrace", "-E #", "Set perspective eye distance from model (default: 1.414)", 100);
  option("Raytrace", "-H #", "Specify number of hypersample rays per pixel (default: 0)", 100);
  option("Raytrace", "-J #", "Specify a \"jitter\" pattern (default: 0 - no jitter)", 100);
  option("Raytrace", "-P #", "Specify number of processors to use (default: all available)", 100);
  option("Raytrace", "-T # or -T #/#", "Tolerance as distance or distance/angular", 100);

  option("Advanced", "-c \"command\"", "[eventually will] Run a semicolon-separated list of commands (just samples for now)", 0);
  option("Advanced", "-M", "Read matrix + commands on stdin (RT 'saveview' scripts)", 100);
  option("Advanced", "-D #", "Specify starting frame number (ending is specified via -K #)", 100);
  option("Advanced", "-K #", "Specify ending frame number (starting is specified via -D #)", 100);
  option("Advanced", "-g #", "Specify grid cell (pixel) width, in millimeters", 100);
  option("Advanced", "-G #", "Specify grid cell (pixel) height, in millimeters", 100);
  option("Advanced", "-S", "Enable stereo rendering", 100);
  option("Advanced", "-U #", "Turn on air region rendering (default: 0 - off)", 100);
  option("Advanced", "-V #", "View (pixel) aspect ratio (width/height)", 100);
  option("Advanced", "-j xmin,xmax,ymin,ymax", "Only render pixels within the specified sub-rectangle", 100);
  option("Advanced", "-k xdir,ydir,zdir,dist", "Specify a cutting plane for the entire render scene", 100);

  option("Developer", "-v [#]", "Specify or increase RT verbosity", 100);
  option("Developer", "-X #", "Specify RT debugging flags", 100);
  option("Developer", "-x #", "Specify librt debugging flags", 100);
  option("Developer", "-N #", "Specify libnmg debugging flags", 100);
  option("Developer", "-! #", "Specify libbu debugging flags", 100);
  option("Developer", "-, #", "Specify space partitioning algorithm", 100);
  option("Developer", "-B", "Disable randomness for \"benchmark\"-style repeatability", 100);
  option("Developer", "-b \"x y\"", "Only shoot one ray at pixel coordinates (quotes required)", 100);
  option("Developer", "-Q x,y", "Shoot one pixel with debugging; compute others without", 100);
}

// Define shorter namespaces for convenience.
namespace asf = foundation;
namespace asr = renderer;

/* db_walk_tree() callback to register all regions within the scene
 * using either legacy rgb sets and phong shaders or specified material OSL
 * optical shader
 */
int register_region(struct db_tree_state* tsp __attribute__((unused)),
                const struct db_full_path* pathp __attribute__((unused)),
                const struct rt_comb_internal* combp __attribute__((unused)),
                void* data)
{
  // We open the db using the region path to get objects name
  struct directory* dp = DB_FULL_PATH_CUR_DIR(pathp);

  char* name;
  name = dp->d_namep;

  /*
  this is for testing bounding box with the parent directory - using build/share/db/moss.g
  eventually all comments using this will be deleted
  */
  // const char* name_char;
  // std::string name_test = "all.g/" + std::string(name);
  // name_char = name_test.c_str();
  // bu_log("name: %s\n", APP.a_rt_i->rti_dbip->dbi_filename);

  // get objects bounding box
  struct ged* ged;
  ged = ged_open("db", APP.a_rt_i->rti_dbip->dbi_filename, 1);
  point_t min;
  point_t max;
  int ret = ged_get_obj_bounds(ged, 1, (const char**)&name, 1, min, max);
  // int ret = ged_get_obj_bounds(ged, 1, (const char**)&name_char, 1, min, max);

  bu_log("ged: %i | min: %f %f %f | max: %f %f %f\n", ret, V3ARGS(min), V3ARGS(max));

  VMOVE(APP.a_uvec, min);
  VMOVE(APP.a_vvec, max);


  /*
  create object paramArray to pass to constructor
  NOTE: we can likely remove min/max values from here if the above bounding box calculation works
  */
  renderer::ParamArray geometry_parameters = asr::ParamArray()
               .insert("database_path", name)
               // .insert("database_path", name_char)
               .insert("object_count", objc)
               .insert("minX", min[0])
               .insert("minY", min[1])
               .insert("minZ", min[2])
               .insert("maxX", max[0])
               .insert("maxY", max[1])
               .insert("maxZ", max[2]);


  asf::auto_release_ptr<renderer::Object> brlcad_object(
  new BrlcadObject{
     name,
     // name_char,
     geometry_parameters,
     &APP, resources});

  // typecast our scene using the callback function 'data'
  asr::Scene* scene = static_cast<asr::Scene*>(data);

  // create assembly for current object
  std::string assembly_name = std::string(name) + "_object_assembly";
  // std::string assembly_name = std::string(name_test) + "_object_assembly";
  asf::auto_release_ptr<asr::Assembly> assembly(
    asr::AssemblyFactory().create(
      assembly_name.c_str(),
      asr::ParamArray()));

  // create a shader group
  std::string shader_name = std::string(name) + "_shader";
  // std::string shader_name = std::string(name_test) + "_shader";
  asf::auto_release_ptr<asr::ShaderGroup> shader_grp(
      asr::ShaderGroupFactory().create(
          shader_name.c_str(),
          asr::ParamArray()));

  // THIS IS OUR INPUT SHADER - add to shader group
  /* This uses an already created appleseed .oso shader
  in the form of
  type
  shader name
  layer
  paramArray
  */
  struct bu_vls v=BU_VLS_INIT_ZERO;
  bu_vls_printf(&v, "color %f %f %f", combp->rgb[0]/255.0, combp->rgb[1]/255.0, combp->rgb[2]/255.0);
  const char* color = bu_vls_cstr(&v);
  shader_grp->add_shader(
      "shader",
      "as_disney_material",
      "shader_in",
      asr::ParamArray()
        .insert("in_color", color)
  );
  bu_vls_free(&v);

  /* import non compiled .osl shader in the form of
  type
  name
  layer
  source
  paramArray
  note: this relies on appleseed triggering on osl compiler
  */
  // shader_grp->add_source_shader(
  //   "shader",
  //   shader_name.c_str(),
  //   "shader_in",
  //   "shader_in",
  //   asr::ParamArray()
  // );

  // add material2surface so we can map input shader to object surface
  shader_grp->add_shader(
    "surface",
    "as_closure2surface",
    "close",
    asr::ParamArray()
  );

  // connect the two shader nodes within the group
  shader_grp->add_connection(
    "shader_in",
    "out_outColor",
    "close",
    "in_input"
  );

  // add the shader group to the assembly
  assembly->shader_groups().insert(
    shader_grp
  );

  // Create a physical surface shader and insert it into the assembly.
  // This is technically not needed with the current shader implementation
  assembly->surface_shaders().insert(
    asr::PhysicalSurfaceShaderFactory().create(
    "Material_mat_surface_shader",
    asr::ParamArray()
      .insert("lighting_samples", "1")
    )
  );

  // create a material with our shader_group
  std::string material_mat = shader_name + "_mat";
  assembly->materials().insert(
      asr::OSLMaterialFactory().create(
          material_mat.c_str(),
          asr::ParamArray()
              .insert("osl_surface", shader_name.c_str())
              .insert("surface_shader", "Material_mat_surface_shader")
      )
  );

  // insert object into object array in assembly
  assembly->objects().insert(brlcad_object);

  // create an instance for our newly created object within the assembly
  const std::string instance_name = std::string(assembly_name) + "_brlcad_inst";
  assembly->object_instances().insert(
    asr::ObjectInstanceFactory::create(
    instance_name.c_str(),
    asr::ParamArray(),
    name,
    // name_char,
    asf::Transformd::identity(),
    asf::StringDictionary()
    .insert("default", material_mat.c_str())
    .insert("default2", material_mat.c_str())
  ));

  // add assembly to assemblies array in scene
  scene->assemblies().insert(assembly);

  // finally, we add an instance to use the assembly in the render
  std::string assembly_inst_name = assembly_name + "_inst";
  asf::auto_release_ptr<asr::AssemblyInstance> assembly_instance(
    asr::AssemblyInstanceFactory::create(
    assembly_inst_name.c_str(),
    asr::ParamArray(),
    assembly_name.c_str()
    )
  );
  assembly_instance->transform_sequence()
    .set_transform(
      0.0f,
      asf::Transformd::identity());
  scene->assembly_instances().insert(assembly_instance);

  return 0;
}

void
do_ae(double azim, double elev)
{
    vect_t temp;
    vect_t diag;
    mat_t toEye;
    struct rt_i* rtip = APP.a_rt_i;

    if (rtip == NULL)
	return;

    if (rtip->nsolids <= 0)
	bu_exit(EXIT_FAILURE, "ERROR: no primitives active\n");

    if (rtip->nregions <= 0)
	bu_exit(EXIT_FAILURE, "ERROR: no regions active\n");

    if (rtip->mdl_max[X] >= INFINITY) {
	bu_log("do_ae: infinite model bounds? setting a unit minimum\n");
	VSETALL(rtip->mdl_min, -1);
    }
    if (rtip->mdl_max[X] <= -INFINITY) {
	bu_log("do_ae: infinite model bounds? setting a unit maximum\n");
	VSETALL(rtip->mdl_max, 1);
    }

    /*
     * Enlarge the model RPP just slightly, to avoid nasty effects
     * with a solid's face being exactly on the edge NOTE: This code
     * is duplicated out of librt/tree.c/rt_prep(), and has to appear
     * here to enable the viewsize calculation to match the final RPP.
     */
    rtip->mdl_min[X] = floor(rtip->mdl_min[X]);
    rtip->mdl_min[Y] = floor(rtip->mdl_min[Y]);
    rtip->mdl_min[Z] = floor(rtip->mdl_min[Z]);
    rtip->mdl_max[X] = ceil(rtip->mdl_max[X]);
    rtip->mdl_max[Y] = ceil(rtip->mdl_max[Y]);
    rtip->mdl_max[Z] = ceil(rtip->mdl_max[Z]);

    MAT_IDN(Viewrotscale);
    bn_mat_angles(Viewrotscale, 270.0 + elev, 0.0, 270.0 - azim);

    /* Look at the center of the model */
    MAT_IDN(toEye);
    toEye[MDX] = -((rtip->mdl_max[X] + rtip->mdl_min[X]) / 2.0);
    toEye[MDY] = -((rtip->mdl_max[Y] + rtip->mdl_min[Y]) / 2.0);
    toEye[MDZ] = -((rtip->mdl_max[Z] + rtip->mdl_min[Z]) / 2.0);

    /* Fit a sphere to the model RPP, diameter is viewsize, unless
     * viewsize command used to override.
     */
    if (viewsize <= 0) {
	VSUB2(diag, rtip->mdl_max, rtip->mdl_min);
	viewsize = MAGNITUDE(diag);
	if (aspect > 1) {
	    /* don't clip any of the image when autoscaling */
	    viewsize *= aspect;
	}
    }

    /* sanity check: make sure viewsize still isn't zero in case
     * bounding box is empty, otherwise bn_mat_int() will bomb.
     */
    if (viewsize < 0 || ZERO(viewsize)) {
	viewsize = 2.0; /* arbitrary so Viewrotscale is normal */
    }

    Viewrotscale[15] = 0.5 * viewsize;	/* Viewscale */
    bn_mat_mul(model2view, Viewrotscale, toEye);
    bn_mat_inv(view2model, model2view);
    VSET(temp, 0, 0, eye_backoff);
    MAT4X3PNT(eye_model, view2model, temp);
}

asf::auto_release_ptr<asr::Project> build_project(const char* UNUSED(file), const char* UNUSED(objects))
{
    /* If user gave no sizing info at all, use 512 as default */
    struct bu_vls dimensions = BU_VLS_INIT_ZERO;
    if (width <= 0)
	width = 512;
    if (height <= 0)
	height = 512;

    // Create an empty project.
    asf::auto_release_ptr<asr::Project> project(asr::ProjectFactory::create("test project"));
    project->search_paths().push_back_explicit_path("build/Debug");
    // ***** add precompiled shaders from appleseed
    char root[MAXPATHLEN];
    project->search_paths().push_back_explicit_path(bu_dir(root, MAXPATHLEN, APPLESEED_ROOT, "shaders/appleseed", NULL));
    project->search_paths().push_back_explicit_path(bu_dir(root, MAXPATHLEN, APPLESEED_ROOT, "shaders/max", NULL));

    // Add default configurations to the project.
    project->add_default_configurations();

    // Set the number of samples. This is the main quality parameter: the higher the
    // number of samples, the smoother the image but the longer the rendering time.
    // we overwrite via command line -c "set"
    project->configurations()
    .get_by_name("final")->get_parameters()
    .insert_path("uniform_pixel_renderer.samples", samples)
    .insert_path("rendering_threads", "1"); /* multithreading not supported yet */

    project->configurations()
	.get_by_name("interactive")->get_parameters()
	.insert_path("rendering_threads", "1"); /* no multithreading - for debug rendering on appleseed */

    // Create a scene.
    asf::auto_release_ptr<asr::Scene> scene(asr::SceneFactory::create());
    // asr::Scene scene(asr::SceneFactory::create());

    // Create an assembly.
    asf::auto_release_ptr<asr::Assembly> assembly(
	asr::AssemblyFactory().create(
	    "assembly",
	    asr::ParamArray()));

    //------------------------------------------------------------------------
    // Materials
    //------------------------------------------------------------------------

    // walk the db to register all regions
    struct db_tree_state state = rt_initial_tree_state;
    state.ts_dbip = APP.a_rt_i->rti_dbip;
    state.ts_resp = resources;

    db_walk_tree(APP.a_rt_i->rti_dbip, objc, (const char**)objv, 1, &state, register_region, NULL, NULL, reinterpret_cast<void *>(scene.get()));

    //------------------------------------------------------------------------
    // Light
    //------------------------------------------------------------------------

    // Create a color called "light_intensity" and insert it into the assembly.
    static const float LightRadiance[] = { 1.0f, 1.0f, 1.0f };
    assembly->colors().insert(
	asr::ColorEntityFactory::create(
	    "light_intensity",
	    asr::ParamArray()
	    .insert("color_space", "srgb")
	    .insert("multiplier", "30.0"),
	    asr::ColorValueArray(3, LightRadiance)));

    // Create a point light called "light" and insert it into the assembly.
    asf::auto_release_ptr<asr::Light> light(
	asr::PointLightFactory().create(
	    "light",
	    asr::ParamArray()
	    .insert("intensity", "light_intensity")));
    light->set_transform(
	asf::Transformd::from_local_to_parent(
	    asf::Matrix4d::make_translation(asf::Vector3d(0.6, 2.0, 1.0))));
    assembly->lights().insert(light);

    //------------------------------------------------------------------------
    // Assembly instance
    //------------------------------------------------------------------------

    // Create an instance of the assembly and insert it into the scene.
    asf::auto_release_ptr<asr::AssemblyInstance> assembly_instance(
	asr::AssemblyInstanceFactory::create(
	    "assembly_inst",
	    asr::ParamArray(),
	    "assembly"));
    assembly_instance
    ->transform_sequence()
    .set_transform(
	0.0f,
	asf::Transformd::identity());
    scene->assembly_instances().insert(assembly_instance);

    // Insert the assembly into the scene.
    scene->assemblies().insert(assembly);

    //------------------------------------------------------------------------
    // Environment
    //------------------------------------------------------------------------

    // OPTIONAL: this creates a background color
    // static const float SkyRadiance[] = { 0.75f, 0.80f, 1.0f };
    // statically making this 'white' for now so we don't blue wash the image
    // Create a color called "sky_radiance" and insert it into the scene.
    static const float SkyRadiance[] = { 1.0f, 1.0f, 1.0f };
    scene->colors().insert(
	asr::ColorEntityFactory::create(
	    "sky_radiance",
	    asr::ParamArray()
	    .insert("color_space", "srgb")
	    .insert("multiplier", "0.5"),
	    asr::ColorValueArray(3, SkyRadiance)));

    // Create an environment EDF called "sky_edf" and insert it into the scene.
    scene->environment_edfs().insert(
	asr::ConstantEnvironmentEDFFactory().create(
	    "sky_edf",
	    asr::ParamArray()
	    .insert("radiance", "sky_radiance")));

    // Create an environment shader called "sky_shader" and insert it into the scene.
    scene->environment_shaders().insert(
	asr::EDFEnvironmentShaderFactory().create(
	    "sky_shader",
	    asr::ParamArray()
	    .insert("environment_edf", "sky_edf")));

    // Create an environment called "sky" and bind it to the scene.
    scene->set_environment(
	asr::EnvironmentFactory::create(
	    "sky",
	    asr::ParamArray()
	    .insert("environment_edf", "sky_edf")
	    .insert("environment_shader", "sky_shader")));

    //------------------------------------------------------------------------
    // Camera
    //------------------------------------------------------------------------

    // Create a pinhole camera with film dimensions
    bu_vls_sprintf(&dimensions, "%f %f", 0.08 * (double) width / height, 0.08);
    asf::auto_release_ptr<asr::Camera> camera(
	asr::PinholeCameraFactory().create(
	    "camera",
	    asr::ParamArray()
	    .insert("film_dimensions", bu_vls_cstr(&dimensions))
	    .insert("focal_length", "0.035")));

    // Place and orient the camera. By default cameras are located in (0.0, 0.0, 0.0)
    // and are looking toward Z- (0.0, 0.0, -1.0).
    camera->transform_sequence().set_transform(
	0.0f,
	asf::Transformd::from_local_to_parent(
	    asf::Matrix4d::make_translation(asf::Vector3d(eye_model[0], eye_model[2], -eye_model[1])) * /* camera location */
	    asf::Matrix4d::make_rotation(asf::Vector3d(0.0, 1.0, 0.0), asf::deg_to_rad(azimuth - 270)) * /* azimuth */
	    asf::Matrix4d::make_rotation(asf::Vector3d(1.0, 0.0, 0.0), asf::deg_to_rad(-elevation)) /* elevation */
	));

    // Bind the camera to the scene.
    scene->cameras().insert(camera);

    //------------------------------------------------------------------------
    // Frame
    //------------------------------------------------------------------------

    // Create a frame and bind it to the project.
    bu_vls_sprintf(&dimensions, "%zd %zd", width, height);
    project->set_frame(
	asr::FrameFactory::create(
	    "beauty",
	    asr::ParamArray()
	    .insert("camera", "camera")
	    .insert("resolution", bu_vls_cstr(&dimensions))));

    // Bind the scene to the project.
    project->set_scene(scene);

    return project;
}


int
main(int argc, char **argv)
{
    // Create a log target that outputs to stderr, and binds it to the renderer's global logger.
    // Eventually you will probably want to redirect log messages to your own target. For this
    // you will need to implement foundation::ILogTarget (foundation/utility/log/ilogtarget.h).
    std::unique_ptr<asf::ILogTarget> log_target(asf::create_console_log_target(stderr));
    asr::global_logger().add_target(log_target.get());

    // Print appleseed's version string.
    RENDERER_LOG_INFO("%s", asf::Appleseed::get_synthetic_version_string());

    struct rt_i* rtip;
    const char *title_file = NULL;
    //const char *title_obj = NULL;	/* name of file and first object */
    struct bu_vls str = BU_VLS_INIT_ZERO;
    //int objs_free_argv = 0;

    bu_setprogname(argv[0]);

    // initialize options and overload menu before parsing
    init_options();

    /* Process command line options */
    int i = get_args(argc, (const char**)argv);
    if (i < 0) {
	usage(argv[0], 0);
	return 1;
    }
    // explicitly asking for help
    else if (i == 0) {
	usage(argv[0], 99);
	return 0;
    }

    if (bu_optind >= argc) {
	RENDERER_LOG_INFO("%s: BRL-CAD geometry database not specified\n", argv[0]);
	usage(argv[0], 0);
	return 1;
    }

    title_file = argv[bu_optind];
    //title_obj = argv[bu_optind + 1];
    if (!objv) {
	objc = argc - bu_optind - 1;
	if (objc) {
	    objv = (char **)&(argv[bu_optind+1]);
	} else {
	    /* No objects in either input file or argv - try getting objs from
	     * command processing.  Initialize the table. */
	    BU_GET(cmd_objs, struct bu_ptbl);
	    bu_ptbl_init(cmd_objs, 8, "initialize cmdobjs table");

      // log and gracefully exit for now
      bu_exit(EXIT_FAILURE, "No Region specified\n");
	}
    } else {
	//objs_free_argv = 1;
    }

    bu_vls_from_argv(&str, objc, (const char**)objv);

    resources = static_cast<resource*>(bu_calloc(1, sizeof(resource) * MAX_PSW, "appleseed"));
    char title[1024] = { 0 };

    /* load the specified geometry database */
    rtip = rt_dirbuild(title_file, title, sizeof(title));
    if (rtip == RTI_NULL)
    {
	RENDERER_LOG_INFO("building the database directory for [%s] FAILED\n", title_file);
	return -1;
    }

    for (int ic = 0; ic < MAX_PSW; ic++) {
	rt_init_resource(&resources[ic], ic, rtip);
	RT_CK_RESOURCE(&resources[ic]);
    }

    /* print optional title */
    if (title[0])
    {
	RENDERER_LOG_INFO("database title: %s\n", title);
    }

    /* include objects from database */
    if (rt_gettrees(rtip, objc, (const char**)objv, npsw) < 0)
    {
	RENDERER_LOG_INFO("loading the geometry for [%s...] FAILED\n", objv[0]);
	return -1;
    }

    /* prepare database for raytracing */
    if (rtip->needprep)
	rt_prep_parallel(rtip, 1);

    /* initialize values in application struct */
    RT_APPLICATION_INIT(&APP);

    /* configure raytrace application */
    APP.a_rt_i = rtip;
    APP.a_onehit = 1;
    APP.a_hit = brlcad_hit;
    APP.a_miss = brlcad_miss;

    do_ae(azimuth, elevation);
    RENDERER_LOG_INFO("View model: (%f, %f, %f)", eye_model[0], -eye_model[2], eye_model[1]);

    // Build the project.
    asf::auto_release_ptr<asr::Project> project(build_project(title_file, bu_vls_cstr(&str)));


    // Create the master renderer.
    asr::DefaultRendererController renderer_controller;
    asf::SearchPaths resource_search_paths;
    std::unique_ptr<asr::MasterRenderer> renderer(
	new asr::MasterRenderer(
	    project.ref(),
	    project->configurations().get_by_name("final")->get_inherited_parameters(),
	    resource_search_paths));

    // Render the frame.
    renderer->render(renderer_controller);

    // Save the frame to disk.
    char *default_out = bu_strdup("output/art.png");
    outputfile = default_out;
    project->get_frame()->write_main_image(outputfile);
    bu_free(default_out, "default name");

    // Save the project to disk.
    asr::ProjectFileWriter::write(project.ref(), "output/objects.appleseed");

    // Make sure to delete the master renderer before the project and the logger / log target.
    renderer.reset();

    // clean up resources
    bu_free(resources, "appleseed");

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
