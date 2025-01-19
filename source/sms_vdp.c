/*
 * Sneptile
 * Joppy Furr 2024
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sneptile.h"
#include "sms_vdp.h"

/* State */
static uint32_t pattern_index = 0;

/* Palettes */
static uint8_t background_palette [16] = { };
static uint32_t background_palette_size = 0;
static uint8_t sprite_palette [16] = { };
static uint32_t sprite_palette_size = 0;

/* Mode-4 Output Files */
static FILE *pattern_file = NULL;
static FILE *pattern_index_file = NULL;
static FILE *palette_file = NULL;


/*
 * Open the three output files.
 */
int mode4_open_files (void)
{
    char *patterns_path = "patterns.h";
    char *pattern_index_path = "pattern_index.h";
    char *palette_path = "palette.h";

    /* If the user has specified an output
     * directory, create and change into it */
    if (output_dir != NULL)
    {
        asprintf (&patterns_path, "%s/%s", output_dir, patterns_path);
        asprintf (&pattern_index_path, "%s/%s", output_dir, pattern_index_path);
        asprintf (&palette_path, "%s/%s", output_dir, palette_path);
    }

    /* Pattern file */
    pattern_file = fopen (patterns_path, "w");
    if (pattern_file == NULL)
    {
        fprintf (stderr, "Unable to open output file patterns.h\n");
        return RC_ERROR;
    }
    fprintf (pattern_file, "/*\n");
    fprintf (pattern_file, " * VDP Pattern data\n");
    fprintf (pattern_file, " */\n");

    /* Pattern index file */
    pattern_index_file = fopen (pattern_index_path, "w");
    if (pattern_index_file == NULL)
    {
        fprintf (stderr, "Unable to open output file pattern_index.h\n");
        return RC_ERROR;
    }
    fprintf (pattern_index_file, "/*\n");
    fprintf (pattern_index_file, " * VDP Pattern index data\n");
    fprintf (pattern_index_file, " */\n");

    /* Palette file */
    palette_file = fopen (palette_path, "w");
    if (palette_file == NULL)
    {
        fprintf (stderr, "Unable to open output file palette.h\n");
        return RC_ERROR;
    }
    fprintf (palette_file, "/*\n");
    fprintf (palette_file, " * VDP Palette data\n");
    fprintf (palette_file, " */\n");

    if (output_dir != NULL)
    {
        free (patterns_path);
        free (pattern_index_path);
        free (palette_path);
    }

    return RC_OK;
}


/*
 * Mark the start of a new source file.
 */
void mode4_new_input_file (const char *name)
{
    static bool first = true;
    if (first)
    {
        first = false;
    }
    else
    {
        fprintf (pattern_file, "};\n");
    }

    /* Strip the extension for the array name */
    char *base_name = strdup (name);
    char *extension = strchr (base_name, '.');
    if (extension)
    {
        extension [0] = '\0';
    }

    /* Start new data array in patterns file */
    fprintf (pattern_file, "\nconst uint32_t %s_patterns [] = {\n", base_name);
    free (base_name);

    /* Pattern indices are within the current output array */
    pattern_index = 0;
}


/*
 * Generate panel indexes for the file.
 * Bit 11 in the index indicates use of the sprite palette.
 */
void mode4_process_panels (const char *name, palette_t palette, uint32_t panel_count, uint32_t panel_width, uint32_t panel_height, pixel_t *buffer)
{
    /* Strip the extension for the array name */
    char *base_name = strdup (name);
    char *extension = strchr (base_name, '.');
    if (extension)
    {
        extension [0] = '\0';
    }

    fprintf (pattern_index_file, "\nconst uint16_t %s_panels [%d] [%d] = {\n", base_name, panel_count, panel_width * panel_height);
    free (base_name);

    for (uint32_t panel_row = 0; panel_row < current_image.height; panel_row += 8 * panel_height)
    for (uint32_t panel_col = 0; panel_col < current_image.width; panel_col += 8 * panel_width)
    {
        uint32_t tile_count = 0;
        fprintf (pattern_index_file, "    { ");
        for (uint32_t row = panel_row; row < panel_row + panel_height * 8; row += 8)
        for (uint32_t col = panel_col; col < panel_col + panel_width * 8; col += 8)
        {
            fprintf (pattern_index_file, "0x%04x", sneptile_get_match (&buffer [row * current_image.width + col]));

            if (!(row == panel_row + (panel_height - 1) * 8 &&
                  col == panel_col + (panel_width - 1) * 8))
            {
                fprintf (pattern_index_file, "%s", (tile_count == 11) ? ",\n      " : ", ");
            }

            tile_count = (tile_count + 1) % 12;
        }
        fprintf (pattern_index_file, " }%s\n", (panel_count > 1) ? "," : "");

        if (--panel_count == 0)
        {
            break;
        }
    }
    fprintf (pattern_index_file, "};\n");
}

/*
 * Convert from 6-bit SMS colour to the equivalent 12-bit GG colour.
 */
