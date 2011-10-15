package com.demo.TargetDemo;

import android.content.Context;
import android.os.Handler;
import android.os.Message;
import android.view.View;
import android.widget.Button;
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
	private Button refresh;
	
	public native void nativeRefresh();
	
	/** Initialize the GUIManager. */
	public GUIManager(TargetDemo _demo, Context context)
	{
		overlayView = View.inflate(context, R.layout.information_layout, null);
		applicationContext = context;
		
		refresh = (Button)overlayView.findViewById(R.id.refresh_button);
		refresh.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				nativeRefresh();
				}
	        });
	    
		mainActivityHandler = new Handler() {
		@Override
		public void handleMessage(Message msg) {
	       		String text = (String) msg.obj;
	       		Toast.makeText(applicationContext, text, Toast.LENGTH_LONG).show();
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
