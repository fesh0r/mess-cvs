#include <assert.h>
#include "SmartListView.h"

/* Add ... to Items in ListView if needed */
static LPCTSTR MakeShortString(HDC hDC, LPCTSTR lpszLong, int nColumnLen, int nOffset)
{
    static const CHAR szThreeDots[]="...";
    static CHAR szShort[MAX_PATH];
    int nStringLen=lstrlen(lpszLong);
    int nAddLen;
    SIZE size;
    int i;

    GetTextExtentPoint32(hDC,lpszLong, nStringLen, &size);
    if(nStringLen==0 || size.cx + nOffset<=nColumnLen)
        return lpszLong;

    lstrcpy(szShort,lpszLong);
    GetTextExtentPoint32(hDC,szThreeDots,sizeof(szThreeDots),&size);
    nAddLen=size.cx;

    for( i = nStringLen-1; i > 0; i--)
    {
        szShort[i]=0;
        GetTextExtentPoint32(hDC, szShort,i, &size);
        if(size.cx + nOffset + nAddLen <= nColumnLen)
            break;
    }

    lstrcat(szShort,szThreeDots);

    return szShort;
}

static BOOL PickerHitTest(HWND hWnd)
{
	RECT            rect;
	POINTS          p;
	DWORD           res = GetMessagePos();
	LVHITTESTINFO   htInfo;

	memset(&htInfo,'\0', sizeof(LVHITTESTINFO));
	p = MAKEPOINTS(res);
	GetWindowRect(hWnd, &rect);
	htInfo.pt.x = p.x - rect.left;
	htInfo.pt.y = p.y - rect.top;
	ListView_HitTest(hWnd, &htInfo);

	return !(htInfo.flags & LVHT_NOWHERE);
}

static int GetNumColumns(struct SmartListView *pListView)
{
	int  nColumn = 0;
	int  i;
	HWND hwndHeader;
	int  *shown;

	shown = _alloca(pListView->pClass->nNumColumns * sizeof(int));
	pListView->pClass->pfnGetColumnInfo(pListView, shown, NULL, NULL);
	hwndHeader = GetDlgItem(pListView->hwndListView, 0);

	if ((nColumn = Header_GetItemCount(hwndHeader)) < 1) {
		nColumn = 0;
		for (i = 0; i < pListView->pClass->nNumColumns; i++ ) {
			if (shown[i])
				nColumn++;
		}
	}
	return nColumn;
}

struct SmartListView *SmartListView_Init(struct SmartListViewOptions *pOptions)
{
	HWND hwndListView;
	struct SmartListView *pListView;
	RECT rParent, rListView;

	assert(pOptions);
	assert(pOptions->pClass);
	assert(pOptions->hwndParent);
	
	hwndListView = GetDlgItem(pOptions->hwndParent, pOptions->nIDDlgItem);
	assert(hwndListView);

	pListView = malloc(sizeof(struct SmartListView));
	if (!pListView) {
		return NULL;
	}
	pListView->piRealColumns = malloc(pOptions->pClass->nNumColumns * sizeof(int));
	if (!pListView->piRealColumns) {
		free(pListView);
		return NULL;
	}

	pListView->hwndListView = hwndListView;
	pListView->pClass = pOptions->pClass;
	pListView->hwndParent = pOptions->hwndParent;
	pListView->nIDDlgItem = pOptions->nIDDlgItem;
	pListView->hBackground = pOptions->hBackground;
	pListView->hPALbg = pOptions->hPALbg;
	pListView->bmDesc = pOptions->bmDesc;
	pListView->bOldControl = pOptions->bOldControl;
	pListView->rgbListFontColor = pOptions->rgbListFontColor;
	pListView->nSortCondition = 0;
	pListView->nNumRows = 0;
	pListView->rowMapping = NULL;

	/* Create IconsList for ListView Control */
	if (pOptions->hSmall)
		ListView_SetImageList(hwndListView, pOptions->hSmall, LVSIL_SMALL);
	if (pOptions->hLarge)
		ListView_SetImageList(hwndListView, pOptions->hLarge, LVSIL_NORMAL);

