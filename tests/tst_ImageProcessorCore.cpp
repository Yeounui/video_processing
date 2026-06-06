#include <QtTest>
#include "ImageProcessorCore.h"
#include <algorithm>

static ImageBuffer makeImage(int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    ImageBuffer img;
    img.width = w;
    img.height = h;
    img.channels = 3;
    img.data.resize(w * h * 3);
    for (int i = 0; i < w * h; ++i) {
        img.data[i * 3] = r;
        img.data[i * 3 + 1] = g;
        img.data[i * 3 + 2] = b;
    }
    return img;
}

static ImageBuffer makeRgbaImage() {
    ImageBuffer img;
    img.width = 2;
    img.height = 1;
    img.channels = 4;
    img.data = {
        100, 50, 25, 0,
        10, 20, 30, 128
    };
    return img;
}

static uint8_t getPx(const ImageBuffer& img, int x, int y, int c) {
    return img.data[(y * img.width + x) * img.channels + c];
}

class TestImageProcessorCore : public QObject {
    Q_OBJECT
private slots:
    void testBrightness();
    void testBrightnessClamp();
    void testMultiply();
    void testGammaIdentity();
    void testFixedThreshold();
    void testBitwiseAnd();
    void testOpenCvAlgorithmsAcceptEmptyImage();
    void testOpenCvPointPreservesAlphaAndClearsTransparentRgb();
    void testFlipH();
    void testOpenCvFlipPreservesAlphaAndClearsTransparentRgb();
    void testRotate90ExpandsCanvas();
    void testRotate180KeepsCanvasSize();
    void testRotateArbitraryAngleExpandsCanvas();
    void testRotatePreservesTransparentBackground();
    void testRotateThenBrightnessKeepsTransparentBackground();
    void testRotateUsesVisibleAlphaBounds();
    void testRotateRepeatedDoesNotShrink();
    void testRotateOutputIsAlways4Channel();
    void testBlur3x3Uniform();
    void testGrayscaleAverage();
    void testGrayscaleLuminosity();
    void testGrayscaleLightness();
    void testOpenCvGrayscalePreservesAlphaAndClearsTransparentRgb();
    void testContrastStretch();
    void testSpecsCount();
    void testSpecsIds();
    void testAverageThreshold();
    void testEmboss();
    void testHorizontalEdgeSobelGy();
    void testVerticalEdgeSobelGx();
    void testHistogramStretch();
    void testEndpointDetectionOutOfBoundsIsBackground();
    void testBlur3x3BoundaryClampToEdge();
};

void TestImageProcessorCore::testBrightness() {
    auto src = makeImage(1, 1, 100, 100, 100);
    ImageBuffer dst;
    EffectParams p;
    p["delta"] = 30;
    ImageProcessorCore::apply(src, dst, 1, p);
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)130);
    QCOMPARE(getPx(dst, 0, 0, 1), (uint8_t)130);
    QCOMPARE(getPx(dst, 0, 0, 2), (uint8_t)130);
}

void TestImageProcessorCore::testBrightnessClamp() {
    auto src = makeImage(1, 1, 240, 240, 240);
    ImageBuffer dst;
    EffectParams p;
    p["delta"] = 30;
    ImageProcessorCore::apply(src, dst, 1, p);
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)255);
}

void TestImageProcessorCore::testMultiply() {
    auto src = makeImage(1, 1, 100, 50, 200);
    ImageBuffer dst;
    EffectParams p;
    p["factor"] = 2.0;
    ImageProcessorCore::apply(src, dst, 2, p);
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)200);
    QCOMPARE(getPx(dst, 0, 0, 1), (uint8_t)100);
    QCOMPARE(getPx(dst, 0, 0, 2), (uint8_t)255);
}

void TestImageProcessorCore::testGammaIdentity() {
    auto src = makeImage(1, 1, 128, 64, 200);
    ImageBuffer dst;
    EffectParams p;
    p["gamma"] = 1.0;
    ImageProcessorCore::apply(src, dst, 3, p);
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)128);
    QCOMPARE(getPx(dst, 0, 0, 1), (uint8_t)64);
    QCOMPARE(getPx(dst, 0, 0, 2), (uint8_t)200);
}

void TestImageProcessorCore::testFixedThreshold() {
    auto src = makeImage(1, 1, 200, 200, 200);
    ImageBuffer dst;
    EffectParams p;
    p["threshold"] = 127;
    ImageProcessorCore::apply(src, dst, 4, p);
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)255);
    src = makeImage(1, 1, 50, 50, 50);
    ImageProcessorCore::apply(src, dst, 4, p);
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)0);
}

