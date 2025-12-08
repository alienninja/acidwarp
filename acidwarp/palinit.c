#include <stdlib.h>
#include "handy.h"
#include "acidwarp.h"
#include "palinit.h"
#include "warp_text.h"

/**
 * Initializes a palette array to one of the palette types.
 * 
 * @param palArray The palette array to initialize.
 * @param pal_type The type of palette to initialize to.
 */
void initPalArray (UCHAR *palArray, int pal_type)
{
    switch (pal_type)
    {
        case RGBW_PAL:
            init_rgbw_palArray (palArray);
        break;
        case W_PAL:
            init_w_palArray (palArray);
        break;
        case W_HALF_PAL:
            init_w_half_palArray (palArray);
        break;
        case PASTEL_PAL:
            init_pastel_palArray (palArray);
        break;
        case RGBW_LIGHTNING_PAL:
              init_rgbw_palArray (palArray);
              add_sparkles_to_palette (palArray, 9);
              init_w_palArray (palArray);
              add_sparkles_to_palette (palArray, 9);
            break;
        case W_HALF_LIGHTNING_PAL:
                      init_w_half_palArray (palArray);
                      add_sparkles_to_palette (palArray, 9);
                break;
        case PASTEL_LIGHTNING_PAL:
                      init_pastel_palArray (palArray);
                      add_sparkles_to_palette (palArray, 9);
                break;
        case W_LIGHTNING_PAL:
              init_w_palArray (palArray);
              add_sparkles_to_palette (palArray, 9);
            break;
        default:
            init_w_palArray(palArray);
        break;
    }
}

/**
 * Adds sparkles to a palette array.
 * 
 * @param palArray The palette array to add sparkles to.
 * @param sparkle_amount The amount of sparkle to add.
 */
void add_sparkles_to_palette (UCHAR *palArray, int sparkle_amount)
{
    int palRegNum;

    for (palRegNum = 1; palRegNum < 256;  palRegNum += 4)
    {
        palArray[palRegNum * 3    ] = (UCHAR)MIN(63, palArray[palRegNum * 3    ] + sparkle_amount);
        palArray[palRegNum * 3 + 1] = (UCHAR)MIN(63, palArray[palRegNum * 3 + 1] + sparkle_amount);
        palArray[palRegNum * 3 + 2] = (UCHAR)MIN(63, palArray[palRegNum * 3 + 2] + sparkle_amount);
    }
}

/**
 * Initializes an RGBW palette array.
 * 
 * @param palArray The palette array to initialize.
 */
void init_rgbw_palArray (UCHAR *palArray)
{
	int palRegNum;

	for (palRegNum = 0; palRegNum < 32; ++palRegNum)
	{
		palArray[ palRegNum        * 3    ] = (UCHAR)palRegNum * 2;
		palArray[(palRegNum +  64) * 3    ] = (UCHAR)0;
		palArray[(palRegNum + 128) * 3    ] = (UCHAR)0;
		palArray[(palRegNum + 192) * 3    ] = (UCHAR)palRegNum * 2;

		palArray[ palRegNum        * 3 + 1] = (UCHAR)0;
		palArray[(palRegNum +  64) * 3 + 1] = (UCHAR)palRegNum * 2;
		palArray[(palRegNum + 128) * 3 + 1] = (UCHAR)0;
		palArray[(palRegNum + 192) * 3 + 1] = (UCHAR)palRegNum * 2;

		palArray[ palRegNum        * 3 + 2] = (UCHAR)0;
		palArray[(palRegNum +  64) * 3 + 2] = (UCHAR)0;
		palArray[(palRegNum + 128) * 3 + 2] = (UCHAR)palRegNum * 2;
		palArray[(palRegNum + 192) * 3 + 2] = (UCHAR)palRegNum * 2;
	}

	for (palRegNum = 32; palRegNum < 64; ++palRegNum)
	{
		palArray[ palRegNum        * 3    ] = (UCHAR)(63 - palRegNum) * 2;
		palArray[(palRegNum +  64) * 3    ] = (UCHAR)0;
		palArray[(palRegNum + 128) * 3    ] = (UCHAR)0;
		palArray[(palRegNum + 192) * 3    ] = (UCHAR)(63 - palRegNum) * 2;

		palArray[ palRegNum        * 3 + 1] = (UCHAR)0;
		palArray[(palRegNum +  64) * 3 + 1] = (UCHAR)(63 - palRegNum) * 2;
		palArray[(palRegNum + 128) * 3 + 1] = (UCHAR)0;
		palArray[(palRegNum + 192) * 3 + 1] = (UCHAR)(63 - palRegNum) * 2;

		palArray[ palRegNum        * 3 + 2] = (UCHAR)0;
		palArray[(palRegNum +  64) * 3 + 2] = (UCHAR)0;
		palArray[(palRegNum + 128) * 3 + 2] = (UCHAR)(63 - palRegNum) * 2;
		palArray[(palRegNum + 192) * 3 + 2] = (UCHAR)(63 - palRegNum) * 2;
	}
}


