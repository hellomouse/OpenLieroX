/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Checkbox
// Created 30/3/03
// Jason Boettcher

#include <cassert>
#include "LieroX.h"

#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "StringUtils.h"
#include "DeprecatedGUI/CCheckbox.h"


namespace DeprecatedGUI {

///////////////////
// Draw the checkbox
void CCheckbox::Draw(SDL_Surface * bmpDest)
{
	if (bRedrawMenu)
		Menu_redrawBufferRect( iX,iY, 17,17 );

    if(bValue)
		DrawImageAdv(bmpDest, bmpImage, 17,0,iX,iY,17,17);
	else
	    DrawImageAdv(bmpDest, bmpImage, 0,0,iX,iY,17,17);
}


///////////////////
// Create
void CCheckbox::Create()
{
    bmpImage = LoadGameImage("data/frontend/checkbox.png");
}


static bool CCheckBox_WidgetRegistered = 
	CGuiSkin::RegisterWidget( "checkbox", & CCheckbox::WidgetCreator )
							( "var", SVT_STRING )
							( "click", SVT_STRING );

	
CCheckbox::CCheckbox(ScriptVar_t& var) {
	assert(var.type == SVT_BOOL);
	bValue = var.b;
	bmpImage = NULL;
	iType = wid_Checkbox;
	bVar = &var.b;
	iVar = NULL;
}


}; // namespace DeprecatedGUI



