/*
Open Tracker License

Terms and Conditions

Copyright (c) 1991-2000, Be Incorporated. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice applies to all licensees
and shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF TITLE, MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
BE INCORPORATED BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Be Incorporated shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
this Software without prior written authorization from Be Incorporated.

Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered trademarks
of Be Incorporated in the United States and other countries. Other brand product
names are registered trademarks or trademarks of their respective holders.
All rights reserved.
*/

#ifndef	_POSE_H
#define _POSE_H

#include <Region.h>

#include "TextWidget.h"
#include "Model.h"
#include "Utilities.h"

namespace BPrivate {

class BPoseView;
class BTextWidget;

enum {
	B_NAME_WIDGET,
	B_ALL_WIDGETS
};


class BPose {
public:
	BPose(Model *adopt, BPoseView *, bool selected = false);
	virtual ~BPose();

	BTextWidget *AddWidget(BPoseView *, BColumn *);
	BTextWidget *AddWidget(BPoseView *, BColumn *, ModelNodeLazyOpener &opener);
	void RemoveWidget(BPoseView *, BColumn *);
	void SetLocation(BPoint);
	void MoveTo(BPoint, BPoseView *, bool inval = true);

	void Draw(BRect, BPoseView *, bool fullDraw = true, const BRegion * = 0,
		bool recalculateText = false);
	void Draw(BRect, BPoseView *, BView *drawView, bool fullDraw,
		const BRegion *, BPoint offset, bool selected, bool recalculateText = false);
	void DeselectWithoutErasingBackground(BRect rect, BPoseView *poseView);
		// special purpose draw call for deselecting over a textured
		// background

	void DrawBar(BPoint where,BView *view,icon_size kind);

	void DrawIcon(BPoint, BView *, icon_size, bool direct, bool drawUnselected = false);
	void DrawToggleSwitch(BRect, BPoseView *);
	void MouseUp(BPoint poseLoc, BPoseView *, BPoint where, int32 index);
	Model* TargetModel() const;
	Model* ResolvedModel() const;
	void Select(bool selected);
	bool IsSelected() const;
		// Rename to IsHighlighted

	BTextWidget *ActiveWidget() const;
	BTextWidget *WidgetFor(uint32 hashAttr, int32 *index = 0) const;
	BTextWidget *WidgetFor(BColumn *column, BPoseView *poseView, ModelNodeLazyOpener &opener,
		int32 *index = NULL);
		// adds the widget if needed

	bool PointInPose(BPoint poseLoc, const BPoseView *, BPoint where,
		BTextWidget ** = NULL) const;
	bool PointInPose(const BPoseView *, BPoint where) const ;
	BRect CalcRect(BPoint loc, const BPoseView *, bool minimal_rect = false);
	BRect CalcRect(const BPoseView *);
	void UpdateAllWidgets(int32 poseIndex, BPoint poseLoc, BPoseView *);
	void UpdateWidgetAndModel(Model *resolvedModel, const char *attrName,
		uint32 attrType, int32 poseIndex, BPoint poseLoc, BPoseView *view);
	bool UpdateVolumeSpaceBar(BVolume *volume);
	void UpdateIcon(BPoint poseLoc, BPoseView *);

	//void UpdateFixedSymlink(BPoint poseLoc, BPoseView *);	
	void UpdateBrokenSymLink(BPoint poseLoc, BPoseView *);	
	void UpdateWasBrokenSymlink(BPoint poseLoc, BPoseView *poseView);
		
	void Commit(bool saveChanges, BPoint loc, BPoseView *, int32 index);
	void EditFirstWidget(BPoint poseLoc, BPoseView *);
	void EditNextWidget(BPoseView *);
	void EditPreviousWidget(BPoseView *);

	BPoint Location() const;
	bool DelayedEdit() const;
	void SetDelayedEdit(bool delay);
	bool ListModeInited() const;
	bool HasLocation() const;
	bool NeedsSaveLocation() const;
	void SetSaveLocation();
	bool WasAutoPlaced() const;
	void SetAutoPlaced(bool);

	uint32 ClipboardMode() const;
	void SetClipboardMode(uint32 clipboardMode);

#if DEBUG
	void PrintToStream();
#endif

private:
	static bool _PeriodicUpdateCallback(BPose *pose, void *cookie);
	void EditPreviousNextWidgetCommon(BPoseView *poseView, bool next);
	void CreateWidgets(BPoseView *);
	bool TestLargeIconPixel(BPoint) const;
	
