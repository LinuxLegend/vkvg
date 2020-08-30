/*
 * Copyright (c) 2018-2020 Jean-Philippe Bruyère <jp_bruyere@hotmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
 * Software, and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef VKVG_H
#define VKVG_H

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************
 * Doxygen documentation
 *************************************************************************/

/*! @file vkvg.h
 *  @brief The header of the VKVG library.
 *
 *  This is the header file of the VKVG library.  It defines all its types and
 *  declares all its functions.
 *
 *  For more information about how to use this file, see @ref build_include.
 */
/*! @defgroup device Device reference
 *  @brief Functions and types related to VKVG device .
 *
 *  This is the reference documentation for creating, using and destroying VKVG
 *  Devices used to connect to vulkan context.
 */
/*! @defgroup surface Surface reference
 *  @brief Functions and types related to VKVG device .
 *
 *  This is the reference documentation for creating, using and destroying VKVG
 *  Surfaces used as backend for drawing operations.
 */
/*! @defgroup context Context reference
 *  @brief Functions and types related to VKVG contexts.
 *
 *  This is the reference documentation for VKVG contexts used to draw on @ref surface.
 */
/*! @defgroup path Path creation and manipulation reference.
 *  @brief Functions and types related to path edition.
 */

#include <vulkan/vulkan.h>
#include <math.h>
#include <stdbool.h>

/*! @name GLFW version macros
 *  @{ */
/*! @brief The major version number of the VKVG library.
 *
 *  This is incremented when the API is changed in non-compatible ways.
 *  @ingroup init
 */
#define VKVG_VERSION_MAJOR          0
/*! @brief The minor version number of the VKVG library.
 *
 *  This is incremented when features are added to the API but it remains
 *  backward-compatible.
 *  @ingroup init
 */
#define VKVG_VERSION_MINOR          1
/*! @brief The revision number of the VKVG library.
 *
 *  This is incremented when a bug fix release is made that does not contain any
 *  API changes.
 *  @ingroup init
 */
#define VKVG_VERSION_REVISION       1
/*! @} */

#define VKVG_LOG_ERR		0x10
#define VKVG_LOG_DEBUG		0x20
#define VKVG_LOG_INFO		0x40
#define VKVG_LOG_INFO_PATH	0x41
#define VKVG_LOG_DBG_ARRAYS	0x80
#define VKVG_LOG_FULL		0xff

#ifdef DEBUG
extern uint8_t vkvg_log_level;
#define LOG(level,...) (vkvg_log_level & level) ? fprintf (stdout, __VA_ARGS__):true;
#else
#define LOG
#endif

typedef enum {
	VKVG_STATUS_SUCCESS = 0,
	VKVG_STATUS_NO_MEMORY,
	VKVG_STATUS_INVALID_RESTORE,
	VKVG_STATUS_INVALID_POP_GROUP,
	VKVG_STATUS_NO_CURRENT_POINT,
	VKVG_STATUS_INVALID_MATRIX,
	VKVG_STATUS_INVALID_STATUS,
	VKVG_STATUS_NULL_POINTER,
	VKVG_STATUS_INVALID_STRING,
	VKVG_STATUS_INVALID_PATH_DATA,
	VKVG_STATUS_READ_ERROR,
	VKVG_STATUS_WRITE_ERROR,
	VKVG_STATUS_SURFACE_FINISHED,
	VKVG_STATUS_SURFACE_TYPE_MISMATCH,
	VKVG_STATUS_PATTERN_TYPE_MISMATCH,
	VKVG_STATUS_INVALID_CONTENT,
	VKVG_STATUS_INVALID_FORMAT,
	VKVG_STATUS_INVALID_VISUAL,
	VKVG_STATUS_FILE_NOT_FOUND,
	VKVG_STATUS_INVALID_DASH,
	VKVG_STAtUS_NOT_ENOUGH_POINTS_TO_CLOSE_PATH
}vkvg_status_t;

typedef enum {
	VKVG_HORIZONTAL	= 0,
	VKVG_VERTICAL	= 1
}vkvg_direction_t;

