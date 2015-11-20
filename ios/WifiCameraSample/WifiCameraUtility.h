//
//  WifiCameraUtility.h
//  WifiCameraSample
//
//  Created by yma on 2015/11/19.
//  Copyright © 2015年 yma@asra. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@protocol WifiCameraDelegate <NSObject>
@optional
- (void)updateFrame:(UIImage*)frame;
@end

@interface WifiCameraUtility : NSObject
- (void) setDelegate:(id)delegate;
- (void) open;
@property (strong, nonatomic) id<WifiCameraDelegate> delegate_;
@end
