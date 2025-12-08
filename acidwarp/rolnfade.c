#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "handy.h"
#include "acidwarp.h"
#include "rolnfade.h"
#include "palinit.h"
#include "warp_text.h"

int RedRollDirection = 0, GrnRollDirection = 0, BluRollDirection = 0;

// Use extern declaration for FadeCompleteFlag, defined in acidwarp.c
extern int FadeCompleteFlag;

/*
extern UCHAR MainPalArray  [256 * 3];
extern UCHAR TargetPalArray  [256 * 3];
extern UCHAR TempPalArray [256 * 3];
*/

void rotatebackward(int color, UCHAR *Pal)
{
  int temp;
  int x;
  
  temp = Pal[((254)*3)+3+color];

  for(x=(254); x >= 1; --x)
    Pal[(x*3)+3+color] = Pal[(x*3)+color];
  Pal[(1*3)+color] = temp; 
  
}

void rotateforward(int color, UCHAR *Pal)
{  
  int temp;
  int x;
  
  temp = Pal[(1*3)+color];
  for(x=1; x < (256) ; ++x)
    Pal[x*3+color] = Pal[(x*3)+3+color]; 
  Pal[((256)*3)-3+color] = temp;
}


void rollMainPalArrayAndLoadDACRegs(UCHAR *MainPalArray)
{
    maybeInvertSubPalRollDirection();
    roll_rgb_palArray(MainPalArray);
}


void rolNFadeWhtMainPalArrayNLoadDAC(UCHAR *MainPalArray)
{
    if (!FadeCompleteFlag)
    {
        if (fadePalArrayToWhite(MainPalArray) == DONE)
            FadeCompleteFlag = 1;
        rollMainPalArrayAndLoadDACRegs(MainPalArray);
    }
}

void rolNFadeBlkMainPalArrayNLoadDAC(UCHAR *MainPalArray)
{
    if (!FadeCompleteFlag)
    {
        if (fadePalArrayToBlack (MainPalArray) == DONE)
            FadeCompleteFlag = 1;
        rollMainPalArrayAndLoadDACRegs(MainPalArray);
    }
}

void rolNFadeMainPalAryToTargNLodDAC(UCHAR *MainPalArray, UCHAR *TargetPalArray)
{
    if (!FadeCompleteFlag)
    {
        if (fadePalArrayToTarget (MainPalArray, TargetPalArray) == DONE)
            FadeCompleteFlag = 1;

        maybeInvertSubPalRollDirection();
        roll_rgb_palArray (  MainPalArray);
        roll_rgb_palArray (TargetPalArray);
    }
    else
        rollMainPalArrayAndLoadDACRegs(MainPalArray);
}

/* WARNING! This is the function that handles the case of the SPECIAL PALETTE TYPE.
   This palette type is special in that there is no specific palette assigned to its
   palette number. Rather the palette is morphed from one static palette to another.
   The effect is quite interesting.
*/

void rolNFadMainPalAry2RndTargNLdDAC(UCHAR *MainPalArray, UCHAR *TargetPalArray)
{
	if (fadePalArrayToTarget (MainPalArray, TargetPalArray) == DONE)
         initPalArray (TargetPalArray, RANDOM (NUM_PALETTE_TYPES));

	maybeInvertSubPalRollDirection();
	roll_rgb_palArray (  MainPalArray);
	roll_rgb_palArray (TargetPalArray);
    // TODO: Update SDL2 palette here
}

/**********************************************************************************/

/* These routines do the actual fading of a palette array to white, black,
	or to the values of another ("target") palette array.
*/

int fadePalArrayToWhite (UCHAR *MainpalArray)
{
    UCHAR *palArray = MainpalArray;
    int palByteNum, num_white = 0;
    for (palByteNum = 3; palByteNum < 768; ++palByteNum)
    {
        if (palArray[palByteNum] < 63)
            ++palArray[palByteNum];
        else
            ++num_white;
    }
    return ((num_white >= 765) ? DONE : NOT_DONE);
}

int fadePalArrayToBlack (UCHAR *MainpalArray)
{
    UCHAR *palArray = MainpalArray;
    int palByteNum, num_black = 0;
    for (palByteNum = 3; palByteNum < 768; ++palByteNum)
    {
        if (palArray[palByteNum] > 0)
            --palArray[palByteNum];
        else
            ++num_black;
    }
    return ((num_black >= 765) ? DONE : NOT_DONE);
}

int fadePalArrayToTarget (UCHAR *palArrayBeingChanged, const UCHAR *targetPalArray)
{
    int palByteNum, num_equal = 0;
    for (palByteNum = 3; palByteNum < 768; ++palByteNum)
    {
        if   (palArrayBeingChanged[palByteNum] < targetPalArray[palByteNum])
             ++palArrayBeingChanged[palByteNum];
        else if (palArrayBeingChanged[palByteNum] > targetPalArray[palByteNum])
             --palArrayBeingChanged[palByteNum];
        else
            ++num_equal;
    }
    return ((num_equal >= 765) ? DONE : NOT_DONE);
}

void roll_rgb_palArray(UCHAR *Pal)
{
    if (!RedRollDirection)
         rotateforward(RED,Pal);
    else
         rotatebackward(RED,Pal);

    if(!GrnRollDirection)
         rotateforward(GREEN,Pal);
    else
         rotatebackward(GREEN,Pal);

    if(!BluRollDirection)
         rotateforward(BLUE,Pal);
    else
         rotatebackward(BLUE,Pal);
}

/* This routine switches the current direction of one of the sub-palettes (R, G, or B) with probability
 * 1/DIRECTN_CHANGE_PERIOD_IN_TICKS, when it is called by the timer ISR.  Only one color direction can change at a time.
 */

void maybeInvertSubPalRollDirection(void)
{
  switch (RANDOM(DIRECTN_CHANGE_PERIOD_IN_TICKS))
	{
		case 0 :
			RedRollDirection = !RedRollDirection;
		break;

		case 1 :
			GrnRollDirection = !GrnRollDirection;
		break;

		case 2 :
			BluRollDirection = !BluRollDirection;
		break;
	}
}