static uint16_t mode4_sms_colour_to_gg (uint8_t sms_colour)
{
    uint16_t gg_colour = 0;

    gg_colour |=  (sms_colour      & 0x03) * 5;         /* Red */
    gg_colour |= ((sms_colour >> 2 & 0x03) * 5) << 4;   /* Green */
    gg_colour |= ((sms_colour >> 4 & 0x03) * 5) << 8;   /* Blue */

    return gg_colour;
}


/*
 * Output the palette file.
 */
static int mode4_palette_write (void)
{
    if (background_palette_size > 16 || sprite_palette_size > 16)
    {
        fprintf (stderr, "Error: Exceeded palette size limit.\n");
        fprintf (stderr, "       Background palette: %u colours.\n", background_palette_size);
        fprintf (stderr, "       Sprite palette: %u colours.\n", sprite_palette_size);
        return RC_ERROR;
    }

    /* SMS Palette */
    fprintf (palette_file, "\n#ifdef TARGET_SMS\n");

    fprintf (palette_file, "static const uint8_t background_palette [16] = { ");
    for (uint32_t i = 0; i < background_palette_size; i++)
    {
        fprintf (palette_file, "0x%02x%s", background_palette [i], ((i + 1) < background_palette_size) ? ", " : " };\n");
    }

    fprintf (palette_file, "static const uint8_t sprite_palette [16] = { ");
    for (uint32_t i = 0; i < sprite_palette_size; i++)
    {
        fprintf (palette_file, "0x%02x%s", sprite_palette [i], ((i + 1) < sprite_palette_size) ? ", " : " };\n");
    }

    /* GG Palette */
    fprintf (palette_file, "#elif defined (TARGET_GG)\n");

    fprintf (palette_file, "static const uint16_t background_palette [16] = { ");
    for (uint32_t i = 0; i < background_palette_size; i++)
    {
        fprintf (palette_file, "0x%04x%s", mode4_sms_colour_to_gg (background_palette [i]), ((i + 1) < background_palette_size) ? ", " : " };\n");
    }

    fprintf (palette_file, "static const uint16_t sprite_palette [16] = { ");
    for (uint32_t i = 0; i < sprite_palette_size; i++)
    {
        fprintf (palette_file, "0x%04x%s", mode4_sms_colour_to_gg (sprite_palette [i]), ((i + 1) < sprite_palette_size) ? ", " : " };\n");
    }

    fprintf (palette_file, "#endif\n");

    return RC_OK;
}


/*
 * Finalize and close the three output files.
 */
int mode4_close_files (void)
{
    int rc;

    /* First, write the completed palette to file */
    rc = mode4_palette_write ();

    /* Pattern file */
    fprintf (pattern_file, "};\n");
    fclose (pattern_file);
    pattern_file = NULL;

    /* Pattern index file */
    fclose (pattern_index_file);
    pattern_index_file = NULL;

    /* Palette file */
    fclose (palette_file);
    palette_file = NULL;

    return rc;
}


/*
 * Add a colour to the palette.
 * Return the index of the newly added colour.
 */
uint8_t mode4_palette_add_colour (palette_t palette, uint8_t colour)
{
    if (palette == PALETTE_BACKGROUND)
    {
        background_palette [background_palette_size] = colour;
        return background_palette_size++;
    }
    else
    {
        sprite_palette [sprite_palette_size] = colour;
        return sprite_palette_size++;
    }
}


/*
 * Convert from pixel colour to palette index.
 * New colours are added to the palette as needed.
 */
static uint8_t mode4_rgb_to_index (palette_t palette, pixel_t p)
{
    /* First, convert the pixel to a 6-bit Master System colour. */
    uint8_t colour = ((p.r & 0xc0) >> 6)
                   | ((p.g & 0xc0) >> 4)
                   | ((p.b & 0xc0) >> 2);

    /* Next, check if the colour is already in the palette */
    uint32_t start = (target == VDP_MODE_4_SPRITES) ? 1 : 0;

    if (palette == PALETTE_BACKGROUND)
    {
        for (uint32_t i = start; i < background_palette_size; i++)
        {
            if (background_palette [i] == colour)
            {
                return i;
            }
        }
    }
    else
    {
        for (uint32_t i = start; i < sprite_palette_size; i++)
        {
            if (sprite_palette [i] == colour)
            {
                return i;
            }
        }
    }

    /* If not, add it */
    return mode4_palette_add_colour (palette, colour);
}


/*
 * Process a single 8×8 tile.
 */
void mode4_process_tile (palette_t palette, pixel_t *buffer)
{
    fprintf (pattern_file, "    ");
    for (uint32_t y = 0; y < 8; y++)
    {
        uint8_t line_data[4] = { };

        for (uint32_t x = 0; x < 8; x++)
        {
            uint8_t index = 0;
            pixel_t p = buffer [x + y * current_image.width];

            /* If the pixel is non-transparent, calculate its colour index */
            if (p.a != 0)
            {
                index = mode4_rgb_to_index (palette, p);
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




