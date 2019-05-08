#include "MainScene.h"

#include "Core/EntityManager.h"
#include "ComponentFactory.h"
#include "Rendering/CubeMesh.h"
#include "Rendering/RenderComponent.h"
#include "Rendering/Camera.h"
#include "Rendering/Lighting/DirectionalLight.h"
#include "Rendering/UI/ImageComponent.h"
#include "UI/ImageComponent.h"
#include <string>

void MainScene::InitScene() 
{
	auto cubeMesh = std::make_shared<CubeMesh>();

	auto defaultMaterial = std::make_shared<Material>();

	auto renderable = std::make_shared<Renderable>();
	renderable->mesh = cubeMesh;
	renderable->material = defaultMaterial;

	auto c_render = ComponentFactory::Create<RenderComponent>();
	c_render->addRenderable(renderable);

	auto e_test = EntityManager::Instance().Create();
	e_test->AddComponent(c_render);
	e_test->transform.setLocalPosition(glm::vec3(1.0f, 0, -5.0f));
	e_test->transform.setLocalRotation(glm::vec3(0.1f, 0.2f, 0.3f));

	auto c_cam = ComponentFactory::Create<Camera>();
	auto e_cam = EntityManager::Instance().Create();
	e_cam->AddComponent(c_cam);

	auto c_light = ComponentFactory::Create<DirectionalLight>();
	auto e_light = EntityManager::Instance().Create();
	e_light->AddComponent(c_light);
	e_light->transform.setLocalRotation(glm::vec3(0.42f, 0, 0));
	
	//auto c_img = ComponentFactory::Create<ImageComponent>();
	//c_img->loadImage("res/textures/araragi_karen.png");
	//auto e_img = EntityManager::Instance().Create();
	//e_img->AddComponent(c_img);
	// root.AddChild(e_img);

	auto c_uimg = ComponentFactory::Create<ImgComponent>(std::string("res/textures/araragi_karen.png"), 1.0f, 1.0f, 0.0f, 0.0f);
	c_uimg->size = { 0.5f, 0.5f };
	auto e_uimg = EntityManager::Instance().Create();
	e_uimg->AddComponent(c_uimg);
	e_uimg->transform.scale(2.0f);

	auto c_uimg2 = ComponentFactory::Create<ImgComponent>(std::string("res/textures/bob_ross.png"), 1.0f, 1.0f, 0.0f, 0.0f);
	c_uimg2->hAnchor = HorizontalAnchor::ANCHOR_RIGHT;
	c_uimg2->vAnchor = VerticalAnchor::ANCHOR_BOTTOM;
	c_uimg2->size = { 0.5f, 0.5f };
	auto e_uimg2 = EntityManager::Instance().Create();
	e_uimg2->AddComponent(c_uimg2);

	e_uimg->AddChild(e_uimg2);

	root.AddChild(e_test);
	root.AddChild(e_cam);
	root.AddChild(e_light);
	root.AddChild(e_uimg);
}

void MainScene::Update(const float delta) 
{

}

void MainScene::CleanUp() 
{

}