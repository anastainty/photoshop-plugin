#include "Invert.h"
#include "InvertUI.h"
#include "InvertScripting.h"
#include "InvertRegistry.h"
#include "FilterBigDocument.h"
#include <time.h>
#include "Logger.h"
#include "Timer.h"

FilterRecord* gFilterRecord = NULL;
intptr_t* gDataHandle = NULL;
int16* gResult = NULL;
SPBasicSuite* sSPBasic = NULL;

Data* gData = NULL;
Parameters* gParams = NULL;

void DoAbout(void);
void DoParameters(void);
void DoPrepare(void);
void DoStart(void);
void DoContinue(void);
void DoFinish(void);

void DoFilter(void);
void CalcProxyScaleFactor(void);
void ConvertRGBColorToMode(const int16 imageMode, FilterColor& color);
void ScaleRect(VRect& destination, const int16 num, const int16 den);
void ShrinkRect(VRect& destination, const int16 width, const int16 height);
void CopyRect(VRect& destination, const VRect& source);
void LockHandles(void);
void UnlockHandles(void);
void CreateParametersHandle(void);
void InitParameters(void);
void CreateDataHandle(void);
void InitData(void);
void InvertRectangle(void* data,
	int32 dataRowBytes,
	void* mask,
	int32 maskRowBytes,
	VRect tileRect,
	uint8 color,
	int32 depth);

DLLExport MACPASCAL void PluginMain(const int16 selector,
	FilterRecordPtr filterRecord,
	intptr_t* data,
	int16* result)
{

	try {


		Logger logIt("Invert");
		Timer timeIt;

		logIt.Write("Selector: ", false);
		logIt.Write(selector, false);
		logIt.Write(" ", false);

		gFilterRecord = filterRecord;
		gDataHandle = data;
		gResult = result;

		if (selector == filterSelectorAbout)
		{
			sSPBasic = ((AboutRecord*)gFilterRecord)->sSPBasic;
		}
		else
		{
			sSPBasic = gFilterRecord->sSPBasic;

			if (gFilterRecord->bigDocumentData != NULL)
				gFilterRecord->bigDocumentData->PluginUsing32BitCoordinates = true;
		}

		switch (selector)
		{
		case filterSelectorAbout:
			DoAbout();
			break;
		case filterSelectorParameters:
			DoParameters();
			break;
		case filterSelectorPrepare:
			DoPrepare();
			break;
		case filterSelectorStart:
			DoStart();
			break;
		case filterSelectorContinue:
			DoContinue();
			break;
		case filterSelectorFinish:
			DoFinish();
			break;
		default:
			break;
		}

		if (selector != filterSelectorAbout)
			UnlockHandles();

		logIt.Write(timeIt.GetElapsed(), true);

	}
	catch (...)
	{
		if (NULL != result)
			*result = -1;
	}

}

void DoParameters(void)
{
	if (gFilterRecord->parameters == NULL)
		CreateParametersHandle();
	if ((*gDataHandle) == 0)
		CreateDataHandle();
	if (*gResult == noErr)
	{
		LockHandles();
		InitParameters();
		InitData();
	}
}


void DoPrepare(void)
{
	if (gFilterRecord->parameters != NULL && (*gDataHandle) != 0)
		LockHandles();
	else
	{
		if (gFilterRecord->parameters == NULL)
			CreateParametersHandle();
		if ((*gDataHandle) == 0)
			CreateDataHandle();
		if (*gResult == noErr)
		{
			LockHandles();
			InitParameters();
			InitData();
		}
	}

	gFilterRecord->bufferSpace = 0;

	VRect filterRect = GetFilterRect();
	int32 tileHeight = filterRect.bottom - filterRect.top;
	int32 tileWidth = filterRect.right - filterRect.left;
	if (tileHeight > 256)
		tileHeight = 256;
	if (tileWidth > 256)
		tileWidth = 256;

	int32 tileSize = tileHeight * tileWidth;
	int32 planes = gFilterRecord->planes;
	if (gFilterRecord->maskData != NULL)
		planes++;
	planes *= 2;

	int32 totalSize = tileSize * planes;
	if (gFilterRecord->maxSpace > totalSize)
		gFilterRecord->maxSpace = totalSize;
}

