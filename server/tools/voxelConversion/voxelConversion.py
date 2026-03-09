##Converts something from .obj to 'VL32' format. THen the `VL32` format to something we can use in python
#by Daniel Chu
#
# VL32 format spec: https://eisenwave.github.io/voxel-compression-docs/file_formats/vl32.html
import matplotlib.pyplot as plt
import os
import sys, time, pdb
import argparse
#External custom modules
from coordinateConversionMath import * #
from fileConversionFunctions import *

#defines
DEFAULT_OBJ_FILEPATH = "tea.obj"
HELPSTRING = '''
.obj 2 .hvox tool by Daniel Chu

example usage: py voxelConversion.py tea.obj -dv
'''

#globals
args = None
parser = None

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

#arg actions

def actionDebugVisualize():
    '''
    visualize .obj in matplotlib as 3d plot
    '''
    objFilepath = argsGetFilepath()
    voxels = getVoxelsFromObj(objFilepath)#get the voxels from file
    print(f"debug {args.debug}")
    plotVoxels(voxels)#visualize the voxels for debug

def actionConvertToHeader():
    '''
    convert .obj file to a C header in quantized cylindrical coords
    '''
    objFilepath = argsGetFilepath()
    

# arg functions
def argsInit():
    '''
    Initialize parser and args. Parse all given args.
    '''
    global args, parser
    parser = argparse.ArgumentParser(description=HELPSTRING)
    parser.add_argument("objFilepath", nargs="?", help="Optional .obj file path")
    parser.add_argument("-d", "--debug", action="store_true")
    parser.add_argument("-dv", "--debugVisualize", action="store_true",  help="visualize .obj in matplotlib as 3d plot")
    parser.add_argument("-ch", "--convertHeader", action="store_true", help="convert .obj file to a C header in quantized cylindrical coords")
    args = parser.parse_args()

def argsParseAndRunFlags():
    '''
    Look through all the given arguements and run what is needed based on what is given
    '''
    global args, parser
    if args.debugVisualize:
        actionDebugVisualize()
    

def argsGetFilepath():
    '''
    get the filepath arg from args
    '''
    global args, parser
    objFilepath = DEFAULT_OBJ_FILEPATH
    if args.objFilepath:#set custom filepath if given
        givenPath = args.objFilepath
        pdb.set_trace()
        if not os.path.exists(givenPath):#does the file exist
            parser.error(f"file {givenPath} not found")
        objFilepath = givenPath
    return objFilepath


if __name__ == "__main__":
    #Handle Arg parse
    argsInit()
    argsParseAndRunFlags()