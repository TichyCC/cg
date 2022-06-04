#version 330 core

out vec4 fragColor;
in vec3 vertex_color;
in vec3 vertex_normal;
in vec3 vertex_position;
in vec2 texCoord;

// transformation matrix
uniform mat4 mvp;
uniform mat4 mv;
uniform mat4 v; // viewing matrix
uniform mat4 normTrans;

// flags to control the flow of shading
uniform int lightMode;
uniform int shadingMode;

// lighting properties (general)
uniform vec3 position;
uniform vec3 diffuseIntensity;
uniform vec3 ambientIntensity;
uniform vec3 specularIntensity;

// attenuation
uniform float constant;
uniform float linear;
uniform float quadratic;

// lighting properties (spot light)
uniform vec3 direction;
uniform float exponent;
uniform float cutoff;

// material properties
uniform vec3 Ka;
uniform vec3 Kd;
uniform vec3 Ks;
uniform float shininess;


// [TODO] passing texture from main.cpp
// Hint: sampler2D
uniform sampler2D tex;

void main() {
//    fragColor = vec4(texCoord.xy, 0, 1);
//
//    // [TODO] sampleing from texture
//    // Hint: texture
    
    
    // if in vertex shading mode,
    // directly set fragmnet color to be color hase been calculate by vertex shader
    if (shadingMode == 0) {
        vec3 texColor = texture(tex, texCoord).rgb;
        fragColor = vec4(vertex_color * texColor, 1.0);
        return;
    }
    
    // calculate light_position, viewing_position
    vec3 light_pos = (v * vec4(position, 1.0)).xyz;
    vec3 view_pos = vec3(0, 0, 0); // because we are in viewing space
    
    // calculate light_vector, viewing_vector, halfway_vector
    vec3 light_vector = (lightMode == 0) ? normalize( light_pos ) : normalize( light_pos - vertex_position ); // if mode == directional, set as light_pos - origin
    vec3 view_vector = normalize( view_pos - vertex_position );
    vec3 halfway_vector = normalize( light_vector + view_vector );
    
    // calculate ambient
    vec3 ambient = ambientIntensity * Ka; // [my TODO] intensity in range [0, 1] ?
    
    // calculate diffuse
    float diffuse_rate = max( dot(light_vector, vertex_normal), 0 );
    vec3 diffuse = diffuse_rate * diffuseIntensity * Kd;
    
    // calculate specular
    float specular_rate = pow( max( dot(halfway_vector, vertex_normal), 0 ), shininess );
    vec3 specular = specular_rate * specularIntensity * Ks;
    
    // attenuation
    float dis = length(light_pos - vertex_position); // distance
    float attenuation = (lightMode == 0) ? 1 : 1 / (constant + linear * dis + quadratic * dis * dis); // if mode == directional, set to 1
    
    
    // calculate spotlight effect
    float cos_vertex_direction = dot(-light_vector, direction); // cosine of angle between vector from light_pos to vertex_pos and direction
    float spotlight_effect = (lightMode != 2)                     ? 1: // if not in spotlight mode, set to 1
                             (cos_vertex_direction < cos(cutoff)) ? 0: // outoff spotlight angle
                                 pow( max(cos_vertex_direction, 0), exponent ); // spotlight effect
    
    vec3 result = ambient + attenuation * spotlight_effect * (diffuse + specular);
    
    vec3 texColor = texture(tex, texCoord).rgb;
    fragColor = vec4(result * texColor, 1.0);
}