typedef enum {
	VKVG_FORMAT_ARGB32,
	VKVG_FORMAT_RGB24,
	VKVG_FORMAT_A8,
	VKVG_FORMAT_A1
} vkvg_format_t;

typedef enum {
	VKVG_EXTEND_NONE,
	VKVG_EXTEND_REPEAT,
	VKVG_EXTEND_REFLECT,
	VKVG_EXTEND_PAD
} vkvg_extend_t;

typedef enum {
	VKVG_FILTER_FAST,
	VKVG_FILTER_GOOD,
	VKVG_FILTER_BEST,
	VKVG_FILTER_NEAREST,
	VKVG_FILTER_BILINEAR,
	VKVG_FILTER_GAUSSIAN,
} vkvg_filter_t;

typedef enum {
	VKVG_PATTERN_TYPE_SOLID,
	VKVG_PATTERN_TYPE_SURFACE,
	VKVG_PATTERN_TYPE_LINEAR,
	VKVG_PATTERN_TYPE_RADIAL,
	VKVG_PATTERN_TYPE_MESH,
	VKVG_PATTERN_TYPE_RASTER_SOURCE,
} vkvg_pattern_type_t;

typedef enum {
	VKVG_LINE_CAP_BUTT,
	VKVG_LINE_CAP_ROUND,
	VKVG_LINE_CAP_SQUARE
} vkvg_line_cap_t;


typedef enum {
	VKVG_LINE_JOIN_MITER,
	VKVG_LINE_JOIN_ROUND,
	VKVG_LINE_JOIN_BEVEL
} vkvg_line_join_t;

typedef enum {
	VKVG_FILL_RULE_EVEN_ODD,
	VKVG_FILL_RULE_NON_ZERO
} vkvg_fill_rule_t;

typedef struct {
	float r;
	float g;
	float b;
	float a;
} vkvg_color_t;

#define VKVG_IDENTITY_MATRIX {1,0,0,1,0,0}
/**
 * @xx: xx component of the affine transformation
 * @yx: yx component of the affine transformation
 * @xy: xy component of the affine transformation
 * @yy: yy component of the affine transformation
 * @x0: X translation component of the affine transformation
 * @y0: Y translation component of the affine transformation
 *
 * A #vkvg_matrix_t holds an affine transformation, such as a scale,
 * rotation, shear, or a combination of those. The transformation of
 * a point (x, y) is given by:
 * <programlisting>
 *     x_new = xx * x + xy * y + x0;
 *     y_new = yx * x + yy * y + y0;
 * </programlisting>
 */
typedef struct {
	float xx; float yx;
	float xy; float yy;
	float x0; float y0;
} vkvg_matrix_t;

typedef struct {
	float ascent;
	float descent;
	float height;
	float max_x_advance;
	float max_y_advance;
} vkvg_font_extents_t;

typedef struct {
	float x_bearing;
	float y_bearing;
	float width;
	float height;
	float x_advance;
	float y_advance;
} vkvg_text_extents_t;

/**
 * @brief Opaque pointer on a vkvg text run.
 * A #VkvgText is an intermediate representation
 * of a text to be drawn on a #VkvgSurface.
 * It contains the measurments computed for character poisitioning.
 *
 * This object is used to speed up the rendering of the same
 * text with the same font multiple times.
 *
 * Drawing text with @ref vkvg_show_text implicitly create such intermediate structure
 * that is destroyed imediatly after the function call.
 */
typedef struct _vkvg_text_run_t* VkvgText;

/**
 * @brief Opaque pointer on a Vkvg Context structure.
 *
 * A #VkvgContext is the central object for drawing operations.
 * #vkvg_context_t structure internals this pointer point to are
 * hidden to the client application.
 */
typedef struct _vkvg_context_t* VkvgContext;
/**
 * @brief Opaque pointer on a Vkvg Surface structure.
 *
 * A #VkvgSurface represents an image, either as the destination
 * of a drawing operation or as source when drawing onto another
 * surface.  To draw to a #VkvgSurface, create a vkvg context
 * with the surface as the target, using vkvg_create().
 * hidden internals.
 *
 * #VkvgSurface are created with a @ref VkvgDevice which has to stay
 * active as long has the surface is in use.
 */
