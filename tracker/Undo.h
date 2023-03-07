#ifndef _UNDO_H_
#define _UNDO_H_

#include "Tracker.h"
#include "Commands.h"
#include "BackgroundImage.h"
#include "FSContext.h"
#include <String.h>
#include <SupportDefs.h>

namespace BPrivate {

using namespace fs;
const uint32 kEditName = 'Ednm';

class UndoContext;
class UndoHistory;
extern UndoHistory gUndoHistory;

class UndoHistory {
public:
							UndoHistory();

		void				PrepareUndoContext(FSContext *context);
		void				CommitUndoContext(FSContext *context);
		void				AddItem(BMessage *message); // add an action without context
		void				AddItemToContext(BMessage *message, FSContext *context);
		void				SetSourceForContext(BDirectory &source, FSContext *context);
		void				SetTargetForContext(BDirectory &target, FSContext *context);

		int32				Undo(int32 count);
		int32				Redo(int32 count, BContainerWindow *source = NULL);

private:
		UndoContext			*FindContext(FSContext *context);
		UndoContext			*CurrentContext(int32 offsetby);
		void				AddContext(UndoContext *context);
		void				IgnoreContext(FSContext *context);
		void				CheckSize();

		BObjectList<UndoContext>	fList; // the list of undoable actions
		BObjectList<UndoContext>	fOpen; // the list of open contexts
		int32				fPosition;
		int32				fDepth;
};


class UndoContext {
public:
							UndoContext(FSContext *context, int32 items = 20);

		void				AddItem(BMessage *message) { fActions.AddItem(message);	};
		int32				CountActions() { return fActions.CountItems(); };
		void				RewindActions() { fPosition = 0; };
		BMessage			*NextAction() { return fActions.ItemAt(fPosition++); };

		FSContext			*Context() { return fContext; };
		void				SetSourceForContext(BDirectory &source);
		void				SetTargetForContext(BDirectory &target);
		const char			*SourceForContext() { return fMoveSource.String(); };
		const char			*TargetForContext() { return fMoveTarget.String(); };

		void				SetOverwrite(bool overwrite) { fOverwrite = overwrite; };
		bool				Overwrite() { return fOverwrite; };

private:
		BObjectList<BMessage>	fActions;
		int32				fPosition;
		FSContext			*fContext;
		BString				fMoveSource;
		BString				fMoveTarget;
		bool				fOverwrite;
};

} // namespace BPrivate

#endif