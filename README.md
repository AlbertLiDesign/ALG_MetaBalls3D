## Introduction

ALG_MarchingCubes_GPU is an isosurface extraction plug-in for Grasshopper using [Marching Cubes algorithm](https://en.wikipedia.org/wiki/Marching_cubes) on GPU. The development of this project is an important learning experience for me. I hope it can be a parallel programming reference case which can help GPU programming beginners and Grasshopper developers.

At present, its computational performance can still be optimized. I will continue to improve it in my future work.

![](https://albertlidesign.github.io/post-images/1586082938627.png)

## Algorithm

1. Compute bounding box from input points. 

2. Generation of a voxel-based grid inside the box. 

3. Classify voxels: Mark all the active voxels to get an active voxels array and calculate the number of vertices in each voxel by looking up vertices table. It is executed using one thread per voxel.

4. Exclusive sum scan:  Get the total number of active voxels and the total number of resulting vertices using exclusive scan algorithm. They are obtained by the sum of the last value of the exclusive scan and the last value of input array.

5. Compact voxels: This compacts the active voxels array to get rid of empty voxels. This allows us to execute Isosurface Extraction on only the active voxels.

6. Isosurface extraction: Calculate the position of the points in the active voxel and obtain all the result points by looking up triangle table.

7. Generate a mesh model from result points.

## How metaballs work [10]

Each metaball has a "sphere of influence". When you [merge](https://www.sidefx.com/docs/houdini/nodes/sop/merge.html) two metaballs and they extend into each other’s sphere of influence, they react in a way similar to drops of water: the surface tension works to form a smooth bridge between them. This is useful for making organic "blobby" shapes which meld into each other.

Metaballs can be thought of as spherical force fields whose surface is an implicit function defined at any point where the density of the force field equals a certain threshold. Because the density of the force field can be increased by the proximity of other metaball force fields, metaballs have the unique property that they change their shape to adapt and fuse with surrounding metaballs. This makes them very effective for modeling organic surfaces. For example, below we have a metaball. The surface of the metaball exists whenever the density of the metaball’s field reaches a certain threshold:

![](https://www.sidefx.com/docs/houdini/nodes/images/MetaballFields.jpg)

When two or more metaball force fields are combined, as in the illustration below, the resulting density of the force fields is added, and the surface extends to include that area where the force fields intersect and create density values with a value of one. 

![](https://www.sidefx.com/docs/houdini/nodes/images/MetaballFieldsa.jpg)

## Performance


## Reference

[1] Dyken, C., Ziegler, G., Theobalt, C., & Seidel, H. P. (2008, December). High‐speed marching cubes using histopyramids. In Computer Graphics Forum (Vol. 27, No. 8, pp. 2028-2039). Oxford, UK: Blackwell Publishing Ltd.

[2] Congote, J., Moreno, A., Barandiaran, I., Barandiaran, J., Posada, J., & Ruiz, O. (2010). Marching cubes in an unsigned distance field for surface reconstruction from unorganized point sets. In *Proceedings of the International Conference on Computer Graphics Theory and Applications, vol. 1, pp. 143,147, 2010*.

[3] Lorensen W E, Cline H E. Marching cubes: A high resolution 3D surface construction algorithm. ACM SIGGRAPH Computer Graphics. 1987;21(4)

[4] C. Dyken, G. Ziegler, C. Theobalt, and H.-P. Seidel. High-speed Marching Cubes using HistoPyramids. Computer Graphics Forum, 27(8):2028–2039, Dec. 2008.

[5] The algorithm and lookup tables by Paul Bourke httppaulbourke.netgeometrypolygonise：http://paulbourke.net/geometry/polygonise/

[6] Marching Cubes implementation using OpenCL and OpenGL：https://www.eriksmistad.no/marching-cubes-implementation-using-opencl-and-opengl/

[7] A sample extracts a geometric isosurface from a volume dataset using the marching cubes algorithm.: https://github.com/tpn/cuda-samples/tree/master/v10.2/2_Graphics/marchingCubes

[8] The introduction of marching cubes: http://www.cs.carleton.edu/cs_comps/0405/shape/marching_cubes.html

[9] The introduction of marching cubes: https://medium.com/zeg-ai/voxel-to-mesh-conversion-marching-cube-algorithm-43dbb0801359
