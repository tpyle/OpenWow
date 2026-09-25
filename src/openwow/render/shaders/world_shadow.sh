#ifndef OPENWOW_WORLD_SHADOW_SH
#define OPENWOW_WORLD_SHADOW_SH

SAMPLER2DSHADOW(s_worldShadow0, 5);
SAMPLER2DSHADOW(s_worldShadow1, 6);
SAMPLER2DSHADOW(s_worldShadow2, 7);
SAMPLER2DSHADOW(s_worldShadow3, 8);

uniform mat4 u_worldShadowMtx[4];
uniform vec4 u_worldShadowParams[3];

vec4 openwowShadowCoordinate(mat4 shadowMatrix, vec3 worldPosition)
{
    vec4 shadowPosition = mul(shadowMatrix, vec4(worldPosition, 1.0));
    vec3 projected = shadowPosition.xyz / shadowPosition.w;
    bool inside = !any(greaterThan(projected, vec3_splat(1.0)))
               && !any(lessThan(projected, vec3_splat(0.0)));
    return vec4(projected, inside ? 1.0 : 0.0);
}

// 3x3 percentage-closer filter: nine comparisons one texel apart (each
// also bilinearly filtered by the comparison sampler), averaged so shadow
// edges fade over a few texels instead of stepping per texel. `_texel` is
// the shadow map's texel size in UV (u_worldShadowParams[0].w).
#define OPENWOW_SHADOW_TAP(_sampler, _uv, _z, _dx, _dy) \
    shadow2D(_sampler, vec3((_uv) + vec2(_dx, _dy), _z))

#define OPENWOW_SHADOW_PCF3X3(_sampler, _uv, _z, _texel) ( ( \
      OPENWOW_SHADOW_TAP(_sampler, _uv, _z, -(_texel), -(_texel)) \
    + OPENWOW_SHADOW_TAP(_sampler, _uv, _z,       0.0, -(_texel)) \
    + OPENWOW_SHADOW_TAP(_sampler, _uv, _z,  (_texel), -(_texel)) \
    + OPENWOW_SHADOW_TAP(_sampler, _uv, _z, -(_texel),       0.0) \
    + OPENWOW_SHADOW_TAP(_sampler, _uv, _z,       0.0,       0.0) \
    + OPENWOW_SHADOW_TAP(_sampler, _uv, _z,  (_texel),       0.0) \
    + OPENWOW_SHADOW_TAP(_sampler, _uv, _z, -(_texel),  (_texel)) \
    + OPENWOW_SHADOW_TAP(_sampler, _uv, _z,       0.0,  (_texel)) \
    + OPENWOW_SHADOW_TAP(_sampler, _uv, _z,  (_texel),  (_texel)) ) * (1.0 / 9.0) )

float openwowSampleWorldShadow(vec3 worldPosition)
{
    if (u_worldShadowParams[0].z < 0.5) {
        return 1.0;
    }

    int productCount = int(u_worldShadowParams[0].x + 0.5);
    int firstProduct = int(u_worldShadowParams[0].y + 0.5);
    float texel = u_worldShadowParams[0].w;
    vec4 coordinate = openwowShadowCoordinate(
        u_worldShadowMtx[0], worldPosition);
    if (firstProduct <= 0 && productCount > 0 && coordinate.w > 0.5) {
        return OPENWOW_SHADOW_PCF3X3(s_worldShadow0, coordinate.xy,
                                     coordinate.z - u_worldShadowParams[1].x, texel);
    }
    coordinate = openwowShadowCoordinate(u_worldShadowMtx[1], worldPosition);
    if (firstProduct <= 1 && productCount > 1 && coordinate.w > 0.5) {
        return OPENWOW_SHADOW_PCF3X3(s_worldShadow1, coordinate.xy,
                                     coordinate.z - u_worldShadowParams[1].y, texel);
    }
    coordinate = openwowShadowCoordinate(u_worldShadowMtx[2], worldPosition);
    if (firstProduct <= 2 && productCount > 2 && coordinate.w > 0.5) {
        return OPENWOW_SHADOW_PCF3X3(s_worldShadow2, coordinate.xy,
                                     coordinate.z - u_worldShadowParams[1].z, texel);
    }
    coordinate = openwowShadowCoordinate(u_worldShadowMtx[3], worldPosition);
    if (firstProduct <= 3 && productCount > 3 && coordinate.w > 0.5) {
        return OPENWOW_SHADOW_PCF3X3(s_worldShadow3, coordinate.xy,
                                     coordinate.z - u_worldShadowParams[1].w, texel);
    }
    return 1.0;
}

vec3 openwowApplyWorldShadow(vec3 color, vec3 worldPosition)
{
    float visibility = openwowSampleWorldShadow(worldPosition);
    vec3 modulation = mix(u_worldShadowParams[2].rgb,
                          vec3_splat(1.0), visibility);
    return color * modulation;
}

// Like openwowApplyWorldShadow, for surfaces with a normal. A surface facing
// away from the light isn't directly lit, so casters on the far side (e.g.
// buildings behind a city wall) must not print shadows onto it; the shadow
// fades out as the surface turns edge-on to the light.
vec3 openwowApplyWorldShadowFacing(vec3 color, vec3 worldPosition, vec3 normal,
                                   vec3 surfaceToLight)
{
    float facing = dot(normalize(normal), normalize(surfaceToLight));
    float visibility = mix(1.0, openwowSampleWorldShadow(worldPosition),
                           smoothstep(0.0, 0.15, facing));
    vec3 modulation = mix(u_worldShadowParams[2].rgb,
                          vec3_splat(1.0), visibility);
    return color * modulation;
}

#endif
