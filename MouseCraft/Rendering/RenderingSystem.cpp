#include "RenderingSystem.h"

#include <typeinfo>
#include <typeindex>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "Shader.h"
#include "Constants.h"
#include "PostProcess\PostProcess.h"
#include "PostProcess\NegativePP.h"
#include "PostProcess\BlurPP.h"
#include "PostProcess\FxaaPP.h"
//#include "../Core/Game.h"
// #include "../Core/ComponentManager.h"
#include "../ComponentMan.h"
#include "../Core/OmegaEngine.h"
#include "TextRenderer.h"

RenderingSystem::RenderingSystem() : System()
{
	// system initialization 
	// onlyReceiveFrameUpdates = true;

	// create profiler 
	profiler.InitializeTimers(10);		// 1 for each pass so far 
	cpuProfiler.InitializeTimers(10);	// 1 for each pass, 1 for general use, 1 for initialization
	cpuProfiler.LogOutput("RenderingSystem.log")
		.FormatMilliseconds(true);
		//.PrintOutput(true);
	
	cpuProfiler.StartTimer(7);

	// initialize default shaders  
	geometryShader = new Shader("shaders/geometry_vertex.glsl", "shaders/geometry_fragment.glsl");
	compDLightShader = new Shader("shaders/comp_vertex.glsl", "shaders/comp_dl_fragment.glsl");
	compPLightShader = new Shader("shaders/comp_vertex.glsl", "shaders/comp_pl_fragment.glsl");
	shadowmapShader = new Shader("shaders/shadowmap_vertex.glsl", "shaders/shadowmap_fragment.glsl");
	imageShader = new Shader("shaders/image_vertex.glsl", "shaders/image_fragment.glsl");
	postShader = new Shader("shaders/post_vertex.glsl", "shaders/post_fragment.glsl");
	postToScreenShader = new Shader("shaders/post2screen_vertex.glsl", "shaders/post2screen_fragment.glsl");
	skyboxShader = new Shader("shaders/skybox_vertex.glsl", "shaders/skybox_fragment.glsl");

	// initialize frame buffers for geometry rendering pass 
	// InitializeFrameBuffers(); waiting for screen size
	
	// create a quad that covers the screen for the composition pass 
	InitializeScreenQuad();
	// create a cube that can be used to render cubemaps
	InitializeScreenCube();

	// built-in post processing 
	// pseudo FXAA
	auto fxaa = std::make_unique<FxaaPP>();
	addPostProcess("FXAA", std::move(fxaa));
	// fog 
	auto fogPp = std::make_unique<PostProcess>();	// too lazy to make a dedicated class
	fogPp->shader = std::make_unique<Shader>("shaders/pp_base_vertex.glsl", "shaders/pp_fog_fragment.glsl");
	fogPp->settings = std::make_unique<Material>();	// u_FogDensity, u_FogStart, u_FogColor
	fogPp->enabled = false;
	addPostProcess("Fog", std::move(fogPp));
	// outline
	auto outlinePp = std::make_unique<PostProcess>();	// too lazy to make a dedicated class
	outlinePp->shader = std::make_unique<Shader>("shaders/pp_base_vertex.glsl", "shaders/pp_outline_fragment.glsl");
	outlinePp->settings = std::make_unique<Material>();	// u_FogDensity, u_FogStart, u_FogColor
	outlinePp->enabled = true;
	addPostProcess("Outline", std::move(outlinePp));
	// blur 
	auto blurHPp = std::make_unique<BlurPP>();
	blurHPp->settings->SetBool("u_Horizontal", true);
	blurHPp->enabled = false;
	addPostProcess("BlurH", std::move(blurHPp));
	auto blurVPp = std::make_unique<BlurPP>();
	blurVPp->settings->SetBool("u_Horizontal", false);
	blurVPp->enabled = false;
	addPostProcess("BlurV", std::move(blurVPp));

	// default skybox 
	std::vector<std::string> skyboxFaces = {
		"textures/cubemaps/skybox/right.jpg",
		"textures/cubemaps/skybox/left.jpg",
		"textures/cubemaps/skybox/up.jpg",
		"textures/cubemaps/skybox/down.jpg",
		"textures/cubemaps/skybox/front.jpg",
		"textures/cubemaps/skybox/back.jpg"
	};
	// auto skyboxTexId = Game::instance().loader.LoadCubemap(skyboxFaces);
	// setSkybox(skyboxTexId);

	cpuProfiler.StopTimer(7);
}

