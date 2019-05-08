#pragma once

#include "UI/View.h"
#include "../GL/glad.h"
#include <glm/glm.hpp>
#include <ft2build.h>
#include <freetype/freetype.h>
#include <string>
#include <map>
#include <vector>
#include <array>
#include "Shader.h"
#define FONT_MANAGER_DEFAULT_FONT "res/fonts/arial.ttf"
#define FONT_MANAGER_DEFAULT_FONTSIZE 48
#define FREETYPE_DYNAMIC_WIDTH 0

struct Rect
{
	float left;
	float top;
	float right;
	float bot;
};

// A freetype character 
struct Character {
	GLuint     TextureID;  // ID handle of the glyph texture
	glm::ivec2 Size;       // Size of glyph
	glm::ivec2 Bearing;    // Offset from baseline to left/top of glyph
	GLuint     Advance;    // Offset to advance to next glyph
};

// Font information used to render 
struct FontInfo
{
	std::string Path;
	GLuint LineHeight;
	std::map<GLchar, Character> Characters;
};

struct AtlasCharacter
{
	glm::vec2 Size;			// Size of glyph 
	glm::vec2 Bearing;		// Offset from baseline to left/top of glyph
	Rect TexCoords;			// Texture coordinates (left/top/right/bottom)
	GLfloat	  Advance;		// Offset to next glyph (in pixels)
};

struct AtlasFont
{
	std::string Path;		// Path this font was loaded from
	GLuint TexId;			// OpenGL texture ID 
	glm::vec2 Size;			// Size of texture
	GLuint LineHeight;		// Pixel height of each line
	std::map<GLchar, AtlasCharacter> Characters;
};

struct TextMesh
{
	glm::vec2 Size;
	std::vector<glm::vec4> Mesh;
	TextAlignment Alignment;	
	GLuint TexId;				// Texture used to generate this mesh 
};

class TextRenderer
{
// singleton pattern 
public:
	static TextRenderer& Instance()
	{
		static TextRenderer _instance;
		return _instance;
	}
	TextRenderer(TextRenderer const&) = delete;
	void operator=(TextRenderer const&) = delete;
private:
	TextRenderer();
	~TextRenderer();
	
// functions
public:
	void SetScreenSize(unsigned int width, unsigned int height);
	void LoadFont(std::string path);
	void UnloadFont(std::string path);

	// Convenience function to render text directly onto the screen.
	// Not recommended, use RenderText(TextMesh*) instead. 
	void RenderText(
		std::string text, 
		GLfloat x, 
		GLfloat y,
		glm::vec3 color			= glm::vec3(1.0f),
		GLfloat scale			= 1.0f,
		TextAlignment alignment = TextAlignment::Left,
		Shader *s				= nullptr, 
		std::string font		= FONT_MANAGER_DEFAULT_FONT);

	// Renders a TextMesh with given settings. 
	void RenderText(
		TextMesh* textMesh,
		glm::mat4 transformation = glm::mat4(1.0f),
		glm::vec3 color = glm::vec3(1.0f),
		Shader* s = nullptr);

	// Convenience function generate a text mesh.
	// The origin point is the top-left of the bounding box. 
	TextMesh* GenerateTextMesh(
		std::string text,
		std::string font = FONT_MANAGER_DEFAULT_FONT,
		TextAlignment alignment = TextAlignment::Left);

private:
	// Generates a TextMesh
	// The origin point is the top-left of the bounding box. 
	TextMesh* GenerateTextMesh(
		std::string text,
		AtlasFont& font,
		TextAlignment alignment = TextAlignment::Left);

	int width, height;
	FT_Library ft;
	std::map<std::string, AtlasFont> _fonts;
	unsigned int textVAO;	// quad to draw a single letter 
	unsigned int textVBO;	// quad buffer to draw a single letter
	Shader* textShader;		// default text shader 
	glm::mat4 uiProjection;
};

