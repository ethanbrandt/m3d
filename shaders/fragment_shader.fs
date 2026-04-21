#version 460 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;

uniform sampler2D texture_diffuse1;

void main()
{
	vec4 texColor = texture(texture_diffuse1, TexCoords);

	vec3 normal = normalize(Normal);

	vec3 lightDir = normalize(vec3(-0.4, 0.8, 0.3));

	float diffuse = max(dot(normal, lightDir), 0.0);

	float ambient = 0.25;
	float lightStrength = ambient + diffuse * 0.75;

	FragColor = vec4(texColor.rgb * lightStrength, texColor.a);
}