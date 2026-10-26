<h2>Overview</h2>
This project is a simple gravity simulation meant to show off some Vulkan capaibilties. <br>
![Simulation showing two oribiting bodies](/assets/simulation.gif)

The project is split up into several core files: app.cpp, which runs all the logic between simulation.cpp and draw3d.cpp, and vulkan_renderer.cpp, which handle all the vulkan logic, with draw3d.cpp acting as an abstraction on top of it. vulkan_renderer.cpp was designed to be as loosely coupled and generic as possible, so it can hopefully be reused in my future projects.
<h3>Shaders</h3>
Shaders are written using Slang and compiled to SPIR-V. When a shader is used, descriptors, uniforms, and push constants are all generated via reflection using SPIRV reflect. <br>
<h3>Textures</h3>
Textures are drawn using descriptor indexing, a Vulkan 1.2 extension. A sizeless texture array is declared in the shader:
<code>
[[vk::binding(1, 2)]] <br>
uniform Texture2D textures[];
</code> <br>
And is sampled based on the object's texture ID. <br>
<code>
textures[NonUniformResourceIndex(vertIn.texId)]
</code> <br>
This avoids issues with a fixed size texture array (which requires all textures to be the same size), while also avoiding mipmap issues when using a texture atlas.
<h3> Graphics Pipelines </h3>
While draw3D has pre-made pipelines, it also supports custom graphics pipelines. A builder pattern is used to create a custom pipeline, and even build a new pipeline from an existing one.





<h2>Compilation Instructions</h2>
To compile without tracy: <br>
cmake -G Ninja -S . -B build <br>
cmake --build build <br>
Output file is in the build folder, run ./build/game <br>
<br>


Compile with tracy: <br>
cmake -G Ninja -S . -B build \
-DTRACY_ENABLE=ON \
-DTRACY_CALLSTACK=8 \
-DCMAKE_BUILD_TYPE=RelWithDebInfo

Tracy profiler is included in the CMake. Run these commands to build: <br>
cmake -S build/_deps/tracy-src/profiler -B build/tracy-profiler -DCMAKE_BUILD_TYPE=Release <br>
cmake --build build/tracy-profiler --parallel <br>
./build/tracy-profiler/tracy-profiler


