/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// CBar header file
// Created 22/6/07
// Dark Charlie

#ifndef __CBAR_H__DEPRECATED_GUI__
#define __CBAR_H__DEPRECATED_GUI__

#include <SDL.h>
#include "MathLib.h"
#include "Color.h"

namespace DeprecatedGUI {

// Bar directions
enum {
	BAR_LEFTTORIGHT,
	BAR_RIGHTTOLEFT,
	BAR_TOPTOBOTTOM,
	BAR_BOTTOMTOTOP
};

class CBar {
public:
	CBar() {}
	CBar(SmartPointer<SDL_Surface> bmp, int x, int y, int label_x, int label_y, int dir, int num_fore_states = 1, int num_bg_states = 1);

private:
	// Variables
	int  X;
	int  Y;
	int  LabelX;
	int  LabelY;
	bool LabelVisible;
	int  Direction;
	int  Position;
	int  NumForeStates;
	int	 NumBgStates;
	int  CurrentForeState;
	int	 CurrentBgState;

	SmartPointer<SDL_Surface> bmpBar;

	Color	bgColor;
	Color	foreColor;

public:
	// Methods
	void		Draw(SDL_Surface * dst);
	int			GetWidth();
	int			GetHeight();
	inline int	GetPosition()  { return Position; }
	inline void SetPosition(int _p) { Position = _p; }
	inline int	GetX() { return X; }
	inline void SetX(int _x) { X = _x; }
	inline int	GetY() { return Y; }
	inline void SetY(int _y) { Y = _y; }
	inline int	GetLabelX() { return LabelX; }
	inline void SetLabelX(int _x) { LabelX = _x; }
	inline int	GetLabelY() { return LabelY; }
	inline void SetLabelY(int _y) { LabelY = _y; }
	inline int	GetNumForeStates()  { return NumForeStates; }
	inline void SetNumForeStates(int _s) { NumForeStates = _s; }  // NOTE: number of states is one state less than count of images
	inline int	GetCurrentForeState()  { return CurrentForeState; }  //
	inline void SetCurrentForeState(int _s) { CurrentForeState = MIN(NumForeStates-1, _s); } // NOTE: the first state is 0
	inline int	GetNumBgStates()  { return NumBgStates; }
	inline void SetNumBgStates(int _s) { NumBgStates = _s; }  // NOTE: number of states is one state less than count of images
	inline int	GetCurrentBgState()  { return CurrentBgState; }  //
	inline void SetCurrentBgState(int _s) { CurrentBgState = MIN(NumBgStates-1, _s); } // NOTE: the first state is 0
	inline bool IsLabelVisible()  { return LabelVisible; }
	inline void SetLabelVisible(bool _v)  { LabelVisible = _v; }
	inline Color GetBgColor()  { return bgColor; }
	inline void SetBgColor(Color _cl)  { bgColor = _cl; }
	inline Color GetForeColor()  { return foreColor; }
	inline void SetForeColor(Color _cl)  { foreColor = _cl; }
	inline bool IsProperlyLoaded()  { return bmpBar.get() != NULL; }
};

} // namespace DeprecatedGUI

#endif // __CBAR_H__DEPRECATED_GUI__
