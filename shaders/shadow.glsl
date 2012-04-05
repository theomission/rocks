uniform mat4 mvp;

#ifdef VERTEX_P
in vec3 pos;
void main()
{
	gl_Position = mvp * vec4(pos, 1);
}
#endif

#ifdef FRAGMENT_P
void main()
{
}
#endif

