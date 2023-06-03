#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <spng.h>

/* TODO:
 *  - Take in a palette as a command line option.
 *  - Output an updated palette as a new .h file
 *  - Option for tall-sprite-mode tile ordering
 *  - Detect and remove duplicate tiles
 */

typedef struct pixel_s {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} pixel_t;

static uint8_t palette [16] = {};
static uint32_t palette_size = 0;

/*
 * Convert from colour to palette index.
 * New colours are added to the palette as needed.
 */
static uint8_t colour_to_index (uint8_t colour)
{
    /* First, check if the colour is already in the palette */
    for (uint32_t i = 0; i < palette_size; i++)
    {
        if (palette [i] == colour)
        {
            return i;
        }
    }

    /* If not, add it */
    palette [palette_size] = colour;

    return palette_size++;
}


/*
 * Process a single 8x8 tile.
 */
static void process_tile (pixel_t *buffer, uint32_t stride)
{
    printf ("Pattern:\n");

    for (uint32_t y = 0; y < 8; y++)
    {
        for (uint32_t x = 0; x < 8; x++)
        {
            uint8_t index = 0;
            pixel_t p = buffer [x + y * stride];

            /* If the pixel is non-transparrent, calculate its colour index */
            if (p.a != 0)
            {
                uint8_t colour = ((p.r & 0xc0) >> 6)
                               | ((p.g & 0xc0) >> 4)
                               | ((p.b & 0xc0) >> 2);

                index = colour_to_index (colour);;
            }

            printf ("%02x ", index);
        }
        printf ("\n");
    }
    printf ("\n");
}


/*
 * Process an image made up of 8x8 tiles.
 */
static int process_image (pixel_t *buffer, uint32_t width, uint32_t height)
{
    /* Sanity check */
    if ((width % 8 != 0) || (height % 8 != 0))
    {
        fprintf (stderr, "Error: Invalid resoulution %ux%u\n", width, height);
        return -1;
    }

    for (uint32_t row = 0; row < height; row += 8)
    {
        for (uint32_t col = 0; col < width; col += 8)
        {
            process_tile (&buffer [row * width + col], width);
        }
    }

    return 0;
}


int main (int argc, char **argv)
{
    int rc = EXIT_SUCCESS;

    if (argc < 2)
    {
        fprintf (stderr, "Usage: %s <tiles.png>\n", argv [0]);
        return EXIT_FAILURE;
    }

    for (uint32_t i = 1; i < argc; i++)
    {
        spng_ctx *spng_context = spng_ctx_new (0);

        /* Try to open the file */
        FILE *png_file = fopen (argv [i], "r");
        if (png_file == NULL)
        {
            fprintf (stderr, "Error: Unable to open %s.\n", argv [i]);
            rc = EXIT_FAILURE;
            break;
        }

        /* Get the file size */
        uint32_t png_size = 0;
        fseek (png_file, 0, SEEK_END);
        png_size = ftell (png_file);
        rewind (png_file);

        /* Allocate memory for the .png file */
        uint8_t *png_buffer = calloc (png_size, 1);
        if (png_buffer == NULL)
        {
            fprintf (stderr, "Error: Failed to allocate memory for %s.\n", argv [i]);
            rc = EXIT_FAILURE;
            break;
        }

        /* Read and close the file */
        uint32_t bytes_read = 0;
        while (bytes_read < png_size)
        {
            bytes_read += fread (png_buffer + bytes_read, 1, png_size - bytes_read, png_file);
        }
        fclose (png_file);
        png_file = NULL;

        /* Get the decompressed image size */
        size_t image_size = 0;
        if (spng_set_png_buffer (spng_context, png_buffer, png_size) != 0)
        {
            fprintf (stderr, "Error: Failed to set file buffer for %s.\n", argv [i]);
            rc = EXIT_FAILURE;
            break;
        }

        if (spng_decoded_image_size (spng_context, SPNG_FMT_RGBA8, &image_size) != 0)
        {
            fprintf (stderr, "Error: Failed to determine decompression size for %s.\n", argv [i]);
            rc = EXIT_FAILURE;
            break;
        }

        /* Allocate memory for the decompressed image */
        uint8_t *image_buffer = calloc (image_size, 1);
        if (image_buffer == NULL)
        {
            fprintf (stderr, "Error: Failed to allocate decompression memory for %s.\n", argv [i]);
            rc = EXIT_FAILURE;
            break;
        }

        /* Decode the image */
        if (spng_decode_image (spng_context, image_buffer, image_size, SPNG_FMT_RGBA8, SPNG_DECODE_TRNS) != 0)
        {
            fprintf (stderr, "Error: Failed to decode image %s.\n", argv [i]);
            rc = EXIT_FAILURE;
            break;
        }

        /* Process the image */
        struct spng_ihdr header = {};
        spng_get_ihdr(spng_context, &header);
        if (process_image ((pixel_t *) image_buffer, header.width, header.height) != 0)
        {
            fprintf (stderr, "Error: Failed to process image %s.\n", argv [i]);
            rc = EXIT_FAILURE;
            break;
        }

        /* Tidy up */
        free (png_buffer);
        free (image_buffer);
        spng_ctx_free (spng_context);
    }

    if (palette_size > 16)
    {
        fprintf (stderr, "Error: Exceeded palette limit with %d colours.\n", palette_size);
        rc = EXIT_FAILURE;
    }
    else
    {
        printf ("uint8_t palette [16] = { ");

        for (uint32_t i = 0; i < palette_size; i++)
        {
            printf ("0x%02x%s", palette [i], ((i + 1) < palette_size) ? ", " : " };\n");
        }
    }

    return rc;
}