void TestImageProcessorCore::testBitwiseAnd() {
    auto src = makeImage(1, 1, 0xFF, 0xAA, 0x55);
    ImageBuffer dst;
    EffectParams p;
    p["mask"] = 0xF0;
    ImageProcessorCore::apply(src, dst, 6, p);
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)0xF0);
    QCOMPARE(getPx(dst, 0, 0, 1), (uint8_t)0xA0);
    QCOMPARE(getPx(dst, 0, 0, 2), (uint8_t)0x50);
}

void TestImageProcessorCore::testOpenCvAlgorithmsAcceptEmptyImage() {
    ImageBuffer src;
    ImageBuffer dst;
    QVERIFY(ImageProcessorCore::apply(src, dst, 1, EffectParams{}));
    QCOMPARE(dst.width, 0);
    QCOMPARE(dst.height, 0);
    QCOMPARE(dst.channels, 3);
    QVERIFY(dst.data.empty());
}

void TestImageProcessorCore::testOpenCvPointPreservesAlphaAndClearsTransparentRgb() {
    auto src = makeRgbaImage();
    ImageBuffer dst;
    EffectParams p;
    p["delta"] = 20;
    ImageProcessorCore::apply(src, dst, 1, p);

    QCOMPARE(dst.channels, 4);
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)0);
    QCOMPARE(getPx(dst, 0, 0, 1), (uint8_t)0);
    QCOMPARE(getPx(dst, 0, 0, 2), (uint8_t)0);
    QCOMPARE(getPx(dst, 0, 0, 3), (uint8_t)0);
    QCOMPARE(getPx(dst, 1, 0, 0), (uint8_t)30);
    QCOMPARE(getPx(dst, 1, 0, 1), (uint8_t)40);
    QCOMPARE(getPx(dst, 1, 0, 2), (uint8_t)50);
    QCOMPARE(getPx(dst, 1, 0, 3), (uint8_t)128);
}

void TestImageProcessorCore::testFlipH() {
    ImageBuffer src;
    src.width = 2;
    src.height = 1;
    src.channels = 3;
    src.data.resize(6);
    src.data = {255, 0, 0, 0, 0, 255};
    ImageBuffer dst;
    EffectParams p;
    p["mode"] = "H";
    ImageProcessorCore::apply(src, dst, 7, p);
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)0);
    QCOMPARE(getPx(dst, 0, 0, 2), (uint8_t)255);
    QCOMPARE(getPx(dst, 1, 0, 0), (uint8_t)255);
}

void TestImageProcessorCore::testOpenCvFlipPreservesAlphaAndClearsTransparentRgb() {
    auto src = makeRgbaImage();
    ImageBuffer dst;
    EffectParams p;
    p["mode"] = "H";
    ImageProcessorCore::apply(src, dst, 7, p);

    QCOMPARE(dst.channels, 4);
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)10);
    QCOMPARE(getPx(dst, 0, 0, 1), (uint8_t)20);
    QCOMPARE(getPx(dst, 0, 0, 2), (uint8_t)30);
    QCOMPARE(getPx(dst, 0, 0, 3), (uint8_t)128);
    QCOMPARE(getPx(dst, 1, 0, 0), (uint8_t)0);
    QCOMPARE(getPx(dst, 1, 0, 1), (uint8_t)0);
    QCOMPARE(getPx(dst, 1, 0, 2), (uint8_t)0);
    QCOMPARE(getPx(dst, 1, 0, 3), (uint8_t)0);
}

void TestImageProcessorCore::testRotate90ExpandsCanvas() {
    ImageBuffer src;
    src.width = 2;
    src.height = 3;
    src.channels = 3;
    src.data = {
        10, 0, 0, 20, 0, 0,
        30, 0, 0, 40, 0, 0,
        50, 0, 0, 60, 0, 0,
    };

    ImageBuffer dst;
    EffectParams p;
    p["degree"] = 90.0;
    ImageProcessorCore::apply(src, dst, 8, p);

    QCOMPARE(dst.width, 3);
    QCOMPARE(dst.height, 2);
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)50);
    QCOMPARE(getPx(dst, 1, 0, 0), (uint8_t)30);
    QCOMPARE(getPx(dst, 2, 0, 0), (uint8_t)10);
    QCOMPARE(getPx(dst, 0, 1, 0), (uint8_t)60);
    QCOMPARE(getPx(dst, 1, 1, 0), (uint8_t)40);
    QCOMPARE(getPx(dst, 2, 1, 0), (uint8_t)20);
}

