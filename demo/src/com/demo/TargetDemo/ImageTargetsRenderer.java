package com.demo.TargetDemo;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
import android.opengl.GLSurfaceView;
import android.os.Message;

import com.qualcomm.QCAR.QCAR;

public class ImageTargetsRenderer implements GLSurfaceView.Renderer{
    public boolean mIsActive = false;
    public native void initRendering();
    public native void updateRendering(int width, int height);   
    public native void renderFrame();
    public native void initNativeCallback();
    
    /** Called when the surface is created or recreated. */
    public void onSurfaceCreated(GL10 gl, EGLConfig config)
    {
        initRendering();
        QCAR.onSurfaceCreated();
        initNativeCallback();
    }
    
    /** Called when the surface changed size. */
    public void onSurfaceChanged(GL10 gl, int width, int height)
    {
        updateRendering(width, height);
        QCAR.onSurfaceChanged(width, height);
    }
    
    /** Called to draw the current frame. */
    public void onDrawFrame(GL10 gl)
    {
        if (!mIsActive)
            return;
        renderFrame();
    }
    
    GUIManager guiManager;
    public void setMainActivity(GUIManager _guiManager) {
    	this.guiManager = _guiManager;
    }
    
    public void displayMessage(String text) {
    	Message message = new Message();
        message.what = 1;
        message.obj = text;
        guiManager.sendThreadSafeGUIMessage(message);
    }
}
