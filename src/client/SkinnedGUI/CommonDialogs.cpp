
#include "LieroX.h"

#include "SkinnedGUI/CommonDialogs.h"
#include "SkinnedGUI/CButton.h"
#include "SkinnedGUI/CTabControl.h"
#include "SkinnedGUI/CLabel.h"
#include "SkinnedGUI/CTextbox.h"
#include "SkinnedGUI/CSlider.h"
#include "SkinnedGUI/CCheckbox.h"
#include "SkinnedGUI/CListview.h"
#include "MathLib.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "CGameScript.h"
#include "CWpnRest.h"
#include "AuxLib.h"
#include "Options.h"


namespace SkinnedGUI {

#define MAX_LOADING_TIME 500
#define DEFAULT_LOADING_TIME 100

//////////////////
// Create
CMessageBox::CMessageBox(CContainerWidget *parent, const std::string &title, const std::string &text, MessageBoxType type) :
CDialog("_MessageBox", parent, title, true)
{
	// Dimensions
	int w = MIN(VideoPostProcessor::get()->screenWidth(), GetTextWidth(cFont, text));
	int h = MIN(VideoPostProcessor::get()->screenHeight(), GetTextHeight(cFont, text));
	Resize((VideoPostProcessor::get()->screenWidth() - w)/2, (VideoPostProcessor::get()->screenHeight() - h)/2, w, h);

	iType = type;
	sText = text;
	CLEAR_EVENT(OnReturn);

	// Buttons
	if (iType == LMB_OK)  {
		btnOk = new CButton("_Ok", this, "OK");
		btnYes = btnNo = NULL;
	} else {
		btnYes = new CButton("_Yes", this, "Yes");
		btnNo = new CButton("_No", this, "No");
		btnOk = NULL;
	}
}

///////////////////////
// Close (internal)
void CMessageBox::CloseBox(int result)
{
	CDialog::Close();

	CALL_EVENT(OnReturn, (this, result));
}

///////////////////////
// Key down event
int	CMessageBox::DoKeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)
{
	switch (keysym)  {
		case SDLK_ESCAPE:
			CloseBox(iType == LMB_OK ? MBR_OK : MBR_NO);
			break;

		case SDLK_RETURN:
		case SDLK_KP_ENTER:
			CloseBox(iType == LMB_OK ? MBR_OK : MBR_YES);
			break;
	}

	CDialog::DoKeyDown(c, keysym, modstate);
	return WID_PROCESSED;
}

////////////////////////
// Button click event
void CMessageBox::OnButtonClick(CWidget *sender, int x, int y, int dx, int dy, int button)
{
	int result = MBR_OK;
	if (sender->getName() == "msb_Ok")
		result = MBR_OK;
	else if (sender->getName() == "msb_Yes")
		result = MBR_YES;
	else if (sender->getName() == "msb_No")
		result = MBR_NO;
	else return;

	CloseBox(result);
}

////////////////////////
// Draw the messagebox
void CMessageBox::DoRepaint()
{
	CDialog::DoRepaint();

	// Draw the text
	SDL_Rect r = getClientRect();
	DrawGameText(bmpBuffer.get(), sText, cFont, CTextProperties(&r, algCenter, algTop));
}

//
// File dialog (abstract class)
//

	// File adder functor
	class FileDialogAdder { public:
		CListview* listview;
		std::string extension;
		FileDialogAdder(CListview* lv_, const std::string& ext) : listview(lv_), extension(ext) {}
		inline bool operator() (const std::string& f) {
			if(stringcaseequal(GetFileExtension(f), extension)) {
				std::string fname = GetBaseFilename(f);
				std::string name = fname.substr(0, fname.size() - 4); // remove the extension, the size calcing is safe here
				if(!listview->getItem(fname)) {
					listview->AddItem(fname);
					listview->AddTextSubitem(name);
				}
			}
			return true;
		}
	};