RenderingSystem::~RenderingSystem()
{
	System::~System();
}

void RenderingSystem::SetSize(unsigned int width, unsigned int height)
{
	if (width == 0 || height == 0)
	{
		std::cerr << "ERROR: Screen size cannot be 0" << std::endl;
		return;
	}

	screenWidth = width;
	screenHeight = height;
	InitializeFrameBuffers();
	TextRenderer::Instance().SetScreenSize(width, height);
	
}

// todo: refactor with current system
void RenderingSystem::SetCamera(Camera * camera)
{
	this->activeCamera = camera;
}


void RenderingSystem::RenderGeometryPass()
{
	// 1. First pass - Geometry into gbuffers 
	// cleanup 
	glBindFramebuffer(GL_FRAMEBUFFER, ppWriteFBO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, ppReadFBO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	geometryShader->use();
	// bind geometry frame buffers 
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glDrawBuffers(3, attachments);	// need to clear out ALL frame buffers (even though we only draw to 3 + depth at the moment).
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	// begin rendering each renderable 
	geometryShader->setMat4(SHADER_PROJECTION, projection);
	geometryShader->setMat4(SHADER_VIEW, view);
	
	// go thru each component 
	auto components = ComponentMan<RenderComponent>::Instance().All();
	
	for (auto& rc : components)
	{
		auto model = rc->GetEntity()->transform.getWorldTransformation();
		geometryShader->setMat4(SHADER_MODEL, model);
		
		// ensure default values incase material does not have it
		geometryShader->setVec3(SHADER_DIFFUSE, glm::vec3(1,1,1));
		geometryShader->setVec3(SHADER_SPECULAR, glm::vec3(1,1,1));
		geometryShader->setVec3(SHADER_AMBIENT, glm::vec3(1,1,1));

		// go thru each renderable package 
		for (int i = 0; i < rc->renderables.size(); i++)
		{
			auto r = rc->renderables[i];
			r->material->LoadMaterial(geometryShader, 0);
			glBindVertexArray(r->mesh->VAO);
			glDrawElements(GL_TRIANGLES, r->mesh->indices.size(), GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);
			/* Proper way
			if (custom shader has been set) then
				shader use
				shader set model 
				load material (shader) 
				load mesh () 
			*/
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderingSystem::RenderShadowMapsPass()
{
	// 2nd Pass - lighting shadow map   
	auto components = ComponentMan<DirectionalLight>::Instance().All();
	auto render_components = ComponentMan<RenderComponent>::Instance().All();
	for (auto& dl : components)
	{
		shadowmapShader->use();
		shadowmapShader->setMat4("u_LightSpaceMatrix", dl->getLightSpaceMatrix());
		dl->PrepareShadowmap(shadowmapShader);

		// go thru each component 
		for (auto& rc : render_components)
		{
			//auto model = rc->GetEntity()->transform.getWorldTransformation();		// this can be optimized 
			//shadowmapShader->setMat4("u_Model", rc->GetEntity()->transform.getWorldTransformation());
			
			// above 2 lines is equivalent to below (but below is 2-4x faster for some reason).
			glUniformMatrix4fv(
				glGetUniformLocation(shadowmapShader->programId, "u_Model"),
				1,
				GL_FALSE,
				&rc->GetEntity()->transform.getWorldTransformation()[0][0]);
			
			shadowmapShader->setMat4(SHADER_MODEL, rc->GetEntity()->transform.getWorldTransformation());

			// go thru each renderable package 
			for (int i = 0; i < rc->renderables.size(); i++)
			{
				auto r = rc->renderables[i];
				glBindVertexArray(r->mesh->VAO);
				glDrawElements(GL_TRIANGLES, r->mesh->indices.size(), GL_UNSIGNED_INT, 0);
				glBindVertexArray(0);
			}
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, screenWidth, screenHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void RenderingSystem::RenderDirectionalLightingPass()
{
	// 3rd pass - composision
	glBindFramebuffer(GL_FRAMEBUFFER, ppWriteFBO);
	glDrawBuffers(1, ppAttachments);

	compDLightShader->use();
	compDLightShader->setInt("u_PosTex", 0);	// set order 
	compDLightShader->setInt("u_NrmTex", 1);
	compDLightShader->setInt("u_ColTex", 2);
	compDLightShader->setInt("u_DphTex", 3);
	compDLightShader->setInt("u_ShadowMap", 4);
	compDLightShader->setVec3("u_ViewPosition", activeCamera->GetEntity()->transform.getWorldPosition());

	// enable the sampler2D shader variables 
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, posTex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, nrmTex);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, colTex);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, dphTex);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	auto dlights = ComponentMan<DirectionalLight>::Instance().All();
	for (auto& dl : dlights)
	{
		compDLightShader->setVec3("u_LightPos", dl->GetEntity()->transform.getWorldPosition());
		compDLightShader->setMat4("u_LightSpaceMatrix", dl->getLightSpaceMatrix());
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, dynamic_cast<DirectionalLight*>(dl)->TexId);

		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindVertexArray(0);
	}

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	profiler.StopTimer(2);
	cpuProfiler.StopTimer(2);
}

void RenderingSystem::RenderPointLightingPass()
{
	glBindFramebuffer(GL_FRAMEBUFFER, ppWriteFBO);
	glDrawBuffers(1, ppAttachments);

	compPLightShader->use();
	compPLightShader->setInt("u_PosTex", 0);	// set order 
	compPLightShader->setInt("u_NrmTex", 1);
	compPLightShader->setInt("u_ColTex", 2);
	compPLightShader->setInt("u_DphTex", 3);
	compPLightShader->setInt("u_ShadowMap", 4);
	compPLightShader->setVec3("u_ViewPosition", activeCamera->GetEntity()->transform.getWorldPosition());
	// we're rendering from camera this time - using light volumes
	compPLightShader->setMat4("u_Projection", projection);
	compPLightShader->setMat4("u_View", view);

	// enable the sampler2D shader variables 
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, posTex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, nrmTex);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, colTex);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, dphTex);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	auto plights = ComponentMan<PointLight>::Instance().All();
	for (auto& pl : plights)
	{
		compPLightShader->setVec3(SHADER_LIGHT_POS, pl->GetEntity()->transform.getWorldPosition());
		compPLightShader->setVec3(SHADER_LIGHT_COLOR, pl->color);
		compPLightShader->setFloat(SHADER_LIGHT_INTENSITY, pl->intensity);
		compPLightShader->setFloat(SHADER_LIGHT_RANGE, pl->range);

		// scale model matrix (for light volume) one more time for light range 
		auto model = glm::scale(
			pl->GetEntity()->transform.getWorldTransformation(),
			glm::vec3(pl->range));
		compPLightShader->setMat4(SHADER_MODEL, model);

		// we're using a cube instead of a sphere b/c of less vertices 
		//glBindVertexArray(cubeVAO);
		//glDrawArrays(GL_TRIANGLES, 0, 36);
		//glBindVertexArray(0);

		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindVertexArray(0);
	}

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// 3.5th pass - skybox pass (should be relatively cheap).
void RenderingSystem::RenderSkyboxPass()
{
	// skybox pass 
	if (skyboxTexture != 0)
	{
		// set render target 
		glBindFramebuffer(GL_FRAMEBUFFER, ppWriteFBO);
		glDrawBuffers(1, ppAttachments);
		// load settings 
		// glDisable(GL_DEPTH_TEST);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		skyboxShader->use();
		skyboxShader->setMat4("u_Projection", projection);
		skyboxShader->setMat4("u_View", glm::mat4(glm::mat3(view)));	// view -> mat3 (remove translation) -> mat4 (compatable format)
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
		// draw 
		glBindVertexArray(cubeVAO);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);
		// we're going to manually revert the depth test (b/c we don't want every single other pass doing it)
		glDepthFunc(GL_LESS);
	}
}

