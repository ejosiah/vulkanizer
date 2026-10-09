#include "bistro_common.glsl"
layout(location=0) in vec3 nearPoint;
layout(location=1) in vec3 farPoint;
/** @brief Intersect the camera ray with the unbounded world plane y=0. */
vec3 floorPosition() {
    vec3 ray=farPoint-nearPoint;
    if(abs(ray.y)<1e-7) discard;
    float t=-nearPoint.y/ray.y;
    if(t<0 || t>1) discard;
    vec3 world=nearPoint+t*ray;
    vec4 clip=scene.matrix*vec4(world,1);
    gl_FragDepth=clip.z/clip.w;
    return world;
}
vec4 floorColor(vec3 world, vec2 shadow) {
    // Antialiased world-space grid makes motion and the horizon visible without a texture.
    vec2 spacing=max(fwidth(world.xz),vec2(1e-5));
    vec2 grid=abs(fract(world.xz-0.5)-0.5)/spacing;
    float line=1-min(min(grid.x,grid.y),1);
    line*=1-clamp(max(spacing.x,spacing.y)*0.5,0,1);
    vec3 albedo=mix(vec3(0.48,0.50,0.53),vec3(0.22,0.25,0.30),line);
    float cosine=max(dot(vec3(0,1,0),-scene.lightDirection.xyz),0);
    vec3 lit=albedo*(vec3(0.18)+vec3(1.4,1.3,1.1)*cosine*shadow.x);
    if(scene.info.z==1u) lit=vec3(shadow.x);
    if(scene.info.z==2u) lit=shadow.y>0.5?vec3(0.1,0.8,0.2):vec3(1,0,0.6);
    return vec4(lit/(1+lit),1);
}
