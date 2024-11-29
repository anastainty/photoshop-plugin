// ADOBE SYSTEMS INCORPORATED
// Copyright  2009 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE:  Adobe permits you to use, modify, and distribute this 
// file in accordance with the terms of the Adobe license agreement
// accompanying it.  If you have received this file from a source
// other than Adobe, then your use, modification, or distribution
// of it requires the prior written permission of Adobe.
//-------------------------------------------------------------------------------

#import "InvertController.h"
#import "InvertProxyView.h"

InvertController *gInvertController = NULL;

/* Make sure this is unique to you and everyone you might encounter, search for
"Preventing Name Conflicts" or use this link
http://developer.apple.com/mac/library/documentation/UserExperience/Conceptual/PreferencePanes/Tasks/Conflicts.html
*/

// get the current value and force an update
@implementation InvertTextField
	
- (void)keyUp:(NSEvent *)theEvent 
{
	NSLog(@"Invert start keyUp, %d", [theEvent keyCode]);
	[gInvertController updateAmountValue];
	[gInvertController updateProxy];
	NSLog(@"Invert end keyUp, %d", gParams->percent);
}

@end

/* Make sure this is unique to you and everyone you might encounter, search for
"Preventing Name Conflicts" or use this link
http://developer.apple.com/mac/library/documentation/UserExperience/Conceptual/PreferencePanes/Tasks/Conflicts.html
*/

// controller for the entire dialog
@implementation InvertController

+ (InvertController *) invertController 
{
    return gInvertController;
}


- (id) init 
{
    self = [super init];

	amountValue = [NSString stringWithFormat:@"%d", gParams->percent];
    
    NSBundle * plugin = [NSBundle bundleForClass:[self class]];

    if (![plugin loadNibNamed:@"InvertDialog"
                 owner:self
                 topLevelObjects:nil])
	{
        NSLog(@"Invert failed to load InvertDialog xib");
    }
    
	gInvertController = self;

    [textField setStringValue:amountValue];

	switch (gParams->disposition) 
	{
		case 0: // clear
			[dispositionClear setState:true];
			[dispositionCool setState:false];
			[dispositionHot setState:false];
			[dispositionSick setState:false];
			break;
		case 2: // hot
			[dispositionClear setState:false];
			[dispositionCool setState:false];
			[dispositionHot setState:true];
			[dispositionSick setState:false];
			break;
		case 3: // sick
			[dispositionClear setState:false];
			[dispositionCool setState:false];
			[dispositionHot setState:false];
			[dispositionSick setState:true];
			break;
		default:
		case 1: // cool
			[dispositionClear setState:false];
			[dispositionCool setState:true];
			[dispositionHot setState:false];
			[dispositionSick setState:false];
		break;
	}
	
	NSLog(@"Invert Trying to set initial disposition");

	[(InvertProxyView*)proxyPreview setDispositionColor:gParams->disposition];

	NSLog(@"Invert Trying to set setNeedsDisplay");

	[proxyPreview setNeedsDisplay:YES];

	NSLog(@"Invert Done with init");

    return self;
}

- (int) showWindow 
{
    [invertWindow makeKeyAndOrderFront:nil];
	int b = [[NSApplication sharedApplication] runModalForWindow:invertWindow];
	[invertWindow orderOut:self];
	return b;
}

- (NSString *) getAmountValue 
{
	return amountValue;
}

- (void) updateAmountValue 
{
	amountValue = [textField stringValue];
	NSLog(@"Invert updateAmountValue invert %@", amountValue);
	gParams->percent = [amountValue intValue];
	NSLog(@"Invert Percent after updateAmountValue: %d", gParams->percent);
}

- (IBAction) okPressed: (id) sender 
{
	amountValue = [textField stringValue];
	[NSApp stopModalWithCode:1];
	NSLog(@"Invert after nsapp stopmodal");
}

- (IBAction) cancelPressed: (id) sender 
{
	NSLog(@"Invert cancel pressed");
	[NSApp stopModalWithCode:0];
	NSLog(@"Invert after nsapp abortmodal");
}

- (IBAction) clearPressed: (id) sender 
{
	NSLog(@"Invert clear pressed");
	gParams->disposition = 0;
	[gInvertController updateProxy];
}

- (IBAction) coolPressed: (id) sender 
{
	NSLog(@"Invert cool pressed");
	gParams->disposition = 1;
	[gInvertController updateProxy];
}

- (IBAction) hotPressed: (id) sender 
{
	NSLog(@"Invert hot pressed");
	gParams->disposition = 2;
	[gInvertController updateProxy];
}

- (IBAction) sickPressed: (id) sender 
{
	NSLog(@"Invert sick pressed");
	gParams->disposition = 3;
	[gInvertController updateProxy];
}

- (void) updateProxy 
{
	CopyColor(gData->color, gData->colorArray[gParams->disposition]);
	[(InvertProxyView*)proxyPreview setDispositionColor:gParams->disposition];
	[proxyPreview setNeedsDisplay:YES];
}

- (void) updateCursor
{
	NSLog(@"Invert Trying to updateCursor");
	sPSUIHooks->SetCursor(kPICursorArrow);
	NSLog(@"Invert Seemed to updateCursor");
}

@end

/* Carbon entry point and C-callable wrapper functions*/
OSStatus initializeCocoaInvert(void) 
{
	[[InvertController alloc] init];
    return noErr;
}

OSStatus orderWindowFrontInvert(void) 
{
    int okPressed = [[InvertController invertController] showWindow];
    return okPressed;
}

// end InvertController.m