void RenderingSystem::RenderPostProcessPass()
{
	// post processing 
	glDisable(GL_DEPTH_TEST);

	for (auto& pp : _postProcesses)
	{
		if (!pp.second->enabled) continue;

		// we keep on swapping the finished read/write texture so PP can read/write to current image 
		std::swap(finWriteTex, finReadTex);				// allow pp to read from current rendered image 
		std::swap(ppWriteFBO, ppReadFBO);				// allow pp to write to "current rendered" image (for next pp)
		glBindFramebuffer(GL_FRAMEBUFFER, ppWriteFBO);
		glDrawBuffers(1, ppAttachments);

		pp.second->shader->use();
		// set order 
		pp.second->shader->setInt("u_PosTex", 0);
		pp.second->shader->setInt("u_NrmTex", 1);
		pp.second->shader->setInt("u_ColTex", 2);
		pp.second->shader->setInt("u_DphTex", 3);
		pp.second->shader->setInt("u_FinTex", 4);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, posTex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, nrmTex);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, colTex);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, dphTex);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, finReadTex);

		// load post-process settings 
		pp.second->settings->LoadMaterial(pp.second->shader.get(), GL_TEXTURE5);

		// render 
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindVertexArray(0);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// render to back buffer
	postToScreenShader->use();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, finWriteTex);	// load in the last thing post process wrote in
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

