//
//  ViewController.swift
//  WifiCameraSample
//
//  Created by yma on 2015/11/19.
//  Copyright © 2015年 yma@asra. All rights reserved.
//

import UIKit

class ViewController: UIViewController ,WifiCameraDelegate{
    
    @IBOutlet var cameraView : UIImageView? = nil;
    
    var image:UIImage? = nil;
    var wifiCamera:WifiCameraUtility = WifiCameraUtility();
    var counterAccept:Int32  = 0;
    var counterShow:Int32  = 0;

    
    
    override init(nibName nibNameOrNil: String?, bundle nibBundleOrNil: NSBundle?) {
        super.init(nibName: nibNameOrNil, bundle: nibBundleOrNil)
        // Custom initialization
    }
    

    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        // Do any additional setup after loading the view, typically from a nib.
        print(cameraView);
        NSThread.detachNewThreadSelector("threadFunc:",toTarget:self,withObject:nil);
   }

    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
        print("didReceiveMemoryWarning=<",counterAccept,">");
    }
    
    func threadFunc(paramObj : AnyObject) {
        print("threadFunc");
        print(wifiCamera);
        [wifiCamera.setDelegate(self)];
        [wifiCamera.open()];
    }
    /*
        update frame image from wifi camera.
    */
    func updateFrame(frame:UIImage){
        print(frame);
        print("counterAccept=",counterAccept++);
        print("updateFrame ",frame);
        dispatch_sync(dispatch_get_main_queue(),{
            self.cameraView?.image = frame;
            print("counterShow=<%d>",self.counterShow++);
            print("dispatch_get_main_queue ",frame);
        });
    }
}