/////////////////////////////
// Constructor
CFileDialog::CFileDialog(COMMON_PARAMS, const std::string &title, const std::string &directory, const std::string &extension) :
CDialog(name, parent, title)
{
	CLEAR_EVENT(OnConfirm);
	sDirectory = directory;
	sExtension = extension;

	// Append a slash
	if (*sDirectory.rbegin() != '\\' && *sDirectory.rbegin() != '/')
		sDirectory += '/';

	btnOk = new CButton("_Ok", this, "OK");
	btnCancel = new CButton("_Cancel", this, "Cancel");
	lsvFileList = new CListview("_FileList", this);

	// Fill the listview
	FileDialogAdder adder(lsvFileList, sExtension);
	FindFiles(adder, sDirectory, false, FM_REG);
}

//
// File load dialog
//

///////////////////
// Constructor
CFileLoadDialog::CFileLoadDialog(COMMON_PARAMS, const std::string &title, const std::string &directory, const std::string &extension) :
CFileDialog(name, parent, title, directory, extension)
{

}

///////////////////
// Confirm click
void CFileLoadDialog::OkClick(CButton *sender, MouseButton button, const ModifiersState &modstate)
{
	std::string file = lsvFileList->getSelectedSIndex();
	if (file.size())  {
		CALL_EVENT(OnConfirm, (this, sDirectory + file));
		Close();
	}
}

//////////////////
// Cancel click
void CFileLoadDialog::CancelClick(CButton *sender, MouseButton button, const ModifiersState &modstate)
{
	Close();
}

/////////////////
// Double click on the file list
void CFileLoadDialog::FileListDoubleClick(CListview *sender, CListviewItem *item, int index)
{
	CALL_EVENT(OnConfirm, (this, sDirectory + item->getSIndex()));
	Close();
}

//
// File save dialog
//

///////////////////
// Constructor
CFileSaveDialog::CFileSaveDialog(COMMON_PARAMS, const std::string &title, const std::string &directory, const std::string &extension) :
CFileDialog(name, parent, title, directory, extension)
{
	txtFileName = new CTextbox("_FileName", this);
}

////////////////////
// Get the file name for saving
std::string CFileSaveDialog::GetFileName()
{
	std::string file = txtFileName->getText();
	if (!file.size())
		return "";

	// Test if the user added the extension, if not, add it
	bool have_ext = false;
	if (file.size() >= 4)
		have_ext =  stringcaseequal(file.substr(file.size() - 4, 4), "." + sExtension);
	if (!have_ext)
		file += "." + sExtension;

	return file;
}

//////////////////////
// Confirm click
void CFileSaveDialog::OkClick(CButton *sender, MouseButton button, const ModifiersState &modstate)
{
	std::string file = GetFileName();
	if (IsFileAvailable(file, false))  {
		dlgConfirmOverwrite = new CMessageBox(this, "Confirm Overwrite", "The file already exists, overwrite?", LMB_YESNO);
		return;
	}

	if (file.size())  {
		CALL_EVENT(OnConfirm, (this, sDirectory + file));
		Close();
	}
}

/////////////////////
// Cancel click
void CFileSaveDialog::CancelClick(CButton *sender, MouseButton button, const ModifiersState &modstate)
{
	Close();
}

/////////////////////
// Double click in the filelist
void CFileSaveDialog::FileListDoubleClick(CListview *sender, CListviewItem *item, int index)
{
	dlgConfirmOverwrite = new CMessageBox(this, "Confirm Overwrite", "The file already exists, overwrite?", LMB_YESNO);
}

//////////////////////
// Listview selection change
void CFileSaveDialog::FileListChange(CListview *sender, CListviewItem *newitem, int newindex, bool &cancel)
{
	txtFileName->setText(newitem->getName());
}

//////////////////////
// Overwrite confirm close
void CFileSaveDialog::MessageBoxClose(CMessageBox *sender, int result)
{
	if (result == MBR_YES)  {
		std::string file = GetFileName();
		if (file.size())
			CALL_EVENT(OnConfirm, (this, file));
		Close();
	}
}