typedef struct _vkvg_surface_t* VkvgSurface;
/**
 * @brief Opaque pointer on a Vkvg device structure.
 *
 * A #VkvgDevice is required for creating new surfaces.
 */
typedef struct _vkvg_device_t*  VkvgDevice;
/**
 * @brief Opaque pointer on a Vkvg pattern structure.
 *
 * Patterns are images to be drawn on surface with several
 * configurable parameters such as the wrap mode, the filtering, etc...
 */
typedef struct _vkvg_pattern_t* VkvgPattern;
/**
 * @brief Create a new vkvg device.
 *
 * On success, create a new vkvg device set its reference count to 1.
 *
 * @param inst Vulkan instance to create the device from.
 * @param phy Vulkan physical device used to create the vkvg device.
 * @param vkdev Vulkan logical device to create the vkvg device for.
 * @param qFamIdx Queue family Index of the graphic queue used for drawing operations.
 * @param qIndex Index of the queue into the choosen familly, 0 in general.
 * @return The handle of the created vkvg device, or null if an error occured.
 */
VkvgDevice vkvg_device_create (VkInstance inst, VkPhysicalDevice phy, VkDevice vkdev, uint32_t qFamIdx, uint32_t qIndex);
/**
 * @brief Create a new multisampled vkvg device.
 *
 * This function allows to create vkvg device for working with multisampled surfaces.
 * Multisampling is used to smooth color transitions in drawings, making lines not pixelised and
 * diagonal edges not stepped. Multisampling has a performance cost.
 * The final image of the surface, accessible by the user will be a resolved single sampled image.
 *
 * @param inst Vulkan instance to create the device from.
 * @param phy Vulkan physical device used to create the vkvg device.
 * @param vkdev Vulkan logical device to create the vkvg device for.
 * @param qFamIdx Queue family Index of the graphic queue used for drawing operations.
 * @param qIndex Index of the queue into the choosen familly, 0 in general.
 * @param samples The sample count that will be setup for the surfaces created by this device.
 * @param deferredResolve If true, the final simple sampled image of the surface will only be resolved on demand
 * when calling @ref vkvg_surface_get_vk_image or by explicitly calling @ref vkvg_multisample_surface_resolve. If false, multisampled image is resolved on each draw operation.
 * @return The handle of the created vkvg device, or null if an error occured.
 */
VkvgDevice vkvg_device_create_multisample (VkInstance inst, VkPhysicalDevice phy, VkDevice vkdev, uint32_t qFamIdx, uint32_t qIndex, VkSampleCountFlags samples, bool deferredResolve);
/**
 * @brief Decrement the reference count of the device by 1. Release all it's ressources if count reach 0.
 *
 * If device is reference by several active surfaces, calling destroy will only decrement the reference count by 1.
 * When the reference count reach 0, the vkvg device is effectively destroyed, the device pointer is freed, and
 * vulkan objects are released.
 * Vkvg Devices has to stay active as long as surfaces created by it are used.
 * @param dev The vkvg device to destroy.
 */
void vkvg_device_destroy (VkvgDevice dev);
/**
 * @brief Increment by one the reference count on the device.
 * @param The vkvg device pointer to increment reference for.
 * @return ?
 */
VkvgDevice vkvg_device_reference (VkvgDevice dev);
/**
 * @brief Get the actual reference count on this device.
 * @param dev The vkvg device to get the reference of.
 * @return The reference count on this device.
 */
uint32_t vkvg_device_get_reference_count (VkvgDevice dev);
/**
 * @brief Set the screen dot per inch for this device.
 *
 * Set the current values for horizontal and vertical dot per inch of the screen for this device.
 * @param dev The vkvg device to set the dpy for.
 * @param hdpy Horizontal dot per inch.
 * @param vdpy Vertical dot per inch.
 */
void vkvg_device_set_dpy (VkvgDevice dev, int hdpy, int vdpy);
/**
 * @brief Get the current dpy values.
 *
 * Get the current values for horizontal and vertical dot per inch of the screen for this device.
 * The default values for horizontal and vertical dpy on device creation is 96.
 * @param dev The vkvg device to get the dpy configuration for.
 * @param hdpy[out] The current horizontal dot per inch.
 * @param vdpy[out] The current vertical dot per inch.
 */
