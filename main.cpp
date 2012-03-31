#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cfloat>
#include <SDL/SDL.h>
#include <GL/glew.h>
#include <memory>
#include <sstream>
#include <sys/stat.h>
#include "common.hh"
#include "render.hh"
#include "vec.hh"
#include "camera.hh"
#include "commonmath.hh"
#include "framemem.hh"
#include "noise.hh"
#include "tweaker.hh"
#include "menu.hh"
#include "matrix.hh"
#include "font.hh"
#include "frame.hh"
#include "debugdraw.hh"
#include "task.hh"
#include "ui.hh"
#include "gputask.hh"
#include "timer.hh"
#include "compute.hh"

////////////////////////////////////////////////////////////////////////////////
// types
class RockTextureParams
{
public:
	RockTextureParams() 
		: m_marbleDepth(4)
		, m_marbleTurb(2.5)
		, m_baseColor0(122/255.0, 138/255.0, 162/255.0)
		, m_baseColor1(38/255.0, 58/255.0, 85/255.0)
		, m_baseColor2(136/255.0, 87/255.0, 9/255.0)
		, m_darkColor(0.f)
		, m_scale(7)
		, m_noiseScaleColor(7.0)
		, m_noiseScaleHeight(6.0)
		, m_noiseScalePt(3.f)
		, m_lacunarity(0.7f)
		, m_H(1.9f)
		, m_octaves(5)
		, m_offset(2.f)
	{}

	int m_marbleDepth;
	float m_marbleTurb;
	Color m_baseColor0;
	Color m_baseColor1;
	Color m_baseColor2;
	Color m_darkColor;
	float m_scale;
	float m_noiseScaleColor;
	float m_noiseScaleHeight;
	float m_noiseScalePt;
	float m_lacunarity;
	float m_H;
	int m_octaves;
	float m_offset;

};
////////////////////////////////////////////////////////////////////////////////
// file scope globals

// cameras
static std::shared_ptr<Camera> g_curCamera;
static std::shared_ptr<Camera> g_mainCamera;	
static std::shared_ptr<Camera> g_debugCamera;

// main camera variables, used for saving/loading the camera
static vec3 g_focus, g_defaultFocus, g_defaultEye, g_defaultUp;

// display toggles
static int g_menuEnabled;
static int g_wireframe;
static int g_debugDisplay;

// orbit cam
static bool g_orbitCam = false;
static float g_orbitAngle;
static vec3 g_orbitFocus;
static vec3 g_orbitStart;
static float g_orbitRate;
static float g_orbitLength;

// debug cam
static vec3 g_fakeFocus = {-10.f, 0.f, 0.f};

// fps tracking
static float g_fpsDisplay;
static int g_frameCount;
static int g_frameSampleCount;
static Timer g_frameTimer;

// dt tracking
static float g_dt;
static Clock g_timer;

// lighting
static Color g_sunColor;
static vec3 g_sundir;

// control vars for debug texture render
static GLuint g_debugTexture;
static bool g_debugTextureSplit;

// screenshots & recording
static bool g_screenshotRequested = false;
static bool g_recording = false;
static int g_recordFps = 30;
static int g_recordCurFrame;
static int g_recordFrameCount = 300;
static Limits<float> g_recordTimeRange;

// default compute stuff
std::string g_defaultComputeDevice;

// demo specific stuff:
static constexpr int kRockTextureDim = 1024;
static GLuint g_rockTexture;
static GLuint g_rockHeightTexture;
static std::shared_ptr<Geom> g_rockGeom;
static RockTextureParams m_rockParams;
static std::shared_ptr<ComputeProgram> g_rockGenProgram;

////////////////////////////////////////////////////////////////////////////////
// Shaders

enum RockUniformLocType {
	ROCKBIND_DiffuseMap,
	ROCKBIND_HeightMap,
	ROCKBIND_TexDim,
};

static const std::vector<CustomShaderAttr> g_rockShaderUniforms = {
	{ ROCKBIND_DiffuseMap, "diffuseMap" },
	{ ROCKBIND_HeightMap, "heightMap" },
	{ ROCKBIND_TexDim, "texDim" },
};

static std::shared_ptr<ShaderInfo> g_rockShader ;

////////////////////////////////////////////////////////////////////////////////
// forward decls
static void record_Start();
static void generateRockTexture();
static void generateRockGeom();