//
// Game settings dialog
//

////////////////////
// Constructor
CGameSettingsDialog::CGameSettingsDialog(COMMON_PARAMS) : CDialog(name, parent, "Game Settings")
{
	// Default buttons
	btnOk = new CButton("_Ok", this, "OK");
	btnDefault = new CButton("_Default", this, "Default");
	tbcMain = new CTabControl("_MainTabControl", this);

	// Setup the tab control
	CTab *tbGeneral = new CTab("_General", NULL, "General");
	CTab *tbBonus = new CTab("_Bonus", NULL, "Bonus");
	CGuiSkinnedLayout *pgGeneral = tbcMain->AddBlankPage(tbGeneral);
	CGuiSkinnedLayout *pgBonus = tbcMain->AddBlankPage(tbBonus);

	// Setup events
	SET_BTNCLICK(btnOk, &CGameSettingsDialog::OkClick);
	SET_BTNCLICK(btnDefault, &CGameSettingsDialog::DefaultClick);

	// Add the widgets

	//
	// General tab
	//

	// Lives
	new CLabel(STATIC, pgGeneral, "Lives");
	txtLives = new CTextbox("_Lives", pgGeneral);
	txtLives->setMax(6);

	// Max kills
	new CLabel(STATIC, pgGeneral, "Max Kills");
	txtMaxKills = new CTextbox("_MaxKills", pgGeneral);
	txtMaxKills->setMax(6);

	// Loading time
	new CLabel(STATIC, pgGeneral, "Loading Time");
	sldLoadingTime = new CSlider("_LoadingTime", pgGeneral, MAX_LOADING_TIME);
	lblLoadingTime = new CLabel("_LoadingTimeLabel", pgGeneral, "");

	// Time limit
	new CLabel(STATIC, pgGeneral, "Time limit, minutes");
	txtTimeLimit = new CTextbox("_TimeLimit", pgGeneral);
	txtTimeLimit->setMax(3);

	// Respawn time
	new CLabel(STATIC, pgGeneral, "Respawn time, seconds");
	txtRespawnTime = new CTextbox("_RespawnTime", pgGeneral);
	txtRespawnTime->setMax(3);

	// Empty weapons when respawning
	new CLabel(STATIC, pgGeneral, "Empty weapons\nwhen respawning");
	chkEmptyWeapons = new CCheckbox("_EmptyWeapons", pgGeneral, false);
	
	// Respawn in waves
	new CLabel(STATIC, pgGeneral, "Respawn in waves");
	chkWaveRespawn = new CCheckbox("_RespawnInWaves", pgGeneral, false);

	// Group teams
	new CLabel(STATIC, pgGeneral, "Group Teams");
	chkGroupTeams = new CCheckbox("_GroupTeams", pgGeneral, false);

	// Suicide or teamkill decreases score
	new CLabel(STATIC, pgGeneral, "Suicide or teamkill\ndecreases score");
	chkDecreaseScore = new CCheckbox("_DecreaseScore", pgGeneral, false);


	//
	// Bonus tab
	//

	// Bonuses on
	new CLabel(STATIC, pgBonus, "Bonuses");
	chkBonusesOn = new CCheckbox("_BonusesOn", pgBonus, false);

	// Bonus spawn time
	new CLabel(STATIC, pgBonus, "Bonus Spawn AbsTime");
	txtBonusSpawnTime = new CTextbox("_BonusSpawnTime", pgBonus);

	// Show bonus names
	new CLabel(STATIC, pgBonus, "Show Bonus Names");
	chkShowBonusNames = new CCheckbox("_BonusNames", pgBonus, false);

	// Bonus life time
	new CLabel(STATIC, pgBonus, "Bonus Life AbsTime");
	txtBonusLifeTime = new CTextbox("_BonusLifeTime", pgBonus);

	// Health to weapon chance
	new CLabel(STATIC, pgBonus, "Health");
	new CLabel(STATIC, pgBonus, "Weapon");
	lblHealthChance = new CLabel(STATIC, pgBonus, "");
	lblWeaponChance = new CLabel(STATIC, pgBonus, "");
	sldWeaponToHealthChance = new CSlider("_WeaponToHealth", pgBonus, 100);

	// Load
	LoadFromOptions();
}

