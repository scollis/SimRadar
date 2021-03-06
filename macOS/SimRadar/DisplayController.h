//
//  DisplayController.h
//
//  Created by Boon Leng Cheong on 10/29/13.
//  Copyright (c) 2013 Boon Leng Cheong. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "SimPoint.h"
#import "SimGLView.h"
//#import "Recorder.h"

@interface DisplayController : NSWindowController <NSWindowDelegate> {
	
	SimPoint *sim;
	SimGLView *glView;

@private

	id rootSender;
	NSTimer *inputMonitorTimer;

    int debrisId;
    
    GLfloat *sampleAnchorLines, *sampleAnchors;
    
//    Recorder *recorder;
    
    int mkey;
}

@property (nonatomic, retain) SimPoint *sim;
@property (nonatomic, retain) IBOutlet SimGLView *glView;

- (id)initWithWindowNibName:(NSString *)windowNibName viewDelegate:(id)sender;
- (IBAction)spinModel:(id)sender;
- (IBAction)normalSizeWindow:(id)sender;
- (IBAction)doubleSizeWindow:(id)sender;
- (IBAction)setSizeTo720p:(id)sender;
- (IBAction)setSizeTo1080p:(id)sender;
- (IBAction)setSizeTo4K:(id)sender;

- (void)emptyDomain;
- (void)setDrawMode:(int)mode;

@end
