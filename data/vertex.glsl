#ifdef GL_ES
precision mediump float;
precision mediump int;
#endif

attribute vec4 al_pos;
attribute vec4 al_color;
attribute vec2 al_texcoord;

uniform int scaleFactor;

uniform mat4 al_projview_matrix;
varying vec4 varying_color;
varying vec2 varying_texcoord;
        varying vec2 one;
        varying float mod_factor;
        
        #define size vec2(480*scaleFactor, 360*scaleFactor)
        #define rubyInputSize size
        #define rubyOutputSize size
        #define rubyTextureSize size
        
void main()
{
   varying_color = al_color;
   varying_texcoord = al_texcoord;
   gl_Position = al_projview_matrix * al_pos;
                   // The size of one texel, in texture-coordinates.
               one = 1.0 / rubyTextureSize;
 
                // Resulting X pixel-coordinate of the pixel we're drawing.
               mod_factor = al_texcoord.x * rubyTextureSize.x * rubyOutputSize.x / rubyInputSize.x;
               //mod_factor = al_texcoord.x * 320.0 * 3200.0 / 320.0;
}
/*
uniform vec2 rubyInputSize;
        uniform vec2 rubyOutputSize;
        uniform vec2 rubyTextureSize;
        
        
 uniform mat4 al_projview_matrix;
attribute vec4 al_pos;
attribute vec4 al_color;
attribute vec2 al_texcoord;
// Define some calculations that will be used in fragment shader.
        //varying vec2 texCoord;
        varying vec2 one;
        varying float mod_factor;
 
        void main()
        {
                // Do the standard vertex processing.
                gl_Position = al_projview_matrix * al_pos;
    varying_color = al_color;
   varying_texcoord = al_texcoord;
                // Precalculate a bunch of useful values we'll need in the fragment
                // shader.
 
                // Texture coords.
                //texCoord = al_texcoord.xy;
 
                // The size of one texel, in texture-coordinates.
                one = 1.0;// / rubyTextureSize;
 
                // Resulting X pixel-coordinate of the pixel we're drawing.
                mod_factor = al_texcoord.x; // * rubyTextureSize.x * rubyOutputSize.x / rubyInputSize.x;
        }
        */