void RenderingSystem::RenderUIImagesPass()
{
	/*
	// 5th pass - UI images 
	profiler.StartTimer(4);
	cpuProfiler.StartTimer(4);

	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// render text components 
	std::sort(_images.begin(), _images.end(), View::comparePointers);	// sort for rendering order
	for (int i = 0; i < _images.size(); i++)
	{
		RenderImage(*imageShader, _images[i]);
	}

	profiler.StopTimer(4);
	cpuProfiler.StopTimer(4);
	*/
}

void RenderingSystem::RenderUITextPass()
{
	/*
	// 6th pass - UI text 
	profiler.StartTimer(5);
	cpuProfiler.StartTimer(5);

	// render text components 
	std::sort(_texts.begin(), _texts.end(), View::comparePointers);		// sort for rendering order
	for (int i = 0; i < _texts.size(); i++)
	{
		auto pos = _texts[i]->getPosition();
		RenderText(*textShader,
			_texts[i]->getText(),
			_texts[i]->alignment,
			pos.x, pos.y,
			_texts[i]->scale,
			_texts[i]->color,
			_texts[i]->font);
	}
	// glDisable(GL_BLEND);

	profiler.StopTimer(5);
	cpuProfiler.StopTimer(5);
	*/
}

void RenderingSystem::RenderCompositionPass()
{
}


void RenderingSystem::DrawComponent(RenderComponent* component)
{
	glm::mat4 model = component->GetEntity()->transform.getWorldTransformation();

	for (int i = 0; i < component->renderables.size(); i++)
	{
		auto r = component->renderables[i];
	}
}

void RenderingSystem::RenderImage(Shader & s, ImageComponent * image)
{
	s.use();

	auto size = glm::vec2(image->width / 2, image->height / 2);		// size of box 
	auto transform = image->GetEntity()->transform.getWorldTransformation();	// transform

	// default 
	if (image->alignment == TextAlignment::Left)
		transform = glm::translate(transform, glm::vec3(size.x, 0, 0));
	else if (image->alignment == TextAlignment::Right)
		transform = glm::translate(transform, glm::vec3(-size.x, 0, 0));
	// else alignment is center, don't do anything.

	imageShader->setVec2("u_Size", size);
	imageShader->setMat4("u_Model", transform);
	imageShader->setMat4("u_Projection", uiProjection);
	imageShader->setVec3("u_Tint", image->tint);
	imageShader->setFloat("u_Opacity", image->opacity);

	glBindVertexArray(quadVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, image->imageId);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
}

// initializes a framebuffer object for deferred rendering
void RenderingSystem::InitializeFrameBuffers()
{
	// cleanup any previous FBO 
	if (FBO != 0)
	{
		glDeleteFramebuffers(3, new unsigned int[3] { FBO, ppWriteFBO, ppReadFBO });
	}

	// initialize frame buffer object 
	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	// position texture 
	glGenTextures(1, &posTex);
	glBindTexture(GL_TEXTURE_2D, posTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, posTex, 0);
	// normal texture 
	glGenTextures(1, &nrmTex);
	glBindTexture(GL_TEXTURE_2D, nrmTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, nrmTex, 0);
	// color (albedo) texture
	glGenTextures(1, &colTex);
	glBindTexture(GL_TEXTURE_2D, colTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, screenWidth, screenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, colTex, 0);
	// depth texture (create as texture so we can read from it) 
	glGenTextures(1, &dphTex);
	glBindTexture(GL_TEXTURE_2D, dphTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, screenWidth, screenHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, dphTex, 0);
	
	// Create our render targets. Order in array determines order for shader layout = #. 
	// unsigned int attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, attachments);	// defined in header file 

	// check if FBO is ok 
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "OPENGL: Failed to create FBO. GL ERROR: " << glGetError() << std::endl;
	}

	// initialize frame buffer object 
	glGenFramebuffers(1, &ppWriteFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, ppWriteFBO);
	// final (rendered image) writeable texture
	glGenTextures(1, &finWriteTex);
	glBindTexture(GL_TEXTURE_2D, finWriteTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, screenWidth, screenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, finWriteTex, 0);
	// create depth buffer 
	unsigned int RBO;
	glGenRenderbuffers(1, &RBO);
	glBindRenderbuffer(GL_RENDERBUFFER, RBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, screenWidth, screenHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, RBO);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// create render target
	glDrawBuffers(1, ppAttachments);
	// check if FBO is ok 
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "OPENGL: Failed to create Post-Process write FBO. GL ERROR: " << glGetError() << std::endl;
	}

	// initialize frame buffer object 
	glGenFramebuffers(1, &ppReadFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, ppReadFBO);
	// final (rendered image) readable texture
	glGenTextures(1, &finReadTex);
	glBindTexture(GL_TEXTURE_2D, finReadTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, screenWidth, screenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, finReadTex, 0);
	// create depth buffer 
	glGenRenderbuffers(1, &RBO);
	glBindRenderbuffer(GL_RENDERBUFFER, RBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, screenWidth, screenHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, RBO);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// create render target
	glDrawBuffers(1, ppAttachments);
	// check if FBO is ok 
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "OPENGL: Failed to create Post-Process read FBO. GL ERROR: " << glGetError() << std::endl;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderingSystem::InitializeScreenQuad()
{
	float quadVertices[] = {
		// positions        // texture Coords
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
	};

	glGenVertexArrays(1, &quadVAO);
	glBindVertexArray(quadVAO);

	unsigned int VBO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);	// remember this syntax only works here b/c we defined the array in the same scope 

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
}