////////////////////////////////////////////////////////////////////////////////
// tweak vars - these are checked into git
extern float g_tileDrawErrorThreshold;
static std::vector<std::shared_ptr<TweakVarBase>> g_tweakVars = {
	std::make_shared<TweakVector>("cam.eye", &g_defaultEye, vec3(8.f, 0.f, 2.f)),
	std::make_shared<TweakVector>("cam.focus", &g_defaultFocus),
	std::make_shared<TweakVector>("cam.up", &g_defaultUp, vec3(0,0,1)),
	std::make_shared<TweakVector>("cam.orbitFocus", &g_orbitFocus),
	std::make_shared<TweakVector>("cam.orbitStart", &g_orbitStart, vec3(100,100,100)),
	std::make_shared<TweakFloat>("cam.orbitRate", &g_orbitRate, M_PI/180.f),
	std::make_shared<TweakFloat>("cam.orbitLength", &g_orbitLength, 1000.f),
	std::make_shared<TweakVector>("lighting.sundir", &g_sundir, vec3(0,0,1)),
	std::make_shared<TweakColor>("lighting.suncolor", &g_sunColor, Color(1,1,1)),
	std::make_shared<TweakInt>("rocktexture.marbleDepth", &m_rockParams.m_marbleDepth, 4),
	std::make_shared<TweakFloat>("rocktexture.marbleTurb", &m_rockParams.m_marbleTurb, 2.5),
	std::make_shared<TweakColor>("rocktexture.baseColor0", &m_rockParams.m_baseColor0, Color(122/255.0, 138/255.0, 162/255.0)),
	std::make_shared<TweakColor>("rocktexture.baseColor1", &m_rockParams.m_baseColor1, Color(38/255.0, 58/255.0, 85/255.0)),
	std::make_shared<TweakColor>("rocktexture.baseColor2", &m_rockParams.m_baseColor2, Color(136/255.0, 87/255.0, 9/255.0)),
	std::make_shared<TweakColor>("rocktexture.darkColor", &m_rockParams.m_darkColor, Color(0.f)),
	std::make_shared<TweakFloat>("rocktexture.scale", &m_rockParams.m_scale, 7.f),
	std::make_shared<TweakFloat>("rocktexture.noiseScaleColor", &m_rockParams.m_noiseScaleColor, 7.f),
	std::make_shared<TweakFloat>("rocktexture.noiseScaleHeight", &m_rockParams.m_noiseScaleHeight, 6.f),
	std::make_shared<TweakFloat>("rocktexture.noiseScalePt", &m_rockParams.m_noiseScalePt, 3.f),
	std::make_shared<TweakFloat>("rocktexture.lacunarity", &m_rockParams.m_lacunarity, 0.7),
	std::make_shared<TweakFloat>("rocktexture.H", &m_rockParams.m_H, 2.0),
	std::make_shared<TweakInt>("rocktexture.octaves", &m_rockParams.m_octaves, 5),
	std::make_shared<TweakFloat>("rocktexture.offset", &m_rockParams.m_offset, 2),
};

static void SaveCurrentCamera()
{
	g_defaultEye = g_mainCamera->GetPos();
	g_defaultFocus = g_mainCamera->GetPos() + g_mainCamera->GetViewframe().m_fwd;
	g_defaultUp = Normalize(g_mainCamera->GetViewframe().m_up);
}

////////////////////////////////////////////////////////////////////////////////
// Settings vars - don't get checked in to git
static std::vector<std::shared_ptr<TweakVarBase>> g_settingsVars = {
	std::make_shared<TweakBool>("cam.orbit", &g_orbitCam, false),
	std::make_shared<TweakBool>("debug.wireframe", &g_wireframe, false),
	std::make_shared<TweakBool>("debug.fpsDisplay", &g_debugDisplay, false),
	std::make_shared<TweakBool>("debug.draw", 
			[](){ return dbgdraw_IsEnabled(); },
			[](bool enabled) { dbgdraw_SetEnabled(int(enabled)); },
			false),
	std::make_shared<TweakBool>("debug.drawdepth", 
			[](){ return dbgdraw_IsDepthTestEnabled(); },
			[](bool enabled) { dbgdraw_SetDepthTestEnabled(int(enabled)); },
			true),

	std::make_shared<TweakInt>("record.fps", &g_recordFps, 30),
	std::make_shared<TweakInt>("record.count", &g_recordFrameCount, 300),
	std::make_shared<TweakFloat>("record.timeStart", &g_recordTimeRange.m_min, 1.0),
	std::make_shared<TweakFloat>("record.timeEnd", &g_recordTimeRange.m_max, 2.0),
	std::make_shared<TweakString>("compute.device", &g_defaultComputeDevice)
};

