#ifndef _SEQUENCE_OPTIONS_H_
#define _SEQUENCE_OPTIONS_H_

#include "Commands.h"
#include "IconTheme.h"

#include <Button.h>
#include <ListView.h>
#include <Window.h>

class SequenceWindow : public BWindow {

public:
					SequenceWindow(BRect frame);

	virtual	void	MessageReceived(BMessage *message);
	void			MoveCloseToMouse();

private:
	BListView		*fSequence;
	BButton			*fMoveUp;
	BButton			*fMoveDown;
	BButton			*fAccept;
};

#endif