/**
 * Initializes a W palette array.
 * 
 * @param palArray The palette array to initialize.
 */
void init_w_palArray (UCHAR *palArray)
{
        int palRegNum;

        for (palRegNum = 0; palRegNum < 128; ++palRegNum)
        {
                palArray[palRegNum * 3    ] = (UCHAR)palRegNum/2;
                palArray[palRegNum * 3 + 1] = (UCHAR)palRegNum/2;
                palArray[palRegNum * 3 + 2] = (UCHAR)palRegNum/2;
        }

        for (palRegNum = 128; palRegNum < 256; ++palRegNum)
        {
                palArray[palRegNum * 3    ] = (UCHAR)(255 - palRegNum)/2;
                palArray[palRegNum * 3 + 1] = (UCHAR)(255 - palRegNum)/2;
                palArray[palRegNum * 3 + 2] = (UCHAR)(255 - palRegNum)/2;
        }

}

/**
 * Initializes a W half palette array.
 * 
 * @param palArray The palette array to initialize.
 */
void init_w_half_palArray (UCHAR *palArray)
{
	int palRegNum;

	for (palRegNum = 0; palRegNum < 64; ++palRegNum)
	{
		palArray[palRegNum        * 3    ] = (UCHAR)palRegNum;
		palArray[palRegNum        * 3 + 1] = (UCHAR)palRegNum;
		palArray[palRegNum        * 3 + 2] = (UCHAR)palRegNum;

		palArray[(palRegNum + 64) * 3    ] = (UCHAR)(63 - palRegNum);
		palArray[(palRegNum + 64) * 3 + 1] = (UCHAR)(63 - palRegNum);
		palArray[(palRegNum + 64) * 3 + 2] = (UCHAR)(63 - palRegNum);
	}

	for (palRegNum = 128; palRegNum < 256; ++palRegNum)
	{
		palArray[palRegNum        * 3    ] = 0;
		palArray[palRegNum        * 3 + 1] = 0;
		palArray[palRegNum        * 3 + 2] = 0;
	}
}

/**
 * Initializes a pastel palette array.
 * 
 * @param palArray The palette array to initialize.
 */
void init_pastel_palArray (UCHAR *palArray)
{
	int palRegNum;

	for (palRegNum = 0; palRegNum < 128; ++palRegNum)
	{
		palArray[ palRegNum        * 3    ] = (UCHAR)31 + palRegNum/4;
		palArray[ palRegNum        * 3 + 1] = (UCHAR)31 + palRegNum/4;
		palArray[ palRegNum        * 3 + 2] = (UCHAR)31 + palRegNum/4;

		palArray[(palRegNum + 128) * 3    ] = (UCHAR)31 + (127 - palRegNum)/4;
		palArray[(palRegNum + 128) * 3 + 1] = (UCHAR)31 + (127 - palRegNum)/4;
		palArray[(palRegNum + 128) * 3 + 2] = (UCHAR)31 + (127 - palRegNum)/4;
	}
}
