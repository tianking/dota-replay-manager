#include "core/app.h"
#include "graphics/imagelib.h"
#include "base/version.h"
#include "dota/colors.h"
#include "base/utils.h"
#include "base/mpqfile.h"
#include "dota/consts.h"
#include "frameui/dragdrop.h"
#include "ui/batchdlg.h"
#include "ui/searchres.h"
#include "ui/mainwnd.h"
#include "replay/cache.h"

#include "gameinfo.h"

class PlayerFinder : public BatchFunc
{
	String player;
	SearchResults* results;
public:
	PlayerFinder(String name);
	void run();
	void handle(String file, GameCache* gc);
};
PlayerFinder::PlayerFinder(String name)
{
	player = name;
	results = (SearchResults*) SendMessage(getApp()->getMainWindow(), WM_GETVIEW, MAINWND_SEARCHRES, 0);
	SendMessage(getApp()->getMainWindow(), WM_REBUILDTREE, 1, 0);
	SendMessage(getApp()->getMainWindow(), WM_PUSHVIEW,
		(uint32) new SearchResViewItem(), 0);
}
void PlayerFinder::run()
{
	BatchDialog* batch = new BatchDialog(BatchDialog::mFunc, this, "Searching");
	batch->addFolder(cfg.replayPath);
	results->doBatch(batch);
}

void PlayerFinder::handle(String file, GameCache* gameCache)
{
	for (int i = 0; i < gameCache->players; i++)
	{
		if (gameCache->pname[i].icompare(player) == 0)
		{
			results->addFile(file);
			break;
		}
	}
}

#define IDC_MAPIMAGE          100
#define IDC_WATCHREPLAY       101
#define IDC_COPYMATCHUP       102

#define INFO_VERSION      0
#define INFO_MAP          1
#define INFO_LOCATION     2
#define INFO_HOST         3
#define INFO_SAVER        4
#define INFO_LENGTH       5
#define INFO_RATIO        6
#define INFO_SCORE        7
#define INFO_WINNER       8
#define INFO_OBSERVERS    9
#define INFO_SAVED        10
#define INFO_NAME         11
#define INFO_MODE         12

#define PL_NAME           0
#define PL_LEVEL          1
#define PL_ITEM           2
#define PL_KILLS          3
#define PL_DEATHS         4
#define PL_ASSISTS        5
#define PL_CREEPS         6
#define PL_DENIES         7
#define PL_NEUTRALS       8
#define PL_LANE           9
#define PL_ITEMBUILD      10
#define PL_APM            11
#define PL_LEFT           12

#define PL_TEAM           1
#define PL_LAPM           2
#define PL_LLEFT          3

#define ID_PLAYER_SHOWBUILD         102
#define ID_PLAYER_SHOWACTIONS       103
#define ID_PLAYER_COPYNAME          104
#define ID_PLAYER_COPYSTATS         105
#define ID_PLAYER_FINDGAMES         106

static char infoText[][64] = {
	"Patch version",
	"Map name",
	"Map location",
	"Host name",
	"Replay saver",
	"Game length",
	"listPlayers",
	"Game score (may be wrong)",
	"Winner",
	"Observers",
	"Replay saved",
	"Game name",
	"Game mode"
};