void vkvg_device_get_dpy (VkvgDevice dev, int* hdpy, int* vdpy);
/**
 * @brief Create a new vkvg surface.
 * @param dev The vkvg device used for creating the surface.
 * @param width Width in pixel of the surface to create.
 * @param height Height in pixel of the surface to create.
 * @return The new vkvg surface pointer, or null if an error occured.
 */
VkvgSurface vkvg_surface_create             (VkvgDevice dev, uint32_t width, uint32_t height);
/**
 * @brief Create a new vkvg surface by loading an image file. The resulting surface will have the same dimension as the supplied image.
 *
 * @param dev The vkvg device used for creating the surface.
 * @param filePath The path of the image to load for creating the surface.
 * @return The new vkvg surface with the loaded image as content, or null if an error occured.
 */
VkvgSurface vkvg_surface_create_from_image  (VkvgDevice dev, const char* filePath);
/**
 * @brief Create a new vkvg surface by loading a SVG file.
 * @param dev The vkvg device used for creating the surface.
 * @param filePath The path of the SVG file to load.
 * @return The new vkvg surface with the loaded SVG drawing as content, or null if an error occured.
 */
VkvgSurface vkvg_surface_create_from_svg    (VkvgDevice dev, const char* filePath);
/**
 * @brief Create a new vkvg surface by parsing a string with a valid SVG fragment passed as argument.
 * @param dev The vkvg device used for creating the surface.
 * @param fragment The SVG fragment to parse.
 * @return The new vkvg surface with the parsed SVG fragment as content, or null if an error occured.
 */
VkvgSurface vkvg_surface_create_from_svg_fragment (VkvgDevice dev, char *fragment);
/**
 * @brief Create a new vkvg surface that will used an existing vulkan texture as backend.
 * @param dev The vkvg device used for creating the surface.
 * @param vkhImg The VkhImage to use as the backend texture for drawing operations.
 * @return A new surface, or null if an error occured.
 */
VkvgSurface vkvg_surface_create_for_VkhImage(VkvgDevice dev, void* vkhImg);
// VkvgSurface vkvg_surface_create_from_bitmap (VkvgDevice dev, unsigned char* img, uint32_t width, uint32_t height);
/**
 * @brief Increment reference count on the surface by one
 * @param The vkvg surface to increment the reference count for.
 * @return ?
 */
VkvgSurface vkvg_surface_reference          (VkvgSurface surf);
/**
 * @brief Get the current reference count on this surface.
 * @param The vkvg surface to get the reference count for.
 * @return The reference count on the surface.
 */
uint32_t    vkvg_surface_get_reference_count(VkvgSurface surf);
/**
 * @brief Decrement the reference count on the surface by one. Destroy it if count reach 0.
 * @param The vkvg surface to destroy.
 */
void		vkvg_surface_destroy		(VkvgSurface surf);
/**
 * @brief Clear the surface content, alpha is also set to 0 resulting in a transparent image.
 *
 * @remark Internaly, the vulkan method used to clear the surface is the slowest, prefer using the @ref vkvg_clear
 * function of the context that will try to use the render pass load operations when possible.
 * @param The surface to clear.
 */
void        vkvg_surface_clear          (VkvgSurface surf);
/**
 * @brief Get the final single sampled vulkan image of this surface.
 * @param The vkvg surface to get the vulkan texture of.
 * @return The VkImage object containing the result of the drawing operations on the surface.
 */
VkImage		vkvg_surface_get_vk_image	(VkvgSurface surf);
/**
 * @brief Get the vulkan format of the vulkan texture used as backend for this surface.
 * @param The surface to get the format for.
 * @return The VkFormat.
 */
VkFormat	vkvg_surface_get_vk_format	(VkvgSurface surf);
/**
 * @brief Get the actual surface width.
 * @param The vkvg surface to get the width for.
 * @return The width in pixel of the surface.
 */
uint32_t   	vkvg_surface_get_width      (VkvgSurface surf);
/**
 * @brief Get the actual surface height.
 * @param The vkvg surface to get the height for.
 * @return The height in pixel of the surface.
 */
