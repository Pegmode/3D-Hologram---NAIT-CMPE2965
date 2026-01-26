#lookup table generator. See coordinate conversion algorithm document
#Author: Daniel Chu
import numpy
import matplotlib.pyplot as plt
LED_DISTANCE = 10#

##functions
##
##Math functions
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
    r = numpy.sqrt(x^2+y^2)
    theta = numpy.arctan2(y,x)#angle in radians
    return theta, r, z
    
##LUT functions
def debugConvertSystem2D():
    '''
    Debug test for visual verification of cartesian => cylindrical conversion
    '''
    cartesianSize = 30#
    coordList = []
    for y in range(cartesianSize):
        for x in range(cartesianSize):
            theta, r, _ = carteseian2Cylindrical((x,y,0))
            coordList.append((theta, r))
    return coordList

##main
if __name__ == "__main__":
    figure = plt.figure()
    ax = figure.add_subplot(111, projection="polar")
    points = debugConvertSystem2D()
    thetas, rs = zip(*points)
    ax.scatter(thetas, rs)
    plt.show()

    pass