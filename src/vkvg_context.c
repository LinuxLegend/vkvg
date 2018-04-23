﻿/*
 * Copyright (c) 2018 Jean-Philippe Bruyère <jp_bruyere@hotmail.com>
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

#include "vkvg_device_internal.h"
#include "vkvg_context_internal.h"
#include "vkvg_surface_internal.h"
#include "vkvg_pattern.h"
#include "vkh_queue.h"
#include "seidel/interface.h"

#ifdef DEBUG
static vec2 debugLinePoints[1000];
static uint32_t dlpCount = 0;
#endif

VkvgContext vkvg_create(VkvgSurface surf)
{
    VkvgDevice dev = surf->dev;
    VkvgContext ctx = (vkvg_context*)calloc(1, sizeof(vkvg_context));

    ctx->sizePoints     = VKVG_PTS_SIZE;
    ctx->sizeVertices   = VKVG_VBO_SIZE;
    ctx->sizeIndices    = VKVG_IBO_SIZE;
    ctx->sizePathes     = VKVG_PATHES_SIZE;
    ctx->curPos.x       = ctx->curPos.y = 0;
    ctx->lineWidth      = 1;
    ctx->pSurf          = surf;

    push_constants pc = {
            {0,0,0,1},
            {(float)ctx->pSurf->width,(float)ctx->pSurf->height},
            VKVG_PATTERN_TYPE_SOLID,
            0,
            VKVG_IDENTITY_MATRIX,
            VKVG_IDENTITY_MATRIX
    };
    ctx->pushConsts = pc;

    ctx->pPrev          = surf->dev->lastCtx;
    if (ctx->pPrev != NULL)
        ctx->pPrev->pNext = ctx;
    surf->dev->lastCtx = ctx;

    ctx->selectedFont.fontFile = (char*)calloc(FONT_FILE_NAME_MAX_SIZE,sizeof(char));

    ctx->flushFence = vkh_fence_create(dev->vkDev);

    ctx->points = (vec2*)       malloc (VKVG_VBO_SIZE*sizeof(vec2));
    ctx->pathes = (uint32_t*)   malloc (VKVG_PATHES_SIZE*sizeof(uint32_t));

    ctx->cmdPool = vkh_cmd_pool_create (dev->vkDev, dev->gQueue->familyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    _create_vertices_buff   (ctx);
    _create_gradient_buff   (ctx);
    _create_cmd_buff        (ctx);
    _createDescriptorPool   (ctx);
    _init_descriptor_sets   (ctx);
    _update_descriptor_set  (ctx, ctx->pSurf->dev->fontCache->cacheTex, ctx->dsFont);
    _update_gradient_desc_set(ctx);
    _init_cmd_buff          (ctx);
    _clear_path             (ctx);

    return ctx;
}
void vkvg_flush (VkvgContext ctx){
    _flush_cmd_buff(ctx);
    _init_cmd_buff(ctx);

#ifdef DEBUG

    vec4 red = {0,0,1,1};
    vec4 green = {0,1,0,1};
    vec4 white = {1,1,1,1};

    int j = 0;
    while (j < dlpCount) {
        add_line(ctx, debugLinePoints[j], debugLinePoints[j+1],green);
        j+=2;
        add_line(ctx, debugLinePoints[j], debugLinePoints[j+1],red);
        j+=2;
        add_line(ctx, debugLinePoints[j], debugLinePoints[j+1],white);
        j+=2;
    }
    dlpCount = 0;
    vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pSurf->dev->pipelineLineList);
    vkCmdDrawIndexed(ctx->cmd, ctx->indCount-ctx->curIndStart, 1, ctx->curIndStart, 0, 1);
    _flush_cmd_buff(ctx);
#endif

}

void vkvg_destroy (VkvgContext ctx)
{
    _flush_cmd_buff(ctx);

    VkDevice dev = ctx->pSurf->dev->vkDev;

    vkDestroyFence      (dev, ctx->flushFence,NULL);
    vkFreeCommandBuffers(dev, ctx->cmdPool, 1, &ctx->cmd);
    vkDestroyCommandPool(dev, ctx->cmdPool, NULL);

    VkDescriptorSet dss[] = {ctx->dsFont, ctx->dsSrc, ctx->dsGrad};
    vkFreeDescriptorSets    (dev, ctx->descriptorPool, 3, dss);

    vkDestroyDescriptorPool (dev, ctx->descriptorPool,NULL);

    vkvg_buffer_destroy (&ctx->uboGrad);
    vkvg_buffer_destroy (&ctx->indices);
    vkvg_buffer_destroy (&ctx->vertices);

    //vkh_image_destroy   (ctx->source);

    free(ctx->selectedFont.fontFile);
    free(ctx->pathes);
    free(ctx->points);

    //free saved context stack elmt
    vkvg_context_save_t* next = ctx->pSavedCtxs;
    while (next != NULL) {
        vkvg_context_save_t* cur = next;
        next = cur->pNext;
        _free_ctx_save (cur);
    }

    if (ctx->pSurf->dev->lastCtx == ctx){
        ctx->pSurf->dev->lastCtx = ctx->pPrev;
        if (ctx->pPrev != NULL)
            ctx->pPrev->pNext = NULL;
    }else if (ctx->pPrev == NULL){
        //first elmt, and it's not last one so pnext is not null
        ctx->pNext->pPrev = NULL;
    }else{
        ctx->pPrev->pNext = ctx->pNext;
        ctx->pNext->pPrev = ctx->pPrev;
    }

    free(ctx);
}

void vkvg_close_path (VkvgContext ctx){
    if (ctx->pathPtr % 2 == 0)//current path is empty
        return;
    //check if at least 3 points are present
    if (ctx->pointCount - ctx->pathes [ctx->pathPtr-1] > 2){
        //set end idx of path to the same as start idx
        ctx->pathes[ctx->pathPtr] = ctx->pathes [ctx->pathPtr-1];
        //if last point of path is same pos as first point, remove it
        if (vec2_equ(ctx->points[ctx->pointCount-1], ctx->points[ctx->pathes[ctx->pathPtr]]))
            ctx->pointCount--;
        //start new path
        _check_pathes_array(ctx);
        ctx->pathPtr++;
    }
}
void vkvg_rel_line_to (VkvgContext ctx, float x, float y){
    vkvg_line_to(ctx, ctx->curPos.x + x, ctx->curPos.y + y);
}
void vkvg_line_to (VkvgContext ctx, float x, float y)
{
    vec2 p = {x,y};
    if (ctx->curPosExists){
        if (vec2_equ(ctx->curPos,p))
            return;
        if (_current_path_is_empty (ctx)){
            _start_sub_path (ctx);
            _add_curpos(ctx);
        }

        _add_point_cp_update(ctx, x, y);
    }else
        vkvg_move_to(ctx, x,y);
}

void vkvg_arc (VkvgContext ctx, float xc, float yc, float radius, float a1, float a2){
    while (a2 < a1)
        a2 += 2*M_PI;

    vec2 v = {cos(a1)*radius + xc, sin(a1)*radius + yc};

    float step = M_PI/radius;
    float a = a1;

    if (ctx->curPosExists){
        vkvg_line_to(ctx, v.x, v.y);
        a+=step;
    }else
        _set_current_point(ctx, v);

    if (a2 == a1)
        return;

    if (_current_path_is_empty (ctx))
        _start_sub_path (ctx);

    while(a < a2){
        v.x = cos(a)*radius + xc;
        v.y = sin(a)*radius + yc;
        _add_point (ctx,v.x,v.y);
        a+=step;
    }

    a = a2;
    vec2 lastP = v;
    v.x = cos(a)*radius + xc;
    v.y = sin(a)*radius + yc;
    if (vec2_equ (v,lastP))
        _set_current_point(ctx, v);
    else
        _add_point_cp_update(ctx,v.x,v.y);
}
void vkvg_arc_negative (VkvgContext ctx, float xc, float yc, float radius, float a1, float a2) {
    while (a2 > a1)
        a2 -= 2*M_PI;

    vec2 v = {cos(a1)*radius + xc, sin(a1)*radius + yc};

    float step = M_PI/radius;
    float a = a1;

    if (ctx->curPosExists){
        vkvg_line_to(ctx, v.x, v.y);
        a+=step;
    }else
        _set_current_point(ctx, v);

    if (a2 == a1)
        return;

    if (_current_path_is_empty (ctx))
        _start_sub_path (ctx);

    while(a > a2){
        v.x = cos(a)*radius + xc;
        v.y = sin(a)*radius + yc;
        _add_point (ctx,v.x,v.y);
        a-=step;
    }

    a = a2;
    vec2 lastP = v;
    v.x = cos(a)*radius + xc;
    v.y = sin(a)*radius + yc;
    if (vec2_equ (v,lastP))
        _set_current_point(ctx, v);
    else
        _add_point_cp_update(ctx,v.x,v.y);
}
void vkvg_rel_move_to (VkvgContext ctx, float x, float y)
{
    vkvg_move_to(ctx, ctx->curPos.x + x, ctx->curPos.y + y);
}
void vkvg_move_to (VkvgContext ctx, float x, float y)
{
    _finish_path(ctx);

    ctx->curPos.x = x;
    ctx->curPos.y = y;
    ctx->curPosExists = true;
}
void vkvg_curve_to (VkvgContext ctx, float x1, float y1, float x2, float y2, float x3, float y3) {
    vec2 p = ctx->curPos;
    vec2 p1 = {x1,y1};

    if (_current_path_is_empty(ctx)){
        _start_sub_path(ctx);
        if (!ctx->curPosExists)
            p = p1;
        if (!vec2_equ (p,p1))
            _add_point (ctx, p.x, p.y);
    }

    _recursive_bezier       (ctx, p.x, p.y, x1, y1, x2, y2, x3, y3, 0);
    _add_point_cp_update    (ctx, x3, y3);
}
void vkvg_rel_curve_to (VkvgContext ctx, float x1, float y1, float x2, float y2, float x3, float y3) {
    vkvg_curve_to (ctx, ctx->curPos.x + x1, ctx->curPos.y + y1, ctx->curPos.x + x2, ctx->curPos.y + y2, ctx->curPos.x + x3, ctx->curPos.y + y3);
}

void vkvg_rectangle (VkvgContext ctx, float x, float y, float w, float h){
    _finish_path (ctx);

    //set start to current idx in point array
    ctx->pathes[ctx->pathPtr] = ctx->pointCount;
    _check_pathes_array(ctx);
    ctx->pathPtr++;

    _add_point (ctx, x, y);
    _add_point (ctx, x + w, y);
    _add_point (ctx, x + w, y + h);
    _add_point (ctx, x, y + h);

    vkvg_close_path (ctx);

    ctx->curPos.x = x;
    ctx->curPos.y = y;
}
void vkvg_clip_preserve (VkvgContext ctx){
    vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pSurf->dev->pipelineClipping);
    vkvg_fill_preserve(ctx);
    vkvg_flush(ctx);
    //should test current operator to bind correct pipeline
    ctx->stencilRef++;
    vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pSurf->dev->pipeline);
    vkCmdSetStencilReference(ctx->cmd,VK_STENCIL_FRONT_AND_BACK, ctx->stencilRef);
}
void vkvg_reset_clip (VkvgContext ctx){
    _flush_cmd_buff(ctx);
    _clear_stencil(ctx->pSurf);
    ctx->stencilRef=0;
    _init_cmd_buff(ctx);
}
void vkvg_clip (VkvgContext ctx){
    vkvg_clip_preserve(ctx);
    _clear_path(ctx);
}
void vkvg_fill_preserve (VkvgContext ctx){
    if (ctx->pathPtr == 0)      //nothing to fill
        return;
    if (ctx->pathPtr % 2 != 0)  //current path is not finished, close it
        vkvg_close_path(ctx);
    if (ctx->pointCount * 4 > ctx->sizeIndices - ctx->indCount)//flush if vk buff is full
        vkvg_flush(ctx);

    uint32_t ptrPath = 0;;
    Vertex v = {};
    v.uv.z = -1;

    while (ptrPath < ctx->pathPtr){
        if (!_path_is_closed(ctx, ptrPath))
            //close path
            ctx->pathes[ptrPath+1] = ctx->pathes[ptrPath];

        uint32_t firstPtIdx = ctx->pathes[ptrPath];
        uint32_t lastPtIdx = _get_last_point_of_closed_path (ctx, ptrPath);
        uint32_t pathPointCount = lastPtIdx - ctx->pathes[ptrPath] + 1;
        uint32_t firstVertIdx = ctx->vertCount;

        float triIn[pathPointCount+1][2];
        memcpy(&triIn[1], &ctx->points[firstPtIdx], 2* sizeof(float) * pathPointCount);
        int cpt[] = {pathPointCount};
        int triOut[pathPointCount-2][3];

        triangulate_polygon(1, cpt, triIn, triOut);

        for (int i=0; i<pathPointCount;i++){
            v.pos = ctx->points[i+firstPtIdx];
            _add_vertex(ctx, v);
        }

        firstVertIdx--;//indices start at 1
        for (int i=0; i<pathPointCount-2;i++){
            _add_triangle_indices (ctx,
                    triOut[i][2] + firstVertIdx,
                    triOut[i][1] + firstVertIdx,
                    triOut[i][0] + firstVertIdx);
        }

        ptrPath+=2;
    }
    _record_draw_cmd(ctx);
}
void vkvg_fill (VkvgContext ctx){
    vkvg_fill_preserve(ctx);
    _clear_path(ctx);
}
void vkvg_stroke_preserve (VkvgContext ctx)
{
    _finish_path(ctx);

    if (ctx->pathPtr == 0)//nothing to stroke
        return;
    if (ctx->pointCount * 4 > ctx->sizeIndices - ctx->indCount)
        vkvg_flush(ctx);

    Vertex v = {};
    v.uv.z = -1;

    float hw = ctx->lineWidth / 2.0;
    int i = 0, ptrPath = 0;

    uint32_t lastPathPointIdx, iL, iR;

    while (ptrPath < ctx->pathPtr){
        uint32_t firstIdx = ctx->vertCount;

        if (_path_is_closed(ctx,ptrPath)){
            lastPathPointIdx = _get_last_point_of_closed_path(ctx,ptrPath);
            iL = lastPathPointIdx;
        }else{
            lastPathPointIdx = ctx->pathes[ptrPath+1];
            vec2 n = vec2_line_norm(ctx->points[i], ctx->points[i+1]);

            vec2 p0 = ctx->points[i];
            vec2 vhw = vec2_mult(n,hw);

            if (ctx->lineCap == VKVG_LINE_CAP_SQUARE)
                p0 = vec2_sub(p0, vhw);

            vhw = vec2_perp(vhw);

            if (ctx->lineCap == VKVG_LINE_CAP_ROUND){
                float step = M_PI_2 / hw;
                float a = acos(n.x) + M_PI_2;
                if (n.y < 0)
                    a = M_PI-a;
                float a1 = a + M_PI;

                a+=step;
                while (a < a1){
                    _add_vertexf(ctx, cos(a) * hw + p0.x, sin(a) * hw + p0.y);
                    a+=step;
                }
                uint32_t p0Idx = ctx->vertCount;
                for (int p = firstIdx; p < p0Idx; p++)
                    _add_triangle_indices(ctx, p0Idx+1, p, p+1);
                firstIdx = p0Idx;
            }

            v.pos = vec2_add(p0, vhw);
            _add_vertex(ctx, v);
            v.pos = vec2_sub(p0, vhw);
            _add_vertex(ctx, v);
            _add_tri_indices_for_rect(ctx, firstIdx);

            iL = i++;
        }

        while (i < lastPathPointIdx){
            iR = i+1;
            _build_vb_step(ctx,v,hw,iL,i,iR);
            iL = i++;
        }

        if (!_path_is_closed(ctx,ptrPath)){
            vec2 n = vec2_line_norm(ctx->points[i-1], ctx->points[i]);
            vec2 p0 = ctx->points[i];
            vec2 vhw = vec2_mult(n, hw);

            if (ctx->lineCap == VKVG_LINE_CAP_SQUARE)
                p0 = vec2_add(p0, vhw);

            vhw = vec2_perp(vhw);

            v.pos = vec2_add(p0, vhw);
            _add_vertex(ctx, v);
            v.pos = vec2_sub(p0, vhw);
            _add_vertex(ctx, v);

            if (ctx->lineCap == VKVG_LINE_CAP_ROUND){
                firstIdx = ctx->vertCount;
                float step = M_PI_2 / hw;
                float a = acos(n.x)+ M_PI_2;
                if (n.y < 0)
                    a = M_PI-a;
                float a1 = a - M_PI;
                a-=step;
                while ( a > a1){
                    _add_vertexf(ctx, cos(a) * hw + p0.x, sin(a) * hw + p0.y);
                    a-=step;
                }

                uint32_t p0Idx = ctx->vertCount-1;
                for (int p = firstIdx-1 ; p < p0Idx; p++)
                    _add_triangle_indices(ctx, p+1, p, firstIdx-2);
            }

            i++;
        }else{
            iR = ctx->pathes[ptrPath];
            _build_vb_step(ctx,v,hw,iL,i,iR);

            uint32_t* inds = (uint32_t*)(ctx->indices.mapped + ((ctx->indCount-6) * sizeof(uint32_t)));
            uint32_t ii = firstIdx;
            inds[1] = ii;
            inds[4] = ii;
            inds[5] = ii+1;
            i++;
        }

        ptrPath+=2;
    }
    _record_draw_cmd(ctx);
}
void vkvg_stroke (VkvgContext ctx)
{
    vkvg_stroke_preserve(ctx);
    _clear_path(ctx);
}
void _vkvg_fill_rectangle (VkvgContext ctx, float x, float y, float width, float height){
    Vertex v[4] =
    {
        {{x,y},             {0,0,-1}},
        {{x,y+height},      {0,0,-1}},
        {{x+width,y},       {0,0,-1}},
        {{x+width,y+height},{0,0,-1}}
    };
    uint32_t firstIdx = ctx->vertCount;
    Vertex* pVert = (Vertex*)(ctx->vertices.mapped + ctx->vertCount * sizeof(Vertex));
    memcpy (pVert,v,4*sizeof(Vertex));
    ctx->vertCount+=4;
    _add_tri_indices_for_rect(ctx, firstIdx);
}
void vkvg_paint (VkvgContext ctx){
    _vkvg_fill_rectangle (ctx, 0, 0, ctx->pSurf->width, ctx->pSurf->height);
    _record_draw_cmd (ctx);
}

inline void vkvg_set_source_rgb (VkvgContext ctx, float r, float g, float b) {
    vkvg_set_source_rgba (ctx, r, g, b, 1);
}
void vkvg_set_source_rgba (VkvgContext ctx, float r, float g, float b, float a)
{
    uint32_t lastPat = ctx->pushConsts.patternType;

    vec4 c = {r,g,b,a};
    ctx->pushConsts.source = c;
    ctx->pushConsts.patternType = VKVG_PATTERN_TYPE_SOLID;

    if (lastPat == VKVG_PATTERN_TYPE_SURFACE){
        _flush_cmd_buff             (ctx);
        _reset_src_descriptor_set   (ctx);
        _init_cmd_buff              (ctx);//push csts updated by init
    }else
        _update_push_constants (ctx);

    ctx->curRGBA.x = r;
    ctx->curRGBA.y = g;
    ctx->curRGBA.z = b;
    ctx->curRGBA.w = a;
}
void vkvg_set_source_surface(VkvgContext ctx, VkvgSurface surf, float x, float y){
    _flush_cmd_buff(ctx);

    ctx->source = surf->img;

    if (vkh_image_get_sampler(ctx->source) == VK_NULL_HANDLE)
        vkh_image_create_sampler(ctx->source,VK_FILTER_NEAREST, VK_FILTER_NEAREST,
                             VK_SAMPLER_MIPMAP_MODE_NEAREST,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

    if (vkh_image_get_layout (ctx->source) != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL){
        vkh_cmd_begin (ctx->cmd,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        vkh_image_set_layout        (ctx->cmd, ctx->source, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        vkh_cmd_end                 (ctx->cmd);

        _submit_wait_and_reset_cmd  (ctx);
    }

    _update_descriptor_set          (ctx, ctx->source, ctx->dsSrc);

    vec4 srcRect = {x,y,surf->width,surf->height};
    ctx->pushConsts.source = srcRect;
    ctx->pushConsts.patternType = VKVG_PATTERN_TYPE_SURFACE;

    _init_cmd_buff                  (ctx);
}
void vkvg_set_source (VkvgContext ctx, VkvgPattern pat){
    if (pat->type == VKVG_PATTERN_TYPE_SOLID){
        vkvg_color_t* c = (vkvg_color_t*)pat->data;
        vkvg_set_source_rgba (ctx, c->r, c->g, c->b, c->a);
        return;
    }
    if (pat->type == VKVG_PATTERN_TYPE_SURFACE){
        vkvg_set_source_surface (ctx, (VkvgSurface)pat->data, 0, 0);
        return;
    }

    _flush_cmd_buff (ctx);

    if (ctx->pushConsts.patternType == VKVG_PATTERN_TYPE_SURFACE)
        _reset_src_descriptor_set (ctx);

    ctx->pushConsts.patternType = pat->type;
    vec4 bounds = {ctx->pSurf->width, ctx->pSurf->height, 0, 0};//store img bounds in unused source field
    ctx->pushConsts.source = bounds;
    _update_push_constants (ctx);

    //transform control point with current ctx matrix
    vkvg_gradient_t grad = {};
    memcpy(&grad, pat->data, sizeof(vkvg_gradient_t));

    vkvg_matrix_transform_point(&ctx->pushConsts.mat, &grad.cp[0].x, &grad.cp[0].y);
    vkvg_matrix_transform_point(&ctx->pushConsts.mat, &grad.cp[1].x, &grad.cp[1].y);
    //to do, scale radial radiuses in cp[2]

    memcpy(ctx->uboGrad.mapped, &grad, sizeof(vkvg_gradient_t));

    _init_cmd_buff (ctx);
}
void vkvg_set_line_width (VkvgContext ctx, float width){
    ctx->lineWidth = width;
}
void vkvg_set_line_cap (VkvgContext ctx, vkvg_line_cap_t cap){
    ctx->lineCap = cap;
}
void vkvg_set_line_join (VkvgContext ctx, vkvg_line_join_t join){
    ctx->lineJoint = join;
}



void vkvg_select_font_face (VkvgContext ctx, const char* name){

    _select_font_face (ctx, name);
}
void vkvg_set_font_size (VkvgContext ctx, uint32_t size){
    _set_font_size (ctx,size);
}

void vkvg_set_text_direction (vkvg_context* ctx, vkvg_direction_t direction){

}

void vkvg_show_text (VkvgContext ctx, const char* text){
    _show_text (ctx, text);
    _record_draw_cmd (ctx);
}

void vkvg_save (VkvgContext ctx){
    _flush_cmd_buff(ctx);

    VkvgDevice dev = ctx->pSurf->dev;
    vkvg_context_save_t* sav = (vkvg_context_save_t*)calloc(1,sizeof(vkvg_context_save_t));

    sav->stencilMS = vkh_image_ms_create(dev,VK_FORMAT_S8_UINT,VKVG_SAMPLES,ctx->pSurf->width,ctx->pSurf->height,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    vkh_cmd_begin (ctx->cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    vkh_image_set_layout (ctx->cmd, ctx->pSurf->stencilMS, VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    vkh_image_set_layout (ctx->cmd, sav->stencilMS, VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    VkImageCopy cregion = { .srcSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1},
                            .dstSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1},
                            .extent = {ctx->pSurf->width,ctx->pSurf->height,1}};
    vkCmdCopyImage(ctx->cmd,
                   vkh_image_get_vkimage (ctx->pSurf->stencilMS),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   vkh_image_get_vkimage (sav->stencilMS),       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &cregion);

    vkh_image_set_layout (ctx->cmd, ctx->pSurf->stencilMS, VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    VK_CHECK_RESULT(vkEndCommandBuffer(ctx->cmd));
    _submit_ctx_cmd(ctx);

    sav->stencilRef = ctx->stencilRef;
    sav->sizePoints = ctx->sizePoints;
    sav->pointCount = ctx->pointCount;

    sav->points = (vec2*)malloc (sav->pointCount * sizeof(vec2));
    memcpy (sav->points, ctx->points, sav->pointCount * sizeof(vec2));

    sav->pathPtr    = ctx->pathPtr;
    sav->sizePathes = ctx->sizePathes;

    sav->pathes = (uint32_t*)malloc (sav->pathPtr * sizeof(uint32_t));
    memcpy (sav->pathes, ctx->pathes, sav->pathPtr * sizeof(uint32_t));

    sav->curPos     = ctx->curPos;
    sav->curPosExists=ctx->curPosExists;
    sav->curRGBA    = ctx->curRGBA;
    sav->lineWidth  = ctx->lineWidth;
    sav->lineCap    = ctx->lineCap;
    sav->lineWidth  = ctx->lineWidth;
    sav->mat        = ctx->pushConsts.mat;
    sav->matInv     = ctx->pushConsts.matInv;

    sav->selectedFont = ctx->selectedFont;
    sav->selectedFont.fontFile = (char*)calloc(FONT_FILE_NAME_MAX_SIZE,sizeof(char));
    strcpy (sav->selectedFont.fontFile, ctx->selectedFont.fontFile);

    sav->currentFont  = ctx->currentFont;
    sav->textDirection= ctx->textDirection;
    sav->pushConsts   = ctx->pushConsts;
    sav->source       = ctx->source;

    sav->pNext      = ctx->pSavedCtxs;
    ctx->pSavedCtxs = sav;

    _wait_and_reset_ctx_cmd (ctx);
    _init_cmd_buff          (ctx);
}
void vkvg_restore (VkvgContext ctx){
    if (ctx->pSavedCtxs == NULL)
        return;
    _flush_cmd_buff(ctx);

    vkvg_context_save_t* sav = ctx->pSavedCtxs;
    ctx->pSavedCtxs = sav->pNext;

    vkh_cmd_begin (ctx->cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    vkh_image_set_layout (ctx->cmd, ctx->pSurf->stencilMS, VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    vkh_image_set_layout (ctx->cmd, sav->stencilMS, VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    VkImageCopy cregion = { .srcSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1},
                            .dstSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1},
                            .extent = {ctx->pSurf->width,ctx->pSurf->height,1}};
    vkCmdCopyImage(ctx->cmd,
                   vkh_image_get_vkimage (sav->stencilMS),       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   vkh_image_get_vkimage (ctx->pSurf->stencilMS),VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &cregion);
    vkh_image_set_layout (ctx->cmd, ctx->pSurf->stencilMS, VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    VK_CHECK_RESULT(vkEndCommandBuffer(ctx->cmd));
    _submit_ctx_cmd(ctx);

    ctx->stencilRef = sav->stencilRef;
    ctx->sizePoints = sav->sizePoints;
    ctx->pointCount = sav->pointCount;

    ctx->points = (vec2*)realloc ( ctx->points, ctx->sizePoints * sizeof(vec2));
    memset (ctx->points, 0, ctx->sizePoints * sizeof(vec2));
    memcpy (ctx->points, sav->points, ctx->pointCount * sizeof(vec2));

    ctx->pathPtr    = sav->pathPtr;
    ctx->sizePathes = sav->sizePathes;

    ctx->pathes = (uint32_t*)realloc (ctx->pathes, ctx->sizePathes * sizeof(uint32_t));
    memset (ctx->pathes, 0, ctx->sizePathes * sizeof(uint32_t));
    memcpy (ctx->pathes, sav->pathes, ctx->pathPtr * sizeof(uint32_t));

    ctx->curPos     = sav->curPos;
    ctx->curPosExists=sav->curPosExists;
    ctx->curRGBA    = sav->curRGBA;
    ctx->lineWidth  = sav->lineWidth;
    ctx->lineCap    = sav->lineCap;
    ctx->lineJoint  = sav->lineJoint;
    ctx->pushConsts.mat     = sav->mat;
    ctx->pushConsts.matInv  = sav->matInv;

    ctx->selectedFont.charSize = sav->selectedFont.charSize;
    strcpy (ctx->selectedFont.fontFile, sav->selectedFont.fontFile);

    ctx->currentFont  = sav->currentFont;
    ctx->textDirection= sav->textDirection;
    ctx->pushConsts   = sav->pushConsts;
    ctx->source       = sav->source;

    _wait_and_reset_ctx_cmd (ctx);
    _init_cmd_buff          (ctx);

    _free_ctx_save(sav);
}

void vkvg_translate (VkvgContext ctx, float dx, float dy){
    vkvg_matrix_translate (&ctx->pushConsts.mat, dx, dy);
    ctx->pushConsts.matInv = ctx->pushConsts.mat;
    vkvg_matrix_invert (&ctx->pushConsts.matInv);
    _update_push_constants (ctx);
}
void vkvg_scale (VkvgContext ctx, float sx, float sy){
    vkvg_matrix_scale (&ctx->pushConsts.mat, sx, sy);
    ctx->pushConsts.matInv = ctx->pushConsts.mat;
    vkvg_matrix_invert (&ctx->pushConsts.matInv);
    _update_push_constants (ctx);
}
void vkvg_rotate (VkvgContext ctx, float radians){
    vkvg_matrix_rotate (&ctx->pushConsts.mat, radians);
    ctx->pushConsts.matInv = ctx->pushConsts.mat;
    vkvg_matrix_invert (&ctx->pushConsts.matInv);
    _update_push_constants (ctx);
}