void TestImageProcessorCore::testRotate180KeepsCanvasSize() {
    auto src = makeImage(7, 5, 128, 128, 128);
    ImageBuffer dst;
    EffectParams p;
    p["degree"] = 180.0;
    ImageProcessorCore::apply(src, dst, 8, p);

    QCOMPARE(dst.width, src.width);
    QCOMPARE(dst.height, src.height);
}

void TestImageProcessorCore::testRotateArbitraryAngleExpandsCanvas() {
    auto src = makeImage(10, 4, 128, 128, 128);
    ImageBuffer dst;
    EffectParams p;
    p["degree"] = 30.0;
    ImageProcessorCore::apply(src, dst, 8, p);

    QVERIFY(dst.width > src.width);
    QVERIFY(dst.height > src.height);
    QCOMPARE((int)dst.data.size(), dst.width * dst.height * dst.channels);
}

void TestImageProcessorCore::testRotatePreservesTransparentBackground() {
    ImageBuffer src;
    src.width = 5;
    src.height = 5;
    src.channels = 4;
    src.data.assign(src.width * src.height * src.channels, 0);
    for (int y = 1; y <= 3; ++y) {
        for (int x = 1; x <= 3; ++x) {
            src.data[(y * src.width + x) * src.channels + 0] = 255;
            src.data[(y * src.width + x) * src.channels + 3] = 255;
        }
    }

    ImageBuffer dst;
    EffectParams p;
    p["degree"] = 45.0;
    ImageProcessorCore::apply(src, dst, 8, p);

    QCOMPARE(dst.channels, 4);
    QVERIFY(std::any_of(dst.data.begin(), dst.data.end(), [](uint8_t v) { return v == 255; }));
    QCOMPARE(getPx(dst, 0, 0, 3), (uint8_t)0);
}

void TestImageProcessorCore::testRotateThenBrightnessKeepsTransparentBackground() {
    auto src = makeImage(20, 20, 120, 80, 40);

    ImageBuffer rotated;
    EffectParams rotateParams;
    rotateParams["degree"] = 45.0;
    ImageProcessorCore::apply(src, rotated, 8, rotateParams);

    ImageBuffer brightened;
    EffectParams brightnessParams;
    brightnessParams["delta"] = 60;
    ImageProcessorCore::apply(rotated, brightened, 1, brightnessParams);

    QCOMPARE(brightened.channels, 4);
    QCOMPARE(getPx(brightened, 0, 0, 0), (uint8_t)0);
    QCOMPARE(getPx(brightened, 0, 0, 1), (uint8_t)0);
    QCOMPARE(getPx(brightened, 0, 0, 2), (uint8_t)0);
    QCOMPARE(getPx(brightened, 0, 0, 3), (uint8_t)0);
}

void TestImageProcessorCore::testRotateUsesVisibleAlphaBounds() {
    ImageBuffer src;
    src.width = 9;
    src.height = 9;
    src.channels = 4;
    src.data.assign(src.width * src.height * src.channels, 0);
    for (int y = 3; y <= 5; ++y) {
        for (int x = 3; x <= 5; ++x) {
            src.data[(y * src.width + x) * src.channels + 0] = 200;
            src.data[(y * src.width + x) * src.channels + 3] = 255;
        }
    }

    ImageBuffer dst;
    EffectParams p;
    p["degree"] = 45.0;
    ImageProcessorCore::apply(src, dst, 8, p);

    QVERIFY(dst.width < src.width);
    QVERIFY(dst.height < src.height);
    QCOMPARE(dst.channels, 4);
}

void TestImageProcessorCore::testRotateRepeatedDoesNotShrink() {
    // Rotating a solid RGB image 30° three times should not progressively
    // grow the canvas (which would make the image appear smaller each time).
    // After the first rotation, output is RGBA; subsequent rotations use the
    // alpha-based content bounds so the canvas stays stable.
    auto src = makeImage(100, 100, 200, 100, 50);
    EffectParams p;
    p["degree"] = 30.0;

    ImageBuffer r1;
    ImageProcessorCore::apply(src, r1, 8, p);
    QCOMPARE(r1.channels, 4);
    int w1 = r1.width, h1 = r1.height;

    ImageBuffer r2;
    ImageProcessorCore::apply(r1, r2, 8, p);
    int w2 = r2.width, h2 = r2.height;

    ImageBuffer r3;
    ImageProcessorCore::apply(r2, r3, 8, p);
    int w3 = r3.width, h3 = r3.height;

    // Canvas must not grow cumulatively: each subsequent canvas should be
    // no larger than 10% above the first rotation's output.
    QVERIFY2(w2 <= w1 * 110 / 100, "Canvas grew more than 10% on second rotation");
    QVERIFY2(h2 <= h1 * 110 / 100, "Canvas grew more than 10% on second rotation");
    QVERIFY2(w3 <= w1 * 110 / 100, "Canvas grew more than 10% on third rotation");
    QVERIFY2(h3 <= h1 * 110 / 100, "Canvas grew more than 10% on third rotation");
}

