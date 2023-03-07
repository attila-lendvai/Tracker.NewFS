#include "HelperProtocol.h"
#include "TrackerHelper.h"

TrackerHelper::TrackerHelper()
	:	BApplication("application/x-vnd.mmlr-TrackerHelper"),
		fExtendedIcon()
{
	fLoop = spawn_thread(HelperEntry, "TrackerHelper", B_LOW_PRIORITY, this);
	resume_thread(fLoop);
}


TrackerHelper::~TrackerHelper()
{
	kill_thread(fLoop);
	delete_port(fPort);
}


int32
TrackerHelper::HelperEntry(void *arg)
{
	TrackerHelper *helper = (TrackerHelper *)arg;
	return helper->MainLoop();
}


int32
TrackerHelper::MainLoop()
{
	int32 code;
	size_t bufferSize = max_c(sizeof(status_t), sizeof(area_id));
	uint8 buffer[bufferSize];

	fPort = create_port(1, HELPER_PORT_NAME);

	printf("main loop\n");
	while (read_port(fPort, &code, buffer, bufferSize) >= B_OK) {
		//printf("code: 0x%08x;\n", code);

		switch (code) {
			case HELPER_HANDSHAKE: {
				fDataArea = clone_area("HelperDataArea", (void **)&fData,
					B_ANY_ADDRESS, B_READ_AREA | B_WRITE_AREA,
					*(area_id *)&buffer);
				fRequest = (IconRequest *)fData;
			} break;
			case HELPER_GET_ICON: {
				/*printf("requested icon:\n");
				printf("\tsize: %ld\n", fRequest->size);
				printf("\tname: %s\n", fRequest->name);
				printf("\tprefix: %s\n", fRequest->prefix);
				printf("\tsuffix: %s\n", fRequest->suffix);*/

				entry_ref ref;
				memcpy(&ref.device, &fRequest->ref.device, sizeof(ref.device));
				memcpy(&ref.directory, &fRequest->ref.directory, sizeof(ref.directory));
				ref.set_name(fRequest->name);
				/*BPath path(&ref);
				printf("\tpath: %s\n", path.Path());*/

				BNode node(&ref);
				BBitmap *bitmap = NULL;
				fExtendedIcon.Lock();
				fExtendedIcon.SetTo(&node, (icon_size)fRequest->size, fRequest->prefix, fRequest->suffix);
				status_t error = fExtendedIcon.GetIcon(bitmap);
				fExtendedIcon.Unlock();

				port_id notify = fRequest->notify;
				if (error == B_OK && bitmap) {
					memcpy(fData, bitmap->Bits(), bitmap->BitsLength());
					delete bitmap;
				}

				write_port(notify, HELPER_STATUS, &error,
					sizeof(error));
			} break;
		}
	}

	printf("ending main loop\n");
	return 0;
}