	Model *fModel;
	BObjectList<BTextWidget> fWidgetList;
	BPoint fLocation;

	uint32 fClipboardMode;
	int32 fPercent;
	
	bool fIsSelected : 1;
	bool fDelayedEdit : 1;
	bool fHasLocation : 1;
	bool fNeedsSaveLocation : 1;
	bool fListModeInited : 1;
	bool fWasAutoPlaced : 1;
	bool fBrokenSymLink : 1;
	bool fBackgroundClean : 1;
};

template<class Param1>
void 
EachTextWidget(BPose *pose, BPoseView *poseView,
	void (*func)(BTextWidget *, BPose *, BPoseView *, BColumn *, Param1), Param1 p1)
{
	for (int32 index = 0; ; index++) {
		BColumn *column = poseView->ColumnAt(index);
		if (!column)
			break;

		BTextWidget *widget = pose->WidgetFor(column->AttrHash());
		if (widget)
			(func)(widget, pose, poseView, column, p1);
	}
}

template<class Param1, class Param2>
void 
EachTextWidget(BPose *pose, BPoseView *poseView,
	void (*func)(BTextWidget *, BPose *, BPoseView *, BColumn *,
	Param1, Param2), Param1 p1, Param2 p2)
{
	for (int32 index = 0; ; index++) {
		BColumn *column = poseView->ColumnAt(index);
		if (!column)
			break;

		BTextWidget *widget = pose->WidgetFor(column->AttrHash());
		if (widget)
			(func)(widget, pose, poseView, column, p1, p2);
	}
}

template<class Result, class Param1, class Param2>
Result 
WhileEachTextWidget(BPose *pose, BPoseView *poseView,
	Result (*func)(BTextWidget *, BPose *, BPoseView *, BColumn *,
	Param1, Param2), Param1 p1, Param2 p2)
{
	for (int32 index = 0; ; index++) {
		BColumn *column = poseView->ColumnAt(index);
		if (!column)
			break;

		BTextWidget *widget = pose->WidgetFor(column->AttrHash());
		if (widget) {
			Result result = (func)(widget, pose, poseView, column, p1, p2);
			if (result)
				return result;
		}
	}
	return 0;
}

inline Model *
BPose::TargetModel() const
{
	return fModel;
}

inline Model *
BPose::ResolvedModel() const
{
	return fModel->IsSymLink() ?
		(fModel->LinkTo() ? fModel->LinkTo() : fModel) : fModel;
}


inline bool
BPose::IsSelected()	const
{
	return fIsSelected;
}

inline void
BPose::Select(bool on)
{
	fIsSelected = on;
}

inline bool
BPose::DelayedEdit() const
{
	return fDelayedEdit;
}

inline void
BPose::SetDelayedEdit(bool on)
{
	fDelayedEdit = on;
}

inline bool
BPose::NeedsSaveLocation() const
{
	return fNeedsSaveLocation;
}

inline void
BPose::SetSaveLocation()
{
	fNeedsSaveLocation = true;
}

inline bool
BPose::ListModeInited() const
{
	return fListModeInited;
}

inline bool
BPose::WasAutoPlaced() const
{
	return fWasAutoPlaced;
}

inline void
BPose::SetAutoPlaced(bool on)
{
	fWasAutoPlaced = on;
}

inline bool
BPose::HasLocation() const
{
	return fHasLocation;
}

inline BPoint
BPose::Location() const
{
	return fLocation;
}

inline void
BPose::SetLocation(BPoint point)
{
	fLocation = BPoint(floorf(point.x), floorf(point.y));
	fHasLocation = true;
}

inline void
BPose::Draw(BRect rect, BPoseView *view, bool fullDraw, const BRegion *updateRgn,
	bool recalculateText)
{
	Draw(rect, view, (BView *)view, fullDraw, updateRgn, BPoint(0, 0),
		IsSelected(), recalculateText);
}

inline uint32
BPose::ClipboardMode()	const
{
	return fClipboardMode;
}


inline void
BPose::SetClipboardMode(uint32 clipboardMode)
{
	fClipboardMode = clipboardMode;
}

} // namespace BPrivate

using namespace BPrivate;

#endif