void TestImageProcessorCore::testRotateOutputIsAlways4Channel() {
    // Rotate on RGB input must output RGBA so the viewport shader can
    // composite transparent corners over the background colour.
    auto src = makeImage(20, 20, 128, 128, 128);
    QCOMPARE(src.channels, 3);

    ImageBuffer dst;
    EffectParams p;
    p["degree"] = 45.0;
    ImageProcessorCore::apply(src, dst, 8, p);

    QCOMPARE(dst.channels, 4);
    // Corner pixel (0,0) should be fully transparent (outside source bounds).
    QCOMPARE(getPx(dst, 0, 0, 3), (uint8_t)0);
    // Some pixel near the centre should be opaque.
    int cx = dst.width / 2, cy = dst.height / 2;
    QCOMPARE(getPx(dst, cx, cy, 3), (uint8_t)255);
}

void TestImageProcessorCore::testBlur3x3Uniform() {
    auto src = makeImage(5, 5, 128, 128, 128);
    ImageBuffer dst;
    ImageProcessorCore::apply(src, dst, 11, EffectParams{});
    QCOMPARE(getPx(dst, 2, 2, 0), (uint8_t)128);
}

void TestImageProcessorCore::testGrayscaleAverage() {
    auto src = makeImage(1, 1, 60, 90, 150);
    ImageBuffer dst;
    ImageProcessorCore::apply(src, dst, 26, EffectParams{});
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)100);
    QCOMPARE(getPx(dst, 0, 0, 1), (uint8_t)100);
    QCOMPARE(getPx(dst, 0, 0, 2), (uint8_t)100);
}

void TestImageProcessorCore::testGrayscaleLuminosity() {
    auto src = makeImage(1, 1, 255, 0, 0);
    ImageBuffer dst;
    ImageProcessorCore::apply(src, dst, 27, EffectParams{});
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)76);
}

void TestImageProcessorCore::testGrayscaleLightness() {
    auto src = makeImage(1, 1, 100, 200, 50);
    ImageBuffer dst;
    ImageProcessorCore::apply(src, dst, 28, EffectParams{});
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)125);
}

void TestImageProcessorCore::testOpenCvGrayscalePreservesAlphaAndClearsTransparentRgb() {
    auto src = makeRgbaImage();
    ImageBuffer dst;
    ImageProcessorCore::apply(src, dst, 26, EffectParams{});

    QCOMPARE(dst.channels, 4);
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)0);
    QCOMPARE(getPx(dst, 0, 0, 1), (uint8_t)0);
    QCOMPARE(getPx(dst, 0, 0, 2), (uint8_t)0);
    QCOMPARE(getPx(dst, 0, 0, 3), (uint8_t)0);
    QCOMPARE(getPx(dst, 1, 0, 0), (uint8_t)20);
    QCOMPARE(getPx(dst, 1, 0, 1), (uint8_t)20);
    QCOMPARE(getPx(dst, 1, 0, 2), (uint8_t)20);
    QCOMPARE(getPx(dst, 1, 0, 3), (uint8_t)128);
}

void TestImageProcessorCore::testContrastStretch() {
    auto src = makeImage(1, 1, 100, 100, 100);
    ImageBuffer dst;
    EffectParams p;
    p["stat_min"] = 50;
    p["stat_max"] = 150;
    ImageProcessorCore::apply(src, dst, 10, p);
    QCOMPARE(getPx(dst, 0, 0, 0), (uint8_t)128);
}

void TestImageProcessorCore::testSpecsCount() {
    QCOMPARE((int)ImageProcessorCore::specs().size(), 28);
}

void TestImageProcessorCore::testSpecsIds() {
    const auto& s = ImageProcessorCore::specs();
    for (int i = 0; i < 28; ++i)
        QCOMPARE(s[i].id, i + 1);
}

void TestImageProcessorCore::testAverageThreshold() {
    // stat_average=128: lum(200,200,200)=200 >= 128 → white
    auto src = makeImage(1,1, 200,200,200);
    ImageBuffer dst;
    EffectParams p; p["stat_average"] = 128.0;
    ImageProcessorCore::apply(src, dst, 5, p);
    QCOMPARE(getPx(dst,0,0,0), (uint8_t)255);
    // lum(50,50,50)=50 < 128 → black
    src = makeImage(1,1, 50,50,50);
    ImageProcessorCore::apply(src, dst, 5, p);
    QCOMPARE(getPx(dst,0,0,0), (uint8_t)0);
}