ReplayGameInfoTab::ReplayGameInfoTab(Frame* parent)
	: ReplayTab(parent)
{
	popout = NULL;
	mapImages[0] = NULL;
	mapImages[1] = NULL;
	curImage = 0;
	listPlayers = new ListFrame(this);
	listPlayers->setColorMode(ListFrame::colorParam);
	listPlayers->setPoint(PT_BOTTOMLEFT, 10, -10);
	listPlayers->setPoint(PT_BOTTOMRIGHT, -10, -10);
	listPlayers->setHeight(250);
	listPlayers->show();

	info = new SimpleListFrame(this, 0, LVS_ALIGNLEFT | LVS_REPORT |
		LVS_NOCOLUMNHEADER | LVS_NOSCROLL | LVS_SINGLESEL | WS_DISABLED, WS_EX_STATICEDGE);
	info->setPoint(PT_TOPLEFT, 10, 10);
	info->setPoint(PT_TOPRIGHT, -200, 10);
	info->setPoint(PT_BOTTOM, listPlayers, PT_TOP, 0, -10);

	watchReplay = new ButtonFrame("Watch replay", this, IDC_WATCHREPLAY);
	watchReplay->setHeight(23);
	watchReplay->setPoint(PT_TOPLEFT, info, PT_TOPRIGHT, 6, 0);
	watchReplay->setPoint(PT_TOPRIGHT, -10, 10);

	copyMatchup = new ButtonFrame("Copy matchup", this, IDC_COPYMATCHUP);
	copyMatchup->setHeight(23);
	copyMatchup->setPoint(PT_TOPLEFT, watchReplay, PT_BOTTOMLEFT, 0, 5);
	copyMatchup->setPoint(PT_TOPRIGHT, watchReplay, PT_BOTTOMRIGHT, 0, 5);

	map = new StaticFrame(this, IDC_MAPIMAGE, SS_BITMAP | SS_CENTERIMAGE, WS_EX_CLIENTEDGE);
	map->setPoint(PT_TOP, copyMatchup, PT_BOTTOM, 0, 10);
	map->setSize(132, 132);

	mapCanvas = new Image(128, 128);
	mapCanvas->fill(Image::clr(0, 255, 0));
	HDC hDC = GetDC(map->getHandle());
	mapBitmap = mapCanvas->createBitmap(hDC);
	ReleaseDC(map->getHandle(), hDC);
	map->setImage(mapBitmap, IMAGE_BITMAP);

	info->setColumns(3);
	info->setColumn(0, 10, LVCFMT_LEFT);
	info->setColumn(1, 150, LVCFMT_RIGHT);
	info->setColumn(2, 150, LVCFMT_LEFT);
	info->show();

	ctxMenu = CreatePopupMenu();
	MENUITEMINFO miiSep;
	memset(&miiSep, 0, sizeof miiSep);
	miiSep.cbSize = sizeof miiSep;
	miiSep.fMask = MIIM_FTYPE;
	miiSep.fType = MFT_SEPARATOR;
	MENUITEMINFO mii;
	memset(&mii, 0, sizeof mii);
	mii.cbSize = sizeof mii;
	mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_STATE | MIIM_ID;
	mii.fType = MFT_STRING;
	mii.fState = MFS_DEFAULT;
	mii.dwTypeData = "Show build";
	mii.cch = strlen(mii.dwTypeData);
	mii.wID = ID_PLAYER_SHOWBUILD;
	InsertMenuItem(ctxMenu, 0, TRUE, &mii);
	mii.fMask &= ~MIIM_STATE;
	mii.dwTypeData = "Show actions";
	mii.cch = strlen(mii.dwTypeData);
	mii.wID = ID_PLAYER_SHOWACTIONS;
	InsertMenuItem(ctxMenu, 1, TRUE, &mii);
	InsertMenuItem(ctxMenu, 2, TRUE, &miiSep);
	mii.dwTypeData = "Copy name";
	mii.cch = strlen(mii.dwTypeData);
	mii.wID = ID_PLAYER_COPYNAME;
	InsertMenuItem(ctxMenu, 3, TRUE, &mii);
	mii.dwTypeData = "Copy stats";
	mii.cch = strlen(mii.dwTypeData);
	mii.wID = ID_PLAYER_COPYSTATS;
	InsertMenuItem(ctxMenu, 4, TRUE, &mii);
	mii.dwTypeData = "Find games";
	mii.cch = strlen(mii.dwTypeData);
	mii.wID = ID_PLAYER_FINDGAMES;
	InsertMenuItem(ctxMenu, 5, TRUE, &mii);
}
ReplayGameInfoTab::~ReplayGameInfoTab()
{
	DestroyMenu(ctxMenu);
	delete mapImages[0];
	delete mapImages[1];
	delete mapCanvas;
	if (mapBitmap)
		DeleteObject(mapBitmap);
	delete popout;
}

void ReplayGameInfoTab::addInfo(String name, String value, bool utf8)
{
	int pos = info->addItem("");
	info->setItemText(pos, 1, name);
	if (utf8)
	{
		//������һ��������������
		//info->setItemTextUtf8(pos, 2, ansi);
		String ansi(value);
		ansi.toAnsi();
		info->setItemText(pos, 2, ansi);
	}
	else
		info->setItemText(pos, 2, value);
}

