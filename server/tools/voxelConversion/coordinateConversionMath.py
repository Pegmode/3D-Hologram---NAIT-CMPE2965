import numpy, pdb
#Coordinate conversion math functions

def carteseian2Cylindrical(cartesianCoordinate):
    '''
    Take a cartesian coordinate (x,y,z) and convert it to a cylindrical coordinate (theta, r, h)
    where theta is in radians 
    
    :param cartesianCoordinate: 3D touple of x, y, z cartesian coordinate
    '''
    #theta = arctan2(y,x)
    #r = sqrt(x^2+y^2)
    #h = z
    x, y, z = cartesianCoordinate
    r = numpy.sqrt(x*x+y*y)
    theta = numpy.arctan2(y,x)#angle in radians
    return theta, r, z
    
def cylindrical2Cartesian(clyindricalCoordinate):
    '''
    Convert a clyindrical coordinate into a cartesian coordingate,
    returns (x,y,z)
    
    :param clyindricalCoordinate: Clyindrical coordinate in the form (theta, r, h)
    '''
    theta, r, h = clyindricalCoordinate
    x = r * numpy.cos(theta)
    y = r * numpy.sin(theta)
    z = h
    return x, y, z
    
def cartesianList2Cylindrical(cartesianList):
    '''
    Convert a list of cartesian coordinates [(x1,y1,z1),...(xn,yn,zn)] and convert 
    to cylindrical coordinates [...(thetan, rn, hn)]
    
    returns a list comprehension
    
    :param cartesianList: list of cartesian coordinates in the form [(x1,y1,z1),...(xn,yn,zn)]
    '''
    return [carteseian2Cylindrical(v) for v in cartesianList]

def cylindricalList2Quantized(cylindricalList, sliceCount, width, height):
    '''
    Convert a list of  cylindrical coordinates [...(theta, r, h)] to normalized and quantized values
    that fit on the hologram display board [(nth slice, h, horizontal position on board r)]
    
    returns the quantized list
    
    :param cylindricalList: List of cylindrical coordinates [...(theta, r, h)] 
    :param sliceCount: the number of slices we want to make from angles [0, 180)
    :param width: The width of the display in LEDs
    :param height: The height of the display in LEDs
    '''
    ##Split
    #Case1 => R Theta1 -> [0,180] => map normally
    #Case2 => R Theta2 -> (180,360) => map from center of board, r = RTheta2 + (1/2) boardWidth
    #Check for bad inputs....
    if width % 2 != 0:
        raise ValueError("width must be an even number so that the board can be split in 2 halves")
    if sliceCount <= 0:
        raise ValueError("sliceCount must be > 0")
    if not cylindricalList:
        return []
    #perform the quantization...
    quantizedCoordinates = []
    halfWidth = width // 2
    anglePerSlice = 180.0 / sliceCount
    #Do some math to make sure our model fits in the circle...
    maxRadius = max(r for _, r, _ in cylindricalList)
    if maxRadius == 0:
        maxRadius = 1.0
    #Perform converison for every voxel
    for thetaRad, r, h in cylindricalList:
        thetaDegree = (numpy.degrees(thetaRad) + 360.0) % 360.0
        thetaFold = thetaDegree % 180.0 #I need to fold the angles into the range [0,180) degrees
        #deal with board indices...
        sliceIndex = int(thetaFold // anglePerSlice)
        if sliceIndex >= sliceCount:
            sliceIndex = sliceCount - 1 #clamp upper range
        #rQuantized = int(numpy.floor(r))
        rQuantized = int(numpy.floor((r / maxRadius) * (halfWidth - 1)))#try to scale the radius into one half of the board. Use the ratio of r relative to the max r to deal with corners from the cartesian conversion.
        if thetaDegree >= 180.0:#If the voxel is in the "other right half" of the board normalize it to our board
            rNormalized = rQuantized + halfWidth
        else:
            rNormalized =  7 -  rQuantized#min + max - value = mirrored about midway between min,max
        hNormalized = int(numpy.floor(h))
        #cleanup values for C array so that the microcontroller doesn't explode
        if rNormalized < 0 or rNormalized >= width:
            continue
        if hNormalized < 0 or hNormalized >= height:
            continue
        quantizedCoordinates.append((sliceIndex, hNormalized, rNormalized))#Our board array will be of the form [slice][height][horizontal position]
    return quantizedCoordinates 



def getQuantized3dIndex(theta, r, h, rCount, hCount):
    '''
    Get the array index of a quantized 3d voxel
    
    :param theta: Quantized angle n
    :param r: Quantized radius n
    :param h: Quantized height n
    :param rCount: size of r
    :param hCount: size of h
    '''
    return theta * (rCount * hCount) + h * rCount + r