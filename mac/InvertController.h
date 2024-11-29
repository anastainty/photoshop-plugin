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

#import <Cocoa/Cocoa.h>
#include "Invert.h"

OSStatus initializeCocoaInvert(void);
OSStatus orderWindowFrontInvert(void);

/* Make sure this is unique to you and everyone you might encounter, search for
"Preventing Name Conflicts" or use this link
http://developer.apple.com/mac/library/documentation/UserExperience/Conceptual/PreferencePanes/Tasks/Conflicts.html
*/

// sub class the text field so proxy updates occur on each key
@interface InvertTextField : NSTextField 
{
}
- (void) keyUp: (NSEvent *) theEvent; 
@end

// sub class the dialog so all things work
@interface InvertController : NSObject 
{
    id invertWindow;
    IBOutlet InvertTextField * textField;
	id dispositionClear;
	id dispositionCool;
	id dispositionHot;
	id dispositionSick;
	id proxyPreview;
	
	NSString * amountValue;
	
}
- (void) updateProxy;
- (void) updateAmountValue;
- (void) updateCursor;
- (int) showWindow;
- (NSString *) getAmountValue;
+ (InvertController *) invertController;
@end

// end InvertController.h