void DoStart(void)
{
	LockHandles();

	ReadRegistryParameters();

	int16 lastDisposition = gParams->disposition;
	int16 lastPercent = gParams->percent;
	Boolean lastIgnoreSelection = gParams->ignoreSelection;

	Boolean isOK = true;
	Boolean displayDialog;
	OSErr err = ReadScriptParameters(&displayDialog);

	if (!err && displayDialog)
		isOK = DoUI();

	gData->queryForParameters = false;

	if (isOK)
	{
		DoFilter();
	}
	else
	{
		gParams->disposition = lastDisposition;
		gParams->percent = lastPercent;
		gParams->ignoreSelection = lastIgnoreSelection;
		*gResult = userCanceledErr;
	}
}


void DoContinue(void)
{
	VRect zeroRect = { 0, 0, 0, 0 };

	SetInRect(zeroRect);
	SetOutRect(zeroRect);
	SetMaskRect(zeroRect);
}


void DoFinish(void)
{
	LockHandles();
	WriteScriptParameters();
	WriteRegistryParameters();
}

void DoFilter(void)
{
	srand((unsigned)time(NULL));

	int32 tileHeight = gFilterRecord->outTileHeight;
	int32 tileWidth = gFilterRecord->outTileWidth;

	if (tileWidth == 0 || tileHeight == 0)
	{
		*gResult = filterBadParameters;
		return;
	}

	VRect filterRect = GetFilterRect();
	int32 rectWidth = filterRect.right - filterRect.left;
	int32 rectHeight = filterRect.bottom - filterRect.top;

	CreateInvertBuffer(tileWidth, tileHeight);

	int32 tilesVert = (tileHeight - 1 + rectHeight) / tileHeight;
	int32 tilesHoriz = (tileWidth - 1 + rectWidth) / tileWidth;

	gFilterRecord->inputRate = (int32)1 << 16;
	gFilterRecord->maskRate = (int32)1 << 16;

	int32 progressTotal = tilesVert * tilesHoriz;
	int32 progressDone = 0;

	for (int32 vertTile = 0; vertTile < tilesVert; vertTile++)
	{
		for (int32 horizTile = 0; horizTile < tilesHoriz; horizTile++)
		{
			UpdateInvertBuffer(tileWidth, tileHeight);

			filterRect = GetFilterRect();
			VRect inRect = GetInRect();

			inRect.top = vertTile * tileHeight + filterRect.top;
			inRect.left = horizTile * tileWidth + filterRect.left;
			inRect.bottom = inRect.top + tileHeight;
			inRect.right = inRect.left + tileWidth;

			if (inRect.bottom > rectHeight)
				inRect.bottom = rectHeight;
			if (inRect.right > rectWidth)
				inRect.right = rectWidth;

			SetInRect(inRect);

			SetOutRect(inRect);

			if (gFilterRecord->haveMask)
			{
				SetMaskRect(inRect);
			}

			for (int16 plane = 0; plane < gFilterRecord->planes; plane++)
			{
				gFilterRecord->outLoPlane = gFilterRecord->inLoPlane = plane;
				gFilterRecord->outHiPlane = gFilterRecord->inHiPlane = plane;

				*gResult = gFilterRecord->advanceState();
				if (*gResult != noErr) return;

				uint8 color = 255;
				int16 expectedPlanes = CSPlanesFromMode(gFilterRecord->imageMode,
					0);

				if (plane < expectedPlanes)
					color = gData->color[plane];

				InvertRectangle(gFilterRecord->outData,
					gFilterRecord->outRowBytes,
					gFilterRecord->maskData,
					gFilterRecord->maskRowBytes,
					GetOutRect(),
					color,
					gFilterRecord->depth);
			}

			gFilterRecord->progressProc(++progressDone, progressTotal);

			if (gFilterRecord->abortProc())
			{
				*gResult = userCanceledErr;
				return;
			}
		}
	}
	DeleteInvertBuffer();
}