////////////////////////////////////////////////////////////////////////////////
// Camera swap
static bool camera_GetDebugCamera()
{
	return (g_curCamera == g_debugCamera);
}

static void camera_SetDebugCamera(bool useDebug)
{
	if(useDebug) {
		g_curCamera = g_debugCamera;
	} else {
		g_curCamera = g_mainCamera;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Menu creation
static std::shared_ptr<TopMenuItem> MakeMenu()
{
	std::vector<std::shared_ptr<MenuItem>> cameraMenu = {
		std::make_shared<VecSliderMenuItem>("eye", &g_defaultEye),
		std::make_shared<VecSliderMenuItem>("focus", &g_defaultFocus),
		std::make_shared<VecSliderMenuItem>("orbit focus", &g_orbitFocus),
		std::make_shared<VecSliderMenuItem>("orbit start", &g_orbitStart),
		std::make_shared<FloatSliderMenuItem>("orbit rate", &g_orbitRate),
		std::make_shared<FloatSliderMenuItem>("orbit length", &g_orbitLength),
		std::make_shared<BoolMenuItem>("toggle orbit cam", &g_orbitCam),
		std::make_shared<ButtonMenuItem>("save current camera", SaveCurrentCamera)
	};
	std::vector<std::shared_ptr<MenuItem>> lightingMenu = {
		std::make_shared<VecSliderMenuItem>("sundir", &g_sundir),
		std::make_shared<ColorSliderMenuItem>("suncolor", &g_sunColor),
	};
	std::vector<std::shared_ptr<MenuItem>> debugMenu = {
		std::make_shared<ButtonMenuItem>("reload shaders", render_RefreshShaders),
		std::make_shared<BoolMenuItem>("wireframe", &g_wireframe),
		std::make_shared<BoolMenuItem>("fps & info", &g_debugDisplay),
		std::make_shared<BoolMenuItem>("debugcam", camera_GetDebugCamera, camera_SetDebugCamera),
		std::make_shared<IntSliderMenuItem>("debug texture id", 
			[&g_debugTexture](){return int(g_debugTexture);},
			[&g_debugTexture](int val) { g_debugTexture = static_cast<unsigned int>(val); }),
		std::make_shared<BoolMenuItem>("debug rendering", 
			[](){ return dbgdraw_IsEnabled(); },
			[](bool enabled) { dbgdraw_SetEnabled(int(enabled)); }),
		std::make_shared<BoolMenuItem>("debug depth test", 
			[](){ return dbgdraw_IsDepthTestEnabled(); },
			[](bool enabled) { dbgdraw_SetDepthTestEnabled(int(enabled)); }),
	};
	std::vector<std::shared_ptr<MenuItem>> textureMenu = {
		std::make_shared<ButtonMenuItem>("regenerate", [](){ generateRockTexture(); }),
		std::make_shared<IntSliderMenuItem>("marbleDepth", &m_rockParams.m_marbleDepth),
		std::make_shared<FloatSliderMenuItem>("marbleTurb", &m_rockParams.m_marbleTurb, 0.1f),
		std::make_shared<ColorSliderMenuItem>("baseColor0", &m_rockParams.m_baseColor0),
		std::make_shared<ColorSliderMenuItem>("baseColor1", &m_rockParams.m_baseColor1),
		std::make_shared<ColorSliderMenuItem>("baseColor2", &m_rockParams.m_baseColor2),
		std::make_shared<ColorSliderMenuItem>("darkColor", &m_rockParams.m_darkColor),
		std::make_shared<FloatSliderMenuItem>("scale", &m_rockParams.m_scale, 1.f),
		std::make_shared<FloatSliderMenuItem>("noiseScaleColor", &m_rockParams.m_noiseScaleColor, 1.f),
		std::make_shared<FloatSliderMenuItem>("noiseScaleHeight", &m_rockParams.m_noiseScaleHeight, 1.f),
		std::make_shared<FloatSliderMenuItem>("noiseScalePt", &m_rockParams.m_noiseScalePt, 1.f),
		std::make_shared<FloatSliderMenuItem>("lacunarity", &m_rockParams.m_lacunarity, 0.1f),
		std::make_shared<FloatSliderMenuItem>("H", &m_rockParams.m_H, 0.1f),
		std::make_shared<IntSliderMenuItem>("octaves", &m_rockParams.m_octaves),
		std::make_shared<FloatSliderMenuItem>("offset", &m_rockParams.m_offset, 1.f),
		std::make_shared<ButtonMenuItem>("recompile", [](){ g_rockGenProgram->Recompile(); }),
	};
	std::vector<std::shared_ptr<MenuItem>> geomMenu = {
		std::make_shared<ButtonMenuItem>("regenerate", [](){ generateRockGeom(); }),
	};
	std::vector<std::shared_ptr<MenuItem>> tweakMenu = {
		std::make_shared<SubmenuMenuItem>("cam", std::move(cameraMenu)),
		std::make_shared<SubmenuMenuItem>("lighting", std::move(lightingMenu)),
		std::make_shared<SubmenuMenuItem>("texture", std::move(textureMenu)),
		std::make_shared<SubmenuMenuItem>("geom", std::move(geomMenu)),
		std::make_shared<SubmenuMenuItem>("debug", std::move(debugMenu)),
	};
	std::vector<std::shared_ptr<MenuItem>> recordMenu = {
		std::make_shared<ButtonMenuItem>("take screenshot", [](){ g_screenshotRequested = true; }),
		std::make_shared<IntSliderMenuItem>("record fps", &g_recordFps),
		std::make_shared<IntSliderMenuItem>("record frame count", &g_recordFrameCount),
		std::make_shared<FloatSliderMenuItem>("start time", &g_recordTimeRange.m_min),
		std::make_shared<FloatSliderMenuItem>("end time", &g_recordTimeRange.m_max),
		std::make_shared<ButtonMenuItem>("start", record_Start),
	};
	std::vector<std::shared_ptr<MenuItem>> computeMenu = {
		compute_CreateDeviceMenu(),
	};
	std::vector<std::shared_ptr<MenuItem>> topMenu = {
		std::make_shared<SubmenuMenuItem>("tweak", std::move(tweakMenu)),
		std::make_shared<SubmenuMenuItem>("record", std::move(recordMenu)),
		std::make_shared<SubmenuMenuItem>("compute", std::move(computeMenu)),
	};
	return std::make_shared<TopMenuItem>(topMenu);
}

////////////////////////////////////////////////////////////////////////////////
static void record_Start()
{
	if(g_recording) return;

	g_recordCurFrame = 0;
	g_dt = 1.f / g_recordFps;
	g_recording = true;
}

static void record_SaveFrame()
{
	ASSERT(g_recording);

	mkdir("frames", S_IRUSR | S_IWUSR | S_IXUSR);
	std::stringstream sstr ;
	sstr << "frames/frame_" << g_recordCurFrame << ".tga" ;
	std::cout << "Saving frame " << sstr.str() << std::endl;
	render_SaveScreen(sstr.str().c_str());
}

static void record_Advance()
{
	ASSERT(g_recording);

	++g_recordCurFrame;
	if(g_recordCurFrame >= g_recordFrameCount)
		g_recording = false;
	std::cout << "done." << std::endl;
}

static void drawRockGeom(const vec3& sundir, const mat4& matProjView)
{
	mat4 model = MakeScale(vec3(100.0f));
	mat4 modelIT = TransposeOfInverse(model);
	mat4 mvp = matProjView * model;

	const ShaderInfo* shader = g_rockShader.get();
	glUseProgram(shader->m_program);

	GLint mvpLoc = shader->m_uniforms[BIND_Mvp];
	GLint modelLoc = shader->m_uniforms[BIND_Model];
	GLint modelITLoc = shader->m_uniforms[BIND_ModelIT];
	GLint eyePosLoc = shader->m_uniforms[BIND_Eyepos];
	GLint sundirLoc = shader->m_uniforms[BIND_Sundir];
	GLint sunColorLoc = shader->m_uniforms[BIND_SunColor];
	GLint diffuseMapLoc = shader->m_custom[ROCKBIND_DiffuseMap];
	GLint heightMapLoc = shader->m_custom[ROCKBIND_HeightMap];
	GLint texDimLoc = shader->m_custom[ROCKBIND_TexDim];

	glUniformMatrix4fv(mvpLoc, 1, 0, mvp.m);
	glUniformMatrix4fv(modelLoc, 1, 0, model.m);
	glUniformMatrix4fv(modelITLoc, 1, 0, modelIT.m);
	glUniform3fv(sundirLoc, 1, &sundir.x);
	glUniform3fv(sunColorLoc, 1, &g_sunColor.r);
	glUniform3fv(eyePosLoc, 1, &g_curCamera->GetPos().x);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_rockTexture);
	glUniform1i(diffuseMapLoc, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, g_rockHeightTexture);
	glUniform1i(heightMapLoc, 1);
	constexpr float invTexDim = 1.0 / kRockTextureDim;
	glUniform2f(texDimLoc, invTexDim, invTexDim);

	g_rockGeom->Render(*shader);
}

////////////////////////////////////////////////////////////////////////////////
static void draw(Framedata& frame)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0.0f,0.0f,0.0f,1.f);
	glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	int scissorHeight = g_screen.m_width/1.777;
	glScissor(0, 0.5*(g_screen.m_height - scissorHeight), g_screen.m_width, scissorHeight);
	if(!camera_GetDebugCamera()) {
		glEnable(GL_SCISSOR_TEST);
	}

	if(g_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	vec3 normalizedSundir = Normalize(g_sundir);

	////////////////////////////////////////////////////////////////////////////////
	if(!camera_GetDebugCamera()) glEnable(GL_SCISSOR_TEST);
	glEnable(GL_DEPTH_TEST);

	mat4 matProjView = g_curCamera->GetProj() * g_curCamera->GetView();
	
	// render rock geom
	drawRockGeom(normalizedSundir, matProjView);
	
	// everything below here is feedback for the user, so record the frame if we're recording
	if(g_recording)
	{
		record_SaveFrame();
		record_Advance();
	}

	// debug draw
	dbgdraw_Render(*g_curCamera);
	checkGlError("draw(): post dbgdraw");

	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	render_drawDebugTexture(g_debugTexture, g_debugTextureSplit);

	if(g_menuEnabled)
		menu_Draw(*g_curCamera);

	checkGlError("draw(): post menu");

	if(g_debugDisplay)
	{
		char fpsStr[32] = {};
		static Color fpsCol = {1,1,1};
		snprintf(fpsStr, sizeof(fpsStr) - 1, "%.2f", g_fpsDisplay);
		font_Print(g_screen.m_width-180,24, fpsStr, fpsCol, 16.f);

		char cameraPosStr[64] = {};
		const vec3& pos = g_curCamera->GetPos();
		snprintf(cameraPosStr, sizeof(cameraPosStr) - 1, "eye: %.2f %.2f %.2f", pos.x, pos.y, pos.z);
		font_Print(g_screen.m_width-180, 40, cameraPosStr, fpsCol, 16.f);
	}

	task_RenderProgress();
	checkGlError("end draw");

	if(g_screenshotRequested) {
		render_SaveScreen("screenshot.tga");
		g_screenshotRequested = false;
	}
	
	SDL_GL_SwapBuffers();
	checkGlError("swap");
	
}