uint32_t	vkvg_surface_get_height     (VkvgSurface surf);
/**
 * @brief Write surface content to a png file on disk.
 * @param The surface to save on disk.
 * @param The png file path.
 */
void        vkvg_surface_write_to_png   (VkvgSurface surf, const char* path);
/**
 * @brief Explicitly resolve a multisampled surface.
 *
 * When creating a multisampled vkvg device with deffered resolve set to true, this function has to be
 * called to have the multisampled texture used for drawing operations resolved on the single sampled accessible
 * texture. Note that a call to @ref vkvg_surface_get_vk_image will automaticaly call this method before returning
 * the backend image.
 *
 * @param The vkvg surface to resolve.
 */
void        vkvg_multisample_surface_resolve (VkvgSurface surf);

//nsvg interface for easy svg drawing
typedef struct NSVGimage NSVGimage;

NSVGimage*  nsvg_load_file   (VkvgDevice dev, const char* filePath);
NSVGimage*  nsvg_load        (VkvgDevice dev, char* fragment);
void        nsvg_destroy     (NSVGimage* svg);
void        nsvg_get_size    (NSVGimage* svg, int* width, int* height);
void        vkvg_render_svg  (VkvgContext ctx, NSVGimage* svg, char* subId);


//mimic from cairo, to facilitate usage of vkvg as cairo vulkan backend
typedef enum _vkvg_operator {
	VKVG_OPERATOR_CLEAR,

	VKVG_OPERATOR_SOURCE,
	VKVG_OPERATOR_OVER,
	VKVG_OPERATOR_IN,
	VKVG_OPERATOR_OUT,
	VKVG_OPERATOR_ATOP,

	VKVG_OPERATOR_DEST,
	VKVG_OPERATOR_DEST_OVER,
	VKVG_OPERATOR_DEST_IN,
	VKVG_OPERATOR_DEST_OUT,
	VKVG_OPERATOR_DEST_ATOP,

	VKVG_OPERATOR_XOR,
	VKVG_OPERATOR_ADD,
	VKVG_OPERATOR_SATURATE,

	VKVG_OPERATOR_MULTIPLY,
	VKVG_OPERATOR_SCREEN,
	VKVG_OPERATOR_OVERLAY,
	VKVG_OPERATOR_DARKEN,
	VKVG_OPERATOR_LIGHTEN,
	VKVG_OPERATOR_COLOR_DODGE,
	VKVG_OPERATOR_COLOR_BURN,
	VKVG_OPERATOR_HARD_LIGHT,
	VKVG_OPERATOR_SOFT_LIGHT,
	VKVG_OPERATOR_DIFFERENCE,
	VKVG_OPERATOR_EXCLUSION,
	VKVG_OPERATOR_HSL_HUE,
	VKVG_OPERATOR_HSL_SATURATION,
	VKVG_OPERATOR_HSL_COLOR,
	VKVG_OPERATOR_HSL_LUMINOSITY
} vkvg_operator_t;

/**
 * @brief Create a new vkvg context used for drawing on surfaces.
 * @param surf The target surface of the drawing operations.
 * @return A new #VkvgContext or null if an error occured.
 */
VkvgContext vkvg_create (VkvgSurface surf);
/**
 * @brief Decrement the reference count on the context by one. Destroy it if count reach 0.
 * @param ctx The vkvg context to destroy.
 */
void vkvg_destroy (VkvgContext ctx);
/**
 * @brief Increment by one the reference count on this context.
 * @param ctx The context to increment the reference count for.
 * @return
 */
VkvgContext vkvg_reference (VkvgContext ctx);
/**
 * @brief Get the current reference count of this context.
 * @param The vkvg context to query.
 * @return The current reference count for this context.
 */
uint32_t vkvg_get_reference_count (VkvgContext ctx);
/**
 * @brief Perform all the pending drawing operations on a context.
 *
 * Drawing operations by a context on a surface are delayed in several cases
 * for optimisation purpose. Calling #vkvg_flush on a context ensure that
 * all possibly delayed drawing command are executed.
 * The context is automatically flushed on destruction.
 * @param ctx The vkvg context to flush.
 */
