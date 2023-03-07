#include <fs_attr.h>
#include "LanguageTheme.h"
#include "ObjectList.h"
#include "TFSContext.h"
#include "ThreadMagic.h"
#include "Undo.h"

namespace BPrivate {

static UndoHistory gUndoHistory;

UndoHistory::UndoHistory()
	:	fList(20),
		fOpen(10),
		fPosition(0),
		fDepth(0)
{
}

void
UndoHistory::PrepareUndoContext(FSContext *context)
{
	UndoContext *newContext = new UndoContext(context);
	fOpen.AddItem(newContext);
}

void
UndoHistory::CommitUndoContext(FSContext *context)
{
	UndoContext *ourContext = FindContext(context);
	
	if (ourContext) {
		fOpen.RemoveItem(ourContext);
		
		if (ourContext->CountActions() > 0)
			AddContext(ourContext);
		else
			delete ourContext;
	}
}

void
UndoHistory::AddItem(BMessage *message)
{
	UndoContext *newContext = new UndoContext(NULL, 1);
	newContext->AddItem(message);
	AddContext(newContext);
}

void
UndoHistory::AddItemToContext(BMessage *message, FSContext *context)
{
	UndoContext *ourContext = FindContext(context);
	if (ourContext)
		ourContext->AddItem(message);
}

void
UndoHistory::AddContext(UndoContext *context)
{
	if (fList.CountItems() > fPosition + 1) {
		UndoContext *oldContext = NULL;
		oldContext = fList.SwapWithItem(fPosition, context);
		
		if (oldContext)
			delete oldContext;
		
		fList.ItemAt(fPosition + 1)->SetOverwrite(true);
	} else
		fList.AddItem(context, fPosition);
	
	fPosition++;
	CheckSize();
}

void
UndoHistory::SetSourceForContext(BDirectory &source, FSContext *context)
{
	UndoContext *ourContext = FindContext(context);
	if (ourContext != NULL)
		ourContext->SetSourceForContext(source);
}

void
UndoHistory::SetTargetForContext(BDirectory &target, FSContext *context)
{
	UndoContext *ourContext = FindContext(context);
	if (ourContext != NULL)
		ourContext->SetTargetForContext(target);
}

int32
UndoHistory::Undo(int32 count)
{
	if (count <= 0)
		return 0;
	
	TFSContext *tfscontext;
	BMessage *message = NULL;
	UndoContext *context = NULL;
	int32 result;
	
	for (result = 0; result < count; result++) {
		context = CurrentContext(-1);
		if (context == NULL)
			break;
		
		context->RewindActions();
		
		BObjectList<entry_ref> trash_list(20);
		BObjectList<entry_ref> move_list(20);
		BObjectList<entry_ref> restore_list(20);
		
		while ((message = context->NextAction()) != NULL) {
			entry_ref *new_ref = new entry_ref;
			if (message->FindRef("new_ref", new_ref) != B_OK)
				continue;
			
			BEntry entry(new_ref);
			if (entry.InitCheck() != B_OK || !entry.Exists())
				continue;
					
			switch (message->what) {
				case kNewFolder:
				case kCreateLink:
				case kCreateRelativeLink:
				case kCopySelectionTo:
				case kRestoreFromTrash:
					trash_list.AddItem(new_ref);
					break;
				case kMoveSelectionTo:
					move_list.AddItem(new_ref);
					break;
				case kDelete:
					restore_list.AddItem(new_ref);
					break;
				case kEditName: {
					BEntry entry(new_ref);
					if (entry.InitCheck() != B_OK || !entry.Exists())
						break;
					
					char *old_name;
					if (message->FindString("old_name",
						(const char **)&old_name) != B_OK)
						break;
					
					// save redo data
					char name[B_FILE_NAME_LENGTH];
					entry.GetName(name);
					message->AddString("new_name", name);
					
					
					entry.Rename(old_name);
					
					// and again redo stuff
					entry_ref orig_ref;
					entry.GetRef(&orig_ref);
					message->AddRef("orig_ref", &orig_ref);
					break;
				}
				case kEditItem: {
					BEntry entry(new_ref);
					if (entry.InitCheck() != B_OK || !entry.Exists())
						break;
					
					const char *attr[B_ATTR_NAME_LENGTH];
					if (message->FindString("attr", attr) != B_OK)
						break;
					int32 offset;
					if (message->FindInt32("offset", &offset) != B_OK)
						break;

					// save redo data
					BNode node(new_ref);
					entry_ref orig_ref;
					entry.GetRef(&orig_ref);
					uint8 redo_buffer[B_FILE_NAME_LENGTH * 10];
					struct attr_info info;
					node.GetAttrInfo(*attr, &info);
					type_code new_type = info.type;
					ssize_t new_size = node.ReadAttr(*attr, new_type,
														offset, redo_buffer,
														B_FILE_NAME_LENGTH * 10);
					
					message->AddInt32("new_type", new_type);
					message->AddInt64("new_size", new_size);
					message->AddData("new_data", new_type, redo_buffer, new_size);
					message->AddRef("orig_ref", &orig_ref);
					// end redo
					
					type_code type;
					if (message->FindInt32("old_type", (int32)&type) != B_OK)
						break;
					
					ssize_t size;
					uint8 *buffer;
					if (message->FindData("old_data", B_ANY_TYPE,
						(const void **)&buffer, &size) != B_OK ||
						size <= 0)
						node.RemoveAttr(*attr);
					else
						node.WriteAttr(*attr, type, offset, buffer, size);
					
					// hack to get it updated...
					char name[B_FILE_NAME_LENGTH];
					entry.GetName(name);
					entry.Rename("<null>");
					entry.Rename(name);
					break;
				}
				default:
					break;
			}
		}
		
		if (trash_list.CountItems() > 0) {
			tfscontext = new TFSContext(&trash_list);
			IgnoreContext(tfscontext);
			tfscontext->MoveToTrash(false);
		}
		
		if (move_list.CountItems() > 0) {
			tfscontext = new TFSContext(&move_list);
			IgnoreContext(tfscontext);
			BDirectory target_dir(context->SourceForContext());
			tfscontext->MoveTo(target_dir, false);
		}
		
		if (restore_list.CountItems() > 0) {
			tfscontext = new TFSContext(&restore_list);
			IgnoreContext(tfscontext);
			tfscontext->RestoreFromTrash(false);
		}
	}
		
	return result;
}

int32
UndoHistory::Redo(int32 count, BContainerWindow */*source*/)
{
	if (count <= 0)
		return 0;
	
	TFSContext *tfscontext;
	BMessage *message = NULL;
	UndoContext *context = NULL;
	int32 result;
	
	for (result = 0; result < count; result++) {
		context = CurrentContext(1);
		if (context == NULL)
			break;
		
		context->RewindActions();
		
		BObjectList<entry_ref> trash_list(20);
		BObjectList<entry_ref> move_list(20);
		BObjectList<entry_ref> restore_list(20);
		BObjectList<entry_ref> copy_list(20);
		BObjectList<entry_ref> link_list(20);
		BObjectList<entry_ref> rlink_list(20);
		
		while ((message = context->NextAction()) != NULL) {
			entry_ref *orig_ref = new entry_ref;
			if (message->FindRef("orig_ref", orig_ref) != B_OK &&
				message->FindRef("new_ref", orig_ref) != B_OK)
				continue;
			
			BEntry entry(orig_ref);
			if (entry.InitCheck() != B_OK || !entry.Exists())
				continue;
					
			switch (message->what) {
				case kNewFolder: {
					const char *name;
					if (message->FindString("name", &name) != B_OK)
						name = NULL;
					
					BDirectory dir(orig_ref);
					if (dir.InitCheck() != B_OK)
						break;
					
					BDirectory new_dir;
					char name_buf[B_FILE_NAME_LENGTH];
					strcpy(name_buf, (name) ? name : LOCALE("New Folder"));
					while (dir.CreateDirectory(name_buf, &new_dir) == B_FILE_EXISTS &&
							name == NULL)
						FSContext::MakeUniqueName(dir, name_buf);
					break;
				}
				case kCreateLink:
					link_list.AddItem(orig_ref);
					break;
				case kCreateRelativeLink:
					rlink_list.AddItem(orig_ref);
					break;
				case kCopySelectionTo:
					copy_list.AddItem(orig_ref);
					break;
				case kRestoreFromTrash:
					restore_list.AddItem(orig_ref);
					break;
				case kMoveSelectionTo:
					move_list.AddItem(orig_ref);
					break;
				case kDelete:
					trash_list.AddItem(orig_ref);
					break;
				case kEditName: {
					char *new_name;
					if (message->FindString("new_name",
						(const char **)&new_name) != B_OK)
						break;
					
					entry.Rename(new_name);
					break;
				}
				case kEditItem: {
					const char *attr[B_ATTR_NAME_LENGTH];
					if (message->FindString("attr", attr) != B_OK)
						break;
					int32 offset;
					if (message->FindInt32("offset", &offset) != B_OK)
						break;

					type_code type;
					if (message->FindInt32("new_type", (int32)&type) != B_OK)
						break;
					ssize_t size;
					uint8 *buffer;
					BNode node(&entry);
					if (message->FindData("new_data", B_ANY_TYPE,
						(const void **)&buffer, &size) != B_OK ||
						size <= 0)
						node.RemoveAttr(*attr);
					else
						node.WriteAttr(*attr, type, offset, buffer, size);
					
					// hack to get it updated...
					char name[B_FILE_NAME_LENGTH];
					entry.GetName(name);
					entry.Rename("<null>");
					entry.Rename(name);
					break;
				}
				default:
					break;
			}
		}
		
		BDirectory target_dir(context->TargetForContext());
		
		if (trash_list.CountItems() > 0) {
			tfscontext = new TFSContext(&trash_list);
			IgnoreContext(tfscontext);
			tfscontext->MoveToTrash(false);
		}
		
		if (move_list.CountItems() > 0) {
			tfscontext = new TFSContext(&move_list);
			IgnoreContext(tfscontext);
			tfscontext->MoveTo(target_dir, false);
		}
		
		if (restore_list.CountItems() > 0) {
			tfscontext = new TFSContext(&restore_list);
			IgnoreContext(tfscontext);
			tfscontext->RestoreFromTrash(false);
		}
		
		if (copy_list.CountItems() > 0) {
			tfscontext = new TFSContext(&copy_list);
			IgnoreContext(tfscontext);
			tfscontext->CopyTo(target_dir, false);
		}
		
		if (link_list.CountItems() > 0) {
			tfscontext = new TFSContext(&link_list);
			IgnoreContext(tfscontext);
			tfscontext->CreateLinkTo(target_dir, false, false);
		}
		
		if (rlink_list.CountItems() > 0) {
			tfscontext = new TFSContext(&rlink_list);
			IgnoreContext(tfscontext);
			tfscontext->CreateLinkTo(target_dir, true, false);
		}
	}
		
	return result;
}

void
UndoHistory::CheckSize()
{
	fDepth = gTrackerSettings.UndoDepth();
	while (fPosition > fDepth) {
		delete fList.RemoveItemAt(0);
		fPosition--;
	}
}

UndoContext *
UndoHistory::FindContext(FSContext *context)
{
	UndoContext *item = NULL;
	int32 count = fOpen.CountItems();
	
	for (int index = 0; index < count; index++) {
		item = fOpen.ItemAt(index);
		if (item && item->Context() == context)
			return item;
	}
	
	return NULL;
}

void
UndoHistory::IgnoreContext(FSContext *context)
{
	UndoContext *ourContext = FindContext(context);
	
	if (ourContext) {
		fOpen.RemoveItem(ourContext);
		delete ourContext;
	}
}

UndoContext	*
UndoHistory::CurrentContext(int32 offsetby)
{
	UndoContext *result = fList.ItemAt(fPosition + (offsetby > 0 ? 0 : -1));
	
	if (result) {
		if (result->Overwrite())
			result = NULL;
		else
			fPosition += offsetby;
	}
	
	if (fPosition < 0) {
		fPosition = 0;
		result = NULL;
	}
	
	return result;
}


UndoContext::UndoContext(FSContext *context, int32 items)
	:	fActions(BObjectList<BMessage>(items, true)),
		fContext(context),
		fOverwrite(false)
{
}

void
UndoContext::SetSourceForContext(BDirectory &source)
{
	BEntry entry;
	source.GetEntry(&entry);
	BPath path;
	entry.GetPath(&path);
	fMoveSource = path.Path();
}

void
UndoContext::SetTargetForContext(BDirectory &target)
{
	BEntry entry;
	target.GetEntry(&entry);
	BPath path;
	entry.GetPath(&path);
	fMoveTarget = path.Path();
}

} // namespace BPrivate
