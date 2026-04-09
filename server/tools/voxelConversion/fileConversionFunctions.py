##Converts something from .obj to 'VL32' format. THen the `VL32` format to something we can use in python
#by Daniel Chu
#
# VL32 format spec: https://eisenwave.github.io/voxel-compression-docs/file_formats/vl32.html

import struct
import matplotlib.pyplot as plt
import os, subprocess
import requests,sys
from pathlib import Path
import pdb


DEFAULT_OBJ_FILEPATH = "tea.obj"
DEFAULT_VL32_FILEPATH = "tea.vl32"
DEFAULT_OBJ2VOX_FILEPATH = "obj2voxel-v1.3.4.exe"
DEFAULT_VOXEL_RESOLUTION = 16 #LED resolution 16x32
ORIGIN_DISTANCE = DEFAULT_VOXEL_RESOLUTION // 2
OBJ2VOXEL_EXE_URL = "https://github.com/eisenwave/obj2voxel/releases/download/v1.3.4/obj2voxel-v1.3.4.exe"

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
                voxels.append((x - ORIGIN_DISTANCE, y - ORIGIN_DISTANCE, z))
    return voxels

def externalConvertObj2Vl32(objFilepath, vl32Filepath):
    '''
    Using external obj2voxel tool, convert an obj to vl32 format
    
    :param objFilepath: Filepath to .obj
    '''
    subprocessArgs = [DEFAULT_OBJ2VOX_FILEPATH, objFilepath, vl32Filepath, "-r",  str(DEFAULT_VOXEL_RESOLUTION)]
    subprocess.run(subprocessArgs)

def downloadConversionProgram():
    '''
    Check if obj2voxel executable exists. If it does not exist, download it.
    '''
    if os.path.exists(DEFAULT_OBJ2VOX_FILEPATH):#if the file is already there do nothing\
        print(f"{DEFAULT_OBJ2VOX_FILEPATH} found!")
        sys.stdout.flush()
        return
    print(f"{DEFAULT_OBJ2VOX_FILEPATH} not found downloading from github....")
    sys.stdout.flush()
    try:
        response = requests.get(OBJ2VOXEL_EXE_URL, stream=True, timeout=30)
    except:
        print(f"ERROR: failed attempting to download {DEFAULT_OBJ2VOX_FILEPATH} from {OBJ2VOXEL_EXE_URL}!")
        sys.stdout.flush()
    try:
        with open(DEFAULT_OBJ2VOX_FILEPATH, "wb") as f:
            for chunk in response.iter_content(chunk_size=8192):
                if chunk:
                    f.write(chunk)
    except:
        print(f"ERROR: could not write {DEFAULT_OBJ2VOX_FILEPATH} file!")
    print(f"downloaded {DEFAULT_OBJ2VOX_FILEPATH}!")
    sys.stdout.flush()

def getVoxelsFromObj(objFilepath):
    '''
    Convert a .obj file to voxels. Returns the voxel list.
    
    :param objFilepath: filepath to OBJ file
    '''
    ##Check if filepath containts obj extension
    objPathObject = Path(objFilepath)
    if objPathObject.suffix.lower() != ".obj":
        raise Exception(f"ERROR: file {objFilepath} does not have .obj extension")
    vl32Filepath = str(objPathObject.with_suffix(".vl32"))
    downloadConversionProgram()
    externalConvertObj2Vl32(objFilepath, vl32Filepath)
    voxels = readVL32(vl32Filepath)
    sys.stdout.flush()
    return voxels