////////////////////////////////////////////////////////////////////////////////	
static void generateRockTexture()
{
	auto rockKernel = g_rockGenProgram->CreateKernel("generateRockTexture");
	if(!rockKernel)
		return;

	auto imageObj = compute_CreateImage2DWO(kRockTextureDim, kRockTextureDim,
		CL_RGBA, CL_UNORM_INT8);
	auto heightObj = compute_CreateImage2DWO(kRockTextureDim, kRockTextureDim,
		CL_R, CL_UNORM_INT8);
	rockKernel->SetArg(0, imageObj.get());
	rockKernel->SetArg(1, heightObj.get());
	rockKernel->SetArg(2, &m_rockParams.m_marbleDepth);
	rockKernel->SetArg(3, &m_rockParams.m_marbleTurb);
	rockKernel->SetArg(4, &m_rockParams.m_baseColor0);
	rockKernel->SetArg(5, &m_rockParams.m_baseColor1);
	rockKernel->SetArg(6, &m_rockParams.m_baseColor2);
	rockKernel->SetArg(7, &m_rockParams.m_darkColor);
	rockKernel->SetArg(8, &m_rockParams.m_scale);
	rockKernel->SetArg(9, &m_rockParams.m_noiseScaleColor);
	rockKernel->SetArg(10, &m_rockParams.m_noiseScaleHeight);
	rockKernel->SetArg(11, &m_rockParams.m_noiseScalePt);
	rockKernel->SetArg(12, &m_rockParams.m_lacunarity);
	rockKernel->SetArg(13, &m_rockParams.m_H);
	rockKernel->SetArg(14, &m_rockParams.m_octaves);
	rockKernel->SetArg(15, &m_rockParams.m_offset);

	auto event = rockKernel->EnqueueEv(2, (const size_t[]){kRockTextureDim, kRockTextureDim});

	std::vector<unsigned char> hostImageData(kRockTextureDim * kRockTextureDim * 4);
	std::vector<unsigned char> hostHeightData(kRockTextureDim * kRockTextureDim);
	compute_EnqueueWaitForEvent(event);
	imageObj->EnqueueRead(
		(const size_t[]){0,0,0}, 
		(const size_t[]){kRockTextureDim, kRockTextureDim, 1},
		&hostImageData[0]);
	heightObj->EnqueueRead(
		(const size_t[]){0,0,0}, 
		(const size_t[]){kRockTextureDim, kRockTextureDim, 1},
		&hostHeightData[0]);
	compute_Finish();	

	// copy to the texture
	if(!g_rockTexture)
		glGenTextures(1, &g_rockTexture);
	if(!g_rockHeightTexture)
		glGenTextures(1, &g_rockHeightTexture);

	glBindTexture(GL_TEXTURE_2D, g_rockTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kRockTextureDim, kRockTextureDim, 0, GL_RGBA, GL_UNSIGNED_BYTE, &hostImageData[0]);
	glGenerateMipmap(GL_TEXTURE_2D);
	
	glBindTexture(GL_TEXTURE_2D, g_rockHeightTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kRockTextureDim, kRockTextureDim, 0, GL_RED, GL_UNSIGNED_BYTE, &hostHeightData[0]);
	glGenerateMipmap(GL_TEXTURE_2D);
}