void TestImageProcessorCore::testEmboss() {
    // 3x3 image, center pixel: contribution = px(2,2,c) - px(0,0,c) + 128
    // Set pixel (2,2) = 200, pixel (0,0) = 50, center = (1,1)
    // emboss center = 200 - 50 + 128 = 278 → clamped to 255
    ImageBuffer src; src.width=3; src.height=3; src.channels=3;
    src.data.assign(27, 100); // fill with 100
    // (0,0) = 50 for all channels
    src.data[0]=50; src.data[1]=50; src.data[2]=50;
    // (2,2) = 200 for all channels
    src.data[24]=200; src.data[25]=200; src.data[26]=200;
    ImageBuffer dst;
    ImageProcessorCore::apply(src, dst, 9, EffectParams{});
    // center pixel (1,1): px(2,2,c)=200, px(0,0,c)=50 → 200-50+128=278 → 255
    QCOMPARE(getPx(dst,1,1,0), (uint8_t)255);
}

void TestImageProcessorCore::testHorizontalEdgeSobelGy() {
    // Sobel Gy: horizontal gradient (detects horizontal edges / vertical changes)
    // Uniform 5x5 image → Gy=0 → edge=0
    auto src = makeImage(5,5, 128,128,128);
    ImageBuffer dst;
    EffectParams p; p["scale"] = 1.0;
    ImageProcessorCore::apply(src, dst, 19, p);
    // Interior pixel should have zero edge on uniform image
    QCOMPARE(getPx(dst,2,2,0), (uint8_t)0);
    // Output is grayscale: R=G=B
    QCOMPARE(getPx(dst,2,2,0), getPx(dst,2,2,1));
    QCOMPARE(getPx(dst,2,2,1), getPx(dst,2,2,2));
}

void TestImageProcessorCore::testVerticalEdgeSobelGx() {
    // Sobel Gx: vertical gradient (detects vertical edges / horizontal changes)
    // Uniform 5x5 image → Gx=0 → edge=0
    auto src = makeImage(5,5, 128,128,128);
    ImageBuffer dst;
    EffectParams p; p["scale"] = 1.0;
    ImageProcessorCore::apply(src, dst, 20, p);
    QCOMPARE(getPx(dst,2,2,0), (uint8_t)0);
    // Output is grayscale
    QCOMPARE(getPx(dst,2,2,0), getPx(dst,2,2,1));
}

void TestImageProcessorCore::testHistogramStretch() {
    // src[c]=100, stat_hmin=50, stat_hmax=150 → (100-50)*255/100 = 127.5 → 127 (truncated)
    auto src = makeImage(1,1, 100,100,100);
    ImageBuffer dst;
    EffectParams p; p["stat_hmin"]=50; p["stat_hmax"]=150;
    ImageProcessorCore::apply(src, dst, 23, p);
    QCOMPARE(getPx(dst,0,0,0), (uint8_t)127);
}

void TestImageProcessorCore::testEndpointDetectionOutOfBoundsIsBackground() {
    // 1x1 image: single foreground pixel with no neighbors → but out-of-image = background
    // 8 neighbors all out of bounds → count=0 → NOT an endpoint (need exactly 1)
    // → output should be black (0)
    auto src = makeImage(1,1, 255,255,255); // foreground (lum=255 >= 127)
    ImageBuffer dst;
    EffectParams p; p["threshold"]=127;
    ImageProcessorCore::apply(src, dst, 24, p);
    QCOMPARE(getPx(dst,0,0,0), (uint8_t)0); // count==0, not 1, so not endpoint
}

void TestImageProcessorCore::testBlur3x3BoundaryClampToEdge() {
    // 1x1 image: all 9 neighbor samples should clamp to the single pixel
    // → average of 9 same values = same value
    auto src = makeImage(1,1, 80,120,200);
    ImageBuffer dst;
    ImageProcessorCore::apply(src, dst, 11, EffectParams{});
    QCOMPARE(getPx(dst,0,0,0), (uint8_t)80);
    QCOMPARE(getPx(dst,0,0,1), (uint8_t)120);
    QCOMPARE(getPx(dst,0,0,2), (uint8_t)200);
}

QTEST_MAIN(TestImageProcessorCore)
#include "tst_ImageProcessorCore.moc"