void InvertRectangle(void* data,
	int32 dataRowBytes,
	void* mask,
	int32 maskRowBytes,
	VRect tileRect,
	uint8 color,
	int32 depth)
{

	uint8* pixel = (uint8*)data;
	uint16* bigPixel = (uint16*)data;
	float* fPixel = (float*)data;
	uint8* maskPixel = (uint8*)mask;

	int32 rectHeight = tileRect.bottom - tileRect.top;
	int32 rectWidth = tileRect.right - tileRect.left;

	for (int32 pixelY = 0; pixelY < rectHeight; pixelY++)
	{
		for (int32 pixelX = 0; pixelX < rectWidth; pixelX++)
		{

			bool leaveItAlone = false;
			if (maskPixel != NULL && !(*maskPixel) && !gParams->ignoreSelection)
				leaveItAlone = true;

			if (!leaveItAlone)
			{
				if (depth == 32)
					*fPixel = 1.0 - *fPixel;
				else if (depth == 16)
					*bigPixel = UINT16_MAX - *bigPixel;
				else
					*pixel = UINT8_MAX - *pixel;
			}
			pixel++;
			bigPixel++;
			fPixel++;
			if (maskPixel != NULL)
				maskPixel++;
		}
		pixel += (dataRowBytes - rectWidth);
		bigPixel += (dataRowBytes / 2 - rectWidth);
		fPixel += (dataRowBytes / 4 - rectWidth);
		if (maskPixel != NULL)
			maskPixel += (maskRowBytes - rectWidth);
	}
}

void CreateParametersHandle(void)
{
	gFilterRecord->parameters = gFilterRecord->handleProcs->newProc
	(sizeof(Parameters));
	if (gFilterRecord->parameters == NULL)
		*gResult = memFullErr;
}

void InitParameters(void)
{
	gParams->disposition = 1;
	gParams->ignoreSelection = false;
	gParams->percent = 50;
}

void CreateDataHandle(void)
{
	Handle h = gFilterRecord->handleProcs->newProc(sizeof(Data));
	if (h != NULL)
		*gDataHandle = (intptr_t)h;
	else
		*gResult = memFullErr;
}

void InitData(void)
{
	CopyColor(gData->colorArray[0], gFilterRecord->backColor);
	SetColor(gData->colorArray[1], 0, 0, 255, 0);
	SetColor(gData->colorArray[2], 255, 0, 0, 0);
	SetColor(gData->colorArray[3], 0, 255, 0, 0);
	for (int a = 1; a < 4; a++)
		ConvertRGBColorToMode(gFilterRecord->imageMode, gData->colorArray[a]);
	CopyColor(gData->color, gData->colorArray[gParams->disposition]);
	gData->proxyRect.left = 0;
	gData->proxyRect.right = 0;
	gData->proxyRect.top = 0;
	gData->proxyRect.bottom = 0;
	gData->scaleFactor = 1.0;
	gData->queryForParameters = true;
	gData->invertBufferID = NULL;
	gData->invertBuffer = NULL;
	gData->proxyBufferID = NULL;
	gData->proxyBuffer = NULL;
	gData->proxyWidth = 0;
	gData->proxyHeight = 0;
	gData->proxyPlaneSize = 0;
}

void CreateInvertBuffer(const int32 width, const int32 height)
{
	BufferProcs* bufferProcs = gFilterRecord->bufferProcs;

	bufferProcs->allocateProc(width * height, &gData->invertBufferID);
	gData->invertBuffer = bufferProcs->lockProc(gData->invertBufferID, true);
}


void UpdateInvertBuffer(const int32 width, const int32 height)
{
	if (gData->invertBuffer != NULL)
	{
		Ptr invert = gData->invertBuffer;
		for (int32 x = 0; x < width; x++)
		{
			for (int32 y = 0; y < height; y++)
			{
				*invert = ((unsigned16)rand()) % 100 < gParams->percent;
				invert++;
			}
		}
	}
}

void DeleteInvertBuffer(void)
{
	if (gData->invertBufferID != NULL)
	{
		gFilterRecord->bufferProcs->unlockProc(gData->invertBufferID);
		gFilterRecord->bufferProcs->freeProc(gData->invertBufferID);
		gData->invertBufferID = NULL;
		gData->invertBuffer = NULL;
	}
}

void SetupFilterRecordForProxy(void)
{
	CalcProxyScaleFactor();

	SetInRect(GetFilterRect());
	VRect tempRect = GetInRect();

	ScaleRect(tempRect, 1, (int16)gData->scaleFactor);

	SetInRect(tempRect);

	SetMaskRect(GetInRect());
	gFilterRecord->inputRate = (int32)gData->scaleFactor << 16;
	gFilterRecord->maskRate = (int32)gData->scaleFactor << 16;

	gFilterRecord->inputPadding = 255;
	gFilterRecord->maskPadding = gFilterRecord->inputPadding;

	gData->proxyWidth = gData->proxyRect.right - gData->proxyRect.left;
	gData->proxyHeight = gData->proxyRect.bottom - gData->proxyRect.top;
	gData->proxyPlaneSize = gData->proxyWidth * gData->proxyHeight;
}


