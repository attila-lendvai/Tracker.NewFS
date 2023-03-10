# Tracker makefile
# Based on the makefile template

NAME= Tracker
LIB_NAME= libtracker.so

DEBUGGER= FALSE

ROOT_DIR = ..

#	determine wheather running on x86 or ppc
MACHINE=$(shell uname -m)
ifeq ($(MACHINE), BePC)
	CPU = x86
else
	CPU = ppc
endif

BIN_DIR = $(ROOT_DIR)/bin/$(CPU)

SHARED_INCLUDE_PATHS = shared
LOCAL_INCLUDE_PATHS = $(ROOT_DIR)/$(SHARED_INCLUDE_PATHS)
SYSTEM_INCLUDE_PATHS = $(ROOT_DIR)/boost

BERES = beres

ifeq ($(CPU), x86)
	WARNINGS = \
		-W -Wall \
		-Wno-multichar \
		-Woverloaded-virtual \
		-Wreturn-type \
		-Wnon-virtual-dtor \
		-Wno-ctor-dtor-privacy

	ifeq ($(DEBUGGER), TRUE)
		OPTIMIZER := -O0 -DDEBUG=1
		DEBUG += -g
	else
		OPTIMIZER := -O3
	endif
	DEPENDENCIES = .x86_dependencies
else
	WARNINGS = \
		-w all \
		-w nonotinlined \

	ifeq ($(DEBUGGER), TRUE)
		OPTIMIZER := -O0 -DDEBUG=1
		DEBUG += -g
	else
		OPTIMIZER := -O7
	endif
	DEPENDENCIES = .ppc_dependencies
endif


# @src->@ 

ORIGIN := /boot/home/src/OpenTracker/ 

sources_src := \
	TrackerFilters.cpp \
	FSUtils.cpp \
	FSDialogWindow.cpp \
	FSStatusWindow.cpp \
	TFSContext.cpp \
	FSContext.cpp \
	Settings.cpp \
	AttributeStream.cpp \
	AutoMounter.cpp \
	AutoMounterSettings.cpp \
	BackgroundImage.cpp \
	Bitmaps.cpp \
	ContainerWindow.cpp \
	ExtendedIcon.cpp \
	FSClipboard.cpp\
	CountView.cpp \
	DeskWindow.cpp \
	DesktopPoseView.cpp \
	DialogPane.cpp \
	DirMenu.cpp \
	EntryIterator.cpp \
	FavoritesConfig.cpp \
	FavoritesMenu.cpp \
	FilePanel.cpp \
	FilePanelPriv.cpp \
	FilePermissionsView.cpp \
	FindPanel.cpp \
	IconCache.cpp \
	IconMenuItem.cpp \
	InfoWindow.cpp \
	MiniMenuField.cpp \
	Model.cpp \
	MountMenu.cpp \
	Navigator.cpp \
	NavMenu.cpp \
	NodePreloader.cpp \
	NodeWalker.cpp \
	OpenWithWindow.cpp \
	OverrideAlert.cpp \
	PendingNodeMonitorCache.cpp \
	Pose.cpp \
	PoseList.cpp \
	PoseView.cpp \
	TemplatesMenu.cpp \
	FBCPadding.cpp \
	VolumeWindow.cpp \
	MimeTypeList.cpp \
	PoseViewScripting.cpp \
	QueryContainerWindow.cpp \
	QueryPoseView.cpp \
	RecentItems.cpp \
	RegExp.cpp \
	SelectionWindow.cpp \
	SettingsHandler.cpp \
	SettingsViews.cpp \
	SlowContextPopup.cpp \
	SlowMenu.cpp \
	TaskLoop.cpp \
	Tests.cpp \
	TextWidget.cpp \
	ThreadMagic.cpp \
	TitleView.cpp \
	Tracker.cpp \
	TrackerInitialState.cpp \
	TrackerScripting.cpp \
	TrackerSettings.cpp \
	TrackerSettingsWindow.cpp \
	TrackerString.cpp \
	TrashWatcher.cpp \
	Undo.cpp \
	Utilities.cpp \
	ViewState.cpp \
	WidgetAttributeText.cpp \