	/* Do we automatically center on our parent? */
	if (pOptions->bCenterOnParent) {
		GetClientRect(pOptions->hwndParent, &rParent);

		rListView.left		= rParent.left		+ pOptions->nInsetPixels;
		rListView.top		= rParent.top		+ pOptions->nInsetPixels;
		rListView.right		= rParent.right		- pOptions->nInsetPixels;
		rListView.bottom	= rParent.bottom	- pOptions->nInsetPixels;

		SetWindowPos(hwndListView, HWND_TOP,
			rListView.left,
			rListView.top,
			rListView.right - rListView.left,
			rListView.bottom - rListView.top,
			SWP_DRAWFRAME);
	}

	SmartListView_ResetColumnDisplay(pListView);
	return pListView;
}

void SmartListView_Free(struct SmartListView *pListView)
{
	if (pListView->rowMapping)
		free(pListView->rowMapping);
	free(pListView->piRealColumns);
	free(pListView);
}

/* ------------------------------------------------------------------------ *
 * Liaison functions; for invoking callbacks                                *
 * ------------------------------------------------------------------------ */

static int SmartListView_LogicalRowToVisual(struct SmartListView *pListView, int nLogicalRow)
{
	assert(nLogicalRow >= 0);
	assert(nLogicalRow < pListView->nNumRows);
	return pListView->rowMapping[nLogicalRow].nLogicalToVisual;
}

static int SmartListView_VisualRowToLogical(struct SmartListView *pListView, int nVisualRow)
{
	assert(nVisualRow >= 0);
	assert(nVisualRow < pListView->nNumRows);
	return pListView->rowMapping[nVisualRow].nVisualToLogical;
}

static int SmartListView_VisualColumnToLogical(struct SmartListView *pListView, int nVisualColumn)
{
	assert(nVisualColumn >= 0);
	assert(nVisualColumn < pListView->pClass->nNumColumns);
	return pListView->piRealColumns[nVisualColumn];
}

static const char *SmartListView_GetText(struct SmartListView *pListView, int nLogicalRow, int nVisualColumn)
{
	int nLogicalColumn;
	const char *s = NULL;

	if (pListView->pClass->pfnGetText) {
		nLogicalColumn = SmartListView_VisualColumnToLogical(pListView, nVisualColumn);
		s = pListView->pClass->pfnGetText(pListView, nLogicalRow, nLogicalColumn);
	}
	return s ? s : "";
}

static BOOL SmartListView_IsItemSelected(struct SmartListView *pListView, int nVisualRow)
{
	int nLogicalRow;
	nLogicalRow = SmartListView_VisualRowToLogical(pListView, nVisualRow);
	return pListView->pClass->pfnIsItemSelected(pListView, nLogicalRow);
}

static BOOL SmartListView_ItemChanged(struct SmartListView *pListView, NM_LISTVIEW *lpNmHdr)
{
	BOOL bWasSelected, bNowSelected;
	int nLogicalRow;

	if (!pListView->pClass->pfnItemChanged)
		return FALSE;

	bWasSelected = (lpNmHdr->uOldState & LVIS_SELECTED) ? TRUE : FALSE;
	bNowSelected = (lpNmHdr->uNewState & LVIS_SELECTED) ? TRUE : FALSE;
	nLogicalRow = lpNmHdr->lParam;

	return pListView->pClass->pfnItemChanged(pListView, bWasSelected, bNowSelected, nLogicalRow);
}

static int SmartListView_WhichIcon(struct SmartListView *pListView, int nLogicalRow)
{
	int nIcon;

	if (pListView->pClass->pfnWhichIcon) {
		nIcon = pListView->pClass->pfnWhichIcon(pListView, nLogicalRow);
	}
	else {
		nIcon = 0;
	}
	return nIcon;
}

/* ------------------------------------------------------------------------ *
 * Event Handling                                                           *
 * ------------------------------------------------------------------------ */

BOOL SmartListView_IsEvent(struct SmartListView *pListView, UINT message, UINT wParam, LONG lParam)
{
	BOOL bIsEvent = FALSE;
	LPNMHDR lpNmHdr;
	LPDRAWITEMSTRUCT lpDis;

	switch(message) {
    case WM_NOTIFY:
		lpNmHdr = (LPNMHDR)lParam;
		bIsEvent = lpNmHdr->hwndFrom == pListView->hwndListView;
		break;

	case WM_DRAWITEM:
		lpDis = (LPDRAWITEMSTRUCT)lParam;
		bIsEvent = lpDis->CtlID == (UINT)pListView->nIDDlgItem;
		break;
	}

	return bIsEvent;
}

