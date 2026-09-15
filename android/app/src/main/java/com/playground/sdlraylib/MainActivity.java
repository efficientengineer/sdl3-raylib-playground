package com.playground.sdlraylib;

import android.app.NativeActivity;

public class MainActivity extends NativeActivity {
    static {
        System.loadLibrary("combined_demo");
    }
}
