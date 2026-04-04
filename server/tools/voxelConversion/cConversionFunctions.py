#functions for data handling in C


def packFlattenedVoxelsToBytes(flatBits):
    '''
    Convert a list of C booleans (int 0,1) into a list of bytes
    
    :param flatBits: List of Quantized voxels

    return a list of packed bytes
    '''
    packedBytes = []
    for i in range(0, len(flatBits), 8):
        chunk = flatBits[i: i + 8]#select a chunk of values to fill a byte
        while len(chunk) < 8:#fill the end with zeros if no voxels on the end
            chunk.append(0)
        #build the byte
        byteValue = 0
        for bitIndex, bit in enumerate(chunk):
            byteValue |= (bit & 1) << (7 - bitIndex) #fill the byte bitwise
        packedBytes.append(byteValue)
    return packedBytes


def convertVoxelsToFlatList(quantizedVoxels, sliceCount, boardWidth, boardHeight):
    '''
    Convert a quantized 3D list of voxels to a 1D List of C booleans in the format i = [nth slice][h][nth horizontal led]
    
    :param quantizedVoxels: List of Quantized voxels
    :param sliceCount: number of slices
    :param boardWidth: board width in LEDs
    :param boardHeight: board height in LEDs

    return a list of flatened C booleans 
    '''
    voxels = [[[0 for _ in range(boardWidth)] for _ in range(boardHeight)]for _ in range(sliceCount)]
    
    for sliceN, h, r in quantizedVoxels:#build the bit array
        voxels[sliceN][h][r] = 1
    #turn our 3d LIST into a 1D list
    flat = []
    for sliceN in range(sliceCount):
        for h in range(boardHeight):
            for r in range(boardWidth):
                flat.append(voxels[sliceN][h][r])
    return flat