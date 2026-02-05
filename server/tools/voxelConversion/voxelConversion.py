##Converts something from the `VL332` format to something we can use in python
#by Daniel Chu
#
# VL32 format spec: https://eisenwave.github.io/voxel-compression-docs/file_formats/vl32.html
import struct
import matplotlib.pyplot as plt

DEBUG_FILEPATH = "tea.vl32"

def readVL32(path):
    '''
    Read a vl32 file and return a 3D list of coordinates (x, y, z)
    
    :param path: Path to .vl32 file
    '''
    voxels = []
    with open(path, "rb") as f:
        while True:
            chunk = f.read(16) #format spec shows each voxel is 16 bytes 
            if len(chunk) < 16:#check if there is tailing data at the end of the file
                break
            x, y, z, a, _, _, _ = struct.unpack(">iiiBBBB", chunk)# unpack the struct given in the format spec, ignore color data, keep alpha for pixel is present
            if a != 0: #dont save the voxel if its see through / not present
                voxels.append((x, y, z))
    return voxels

def plotVoxels(voxels):
    '''
    Plot converted voxels using mat plot lib 3D projection plot
    
    :param voxels: List of voxels (x, y, z)
    '''
    xList, yList, zList = zip(*voxels)#unpack coords for matplot lib
    ##setup minimum matplotlib 3D plot
    fig = plt.figure()
    ax = fig.add_subplot(111, projection="3d")
    ax.scatter(xList, yList, zList, marker='s', s=20)
    plt.show()

if __name__ == "__main__":
    voxels = readVL32(DEBUG_FILEPATH)
    plotVoxels(voxels)