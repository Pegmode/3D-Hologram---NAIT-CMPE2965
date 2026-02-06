##Converts something from .obj to 'VL32' format. THen the `VL32` format to something we can use in python
#by Daniel Chu
#
# VL32 format spec: https://eisenwave.github.io/voxel-compression-docs/file_formats/vl32.html
import struct
import matplotlib.pyplot as plt
import os, subprocess
import sys, time, pdb
import requests
DEFAULT_OBJ_FILEPATH = "tea.obj"
DEFAULT_VL32_FILEPATH = "tea.vl32"
DEFAULT_OBJ2VOX_FILEPATH = "obj2voxel-v1.3.4.exe"
DEFAULT_VOXEL_RESOLUTION = 60 #
OBJ2VOXEL_EXE_URL = "https://github.com/eisenwave/obj2voxel/releases/download/v1.3.4/obj2voxel-v1.3.4.exe"

HELPSTRING = '''
.obj 2 .hvox tool by daniel chu

stand alone use:
voxelConversion.py [optional .obj filepath]
'''

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


def externalConvertObj2Vl32(objFilepath, vl32Filepath):
    '''
    Using external obj2voxel tool, convert an obj to vl32 format
    
    :param objFilepath: Filepath to .obj
    '''
    pass
    ##TEMP DEBG
    #BUild args
    subprocessArgs = [DEFAULT_OBJ2VOX_FILEPATH, objFilepath, vl32Filepath, "-r",  str(DEFAULT_VOXEL_RESOLUTION)]
    subprocess.run(subprocessArgs)

def downloadConversionProgram():
    '''
    Check if obj2voxel executable exists. If it does not exist, download it.
    '''
    if os.path.exists(DEFAULT_OBJ2VOX_FILEPATH):#if the file is already there do nothing\
        print(f"{DEFAULT_OBJ2VOX_FILEPATH} found!")
        return
    print(f"{DEFAULT_OBJ2VOX_FILEPATH} not found downloading from github....")
    try:
        response = requests.get(OBJ2VOXEL_EXE_URL, stream=True, timeout=30)
    except:
        print(f"ERROR: failed attempting to download {DEFAULT_OBJ2VOX_FILEPATH} from {OBJ2VOXEL_EXE_URL}!")
    try:
        with open(DEFAULT_OBJ2VOX_FILEPATH, "wb") as f:
            for chunk in response.iter_content(chunk_size=8192):
                if chunk:
                    f.write(chunk)
    except:
        print(f"ERROR: could not write {DEFAULT_OBJ2VOX_FILEPATH} file!")
    print(f"downloaded {DEFAULT_OBJ2VOX_FILEPATH}!")
    
 

if __name__ == "__main__":
    objFilepath = DEFAULT_OBJ_FILEPATH
    if len(sys.argv) > 1:#set custom filepath if given
        if not os.path.exists:#does the file exist
            print(f"ERROR: file {sys.argv[1]} not found\n\n{HELPSTRING}")
            sys.exit(0)
        objFilepath = sys.argv[1]
    ##Check if containts obj extension
    if objFilepath.split(".")[-1] != "obj":
        print(f"ERROR: file {objFilepath} does not have .obj extension\n\n{HELPSTRING}")
        sys.exit(0)
    vl32Filepath = f"{''.join(objFilepath.split('.')[:-1])}.vl32"
    if vl32Filepath[0] == "\\":#TODO running in os can have bad stuff here, figure out a way to filter this better
        vl32Filepath = "." + vl32Filepath
    downloadConversionProgram()
    externalConvertObj2Vl32(objFilepath, vl32Filepath)
    voxels = readVL32(vl32Filepath)
    plotVoxels(voxels)