void vkvg_flush (VkvgContext ctx);
/**
 * @brief Start a new empty path.
 *
 * Clear current path stored in the context without drawing anything on the target surface.
 * The current position is reseted.
 * @param ctx The vkvg context pointer.
 */
void vkvg_new_path (VkvgContext ctx);
/**
 * @brief Close the current path.
 *
 * Close the last path emited for this context if its point count is greater than 2.
 * This function has no effect if the current path is empty or if its point count is less
 * than 2.
 * This function is automatically call when you try to perform a filling operation on an unclosed path.
 * @param ctx The vkvg context pointer.
 */
void vkvg_close_path (VkvgContext ctx);
/**
 * @brief Start new sub path, no current point is defined
 *
 * If the current path is empty, this function has no effect.
 * @param ctx The vkvg context pointer.
 */
void vkvg_new_sub_path (VkvgContext ctx);
/**
 * @brief Add a line to the current path from the current point to the coordinate given in arguments.
 *
 * After this call, the current position will be (x,y).
 * If no current point is defined, this function call is equivalent to a
 * single #vkvg_move_to command.
 * @param ctx The vkvg context pointer.
 * @param x absolute x coordinate of second point
 * @param y aboslute y coordinate of second point
 */
void vkvg_line_to (VkvgContext ctx, float x, float y);
/**
 * @brief Add a line to the current path from the current point to the coordinate relative to it.
 *
 * Given a current point of (x, y), after this call the current point will be  (x + dx, y + dy).
 * If no current point is defined, this function call is equivalent to a
 * single #move_to command to the absolute position given by the x and y arguments.
 * @param ctx The vkvg context pointer.
 * @param dx delta x
 * @param dy delta y
 */
void vkvg_rel_line_to (VkvgContext ctx, float dx, float dy);
/**
 * @brief Move the context pen to the position given in argument.
 *
 * This command set the current pen position of the context to the coordinate
 * given by the x and y arguments.
 * If the current path is not empty, this command will start a new
 * subpath and set the current pen position to the coordinates given in
 * arguments.
 * @param ctx The vkvg context pointer.
 * @param x new x position of the pen
 * @param y new y position of the pen
 */
void vkvg_move_to (VkvgContext ctx, float x, float y);
/**
 * @brief Move the context pen relative to the current point.
 *
 * This command will set the current pen position of the context to the
 * coordinate given by the x and y deltas in arguments.
 * If the current path is not empty, this command will start a new
 * subpath and set the current pen position.
 * @param ctx The vkvg context pointer.
 * @param x delta in the horizontal direction.
 * @param y delta in the vertical direction.
 */
void vkvg_rel_move_to (VkvgContext ctx, float x, float y);
/**
 * @brief Adds a circular arc of the given radius to the current path.
 *
 * Adds a circular arc in clockwise order of the given radius to the current path following angles of a trigonometric circle.
 * After this call the current point will be the last computed point of the arc.
 * The arc is centered at (xc , yc ), begins at angle a1 and proceeds in the direction of increasing angles to end at angle a2.
 * If a2 is less than a1, it will be progressively increased by 2*PI until it is greater than a1.
 *
 * If there is a current point, an initial line segment will be added to the path to connect the current point to the beginning of the arc.
 * If this initial line is undesired, it can be avoided by calling vkvg_new_sub_path() before calling vkvg_arc().
 *
 * Angles are measured in radians. An angle of 0.0 is in the direction of the positive X axis.
 * An angle of PI/2 radians (90 degrees) is in the direction of the positive Y axis. Angles increase in the direction
 * from the positive X axis toward the positive Y axis. So with the default transformation matrix, angles increase in a clockwise direction.
 *
 * @remark To convert from degrees to radians, use degrees * (PI/180).
 * @param ctx The vkvg context pointer.
 * @param xc center x coordinate
 * @param xy center y coordinate
 * @param radius The radius of the arc.
 * @param a1 start angle in radians of the arc as if on a trigonometric circle.
 * @param a2 end angle in radians of the arc to draw.
 */