static void generateRockGeom()
{
	auto densityKernel = g_rockGenProgram->CreateKernel("generateRockDensity");
	if(!densityKernel)
		return;
	
	constexpr int kDensityDim = 256;
	constexpr int densityBufferSliceSize = kDensityDim * kDensityDim ;
	auto densityBufferA = compute_CreateBufferWO(densityBufferSliceSize);
	auto densityBufferB = compute_CreateBufferWO(densityBufferSliceSize);

	densityKernel->SetArgVal(1, 0.5f); // radius
	densityKernel->SetArg(2, sizeof(cl_float3), (cl_float3[]){{{10.0f,10.0f,10.0f}}});
	densityKernel->SetArgVal(3, 2.f); // H
	densityKernel->SetArgVal(4, 0.7f); // lacunarity
	densityKernel->SetArgVal(5, 8.8f); // octaves

	std::vector<unsigned char> hostDensityBuffer(densityBufferSliceSize * kDensityDim);

	int curBuffer = 0;
	ComputeEvent lastEvent[2];
	const ComputeBuffer* buffers[2] = { densityBufferA.get(), densityBufferB.get() };
	for(int z = 0; z < 2; ++z)
	{
		densityKernel->SetArg(0, buffers[curBuffer]);
		auto taskEv = densityKernel->EnqueueEv(3, 
				(const size_t[]){0, 0, z},
				(const size_t[]){kDensityDim, kDensityDim, 1},
				(const size_t[]){16,16,1},
				lastEvent[curBuffer].m_event ? 1 : 0, 
				(const cl_event[]){lastEvent[curBuffer].m_event});
		auto readEv = buffers[curBuffer]->EnqueueRead(0, densityBufferSliceSize, 
			&hostDensityBuffer[z * densityBufferSliceSize], 
			1, (const cl_event[]){taskEv.m_event});
		lastEvent[curBuffer] = readEv;
		curBuffer = curBuffer ^ 1;
	}
	compute_Finish();

	// TODO kick marching tetrahedrons

	// TODO create geom from marching tetrahedron output

	// ahem.
	g_rockGeom = render_GenerateSphereGeom(100,100);
}

