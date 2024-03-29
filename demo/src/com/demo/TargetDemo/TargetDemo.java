package com.demo.TargetDemo;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.SubMenu;
import android.view.View;
import android.view.WindowManager;
import android.view.ViewGroup.LayoutParams;
import java.util.Vector;

import com.qualcomm.QCAR.QCAR;

public class TargetDemo extends Activity {
    private static final int APPSTATUS_UNINITED         = -1;
    private static final int APPSTATUS_INIT_APP         = 0;
    private static final int APPSTATUS_INIT_QCAR        = 1;
    private static final int APPSTATUS_INIT_APP_AR      = 2;
    private static final int APPSTATUS_INIT_TRACKER     = 3;
    private static final int APPSTATUS_INITED           = 4;
    private static final int APPSTATUS_CAMERA_STOPPED   = 5;
    private static final int APPSTATUS_CAMERA_RUNNING   = 6;

    // Name of the native dynamic libraries to load:
    private static final String NATIVE_LIB_SAMPLE = "TargetDemo";
    private static final String NATIVE_LIB_QCAR = "QCAR"; 

    // The current application status
    private int mAppStatus = APPSTATUS_UNINITED;
    // Our OpenGL view:
    private QCARSampleGLView mGlView;
    // Our renderer:
    private ImageTargetsRenderer mRenderer;
    private GUIManager mGUIManager;

    // Display size of the device
    private int mScreenWidth = 0;
    private int mScreenHeight = 0;
    // The async tasks to initialize the QCAR SDK 
    private InitQCARTask mInitQCARTask;
    private LoadTrackerTask mLoadTrackerTask;
    // QCAR initialization flags
    private int mQCARFlags = 0;
    // The textures we will use for rendering:
    private Vector<Texture> mTextures;

    /** Static initializer block to load native libraries on start-up. */
    static
    {
        loadLibrary(NATIVE_LIB_QCAR);
        loadLibrary(NATIVE_LIB_SAMPLE);
    }

    /** An async task to initialize QCAR asynchronously. */
    private class InitQCARTask extends AsyncTask<Void, Integer, Boolean>
    {   
        // Initialize with invalid value
        private int mProgressValue = -1;
        
        protected Boolean doInBackground(Void... params)
        {
            QCAR.setInitParameters(TargetDemo.this, mQCARFlags);
            do
            {
                // QCAR.init() blocks until an initialization step is complete,
                // then it proceeds to the next step and reports progress in
                // percents (0 ... 100%)
                // If QCAR.init() returns -1, it indicates an error.
                // Initialization is done when progress has reached 100%.
                mProgressValue = QCAR.init();
                // Publish the progress value:
                publishProgress(mProgressValue);
                // We check whether the task has been canceled in the meantime
                // (by calling AsyncTask.cancel(true))
                // and bail out if it has, thus stopping this thread.
                // This is necessary as the AsyncTask will run to completion
                // regardless of the status of the component that started is.
            } while (!isCancelled() && mProgressValue >= 0 && mProgressValue < 100);
            
            return (mProgressValue > 0);
        }

        protected void onProgressUpdate(Integer... values)
        {
            // Do something with the progress value "values[0]", e.g. update
            // splash screen, progress bar, etc.
        }

        protected void onPostExecute(Boolean result)
        {
            // Done initializing QCAR, proceed to next application
            // initialization status:
            if (result)
                updateApplicationStatus(APPSTATUS_INIT_APP_AR);
            else
            {
                // Create dialog box for display error:
                AlertDialog dialogError = new AlertDialog.Builder(TargetDemo.this).create();
                dialogError.setButton(
                    "Close",
                    new DialogInterface.OnClickListener()
                    {
                        public void onClick(DialogInterface dialog, int which)
                        {
                            // Exiting application
                            System.exit(1);
                        }
                    }
                ); 
                String logMessage;
                // NOTE: Check if initialization failed because the device is
                // not supported. At this point the user should be informed
                // with a message.
                if (mProgressValue == QCAR.INIT_DEVICE_NOT_SUPPORTED)
                {
                    logMessage = "Failed to initialize QCAR because this " +
                        "device is not supported.";
                }
                else if (mProgressValue ==
                            QCAR.INIT_CANNOT_DOWNLOAD_DEVICE_SETTINGS)
                {
                    logMessage = 
                        "Network connection required to initialize camera " +
                        "settings. Please check your connection and restart " +
                        "the application. If you are still experiencing " +
                        "problems, then your device may not be currently " +
                        "supported.";
                }
                else
                {
                    logMessage = "Failed to initialize QCAR.";
                }
                // Show dialog box with error message:
                dialogError.setMessage(logMessage);
                dialogError.show();
            }
        }
    }