void CalcProxyScaleFactor(void)
{
	int32 filterHeight, filterWidth, itemHeight, itemWidth;
	VPoint fraction;

	VRect filterRect = GetFilterRect();

	ShrinkRect(gData->proxyRect, 2, 2);

	filterHeight = filterRect.bottom - filterRect.top;
	filterWidth = filterRect.right - filterRect.left;

	itemHeight = (gData->proxyRect.bottom - gData->proxyRect.top);
	itemWidth = (gData->proxyRect.right - gData->proxyRect.left);

	if (itemHeight > filterHeight)
		itemHeight = filterHeight;

	if (itemWidth > filterWidth)
		itemWidth = filterWidth;

	fraction.h = ((filterWidth + itemWidth) / itemWidth);
	fraction.v = ((filterHeight + itemHeight) / itemHeight);

	if (fraction.h > fraction.v)
		gData->scaleFactor = ((float)filterWidth + (float)itemWidth) /
		(float)itemWidth;
	else
		gData->scaleFactor = ((float)filterHeight + (float)itemHeight) /
		(float)itemHeight;

	CopyRect(gData->proxyRect, filterRect);
	ScaleRect(gData->proxyRect, 1, (int16)gData->scaleFactor);

	if (fraction.h > fraction.v)
		gData->scaleFactor = (float)filterWidth /
		(float)(gData->proxyRect.right -
			gData->proxyRect.left);
	else
		gData->scaleFactor = (float)filterHeight /
		(float)(gData->proxyRect.bottom -
			gData->proxyRect.top);
}


void ConvertRGBColorToMode(const int16 imageMode, FilterColor& color)
{
	if (imageMode != plugInModeRGBColor)
	{
		ColorServicesInfo	csInfo;

		csInfo.selector = plugIncolorServicesConvertColor;
		csInfo.sourceSpace = plugIncolorServicesRGBSpace;
		csInfo.reservedSourceSpaceInfo = NULL;
		csInfo.reservedResultSpaceInfo = NULL;
		csInfo.reserved = NULL;
		csInfo.selectorParameter.pickerPrompt = NULL;
		csInfo.infoSize = sizeof(csInfo);

		csInfo.resultSpace = CSModeToSpace(gFilterRecord->imageMode);
		for (int16 a = 0; a < 4; a++)
			csInfo.colorComponents[a] = color[a];

		if (!(gFilterRecord->colorServices(&csInfo)))
			for (int16 b = 0; b < 4; b++)
				color[b] = (int8)csInfo.colorComponents[b];
	}
}

void LockHandles(void)
{
	if (gFilterRecord->parameters == NULL || (*gDataHandle) == 0)
	{
		*gResult = filterBadParameters;
		return;
	}
	gParams = (Parameters*)gFilterRecord->handleProcs->lockProc
	(gFilterRecord->parameters, TRUE);
	gData = (Data*)gFilterRecord->handleProcs->lockProc
	((Handle)*gDataHandle, TRUE);
	if (gParams == NULL || gData == NULL)
	{
		*gResult = memFullErr;
		return;
	}
}

void UnlockHandles(void)
{
	if ((*gDataHandle) != 0)
		gFilterRecord->handleProcs->unlockProc((Handle)*gDataHandle);
	if (gFilterRecord->parameters != NULL)
		gFilterRecord->handleProcs->unlockProc(gFilterRecord->parameters);
}

void ScaleRect(VRect& destination, const int16 num, const int16 den)
{
	if (den != 0)
	{
		destination.left = (int16)((destination.left * num) / den);
		destination.top = (int16)((destination.top * num) / den);
		destination.right = (int16)((destination.right * num) / den);
		destination.bottom = (int16)((destination.bottom * num) / den);
	}
}

void ShrinkRect(VRect& destination, const int16 width, const int16 height)
{
	destination.left = (int16)(destination.left + width);
	destination.top = (int16)(destination.top + height);
	destination.right = (int16)(destination.right - width);
	destination.bottom = (int16)(destination.bottom - height);
}