////////////////////
// Load the values from options
void CGameSettingsDialog::LoadFromOptions()
{
	// TODO: Hey, there should be some way without putting here all vars from options, it's just lame
	// Options system uses for() on CScriptableVars::Vars() array for that, that's the whole point of the scriptable vars
	// Lives
	if (tLXOptions->tGameInfo.iLives >= 0)
		txtLives->setText(itoa(tLXOptions->tGameInfo.iLives));

	// Kills
	if (tLXOptions->tGameInfo.iKillLimit >= 0)
		txtLives->setText(itoa(tLXOptions->tGameInfo.iKillLimit));

	// Loading time
	sldLoadingTime->setValue(tLXOptions->tGameInfo.iLoadingTime);
	lblLoadingTime->setText(itoa(tLXOptions->tGameInfo.iLoadingTime));

	// Time limit
	if (tLXOptions->tGameInfo.fTimeLimit > 0)
		txtTimeLimit->setText(ftoa(tLXOptions->tGameInfo.fTimeLimit));

	// Respawn time
	txtRespawnTime->setText(ftoa(tLXOptions->tGameInfo.fRespawnTime));

	// Empty weapons when respawning
	chkEmptyWeapons->setValue(tLXOptions->tGameInfo.bEmptyWeaponsOnRespawn);

	// Group teams
	chkGroupTeams->setValue(tLXOptions->tGameInfo.bRespawnGroupTeams);

	// Suicide or teamkill decreases score
	chkDecreaseScore->setValue(tLXOptions->tGameInfo.features[FT_SuicideDecreasesScore]);

	// Bonuses on
	chkBonusesOn->setValue(tLXOptions->tGameInfo.bBonusesOn);

	// Bonus spawn time
	txtBonusSpawnTime->setText(ftoa(tLXOptions->tGameInfo.fBonusFreq));

	// Show bonus names
	chkShowBonusNames->setValue(tLXOptions->tGameInfo.bShowBonusName);

	// Bonus life time
	txtBonusLifeTime->setText(ftoa(tLXOptions->tGameInfo.fBonusLife));

	// Health to weapon chance
	sldWeaponToHealthChance->setValue((int)(tLXOptions->tGameInfo.fBonusHealthToWeaponChance*100.0f));
	lblHealthChance->setText(itoa(100 - sldWeaponToHealthChance->getValue()) + " %");
	lblWeaponChance->setText(itoa(sldWeaponToHealthChance->getValue()) + " %");
}

