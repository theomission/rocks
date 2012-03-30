uniform mat4 mvp;
uniform mat4 model;
uniform mat4 modelIT;
uniform vec3 eyePos;
uniform vec3 sundir = vec3(1,1,1);
uniform vec3 sunColor = vec3(1,1,1);
uniform float shininess = 1;
uniform float ambient = 1.0;
uniform float Ka = 0.2;
uniform float Kd = 0.8;
uniform float Ks = 0.2;
uniform vec2 texDim;
uniform sampler2D heightMap;
uniform sampler2D diffuseMap;

const float PI = 3.14159265358979;
const float invPI = 1.0 / PI;
#ifdef VERTEX_P
in vec3 pos;
in vec3 normal;
out vec3 vNormal;
out vec3 vToEye;
out vec2 vCoord;
out vec3 vPos;
void main()
{
	gl_Position = mvp * vec4(pos, 1);
	vPos = (model * vec4(pos, 1)).xyz;
	vec3 xfmNormal = normalize((modelIT * vec4(normal, 0)).xyz);
	vNormal = xfmNormal;
	vToEye = eyePos - pos;
	vCoord = asin(xfmNormal.xy) * invPI + vec2(0.5);
}
#endif

// from "Bump Mapping Unparametrized Surfaces on the GPU" - M. Mikkelsen 2010
vec3 PerturbNormal(vec3 pos, vec3 N, vec2 coord)
{
	vec3 dSigmaS = dFdx(pos);
	vec3 dSigmaT = dFdy(pos);

	vec3 r1 = cross(dSigmaT, N);
	vec3 r2 = cross(N, dSigmaS);

	float det = dot(dSigmaS, r1);

	vec2 texDx = dFdx(coord);
	vec2 texDy = dFdy(coord);

	vec2 texRight = coord + vec2(texDim.x, 0);
	vec2 texUp = coord + vec2(0, texDim.y);

	float H00 = texture(heightMap, coord).r;
	float H10 = texture(heightMap, texRight).r;
	float H01 = texture(heightMap, texUp).r;

	float dBs = H10 - H00;
	float dBt = H01 - H00;

	vec3 surfGrad = sign(det) * (dBs * r1 + dBt * r2);
	return normalize(abs(det) * N - surfGrad);
}

#ifdef FRAGMENT_P
in vec3 vNormal;
in vec3 vToEye;
in vec2 vCoord;
in vec3	vPos;
out vec4 outColor;
void main()
{
	vec3 N = normalize(vNormal);
	N = PerturbNormal(vPos, N, vCoord);
	vec3 V = normalize(vToEye);
	vec3 H = normalize (V + sundir);
	float sunDotN = dot(sundir, N);
	float diffuseAmount = max(0,sunDotN);
	float specularAmount = max(0, dot(H, N)) ;
	float isVisible = float(sunDotN > 0.0);
	specularAmount *= isVisible;
	specularAmount = pow(specularAmount, shininess);

	vec3 albedo = texture(diffuseMap, vCoord).rgb;
	albedo = pow(albedo, vec3(1/2.2));
	float diffuseLight = Kd * diffuseAmount ;
	float specularLight = Ks * specularAmount ;
	float ambientLight = Ka * ambient ;

	vec3 combined = albedo * sunColor * (diffuseLight + specularLight + ambientLight);
	combined = pow(combined, vec3(2.2));
	outColor = vec4(combined, 1);
}
#endif