////////////////////////////////////////////////////////////////////////////////	
static void initialize()
{
	tweaker_LoadVars(".settings", g_settingsVars);
	tweaker_LoadVars("tweaker.txt", g_tweakVars);

	//task_Startup(3);
	dbgdraw_Init();
	render_Init();
	framemem_Init(); 
	font_Init();
	menu_SetTop(MakeMenu());
	ui_Init();
	compute_Init(g_defaultComputeDevice.c_str());
	g_defaultComputeDevice = compute_GetCurrentDeviceName();
	g_rockGenProgram = compute_CompileProgram("programs/rock.cl");
	g_rockShader = render_CompileShader("shaders/rock.glsl", g_rockShaderUniforms);
	generateRockTexture();
	generateRockGeom();

	g_mainCamera = std::make_shared<Camera>(30.f, g_screen.m_aspect);
	g_debugCamera = std::make_shared<Camera>(30.f, g_screen.m_aspect);

	g_mainCamera->LookAt(g_defaultFocus, g_defaultEye, Normalize(g_defaultUp));
	g_curCamera = g_mainCamera;
}

////////////////////////////////////////////////////////////////////////////////
static void orbitcam_Update()
{
	if(!g_orbitCam) return;
		
	static const vec3 kUp(0,0,1);
	vec3 startPos = g_orbitStart;
	mat4 rotZ = RotateAround(kUp, g_orbitAngle);
	vec3 pos = TransformPoint(rotZ, startPos);

	pos *= g_orbitLength / Length(pos);

	g_mainCamera->LookAt(g_orbitFocus, pos, kUp);

	g_orbitAngle += g_dt * g_orbitRate;
	g_orbitAngle = AngleWrap(g_orbitAngle);
}

