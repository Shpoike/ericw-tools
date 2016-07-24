/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

#ifndef __LIGHT_ENTITIES_H__
#define __LIGHT_ENTITIES_H__

#include <map>
#include <string>
#include <vector>

#include <common/mathlib.h>
#include <common/bspfile.h>
#include <light/light.hh>

/*
 * Light attenuation formalae
 * (relative to distance 'x' from the light source)
 */
#define LF_SCALE 128
typedef enum {
    LF_LINEAR    = 0,   /* Linear (x) (DEFAULT) */
    LF_INVERSE   = 1,   /* Inverse (1/x), scaled by 1/128 */
    LF_INVERSE2  = 2,   /* Inverse square (1/(x^2)), scaled by 1/(128^2) */
    LF_INFINITE  = 3,   /* No attenuation, same brightness at any distance */
    LF_LOCALMIN  = 4,   /* No attenuation, non-additive minlight effect within
                           line of sight of the light source. */
    LF_INVERSE2A = 5,   /* Inverse square, with distance adjusted to avoid
                           exponentially bright values near the source.
                             (1/(x+128)^2), scaled by 1/(128^2) */
    LF_COUNT
} light_formula_t;

typedef struct entity_s {
    vec3_t origin;

    qboolean spotlight;
    vec3_t spotvec;
    float spotangle;
    float spotfalloff;
    float spotangle2;
    float spotfalloff2;
    miptex_t *projectedmip; /*projected texture*/
    float projectionmatrix[16]; /*matrix used to project the specified texture. already contains origin.*/

    lightsample_t light;
    light_formula_t formula;
    float atten;
    float anglescale;
    int style;
    qboolean bleed;

    /* worldspawn, light entities */
    vec_t dirtscale;
    vec_t dirtgain;
    int dirt;

    /* light entities: q3map2 penumbra */
    vec_t deviance;
    int num_samples;
    
    std::map<std::string, std::string> epairs;
    
    const struct entity_s *targetent;

    qboolean generated;     // if true, don't write to the bsp

    const bsp2_dleaf_t *leaf;    // for vis testing
    
    struct entity_s *next;
    
    const char *classname() const;
} entity_t;

/*
 * atten:
 *    Takes a float as a value (default 1.0).
 *    This reflects how fast a light fades with distance.
 *    For example a value of 2 will fade twice as fast, and a value of 0.5
 *      will fade half as fast. (Just like arghlite)
 *
 *  mangle:
 *    If the entity is a light, then point the spotlight in this direction.
 *    If it is the worldspawn, then this is the sunlight mangle
 *
 *  lightcolor:
 *    Stores the RGB values to determine the light color
 */

#define MAX_LIGHTS 65536
extern entity_t *lights[MAX_LIGHTS];

entity_t *FindEntityWithKeyPair(const char *key, const char *value);
const char *ValueForKey(const entity_t *ent, const char *key);
void GetVectorForKey(const entity_t *ent, const char *key, vec3_t vec);

void SetWorldKeyValue(const char *key, const char *value);
const char *WorldValueForKey(const char *key);

void LoadEntities(const bsp2_t *bsp);
void SetupLights(const bsp2_t *bsp);
void WriteEntitiesToString(bsp2_t *bsp);

vec_t GetLightValue(const lightsample_t *light, const entity_t *entity, vec_t dist);
    
bool Light_PointInSolid(const bsp2_t *bsp, const vec3_t point );

#endif /* __LIGHT_ENTITIES_H__ */