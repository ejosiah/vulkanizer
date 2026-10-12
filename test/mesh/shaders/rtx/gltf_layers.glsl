#ifndef VKZ_GLTF_LAYERS_GLSL
#define VKZ_GLTF_LAYERS_GLSL
#include "gltf_iridescence.glsl"
float gltf_max(vec3 x) { return max(x.x, max(x.y, x.z)); }
vec3 gltf_dielectric_fresnel(gltf_material material, vec4 samples[17], float cosine) {
    float weight = material.specular.a * samples[7].a;
    vec3 f0 = min(vec3(gltf_f0(material.optical.x)) * material.specular.rgb * samples[8].rgb, vec3(1));
    return material.optical.x == 1.0 ? vec3(0) : weight * gltf_schlick(f0, cosine);
}
vec3 gltf_film_fresnel(gltf_material material, vec4 samples[17], vec3 f0, vec3 fresnel, float cosine) {
    float factor = material.iridescence.x * samples[15].r;
    float thickness = material.textures[16].y != 0 ? mix(material.iridescence.z, material.iridescence.w, samples[16].g) : material.iridescence.w;
    if (factor <= 0.0 || thickness <= 0.0) return fresnel;
    return mix(fresnel, gltf_iridescence(1.0, material.iridescence.y, cosine, thickness, f0), factor);
}
vec3 gltf_boundary_fresnel(gltf_material material, vec4 samples[17], float cosine) {
    float weight = material.specular.a * samples[7].a;
    vec3 f0 = min(vec3(gltf_f0(material.optical.x)) * material.specular.rgb * samples[8].rgb, vec3(1));
    vec3 fresnel = material.optical.x == 1.0 ? vec3(0) : gltf_schlick(f0, cosine);
    return weight * gltf_film_fresnel(material, samples, f0, fresnel, cosine);
}
vec3 gltf_anisotropic_tangent(gltf_material material, vec4 sample_value, gltf_geometry geometry) {
    vec3 n = geometry.normal;
    vec3 t = geometry.tangent - n * dot(n, geometry.tangent);
    if (dot(t,t) < 1e-10) t = cross(abs(n.z) < 0.999 ? vec3(0,0,1) : vec3(0,1,0), n);
    t = normalize(t);
    vec2 d = material.textures[14].y != 0 ? sample_value.rg * 2.0 - 1.0 : vec2(1,0);
    if (dot(d,d) < 1e-10) d = vec2(1,0);
    d = normalize(d);
    d = mat2(material.anisotropy.y, material.anisotropy.z, -material.anisotropy.z, material.anisotropy.y) * d;
    float handedness = dot(cross(n,t), geometry.bitangent) < 0.0 ? -1.0 : 1.0;
    return normalize(t * d.x + cross(n,t) * (d.y * handedness));
}
float gltf_ggx(float roughness, float anisotropy, vec3 n, vec3 t, vec3 v, vec3 l, vec3 h) {
    float nv = max(dot(n,v),0.0), nl = max(dot(n,l),0.0);
    if (nv <= 0.0 || nl <= 0.0) return 0.0;
    float ab = max(roughness * roughness, 0.001);
    float at = mix(ab, 1.0, anisotropy * anisotropy);
    vec3 b = cross(n,t);
    float nh = max(dot(n,h),0.0);
    vec3 q = vec3(dot(t,h)/at, dot(b,h)/ab, nh);
    float distribution = 1.0 / (GLTF_PI * at * ab * dot(q,q) * dot(q,q));
    float visibility = 0.5 / max(nl * length(vec3(at*dot(t,v),ab*dot(b,v),nv)) + nv * length(vec3(at*dot(t,l),ab*dot(b,l),nl)), 1e-10);
    return distribution * visibility;
}
float gltf_sheen_fit(float x, float alpha) {
    float w = (1.0-alpha)*(1.0-alpha);
    return mix(21.5473,25.3245,w)/(1.0+mix(3.82987,3.32435,w)*pow(x,mix(0.19823,0.16801,w))) + mix(-1.97760,-1.27393,w)*x + mix(-4.32054,-4.85967,w);
}
float gltf_sheen_lambda(float c, float alpha) {
    return exp(c < 0.5 ? gltf_sheen_fit(c,alpha) : 2.0*gltf_sheen_fit(0.5,alpha)-gltf_sheen_fit(1.0-c,alpha));
}
float gltf_charlie(float roughness, float nv, float nl, float nh) {
    if (nv <= 0.0 || nl <= 0.0) return 0.0;
    float alpha = max(roughness*roughness, 0.0001);
    float d = (2.0+1.0/alpha)*pow(max(1.0-nh*nh,0.0),0.5/alpha)/(2.0*GLTF_PI);
    float visibility = 1.0 / ((1.0+gltf_sheen_lambda(nv,alpha)+gltf_sheen_lambda(nl,alpha))*4.0*nv*nl);
    return d * visibility;
}
vec3 gltf_hemisphere(uint i) {
    float z = (float(i)+0.5)/32.0;
    float phi = float(i) * 2.399963229728653;
    float r = sqrt(1.0-z*z);
    return vec3(r*cos(phi),r*sin(phi),z);
}
float gltf_sheen_energy(float nv, float roughness) {
    nv = max(nv,0.0001);
    vec3 v = vec3(sqrt(max(1.0-nv*nv,0.0)),0,nv);
    float energy = 0.0;
    for (uint i=0u;i<32u;++i) {
        vec3 l = gltf_hemisphere(i);
        energy += gltf_charlie(roughness,nv,l.z,normalize(v+l).z)*l.z;
    }
    return clamp(energy*(2.0*GLTF_PI/32.0),0.0,1.0);
}
vec3 gltf_layered_direct(gltf_material material, vec4 samples[17], gltf_geometry geometry, vec3 base, vec2 mr, float transmission, vec3 v, vec3 l) {
    vec3 n = geometry.normal;
    float nv = max(dot(n,v),0.0), nl = max(dot(n,l),0.0);
    if (dot(v+l,v+l)<1e-10) return vec3(0);
    vec3 h = normalize(v+l);
    float vh = clamp(dot(v,h),0.0,1.0);
    vec3 dielectric = gltf_boundary_fresnel(material,samples,vh);
    vec3 metal = gltf_film_fresnel(material,samples,base,gltf_schlick(base,vh),vh);
    vec3 t = gltf_anisotropic_tangent(material,samples[14],geometry);
    float ggx = gltf_ggx(mr.y,material.anisotropy.x*samples[14].b,n,t,v,l,h);
    vec3 diffuse = base * (1.0-gltf_max(dielectric)) * (1.0-transmission)/GLTF_PI;
    vec3 result = nv > 0.0 ? mix(diffuse+dielectric*ggx,metal*ggx,mr.x)*nl : vec3(0);
    vec3 sheen = material.sheen.rgb*samples[12].rgb;
    if (gltf_max(sheen)>0.0) {
        float roughness = material.sheen.a*samples[13].a;
        float energy = max(gltf_sheen_energy(nv,roughness),gltf_sheen_energy(nl,roughness));
        result = result*(1.0-gltf_max(sheen)*energy) + sheen*gltf_charlie(roughness,nv,nl,max(dot(n,h),0.0))*nl;
    }
    float coat = material.clearcoat.x*samples[9].r;
    if (coat>0.0) {
        vec3 cn = geometry.coat_normal;
        float f = gltf_schlick(0.04,abs(dot(cn,v)));
        float coat_ggx = gltf_ggx(material.clearcoat.y*samples[10].g,0.0,cn,normalize(cross(abs(cn.z)<0.999?vec3(0,0,1):vec3(0,1,0),cn)),v,l,h);
        result = result*(1.0-coat*f) + coat*f*coat_ggx*max(dot(cn,l),0.0);
    }
    return result;
}
#ifdef VKZ_REFERENCE_IBL
vec3 gltf_layered_ibl(gltf_material material, vec4 samples[17], gltf_geometry geometry, vec3 base, vec2 mr, float transmission, vec3 v) {
    vec3 n = geometry.normal;
    float nv = clamp(dot(n,v),0.0,1.0);
    float weight = material.specular.a*samples[7].a;
    vec3 f0 = min(vec3(gltf_f0(material.optical.x))*material.specular.rgb*samples[8].rgb,vec3(1));
    vec3 dielectric = gltf_film_fresnel(material,samples,f0,gltf_schlick(f0,nv),nv)*weight;
    vec3 metal = gltf_film_fresnel(material,samples,base,gltf_schlick(base,nv),nv);
    vec3 irrad = textureLod(ibl_irradiance,gltf_environment_direction(n),0.0).rgb;
    vec3 diffuse = base*(1.0-mr.x)*(1.0-transmission)*(1.0-gltf_max(dielectric))*irrad/GLTF_PI;
    vec3 t = gltf_anisotropic_tangent(material,samples[14],geometry);
    float strength = material.anisotropy.x*samples[14].b;
    vec3 b = normalize(cross(n,t));
    vec3 an = cross(cross(b,v),b);
    if (dot(an,an)>1e-10) an=normalize(an);
    else an=n;
    vec3 bent = mix(an,n,pow(1.0-strength*(1.0-mr.y),4.0));
    if (dot(bent,bent)<1e-10) bent=n;
    bent=normalize(bent);
    vec3 reflected = normalize(mix(reflect(-v,bent),bent,mr.y*mr.y));
    float levels = float(textureQueryLevels(ibl_specular)-1);
    vec3 specular = textureLod(ibl_specular,gltf_environment_direction(reflected),mr.y*levels).rgb;
    vec2 brdf = textureLod(ibl_brdf_lut,vec2(nv,mr.y),0.0).rg;
    vec3 film_f0 = gltf_film_fresnel(material,samples,f0,f0,nv);
    vec3 metal_f0 = gltf_film_fresnel(material,samples,base,base,nv);
    vec3 response = mix(weight*(film_f0*brdf.x+brdf.y)*(material.optical.x==1.0 && material.iridescence.x*samples[15].r==0.0?0.0:1.0),metal_f0*brdf.x+brdf.y,mr.x);
    vec3 result = diffuse+specular*response;
    vec3 sheen = material.sheen.rgb*samples[12].rgb;
    if (gltf_max(sheen)>0.0) {
        float roughness = material.sheen.a*samples[13].a;
        vec3 tangent = normalize(cross(abs(n.z)<0.999?vec3(0,0,1):vec3(0,1,0),n));
        mat3 frame = mat3(tangent,cross(n,tangent),n);
        vec3 integral = vec3(0);
        for (uint i=0u;i<32u;++i) {
            vec3 local = gltf_hemisphere(i), l=frame*local;
            float value=gltf_charlie(roughness,nv,local.z,max(dot(n,normalize(v+l)),0.0))*local.z;
            integral += textureLod(ibl_environment,gltf_environment_direction(l),0.0).rgb*value;
        }
        result = result*(1.0-gltf_max(sheen)*gltf_sheen_energy(nv,roughness))+sheen*integral*(2.0*GLTF_PI/32.0);
    }
    float coat=material.clearcoat.x*samples[9].r;
    if(coat>0.0) {
        vec3 cn=geometry.coat_normal;
        float cv=clamp(dot(cn,v),0.0,1.0), roughness=material.clearcoat.y*samples[10].g;
        vec2 lut=textureLod(ibl_brdf_lut,vec2(cv,roughness),0.0).rg;
        vec3 light=textureLod(ibl_specular,gltf_environment_direction(reflect(-v,cn)),roughness*levels).rgb;
        result=result*(1.0-coat*gltf_schlick(0.04,cv))+coat*light*(0.04*lut.x+lut.y);
    }
    return result*ibl.parameters.x;
}
#endif
#endif