#

resources_rdef := \
	TrackerIcons.rdef \
#

resources_app := \
	Tracker.rsrc \
#

# @<-src@ 

SRCS := \
	$(sources_src) \
	main.cpp \
#

RDEFS := \
	$(resources_rdef) \
#

APP_RSRCS := \
	$(resources_app) \
#

LIB_SRCS := \
	$(sources_src) \
#

# list the libraries your binary needs to link against
# (lbe, ltracker, etc.)
# note that libbe isn't included here because it appears
# that this is included by default on PPC (only).
LIBS= translation

#	set the directory where object files and binaries will be created
OBJ_DIR		:= obj.$(CPU)

# 	specify that the binary should be created in the object directory
TARGET		:= "$(OBJ_DIR)/$(NAME)"
TARGET_LIB	:= "$(OBJ_DIR)/$(LIB_NAME)"
LIB_EXP		:= $(OBJ_DIR)/$(basename $(LIB_NAME)).exp

MIMESET		:= mimeset
XRES		:= xres

CFLAGS := $(DEBUG) $(OPTIMIZER) $(WARNINGS) -D_BUILDING_tracker=1 -DOPEN_TRACKER=1 -D_IMPEXP_TRACKER="" -D_IMPEXP_BE="" -D_IMPEXP_ROOT=""
LDFLAGS	:= $(DEBUG)

#	x86 Settings
ifeq ($(CPU), x86)
#	set the compiler and compiler flags
	CC		:= gcc
	LD		:= gcc
	LDFLAGS += -lbe #-lstdc++.r4

	APP_LDFLAGS = $(LDFLAGS) \
				-Xlinker -soname=_APP_
	
	LIB_LDFLAGS = $(LDFLAGS) \
				-Xlinker -soname=$(LIB_NAME) \
				-nostart \
				-e 0 \
	
	BUILD_LIB	= $(LD) -o $@ $(LIB_OBJS) $(LIB_LDFLAGS)

	BUILD_EXP = touch "$(LIB_EXP)"

else

#	ppc Settings
ifeq ($(CPU), ppc)
	CC		:= mwcc
	LD		:= mwldppc

	LDFLAGS +=	-msg nodup \
				-init _init_routine_ \
				-term _term_routine_ \
				/boot/develop/lib/ppc/init_term_dyn.o
	
	APP_LDFLAGS = $(LDFLAGS) \
				-export pragma \
				/boot/develop/lib/ppc/start_dyn.o
#

	LIB_LDFLAGS = $(LDFLAGS) \
				-nodefaults -xms -L$(BELIBRARIES) glue-noinit.a -lroot -lbe

#	SETTING: output symbols in an xMAP file
	ifeq ($(SYMBOLS), TRUE)
		APP_LDFLAGS += -map $(TARGET).xMAP
		LIB_LDFLAGS += -map $(TARGET_LIB).xMAP
	endif

#	SETTING: output debugging info to a .SYM file
	ifeq ($(DEBUGGER), TRUE)
		APP_LDFLAGS += -g -osym $(TARGET).SYM
		LIB_LDFLAGS += -g -osym $(TARGET_LIB).SYM
	endif

	BUILD_LIB = $(LD) -o $@ -f "$(LIB_EXP)" $(LIB_OBJS) $(LIB_LDFLAGS)

	BUILD_EXP = \
		rm -f "$(OBJ_DIR)/tmp.exp" "$(OBJ_DIR)/tmp.sed" "$(LIB_EXP)"; \
		$(LD) $(LIB_LDFLAGS) -o $@ -f "$(OBJ_DIR)/tmp.exp" $(LIB_OBJS); \
		cat libtracker.noexport | \
			awk '/^[^ \n\t\#]/ { printf "s:\\(%s\\):\#\\1:\n", $$1 }' > \
				"$(OBJ_DIR)/tmp.sed" ; \
		sed -f "$(OBJ_DIR)/tmp.sed" < "$(OBJ_DIR)/tmp.exp" > \
			"$(LIB_EXP)" ;