void RenderingSystem::InitializeScreenCube()
{
	float skyboxVertices[] = {
		// positions          
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};

	glGenVertexArrays(1, &cubeVAO);
	glBindVertexArray(cubeVAO);

	unsigned int VBO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);	// remember this syntax only works here b/c we defined the array in the same scope 

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
}

void RenderingSystem::Update(float dt)
{
	cpuProfiler.StartTimer(0);

	// Cleanup 
	glClearDepth(1.0f);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// check for valid state 
	if (screenWidth == 0 || screenHeight == 0)
	{
		std::cerr << "ERROR: Screen dimensions are 0" << std::endl;
		return;
	}
	//else if (_cameras.size() == 0)
	//{
	//	std::cerr << "ERROR: There are no cameras" << std::endl;
	//	return;
	//}

	// use first main camera
	SetCamera(nullptr);
	auto _cams = ComponentMan<Camera>::Instance().All();
	for (auto& c : _cams)
	{
		if (c->isMain)
		{
			SetCamera(c);
			break;
		}
	}
	//for (int i = 0; i < _cameras.size(); i++)
	//{
	//	if (_cameras[i]->isMain)
	//	{
	//		SetCamera(_cameras[i]);
	//		break;
	//	}
	//}
	if (activeCamera == nullptr)
	{
		std::cerr << "ERROR: No camera has been set for rendering" << std::endl;
	}


	// Render 

	// prepare data 
	projection = activeCamera->GetProjectionMatrix();
	view = activeCamera->GetViewMatrix();

	cpuProfiler.StopTimer(0);

	// draw everything
	profiler.StartTimer(1);
	cpuProfiler.StartTimer(1);
	RenderGeometryPass();				// 1st pass (geometry)
	profiler.StopTimer(1);
	cpuProfiler.StopTimer(1);

	profiler.StartTimer(2);
	cpuProfiler.StartTimer(2);
	RenderShadowMapsPass();				// 2nd pass (shadow maps)
	profiler.StopTimer(2);
	cpuProfiler.StopTimer(2);

	profiler.StartTimer(3);
	cpuProfiler.StartTimer(3);
	RenderDirectionalLightingPass();	// 3rd pass (lighting)
	profiler.StopTimer(3);
	cpuProfiler.StopTimer(3);

	profiler.StartTimer(4);
	cpuProfiler.StartTimer(4);
	RenderPointLightingPass();			// 4rd pass (lighting)
	profiler.StopTimer(4);
	cpuProfiler.StopTimer(4);

	profiler.StartTimer(5);
	cpuProfiler.StartTimer(5);
	RenderSkyboxPass();					// 5th pass (skybox)
	profiler.StopTimer(5);
	cpuProfiler.StopTimer(5);

	profiler.StartTimer(6);
	cpuProfiler.StartTimer(6);
	RenderPostProcessPass();			// 6th pass (post processes)
	profiler.StopTimer(6);
	cpuProfiler.StopTimer(6);

	profiler.StartTimer(7);
	cpuProfiler.StartTimer(7);
	RenderUIImagesPass();				// 7th pass (UI)
	profiler.StopTimer(7);
	cpuProfiler.StopTimer(7);

	profiler.StartTimer(8);
	cpuProfiler.StartTimer(8);
	RenderUITextPass();					// 8th pass (UI)
	profiler.StopTimer(8);
	cpuProfiler.StopTimer(8);

	profiler.FrameFinish();
	cpuProfiler.FrameFinish();

	// render debug info (please note that this is pretty performance heavy ~2ms)
	std::stringstream ss;
	ss << std::fixed << std::setprecision(2)
		<< "Geometry Pass: " << profiler.GetDuration(1) / 1000000.0 << "ms\n"
		<< "Shadowmap Pass: " << profiler.GetDuration(2) / 1000000.0 << "ms\n"
		<< "Directional Lighting Pass: " << profiler.GetDuration(3) / 1000000.0 << "ms\n"
		<< "Point Lighting Pass: " << profiler.GetDuration(4) / 1000000.0 << "ms\n"
		<< "Skybox Pass: " << profiler.GetDuration(5) / 1000000.0 << "ms\n"
		<< "Postprocessing Pass: " << profiler.GetDuration(6) / 1000000.0 << "ms\n"
		<< "Image Pass: " << profiler.GetDuration(7) / 1000000.0 << "ms\n"
		<< "Text Pass: " << profiler.GetDuration(8) / 1000000.0 << "ms\n";

	TextRenderer::Instance().RenderText(ss.str(), 0.0f, screenHeight, glm::vec3(1,0,0));
}