static BOOL SmartListView_HandleNotify(struct SmartListView *pListView, LPNMHDR lpNmHdr)
{
	BOOL bReturn = FALSE;
	LV_DISPINFO *pLvDispinfo;
	NM_LISTVIEW *pNmListview;
	int nItem, nColumn;

	switch (lpNmHdr->code) {
	case NM_RCLICK:
	case NM_CLICK:
		/* don't allow selection of blank spaces in the listview */
		bReturn = !PickerHitTest(pListView->hwndListView);
		break;

	case NM_DBLCLK:
		if (PickerHitTest(pListView->hwndListView)) {
			if (pListView->pClass->pfnRun)
				pListView->pClass->pfnRun(pListView);
		}
		bReturn = TRUE;
		break;

	case LVN_GETDISPINFO:
		pLvDispinfo = (LV_DISPINFO *) lpNmHdr;
		nItem = pLvDispinfo->item.lParam;

		if (pLvDispinfo->item.mask & LVIF_IMAGE)
			pLvDispinfo->item.iImage = SmartListView_WhichIcon(pListView, nItem);

		if (pLvDispinfo->item.mask & LVIF_STATE)
			pLvDispinfo->item.state = 0;

		if (pLvDispinfo->item.mask & LVIF_TEXT) {
			pLvDispinfo->item.pszText = (char *) SmartListView_GetText(pListView, nItem, pLvDispinfo->item.iSubItem);
		}
		bReturn = TRUE;
		break;

	case LVN_ITEMCHANGED:
        pNmListview = (NM_LISTVIEW *) lpNmHdr;
		bReturn = SmartListView_ItemChanged(pListView, pNmListview);
		break;

    case LVN_COLUMNCLICK:
        pNmListview = (NM_LISTVIEW *) lpNmHdr;
		nColumn = SmartListView_VisualColumnToLogical(pListView, pNmListview->iSubItem);
		SmartListView_ToggleSorting(pListView, nColumn);
		bReturn = TRUE;
		break;
	}
	return bReturn;
}