void vkvg_arc (VkvgContext ctx, float xc, float yc, float radius, float a1, float a2);
/**
 * @brief Add a circular arc in counter clockwise order to the current path.
 *
 * Adds a circular arc in counter clockwise order of the given radius to the current path following angles of a trigonometric circle.
 * After this call the current point will be the last computed point of the arc.
 * The arc is centered at (xc , yc ), begins at angle a1 and proceeds in the direction of decreasing angles to end at angle a2.
 * If a2 is greater than a1, it will be progressively decreased by 2*PI until it is less than a1.
 *
 * See @ref vkvg_arc for more details. This function differs only in the direction of the arc between the two angles.
 * @param ctx The vkvg context pointer.
 * @param xc center x coordinate
 * @param xy center y coordinate
 * @param radius The radius of the arc.
 * @param a1 start angle in radians of the arc as if on a trigonometric circle.
 * @param a2 end angle in radians of the arc to draw.
 */
void vkvg_arc_negative (VkvgContext ctx, float xc, float yc, float radius, float a1, float a2);
/**
 * @brief Adds a cubic Bézier spline to the current path.
 *
 * Adds a cubic Bézier spline to the path from the current point to position (x3, y3), using (x1, y1) and (x2, y2) as the control points.
 * After this call the current point will be (x3, y3).
 *
 * If there is no current point before the call to vkvg_curve_to() this function will behave as if preceded by a call to vkvg_move_to(ctx, x1, y1).
 * @param ctx The vkvg context pointer.
 * @param x1 The X coordinate of the first control point.
 * @param y1 The Y coordinate of the first control point.
 * @param x2 The X coordinate of the second control point.
 * @param y2 The Y coordinate of the second control point.
 * @param x3 The X coordinate of the end of the curve.
 * @param y3 The Y coordinate of the end of the curve.
 */
void vkvg_curve_to (VkvgContext ctx, float x1, float y1, float x2, float y2, float x3, float y3);
/**
 * @brief Add an axis aligned rectangle subpath to the current path.
 *
 * Adds a closed sub-path rectangle of the given size to the current path at position (x, y).
 * @param ctx The vkvg context pointer.
 * @param x The x coordinate of the top left corner of the rectangle to emit.
 * @param y The y coordinate of the top left corner of the rectangle to emit.
 * @param w The width in pixel of the rectangle to draw.
 * @param h The height in pixel of the rectangle to draw.
 */
void vkvg_rectangle         (VkvgContext ctx, float x, float y, float w, float h);

void vkvg_fill_rectangle    (VkvgContext ctx, float x, float y, float w, float h);
void vkvg_stroke			(VkvgContext ctx);
void vkvg_stroke_preserve	(VkvgContext ctx);
void vkvg_fill				(VkvgContext ctx);
void vkvg_fill_preserve		(VkvgContext ctx);
void vkvg_paint             (VkvgContext ctx);
void vkvg_clear             (VkvgContext ctx);//use vkClearAttachment to speed up clearing surf
void vkvg_reset_clip        (VkvgContext ctx);
void vkvg_clip              (VkvgContext ctx);
void vkvg_clip_preserve     (VkvgContext ctx);
void vkvg_set_source_rgba	(VkvgContext ctx, float r, float g, float b, float a);
void vkvg_set_source_rgb    (VkvgContext ctx, float r, float g, float b);
void vkvg_set_line_width	(VkvgContext ctx, float width);
void vkvg_set_line_cap      (VkvgContext ctx, vkvg_line_cap_t cap);
void vkvg_set_line_join     (VkvgContext ctx, vkvg_line_join_t join);
void vkvg_set_source_surface(VkvgContext ctx, VkvgSurface surf, float x, float y);
void vkvg_set_source        (VkvgContext ctx, VkvgPattern pat);
void vkvg_set_operator      (VkvgContext ctx, vkvg_operator_t op);
void vkvg_set_fill_rule     (VkvgContext ctx, vkvg_fill_rule_t fr);

void vkvg_set_dash          (VkvgContext ctx, const float* dashes, uint32_t num_dashes, float offset);
void vkvg_get_dash          (VkvgContext ctx, const float *dashes, uint32_t* num_dashes, float* offset);