///////////////////////
// Save everything to tGameInfo and tLXOptions
void CGameSettingsDialog::Save()
{
	// Default to no setting
	tLXOptions->tGameInfo.iLives = -2;
	tLXOptions->tGameInfo.iKillLimit = -1;
	tLXOptions->tGameInfo.fTimeLimit = -1;
	tLXOptions->tGameInfo.iTagLimit = -1;
	tLXOptions->tGameInfo.fRespawnTime = 2.5;
	tLXOptions->tGameInfo.bBonusesOn = true;
	tLXOptions->tGameInfo.bShowBonusName = true;

	//
	// General
	//

	// Lives
	if(txtLives->getText().size())
		tLXOptions->tGameInfo.iLives = atoi(txtLives->getText());

	// Max kills
	if(txtMaxKills->getText().size())
		tLXOptions->tGameInfo.iKillLimit = atoi(txtMaxKills->getText());

	// Loading time
	tLXOptions->tGameInfo.iLoadingTime = sldLoadingTime->getValue();

	// Time limit
	if(txtTimeLimit->getText().size())
		tLXOptions->tGameInfo.fTimeLimit = atof(txtTimeLimit->getText());

	// Respawn time
	if(txtRespawnTime->getText().size())
		tLXOptions->tGameInfo.fRespawnTime = atof(txtRespawnTime->getText());

	// Group teams
	tLXOptions->tGameInfo.bRespawnGroupTeams = chkGroupTeams->getValue();

	// Suicide and teamkill decrease score
	tLXOptions->tGameInfo.features[FT_SuicideDecreasesScore] = chkDecreaseScore->getValue();

	// Empty weapons on respawn
	tLXOptions->tGameInfo.bEmptyWeaponsOnRespawn = chkEmptyWeapons->getValue();

	//
	// Bonuses
	//

	// Bonuses on
	tLXOptions->tGameInfo.bBonusesOn = chkBonusesOn->getValue();	

	// Bonus spawn time
	if(txtBonusSpawnTime->getText().size())
		tLXOptions->tGameInfo.fBonusFreq = atof(txtBonusSpawnTime->getText());

	// Show bonus names
	tLXOptions->tGameInfo.bShowBonusName = chkShowBonusNames->getValue();

	// Bonus life time
	if(txtBonusLifeTime->getText().size())
		tLXOptions->tGameInfo.fBonusLife = atof(txtBonusLifeTime->getText());

	// Health to weapon chance
	tLXOptions->tGameInfo.fBonusHealthToWeaponChance = (float)sldWeaponToHealthChance->getValue() / 100.0f;
}

////////////////////////
// Set everything to default
void CGameSettingsDialog::Default()
{
	//
	// General
	//

	// Lives
	txtLives->setText("10");

	// Max kills
	txtMaxKills->setText("");

	// Loading time
	sldLoadingTime->setValue(100);

	// Time limit
	txtTimeLimit->setText("");

	// Respawn time
	txtRespawnTime->setText("2.5");

	// Respawn in waves
	chkWaveRespawn->setValue(false);

	// Group teams
	chkGroupTeams->setValue(false);

	// Suicide and teamkill decrease score
	chkDecreaseScore->setValue(false);

	// Empty weapons on respawn
	chkEmptyWeapons->setValue(false);

	//
	// Bonuses
	//

	// Bonuses on
	chkBonusesOn->setValue(true);	

	// Bonus spawn time
	txtBonusSpawnTime->setText("30");

	// Show bonus names
	chkShowBonusNames->setValue(true);

	// Bonus life time
	txtBonusLifeTime->setText("30");

	// Health to weapon chance
	sldWeaponToHealthChance->setValue(50);
}

/////////////////
// OK button click
void CGameSettingsDialog::OkClick(CButton *sender, MouseButton button, const ModifiersState &modstate)
{
	Save();
	Close();
}

/////////////////
// Default button click
void CGameSettingsDialog::DefaultClick(CButton *sender, MouseButton button, const ModifiersState &modstate)
{
	Default();
}

//
// Weapon options dialog
//

static const std::string states[] = {"Enabled", "Bonus", "Banned"};

CWeaponOptionsDialog::CWeaponOptionsDialog(COMMON_PARAMS, const std::string& mod) : 
CDialog(name, parent, "Weapon Options")
{
	// Create the widgets
	btnOk = new CButton("_Ok", this, "OK");
	btnRandom = new CButton("_Random", this, "Random");
	btnCycle = new CButton("_Cycle", this, "Reset");
	btnLoad = new CButton("_Load", this, "Load");
	btnSave = new CButton("_Save", this, "Save");
	lsvRestrictions = new CListview("_WeaponList", this);

	// Events
	SET_BTNCLICK(btnOk, &CWeaponOptionsDialog::OkClick);
	SET_BTNCLICK(btnRandom, &CWeaponOptionsDialog::RandomClick);
	SET_BTNCLICK(btnCycle, &CWeaponOptionsDialog::CycleClick);
	SET_BTNCLICK(btnLoad, &CWeaponOptionsDialog::LoadClick);
	SET_BTNCLICK(btnSave, &CWeaponOptionsDialog::SaveClick);
	SET_LSVITEMCLICK(lsvRestrictions, &CWeaponOptionsDialog::ItemClick);

	// Allocate gamescript & weapon restrictions
	cGameScript = new CGameScript();
	cRestrictionList = new CWpnRest();

	// Load the gamescript and apply the restrictions
	cRestrictionList->loadList("cfg/wpnrest.dat");
	if (cGameScript->Load(mod))
		cRestrictionList->updateList(cGameScript);

	UpdateListview();
}

