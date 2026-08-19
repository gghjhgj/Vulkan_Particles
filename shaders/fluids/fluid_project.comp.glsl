#version 450
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

struct FluidCell
{
    float vx;
    float vy;
    float pressure;
    float divergence;
    vec4 color; 
};

layout(std430, binding = 0) readonly buffer InBuffer  { FluidCell inCells[]; };
layout(std430, binding = 1) writeonly buffer OutBuffer { FluidCell outCells[]; };

layout(push_constant) uniform Push
{
    float mouseX;
    float mouseY;
    float prevMouseX;
    float prevMouseY;
    float dt;
    float splatRadius;
    float splatForce;
    float velocityDissipation;
    float densityDissipation;
    float vorticity;        
    uint simWidth;          
    uint simHeight;         
    uint windowWidth;       
    uint windowHeight;      
    uint isMouseDown;       
} push;

float getP(int x, int y)
{
    x = clamp(x, 0, int(push.simWidth - 1));
    y = clamp(y, 0, int(push.simHeight - 1));
    return inCells[y * int(push.simWidth) + x].pressure; 
}

void main()
{
    uint id = gl_GlobalInvocationID.x;
    if (id >= push.simWidth * push.simHeight) return;

    int x = int(id % push.simWidth);
    int y = int(id / push.simWidth);

    float pL = getP(x - 1, y);
    float pR = getP(x + 1, y);
    float pB = getP(x, y - 1);
    float pT = getP(x, y + 1);

    FluidCell cell = inCells[id];

    cell.vx -= 0.5 * (pR - pL);
    cell.vy -= 0.5 * (pT - pB);
    cell.divergence = 0.0;

    outCells[id] = cell;
}