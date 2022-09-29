package com.bir.ffmpeg.demo;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceHolder;
import android.widget.TextView;

import com.bir.ffmpeg.demo.databinding.ActivityMainBinding;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'demo' library on application startup.
    static {
        System.loadLibrary("ffmpeg-demo");
    }

    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        StringBuffer sb = new StringBuffer(getFFmpegVersion());
        // Example of a call to a native method
        TextView tv = binding.sampleText;
        tv.setText(sb.toString());

        binding.surface.getHolder().addCallback(new SurfaceCallback());
    }

    /**
     * A native method that is implemented by the 'demo' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();

    public native String getFFmpegVersion();

    public native String play(String filePath, Object surface);

    private class SurfaceCallback implements SurfaceHolder.Callback {
        @Override
        public void surfaceCreated(@NonNull SurfaceHolder holder) {
            Log.d("daniel", "surfaceCreated() called with: holder = [" + holder + "]");
            new Thread(new Runnable() {
                @Override
                public void run() {
                    play("/sdcard/DCIM/Camera/VID_20220927_143914.mp4", holder.getSurface());
                }
            }).start();
        }

        @Override
        public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {

        }

        @Override
        public void surfaceDestroyed(@NonNull SurfaceHolder holder) {

        }
    }
}