////////////////
// Destructor
CWeaponOptionsDialog::~CWeaponOptionsDialog()
{
	delete cGameScript;
	delete cRestrictionList;
}

///////////////////////
// Updates the listview
void CWeaponOptionsDialog::UpdateListview()
{
	// Clear
	lsvRestrictions->Clear();

	// Get the list
	wpnrest_t *psWpn = cRestrictionList->getList();
	if (!psWpn)
		return;
	
	// Fill the listview
	for(int i=0; i < cRestrictionList->getNumWeapons(); i++) {
		if(cGameScript->weaponExists(psWpn[i].psLink->szName))  {
			lsvRestrictions->AddItem(psWpn[i].psLink->szName, i);
			lsvRestrictions->AddTextSubitem(psWpn[i].psLink->szName);
			lsvRestrictions->AddTextSubitem(states[CLAMP(psWpn[i].psLink->nState, 0, 2)]);
		}
	}
}

///////////////
// Confirm click
void CWeaponOptionsDialog::OkClick(CButton *sender, MouseButton button, const ModifiersState &modstate)
{
	Close();
}

/////////////////
// Cycle click
void CWeaponOptionsDialog::CycleClick(CButton *sender, MouseButton button, const ModifiersState &modstate)
{
	cRestrictionList->cycleVisible(cGameScript);
}

///////////////////
// Random click
void CWeaponOptionsDialog::RandomClick(CButton *sender, MouseButton button, const ModifiersState &modstate)
{
	cRestrictionList->randomizeVisible(cGameScript);
}

////////////////////
// Load click
void CWeaponOptionsDialog::LoadClick(CButton *sender, MouseButton button, const ModifiersState &modstate)
{
	dlgLoadRest = new CFileLoadDialog("_LoadDialog", this, "Load Preset", "cfg/presets", "wps");
	SET_FILEDLGCONFIRM(dlgLoadRest, &CWeaponOptionsDialog::LoadDialogConfirm);
}

//////////////////
// Save click
void CWeaponOptionsDialog::SaveClick(CButton *sender, MouseButton button, const ModifiersState &modstate)
{
	dlgSaveRest = new CFileSaveDialog("_SaveDialog", this, "Save Preset", "cfg/presets", "wps");
	SET_FILEDLGCONFIRM(dlgSaveRest, &CWeaponOptionsDialog::SaveDialogConfirm);
}

////////////////////
// Confirmed the load dialog
void CWeaponOptionsDialog::LoadDialogConfirm(CFileDialog *sender, const std::string &file)
{
	cRestrictionList->loadList(file);
	cRestrictionList->updateList(cGameScript);
	UpdateListview();
}

///////////////////////
// Confirmed the save dialog
void CWeaponOptionsDialog::SaveDialogConfirm(CFileDialog *sender, const std::string &file)
{
	cRestrictionList->saveList(file);
}

///////////////////////
// Clicked on an item in the list
void CWeaponOptionsDialog::ItemClick(CListview *sender, CListviewItem *item, int index)
{
	// Get the list
	wpnrest_t *psWpn = cRestrictionList->getList();
	if (!psWpn)
		return;

	// Check
	if (index < 0 || index >= cRestrictionList->getNumWeapons())
		return;

	// Change the state
	psWpn[index].nState++;
	psWpn[index].nState %= 3;

	// Update the listview
	CListviewSubitem *sub = item->getSubitem(1);
	if (sub)
		sub->setName(states[psWpn[index].nState]);
}

}; // namespace SkinnedGUI
