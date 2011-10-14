package com.demo.TargetDemo;

import android.content.Context;
import android.os.Handler;
import android.os.Message;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

public class GUIManager {
	// Custom views
	private View overlayView;
	private TextView tv;
	// The main application context
	private Context applicationContext;
	// A Handler for working with the gui from other threads
	private Handler mainActivityHandler;
	
	/** Initialize the GUIManager. */
	public GUIManager(Context context)
	{
		overlayView = View.inflate(context, R.layout.information_layout, null);
		tv = (TextView) overlayView.findViewById(R.id.debug_info);
		applicationContext = context;
		mainActivityHandler = new Handler() {
		@Override
		public void handleMessage(Message msg) {
		    switch (msg.what) {
		        case 1:
		       		String text = (String) msg.obj;
		       		tv.setText(text);
		        	break;
		    }
		}
		};
	}
	
	public void sendThreadSafeGUIMessage(Message message) {
		mainActivityHandler.sendMessage(message);
	}
	
	/** Getter for the overlay view. */
	public View getOverlayView()
	{
		return overlayView;
	}
}
