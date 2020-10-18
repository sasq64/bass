


%{

function my_test(arr)
    for i=1,#arr do
        print(arr[i])
    end
    return 0
end

function check_img(img, cols)
    for i=1,#img.colors do
        print(i, img.colors[i])
    end

    print(img.width)
end

}%


    a = [1,2,3]
    b = a[2]
    !print b
    c = my_test(a[0:2])

    min(5, 3)
    min($, 1)
    min(8, $)
    !print $


    load_png("../data/test.png")
    save_png("org.png", $)
    remap_image($, [$ffffff, $000000, $080000, $0000e0, $c0c000])
    save_png("remapped.png", $)
    change_bpp($, 4)
    save_png("bpp.png", $)
    layout_image($, 8, 8)
    save_png("layed.png", $)
    !section "main",0
    !fill $.pixels


    load_png("../data/mountain.png")
    remap_image($, [$0000, $555555, $aaaaaa, $ffffff])
    remapped = remap_image($, [$0000, $ffffff])
    x = to_monochrome(remapped.pixels)
    remapped2 = change_bpp(remapped, 1)
    save_png("mountain2.png", remapped2)

    !assert x == remapped2.pixels