void CopyRect(VRect& destination, const VRect& source)
{
	destination.left = source.left;
	destination.top = source.top;
	destination.right = source.right;
	destination.bottom = source.bottom;
}

void CopyColor(FilterColor& destination, const FilterColor& source)
{
	for (int a = 0; a < sizeof(FilterColor); a++)
		destination[a] = source[a];
}

void SetColor(FilterColor& destination,
	const uint8 a,
	const uint8 b,
	const uint8 c,
	const uint8 d)
{
	destination[0] = a;
	destination[1] = b;
	destination[2] = c;
	destination[3] = d;
}

void CreateProxyBuffer(void)
{
	int32 proxySize = gData->proxyPlaneSize * gFilterRecord->planes;
	OSErr e = gFilterRecord->bufferProcs->allocateProc(proxySize, &gData->proxyBufferID);
	if (!e && gData->proxyBufferID)
		gData->proxyBuffer = gFilterRecord->bufferProcs->lockProc(gData->proxyBufferID, true);
}

extern "C" void ResetProxyBuffer(void)
{
	uint8* proxyPixel = (uint8*)gData->proxyBuffer;

	if (proxyPixel != NULL)
	{
		for (int16 plane = 0; plane < gFilterRecord->planes; plane++)
		{
			gFilterRecord->inLoPlane = plane;
			gFilterRecord->inHiPlane = plane;

			*gResult = gFilterRecord->advanceState();
			if (*gResult != noErr) return;

			uint8* inPixel = (uint8*)gFilterRecord->inData;

			for (int32 y = 0; y < gData->proxyHeight; y++)
			{
				uint8* start = inPixel;

				for (int32 x = 0; x < gData->proxyWidth; x++)
				{
					if (gFilterRecord->depth == 32)
					{
						float* reallyBigPixel = (float*)inPixel;
						if (*reallyBigPixel > 1.0)
							*reallyBigPixel = 1.0;
						if (*reallyBigPixel < 0.0)
							*reallyBigPixel = 0.0;
						*proxyPixel = (uint8)(*reallyBigPixel * 255);
						inPixel += 4;
					}
					else 	if (gFilterRecord->depth == 16)
					{
						uint16* bigPixel = (uint16*)inPixel;
						*proxyPixel = (uint8)(*bigPixel * 10 / 1285);
						inPixel += 2;
					}
					else
					{
						*proxyPixel = *inPixel;
						inPixel++;
					}
					proxyPixel++;
				}
				inPixel = start + gFilterRecord->inRowBytes;
			}
		}
	}
}

extern "C" void UpdateProxyBuffer(void)
{
	Ptr localData = gData->proxyBuffer;

	if (localData != NULL)
	{
		UpdateInvertBuffer(gData->proxyWidth, gData->proxyHeight);
		for (int16 plane = 0; plane < gFilterRecord->planes; plane++)
		{
			uint8 color = 255;
			uint16 expectedPlanes = CSPlanesFromMode(gFilterRecord->imageMode, 0);
			if (plane < expectedPlanes)
				color = gData->color[plane];
			InvertRectangle(localData,
				gData->proxyWidth,
				gFilterRecord->maskData,
				gFilterRecord->maskRowBytes,
				gData->proxyRect,
				color,
				8);
			localData += (gData->proxyPlaneSize);
		}
	}
}

void DeleteProxyBuffer(void)
{
	gFilterRecord->bufferProcs->unlockProc(gData->proxyBufferID);
	gFilterRecord->bufferProcs->freeProc(gData->proxyBufferID);
	gData->proxyBufferID = NULL;
	gData->proxyBuffer = NULL;
}


int32 DisplayPixelsMode(int16 mode)
{
	int32 returnMode = mode;
	switch (mode)
	{
	case plugInModeGray16:
	case plugInModeGray32:
		returnMode = plugInModeGrayScale;
		break;
	case plugInModeRGB96:
	case plugInModeRGB48:
		returnMode = plugInModeRGBColor;
		break;
	case plugInModeLab48:
		returnMode = plugInModeLabColor;
		break;
	case plugInModeCMYK64:
		returnMode = plugInModeCMYKColor;
		break;
	case plugInModeDeepMultichannel:
		returnMode = plugInModeMultichannel;
		break;
	case plugInModeDuotone16:
		returnMode = plugInModeDuotone;
		break;
	}
	return (returnMode);
}