static void updateFps()
{
	int numFrames = g_frameCount - g_frameSampleCount;
	if(numFrames >= 30 * 5)
	{
		g_frameTimer.Stop();
		
		float delta = g_frameTimer.GetTime();
		float fps = (numFrames) / delta;
		g_frameSampleCount = g_frameCount;
		g_fpsDisplay = fps;

		g_frameTimer.Start();
	}
}

////////////////////////////////////////////////////////////////////////////////
static void update(Framedata& frame)
{
	if(!g_recording)
	{
		constexpr float kMinDt = 1.0f/60.f;
		g_timer.Step(kMinDt);
		g_dt = g_timer.GetDt();
	}
	else
	{
		g_dt = 1.0 / g_recordFps;
	}

	orbitcam_Update();

	updateFps();

	g_curCamera->Compute();

	menu_Update(g_dt);		

	gputask_Join();
	gputask_Kick();

	task_Update();
}

////////////////////////////////////////////////////////////////////////////////
enum CameraDirType {
	CAMDIR_FWD, 
	CAMDIR_BACK,
	CAMDIR_LEFT, 
	CAMDIR_RIGHT
};

static void move_camera(int dir)
{	
	int mods = SDL_GetModState();
	float kScale = 50.0;
	if(mods & KMOD_SHIFT && mods & KMOD_CTRL) kScale *= 100.f;
	else if (mods & KMOD_SHIFT) kScale *= 10.0f;

	vec3 dirs[] = {
		g_curCamera->GetViewframe().m_fwd,
		g_curCamera->GetViewframe().m_side,
	};

	vec3 off = dirs[dir>>1] * kScale * g_dt;
	if(1&dir) off = -off;

	g_curCamera->MoveBy(off);
}

////////////////////////////////////////////////////////////////////////////////
static void resize(int w, int h)
{
	g_screen.Resize(w,h);
	g_mainCamera->SetAspect(g_screen.m_aspect);
	g_debugCamera->SetAspect(g_screen.m_aspect);
	g_mainCamera->ComputeViewFrustum();
	g_debugCamera->ComputeViewFrustum();
	glViewport(0,0,w,h);
}

