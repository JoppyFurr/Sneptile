#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <spng.h>

/* TODO:
 *  - Output an updated palette as source
 *  - Output patterns as source
 *  - Output defines for pattern indexes based on file name
 *  - Option for tall-sprite-mode tile ordering
 *  - Detect and remove duplicate tiles
 *  - Some way to specify index for colour cycling
 *  - A sprite-mode to not colour-match against index 0
 */

typedef struct pixel_s {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} pixel_t;

static uint32_t pattern_index = 0;
static uint8_t palette [16] = {};
static uint32_t palette_size = 0;

FILE *palette_file = NULL;
FILE *pattern_file = NULL;
FILE *pattern_index_file = NULL;


/*
 * Open the three output files.
 */
static int open_files (void)
{
    /* TODO: User-specidied output directory */

    pattern_file = fopen ("pattern.h", "w");
    if (pattern_file == NULL)
    {
        /* TODO */
        fprintf (stderr, "Unable to open ...");
        return -1;
    }

    pattern_index_file = fopen ("pattern_index.h", "w");
    if (pattern_index_file == NULL)
    {
        /* TODO */
        fprintf (stderr, "Unable to open ...");
        return -1;
    }

    palette_file = fopen ("palette.h", "w");
    if (palette_file == NULL)
    {
        /* TODO */
        fprintf (stderr, "Unable to open ...");
        return -1;
    }

    return 0;
}


/*
 * Close the three output files.
 */
static void close_files (void)
{
    if (pattern_file != NULL)
    {
        fclose (pattern_file);
        pattern_file = NULL;
    }

    if (pattern_index_file != NULL)
    {
        fclose (pattern_index_file);
        pattern_index_file = NULL;
    }

    if (palette_file != NULL)
    {
        fclose (palette_file);
        palette_file = NULL;
    }
}


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
    fprintf (pattern_file, "    ");
    for (uint32_t y = 0; y < 8; y++)
    {
        uint8_t line_data[4] = {};

        for (uint32_t x = 0; x < 8; x++)
        {
            uint8_t index = 0;
            pixel_t p = buffer [x + y * stride];

            /* If the pixel is non-transparent, calculate its colour index */
            if (p.a != 0)
            {
                uint8_t colour = ((p.r & 0xc0) >> 6)
                               | ((p.g & 0xc0) >> 4)
                               | ((p.b & 0xc0) >> 2);

                index = colour_to_index (colour);;
            }

            /* Convert index to bitplane representation */
            for (uint32_t i = 0; i < 4; i++)
            {
                if (index & (1 << i))
                {
                    line_data [i] |= (1 << (7 - x));
                }
            }
        }

        fprintf (pattern_file, "0x%02x%02x%02x%02x%s",
                 line_data [3], line_data [2], line_data [1], line_data [0],
                 (y < 7) ? ", " : ",\n");
    }

    pattern_index++;
}


/*
 * Generate a #define for the index of the upcoming pattern.
 */
static void generate_pattern_index (char *name)
{
    fprintf (pattern_index_file, "#define PATTERN_");

    for (char c = *name; *name != '\0'; c = *++name)
    {
        /* Don't include the file extension */
        if (c == '.')
        {
            break;
        }
        if (!isalnum (c))
        {
            c = '_';
        }

        fprintf (pattern_index_file, "%c", toupper(c));
    }

    fprintf (pattern_index_file, " %d\n", pattern_index);
}


/*
 * Process an image made up of 8x8 tiles.
 */
static int process_image (pixel_t *buffer, uint32_t width, uint32_t height, char *name)
{
    generate_pattern_index (name);

    fprintf (pattern_file, "\n    /* %s */\n", name);
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


/*
 * Process a single .png file.
 */
static int process_file (char *name)
{
    spng_ctx *spng_context = spng_ctx_new (0);

    /* Try to open the file */
    FILE *png_file = fopen (name, "r");
    if (png_file == NULL)
    {
        fprintf (stderr, "Error: Unable to open %s.\n", name);
        return -1;
    }

    /* Once the file has been opened, drop the path and use only the file name */
    if (strrchr (name, '/') != NULL)
    {
        name = strrchr (name, '/') + 1;
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
        fprintf (stderr, "Error: Failed to allocate memory for %s.\n", name);
        return -1;
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
        fprintf (stderr, "Error: Failed to set file buffer for %s.\n", name);
        return -1;
    }

    if (spng_decoded_image_size (spng_context, SPNG_FMT_RGBA8, &image_size) != 0)
    {
        fprintf (stderr, "Error: Failed to determine decompression size for %s.\n", name);
        return -1;
    }

    /* Allocate memory for the decompressed image */
    uint8_t *image_buffer = calloc (image_size, 1);
    if (image_buffer == NULL)
    {
        fprintf (stderr, "Error: Failed to allocate decompression memory for %s.\n", name);
        return -1;
    }

    /* Decode the image */
    if (spng_decode_image (spng_context, image_buffer, image_size, SPNG_FMT_RGBA8, SPNG_DECODE_TRNS) != 0)
    {
        fprintf (stderr, "Error: Failed to decode image %s.\n", name);
        return -1;
    }

    /* Process the image */
    struct spng_ihdr header = {};
    spng_get_ihdr(spng_context, &header);
    if (process_image ((pixel_t *) image_buffer, header.width, header.height, name) != 0)
    {
        fprintf (stderr, "Error: Failed to process image %s.\n", name);
        return -1;
    }

    /* Tidy up */
    free (png_buffer);
    free (image_buffer);
    spng_ctx_free (spng_context);

    return 0;
}


/*
 * Export the palette array.
 */
static int export_palette (void)
{
    if (palette_size > 16)
    {
        fprintf (stderr, "Error: Exceeded palette limit with %d colours.\n", palette_size);
        return -1;
    }

    fprintf (palette_file, "const uint8_t palette [16] = { ");

    for (uint32_t i = 0; i < palette_size; i++)
    {
        fprintf (palette_file, "0x%02x%s", palette [i], ((i + 1) < palette_size) ? ", " : " };\n");
    }

    return 0;
}


int main (int argc, char **argv)
{
    int rc = EXIT_SUCCESS;

    if (argc < 2)
    {
        fprintf (stderr, "Usage: %s [--palette 0x00 0x01..] <tiles.png>\n", argv [0]);
        return EXIT_FAILURE;
    }
    argv++;
    argc--;

    /* Accept a user-initialized palette */
    if (strcmp (argv [0], "--palette") == 0)
    {
        while (++argv, --argc)
        {
            if (strncmp (argv [0], "0x", 2) == 0 && strlen (argv[0]) == 4)
            {
                palette [palette_size++] = strtol (argv [0], NULL, 16);
            }
            else
            {
                break;
            }
        }
    }

    if (open_files () < 0)
    {
        rc = EXIT_FAILURE;
    }
    else
    {
        fprintf (pattern_file, "const uint32_t patterns [] = {\n");

        for (uint32_t i = 0; i < argc; i++)
        {
            if (process_file (argv [i]) < 0)
            {
                rc = EXIT_FAILURE;
                break;
            }
        }

        fprintf (pattern_file, "};\n");
    }

    if (rc == EXIT_SUCCESS)
    {
        if (export_palette () < 0)
        {
            rc = EXIT_FAILURE;
        }
    }

    close_files ();

    return rc;
}