endif
endif


# psuedo-function for converting a list of source files in SRCS variable
# to a corresponding list of object files in $(OBJ_DIR)/xxx.o
# The "function" strips off the src file suffix (.ccp or .c or whatever)
# and then strips of the directory name, leaving just the root file name.
# It then appends the .o suffix and prepends the $(OBJ_DIR)/ path
define SRCS_LIST_TO_OBJS
	$(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(foreach file, $(SRCS), \
	$(basename $(notdir $(file))))))
endef

define LIB_SRCS_LIST_TO_OBJS
	$(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(foreach file, $(LIB_SRCS), \
	$(basename $(notdir $(file))))))
endef

define RDEFS_LIST_TO_RSRCS
	$(addprefix $(OBJ_DIR)/, $(addsuffix .rsrc, $(foreach file, $(RDEFS), \
	$(basename $(notdir $(file))))))
endef

OBJS = $(SRCS_LIST_TO_OBJS)
LIB_OBJS = $(LIB_SRCS_LIST_TO_OBJS)
RSRCS = $(RDEFS_LIST_TO_RSRCS)

# create a unique list of paths to our sourcefiles
SRC_PATHS += $(sort $(foreach file, $(SRCS), $(dir $(file))))

# add source paths to VPATH if not already present
VPATH :=
VPATH += $(addprefix :, $(subst  ,:, $(filter-out $($(subst, :, ,$(VPATH))), $(SRC_PATHS))))

#	SETTING: build the local and system include paths
ifeq ($(CPU), x86)
	LOC_INCLUDES = $(foreach path, $(SRC_PATHS) $(LOCAL_INCLUDE_PATHS), $(addprefix -I, $(path)))
	SYS_INCLUDES += -I-
	SYS_INCLUDES += $(foreach path, $(SYSTEM_INCLUDE_PATHS), $(addprefix -I, $(path)))
else
ifeq ($(CPU), ppc)
	LOC_INCLUDES = $(foreach path, $(SRC_PATHS) $(LOCAL_INCLUDE_PATHS), $(addprefix -I, $(path)))
	SYS_INCLUDES += -i-
	SYS_INCLUDES += $(foreach path, $(SYSTEM_INCLUDE_PATHS), $(addprefix -i , $(path)))
endif
endif

INCLUDES = $(LOC_INCLUDES) $(SYS_INCLUDES)

# SETTING: add the -L prefix to all library paths to search
LINK_PATHS = $(foreach path, $(SRC_PATHS) $(LIBPATHS), \
	$(addprefix -L, $(path)))

#	SETTING: specify the additional libraries to link against
#	if the libraries have a .so or .a prefix, or if they are _APP_ or _KERNEL_
#	simply add them to the list
LINK_LIBS += $(filter %.so %.a _APP_ _KERNEL_, $(LIBS))
#	if the libraries do not have suffixes and are not _APP_ or _KERNEL_
#	prepend -l to each name: be becomes -lbe
LINK_LIBS += $(foreach lib, $(filter-out %.so %.a _APP_ _KERNEL_, $(LIBS)), $(addprefix -l, $(lib)))

# add to the linker flags 
LDFLAGS += $(LINK_PATHS)  $(LINK_LIBS)

#	SETTING: add the defines to the compiler flags
CFLAGS += $(foreach define, $(DEFINES), $(addprefix -D, $(define)))

#	SETTING: add the additional compiler flags
CFLAGS += $(COMPILER_FLAGS)

#	SETTING: add the additional linker flags
LDFLAGS += $(LINKER_FLAGS)
LIB_LDFLAGS += $(LINKER_FLAGS)

#	define the actual work to be done	
all: $(TARGET)
default: $(TARGET_LIB)
tracker: $(TARGET)
libtracker: $(TARGET_LIB)

