/*
	Copyright 1999, Be Incorporated.   All Rights Reserved.
	This file may be used under the terms of the Be Sample Code License.
*/

typedef long status_t;
typedef unsigned long uint32;

enum scale_method {
	IMG_SCALE_BILINEAR = 1
};

class BBitmap;

// Scale an image.  It will work in either direction.
// Scaling up or down.  Scaling by integer values will be
// most optimal.

status_t	scale(const BBitmap *source, BBitmap *dst, 
				const float xFactor = -1, const float yFactor = -1,
				scale_method = IMG_SCALE_BILINEAR);