void RenderingSystem::addPostProcess(const std::string name, std::unique_ptr<PostProcess> postProcess)
{
	_postProcesses[name] = std::move(postProcess);
}

void RenderingSystem::removePostProcess(const std::string name)
{
	_postProcesses.erase(name);
}

PostProcess* RenderingSystem::getPostProcess(const std::string name)
{
	auto pp = _postProcesses.find(name);
	return (pp != _postProcesses.end()) ? pp->second.get() : nullptr;
}

void RenderingSystem::setSkybox(unsigned int cubemapId)
{
	skyboxTexture = cubemapId;
}

//void RenderingSystem::onComponentCreated(std::type_index t, Component* c)
//{
//	if (t == std::type_index(typeid(RenderComponent)))
//	{
//		std::cout << "RenderingSystem adding render component" << std::endl;
//		_components.push_back(static_cast<RenderComponent*>(c));
//	}
//	else if (t == std::type_index(typeid(DirectionalLight)))
//	{
//		std::cout << "RenderingSystem adding light" << std::endl;
//		_dlights.push_back(static_cast<DirectionalLight*>(c));
//	}
//	else if (t == std::type_index(typeid(Camera)))
//	{
//		std::cout << "RenderingSystem adding camera" << std::endl;
//		_cameras.push_back(static_cast<Camera*>(c));
//	}
//}
//
//void RenderingSystem::onComponentDestroyed(std::type_index t, Component* c)
//{
//	if (t == std::type_index(typeid(RenderComponent)))
//	{
//		auto target = std::find(_components.begin(), _components.end(), c);
//		if (target != _components.end())
//			_components.erase(target);
//		else
//			std::cerr << "ERROR: RenderingSystem removing component but couldn't find it!" << std::endl;
//	}
//	else if (t == std::type_index(typeid(DirectionalLight)))
//	{
//		auto target = std::find(_dlights.begin(), _dlights.end(), c);
//		if (target != _dlights.end())
//			_dlights.erase(target);
//		else
//			std::cerr << "ERROR: RenderingSystem removing component but couldn't find it!" << std::endl;
//	}
//	else if (t == std::type_index(typeid(Camera)))
//	{
//		std::cout << "Camera destroyed" << std::endl;
//		auto target = std::find(_cameras.begin(), _cameras.end(), c);
//		if (target != _cameras.end())
//			_cameras.erase(target);
//		else
//			std::cerr << "ERROR: RenderingSystem removing component but couldn't find it!" << std::endl;
//	}
//}

/* Notes:

	// create depth buffer (you could also make this a texture - if you need to read from it)
	//unsigned int RBO;
	//glGenRenderbuffers(1, &RBO);
	//glBindRenderbuffer(GL_RENDERBUFFER, RBO);
	//glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCREEN_WIDTH, SCREEN_HEIGHT);
	//glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, RBO);

	// more texture parameters you might need
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

*/