String ReplayGameInfoTab::getWatchCmd()
{
	if (w3g == NULL)
	{
		watchReplay->setText("Watch replay");
		return "";
	}
	if (w3g->getFileInfo() == NULL)
	{
		watchReplay->setText("Unknown file location");
		return "";
	}
	if (cfg.warPath.isEmpty())
	{
		watchReplay->setText("Warcraft III not found");
		return "";
	}
	String exe = String::buildFullName(cfg.warPath, "War3.exe");
	FileInfo fi;
	if (!getFileInfo(exe, fi))
	{
		watchReplay->setText("Warcraft III not found");
		return "";
	}
	String cmd = String::format("\"%s\" -loadfile \"%s\"", exe, w3g->getFileInfo()->path);
	if (cfg.viewWindow)
		cmd += " -window";
	return cmd;
}

void ReplayGameInfoTab::onSetReplay()
{
	listPlayers->clearColumns();
	listPlayers->clear();
	info->clear();
	delete mapImages[0];
	delete mapImages[1];
	mapImages[0] = NULL;
	mapImages[1] = NULL;
	curImage = 0;
	mapCanvas->fill(Image::clr(0, 0, 0));
	HDC hDC = GetDC(map->getHandle());
	mapCanvas->fillBitmap(mapBitmap, hDC);
	ReleaseDC(map->getHandle(), hDC);
	map->invalidate();
	listPlayers->enable(w3g != NULL);
	watchReplay->enable(!getWatchCmd().isEmpty());
	copyMatchup->enable(w3g != NULL);
	if (w3g == NULL)
		return;

	MPQArchive* mapArchive = MPQArchive::open(String::buildFullName(
		cfg.warPath, w3g->getGameInfo()->map), MPQFILE_READ);
	if (mapArchive)
	{
		File* file = mapArchive->openFile("war3mapPreview.tga", MPQFILE_READ);
		if (file)
		{
			mapImages[0] = new Image(file);
			delete file;
		}
		file = mapArchive->openFile("war3mapMap.blp", MPQFILE_READ);
		if (file)
		{
			mapImages[1] = new Image(file);
			delete file;
			if (mapImages[1])
			{
				file = mapArchive->openFile("war3map.mmp", MPQFILE_READ);
				if (file)
				{
					addMapIcons(mapImages[1], file);
					delete file;
				}
			}
		}
		delete mapArchive;
	}
	if (mapImages[0] == NULL)
	{
		mapImages[0] = mapImages[1];
		mapImages[1] = NULL;
	}
	if (mapImages[0])
	{
		BLTInfo info(mapImages[0]);
		info.setDstSize(mapCanvas->width(), mapCanvas->height());
		mapCanvas->blt(info);
		HDC hDC = GetDC(map->getHandle());
		mapCanvas->fillBitmap(mapBitmap, hDC);
		ReleaseDC(map->getHandle(), hDC);
	}
	map->invalidate();

	//��Ϸreplay�ſ�
	if (w3g->getFileInfo())
		addInfo("Replay saved", format_systime(w3g->getFileInfo()->ftime, "%c"));
	addInfo("Warcraft version", formatVersion(w3g->getVersion()));
	addInfo("Map", String::getFileTitle(w3g->getGameInfo()->map));
	addInfo("Map location", String::getPath(w3g->getGameInfo()->map));
	addInfo("Game name", w3g->getGameInfo()->name, true);
	if (w3g->getDotaInfo())
		addInfo("Game mode", w3g->getGameInfo()->game_mode);
	addInfo("Host name", w3g->getGameInfo()->creator, true);
	if (W3GPlayer* saver = w3g->getGameInfo()->saver)
		addInfo("Replay saver", saver->name, true);
	addInfo("Game length", format_time(w3g->getLength()));
	info->setColumnWidth(2, LVSCW_AUTOSIZE);

	String observers = "";
	for (int i = 0; i < w3g->getNumPlayers(); i++)
	{
		W3GPlayer* player = w3g->getPlayer(i);
		if (player->slot.color > 11 || player->slot.slot_status == 0)
		{
			if (!observers.isEmpty()) observers += ", ";
			observers += player->name;
		}
	}
	if (!observers.isEmpty())
		addInfo("Observers", observers, true);

	LockWindowUpdate(listPlayers->getHandle());

	ImageLibrary* ilib = getApp()->getImageLibrary();

	if (DotaInfo const* dotaInfo = w3g->getDotaInfo())
	{
		//dota�ſ�
		addInfo("listPlayers", String::format("%dv%d", dotaInfo->team_size[0], dotaInfo->team_size[1]));
		addInfo("Score", String::format("%d/%d", dotaInfo->team_kills[0], dotaInfo->team_kills[1]));
		if (w3g->getGameInfo()->winner == WINNER_UNKNOWN)
			addInfo("Winner", "Unknown");
		else if (w3g->getGameInfo()->winner == WINNER_SENTINEL)
			addInfo("Winner", "Sentinel");
		else if (w3g->getGameInfo()->winner == WINNER_GSENTINEL || w3g->getGameInfo()->winner == WINNER_PSENTINEL)
		{
			addInfo("Winner", "Unknown");
			addInfo("Winner (guess)", "Sentinel");
		}
		else if (w3g->getGameInfo()->winner == WINNER_SCOURGE)
			addInfo("Winner", "Scourge");
		else if (w3g->getGameInfo()->winner == WINNER_GSCOURGE || w3g->getGameInfo()->winner == WINNER_PSCOURGE)
		{
			addInfo("Winner", "Unknown");
			addInfo("Winner (guess)", "Scourge");
		}

		//��ʼ������б�
		listPlayers->insertColumn(PL_NAME, "Name");
		//listPlayers->setColumnUTF8(PL_NAME, true);
		listPlayers->insertColumn(PL_LEVEL, "Level", LVCFMT_RIGHT);
		listPlayers->insertColumn(PL_ITEM, "Cost", LVCFMT_RIGHT);
		listPlayers->insertColumn(PL_KILLS, "Kills", LVCFMT_RIGHT);
		listPlayers->insertColumn(PL_DEATHS, "Deaths", LVCFMT_RIGHT);
		listPlayers->insertColumn(PL_ASSISTS, "Assists", LVCFMT_RIGHT);
		listPlayers->insertColumn(PL_CREEPS, "Creeps", LVCFMT_RIGHT);
		listPlayers->insertColumn(PL_DENIES, "Denies", LVCFMT_RIGHT);
		listPlayers->insertColumn(PL_NEUTRALS, "Neutrals", LVCFMT_RIGHT);
		listPlayers->insertColumn(PL_LANE, "Lane");
		listPlayers->insertColumn(PL_ITEMBUILD, "Items");
		listPlayers->insertColumn(PL_APM, "APM", LVCFMT_RIGHT);
		listPlayers->insertColumn(PL_LEFT, "Left", LVCFMT_RIGHT);

		//��ӽ������������
		if (dotaInfo->team_size[0])
		{
			listPlayers->addItem("Sentinel", ilib->getListIndex("RedBullet"), 0xFF000000);
			for (int i = 0; i < dotaInfo->team_size[0]; i++)
				addPlayer(dotaInfo->teams[0][i]);
		}
		if (dotaInfo->team_size[1])
		{
			listPlayers->addItem("Scourge", ilib->getListIndex("GreenBullet"), 0xFF000000);
			for (int i = 0; i < dotaInfo->team_size[1]; i++)
				addPlayer(dotaInfo->teams[1][i]);
		}

		//�Ͱ汾����ĳЩ��
		for (int i = 0; i <= PL_LEFT; i++)
		{
			if (dotaInfo->version < makeVersion(6, 53, 0) && (i == PL_ASSISTS || i == PL_NEUTRALS))
				listPlayers->setColumnWidth(i, 0);
			else
				listPlayers->setColumnWidth(i, cfg.giColWidth[i]);
		}
	}
	else
	{
		listPlayers->insertColumn(PL_NAME, "Name");
		listPlayers->setColumnUTF8(PL_NAME, true);
		listPlayers->insertColumn(PL_TEAM, "Team");
		listPlayers->insertColumn(PL_LAPM, "APM", LVCFMT_RIGHT);
		listPlayers->insertColumn(PL_LLEFT, "Left", LVCFMT_RIGHT);

		if (w3g->getGameInfo()->ladder_winner < 0)
			addInfo("Winner", "Unknown");
		else if (w3g->getGameInfo()->ladder_wplayer)
			addInfo("Winner", w3g->getGameInfo()->ladder_wplayer->name, true);
		else
			addInfo("Winner", String::format("Team %d", w3g->getGameInfo()->ladder_winner + 1));

		for (int i = 0; i < w3g->getNumPlayers(); i++)
			addLadderPlayer(w3g->getPlayer(i));

		for (int i = 0; i <= PL_LLEFT; i++)
			listPlayers->setColumnWidth(i, cfg.giLColWidth[i]);
	}

	LockWindowUpdate(NULL);
}

