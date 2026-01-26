## Simulate hologram board rotation
# by Daniel Chu for Capstone project
import matplotlib.pyplot as plt, numpy
from PIL import Image, ImageDraw
from io import BytesIO
BOARD_DIMENSIONS = (16*10, 32*10)##xy dimensions in LEDs
LED_COUNTS = (16, 32)# led "coords" on board
LED_DISTANCES = (4,4)# xy distance between LEDs
BOARD_RADIUS = BOARD_DIMENSIONS[0] / 2##radius is half the display board
LED_RADIUS = LED_DISTANCES[0] * LED_COUNTS[0]##radius of the LEDs
SLICECOUNT = 32##how many slices eg: angles 0-180
DISPLAY_PADDING = 5
PLOT_TITLE = "Visual Hologram Plot Test (Top Down View)"
VOXEL_DIMENSIONS = (10, 10)

#### MATH EQN
def circleEquationVector(angle):
    '''
    Calculate the direction vector for a circle, given an angle
    
    :param angle: the angle
    '''
    return numpy.cos(angle), numpy.sin(angle) #point (x,y) = cos(theta), sin(theta)

def getPointDistances(radius, pointCount):
    '''
    Gets evenly spaced numbers over the board. Contains the distances from the origin/center
    
    :param radius: radius
    :param pointCount: number of points along the line
    '''
    return numpy.linspace(-radius, radius, pointCount)

def getPoints(angle):
    pointCount = LED_COUNTS[0]
    distances = getPointDistances(LED_RADIUS, pointCount)
    vectors = circleEquationVector(angle)
    return distances * vectors[0], distances * vectors[1]

### plot
def addInfoToImage(image, angle, frame):
    '''
    Add the info as text to image
    
    :param image: Description
    :param angle: Description
    :param frame: Description
    '''
    draw = ImageDraw.Draw(image)
    #text fields
    text = f"""
Frame: {frame}
Angle: {angle}
LED Count Dimensions:{LED_COUNTS[0]}x{LED_COUNTS[1]}
Voxel Dimensions:{ VOXEL_DIMENSIONS[0]}x{ VOXEL_DIMENSIONS[0]}
Slice Count: {SLICECOUNT}
LED Distances: {LED_DISTANCES}
Board Diameter: {BOARD_RADIUS * 2}
LED Diameter: {LED_RADIUS * 2}
    """
    draw.multiline_text((0,0), text, fill = "black")

if __name__ == "__main__":
    angles = range(0, 180, 180 // SLICECOUNT)
    plotImages = []
    #calculate plot axis ticks for voxels
    axisTickList = list(range(0, LED_RADIUS * 2 + (LED_RADIUS * 2) // VOXEL_DIMENSIONS[0] - 1,  (LED_RADIUS * 2) // VOXEL_DIMENSIONS[0]))
    avg = sum(axisTickList) / len(axisTickList)
    shiftedAxisTickList = [x - avg for x in axisTickList]

    i = 0
    for angleDegree in angles:
        angle = numpy.deg2rad(angleDegree)
        points = getPoints(angle)#grouping of X, Y coords
        # print(points)
        fig, ax = plt.subplots()
        #Add circle
        circle = plt.Circle((0, 0), BOARD_RADIUS, fill=False)
        ax.add_artist(circle)
        #draw line
        ax.scatter(points[0], points[1])
        #format
        plt.title(PLOT_TITLE)
        ax.set_aspect('equal')
        plt.xticks(shiftedAxisTickList)
        plt.yticks(shiftedAxisTickList)
        ax.set_xticklabels([])
        ax.set_yticklabels([])
        ax.grid(True, color='gray', linestyle='--', linewidth=0.5)
        ax.set_xlim(-BOARD_RADIUS - DISPLAY_PADDING, BOARD_RADIUS + DISPLAY_PADDING)
        ax.set_ylim(-BOARD_RADIUS - DISPLAY_PADDING, BOARD_RADIUS + DISPLAY_PADDING)
        ax.axhline(0)
        ax.axvline(0)
        fig.canvas.draw()
        #write image to buffer and save as PIL image
        buffer = BytesIO()
        plt.savefig(buffer, format="png")
        buffer.seek(0)
        image = Image.open(buffer).convert("RGB")
        image.load()
        addInfoToImage(image, angleDegree, i)
        plotImages.append(image)
        buffer.close()
        plt.close()
        i += 1
    

    ##write to gif
    plotImages[0].save("simulation.gif", save_all = True, append_images = plotImages[1:], duration = 500, loop = 0)