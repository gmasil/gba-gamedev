#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint8_t *read_bmp_data(const char *filename, unsigned int *width, unsigned int *height) {
    // Data read from the header of the BMP file
    uint8_t header[54];
    uint16_t bpp;
    uint16_t bpp_prefix;
    uint32_t dataPos;
    uint32_t imageSize;
    // Actual RGB data
    uint8_t *data;

    // Open the file
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("File %s could not be opened.\n", filename);
        return NULL;
    }

    /*
     * read the first 54 bytes to read at least the smallest header possible
     *
     * BITMAPCOREHEADER (14B)
     * BITMAPCOREHEADER2 (64B)
     * BITMAPINFOHEADER (40B) -> smallest header 14B + 40B = 54B
     * BITMAPV2INFOHEADER (52B)
     * BITMAPV4HEADER (108B)
     * BITMAPV5HEADER (124B)
     */

    // If less than 54 bytes are read, problem
    if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
        printf("Not a correct BMP file\n");
        fclose(file);
        return NULL;
    }
    // A BMP files always begins with "BM"
    if (header[0] != 'B' || header[1] != 'M') {
        printf("Not a correct BMP file\n");
        fclose(file);
        return NULL;
    }

    // Read the information about the image
    bpp        = read_le16(&header[0x1C]);
    bpp_prefix = read_le16(&header[0x1E]);
    dataPos    = read_le32(&header[0x0A]);
    imageSize  = read_le32(&header[0x22]);
    *width     = read_le32(&header[0x12]);
    *height    = read_le32(&header[0x16]);

    // Make sure this is a 24bpp file
    if (bpp_prefix != 0) {
        printf("Not a correct BMP file\n");
        fclose(file);
        return NULL;
    }
    if (bpp != 24) {
        printf("Not a correct BMP file\n");
        fclose(file);
        return NULL;
    }

    // Some BMP files are misformatted, guess missing information
    if (imageSize == 0) {
        imageSize = *width * *height * 3; // 3 : one byte for each Red, Green and Blue component
    }
    if (dataPos == 0) {
        dataPos = 54; // The BMP header is done that way
    }

    // Create a buffer
    data = malloc(imageSize);
    if (!data) {
        printf("Could not allocate %u bytes for BMP data\n", imageSize);
        fclose(file);
        return NULL;
    }

    // skip the rest of the header to where the rgb data starts
    if (fseek(file, (long)dataPos, SEEK_SET) != 0) {
        printf("Could not seek to BMP pixel data\n");
        free(data);
        fclose(file);
        return NULL;
    }

    // Read the actual data from the file into the buffer
    if (fread(data, 1, imageSize, file) != imageSize) {
        printf("Could not read BMP pixel data\n");
        free(data);
        fclose(file);
        return NULL;
    }

    // Everything is in memory now, the file can be closed.
    fclose(file);

    return data;
}

static int convert_bmp(const char *input_filename, const char *output_filename) {
    uint8_t *bmp_888_data;
    uint8_t *bmp_888_start;
    unsigned int width;
    unsigned int height;

    unsigned int i;
    unsigned int x;
    unsigned int y;

    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint16_t pixel;

    FILE *output_file;

    bmp_888_data = read_bmp_data(input_filename, &width, &height);
    if (!bmp_888_data) {
        return -1;
    }
    bmp_888_start = bmp_888_data;

    if (width != 240 || height != 160) {
        printf("The bitmap '%s' must be 240x160, but is %ux%u\n", input_filename, width, height);
        free(bmp_888_data);
        return -1;
    }

    remove(output_filename);

    output_file = fopen(output_filename, "w");
    if (!output_file) {
        printf("Cannot open '%s'\n", output_filename);
        free(bmp_888_data);
        return -2;
    }

    fprintf(output_file, "static unsigned short data[] = { ");

    i = 0;
    for (y = 0; y < height; y++) {
        unsigned int src_y = height - 1 - y;
        for (x = 0; x < width; x++) {
            unsigned int src_index = (src_y * width + x) * 3;
            r                      = bmp_888_start[src_index + 0];
            g                      = bmp_888_start[src_index + 1];
            b                      = bmp_888_start[src_index + 2];
            pixel                  = (uint16_t)(((uint16_t)(r >> 3) << 10) | ((uint16_t)(g >> 3) << 5) | (uint16_t)(b >> 3));
            if (i != 0) {
                fprintf(output_file, ", ");
            }
            fprintf(output_file, "%u", (unsigned int)pixel);
            i++;
        }
    }

    fprintf(output_file, " };\n");
    fclose(output_file);
    free(bmp_888_start);

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s input.bmp output.c\n", argv[0]);
        return -1;
    }
    return convert_bmp(argv[1], argv[2]);
}