////////////////////////////////////////////////////////////////////////////////
int main(void)
{
	SDL_Event event;

	SDL_SetVideoMode(g_screen.m_width, g_screen.m_height, 0, SDL_OPENGL | SDL_RESIZABLE);
	glewInit();
	glViewport(0,0,g_screen.m_width, g_screen.m_height);

	//SDL_ShowCursor(g_menuEnabled ? SDL_ENABLE : SDL_DISABLE );
	SDL_ShowCursor(SDL_ENABLE);

	glEnable(GL_TEXTURE_1D);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_3D);

	initialize();
	g_frameTimer.Start();

	checkGlError("after init");
	int done = 0;
	float keyRepeatTimer = 0.f;
	float nextKeyTimer = 0.f;
	int turning = 0, rolling = 0;
	int xTurnCenter = 0, yTurnCenter = 0;
	do
	{
		if(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_KEYDOWN:
					{
						keyRepeatTimer = 0.f;
						nextKeyTimer = 0.f;
						if(g_menuEnabled)
						{
							if(event.key.keysym.sym == SDLK_SPACE)
								g_menuEnabled = 1 ^ g_menuEnabled;
							else
								menu_Key(event.key.keysym.sym, event.key.keysym.mod);
						}
						else
						{
							switch(event.key.keysym.sym)
							{
								case SDLK_ESCAPE:
									done = 1;
									break;
								case SDLK_SPACE:
									g_menuEnabled = 1 ^ g_menuEnabled;
									SDL_ShowCursor(g_menuEnabled ? SDL_ENABLE : SDL_DISABLE );
									break;
								case SDLK_KP_PLUS:
									++g_debugTexture;
									break;
								case SDLK_KP_MINUS:
									if(g_debugTexture>0)
										--g_debugTexture;
									break;
								case SDLK_KP_ENTER:
									g_debugTextureSplit = !g_debugTextureSplit;
									break;
								case SDLK_PAGEUP:
									{
										vec3 off = 10.f * g_curCamera->GetViewframe().m_up ;
										g_curCamera->MoveBy(off);
									}
									break;
								case SDLK_PAGEDOWN:
									{
										vec3 off = -10.f * g_curCamera->GetViewframe().m_up;
										g_curCamera->MoveBy(off);
									}
									break;
								case SDLK_p:
									if(event.key.keysym.mod & KMOD_SHIFT)
										g_screenshotRequested = true;
									break;

								default: break;
							}
						}
					}
					break;

				case SDL_MOUSEBUTTONDOWN:
					{
						int xPos, yPos;
						Uint8 buttonState = SDL_GetMouseState(&xPos, &yPos);
						turning = (buttonState & SDL_BUTTON(SDL_BUTTON_LEFT));
						rolling = (buttonState & SDL_BUTTON(SDL_BUTTON_RIGHT));
						if(turning || rolling) {
							xTurnCenter = xPos; yTurnCenter = yPos;
						}
					}
					break;
				case SDL_MOUSEBUTTONUP:
					{
						int xPos, yPos;
						Uint8 buttonState = SDL_GetMouseState(&xPos, &yPos);
						turning = (buttonState & SDL_BUTTON(SDL_BUTTON_LEFT));
						rolling = (buttonState & SDL_BUTTON(SDL_BUTTON_RIGHT));
					}
					break;
				case SDL_KEYUP:
					{
						keyRepeatTimer = 0.f;
						nextKeyTimer = 0.f;
					}
					break;
	
		
				case SDL_VIDEORESIZE:
				{
					int w = Max(event.resize.w, 256);
					int h = Max(event.resize.h, 256);
					SDL_Surface* surface = SDL_SetVideoMode(w, h, 0,
						SDL_OPENGL | SDL_RESIZABLE);
					if(!surface) {
						std::cerr << "Failed to resize window." << std::endl;
						abort();
					}
					resize(event.resize.w, event.resize.h);
				}
					
					break;
				case SDL_QUIT:
					done = 1;
					break;
				default: break;
			}
		}
	
		if(turning || rolling)
		{
			int xPos, yPos;
			(void)SDL_GetMouseState(&xPos, &yPos);
			int xDelta = xTurnCenter - xPos;
			int yDelta = yTurnCenter - yPos;
			if(turning)
			{
				float turn = (xDelta / g_screen.m_width) * (1 / 180.f) * M_PI;
				float tilt = -(yDelta / g_screen.m_height) * (1 / 180.f) * M_PI;
				g_curCamera->TurnBy(turn);
				g_curCamera->TiltBy(tilt);
			}

			if(rolling)
			{	
				float roll = -(xDelta / g_screen.m_width) * (1 / 180.f) * M_PI;
				g_curCamera->RollBy(roll);
			}
		}

		if(!g_menuEnabled)
		{
			const Uint8* keystate = SDL_GetKeyState(NULL);
			if(keystate[SDLK_w])
				move_camera(CAMDIR_FWD);
			if(keystate[SDLK_a])
				move_camera(CAMDIR_LEFT);
			if(keystate[SDLK_s])
				move_camera(CAMDIR_BACK);
			if(keystate[SDLK_d])
				move_camera(CAMDIR_RIGHT);
		}

		// clear old frame scratch space and recreate it.
		dbgdraw_Clear();
		framemem_Clear();
		Framedata* frame = frame_New();

		update(*frame);
		draw(*frame);
		g_frameCount++;

		keyRepeatTimer += g_dt;
		if(g_menuEnabled && keyRepeatTimer > 0.33f)
		{
			nextKeyTimer += g_dt;
			if(nextKeyTimer > 0.1f)
			{
				int mods = SDL_GetModState();
				const Uint8* keystate = SDL_GetKeyState(NULL);
				for(int i = 0; i < SDLK_LAST; ++i)
				{
					if(keystate[i])
						menu_Key(i, mods);
				}
				nextKeyTimer = 0.f;
			}
		}


	} while(!done);

	task_Shutdown();

	tweaker_SaveVars("tweaker.txt", g_tweakVars);
	tweaker_SaveVars(".settings", g_settingsVars);

	SDL_Quit();

	return 0;
}

