#pragma once

// Simple OpenGL FrameBuffer object that encompasses the entire screen with one texture. 
class FrameBuffer
{
public:
	FrameBuffer();
	FrameBuffer(unsigned int width, unsigned int height);
	~FrameBuffer();

	// Bind framebuffer's to read from. Binds texture(s) for reading.
	// Returns the number of textures bound.
	unsigned int Read(unsigned int textureSlot = 0);
	
	// Bind framebuffer to write to. 
	void Draw(bool clear = true);
	
	// Returns the main texture id. 
	unsigned int GetTexId();

	unsigned int GetFboId() { return _fbo; }

private:
	virtual void GenerateFrameBuffer(unsigned int width, unsigned int height);
	unsigned int _tex;
	unsigned int _fbo;
};
