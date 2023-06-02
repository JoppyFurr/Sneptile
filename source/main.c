#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <spng.h>

/* TODO:
 *  - Take in multiple files
 *  - Take in a palette as a command line option.
 *  - Output an updated palette as a new .h file
 *  - Option for tall-sprite-mode tile ordering
 *  - Detect and remove duplicate tiles
 */

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

        if ( spng_decoded_image_size (spng_context, SPNG_FMT_RGBA8, &image_size) != 0)
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
        if (spng_decode_image (spng_context, image_buffer, image_size, SPNG_FMT_RGBA8, 0) != 0)
        {
            fprintf (stderr, "Error: Failed to decode image %s.\n", argv [i]);
            rc = EXIT_FAILURE;
            break;
        }

        /* DO SOMETHING */
        struct spng_ihdr header = {};
        spng_get_ihdr(spng_context, &header);
        printf ("Image width: %d.\n", header.width);
        printf ("Image height: %d.\n", header.height);
        printf ("Image bit_depth: %d.\n", header.bit_depth);
        printf ("Image color_type: %d.\n", header.color_type);

        /* Tidy up */
        free (png_buffer);
        free (image_buffer);
        spng_ctx_free (spng_context);
    }

    return rc;
}
