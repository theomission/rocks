uniform mat4 mvp;
uniform mat4 model;
uniform mat4 modelIT;
uniform vec3 eyePos;
uniform vec3 sundir = vec3(1,1,1);
uniform vec3 sunColor = vec3(1,1,1);
uniform float shininess = 20;
uniform float ambient = 1.0;
uniform float Ka = 0.5;
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
out vec2 vCoord0;
out vec2 vCoord1;
out vec2 vCoord2;
out vec3 vPos;
void main()
{
	gl_Position = mvp * vec4(pos, 1);
	vPos = (model * vec4(pos, 1)).xyz;
	vec3 xfmNormal = normalize((modelIT * vec4(normal, 0)).xyz);
	vNormal = xfmNormal;
	vToEye = eyePos - pos;
	vCoord0 = pos.xz * 0.5 + 0.5;
	vCoord1 = pos.yz * 0.5 + 0.5;
	vCoord2 = pos.xy * 0.5 + 0.5;
}
#endif

vec3 BlendH(vec3 N, vec3 h0, vec3 h1, vec3 h2)
{
	float horizT = smoothstep(0.5, 0.8, abs(N.y));
	float vertT = smoothstep(0.5, 0.8, abs(N.z));
	vec3 horizResult = mix(h1, h0, horizT);
	vec3 vertResult = mix(horizResult, h2, vertT);
	return vertResult;
}

vec3 GetHeights(vec2 coord)
{
	vec2 texDx = dFdx(coord);
	vec2 texDy = dFdy(coord);

	vec2 texRight = coord + vec2(texDim.x, 0);
	vec2 texUp = coord + vec2(0, texDim.y);

	float H00 = texture(heightMap, coord).r;
	float H10 = texture(heightMap, texRight).r;
	float H01 = texture(heightMap, texUp).r;

	return vec3(H00, H10, H01);
}

// from "Bump Mapping Unparametrized Surfaces on the GPU" - M. Mikkelsen 2010
vec3 PerturbNormal(vec3 pos, vec3 N, vec2 coord0, vec2 coord1, vec2 coord2)
{
	vec3 dSigmaS = dFdx(pos);
	vec3 dSigmaT = dFdy(pos);

	vec3 r1 = cross(dSigmaT, N);
	vec3 r2 = cross(N, dSigmaS);

	float det = dot(dSigmaS, r1);
	
	vec3 heights = BlendH(N,
		GetHeights(coord0),
		GetHeights(coord1),
		GetHeights(coord2));

	float dBs = heights.y - heights.x;
	float dBt = heights.z - heights.x;

	vec3 surfGrad = sign(det) * (dBs * r1 + dBt * r2);
	return normalize(abs(det) * N - surfGrad);
}
	
vec3 BlendAlbedo(vec3 N, vec3 val0, vec3 val1, vec3 val2)
{
	float horizT = smoothstep(0.5, 0.8, abs(N.y));
	float vertT = smoothstep(0.5, 0.8, abs(N.z));
	vec3 horizResult = mix(val1, val0, horizT);
	vec3 vertResult = mix(horizResult, val2, vertT);
	return vertResult;
}

#ifdef FRAGMENT_P
in vec3 vNormal;
in vec3 vToEye;
in vec2 vCoord0;
in vec2 vCoord1;
in vec2 vCoord2;
in vec3	vPos;
out vec4 outColor;
void main()
{
	vec3 N = normalize(vNormal);
	N = PerturbNormal(vPos, N, vCoord0, vCoord1, vCoord2);
	vec3 V = normalize(vToEye);
	vec3 H = normalize (V + sundir);
	float sunDotN = dot(sundir, N);
	float diffuseAmount = max(0,sunDotN);
	float specularAmount = max(0, dot(H, N)) ;
	float isVisible = float(sunDotN > 0.0);
	specularAmount *= isVisible;
	specularAmount = pow(specularAmount, shininess);

	vec3 albedo0 = pow(texture(diffuseMap, vCoord0).rgb, vec3(1/2.2));
	vec3 albedo1 = pow(texture(diffuseMap, vCoord1).rgb, vec3(1/2.2));
	vec3 albedo2 = pow(texture(diffuseMap, vCoord2).rgb, vec3(1/2.2));

	vec3 albedo = BlendAlbedo(N, albedo0, albedo1, albedo2);

	float diffuseLight = Kd * diffuseAmount ;
	float specularLight = Ks * specularAmount ;
	float ambientLight = Ka * ambient ;

	vec3 combined = albedo * sunColor * (diffuseLight + specularLight + ambientLight);
	combined = pow(combined, vec3(2.2));
	outColor = vec4(combined, 1);
}
#endif

