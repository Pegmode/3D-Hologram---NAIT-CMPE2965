#lookup table generator. See coordinate conversion algorithm document
#Author: Daniel Chu
import numpy

LED_DISTANCE = 10#

##functions
def carteseian2Cylindrical(cartesianCoordinate):

    #theta = arctan2(y,x)
    #r = sqrt(x^2+y^2)
    #h = z
    x, y, z = cartesianCoordinate
    r = numpy.sqrt(x^2+y^2)
    theta = numpy.arctan2(y,x)#angle in radians
    pass
    

##main
if __name__ == "__main__":
    pass