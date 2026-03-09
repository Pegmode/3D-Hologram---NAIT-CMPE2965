import numpy
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
    
    :param cartesianList: list of cartesian coordinates in the form [(x1,y1,z1),...(xn,yn,zn)]
    '''
    return [carteseian2Cylindrical(v) for v in cartesianList]

def CylindricalList2Quantized(cylindricalList):
    pass