//���б���������
void ReplayGameInfoTab::addPlayer(W3GPlayer* player)
{
	ImageLibrary* ilib = getApp()->getImageLibrary();
	int i;
	if (player->hero && w3g->getLength())
	{
		char hex[256] = {0};
		int len = player->name.length();
		char* p = player->name.getBuffer();
		int written = 0;
		for(int j = 0; j < len; j++) {
			char c = *(p++);
			written += sprintf(hex + written, "%2X", c);
		}

		//ʹ��ANSI���������һ��������������
		String ansi(player->name);
		ansi.toAnsi();
		i = listPlayers->addItem(ansi, ilib->getListIndex(player->hero->hero->icon),
			(player->player_id << 24) | getLightColor(player->slot.color));
		listPlayers->setItemText(i, PL_LEVEL, String(player->level));		//�ȼ�
		listPlayers->setItemText(i, PL_ITEM, String(player->item_cost));	//���ѽ�Ǯ
		if (w3g->getDotaInfo()->endgame)
		{
			listPlayers->setItemText(i, PL_KILLS, String(player->stats[STAT_KILLS]));	//��ͷ��
			listPlayers->setItemText(i, PL_DEATHS, String(player->stats[STAT_DEATHS]));	//������
			listPlayers->setItemText(i, PL_CREEPS, String(player->stats[STAT_CREEPS])); //С����
			listPlayers->setItemText(i, PL_DENIES, String(player->stats[STAT_DENIES]));	//����
			if (w3g->getDotaInfo()->version >= makeVersion(6, 54, 0))
			{
				listPlayers->setItemText(i, PL_ASSISTS, String(player->stats[STAT_ASSISTS]));	//����
				listPlayers->setItemText(i, PL_NEUTRALS, String(player->stats[STAT_NEUTRALS]));	//Ұ��
			}
			listPlayers->setItemText(i, PL_LANE, getLaneName(player->lane));	//��·
		}

		//��Ʒ��
		String items = "";
		for (int it = 0; it < 6; it++)
		{
			if (player->inv.final[it])
				items.printf("$%d$", ilib->getListIndex(player->inv.final[it]->icon, "Unknown"));
			else if (cfg.showEmptySlots)
				items.printf("$%d$", ilib->getListIndex("EmptySlot"));
		}
		listPlayers->setItemText(i, PL_ITEMBUILD, items);
	}
	else
		i = listPlayers->addItem(player->name, ilib->getListIndex("Empty"),
		(player->player_id << 24) | getLightColor(player->slot.color));

	listPlayers->setItemText(i, PL_APM, String(player->apm()));				//APM
	if (player->time >= w3g->getLength() || player->player_id >= 0x80)	//�˳�ʱ��
		listPlayers->setItemText(i, PL_LEFT, "End");
	else
		listPlayers->setItemText(i, PL_LEFT, w3g->formatTime(player->time));
}