float               vkvg_get_line_width     (VkvgContext ctx);
vkvg_line_cap_t     vkvg_get_line_cap       (VkvgContext ctx);
vkvg_line_join_t    vkvg_get_line_join      (VkvgContext ctx);
vkvg_operator_t     vkvg_get_operator       (VkvgContext ctx);
vkvg_fill_rule_t    vkvg_get_fill_rule      (VkvgContext ctx);
VkvgPattern         vkvg_get_source         (VkvgContext ctx);

void vkvg_save              (VkvgContext ctx);
void vkvg_restore           (VkvgContext ctx);

void vkvg_translate         (VkvgContext ctx, float dx, float dy);
void vkvg_scale             (VkvgContext ctx, float sx, float sy);
void vkvg_rotate            (VkvgContext ctx, float radians);
void vkvg_transform         (VkvgContext ctx, const vkvg_matrix_t* matrix);
void vkvg_set_matrix        (VkvgContext ctx, const vkvg_matrix_t* matrix);
void vkvg_get_matrix        (VkvgContext ctx, const vkvg_matrix_t* matrix);
void vkvg_identity_matrix   (VkvgContext ctx);

//text
void vkvg_select_font_face	(VkvgContext ctx, const char* name);
void vkvg_set_font_size		(VkvgContext ctx, uint32_t size);
void vkvg_show_text			(VkvgContext ctx, const char* text);
void vkvg_text_extents      (VkvgContext ctx, const char* text, vkvg_text_extents_t* extents);
void vkvg_font_extents      (VkvgContext ctx, vkvg_font_extents_t* extents);

//text run holds harfbuz datas, and prevent recreating them multiple times for the same line of text.
VkvgText    vkvg_text_run_create    (VkvgContext ctx, const char* text);
void        vkvg_text_run_destroy   (VkvgText textRun);
void        vkvg_show_text_run      (VkvgContext ctx, VkvgText textRun);
void        vkvg_text_run_get_extents(VkvgText textRun, vkvg_text_extents_t* extents);

//pattern
VkvgPattern vkvg_pattern_reference          (VkvgPattern pat);
uint32_t    vkvg_pattern_get_reference_count(VkvgPattern pat);
VkvgPattern vkvg_pattern_create_for_surface (VkvgSurface surf);
VkvgPattern vkvg_pattern_create_linear      (float x0, float y0, float x1, float y1);
VkvgPattern vkvg_pattern_create_radial      (float cx0, float cy0, float radius0,
											 float cx1, float cy1, float radius1);
void        vkvg_pattern_destroy            (VkvgPattern pat);

void vkvg_pattern_add_color_stop(VkvgPattern pat, float offset, float r, float g, float b, float a);
void vkvg_pattern_set_extend    (VkvgPattern pat, vkvg_extend_t extend);
void vkvg_pattern_set_filter    (VkvgPattern pat, vkvg_filter_t filter);

vkvg_extend_t   vkvg_pattern_get_extend (VkvgPattern pat);
vkvg_filter_t   vkvg_pattern_get_filter (VkvgPattern pat);

//matrix
void vkvg_matrix_init_identity (vkvg_matrix_t *matrix);
void vkvg_matrix_init (vkvg_matrix_t *matrix,
		   float xx, float yx,
		   float xy, float yy,
		   float x0, float y0);
void vkvg_matrix_init_translate     (vkvg_matrix_t *matrix, float tx, float ty);
void vkvg_matrix_init_scale         (vkvg_matrix_t *matrix, float sx, float sy);
void vkvg_matrix_init_rotate        (vkvg_matrix_t *matrix, float radians);
void vkvg_matrix_translate          (vkvg_matrix_t *matrix, float tx, float ty);
void vkvg_matrix_scale              (vkvg_matrix_t *matrix, float sx, float sy);
void vkvg_matrix_rotate             (vkvg_matrix_t *matrix, float radians);
void vkvg_matrix_multiply           (vkvg_matrix_t *result, const vkvg_matrix_t *a, const vkvg_matrix_t *b);
void vkvg_matrix_transform_distance (const vkvg_matrix_t *matrix, float *dx, float *dy);
void vkvg_matrix_transform_point    (const vkvg_matrix_t *matrix, float *x, float *y);
void vkvg_matrix_invert             (vkvg_matrix_t *matrix);

#ifdef __cplusplus
}
#endif

#endif
