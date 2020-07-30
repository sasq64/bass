
    png = load_png("../data/face.png")

    !section "data",$8000

data:
    !fill png.pixels
data_end:

    !assert data_end - data == 320*200