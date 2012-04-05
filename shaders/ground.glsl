uniform mat4 mvp;
uniform mat4 model;
uniform mat4 modelIT;
uniform vec3 eyePos;
uniform vec3 sundir = vec3(1,1,1);
uniform vec3 sunColor = vec3(1,1,1);
uniform float shininess = 50;
uniform float ambient = 1.0;
uniform float Ka = 0.4;
uniform float Kd = 0.4;
uniform float Ks = 0.4;
uniform vec2 texDim;
uniform sampler2DShadow shadowMap;
uniform mat4 shadowMat;

#ifdef VERTEX_P
in vec3 pos;
in vec3 normal;
out vec3 vNormal;
out vec3 vToEye;
out vec4 vShadowMapCoord;
void main()
{
	gl_Position = mvp * vec4(pos, 1);
	vec3 xfmPos = (model * vec4(pos, 1)).xyz;
	vec3 xfmNormal = normalize((modelIT * vec4(normal, 0)).xyz);
	vNormal = xfmNormal;
	vToEye = eyePos - xfmPos;
	vShadowMapCoord = shadowMat * vec4(pos, 1);
}
#endif

#ifdef FRAGMENT_P
in vec3 vNormal;
in vec3 vToEye;
in vec4 vShadowMapCoord;
out vec4 outColor;
void main()
{
	float shadowVis = textureProj(shadowMap, vShadowMapCoord);
	vec3 N = normalize(vNormal);
	vec3 V = normalize(vToEye);
	vec3 H = normalize (V + sundir);
	float sunDotN = dot(sundir, N);
	float diffuseAmount = max(0,sunDotN);
	float specularAmount = max(0, dot(H, N)) ;
	float isVisible = float(sunDotN > 0.0);

	specularAmount *= isVisible;
	specularAmount = pow(specularAmount, shininess);

	float diffuseLight = Kd * diffuseAmount ;
	float specularLight = Ks * specularAmount ;
	float ambientLight = Ka * ambient ;

	vec3 combined = sunColor * (shadowVis * (diffuseLight + specularLight) + ambientLight);
	combined = pow(combined, vec3(2.2));
	outColor = vec4(combined, 1);
}
#endif