# The old way of duplicating everything in a seperat Tracker
#$(TARGET):	$(OBJ_DIR) $(OBJS) $(RSRCS) $(APP_RSRCS)
#		$(LD) -o $@ $(OBJS) $(APP_LDFLAGS)
#		$(XRES) -o "$(TARGET)" $(RSRCS) $(APP_RSRCS)
#		$(MIMESET) -f $@

# And the new way: simply linking against the libtracker that includes everything
$(TARGET):	$(OBJ_DIR) $(OBJS) $(RSRCS) $(APP_RSRCS)
		$(LD) -o $@ "$(OBJ_DIR)/main.o" -ltracker $(APP_LDFLAGS)
		$(XRES) -o "$(TARGET)" $(RSRCS) $(APP_RSRCS)
		$(MIMESET) -f $@

$(TARGET_LIB):	$(OBJ_DIR) $(LIB_OBJS) $(RSRCS) $(LIB_EXP)
		$(BUILD_LIB)
		$(XRES) -o "$(TARGET_LIB)" $(RSRCS)
		$(MIMESET) -f $@
 
$(LIB_EXP):
		$(BUILD_EXP)

installlib:	$(TARGET_LIB)
		cp $(TARGET_LIB) /system/lib
		copyattr $(TARGET_LIB) /system/lib/$(LIB_NAME)

installapp:	$(TARGET)
		cp $(TARGET) /system
		copyattr $(TARGET) /system/$(NAME)

install:	installlib installapp

#	rule to create the object file directory if needed
$(OBJ_DIR)::
	@[ -d $(OBJ_DIR) ] || mkdir $(OBJ_DIR) > /dev/null 2>&1

$(OBJ_DIR)/%.o : %.c
	$(CC) $(INCLUDES) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.o : %.cpp
	$(CC) $(INCLUDES) $(CFLAGS) -c $< -o $@
$(OBJ_DIR)/%.rsrc : %.rdef
	$(BERES) -o $@ $<

#	empty rule. Things that depend on this rule will always get triggered
FORCE:

#	The generic clean command. Delete everything in the object folder.
clean :: FORCE
	-rm -rf $(OBJ_DIR)/*

#	remove just the application from the object folder
rmapp ::
	-rm -f "$(TARGET)"

# dependency generation

ifeq ($(CPU), ppc)
DEPFLAG = -make
SEDSCRIPT = sed -e'/\/boot\/develop\/headers\/.*\\/d' \
				-e's%^.*/boot/develop/headers/.*% %g' \
				-e's%^.*/\([^/]*\.o\)%$$(OBJ_DIR)/\1%' \
				-e"s%`pwd`/%%g" \
				-e"s%.*/$(SHARED_INCLUDE_PATHS)/%	../$(SHARED_INCLUDE_PATHS)/%g"
else
DEPFLAG = -MM
SEDSCRIPT = sed -e's%^\(.*\.o\)%$$(OBJ_DIR)/\1%'
endif

deps :: FORCE
	echo "# auto-generated by make deps; do not edit" > $(DEPENDENCIES)
	$(CC) $(INCLUDES) $(CFLAGS) $(DEPFLAG) $(SRCS) | $(SEDSCRIPT) >> $(DEPENDENCIES)

maintainerdeps :: FORCE
	p4 edit $(DEPENDENCIES)
	echo "# auto-generated by make deps; do not edit" > $(DEPENDENCIES)
	$(CC) $(INCLUDES) $(CFLAGS) $(DEPFLAG) $(SRCS) | $(SEDSCRIPT) >> $(DEPENDENCIES)
	echo "Change: new" > /tmp/tmp.deps
	echo "Status: new" >> /tmp/tmp.deps
	echo "Description:" >> /tmp/tmp.deps
	echo "	Auto-generated dependencies" >> /tmp/tmp.deps
	echo "Files:" >> /tmp/tmp.deps
	echo "	//depot/tracker/"$(DEPENDENCIES) >> /tmp/tmp.deps
	cat /tmp/tmp.deps | p4 submit -i

cleandeps :: FORCE
	-rm -f $(DEPENDENCIES)

cleanexp :: FORCE
	rm -f $(LIB_EXP)

-include $(DEPENDENCIES)