void ReplayGameInfoTab::addLadderPlayer(W3GPlayer* player)
{
	if (player->slot.color > 11 || player->slot.slot_status == 0)
		return;
	ImageLibrary* ilib = getApp()->getImageLibrary();
	int i = listPlayers->addItem(player->name,
		ilib->getListIndex(getRaceIcon(player->race)),
		(player->player_id << 24) | getLightColor(player->slot.color));
	listPlayers->setItemText(i, PL_TEAM, String::format("Team %d", player->slot.team + 1));
	listPlayers->setItemText(i, PL_LAPM, String(player->apm()));
	if (player->time >= w3g->getLength() || player->player_id >= 0x80)
		listPlayers->setItemText(i, PL_LLEFT, "End");
	else
		listPlayers->setItemText(i, PL_LLEFT, w3g->formatTime(player->time));
}

uint32 ReplayGameInfoTab::onMessage(uint32 message, uint32 wParam, uint32 lParam)
{
	if (message == WM_CONTEXTMENU && (HWND) wParam == listPlayers->getHandle())
	{
		int sel = ListView_GetNextItem(listPlayers->getHandle(), -1, LVNI_SELECTED);
		if (w3g && sel >= 0)
		{
			W3GPlayer* player = w3g->getPlayerById(listPlayers->getItemParam(sel) >> 24);
			if (player)
			{
				POINT pt;
				GetCursorPos(&pt);
				int result = TrackPopupMenuEx(ctxMenu, TPM_HORIZONTAL | TPM_LEFTALIGN |
					TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, listPlayers->getHandle(), NULL);
				switch (result)
				{
				case ID_PLAYER_SHOWBUILD:
					notify(WM_SETPLAYER, (uint32) player, 0);
					notify(WM_SETTAB, REPLAY_PLAYERINFO, 0);
					break;
				case ID_PLAYER_SHOWACTIONS:
					notify(WM_SETPLAYER, (uint32) player, 0);
					notify(WM_SETTAB, REPLAY_ACTIONS, 0);
					break;
				case ID_PLAYER_COPYNAME:
					SetClipboard(CF_UNICODETEXT, CreateGlobalText(player->name));
					break;
				case ID_PLAYER_COPYSTATS:
					if (w3g->getDotaInfo())
					{
						String text = String::format("%s (%s), level %d, build cost %d, KDA: %d-%d-%d, "
							"CS: %d/%d/%d, lane: %s, APM: %d, left: %s", player->name,
							player->hero ? player->hero->hero->name : "No Hero",
							player->level, player->item_cost, player->stats[STAT_KILLS],
							player->stats[STAT_DEATHS], player->stats[STAT_ASSISTS],
							player->stats[STAT_CREEPS], player->stats[STAT_DENIES],
							player->stats[STAT_NEUTRALS], getLaneName(player->lane),
							player->apm(), w3g->formatTime(player->time));
						SetClipboard(CF_UNICODETEXT, CreateGlobalText(text));
					}
					break;
				case ID_PLAYER_FINDGAMES:
					{
						PlayerFinder* finder = new PlayerFinder(player->name);
						finder->run();
					}
					break;
				}
			}
		}
	}
	else if (message == WM_NOTIFY)
	{
		NMHDR* hdr = (NMHDR*) lParam;
		if (hdr->code == HDN_ENDTRACK)
		{
			NMHEADER* header = (NMHEADER*) hdr;
			if (header->iItem >= 0 && header->iItem <= PL_LEFT &&
				header->pitem && header->pitem->mask & HDI_WIDTH)
			{
				if (w3g && w3g->getDotaInfo())
					cfg.giColWidth[header->iItem] = header->pitem->cxy;
				else if (header->iItem <= PL_LLEFT)
					cfg.giLColWidth[header->iItem] = header->pitem->cxy;
			}
		}
		else if (hdr->code == LVN_ITEMACTIVATE)
		{
			int sel = ListView_GetNextItem(listPlayers->getHandle(), -1, LVNI_SELECTED);
			if (w3g && sel >= 0)
			{
				int data = listPlayers->getItemParam(sel) >> 24;
				W3GPlayer* player = w3g->getPlayerById(data);
				if (player)
				{
					notify(WM_SETPLAYER, (uint32) player, 0);
					notify(WM_SETTAB, REPLAY_PLAYERINFO, 0);
				}
			}
		}
	}
	else if (message == WM_COMMAND)
	{
		switch (LOWORD(wParam))
		{
		case IDC_MAPIMAGE:
			if (HIWORD(wParam) == STN_CLICKED)
			{
				if (mapImages[curImage] && popout == NULL)
				{
					RootWindow::setCapture(this);
					RECT rc;
					GetClientRect(map->getHandle(), &rc);
					ClientToScreen(map->getHandle(), (POINT*) &rc.left);
					ClientToScreen(map->getHandle(), (POINT*) &rc.right);
					rc.left = (rc.left + rc.right - mapImages[curImage]->width()) / 2;
					rc.top = (rc.top + rc.bottom - mapImages[curImage]->height()) / 2;
					rc.right = rc.left + mapImages[curImage]->width();
					rc.bottom = rc.top + mapImages[curImage]->height();
					popout = new MapPopout(&rc, mapImages[curImage]);
					ShowWindow(popout->getHandle(), SW_SHOWNA);
				}
			}
			else if (HIWORD(wParam) == STN_DBLCLK)
			{
				if (mapImages[1 - curImage])
				{
					curImage = 1 - curImage;
					mapCanvas->fill(Image::clr(0, 0, 0));
					BLTInfo info(mapImages[curImage]);
					info.setDstSize(mapCanvas->width(), mapCanvas->height());
					mapCanvas->blt(info);
					HDC hDC = GetDC(map->getHandle());
					mapCanvas->fillBitmap(mapBitmap, hDC);
					ReleaseDC(map->getHandle(), hDC);
					map->invalidate();
				}
			}
			return 0;
		case IDC_WATCHREPLAY:
			{
				String cmd = getWatchCmd();
				if (!cmd.isEmpty())
				{
					STARTUPINFO info;
					GetStartupInfo(&info);
					PROCESS_INFORMATION pi;
					CreateProcess(NULL, cmd.getBuffer(), NULL, NULL, FALSE, 0, NULL, NULL, &info, &pi);
					CloseHandle(pi.hProcess);
					CloseHandle(pi.hThread);
				}
			}
			return 0;
		case IDC_COPYMATCHUP:
			if (w3g)
			{
				String result = "";
				int marked[12];
				memset(marked, 0, sizeof marked);
				for (int i = 0; i < 12; i++)
				{
					if (marked[i])
						continue;
					W3GPlayer* team = w3g->getPlayerInSlot(i);
					if (team)
					{
						if (!result.isEmpty())
							result += " -VS- ";
						bool first = true;
						for (int j = 0; j < 12; j++)
						{
							W3GPlayer* player = w3g->getPlayerInSlot(j);
							if (player && player->slot.team == team->slot.team)
							{
								marked[j] = 1;
								if (!first)
									result += ", ";
								first = false;
								result += player->name;
								if (w3g->getDotaInfo() && player->hero)
								{
									String abbr = getApp()->getDotaLibrary()->getHeroAbbreviation(player->hero->hero->point);
									if (abbr.isEmpty())
										result += String::format(" (%s)", player->hero->hero->shortName);
									else
										result += String::format(" (%s)", abbr);
								}
							}
						}
					}
				}
				if (!result.isEmpty())
					SetClipboard(CF_UNICODETEXT, CreateGlobalText(result));
			}
			return 0;
		}
	}
	else if (message == WM_LBUTTONUP)
	{
		if (popout)
		{
			delete popout;
			popout = NULL;
			ReleaseCapture();
			return 0;
		}
	}
	return M_UNHANDLED;
}

/////////////////////////////////////////////////

uint32 MapPopout::onWndMessage(uint32 message, uint32 wParam, uint32 lParam)
{
	if (message == WM_PAINT)
	{
		PAINTSTRUCT ps;
		RECT rc;
		GetClientRect(hWnd, &rc);

		HDC hDC = BeginPaint(hWnd, &ps);
		BitBlt(hDC, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
		EndPaint(hWnd, &ps);

		return 0;
	}
	return Window::onWndMessage(message, wParam, lParam);
}
MapPopout::MapPopout(RECT const* rc, Image* image)
{
	if (WNDCLASSEX* wcx = createclass("MapPopout"))
	{
		wcx->style = CS_DROPSHADOW;
		wcx->hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
		RegisterClassEx(wcx);
	}
	create(rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top, "",
		WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_TOPMOST, NULL);
	HDC hDC = GetDC(hWnd);
	dc = CreateCompatibleDC(hDC);
	bitmap = image->createBitmap(hDC);
	SelectObject(dc, bitmap);
	ReleaseDC(hWnd, hDC);
}
MapPopout::~MapPopout()
{
	DeleteDC(dc);
	DeleteObject(bitmap);
}