    /** An async task to load the tracker data asynchronously. */
    private class LoadTrackerTask extends AsyncTask<Void, Integer, Boolean>
    {
        protected Boolean doInBackground(Void... params)
        {
            // Initialize with invalid value
            int progressValue = -1;
            do
            {
                progressValue = QCAR.load();
                publishProgress(progressValue);
            } while (!isCancelled() && progressValue >= 0 &&
                        progressValue < 100);
            return (progressValue > 0);
        }
        
        protected void onProgressUpdate(Integer... values)
        {
            // Do something with the progress value "values[0]", e.g. update
            // splash screen, progress bar, etc.
        }
        
        protected void onPostExecute(Boolean result)
        {
            // Done loading the tracker, update application status: 
            updateApplicationStatus(APPSTATUS_INITED);
        }
    }
    
    private Handler activityHandler;
    private long lastActionTime;
    public final static int GUIMESSAGE = 0;
    public final static int SHOWTIME = 1;
    public final static int SHOWINFO = 2;
    public final static int SHARE = 3;
    public final static int GOTOYOUTUBE = 4;
    
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mTextures = new Vector<Texture>();
        loadTextures();
        mQCARFlags = getInitializationFlags();
        lastActionTime = 0;
        activityHandler = new Handler() {
        	@Override
    		public void handleMessage(Message msg) {
        		String str = (String) msg.obj;
        		long cur = System.currentTimeMillis();
        		Log.v("demo", str);
        		switch (msg.what) {
        			case SHARE:
        				if (cur - lastActionTime > 3000) {
        					lastActionTime = cur;
        					Intent intent3 = new Intent(TargetDemo.this, MotionPosterTweetActivity.class);
        					Bundle bl3 = new Bundle();
        					bl3.putString("name", str);
        					intent3.putExtras(bl3);
        					startActivity(intent3);
        				}
        				break;
        			case SHOWTIME:
        				if (cur - lastActionTime > 3000) {
        					lastActionTime = cur;
        					String[] items = str.split(":");
        					Intent intent1 = new Intent(TargetDemo.this, TheaterMap.class);
        					Bundle bl1 = new Bundle();
        					bl1.putString("id", items[1]);
        					bl1.putString("name", items[0]);
        					intent1.putExtras(bl1);
        					startActivity(intent1);
        				}
        				break;
    		    	case SHOWINFO:
    		        	if (cur - lastActionTime > 3000) {
    		        		lastActionTime = cur;
    		        		Intent intent2 = new Intent(TargetDemo.this, Information.class);
    		        		Bundle bl2 = new Bundle();
    		        		bl2.putString("url", "http://motion-poster.appspot.com/info?id=" + str);
    		        		intent2.putExtras(bl2);
    		        		startActivity(intent2);
    		        	}
    		    		break;
    		        case GOTOYOUTUBE:
    		        	if (cur - lastActionTime > 3000) {
    		        		lastActionTime = cur;
    		        		startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(str)));
    		        	}
    		        	break;
    		    }
    		}
        };
        updateApplicationStatus(APPSTATUS_INIT_APP);
    }
    
    public void sendThreadSafeGUIMessage(Message message) {
    	activityHandler.sendMessage(message);
	}

    /** We want to load specific textures from the APK, which we will later
    use for rendering. */
    private void loadTextures()
    {
        mTextures.add(Texture.loadTextureFromApk("orange-button.png", getAssets()));
        mTextures.add(Texture.loadTextureFromApk("green-button.png", getAssets()));
        mTextures.add(Texture.loadTextureFromApk("blue-button.png", getAssets()));
        mTextures.add(Texture.loadTextureFromApk("purple-button.png", getAssets()));
    }

    /** Configure QCAR with the desired version of OpenGL ES. */
    private int getInitializationFlags()
    {
        int flags = 0;
        if (getOpenGlEsVersionNative() == 1)
            flags = QCAR.GL_11;
        else
            flags = QCAR.GL_20;
        return flags;
    }    

    protected void onResume()
    {
        super.onResume();
        // QCAR-specific resume operation
        QCAR.onResume();
        // We may start the camera only if the QCAR SDK has already been 
        // initialized
        if (mAppStatus == APPSTATUS_CAMERA_STOPPED)
            updateApplicationStatus(APPSTATUS_CAMERA_RUNNING);
        // Resume the GL view:
        if (mGlView != null)
        {
            mGlView.setVisibility(View.VISIBLE);
            mGlView.onResume();
        }        
    }
    
    protected void onPause()
    {
    	//onDestroy();
        super.onPause();
        if (mGlView != null)
        {
            mGlView.setVisibility(View.INVISIBLE);
            mGlView.onPause();
        }
        // QCAR-specific pause operation
        QCAR.onPause();
        if (mAppStatus == APPSTATUS_CAMERA_RUNNING)
            updateApplicationStatus(APPSTATUS_CAMERA_STOPPED);
    }
    /** The final call you receive before your activity is destroyed.*/
    protected void onDestroy()
    {
        super.onDestroy();
        // Cancel potentially running tasks
        if (mInitQCARTask != null &&
            mInitQCARTask.getStatus() != InitQCARTask.Status.FINISHED)
        {
            mInitQCARTask.cancel(true);
            mInitQCARTask = null;
        }
        if (mLoadTrackerTask != null &&
            mLoadTrackerTask.getStatus() != LoadTrackerTask.Status.FINISHED)
        {
            mLoadTrackerTask.cancel(true);
            mLoadTrackerTask = null;
        }
        // Do application deinitialization in native code
        deinitApplicationNative();
        // Unload texture
        if (mTextures != null) {
        	mTextures.clear();
        	mTextures = null;
        }
        // Deinitialize QCAR SDK
        QCAR.deinit();
        System.gc();
    }

    /** NOTE: this method is synchronized because of a potential concurrent
     * access by ImageTargets::onResume() and InitQCARTask::onPostExecute(). */
    private synchronized void updateApplicationStatus(int appStatus)
    {
        // Exit if there is no change in status
        if (mAppStatus == appStatus)
            return;
        // Store new status value      
        mAppStatus = appStatus;
        // Execute application state-specific actions
        switch (mAppStatus)
        {
            case APPSTATUS_INIT_APP:
                // Initialize application elements that do not rely on QCAR
                // initialization  
                initApplication();
                // Proceed to next application initialization status
                updateApplicationStatus(APPSTATUS_INIT_QCAR);
                break;
            case APPSTATUS_INIT_QCAR:
                // Initialize QCAR SDK asynchronously to avoid blocking the
                // main (UI) thread.
                // This task instance must be created and invoked on the UI
                // thread and it can be executed only once!
                try
                {
                    mInitQCARTask = new InitQCARTask();
                    mInitQCARTask.execute();
                }
                catch (Exception e)
                {
                	Log.e("demo", "Initializing QCAR SDK failed");
                }
                break;
            case APPSTATUS_INIT_APP_AR:
                // Initialize Augmented Reality-specific application elements
                // that may rely on the fact that the QCAR SDK has been
                // already initialized
                initApplicationAR();
                // Proceed to next application initialization status
                updateApplicationStatus(APPSTATUS_INIT_TRACKER);
                break;
            case APPSTATUS_INIT_TRACKER:
                // Load the tracking data set
                // This task instance must be created and invoked on the UI
                // thread and it can be executed only once!
                try
                {
                    mLoadTrackerTask = new LoadTrackerTask();
                    mLoadTrackerTask.execute();
                }
                catch (Exception e)
                {
                	Log.e("demo", "Loading tracking data set failed");
                }
                break;
            case APPSTATUS_INITED:
                // Hint to the virtual machine that it would be a good time to
                // run the garbage collector.
                //
                // NOTE: This is only a hint. There is no guarantee that the
                // garbage collector will actually be run.
                System.gc();
                // Native post initialization:
                onQCARInitializedNative();
                // Request a callback function after a given timeout to dismiss
                // the splash screen:
                Handler handler = new Handler();
                handler.postDelayed(
                    new Runnable() {
                        public void run()
                        {                            
                            // Activate the renderer
                            mRenderer.mIsActive = true;
                            // Now add the GL surface view. It is important
                            // that the OpenGL ES surface view gets added
                            // BEFORE the camera is started and video
                            // background is configured.
                            addContentView(mGlView, new LayoutParams(
                                            LayoutParams.FILL_PARENT,
                                            LayoutParams.FILL_PARENT));
                            addContentView(mGUIManager.getOverlayView(), new LayoutParams(
                                    LayoutParams.FILL_PARENT,
                                    LayoutParams.FILL_PARENT));
                            // Start the camera:
                            updateApplicationStatus(APPSTATUS_CAMERA_RUNNING);
                        }
                    } , 0);
                break;
            case APPSTATUS_CAMERA_STOPPED:
                // Call the native function to stop the camera
                stopCamera();
                break;
            case APPSTATUS_CAMERA_RUNNING:
                // Call the native function to start the camera
                startCamera(); 
                break;
            default:
                throw new RuntimeException("Invalid application state");
        }
    }
    
    /** Initialize application GUI elements that are not related to AR. */
    private void initApplication()
    {
    	int screenOrientation = 1;
        // Apply screen orientation
        setRequestedOrientation(screenOrientation);
        // Pass on screen orientation info to native code
        setActivityPortraitMode(screenOrientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        // Query display dimensions
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
        mScreenWidth = metrics.widthPixels;
        mScreenHeight = metrics.heightPixels;
        // As long as this window is visible to the user, keep the device's
        // screen turned on and bright.
        getWindow().setFlags(
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    /** Initializes AR application components. */
    private void initApplicationAR()
    {        
        // Do application initialization in native code (e.g. registering
        // callbacks, etc.)
        initApplicationNative(mScreenWidth, mScreenHeight);
        // Create OpenGL ES view:
        int depthSize = 16;
        int stencilSize = 0;
        boolean translucent = QCAR.requiresAlpha();
        mGlView = new QCARSampleGLView(this);
        mGlView.init(mQCARFlags, translucent, depthSize, stencilSize);
        mRenderer = new ImageTargetsRenderer();
        mGlView.setRenderer(mRenderer);
        mGUIManager = new GUIManager(this, getApplicationContext());
        mRenderer.setMainActivity(this, mGUIManager);
    }

    /** Invoked the first time when the options menu is displayed to give
     *  the Activity a chance to populate its Menu with menu items. */
    public boolean onCreateOptionsMenu(Menu menu)
    {
        super.onCreateOptionsMenu(menu);
        menu.add("Toggle flash");
        menu.add("Autofocus");
        SubMenu focusModes = menu.addSubMenu("Focus Modes");
        focusModes.add("Auto Focus").setCheckable(true);
        focusModes.add("Fixed Focus").setCheckable(true);
        focusModes.add("Infinity").setCheckable(true);
        focusModes.add("Macro Mode").setCheckable(true);
        return true;
    }
    
    /** Invoked when the user selects an item from the Menu */
    public boolean onOptionsItemSelected(MenuItem item)
    {
        if(item.getTitle().equals("Toggle flash"))
        {
            mFlash = !mFlash;
            toggleFlash(mFlash);
        }
        else if(item.getTitle().equals("Autofocus"))
        	autofocus();
        else 
        {
            int arg = -1;
            if(item.getTitle().equals("Auto Focus"))
                arg = 0;
            if(item.getTitle().equals("Fixed Focus"))
                arg = 1;
            if(item.getTitle().equals("Infinity"))
                arg = 2;
            if(item.getTitle().equals("Macro Mode"))
                arg = 3;
            if(arg != -1)
            {
                item.setChecked(true);
                if(checked!= null)
                    checked.setChecked(false);
                checked = item;
                setFocusMode(arg);
            }
        }
        return true;
    }
    
    private MenuItem checked;
    private boolean mFlash = false;
    private native boolean toggleFlash(boolean flash);
    private native boolean autofocus();
    private native boolean setFocusMode(int mode);
    
    /** Returns the number of registered textures. */
    public int getTextureCount()
    {
        return mTextures.size();
    }
    /** Returns the texture object at the specified index. */
    public Texture getTexture(int i)
    {
        return mTextures.elementAt(i);
    }

    /** A helper for loading native libraries stored in "libs/armeabi*". */
    public static boolean loadLibrary(String nLibName)
    {
        try
        {
            System.loadLibrary(nLibName);
            return true;
        }
        catch (UnsatisfiedLinkError ulee)
        {
            Log.e("demo", "The library lib" + nLibName + ".so could not be loaded");
        }
        catch (SecurityException se)
        {
            Log.e("demo", "The library lib" + nLibName + ".so was not allowed to be loaded");
        }
        return false;
    }
    
    /** Native function to initialize the application. */
    private native void initApplicationNative(int width, int height);
    /** native method for querying the OpenGL ES version.
     * Returns 1 for OpenGl ES 1.1, returns 2 for OpenGl ES 2.0. */
    public native int getOpenGlEsVersionNative();
    /** Native sample initialization. */
    public native void onQCARInitializedNative();
    /** Native methods for starting and stoping the camera. */ 
    private native void startCamera();
    private native void stopCamera();
    /** Native function to deinitialize the application.*/
    private native void deinitApplicationNative();
    /** Tells native code whether we are in portait or landscape mode */
    private native void setActivityPortraitMode(boolean isPortrait);
    
    /** Native function to receive touch events. */
    public native void nativeTouchEvent(float x, float y);
    
    /** Send touch events to native. */
    @Override
    public boolean onTouchEvent(MotionEvent event)
    {
        int action = event.getAction();
        int actionType = -1;
        switch (action & MotionEvent.ACTION_MASK) {
            case MotionEvent.ACTION_DOWN:
                actionType = 0;
                break;
        }
        if (actionType == 0) {
        	float x = event.getX();
        	float y = event.getY();
        	nativeTouchEvent(x, y);
        }
        return true;
    }
}