# Coordinate Translation Algorithm 
The hologram consists of a virtical circuit board rotating about a central axis. Therefore this requires translating a standard $(x,y,z)$ cartesian coordinate system to a $(\theta, r, h)$ cylindrical system.
## Math Description
### Definitions
Given a 3D cartesian point $(x,y,z)$ and a cylindrical point $(\theta, r, h)$ on a cylinder where:
- $\theta$ is the angle of rotation within the inclusive range $[0^\circ\text{ - }180^\circ]$
- $r$ is the radius from the center of rotation
- $h$ is the height on the cylinder
### 2D General Algorithm
For the 2D algorithm, we only consider a single slice from the top down perspective. This matches a cartesian to polar mapping.<br><br>
Coordinate conversion $(x,y) \rightarrow (\theta, r)$:<br>
<br>$$x = r \cos(\theta)$$
<br>$$y = r \sin(\theta)$$
<br><br>Solving for $(\theta, r)$:
<br>$$\boxed{r = \sqrt{x^2+y^2}}$$
<br>$$\boxed{\theta = \arctan 2(y,x)}$$ where $\theta$ is in radians
### 3D General Algorithm
To adapt the algorithm to 3D, we add a $x \rightarrow h$ mapping.<br><br>
$$\boxed{h=z}$$

## Lookup Algorithm
The cartesian and cylindrical coordinates are both represented as 3D arrays of 1-bit values. Because the cylindrical coordinates are a representation of slices on a rotating object, the angular coordinate $\theta$ is descrete and represented as $(\theta_n, r, h)$.<br><br>