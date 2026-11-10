import { crc32, deflateSync } from 'node:zlib';

// Colors of the bars of the test card, left to right
const BAR_COLORS = [
    [255, 255, 255],
    [255, 221, 0],
    [0, 214, 214],
    [0, 200, 60],
    [214, 0, 214],
    [230, 40, 40],
    [30, 60, 230],
    [27, 34, 45]
];

// Orange of the moving marker (the accent of the app)
const MARKER_COLOR = [247, 147, 26];

// PNG file signature
const PNG_SIGNATURE = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);

// Build one PNG chunk: length, type, data, and the CRC of type and data
const buildChunk = (type, data) => {
    const typeAndData = Buffer.concat([Buffer.from(type, 'ascii'), data]);
    const length = Buffer.alloc(4);
    length.writeUInt32BE(data.length);
    const checksum = Buffer.alloc(4);
    checksum.writeUInt32BE(crc32(typeAndData));
    return Buffer.concat([length, typeAndData, checksum]);
};

// Pick the color of one pixel: bars on top, a gray ramp below, and a marker that moves with the seconds (so two snapshots differ, like a real camera)
const getPixelColor = ({ x, y, width, height, markerX, brightness }) => {
    const isMarker = Math.abs(x - markerX) < width / 40 && y > height * 0.66 && y < height * 0.9;
    let color;
    if (isMarker) {
        color = MARKER_COLOR;
    } else if (y < height * 0.66) {
        color = BAR_COLORS[Math.floor((x / width) * BAR_COLORS.length)];
    } else {
        const gray = Math.round((x / width) * 255);
        color = [gray, gray, gray];
    }

    // Return the color, brighter with the flash
    return color.map(channel => Math.min(255, Math.round(channel * brightness)));
};

// Build the test card the mock camera sends instead of a photo, as a PNG (MCP clients only take raster images)
export const buildTestCardPng = ({ width, height, useFlash = false, takenAt = new Date() }) => {
    const rowLength = 1 + width * 3; // A filter byte, then RGB
    const pixels = Buffer.alloc(rowLength * height);
    const markerX = ((takenAt.getSeconds() + takenAt.getMilliseconds() / 1000) / 60) * width;
    const brightness = useFlash ? 1 : 0.8;

    // Draw the rows (each starts with filter 0: no filter)
    for (let y = 0; y < height; y += 1) {
        for (let x = 0; x < width; x += 1) {
            const [red, green, blue] = getPixelColor({ x, y, width, height, markerX, brightness });
            const offset = y * rowLength + 1 + x * 3;
            pixels[offset] = red;
            pixels[offset + 1] = green;
            pixels[offset + 2] = blue;
        }
    }

    // Describe the image: size, 8 bits per channel, RGB, no interlace
    const header = Buffer.alloc(13);
    header.writeUInt32BE(width, 0);
    header.writeUInt32BE(height, 4);
    header.writeUInt8(8, 8);
    header.writeUInt8(2, 9);

    // Return the PNG file
    return Buffer.concat([
        PNG_SIGNATURE,
        buildChunk('IHDR', header),
        buildChunk('IDAT', deflateSync(pixels)),
        buildChunk('IEND', Buffer.alloc(0))
    ]);
};