static void SmartListView_HandleDrawItem(struct SmartListView *pListView, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    HDC         hDC = lpDrawItemStruct->hDC;
    RECT        rcItem = lpDrawItemStruct->rcItem;
    UINT        uiFlags = ILD_TRANSPARENT;
    HIMAGELIST  hImageList;
    int         nVisualItem = lpDrawItemStruct->itemID;
    COLORREF    clrTextSave, clrBkSave;
    COLORREF    clrImage = GetSysColor(COLOR_WINDOW);
    static CHAR szBuff[MAX_PATH];
    BOOL        bFocus = (GetFocus() == pListView->hwndListView);
    LPCTSTR     pszText;
    UINT        nStateImageMask;
    BOOL        bSelected;
    LV_COLUMN   lvc;
    LV_ITEM     lvi;
    RECT        rcAllLabels;
    RECT        rcLabel;
    RECT        rcIcon;
    int         offset;
    SIZE        size;
    int         i, j;
    int         nColumn;
    int         nColumnMax = 0;
    int         nResults = 0;
    int         *order;
	int nItemCount;
	HBITMAP hBackground;

	order = _alloca(pListView->pClass->nNumColumns * sizeof(int));

	nColumnMax = GetNumColumns(pListView);

	if (pListView->bOldControl) {
		pListView->pClass->pfnGetColumnInfo(pListView, NULL, order, NULL);
	}
	else {
		/* Get the Column Order and save it */
		ListView_GetColumnOrderArray(pListView->hwndListView, nColumnMax, order);

		/* Disallow moving column 0 */
		if (order[0] != 0) {
			for (i = 0; i < nColumnMax; i++) {
				if (order[i] == 0) {
					order[i] = order[0];
					order[0] = 0;
				}
			}
			ListView_SetColumnOrderArray(pListView->hwndListView, nColumnMax, order);
		}
	}

    // Labels are offset by a certain amount  
    // This offset is related to the width of a space character
    GetTextExtentPoint32(hDC, " ", 1 , &size);
    offset = size.cx * 2;

    lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvi.iItem = nVisualItem;
    lvi.iSubItem = order[0];
    lvi.pszText = szBuff;
    lvi.cchTextMax = sizeof(szBuff);
    lvi.stateMask = 0xFFFF;       // get all state flags
    ListView_GetItem(pListView->hwndListView, &lvi);

    // This makes NO sense, but doesn't work without it?
    strcpy(szBuff, lvi.pszText);

    bSelected = ((bFocus) || (GetWindowLong(pListView->hwndListView, GWL_STYLE) & LVS_SHOWSELALWAYS))
        && SmartListView_IsItemSelected(pListView, nVisualItem);

    ListView_GetItemRect(pListView->hwndListView, nVisualItem, &rcAllLabels, LVIR_BOUNDS);

    ListView_GetItemRect(pListView->hwndListView, nVisualItem, &rcLabel, LVIR_LABEL);
    rcAllLabels.left = rcLabel.left;

	nItemCount = ListView_GetItemCount(pListView->hwndListView);

	hBackground = pListView->hBackground;

    if (hBackground) {
        RECT        rcClient;
        HRGN        rgnBitmap;
        RECT        rcTmpBmp = rcItem;
        RECT        rcFirstItem;
        HPALETTE    hPAL;
        HDC         htempDC;
        HBITMAP     oldBitmap;

        htempDC = CreateCompatibleDC(hDC);

        oldBitmap = SelectObject(htempDC, hBackground);

        GetClientRect(pListView->hwndListView, &rcClient); 
        rcTmpBmp.right = rcClient.right;
        // We also need to check whether it is the last item
        // The update region has to be extended to the bottom if it is
        if ((nItemCount == 0) || (nVisualItem == nItemCount - 1))
            rcTmpBmp.bottom = rcClient.bottom;
        
        rgnBitmap = CreateRectRgnIndirect(&rcTmpBmp);
        SelectClipRgn(hDC, rgnBitmap);
        DeleteObject(rgnBitmap);

        hPAL = (!pListView->hPALbg) ? CreateHalftonePalette(hDC) : pListView->hPALbg;

        if(GetDeviceCaps(htempDC, RASTERCAPS) & RC_PALETTE && hPAL != NULL )
        {
            SelectPalette(htempDC, hPAL, FALSE );
            RealizePalette(htempDC);
        }
        
        ListView_GetItemRect(pListView->hwndListView, 0, &rcFirstItem, LVIR_BOUNDS);
        
        for( i = rcFirstItem.left; i < rcClient.right; i += pListView->bmDesc.bmWidth )
            for( j = rcFirstItem.top; j < rcClient.bottom; j += pListView->bmDesc.bmHeight )
                BitBlt(hDC, i, j, pListView->bmDesc.bmWidth, pListView->bmDesc.bmHeight, htempDC, 0, 0, SRCCOPY );

		SelectObject(htempDC, oldBitmap);
        DeleteDC(htempDC);

        if (!pListView->bmDesc.bmColors)
        {
            DeleteObject(hPAL);
            hPAL = 0;
        }
    }

    SetTextColor(hDC, pListView->rgbListFontColor);

    if(bSelected)
    {
        HBRUSH hBrush;
        HBRUSH hOldBrush;

        if (bFocus)
        {
            clrTextSave = SetTextColor(hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
            clrBkSave = SetBkColor(hDC, GetSysColor(COLOR_HIGHLIGHT));
            hBrush = CreateSolidBrush(GetSysColor(COLOR_HIGHLIGHT));
        }
        else
        {
            clrTextSave = SetTextColor(hDC, GetSysColor(COLOR_BTNTEXT));
            clrBkSave = SetBkColor(hDC, GetSysColor(COLOR_BTNFACE));
            hBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        }

        hOldBrush = SelectObject(hDC, hBrush);
        FillRect(hDC, &rcAllLabels, hBrush);
        SelectObject(hDC, hOldBrush);
        DeleteObject(hBrush);
    }
    else if (!hBackground)
    {
        HBRUSH hBrush;

        hBrush = CreateSolidBrush( GetSysColor(COLOR_WINDOW));
        FillRect(hDC, &rcAllLabels, hBrush);
        DeleteObject(hBrush);
    }

    if (nItemCount == 0)
        return;

    if(lvi.state & LVIS_CUT)
    {
        clrImage = GetSysColor(COLOR_WINDOW);
        uiFlags |= ILD_BLEND50;
    }
    else if(bSelected)
    {
        if (bFocus)
            clrImage = GetSysColor(COLOR_HIGHLIGHT);
        else
            clrImage = GetSysColor(COLOR_BTNFACE);

        uiFlags |= ILD_BLEND50;
    }

    nStateImageMask = lvi.state & LVIS_STATEIMAGEMASK;

    if(nStateImageMask)
    {
        int nImage = (nStateImageMask >> 12) - 1;
        hImageList = ListView_GetImageList(pListView->hwndListView, LVSIL_STATE);
        if(hImageList)
            ImageList_Draw(hImageList, nImage, hDC, rcItem.left, rcItem.top, ILD_TRANSPARENT);
    }

    ListView_GetItemRect(pListView->hwndListView, nVisualItem, &rcIcon, LVIR_ICON);

    hImageList = ListView_GetImageList(pListView->hwndListView, LVSIL_SMALL);
    if(hImageList)
    {
        UINT nOvlImageMask = lvi.state & LVIS_OVERLAYMASK;
        if(rcItem.left < rcItem.right-1) {
			if (!hBackground) {
				HBRUSH hBrush;
				hBrush = CreateSolidBrush( GetSysColor(COLOR_WINDOW));
				FillRect(hDC, &rcIcon, hBrush);
				DeleteObject(hBrush);
			}
            ImageList_DrawEx(hImageList, lvi.iImage, hDC, rcIcon.left, rcIcon.top,
                16, 16, GetSysColor(COLOR_WINDOW), clrImage, uiFlags | nOvlImageMask);
		}
    }

    ListView_GetItemRect(pListView->hwndListView, nVisualItem, &rcItem,LVIR_LABEL);

    pszText = MakeShortString(hDC, szBuff,rcItem.right - rcItem.left,  2 * offset);

    rcLabel = rcItem;
    rcLabel.left += offset;
    rcLabel.right -= offset;

    DrawText(hDC, pszText,-1, &rcLabel,
        DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER);

    for(nColumn = 1; nColumn < nColumnMax ; nColumn++)
    {
        int nRetLen;
        UINT nJustify;
        LV_ITEM lvi;
        
        lvc.mask = LVCF_FMT | LVCF_WIDTH;
        ListView_GetColumn(pListView->hwndListView, order[nColumn] , &lvc);

        lvi.mask = LVIF_TEXT;
        lvi.iItem = nVisualItem; 
        lvi.iSubItem = order[nColumn];
        lvi.pszText = szBuff;
        lvi.cchTextMax = sizeof(szBuff);

        if (ListView_GetItem(pListView->hwndListView, &lvi) == FALSE)
            continue;

        /* This shouldn't oughtta be, but it's needed!!! */
        strcpy(szBuff, lvi.pszText);

        rcItem.left = rcItem.right;
        rcItem.right += lvc.cx;

        nRetLen = strlen(szBuff);
        if(nRetLen == 0)
            continue;

        pszText = MakeShortString(hDC, szBuff, rcItem.right - rcItem.left, 2 * offset);

        nJustify = DT_LEFT;

        if(pszText == szBuff)
        {
            switch(lvc.fmt & LVCFMT_JUSTIFYMASK)
            {
            case LVCFMT_RIGHT:
                nJustify = DT_RIGHT;
                break;
            case LVCFMT_CENTER:
                nJustify = DT_CENTER;
                break;
            default:
                break;
            }
        }

        rcLabel = rcItem;
        rcLabel.left += offset;
        rcLabel.right -= offset;
        DrawText(hDC, pszText, -1, &rcLabel,
            nJustify | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER);
    }

    if(lvi.state & LVIS_FOCUSED && bFocus)
        DrawFocusRect(hDC, &rcAllLabels);

    if(bSelected)
    {
        SetTextColor(hDC, clrTextSave);
        SetBkColor(hDC, clrBkSave);
    }
}


BOOL SmartListView_HandleEvent(struct SmartListView *pListView, UINT message, UINT wParam, LONG lParam)
{
	BOOL bReturn = FALSE;
	LPNMHDR lpNmHdr;
	LPDRAWITEMSTRUCT lpDis;

	assert(SmartListView_IsEvent(pListView, message, wParam, lParam));

	switch(message) {
    case WM_NOTIFY:
		lpNmHdr = (LPNMHDR)lParam;
		bReturn = SmartListView_HandleNotify(pListView, lpNmHdr);
		break;

	case WM_DRAWITEM:
		lpDis = (LPDRAWITEMSTRUCT)lParam;
		SmartListView_HandleDrawItem(pListView, lpDis);
		bReturn = TRUE;
		break;
	}
	return bReturn;
}

/* ------------------------------------------------------------------------ */

void SmartListView_ResetColumnDisplay(struct SmartListView *pListView)
{
	LV_COLUMN lvc;
	int *shown;
	int *order;
	int *widths;
	int i;
	int nColumn;
	int nNumColumns;

	shown = (int *) _alloca(pListView->pClass->nNumColumns * sizeof(*shown));
	order = (int *) _alloca(pListView->pClass->nNumColumns * sizeof(*order));
	widths = (int *) _alloca(pListView->pClass->nNumColumns * sizeof(*widths));

	pListView->pClass->pfnGetColumnInfo(pListView, shown, order, widths);

	ListView_SetExtendedListViewStyle(pListView->hwndListView, LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP);

	nColumn = 0;
	nNumColumns = pListView->pClass->nNumColumns;

	for (i = 0; i < nNumColumns; i++) {
		if (shown[order[i]]) {
			lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_TEXT;
			lvc.fmt = LVCFMT_LEFT; 
			lvc.pszText = (char *) pListView->pClass->ppColumnNames[order[i]];
			lvc.iSubItem = nColumn;
			lvc.cx = widths[order[i]]; 
			ListView_InsertColumn(pListView->hwndListView, nColumn, &lvc);
			pListView->piRealColumns[nColumn] = order[i];
			nColumn++;
		}
	}

	// Fill this in so we can still sort on columns NOT shown
	for (i = 0; i < nNumColumns && nColumn < nNumColumns; i++) {
		if (!shown[order[i]]) {
			pListView->piRealColumns[nColumn] = order[i];
			nColumn++;
		}
	}

	ListView_SetTextColor(pListView->hwndListView, pListView->rgbListFontColor);
}

void SmartListView_InsertItem(struct SmartListView *pListView, int nItem)
{
	/* TODO: How does this work with mapped rows? */
	LV_ITEM lvi;
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM; 
	lvi.stateMask = 0;
	lvi.iItem = nItem;
	lvi.iSubItem = 0; 
	lvi.lParam = nItem;
	lvi.pszText  = LPSTR_TEXTCALLBACK;
	lvi.iImage   = I_IMAGECALLBACK;
	ListView_InsertItem(pListView->hwndListView, &lvi);
}

BOOL SmartListView_SetTotalItems(struct SmartListView *pListView, int nItemCount)
{
	int i;
	struct RowMapping *newRowMapping;

	assert(nItemCount >= 0);

	/* Create a new map */
	if (nItemCount > 0) {
		newRowMapping = (struct RowMapping *) malloc(nItemCount * sizeof(struct RowMapping));
		if (!newRowMapping)
			return FALSE;
		for (i = 0; i < nItemCount; i++) {
			newRowMapping[i].nVisualToLogical = i;
			newRowMapping[i].nLogicalToVisual = i;
		}
	}
	else {
		newRowMapping = NULL;
	}
	if (pListView->rowMapping)
		free(pListView->rowMapping);
	pListView->rowMapping = newRowMapping;
	
	ListView_DeleteAllItems(pListView->hwndListView);
	ListView_SetItemCount(pListView->hwndListView, nItemCount);

	for (i = 0; i < nItemCount; i++) {
		SmartListView_InsertItem(pListView, i);
	}

	pListView->nNumRows = nItemCount;

	return TRUE;
}

void SmartListView_SelectItem(struct SmartListView *pListView, int nItem, BOOL bFocus)
{
	int nVisualRow;
	nVisualRow = SmartListView_LogicalRowToVisual(pListView, nItem);

	if (bFocus) {
		ListView_SetItemState(pListView->hwndListView, nVisualRow, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
		ListView_EnsureVisible(pListView->hwndListView, nVisualRow, FALSE);
	}
	else {
		ListView_SetItemState(pListView->hwndListView, nVisualRow, LVIS_SELECTED, LVIS_SELECTED);
	}
}

void SmartListView_Update(struct SmartListView *pListView, int nItem)
{
	int nVisualRow;
	nVisualRow = SmartListView_LogicalRowToVisual(pListView, nItem);

	ListView_Update(pListView->hwndListView, nVisualRow);
}

void SmartListView_RedrawItem(struct SmartListView *pListView, int nItem)
{
	int nVisualRow;
	nVisualRow = SmartListView_LogicalRowToVisual(pListView, nItem);
	ListView_RedrawItems(pListView->hwndListView, nVisualRow, nVisualRow);
}

/* ------------------------------------------------------------------------ *
 * Sorting                                                                  *
 * ------------------------------------------------------------------------ */

struct sort_Info {
	struct SmartListView *pListView;
	int nColumn;
	BOOL bReverse;
};

static int CALLBACK sort_Compare(LPARAM nLogicalRow1, LPARAM nLogicalRow2, int nParam)
{
	struct sort_Info *pSortInfo;
	struct SmartListView *pListView;
	int nColumn;
	BOOL bReverse;
	int nResult;

	pSortInfo = (struct sort_Info *) nParam;
	pListView = pSortInfo->pListView;
	nColumn = pSortInfo->nColumn;
	bReverse = pSortInfo->bReverse;

	nResult = pListView->pClass->pfnCompare(pListView, nLogicalRow1, nLogicalRow2, nColumn); 
	return bReverse ? -nResult : nResult;
}

void SmartListView_SetSorting(struct SmartListView *pListView, int nColumn, BOOL bReverse)
{
	struct sort_Info si;
	int i;
	LVITEM lvi;

	memset(&lvi, 0, sizeof(lvi));
	lvi.mask = LVIF_PARAM;

	si.pListView = pListView;
	si.nColumn = nColumn;
	si.bReverse = bReverse;

	ListView_SortItems(pListView->hwndListView, sort_Compare, &si);

	/* Update sort condition */
	pListView->nSortCondition = (nColumn + 1) * (bReverse ? -1 : 1);

	/* Update logical -> visual row mappings */
	for (i = 0; i < pListView->nNumRows; i++) {
		lvi.iItem = i;
		ListView_GetItem(pListView->hwndListView, &lvi);

		pListView->rowMapping[i].nVisualToLogical = lvi.lParam;
		pListView->rowMapping[lvi.lParam].nLogicalToVisual = i;
	}
}

void SmartListView_GetSorting(struct SmartListView *pListView, int *nColumn, BOOL *bReverse)
{
	if (pListView->nSortCondition >= 0) {
		*nColumn = pListView->nSortCondition - 1;
		*bReverse = FALSE;
	}
	else {
		*nColumn = -pListView->nSortCondition + 1;
		*bReverse = TRUE;
	}
}

void SmartListView_ToggleSorting(struct SmartListView *pListView, int nColumn)
{
	int nCurrentSorting;
	BOOL bCurrentReverse;

	SmartListView_GetSorting(pListView, &nCurrentSorting, &bCurrentReverse);
	SmartListView_SetSorting(pListView, nColumn, (nCurrentSorting == nColumn) && !bCurrentReverse);
}

/* ------------------------------------------------------------------------ *
 * Idling                                                                   *
 * ------------------------------------------------------------------------ */

BOOL SmartListView_CanIdle(struct SmartListView *pListView)
{
	if (pListView->pClass->pfnCanIdle)
		return pListView->pClass->pfnCanIdle(pListView);
	return FALSE;
}

void SmartListView_Idle(struct SmartListView *pListView)
{
	if (pListView->pClass->pfnIdle)
		pListView->pClass->pfnIdle(pListView);
}

void SmartListView_IdleUntilMsg(struct SmartListView *pListView)
{
	MSG msg;
	while(SmartListView_CanIdle(pListView) && !PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		SmartListView_Idle(pListView);
}

