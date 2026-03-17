#include <stdio.h>
#include <stdlib.h>

unsigned char *read_bmp_data(char *filename, unsigned int *width, unsigned int *height) {
    // Data read from the header of the BMP file
    unsigned char header[54];
    unsigned int bpp;
    unsigned int bpp_prefix;
    unsigned int dataPos;
    unsigned int imageSize;
    // Actual RGB data
    unsigned char *data;

    // Open the file
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("File %s could not be opened.\n", filename);
        return 0;
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
    if (fread(header, 1, 54, file) != 54) {
        printf("Not a correct BMP file\n");
        fclose(file);
        return 0;
    }
    // A BMP files always begins with "BM"
    if (header[0] != 'B' || header[1] != 'M') {
        printf("Not a correct BMP file\n");
        fclose(file);
        return 0;
    }

    // Read the information about the image
    bpp        = *(int *)&(header[0x1C]);
    bpp_prefix = *(int *)&(header[0x1E]);
    dataPos    = *(int *)&(header[0x0A]);
    imageSize  = *(int *)&(header[0x22]);
    *width     = *(int *)&(header[0x12]);
    *height    = *(int *)&(header[0x16]);

    // Make sure this is a 24bpp file
    if (bpp_prefix != 0) {
        printf("Not a correct BMP file\n");
        fclose(file);
        return 0;
    }
    if (bpp != 24) {
        printf("Not a correct BMP file\n");
        fclose(file);
        return 0;
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

    // skip the rest of the header to where the rgb data starts
    fseek(file, dataPos, SEEK_SET);

    // Read the actual data from the file into the buffer
    fread(data, 1, imageSize, file);

    // Everything is in memory now, the file can be closed.
    fclose(file);

    return data;
}

int convert_bmp(char *input_filename, char *output_filename) {
    unsigned char *bmp_888_data;
    unsigned int width;
    unsigned int height;

    unsigned int i;

    unsigned char r;
    unsigned char g;
    unsigned char b;

    unsigned short *rgb_555_data;

    FILE *output_file;

    bmp_888_data = read_bmp_data(input_filename, &width, &height);
    
    if(width != 240 || height != 160){
        printf("The bitmap '%s' must be 240x160, but is %ux%u\n", input_filename, width, height);
        free(bmp_888_data);
        return -1;
    }

    rgb_555_data = malloc(240*160*2);

    remove(output_filename);

    output_file = fopen(output_filename, "w");
    if(!output_file) {
        printf("Cannot open '%s'\n", output_filename);
        return -2;
    }

    fprintf(output_file, "static unsigned short data[] = { ");

    for(i = 0; i < 240 * 160; i++) {
        r = *bmp_888_data++;
        g = *bmp_888_data++;
        b = *bmp_888_data++;
        *rgb_555_data = ((r >> 3) << 10) + ((g >> 3) << 5) + (b >> 3);
        if(i != 0) {
            fprintf(output_file, ", ");
        }
        fprintf(output_file, "%d", *rgb_555_data);
        rgb_555_data++;
    }

    fprintf(output_file, " };\n");

    return 0;
}

int main(int argc, char **argv) {
    if(argc != 3) {
        printf("Usage: %s input.bmp output.c\n", argv[0]);
        return -1;
    }
    return convert_bmp(argv[1], argv[2]);
}
